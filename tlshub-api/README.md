# TLShub 用户态 API

本目录包含 TLShub 内核模块的用户态接口实现。

## 文件说明

### 核心文件

- **tlshub.h** - TLShub API 头文件
  - 定义了所有公开的 API 函数
  - 定义了数据结构（如 `struct key_back`）
  - 定义了调试级别枚举

- **tlshub.c** - TLShub API 实现
  - 实现了与 TLShub 内核模块的 Netlink 通信
  - 提供了初始化、握手、获取密钥等功能
  - 包含辅助函数（如时间测量、十六进制打印）

### 示例程序

- **tlshub_handshake.c** - 握手测试程序
  - 演示如何使用 TLShub API 进行握手
  - 支持多次握手测试
  - 包含超时处理和错误处理示例

### 历史文件（已重命名）

- ~~hypertls.h~~ → tlshub.h
- ~~hypertls.c~~ → tlshub.c
- ~~hypertls_handshake.c~~ → tlshub_handshake.c

**注意：** 为了与 TLShub 项目名称保持一致，所有文件已从 "hypertls" 重命名为 "tlshub"。

## 快速开始

### 1. 编译示例程序

```bash
gcc -o tlshub_handshake_test tlshub_handshake.c tlshub.c -I.
```

### 2. 运行测试（需要 root 权限）

```bash
# 确保 TLShub 内核模块已加载
sudo insmod /path/to/tlshub.ko

# 运行测试
sudo ./tlshub_handshake_test 10.0.1.100 10.0.2.200
```

### 3. 在您的项目中使用

```c
#include "tlshub.h"
#include <arpa/inet.h>

int main() {
    // 1. 初始化 TLShub 服务
    if (tlshub_service_init() != 0) {
        return -1;
    }
    
    // 2. 准备连接信息
    uint32_t client_ip = inet_addr("10.0.1.100");
    uint32_t server_ip = inet_addr("10.0.2.200");
    unsigned short client_port = 8080;
    unsigned short server_port = 9090;
    
    // 3. 发起握手
    int ret = tlshub_handshake(client_ip, server_ip, 
                               client_port, server_port);
    if (ret == 0) {
        printf("握手成功\n");
        
        // 4. 获取密钥
        struct key_back key = tlshub_fetch_key(client_ip, server_ip,
                                               client_port, server_port);
        if (key.status == 0) {
            printf("成功获取密钥\n");
            // 使用 key.masterkey
        }
    }
    
    return 0;
}
```

## API 参考

### 主要函数

#### tlshub_service_init()

```c
int tlshub_service_init(void);
```

初始化 TLShub 服务。必须在使用其他 API 之前调用。

**返回值：**
- `0`: 成功
- `负数`: 失败

---

#### tlshub_handshake()

```c
int tlshub_handshake(uint32_t client_pod_ip,
                     uint32_t server_pod_ip,
                     unsigned short client_pod_port,
                     unsigned short server_pod_port);
```

发起 Pod 级 TLS 握手。

**参数：**
- `client_pod_ip`: 客户端 Pod IP（网络字节序）
- `server_pod_ip`: 服务端 Pod IP（网络字节序）
- `client_pod_port`: 客户端 Pod 端口（主机字节序）
- `server_pod_port`: 服务端 Pod 端口（主机字节序）

**返回值：**
- `0`: 握手成功
- `负数`: 握手失败

---

#### tlshub_fetch_key()

```c
struct key_back tlshub_fetch_key(uint32_t client_pod_ip,
                                 uint32_t server_pod_ip,
                                 unsigned short client_pod_port,
                                 unsigned short server_pod_port);
```

获取 TLS 会话密钥。

**参数：** 同 `tlshub_handshake()`

**返回值：** `struct key_back` 结构体
- `status`: 
  - `0`: 成功
  - `-1`: 握手未完成或出错
  - `-2`: 密钥过期
- `masterkey[32]`: 32 字节主密钥

## 数据结构

### struct key_back

```c
struct key_back {
    int status;                  // 密钥状态
    unsigned char masterkey[32]; // 主密钥
};
```

### 内部结构（仅供参考）

#### struct my_msg

Netlink 消息载荷：

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
- `TLS_SERVICE_INIT (0)`: 初始化
- `TLS_SERVICE_START (1)`: 开始握手
- `TLS_SERVICE_FETCH (2)`: 获取密钥

#### user_msg_info

Netlink 消息容器：

```c
typedef struct _user_msg_info {
    struct nlmsghdr hdr;
    char msg_type;
    char msg[125];
} user_msg_info;
```

