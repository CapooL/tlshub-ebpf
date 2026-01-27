#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <sys/syscall.h>
#include <arpa/inet.h>
#include "tlshub_client.h"

#define NETLINK_TEST 31  /* TLSHub Netlink 协议号 */
#define MAX_PAYLOAD 125

/* TLSHub 消息结构 */
typedef struct _user_msg_info {
    struct nlmsghdr hdr;
    char msg_type;
    char msg[MAX_PAYLOAD];
} user_msg_info;

/* TLSHub 消息载荷 */
struct my_msg {
    uint32_t client_pod_ip;
    uint32_t server_pod_ip;
    uint32_t server_node_ip;  /* 新增：服务端 Node IP（网络字节序） */
    unsigned short client_pod_port;
    unsigned short server_pod_port;
    char opcode;
    bool server;
};

/* TLSHub 操作码 */
enum {
    TLS_SERVICE_INIT,   // 0
    TLS_SERVICE_START,  // 1 
    TLS_SERVICE_FETCH   // 2
};

/* TLSHub 响应消息类型 */
enum {
    MSG_TYPE_HANDSHAKE_SUCCESS_FIRST = 0x01,  // 首次握手成功
    MSG_TYPE_HANDSHAKE_SUCCESS = 0x02,        // 非首次握手成功
    MSG_TYPE_HANDSHAKE_FAILED = 0x03,         // 握手失败
    MSG_TYPE_LOG = 0x04,                       // 日志消息
    MSG_TYPE_ALREADY_CONNECTED = 0x05,         // 已连接
    MSG_TYPE_INIT_COMPLETE = 0x07              // 初始化完成
};

/* 密钥返回结构 */
struct key_back {
    int status; // 0:成功 -1:失败 -2:过期
    unsigned char masterkey[32];
};

static int netlink_sock = -1;
static struct sockaddr_nl dest_addr;

pid_t gettid(void)
{
    return syscall(SYS_gettid);
}

/**
 * 初始化 TLSHub 客户端
 */
int tlshub_client_init(void) {
    struct sockaddr_nl src_addr;
    struct nlmsghdr *nlh = NULL;
    user_msg_info u_info;
    int ret;
    socklen_t len;
    
    /* 创建 Netlink socket */
    netlink_sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_TEST);
    if (netlink_sock < 0) {
        perror("Failed to create netlink socket");
        return -1;
    }
    
    /* 绑定本地地址 */
    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = AF_NETLINK;
    src_addr.nl_pid = gettid();
    src_addr.nl_groups = 0;
    
    if (bind(netlink_sock, (struct sockaddr*)&src_addr, sizeof(src_addr)) < 0) {
        perror("Failed to bind netlink socket");
        close(netlink_sock);
        netlink_sock = -1;
        return -1;
    }
    
    /* 设置目标地址（内核） */
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.nl_family = AF_NETLINK;
    dest_addr.nl_pid = 0;
    dest_addr.nl_groups = 0;
    
    /* 分配并初始化 Netlink 消息 */
    nlh = (struct nlmsghdr*)malloc(NLMSG_SPACE(MAX_PAYLOAD));
    if (!nlh) {
        fprintf(stderr, "Failed to allocate netlink message\n");
        close(netlink_sock);
        netlink_sock = -1;
        return -1;
    }
    
    memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
    nlh->nlmsg_len = sizeof(struct nlmsghdr) + sizeof(struct my_msg);
    nlh->nlmsg_flags = 0;
    nlh->nlmsg_type = 0;
    nlh->nlmsg_seq = 0;
    nlh->nlmsg_pid = gettid();
    
    /* 准备初始化消息 */
    struct my_msg mmsg;
    memset(&mmsg, 0, sizeof(mmsg));
    mmsg.opcode = TLS_SERVICE_INIT;
    mmsg.server = true;
    memcpy(NLMSG_DATA(nlh), &mmsg, sizeof(struct my_msg));
    
    /* 发送初始化消息 */
    ret = sendto(netlink_sock, nlh, nlh->nlmsg_len, 0,
                 (struct sockaddr*)&dest_addr, sizeof(struct sockaddr_nl));
    if (!ret) {
        fprintf(stderr, "Failed to send init message\n");
        free(nlh);
        close(netlink_sock);
        netlink_sock = -1;
        return -1;
    }
    
    /* 等待初始化响应 */
    while (1) {
        memset(&u_info, 0, sizeof(u_info));
        len = sizeof(struct sockaddr_nl);
        ret = recvfrom(netlink_sock, &u_info, sizeof(user_msg_info), 0,
                       (struct sockaddr*)&dest_addr, &len);
        if (!ret) {
            fprintf(stderr, "Failed to receive init response\n");
            free(nlh);
            close(netlink_sock);
            netlink_sock = -1;
            return -1;
        }
        
        switch (u_info.msg_type) {
        case MSG_TYPE_INIT_COMPLETE:
            printf("TLSHub client initialized successfully\n");
            free(nlh);
            return 0;
        case MSG_TYPE_LOG:
            printf("TLSHub log: %s\n", u_info.msg);
            break;
        default:
            fprintf(stderr, "Unexpected message type during init: 0x%02x\n", u_info.msg_type);
            break;
        }
    }
    
    free(nlh);
    return 0;
}

