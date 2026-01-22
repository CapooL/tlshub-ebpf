/* SPDX-License-Identifier: GPL-2.0 */
/*
 * TLSHub eBPF - 用户空间程序
 * 负责加载 eBPF 程序、监听事件和管理配置
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/resource.h>
#include <arpa/inet.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include <net/if.h>
#include <linux/if_link.h>

#include "../../include/tlshub_common.h"

#define PERF_BUFFER_PAGES 64
#define PERF_POLL_TIMEOUT_MS 100

static volatile sig_atomic_t keep_running = 1;
static int ifindex = 0;

/* 信号处理函数 */
static void sig_handler(int sig)
{
    keep_running = 0;
}

/* 将 IP 地址转换为字符串 */
static void ip_to_str(__u32 ip, char *buf, size_t size)
{
    struct in_addr addr;
    addr.s_addr = ip;
    inet_ntop(AF_INET, &addr, buf, size);
}

/* 获取事件类型字符串 */
static const char *event_type_str(__u32 type)
{
    switch (type) {
    case EVENT_PACKET_CAPTURE:
        return "数据包捕获";
    case EVENT_TLS_HANDSHAKE:
        return "TLS握手";
    case EVENT_CONNECTION_NEW:
        return "新连接";
    case EVENT_CONNECTION_CLOSE:
        return "连接关闭";
    default:
        return "未知";
    }
}

/* 获取方向字符串 */
static const char *direction_str(__u8 dir)
{
    return (dir == DIR_INGRESS) ? "入站" : "出站";
}

/* Perf 事件处理回调函数 */
static void handle_event(void *ctx, int cpu, void *data, __u32 data_sz)
{
    struct event_data *e = data;
    char saddr_str[INET_ADDRSTRLEN];
    char daddr_str[INET_ADDRSTRLEN];
    
    ip_to_str(e->pkt_info.saddr, saddr_str, sizeof(saddr_str));
    ip_to_str(e->pkt_info.daddr, daddr_str, sizeof(daddr_str));
    
    printf("[%llu] 事件类型: %s | 方向: %s\n",
           e->timestamp, event_type_str(e->event_type),
           direction_str(e->pkt_info.direction));
    printf("  源地址: %s:%u -> 目标地址: %s:%u\n",
           saddr_str, e->pkt_info.sport,
           daddr_str, e->pkt_info.dport);
    printf("  协议: %u | PID: %u | 载荷长度: %u 字节\n",
           e->pkt_info.protocol, e->pid, e->pkt_info.payload_len);
    
    /* 如果是 TLS 握手，打印前几个字节 */
    if (e->event_type == EVENT_TLS_HANDSHAKE && e->pkt_info.payload_len > 0) {
        printf("  TLS 载荷 (前16字节): ");
        int print_len = e->pkt_info.payload_len > 16 ? 16 : e->pkt_info.payload_len;
        for (int i = 0; i < print_len; i++) {
            printf("%02x ", e->payload[i]);
        }
        printf("\n");
    }
    printf("\n");
}

/* Perf 事件丢失回调函数 */
static void handle_lost_events(void *ctx, int cpu, __u64 lost_cnt)
{
    fprintf(stderr, "警告: CPU %d 丢失了 %llu 个事件\n", cpu, lost_cnt);
}

/* 打印统计信息 */
static void print_stats(int stats_fd)
{
    __u32 key = 0;
    struct stats_info stats = {};
    
    if (bpf_map_lookup_elem(stats_fd, &key, &stats) == 0) {
        printf("\n=== 流量统计 ===\n");
        printf("接收: %llu 数据包, %llu 字节\n", 
               stats.rx_packets, stats.rx_bytes);
        printf("发送: %llu 数据包, %llu 字节\n", 
               stats.tx_packets, stats.tx_bytes);
        printf("丢弃: %llu 数据包\n", stats.dropped);
        printf("================\n\n");
    }
}

/* 设置端口过滤器 */
static int setup_port_filter(int filter_fd, __u16 *ports, int num_ports)
{
    __u32 value = 1;  /* 启用 */
    
    for (int i = 0; i < num_ports; i++) {
        __u32 key = ports[i];
        if (bpf_map_update_elem(filter_fd, &key, &value, BPF_ANY) < 0) {
            fprintf(stderr, "设置端口过滤器失败 (端口 %u): %s\n",
                    ports[i], strerror(errno));
            return -1;
        }
        printf("已添加端口过滤: %u\n", ports[i]);
    }
    
    return 0;
}

/* 使用说明 */
static void usage(const char *prog)
{
    fprintf(stderr,
            "用法: %s [选项]\n"
            "选项:\n"
            "  -i <接口名>    指定网络接口 (必需)\n"
            "  -p <端口>      添加端口过滤 (可多次使用)\n"
            "  -h             显示帮助信息\n"
            "\n"
            "示例:\n"
            "  %s -i eth0                     # 监听 eth0 接口的所有流量\n"
            "  %s -i eth0 -p 443 -p 8443      # 只监听端口 443 和 8443\n",
            prog, prog, prog);
}

