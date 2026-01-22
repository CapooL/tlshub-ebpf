#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
TLShub 性能数据分析工具

用于分析和可视化 TLShub 性能测试结果
"""

import json
import sys
import os
from typing import List, Dict, Any

def load_json_file(filename: str) -> Dict[str, Any]:
    """加载 JSON 性能数据文件"""
    try:
        with open(filename, 'r') as f:
            return json.load(f)
    except Exception as e:
        print(f"错误: 无法加载文件 {filename}: {e}")
        return None

def print_summary(data: Dict[str, Any], mode_name: str = ""):
    """打印性能数据摘要"""
    if not data or 'summary' not in data:
        print("错误: 无效的数据格式")
        return
    
    summary = data['summary']
    
    print(f"\n{'='*60}")
    if mode_name:
        print(f"  {mode_name} 模式性能数据")
    else:
        print(f"  性能数据摘要")
    print(f"{'='*60}")
    print(f"总连接数:           {summary.get('total_connections', 0)}")
    print(f"成功连接:           {summary.get('successful_connections', 0)}")
    print(f"失败连接:           {summary.get('failed_connections', 0)}")
    print(f"\n【连接延迟】")
    print(f"  平均:             {summary.get('avg_connection_latency_ms', 0):.3f} ms")
    print(f"  最小:             {summary.get('min_connection_latency_ms', 0):.3f} ms")
    print(f"  最大:             {summary.get('max_connection_latency_ms', 0):.3f} ms")
    print(f"\n【密钥协商】")
    print(f"  平均:             {summary.get('avg_key_negotiation_ms', 0):.3f} ms")
    print(f"  最小:             {summary.get('min_key_negotiation_ms', 0):.3f} ms")
    print(f"  最大:             {summary.get('max_key_negotiation_ms', 0):.3f} ms")
    print(f"\n【吞吐量】")
    print(f"  平均:             {summary.get('avg_throughput_mbps', 0):.2f} Mbps")
    print(f"  峰值:             {summary.get('max_throughput_mbps', 0):.2f} Mbps")
    print(f"  总传输:           {summary.get('total_bytes_transferred', 0) / (1024*1024):.2f} MB")
    print(f"\n【系统资源】")
    print(f"  CPU 使用率:       {summary.get('cpu_usage_percent', 0):.2f}%")
    print(f"  内存使用:         {summary.get('memory_usage_kb', 0) / 1024:.2f} MB")
    print(f"{'='*60}\n")

def compare_modes(files: List[str], mode_names: List[str]):
    """对比多个模式的性能"""
    datasets = []
    valid_modes = []
    
    # 加载所有数据
    for i, filename in enumerate(files):
        data = load_json_file(filename)
        if data:
            datasets.append(data)
            valid_modes.append(mode_names[i] if i < len(mode_names) else f"模式{i+1}")
    
    if len(datasets) == 0:
        print("错误: 没有有效的数据文件")
        return
    
    print(f"\n{'='*80}")
    print(f"  性能对比分析")
    print(f"{'='*80}\n")
    
    # 对比表格
    print(f"{'指标':<30} ", end="")
    for mode in valid_modes:
        print(f"{mode:>15} ", end="")
    print()
    print("-" * 80)
    
    # 连接延迟
    print(f"{'平均连接延迟 (ms)':<30} ", end="")
    for data in datasets:
        val = data['summary'].get('avg_connection_latency_ms', 0)
        print(f"{val:>15.3f} ", end="")
    print()
    
    # 密钥协商
    print(f"{'平均密钥协商时间 (ms)':<30} ", end="")
    for data in datasets:
        val = data['summary'].get('avg_key_negotiation_ms', 0)
        print(f"{val:>15.3f} ", end="")
    print()
    
    # 吞吐量
    print(f"{'平均吞吐量 (Mbps)':<30} ", end="")
    for data in datasets:
        val = data['summary'].get('avg_throughput_mbps', 0)
        print(f"{val:>15.2f} ", end="")
    print()
    
    # CPU
    print(f"{'CPU 使用率 (%)':<30} ", end="")
    for data in datasets:
        val = data['summary'].get('cpu_usage_percent', 0)
        print(f"{val:>15.2f} ", end="")
    print()
    
    # 内存
    print(f"{'内存使用 (MB)':<30} ", end="")
    for data in datasets:
        val = data['summary'].get('memory_usage_kb', 0) / 1024
        print(f"{val:>15.2f} ", end="")
    print()
    
    print("-" * 80)
    
    # 找出最佳性能
    print("\n【性能排名】")
    
    # 连接延迟（越小越好）
    latencies = [(data['summary'].get('avg_connection_latency_ms', float('inf')), mode) 
                 for data, mode in zip(datasets, valid_modes)]
    latencies.sort()
    print(f"连接延迟最佳: {latencies[0][1]} ({latencies[0][0]:.3f} ms)")
    
    # 密钥协商（越小越好）
    negotiations = [(data['summary'].get('avg_key_negotiation_ms', float('inf')), mode) 
                    for data, mode in zip(datasets, valid_modes)]
    negotiations.sort()
    print(f"密钥协商最佳: {negotiations[0][1]} ({negotiations[0][0]:.3f} ms)")
    
    # 吞吐量（越大越好）
    throughputs = [(data['summary'].get('avg_throughput_mbps', 0), mode) 
                   for data, mode in zip(datasets, valid_modes)]
    throughputs.sort(reverse=True)
    print(f"吞吐量最佳:   {throughputs[0][1]} ({throughputs[0][0]:.2f} Mbps)")
    
    # CPU（越小越好）
    cpus = [(data['summary'].get('cpu_usage_percent', float('inf')), mode) 
            for data, mode in zip(datasets, valid_modes)]
    cpus.sort()
    print(f"CPU 效率最佳: {cpus[0][1]} ({cpus[0][0]:.2f}%)")
    
    print(f"{'='*80}\n")

def analyze_connections(data: Dict[str, Any]):
    """分析连接详细数据"""
    if not data or 'connections' not in data:
        print("错误: 没有连接数据")
        return
    
    connections = data['connections']
    if len(connections) == 0:
        print("没有连接数据可分析")
        return
    
    print(f"\n连接详细数据分析 (共 {len(connections)} 个连接)\n")
    print(f"{'连接ID':<20} {'延迟(ms)':>10} {'协商(ms)':>10} {'吞吐量(Mbps)':>15}")
    print("-" * 65)
    
    for conn in connections[:10]:  # 只显示前10个
        print(f"{conn.get('connection_id', 0):<20} "
              f"{conn.get('connection_latency_ms', 0):>10.3f} "
              f"{conn.get('key_negotiation_ms', 0):>10.3f} "
              f"{conn.get('throughput_mbps', 0):>15.2f}")
    
    if len(connections) > 10:
        print(f"... 还有 {len(connections) - 10} 个连接")
    print()

def main():
    if len(sys.argv) < 2:
        print("TLShub 性能数据分析工具")
        print()
        print("用法:")
        print(f"  {sys.argv[0]} <json文件>                    # 查看单个文件")
        print(f"  {sys.argv[0]} <文件1> <文件2> [文件3...]    # 对比多个文件")
        print()
        print("示例:")
        print(f"  {sys.argv[0]} perf_metrics_123456.json")
        print(f"  {sys.argv[0]} perf_tlshub.json perf_openssl.json perf_boringssl.json")
        sys.exit(1)
    
    files = sys.argv[1:]
    
    # 检查文件是否存在
    for f in files:
        if not os.path.exists(f):
            print(f"错误: 文件不存在: {f}")
            sys.exit(1)
    
    if len(files) == 1:
        # 单个文件分析
        data = load_json_file(files[0])
        if data:
            # 尝试从文件名推断模式
            mode_name = ""
            if "tlshub" in files[0].lower():
                mode_name = "TLShub"
            elif "openssl" in files[0].lower():
                mode_name = "OpenSSL"
            elif "boringssl" in files[0].lower():
                mode_name = "BoringSSL"
            
            print_summary(data, mode_name)
            analyze_connections(data)
    else:
        # 多文件对比
        mode_names = []
        for f in files:
            if "tlshub" in f.lower():
                mode_names.append("TLShub")
            elif "openssl" in f.lower():
                mode_names.append("OpenSSL")
            elif "boringssl" in f.lower():
                mode_names.append("BoringSSL")
            else:
                mode_names.append(os.path.basename(f))
        
        compare_modes(files, mode_names)

if __name__ == '__main__':
    main()
