# TLS-Perf 工具在 TLShub 握手性能测试中的适用性分析

## 文档信息

- **文档版本**: 1.0.0
- **创建日期**: 2026-01-23
- **作者**: TLShub 开发团队
- **目标**: 分析 tls-perf 工具是否能用于测试 TLShub 握手性能

---

## 目录

- [1. 执行摘要](#1-执行摘要)
- [2. TLS-Perf 工具分析](#2-tls-perf-工具分析)
- [3. TLShub 架构分析](#3-tlshub-架构分析)
- [4. 兼容性分析](#4-兼容性分析)
- [5. 改造方案](#5-改造方案)
- [6. 实施步骤](#6-实施步骤)
- [7. 测试方案](#7-测试方案)
- [8. 替代方案](#8-替代方案)
- [9. 总结与建议](#9-总结与建议)

---

## 1. 执行摘要

### 1.1 核心结论

**TLS-Perf 工具不能直接用于测试 TLShub 握手性能。**

主要原因：
- ✗ TLS-Perf 使用标准 OpenSSL 进行应用层 TLS 握手
- ✗ TLShub 使用内核级 Netlink 通信进行 Pod 级握手
- ✗ 两者的握手协议和通信机制完全不同

### 1.2 推荐方案

**方案一（推荐）**：开发专门的 TLShub 性能测试工具
- ✓ 直接调用 TLShub API 进行握手测试
- ✓ 准确反映 TLShub 的实际性能
- ✓ 开发周期短（约 3-5 天）

**方案二**：改造 TLS-Perf 工具
- ✓ 可重用 TLS-Perf 的基础框架和统计功能
- ✗ 需要大量修改核心握手逻辑
- ✗ 改造复杂度高

---

## 2. TLS-Perf 工具分析

### 2.1 工具概述

TLS-Perf 是由 Tempesta Technologies 开发的 TLS 握手基准测试工具。

**项目信息**：
- GitHub: https://github.com/tempesta-tech/tls-perf
- 语言: C++
- 依赖: OpenSSL/BoringSSL
- 许可证: GPL v2

### 2.2 工作原理

TLS-Perf 的核心工作流程如下：

```
┌─────────────┐
│  tls-perf   │
│   客户端    │
└──────┬──────┘
       │
       │ 1. TCP connect()
       ↓
┌─────────────┐
│  目标服务器  │
│             │
└──────┬──────┘
       │
       │ 2. SSL_connect()
       │    (标准 TLS 握手)
       ↓
┌─────────────┐
│ TLS 握手完成 │
└──────┬──────┘
       │
       │ 3. 立即断开连接
       │ 4. 收集统计数据
       ↓
```

### 2.3 核心技术特点

#### 2.3.1 连接管理

```cpp
// 源码分析：main.cc:575-597
bool Peer::tcp_connect()
{
    // 1. 创建标准 TCP socket
    sd = socket(g_opt.ip.sin6_family, SOCK_STREAM, IPPROTO_TCP);
    
    // 2. 设置非阻塞模式
    fcntl(sd, F_SETFL, fcntl(sd, F_GETFL, 0) | O_NONBLOCK);
    
    // 3. 发起标准 TCP 连接
    int r = connect(sd, (struct sockaddr *)&g_opt.ip, sz);
    
    // 4. 使用 epoll 管理并发连接
}
```

#### 2.3.2 TLS 握手

```cpp
// 源码分析：main.cc:480-525
bool Peer::tls_handshake()
{
    // 1. 创建 OpenSSL TLS 上下文
    if (!tls_) {
        tls_ = io_.new_tls_ctx(this);
    }
    
    // 2. 执行标准 OpenSSL TLS 握手
    int r = SSL_connect(tls_);
    
    // 3. 握手成功后立即断开
    if (r == 1) {
        disconnect();
        io_.queue_reconnect(this);  // 准备下一次握手
        return true;
    }
    
    // 4. 处理 WANT_READ/WANT_WRITE
}
```

#### 2.3.3 性能统计

TLS-Perf 收集以下性能指标：

| 指标 | 说明 | 单位 |
|------|------|------|
| Handshakes/sec | 每秒完成的握手次数 | 次/秒 |
| Latency | 单次握手延迟 | 微秒 |
| 95th Percentile | 95分位延迟 | 微秒 |
| Concurrent Connections | 并发连接数 | 个 |
| Error Count | 错误次数 | 次 |

### 2.4 使用示例

```bash
# 测试 TLS 1.3 握手，10秒，8线程，每线程100并发
./tls-perf -T 10 -l 100 -t 8 --tls 1.3 192.168.1.100 443

# 输出示例：
========================================
 TOTAL:                  SECONDS 10; HANDSHAKES 5620
 MEASURES (seconds):     MAX h/s 618; AVG h/s 562; 95P h/s 448
 LATENCY (microseconds): MIN 26; AVG 50; 95P 74; MAX 3945
```

### 2.5 优缺点分析

#### 优点
- ✓ 多线程并发测试能力
- ✓ 完善的统计和报告功能
- ✓ 支持多种 TLS 版本（1.2、1.3）
- ✓ 支持指定加密套件和曲线
- ✓ 基于 epoll 的高效 I/O 处理
- ✓ 支持会话恢复测试

#### 缺点
- ✗ 只支持标准 TLS 握手协议
- ✗ 必须有实际的 TLS 服务器端点
- ✗ 无法测试自定义握手协议
- ✗ 硬编码使用 OpenSSL API

---

## 3. TLShub 架构分析

### 3.1 TLShub 概述

TLShub 是一个 **Layer 4 友好的内核级 mTLS 握手模块**，工作在内核空间，与标准应用层 TLS 完全不同。

### 3.2 TLShub 握手流程

```
┌──────────────┐                              ┌──────────────┐
│  应用程序    │                              │  应用程序    │
│  (客户端)    │                              │  (服务端)    │
└──────┬───────┘                              └───────┬──────┘
       │                                              │
       │ 1. 普通 TCP 连接                             │
       ├──────────────────────────────────────────────┤
       │                                              │
┌──────┴───────┐                              ┌───────┴──────┐
│ eBPF 程序    │                              │ eBPF 程序    │
│ 捕获连接     │                              │ 捕获连接     │
└──────┬───────┘                              └───────┬──────┘
       │                                              │
       │ 2. Netlink 通知                             │
       │                                              │
       ↓                                              ↓
┌──────────────────────────────────────────────────────────┐
│              TLShub 内核模块                              │
│                                                          │
│  3. 执行 Pod 级 mTLS 握手                                │
│     (内核空间，非标准 TLS 协议)                          │
│                                                          │
│  4. 生成并存储会话密钥                                   │
└──────────────┬───────────────────────────────────────────┘
               │
               │ 5. 用户态获取密钥
               ↓
┌──────────────────────────────────────────────────────────┐
│              用户态程序 (capture)                         │
│                                                          │
│  6. 配置 KTLS (Kernel TLS)                               │
│     将密钥应用到 TCP 连接                                │
└──────────────────────────────────────────────────────────┘
```

### 3.3 关键技术特点

#### 3.3.1 Netlink 通信

TLShub 使用 Netlink 套接字与内核模块通信：

```c
// 用户态 API (tlshub-api/tlshub.c)

// 1. 初始化服务
int tlshub_service_init(void);

// 2. 发起握手 (通过 Netlink)
int tlshub_handshake(
    uint32_t client_pod_ip,
    uint32_t server_pod_ip,
    unsigned short client_pod_port,
    unsigned short server_pod_port
);

// 3. 获取密钥 (通过 Netlink)
struct key_back tlshub_fetch_key(
    uint32_t client_pod_ip,
    uint32_t server_pod_ip,
    unsigned short client_pod_port,
    unsigned short server_pod_port
);
```

#### 3.3.2 消息结构

```c
// Netlink 消息载荷
struct my_msg {
    uint32_t client_pod_ip;
    uint32_t server_pod_ip;
    unsigned short client_pod_port;
    unsigned short server_pod_port;
    char opcode;  // TLS_SERVICE_INIT / START / FETCH
    bool server;
};

// Netlink 消息容器
typedef struct _user_msg_info {
    struct nlmsghdr hdr;
    char msg_type;  // 0x01=首次成功, 0x02=非首次, 0x03=失败...
    char msg[125];
} user_msg_info;
```

#### 3.3.3 密钥返回

```c
struct key_back {
    int status;              // 0=成功, -1=未完成, -2=过期
    unsigned char masterkey[32];  // 32字节主密钥
};
```

### 3.4 TLShub vs 标准 TLS

| 特性 | 标准 TLS | TLShub |
|------|----------|--------|
| **工作层次** | 应用层 (Layer 7) | 内核层 (Layer 4) |
| **握手协议** | TLS 1.2/1.3 标准协议 | 自定义 Pod 级协议 |
| **通信方式** | TCP socket + OpenSSL | Netlink + 内核模块 |
| **目标对象** | 单个 TCP 连接 | Pod 到 Pod |
| **密钥管理** | 会话级 | Pod 级 + KTLS |
| **应用感知** | 应用需要使用 SSL 库 | 对应用透明 |

---

## 4. 兼容性分析

### 4.1 技术差异对比

| 维度 | TLS-Perf | TLShub | 兼容性 |
|------|----------|--------|--------|
| **握手协议** | 标准 TLS 1.2/1.3 | 自定义 Pod 级协议 | ✗ 不兼容 |
| **通信接口** | OpenSSL API | Netlink API | ✗ 不兼容 |
| **连接对象** | TCP socket | Pod 四元组 | ✗ 不兼容 |
| **密钥获取** | SSL 会话内置 | 需要额外 fetchkey | ✗ 不兼容 |
| **工作层次** | 用户态 | 内核态交互 | ✗ 不兼容 |
| **性能统计** | 握手延迟、吞吐 | 可复用框架 | ✓ 部分可用 |

### 4.2 不兼容的核心原因

#### 4.2.1 协议层面不兼容

```
TLS-Perf 期望的握手流程：
  Client                          Server
    |                               |
    |-------- ClientHello --------->|
    |<------- ServerHello ----------|
    |<------- Certificate ----------|
    |------- ClientKeyExchange ----->|
    |------- ChangeCipherSpec ------>|
    |-------- Finished ------------->|
    |<------- Finished --------------|

TLShub 实际的握手流程：
  Client                          Kernel Module
    |                               |
    |-- Netlink: TLS_SERVICE_START ->|
    |                               |
    |   (内核模块执行 Pod 级握手)    |
    |                               |
    |<- Netlink: 0x01 (首次成功) ---|
    |                               |
    |-- Netlink: TLS_SERVICE_FETCH ->|
    |<- Netlink: masterkey[32] -----|
```

#### 4.2.2 API 层面不兼容

```cpp
// TLS-Perf 使用的 API
SSL *tls = SSL_new(tls_ctx);
SSL_set_fd(tls, socket_fd);
int r = SSL_connect(tls);  // 标准 OpenSSL 握手

// TLShub 需要的 API
int r = tlshub_handshake(
    client_ip,    // Pod IP
    server_ip,    // Pod IP
    client_port,  // Pod Port
    server_port   // Pod Port
);  // Netlink 握手
struct key_back key = tlshub_fetch_key(...);
```

### 4.3 结论

**TLS-Perf 工具不能直接用于测试 TLShub 握手性能**，必须进行实质性改造。

---

## 5. 改造方案

虽然 TLS-Perf 不能直接使用，但我们可以参考其架构，开发适用于 TLShub 的性能测试工具。

### 5.1 改造策略

#### 策略 A：深度改造 TLS-Perf（不推荐）

**需要修改的内容**：
1. 移除所有 OpenSSL 相关代码
2. 集成 TLShub API (`tlshub.h`, `tlshub.c`)
3. 修改连接管理逻辑
4. 重写握手流程
5. 保留统计和报告模块

**评估**：
- 工作量：约 60-70% 的代码需要重写
- 复杂度：高
- 风险：可能引入新的 bug
- 收益：可重用统计框架

#### 策略 B：开发新的 TLShub 性能测试工具（推荐）

**设计思路**：
1. 参考 TLS-Perf 的统计和报告设计
2. 从零开始实现 TLShub 握手逻辑
3. 使用更简洁的 C 语言实现
4. 专门针对 TLShub 特性优化

**评估**：
- 工作量：约 1000-1500 行代码
- 复杂度：中等
- 风险：可控
- 收益：更适合 TLShub，易于维护

### 5.2 推荐方案：开发 tlshub-perf 工具

#### 5.2.1 架构设计

```
┌─────────────────────────────────────────────────────────┐
│                   tlshub-perf                           │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  ┌─────────────┐  ┌─────────────┐  ┌──────────────┐   │
│  │ 命令行解析   │  │ 配置管理     │  │ 性能统计      │   │
│  └─────────────┘  └─────────────┘  └──────────────┘   │
│                                                         │
│  ┌──────────────────────────────────────────────────┐  │
│  │           多线程握手引擎                          │  │
│  │  ┌──────┐  ┌──────┐  ┌──────┐  ┌──────┐        │  │
│  │  │ 线程1 │  │ 线程2 │  │ ...  │  │ 线程N │        │  │
│  │  └──────┘  └──────┘  └──────┘  └──────┘        │  │
│  └──────────────────────────────────────────────────┘  │
│                          │                              │
│                          ↓                              │
│  ┌──────────────────────────────────────────────────┐  │
│  │           TLShub API 集成层                       │  │
│  │  - tlshub_service_init()                         │  │
│  │  - tlshub_handshake()                            │  │
│  │  - tlshub_fetch_key()                            │  │
│  └──────────────────────────────────────────────────┘  │
│                          │                              │
└──────────────────────────┼──────────────────────────────┘
                           │ Netlink
                           ↓
┌──────────────────────────────────────────────────────────┐
│                TLShub 内核模块                            │
└──────────────────────────────────────────────────────────┘
```

#### 5.2.2 核心模块

**1. 主程序 (main.c)**
```c
int main(int argc, char *argv[])
{
    // 1. 解析命令行参数
    parse_options(argc, argv);
    
    // 2. 初始化 TLShub 服务
    if (tlshub_service_init() != 0) {
        fprintf(stderr, "初始化 TLShub 失败\n");
        return -1;
    }
    
    // 3. 初始化性能统计
    perf_stats_init();
    
    // 4. 创建工作线程
    pthread_t threads[g_opt.n_threads];
    for (int i = 0; i < g_opt.n_threads; i++) {
        pthread_create(&threads[i], NULL, worker_thread, &i);
    }
    
    // 5. 等待测试完成
    for (int i = 0; i < g_opt.n_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // 6. 打印性能报告
    perf_stats_report();
    
    return 0;
}
```

**2. 工作线程 (worker.c)**
```c
void* worker_thread(void *arg)
{
    int thread_id = *(int*)arg;
    
    // 线程本地统计
    struct thread_stats stats = {0};
    
    // 根据测试模式执行
    if (g_opt.duration > 0) {
        // 按时间测试
        time_t start = time(NULL);
        while (time(NULL) - start < g_opt.duration) {
            do_one_handshake(&stats);
        }
    } else if (g_opt.n_handshakes > 0) {
        // 按次数测试
        for (int i = 0; i < g_opt.n_handshakes_per_thread; i++) {
            do_one_handshake(&stats);
        }
    }
    
    // 汇总统计
    aggregate_stats(&stats);
    
    return NULL;
}
```

**3. 握手执行 (handshake.c)**
```c
int do_one_handshake(struct thread_stats *stats)
{
    // 1. 准备 Pod 信息
    uint32_t client_ip = get_random_client_ip();
    uint32_t server_ip = g_opt.server_ip;
    unsigned short client_port = get_random_port();
    unsigned short server_port = g_opt.server_port;
    
    // 2. 开始计时
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // 3. 执行握手
    int ret = tlshub_handshake(client_ip, server_ip, 
                               client_port, server_port);
    
    // 4. 结束计时
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    // 5. 记录结果
    if (ret == 0) {
        stats->success_count++;
        double latency_ms = timespec_diff_ms(&start, &end);
        stats->total_latency_ms += latency_ms;
        update_latency_histogram(stats, latency_ms);
    } else {
        stats->error_count++;
    }
    
    // 6. 可选：获取密钥
    if (g_opt.fetch_key) {
        struct key_back key = tlshub_fetch_key(client_ip, server_ip,
                                               client_port, server_port);
        if (key.status == 0) {
            stats->key_fetch_success++;
        }
    }
    
    return ret;
}
```

**4. 性能统计 (stats.c)**
```c
struct perf_stats {
    // 握手统计
    uint64_t total_handshakes;
    uint64_t success_handshakes;
    uint64_t failed_handshakes;
    
    // 延迟统计
    double min_latency_ms;
    double max_latency_ms;
    double avg_latency_ms;
    double p95_latency_ms;
    double p99_latency_ms;
    
    // 吞吐量统计
    double handshakes_per_sec;
    
    // 直方图
    uint64_t latency_histogram[HISTOGRAM_BUCKETS];
};

void perf_stats_report(void)
{
    printf("========================================\n");
    printf(" TLShub 性能测试报告\n");
    printf("========================================\n");
    printf("总握手次数:     %lu\n", g_stats.total_handshakes);
    printf("成功次数:       %lu\n", g_stats.success_handshakes);
    printf("失败次数:       %lu\n", g_stats.failed_handshakes);
    printf("成功率:         %.2f%%\n", 
           100.0 * g_stats.success_handshakes / g_stats.total_handshakes);
    printf("\n");
    printf("延迟统计 (毫秒):\n");
    printf("  最小延迟:     %.3f\n", g_stats.min_latency_ms);
    printf("  平均延迟:     %.3f\n", g_stats.avg_latency_ms);
    printf("  95分位:       %.3f\n", g_stats.p95_latency_ms);
    printf("  99分位:       %.3f\n", g_stats.p99_latency_ms);
    printf("  最大延迟:     %.3f\n", g_stats.max_latency_ms);
    printf("\n");
    printf("吞吐量:         %.2f 握手/秒\n", g_stats.handshakes_per_sec);
    printf("========================================\n");
}
```

#### 5.2.3 命令行接口

```bash
# 基本用法
tlshub-perf [选项] <服务端IP> <服务端端口>

# 选项说明
  -h, --help              显示帮助信息
  -t <N>                  线程数 (默认: 1)
  -n <N>                  总握手次数
  -T <N>                  测试时长 (秒)
  -c <IP>                 客户端 IP 池起始地址
  -p <PORT>               客户端端口范围起始
  -f, --fetch-key         测试密钥获取
  -o <FILE>               输出结果到文件 (JSON 格式)
  -v, --verbose           详细输出
  --csv <FILE>            导出 CSV 格式数据

# 示例
# 1. 测试 10 秒，4 线程
sudo ./tlshub-perf -T 10 -t 4 10.0.2.100 443

# 2. 测试 1000 次握手
sudo ./tlshub-perf -n 1000 -t 2 10.0.2.100 443

# 3. 测试握手和密钥获取
sudo ./tlshub-perf -T 10 -t 4 -f 10.0.2.100 443

# 4. 导出结果到 JSON
sudo ./tlshub-perf -T 10 -o results.json 10.0.2.100 443
```

#### 5.2.4 输出示例

```
========================================
 TLShub 性能测试报告
========================================
测试配置:
  服务端:         10.0.2.100:443
  线程数:         4
  测试时长:       10 秒
  客户端IP池:     10.0.1.1 - 10.0.1.254

握手统计:
  总握手次数:     5620
  成功次数:       5618
  失败次数:       2
  成功率:         99.96%

延迟统计 (毫秒):
  最小延迟:       3.245
  平均延迟:       7.123
  95分位:         12.456
  99分位:         18.789
  最大延迟:       28.901

吞吐量:           562.0 握手/秒

密钥获取统计:
  成功次数:       5618
  平均耗时:       1.234 ms

========================================
```

---

## 6. 实施步骤

### 6.1 阶段一：需求确认与设计 (1天)

**任务**：
- [ ] 确认测试需求和性能指标
- [ ] 设计工具架构
- [ ] 评审设计方案

**交付物**：
- 设计文档
- 接口定义

### 6.2 阶段二：核心功能开发 (3天)

**任务**：
- [ ] 实现命令行参数解析
- [ ] 集成 TLShub API
- [ ] 实现多线程握手引擎
- [ ] 实现性能统计模块

**文件清单**：
```
tlshub-perf/
├── Makefile
├── README.md
├── src/
│   ├── main.c           # 主程序
│   ├── worker.c         # 工作线程
│   ├── handshake.c      # 握手逻辑
│   ├── stats.c          # 性能统计
│   ├── options.c        # 参数解析
│   └── utils.c          # 工具函数
├── include/
│   ├── tlshub_perf.h    # 主头文件
│   ├── worker.h
│   ├── stats.h
│   └── options.h
└── tlshub-api/
    ├── tlshub.h
    └── tlshub.c
```

### 6.3 阶段三：测试与优化 (2天)

**任务**：
- [ ] 单元测试
- [ ] 功能测试
- [ ] 性能测试
- [ ] Bug 修复

**测试场景**：
1. 单线程测试
2. 多线程测试 (4, 8, 16 线程)
3. 长时间稳定性测试 (1小时)
4. 压力测试 (最大并发)
5. 错误处理测试

### 6.4 阶段四：文档与发布 (1天)

**任务**：
- [ ] 编写使用文档
- [ ] 编写测试报告模板
- [ ] 准备示例和教程

**交付物**：
- 可执行程序 `tlshub-perf`
- 用户手册
- 测试报告模板

### 6.5 总体时间估算

- **总工期**: 约 7 个工作日
- **人力**: 1 人
- **风险缓冲**: +2 天

---

## 7. 测试方案

### 7.1 测试环境准备

#### 7.1.1 硬件要求

```
测试机配置:
  CPU:     8 核或以上
  内存:    16GB 或以上
  网络:    1Gbps 或以上
  OS:      Linux Kernel >= 5.2 (支持 KTLS)
```

#### 7.1.2 软件要求

```bash
# 1. 安装依赖
sudo apt-get install -y gcc make libbpf-dev \
    linux-headers-$(uname -r)

# 2. 加载 TLShub 内核模块
sudo insmod /path/to/tlshub.ko

# 3. 验证模块加载
lsmod | grep tlshub

# 4. 启动 capture 程序
cd capture/
sudo ./capture
```

### 7.2 基准测试方案

#### 7.2.1 单线程性能测试

**目标**: 测试单线程最大握手速率

```bash
# 测试 60 秒
sudo ./tlshub-perf -T 60 -t 1 -o baseline_1t.json 10.0.2.100 443
```

**预期结果**:
- 握手速率: 100-200 次/秒
- 平均延迟: 5-10 ms

#### 7.2.2 多线程扩展性测试

**目标**: 测试多线程性能扩展

```bash
# 分别测试 1, 2, 4, 8, 16 线程
for threads in 1 2 4 8 16; do
    sudo ./tlshub-perf -T 60 -t $threads \
        -o scalability_${threads}t.json \
        10.0.2.100 443
done
```

**分析指标**:
- 线程扩展系数 = 握手速率(N线程) / 握手速率(1线程)
- 理想值: 接近线性扩展

#### 7.2.3 延迟分布测试

**目标**: 分析握手延迟分布

```bash
# 长时间测试收集足够样本
sudo ./tlshub-perf -T 300 -t 4 \
    -o latency_dist.json \
    --csv latency_dist.csv \
    10.0.2.100 443
```

**分析指标**:
- P50, P95, P99, P99.9 延迟
- 延迟直方图

#### 7.2.4 压力测试

**目标**: 测试系统极限性能

```bash
# 最大并发数测试
sudo ./tlshub-perf -T 60 -t 16 \
    -o stress_test.json \
    10.0.2.100 443
```

**监控指标**:
- CPU 使用率
- 内存使用量
- 握手成功率
- 错误率

### 7.3 对比测试方案

#### 7.3.1 TLShub vs OpenSSL

**目标**: 对比 TLShub 与标准 OpenSSL 的性能

```bash
# 1. 测试 TLShub 模式
sudo ./tlshub-perf -T 60 -t 4 -o tlshub_mode.json 10.0.2.100 443

# 2. 修改 capture 配置使用 OpenSSL 模式
sudo vi /etc/tlshub/capture.conf
# mode = openssl

# 3. 重启 capture
sudo pkill capture
sudo ./capture

# 4. 再次测试
sudo ./tlshub-perf -T 60 -t 4 -o openssl_mode.json 10.0.2.100 443

# 5. 对比结果
python3 compare_results.py tlshub_mode.json openssl_mode.json
```

### 7.4 测试报告模板

```markdown
# TLShub 性能测试报告

## 测试信息
- 测试日期: YYYY-MM-DD
- 测试人员: XXX
- TLShub 版本: vX.X.X
- 测试工具: tlshub-perf v1.0

## 测试环境
- CPU: XXX
- 内存: XXX GB
- 网络: XXX
- 操作系统: Linux X.X.X

## 测试结果

### 1. 单线程性能
- 握手速率: XXX 次/秒
- 平均延迟: XXX ms
- P95 延迟: XXX ms
- P99 延迟: XXX ms

### 2. 多线程扩展性
| 线程数 | 握手速率 (次/秒) | 扩展系数 |
|--------|-----------------|----------|
| 1      | XXX             | 1.0      |
| 2      | XXX             | XXX      |
| 4      | XXX             | XXX      |
| 8      | XXX             | XXX      |

### 3. 延迟分布
- P50: XXX ms
- P95: XXX ms
- P99: XXX ms
- P99.9: XXX ms

### 4. 压力测试
- 最大握手速率: XXX 次/秒
- CPU 使用率: XXX%
- 内存使用: XXX MB
- 错误率: XXX%

### 5. 对比测试
| 模式 | 握手速率 | 平均延迟 | 优势 |
|------|----------|----------|------|
| TLShub | XXX | XXX ms | +XX% |
| OpenSSL | XXX | XXX ms | - |

## 结论
XXX

## 附录
- 原始数据: XXX.json
- 详细日志: XXX.log
```

---

## 8. 替代方案

### 8.1 方案对比

| 方案 | 优点 | 缺点 | 适用场景 |
|------|------|------|----------|
| **自研 tlshub-perf** | 完全适配 TLShub; 可控; 易维护 | 需要开发时间 | 推荐用于生产 |
| **改造 tls-perf** | 可重用部分代码 | 改造复杂; 维护困难 | 不推荐 |
| **使用 wrk/ab** | 现成工具 | 只能测应用层 HTTP | 无法测 TLShub |
| **使用 iperf3** | 现成工具 | 只能测网络吞吐 | 无法测握手 |
| **手动测试脚本** | 简单快速 | 功能有限 | 适合快速验证 |

### 8.2 简单测试脚本（临时方案）

如果需要快速验证，可以先使用简单脚本。

**我们已经提供了一个现成的快速测试脚本**：`docs/tlshub_quick_test.sh`

#### 使用方法

```bash
# 进入 docs 目录
cd docs/

# 运行测试（需要 root 权限）
sudo ./tlshub_quick_test.sh <客户端IP> <服务端IP> [测试次数]

# 示例：测试 100 次握手
sudo ./tlshub_quick_test.sh 10.0.1.100 10.0.2.100 100
```

#### 输出示例

```
========================================
TLShub 快速性能测试
========================================
客户端 IP: 10.0.1.100
服务端 IP: 10.0.2.100
测试次数:  100
========================================

开始测试...
[100/100] 成功: 98, 失败: 2, 最后延迟: 7 ms

========================================
测试完成
========================================
总测试次数: 100
成功次数:   98
失败次数:   2
成功率:     98.00%

延迟统计 (毫秒):
  最小延迟: 3
  平均延迟: 7.123
  最大延迟: 15
  P95 延迟: 12
  P99 延迟: 14

吞吐量:     13.74 握手/秒
========================================
```

**注意**: 这个脚本仅用于快速验证，不适合正式的性能测试。正式测试请使用专门的 `tlshub-perf` 工具。

---

## 9. 总结与建议

### 9.1 核心结论

1. **TLS-Perf 不能直接使用**
   - 技术架构完全不同
   - 协议层面不兼容
   - API 接口不兼容

2. **推荐开发专用工具**
   - 开发 `tlshub-perf` 工具
   - 直接集成 TLShub API
   - 预计 7-10 个工作日完成

3. **临时可用简单脚本**
   - 用于快速验证
   - 功能有限
   - 不适合正式测试

### 9.2 实施建议

#### 短期 (1-2周)
1. ✓ 立即启动 `tlshub-perf` 工具开发
2. ✓ 同时使用简单脚本进行初步测试
3. ✓ 准备测试环境和基准数据

#### 中期 (1-2月)
1. ✓ 完成 `tlshub-perf` 工具并投入使用
2. ✓ 建立完整的性能测试流程
3. ✓ 积累性能基准数据
4. ✓ 定期进行性能回归测试

#### 长期 (3-6月)
1. ✓ 集成到 CI/CD 流程
2. ✓ 建立性能监控系统
3. ✓ 持续优化 TLShub 性能
4. ✓ 对比业界标准 TLS 性能

### 9.3 风险提示

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| TLShub API 变更 | 工具需要更新 | 保持 API 稳定性 |
| 内核模块不稳定 | 测试结果不准 | 充分测试内核模块 |
| 测试环境差异 | 结果不可比 | 标准化测试环境 |
| 并发冲突 | 测试失败率高 | 实现连接池管理 |

### 9.4 后续工作

1. **工具开发**
   - 开发 `tlshub-perf` 工具
   - 编写使用文档
   - 提供示例和教程

2. **性能优化**
   - 分析性能瓶颈
   - 优化握手流程
   - 提升吞吐量

3. **对比测试**
   - 与 OpenSSL 对比
   - 与 BoringSSL 对比
   - 与业界方案对比

4. **文档完善**
   - 性能测试指南
   - 最佳实践
   - 故障排查手册

---

## 附录

### 附录 A: TLShub API 参考

详见 [docs/TLSHUB_API_CN.md](./TLSHUB_API_CN.md)

### 附录 B: TLS-Perf 源码分析

关键代码片段已在第 2 节中分析。

完整源码: https://github.com/tempesta-tech/tls-perf

### 附录 C: 相关文档

- [TLShub 项目](https://github.com/lin594/tlshub)
- [性能指标文档](./PERFORMANCE_METRICS_CN.md)
- [集成指南](./INTEGRATION_GUIDE_CN.md)

### 附录 D: 联系方式

如有问题或建议，请通过以下方式联系：
- GitHub Issues: https://github.com/CapooL/tlshub-ebpf/issues
- 邮件: [项目维护者邮箱]

---

**文档版本**: 1.0.0  
**最后更新**: 2026-01-23  
**作者**: TLShub 开发团队
