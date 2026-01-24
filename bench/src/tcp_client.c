#include "tcp_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <inttypes.h>

// Connect to target server
int tcp_client_connect(const char *target_ip, uint16_t target_port, 
                       conn_stats_t *stats) {
    int sockfd;
    struct sockaddr_in server_addr;
    
    clock_gettime(CLOCK_MONOTONIC, &stats->start_time);
    stats->state = CONN_STATE_CONNECTING;
    
    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        stats->state = CONN_STATE_FAILED;
        return -1;
    }
    
    // Enable TCP_NODELAY for better latency measurement
    int flag = 1;
    setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    
    // Set up server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(target_port);
    
    if (inet_pton(AF_INET, target_ip, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid address: %s\n", target_ip);
        close(sockfd);
        stats->state = CONN_STATE_FAILED;
        return -1;
    }
    
    // Connect to server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect failed");
        close(sockfd);
        stats->state = CONN_STATE_FAILED;
        return -1;
    }
    
    clock_gettime(CLOCK_MONOTONIC, &stats->connect_time);
    stats->connection_latency_ms = timespec_diff_ms(&stats->start_time, &stats->connect_time);
    stats->state = CONN_STATE_CONNECTED;
    
    return sockfd;
}

// Send data to server
int tcp_client_send_data(int sockfd, size_t data_size, conn_stats_t *stats) {
    struct timespec send_start, send_end;
    char *buffer;
    ssize_t sent, total_sent = 0;
    
    stats->state = CONN_STATE_SENDING;
    clock_gettime(CLOCK_MONOTONIC, &send_start);
    
    // Allocate buffer and fill with test data
    buffer = malloc(data_size);
    if (!buffer) {
        perror("malloc failed");
        stats->state = CONN_STATE_FAILED;
        return -1;
    }
    
    // Fill buffer with predictable pattern efficiently
    if (data_size > 0) {
        memset(buffer, 0xAA, data_size);  // Fill with pattern 0xAA
        // Optionally add some variation
        for (size_t i = 0; i < data_size && i < 256; i++) {
            buffer[i] = (char)(i % 256);
        }
    }
    
    // Send data
    while (total_sent < (ssize_t)data_size) {
        sent = send(sockfd, buffer + total_sent, data_size - total_sent, 0);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("send failed");
            free(buffer);
            stats->state = CONN_STATE_FAILED;
            return -1;
        }
        total_sent += sent;
    }
    
    clock_gettime(CLOCK_MONOTONIC, &send_end);
    stats->bytes_sent = total_sent;
    
    if (stats->first_byte_time.tv_sec == 0) {
        stats->first_byte_time = send_end;
    }
    
    free(buffer);
    return 0;
}

// Receive data from server (echo response)
int tcp_client_receive_data(int sockfd, conn_stats_t *stats) {
    char buffer[BUFFER_SIZE];
    ssize_t received;
    struct pollfd pfd;
    int poll_ret;
    
    stats->state = CONN_STATE_RECEIVING;
    
    // Set up poll for timeout
    pfd.fd = sockfd;
    pfd.events = POLLIN;
    
    while (1) {
        // Wait for data with 5 second timeout
        poll_ret = poll(&pfd, 1, 5000);
        
        if (poll_ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("poll failed");
            stats->state = CONN_STATE_FAILED;
            return -1;
        } else if (poll_ret == 0) {
            // Timeout - check if we received all expected data
            if (stats->bytes_received < stats->bytes_sent) {
                // Incomplete transfer - mark as timeout
                stats->state = CONN_STATE_TIMEOUT;
                return -1;
            }
            // No more data but we got everything
            break;
        }
        
        received = recv(sockfd, buffer, sizeof(buffer), 0);
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("recv failed");
            stats->state = CONN_STATE_FAILED;
            return -1;
        } else if (received == 0) {
            // Connection closed by server
            break;
        }
        
        stats->bytes_received += received;
        
        // If we received all the data we sent, we're done
        if (stats->bytes_received >= stats->bytes_sent) {
            break;
        }
    }
    
    return 0;
}

