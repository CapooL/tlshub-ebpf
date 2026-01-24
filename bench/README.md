# TLShub Benchmark Tool (tlshub_bench)

TLShub 的 Layer 7（应用层）性能测试工具，用于模拟真实业务流量，测试 tlshub 的流量捕获和 mTLS 握手功能。

## 功能特性

### 核心功能

1. **TCP 客户端模拟器**
   - 模拟真实的 TCP 客户端行为
   - 支持可配置的目标 IP/端口
   - 发送可配置大小的数据块
   - **正确的连接关闭**：使用 FIN 包进行四次挥手，而非 RST 重置
   - 连接生命周期状态跟踪

2. **Echo Server 模式**
   - 提供简单的回声服务器
   - 当前实现为单连接串行处理，适用于基础回显测试
   - 形成完整的通信闭环

3. **性能指标收集**
   - 连接建立延迟（Connection Latency）
   - 数据传输时间（Data Transfer Time）
   - 吞吐量（Throughput）
   - CPU 使用率（CPU Usage）
   - 内存使用量（Memory Usage）

4. **数据导出**
   - JSON 格式：便于程序分析
   - CSV 格式：便于 Excel 查看

## 系统要求

- Linux 系统（内核 >= 4.9，用于运行 `tlshub_bench` 工具本身）
- 如果需要与 TLShub + KTLS 集成进行联调/压测，请使用 Linux 系统（内核 >= 5.2，满足 KTLS 支持要求）
- GCC 编译器
- Make 构建工具
- （可选）TLShub 内核模块（用于实际测试 / 与 TLShub+KTLS 集成）

## 安装

### 1. 编译

```bash
cd bench/
make
```

编译成功后会生成 `tlshub_bench` 可执行文件。

### 2. 系统安装（可选）

```bash
sudo make install
```

这会将 `tlshub_bench` 安装到 `/usr/local/bin/`，之后可以在任何位置直接运行。

## 使用方法

### 命令行参数

```
Usage: tlshub_bench [OPTIONS]

Options:
  -m, --mode MODE           运行模式: client 或 server (默认: client)
  -t, --target-ip IP        目标服务器 IP 地址 (默认: 127.0.0.1)
  -p, --target-port PORT    目标服务器端口 (默认: 8080)
  -l, --listen-port PORT    Echo server 监听端口 (默认: 9090)
  -s, --data-size SIZE      发送数据大小（字节） (默认: 1024)
  -c, --concurrency NUM     并发连接数 (默认: 1)
  -n, --total-connections NUM  总连接数 (默认: 10)
  -j, --json FILE           JSON 输出文件 (默认: bench_metrics_<timestamp>.json)
  -o, --csv FILE            CSV 输出文件 (默认: bench_metrics_<timestamp>.csv)
  -v, --verbose             启用详细输出
  -h, --help                显示帮助信息
```

### 基本使用示例

#### 1. 运行 Echo Server

在终端 1 中启动 echo server：

```bash
./tlshub_bench --mode server --listen-port 9090
```

服务器会持续运行，直到按 Ctrl+C 停止。

#### 2. 运行客户端测试

在终端 2 中运行客户端：

```bash
./tlshub_bench --mode client \
  --target-ip 127.0.0.1 \
  --target-port 9090 \
  --data-size 4096 \
  --total-connections 100 \
  --verbose
```

测试完成后会生成两个文件：
- `bench_metrics_<timestamp>.json` - JSON 格式的详细指标
- `bench_metrics_<timestamp>.csv` - CSV 格式的汇总数据

#### 3. 压力测试

```bash
./tlshub_bench --mode client \
  --target-ip 127.0.0.1 \
  --target-port 9090 \
  --data-size 65536 \
  --total-connections 1000 \
  --concurrency 10
```

## 与 TLShub 集成测试

### 测试流程

以下是完整的 TLShub 集成测试流程：

#### 步骤 1: 准备 TLShub 环境

```bash
# 1. 加载 TLShub 内核模块（如果尚未加载）
cd tlshub/
sudo insmod tlshub.ko

# 2. 启动 capture 模块
cd ../capture/
sudo ./capture /etc/tlshub/capture.conf
```

