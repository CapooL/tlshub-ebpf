# TLShub Netlink 接口适配说明

## 变更概述

本次更新对 capture 模块进行了重要修改，在与内核 TLShub 模块的 Netlink 通信中添加了 `server_node_ip` 字段，以支持 node-level 连接建立。

## 背景

内核 TLShub 模块期望用户态发送的 Netlink 消息中包含 `server_node_ip`（服务端 Node 的 IP 地址），用于建立 node-level 的 TLS 连接。原有实现只发送了 Pod 级别的 IP 和端口信息，导致协议不匹配。

## 主要变更

### 1. Netlink 消息结构扩展

在 `struct my_msg` 中新增 `server_node_ip` 字段：

```c
struct my_msg {
    uint32_t client_pod_ip;
    uint32_t server_pod_ip;
    uint32_t server_node_ip;  // 新增字段
    unsigned short client_pod_port;
    unsigned short server_pod_port;
    char opcode;
    bool server;
};
```

### 2. Pod-Node 映射增强

扩展 Pod-Node 映射表以存储 Node IP：

```c
struct pod_node_mapping {
    char pod_name[MAX_POD_NAME];
    char node_name[MAX_NODE_NAME];
    uint32_t node_ip;  // 新增字段
};
```

### 3. 配置文件格式更新

配置文件格式从两字段升级为三字段：

**旧格式：**
```
pod_name node_name
```

**新格式：**
```
pod_name node_name node_ip
```

**示例：**
```
web-pod-1 node-1 192.168.1.10
api-pod-1 node-2 192.168.1.20
```

### 4. API 函数签名更新

TLSHub 客户端函数增加 `server_node_ip` 参数：

```c
// 握手函数
int tlshub_handshake(struct flow_tuple *tuple, uint32_t server_node_ip);

// 获取密钥函数
int tlshub_fetch_key(struct flow_tuple *tuple, 
                     struct tls_key_info *key_info, 
                     uint32_t server_node_ip);
```

## 使用方法

### 1. 更新配置文件

编辑 `/etc/tlshub/pod_node_mapping.conf`，按新格式添加 Node IP：

```bash
sudo vi /etc/tlshub/pod_node_mapping.conf
```

示例配置：
```
# 格式: pod_name node_name node_ip
my-app-pod-1 worker-node-1 10.0.1.100
my-app-pod-2 worker-node-1 10.0.1.100
my-db-pod-1 worker-node-2 10.0.1.200
```

### 2. 重新编译

```bash
cd capture/
make clean
make
```

### 3. 运行

```bash
sudo ./capture
```

程序会自动加载 Pod-Node-IP 映射，并在与内核通信时包含 Node IP。

## 技术细节

### 字节序

所有 IP 地址字段使用网络字节序（big-endian）：
- 配置文件中的 IP 字符串通过 `inet_addr()` 转换
- 内核接收后使用 `ntohl()` 转换为主机序

### 查询流程

1. 捕获到 TCP 连接事件
2. 从 Pod-Node 映射表查询 `server_pod_ip` 对应的 `server_node_ip`
3. 将 Node IP 填充到 Netlink 消息
4. 发送给内核 TLShub 模块

### 容错机制

- 如果未找到 Node IP，使用 0 作为默认值
- 程序会输出警告但继续运行
- 内核模块需要能够处理 `server_node_ip = 0` 的情况

## 兼容性说明

### 向后兼容性

- 旧格式配置文件**无法**正常工作
- 必须更新为新格式（三字段）
- 建议：在部署前更新所有配置文件

### 内核模块要求

- 内核 TLShub 模块必须支持接收 `server_node_ip` 字段
- 确保内核模块版本与用户态版本匹配

## 文件修改清单

### 修改的文件

- `capture/src/tlshub_client.c` - Netlink 消息结构和发送逻辑
- `capture/include/tlshub_client.h` - 函数接口声明
- `capture/src/pod_mapping.c` - 配置解析和查询实现
- `capture/include/pod_mapping.h` - 映射结构和函数声明
- `capture/src/key_provider.c` - Node IP 查询集成
- `capture/include/key_provider.h` - 接口声明
- `capture/src/main.c` - 映射表初始化和传递
- `capture/config/pod_node_mapping.conf` - 配置文件格式

### 新增的文件

- `capture/docs/NETLINK_INTERFACE_UPDATE_CN.md` - 详细技术文档
- `docs/NETLINK_ADAPTATION_SUMMARY_CN.md` - 本文档（概要说明）

## 详细文档

完整的技术文档请参阅：
- [capture/docs/NETLINK_INTERFACE_UPDATE_CN.md](../capture/docs/NETLINK_INTERFACE_UPDATE_CN.md)

该文档包含：
- 详细的代码修改说明
- 数据结构定义
- 函数接口文档
- 配置示例
- 测试建议
- 故障排查指南

## 测试建议

1. **配置验证**
   - 检查配置文件格式
   - 验证 IP 地址有效性

2. **功能测试**
   - 启动 capture 模块
   - 触发 TCP 连接
   - 检查 Node IP 查询日志

3. **集成测试**
   - 与内核 TLShub 模块联调
   - 验证握手和密钥获取流程

## 已知限制

1. **Pod IP 映射**
   - 当前映射表基于 Pod 名称而非 Pod IP
   - `get_node_ip_by_pod_ip()` 暂时返回 0
   - 建议未来版本支持直接的 IP 查询

2. **动态更新**
   - 配置文件在程序启动时加载
   - 修改配置需要重启程序

## 故障排查

### 问题：配置文件加载失败

**症状：** `Loaded 0 pod-node mappings`

**解决方案：**
1. 检查配置文件路径
2. 验证文件格式（三字段）
3. 检查文件权限

### 问题：Node IP 查询返回 0

**症状：** `Warning: server_node_ip not found in mapping table`

**原因：**
- Pod IP 到 Node IP 的查询未实现
- 当前需要通过 Pod 名称查询

**临时方案：**
- 确保配置文件完整
- 程序会使用 0 作为默认值继续运行

### 问题：编译错误

**症状：** 缺少头文件或链接错误

**解决方案：**
1. 安装必要的依赖库
2. 检查 libbpf、libssl 是否正确安装
3. 参考 README 安装系统依赖

## 相关 Issue

- Issue: "capture：适配 TLShub Netlink 接口"

## 贡献者

- 实现：GitHub Copilot
- 审核：待定

## 更新历史

- **2026-01-27**: 初始实现，添加 `server_node_ip` 支持
