#include "bench_common.h"
#include "tcp_client.h"
#include "echo_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>

void print_usage(const char *program_name) {
    printf("TLShub Benchmark Tool - Layer 7 Traffic Simulator\n\n");
    printf("Usage: %s [OPTIONS]\n\n", program_name);
    printf("Options:\n");
    printf("  -m, --mode MODE           Operating mode: client or server (default: client)\n");
    printf("  -t, --target-ip IP        Target server IP address (default: 127.0.0.1)\n");
    printf("  -p, --target-port PORT    Target server port (default: 8080)\n");
    printf("  -l, --listen-port PORT    Echo server listen port (default: 9090)\n");
    printf("  -s, --data-size SIZE      Data size to send in bytes (default: 1024)\n");
    printf("  -c, --concurrency NUM     Number of concurrent connections (default: 1)\n");
    printf("                            Note: Currently used for planning; actual execution\n");
    printf("                            is sequential. True concurrency in future versions.\n");
    printf("  -n, --total-connections NUM  Total number of connections (default: 10)\n");
    printf("  -j, --json FILE           Output JSON file (default: bench_metrics_<timestamp>.json)\n");
    printf("  -o, --csv FILE            Output CSV file (default: bench_metrics_<timestamp>.csv)\n");
    printf("  -v, --verbose             Enable verbose output\n");
    printf("  -h, --help                Show this help message\n\n");
    printf("Examples:\n");
    printf("  # Run echo server\n");
    printf("  %s --mode server --listen-port 9090\n\n", program_name);
    printf("  # Run client test\n");
    printf("  %s --mode client --target-ip 127.0.0.1 --target-port 9090 \\\n", program_name);
    printf("    --data-size 4096 --total-connections 100\n\n");
    printf("  # Run server and client in separate terminals\n");
    printf("  # Terminal 1:\n");
    printf("  %s --mode server --listen-port 9090\n", program_name);
    printf("  # Terminal 2:\n");
    printf("  %s --mode client --target-port 9090\n\n", program_name);
    printf("Integration with TLShub:\n");
    printf("  1. Ensure TLShub kernel module is loaded\n");
    printf("  2. Configure TLShub to intercept traffic to target IP:port\n");
    printf("  3. Start echo server: %s --mode server\n", program_name);
    printf("  4. Run client benchmark: %s --mode client\n", program_name);
    printf("  5. Check metrics in generated JSON/CSV files\n\n");
}

int parse_mode(const char *mode_str) {
    if (strcmp(mode_str, "client") == 0) {
        return MODE_CLIENT;
    } else if (strcmp(mode_str, "server") == 0) {
        return MODE_SERVER;
    }
    // Note: MODE_BOTH removed as it's not yet implemented
    // It would require running server and client in the same process
    return -1;
}

