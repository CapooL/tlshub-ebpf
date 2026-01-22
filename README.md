# TLSHub eBPF

[![License](https://img.shields.io/badge/license-GPL--2.0-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Linux-green.svg)](https://www.kernel.org/)

TLSHub eBPF 是一个基于 eBPF (Extended Berkeley Packet Filter) 技术的高性能网络流量捕获和分析工具，专为 TLSHub 项目设计。

## 目录

- [功能特性](#功能特性)
- [系统要求](#系统要求)
- [快速开始](#快速开始)
- [使用说明](#使用说明)
- [项目结构](#项目结构)
- [详细文档](#详细文档)
- [开发指南](#开发指南)
- [常见问题](#常见问题)
- [贡献指南](#贡献指南)
- [许可证](#许可证)

## 功能特性

### 核心功能

- ✅ **高性能数据包捕获**: 基于 XDP 和 TC 的内核级流量捕获
- ✅ **TLS 流量识别**: 自动识别和标记 TLS 握手包
- ✅ **双向流量监控**: 同时捕获入站和出站流量
- ✅ **端口过滤**: 支持动态配置端口过滤规则
- ✅ **实时统计**: 流量统计和性能指标监控
- ✅ **低侵入性**: 无需修改内核代码或加载内核模块

### 技术特点

- 基于 eBPF 技术，在内核空间高效处理数据包
- 使用 XDP (eXpress Data Path) 实现零拷贝数据包处理
- 通过 Perf Buffer 高效传递事件到用户空间
- Per-CPU 统计避免锁竞争，支持多核扩展

## 系统要求

### 操作系统

- **Linux 内核**: 5.4 或更高版本（推荐 5.10+）
- **架构**: x86_64 (amd64)

### 必需软件

- `clang` >= 10.0
- `llvm` >= 10.0
- `gcc`
- `make`
- `libbpf-dev` / `libbpf-devel`
- `linux-headers` (内核头文件)
- `libelf-dev` / `elfutils-libelf-devel`

### 权限要求

- Root 权限或 `CAP_BPF` + `CAP_NET_ADMIN` 能力

## 快速开始

### 1. 安装依赖

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
    libelf-dev
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

### 2. 编译项目

```bash
# 克隆仓库
git clone https://github.com/CapooL/tlshub-ebpf.git
cd tlshub-ebpf

# 编译
make

# 验证编译结果
ls -lh build/tlshub_kern.o  # eBPF 内核程序
ls -lh bin/tlshub           # 用户空间程序
```

### 3. 运行测试

```bash
# 运行自动化测试（需要 root 权限）
sudo ./tools/test.sh
```

### 4. 基本使用

```bash
# 查看网络接口
ip link show

# 监听所有 TCP 流量（替换 eth0 为实际接口名）
sudo bin/tlshub -i eth0

# 只监听 HTTPS 流量（端口 443）
sudo bin/tlshub -i eth0 -p 443

# 监听多个端口
sudo bin/tlshub -i eth0 -p 80 -p 443 -p 8443

# 按 Ctrl+C 停止程序
```

## 使用说明

### 命令行选项

```
用法: tlshub [选项]

选项:
  -i <接口名>    指定网络接口 (必需)
                示例: -i eth0, -i wlan0
  
  -p <端口>      添加端口过滤 (可多次使用)
                只捕获指定端口的流量
                示例: -p 443, -p 80 -p 443
  
  -h             显示帮助信息
```

### 使用示例

#### 示例 1: 监听所有流量

```bash
sudo bin/tlshub -i eth0
```

**输出示例**:
```
正在加载 eBPF 程序...
正在附加 XDP 程序到接口 eth0...

开始捕获数据包... (按 Ctrl+C 停止)

[1234567890123] 事件类型: 数据包捕获 | 方向: 入站
  源地址: 192.168.1.100:54321 -> 目标地址: 93.184.216.34:80
  协议: 6 | PID: 1234 | 载荷长度: 128 字节

[1234567890456] 事件类型: TLS握手 | 方向: 入站
  源地址: 192.168.1.100:54322 -> 目标地址: 142.250.185.46:443
  协议: 6 | PID: 1235 | 载荷长度: 517 字节
  TLS 载荷 (前16字节): 16 03 01 02 00 01 00 01 fc 03 03 a1 b2 c3 d4 e5
```

#### 示例 2: 只监听 HTTPS 流量

```bash
sudo bin/tlshub -i eth0 -p 443
```

这只会捕获端口 443 的流量，过滤掉其他端口。

#### 示例 3: 使用示例脚本

```bash
cd examples
sudo ./monitor_https.sh
```

### 输出说明

程序输出包含以下信息：

- **时间戳**: 数据包捕获的纳秒级时间戳
- **事件类型**: 
  - `数据包捕获`: 普通 TCP 数据包
  - `TLS握手`: 识别出的 TLS 握手包
  - `新连接`: 新建立的连接（未来功能）
  - `连接关闭`: 连接关闭（未来功能）
- **方向**: `入站` 或 `出站`
- **源地址/目标地址**: IP 地址和端口号
- **协议**: IP 协议号（6 = TCP, 17 = UDP）
- **PID**: 进程 ID（如果可获取）
- **载荷长度**: 捕获的载荷字节数

停止程序时会显示统计信息：

```
=== 流量统计 ===
接收: 12345 数据包, 1234567 字节
发送: 6789 数据包, 678901 字节
丢弃: 0 数据包
================
```

## 项目结构

```
tlshub-ebpf/
├── src/                        # eBPF 内核程序源码
│   └── tlshub_kern.c          # XDP/TC 程序实现
├── userspace/                  # 用户空间程序
│   └── src/
│       └── tlshub_user.c      # 用户空间主程序
├── include/                    # 共享头文件
│   └── tlshub_common.h        # 内核/用户空间共享数据结构
├── build/                      # 编译输出 (eBPF .o 文件)
├── bin/                        # 可执行文件
├── examples/                   # 使用示例
│   └── monitor_https.sh       # HTTPS 监控示例
├── tools/                      # 工具脚本
│   └── test.sh                # 自动化测试脚本
├── Makefile                    # 构建配置
├── README.md                   # 本文件
├── DESIGN.md                   # 设计文档
├── TESTING.md                  # 测试文档
└── tlshub/                     # TLSHub 子模块 (可选)
```

### 核心文件说明

- **tlshub_kern.c**: eBPF 内核程序，实现数据包捕获和处理逻辑
- **tlshub_user.c**: 用户空间程序，负责加载 eBPF 程序并处理事件
- **tlshub_common.h**: 定义内核和用户空间共享的数据结构

## 详细文档

- 📘 [设计文档](DESIGN.md) - 系统架构和技术细节
- 📗 [测试文档](TESTING.md) - 测试方案和故障排查指南

### 设计文档概览

设计文档包含：

1. **系统架构**: 整体架构和数据流
2. **核心组件**: eBPF 程序、Maps、用户空间程序详解
3. **关键技术**: eBPF、XDP、Perf Buffer 技术说明
4. **性能优化**: 内核空间和用户空间优化策略
5. **安全考虑**: 权限控制和数据安全
6. **扩展性**: 如何添加新功能和协议支持

### 测试文档概览

测试文档包含：

1. **环境准备**: 系统要求和依赖安装
2. **编译测试**: 构建验证
3. **功能测试**: 8 个详细的功能测试用例
4. **性能测试**: 延迟、吞吐量、资源使用测试
5. **集成测试**: 与其他工具的配合测试
6. **故障排查**: 常见问题和解决方案

## 开发指南

### 编译流程

```bash
# 清理旧的构建文件
make clean

# 完整构建
make

# 只构建 eBPF 内核程序
make build/tlshub_kern.o

# 只构建用户空间程序
make bin/tlshub
```

### 调试技巧

#### 查看 eBPF 程序信息

```bash
# 列出已加载的 eBPF 程序
sudo bpftool prog list

# 查看 BPF maps
sudo bpftool map list

# 查看 map 内容
sudo bpftool map dump name stats_map
```

#### 查看内核日志

```bash
# 查看 eBPF 相关日志
sudo dmesg | grep -i bpf

# 实时监控
sudo dmesg -w
```

#### 反汇编 eBPF 程序

```bash
llvm-objdump -S build/tlshub_kern.o
```

### 添加新功能

1. **添加新的协议支持**:
   - 在 `tlshub_kern.c` 中添加协议解析函数
   - 更新 `parse_*` 函数

2. **添加新的事件类型**:
   - 在 `tlshub_common.h` 中定义新事件类型
   - 在内核程序中生成事件
   - 在用户程序中处理事件

3. **添加新的 Hook 点**:
   - 在 `tlshub_kern.c` 中添加新的 SEC 节
   - 在用户程序中加载和附加新程序

## 常见问题

### Q1: 无法编译，提示找不到头文件

**A**: 确保已安装内核头文件和 libbpf-dev：

```bash
sudo apt-get install linux-headers-$(uname -r) libbpf-dev
```

### Q2: 运行时提示 "无法附加 XDP 程序"

**A**: 可能原因：
- 检查是否有 root 权限
- 确认网络接口名称正确
- 卸载已有的 XDP 程序: `sudo ip link set dev eth0 xdp off`

### Q3: 捕获不到任何数据包

**A**: 排查步骤：
1. 确认网络接口有流量: `sudo tcpdump -i eth0 -c 10`
2. 检查端口过滤设置是否过于严格
3. 查看内核日志: `sudo dmesg | tail -20`

### Q4: 出现 "事件丢失" 警告

**A**: 这是因为流量过大，perf buffer 溢出。解决方法：
- 使用端口过滤减少监控范围
- 增加 PERF_BUFFER_PAGES 值（需要重新编译）
- 优化事件处理速度

### Q5: 支持哪些网络接口类型？

**A**: 支持大多数常见的网络接口：
- 物理网卡 (eth0, ens33)
- 无线网卡 (wlan0)
- 虚拟网卡 (veth, tap)
- 回环接口 (lo)

### Q6: 能否在虚拟机中使用？

**A**: 可以，但需要：
- 虚拟机运行 Linux 5.4+ 内核
- 确保 eBPF 功能未被禁用
- 某些云环境可能限制 eBPF 使用

## 性能考虑

### 最佳实践

1. **使用端口过滤**: 只监控需要的端口可显著降低开销
2. **选择合适的网络接口**: 监控流量较小的接口
3. **调整 buffer 大小**: 根据实际流量调整 PERF_BUFFER_PAGES
4. **避免过度日志**: 高流量场景下考虑重定向输出

### 性能指标参考

在标准配置下（Intel Core i5, 4GB RAM）：

- **延迟**: < 100 μs (微秒)
- **吞吐量**: > 10 Gbps
- **CPU 使用**: < 5% (正常流量)
- **内存使用**: < 50 MB

## 安全注意事项

1. **权限管理**: 程序需要 root 权限，请确保运行环境安全
2. **数据隐私**: 程序会捕获网络流量，包括部分载荷数据
3. **生产环境**: 在生产环境使用前请充分测试
4. **日志管理**: 注意日志文件可能包含敏感信息

## 贡献指南

欢迎贡献！请遵循以下步骤：

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 开启 Pull Request

### 代码风格

- 遵循 Linux 内核代码风格
- 使用有意义的变量名和函数名
- 添加必要的注释（中文或英文）
- 确保代码通过测试

## 路线图

### 当前版本 (v1.0)

- ✅ 基本数据包捕获
- ✅ TLS 流量识别
- ✅ 端口过滤
- ✅ 流量统计

### 计划功能

- [ ] IPv6 支持
- [ ] UDP 协议支持
- [ ] 更详细的 TLS 分析
- [ ] 配置文件支持
- [ ] Web 界面
- [ ] 与 tlshub 内核模块集成

## 致谢

- Linux eBPF 社区
- libbpf 项目
- XDP 项目
- TLSHub 项目

## 许可证

本项目采用 GPL-2.0 许可证。详见 [LICENSE](LICENSE) 文件。

## 联系方式

- 项目主页: https://github.com/CapooL/tlshub-ebpf
- 问题反馈: https://github.com/CapooL/tlshub-ebpf/issues

---

**注意**: 本项目仅用于学习和研究目的。在生产环境使用前请充分测试，并遵守相关法律法规。