/* SPDX-License-Identifier: GPL-2.0 */
/*
 * TLSHub eBPF - 内核空间数据包捕获程序
 * 基于 XDP 和 TC 实现高性能数据包捕获和流量拦截
 */

#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/in.h>
#include <linux/types.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>

#define MAX_PACKET_SIZE 1500
#define MAX_PAYLOAD_SIZE 512

/* 事件类型定义 */
#define EVENT_PACKET_CAPTURE 1
#define EVENT_TLS_HANDSHAKE 2
#define EVENT_CONNECTION_NEW 3
#define EVENT_CONNECTION_CLOSE 4

/* 数据包方向 */
#define DIR_INGRESS 0
#define DIR_EGRESS 1

/* 数据包信息结构 */
struct packet_info {
    __u32 saddr;          /* 源 IP 地址 */
    __u32 daddr;          /* 目的 IP 地址 */
    __u16 sport;          /* 源端口 */
    __u16 dport;          /* 目的端口 */
    __u8 protocol;        /* 协议类型 */
    __u8 direction;       /* 数据包方向 */
    __u16 payload_len;    /* 载荷长度 */
} __attribute__((packed));

/* eBPF 事件数据结构 */
struct event_data {
    __u64 timestamp;                    /* 时间戳 */
    __u32 event_type;                   /* 事件类型 */
    __u32 pid;                          /* 进程 ID */
    struct packet_info pkt_info;        /* 数据包信息 */
    __u8 payload[MAX_PAYLOAD_SIZE];     /* 载荷数据 */
} __attribute__((packed));

/* 统计信息结构 */
struct stats_info {
    __u64 rx_packets;    /* 接收的数据包数 */
    __u64 tx_packets;    /* 发送的数据包数 */
    __u64 rx_bytes;      /* 接收的字节数 */
    __u64 tx_bytes;      /* 发送的字节数 */
    __u64 dropped;       /* 丢弃的数据包数 */
} __attribute__((packed));

/* BPF Maps 定义 */

/* Perf 事件缓冲区，用于向用户空间传递事件 */
struct {
    __uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
    __uint(key_size, sizeof(__u32));
    __uint(value_size, sizeof(__u32));
} events SEC(".maps");

/* 统计信息 Map */
struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
    __uint(max_entries, 1);
    __type(key, __u32);
    __type(value, struct stats_info);
} stats_map SEC(".maps");

/* 配置 Map - 用于用户空间控制过滤规则 */
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 1024);
    __type(key, __u32);    /* 端口号作为 key */
    __type(value, __u32);  /* 1 表示启用，0 表示禁用 */
} filter_ports SEC(".maps");

/* 辅助函数：解析 IPv4 头部 */
static __always_inline int parse_ipv4(struct iphdr *iph, void *data_end,
                                       struct packet_info *pkt_info)
{
    if ((void *)(iph + 1) > data_end)
        return -1;

    pkt_info->saddr = iph->saddr;
    pkt_info->daddr = iph->daddr;
    pkt_info->protocol = iph->protocol;

    return 0;
}

/* 辅助函数：解析 TCP 头部 */
static __always_inline int parse_tcp(struct tcphdr *tcph, void *data_end,
                                      struct packet_info *pkt_info)
{
    if ((void *)(tcph + 1) > data_end)
        return -1;

    pkt_info->sport = bpf_ntohs(tcph->source);
    pkt_info->dport = bpf_ntohs(tcph->dest);

    return 0;
}

/* 辅助函数：判断是否为 TLS 流量（通过常见 TLS 端口） */
static __always_inline int is_tls_port(__u16 port)
{
    /* 常见的 TLS 端口：443 (HTTPS), 8443, 853 (DNS over TLS) */
    return (port == 443 || port == 8443 || port == 853);
}

/* 辅助函数：检查端口过滤器 */
static __always_inline int should_capture_port(__u16 port)
{
    __u32 key = port;
    __u32 *value = bpf_map_lookup_elem(&filter_ports, &key);
    
    /* 如果没有配置过滤规则，默认捕获所有端口 */
    if (!value)
        return 1;
    
    return *value != 0;
}

/* 辅助函数：更新统计信息 */
static __always_inline void update_stats(__u8 direction, __u32 bytes)
{
    __u32 key = 0;
    struct stats_info *stats = bpf_map_lookup_elem(&stats_map, &key);
    
    if (!stats)
        return;
    
    if (direction == DIR_INGRESS) {
        __sync_fetch_and_add(&stats->rx_packets, 1);
        __sync_fetch_and_add(&stats->rx_bytes, bytes);
    } else {
        __sync_fetch_and_add(&stats->tx_packets, 1);
        __sync_fetch_and_add(&stats->tx_bytes, bytes);
    }
}

