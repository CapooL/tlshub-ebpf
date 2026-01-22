#ifndef __PERFORMANCE_METRICS_H__
#define __PERFORMANCE_METRICS_H__

#include <linux/types.h>
#include <time.h>
#include <sys/time.h>

/* 性能指标类型 */
enum metric_type {
    METRIC_CONNECTION_LATENCY = 0,   /* 连接建立延迟 */
    METRIC_KEY_NEGOTIATION_TIME = 1, /* 密钥协商时间 */
    METRIC_THROUGHPUT = 2,            /* 吞吐量 */
    METRIC_CPU_USAGE = 3,             /* CPU使用率 */
    METRIC_MEMORY_USAGE = 4,          /* 内存使用量 */
};

/* 时间测量点 */
struct timepoint {
    struct timespec start;
    struct timespec end;
    __u64 duration_ns;  /* 持续时间（纳秒） */
};

/* 连接性能指标 */
struct connection_metrics {
    __u64 connection_id;           /* 连接ID */
    struct timepoint connection_latency;  /* 连接建立延迟 */
    struct timepoint key_negotiation;     /* 密钥协商时间 */
    __u64 bytes_sent;              /* 发送字节数 */
    __u64 bytes_received;          /* 接收字节数 */
    double throughput_mbps;        /* 吞吐量（Mbps） */
    struct timespec first_data_time;  /* 第一个数据包时间 */
    struct timespec last_data_time;   /* 最后一个数据包时间 */
};

/* 系统性能指标 */
struct system_metrics {
    double cpu_usage_percent;      /* CPU使用率（百分比） */
    __u64 memory_usage_kb;         /* 内存使用量（KB） */
    __u64 memory_rss_kb;           /* 物理内存使用量（KB） */
    __u64 memory_vms_kb;           /* 虚拟内存使用量（KB） */
    __u32 active_connections;      /* 活跃连接数 */
    struct timespec measurement_time;  /* 测量时间 */
};

/* 性能统计汇总 */
struct performance_stats {
    __u64 total_connections;       /* 总连接数 */
    __u64 successful_connections;  /* 成功连接数 */
    __u64 failed_connections;      /* 失败连接数 */
    
    /* 连接延迟统计 */
    double avg_connection_latency_ms;
    double min_connection_latency_ms;
    double max_connection_latency_ms;
    
    /* 密钥协商时间统计 */
    double avg_key_negotiation_ms;
    double min_key_negotiation_ms;
    double max_key_negotiation_ms;
    
    /* 吞吐量统计 */
    double avg_throughput_mbps;
    double max_throughput_mbps;
    __u64 total_bytes_transferred;
    
    /* 系统资源统计 */
    double avg_cpu_usage;
    double peak_cpu_usage;
    __u64 avg_memory_usage_kb;
    __u64 peak_memory_usage_kb;
};

/* 性能指标上下文 */
struct perf_metrics_ctx {
    struct connection_metrics *conn_metrics;  /* 连接指标数组 */
    __u32 max_connections;                    /* 最大连接数 */
    __u32 current_connections;                /* 当前连接数 */
    
    struct system_metrics system_metrics;     /* 系统指标 */
    struct performance_stats stats;           /* 统计汇总 */
    
    struct timespec start_time;               /* 监控开始时间 */
    int monitoring_enabled;                   /* 是否启用监控 */
};

/**
 * 初始化性能指标模块
 * @param max_connections 最大连接数
 * @return 成功返回性能指标上下文，失败返回 NULL
 */
struct perf_metrics_ctx* perf_metrics_init(__u32 max_connections);

/**
 * 清理性能指标模块
 * @param ctx 性能指标上下文
 */
void perf_metrics_cleanup(struct perf_metrics_ctx *ctx);

/**
 * 启用/禁用性能监控
 * @param ctx 性能指标上下文
 * @param enabled 是否启用
 */
void perf_metrics_set_enabled(struct perf_metrics_ctx *ctx, int enabled);

/**
 * 开始测量时间点
 * @param tp 时间点结构
 */
