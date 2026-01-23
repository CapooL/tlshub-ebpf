/**
 * 性能指标使用示例
 * 
 * 本示例展示如何在您的程序中使用性能指标模块
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "performance_metrics.h"

int main() {
    struct perf_metrics_ctx *perf_ctx;
    int conn_index;
    
    printf("TLShub 性能指标使用示例\n");
    printf("========================\n\n");
    
    /* 1. 初始化性能指标模块 */
    perf_ctx = perf_metrics_init(100);  /* 最多跟踪 100 个连接 */
    if (!perf_ctx) {
        fprintf(stderr, "Failed to initialize performance metrics\n");
        return 1;
    }
    
    printf("性能指标模块已初始化\n\n");
    
    /* 模拟连接处理 */
    for (int i = 0; i < 5; i++) {
        printf("--- 处理连接 #%d ---\n", i + 1);
        
        /* 2. 记录连接开始 */
        conn_index = perf_metrics_connection_start(perf_ctx, 10000 + i);
        if (conn_index < 0) {
            continue;
        }
        
        /* 3. 测量连接建立延迟 */
        perf_metrics_connection_latency_start(perf_ctx, conn_index);
        usleep(10000 + (i * 1000));  /* 模拟连接建立（10-14ms） */
        perf_metrics_connection_latency_end(perf_ctx, conn_index);
        
        /* 4. 测量密钥协商时间 */
        perf_metrics_key_negotiation_start(perf_ctx, conn_index);
        usleep(5000 + (i * 500));   /* 模拟密钥协商（5-7ms） */
        perf_metrics_key_negotiation_end(perf_ctx, conn_index);
        
        /* 5. 记录数据传输 */
        perf_metrics_record_data_transfer(perf_ctx, conn_index, 
                                         1024 * 1024 * (i + 1),  /* 发送字节 */
                                         2048 * 1024 * (i + 1)); /* 接收字节 */
        
        /* 6. 记录连接结束 */
        perf_metrics_connection_end(perf_ctx, conn_index, 1);  /* 1 = 成功 */
        
        printf("连接 #%d 处理完成\n\n", i + 1);
    }
    
    /* 7. 更新系统性能指标 */
    printf("更新系统性能指标...\n");
    perf_metrics_update_system(perf_ctx);
    
    /* 8. 打印性能报告 */
    perf_metrics_print_report(perf_ctx);
    
    /* 9. 导出数据 */
    printf("\n导出性能数据...\n");
    perf_metrics_export_json(perf_ctx, "example_metrics.json");
    perf_metrics_export_csv(perf_ctx, "example_metrics.csv");
    
    /* 10. 清理 */
    perf_metrics_cleanup(perf_ctx);
    
    printf("\n示例程序执行完成！\n");
    printf("请查看生成的文件:\n");
    printf("  - example_metrics.json\n");
    printf("  - example_metrics.csv\n");
    
    return 0;
}
