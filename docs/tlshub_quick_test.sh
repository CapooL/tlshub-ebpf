#!/bin/bash
# tlshub_quick_test.sh - TLShub 快速性能测试脚本
#
# 这是一个简单的测试脚本，用于快速验证 TLShub 握手性能
# 完整的性能测试建议使用专门的 tlshub-perf 工具
#
# 使用方法:
#   sudo ./tlshub_quick_test.sh <客户端IP> <服务端IP> [测试次数]
#
# 示例:
#   sudo ./tlshub_quick_test.sh 10.0.1.100 10.0.2.100 100

set -e

# 检查参数
if [ $# -lt 2 ]; then
    echo "用法: $0 <客户端IP> <服务端IP> [测试次数]"
    echo ""
    echo "示例:"
    echo "  sudo $0 10.0.1.100 10.0.2.100 100"
    exit 1
fi

CLIENT_IP="$1"
SERVER_IP="$2"
N_TESTS="${3:-100}"  # 默认 100 次测试

# 检查是否有 root 权限
if [ "$EUID" -ne 0 ]; then
    echo "错误: 需要 root 权限运行此脚本"
    echo "请使用: sudo $0 $@"
    exit 1
fi

# 检查 tlshub_handshake_test 程序是否存在
TLSHUB_TEST="../tlshub-api/tlshub_handshake_test"
if [ ! -f "$TLSHUB_TEST" ]; then
    echo "错误: 找不到 tlshub_handshake_test 程序"
    echo "请先编译: cd ../tlshub-api && make"
    exit 1
fi

echo "========================================"
echo "TLShub 快速性能测试"
echo "========================================"
echo "客户端 IP: $CLIENT_IP"
echo "服务端 IP: $SERVER_IP"
echo "测试次数:  $N_TESTS"
echo "========================================"
echo ""

# 初始化统计变量
success=0
failed=0
total_time=0
min_time=999999999
max_time=0

# 创建临时文件存储每次测试的延迟
LATENCY_FILE=$(mktemp)

echo "开始测试..."
for i in $(seq 1 $N_TESTS); do
    # 生成随机端口
    client_port=$((10000 + RANDOM % 50000))
    server_port=443
    
    # 记录开始时间（纳秒）
    start=$(date +%s%N)
    
    # 调用测试程序
    if $TLSHUB_TEST $CLIENT_IP $SERVER_IP > /dev/null 2>&1; then
        # 记录结束时间
        end=$(date +%s%N)
        
        # 计算延迟（毫秒）
        elapsed=$(( (end - start) / 1000000 ))
        
        # 更新统计
        ((success++))
        total_time=$((total_time + elapsed))
        echo "$elapsed" >> "$LATENCY_FILE"
        
        # 更新最小/最大延迟
        if [ $elapsed -lt $min_time ]; then
            min_time=$elapsed
        fi
        if [ $elapsed -gt $max_time ]; then
            max_time=$elapsed
        fi
        
        # 输出进度
        printf "\r[%d/%d] 成功: %d, 失败: %d, 最后延迟: %d ms    " \
            $i $N_TESTS $success $failed $elapsed
    else
        ((failed++))
        printf "\r[%d/%d] 成功: %d, 失败: %d, 最后测试: 失败    " \
            $i $N_TESTS $success $failed
    fi
done

echo ""
echo ""
echo "========================================"
echo "测试完成"
echo "========================================"
echo "总测试次数: $N_TESTS"
echo "成功次数:   $success"
echo "失败次数:   $failed"

if [ $success -gt 0 ]; then
    success_rate=$(echo "scale=2; $success * 100 / $N_TESTS" | bc)
    avg_time=$(echo "scale=3; $total_time / $success" | bc)
    
    echo "成功率:     ${success_rate}%"
    echo ""
    echo "延迟统计 (毫秒):"
    echo "  最小延迟: ${min_time}"
    echo "  平均延迟: ${avg_time}"
    echo "  最大延迟: ${max_time}"
    
    # 计算 P95 和 P99
    if command -v sort >/dev/null 2>&1; then
        p95_index=$(echo "scale=0; $success * 0.95 / 1" | bc)
        p99_index=$(echo "scale=0; $success * 0.99 / 1" | bc)
        
        p95=$(sort -n "$LATENCY_FILE" | sed -n "${p95_index}p")
        p99=$(sort -n "$LATENCY_FILE" | sed -n "${p99_index}p")
        
        echo "  P95 延迟: ${p95:-N/A}"
        echo "  P99 延迟: ${p99:-N/A}"
    fi
    
    # 计算吞吐量
    if [ $N_TESTS -ge 10 ]; then
        # 估算测试总时长（秒）
        total_duration=$(echo "scale=3; $total_time / 1000" | bc)
        if [ $(echo "$total_duration > 0" | bc) -eq 1 ]; then
            throughput=$(echo "scale=2; $success / $total_duration" | bc)
            echo ""
            echo "吞吐量:     ${throughput} 握手/秒"
        fi
    fi
else
    echo "成功率:     0%"
    echo ""
    echo "错误: 所有测试都失败了"
    echo "请检查:"
    echo "  1. TLShub 内核模块是否已加载 (lsmod | grep tlshub)"
    echo "  2. capture 程序是否正在运行"
    echo "  3. IP 地址和端口是否正确"
fi

echo "========================================"

# 清理临时文件
rm -f "$LATENCY_FILE"

# 返回状态码
if [ $failed -eq 0 ] && [ $success -gt 0 ]; then
    exit 0
else
    exit 1
fi
