#include "bench_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/time.h>

// Calculate time difference in milliseconds
double timespec_diff_ms(struct timespec *start, struct timespec *end) {
    double diff = (end->tv_sec - start->tv_sec) * 1000.0;
    diff += (end->tv_nsec - start->tv_nsec) / 1000000.0;
    return diff;
}

// Initialize configuration with defaults
void init_config(bench_config_t *config) {
    memset(config, 0, sizeof(bench_config_t));
    config->mode = MODE_CLIENT;
    strcpy(config->target_ip, "127.0.0.1");
    config->target_port = DEFAULT_TARGET_PORT;
    config->listen_port = DEFAULT_LISTEN_PORT;
    config->data_size = DEFAULT_DATA_SIZE;
    config->concurrency = DEFAULT_CONCURRENCY;
    config->total_connections = DEFAULT_TOTAL_CONNECTIONS;
    config->verbose = false;
    snprintf(config->output_json, sizeof(config->output_json), 
             "bench_metrics_%ld.json", (long)time(NULL));
    snprintf(config->output_csv, sizeof(config->output_csv),
             "bench_metrics_%ld.csv", (long)time(NULL));
}

// Print configuration
void print_config(const bench_config_t *config) {
    printf("=== TLShub Benchmark Configuration ===\n");
    printf("Mode: %s\n", 
           config->mode == MODE_CLIENT ? "Client" : "Server");
    if (config->mode == MODE_CLIENT) {
        printf("Target: %s:%u\n", config->target_ip, config->target_port);
        printf("Data Size: %zu bytes\n", config->data_size);
        printf("Concurrency: %d (sequential execution)\n", config->concurrency);
        printf("Total Connections: %d\n", config->total_connections);
    }
    if (config->mode == MODE_SERVER) {
        printf("Listen Port: %u\n", config->listen_port);
    }
    printf("Output JSON: %s\n", config->output_json);
    printf("Output CSV: %s\n", config->output_csv);
    printf("=====================================\n\n");
}

// Initialize metrics
void init_metrics(global_metrics_t *metrics) {
    memset(metrics, 0, sizeof(global_metrics_t));
    clock_gettime(CLOCK_MONOTONIC, &metrics->test_start_time);
    metrics->min_connection_latency_ms = 999999.0;
}

// Update metrics with connection statistics
void update_metrics(global_metrics_t *metrics, conn_stats_t *conn_stats) {
    metrics->total_connections++;
    
    if (conn_stats->state == CONN_STATE_CLOSED) {
        metrics->successful_connections++;
        metrics->total_bytes_sent += conn_stats->bytes_sent;
        metrics->total_bytes_received += conn_stats->bytes_received;
        
        // Update latency stats
        if (conn_stats->connection_latency_ms > 0) {
            metrics->avg_connection_latency_ms += conn_stats->connection_latency_ms;
            if (conn_stats->connection_latency_ms < metrics->min_connection_latency_ms) {
                metrics->min_connection_latency_ms = conn_stats->connection_latency_ms;
            }
            if (conn_stats->connection_latency_ms > metrics->max_connection_latency_ms) {
                metrics->max_connection_latency_ms = conn_stats->connection_latency_ms;
            }
        }
        
        // Update throughput
        if (conn_stats->throughput_mbps > 0) {
            metrics->avg_throughput_mbps += conn_stats->throughput_mbps;
        }
    } else if (conn_stats->state == CONN_STATE_FAILED) {
        metrics->failed_connections++;
    } else if (conn_stats->state == CONN_STATE_TIMEOUT) {
        metrics->timeout_connections++;
    }
}

// Finalize metrics calculations
void finalize_metrics(global_metrics_t *metrics) {
    clock_gettime(CLOCK_MONOTONIC, &metrics->test_end_time);
    metrics->total_test_duration_sec = timespec_diff_ms(&metrics->test_start_time, 
                                                         &metrics->test_end_time) / 1000.0;
    
    if (metrics->successful_connections > 0) {
        metrics->avg_connection_latency_ms /= metrics->successful_connections;
        metrics->avg_throughput_mbps /= metrics->successful_connections;
    }
    
    // Get system resource usage
    get_cpu_usage(&metrics->cpu_usage_percent);
    get_memory_usage(&metrics->memory_usage_mb);
}

