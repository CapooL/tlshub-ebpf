# TLShub 流量捕获模块

## 概述

TLShub 流量捕获模块是一个基于 eBPF 的 Layer 4 流量捕获和加密系统，专为 TLSHub mTLS 握手模块设计。该模块能够无感劫持应用程序的 TCP 连接，并使用 KTLS（Kernel TLS）技术为连接配置密钥，将普通 TCP 连接转换为安全通道。

## 主要功能

### 1. Socket 连接捕获
- 使用 eBPF 程序在内核层面捕获应用程序的 Socket 连接
- 检测 TCP 三次握手过程
- 无感劫持连接，不影响应用程序正常运行

### 2. KTLS 密钥配置
- 自动为捕获的 TCP 连接配置 KTLS
- 支持 AES-GCM-128 加密算法
- 同时配置发送和接收密钥

### 3. TLSHub 集成
- 通过 Netlink 与 TLSHub 内核模块通信
- 根据四元组（源IP、目的IP、源端口、目的端口）获取密钥
- 支持自动握手：fetchkey 失败时自动发起 handshake

### 4. Pod-Node 映射
- 维护 Pod 到 Node 的映射关系
- 支持配置文件方式管理映射
- 提供查询接口供其他组件使用

### 5. 多密钥提供者支持
- **TLSHub 模式**（推荐）：使用 TLSHub 进行密钥协商
- **OpenSSL 模式**：使用 OpenSSL 进行密钥协商
- **BoringSSL 模式**：使用 BoringSSL 进行密钥协商
- 支持运行时切换模式，便于性能对比测试

## 架构设计

```
┌─────────────────────────────────────────────────────────────┐
│                      用户态程序 (main.c)                      │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │ Pod-Node     │  │ Key Provider │  │ KTLS Config  │      │
│  │ Mapping      │  │              │  │              │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
│         │                  │                  │              │
│         │                  │                  │              │
│         └──────────────────┴──────────────────┘              │
│                            │                                 │
└────────────────────────────┼─────────────────────────────────┘
                             │
                             │ Perf Buffer Events
                             │
┌────────────────────────────┼─────────────────────────────────┐
│                            │                                 │
│                     eBPF 程序 (capture.bpf.c)                │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │ tcp_connect  │  │ tcp_sendmsg  │  │ Connection   │      │
│  │ Hook         │  │ Hook         │  │ Tracking     │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
│                                                              │
└──────────────────────────────────────────────────────────────┘
                             │
                             │ Netlink
                             │
┌────────────────────────────┼─────────────────────────────────┐
│                            │                                 │
│                     TLSHub 内核模块                          │
│                   (tlshub/src/*)                            │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

## 目录结构

```
capture/
├── include/              # 头文件
│   ├── capture.h        # 核心数据结构定义
│   ├── pod_mapping.h    # Pod-Node 映射接口
│   ├── tlshub_client.h  # TLSHub 客户端接口
│   ├── ktls_config.h    # KTLS 配置接口
│   └── key_provider.h   # 密钥提供者接口
├── src/                 # 源代码
│   ├── main.c           # 主程序
│   ├── capture.bpf.c    # eBPF 程序
│   ├── pod_mapping.c    # Pod-Node 映射实现
│   ├── tlshub_client.c  # TLSHub 客户端实现
│   ├── ktls_config.c    # KTLS 配置实现
│   └── key_provider.c   # 密钥提供者实现
├── config/              # 配置文件
│   ├── capture.conf     # 主配置文件
│   └── pod_node_mapping.conf  # Pod-Node 映射配置
├── test/                # 测试文件
│   ├── test.sh          # 测试脚本
│   └── test_pod_mapping.c  # Pod-Node 映射测试
├── docs/                # 文档
│   ├── README.md        # 本文件
│   ├── ARCHITECTURE.md  # 架构设计文档
│   └── API.md           # API 文档
└── Makefile             # 编译配置
```

## 编译安装

### 系统要求

- Linux 内核版本 >= 5.2（支持 eBPF 和 KTLS）
- 编译工具：gcc, clang, make
- 依赖库：libbpf, libssl (OpenSSL)

### 安装依赖

#### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install -y clang llvm gcc make \
    libbpf-dev libssl-dev linux-headers-$(uname -r)
```

#### CentOS/RHEL
```bash
sudo yum install -y clang llvm gcc make \
    libbpf-devel openssl-devel kernel-devel
```

### 编译

```bash
cd capture/
make
```

编译成功后会生成以下文件：
- `capture`: 用户态主程序
- `capture.bpf.o`: eBPF 字节码

### 安装

```bash
sudo make install
```

这会将以下文件安装到系统：
- `/usr/local/bin/capture`: 主程序
- `/usr/local/lib/capture.bpf.o`: eBPF 程序
- `/etc/tlshub/capture.conf`: 配置文件
- `/etc/tlshub/pod_node_mapping.conf`: Pod-Node 映射配置

## 配置说明

### 主配置文件 (capture.conf)

```ini
# 密钥提供者模式
# 可选值: tlshub, openssl, boringssl
mode = tlshub

# Pod-Node 映射配置文件路径
pod_node_config = /etc/tlshub/pod_node_mapping.conf

# Netlink 协议号
netlink_protocol = 31

# 日志级别
log_level = info

# 是否启用详细日志
verbose = false
```

### Pod-Node 映射配置 (pod_node_mapping.conf)

```
# 格式: pod_name node_name
web-pod-1 node-1
web-pod-2 node-1
api-pod-1 node-2
api-pod-2 node-2
db-pod-1 node-3
```

## 使用方法

### 启动流量捕获模块

