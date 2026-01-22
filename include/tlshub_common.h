/* SPDX-License-Identifier: GPL-2.0 */
/*
 * TLSHub eBPF - 共享头文件
 * 定义内核空间和用户空间共享的数据结构
 */

#ifndef __TLSHUB_COMMON_H
#define __TLSHUB_COMMON_H

#define MAX_PACKET_SIZE 1500
#define MAX_PAYLOAD_SIZE 512

/* 事件类型定义 */
enum event_type {
    EVENT_PACKET_CAPTURE = 1,    /* 数据包捕获事件 */
    EVENT_TLS_HANDSHAKE = 2,     /* TLS 握手事件 */
    EVENT_CONNECTION_NEW = 3,    /* 新连接事件 */
    EVENT_CONNECTION_CLOSE = 4,  /* 连接关闭事件 */
};

/* 数据包方向 */
enum packet_direction {
    DIR_INGRESS = 0,  /* 入方向 */
    DIR_EGRESS = 1,   /* 出方向 */
};

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

#endif /* __TLSHUB_COMMON_H */