/* XDP 程序 - 入方向数据包处理 */
SEC("xdp")
int xdp_packet_capture(struct xdp_md *ctx)
{
    void *data_end = (void *)(long)ctx->data_end;
    void *data = (void *)(long)ctx->data;
    struct ethhdr *eth = data;
    struct event_data event = {};
    
    /* 检查以太网头部 */
    if ((void *)(eth + 1) > data_end)
        return XDP_PASS;
    
    /* 只处理 IPv4 数据包 */
    if (eth->h_proto != bpf_htons(ETH_P_IP))
        return XDP_PASS;
    
    struct iphdr *iph = (struct iphdr *)(eth + 1);
    if (parse_ipv4(iph, data_end, &event.pkt_info) < 0)
        return XDP_PASS;
    
    /* 只处理 TCP 协议 */
    if (event.pkt_info.protocol != IPPROTO_TCP)
        return XDP_PASS;
    
    struct tcphdr *tcph = (struct tcphdr *)((void *)iph + (iph->ihl * 4));
    if (parse_tcp(tcph, data_end, &event.pkt_info) < 0)
        return XDP_PASS;
    
    /* 检查是否需要捕获此端口 */
    if (!should_capture_port(event.pkt_info.dport) && 
        !should_capture_port(event.pkt_info.sport))
        return XDP_PASS;
    
    /* 填充事件数据 */
    event.timestamp = bpf_ktime_get_ns();
    event.event_type = EVENT_PACKET_CAPTURE;
    event.pkt_info.direction = DIR_INGRESS;
    event.pid = bpf_get_current_pid_tgid() >> 32;
    
    /* 复制部分载荷数据 */
    void *payload = (void *)tcph + (tcph->doff * 4);
    __u32 payload_size = data_end - payload;
    
    if (payload_size > MAX_PAYLOAD_SIZE)
        payload_size = MAX_PAYLOAD_SIZE;
    
    event.pkt_info.payload_len = payload_size;
    
    if (payload_size > 0 && payload + payload_size <= data_end) {
        bpf_probe_read_kernel(&event.payload, payload_size & (MAX_PAYLOAD_SIZE - 1), payload);
        
        /* 检查是否为 TLS 握手包（Client Hello: 0x16 0x03）*/
        if (payload_size >= 2 && event.payload[0] == 0x16 && 
            event.payload[1] == 0x03) {
            event.event_type = EVENT_TLS_HANDSHAKE;
        }
    }
    
    /* 更新统计信息 */
    update_stats(DIR_INGRESS, data_end - data);
    
    /* 将事件发送到用户空间 */
    bpf_perf_event_output(ctx, &events, BPF_F_CURRENT_CPU,
                          &event, sizeof(event));
    
    return XDP_PASS;  /* 允许数据包继续传递 */
}

/* TC 程序 - 出方向数据包处理 */
SEC("tc")
int tc_packet_capture(struct __sk_buff *skb)
{
    void *data_end = (void *)(long)skb->data_end;
    void *data = (void *)(long)skb->data;
    struct ethhdr *eth = data;
    struct event_data event = {};
    
    /* 检查以太网头部 */
    if ((void *)(eth + 1) > data_end)
        return 0;  /* TC_ACT_OK */
    
    /* 只处理 IPv4 数据包 */
    if (eth->h_proto != bpf_htons(ETH_P_IP))
        return 0;
    
    struct iphdr *iph = (struct iphdr *)(eth + 1);
    if (parse_ipv4(iph, data_end, &event.pkt_info) < 0)
        return 0;
    
    /* 只处理 TCP 协议 */
    if (event.pkt_info.protocol != IPPROTO_TCP)
        return 0;
    
    struct tcphdr *tcph = (struct tcphdr *)((void *)iph + (iph->ihl * 4));
    if (parse_tcp(tcph, data_end, &event.pkt_info) < 0)
        return 0;
    
    /* 检查是否需要捕获此端口 */
    if (!should_capture_port(event.pkt_info.dport) && 
        !should_capture_port(event.pkt_info.sport))
        return 0;
    
    /* 填充事件数据 */
    event.timestamp = bpf_ktime_get_ns();
    event.event_type = EVENT_PACKET_CAPTURE;
    event.pkt_info.direction = DIR_EGRESS;
    event.pid = bpf_get_current_pid_tgid() >> 32;
    
    /* 复制部分载荷数据 */
    void *payload = (void *)tcph + (tcph->doff * 4);
    __u32 payload_size = data_end - payload;
    
    if (payload_size > MAX_PAYLOAD_SIZE)
        payload_size = MAX_PAYLOAD_SIZE;
    
    event.pkt_info.payload_len = payload_size;
    
    if (payload_size > 0 && payload + payload_size <= data_end) {
        bpf_probe_read_kernel(&event.payload, payload_size & (MAX_PAYLOAD_SIZE - 1), payload);
    }
    
    /* 更新统计信息 */
    update_stats(DIR_EGRESS, data_end - data);
    
    /* 将事件发送到用户空间 */
    bpf_perf_event_output(skb, &events, BPF_F_CURRENT_CPU,
                          &event, sizeof(event));
    
    return 0;  /* TC_ACT_OK - 允许数据包继续传递 */
}

char _license[] SEC("license") = "GPL";
