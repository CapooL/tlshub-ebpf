#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
/* Try to include libbpf_version.h for version detection */
#ifdef __has_include
  #if __has_include(<bpf/libbpf_version.h>)
    #include <bpf/libbpf_version.h>
  #endif
#endif
#include "capture.h"
#include "key_provider.h"
#include "ktls_config.h"
#include "pod_mapping.h"
#include "performance_metrics.h"

#define DEFAULT_CONFIG_FILE "/etc/tlshub/capture.conf"
#define DEFAULT_POD_NODE_CONFIG "/etc/tlshub/pod_node_mapping.conf"
#define DEFAULT_MAX_CONNECTIONS 1000
#define PERF_UPDATE_INTERVAL_SEC 5

static volatile int keep_running = 1;
static struct bpf_object *obj = NULL;
static struct bpf_link *links[10] = {NULL};
static int link_count = 0;
static struct pod_node_table *pod_node_table = NULL;
static struct perf_metrics_ctx *perf_ctx = NULL;

/* TCP 连接事件 */
struct tcp_connect_event {
    __u32 saddr;
    __u32 daddr;
    __u16 sport;
    __u16 dport;
    __u32 pid;
    __u64 timestamp;
};

/**
 * 信号处理函数
 */
static void sig_handler(int sig) {
    keep_running = 0;
    printf("\nReceived signal %d, shutting down...\n", sig);
}

/**
 * 处理 TCP 连接事件
 */
static void handle_tcp_event(void *ctx, int cpu, void *data, __u32 data_sz) {
    struct tcp_connect_event *event = (struct tcp_connect_event *)data;
    struct flow_tuple tuple;
    struct tls_key_info key_info;
    int sockfd;
    int ret;
    int conn_index = -1;
    __u64 connection_id;
    
    printf("\n=== New TCP Connection Detected ===\n");
    printf("Source: %u.%u.%u.%u:%u\n",
           (event->saddr) & 0xFF,
           (event->saddr >> 8) & 0xFF,
           (event->saddr >> 16) & 0xFF,
           (event->saddr >> 24) & 0xFF,
           event->sport);
    printf("Destination: %u.%u.%u.%u:%u\n",
           (event->daddr) & 0xFF,
           (event->daddr >> 8) & 0xFF,
           (event->daddr >> 16) & 0xFF,
           (event->daddr >> 24) & 0xFF,
           event->dport);
    printf("PID: %u\n", event->pid);
    printf("Timestamp: %llu\n", event->timestamp);
    
    /* 准备四元组信息 */
    tuple.saddr = event->saddr;
    tuple.daddr = event->daddr;
    tuple.sport = event->sport;
    tuple.dport = event->dport;
    
    /* 性能指标：记录连接开始 */
    if (perf_ctx) {
        connection_id = event->timestamp;  /* 使用时间戳作为连接ID */
        conn_index = perf_metrics_connection_start(perf_ctx, connection_id);
        
        /* 开始测量连接建立延迟 */
        if (conn_index >= 0) {
            perf_metrics_connection_latency_start(perf_ctx, conn_index);
        }
    }
    
    /* 获取密钥 */
    printf("Fetching TLS key...\n");
    
    /* 性能指标：开始测量密钥协商时间 */
    if (perf_ctx && conn_index >= 0) {
        perf_metrics_key_negotiation_start(perf_ctx, conn_index);
    }
    
    ret = key_provider_get_key(&tuple, &key_info);
    
    /* 性能指标：结束测量密钥协商时间 */
    if (perf_ctx && conn_index >= 0) {
        perf_metrics_key_negotiation_end(perf_ctx, conn_index);
    }
    
    if (ret < 0) {
        fprintf(stderr, "Failed to get TLS key\n");
        /* 性能指标：记录连接失败 */
        if (perf_ctx && conn_index >= 0) {
            perf_metrics_connection_end(perf_ctx, conn_index, 0);
        }
        return;
    }
    
    printf("TLS key obtained successfully (key_len: %u, iv_len: %u)\n",
           key_info.key_len, key_info.iv_len);
    
    /* 性能指标：结束测量连接建立延迟 */
    if (perf_ctx && conn_index >= 0) {
        perf_metrics_connection_latency_end(perf_ctx, conn_index);
    }
    
    /* 
     * 注意：这里需要获取实际的 socket fd
     * 在实际实现中，可能需要通过 /proc/<pid>/fd 或其他方式
     * 这里仅作为示例
     */
    printf("Note: Socket fd retrieval not implemented in this example\n");
    printf("In production, KTLS would be configured here\n");
    
    /* 配置 KTLS (如果能获取到 sockfd) */
    /*
    ret = configure_ktls(sockfd, &key_info);
    if (ret < 0) {
        fprintf(stderr, "Failed to configure KTLS\n");
        return;
    }
    printf("KTLS configured successfully\n");
    */
    
    /* 性能指标：记录连接成功 */
    if (perf_ctx && conn_index >= 0) {
        perf_metrics_connection_end(perf_ctx, conn_index, 1);
    }
}