**消息类型（msg_type）：**
- `0x01`: 首次握手成功
- `0x02`: 非首次握手成功
- `0x03`: 握手失败
- `0x04`: 日志消息
- `0x05`: 已连接
- `0x07`: 初始化完成

## 字节序说明

### 重要提示

TLShub API 对字节序有特定要求：

#### IP 地址

```c
// ✓ 正确：使用 inet_addr 返回的网络字节序
uint32_t ip = inet_addr("192.168.1.1");
tlshub_handshake(ip, ...);

// ✗ 错误：不要手动转换
uint32_t ip = htonl(inet_addr("192.168.1.1"));  // 错误！
```

#### 端口号

```c
// ✓ 正确：使用主机字节序
unsigned short port = 8080;
tlshub_handshake(..., port, ...);

// ✗ 错误：不要转换为网络字节序
unsigned short port = htons(8080);  // 错误！
tlshub_handshake(..., port, ...);
```

### 字节序流程

```
用户空间                内核空间
inet_addr("192.168.1.1")   
    ↓ (网络字节序)
tlshub_handshake()
    ↓ (网络字节序传输)
内核接收
    ↓ ntohl() 转换
    ↓ (主机字节序)
内部处理
```

## 调试

### 启用调试输出

修改 `tlshub.c` 中的 `debugLevel`：

```c
int debugLevel = DEBUG_LEVEL_DEBUG;  // 显示所有调试信息
```

### 调试级别

- `DEBUG_LEVEL_ALL`: 所有信息
- `DEBUG_LEVEL_INFO`: 信息
- `DEBUG_LEVEL_DEBUG`: 调试（推荐开发时使用）
- `DEBUG_LEVEL_WARNING`: 警告
- `DEBUG_LEVEL_ERR`: 错误（推荐生产环境使用）
- `DEBUG_LEVEL_NONE`: 无输出

### 查看内核日志

```bash
# 实时查看
dmesg -w | grep -i tlshub

# 查看最近的日志
dmesg | tail -50
```

## 线程安全

### 线程安全的函数

- `tlshub_handshake()` - 使用 thread-local 存储，多线程安全
- `tlshub_fetch_key()` - 每次创建新上下文，多线程安全

### 非线程安全的函数

- `tlshub_service_init()` - 应该只在单线程环境中调用一次

### 多线程使用建议

```c
// 在主线程初始化
int main() {
    tlshub_service_init();
    
    // 创建工作线程
    pthread_t threads[10];
    for (int i = 0; i < 10; i++) {
        pthread_create(&threads[i], NULL, worker, NULL);
    }
    // ...
}

// 工作线程可以安全地调用握手
void* worker(void* arg) {
    tlshub_handshake(...);  // 线程安全
    tlshub_fetch_key(...);   // 线程安全
    return NULL;
}
```

## 性能考虑

### Netlink 通信开销

- 每次 API 调用涉及用户态-内核态切换
- 建议：批量处理连接，减少调用次数

### 握手延迟

- TLS 握手可能需要几秒钟
- 建议：使用异步方式或单独线程处理

### 密钥缓存

- 考虑在应用层缓存密钥
- 密钥过期时重新握手

## 常见问题

### Q: 为什么从 hypertls 改名为 tlshub？

**A:** 为了与 TLShub 项目名称保持一致，提高代码可读性。所有功能保持不变。

### Q: 必须以 root 权限运行吗？

**A:** 是的，Netlink socket 需要 `CAP_NET_ADMIN` 权限。可以使用 `sudo` 或 `setcap` 授予权限。

### Q: 支持 IPv6 吗？

**A:** 当前版本只支持 IPv4。

### Q: 如何处理握手超时？

**A:** 可以使用 `alarm()` 和信号处理实现超时机制。参见 `tlshub_handshake.c` 中的示例。

### Q: 可以在握手前获取密钥吗？

**A:** 可以尝试，但如果握手未完成，会返回 `status = -1`。建议先握手后获取密钥。

## 更多文档

- **完整 API 文档**: [docs/TLSHUB_API_CN.md](../docs/TLSHUB_API_CN.md)
- **集成指南**: [docs/INTEGRATION_GUIDE_CN.md](../docs/INTEGRATION_GUIDE_CN.md)
- **项目主页**: [README.md](../README.md)

## 参考项目

本 API 的实际应用示例：
- [capture](../capture/) - eBPF 流量捕获模块，展示了如何在实际项目中集成 TLShub

## 许可证

与 TLShub 项目使用相同的许可证。

## 贡献

欢迎提交 Issue 和 Pull Request！
- Issue: https://github.com/CapooL/tlshub-ebpf/issues