// Export metrics to JSON
int export_metrics_json(const char *filename, const bench_config_t *config,
                        const global_metrics_t *metrics, conn_stats_t *conn_stats_array,
                        int num_connections) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        perror("Failed to open JSON file");
        return -1;
    }
    
    fprintf(fp, "{\n");
    fprintf(fp, "  \"test_configuration\": {\n");
    fprintf(fp, "    \"mode\": \"%s\",\n", 
            config->mode == MODE_CLIENT ? "client" :
            config->mode == MODE_SERVER ? "server" : "both");
    fprintf(fp, "    \"target_ip\": \"%s\",\n", config->target_ip);
    fprintf(fp, "    \"target_port\": %u,\n", config->target_port);
    fprintf(fp, "    \"listen_port\": %u,\n", config->listen_port);
    fprintf(fp, "    \"data_size\": %zu,\n", config->data_size);
    fprintf(fp, "    \"concurrency\": %d,\n", config->concurrency);
    fprintf(fp, "    \"total_connections\": %d\n", config->total_connections);
    fprintf(fp, "  },\n");
    
    fprintf(fp, "  \"summary_metrics\": {\n");
    fprintf(fp, "    \"total_connections\": %d,\n", metrics->total_connections);
    fprintf(fp, "    \"successful_connections\": %d,\n", metrics->successful_connections);
    fprintf(fp, "    \"failed_connections\": %d,\n", metrics->failed_connections);
    fprintf(fp, "    \"timeout_connections\": %d,\n", metrics->timeout_connections);
    fprintf(fp, "    \"total_bytes_sent\": %lu,\n", metrics->total_bytes_sent);
    fprintf(fp, "    \"total_bytes_received\": %lu,\n", metrics->total_bytes_received);
    fprintf(fp, "    \"avg_connection_latency_ms\": %.3f,\n", metrics->avg_connection_latency_ms);
    fprintf(fp, "    \"min_connection_latency_ms\": %.3f,\n", metrics->min_connection_latency_ms);
    fprintf(fp, "    \"max_connection_latency_ms\": %.3f,\n", metrics->max_connection_latency_ms);
    fprintf(fp, "    \"avg_throughput_mbps\": %.3f,\n", metrics->avg_throughput_mbps);
    fprintf(fp, "    \"total_test_duration_sec\": %.3f,\n", metrics->total_test_duration_sec);
    fprintf(fp, "    \"cpu_usage_percent\": %.2f,\n", metrics->cpu_usage_percent);
    fprintf(fp, "    \"memory_usage_mb\": %.2f\n", metrics->memory_usage_mb);
    fprintf(fp, "  },\n");
    
    fprintf(fp, "  \"connection_details\": [\n");
    for (int i = 0; i < num_connections; i++) {
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"conn_id\": %d,\n", conn_stats_array[i].conn_id);
        fprintf(fp, "      \"state\": %d,\n", conn_stats_array[i].state);
        fprintf(fp, "      \"bytes_sent\": %lu,\n", conn_stats_array[i].bytes_sent);
        fprintf(fp, "      \"bytes_received\": %lu,\n", conn_stats_array[i].bytes_received);
        fprintf(fp, "      \"connection_latency_ms\": %.3f,\n", conn_stats_array[i].connection_latency_ms);
        fprintf(fp, "      \"data_transfer_time_ms\": %.3f,\n", conn_stats_array[i].data_transfer_time_ms);
        fprintf(fp, "      \"throughput_mbps\": %.3f\n", conn_stats_array[i].throughput_mbps);
        fprintf(fp, "    }%s\n", i < num_connections - 1 ? "," : "");
    }
    fprintf(fp, "  ]\n");
    fprintf(fp, "}\n");
    
    fclose(fp);
    return 0;
}

