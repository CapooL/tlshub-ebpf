#!/bin/bash
# 示例：监听 HTTPS 流量

# 检查是否为 root
if [[ $EUID -ne 0 ]]; then
   echo "错误: 需要 root 权限"
   exit 1
fi

# 获取默认网络接口
IFACE=$(ip route | grep default | awk '{print $5}' | head -n 1)

if [ -z "$IFACE" ]; then
    echo "错误: 无法确定默认网络接口"
    echo "请手动指定接口，例如: ./tlshub -i eth0 -p 443"
    exit 1
fi

echo "使用网络接口: $IFACE"
echo "监听端口: 443 (HTTPS)"
echo ""

# 运行 TLSHub
cd "$(dirname "$0")"
./tlshub -i "$IFACE" -p 443
