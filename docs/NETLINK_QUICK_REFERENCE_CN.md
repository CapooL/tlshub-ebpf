# TLShub Netlink 接口适配 - 快速参考

## 一句话总结

在 Netlink 消息中添加了 `server_node_ip` 字段，需要更新配置文件格式为三字段。

## 快速开始

### 1. 更新配置文件

编辑 `/etc/tlshub/pod_node_mapping.conf`:

```bash
# 旧格式（不再支持）
# web-pod-1 node-1

# 新格式（必须使用）
web-pod-1 node-1 192.168.1.10
```

### 2. 配置示例

```conf
# 格式: pod_name node_name node_ip
web-pod-1 node-1 192.168.1.10
web-pod-2 node-1 192.168.1.10
api-pod-1 node-2 192.168.1.20
api-pod-2 node-2 192.168.1.20
db-pod-1 node-3 192.168.1.30
```

### 3. 重新编译运行

```bash
cd capture/
make clean && make
sudo ./capture
```

## 检查清单

- [ ] 更新所有 `pod_node_mapping.conf` 文件为三字段格式
- [ ] 确保 Node IP 地址正确且可达
- [ ] 重新编译 capture 模块
- [ ] 测试连接是否正常工作
- [ ] 检查日志确认 Node IP 查询成功

## 常见问题

### Q: 配置文件格式错误怎么办？

**A:** 确保每行包含三个字段，用空格分隔：
```
pod_name node_name node_ip
```

### Q: 看到 "server_node_ip not found" 警告？

**A:** 这是正常的，当前实现基于 Pod 名称查询。程序会使用 0 作为默认值继续运行。

### Q: 需要重启内核模块吗？

**A:** 不需要，只需重启用户态 capture 程序即可。

## 验证方法

运行 capture 后检查输出：

```
✓ Loaded 6 pod-node mappings from /etc/tlshub/pod_node_mapping.conf
✓ Pod-Node Mapping Table (6 entries):
  Pod Name          Node Name         Node IP
  web-pod-1         node-1            192.168.1.10
  ...
✓ Found server_node_ip: 192.168.1.20 for server_pod_ip
✓ TLSHub handshake completed successfully
```

## 详细文档

- **技术细节**: [capture/docs/NETLINK_INTERFACE_UPDATE_CN.md](../capture/docs/NETLINK_INTERFACE_UPDATE_CN.md)
- **变更概述**: [docs/NETLINK_ADAPTATION_SUMMARY_CN.md](NETLINK_ADAPTATION_SUMMARY_CN.md)

## 需要帮助？

1. 查看详细日志输出
2. 检查配置文件格式
3. 验证 IP 地址有效性
4. 提交 GitHub Issue

---

**重要**: 本次更新不向后兼容旧格式配置文件，请务必更新配置！
