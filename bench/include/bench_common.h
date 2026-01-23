#ifndef BENCH_COMMON_H
#define BENCH_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// Default configuration values
#define DEFAULT_DATA_SIZE 1024
#define DEFAULT_CONCURRENCY 1
#define DEFAULT_TOTAL_CONNECTIONS 10
#define DEFAULT_TARGET_PORT 8080
#define DEFAULT_LISTEN_PORT 9090
#define MAX_CONNECTIONS 10000
#define BUFFER_SIZE 65536

// Test modes
typedef enum {
    MODE_CLIENT = 0,
    MODE_SERVER = 1,
    MODE_BOTH = 2
} bench_mode_t;

// Connection state
typedef enum {
    CONN_STATE_INIT = 0,
    CONN_STATE_CONNECTING = 1,
    CONN_STATE_CONNECTED = 2,
    CONN_STATE_SENDING = 3,
    CONN_STATE_RECEIVING = 4,
    CONN_STATE_CLOSING = 5,
    CONN_STATE_CLOSED = 6,
    CONN_STATE_FAILED = 7,
    CONN_STATE_TIMEOUT = 8
} conn_state_t;

// Configuration structure
typedef struct {
    bench_mode_t mode;
    char target_ip[256];
    uint16_t target_port;
    uint16_t listen_port;
    size_t data_size;
    int concurrency;
    int total_connections;
    bool verbose;
    char output_json[512];
    char output_csv[512];
} bench_config_t;

// Connection statistics
typedef struct {
    int conn_id;
    conn_state_t state;
    struct timespec start_time;
    struct timespec connect_time;
    struct timespec first_byte_time;
    struct timespec close_time;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    double connection_latency_ms;
    double data_transfer_time_ms;
    double throughput_mbps;
} conn_stats_t;

// Global metrics
typedef struct {
    int total_connections;
    int successful_connections;
    int failed_connections;
    int timeout_connections;
    uint64_t total_bytes_sent;
    uint64_t total_bytes_received;
    double avg_connection_latency_ms;
    double min_connection_latency_ms;
    double max_connection_latency_ms;
    double avg_throughput_mbps;
    double total_test_duration_sec;
    struct timespec test_start_time;
    struct timespec test_end_time;
    double cpu_usage_percent;
    double memory_usage_mb;
} global_metrics_t;

// Function declarations
double timespec_diff_ms(struct timespec *start, struct timespec *end);
void init_config(bench_config_t *config);
void print_config(const bench_config_t *config);
void init_metrics(global_metrics_t *metrics);
void update_metrics(global_metrics_t *metrics, conn_stats_t *conn_stats);
void finalize_metrics(global_metrics_t *metrics);
int export_metrics_json(const char *filename, const bench_config_t *config, 
                        const global_metrics_t *metrics, conn_stats_t *conn_stats_array, 
                        int num_connections);
int export_metrics_csv(const char *filename, const bench_config_t *config,
                       const global_metrics_t *metrics, conn_stats_t *conn_stats_array,
                       int num_connections);
void get_cpu_usage(double *cpu_percent);
void get_memory_usage(double *memory_mb);

#endif // BENCH_COMMON_H
