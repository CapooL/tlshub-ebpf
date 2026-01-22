#ifndef _HYPERTLS_user_SERVICE_H
#define _HYPERTLS_user_SERVICE_H

#include <arpa/inet.h>
#include <errno.h>
#include <linux/netlink.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

enum debug_level {
    DEBUG_LEVEL_ALL,
    DEBUG_LEVEL_INFO,
    DEBUG_LEVEL_DEBUG,
    DEBUG_LEVEL_WARNING,
    DEBUG_LEVEL_ERR,
    DEBUG_LEVEL_NONE
};

struct key_back {
    int status; // 当前key的状态 0：正常返回；-1：握手未完成或出现错误而没有key返回；-2：masterkey过期
    unsigned char masterkey[32];
};

typedef struct netlink_context {
    int sk_fd;
    struct nlmsghdr* nlh;
    struct sockaddr_nl daddr;
} netlink_context_t;

void log_fun(int level,
             const char* file,
             const char* func,
             int line,
             const char* fmt,
             ...);
#define DEBUG_PRINTF(level, fmt, ...) \
    log_fun(level, __FILE__, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)

int hypertls_service_init(void);

/**
 * 发起 Pod 级 TLS 握手（用户空间 API）
 * 
 * @param client_pod_ip   客户端 Pod IP (inet_addr 返回的网络字节序)
 * @param server_pod_ip   服务端 Pod IP (inet_addr 返回的网络字节序)
 * @param client_pod_port 客户端 Pod 端口 (主机字节序)
 * @param server_pod_port 服务端 Pod 端口 (主机字节序)
 * @return 0 成功，负数失败
 */
int hypertls_handshake(uint32_t client_pod_ip,
                       uint32_t server_pod_ip,
                       unsigned short client_pod_port,
                       unsigned short server_pod_port);

/**
 * 获取 TLS 会话密钥
 * 
 * @param client_pod_ip   客户端 Pod IP (inet_addr 返回的网络字节序)
 * @param server_pod_ip   服务端 Pod IP (inet_addr 返回的网络字节序)
 * @param client_pod_port 客户端 Pod 端口 (主机字节序)
 * @param server_pod_port 服务端 Pod 端口 (主机字节序)
 * @return key_back 结构体
 */
struct key_back hypertls_fetch_key(uint32_t client_pod_ip,
                                   uint32_t server_pod_ip,
                                   unsigned short client_pod_port,
                                   unsigned short server_pod_port);

double app_tm_interval(int stop, int user_time); // 测试时间的函数，精确到纳秒
void print_hex_dump(unsigned char* mem, int size); // 按16进制打印内存
#endif