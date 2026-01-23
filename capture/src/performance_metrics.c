#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include "performance_metrics.h"

/**
 * 读取 /proc/stat 获取 CPU 统计信息
 */
static int read_cpu_stat(struct cpu_stat *stat) {
    FILE *fp;
    char line[256];
    
    fp = fopen("/proc/stat", "r");
    if (!fp) {
        return -1;
    }
    
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return -1;
    }
    
    if (sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu",
               &stat->user, &stat->nice, &stat->system, &stat->idle,
               &stat->iowait, &stat->irq, &stat->softirq) != 7) {
        fclose(fp);
        return -1;
    }
    
    fclose(fp);
    return 0;
}

/**
 * 计算 CPU 使用率
 */
static double calculate_cpu_usage(struct cpu_stat *prev, struct cpu_stat *curr) {
    unsigned long long prev_total = prev->user + prev->nice + prev->system + 
                                     prev->idle + prev->iowait + prev->irq + prev->softirq;
    unsigned long long curr_total = curr->user + curr->nice + curr->system + 
                                     curr->idle + curr->iowait + curr->irq + curr->softirq;
    
    unsigned long long prev_idle = prev->idle + prev->iowait;
    unsigned long long curr_idle = curr->idle + curr->iowait;
    
    unsigned long long total_diff = curr_total - prev_total;
    unsigned long long idle_diff = curr_idle - prev_idle;
    
    if (total_diff == 0) {
        return 0.0;
    }
    
    return (double)(total_diff - idle_diff) * 100.0 / total_diff;
}

/**
 * 读取进程内存使用情况
 */
static int read_memory_usage(__u64 *rss_kb, __u64 *vms_kb) {
    FILE *fp;
    char line[256];
    
    fp = fopen("/proc/self/status", "r");
    if (!fp) {
        return -1;
    }
    
    *rss_kb = 0;
    *vms_kb = 0;
    
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line + 6, "%llu", rss_kb);
        } else if (strncmp(line, "VmSize:", 7) == 0) {
            sscanf(line + 7, "%llu", vms_kb);
        }
    }
    
    fclose(fp);
    return 0;
}

/**
 * 初始化性能指标模块
 */
struct perf_metrics_ctx* perf_metrics_init(__u32 max_connections) {
    struct perf_metrics_ctx *ctx;
    
    ctx = (struct perf_metrics_ctx *)calloc(1, sizeof(struct perf_metrics_ctx));
    if (!ctx) {
        fprintf(stderr, "Failed to allocate performance metrics context\n");
        return NULL;
    }
    
    ctx->max_connections = max_connections;
    ctx->current_connections = 0;
    ctx->monitoring_enabled = 1;
    
    /* 分配连接指标数组 */
    ctx->conn_metrics = (struct connection_metrics *)calloc(max_connections, 
                                                            sizeof(struct connection_metrics));
    if (!ctx->conn_metrics) {
        fprintf(stderr, "Failed to allocate connection metrics array\n");
        free(ctx);
        return NULL;
    }
    
    /* 初始化统计数据 */
    memset(&ctx->stats, 0, sizeof(struct performance_stats));
    ctx->stats.min_connection_latency_ms = 999999.0;
    ctx->stats.min_key_negotiation_ms = 999999.0;
    
    /* 初始化 CPU 监控状态 */
    memset(&ctx->prev_cpu_stat, 0, sizeof(struct cpu_stat));
    ctx->cpu_first_call = 1;
    
    /* 记录开始时间 */
    clock_gettime(CLOCK_MONOTONIC, &ctx->start_time);
    
    printf("Performance metrics module initialized (max connections: %u)\n", max_connections);
    
    return ctx;
}

/**
 * 清理性能指标模块
 */
void perf_metrics_cleanup(struct perf_metrics_ctx *ctx) {
    if (!ctx) {
        return;
    }
    
    if (ctx->conn_metrics) {
        free(ctx->conn_metrics);
    }
    
    free(ctx);
}

/**
 * 启用/禁用性能监控
 */
void perf_metrics_set_enabled(struct perf_metrics_ctx *ctx, int enabled) {
    if (ctx) {
        ctx->monitoring_enabled = enabled;
        printf("Performance monitoring %s\n", enabled ? "enabled" : "disabled");
    }
}

/**
 * 开始测量时间点
 */
void perf_timepoint_start(struct timepoint *tp) {
    if (tp) {
        clock_gettime(CLOCK_MONOTONIC, &tp->start);
    }
}

/**
 * 结束测量时间点
 */
