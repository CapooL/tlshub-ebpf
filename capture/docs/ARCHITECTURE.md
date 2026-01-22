# TLShub 流量捕获模块 - 架构设计

## 1. 系统概述

TLShub 流量捕获模块是一个分层架构的系统，由 eBPF 内核程序和用户态控制程序组成。系统的核心目标是在应用程序无感知的情况下，为 TCP 连接建立安全通道。

## 2. 分层架构

### 2.1 内核层 (eBPF)

#### 职责
- 捕获应用程序的 TCP 连接事件
- 收集连接的四元组信息
- 跟踪连接状态变化
- 通过 perf buffer 向用户态发送事件

#### 关键组件

**Hook 点**
1. `kprobe/tcp_v4_connect`: 捕获 TCP 连接发起
2. `kretprobe/tcp_v4_connect`: 捕获 TCP 连接完成
3. `kprobe/tcp_sendmsg`: 捕获首次数据发送（连接已建立）

**eBPF Maps**
1. `conn_track_map`: 连接状态跟踪表
   - Key: Socket 指针
   - Value: 连接状态（1=连接中, 2=已处理）
   
2. `sock_info_map`: Socket 信息表
   - Key: Socket 指针
   - Value: 四元组信息 + PID

3. `events`: Perf 事件数组
   - 用于向用户态发送连接事件

### 2.2 用户态层

#### 主程序 (main.c)
- 加载和管理 eBPF 程序
- 接收和处理内核事件
- 协调各个功能模块
- 生命周期管理

#### 功能模块

**1. Pod-Node 映射模块 (pod_mapping.c)**
```
┌─────────────────────────┐
│  pod_node_mapping.conf  │
└────────┬────────────────┘
         │ 读取
         ▼
┌─────────────────────────┐
│  Pod-Node 映射表         │
│  ┌───────────────────┐  │
│  │ web-pod-1  node-1 │  │
│  │ api-pod-1  node-2 │  │
│  │ db-pod-1   node-3 │  │
│  └───────────────────┘  │
└─────────────────────────┘
         │ 查询
         ▼
    返回 Node 名称
```

**功能**：
- 从配置文件加载 Pod-Node 映射关系
- 提供快速查询接口
- 支持动态更新（预留）

**2. 密钥提供者模块 (key_provider.c)**
```
┌──────────────────────────────────────┐
│        Key Provider Interface         │
├──────────────────────────────────────┤
│  key_provider_get_key()               │
│  key_provider_set_mode()              │
└──────┬───────────┬───────────┬────────┘
       │           │           │
       ▼           ▼           ▼
┌─────────┐ ┌──────────┐ ┌───────────┐
│ TLSHub  │ │ OpenSSL  │ │ BoringSSL │
│  Mode   │ │   Mode   │ │   Mode    │
└─────────┘ └──────────┘ └───────────┘
```

**功能**：
- 抽象密钥获取接口
- 支持多种密钥提供者
- 运行时模式切换
- 统一错误处理

**3. TLSHub 客户端模块 (tlshub_client.c)**
```
┌──────────────────┐
│  用户态程序       │
│  tlshub_client   │
└────────┬─────────┘
         │ Netlink
         │ (NETLINK_TLSHUB)
         ▼
┌──────────────────┐
│  内核态模块       │
│  TLSHub          │
└──────────────────┘
```

**功能**：
- 通过 Netlink 与 TLSHub 内核模块通信
- 实现 fetchkey 操作
- 实现 handshake 操作
- 处理异步响应

**消息流程**：
```
1. fetchkey 流程：
   用户态 → [NLMSG_TYPE_FETCH_KEY + 四元组] → 内核
   内核 → [密钥信息] → 用户态

2. handshake 流程：
   用户态 → [NLMSG_TYPE_HANDSHAKE + 四元组] → 内核
   内核 → [握手结果] → 用户态
   
3. 完整流程：
   fetchkey() → 失败 → handshake() → fetchkey() → 成功
```

**4. KTLS 配置模块 (ktls_config.c)**
```
┌──────────────────┐
│  应用程序 Socket  │
└────────┬─────────┘
         │
         ▼
┌──────────────────────────────┐
│  KTLS Configuration           │
│                               │
│  1. setsockopt(TCP_ULP, tls)  │
│  2. setsockopt(TLS_TX, key)   │
│  3. setsockopt(TLS_RX, key)   │
└────────┬─────────────────────┘
         │
         ▼
┌──────────────────┐
│  Kernel TLS       │
│  (加密/解密)      │
└──────────────────┘
```

**功能**：
- 为 Socket 启用 KTLS
- 配置发送密钥 (TLS_TX)
- 配置接收密钥 (TLS_RX)
- 支持 AES-GCM-128 加密

## 3. 数据流

### 3.1 连接捕获流程

```
应用程序调用 connect()
         │
         ▼
    [内核 TCP 栈]
         │
         ├─→ kprobe/tcp_v4_connect
         │   └─→ 记录连接状态
         │
         ├─→ TCP 三次握手
         │
         ├─→ kretprobe/tcp_v4_connect
         │   └─→ 确认连接完成
         │
         ▼
应用程序调用 send()
         │
         ▼
    kprobe/tcp_sendmsg
         │
         ├─→ 提取四元组信息
         ├─→ 发送 perf event
         └─→ 标记为已处理
         │
         ▼
    用户态接收事件
```

### 3.2 密钥获取流程

