# TLShub 性能指标监测文档

## 概述

TLShub 性能指标监测模块提供了全面的性能数据收集和分析功能，帮助用户评估和优化 TLS 连接的性能表现。本模块支持实时监测多种关键性能指标，并提供详细的统计报告和数据导出功能。

## 功能特性

### 支持的性能指标

1. **连接建立延迟** (Connection Establishment Latency)
   - 测量从连接开始到连接建立完成的时间
   - 包括 TCP 三次握手和 TLS 握手的总时间
   - 提供平均值、最小值、最大值统计

2. **密钥协商时间** (Key Negotiation Time)
   - 测量密钥协商过程的耗时
   - 支持不同密钥提供者模式（TLShub、OpenSSL、BoringSSL）
   - 用于对比不同方案的性能差异

3. **吞吐量** (Throughput)
   - 测量数据传输速率（Mbps）
   - 统计发送和接收的字节数
   - 计算平均吞吐量和峰值吞吐量

4. **CPU 使用率** (CPU Usage)
   - 实时监测程序的 CPU 使用率
   - 记录平均 CPU 使用率和峰值使用率
   - 帮助识别性能瓶颈

5. **内存使用量** (Memory Usage)
   - 监测物理内存（RSS）和虚拟内存（VMS）使用情况
   - 以 KB 和 MB 为单位显示
   - 跟踪内存使用趋势

### 数据导出功能

- **JSON 格式**: 结构化数据，便于程序处理和分析
- **CSV 格式**: 表格数据，便于在 Excel 等工具中查看
- **实时报告**: 在程序退出时自动生成性能统计报告

## 快速开始

### 1. 编译

性能指标模块已集成到主程序中，使用标准编译命令即可：

```bash
cd capture/
make clean
make
```

### 2. 运行

启动 capture 程序后，性能指标监测会自动启用：

```bash
sudo ./capture
```

程序运行时会：
- 自动收集性能数据
- 每 5 秒更新一次系统性能指标（CPU、内存）
- 在程序退出时生成完整的性能报告

### 3. 查看结果

程序退出后，会自动生成性能报告并导出数据文件：

```bash
# 查看控制台输出的性能报告
# 报告会显示在程序退出时

# 查看导出的 JSON 数据
cat perf_metrics_*.json

# 查看导出的 CSV 数据
cat perf_metrics_*.csv
```

## 使用示例

### 基本使用

```bash
# 1. 启动 capture 程序
sudo ./capture

# 2. 程序会开始监测性能
# 3. 按 Ctrl+C 停止程序
# 4. 程序会自动打印性能报告并导出数据
```

### 使用性能测试脚本

我们提供了便捷的性能测试脚本，可以自动测试不同模式的性能：

```bash
cd capture/test/
chmod +x perf_test.sh
sudo ./perf_test.sh
```

测试脚本支持：
- 单独测试 TLShub、OpenSSL 或 BoringSSL 模式
- 一键测试所有模式并对比
- 自定义测试时长
- 自动保存测试结果

## 性能报告示例

### 控制台输出

程序退出时会显示如下格式的性能报告：

```
╔════════════════════════════════════════════════════════════════╗
║            TLShub 性能指标统计报告                              ║
╚════════════════════════════════════════════════════════════════╝

【连接统计】
  总连接数:       150
  成功连接:       148
  失败连接:       2
  活跃连接:       10

【连接建立延迟】
  平均延迟:       12.345 ms
  最小延迟:       8.123 ms
  最大延迟:       25.678 ms

【密钥协商时间】
  平均时间:       5.678 ms
  最小时间:       3.456 ms
  最大时间:       10.234 ms

【吞吐量统计】
  平均吞吐量:     125.67 Mbps
  峰值吞吐量:     256.89 Mbps
  总传输字节:     1048576000 bytes (1000.00 MB)

【CPU 使用率】
  当前 CPU:       15.67%
  峰值 CPU:       28.45%

【内存使用量】
  当前内存 (RSS): 12345 KB (12.05 MB)
  虚拟内存 (VMS): 45678 KB (44.61 MB)
```

### JSON 输出示例

```json
{
  "timestamp": 1706000000,
  "summary": {
    "total_connections": 150,
    "successful_connections": 148,
    "failed_connections": 2,
    "active_connections": 10,
    "avg_connection_latency_ms": 12.345,
    "min_connection_latency_ms": 8.123,
    "max_connection_latency_ms": 25.678,
    "avg_key_negotiation_ms": 5.678,
    "min_key_negotiation_ms": 3.456,
    "max_key_negotiation_ms": 10.234,
    "avg_throughput_mbps": 125.67,
    "max_throughput_mbps": 256.89,
    "total_bytes_transferred": 1048576000,
    "cpu_usage_percent": 15.67,
    "memory_usage_kb": 12345
  },
  "connections": [
    {
      "connection_id": 1234567890123456,
      "connection_latency_ms": 12.345,
      "key_negotiation_ms": 5.678,
      "bytes_sent": 1024000,
      "bytes_received": 2048000,
      "throughput_mbps": 125.67
    }
    // ... 更多连接数据
  ]
}
```

