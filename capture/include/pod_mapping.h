#ifndef __POD_MAPPING_H__
#define __POD_MAPPING_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>

#define MAX_POD_NAME 256
#define MAX_NODE_NAME 256
#define MAX_MAPPINGS 1024

/* Pod-Node 映射条目 */
struct pod_node_mapping {
    char pod_name[MAX_POD_NAME];
    char node_name[MAX_NODE_NAME];
    uint32_t node_ip;  /* Node IP 地址（网络字节序） */
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
 * 根据 Pod IP 查找对应的 Node IP
 * @param table: 映射表
 * @param pod_ip: Pod IP 地址（网络字节序）
 * @return: Node IP 地址（网络字节序），未找到返回 0
 */
uint32_t get_node_ip_by_pod_ip(struct pod_node_table *table, uint32_t pod_ip);

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
