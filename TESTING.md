# TLSHub eBPF 测试文档

## 1. 测试环境准备

### 1.1 系统要求

- **操作系统**: Linux 内核 5.4 或更高版本
- **架构**: x86_64 (amd64)
- **权限**: root 或具有 CAP_BPF/CAP_NET_ADMIN 权限

### 1.2 必需的软件包

#### Ubuntu/Debian

```bash
sudo apt-get update
sudo apt-get install -y \
    clang \
    llvm \
    gcc \
    make \
    libbpf-dev \
    linux-headers-$(uname -r) \
    libelf-dev \
    pkg-config
```

#### CentOS/RHEL

```bash
sudo yum install -y \
    clang \
    llvm \
    gcc \
    make \
    libbpf-devel \
    kernel-devel \
    elfutils-libelf-devel
```

### 1.3 验证安装

```bash
# 检查内核版本
uname -r

# 检查编译器
clang --version
gcc --version

# 检查 libbpf
ldconfig -p | grep libbpf
```

## 2. 编译测试

### 2.1 构建项目

```bash
cd /path/to/tlshub-ebpf
make clean
make
```

### 2.2 验证构建产物

```bash
# 检查 eBPF 内核程序
ls -lh build/tlshub_kern.o

# 检查用户空间程序
ls -lh bin/tlshub

# 查看 eBPF 程序信息
llvm-objdump -S build/tlshub_kern.o
```

### 2.3 自动化测试脚本

```bash
sudo ./tools/test.sh
```

## 3. 功能测试

### 3.1 基本功能测试

#### 测试 1: 启动和停止

**目的**: 验证程序能够正常启动和停止

**步骤**:
```bash
# 1. 查看可用的网络接口
ip link show

# 2. 启动程序（将 eth0 替换为实际接口名）
sudo bin/tlshub -i eth0

# 3. 等待几秒后按 Ctrl+C 停止

# 4. 检查是否有错误消息
```

**期望结果**:
- 程序正常启动，显示 "开始捕获数据包..."
- 按 Ctrl+C 后程序正常退出
- 显示最终统计信息
- 无错误或警告消息

#### 测试 2: 捕获所有 TCP 流量

**目的**: 验证基本的数据包捕获功能

**步骤**:
```bash
# 1. 启动 TLSHub
sudo bin/tlshub -i eth0 &
TLSHUB_PID=$!

# 2. 在另一个终端生成流量
curl http://example.com

# 3. 等待几秒查看输出

# 4. 停止 TLSHub
sudo kill -INT $TLSHUB_PID
```

**期望结果**:
- 捕获到 HTTP 请求和响应的数据包
- 显示源和目标 IP 地址、端口
- 显示数据包方向（入站/出站）

#### 测试 3: HTTPS 流量识别

**目的**: 验证 TLS 握手识别功能

**步骤**:
```bash
# 1. 启动 TLSHub，只监听端口 443
sudo bin/tlshub -i eth0 -p 443 &
TLSHUB_PID=$!

# 2. 访问 HTTPS 网站
curl https://www.google.com

# 3. 查看输出

# 4. 停止程序
sudo kill -INT $TLSHUB_PID
```

**期望结果**:
- 捕获到 TLS 握手包
- 事件类型显示为 "TLS握手"
- 显示 TLS 载荷的前 16 字节（包含 0x16 0x03）

#### 测试 4: 端口过滤

**目的**: 验证端口过滤功能

**步骤**:
```bash
# 1. 启动 TLSHub，只监听端口 80 和 443
sudo bin/tlshub -i eth0 -p 80 -p 443 &
TLSHUB_PID=$!

# 2. 访问 HTTP 和 HTTPS 网站
curl http://example.com
curl https://www.google.com

# 3. 尝试访问其他端口（如 SSH 22）
ssh localhost  # 这应该不会被捕获

# 4. 停止程序
sudo kill -INT $TLSHUB_PID
```

**期望结果**:
- 捕获到端口 80 和 443 的流量
- 不捕获其他端口的流量

### 3.2 压力测试