void perf_timepoint_start(struct timepoint *tp);

/**
 * 结束测量时间点
 * @param tp 时间点结构
 */
void perf_timepoint_end(struct timepoint *tp);

/**
 * 记录连接开始
 * @param ctx 性能指标上下文
 * @param connection_id 连接ID
 * @return 连接指标索引，失败返回 -1
 */
int perf_metrics_connection_start(struct perf_metrics_ctx *ctx, __u64 connection_id);

/**
 * 记录连接建立延迟开始
 * @param ctx 性能指标上下文
 * @param conn_index 连接索引
 */
void perf_metrics_connection_latency_start(struct perf_metrics_ctx *ctx, int conn_index);

/**
 * 记录连接建立延迟结束
 * @param ctx 性能指标上下文
 * @param conn_index 连接索引
 */
void perf_metrics_connection_latency_end(struct perf_metrics_ctx *ctx, int conn_index);

/**
 * 记录密钥协商开始
 * @param ctx 性能指标上下文
 * @param conn_index 连接索引
 */
void perf_metrics_key_negotiation_start(struct perf_metrics_ctx *ctx, int conn_index);

/**
 * 记录密钥协商结束
 * @param ctx 性能指标上下文
 * @param conn_index 连接索引
 */
void perf_metrics_key_negotiation_end(struct perf_metrics_ctx *ctx, int conn_index);

/**
 * 记录数据传输
 * @param ctx 性能指标上下文
 * @param conn_index 连接索引
 * @param bytes_sent 发送字节数
 * @param bytes_received 接收字节数
 */
void perf_metrics_record_data_transfer(struct perf_metrics_ctx *ctx, int conn_index,
                                       __u64 bytes_sent, __u64 bytes_received);

/**
 * 记录连接结束
 * @param ctx 性能指标上下文
 * @param conn_index 连接索引
 * @param success 是否成功
 */
void perf_metrics_connection_end(struct perf_metrics_ctx *ctx, int conn_index, int success);

/**
 * 更新系统性能指标
 * @param ctx 性能指标上下文
 * @return 成功返回 0，失败返回 -1
 */
int perf_metrics_update_system(struct perf_metrics_ctx *ctx);

/**
 * 计算统计汇总
 * @param ctx 性能指标上下文
 */
void perf_metrics_calculate_stats(struct perf_metrics_ctx *ctx);

/**
 * 打印性能统计报告
 * @param ctx 性能指标上下文
 */
void perf_metrics_print_report(struct perf_metrics_ctx *ctx);

/**
 * 导出性能指标到 JSON 文件
 * @param ctx 性能指标上下文
 * @param filename 文件名
 * @return 成功返回 0，失败返回 -1
 */
int perf_metrics_export_json(struct perf_metrics_ctx *ctx, const char *filename);

/**
 * 导出性能统计到 CSV 文件
 * @param ctx 性能指标上下文
 * @param filename 文件名
 * @return 成功返回 0，失败返回 -1
 */
int perf_metrics_export_csv(struct perf_metrics_ctx *ctx, const char *filename);

/**
 * 获取当前时间（纳秒精度）
 * @return 当前时间戳（纳秒）
 */
static inline __u64 perf_get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (__u64)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

/**
 * 计算两个时间点的时间差（纳秒）
 * @param start 开始时间
 * @param end 结束时间
 * @return 时间差（纳秒）
 */
static inline __u64 perf_time_diff_ns(struct timespec *start, struct timespec *end) {
    __u64 start_ns = (__u64)start->tv_sec * 1000000000ULL + start->tv_nsec;
    __u64 end_ns = (__u64)end->tv_sec * 1000000000ULL + end->tv_nsec;
    return end_ns - start_ns;
}

/**
 * 纳秒转毫秒
 * @param ns 纳秒
 * @return 毫秒
 */
static inline double perf_ns_to_ms(__u64 ns) {
    return (double)ns / 1000000.0;
}

#endif /* __PERFORMANCE_METRICS_H__ */
