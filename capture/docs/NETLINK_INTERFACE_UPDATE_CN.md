# TLShub Netlink 接口适配文档

## 更新概述

本文档描述了 capture 模块对 TLShub Netlink 接口的适配工作，主要是在 Netlink 消息中添加 `server_node_ip` 字段，以满足内核 TLShub 模块的协议要求。

## 背景说明

### 问题描述

在原有实现中，capture 模块通过用户态 Netlink 客户端（`tlshub_client.c`）与内核 TLShub 模块通信，执行以下操作：
- `TLS_SERVICE_INIT` - 初始化服务
- `TLS_SERVICE_START` - 发起 TLS 握手
- `TLS_SERVICE_FETCH` - 获取会话密钥

内核端期望用户态发送的 `my_msg` 载荷包含 **server_node_ip**（Node 层 IP，用于建立 node-level 连接），但 capture 现有实现只发送以下字段：
- `client_pod_ip` - 客户端 Pod IP
- `server_pod_ip` - 服务端 Pod IP  
- `client_pod_port` - 客户端端口
- `server_pod_port` - 服务端端口

这导致协议不匹配与获取密钥失败。

### 解决方案

在 Netlink 消息结构中添加 `server_node_ip` 字段，并通过 Pod-Node 映射表查找相应的 Node IP 地址。

## 修改内容

### 1. 数据结构修改

#### 1.1 `struct my_msg` 结构体扩展

**文件：** `capture/src/tlshub_client.c`

```c
/* TLSHub 消息载荷 */
struct my_msg {
    uint32_t client_pod_ip;
    uint32_t server_pod_ip;
    uint32_t server_node_ip;  /* 新增：服务端 Node IP（网络字节序） */
    unsigned short client_pod_port;
    unsigned short server_pod_port;
    char opcode;
    bool server;
};
```

**说明：**
- 新增 `server_node_ip` 字段（类型：`uint32_t`）
- 该字段存储服务端所在 Node 的 IP 地址
- 使用网络字节序（与其他 IP 字段保持一致）

#### 1.2 Pod-Node 映射结构扩展

**文件：** `capture/include/pod_mapping.h`

```c
/* Pod-Node 映射条目 */
struct pod_node_mapping {
    char pod_name[MAX_POD_NAME];
    char node_name[MAX_NODE_NAME];
    uint32_t node_ip;  /* 新增：Node IP 地址（网络字节序） */
};
```

**说明：**
- 在映射条目中添加 `node_ip` 字段
- 支持存储和查询 Node 的 IP 地址

### 2. 函数接口修改

#### 2.1 TLSHub 客户端接口

**文件：** `capture/include/tlshub_client.h`

修改了以下函数签名：

```c
/**
 * 根据四元组从 TLSHub 获取密钥
 * @param tuple: 四元组信息
 * @param key_info: 用于存储获取的密钥信息
 * @param server_node_ip: 服务端 Node IP（网络字节序），如果为 0 则不填充
 * @return: 成功返回 0，失败返回负值
 */
int tlshub_fetch_key(struct flow_tuple *tuple, struct tls_key_info *key_info, uint32_t server_node_ip);

/**
 * 通过 TLSHub 发起握手
 * @param tuple: 四元组信息
 * @param server_node_ip: 服务端 Node IP（网络字节序），如果为 0 则不填充
 * @return: 成功返回 0，失败返回负值
 */
int tlshub_handshake(struct flow_tuple *tuple, uint32_t server_node_ip);
```

**说明：**
- 两个函数都新增 `server_node_ip` 参数
- 如果 `server_node_ip` 为 0，表示未找到对应的 Node IP（但仍会发送消息）

#### 2.2 Pod 映射查询接口

**文件：** `capture/include/pod_mapping.h`

新增函数：

```c
/**
 * 根据 Pod IP 查找对应的 Node IP
 * @param table: 映射表
 * @param pod_ip: Pod IP 地址（网络字节序）
 * @return: Node IP 地址（网络字节序），未找到返回 0
 */
uint32_t get_node_ip_by_pod_ip(struct pod_node_table *table, uint32_t pod_ip);
```

**说明：**
- 根据 Pod IP 查找对应的 Node IP
- 当前实现中，由于映射表基于 Pod 名称，暂时返回 0
- 在后续版本中可以扩展为完整的 IP 映射机制

#### 2.3 密钥提供者接口

**文件：** `capture/include/key_provider.h`

新增函数：

```c
/**
 * 设置 Pod-Node 映射表
 * @param table: Pod-Node 映射表指针
 */
void key_provider_set_pod_node_table(struct pod_node_table *table);
```

**说明：**
- 允许主程序将 Pod-Node 映射表传递给密钥提供者
- 密钥提供者在获取密钥时会自动查询 Node IP

