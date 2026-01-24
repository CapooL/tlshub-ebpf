#ifndef ECHO_SERVER_H
#define ECHO_SERVER_H

#include "bench_common.h"

// Echo server functions
int echo_server_start(uint16_t port);
void echo_server_handle_client(int client_fd);
int run_echo_server(const bench_config_t *config);

#endif // ECHO_SERVER_H
