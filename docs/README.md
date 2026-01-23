# TLShub 文档目录

本目录包含 TLShub 项目的完整中文文档。

## 文档列表

### API 文档

- **[TLSHUB_API_CN.md](TLSHUB_API_CN.md)** - TLShub 用户态 API 完整文档
  - API 函数参考
  - 数据结构说明
  - 使用示例
  - 错误处理指南
  - 线程安全说明

### 集成指南

- **[INTEGRATION_GUIDE_CN.md](INTEGRATION_GUIDE_CN.md)** - TLShub 集成指南
  - 在现有项目中集成 TLShub
  - 与 eBPF 程序集成
  - 性能考虑
  - 最佳实践

### 性能文档

- **[PERFORMANCE_METRICS_CN.md](PERFORMANCE_METRICS_CN.md)** - 性能指标监测文档
  - 性能指标收集
  - 数据导出（JSON/CSV）
  - 性能测试最佳实践
  - 故障排查

- **[TLS_PERF_ANALYSIS_CN.md](TLS_PERF_ANALYSIS_CN.md)** - TLS-Perf 工具分析与测试方案 ⭐ 新增
  - TLS-Perf 工具详细分析
  - TLShub 架构对比
  - 兼容性评估
  - 改造方案与实施步骤
  - 完整测试方案
  - 推荐使用 tlshub-perf 工具

### 工具脚本

- **[tlshub_quick_test.sh](tlshub_quick_test.sh)** - 快速性能测试脚本 ⭐ 新增
  - 简单易用的握手性能测试
  - 适合快速验证和初步测试
  - 提供基本的性能统计

## 快速导航

### 我想要...

#### 开始使用 TLShub
→ 查看主 [README.md](../README.md) 获取快速开始指南  
→ 然后阅读 [TLSHUB_API_CN.md](TLSHUB_API_CN.md) 了解 API

#### 在项目中集成 TLShub
→ 阅读 [INTEGRATION_GUIDE_CN.md](INTEGRATION_GUIDE_CN.md)

#### 测试 TLShub 性能
→ 首先阅读 [TLS_PERF_ANALYSIS_CN.md](TLS_PERF_ANALYSIS_CN.md) 了解测试工具选择  
→ 使用 [tlshub_quick_test.sh](tlshub_quick_test.sh) 进行快速测试  
→ 参考 [PERFORMANCE_METRICS_CN.md](PERFORMANCE_METRICS_CN.md) 进行完整性能监测

#### 了解性能指标
→ 阅读 [PERFORMANCE_METRICS_CN.md](PERFORMANCE_METRICS_CN.md)

#### 开发性能测试工具
→ 阅读 [TLS_PERF_ANALYSIS_CN.md](TLS_PERF_ANALYSIS_CN.md) 的第 5-7 节

## 文档更新历史

### 2026-01-23
- ✨ 新增 [TLS_PERF_ANALYSIS_CN.md](TLS_PERF_ANALYSIS_CN.md) - TLS-Perf 工具分析文档
- ✨ 新增 [tlshub_quick_test.sh](tlshub_quick_test.sh) - 快速测试脚本
- 📝 更新主 README 添加性能测试文档链接

### 2024-01
- 📝 从 hypertls 重命名为 tlshub
- 📝 更新所有 API 文档

## 其他资源

### 外部文档

- **Capture 模块文档**: [../capture/docs/](../capture/docs/)
  - [README.md](../capture/docs/README.md) - Capture 模块使用说明
  - [ARCHITECTURE.md](../capture/docs/ARCHITECTURE.md) - 架构设计
  - [API.md](../capture/docs/API.md) - API 参考

- **TLShub API 示例**: [../tlshub-api/](../tlshub-api/)
  - [README.md](../tlshub-api/README.md) - API 快速开始
  - 示例代码和编译说明

### 相关项目

- [TLSHub 内核模块](https://github.com/lin594/tlshub) - Layer 4 友好的内核级 mTLS 握手模块
- [TLS-Perf 工具](https://github.com/tempesta-tech/tls-perf) - TLS 握手基准测试工具

## 反馈与贡献

如有问题或建议，欢迎：
- 提交 Issue: https://github.com/CapooL/tlshub-ebpf/issues
- 提交 Pull Request
- 完善文档

## 许可证

本项目采用与 TLShub 相同的许可证。