/**
 * 加载配置文件
 */
static int load_config(const char *config_file, struct capture_config *config) {
    FILE *fp;
    char line[512];
    
    /* 设置默认值 */
    config->mode = MODE_TLSHUB;
    strncpy(config->pod_node_config_path, DEFAULT_POD_NODE_CONFIG, 
            sizeof(config->pod_node_config_path) - 1);
    
    fp = fopen(config_file, "r");
    if (!fp) {
        printf("Config file not found, using defaults\n");
        return 0;
    }
    
    while (fgets(line, sizeof(line), fp) != NULL) {
        char key[128], value[256];
        
        /* 跳过注释和空行 */
        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }
        
        if (sscanf(line, "%s = %s", key, value) == 2) {
            if (strcmp(key, "mode") == 0) {
                if (strcmp(value, "tlshub") == 0) {
                    config->mode = MODE_TLSHUB;
                } else if (strcmp(value, "openssl") == 0) {
                    config->mode = MODE_OPENSSL;
                } else if (strcmp(value, "boringssl") == 0) {
                    config->mode = MODE_BORINGSSL;
                }
            } else if (strcmp(key, "pod_node_config") == 0) {
                strncpy(config->pod_node_config_path, value, 
                        sizeof(config->pod_node_config_path) - 1);
            }
        }
    }
    
    fclose(fp);
    return 0;
}

/**
 * 主函数
 */
