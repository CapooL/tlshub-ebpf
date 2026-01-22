#!/bin/bash
# TLSHub eBPF 测试脚本
# 用于验证 eBPF 模块的基本功能

set -e

# 颜色定义
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "======================================"
echo "TLSHub eBPF 功能测试"
echo "======================================"

# 检查是否为 root 用户
if [[ $EUID -ne 0 ]]; then
   echo -e "${RED}错误: 此脚本必须以 root 权限运行${NC}"
   exit 1
fi

# 检查必要的工具
echo -e "\n${YELLOW}[1/5] 检查系统依赖...${NC}"
for cmd in clang llvm-strip gcc; do
    if ! command -v $cmd &> /dev/null; then
        echo -e "${RED}错误: 找不到 $cmd 命令${NC}"
        exit 1
    fi
done
echo -e "${GREEN}✓ 系统依赖检查通过${NC}"

# 检查内核头文件
echo -e "\n${YELLOW}[2/5] 检查内核头文件...${NC}"
if [ ! -d "/usr/src/linux-headers-$(uname -r)" ] && [ ! -d "/lib/modules/$(uname -r)/build" ]; then
    echo -e "${RED}错误: 找不到内核头文件${NC}"
    echo "请安装: apt-get install linux-headers-\$(uname -r)"
    exit 1
fi
echo -e "${GREEN}✓ 内核头文件检查通过${NC}"

# 检查 libbpf
echo -e "\n${YELLOW}[3/5] 检查 libbpf 库...${NC}"
if ! ldconfig -p | grep -q libbpf; then
    echo -e "${RED}错误: 找不到 libbpf 库${NC}"
    echo "请安装: apt-get install libbpf-dev"
    exit 1
fi
echo -e "${GREEN}✓ libbpf 库检查通过${NC}"

# 编译测试
echo -e "\n${YELLOW}[4/5] 编译 eBPF 程序...${NC}"
make clean > /dev/null 2>&1 || true
if make; then
    echo -e "${GREEN}✓ 编译成功${NC}"
else
    echo -e "${RED}✗ 编译失败${NC}"
    exit 1
fi

# 检查生成的文件
echo -e "\n${YELLOW}[5/5] 验证生成的文件...${NC}"
if [ -f "build/tlshub_kern.o" ]; then
    echo -e "${GREEN}✓ eBPF 内核程序: build/tlshub_kern.o${NC}"
else
    echo -e "${RED}✗ 未找到 eBPF 内核程序${NC}"
    exit 1
fi

if [ -f "bin/tlshub" ]; then
    echo -e "${GREEN}✓ 用户空间程序: bin/tlshub${NC}"
else
    echo -e "${RED}✗ 未找到用户空间程序${NC}"
    exit 1
fi

# 显示使用说明
echo -e "\n${GREEN}======================================"
echo "所有测试通过！"
echo "======================================${NC}"
echo ""
echo "运行示例:"
echo "  1. 监听所有流量:"
echo "     cd bin && sudo ./tlshub -i eth0"
echo ""
echo "  2. 只监听 HTTPS 流量:"
echo "     cd bin && sudo ./tlshub -i eth0 -p 443"
echo ""
echo "  3. 监听多个端口:"
echo "     cd bin && sudo ./tlshub -i eth0 -p 443 -p 8443"
echo ""
echo "注意: 请将 eth0 替换为实际的网络接口名称"
echo "      使用 'ip link show' 查看可用的网络接口"
