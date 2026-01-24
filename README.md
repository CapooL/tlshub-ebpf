# TLShub-eBPF

TLShub 的 Layer 4 友好性测试仓库，包含配套的流量捕获模块。

> **重要更新（2024-01）**：所有接口已从 "hypertls" 重命名为 "tlshub" 以保持项目命名一致性。详见 [tlshub-api](./tlshub-api/) 目录。

## 项目简介

本项目为 [TLSHub](https://github.com/lin594/tlshub) 提供配套的流量捕获和测试模块。TLSHub 是一个 Layer 4 友好的内核级 mTLS 握手模块，而本项目通过 eBPF 技术实现了应用程序 Socket 连接的无感劫持和 KTLS 密钥配置。

## 主要功能

### 流量捕获模块 (capture/)

1. **Socket 连接捕获**
   - 使用 eBPF 程序捕获应用程序的 TCP 连接
   - 在 TCP 握手完成时无感劫持
   - 自动使用 KTLS 配置密钥，将 TCP 连接转换为安全通道

2. **TLSHub 集成**
   - 通过 Netlink 与 TLSHub 内核模块通信
   - 根据四元组（源/目的 IP、源/目的端口）获取密钥
   - 支持自动握手流程：fetchkey → 失败 → handshake → 再次 fetchkey

3. **Pod-Node 映射**
   - 维护 Pod 到 Node 的映射关系
   - 支持配置文件方式管理
   - 独立的查询接口，便于其他组件使用

4. **多密钥提供者支持**
   - **TLSHub 模式**（推荐）：使用 TLSHub 进行密钥协商
   - **OpenSSL 模式**：使用标准 OpenSSL 进行密钥协商
   - **BoringSSL 模式**：使用 Google BoringSSL 进行密钥协商
   - 支持运行时切换，便于性能对比测试

## 目录结构

```
.
├── README.md            # 本文件
├── tlshub/              # TLSHub 子模块（内核级 mTLS 握手模块）
├── tlshub-api/          # TLSHub 用户态 API（正确的接口调用方式）
│   ├── tlshub.h         # API 头文件
│   ├── tlshub.c         # API 实现
│   ├── tlshub_handshake.c  # 测试示例
│   ├── Makefile         # 编译配置
│   └── README.md        # API 使用说明
├── docs/                # 完整中文文档
│   ├── TLSHUB_API_CN.md        # API 详细文档
│   └── INTEGRATION_GUIDE_CN.md # 集成指南
├── capture/             # 流量捕获模块
│   ├── include/         # 头文件
│   ├── src/             # 源代码（包括 eBPF 程序）
│   ├── config/          # 配置文件
│   ├── test/            # 测试文件
│   ├── docs/            # 详细文档
│   └── Makefile         # 编译配置
└── bench/               # Layer 7 性能测试工具
    ├── include/         # 头文件
    ├── src/             # 源代码
    ├── config/          # 示例配置
    ├── test_bench.sh    # 测试脚本
    ├── Makefile         # 编译配置
    └── README.md        # 详细使用说明
```

## 快速开始

### 系统要求

- Linux 内核版本 >= 5.2（支持 eBPF 和 KTLS）
- 编译工具：gcc, clang, make
- 依赖库：libbpf, libssl (OpenSSL)

### 安装依赖

**Ubuntu/Debian**
```bash
sudo apt-get update
sudo apt-get install -y clang llvm gcc make \
    libbpf-dev libssl-dev linux-headers-$(uname -r)
```

**CentOS/RHEL**
```bash
sudo yum install -y clang llvm gcc make \
    libbpf-devel openssl-devel kernel-devel
```

### 编译

```bash
cd capture/
make
```

### 配置

编辑配置文件：
```bash
sudo mkdir -p /etc/tlshub
sudo cp config/capture.conf /etc/tlshub/
sudo cp config/pod_node_mapping.conf /etc/tlshub/

# 根据实际情况修改 Pod-Node 映射
sudo vi /etc/tlshub/pod_node_mapping.conf
```

### 运行

```bash
# 使用默认配置
sudo ./capture

# 使用自定义配置
sudo ./capture /path/to/custom/capture.conf
```

## 文档

### TLSHub API 文档

- [tlshub-api/README.md](tlshub-api/README.md) - API 快速开始
- [docs/TLSHUB_API_CN.md](docs/TLSHUB_API_CN.md) - 完整的 API 中文文档
- [docs/INTEGRATION_GUIDE_CN.md](docs/INTEGRATION_GUIDE_CN.md) - 集成指南

### Capture 模块文档

详细的中文文档位于 `capture/docs/` 目录：

- [README.md](capture/docs/README.md) - 完整的使用说明
- [ARCHITECTURE.md](capture/docs/ARCHITECTURE.md) - 架构设计文档
- [API.md](capture/docs/API.md) - API 参考文档
- [LIBBPF_COMPATIBILITY_CN.md](capture/docs/LIBBPF_COMPATIBILITY_CN.md) - libbpf 版本兼容性说明

### 性能测试文档

- [docs/PERFORMANCE_METRICS_CN.md](docs/PERFORMANCE_METRICS_CN.md) - 性能指标监测文档
- [docs/TLS_PERF_ANALYSIS_CN.md](docs/TLS_PERF_ANALYSIS_CN.md) - TLS-Perf 工具分析与测试方案

### Layer 7 性能测试工具

本项目提供了 `tlshub_bench` 工具用于 Layer 7（应用层）性能测试：

```bash
cd bench/
make
./tlshub_bench --help
```

详细文档：[bench/README.md](bench/README.md)

#### 快速开始

```bash
# 启动 echo server
./tlshub_bench --mode server --listen-port 9090

# 在另一个终端运行客户端测试
./tlshub_bench --mode client \
  --target-ip 127.0.0.1 \
  --target-port 9090 \
  --data-size 4096 \
  --total-connections 100
```

测试完成后会生成 JSON 和 CSV 格式的性能报告，包含：
- 连接建立延迟
- 吞吐量
- CPU 使用率
- 内存使用量

## 测试

```bash
cd capture/test/
sudo ./test.sh
```

## 性能对比测试

本项目支持多种密钥提供者，可以方便地进行性能对比：

```bash
# 测试 TLSHub 模式
# 编辑 /etc/tlshub/capture.conf，设置 mode = tlshub
sudo ./capture

# 测试 OpenSSL 模式
# 编辑 /etc/tlshub/capture.conf，设置 mode = openssl
sudo ./capture

# 测试 BoringSSL 模式
# 编辑 /etc/tlshub/capture.conf，设置 mode = boringssl
sudo ./capture
```

### 性能指标测试

本项目支持全面的性能指标收集和分析功能。

建议测试的性能指标：
- 连接建立延迟
- 密钥协商时间
- 吞吐量
- CPU 使用率
- 内存使用量

#### 快速开始

```bash
# 启动 capture 程序（自动启用性能监测）
sudo ./capture

# 程序退出时会自动生成性能报告和数据文件
# - perf_metrics_*.json (JSON 格式数据)
# - perf_metrics_*.csv (CSV 格式数据)
```

#### 使用性能测试工具

```bash
cd capture/test/
chmod +x perf_test.sh
sudo ./perf_test.sh
```

性能测试工具支持：
- 自动测试不同密钥提供者模式（TLShub、OpenSSL、BoringSSL）
- 一键对比所有模式的性能
- 自定义测试时长
- 自动保存和整理测试结果

详细文档请参考: [性能指标使用文档](docs/PERFORMANCE_METRICS_CN.md)

## 架构概览

```
┌─────────────────────────────────────────────────────┐
│              用户态程序 (capture)                    │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────┐ │
│  │ Pod-Node     │  │ Key Provider │  │  KTLS    │ │
│  │ Mapping      │  │  (多模式)     │  │  Config  │ │
│  └──────────────┘  └──────────────┘  └──────────┘ │
└──────────────────────┬──────────────────────────────┘
                       │ Perf Buffer Events
                       │
┌──────────────────────┴──────────────────────────────┐
│             eBPF 程序 (capture.bpf.c)                │
│  ┌──────────────┐  ┌──────────────┐               │
│  │ TCP Connect  │  │ TCP SendMsg  │               │
│  │    Hook      │  │    Hook      │               │
│  └──────────────┘  └──────────────┘               │
└──────────────────────┬──────────────────────────────┘
                       │ Netlink
                       │
┌──────────────────────┴──────────────────────────────┐
│              TLSHub 内核模块                         │
│           (tlshub/src/*)                            │
└─────────────────────────────────────────────────────┘
```

## 工作原理

1. **连接捕获**：eBPF 程序在内核层面 hook TCP 连接事件
2. **事件通知**：通过 perf buffer 将连接信息发送到用户态
3. **密钥获取**：用户态程序根据配置的模式获取密钥
   - TLSHub 模式：通过 Netlink 与 TLSHub 通信
   - OpenSSL/BoringSSL 模式：使用相应的 SSL 库
4. **KTLS 配置**：为 Socket 配置 KTLS，启用内核级加密
5. **安全通信**：后续数据传输由内核自动加密/解密

## 贡献

欢迎提交 Issue 和 Pull Request！

## 许可证

本项目采用与 TLSHub 相同的许可证。

## 相关项目

- [TLSHub](https://github.com/lin594/tlshub) - Layer 4 友好的内核级 mTLS 握手模块

## 联系方式

如有问题或建议，请通过以下方式联系：
- Issue: https://github.com/CapooL/tlshub-ebpf/issues