void perf_timepoint_end(struct timepoint *tp) {
    if (tp) {
        clock_gettime(CLOCK_MONOTONIC, &tp->end);
        tp->duration_ns = perf_time_diff_ns(&tp->start, &tp->end);
    }
}

/**
 * 记录连接开始
 */
int perf_metrics_connection_start(struct perf_metrics_ctx *ctx, __u64 connection_id) {
    int index;
    
    if (!ctx || !ctx->monitoring_enabled) {
        return -1;
    }
    
    if (ctx->current_connections >= ctx->max_connections) {
        fprintf(stderr, "Warning: Maximum connections reached, cannot track new connection\n");
        return -1;
    }
    
    index = ctx->current_connections++;
    
    /* 初始化连接指标 */
    memset(&ctx->conn_metrics[index], 0, sizeof(struct connection_metrics));
    ctx->conn_metrics[index].connection_id = connection_id;
    
    ctx->stats.total_connections++;
    ctx->system_metrics.active_connections++;
    
    return index;
}

/**
 * 记录连接建立延迟开始
 */
void perf_metrics_connection_latency_start(struct perf_metrics_ctx *ctx, int conn_index) {
    if (!ctx || !ctx->monitoring_enabled || conn_index < 0 || 
        conn_index >= (int)ctx->current_connections) {
        return;
    }
    
    perf_timepoint_start(&ctx->conn_metrics[conn_index].connection_latency);
}

/**
 * 记录连接建立延迟结束
 */
void perf_metrics_connection_latency_end(struct perf_metrics_ctx *ctx, int conn_index) {
    if (!ctx || !ctx->monitoring_enabled || conn_index < 0 || 
        conn_index >= (int)ctx->current_connections) {
        return;
    }
    
    perf_timepoint_end(&ctx->conn_metrics[conn_index].connection_latency);
}

/**
 * 记录密钥协商开始
 */
void perf_metrics_key_negotiation_start(struct perf_metrics_ctx *ctx, int conn_index) {
    if (!ctx || !ctx->monitoring_enabled || conn_index < 0 || 
        conn_index >= (int)ctx->current_connections) {
        return;
    }
    
    perf_timepoint_start(&ctx->conn_metrics[conn_index].key_negotiation);
}

/**
 * 记录密钥协商结束
 */
void perf_metrics_key_negotiation_end(struct perf_metrics_ctx *ctx, int conn_index) {
    if (!ctx || !ctx->monitoring_enabled || conn_index < 0 || 
        conn_index >= (int)ctx->current_connections) {
        return;
    }
    
    perf_timepoint_end(&ctx->conn_metrics[conn_index].key_negotiation);
}

/**
 * 记录数据传输
 */
void perf_metrics_record_data_transfer(struct perf_metrics_ctx *ctx, int conn_index,
                                       __u64 bytes_sent, __u64 bytes_received) {
    struct connection_metrics *cm;
    
    if (!ctx || !ctx->monitoring_enabled || conn_index < 0 || 
        conn_index >= (int)ctx->current_connections) {
        return;
    }
    
    cm = &ctx->conn_metrics[conn_index];
    
    /* 记录第一个数据包时间 */
    if (cm->bytes_sent == 0 && cm->bytes_received == 0) {
        clock_gettime(CLOCK_MONOTONIC, &cm->first_data_time);
    }
    
    cm->bytes_sent += bytes_sent;
    cm->bytes_received += bytes_received;
    
    /* 更新最后数据包时间 */
    clock_gettime(CLOCK_MONOTONIC, &cm->last_data_time);
    
    /* 计算吞吐量（仅在有足够时间间隔时计算） */
    __u64 total_bytes = cm->bytes_sent + cm->bytes_received;
    __u64 time_diff_ns = perf_time_diff_ns(&cm->first_data_time, &cm->last_data_time);
    
    /* 要求至少有 100ms 的时间间隔以获得有意义的吞吐量测量 */
    if (time_diff_ns > 100000000ULL) {  /* 100ms in nanoseconds */
        /* 吞吐量 (Mbps) = (字节数 * 8) / (时间(秒) * 1000000) */
        double time_seconds = (double)time_diff_ns / 1000000000.0;
        cm->throughput_mbps = ((double)total_bytes * 8.0) / (time_seconds * 1000000.0);
    }
}

/**
 * 记录连接结束
 */
void perf_metrics_connection_end(struct perf_metrics_ctx *ctx, int conn_index, int success) {
    if (!ctx || !ctx->monitoring_enabled || conn_index < 0 || 
        conn_index >= (int)ctx->current_connections) {
        return;
    }
    
    if (success) {
        ctx->stats.successful_connections++;
    } else {
        ctx->stats.failed_connections++;
    }
    
    if (ctx->system_metrics.active_connections > 0) {
        ctx->system_metrics.active_connections--;
    }
}

