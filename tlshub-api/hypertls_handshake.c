/*
 * 字节序处理说明 (Byte Order Handling):
 * 
 * 完整流程 (Complete Pipeline):
 * 1. 用户空间 (User Space):
 *    inet_addr("192.168.1.1") → 返回网络字节序 uint32_t (Big-Endian)
 *    
 * 2. 传递给内核 (Pass to Kernel):
 *    struct my_msg 中保存网络字节序 (network byte order for transmission)
 *    
 * 3. 内核空间 (Kernel Space - src/netlink.c):
 *    uint32_t client_pod_ip_host = ntohl(mmsg->client_pod_ip);  // 网络序 → 主机序
 *    
 * 4. 索引计算 (Index Calculation):
 *    calculate_ssl_index_client(client_pod_ip_host, ...) // 使用主机序进行算术运算
 *    
 * 为什么这样做? (Why?):
 * - 网络传输必须用网络字节序 (network byte order for transmission)
 * - 算术运算必须用主机字节序 (host byte order for arithmetic)
 * - inet_addr() 返回网络序是标准行为 (standard Linux behavior)
 * - 内核用 ntohl() 转换为主机序再计算 (kernel converts for calculation)
 */
#include "hypertls.h"
#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define TIMEOUT_SECONDS 30 // 30 seconds timeout
#define MIN_PORT 10000
#define MAX_PORT 65000

// Timeout handler
void timeout_handler(int signum)
{
    (void)signum; // Suppress unused parameter warning
    fprintf(stderr,
            "ERROR: Test execution timed out after %d seconds\n",
            TIMEOUT_SECONDS);
    exit(1);
}

// Generate random port number
int get_random_port()
{
    return MIN_PORT + (rand() % (MAX_PORT - MIN_PORT + 1));
}

int main(int argc, char* argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <client_pod_ip> <server_pod_ip>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char* client_ip = argv[1];
    const char* server_ip = argv[2];

    // Set up timeout handler
    signal(SIGALRM, timeout_handler);
    alarm(TIMEOUT_SECONDS);
    printf("Test timeout set to %d seconds\n", TIMEOUT_SECONDS);

    printf("Starting handshake from client pod=%s to server pod=%s\n",
           client_ip,
           server_ip);

    // Initialize random seed
    srand((unsigned)time(NULL) + getpid());

    for (int i = 0; i < 3; i++) {
        int client_port = get_random_port();
        int server_port = get_random_port();
        printf("Attempting handshake on ports %d -> %d\n",
               client_port,
               server_port);

        // Parameters: client_pod_ip, server_pod_ip, client_pod_port, server_pod_port
        int rc = hypertls_handshake(
            inet_addr(client_ip), // client_pod_ip = client Pod IP
            inet_addr(server_ip), // server_pod_ip = server Pod IP
            client_port, // client_pod_port = random port
            server_port // server_pod_port = random port
        );
        if (rc == 0) {
            printf(" → success (ports %d -> %d)\n", client_port, server_port);
        } else {
            fprintf(stderr,
                    " → failed (ports %d -> %d, err=%d)\n",
                    client_port,
                    server_port,
                    rc);
            return rc;
        }

        sleep(1);
    }

    // Cancel the alarm if we finish successfully
    alarm(0);
    printf("Test completed successfully within timeout\n");
    return 0;
}