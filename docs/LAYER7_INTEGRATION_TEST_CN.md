# TLShub Layer 7 集成测试指南

本文档介绍如何使用 `tlshub_bench` 工具对 TLShub 进行完整的 Layer 7 应用层测试。

## 测试目标

通过 `tlshub_bench` 工具验证以下功能：

1. TLShub 是否正确拦截应用层 TCP 连接
2. 内核级 mTLS 握手是否成功完成
3. KTLS 密钥配置是否正确
4. 数据传输性能（延迟、吞吐量）
5. 系统资源使用（CPU、内存）

## 前置条件

### 1. 系统要求

- Linux 内核 >= 5.2（支持 KTLS）
- 已编译的 TLShub 内核模块
- 已编译的 capture 程序
- 已编译的 tlshub_bench 工具

### 2. 检查内核支持

```bash
# 检查 KTLS 支持
grep CONFIG_TLS /boot/config-$(uname -r)
# 应该显示: CONFIG_TLS=y 或 CONFIG_TLS=m

# 检查 eBPF 支持
grep CONFIG_BPF /boot/config-$(uname -r)
# 应该显示多个 CONFIG_BPF* 选项
```

### 3. 编译所需组件

```bash
# 编译 capture 模块
cd capture/
make

# 编译 tlshub_bench
cd ../bench/
make
```

## 完整测试流程

### 步骤 1: 准备配置文件

#### 1.1 配置 TLShub Capture

创建或编辑 `/etc/tlshub/capture.conf`：

```
# TLShub Capture Configuration
mode = tlshub
pod_node_config = /etc/tlshub/pod_node_mapping.conf
netlink_protocol = 31
log_level = info
verbose = true
```

#### 1.2 配置 Pod-Node 映射

编辑 `/etc/tlshub/pod_node_mapping.conf`，添加测试 Pod 的映射：

```
# Format: pod_ip=node_ip
127.0.0.1=127.0.0.1
10.0.1.100=10.0.2.200
```

根据实际测试环境调整 IP 地址。

### 步骤 2: 启动 TLShub 内核模块

```bash
# 进入 tlshub 目录
cd tlshub/

# 加载内核模块
sudo insmod tlshub.ko

# 验证模块已加载
lsmod | grep tlshub

# 查看内核日志
dmesg | tail -20 | grep -i tlshub
```

### 步骤 3: 启动 Capture 程序

在终端 1 中启动 capture：

```bash
cd capture/
sudo ./capture /etc/tlshub/capture.conf
```

保持此终端运行，观察 capture 的输出。正常情况下应该看到：

```
TLShub Traffic Capture Module
Loaded eBPF program
Waiting for events...
```

### 步骤 4: 启动 Echo Server

在终端 2 中启动 tlshub_bench echo server：

```bash
cd bench/
./tlshub_bench --mode server --listen-port 9090 --verbose
```

应该看到：

```
=== TLShub Echo Server ===
Listen Port: 9090
Press Ctrl+C to stop

Echo server listening on port 9090
```

### 步骤 5: 运行客户端测试

在终端 3 中运行客户端测试：

```bash
cd bench/

# 基础连接测试
./tlshub_bench --mode client \
  --target-ip 127.0.0.1 \
  --target-port 9090 \
  --data-size 1024 \
  --total-connections 10 \
  --verbose
```

### 步骤 6: 观察测试过程

#### 6.1 在终端 1 (capture) 观察

应该看到类似输出：

```
Event: TCP connection from 127.0.0.1:xxxxx to 127.0.0.1:9090
Attempting TLShub handshake...
Handshake successful
KTLS keys configured for socket
```

#### 6.2 在终端 2 (echo server) 观察

应该看到：

```
New connection from 127.0.0.1:xxxxx
Connection closed, total bytes echoed: 1024
```

#### 6.3 在终端 3 (client) 观察

应该看到：

