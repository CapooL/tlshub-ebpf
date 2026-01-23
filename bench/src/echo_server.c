#include "echo_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <signal.h>

static volatile int keep_running = 1;

void sigint_handler(int signum) {
    (void)signum;
    keep_running = 0;
    printf("\nShutting down echo server...\n");
}

// Start echo server
int echo_server_start(uint16_t port) {
    int server_fd;
    struct sockaddr_in server_addr;
    int opt = 1;
    
    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket creation failed");
        return -1;
    }
    
    // Set socket options
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
                   &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEADDR failed");
        close(server_fd);
        return -1;
    }
    
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT,
                   &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEPORT failed");
        close(server_fd);
        return -1;
    }
    
    // Bind to address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(server_fd);
        return -1;
    }
    
    // Listen for connections
    if (listen(server_fd, 128) < 0) {
        perror("listen failed");
        close(server_fd);
        return -1;
    }
    
    printf("Echo server listening on port %u\n", port);
    return server_fd;
}

// Handle a single client connection
void echo_server_handle_client(int client_fd) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received, bytes_sent;
    uint64_t total_bytes = 0;
    
    // Enable TCP_NODELAY
    int flag = 1;
    setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    
    // Get client address for logging
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    if (getpeername(client_fd, (struct sockaddr *)&client_addr, &addr_len) == 0) {
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        printf("New connection from %s:%u\n", client_ip, ntohs(client_addr.sin_port));
    }
    
    // Echo loop
    while (1) {
        bytes_received = recv(client_fd, buffer, sizeof(buffer), 0);
        
        if (bytes_received < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("recv failed");
            break;
        } else if (bytes_received == 0) {
            // Client closed connection
            break;
        }
        
        total_bytes += bytes_received;
        
        // Echo data back
        ssize_t total_sent = 0;
        while (total_sent < bytes_received) {
            bytes_sent = send(client_fd, buffer + total_sent, 
                            bytes_received - total_sent, 0);
            if (bytes_sent < 0) {
                if (errno == EINTR) {
                    continue;
                }
                perror("send failed");
                close(client_fd);
                return;
            }
            total_sent += bytes_sent;
        }
    }
    
    printf("Connection closed, total bytes echoed: %lu\n", total_bytes);
    
    // Shutdown and close
    shutdown(client_fd, SHUT_RDWR);
    close(client_fd);
}

// Run echo server
int run_echo_server(const bench_config_t *config) {
    int server_fd, client_fd;
    struct sockaddr_in client_addr;
    socklen_t addr_len;
    
    // Set up signal handler for graceful shutdown
    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);
    
    printf("=== TLShub Echo Server ===\n");
    printf("Listen Port: %u\n", config->listen_port);
    printf("Note: Handles clients serially (one at a time)\n");
    printf("Press Ctrl+C to stop\n\n");
    
    // Start server
    server_fd = echo_server_start(config->listen_port);
    if (server_fd < 0) {
        return -1;
    }
    
    // Accept and handle connections
    // Note: Current implementation handles clients serially for simplicity.
    // For production use with high concurrency, consider using fork() or
    // pthread to handle multiple clients simultaneously.
    while (keep_running) {
        addr_len = sizeof(client_addr);
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("accept failed");
            break;
        }
        
        // Handle client (serially)
        echo_server_handle_client(client_fd);
    }
    
    close(server_fd);
    printf("Echo server stopped.\n");
    return 0;
}
