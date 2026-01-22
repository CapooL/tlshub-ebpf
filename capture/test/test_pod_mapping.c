#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/pod_mapping.h"

int main(int argc, char *argv[]) {
    struct pod_node_table *table;
    const char *node;
    const char *config_file = "/etc/tlshub/pod_node_mapping.conf";
    
    if (argc > 1) {
        config_file = argv[1];
    }
    
    printf("=== Pod-Node Mapping Test ===\n\n");
    
    /* 初始化映射表 */
    printf("Initializing pod-node mapping from %s\n", config_file);
    table = init_pod_node_mapping(config_file);
    if (!table) {
        fprintf(stderr, "Failed to initialize pod-node mapping\n");
        return 1;
    }
    
    /* 打印映射表 */
    printf("\n");
    print_pod_node_mapping(table);
    
    /* 测试查询 */
    printf("\n=== Testing Queries ===\n");
    const char *test_pods[] = {"web-pod-1", "api-pod-1", "db-pod-1", "nonexistent-pod"};
    
    for (int i = 0; i < 4; i++) {
        printf("Query: %s -> ", test_pods[i]);
        node = get_node_by_pod(table, test_pods[i]);
        if (node) {
            printf("%s\n", node);
        } else {
            printf("Not found\n");
        }
    }
    
    /* 清理 */
    free_pod_node_mapping(table);
    
    printf("\n=== Test Complete ===\n");
    return 0;
}