int main(int argc, char **argv) {
    struct bpf_program *prog;
    struct perf_buffer *pb = NULL;
    struct capture_config config;
    const char *config_file = DEFAULT_CONFIG_FILE;
    int err = 0;
    
    /* 解析命令行参数 */
    if (argc > 1) {
        config_file = argv[1];
    }
    
    printf("TLShub Traffic Capture Module\n");
    printf("==============================\n\n");
    
    /* 加载配置 */
    printf("Loading configuration from %s\n", config_file);
    err = load_config(config_file, &config);
    if (err < 0) {
        fprintf(stderr, "Failed to load configuration\n");
        return 1;
    }
    
    printf("Configuration:\n");
    printf("  Mode: %d\n", config.mode);
    printf("  Pod-Node Config: %s\n", config.pod_node_config_path);
    printf("\n");
    
    /* 初始化 Pod-Node 映射表 */
    printf("Initializing Pod-Node mapping...\n");
    pod_node_table = init_pod_node_mapping(config.pod_node_config_path);
    if (pod_node_table) {
        print_pod_node_mapping(pod_node_table);
    } else {
        printf("Warning: Pod-Node mapping not available\n");
    }
    printf("\n");
    
    /* 初始化密钥提供者 */
    printf("Initializing key provider (mode: %d)...\n", config.mode);
    err = key_provider_init(config.mode);
    if (err < 0) {
        fprintf(stderr, "Failed to initialize key provider\n");
        goto cleanup;
    }
    
    /* 设置 Pod-Node 映射表到密钥提供者 */
    if (pod_node_table) {
        key_provider_set_pod_node_table(pod_node_table);
    }
    
    /* 初始化性能指标模块 */
    printf("Initializing performance metrics module...\n");
    perf_ctx = perf_metrics_init(DEFAULT_MAX_CONNECTIONS);
    if (!perf_ctx) {
        fprintf(stderr, "Warning: Performance metrics disabled\n");
    }
    printf("\n");
    
    /* 加载 eBPF 程序 */
    printf("Loading eBPF program...\n");
    /* 
     * 尝试从多个位置查找 eBPF 对象文件
     * 1. 当前目录（开发模式）
     * 2. /usr/local/lib/（安装模式）
     */
    const char *bpf_paths[] = {
        "capture.bpf.o",
        "/usr/local/lib/capture.bpf.o",
        NULL
    };
    
    obj = NULL;
    for (int i = 0; bpf_paths[i] != NULL; i++) {
        obj = bpf_object__open_file(bpf_paths[i], NULL);
        if (!libbpf_get_error(obj)) {
            printf("Loaded eBPF program from: %s\n", bpf_paths[i]);
            break;
        }
        obj = NULL;  /* Reset for next iteration */
    }
    
    if (!obj || libbpf_get_error(obj)) {
        fprintf(stderr, "Failed to open eBPF object file from any location\n");
        err = -1;
        goto cleanup;
    }
    
    err = bpf_object__load(obj);
    if (err) {
        fprintf(stderr, "Failed to load eBPF object: %d\n", err);
        goto cleanup;
    }
    
    /* 附加 eBPF 程序 */
    printf("Attaching eBPF programs...\n");
    bpf_object__for_each_program(prog, obj) {
        links[link_count] = bpf_program__attach(prog);
        if (libbpf_get_error(links[link_count])) {
            fprintf(stderr, "Failed to attach program %s\n", 
                    bpf_program__name(prog));
            links[link_count] = NULL;
            continue;
        }
        printf("  Attached: %s\n", bpf_program__name(prog));
        link_count++;
    }
    
    /* 设置 perf buffer */
    printf("Setting up perf buffer...\n");
    int events_fd = bpf_object__find_map_fd_by_name(obj, "events");
    if (events_fd < 0) {
        fprintf(stderr, "Failed to find events map\n");
        err = -1;
        goto cleanup;
    }
    
    /* 根据 libbpf 版本使用不同的 API */
#if defined(LIBBPF_MAJOR_VERSION) && LIBBPF_MAJOR_VERSION >= 1
    /* libbpf 1.0+ API: 回调函数作为独立参数传递 */
    pb = perf_buffer__new(events_fd, 8, handle_tcp_event, NULL, NULL, NULL);
#else
    /* libbpf 0.x API: 回调函数通过 opts 结构体传递 */
    struct perf_buffer_opts pb_opts = {
        .sample_cb = handle_tcp_event,
        .lost_cb = NULL,
        .ctx = NULL,
    };
    pb = perf_buffer__new(events_fd, 8, &pb_opts);
#endif
    if (libbpf_get_error(pb)) {
        fprintf(stderr, "Failed to create perf buffer\n");
        err = -1;
        goto cleanup;
    }
    
    /* 注册信号处理 */
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    
    printf("\nCapture module is running. Press Ctrl+C to stop.\n");
    printf("Monitoring TCP connections...\n");
    if (perf_ctx) {
        printf("Performance metrics monitoring enabled\n");
    }
    printf("\n");
    
    /* 主循环 */
    time_t last_perf_update = time(NULL);
    while (keep_running) {
        err = perf_buffer__poll(pb, 100);
        if (err < 0 && err != -EINTR) {
            fprintf(stderr, "Error polling perf buffer: %d\n", err);
            break;
        }
        
        /* 定期更新系统性能指标 */
        if (perf_ctx) {
            time_t now = time(NULL);
            if (now - last_perf_update >= PERF_UPDATE_INTERVAL_SEC) {
                perf_metrics_update_system(perf_ctx);
                last_perf_update = now;
            }
        }
    }
    
cleanup:
    printf("\nCleaning up...\n");
    
    /* 打印性能报告 */
    if (perf_ctx) {
        printf("\nGenerating performance report...\n");
        perf_metrics_print_report(perf_ctx);
        
        /* 导出性能指标到文件 */
        char json_file[256], csv_file[256];
        time_t now = time(NULL);
        snprintf(json_file, sizeof(json_file), "perf_metrics_%ld.json", now);
        snprintf(csv_file, sizeof(csv_file), "perf_metrics_%ld.csv", now);
        
        perf_metrics_export_json(perf_ctx, json_file);
        perf_metrics_export_csv(perf_ctx, csv_file);
    }
    
    if (pb) {
        perf_buffer__free(pb);
    }
    
    /* 分离所有 eBPF 程序 */
    for (int i = 0; i < link_count; i++) {
        if (links[i]) {
            bpf_link__destroy(links[i]);
        }
    }
    
    if (obj) {
        bpf_object__close(obj);
    }
    
    /* 清理密钥提供者 */
    key_provider_cleanup();
    
    /* 清理性能指标模块 */
    if (perf_ctx) {
        perf_metrics_cleanup(perf_ctx);
    }
    
    /* 清理 Pod-Node 映射表 */
    if (pod_node_table) {
        free_pod_node_mapping(pod_node_table);
    }
    
    printf("Shutdown complete\n");
    return err < 0 ? 1 : 0;
}
