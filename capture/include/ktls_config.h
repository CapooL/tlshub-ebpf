#ifndef __KTLS_CONFIG_H__
#define __KTLS_CONFIG_H__

#include <linux/tls.h>
#include "capture.h"

/**
 * 为 Socket 配置 KTLS
 * @param sockfd: Socket 文件描述符
 * @param key_info: TLS 密钥信息
 * @return: 成功返回 0，失败返回负值
 */
int configure_ktls(int sockfd, struct tls_key_info *key_info);

/**
 * 启用 Socket 的 KTLS 发送
 * @param sockfd: Socket 文件描述符
 * @param key_info: TLS 密钥信息
 * @return: 成功返回 0，失败返回负值
 */
int enable_ktls_tx(int sockfd, struct tls_key_info *key_info);

/**
 * 启用 Socket 的 KTLS 接收
 * @param sockfd: Socket 文件描述符
 * @param key_info: TLS 密钥信息
 * @return: 成功返回 0，失败返回负值
 */
int enable_ktls_rx(int sockfd, struct tls_key_info *key_info);

#endif /* __KTLS_CONFIG_H__ */
