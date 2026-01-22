# TLShub 性能测试工具和示例

本目录包含性能测试工具和使用示例。

## 文件说明

### 性能测试脚本

- **perf_test.sh**: 自动化性能测试脚本
  - 支持测试不同密钥提供者模式（TLShub、OpenSSL、BoringSSL）
  - 提供交互式菜单
  - 自动保存和整理测试结果

### 示例程序

- **example_metrics.c**: 性能指标使用示例
  - 演示如何初始化性能指标模块
  - 展示如何记录各种性能数据
  - 说明如何导出和查看结果

### 其他测试

- **test_pod_mapping.c**: Pod-Node 映射功能测试
- **test.sh**: 基本功能测试脚本
- **analyze_perf.py**: 性能数据分析工具（Python脚本）

## 使用方法

### 运行性能测试脚本

```bash
# 确保脚本有执行权限
chmod +x perf_test.sh

# 运行测试脚本（需要 root 权限）
sudo ./perf_test.sh
```

### 编译和运行示例程序

```bash
# 编译示例程序
gcc -o example_metrics example_metrics.c ../src/performance_metrics.c -I../include

# 运行示例
./example_metrics

# 查看生成的结果文件
cat example_metrics.json
cat example_metrics.csv
```

## 性能测试脚本使用指南

运行 `perf_test.sh` 后，会显示以下菜单：

```
请选择测试模式:
  1) TLShub 模式
  2) OpenSSL 模式
  3) BoringSSL 模式
  4) 全部模式（依次测试）
  5) 自定义测试时长
  6) 退出
```

### 选项说明

1. **单模式测试**: 选择 1、2 或 3 测试特定模式
2. **全部模式测试**: 选择 4 自动测试所有模式并保存结果
3. **自定义时长**: 选择 5 设置测试时长（默认 60 秒）
4. **退出**: 选择 6 退出测试工具

### 测试结果

测试结果保存在 `./perf_results/` 目录：

```
perf_results/
├── perf_tlshub_20240122_143000.json
├── perf_tlshub_20240122_143000.csv
├── perf_openssl_20240122_143100.json
├── perf_openssl_20240122_143100.csv
├── perf_boringssl_20240122_143200.json
└── perf_boringssl_20240122_143200.csv
```

## 性能对比分析

使用测试脚本收集数据后，可以进行性能对比：

### 对比连接延迟

```bash
# 提取所有模式的平均连接延迟
grep "avg_connection_latency_ms" perf_results/*.json
```

### 对比密钥协商时间

```bash
# 提取所有模式的平均密钥协商时间
grep "avg_key_negotiation_ms" perf_results/*.json
```

### 对比吞吐量

```bash
# 提取所有模式的平均吞吐量
grep "avg_throughput_mbps" perf_results/*.json
```

### 使用分析工具

我们提供了 Python 分析工具，可以更方便地查看和对比数据：

```bash
# 查看单个文件
./analyze_perf.py perf_results/perf_tlshub_20240122_143000.json

# 对比多个模式
./analyze_perf.py perf_results/perf_tlshub_*.json \
                  perf_results/perf_openssl_*.json \
                  perf_results/perf_boringssl_*.json
```

分析工具会自动：
- 显示详细的性能数据
- 生成对比表格
- 标识最佳性能的模式

## 故障排查

### 脚本执行失败

如果脚本执行失败，请检查：

1. 是否以 root 权限运行
2. capture 程序是否已编译（在 capture 目录运行 `make`）
3. 配置文件路径是否正确

### 没有生成结果文件

如果没有生成性能数据文件，可能原因：

1. 测试时间太短，没有捕获到连接
2. 程序异常退出
3. 磁盘空间不足

### 性能数据异常

如果性能数据看起来不正常：

1. 确保测试期间有实际的网络流量
2. 检查系统负载是否过高
3. 验证配置文件设置是否正确

## 高级用法

### 自定义流量生成

在测试期间生成特定的流量模式：

```bash
# 启动性能测试
sudo ./perf_test.sh &

# 等待程序启动
sleep 5

# 生成测试流量（示例）
# curl https://example.com
# wget https://example.com

# 等待测试完成
wait
```

### 长时间压力测试

测试长时间运行的稳定性：

```bash
# 运行测试脚本
sudo ./perf_test.sh

# 选择选项 5（自定义测试时长）
# 输入较长的时间，如 3600（1小时）
```

## 相关文档

- [性能指标详细文档](../../docs/PERFORMANCE_METRICS_CN.md)
- [API 参考文档](../docs/API.md)
- [主 README](../../README.md)

## 贡献

欢迎提交新的测试用例和示例！

## 联系方式

如有问题，请提交 Issue：
https://github.com/CapooL/tlshub-ebpf/issues
