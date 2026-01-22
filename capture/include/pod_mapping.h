#ifndef __POD_MAPPING_H__
#define __POD_MAPPING_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_POD_NAME 256
#define MAX_NODE_NAME 256
#define MAX_MAPPINGS 1024

/* Pod-Node 映射条目 */
struct pod_node_mapping {
    char pod_name[MAX_POD_NAME];
    char node_name[MAX_NODE_NAME];
};

/* Pod-Node 映射表 */
struct pod_node_table {
    struct pod_node_mapping mappings[MAX_MAPPINGS];
    int count;
};

/**
 * 初始化 Pod-Node 映射表
 * @param config_file: 配置文件路径
 * @return: 映射表指针，失败返回 NULL
 */
struct pod_node_table* init_pod_node_mapping(const char *config_file);

/**
 * 根据 Pod 名称查找对应的 Node
 * @param table: 映射表
 * @param pod_name: Pod 名称
 * @return: Node 名称，未找到返回 NULL
 */
const char* get_node_by_pod(struct pod_node_table *table, const char *pod_name);

/**
 * 释放映射表资源
 * @param table: 映射表
 */
void free_pod_node_mapping(struct pod_node_table *table);

/**
 * 打印映射表内容（调试用）
 * @param table: 映射表
 */
void print_pod_node_mapping(struct pod_node_table *table);

#endif /* __POD_MAPPING_H__ */