#### 测试 5: 高流量场景

**目的**: 测试在高流量下的性能和稳定性

**步骤**:
```bash
# 1. 启动 TLSHub
sudo bin/tlshub -i eth0 -p 80 &
TLSHUB_PID=$!

# 2. 使用 ab 工具生成大量请求
ab -n 10000 -c 100 http://example.com/

# 3. 观察输出和性能

# 4. 停止程序
sudo kill -INT $TLSHUB_PID
```

**期望结果**:
- 程序保持稳定，不崩溃
- 显示统计信息
- 如果有事件丢失，会显示警告信息

### 3.3 边界测试

#### 测试 6: 无效网络接口

**目的**: 测试错误处理

**步骤**:
```bash
sudo bin/tlshub -i nonexistent_interface
```

**期望结果**:
- 显示错误消息: "找不到网络接口"
- 程序正常退出，返回非零状态码

#### 测试 7: 非 root 用户

**目的**: 测试权限检查

**步骤**:
```bash
bin/tlshub -i eth0
```

**期望结果**:
- 显示权限相关的错误消息
- 程序退出

## 4. 性能测试

### 4.1 延迟测试

**测试脚本**:
```bash
#!/bin/bash
# 测量数据包捕获延迟

# 启动 TLSHub
sudo bin/tlshub -i eth0 -p 80 > /tmp/tlshub.log 2>&1 &
TLSHUB_PID=$!

# 等待初始化
sleep 2

# 发送测试包并记录时间
START=$(date +%s%N)
curl -s http://example.com > /dev/null
END=$(date +%s%N)

# 计算延迟
LATENCY=$(( (END - START) / 1000000 ))
echo "请求延迟: ${LATENCY} ms"

# 停止 TLSHub
sudo kill -INT $TLSHUB_PID

# 检查是否捕获到包
grep "数据包捕获" /tmp/tlshub.log
```

### 4.2 吞吐量测试

**测试脚本**:
```bash
#!/bin/bash
# 测量处理吞吐量

sudo bin/tlshub -i eth0 > /tmp/tlshub.log 2>&1 &
TLSHUB_PID=$!

sleep 2

# 生成流量并测量
START=$(date +%s)
ab -n 50000 -c 50 http://example.com/ > /dev/null 2>&1
END=$(date +%s)

DURATION=$((END - START))
echo "测试持续时间: ${DURATION} 秒"
echo "平均每秒请求数: $((50000 / DURATION))"

sudo kill -INT $TLSHUB_PID

# 显示统计信息
grep "流量统计" -A 5 /tmp/tlshub.log
```

### 4.3 资源使用测试

**监控 CPU 和内存使用**:
```bash
# 启动 TLSHub
sudo bin/tlshub -i eth0 &
TLSHUB_PID=$!

# 监控资源使用（运行 60 秒）
for i in {1..60}; do
    ps aux | grep $TLSHUB_PID | grep -v grep
    sleep 1
done

sudo kill -INT $TLSHUB_PID
```

## 5. 集成测试

### 5.1 与其他工具配合

#### 测试 8: 与 tcpdump 对比

**目的**: 验证捕获的准确性

**步骤**:
```bash
# 1. 同时启动 tcpdump 和 TLSHub
sudo tcpdump -i eth0 -nn 'tcp port 80' -c 100 > /tmp/tcpdump.log 2>&1 &
sudo bin/tlshub -i eth0 -p 80 > /tmp/tlshub.log 2>&1 &

# 2. 生成流量
curl http://example.com

# 3. 等待捕获完成
sleep 5

# 4. 停止两个程序
sudo killall tcpdump tlshub

# 5. 对比结果
echo "tcpdump 捕获数:"
grep "packets captured" /tmp/tcpdump.log

echo "TLSHub 统计:"
grep "流量统计" -A 5 /tmp/tlshub.log
```

## 6. 回归测试

### 6.1 测试清单

创建测试清单，每次修改后都应执行：