// Close connection properly with FIN (not RST)
int tcp_client_close(int sockfd, conn_stats_t *stats) {
    struct timespec close_start, close_end;
    struct timeval timeout;
    
    stats->state = CONN_STATE_CLOSING;
    clock_gettime(CLOCK_MONOTONIC, &close_start);
    
    // Set receive timeout to prevent indefinite blocking
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    // Shutdown write side to send FIN
    if (shutdown(sockfd, SHUT_WR) < 0) {
        // Log but continue - socket may already be in error state
        if (errno != ENOTCONN) {
            perror("shutdown failed");
        }
    }
    
    // Wait for server to close (read until EOF or timeout)
    char buf[1024];
    while (recv(sockfd, buf, sizeof(buf), 0) > 0) {
        // Drain any remaining data
    }
    
    // Now close the socket
    close(sockfd);
    
    clock_gettime(CLOCK_MONOTONIC, &close_end);
    stats->close_time = close_end;
    stats->state = CONN_STATE_CLOSED;
    
    // Calculate metrics
    double total_time_ms = timespec_diff_ms(&stats->start_time, &close_end);
    stats->data_transfer_time_ms = total_time_ms - stats->connection_latency_ms;
    
    if (stats->data_transfer_time_ms > 0 && stats->bytes_sent > 0) {
        // Calculate throughput in Mbps
        double bytes_per_sec = (stats->bytes_sent + stats->bytes_received) / 
                               (stats->data_transfer_time_ms / 1000.0);
        stats->throughput_mbps = (bytes_per_sec * 8.0) / (1024.0 * 1024.0);
    }
    
    return 0;
}

// Run single connection
static int run_single_connection(const bench_config_t *config, conn_stats_t *stats) {
    int sockfd;
    int ret = 0;
    
    // Connect
    sockfd = tcp_client_connect(config->target_ip, config->target_port, stats);
    if (sockfd < 0) {
        return -1;
    }
    
    if (config->verbose) {
        printf("Connection %d: Connected (latency: %.3f ms)\n", 
               stats->conn_id, stats->connection_latency_ms);
    }
    
    // Send data
    if (tcp_client_send_data(sockfd, config->data_size, stats) < 0) {
        close(sockfd);
        return -1;
    }
    
    if (config->verbose) {
        printf("Connection %d: Sent %" PRIu64 " bytes\n", stats->conn_id, stats->bytes_sent);
    }
    
    // Receive echo response
    if (tcp_client_receive_data(sockfd, stats) < 0) {
        close(sockfd);
        return -1;
    }
    
    if (config->verbose) {
        printf("Connection %d: Received %" PRIu64 " bytes\n", stats->conn_id, stats->bytes_received);
    }
    
    // Close connection properly
    if (tcp_client_close(sockfd, stats) < 0) {
        return -1;
    }
    
    if (config->verbose) {
        printf("Connection %d: Closed (throughput: %.3f Mbps)\n", 
               stats->conn_id, stats->throughput_mbps);
    }
    
    return ret;
}

// Run TCP client benchmark
int run_tcp_client(const bench_config_t *config, global_metrics_t *metrics,
                   conn_stats_t *conn_stats_array) {
    int total_conns = config->total_connections;
    int completed = 0;
    int failed = 0;
    
    printf("Starting TCP client benchmark...\n");
    printf("Target: %s:%u\n", config->target_ip, config->target_port);
    printf("Total connections: %d\n", total_conns);
    printf("Data size: %zu bytes\n", config->data_size);
    printf("Concurrency: %d (note: sequential execution in current version)\n\n", 
           config->concurrency);
    
    // Note: Current implementation runs connections sequentially for simplicity
    // and reliability. True concurrent execution can be added in future versions
    // using threads (pthread) or asynchronous I/O (epoll).
    for (int i = 0; i < total_conns; i++) {
        conn_stats_t *stats = &conn_stats_array[i];
        memset(stats, 0, sizeof(conn_stats_t));
        stats->conn_id = i + 1;
        
        if (run_single_connection(config, stats) == 0) {
            completed++;
            update_metrics(metrics, stats);
        } else {
            failed++;
            stats->state = CONN_STATE_FAILED;
            update_metrics(metrics, stats);
        }
        
        // Progress indicator
        if ((i + 1) % 10 == 0 || (i + 1) == total_conns) {
            printf("Progress: %d/%d connections (%d succeeded, %d failed)\n",
                   i + 1, total_conns, completed, failed);
        }
        
        // Small delay between connections to avoid overwhelming the system
        usleep(10000); // 10ms delay
    }
    
    printf("\nClient benchmark completed.\n");
    printf("Total: %d, Succeeded: %d, Failed: %d\n\n", total_conns, completed, failed);
    
    return 0;
}