#### 步骤 2: 配置流量拦截规则

确保 TLShub 配置正确以拦截测试流量。编辑 `/etc/tlshub/capture.conf`：

```
# 示例配置
mode = tlshub
pod_node_config = /etc/tlshub/pod_node_mapping.conf
netlink_protocol = 31
```

配置 pod_node_mapping，确保包含测试的 IP 地址范围。

#### 步骤 3: 启动 Echo Server

```bash
cd bench/
./tlshub_bench --mode server --listen-port 9090
```

#### 步骤 4: 运行客户端测试

在另一个终端：

```bash
cd bench/
./tlshub_bench --mode client \
  --target-ip 127.0.0.1 \
  --target-port 9090 \
  --data-size 8192 \
  --total-connections 50 \
  --json tlshub_test_results.json \
  --csv tlshub_test_results.csv \
  --verbose
```

#### 步骤 5: 查看结果

```bash
# 查看 JSON 格式的详细结果
cat tlshub_test_results.json | jq .

# 查看 CSV 格式的汇总
cat tlshub_test_results.csv

# 或在 Excel 中打开 CSV 文件
```

### 观察 TLShub 行为

在测试期间，可以监控 TLShub 的日志：

```bash
# 查看内核日志
dmesg -w | grep -i tlshub

# 或查看 capture 程序输出
# (capture 程序应该显示拦截的连接和握手过程)
```

## 输出格式说明

### JSON 格式

JSON 文件包含三个主要部分：

1. **test_configuration** - 测试配置参数
2. **summary_metrics** - 汇总性能指标
3. **connection_details** - 每个连接的详细数据

示例：

```json
{
  "test_configuration": {
    "mode": "client",
    "target_ip": "127.0.0.1",
    "target_port": 9090,
    "data_size": 4096,
    "concurrency": 1,
    "total_connections": 100
  },
  "summary_metrics": {
    "total_connections": 100,
    "successful_connections": 98,
    "failed_connections": 2,
    "avg_connection_latency_ms": 1.234,
    "avg_throughput_mbps": 45.678,
    "total_test_duration_sec": 12.345,
    "cpu_usage_percent": 15.32,
    "memory_usage_mb": 8.45
  },
  "connection_details": [
    {
      "conn_id": 1,
      "state": 6,
      "bytes_sent": 4096,
      "bytes_received": 4096,
      "connection_latency_ms": 1.234,
      "throughput_mbps": 50.123
    }
  ]
}
```

### CSV 格式

CSV 文件分为两部分：

1. **汇总部分** - 整体性能指标（以 # 开头的注释和 Metric,Value 表格）
2. **详细部分** - 每个连接的数据（Connection Details 表格）

可以直接在 Excel、Google Sheets 或其他电子表格软件中打开。

## 性能指标说明

### 连接建立延迟 (Connection Latency)

从发起连接到连接建立成功的时间（毫秒）。

- **低延迟** (< 5ms): 本地或局域网连接
- **中等延迟** (5-50ms): 远程连接
- **高延迟** (> 50ms): 可能存在网络问题或系统负载过高

### 吞吐量 (Throughput)

数据传输速率（Mbps），计算公式：

```
Throughput = (Bytes Sent + Bytes Received) * 8 / Transfer Time / 1024 / 1024
```

### CPU 使用率

测试期间进程的 CPU 使用百分比。

### 内存使用量

测试进程使用的最大内存（MB）。

## 故障排查

### 连接失败

如果大量连接失败：

1. 检查 echo server 是否正在运行
2. 检查防火墙设置：`sudo iptables -L`
3. 检查端口是否被占用：`sudo netstat -tlnp | grep <port>`
4. 查看系统日志：`dmesg | tail -50`

### 性能异常

如果性能数据异常：

1. 确保系统负载不高：`top` 或 `htop`
2. 检查网络接口状态：`ifconfig` 或 `ip addr`
3. 验证 TLShub 是否正常工作：`dmesg | grep tlshub`
4. 尝试减少并发连接数

### 测试超时

如果连接超时：