```
Starting TCP client benchmark...
Target: 127.0.0.1:9090
Total connections: 10
Data size: 1024 bytes

Connection 1: Connected (latency: 0.123 ms)
Connection 1: Sent 1024 bytes
Connection 1: Received 1024 bytes
Connection 1: Closed (throughput: 50.123 Mbps)
...
Progress: 10/10 connections (10 succeeded, 0 failed)

Client benchmark completed.
Total: 10, Succeeded: 10, Failed: 0
```

### 步骤 7: 分析测试结果

测试完成后会生成两个文件：

```bash
# 查看 JSON 格式结果
cat bench_metrics_*.json | jq .

# 查看 CSV 格式结果
cat bench_metrics_*.csv
```

#### 关键指标分析

1. **连接建立延迟 (avg_connection_latency_ms)**
   - 正常范围：0.1-5ms (本地)
   - 包含 TCP 握手 + TLShub mTLS 握手时间
   - 如果明显高于 5ms，检查 TLShub 握手性能

2. **吞吐量 (avg_throughput_mbps)**
   - 本地回环：通常 > 1000 Mbps
   - 如果明显降低，检查 KTLS 配置

3. **成功连接数**
   - 应该等于 total_connections
   - 如果有失败，检查 capture 日志

4. **CPU 和内存使用**
   - 记录基准值，用于性能对比

### 步骤 8: 查看内核日志

```bash
# 查看 TLShub 相关日志
dmesg | grep -i tlshub | tail -50

# 查看连接状态
dmesg | grep "connection\|handshake\|key" | tail -20
```

## 高级测试场景

### 场景 1: 压力测试

测试 TLShub 在高并发下的性能：

```bash
./tlshub_bench --mode client \
  --target-ip 127.0.0.1 \
  --target-port 9090 \
  --data-size 4096 \
  --total-connections 1000 \
  --concurrency 50 \
  --json stress_test.json \
  --csv stress_test.csv
```

### 场景 2: 大数据包测试

测试大数据传输性能：

```bash
./tlshub_bench --mode client \
  --target-ip 127.0.0.1 \
  --target-port 9090 \
  --data-size 65536 \
  --total-connections 100 \
  --json large_packet.json \
  --csv large_packet.csv
```

### 场景 3: 跨节点测试

如果有多节点环境，测试跨节点通信：

```bash
# 在节点 A 启动 server
./tlshub_bench --mode server --listen-port 9090

# 在节点 B 运行 client
./tlshub_bench --mode client \
  --target-ip <node-A-IP> \
  --target-port 9090 \
  --data-size 8192 \
  --total-connections 50 \
  --json cross_node.json
```

### 场景 4: 长时间稳定性测试

```bash
# 运行长时间测试
for i in {1..100}; do
  echo "Round $i"
  ./tlshub_bench --mode client \
    --target-ip 127.0.0.1 \
    --target-port 9090 \
    --data-size 4096 \
    --total-connections 100 \
    --json "stability_${i}.json"
  sleep 10
done
```

## 性能对比测试

### 测试不同密钥提供者模式

#### 1. TLShub 模式

```bash
# 修改 /etc/tlshub/capture.conf
mode = tlshub

# 重启 capture
sudo ./capture /etc/tlshub/capture.conf

# 运行测试
./tlshub_bench --mode client \
  --target-port 9090 \
  --total-connections 100 \
  --json tlshub_mode.json
```

#### 2. OpenSSL 模式

```bash
# 修改配置
mode = openssl

# 重启并测试
sudo ./capture /etc/tlshub/capture.conf
./tlshub_bench --mode client \
  --target-port 9090 \
  --total-connections 100 \
  --json openssl_mode.json
```

#### 3. 对比结果

```bash
# 提取关键指标
echo "TLShub Mode:"
jq '.summary_metrics | {latency: .avg_connection_latency_ms, throughput: .avg_throughput_mbps}' tlshub_mode.json

echo "OpenSSL Mode:"
jq '.summary_metrics | {latency: .avg_connection_latency_ms, throughput: .avg_throughput_mbps}' openssl_mode.json
```

## 故障排查

### 问题 1: 连接失败

**症状**: 客户端连接失败或超时

