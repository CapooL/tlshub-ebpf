#!/bin/bash

# TLShub 性能测试脚本
# 用于测试不同模式下的性能指标

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 配置文件路径
CONFIG_FILE="/etc/tlshub/capture.conf"
BACKUP_CONFIG="${CONFIG_FILE}.backup"

# 测试参数
TEST_DURATION=60  # 测试持续时间（秒）
RESULTS_DIR="./perf_results"

echo "=========================================="
echo "  TLShub 性能测试工具"
echo "=========================================="
echo ""

# 检查是否为 root 用户
if [ "$EUID" -ne 0 ]; then 
    echo -e "${RED}错误: 请使用 root 权限运行此脚本${NC}"
    exit 1
fi

# 检查 capture 程序是否存在
if [ ! -f "./capture" ]; then
    echo -e "${RED}错误: 找不到 capture 程序${NC}"
    echo "请先运行 'make' 编译程序"
    exit 1
fi

# 创建结果目录
mkdir -p "$RESULTS_DIR"

# 备份配置文件
if [ -f "$CONFIG_FILE" ]; then
    cp "$CONFIG_FILE" "$BACKUP_CONFIG"
    echo -e "${GREEN}已备份配置文件到: $BACKUP_CONFIG${NC}"
fi

# 测试函数
run_test() {
    local mode=$1
    local mode_name=$2
    
    echo ""
    echo -e "${YELLOW}========================================${NC}"
    echo -e "${YELLOW}测试模式: $mode_name${NC}"
    echo -e "${YELLOW}========================================${NC}"
    
    # 创建配置文件
    cat > "$CONFIG_FILE" <<EOF
# TLShub Capture Configuration
mode = $mode
pod_node_config = /etc/tlshub/pod_node_mapping.conf
netlink_protocol = 31
log_level = info
verbose = false
EOF
    
    echo "配置文件已更新为 $mode 模式"
    
    # 启动 capture 程序
    echo "启动 capture 程序 (运行 ${TEST_DURATION} 秒)..."
    timeout ${TEST_DURATION}s ./capture "$CONFIG_FILE" &
    CAPTURE_PID=$!
    
    # 等待程序启动
    sleep 2
    
    # 模拟一些流量（这里可以根据实际情况调整）
    echo "正在收集性能数据..."
    
    # 等待测试完成
    wait $CAPTURE_PID 2>/dev/null || true
    
    # 移动结果文件
    if ls perf_metrics_*.json 1> /dev/null 2>&1; then
        mv perf_metrics_*.json "${RESULTS_DIR}/perf_${mode}_$(date +%Y%m%d_%H%M%S).json"
    fi
    if ls perf_metrics_*.csv 1> /dev/null 2>&1; then
        mv perf_metrics_*.csv "${RESULTS_DIR}/perf_${mode}_$(date +%Y%m%d_%H%M%S).csv"
    fi
    
    echo -e "${GREEN}$mode_name 模式测试完成${NC}"
}

# 显示菜单
show_menu() {
    echo ""
    echo "请选择测试模式:"
    echo "  1) TLShub 模式"
    echo "  2) OpenSSL 模式"
    echo "  3) BoringSSL 模式"
    echo "  4) 全部模式（依次测试）"
    echo "  5) 自定义测试时长"
    echo "  6) 退出"
    echo ""
}

# 主循环
while true; do
    show_menu
    read -p "请输入选项 [1-6]: " choice
    
    case $choice in
        1)
            run_test "tlshub" "TLShub"
            ;;
        2)
            run_test "openssl" "OpenSSL"
            ;;
        3)
            run_test "boringssl" "BoringSSL"
            ;;
        4)
            echo ""
            echo "将依次测试所有模式..."
            run_test "tlshub" "TLShub"
            run_test "openssl" "OpenSSL"
            run_test "boringssl" "BoringSSL"
            echo ""
            echo -e "${GREEN}所有测试完成！${NC}"
            ;;
        5)
            read -p "请输入测试时长（秒）: " TEST_DURATION
            echo "测试时长已设置为 ${TEST_DURATION} 秒"
            ;;
        6)
            echo "退出测试工具"
            break
            ;;
        *)
            echo -e "${RED}无效选项，请重新选择${NC}"
            ;;
    esac
done

# 恢复配置文件
if [ -f "$BACKUP_CONFIG" ]; then
    mv "$BACKUP_CONFIG" "$CONFIG_FILE"
    echo -e "${GREEN}配置文件已恢复${NC}"
fi

echo ""
echo "测试结果保存在: $RESULTS_DIR"
echo ""
echo "您可以使用以下命令查看结果:"
echo "  cat ${RESULTS_DIR}/perf_*.json"
echo "  cat ${RESULTS_DIR}/perf_*.csv"
echo ""
echo "感谢使用 TLShub 性能测试工具！"