```
用户态接收连接事件
         │
         ▼
    准备四元组信息
         │
         ▼
┌────────────────────┐
│ key_provider_get_  │
│ key()              │
└─────────┬──────────┘
          │
          ▼
    [根据模式选择]
          │
          ├─→ TLSHub 模式
          │   │
          │   ├─→ tlshub_fetch_key()
          │   │   └─→ 成功 → 返回密钥
          │   │   └─→ 失败 ↓
          │   │
          │   ├─→ tlshub_handshake()
          │   │   └─→ 发起握手
          │   │
          │   └─→ tlshub_fetch_key()
          │       └─→ 再次获取密钥
          │
          ├─→ OpenSSL 模式
          │   └─→ 使用 OpenSSL API
          │
          └─→ BoringSSL 模式
              └─→ 使用 BoringSSL API
          │
          ▼
    返回密钥信息
```

### 3.3 KTLS 配置流程

```
获取到密钥信息
         │
         ▼
    configure_ktls(sockfd, key_info)
         │
         ├─→ enable_ktls_tx()
         │   │
         │   ├─→ setsockopt(TCP_ULP, "tls")
         │   │   └─→ 启用 TLS ULP
         │   │
         │   └─→ setsockopt(TLS_TX, crypto_info)
         │       └─→ 配置发送密钥
         │
         └─→ enable_ktls_rx()
             │
             └─→ setsockopt(TLS_RX, crypto_info)
                 └─→ 配置接收密钥
         │
         ▼
    KTLS 配置完成
    后续数据自动加密/解密
```

## 4. 关键数据结构

### 4.1 连接信息

```c
struct flow_tuple {
    __u32 saddr;    // 源 IP 地址
    __u32 daddr;    // 目的 IP 地址
    __u16 sport;    // 源端口
    __u16 dport;    // 目的端口
};

struct connection_info {
    struct flow_tuple tuple;
    __u32 pid;          // 进程 ID
    __u64 timestamp;    // 时间戳
    __u8 state;         // 连接状态
};
```

### 4.2 密钥信息

```c
struct tls_key_info {
    __u8 key[32];       // TLS 密钥
    __u8 iv[16];        // 初始化向量
    __u32 key_len;      // 密钥长度
    __u32 iv_len;       // IV 长度
};
```

### 4.3 配置信息

```c
enum key_provider_mode {
    MODE_TLSHUB = 0,
    MODE_OPENSSL = 1,
    MODE_BORINGSSL = 2,
};

struct capture_config {
    enum key_provider_mode mode;
    char pod_node_config_path[256];
};
```

## 5. 并发控制

### 5.1 eBPF 层
- eBPF Map 操作自动提供原子性
- 使用 BPF_ANY 策略进行 map 更新
- Perf buffer 提供无锁队列

### 5.2 用户态层
- 单线程事件循环模型
- 使用 perf_buffer__poll() 等待事件
- 顺序处理每个连接事件

## 6. 错误处理

### 6.1 eBPF 层
- 使用 bpf_printk 记录调试信息
- 忽略无法处理的连接类型
- 优雅处理 probe_read 失败

### 6.2 用户态层
- 每个模块返回错误码
- 统一的错误日志格式
- 失败时继续处理其他连接

### 6.3 降级策略
```
TLSHub 模式失败
    ↓
切换到 OpenSSL 模式
    ↓
记录警告日志
    ↓
继续运行
```

## 7. 性能优化

### 7.1 eBPF 层优化
- 最小化 Map 查找次数
- 只在必要时发送 perf event
- 使用状态标记避免重复处理

### 7.2 用户态层优化
- 批量处理 perf events
- 缓存 Pod-Node 映射表
- 重用 SSL 上下文

### 7.3 内存管理
- 使用固定大小的 Map
- 及时清理过期连接
- 避免内存泄漏

## 8. 安全考虑

### 8.1 权限管理
- 需要 root 权限运行
- 使用 CAP_BPF 和 CAP_NET_ADMIN

### 8.2 密钥保护
- 密钥仅在内存中传递
- 不记录密钥到日志
- 及时清零敏感数据

### 8.3 攻击防护
- 限制 Map 大小防止 DoS
- 验证 Netlink 消息来源
- 检查配置文件权限

## 9. 可扩展性

### 9.1 支持新的密钥提供者
- 实现 `key_provider` 接口
- 添加到模式枚举
- 更新配置解析

### 9.2 支持新的协议
- 添加新的 eBPF hook 点
- 扩展四元组结构
- 更新事件处理逻辑

### 9.3 支持新的加密算法
- 扩展 `tls_key_info` 结构
- 更新 KTLS 配置逻辑
- 添加算法协商机制

## 10. 调试和监控

### 10.1 日志系统
- 分级日志 (debug, info, warning, error)
- 详细的操作日志
- 性能统计信息

### 10.2 调试工具
- bpftool: 查看 eBPF 程序和 Map
- trace_pipe: 查看内核日志
- perf: 性能分析

### 10.3 监控指标
- 连接捕获数量
- 密钥获取成功率
- KTLS 配置成功率
- 平均处理延迟

## 11. 未来改进

### 11.1 短期目标
- 支持 IPv6
- 支持 UDP 连接
- 动态重载配置

### 11.2 长期目标
- 支持分布式密钥管理
- 集成 Kubernetes API
- Web 管理界面
- 实时性能仪表板

## 12. 参考资料

- Linux Kernel TLS Documentation
- eBPF Documentation
- TLSHub Project Documentation
- OpenSSL API Reference
