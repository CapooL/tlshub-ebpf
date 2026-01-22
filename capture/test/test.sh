#!/bin/bash

# TLShub 流量捕获模块测试脚本
# =============================

set -e

echo "======================================"
echo "TLShub 流量捕获模块测试"
echo "======================================"
echo ""

# 检查是否以 root 权限运行
if [ "$EUID" -ne 0 ]; then 
    echo "错误: 请以 root 权限运行此脚本"
    exit 1
fi

# 检查依赖
echo "1. 检查依赖..."
command -v clang >/dev/null 2>&1 || { echo "错误: 需要安装 clang"; exit 1; }
command -v gcc >/dev/null 2>&1 || { echo "错误: 需要安装 gcc"; exit 1; }
echo "   ✓ 依赖检查通过"
echo ""

# 编译程序
echo "2. 编译流量捕获模块..."
cd ../
make clean
make
if [ $? -eq 0 ]; then
    echo "   ✓ 编译成功"
else
    echo "   ✗ 编译失败"
    exit 1
fi
echo ""

# 测试 Pod-Node 映射
echo "3. 测试 Pod-Node 映射..."
# 创建临时目录和文件
TEMP_DIR=$(mktemp -d)
TEMP_CONF="$TEMP_DIR/test_pod_node.conf"
TEMP_BIN="$TEMP_DIR/test_pod_mapping"

cat > "$TEMP_CONF" <<EOF
test-pod-1 test-node-1
test-pod-2 test-node-2
test-pod-3 test-node-3
EOF

gcc -o "$TEMP_BIN" test_pod_mapping.c ../src/pod_mapping.c -I../include
"$TEMP_BIN" "$TEMP_CONF"
if [ $? -eq 0 ]; then
    echo "   ✓ Pod-Node 映射测试通过"
else
    echo "   ✗ Pod-Node 映射测试失败"
fi

# 清理临时文件
rm -rf "$TEMP_DIR"
echo ""

# 运行捕获模块（演示模式）
echo "4. 运行流量捕获模块（演示模式，5秒）..."
echo "   注意: 如果没有 eBPF 环境，此步骤可能失败"
timeout 5 ../capture ../config/capture.conf 2>/dev/null || true
echo "   ✓ 演示完成"
echo ""

echo "======================================"
echo "测试完成"
echo "======================================"
