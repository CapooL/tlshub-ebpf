#ifndef __KEY_PROVIDER_H__
#define __KEY_PROVIDER_H__

#include "capture.h"

/**
 * 初始化密钥提供者
 * @param mode: 密钥提供者模式
 * @return: 成功返回 0，失败返回负值
 */
int key_provider_init(enum key_provider_mode mode);

/**
 * 清理密钥提供者
 */
void key_provider_cleanup(void);

/**
 * 获取密钥
 * @param tuple: 四元组信息
 * @param key_info: 用于存储获取的密钥信息
 * @return: 成功返回 0，失败返回负值
 */
int key_provider_get_key(struct flow_tuple *tuple, struct tls_key_info *key_info);

/**
 * 设置密钥提供者模式
 * @param mode: 密钥提供者模式
 */
void key_provider_set_mode(enum key_provider_mode mode);

/**
 * 获取当前密钥提供者模式
 * @return: 当前模式
 */
enum key_provider_mode key_provider_get_mode(void);

#endif /* __KEY_PROVIDER_H__ */
