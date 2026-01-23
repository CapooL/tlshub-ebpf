#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include "bench_common.h"

// TCP client functions
int tcp_client_connect(const char *target_ip, uint16_t target_port, 
                       conn_stats_t *stats);
int tcp_client_send_data(int sockfd, size_t data_size, conn_stats_t *stats);
int tcp_client_receive_data(int sockfd, conn_stats_t *stats);
int tcp_client_close(int sockfd, conn_stats_t *stats);
int run_tcp_client(const bench_config_t *config, global_metrics_t *metrics,
                   conn_stats_t *conn_stats_array);

#endif // TCP_CLIENT_H