/**
 * 更新系统性能指标
 */
int perf_metrics_update_system(struct perf_metrics_ctx *ctx) {
    struct cpu_stat curr_cpu_stat;
    __u64 rss_kb, vms_kb;
    
    if (!ctx || !ctx->monitoring_enabled) {
        return -1;
    }
    
    /* 读取 CPU 统计 */
    if (read_cpu_stat(&curr_cpu_stat) < 0) {
        fprintf(stderr, "Failed to read CPU stats\n");
        return -1;
    }
    
    /* 计算 CPU 使用率（第一次调用时跳过） */
    if (!ctx->cpu_first_call) {
        ctx->system_metrics.cpu_usage_percent = calculate_cpu_usage(&ctx->prev_cpu_stat, &curr_cpu_stat);
        
        /* 更新峰值 CPU */
        if (ctx->system_metrics.cpu_usage_percent > ctx->stats.peak_cpu_usage) {
            ctx->stats.peak_cpu_usage = ctx->system_metrics.cpu_usage_percent;
        }
    } else {
        ctx->cpu_first_call = 0;
        ctx->system_metrics.cpu_usage_percent = 0.0;
    }
    ctx->prev_cpu_stat = curr_cpu_stat;
    
    /* 读取内存使用情况 */
    if (read_memory_usage(&rss_kb, &vms_kb) < 0) {
        fprintf(stderr, "Failed to read memory usage\n");
        return -1;
    }
    
    ctx->system_metrics.memory_rss_kb = rss_kb;
    ctx->system_metrics.memory_vms_kb = vms_kb;
    ctx->system_metrics.memory_usage_kb = rss_kb;  /* 使用 RSS 作为主要内存指标 */
    
    /* 更新峰值内存 */
    if (rss_kb > ctx->stats.peak_memory_usage_kb) {
        ctx->stats.peak_memory_usage_kb = rss_kb;
    }
    
    /* 记录测量时间 */
    clock_gettime(CLOCK_MONOTONIC, &ctx->system_metrics.measurement_time);
    
    return 0;
}

/**
 * 计算统计汇总
 */
void perf_metrics_calculate_stats(struct perf_metrics_ctx *ctx) {
    __u32 i;
    __u32 latency_count = 0;
    __u32 negotiation_count = 0;
    double total_connection_latency = 0.0;
    double total_key_negotiation = 0.0;
    double total_throughput = 0.0;
    __u32 throughput_samples = 0;
    
    if (!ctx) {
        return;
    }
    
    /* 重置 min/max 值 */
    ctx->stats.min_connection_latency_ms = 999999.0;
    ctx->stats.max_connection_latency_ms = 0.0;
    ctx->stats.min_key_negotiation_ms = 999999.0;
    ctx->stats.max_key_negotiation_ms = 0.0;
    ctx->stats.max_throughput_mbps = 0.0;
    ctx->stats.total_bytes_transferred = 0;
    
    /* 遍历所有连接，计算统计数据 */
    for (i = 0; i < ctx->current_connections; i++) {
        struct connection_metrics *cm = &ctx->conn_metrics[i];
        double latency_ms, negotiation_ms;
        
        /* 连接延迟统计 */
        if (cm->connection_latency.duration_ns > 0) {
            latency_ms = perf_ns_to_ms(cm->connection_latency.duration_ns);
            total_connection_latency += latency_ms;
            latency_count++;
            
            if (latency_ms < ctx->stats.min_connection_latency_ms) {
                ctx->stats.min_connection_latency_ms = latency_ms;
            }
            if (latency_ms > ctx->stats.max_connection_latency_ms) {
                ctx->stats.max_connection_latency_ms = latency_ms;
            }
        }
        
        /* 密钥协商时间统计 */
        if (cm->key_negotiation.duration_ns > 0) {
            negotiation_ms = perf_ns_to_ms(cm->key_negotiation.duration_ns);
            total_key_negotiation += negotiation_ms;
            negotiation_count++;
            
            if (negotiation_ms < ctx->stats.min_key_negotiation_ms) {
                ctx->stats.min_key_negotiation_ms = negotiation_ms;
            }
            if (negotiation_ms > ctx->stats.max_key_negotiation_ms) {
                ctx->stats.max_key_negotiation_ms = negotiation_ms;
            }
        }
        
        /* 吞吐量统计 */
        if (cm->throughput_mbps > 0.0) {
            total_throughput += cm->throughput_mbps;
            throughput_samples++;
            
            if (cm->throughput_mbps > ctx->stats.max_throughput_mbps) {
                ctx->stats.max_throughput_mbps = cm->throughput_mbps;
            }
        }
        
        /* 累计传输字节数 */
        ctx->stats.total_bytes_transferred += cm->bytes_sent + cm->bytes_received;
    }
    
    /* 计算平均值 - 仅使用有效测量 */
    if (latency_count > 0) {
        ctx->stats.avg_connection_latency_ms = total_connection_latency / latency_count;
    } else {
        /* 没有有效测量时，重置 min 值为 0 */
        ctx->stats.min_connection_latency_ms = 0.0;
    }
    
    if (negotiation_count > 0) {
        ctx->stats.avg_key_negotiation_ms = total_key_negotiation / negotiation_count;
    } else {
        /* 没有有效测量时，重置 min 值为 0 */
        ctx->stats.min_key_negotiation_ms = 0.0;
    }
    
    if (throughput_samples > 0) {
        ctx->stats.avg_throughput_mbps = total_throughput / throughput_samples;
    }
    
    /* CPU 和内存统计 */
    ctx->stats.avg_cpu_usage = ctx->system_metrics.cpu_usage_percent;
    /* peak_cpu_usage 已在 perf_metrics_update_system 中更新 */
    ctx->stats.avg_memory_usage_kb = ctx->system_metrics.memory_usage_kb;
    /* peak_memory_usage_kb 已在 perf_metrics_update_system 中更新 */
}