- [ ] 编译测试通过
- [ ] 基本启动/停止测试通过
- [ ] 数据包捕获功能正常
- [ ] TLS 识别功能正常
- [ ] 端口过滤功能正常
- [ ] 统计信息准确
- [ ] 错误处理正确
- [ ] 无内存泄漏
- [ ] 性能符合预期

### 6.2 自动化脚本

```bash
#!/bin/bash
# 回归测试套件

set -e

echo "开始回归测试..."

# 编译测试
make clean && make
echo "✓ 编译测试通过"

# 基本功能测试
timeout 5 sudo bin/tlshub -i lo &
sleep 2
sudo killall tlshub || true
echo "✓ 启动测试通过"

# 添加更多自动化测试...

echo "所有回归测试通过！"
```

## 7. 故障排查指南

### 7.1 常见问题

#### 问题 1: "无法加载 BPF 对象"

**可能原因**:
- 缺少内核头文件
- eBPF 程序验证失败
- libbpf 版本不兼容

**解决方法**:
```bash
# 安装内核头文件
sudo apt-get install linux-headers-$(uname -r)

# 检查 libbpf 版本
pkg-config --modversion libbpf

# 查看详细错误信息
sudo dmesg | tail -20
```

#### 问题 2: "无法附加 XDP 程序"

**可能原因**:
- 网络接口不支持 XDP
- 已有其他 XDP 程序附加

**解决方法**:
```bash
# 检查是否已有 XDP 程序
ip link show eth0

# 卸载已有的 XDP 程序
sudo ip link set dev eth0 xdp off

# 尝试使用 SKB 模式（兼容性更好）
# 代码已默认使用 XDP_FLAGS_SKB_MODE
```

#### 问题 3: "事件丢失"

**可能原因**:
- 流量过大，perf buffer 溢出
- 用户空间处理速度不够快

**解决方法**:
- 增加 perf buffer 大小 (修改 PERF_BUFFER_PAGES)
- 添加端口过滤，减少需要处理的事件
- 优化事件处理逻辑

### 7.2 调试技巧

#### 查看 eBPF 程序信息

```bash
# 列出已加载的 eBPF 程序
sudo bpftool prog list

# 查看特定程序的详情
sudo bpftool prog show id <ID>

# 查看 map 内容
sudo bpftool map list
sudo bpftool map dump id <ID>
```

#### 内核日志

```bash
# 查看 eBPF 相关的内核日志
sudo dmesg | grep -i bpf

# 实时监控内核日志
sudo dmesg -w
```

## 8. 测试报告模板

### 8.1 测试报告格式

```
测试报告: TLSHub eBPF
测试日期: YYYY-MM-DD
测试人员: [姓名]
测试环境: [Linux 版本], [内核版本]

测试结果总结:
- 编译测试: [通过/失败]
- 功能测试: [通过/失败]
- 性能测试: [通过/失败]
- 压力测试: [通过/失败]

详细测试结果:
1. 基本功能测试
   - 测试 1: [通过/失败] - [备注]
   - 测试 2: [通过/失败] - [备注]
   ...

2. 性能指标
   - 平均延迟: [X] ms
   - 吞吐量: [X] pkt/s
   - CPU 使用率: [X]%
   - 内存使用: [X] MB

问题和建议:
- [列出发现的问题]
- [改进建议]

结论:
[总体评价]
```

## 9. 持续集成建议

### 9.1 CI/CD 流程

建议集成到 CI/CD 管道中：

```yaml
# GitHub Actions 示例
name: Test TLSHub eBPF

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      
      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y clang llvm libbpf-dev
      
      - name: Build
        run: make
      
      - name: Run tests
        run: sudo ./tools/test.sh
```

## 10. 测试总结

完整的测试应包括：

1. **单元测试**: 各个组件的独立测试
2. **集成测试**: 组件间的协同测试
3. **性能测试**: 延迟、吞吐量、资源使用
4. **压力测试**: 高负载场景
5. **回归测试**: 确保修改不破坏现有功能
6. **安全测试**: 权限检查、异常处理

建议在每次代码修改后都运行完整的测试套件，确保软件质量。
