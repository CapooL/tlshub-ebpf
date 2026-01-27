#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "pod_mapping.h"

/**
 * 初始化 Pod-Node 映射表
 */
struct pod_node_table* init_pod_node_mapping(const char *config_file) {
    struct pod_node_table *table = NULL;
    FILE *fp = NULL;
    char line[512];
    char pod_name[MAX_POD_NAME];
    char node_name[MAX_NODE_NAME];
    char node_ip_str[64];
    
    /* 分配映射表内存 */
    table = (struct pod_node_table*)malloc(sizeof(struct pod_node_table));
    if (!table) {
        fprintf(stderr, "Failed to allocate memory for pod-node table\n");
        return NULL;
    }
    memset(table, 0, sizeof(struct pod_node_table));
    
    /* 打开配置文件 */
    fp = fopen(config_file, "r");
    if (!fp) {
        fprintf(stderr, "Failed to open pod-node config file: %s\n", config_file);
        free(table);
        return NULL;
    }
    
    /* 读取配置文件内容 */
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
    
    fclose(fp);
    
    printf("Loaded %d pod-node mappings from %s\n", table->count, config_file);
    return table;
}

/**
 * 根据 Pod 名称查找对应的 Node
 */
const char* get_node_by_pod(struct pod_node_table *table, const char *pod_name) {
    int i;
    
    if (!table || !pod_name) {
        return NULL;
    }
    
    for (i = 0; i < table->count; i++) {
        if (strcmp(table->mappings[i].pod_name, pod_name) == 0) {
            return table->mappings[i].node_name;
        }
    }
    
    return NULL;
}

/**
 * 释放映射表资源
 */
void free_pod_node_mapping(struct pod_node_table *table) {
    if (table) {
        free(table);
    }
}

/**
 * 打印映射表内容（调试用）
 */
void print_pod_node_mapping(struct pod_node_table *table) {
    int i;
    struct in_addr addr;
    
    if (!table) {
        printf("Pod-Node mapping table is NULL\n");
        return;
    }
    
    printf("Pod-Node Mapping Table (%d entries):\n", table->count);
    printf("%-30s %-30s %-20s\n", "Pod Name", "Node Name", "Node IP");
    printf("--------------------------------------------------------------------------------\n");
    for (i = 0; i < table->count; i++) {
        addr.s_addr = table->mappings[i].node_ip;
        printf("%-30s %-30s %-20s\n", 
               table->mappings[i].pod_name,
               table->mappings[i].node_name,
               inet_ntoa(addr));
    }
}

/**
 * 根据 Pod IP 查找对应的 Node IP
 */
uint32_t get_node_ip_by_pod_ip(struct pod_node_table *table, uint32_t pod_ip) {
    /* 注意：这个函数假设 Pod 名称可以从 Pod IP 推导得出
     * 在实际场景中，可能需要维护一个 Pod IP 到 Pod 名称的映射表
     * 这里提供一个简单的实现作为示例
     * 
     * 由于当前映射表是基于 Pod 名称的，我们暂时无法直接通过 IP 查找
     * 需要额外的机制来建立 IP 到名称的映射
     * 
     * 暂时返回 0 表示未找到（可以在后续版本中扩展）
     */
    
    /* TODO: 实现 Pod IP 到 Pod 名称的映射机制 */
    /* 目前返回 0 表示无法找到对应的 Node IP */
    return 0;
}
