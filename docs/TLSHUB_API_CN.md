# TLShub 用户态接口调用文档

## 概述

本文档详细说明如何在用户态程序中正确调用 TLShub 内核模块的接口。TLShub 是一个 Layer 4 友好的内核级 mTLS 握手模块，通过 Netlink 协议与用户态程序通信。

## 目录

- [快速开始](#快速开始)
- [API 参考](#api-参考)
- [数据结构](#数据结构)
- [使用示例](#使用示例)
- [错误处理](#错误处理)
- [字节序说明](#字节序说明)
- [集成指南](#集成指南)

## 快速开始

### 1. 引入头文件

```c
#include "tlshub.h"
```

### 2. 初始化 TLShub 服务

在使用任何 TLShub 功能之前，必须先初始化服务：

```c
int ret = tlshub_service_init();
if (ret != 0) {
    fprintf(stderr, "TLShub 初始化失败\n");
    return -1;
}
```

### 3. 发起握手

```c
uint32_t client_ip = inet_addr("192.168.1.100");
uint32_t server_ip = inet_addr("192.168.1.200");
unsigned short client_port = 8080;
unsigned short server_port = 9090;

int ret = tlshub_handshake(client_ip, server_ip, client_port, server_port);
if (ret == 0) {
    printf("握手成功\n");
} else {
    fprintf(stderr, "握手失败\n");
}
```

### 4. 获取密钥

```c
struct key_back key = tlshub_fetch_key(client_ip, server_ip, client_port, server_port);
if (key.status == 0) {
    printf("成功获取密钥\n");
    // 使用 key.masterkey
} else {
    fprintf(stderr, "获取密钥失败，状态: %d\n", key.status);
}
```

## API 参考

### tlshub_service_init()

初始化 TLShub 服务。

```c
int tlshub_service_init(void);
```

**返回值：**
- `0`: 成功
- `负数`: 失败

**说明：**
- 必须在调用其他 TLShub API 之前调用
- 创建 Netlink socket 并发送初始化请求到内核
- 等待内核确认初始化完成
- 初始化过程中会输出日志信息

**线程安全：**
- 不是线程安全的
- 建议在程序启动时调用一次

### tlshub_handshake()

发起 Pod 级 TLS 握手。

```c
int tlshub_handshake(uint32_t client_pod_ip,
                     uint32_t server_pod_ip,
                     unsigned short client_pod_port,
                     unsigned short server_pod_port);
```

**参数：**
- `client_pod_ip`: 客户端 Pod IP（网络字节序，从 `inet_addr()` 获取）
- `server_pod_ip`: 服务端 Pod IP（网络字节序，从 `inet_addr()` 获取）
- `client_pod_port`: 客户端 Pod 端口（主机字节序）
- `server_pod_port`: 服务端 Pod 端口（主机字节序）

**返回值：**
- `0`: 握手成功
- `负数`: 握手失败

**说明：**
- 使用 thread-local 的 Netlink 上下文，支持多线程调用
- 握手过程可能需要几秒钟
- 内核会返回以下消息类型：
  - `0x01`: 首次握手成功
  - `0x02`: 非首次握手成功
  - `0x03`: 握手失败
  - `0x04`: 日志消息
  - `0x05`: 已连接过

**线程安全：**
- 线程安全（使用 thread-local 存储）

### tlshub_fetch_key()

获取 TLS 会话密钥。

```c
struct key_back tlshub_fetch_key(uint32_t client_pod_ip,
                                 uint32_t server_pod_ip,
                                 unsigned short client_pod_port,
                                 unsigned short server_pod_port);
```

**参数：**
- `client_pod_ip`: 客户端 Pod IP（网络字节序，从 `inet_addr()` 获取）
- `server_pod_ip`: 服务端 Pod IP（网络字节序，从 `inet_addr()` 获取）
- `client_pod_port`: 客户端 Pod 端口（主机字节序）
- `server_pod_port`: 服务端 Pod 端口（主机字节序）

**返回值：**
返回 `struct key_back` 结构体，包含：
- `status`: 密钥状态
  - `0`: 正常返回
  - `-1`: 握手未完成或出现错误
  - `-2`: masterkey 过期
- `masterkey[32]`: 32 字节的主密钥

**说明：**
- 如果握手尚未完成，会返回 status = -1
- 建议先调用 `tlshub_handshake()`，然后调用 `tlshub_fetch_key()`
- 为每次调用创建新的 Netlink 上下文

**线程安全：**
- 线程安全（每次调用创建新上下文）

### 辅助函数

#### app_tm_interval()

测试时间，精确到纳秒。

```c
double app_tm_interval(int stop, int user_time);
```

**参数：**
- `stop`: `TM_START`(1) 开始计时，`0` 停止计时并返回间隔
- `user_time`: 是否输出警告信息

**返回值：**
- 以秒为单位的时间间隔

#### print_hex_dump()

按 16 进制打印内存。

```c
void print_hex_dump(unsigned char* mem, int size);
```

**参数：**
- `mem`: 内存指针
- `size`: 字节数

## 数据结构

### struct key_back

密钥返回结构。

```c
struct key_back {
    int status;                  // 当前key的状态
    unsigned char masterkey[32]; // 主密钥
};
```

**字段说明：**
- `status`: 
  - `0`: 正常返回
  - `-1`: 握手未完成或出现错误而没有key返回
  - `-2`: masterkey过期
- `masterkey`: 32字节的主密钥数组

### struct my_msg（内部使用）

Netlink 消息载荷结构（仅供参考，用户无需直接使用）。

```c
struct my_msg {
    uint32_t client_pod_ip;
    uint32_t server_pod_ip;
    unsigned short client_pod_port;
    unsigned short server_pod_port;
    char opcode;
    bool server;
};
```

**操作码（opcode）：**
- `TLS_SERVICE_INIT (0)`: 初始化服务
- `TLS_SERVICE_START (1)`: 开始握手
- `TLS_SERVICE_FETCH (2)`: 获取密钥

## 使用示例

### 示例 1: 基本握手流程

```c
#include "tlshub.h"
#include <arpa/inet.h>
#include <stdio.h>

int main() {
    // 1. 初始化服务（程序启动时调用一次）
    if (tlshub_service_init() != 0) {
        fprintf(stderr, "初始化失败\n");
        return -1;
    }
    
    // 2. 准备 Pod IP 和端口
    uint32_t client_ip = inet_addr("10.0.1.100");
    uint32_t server_ip = inet_addr("10.0.2.200");
    unsigned short client_port = 8080;
    unsigned short server_port = 9090;
    
    // 3. 发起握手
    int ret = tlshub_handshake(client_ip, server_ip, client_port, server_port);
    if (ret != 0) {
        fprintf(stderr, "握手失败\n");
        return -1;
    }
    
    printf("握手成功！\n");
    return 0;
}
```

### 示例 2: 完整的握手和密钥获取

```c
#include "tlshub.h"
#include <arpa/inet.h>
#include <stdio.h>

int main() {
    // 初始化
    if (tlshub_service_init() != 0) {
        fprintf(stderr, "初始化失败\n");
        return -1;
    }
    
    // Pod 信息
    uint32_t client_ip = inet_addr("10.0.1.100");
    uint32_t server_ip = inet_addr("10.0.2.200");
    unsigned short client_port = 8080;
    unsigned short server_port = 9090;
    
    // 发起握手
    printf("开始握手...\n");
    if (tlshub_handshake(client_ip, server_ip, client_port, server_port) != 0) {
        fprintf(stderr, "握手失败\n");
        return -1;
    }
    printf("握手成功\n");
    
    // 获取密钥
    printf("获取密钥...\n");
    struct key_back key = tlshub_fetch_key(client_ip, server_ip, 
                                           client_port, server_port);
    
    if (key.status == 0) {
        printf("成功获取密钥:\n");
        print_hex_dump(key.masterkey, 32);
    } else {
        fprintf(stderr, "获取密钥失败，状态: %d\n", key.status);
        return -1;
    }
    
    return 0;
}
```

### 示例 3: 多次握手（测试程序）

完整示例请参考 `tlshub-api/tlshub_handshake.c`。

```c
#include "tlshub.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define MIN_PORT 10000
#define MAX_PORT 65000

int get_random_port() {
    return MIN_PORT + (rand() % (MAX_PORT - MIN_PORT + 1));
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "用法: %s <客户端IP> <服务端IP>\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    const char* client_ip_str = argv[1];
    const char* server_ip_str = argv[2];
    
    // 初始化随机数种子
    srand((unsigned)time(NULL) + getpid());
    
    // 转换 IP
    uint32_t client_ip = inet_addr(client_ip_str);
    uint32_t server_ip = inet_addr(server_ip_str);
    
    printf("开始握手测试: %s -> %s\n", client_ip_str, server_ip_str);
    
    // 进行 3 次握手测试
    for (int i = 0; i < 3; i++) {
        int client_port = get_random_port();
        int server_port = get_random_port();
        
        printf("\n测试 %d: 端口 %d -> %d\n", i + 1, client_port, server_port);
        
        int rc = tlshub_handshake(client_ip, server_ip, 
                                  client_port, server_port);
        if (rc == 0) {
            printf("  ✓ 成功\n");
        } else {
            fprintf(stderr, "  ✗ 失败 (错误码=%d)\n", rc);
            return rc;
        }
        
        sleep(1);
    }
    
    printf("\n所有测试完成！\n");
    return 0;
}
```

## 错误处理

### 常见错误及解决方法

#### 1. 初始化失败

**错误：** `tlshub_service_init()` 返回负数

**可能原因：**
- TLShub 内核模块未加载
- Netlink socket 创建失败（权限不足）
- 内核模块未响应

**解决方法：**
```bash
# 检查内核模块是否加载
lsmod | grep tlshub

# 以 root 权限运行程序
sudo ./your_program

# 检查 dmesg 日志
dmesg | tail -20
```

#### 2. 握手失败

**错误：** `tlshub_handshake()` 返回负数

**可能原因：**
- 网络不通
- 对端未启动
- 证书或配置问题
- 握手超时

**解决方法：**
- 检查网络连接性
- 确认两端都已初始化
- 查看内核日志获取详细错误信息

#### 3. 获取密钥返回 status = -1

**错误：** `tlshub_fetch_key()` 返回的 `key.status` 为 `-1`

**可能原因：**
- 握手尚未完成
- 握手过程中出现错误
- 内核中没有该连接的会话信息

**解决方法：**
```c
// 先握手再获取密钥
if (tlshub_handshake(...) == 0) {
    struct key_back key = tlshub_fetch_key(...);
    if (key.status == 0) {
        // 使用密钥
    }
}
```

#### 4. 获取密钥返回 status = -2

**错误：** `tlshub_fetch_key()` 返回的 `key.status` 为 `-2`

**可能原因：**
- 主密钥已过期
- 需要重新握手

**解决方法：**
```c
struct key_back key = tlshub_fetch_key(...);
if (key.status == -2) {
    printf("密钥已过期，重新握手...\n");
    tlshub_handshake(...);
    key = tlshub_fetch_key(...);
}
```

## 字节序说明

TLShub API 中的字节序处理是一个重要概念，需要正确理解以避免错误。

### 完整流程

```
1. 用户空间 (User Space):
   inet_addr("192.168.1.1") → 返回网络字节序 uint32_t (Big-Endian)

2. 传递给内核 (Pass to Kernel):
   struct my_msg 中保存网络字节序 (network byte order for transmission)

3. 内核空间 (Kernel Space):
   uint32_t client_pod_ip_host = ntohl(mmsg->client_pod_ip);  // 网络序 → 主机序

4. 索引计算 (Index Calculation):
   calculate_ssl_index_client(client_pod_ip_host, ...) // 使用主机序进行算术运算
```

### 为什么这样做？

- **网络传输必须用网络字节序**：确保不同架构的机器能正确通信
- **算术运算必须用主机字节序**：CPU 只能正确处理主机字节序的数字
- **inet_addr() 返回网络序是标准行为**：Linux 标准库的约定
- **内核用 ntohl() 转换**：内核在需要时负责转换

### 使用建议

```c
// ✓ 正确：直接使用 inet_addr 的返回值
uint32_t client_ip = inet_addr("192.168.1.100");
tlshub_handshake(client_ip, ...);

// ✗ 错误：不要手动转换字节序
uint32_t client_ip = htonl(inet_addr("192.168.1.100")); // 多余的转换
tlshub_handshake(client_ip, ...);

// ✓ 正确：端口号传入主机字节序
unsigned short port = 8080;  // 主机字节序
tlshub_handshake(..., port, ...);

// ✗ 错误：不要手动转换端口字节序
unsigned short port = htons(8080);  // 错误！内部会再次转换
tlshub_handshake(..., port, ...);
```

## 集成指南

### 在现有项目中集成 TLShub

#### 1. 添加源文件

将以下文件添加到项目：
- `tlshub.h`: 头文件
- `tlshub.c`: 实现文件

#### 2. 修改编译配置

```makefile
# Makefile 示例
CFLAGS += -I/path/to/tlshub-api
LDFLAGS += 

tlshub_app: main.o tlshub.o
	$(CC) -o $@ $^ $(LDFLAGS)

tlshub.o: tlshub.c tlshub.h
	$(CC) $(CFLAGS) -c tlshub.c
```

#### 3. 初始化时机

建议在程序启动时调用一次 `tlshub_service_init()`：

```c
int main() {
    // 早期初始化
    if (tlshub_service_init() != 0) {
        fprintf(stderr, "TLShub 初始化失败\n");
        return -1;
    }
    
    // 其他初始化...
    
    // 主程序逻辑...
}
```

#### 4. 在连接建立时调用

```c
// 当 TCP 连接建立时
void on_connection_established(int sockfd, 
                               struct sockaddr_in* local,
                               struct sockaddr_in* remote) {
    // 转换为 TLShub 需要的格式
    uint32_t client_ip = local->sin_addr.s_addr;   // 已经是网络字节序
    uint32_t server_ip = remote->sin_addr.s_addr;  // 已经是网络字节序
    unsigned short client_port = ntohs(local->sin_port);   // 转为主机序
    unsigned short server_port = ntohs(remote->sin_port);  // 转为主机序
    
    // 发起握手
    if (tlshub_handshake(client_ip, server_ip, 
                         client_port, server_port) == 0) {
        printf("TLS 握手成功\n");
        
        // 可选：获取密钥
        struct key_back key = tlshub_fetch_key(client_ip, server_ip,
                                               client_port, server_port);
        if (key.status == 0) {
            // 使用密钥配置 KTLS 等
        }
    }
}
```

### 与 eBPF 程序集成

如果您使用 eBPF 进行连接跟踪，可以在 eBPF 捕获到连接后调用 TLShub API：

```c
// eBPF 事件处理函数
static void handle_tcp_event(void *ctx, int cpu, void *data, __u32 data_sz) {
    struct connection_info *info = data;
    
    // 调用 TLShub 进行握手
    int ret = tlshub_handshake(info->tuple.saddr, 
                               info->tuple.daddr,
                               info->tuple.sport,
                               info->tuple.dport);
    
    if (ret == 0) {
        // 握手成功，可以配置 KTLS
        // ...
    }
}
```

详细示例请参考 `capture/src/key_provider.c` 中的实现。

## 性能考虑

### 1. Netlink 通信开销

- 每次 API 调用都涉及用户态-内核态切换
- `tlshub_handshake()` 使用 thread-local 上下文，减少创建/销毁开销
- `tlshub_fetch_key()` 每次创建新上下文，适合一次性调用

### 2. 握手延迟

- 完整的 TLS 握手可能需要几秒钟
- 使用异步方式或单独线程处理握手，避免阻塞主线程

### 3. 密钥缓存

- 考虑在应用层缓存已获取的密钥
- 密钥过期时（status = -2）重新握手

### 4. 多线程使用

- `tlshub_handshake()` 是线程安全的（使用 thread-local 存储）
- `tlshub_fetch_key()` 是线程安全的（每次创建新上下文）
- `tlshub_service_init()` 不是线程安全的，建议在单线程初始化阶段调用

## 调试技巧

### 1. 启用调试输出

修改 `tlshub.c` 中的 `debugLevel`：

```c
int debugLevel = DEBUG_LEVEL_DEBUG;  // 显示所有调试信息
```

调试级别：
- `DEBUG_LEVEL_ALL`: 所有信息
- `DEBUG_LEVEL_INFO`: 信息
- `DEBUG_LEVEL_DEBUG`: 调试
- `DEBUG_LEVEL_WARNING`: 警告
- `DEBUG_LEVEL_ERR`: 错误
- `DEBUG_LEVEL_NONE`: 无输出

### 2. 查看内核日志

```bash
# 实时查看内核日志
dmesg -w | grep -i tlshub

# 或者
journalctl -kf | grep -i tlshub
```

### 3. 使用 strace 跟踪系统调用

```bash
strace -e trace=socket,bind,sendto,recvfrom ./your_program
```

### 4. 检查 Netlink 通信

```bash
# 监控 Netlink 消息
ss -A netlink

# 检查进程的 Netlink socket
ls -la /proc/$(pidof your_program)/fd/
```

## 常见问题 (FAQ)

### Q1: 为什么函数名从 hypertls 改为 tlshub？

**A:** 为了更好地与 TLShub 项目名称保持一致，提高代码可读性。所有功能保持不变，只是更名。

### Q2: 可以在同一进程中多次调用 tlshub_service_init() 吗？

**A:** 不推荐。`tlshub_service_init()` 应该只在程序启动时调用一次。多次调用会创建多个 Netlink socket，可能导致资源泄漏。

### Q3: TLShub 支持 IPv6 吗？

**A:** 当前版本只支持 IPv4。未来版本可能会添加 IPv6 支持。

### Q4: 如何处理握手超时？

**A:** 当前实现会一直等待内核响应。建议在调用 `tlshub_handshake()` 时设置超时机制：

```c
#include <signal.h>
#include <setjmp.h>

static jmp_buf timeout_env;

void timeout_handler(int signum) {
    longjmp(timeout_env, 1);
}

int handshake_with_timeout(uint32_t client_ip, uint32_t server_ip,
                           unsigned short client_port, unsigned short server_port,
                           unsigned int timeout_sec) {
    signal(SIGALRM, timeout_handler);
    
    if (setjmp(timeout_env) == 0) {
        alarm(timeout_sec);
        int ret = tlshub_handshake(client_ip, server_ip, client_port, server_port);
        alarm(0);
        return ret;
    } else {
        // 超时
        fprintf(stderr, "握手超时\n");
        return -1;
    }
}
```

### Q5: 可以在握手之前获取密钥吗？

**A:** 可以尝试，但如果握手尚未完成，`tlshub_fetch_key()` 会返回 `status = -1`。建议的流程是：先握手，后获取密钥。

## 参考资料

- [TLShub 项目](https://github.com/lin594/tlshub)
- [Linux Netlink 文档](https://man7.org/linux/man-pages/man7/netlink.7.html)
- [RFC 5705 - Keying Material Exporters for TLS](https://tools.ietf.org/html/rfc5705)

## 更新历史

- **2024-01**: 从 hypertls 重命名为 tlshub
- **2024-01**: 更新 API 文档，添加详细使用说明

## 联系方式

如有问题或建议，请通过以下方式联系：
- Issue: https://github.com/CapooL/tlshub-ebpf/issues