/**
 * 清理 TLSHub 客户端
 */
void tlshub_client_cleanup(void) {
    if (netlink_sock >= 0) {
        close(netlink_sock);
        netlink_sock = -1;
        printf("TLSHub client cleaned up\n");
    }
}

/**
 * 根据四元组从 TLSHub 获取密钥
 * 
 * 注意：flow_tuple 使用标准的 saddr/daddr，需要映射到 Pod IP
 * 在点对点架构中，saddr = client_pod_ip, daddr = server_pod_ip
 */
int tlshub_fetch_key(struct flow_tuple *tuple, struct tls_key_info *key_info, uint32_t server_node_ip) {
    struct nlmsghdr *nlh = NULL;
    user_msg_info u_info;
    int ret;
    socklen_t len = sizeof(struct sockaddr_nl);
    struct key_back key;
    
    if (netlink_sock < 0 || !tuple || !key_info) {
        fprintf(stderr, "Invalid parameters for tlshub_fetch_key\n");
        return -1;
    }
    
    /* 分配 Netlink 消息 */
    nlh = (struct nlmsghdr*)malloc(NLMSG_SPACE(MAX_PAYLOAD));
    if (!nlh) {
        fprintf(stderr, "Failed to allocate netlink message\n");
        return -1;
    }
    
    memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
    nlh->nlmsg_len = sizeof(struct nlmsghdr) + sizeof(struct my_msg);
    nlh->nlmsg_flags = 0;
    nlh->nlmsg_type = 0;
    nlh->nlmsg_seq = 0;
    nlh->nlmsg_pid = gettid();
    
    /* 准备 fetch key 消息 */
    struct my_msg mmsg;
    memset(&mmsg, 0, sizeof(mmsg));
    mmsg.opcode = TLS_SERVICE_FETCH;
    // 字节序说明：
    // - tuple 中的 IP 已经是网络字节序（从 inet_addr 得到）
    // - 内核会用 ntohl() 转换为主机序
    mmsg.client_pod_ip = tuple->saddr;  // 网络字节序
    mmsg.server_pod_ip = tuple->daddr;  // 网络字节序
    mmsg.server_node_ip = server_node_ip;  // 网络字节序（新增）
    mmsg.client_pod_port = htons(tuple->sport);  // 主机序 → 网络序
    mmsg.server_pod_port = htons(tuple->dport);  // 主机序 → 网络序
    mmsg.server = false;
    
    memcpy(NLMSG_DATA(nlh), &mmsg, sizeof(struct my_msg));
    
    /* 发送消息 */
    ret = sendto(netlink_sock, nlh, nlh->nlmsg_len, 0,
                 (struct sockaddr*)&dest_addr, sizeof(struct sockaddr_nl));
    if (!ret) {
        fprintf(stderr, "Failed to send fetch_key message\n");
        free(nlh);
        return -1;
    }
    
    /* 接收响应 */
    memset(&u_info, 0, sizeof(u_info));
    ret = recvfrom(netlink_sock, &u_info, sizeof(user_msg_info), 0,
                   (struct sockaddr*)&dest_addr, &len);
    if (!ret) {
        fprintf(stderr, "Failed to receive fetch_key response\n");
        free(nlh);
        return -1;
    }
    
    /* 解析密钥 */
    memcpy(&key, u_info.msg, sizeof(struct key_back));
    
    free(nlh);
    
    if (key.status != 0) {
        fprintf(stderr, "Fetch key failed with status: %d\n", key.status);
        return -1;
    }
    
    /* 复制密钥信息到 tls_key_info */
    memcpy(key_info->key, key.masterkey, 32);
    key_info->key_len = 32;
    // IV/salt 信息需要从其他地方获取或使用默认值
    memset(key_info->iv, 0, 16);
    key_info->iv_len = 12;
    
    printf("Fetched TLS key from TLSHub (status: %d)\n", key.status);
    return 0;
}

