#include "hypertls.h"

#define NETLINK_TEST 31
#define MSG_LEN 125
#define MAX_PAYLOAD 125
#define TM_START 1

typedef struct _user_msg_info {
    struct nlmsghdr hdr;
    char msg_type;
    char msg[MSG_LEN];
} user_msg_info;

struct my_msg {
    uint32_t
        client_pod_ip; // 客户端 Pod IP 使用 uint32_t (与 inet_addr 返回类型一致)
    uint32_t server_pod_ip; // 服务端 Pod IP 使用 uint32_t
    unsigned short client_pod_port;
    unsigned short server_pod_port;
    char opcode;
    bool server;
};

enum { // opcode
    TLS_SERVICE_INIT,
    TLS_SERVICE_START,
    TLS_SERVICE_FETCH
};

int debugLevel = DEBUG_LEVEL_ALL;
void log_fun(int level,
             const char* file,
             const char* func,
             int line,
             const char* fmt,
             ...)
{
    if (level >= debugLevel) {
        va_list ap;
        va_start(ap, fmt);
        fprintf(stdout, "%s-->%s(%d): ", file, func, line);
        vfprintf(stdout, fmt, ap);
        va_end(ap);
    }
    return;
}

void print_hex_dump(unsigned char* mem, int size)
{
    unsigned char* point = mem;
    for (int i = 0; i < size; i++) {
        printf("%02x ", point[i]);
        if ((i + 1) % 16 == 0) {
            printf("\n");
        }
    }
}
double app_tm_interval(int stop, int user_time)
{
    double ret = 0;
#ifdef CLOCK_REALTIME
    static struct timespec tm_start;
    struct timespec now;
#else
    static unsigned long tm_start;
    unsigned long now;
#endif
    static int warning = 1;

    if (user_time && warning) {
        DEBUG_PRINTF(DEBUG_LEVEL_DEBUG,
                     "To get meaningful results, run"
                     "this program on idle system.\n");
        warning = 0;
    }
#ifdef CLOCK_REALTIME
    clock_gettime(CLOCK_REALTIME, &now);
    if (stop == TM_START)
        tm_start = now;
    else
        ret = ((now.tv_sec + now.tv_nsec * 1e-9)
               - (tm_start.tv_sec + tm_start.tv_nsec * 1e-9));
#else
    now = tickGet();
    if (stop == TM_START)
        tm_start = now;
    else
        ret = (now - tm_start) / (double)sysClkRateGet();
#endif
    return ret;
}

pid_t gettid(void)
{
    return syscall(SYS_gettid);
}

/* Initialize netlink context with socket and addressing */
static int netlink_ctx_init(netlink_context_t* ctx)
{
    if (ctx->sk_fd != -1) {
        return 0;
    }

    struct sockaddr_nl saddr;
    ctx->sk_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_TEST);
    if (ctx->sk_fd == -1) {
        DEBUG_PRINTF(DEBUG_LEVEL_ERR, "create socket error\n");
        return -1;
    }

    memset(&saddr, 0, sizeof(saddr));
    saddr.nl_family = AF_NETLINK;
    saddr.nl_pid = gettid();
    saddr.nl_groups = 0;

    if (bind(ctx->sk_fd, (struct sockaddr*)&saddr, sizeof(saddr)) != 0) {
        DEBUG_PRINTF(DEBUG_LEVEL_ERR, "bind() error\n");
        close(ctx->sk_fd);
        ctx->sk_fd = -1;
        return -1;
    }

    memset(&ctx->daddr, 0, sizeof(ctx->daddr));
    ctx->daddr.nl_family = AF_NETLINK;
    ctx->daddr.nl_pid = 0;
    ctx->daddr.nl_groups = 0;

    return 0;
}