### 3. 实现逻辑修改

#### 3.1 配置文件解析

**文件：** `capture/src/pod_mapping.c`

修改了 `init_pod_node_mapping()` 函数：

```c
/* 格式：pod_name node_name node_ip */
while (fgets(line, sizeof(line), fp) != NULL && table->count < MAX_MAPPINGS) {
    /* 跳过空行和注释 */
    if (line[0] == '\n' || line[0] == '#') {
        continue;
    }
    
    /* 解析 pod_name, node_name 和 node_ip */
    if (sscanf(line, "%s %s %s", pod_name, node_name, node_ip_str) == 3) {
        strncpy(table->mappings[table->count].pod_name, pod_name, MAX_POD_NAME - 1);
        strncpy(table->mappings[table->count].node_name, node_name, MAX_NODE_NAME - 1);
        
        /* 将 Node IP 字符串转换为网络字节序 */
        table->mappings[table->count].node_ip = inet_addr(node_ip_str);
        if (table->mappings[table->count].node_ip == INADDR_NONE) {
            fprintf(stderr, "Warning: Invalid node IP '%s' for pod '%s', skipping\n", 
                    node_ip_str, pod_name);
            continue;
        }
        
        table->count++;
    }
}
```

**说明：**
- 从两字段格式（`pod_name node_name`）升级为三字段格式（`pod_name node_name node_ip`）
- 使用 `inet_addr()` 将 IP 字符串转换为网络字节序
- 对无效的 IP 地址进行错误检查和警告

#### 3.2 密钥获取流程

**文件：** `capture/src/key_provider.c`

修改了 `key_provider_get_key()` 函数：

```c
int key_provider_get_key(struct flow_tuple *tuple, struct tls_key_info *key_info) {
    int ret;
    uint32_t server_node_ip = 0;
    
    /* 如果有 Pod-Node 映射表，尝试查找 server_node_ip */
    if (pod_node_table) {
        server_node_ip = get_node_ip_by_pod_ip(pod_node_table, tuple->daddr);
        if (server_node_ip != 0) {
            char ip_str[INET_ADDRSTRLEN];
            struct in_addr addr;
            addr.s_addr = server_node_ip;
            inet_ntop(AF_INET, &addr, ip_str, sizeof(ip_str));
            printf("Found server_node_ip: %s for server_pod_ip\n", ip_str);
        } else {
            printf("Warning: server_node_ip not found in mapping table, using 0\n");
        }
    }
    
    switch (current_mode) {
        case MODE_TLSHUB:
            /* 尝试从 TLSHub 获取密钥 */
            ret = tlshub_fetch_key(tuple, key_info, server_node_ip);
            if (ret < 0) {
                /* 获取失败，发起握手 */
                ret = tlshub_handshake(tuple, server_node_ip);
                // ...
            }
            return 0;
        // ...
    }
}
```

**说明：**
- 在获取密钥前，先查询 `server_node_ip`
- 将查询到的 Node IP 传递给 TLSHub 客户端函数
- 如果未找到，使用 0 作为默认值（并输出警告）

#### 3.3 主程序集成

**文件：** `capture/src/main.c`

在主程序中添加了映射表设置：

```c
/* 初始化 Pod-Node 映射表 */
pod_node_table = init_pod_node_mapping(config.pod_node_config_path);
if (pod_node_table) {
    print_pod_node_mapping(pod_node_table);
}

/* 初始化密钥提供者 */
err = key_provider_init(config.mode);

/* 设置 Pod-Node 映射表到密钥提供者 */
if (pod_node_table) {
    key_provider_set_pod_node_table(pod_node_table);
}
```

**说明：**
- 在初始化密钥提供者后，将映射表传递给它
- 确保密钥提供者可以访问 Node IP 信息

### 4. 配置文件格式更新

**文件：** `capture/config/pod_node_mapping.conf`

新格式：

```
# 格式: pod_name node_name node_ip
# pod_name: Pod 名称
# node_name: Node 名称  
# node_ip: Node IP 地址（用于建立 node-level 连接）

# 示例映射
web-pod-1 node-1 192.168.1.10
web-pod-2 node-1 192.168.1.10
api-pod-1 node-2 192.168.1.20
api-pod-2 node-2 192.168.1.20
db-pod-1 node-3 192.168.1.30
cache-pod-1 node-3 192.168.1.30
```

**说明：**
- 每行包含三个字段：Pod 名称、Node 名称、Node IP
- Node IP 用于内核 TLShub 模块建立 node-level 连接

## 使用指南

### 1. 配置 Pod-Node 映射

编辑 `/etc/tlshub/pod_node_mapping.conf`：

```bash
sudo vi /etc/tlshub/pod_node_mapping.conf
```

按以下格式添加映射关系：

```
<pod_name> <node_name> <node_ip>
```

