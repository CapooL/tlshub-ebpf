#ifndef __TLSHUB_CLIENT_H__
#define __TLSHUB_CLIENT_H__

#include "capture.h"

/**
 * 初始化 TLSHub 客户端
 * @return: 成功返回 0，失败返回负值
 */
int tlshub_client_init(void);

/**
 * 清理 TLSHub 客户端
 */
void tlshub_client_cleanup(void);

/**
 * 根据四元组从 TLSHub 获取密钥
 * @param tuple: 四元组信息
 * @param key_info: 用于存储获取的密钥信息
 * @param server_node_ip: 服务端 Node IP（网络字节序），如果为 0 则不填充
 * @return: 成功返回 0，失败返回负值
 */
int tlshub_fetch_key(struct flow_tuple *tuple, struct tls_key_info *key_info, uint32_t server_node_ip);

/**
 * 通过 TLSHub 发起握手
 * @param tuple: 四元组信息
 * @param server_node_ip: 服务端 Node IP（网络字节序），如果为 0 则不填充
 * @return: 成功返回 0，失败返回负值
 */
int tlshub_handshake(struct flow_tuple *tuple, uint32_t server_node_ip);

#endif /* __TLSHUB_CLIENT_H__ */