/* Allocate netlink message header if not already allocated */
static int netlink_ctx_alloc_msg(netlink_context_t* ctx)
{
    if (ctx->nlh != NULL) {
        return 0;
    }

    ctx->nlh = (struct nlmsghdr*)malloc(NLMSG_SPACE(MAX_PAYLOAD));
    if (ctx->nlh == NULL) {
        DEBUG_PRINTF(DEBUG_LEVEL_ERR, "malloc netlink header failed\n");
        return -1;
    }

    memset(ctx->nlh, 0, sizeof(struct nlmsghdr));
    ctx->nlh->nlmsg_len = sizeof(struct nlmsghdr) + sizeof(struct my_msg);
    ctx->nlh->nlmsg_flags = 0;
    ctx->nlh->nlmsg_type = 0;
    ctx->nlh->nlmsg_seq = 0;

    return 0;
}

/* Clean up netlink context resources */
static void netlink_ctx_cleanup(netlink_context_t* ctx, bool close_socket)
{
    if (ctx->nlh) {
        free(ctx->nlh);
        ctx->nlh = NULL;
    }

    if (close_socket && ctx->sk_fd != -1) {
        close(ctx->sk_fd);
        ctx->sk_fd = -1;
    }
}

int init_user_netlink()
{
    int sk_fd;
    struct sockaddr_nl saddr;
    sk_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_TEST);
    if (sk_fd == -1) {
        DEBUG_PRINTF(DEBUG_LEVEL_ERR, "create socket error\n");
        return -1;
    }

    memset(&saddr, 0, sizeof(saddr));
    saddr.nl_family = AF_NETLINK; // AF_NETLINK
    saddr.nl_pid = gettid();
    saddr.nl_groups = 0;

    if (bind(sk_fd, (struct sockaddr*)&saddr, sizeof(saddr)) != 0) {
        DEBUG_PRINTF(DEBUG_LEVEL_ERR, "bind() error\n");
        close(sk_fd);
        return -1;
    }

    return sk_fd;
}

static struct nlmsghdr* malloc_netlink_pkthdr()
{
    struct nlmsghdr* nlh = NULL;
    nlh = (struct nlmsghdr*)malloc(NLMSG_SPACE(MAX_PAYLOAD));
    memset(nlh, 0, sizeof(struct nlmsghdr));
    nlh->nlmsg_len = sizeof(struct nlmsghdr) + sizeof(struct my_msg);
    nlh->nlmsg_flags = 0;
    nlh->nlmsg_type = 0;
    nlh->nlmsg_seq = 0;
    nlh->nlmsg_pid = gettid();
    return nlh;
}

int hypertls_service_init(void)
{
    struct sockaddr_nl daddr;
    struct nlmsghdr* nlh = NULL;
    user_msg_info u_info;
    int ret;
    socklen_t len;
    double cost_time = 0.0;
    int sk_fd = -1;

    sk_fd = init_user_netlink();
    if (sk_fd < 0) {
        DEBUG_PRINTF(DEBUG_LEVEL_ERR, "init_user_netlink failed!\n");
        return -1;
    }

    memset(&daddr, 0, sizeof(daddr));
    daddr.nl_family = AF_NETLINK;
    daddr.nl_pid = 0; // to kernel
    daddr.nl_groups = 0;

    nlh = malloc_netlink_pkthdr();

    if (nlh < 0) {
        DEBUG_PRINTF(DEBUG_LEVEL_ERR, "nlh malloc failed !\n");
        return -1;
    }
    struct my_msg mmsg;
    mmsg.opcode = TLS_SERVICE_INIT;
    // In peer-to-peer architecture, the server field is ignored by the kernel
    // We set it to true for consistency with the legacy protocol format
    mmsg.server = true;
    memcpy(NLMSG_DATA(nlh), &mmsg, sizeof(struct my_msg));
    app_tm_interval(TM_START, 0);
    ret = sendto(sk_fd,
                 nlh,
                 nlh->nlmsg_len,
                 0,
                 (struct sockaddr*)&daddr,
                 sizeof(struct sockaddr_nl));
    if (!ret) {
        DEBUG_PRINTF(DEBUG_LEVEL_ERR, "sendto failed\n");
        close(sk_fd);
        exit(-1);
    }

    while (1) {
        memset(&u_info, 0, sizeof(u_info));
        len = sizeof(struct sockaddr_nl);
        ret = recvfrom(sk_fd,
                       &u_info,
                       sizeof(user_msg_info),
                       0,
                       (struct sockaddr*)&daddr,
                       &len);
        if (!ret) {
            DEBUG_PRINTF(DEBUG_LEVEL_ERR, "recv form kernel error\n");
            return -2;
        }
        DEBUG_PRINTF(DEBUG_LEVEL_DEBUG, "%s\n", u_info.msg);
        switch (u_info.msg_type) {
        case 0x07: // 初始化完成（成功或已初始化）
            ret = 0; // 返回成功状态
            goto end;
        case 0x04: // 日志
            break;
        }
    }

end:
    cost_time = app_tm_interval(0, 0);
    DEBUG_PRINTF(DEBUG_LEVEL_DEBUG, "cost time: %.2fms\n", cost_time * 1000);
    close(sk_fd);
    free(nlh);
    return ret;
}