/**
 * 打印性能统计报告
 */
void perf_metrics_print_report(struct perf_metrics_ctx *ctx) {
    if (!ctx) {
        return;
    }
    
    /* 先计算统计数据 */
    perf_metrics_calculate_stats(ctx);
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║            TLShub 性能指标统计报告                              ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    /* 连接统计 */
    printf("【连接统计】\n");
    printf("  总连接数:       %llu\n", ctx->stats.total_connections);
    printf("  成功连接:       %llu\n", ctx->stats.successful_connections);
    printf("  失败连接:       %llu\n", ctx->stats.failed_connections);
    printf("  活跃连接:       %u\n", ctx->system_metrics.active_connections);
    printf("\n");
    
    /* 连接延迟 */
    printf("【连接建立延迟】\n");
    printf("  平均延迟:       %.3f ms\n", ctx->stats.avg_connection_latency_ms);
    printf("  最小延迟:       %.3f ms\n", ctx->stats.min_connection_latency_ms);
    printf("  最大延迟:       %.3f ms\n", ctx->stats.max_connection_latency_ms);
    printf("\n");
    
    /* 密钥协商时间 */
    printf("【密钥协商时间】\n");
    printf("  平均时间:       %.3f ms\n", ctx->stats.avg_key_negotiation_ms);
    printf("  最小时间:       %.3f ms\n", ctx->stats.min_key_negotiation_ms);
    printf("  最大时间:       %.3f ms\n", ctx->stats.max_key_negotiation_ms);
    printf("\n");
    
    /* 吞吐量 */
    printf("【吞吐量统计】\n");
    printf("  平均吞吐量:     %.2f Mbps\n", ctx->stats.avg_throughput_mbps);
    printf("  峰值吞吐量:     %.2f Mbps\n", ctx->stats.max_throughput_mbps);
    printf("  总传输字节:     %llu bytes (%.2f MB)\n", 
           ctx->stats.total_bytes_transferred,
           (double)ctx->stats.total_bytes_transferred / (1024.0 * 1024.0));
    printf("\n");
    
    /* CPU 使用率 */
    printf("【CPU 使用率】\n");
    printf("  当前 CPU:       %.2f%%\n", ctx->stats.avg_cpu_usage);
    printf("  峰值 CPU:       %.2f%%\n", ctx->stats.peak_cpu_usage);
    printf("\n");
    
    /* 内存使用量 */
    printf("【内存使用量】\n");
    printf("  当前内存 (RSS): %llu KB (%.2f MB)\n", 
           ctx->stats.avg_memory_usage_kb,
           (double)ctx->stats.avg_memory_usage_kb / 1024.0);
    printf("  虚拟内存 (VMS): %llu KB (%.2f MB)\n",
           ctx->system_metrics.memory_vms_kb,
           (double)ctx->system_metrics.memory_vms_kb / 1024.0);
    printf("\n");
}

/**
 * 导出性能指标到 JSON 文件
 */
