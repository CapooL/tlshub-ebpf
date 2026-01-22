#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    /* 格式：pod_name node_name */
    while (fgets(line, sizeof(line), fp) != NULL && table->count < MAX_MAPPINGS) {
        /* 跳过空行和注释 */
        if (line[0] == '\n' || line[0] == '#') {
            continue;
        }
        
        /* 解析 pod_name 和 node_name */
        if (sscanf(line, "%s %s", pod_name, node_name) == 2) {
            strncpy(table->mappings[table->count].pod_name, pod_name, MAX_POD_NAME - 1);
            strncpy(table->mappings[table->count].node_name, node_name, MAX_NODE_NAME - 1);
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
    
    if (!table) {
        printf("Pod-Node mapping table is NULL\n");
        return;
    }
    
    printf("Pod-Node Mapping Table (%d entries):\n", table->count);
    printf("%-30s %-30s\n", "Pod Name", "Node Name");
    printf("------------------------------------------------------------\n");
    for (i = 0; i < table->count; i++) {
        printf("%-30s %-30s\n", 
               table->mappings[i].pod_name,
               table->mappings[i].node_name);
    }
}