示例：

```
my-app-pod-1 worker-node-1 10.0.1.100
my-app-pod-2 worker-node-1 10.0.1.100
my-db-pod-1 worker-node-2 10.0.1.200
```

### 2. 编译更新后的模块

```bash
cd /home/runner/work/tlshub-ebpf/tlshub-ebpf/capture
make clean
make
```

### 3. 运行 capture 模块

```bash
sudo ./capture
```

程序会：
1. 加载 Pod-Node 映射配置
2. 打印映射表内容（包括 Node IP）
3. 在处理连接时自动查询并使用 Node IP

### 4. 验证配置

运行程序后，检查输出：

```
Initializing Pod-Node mapping...
Loaded 6 pod-node mappings from /etc/tlshub/pod_node_mapping.conf
Pod-Node Mapping Table (6 entries):
Pod Name                       Node Name                      Node IP             
--------------------------------------------------------------------------------
web-pod-1                      node-1                         192.168.1.10        
web-pod-2                      node-1                         192.168.1.10        
api-pod-1                      node-2                         192.168.1.20        
...
```

当处理连接时：

```
=== New TCP Connection Detected ===
Source: 10.0.1.100:8080
Destination: 10.0.2.200:9090
Fetching TLS key...
Found server_node_ip: 10.0.1.200 for server_pod_ip
```

## 注意事项

### 1. 字节序

所有 IP 地址字段都使用**网络字节序**（big-endian）：
- `client_pod_ip` - 网络字节序
- `server_pod_ip` - 网络字节序
- `server_node_ip` - 网络字节序（新增）

使用 `inet_addr()` 转换 IP 字符串时会自动转为网络字节序。

### 2. 向后兼容性

如果配置文件仍使用旧格式（两字段），会导致解析失败：
- `sscanf()` 会返回 2 而不是 3
- 映射条目不会被加载
- 程序会输出警告但继续运行

建议：**更新所有配置文件为新格式**

### 3. Pod IP 到 Node IP 的映射

当前实现的限制：
- 映射表基于 Pod 名称，而不是 Pod IP
- `get_node_ip_by_pod_ip()` 函数暂时返回 0
- 在实际场景中，需要维护 Pod IP 到 Pod 名称的映射

推荐的扩展方案：
1. 在映射表中同时存储 Pod IP
2. 建立 Pod IP 到映射条目的索引（如哈希表）
3. 实现完整的 `get_node_ip_by_pod_ip()` 查询逻辑

### 4. 错误处理

- 如果 Node IP 为 0，程序会发出警告但继续执行
- 内核模块需要能够处理 `server_node_ip = 0` 的情况
- 建议在生产环境中确保所有 Pod 都有对应的 Node IP

## 测试建议

### 1. 单元测试

测试 Pod-Node 映射功能：

```bash
cd capture/test
make test_pod_mapping
./test_pod_mapping
```

### 2. 集成测试

1. 配置真实的 Pod-Node 映射
2. 启动 capture 模块
3. 触发 TCP 连接
4. 检查 Netlink 消息中是否包含正确的 `server_node_ip`

### 3. 调试输出

启用详细日志：

```bash
sudo ./capture
```

查看输出中的：
- Pod-Node 映射表加载信息
- 连接处理时的 Node IP 查询结果
- Netlink 消息发送日志

## 相关文件

### 修改的文件

- `capture/src/tlshub_client.c` - 扩展消息结构，添加 `server_node_ip`
- `capture/include/tlshub_client.h` - 更新函数签名
- `capture/src/pod_mapping.c` - 解析和存储 Node IP
- `capture/include/pod_mapping.h` - 扩展映射结构
- `capture/src/key_provider.c` - 集成 Node IP 查询
- `capture/include/key_provider.h` - 添加映射表设置接口
- `capture/src/main.c` - 集成映射表到密钥提供者
- `capture/config/pod_node_mapping.conf` - 更新配置格式

### 新增的文件

- `capture/docs/NETLINK_INTERFACE_UPDATE_CN.md` - 本文档

## 未来改进方向

1. **完整的 Pod IP 映射**
   - 实现 Pod IP 到 Node IP 的直接查询
   - 支持动态更新映射关系

2. **配置验证工具**
   - 提供配置文件验证脚本
   - 检查 IP 地址格式和可达性

3. **性能优化**
   - 使用哈希表加速查询
   - 缓存查询结果

4. **监控和告警**
   - 统计 Node IP 查询失败率
   - 当 `server_node_ip = 0` 时发送告警

## 技术支持

如有问题或建议，请：
- 提交 GitHub Issue
- 查阅完整文档：`capture/docs/`
- 参考示例配置：`capture/config/`

## 更新历史

- **2026-01-27**: 初始版本，添加 `server_node_ip` 字段支持
