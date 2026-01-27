#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "key_provider.h"
#include "tlshub_client.h"
#include "pod_mapping.h"

static enum key_provider_mode current_mode = MODE_TLSHUB;
static SSL_CTX *ssl_ctx = NULL;
static struct pod_node_table *pod_node_table = NULL;

/* OpenSSL 密钥协商函数 */
static int openssl_get_key(struct flow_tuple *tuple, struct tls_key_info *key_info);

/* BoringSSL 密钥协商函数 */
static int boringssl_get_key(struct flow_tuple *tuple, struct tls_key_info *key_info);

/**
 * 初始化密钥提供者
 */
int key_provider_init(enum key_provider_mode mode) {
    current_mode = mode;
    
    switch (mode) {
        case MODE_TLSHUB:
            printf("Initializing TLSHub key provider\n");
            return tlshub_client_init();
            
        case MODE_OPENSSL:
            printf("Initializing OpenSSL key provider\n");
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
            /* OpenSSL 1.1.0+ */
            OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, NULL);
#else
            /* OpenSSL 1.0.x */
            SSL_library_init();
            SSL_load_error_strings();
            OpenSSL_add_all_algorithms();
#endif
            ssl_ctx = SSL_CTX_new(TLS_method());
            if (!ssl_ctx) {
                fprintf(stderr, "Failed to create SSL context\n");
                return -1;
            }
            return 0;
            
        case MODE_BORINGSSL:
            printf("Initializing BoringSSL key provider\n");
            /* BoringSSL 初始化 */
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
            OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, NULL);
#else
            SSL_library_init();
#endif
            ssl_ctx = SSL_CTX_new(TLS_method());
            if (!ssl_ctx) {
                fprintf(stderr, "Failed to create SSL context\n");
                return -1;
            }
            return 0;
            
        default:
            fprintf(stderr, "Unknown key provider mode: %d\n", mode);
            return -1;
    }
}

/**
 * 清理密钥提供者
 */
void key_provider_cleanup(void) {
    switch (current_mode) {
        case MODE_TLSHUB:
            tlshub_client_cleanup();
            break;
            
        case MODE_OPENSSL:
        case MODE_BORINGSSL:
            if (ssl_ctx) {
                SSL_CTX_free(ssl_ctx);
                ssl_ctx = NULL;
            }
#if OPENSSL_VERSION_NUMBER < 0x10100000L
            /* Only needed for OpenSSL 1.0.x */
            EVP_cleanup();
#endif
            break;
    }
    
    printf("Key provider cleaned up\n");
}

/**
 * 设置 Pod-Node 映射表
 */
void key_provider_set_pod_node_table(struct pod_node_table *table) {
    pod_node_table = table;
    printf("Pod-Node mapping table set in key provider\n");
}

/**
 * 获取密钥
 */
int key_provider_get_key(struct flow_tuple *tuple, struct tls_key_info *key_info) {
    int ret;
    uint32_t server_node_ip = 0;
    
    if (!tuple || !key_info) {
        fprintf(stderr, "Invalid parameters for key_provider_get_key\n");
        return -1;
    }
    
    /* 如果有 Pod-Node 映射表，尝试查找 server_node_ip */
    if (pod_node_table) {
        server_node_ip = get_node_ip_by_pod_ip(pod_node_table, tuple->daddr);
        if (server_node_ip != 0) {
            char ip_str[INET_ADDRSTRLEN];
            struct in_addr addr;
            addr.s_addr = server_node_ip;
            inet_ntop(AF_INET, &addr, ip_str, sizeof(ip_str));
            printf("Found server_node_ip: %s for server_pod_ip\n", ip_str);
        } else {
            printf("Warning: server_node_ip not found in mapping table, using 0\n");
        }
    } else {
        printf("Warning: Pod-Node mapping table not available, server_node_ip will be 0\n");
    }
    
    switch (current_mode) {
        case MODE_TLSHUB:
            /* 尝试从 TLSHub 获取密钥 */
            ret = tlshub_fetch_key(tuple, key_info, server_node_ip);
            if (ret < 0) {
                /* 获取失败，发起握手 */
                printf("Fetch key failed, initiating handshake\n");
                ret = tlshub_handshake(tuple, server_node_ip);
                if (ret < 0) {
                    fprintf(stderr, "TLSHub handshake failed\n");
                    return ret;
                }
                
                /* 握手成功后重试获取密钥 */
                ret = tlshub_fetch_key(tuple, key_info, server_node_ip);
                if (ret < 0) {
                    fprintf(stderr, "Fetch key failed after handshake\n");
                    return ret;
                }
            }
            return 0;
            
        case MODE_OPENSSL:
            return openssl_get_key(tuple, key_info);
            
        case MODE_BORINGSSL:
            return boringssl_get_key(tuple, key_info);
            
        default:
            fprintf(stderr, "Unknown key provider mode: %d\n", current_mode);
            return -1;
    }
}

/**
 * 设置密钥提供者模式
 */
void key_provider_set_mode(enum key_provider_mode mode) {
    current_mode = mode;
    printf("Key provider mode set to: %d\n", mode);
}

/**
 * 获取当前密钥提供者模式
 */
enum key_provider_mode key_provider_get_mode(void) {
    return current_mode;
}

/**
 * OpenSSL 密钥协商函数
 * 
 * 注意：这是简化实现，用于演示目的
 * 实际生产环境应该建立完整的 TLS 连接并协商密钥
 * 
 * Exporter 标签说明：
 * - 使用自定义标签 "EXPORTER-TLS-Capture" 
 * - 如果与 TLSHub 集成，应确保使用相同的标签
 * - 标准 RFC 5705 定义的标签可用于互操作性
 */
static int openssl_get_key(struct flow_tuple *tuple, struct tls_key_info *key_info) {
    SSL *ssl = NULL;
    unsigned char key_material[64];
    int ret;
    
    if (!ssl_ctx) {
        fprintf(stderr, "SSL context not initialized\n");
        return -1;
    }
    
    /* 创建 SSL 对象 */
    ssl = SSL_new(ssl_ctx);
    if (!ssl) {
        fprintf(stderr, "Failed to create SSL object\n");
        return -1;
    }
    
    /* 这里是简化的实现，实际需要建立完整的 TLS 连接 */
    /* 
     * 生成密钥材料
     * 使用 RFC 5705 定义的 exporter 机制
     * 标签应与 TLSHub 模块保持一致
     */
    ret = SSL_export_keying_material(ssl, key_material, sizeof(key_material),
                                      "EXPORTER-TLS-Capture", 21, NULL, 0, 0);
    if (ret != 1) {
        fprintf(stderr, "Failed to export keying material\n");
        SSL_free(ssl);
        return -1;
    }
    
    /* 填充密钥信息 - 使用正确的 AES-GCM-128 密钥长度 */
    memcpy(key_info->key, key_material, 16);  /* AES-GCM-128: 16 bytes key */
    memcpy(key_info->iv, key_material + 16, 12);  /* GCM IV: 12 bytes (includes 4-byte salt) */
    key_info->key_len = 16;
    key_info->iv_len = 12;
    
    SSL_free(ssl);
    printf("OpenSSL key negotiation completed\n");
    return 0;
}

/**
 * BoringSSL 密钥协商函数
 */
static int boringssl_get_key(struct flow_tuple *tuple, struct tls_key_info *key_info) {
    /* BoringSSL 实现与 OpenSSL 类似 */
    /* 这里简化为调用 OpenSSL 函数 */
    printf("Using BoringSSL for key negotiation\n");
    return openssl_get_key(tuple, key_info);
}