int main(int argc, char **argv)
{
    struct bpf_object *obj = NULL;
    struct bpf_program *prog_xdp = NULL;
    struct perf_buffer *pb = NULL;
    int prog_fd_xdp = -1;
    int events_fd = -1, stats_fd = -1, filter_fd = -1;
    int err;
    const char *ifname = NULL;
    __u16 filter_ports[100];
    int num_filter_ports = 0;
    int opt;
    
    /* 解析命令行参数 */
    while ((opt = getopt(argc, argv, "i:p:h")) != -1) {
        switch (opt) {
        case 'i':
            ifname = optarg;
            break;
        case 'p':
            if (num_filter_ports < 100) {
                filter_ports[num_filter_ports++] = atoi(optarg);
            }
            break;
        case 'h':
        default:
            usage(argv[0]);
            return opt == 'h' ? 0 : 1;
        }
    }
    
    if (!ifname) {
        fprintf(stderr, "错误: 必须指定网络接口\n\n");
        usage(argv[0]);
        return 1;
    }
    
    ifindex = if_nametoindex(ifname);
    if (!ifindex) {
        fprintf(stderr, "错误: 找不到网络接口 %s\n", ifname);
        return 1;
    }
    
    /* 设置资源限制 */
    struct rlimit rlim = {
        .rlim_cur = RLIM_INFINITY,
        .rlim_max = RLIM_INFINITY,
    };
    if (setrlimit(RLIMIT_MEMLOCK, &rlim)) {
        fprintf(stderr, "警告: 无法设置 RLIMIT_MEMLOCK: %s\n", strerror(errno));
    }
    
    /* 加载 eBPF 对象文件 */
    printf("正在加载 eBPF 程序...\n");
    obj = bpf_object__open_file("tlshub_kern.o", NULL);
    if (libbpf_get_error(obj)) {
        fprintf(stderr, "错误: 无法打开 BPF 对象文件\n");
        return 1;
    }
    
    /* 加载 eBPF 程序到内核 */
    err = bpf_object__load(obj);
    if (err) {
        fprintf(stderr, "错误: 无法加载 BPF 对象: %d\n", err);
        goto cleanup;
    }
    
    /* 获取 XDP 程序 */
    prog_xdp = bpf_object__find_program_by_name(obj, "xdp_packet_capture");
    if (!prog_xdp) {
        fprintf(stderr, "错误: 找不到 XDP 程序\n");
        goto cleanup;
    }
    prog_fd_xdp = bpf_program__fd(prog_xdp);
    
    /* 获取 Map 文件描述符 */
    events_fd = bpf_object__find_map_fd_by_name(obj, "events");
    stats_fd = bpf_object__find_map_fd_by_name(obj, "stats_map");
    filter_fd = bpf_object__find_map_fd_by_name(obj, "filter_ports");
    
    if (events_fd < 0 || stats_fd < 0 || filter_fd < 0) {
        fprintf(stderr, "错误: 无法找到 BPF maps\n");
        goto cleanup;
    }
    
    /* 设置端口过滤器 */
    if (num_filter_ports > 0) {
        if (setup_port_filter(filter_fd, filter_ports, num_filter_ports) < 0) {
            goto cleanup;
        }
    }
    
    /* 附加 XDP 程序到网络接口 */
    printf("正在附加 XDP 程序到接口 %s...\n", ifname);
    err = bpf_xdp_attach(ifindex, prog_fd_xdp, XDP_FLAGS_SKB_MODE, NULL);
    if (err) {
        fprintf(stderr, "错误: 无法附加 XDP 程序: %s\n", strerror(-err));
        goto cleanup;
    }
    
    /* 创建 perf buffer */
    pb = perf_buffer__new(events_fd, PERF_BUFFER_PAGES,
                          handle_event, handle_lost_events, NULL, NULL);
    if (libbpf_get_error(pb)) {
        fprintf(stderr, "错误: 无法创建 perf buffer\n");
        goto cleanup_xdp;
    }
    
    /* 注册信号处理 */
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    
    printf("\n开始捕获数据包... (按 Ctrl+C 停止)\n\n");
    
    /* 主循环 */
    while (keep_running) {
        err = perf_buffer__poll(pb, PERF_POLL_TIMEOUT_MS);
        if (err < 0 && err != -EINTR) {
            fprintf(stderr, "错误: perf buffer 轮询失败: %d\n", err);
            break;
        }
    }
    
    printf("\n正在停止...\n");
    
    /* 打印最终统计信息 */
    print_stats(stats_fd);
    
    /* 清理 perf buffer */
    perf_buffer__free(pb);
    
cleanup_xdp:
    /* 分离 XDP 程序 */
    bpf_xdp_detach(ifindex, XDP_FLAGS_SKB_MODE, NULL);
    printf("已分离 XDP 程序\n");
    
cleanup:
    /* 清理 BPF 对象 */
    if (obj)
        bpf_object__close(obj);
    
    return 0;
}