static __thread netlink_context_t g_netlink_ctx = { .sk_fd = -1, .nlh = NULL };

int hypertls_handshake(uint32_t client_pod_ip,
                       uint32_t server_pod_ip,
                       unsigned short client_pod_port,
                       unsigned short server_pod_port)
{
    user_msg_info u_info;
    int ret;
    socklen_t len = sizeof(struct sockaddr_nl);
    double cost_time = 0.0;

    if (netlink_ctx_init(&g_netlink_ctx) != 0) {
        return -1;
    }

    if (netlink_ctx_alloc_msg(&g_netlink_ctx) != 0) {
        return -1;
    }

    g_netlink_ctx.nlh->nlmsg_pid = gettid();

    struct my_msg mmsg;
    mmsg.opcode = TLS_SERVICE_START;
    // 字节序说明 (Byte Order):
    // - inet_addr() 返回网络字节序 (network byte order)
    // - 直接存入 struct 用于传输 (store as-is for transmission)
    // - 内核空间会用 ntohl() 转换为主机序 (kernel uses ntohl() to convert)
    // - 参考: test/hypertls_handshake.c 顶部的详细说明
    mmsg.client_pod_ip
        = client_pod_ip; // 网络字节序 (network byte order from inet_addr)
    mmsg.server_pod_ip
        = server_pod_ip; // 网络字节序 (network byte order from inet_addr)
    mmsg.client_pod_port
        = htons(client_pod_port); // 主机序 → 网络序 (host to network)
    mmsg.server_pod_port
        = htons(server_pod_port); // 主机序 → 网络序 (host to network)
    mmsg.server = 0;
    memcpy(NLMSG_DATA(g_netlink_ctx.nlh), &mmsg, sizeof(struct my_msg));

    app_tm_interval(TM_START, 0);

    if (g_netlink_ctx.sk_fd < 0) {
        DEBUG_PRINTF(DEBUG_LEVEL_ERR, "sk_fd is < 0 error!\n");
        return -1;
    }

    ret = sendto(g_netlink_ctx.sk_fd,
                 g_netlink_ctx.nlh,
                 g_netlink_ctx.nlh->nlmsg_len,
                 0,
                 (struct sockaddr*)&g_netlink_ctx.daddr,
                 len);
    if (!ret) {
        DEBUG_PRINTF(DEBUG_LEVEL_ERR, "sendto error\n");
        netlink_ctx_cleanup(&g_netlink_ctx, true);
        exit(-1);
    }

    while (1) {
        memset(&u_info, 0, sizeof(u_info));
        ret = recvfrom(g_netlink_ctx.sk_fd,
                       &u_info,
                       sizeof(user_msg_info),
                       0,
                       (struct sockaddr*)&g_netlink_ctx.daddr,
                       &len);
        if (!ret) {
            DEBUG_PRINTF(DEBUG_LEVEL_ERR, "recv form kernel error\n");
            return -2;
        }
        DEBUG_PRINTF(DEBUG_LEVEL_DEBUG, "%s\n", u_info.msg);
        switch (u_info.msg_type) {
        case 0x01: // handshake 成功（首次握手）
        case 0x02: // handshake 成功（非首次握手）
        case 0x05: // 已连接过
            ret = 0;
            goto end;
        case 0x03: // handshake 失败，出现alert
            ret = -1;
            goto end;
        case 0x04: // 日志
            break;
        }
    }

end:
    cost_time = app_tm_interval(0, 0);
    DEBUG_PRINTF(DEBUG_LEVEL_DEBUG, "cost time: %.2fms\n", cost_time * 1000);
    return ret;
}

