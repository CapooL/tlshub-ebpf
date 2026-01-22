#ifndef __CAPTURE_H__
#define __CAPTURE_H__

#include <linux/types.h>

/* 模式选择 */
enum key_provider_mode {
    MODE_TLSHUB = 0,    /* 使用 TLSHub 进行密钥协商 */
    MODE_OPENSSL = 1,   /* 使用 OpenSSL 进行密钥协商 */
    MODE_BORINGSSL = 2, /* 使用 BoringSSL 进行密钥协商 */
};

/* 四元组信息 */
struct flow_tuple {
    __u32 saddr;        /* 源IP地址 */
    __u32 daddr;        /* 目的IP地址 */
    __u16 sport;        /* 源端口 */
    __u16 dport;        /* 目的端口 */
};

/* Socket 连接信息 */
struct connection_info {
    struct flow_tuple tuple;
    __u32 pid;          /* 进程ID */
    __u64 timestamp;    /* 捕获时间戳 */
    __u8 state;         /* 连接状态 */
};

/* TLS 密钥信息 */
struct tls_key_info {
    __u8 key[32];       /* TLS密钥 */
    __u8 iv[16];        /* 初始化向量 */
    __u32 key_len;      /* 密钥长度 */
    __u32 iv_len;       /* IV长度 */
};

/* Netlink 消息类型 */
enum {
    NLMSG_TYPE_FETCH_KEY = 1,
    NLMSG_TYPE_HANDSHAKE = 2,
    NLMSG_TYPE_CONFIG = 3,
    NLMSG_TYPE_QUERY = 4,
};

/* 配置信息 */
struct capture_config {
    enum key_provider_mode mode;
    char pod_node_config_path[256];
};

#endif /* __CAPTURE_H__ */