int perf_metrics_export_json(struct perf_metrics_ctx *ctx, const char *filename) {
    FILE *fp;
    __u32 i;
    
    if (!ctx || !filename) {
        return -1;
    }
    
    fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "Failed to open file %s for writing: %s\n", filename, strerror(errno));
        return -1;
    }
    
    /* 先计算统计数据 */
    perf_metrics_calculate_stats(ctx);
    
    fprintf(fp, "{\n");
    fprintf(fp, "  \"timestamp\": %ld,\n", time(NULL));
    fprintf(fp, "  \"summary\": {\n");
    fprintf(fp, "    \"total_connections\": %llu,\n", ctx->stats.total_connections);
    fprintf(fp, "    \"successful_connections\": %llu,\n", ctx->stats.successful_connections);
    fprintf(fp, "    \"failed_connections\": %llu,\n", ctx->stats.failed_connections);
    fprintf(fp, "    \"active_connections\": %u,\n", ctx->system_metrics.active_connections);
    fprintf(fp, "    \"avg_connection_latency_ms\": %.3f,\n", ctx->stats.avg_connection_latency_ms);
    fprintf(fp, "    \"min_connection_latency_ms\": %.3f,\n", ctx->stats.min_connection_latency_ms);
    fprintf(fp, "    \"max_connection_latency_ms\": %.3f,\n", ctx->stats.max_connection_latency_ms);
    fprintf(fp, "    \"avg_key_negotiation_ms\": %.3f,\n", ctx->stats.avg_key_negotiation_ms);
    fprintf(fp, "    \"min_key_negotiation_ms\": %.3f,\n", ctx->stats.min_key_negotiation_ms);
    fprintf(fp, "    \"max_key_negotiation_ms\": %.3f,\n", ctx->stats.max_key_negotiation_ms);
    fprintf(fp, "    \"avg_throughput_mbps\": %.2f,\n", ctx->stats.avg_throughput_mbps);
    fprintf(fp, "    \"max_throughput_mbps\": %.2f,\n", ctx->stats.max_throughput_mbps);
    fprintf(fp, "    \"total_bytes_transferred\": %llu,\n", ctx->stats.total_bytes_transferred);
    fprintf(fp, "    \"cpu_usage_percent\": %.2f,\n", ctx->stats.avg_cpu_usage);
    fprintf(fp, "    \"memory_usage_kb\": %llu\n", ctx->stats.avg_memory_usage_kb);
    fprintf(fp, "  },\n");
    
    fprintf(fp, "  \"connections\": [\n");
    for (i = 0; i < ctx->current_connections; i++) {
        struct connection_metrics *cm = &ctx->conn_metrics[i];
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"connection_id\": %llu,\n", cm->connection_id);
        fprintf(fp, "      \"connection_latency_ms\": %.3f,\n", 
                perf_ns_to_ms(cm->connection_latency.duration_ns));
        fprintf(fp, "      \"key_negotiation_ms\": %.3f,\n", 
                perf_ns_to_ms(cm->key_negotiation.duration_ns));
        fprintf(fp, "      \"bytes_sent\": %llu,\n", cm->bytes_sent);
        fprintf(fp, "      \"bytes_received\": %llu,\n", cm->bytes_received);
        fprintf(fp, "      \"throughput_mbps\": %.2f\n", cm->throughput_mbps);
        fprintf(fp, "    }%s\n", (i < ctx->current_connections - 1) ? "," : "");
    }
    fprintf(fp, "  ]\n");
    fprintf(fp, "}\n");
    
    fclose(fp);
    printf("Performance metrics exported to %s\n", filename);
    
    return 0;
}

/**
 * 导出性能统计到 CSV 文件
 */
int perf_metrics_export_csv(struct perf_metrics_ctx *ctx, const char *filename) {
    FILE *fp;
    __u32 i;
    
    if (!ctx || !filename) {
        return -1;
    }
    
    fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "Failed to open file %s for writing: %s\n", filename, strerror(errno));
        return -1;
    }
    
    /* CSV 头部 */
    fprintf(fp, "connection_id,connection_latency_ms,key_negotiation_ms,bytes_sent,bytes_received,throughput_mbps\n");
    
    /* 数据行 */
    for (i = 0; i < ctx->current_connections; i++) {
        struct connection_metrics *cm = &ctx->conn_metrics[i];
        fprintf(fp, "%llu,%.3f,%.3f,%llu,%llu,%.2f\n",
                cm->connection_id,
                perf_ns_to_ms(cm->connection_latency.duration_ns),
                perf_ns_to_ms(cm->key_negotiation.duration_ns),
                cm->bytes_sent,
                cm->bytes_received,
                cm->throughput_mbps);
    }
    
    fclose(fp);
    printf("Performance metrics exported to %s\n", filename);
    
    return 0;
}