/**
 * 通过 TLSHub 发起握手
 */
int tlshub_handshake(struct flow_tuple *tuple, uint32_t server_node_ip) {
    static __thread struct nlmsghdr *nlh = NULL;
    user_msg_info u_info;
    int ret;
    socklen_t len = sizeof(struct sockaddr_nl);
    
    if (netlink_sock < 0 || !tuple) {
        fprintf(stderr, "Invalid parameters for tlshub_handshake\n");
        return -1;
    }
    
    /* 分配 Netlink 消息（thread-local，重复使用） */
    if (nlh == NULL) {
        nlh = (struct nlmsghdr*)malloc(NLMSG_SPACE(MAX_PAYLOAD));
        if (!nlh) {
            fprintf(stderr, "Failed to allocate netlink message\n");
            return -1;
        }
    }
    
    memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
    nlh->nlmsg_len = sizeof(struct nlmsghdr) + sizeof(struct my_msg);
    nlh->nlmsg_flags = 0;
    nlh->nlmsg_type = 0;
    nlh->nlmsg_seq = 0;
    nlh->nlmsg_pid = gettid();
    
    /* 准备握手消息 */
    struct my_msg mmsg;
    memset(&mmsg, 0, sizeof(mmsg));
    mmsg.opcode = TLS_SERVICE_START;
    mmsg.client_pod_ip = tuple->saddr;  // 网络字节序
    mmsg.server_pod_ip = tuple->daddr;  // 网络字节序
    mmsg.server_node_ip = server_node_ip;  // 网络字节序（新增）
    mmsg.client_pod_port = htons(tuple->sport);  // 主机序 → 网络序
    mmsg.server_pod_port = htons(tuple->dport);  // 主机序 → 网络序
    mmsg.server = false;
    
    memcpy(NLMSG_DATA(nlh), &mmsg, sizeof(struct my_msg));
    
    /* 发送握手消息 */
    ret = sendto(netlink_sock, nlh, nlh->nlmsg_len, 0,
                 (struct sockaddr*)&dest_addr, sizeof(struct sockaddr_nl));
    if (!ret) {
        fprintf(stderr, "Failed to send handshake message\n");
        return -1;
    }
    
    /* 等待握手响应 */
    while (1) {
        memset(&u_info, 0, sizeof(u_info));
        ret = recvfrom(netlink_sock, &u_info, sizeof(user_msg_info), 0,
                       (struct sockaddr*)&dest_addr, &len);
        if (!ret) {
            fprintf(stderr, "Failed to receive handshake response\n");
            return -1;
        }
        
        switch (u_info.msg_type) {
        case MSG_TYPE_HANDSHAKE_SUCCESS_FIRST:
        case MSG_TYPE_HANDSHAKE_SUCCESS:
        case MSG_TYPE_ALREADY_CONNECTED:
            printf("TLSHub handshake completed successfully (type: 0x%02x)\n", 
                   u_info.msg_type);
            return 0;
        case MSG_TYPE_HANDSHAKE_FAILED:
            fprintf(stderr, "TLSHub handshake failed\n");
            return -1;
        case MSG_TYPE_LOG:
            printf("TLSHub log: %s\n", u_info.msg);
            break;
        default:
            fprintf(stderr, "Unexpected message type during handshake: 0x%02x\n", 
                    u_info.msg_type);
            break;
        }
    }
    
    return 0;
}