1. 增加系统的文件描述符限制：`ulimit -n 10000`
2. 检查服务器队列大小
3. 减少并发连接数
4. 增加测试数据大小以延长连接时间

## 高级用法

### 自定义测试脚本

创建一个测试脚本来运行多个测试场景：

```bash
#!/bin/bash

# test_scenarios.sh

# 启动 echo server
./tlshub_bench --mode server --listen-port 9090 &
SERVER_PID=$!
sleep 2

# 场景 1: 小数据包
./tlshub_bench --mode client --target-port 9090 \
  --data-size 512 --total-connections 100 \
  --json results_small.json --csv results_small.csv

# 场景 2: 大数据包
./tlshub_bench --mode client --target-port 9090 \
  --data-size 65536 --total-connections 100 \
  --json results_large.json --csv results_large.csv

# 场景 3: 高并发
./tlshub_bench --mode client --target-port 9090 \
  --data-size 4096 --total-connections 1000 --concurrency 50 \
  --json results_concurrent.json --csv results_concurrent.csv

# 停止 server
kill $SERVER_PID

echo "All tests completed!"
```

### 结合其他工具

#### 使用 tcpdump 抓包

```bash
# 在测试期间抓包
sudo tcpdump -i lo -w tlshub_test.pcap port 9090 &
TCPDUMP_PID=$!

# 运行测试
./tlshub_bench --mode client --target-port 9090 --total-connections 10

# 停止抓包
sudo kill $TCPDUMP_PID

# 分析抓包文件
wireshark tlshub_test.pcap
```

#### 使用 perf 性能分析

```bash
# 运行性能分析
sudo perf record -g ./tlshub_bench --mode client \
  --target-port 9090 --total-connections 100

# 查看报告
sudo perf report
```

## 开发与扩展

### 添加新的指标

要添加新的性能指标：

1. 在 `include/bench_common.h` 中的 `conn_stats_t` 或 `global_metrics_t` 添加字段
2. 在 `src/bench_common.c` 中更新 `update_metrics()` 和 `finalize_metrics()`
3. 在 `export_metrics_json()` 和 `export_metrics_csv()` 中添加导出逻辑

### 实现真正的并发

当前实现为顺序执行连接。要实现真正的并发，可以：

1. 使用 `pthread` 创建工作线程
2. 使用 `fork()` 创建子进程
3. 使用 `epoll` 实现异步 I/O

## 相关文档

- [TLShub 主文档](../README.md)
- [Capture 模块文档](../capture/docs/README.md)
- [性能指标文档](../docs/PERFORMANCE_METRICS_CN.md)
- [TLShub API 文档](../tlshub-api/README.md)

## 常见问题

### Q: 为什么需要 echo server？

**A:** Echo server 确保形成完整的通信闭环，避免 TCP 半开/半闭状态干扰测试结果。它还允许我们测量往返时间和验证数据完整性。

### Q: 如何确保连接正确关闭（FIN 而非 RST）？

**A:** 代码使用 `shutdown(sockfd, SHUT_WR)` 发送 FIN 包，然后等待对方关闭，最后调用 `close()`。这确保了正确的四次挥手过程。

### Q: 可以测试 TLS 加密连接吗？

**A:** 当前版本测试的是 TCP 连接。当与 TLShub 集成时，TLShub 会在内核级别添加 KTLS 加密。应用层不需要额外的 TLS 配置。

### Q: 如何解读密钥协商时间？

**A:** 当前版本不直接测量密钥协商时间。这个指标需要通过 TLShub API 或内核日志获取。连接建立延迟包含了 TCP 握手和可能的 mTLS 握手时间。如需精确测量密钥协商时间，可以通过以下方式：
- 读取 TLShub 内核模块的日志（`dmesg | grep tlshub`）
- 使用 capture 程序的性能指标输出
- 通过 Netlink 事件监控握手过程

### Q: 支持 IPv6 吗？

**A:** 当前版本仅支持 IPv4。IPv6 支持可以作为未来的改进。

## 贡献

欢迎提交 Issue 和 Pull Request！

## 许可证

与 TLShub 项目使用相同的许可证。

## 联系方式

- Issue: https://github.com/CapooL/tlshub-ebpf/issues