// Export metrics to CSV
int export_metrics_csv(const char *filename, const bench_config_t *config,
                       const global_metrics_t *metrics, conn_stats_t *conn_stats_array,
                       int num_connections) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        perror("Failed to open CSV file");
        return -1;
    }
    
    // Write summary section
    fprintf(fp, "# TLShub Benchmark Summary\n");
    fprintf(fp, "# Mode,%s\n", 
            config->mode == MODE_CLIENT ? "client" :
            config->mode == MODE_SERVER ? "server" : "both");
    fprintf(fp, "# Target,%s:%u\n", config->target_ip, config->target_port);
    fprintf(fp, "# Data Size,%zu\n", config->data_size);
    fprintf(fp, "# Concurrency,%d\n", config->concurrency);
    fprintf(fp, "# Total Connections,%d\n\n", config->total_connections);
    
    fprintf(fp, "Metric,Value\n");
    fprintf(fp, "Total Connections,%d\n", metrics->total_connections);
    fprintf(fp, "Successful Connections,%d\n", metrics->successful_connections);
    fprintf(fp, "Failed Connections,%d\n", metrics->failed_connections);
    fprintf(fp, "Timeout Connections,%d\n", metrics->timeout_connections);
    fprintf(fp, "Total Bytes Sent,%lu\n", metrics->total_bytes_sent);
    fprintf(fp, "Total Bytes Received,%lu\n", metrics->total_bytes_received);
    fprintf(fp, "Avg Connection Latency (ms),%.3f\n", metrics->avg_connection_latency_ms);
    fprintf(fp, "Min Connection Latency (ms),%.3f\n", metrics->min_connection_latency_ms);
    fprintf(fp, "Max Connection Latency (ms),%.3f\n", metrics->max_connection_latency_ms);
    fprintf(fp, "Avg Throughput (Mbps),%.3f\n", metrics->avg_throughput_mbps);
    fprintf(fp, "Total Test Duration (sec),%.3f\n", metrics->total_test_duration_sec);
    fprintf(fp, "CPU Usage (%%),%.2f\n", metrics->cpu_usage_percent);
    fprintf(fp, "Memory Usage (MB),%.2f\n\n", metrics->memory_usage_mb);
    
    // Write detailed connection data
    fprintf(fp, "# Connection Details\n");
    fprintf(fp, "Connection ID,State,Bytes Sent,Bytes Received,");
    fprintf(fp, "Connection Latency (ms),Data Transfer Time (ms),Throughput (Mbps)\n");
    
    for (int i = 0; i < num_connections; i++) {
        fprintf(fp, "%d,%d,%lu,%lu,%.3f,%.3f,%.3f\n",
                conn_stats_array[i].conn_id,
                conn_stats_array[i].state,
                conn_stats_array[i].bytes_sent,
                conn_stats_array[i].bytes_received,
                conn_stats_array[i].connection_latency_ms,
                conn_stats_array[i].data_transfer_time_ms,
                conn_stats_array[i].throughput_mbps);
    }
    
    fclose(fp);
    return 0;
}

// Get CPU usage percentage
void get_cpu_usage(double *cpu_percent) {
    static unsigned long long prev_user = 0, prev_nice = 0, prev_system = 0;
    static unsigned long long prev_idle = 0, prev_iowait = 0, prev_irq = 0;
    static unsigned long long prev_softirq = 0, prev_steal = 0;
    
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) {
        *cpu_percent = 0.0;
        return;
    }
    
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
    char cpu_label[16];
    
    if (fscanf(fp, "%s %llu %llu %llu %llu %llu %llu %llu %llu",
               cpu_label, &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) != 9) {
        fclose(fp);
        *cpu_percent = 0.0;
        return;
    }
    fclose(fp);
    
    if (prev_user == 0) {
        // First call, just store values
        prev_user = user;
        prev_nice = nice;
        prev_system = system;
        prev_idle = idle;
        prev_iowait = iowait;
        prev_irq = irq;
        prev_softirq = softirq;
        prev_steal = steal;
        *cpu_percent = 0.0;
        return;
    }
    
    unsigned long long prev_total = prev_user + prev_nice + prev_system + prev_idle +
                                    prev_iowait + prev_irq + prev_softirq + prev_steal;
    unsigned long long total = user + nice + system + idle + iowait + irq + softirq + steal;
    
    unsigned long long prev_idle_total = prev_idle + prev_iowait;
    unsigned long long idle_total = idle + iowait;
    
    unsigned long long total_diff = total - prev_total;
    unsigned long long idle_diff = idle_total - prev_idle_total;
    
    if (total_diff > 0) {
        *cpu_percent = ((double)(total_diff - idle_diff) / (double)total_diff) * 100.0;
    } else {
        *cpu_percent = 0.0;
    }
    
    // Update previous values
    prev_user = user;
    prev_nice = nice;
    prev_system = system;
    prev_idle = idle;
    prev_iowait = iowait;
    prev_irq = irq;
    prev_softirq = softirq;
    prev_steal = steal;
}

// Get memory usage in MB
void get_memory_usage(double *memory_mb) {
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        // ru_maxrss is in kilobytes on Linux
        *memory_mb = (double)usage.ru_maxrss / 1024.0;
    } else {
        *memory_mb = 0.0;
    }
}