### CSV 输出示例

```csv
connection_id,connection_latency_ms,key_negotiation_ms,bytes_sent,bytes_received,throughput_mbps
1234567890123456,12.345,5.678,1024000,2048000,125.67
1234567890123457,11.234,4.567,2048000,4096000,156.78
...
```

## API 参考

### 数据结构

#### 性能指标上下文

```c
struct perf_metrics_ctx {
    struct connection_metrics *conn_metrics;  /* 连接指标数组 */
    __u32 max_connections;                    /* 最大连接数 */
    __u32 current_connections;                /* 当前连接数 */
    struct system_metrics system_metrics;     /* 系统指标 */
    struct performance_stats stats;           /* 统计汇总 */
    int monitoring_enabled;                   /* 是否启用监控 */
};
```

#### 连接性能指标

```c
struct connection_metrics {
    __u64 connection_id;           /* 连接ID */
    struct timepoint connection_latency;  /* 连接建立延迟 */
    struct timepoint key_negotiation;     /* 密钥协商时间 */
    __u64 bytes_sent;              /* 发送字节数 */
    __u64 bytes_received;          /* 接收字节数 */
    double throughput_mbps;        /* 吞吐量（Mbps） */
};
```

### 主要函数

#### 初始化和清理

```c
/* 初始化性能指标模块 */
struct perf_metrics_ctx* perf_metrics_init(__u32 max_connections);

/* 清理性能指标模块 */
void perf_metrics_cleanup(struct perf_metrics_ctx *ctx);

/* 启用/禁用性能监控 */
void perf_metrics_set_enabled(struct perf_metrics_ctx *ctx, int enabled);
```

#### 连接指标记录

```c
/* 记录连接开始 */
int perf_metrics_connection_start(struct perf_metrics_ctx *ctx, __u64 connection_id);

/* 记录连接建立延迟 */
void perf_metrics_connection_latency_start(struct perf_metrics_ctx *ctx, int conn_index);
void perf_metrics_connection_latency_end(struct perf_metrics_ctx *ctx, int conn_index);

/* 记录密钥协商时间 */
void perf_metrics_key_negotiation_start(struct perf_metrics_ctx *ctx, int conn_index);
void perf_metrics_key_negotiation_end(struct perf_metrics_ctx *ctx, int conn_index);

/* 记录数据传输 */
void perf_metrics_record_data_transfer(struct perf_metrics_ctx *ctx, int conn_index,
                                       __u64 bytes_sent, __u64 bytes_received);

/* 记录连接结束 */
void perf_metrics_connection_end(struct perf_metrics_ctx *ctx, int conn_index, int success);
```

#### 系统指标和报告

```c
/* 更新系统性能指标 */
int perf_metrics_update_system(struct perf_metrics_ctx *ctx);

/* 计算统计汇总 */
void perf_metrics_calculate_stats(struct perf_metrics_ctx *ctx);

/* 打印性能统计报告 */
void perf_metrics_print_report(struct perf_metrics_ctx *ctx);

/* 导出性能指标到 JSON 文件 */
int perf_metrics_export_json(struct perf_metrics_ctx *ctx, const char *filename);

/* 导出性能统计到 CSV 文件 */
int perf_metrics_export_csv(struct perf_metrics_ctx *ctx, const char *filename);
```

## 性能测试最佳实践

### 1. 测试环境准备

- 确保系统资源充足（CPU、内存、网络）
- 关闭不必要的后台服务
- 使用相同的硬件配置进行对比测试

### 2. 测试方法

#### 单模式测试

测试特定密钥提供者模式的性能：

```bash
# 测试 TLShub 模式
sudo ./perf_test.sh
# 选择选项 1

# 测试 OpenSSL 模式
sudo ./perf_test.sh
# 选择选项 2

# 测试 BoringSSL 模式
sudo ./perf_test.sh
# 选择选项 3
```

#### 对比测试

自动测试所有模式并对比：

```bash
sudo ./perf_test.sh
# 选择选项 4（全部模式）
```

#### 自定义测试时长

根据需要调整测试时长：

```bash
sudo ./perf_test.sh
# 选择选项 5，然后输入测试时长（如 300 秒）
```

### 3. 结果分析

#### 分析连接延迟

- 关注平均延迟和最大延迟
- 对比不同模式下的延迟差异
- 识别异常高的延迟值

#### 分析密钥协商时间

- 评估不同密钥提供者的性能
- TLShub 模式通常应该最快
- 注意首次协商可能较慢（缓存预热）

#### 分析吞吐量

- 查看平均吞吐量和峰值吞吐量
- 评估是否达到预期性能
- 识别吞吐量瓶颈

#### 分析系统资源