**排查步骤**:
1. 检查 echo server 是否运行：`ps aux | grep tlshub_bench`
2. 检查端口是否监听：`sudo netstat -tlnp | grep 9090`
3. 检查防火墙：`sudo iptables -L | grep 9090`
4. 检查 capture 程序是否运行

### 问题 2: TLShub 未拦截流量

**症状**: 连接成功但 capture 没有日志输出

**排查步骤**:
1. 检查 TLShub 内核模块：`lsmod | grep tlshub`
2. 查看内核日志：`dmesg | tail -50`
3. 检查 eBPF 程序：`sudo bpftool prog list`
4. 验证 capture.conf 配置
5. 检查 pod_node_mapping.conf 是否包含测试 IP

### 问题 3: 握手失败

**症状**: capture 拦截但握手失败

**排查步骤**:
1. 查看详细日志：`dmesg | grep handshake`
2. 检查 Netlink 协议号配置
3. 验证 TLShub 内核模块状态
4. 重新加载内核模块：`sudo rmmod tlshub && sudo insmod tlshub.ko`

### 问题 4: 性能异常

**症状**: 延迟过高或吞吐量过低

**排查步骤**:
1. 检查系统负载：`top` 或 `htop`
2. 检查网络状态：`netstat -s`
3. 检查 KTLS 是否启用：`dmesg | grep ktls`
4. 使用 tcpdump 抓包分析：`sudo tcpdump -i lo port 9090 -w test.pcap`

## 自动化测试脚本

创建自动化测试脚本 `tlshub_integration_test.sh`：

```bash
#!/bin/bash

set -e

# 配置
SERVER_PORT=9090
TEST_DIR="integration_test_results"
mkdir -p $TEST_DIR

# 启动 echo server
echo "Starting echo server..."
./tlshub_bench --mode server --listen-port $SERVER_PORT &
SERVER_PID=$!
sleep 2

# 运行测试套件
echo "Running test suite..."

# Test 1: 小数据包
./tlshub_bench --mode client --target-port $SERVER_PORT \
  --data-size 512 --total-connections 50 \
  --json "$TEST_DIR/small_packets.json"

# Test 2: 中等数据包
./tlshub_bench --mode client --target-port $SERVER_PORT \
  --data-size 4096 --total-connections 100 \
  --json "$TEST_DIR/medium_packets.json"

# Test 3: 大数据包
./tlshub_bench --mode client --target-port $SERVER_PORT \
  --data-size 65536 --total-connections 50 \
  --json "$TEST_DIR/large_packets.json"

# 停止 server
kill $SERVER_PID

# 生成报告
echo "Generating report..."
echo "Test Results:" > "$TEST_DIR/summary.txt"
for f in "$TEST_DIR"/*.json; do
  echo "=== $(basename $f) ===" >> "$TEST_DIR/summary.txt"
  jq '.summary_metrics' "$f" >> "$TEST_DIR/summary.txt"
done

echo "Tests completed. Results in: $TEST_DIR"
```

## 最佳实践

1. **逐步测试**
   - 先测试基本连接性（小连接数）
   - 然后增加并发和数据量
   - 最后进行压力测试

2. **记录基准值**
   - 在不同环境记录性能基准
   - 用于后续对比和回归测试

3. **持续监控**
   - 使用 `dmesg -w` 实时查看内核日志
   - 使用 `htop` 监控资源使用

4. **保存测试数据**
   - 保存所有 JSON/CSV 输出
   - 记录测试时的系统配置
   - 便于后续分析和问题排查

5. **定期验证**
   - 在代码变更后运行完整测试
   - 验证性能没有退化

## 相关文档

- [bench/README.md](../bench/README.md) - tlshub_bench 详细文档
- [capture/docs/README.md](../capture/docs/README.md) - Capture 模块文档
- [docs/PERFORMANCE_METRICS_CN.md](../docs/PERFORMANCE_METRICS_CN.md) - 性能指标文档
- [tlshub-api/README.md](../tlshub-api/README.md) - TLShub API 文档

## 联系方式

如有问题，请提交 Issue：https://github.com/CapooL/tlshub-ebpf/issues