struct key_back hypertls_fetch_key(uint32_t client_pod_ip,
                                   uint32_t server_pod_ip,
                                   unsigned short client_pod_port,
                                   unsigned short server_pod_port)
{
    struct key_back key;
    memset(&key, 0, sizeof(struct key_back));

    netlink_context_t ctx = { .sk_fd = -1, .nlh = NULL };
    user_msg_info u_info;
    int ret;
    socklen_t len = sizeof(struct sockaddr_nl);
    double cost_time = 0.0;

    if (netlink_ctx_init(&ctx) != 0) {
        return key;
    }

    if (netlink_ctx_alloc_msg(&ctx) != 0) {
        netlink_ctx_cleanup(&ctx, true);
        return key;
    }

    ctx.nlh->nlmsg_pid = gettid();

    struct my_msg mmsg;
    mmsg.opcode = TLS_SERVICE_FETCH;
    // 字节序说明 (Byte Order):
    // - inet_addr() 返回网络字节序 (network byte order)
    // - 直接存入 struct 用于传输 (store as-is for transmission)
    // - 内核空间会用 ntohl() 转换为主机序 (kernel uses ntohl() to convert)
    mmsg.client_pod_ip
        = client_pod_ip; // 网络字节序 (network byte order from inet_addr)
    mmsg.server_pod_ip
        = server_pod_ip; // 网络字节序 (network byte order from inet_addr)
    mmsg.client_pod_port
        = htons(client_pod_port); // 主机序 → 网络序 (host to network)
    mmsg.server_pod_port
        = htons(server_pod_port); // 主机序 → 网络序 (host to network)
    memcpy(NLMSG_DATA(ctx.nlh), &mmsg, sizeof(struct my_msg));

    app_tm_interval(TM_START, 0);

    ret = sendto(ctx.sk_fd,
                 ctx.nlh,
                 ctx.nlh->nlmsg_len,
                 0,
                 (struct sockaddr*)&ctx.daddr,
                 len);
    if (!ret) {
        DEBUG_PRINTF(DEBUG_LEVEL_ERR, "sendto error\n");
        netlink_ctx_cleanup(&ctx, true);
        exit(-1);
    }

    memset(&u_info, 0, sizeof(u_info));
    ret = recvfrom(ctx.sk_fd,
                   &u_info,
                   sizeof(user_msg_info),
                   0,
                   (struct sockaddr*)&ctx.daddr,
                   &len);
    if (!ret) {
        DEBUG_PRINTF(DEBUG_LEVEL_ERR, "recv form kernel error\n");
        netlink_ctx_cleanup(&ctx, true);
        exit(-1);
    }

    memcpy(&key, &u_info.msg, sizeof(struct key_back));

    netlink_ctx_cleanup(&ctx, true);

    cost_time = app_tm_interval(0, 0);
    DEBUG_PRINTF(DEBUG_LEVEL_DEBUG, "cost time: %.2fms\n", cost_time * 1000);
    return key;
}
