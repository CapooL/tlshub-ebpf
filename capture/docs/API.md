# TLShub 流量捕获模块 - API 文档

## 目录

1. [Pod-Node 映射 API](#pod-node-映射-api)
2. [密钥提供者 API](#密钥提供者-api)
3. [TLSHub 客户端 API](#tlshub-客户端-api)
4. [KTLS 配置 API](#ktls-配置-api)
5. [数据结构](#数据结构)

---

## Pod-Node 映射 API

### init_pod_node_mapping

**函数原型**
```c
struct pod_node_table* init_pod_node_mapping(const char *config_file);
```

**功能描述**

从配置文件初始化 Pod-Node 映射表。

**参数**
- `config_file`: 配置文件路径

**返回值**
- 成功：返回映射表指针
- 失败：返回 NULL

**配置文件格式**
```
# 注释行
pod_name node_name
web-pod-1 node-1
api-pod-1 node-2
```

**示例**
```c
struct pod_node_table *table;
table = init_pod_node_mapping("/etc/tlshub/pod_node_mapping.conf");
if (!table) {
    fprintf(stderr, "Failed to initialize pod-node mapping\n");
    return -1;
}
```

---

### get_node_by_pod

**函数原型**
```c
const char* get_node_by_pod(struct pod_node_table *table, const char *pod_name);
```

**功能描述**

根据 Pod 名称查找对应的 Node。

**参数**
- `table`: 映射表指针
- `pod_name`: Pod 名称

**返回值**
- 成功：返回 Node 名称字符串
- 失败：返回 NULL

**示例**
```c
const char *node_name;
node_name = get_node_by_pod(table, "web-pod-1");
if (node_name) {
    printf("Pod web-pod-1 is on node: %s\n", node_name);
} else {
    printf("Pod not found\n");
}
```

---

### free_pod_node_mapping

**函数原型**
```c
void free_pod_node_mapping(struct pod_node_table *table);
```

**功能描述**

释放映射表资源。

**参数**
- `table`: 映射表指针

**返回值**

无

**示例**
```c
free_pod_node_mapping(table);
```

---

### print_pod_node_mapping

**函数原型**
```c
void print_pod_node_mapping(struct pod_node_table *table);
```

**功能描述**

打印映射表内容（调试用）。

**参数**
- `table`: 映射表指针

**返回值**

无

**输出格式**
```
Pod-Node Mapping Table (6 entries):
Pod Name                       Node Name
------------------------------------------------------------
web-pod-1                      node-1
web-pod-2                      node-1
api-pod-1                      node-2
```

**示例**
```c
print_pod_node_mapping(table);
```

---

## 密钥提供者 API

### key_provider_init

**函数原型**
```c
int key_provider_init(enum key_provider_mode mode);
```

**功能描述**

初始化密钥提供者。

**参数**
- `mode`: 密钥提供者模式
  - `MODE_TLSHUB`: 使用 TLSHub
  - `MODE_OPENSSL`: 使用 OpenSSL
  - `MODE_BORINGSSL`: 使用 BoringSSL

**返回值**
- 成功：返回 0
- 失败：返回负值

**示例**
```c
int ret;
ret = key_provider_init(MODE_TLSHUB);
if (ret < 0) {
    fprintf(stderr, "Failed to initialize key provider\n");
    return -1;
}
```

---

### key_provider_cleanup

**函数原型**
```c
void key_provider_cleanup(void);
```

**功能描述**

清理密钥提供者资源。

**参数**

无

**返回值**

无

**示例**
```c
key_provider_cleanup();
```

---

### key_provider_get_key

**函数原型**
```c
int key_provider_get_key(struct flow_tuple *tuple, struct tls_key_info *key_info);
```

**功能描述**

根据四元组获取 TLS 密钥。

**工作流程（TLSHub 模式）**
1. 尝试调用 `tlshub_fetch_key()` 获取密钥
2. 如果失败，调用 `tlshub_handshake()` 发起握手
3. 握手成功后，再次调用 `tlshub_fetch_key()` 获取密钥

**参数**
- `tuple`: 四元组信息（源/目的 IP 和端口）
- `key_info`: 用于存储获取的密钥信息

**返回值**
- 成功：返回 0，`key_info` 包含密钥
- 失败：返回负值

**示例**
```c
struct flow_tuple tuple = {
    .saddr = 0xC0A80164,  // 192.168.1.100
    .daddr = 0x0A000032,  // 10.0.0.50
    .sport = 45678,
    .dport = 443
};
struct tls_key_info key_info;

int ret = key_provider_get_key(&tuple, &key_info);
if (ret == 0) {
    printf("Got key: len=%u\n", key_info.key_len);
}
```

---

### key_provider_set_mode

**函数原型**
```c
void key_provider_set_mode(enum key_provider_mode mode);
```

**功能描述**

设置密钥提供者模式（运行时切换）。

**参数**
- `mode`: 新的密钥提供者模式

**返回值**

无

**示例**
```c
key_provider_set_mode(MODE_OPENSSL);
```

---

### key_provider_get_mode

**函数原型**
```c
enum key_provider_mode key_provider_get_mode(void);
```

**功能描述**

获取当前密钥提供者模式。

**参数**

无

**返回值**

当前的密钥提供者模式

**示例**
```c
enum key_provider_mode mode = key_provider_get_mode();
printf("Current mode: %d\n", mode);
```

---

## TLSHub 客户端 API

### tlshub_client_init

**函数原型**
```c
int tlshub_client_init(void);
```

**功能描述**

初始化 TLSHub 客户端，创建 Netlink socket。

**参数**

无

**返回值**
- 成功：返回 0
- 失败：返回负值

**示例**
```c
int ret = tlshub_client_init();
if (ret < 0) {
    fprintf(stderr, "Failed to initialize TLSHub client\n");
    return -1;
}
```

---

### tlshub_client_cleanup

**函数原型**
```c
void tlshub_client_cleanup(void);
```

**功能描述**

清理 TLSHub 客户端，关闭 Netlink socket。

**参数**

无

**返回值**

无

**示例**
```c
tlshub_client_cleanup();
```

---

### tlshub_fetch_key

**函数原型**
```c
int tlshub_fetch_key(struct flow_tuple *tuple, struct tls_key_info *key_info);
```

**功能描述**

通过 Netlink 从 TLSHub 获取密钥。

**通信协议**
- 消息类型：`NLMSG_TYPE_FETCH_KEY`
- 请求数据：`struct flow_tuple`
- 响应数据：`struct tls_key_info`

**参数**
- `tuple`: 四元组信息
- `key_info`: 用于存储获取的密钥

**返回值**
- 成功：返回 0
- 失败：返回负值

**示例**
```c
struct flow_tuple tuple = { /* ... */ };
struct tls_key_info key_info;

int ret = tlshub_fetch_key(&tuple, &key_info);
if (ret < 0) {
    fprintf(stderr, "Failed to fetch key from TLSHub\n");
}
```

---

### tlshub_handshake

**函数原型**
```c
int tlshub_handshake(struct flow_tuple *tuple);
```

**功能描述**

通过 Netlink 请求 TLSHub 发起握手。

**通信协议**
- 消息类型：`NLMSG_TYPE_HANDSHAKE`
- 请求数据：`struct flow_tuple`
- 响应数据：`int`（握手结果）

**参数**
- `tuple`: 四元组信息

**返回值**
- 成功：返回 0
- 失败：返回负值

**示例**
```c
struct flow_tuple tuple = { /* ... */ };

int ret = tlshub_handshake(&tuple);
if (ret < 0) {
    fprintf(stderr, "TLSHub handshake failed\n");
}
```

---

## KTLS 配置 API

### configure_ktls

**函数原型**
```c
int configure_ktls(int sockfd, struct tls_key_info *key_info);
```

**功能描述**

为 Socket 配置 KTLS，包括发送和接收密钥。

**工作流程**
1. 调用 `enable_ktls_tx()` 启用发送加密
2. 调用 `enable_ktls_rx()` 启用接收解密

**参数**
- `sockfd`: Socket 文件描述符
- `key_info`: TLS 密钥信息

**返回值**
- 成功：返回 0
- 失败：返回负值

**示例**
```c
int sockfd = /* 获取 socket fd */;
struct tls_key_info key_info = /* 获取密钥 */;

int ret = configure_ktls(sockfd, &key_info);
if (ret < 0) {
    fprintf(stderr, "Failed to configure KTLS\n");
}
```

---

### enable_ktls_tx

**函数原型**
```c
int enable_ktls_tx(int sockfd, struct tls_key_info *key_info);
```

**功能描述**

为 Socket 启用 KTLS 发送加密。

**系统调用序列**
1. `setsockopt(sockfd, SOL_TCP, TCP_ULP, "tls", ...)`
2. `setsockopt(sockfd, SOL_TLS, TLS_TX, &crypto_info, ...)`

**参数**
- `sockfd`: Socket 文件描述符
- `key_info`: TLS 密钥信息

**返回值**
- 成功：返回 0
- 失败：返回负值

**示例**
```c
int ret = enable_ktls_tx(sockfd, &key_info);
if (ret < 0) {
    perror("Failed to enable KTLS TX");
}
```

---

### enable_ktls_rx

**函数原型**
```c
int enable_ktls_rx(int sockfd, struct tls_key_info *key_info);
```

**功能描述**

为 Socket 启用 KTLS 接收解密。

**系统调用**
- `setsockopt(sockfd, SOL_TLS, TLS_RX, &crypto_info, ...)`

**参数**
- `sockfd`: Socket 文件描述符
- `key_info`: TLS 密钥信息

**返回值**
- 成功：返回 0
- 失败：返回负值

**示例**
```c
int ret = enable_ktls_rx(sockfd, &key_info);
if (ret < 0) {
    perror("Failed to enable KTLS RX");
}
```

---

## 数据结构

### flow_tuple

**定义**
```c
struct flow_tuple {
    __u32 saddr;    // 源 IP 地址（网络字节序）
    __u32 daddr;    // 目的 IP 地址（网络字节序）
    __u16 sport;    // 源端口（主机字节序）
    __u16 dport;    // 目的端口（主机字节序）
};
```

**用途**

标识一个 TCP 连接的四元组。

---

### tls_key_info

**定义**
```c
struct tls_key_info {
    __u8 key[32];      // TLS 密钥
    __u8 iv[16];       // 初始化向量
    __u32 key_len;     // 密钥长度（字节）
    __u32 iv_len;      // IV 长度（字节）
};
```

**用途**

存储 TLS 加密所需的密钥材料。

**支持的加密套件**
- AES-GCM-128: key_len=16, iv_len=12

---

### connection_info

**定义**
```c
struct connection_info {
    struct flow_tuple tuple;
    __u32 pid;          // 进程 ID
    __u64 timestamp;    // 捕获时间戳（纳秒）
    __u8 state;         // 连接状态
};
```

**用途**

完整的连接信息，包括四元组、进程和状态。

---

### key_provider_mode

**定义**
```c
enum key_provider_mode {
    MODE_TLSHUB = 0,    // 使用 TLSHub
    MODE_OPENSSL = 1,   // 使用 OpenSSL
    MODE_BORINGSSL = 2, // 使用 BoringSSL
};
```

**用途**

指定密钥提供者类型。

---

### capture_config

**定义**
```c
struct capture_config {
    enum key_provider_mode mode;
    char pod_node_config_path[256];
};
```

**用途**

存储流量捕获模块的配置信息。

---

## 错误码

### 通用错误码

| 错误码 | 含义 |
|--------|------|
| 0 | 成功 |
| -1 | 一般错误 |
| -EINVAL | 无效参数 |
| -ENOMEM | 内存不足 |
| -ENOENT | 文件不存在 |
| -ECONNREFUSED | 连接被拒绝 |
| -ETIMEDOUT | 操作超时 |

### 模块特定错误

- **Pod-Node 映射**
  - 返回 NULL: 初始化失败或查询失败
  
- **密钥提供者**
  - 负值: 密钥获取失败
  
- **TLSHub 客户端**
  - 负值: Netlink 通信失败
  
- **KTLS 配置**
  - 负值: setsockopt 失败

---

## 使用示例

### 完整的连接处理流程

```c
#include "capture.h"
#include "pod_mapping.h"
#include "key_provider.h"
#include "ktls_config.h"

int process_connection(struct flow_tuple *tuple, int sockfd) {
    struct tls_key_info key_info;
    int ret;
    
    /* 1. 获取密钥 */
    ret = key_provider_get_key(tuple, &key_info);
    if (ret < 0) {
        fprintf(stderr, "Failed to get key\n");
        return -1;
    }
    
    /* 2. 配置 KTLS */
    ret = configure_ktls(sockfd, &key_info);
    if (ret < 0) {
        fprintf(stderr, "Failed to configure KTLS\n");
        return -1;
    }
    
    printf("Connection secured successfully\n");
    return 0;
}

int main() {
    struct pod_node_table *table;
    int ret;
    
    /* 初始化 Pod-Node 映射 */
    table = init_pod_node_mapping("/etc/tlshub/pod_node_mapping.conf");
    if (!table) {
        fprintf(stderr, "Failed to init pod-node mapping\n");
        return 1;
    }
    
    /* 初始化密钥提供者 */
    ret = key_provider_init(MODE_TLSHUB);
    if (ret < 0) {
        fprintf(stderr, "Failed to init key provider\n");
        free_pod_node_mapping(table);
        return 1;
    }
    
    /* 处理连接... */
    
    /* 清理 */
    key_provider_cleanup();
    free_pod_node_mapping(table);
    
    return 0;
}
```

---

## 线程安全性

### 线程安全的函数
- `get_node_by_pod()`: 只读操作，线程安全
- `key_provider_get_mode()`: 只读操作，线程安全

### 非线程安全的函数
- `init_pod_node_mapping()`: 初始化操作，不应并发调用
- `key_provider_init()`: 初始化操作，不应并发调用
- `key_provider_get_key()`: 使用全局状态，需要外部同步
- `tlshub_fetch_key()`: 使用全局 socket，需要外部同步
- `tlshub_handshake()`: 使用全局 socket，需要外部同步

### 建议
- 在单线程环境中使用这些 API
- 如需多线程，使用互斥锁保护共享资源

---

## 性能考虑

### 优化建议

1. **批量处理**
   - 批量获取密钥以减少 Netlink 通信次数

2. **缓存**
   - 缓存最近获取的密钥
   - 缓存 Pod-Node 映射查询结果

3. **异步操作**
   - 使用异步 I/O 处理 Netlink 通信
   - 并行处理多个连接

4. **资源复用**
   - 重用 SSL 上下文
   - 重用 Netlink socket

---

## 调试技巧

### 启用详细日志

在调用各个函数前，设置环境变量：
```bash
export CAPTURE_DEBUG=1
```

### 使用 strace 跟踪系统调用

```bash
strace -e trace=socket,setsockopt,sendmsg,recvmsg ./capture
```

### 查看 Netlink 通信

```bash
# 监控 Netlink 消息
nlmon monitor
```

### 检查 KTLS 状态

```bash
# 查看 socket 选项
ss -ntop | grep -A 5 <pid>
```

---

## 常见问题

### Q1: tlshub_fetch_key() 返回 -1

**可能原因**
- TLSHub 内核模块未加载
- Netlink 协议号不匹配
- 四元组信息不存在

**解决方法**
```bash
# 检查内核模块
lsmod | grep tlshub

# 加载内核模块
insmod tlshub.ko
```

### Q2: configure_ktls() 失败

**可能原因**
- 内核不支持 KTLS
- Socket 已经配置过 KTLS
- 密钥格式不正确

**解决方法**
```bash
# 检查内核配置
grep CONFIG_TLS /boot/config-$(uname -r)

# 应该显示: CONFIG_TLS=y
```

### Q3: Pod-Node 映射查询失败

**可能原因**
- 配置文件格式错误
- Pod 名称拼写错误
- 配置文件未更新

**解决方法**
```bash
# 验证配置文件格式
cat /etc/tlshub/pod_node_mapping.conf

# 测试映射
./test_pod_mapping /etc/tlshub/pod_node_mapping.conf
```

---

## 版本历史

- v1.0.0: 初始版本
  - 基本的连接捕获功能
  - TLSHub、OpenSSL、BoringSSL 支持
  - Pod-Node 映射
  - KTLS 配置

---

## 许可证

本 API 文档遵循与项目相同的许可证。
