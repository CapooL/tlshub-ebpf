# TLShub 集成指南

## 概述

本文档提供了将 TLShub 用户态接口集成到现有项目的详细指南。无论您是在开发新项目还是改造现有项目，本指南都将帮助您快速、正确地集成 TLShub。

## 目录

- [前置要求](#前置要求)
- [集成步骤](#集成步骤)
- [在 capture 项目中的应用](#在-capture-项目中的应用)
- [自定义项目集成](#自定义项目集成)
- [常见集成场景](#常见集成场景)
- [故障排查](#故障排查)

## 前置要求

### 系统要求

- **操作系统**: Linux 内核版本 >= 5.2
- **编译器**: GCC >= 7.0 或 Clang >= 10.0
- **内核模块**: TLShub 内核模块已加载

### 检查内核模块

```bash
# 检查 TLShub 模块是否加载
lsmod | grep tlshub

# 如果未加载，加载模块
sudo insmod /path/to/tlshub.ko

# 检查 Netlink 支持
cat /proc/net/netlink
```

### 编译依赖

```bash
# Ubuntu/Debian
sudo apt-get install -y gcc make

# CentOS/RHEL
sudo yum install -y gcc make
```

## 集成步骤

### 步骤 1: 复制源文件

将以下文件复制到您的项目中：

```bash
# 创建目录（如果不存在）
mkdir -p /path/to/your/project/tlshub

# 复制头文件和源文件
cp tlshub-api/tlshub.h /path/to/your/project/tlshub/
cp tlshub-api/tlshub.c /path/to/your/project/tlshub/
```

### 步骤 2: 修改编译配置

#### 使用 Makefile

在您的 Makefile 中添加以下内容：

```makefile
# TLShub 配置
TLSHUB_DIR = ./tlshub
TLSHUB_INCLUDES = -I$(TLSHUB_DIR)
TLSHUB_SOURCES = $(TLSHUB_DIR)/tlshub.c

# 添加到编译选项
CFLAGS += $(TLSHUB_INCLUDES)

# 添加到源文件列表
SRCS += $(TLSHUB_SOURCES)

# 编译示例
your_app: main.o tlshub.o
	$(CC) $(CFLAGS) -o $@ $^

tlshub.o: $(TLSHUB_DIR)/tlshub.c
	$(CC) $(CFLAGS) -c $< -o $@

main.o: main.c $(TLSHUB_DIR)/tlshub.h
	$(CC) $(CFLAGS) -c $< -o $@
```

#### 使用 CMake

在您的 CMakeLists.txt 中添加：

```cmake
# 设置 TLShub 路径
set(TLSHUB_DIR ${CMAKE_SOURCE_DIR}/tlshub)

# 添加包含目录
include_directories(${TLSHUB_DIR})

# 添加源文件
set(TLSHUB_SOURCES ${TLSHUB_DIR}/tlshub.c)

# 创建可执行文件
add_executable(your_app
    main.c
    ${TLSHUB_SOURCES}
)
```

### 步骤 3: 在代码中引入

在您的源文件中：

```c
#include "tlshub.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    // 初始化 TLShub
    if (tlshub_service_init() != 0) {
        fprintf(stderr, "TLShub 初始化失败\n");
        return -1;
    }
    
    // 您的应用逻辑...
    
    return 0;
}
```

### 步骤 4: 编译和测试

```bash
# 编译
make

# 运行（需要 root 权限）
sudo ./your_app
```

## 在 capture 项目中的应用

本仓库的 `capture` 项目展示了如何在实际项目中集成 TLShub。

### 架构概览

```
capture/
├── src/
│   ├── main.c              # 主程序
│   ├── tlshub_client.c     # TLShub 客户端（封装了 tlshub-api）
│   ├── key_provider.c      # 密钥提供者（支持多种模式）
│   └── ...
├── include/
│   ├── tlshub_client.h     # TLShub 客户端头文件
│   └── ...
└── Makefile
```

### 关键实现

#### 1. TLShub 客户端封装 (tlshub_client.c)

`tlshub_client.c` 封装了 `tlshub-api` 的功能，提供了更高级的接口：

```c
// 初始化（调用 tlshub_service_init）
int tlshub_client_init(void);

// 清理
void tlshub_client_cleanup(void);

// 握手（适配 flow_tuple 结构）
int tlshub_handshake(struct flow_tuple *tuple);

// 获取密钥（适配 flow_tuple 和 tls_key_info 结构）
int tlshub_fetch_key(struct flow_tuple *tuple, struct tls_key_info *key_info);
```

#### 2. 密钥提供者 (key_provider.c)

支持多种密钥获取模式：

```c
enum key_provider_mode {
    MODE_TLSHUB = 0,    // 使用 TLShub
    MODE_OPENSSL = 1,   // 使用 OpenSSL
    MODE_BORINGSSL = 2, // 使用 BoringSSL
};

// 初始化密钥提供者
int key_provider_init(enum key_provider_mode mode);

// 获取密钥（自动选择模式）
int key_provider_get_key(struct flow_tuple *tuple, struct tls_key_info *key_info);
```

#### 3. 自动重试逻辑

在 `key_provider.c` 中实现了智能的密钥获取逻辑：

```c
int key_provider_get_key(struct flow_tuple *tuple, struct tls_key_info *key_info) {
    switch (current_mode) {
        case MODE_TLSHUB:
            // 尝试获取密钥
            ret = tlshub_fetch_key(tuple, key_info);
            if (ret < 0) {
                // 失败则先握手
                printf("Fetch key failed, initiating handshake\n");
                ret = tlshub_handshake(tuple);
                if (ret < 0) {
                    return ret;
                }
                // 握手成功后重试
                ret = tlshub_fetch_key(tuple, key_info);
            }
            return ret;
        // ...
    }
}
```

### 与 eBPF 集成

capture 项目展示了如何与 eBPF 程序集成：

```c
// eBPF 事件处理
static void handle_tcp_event(void *ctx, int cpu, void *data, __u32 data_sz) {
    struct connection_info *info = data;
    struct tls_key_info key_info;
    
    // 使用 key_provider 获取密钥（自动处理握手）
    if (key_provider_get_key(&info->tuple, &key_info) == 0) {
        // 配置 KTLS
        configure_ktls(sockfd, &key_info);
    }
}
```

## 自定义项目集成

### 场景 1: 简单的客户端程序

如果您只需要在客户端发起握手：

```c
#include "tlshub.h"
#include <arpa/inet.h>
#include <stdio.h>

int connect_with_tlshub(const char *server_ip, unsigned short server_port) {
    // 获取本地 IP（简化示例）
    uint32_t client_ip = inet_addr("10.0.1.100");
    uint32_t server_ip_bin = inet_addr(server_ip);
    unsigned short client_port = 12345;
    
    // 发起握手
    if (tlshub_handshake(client_ip, server_ip_bin, 
                         client_port, server_port) != 0) {
        fprintf(stderr, "握手失败\n");
        return -1;
    }
    
    printf("成功与 %s:%d 建立 TLS 连接\n", server_ip, server_port);
    return 0;
}

int main() {
    // 初始化
    if (tlshub_service_init() != 0) {
        return -1;
    }
    
    // 连接服务器
    connect_with_tlshub("10.0.2.200", 8080);
    
    return 0;
}
```

### 场景 2: 服务器程序

如果您的程序是服务器，需要为每个连接进行握手：

```c
#include "tlshub.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>

void handle_client(int client_fd, struct sockaddr_in *client_addr) {
    // 获取本地地址
    struct sockaddr_in local_addr;
    socklen_t addr_len = sizeof(local_addr);
    getsockname(client_fd, (struct sockaddr*)&local_addr, &addr_len);
    
    // 转换为 TLShub 格式
    uint32_t client_ip = client_addr->sin_addr.s_addr;
    uint32_t server_ip = local_addr.sin_addr.s_addr;
    unsigned short client_port = ntohs(client_addr->sin_port);
    unsigned short server_port = ntohs(local_addr.sin_port);
    
    // 发起握手
    if (tlshub_handshake(client_ip, server_ip, 
                         client_port, server_port) == 0) {
        printf("客户端 %s:%d 握手成功\n", 
               inet_ntoa(client_addr->sin_addr), client_port);
        
        // 处理客户端请求...
    } else {
        fprintf(stderr, "握手失败\n");
    }
}

int main() {
    // 初始化 TLShub
    if (tlshub_service_init() != 0) {
        return -1;
    }
    
    // 创建服务器 socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(8080)
    };
    
    bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_fd, 10);
    
    printf("服务器监听在端口 8080\n");
    
    // 接受连接
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(server_fd, 
                              (struct sockaddr*)&client_addr, 
                              &addr_len);
        
        if (client_fd >= 0) {
            handle_client(client_fd, &client_addr);
            close(client_fd);
        }
    }
    
    close(server_fd);
    return 0;
}
```

### 场景 3: 多线程应用

TLShub API 支持多线程（`tlshub_handshake()` 使用 thread-local 存储）：

```c
#include "tlshub.h"
#include <pthread.h>
#include <stdio.h>

void* worker_thread(void* arg) {
    int thread_id = *(int*)arg;
    
    // 每个线程可以独立调用 tlshub_handshake
    uint32_t client_ip = inet_addr("10.0.1.100");
    uint32_t server_ip = inet_addr("10.0.2.200");
    unsigned short client_port = 8000 + thread_id;
    unsigned short server_port = 9000 + thread_id;
    
    printf("线程 %d: 开始握手\n", thread_id);
    
    if (tlshub_handshake(client_ip, server_ip, 
                         client_port, server_port) == 0) {
        printf("线程 %d: 握手成功\n", thread_id);
    } else {
        fprintf(stderr, "线程 %d: 握手失败\n", thread_id);
    }
    
    return NULL;
}

int main() {
    // 初始化（仅在主线程调用一次）
    if (tlshub_service_init() != 0) {
        return -1;
    }
    
    // 创建多个工作线程
    pthread_t threads[5];
    int thread_ids[5];
    
    for (int i = 0; i < 5; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, worker_thread, &thread_ids[i]);
    }
    
    // 等待线程完成
    for (int i = 0; i < 5; i++) {
        pthread_join(threads[i], NULL);
    }
    
    return 0;
}
```

## 常见集成场景

### 与现有 TLS 库共存

如果您的项目已经使用了 OpenSSL 或其他 TLS 库，可以让 TLShub 与它们共存：

```c
#include "tlshub.h"
#include <openssl/ssl.h>

// 模式选择
enum {
    USE_OPENSSL,
    USE_TLSHUB
} tls_mode = USE_TLSHUB;

int establish_tls_connection(const char *server_ip, unsigned short server_port) {
    if (tls_mode == USE_TLSHUB) {
        // 使用 TLShub
        uint32_t client_ip = get_local_ip();
        uint32_t server_ip_bin = inet_addr(server_ip);
        return tlshub_handshake(client_ip, server_ip_bin, 
                                get_local_port(), server_port);
    } else {
        // 使用 OpenSSL
        SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
        // ... OpenSSL 握手逻辑
    }
}
```

### 与 KTLS 集成

TLShub 获取的密钥可以用于配置 KTLS：

```c
#include "tlshub.h"
#include <linux/tls.h>
#include <sys/socket.h>

int configure_ktls_with_tlshub(int sockfd,
                                uint32_t client_ip,
                                uint32_t server_ip,
                                unsigned short client_port,
                                unsigned short server_port) {
    // 获取密钥
    struct key_back key = tlshub_fetch_key(client_ip, server_ip, 
                                           client_port, server_port);
    
    if (key.status != 0) {
        // 先握手
        if (tlshub_handshake(client_ip, server_ip, 
                             client_port, server_port) != 0) {
            return -1;
        }
        key = tlshub_fetch_key(client_ip, server_ip, 
                               client_port, server_port);
    }
    
    if (key.status != 0) {
        return -1;
    }
    
    // 配置 KTLS
    struct tls12_crypto_info_aes_gcm_128 crypto_info = {
        .info = {
            .version = TLS_1_2_VERSION,
            .cipher_type = TLS_CIPHER_AES_GCM_128,
        },
    };
    
    // 复制密钥
    memcpy(crypto_info.key, key.masterkey, 16);
    // 设置 salt 和 IV（从密钥派生或使用固定值）
    // ...
    
    // 启用 KTLS
    setsockopt(sockfd, SOL_TLS, TLS_TX, &crypto_info, sizeof(crypto_info));
    setsockopt(sockfd, SOL_TLS, TLS_RX, &crypto_info, sizeof(crypto_info));
    
    return 0;
}
```

## 故障排查

### 问题 1: 编译错误 - 找不到头文件

**错误信息：**
```
fatal error: tlshub.h: No such file or directory
```

**解决方法：**
```bash
# 检查头文件路径
ls -la /path/to/tlshub/tlshub.h

# 修改 Makefile 的 INCLUDES
INCLUDES = -I/correct/path/to/tlshub
```

### 问题 2: 链接错误 - 未定义的引用

**错误信息：**
```
undefined reference to `tlshub_service_init'
```

**解决方法：**
```makefile
# 确保 tlshub.c 被编译并链接
SRCS += tlshub.c

# 或者直接链接目标文件
your_app: main.o tlshub.o
	$(CC) -o $@ $^
```

### 问题 3: 运行时错误 - 权限不足

**错误信息：**
```
Failed to create netlink socket: Operation not permitted
```

**解决方法：**
```bash
# 以 root 权限运行
sudo ./your_app

# 或者赋予 CAP_NET_ADMIN 能力
sudo setcap cap_net_admin+ep ./your_app
```

### 问题 4: 内核模块未加载

**错误信息：**
```
Failed to receive init response
```

**解决方法：**
```bash
# 检查模块是否加载
lsmod | grep tlshub

# 加载模块
sudo insmod /path/to/tlshub.ko

# 检查内核日志
dmesg | tail -20
```

### 问题 5: 字节序错误

**症状：** 握手失败，或者获取密钥时出现奇怪的错误

**解决方法：**
```c
// ✓ 正确：使用 inet_addr
uint32_t ip = inet_addr("192.168.1.1");

// ✓ 正确：端口使用主机字节序
unsigned short port = 8080;

// ✗ 错误：不要手动转换
uint32_t ip = htonl(inet_addr("192.168.1.1"));  // 错误！
unsigned short port = htons(8080);  // 错误！
```

## 最佳实践

1. **初始化时机**
   - 在程序启动早期调用 `tlshub_service_init()`
   - 只调用一次，不要在每个连接建立时都调用

2. **错误处理**
   - 始终检查返回值
   - 记录错误日志便于调试
   - 实现重试机制

3. **性能优化**
   - 在单独的线程中处理握手，避免阻塞主线程
   - 缓存已获取的密钥
   - 使用连接池减少握手次数

4. **调试**
   - 开发阶段启用 DEBUG_LEVEL_DEBUG
   - 生产环境使用 DEBUG_LEVEL_ERR
   - 定期检查内核日志

5. **测试**
   - 编写单元测试验证集成
   - 测试错误场景（网络断开、模块未加载等）
   - 进行压力测试确保稳定性

## 总结

通过本指南，您应该能够：
- 理解 TLShub 的集成流程
- 在自己的项目中正确使用 TLShub API
- 处理常见的集成问题
- 参考 capture 项目的最佳实践

如果遇到问题，请查阅：
- [API 文档](./TLSHUB_API_CN.md)
- [TLShub 项目](https://github.com/lin594/tlshub)
- [提交 Issue](https://github.com/CapooL/tlshub-ebpf/issues)
