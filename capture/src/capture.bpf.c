#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/in.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>

#define TCP_SYN_FLAG 0x02
#define TCP_ACK_FLAG 0x10

/* 连接跟踪表 */
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __type(key, __u64);  /* sock 指针 */
    __type(value, __u32);  /* 连接状态 */
    __uint(max_entries, 10240);
} conn_track_map SEC(".maps");

/* Socket 信息表 */
struct sock_info {
    __u32 saddr;
    __u32 daddr;
    __u16 sport;
    __u16 dport;
    __u32 pid;
};

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __type(key, __u64);  /* sock 指针 */
    __type(value, struct sock_info);
    __uint(max_entries, 10240);
} sock_info_map SEC(".maps");

/* Perf 事件数组，用于向用户态发送事件 */
struct {
    __uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
    __uint(key_size, sizeof(__u32));
    __uint(value_size, sizeof(__u32));
} events SEC(".maps");

/* TCP 连接事件 */
struct tcp_connect_event {
    __u32 saddr;
    __u32 daddr;
    __u16 sport;
    __u16 dport;
    __u32 pid;
    __u64 timestamp;
};

/**
 * Hook TCP 连接建立
 */
SEC("kprobe/tcp_v4_connect")
int kprobe_tcp_v4_connect(struct pt_regs *ctx) {
    struct sock *sk = (struct sock *)PT_REGS_PARM1(ctx);
    __u64 sock_ptr = (__u64)sk;
    __u32 state = 1;  /* 连接中 */
    __u32 pid = bpf_get_current_pid_tgid() >> 32;
    
    /* 记录连接状态 */
    bpf_map_update_elem(&conn_track_map, &sock_ptr, &state, BPF_ANY);
    
    bpf_printk("TCP connect detected, sock=%llx, pid=%u\n", sock_ptr, pid);
    return 0;
}

/**
 * Hook TCP 连接完成
 */
SEC("kretprobe/tcp_v4_connect")
int kretprobe_tcp_v4_connect(struct pt_regs *ctx) {
    __u64 pid_tgid = bpf_get_current_pid_tgid();
    __u32 pid = pid_tgid >> 32;
    int ret = PT_REGS_RC(ctx);
    
    /* 检查连接是否成功 */
    if (ret != 0) {
        bpf_printk("TCP connect failed, ret=%d\n", ret);
        return 0;
    }
    
    bpf_printk("TCP connect completed, pid=%u\n", pid);
    return 0;
}

/**
 * Hook TCP 数据发送
 */
SEC("kprobe/tcp_sendmsg")
int kprobe_tcp_sendmsg(struct pt_regs *ctx) {
    struct sock *sk = (struct sock *)PT_REGS_PARM1(ctx);
    __u64 sock_ptr = (__u64)sk;
    __u32 *state;
    struct sock_info info = {0};
    struct tcp_connect_event event = {0};
    
    /* 检查是否是新连接 */
    state = bpf_map_lookup_elem(&conn_track_map, &sock_ptr);
    if (!state || *state != 1) {
        return 0;  /* 不是新连接 */
    }
    
    /* 读取 socket 信息 */
    __u16 family;
    bpf_probe_read(&family, sizeof(family), &sk->__sk_common.skc_family);
    
    if (family != AF_INET) {
        return 0;  /* 只处理 IPv4 */
    }
    
    /* 读取四元组信息 */
    bpf_probe_read(&info.saddr, sizeof(info.saddr), &sk->__sk_common.skc_rcv_saddr);
    bpf_probe_read(&info.daddr, sizeof(info.daddr), &sk->__sk_common.skc_daddr);
    bpf_probe_read(&info.sport, sizeof(info.sport), &sk->__sk_common.skc_num);
    bpf_probe_read(&info.dport, sizeof(info.dport), &sk->__sk_common.skc_dport);
    info.dport = bpf_ntohs(info.dport);
    info.pid = bpf_get_current_pid_tgid() >> 32;
    
    /* 保存 socket 信息 */
    bpf_map_update_elem(&sock_info_map, &sock_ptr, &info, BPF_ANY);
    
    /* 发送事件到用户态 */
    event.saddr = info.saddr;
    event.daddr = info.daddr;
    event.sport = info.sport;
    event.dport = info.dport;
    event.pid = info.pid;
    event.timestamp = bpf_ktime_get_ns();
    
    bpf_perf_event_output(ctx, &events, BPF_F_CURRENT_CPU, &event, sizeof(event));
    
    /* 更新状态为已处理 */
    __u32 new_state = 2;
    bpf_map_update_elem(&conn_track_map, &sock_ptr, &new_state, BPF_ANY);
    
    bpf_printk("TCP connection captured: %pI4:%u -> %pI4:%u\n",
               &info.saddr, info.sport, &info.daddr, info.dport);
    
    return 0;
}

char _license[] SEC("license") = "GPL";