- CPU 使用率应在合理范围内（通常 < 50%）
- 内存使用量应稳定，无明显增长
- 注意资源使用的峰值

### 4. 优化建议

根据测试结果进行优化：

- **高延迟**: 检查网络配置、优化密钥协商流程
- **低吞吐量**: 调整 eBPF 缓冲区大小、优化数据处理
- **高 CPU 使用**: 减少不必要的日志、优化算法
- **高内存使用**: 检查内存泄漏、调整缓存大小

## 故障排查

### 性能指标未收集

**问题**: 程序运行但没有生成性能报告

**解决方法**:
1. 检查是否有实际的连接建立
2. 确认性能监控已启用
3. 查看是否有错误日志

### 数据导出失败

**问题**: 无法导出 JSON 或 CSV 文件

**解决方法**:
1. 检查磁盘空间
2. 确认有写入权限
3. 查看错误消息

### 系统指标异常

**问题**: CPU 或内存使用率数据不准确

**解决方法**:
1. 检查 /proc/stat 和 /proc/self/status 是否可访问
2. 确认以足够的权限运行
3. 验证系统内核版本

## 性能基准

以下是典型场景下的性能参考值（仅供参考，实际值取决于硬件和配置）：

| 指标 | TLShub 模式 | OpenSSL 模式 | BoringSSL 模式 |
|------|------------|--------------|----------------|
| 连接建立延迟 | 8-15 ms | 15-25 ms | 12-20 ms |
| 密钥协商时间 | 3-8 ms | 8-15 ms | 6-12 ms |
| 吞吐量 | 100-200 Mbps | 80-150 Mbps | 90-180 Mbps |
| CPU 使用率 | 10-20% | 15-30% | 12-25% |
| 内存使用量 | 10-20 MB | 15-30 MB | 12-25 MB |

**注意**: 这些数值仅供参考，实际性能取决于多种因素。

## 常见问题

### Q1: 如何减少性能监测的开销？

**A**: 性能监测本身会带来一定的开销（通常 < 5%）。如果需要最小化开销：
- 减少测量点的数量
- 降低系统指标更新频率
- 禁用不需要的指标收集

### Q2: 可以在生产环境使用吗？

**A**: 可以，但建议：
- 在低流量时段测试
- 限制最大连接数
- 定期导出数据以避免内存累积

### Q3: 如何自定义性能指标？

**A**: 可以修改 `performance_metrics.h` 和 `performance_metrics.c`：
1. 添加新的指标字段到结构体
2. 实现相应的收集和计算函数
3. 更新报告和导出函数

### Q4: 支持远程监控吗？

**A**: 当前版本不直接支持远程监控，但可以：
- 定期导出数据到共享存储
- 使用日志收集工具（如 Fluentd、Logstash）
- 集成到监控系统（如 Prometheus、Grafana）

## 进阶使用

### 集成到自动化测试

将性能测试集成到 CI/CD 流程：

```bash
#!/bin/bash
# 在 CI/CD 中运行性能测试

# 运行测试
timeout 60s sudo ./capture > test.log 2>&1 &
CAPTURE_PID=$!

# 生成流量
# ... 你的流量生成脚本 ...

# 等待完成
wait $CAPTURE_PID

# 检查结果
if [ -f "perf_metrics_*.json" ]; then
    echo "Performance test completed"
    # 解析 JSON 并验证指标
    python3 validate_metrics.py perf_metrics_*.json
else
    echo "Performance test failed"
    exit 1
fi
```

### 与监控系统集成

将性能数据导出到 Prometheus：

```python
#!/usr/bin/env python3
# convert_to_prometheus.py

import json
import sys

def convert_to_prometheus(json_file):
    with open(json_file, 'r') as f:
        data = json.load(f)
    
    summary = data['summary']
    
    print(f"tlshub_connections_total {summary['total_connections']}")
    print(f"tlshub_connection_latency_avg_ms {summary['avg_connection_latency_ms']}")
    print(f"tlshub_key_negotiation_avg_ms {summary['avg_key_negotiation_ms']}")
    print(f"tlshub_throughput_avg_mbps {summary['avg_throughput_mbps']}")
    print(f"tlshub_cpu_usage_percent {summary['cpu_usage_percent']}")
    print(f"tlshub_memory_usage_kb {summary['memory_usage_kb']}")

if __name__ == '__main__':
    convert_to_prometheus(sys.argv[1])
```

## 更新日志

### v1.0.0 (2024-01)

- 初始版本发布
- 支持基本性能指标收集
- 提供 JSON 和 CSV 导出
- 包含性能测试脚本

## 反馈和贡献

如有问题或建议，请通过以下方式联系：

- GitHub Issues: https://github.com/CapooL/tlshub-ebpf/issues
- 提交 Pull Request 贡献代码

## 许可证

本项目采用与 TLShub 相同的许可证。

---

**文档版本**: 1.0.0  
**最后更新**: 2024-01  
**维护者**: TLShub 开发团队