int main(int argc, char *argv[]) {
    bench_config_t config;
    global_metrics_t metrics;
    conn_stats_t *conn_stats_array = NULL;
    int ret = 0;
    
    // Initialize configuration with defaults
    init_config(&config);
    
    // Parse command line arguments
    static struct option long_options[] = {
        {"mode", required_argument, 0, 'm'},
        {"target-ip", required_argument, 0, 't'},
        {"target-port", required_argument, 0, 'p'},
        {"listen-port", required_argument, 0, 'l'},
        {"data-size", required_argument, 0, 's'},
        {"concurrency", required_argument, 0, 'c'},
        {"total-connections", required_argument, 0, 'n'},
        {"json", required_argument, 0, 'j'},
        {"csv", required_argument, 0, 'o'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt, option_index = 0;
    while ((opt = getopt_long(argc, argv, "m:t:p:l:s:c:n:j:o:vh",
                              long_options, &option_index)) != -1) {
        switch (opt) {
            case 'm':
                {
                    int mode = parse_mode(optarg);
                    if (mode < 0) {
                        fprintf(stderr, "Invalid mode: %s\n", optarg);
                        return EXIT_FAILURE;
                    }
                    config.mode = mode;
                }
                break;
            case 't':
                strncpy(config.target_ip, optarg, sizeof(config.target_ip) - 1);
                break;
            case 'p':
                config.target_port = atoi(optarg);
                if (config.target_port == 0) {
                    fprintf(stderr, "Invalid target port: %s\n", optarg);
                    return EXIT_FAILURE;
                }
                break;
            case 'l':
                config.listen_port = atoi(optarg);
                if (config.listen_port == 0) {
                    fprintf(stderr, "Invalid listen port: %s\n", optarg);
                    return EXIT_FAILURE;
                }
                break;
            case 's':
                config.data_size = atoi(optarg);
                if (config.data_size == 0 || config.data_size > 10*1024*1024) {
                    fprintf(stderr, "Invalid data size: %s (must be 1-10485760)\n", optarg);
                    return EXIT_FAILURE;
                }
                break;
            case 'c':
                config.concurrency = atoi(optarg);
                if (config.concurrency <= 0 || config.concurrency > 1000) {
                    fprintf(stderr, "Invalid concurrency: %s (must be 1-1000)\n", optarg);
                    return EXIT_FAILURE;
                }
                break;
            case 'n':
                config.total_connections = atoi(optarg);
                if (config.total_connections <= 0 || config.total_connections > MAX_CONNECTIONS) {
                    fprintf(stderr, "Invalid total connections: %s (must be 1-%d)\n", 
                            optarg, MAX_CONNECTIONS);
                    return EXIT_FAILURE;
                }
                break;
            case 'j':
                strncpy(config.output_json, optarg, sizeof(config.output_json) - 1);
                break;
            case 'o':
                strncpy(config.output_csv, optarg, sizeof(config.output_csv) - 1);
                break;
            case 'v':
                config.verbose = true;
                break;
            case 'h':
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            default:
                print_usage(argv[0]);
                return EXIT_FAILURE;
        }
    }
    
    // Print configuration
    print_config(&config);
    
    // Execute based on mode
    if (config.mode == MODE_SERVER) {
        // Run echo server
        ret = run_echo_server(&config);
    } else if (config.mode == MODE_CLIENT) {
        // Allocate connection stats array
        conn_stats_array = calloc(config.total_connections, sizeof(conn_stats_t));
        if (!conn_stats_array) {
            fprintf(stderr, "Failed to allocate memory for connection stats\n");
            return EXIT_FAILURE;
        }
        
        // Initialize metrics
        init_metrics(&metrics);
        
        // Run client benchmark
        ret = run_tcp_client(&config, &metrics, conn_stats_array);
        
        // Finalize metrics
        finalize_metrics(&metrics);
        
        // Print summary
        printf("=== Benchmark Results ===\n");
        printf("Total Connections: %d\n", metrics.total_connections);
        printf("Successful: %d\n", metrics.successful_connections);
        printf("Failed: %d\n", metrics.failed_connections);
        printf("Timeout: %d\n", metrics.timeout_connections);
        printf("Total Bytes Sent: %lu\n", metrics.total_bytes_sent);
        printf("Total Bytes Received: %lu\n", metrics.total_bytes_received);
        printf("Avg Connection Latency: %.3f ms\n", metrics.avg_connection_latency_ms);
        printf("Min Connection Latency: %.3f ms\n", metrics.min_connection_latency_ms);
        printf("Max Connection Latency: %.3f ms\n", metrics.max_connection_latency_ms);
        printf("Avg Throughput: %.3f Mbps\n", metrics.avg_throughput_mbps);
        printf("Total Test Duration: %.3f sec\n", metrics.total_test_duration_sec);
        printf("CPU Usage: %.2f%%\n", metrics.cpu_usage_percent);
        printf("Memory Usage: %.2f MB\n", metrics.memory_usage_mb);
        printf("========================\n\n");
        
        // Export metrics
        printf("Exporting metrics...\n");
        if (export_metrics_json(config.output_json, &config, &metrics, 
                               conn_stats_array, config.total_connections) == 0) {
            printf("JSON metrics saved to: %s\n", config.output_json);
        }
        if (export_metrics_csv(config.output_csv, &config, &metrics,
                              conn_stats_array, config.total_connections) == 0) {
            printf("CSV metrics saved to: %s\n", config.output_csv);
        }
        
        free(conn_stats_array);
    } else {
        // MODE_BOTH is not implemented yet
        printf("Invalid mode. Use --mode client or --mode server\n");
        printf("Run server and client in separate terminals:\n");
        printf("  Terminal 1: %s --mode server --listen-port %u\n", 
               argv[0], config.listen_port);
        printf("  Terminal 2: %s --mode client --target-port %u\n",
               argv[0], config.listen_port);
        return EXIT_FAILURE;
    }
    
    return ret == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