```bash
# 使用默认配置
sudo ./capture

# 使用自定义配置
sudo ./capture /path/to/custom/capture.conf
```

### 运行时日志

程序运行时会输出以下信息：
```
TLShub Traffic Capture Module
==============================

Loading configuration from /etc/tlshub/capture.conf
Configuration:
  Mode: 0
  Pod-Node Config: /etc/tlshub/pod_node_mapping.conf

Initializing Pod-Node mapping...
Loaded 6 pod-node mappings from /etc/tlshub/pod_node_mapping.conf

Initializing key provider (mode: 0)...
TLSHub client initialized (netlink socket: 3)

Loading eBPF program...
Attaching eBPF programs...
  Attached: kprobe/tcp_v4_connect
  Attached: kretprobe/tcp_v4_connect
  Attached: kprobe/tcp_sendmsg

Capture module is running. Press Ctrl+C to stop.
Monitoring TCP connections...
```

### 捕获到连接时的输出

```
=== New TCP Connection Detected ===
Source: 192.168.1.100:45678
Destination: 10.0.0.50:443
PID: 12345
Timestamp: 1234567890123456
Fetching TLS key...
Fetched TLS key from TLSHub (key_len: 32)
TLS key obtained successfully (key_len: 32, iv_len: 16)
```

## 测试

### 运行测试脚本

```bash
cd capture/test/
sudo ./test.sh
```

### 测试 Pod-Node 映射

```bash
cd capture/test/
gcc -o test_pod_mapping test_pod_mapping.c ../src/pod_mapping.c -I../include
./test_pod_mapping ../config/pod_node_mapping.conf
```

## 性能对比

### 测试不同密钥提供者模式

1. **TLSHub 模式测试**
```bash
# 编辑配置文件，设置 mode = tlshub
sudo ./capture /etc/tlshub/capture.conf
```

2. **OpenSSL 模式测试**
```bash
# 编辑配置文件，设置 mode = openssl
sudo ./capture /etc/tlshub/capture.conf
```

3. **BoringSSL 模式测试**
```bash
# 编辑配置文件，设置 mode = boringssl
sudo ./capture /etc/tlshub/capture.conf
```

### 性能指标

建议测试的性能指标：
- 连接建立延迟
- 密钥协商时间
- 吞吐量
- CPU 使用率
- 内存使用量

## API 文档

详细的 API 文档请参考 [API.md](API.md)。

### 主要接口

#### Pod-Node 映射
```c
struct pod_node_table* init_pod_node_mapping(const char *config_file);
const char* get_node_by_pod(struct pod_node_table *table, const char *pod_name);
void free_pod_node_mapping(struct pod_node_table *table);
```

#### 密钥提供者
```c
int key_provider_init(enum key_provider_mode mode);
int key_provider_get_key(struct flow_tuple *tuple, struct tls_key_info *key_info);
void key_provider_set_mode(enum key_provider_mode mode);
```

#### TLSHub 客户端
```c
int tlshub_client_init(void);
int tlshub_fetch_key(struct flow_tuple *tuple, struct tls_key_info *key_info);
int tlshub_handshake(struct flow_tuple *tuple);
void tlshub_client_cleanup(void);
```

#### KTLS 配置
```c
int configure_ktls(int sockfd, struct tls_key_info *key_info);
int enable_ktls_tx(int sockfd, struct tls_key_info *key_info);
int enable_ktls_rx(int sockfd, struct tls_key_info *key_info);
```

## 故障排查

### 常见问题

1. **eBPF 程序加载失败**
   - 检查内核版本是否支持 eBPF
   - 检查是否以 root 权限运行
   - 查看内核日志：`dmesg | grep bpf`

2. **Netlink 通信失败**
   - 检查 TLSHub 内核模块是否已加载
   - 验证 Netlink 协议号配置是否正确
   - 检查 SELinux/AppArmor 设置

3. **KTLS 配置失败**
   - 检查内核是否启用 KTLS 支持
   - 验证内核配置：`CONFIG_TLS=y`
   - 检查 TLS 版本和加密套件是否支持

4. **性能问题**
   - 调整 eBPF map 大小
   - 优化 perf buffer 大小
   - 检查是否有大量连接同时建立

### 调试模式

启用详细日志：
```bash
# 编辑配置文件
verbose = true
log_level = debug

# 运行程序
sudo ./capture /etc/tlshub/capture.conf
```

查看内核日志：
```bash
sudo cat /sys/kernel/debug/tracing/trace_pipe
```

## 开发指南

### 添加新的密钥提供者

1. 在 `include/capture.h` 中添加新模式：
```c
enum key_provider_mode {
    MODE_TLSHUB = 0,
    MODE_OPENSSL = 1,
    MODE_BORINGSSL = 2,
    MODE_YOUR_PROVIDER = 3,  // 新增
};
```

2. 在 `src/key_provider.c` 中实现接口：
```c
static int your_provider_get_key(struct flow_tuple *tuple, 
                                  struct tls_key_info *key_info) {
    // 实现密钥获取逻辑
    return 0;
}
```

3. 在 `key_provider_init()` 和 `key_provider_get_key()` 中添加分支处理

### 扩展 eBPF 功能

编辑 `src/capture.bpf.c`，添加新的 hook 点或数据收集逻辑。

## 许可证

本项目采用与 TLSHub 相同的许可证。

## 贡献

欢迎提交 Issue 和 Pull Request！

## 联系方式

如有问题或建议，请通过以下方式联系：
- Issue: https://github.com/CapooL/tlshub-ebpf/issues
- Email: [维护者邮箱]

## 致谢

感谢 TLSHub 项目提供的 mTLS 握手模块支持。
