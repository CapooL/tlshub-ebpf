#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <linux/tls.h>
#include "ktls_config.h"

/**
 * 为 Socket 配置 KTLS
 */
int configure_ktls(int sockfd, struct tls_key_info *key_info) {
    int ret;
    
    /* 启用 KTLS 发送 */
    ret = enable_ktls_tx(sockfd, key_info);
    if (ret < 0) {
        fprintf(stderr, "Failed to enable KTLS TX\n");
        return ret;
    }
    
    /* 启用 KTLS 接收 */
    ret = enable_ktls_rx(sockfd, key_info);
    if (ret < 0) {
        fprintf(stderr, "Failed to enable KTLS RX\n");
        return ret;
    }
    
    printf("KTLS configured successfully for socket %d\n", sockfd);
    return 0;
}

/**
 * 启用 Socket 的 KTLS 发送
 */
int enable_ktls_tx(int sockfd, struct tls_key_info *key_info) {
    struct tls12_crypto_info_aes_gcm_128 crypto_info;
    int ret;
    
    /* 首先启用 TLS ULP (Upper Layer Protocol) */
    ret = setsockopt(sockfd, SOL_TCP, TCP_ULP, "tls", sizeof("tls"));
    if (ret < 0) {
        perror("Failed to set TCP_ULP");
        return ret;
    }
    
    /* 配置加密信息 */
    memset(&crypto_info, 0, sizeof(crypto_info));
    crypto_info.info.version = TLS_1_2_VERSION;
    crypto_info.info.cipher_type = TLS_CIPHER_AES_GCM_128;
    
    /* 复制密钥 */
    memcpy(crypto_info.key, key_info->key, TLS_CIPHER_AES_GCM_128_KEY_SIZE);
    
    /* 
     * 对于 AES-GCM，IV 包含 salt (4 bytes) 和 nonce (8 bytes)
     * 从密钥材料中正确提取 salt 和 IV
     */
    if (key_info->iv_len >= 12) {
        /* 前 4 字节作为 salt，后 8 字节作为 IV */
        memcpy(crypto_info.salt, key_info->iv, TLS_CIPHER_AES_GCM_128_SALT_SIZE);
        memcpy(crypto_info.iv, key_info->iv + TLS_CIPHER_AES_GCM_128_SALT_SIZE, 
               TLS_CIPHER_AES_GCM_128_IV_SIZE);
    } else {
        fprintf(stderr, "Warning: IV length insufficient, using zero padding\n");
        memcpy(crypto_info.salt, key_info->iv, 
               key_info->iv_len < 4 ? key_info->iv_len : 4);
    }
    
    /* 配置发送密钥 */
    ret = setsockopt(sockfd, SOL_TLS, TLS_TX, &crypto_info, sizeof(crypto_info));
    if (ret < 0) {
        perror("Failed to set TLS_TX");
        return ret;
    }
    
    printf("KTLS TX enabled for socket %d\n", sockfd);
    return 0;
}

/**
 * 启用 Socket 的 KTLS 接收
 */
int enable_ktls_rx(int sockfd, struct tls_key_info *key_info) {
    struct tls12_crypto_info_aes_gcm_128 crypto_info;
    int ret;
    
    /* 配置加密信息 */
    memset(&crypto_info, 0, sizeof(crypto_info));
    crypto_info.info.version = TLS_1_2_VERSION;
    crypto_info.info.cipher_type = TLS_CIPHER_AES_GCM_128;
    
    /* 复制密钥 */
    memcpy(crypto_info.key, key_info->key, TLS_CIPHER_AES_GCM_128_KEY_SIZE);
    
    /* 
     * 对于 AES-GCM，IV 包含 salt (4 bytes) 和 nonce (8 bytes)
     * 从密钥材料中正确提取 salt 和 IV
     */
    if (key_info->iv_len >= 12) {
        /* 前 4 字节作为 salt，后 8 字节作为 IV */
        memcpy(crypto_info.salt, key_info->iv, TLS_CIPHER_AES_GCM_128_SALT_SIZE);
        memcpy(crypto_info.iv, key_info->iv + TLS_CIPHER_AES_GCM_128_SALT_SIZE, 
               TLS_CIPHER_AES_GCM_128_IV_SIZE);
    } else {
        fprintf(stderr, "Warning: IV length insufficient, using zero padding\n");
        memcpy(crypto_info.salt, key_info->iv, 
               key_info->iv_len < 4 ? key_info->iv_len : 4);
    }
    
    /* 配置接收密钥 */
    ret = setsockopt(sockfd, SOL_TLS, TLS_RX, &crypto_info, sizeof(crypto_info));
    if (ret < 0) {
        perror("Failed to set TLS_RX");
        return ret;
    }
    
    printf("KTLS RX enabled for socket %d\n", sockfd);
    return 0;
}
