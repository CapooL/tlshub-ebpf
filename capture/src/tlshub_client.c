#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include "tlshub_client.h"

#define NETLINK_TLSHUB 31  /* TLSHub Netlink 协议号 */

static int netlink_sock = -1;

/**
 * 初始化 TLSHub 客户端
 */
int tlshub_client_init(void) {
    struct sockaddr_nl src_addr;
    
    /* 创建 Netlink socket */
    netlink_sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_TLSHUB);
    if (netlink_sock < 0) {
        perror("Failed to create netlink socket");
        return -1;
    }
    
    /* 绑定本地地址 */
    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = AF_NETLINK;
    src_addr.nl_pid = getpid();
    
    if (bind(netlink_sock, (struct sockaddr*)&src_addr, sizeof(src_addr)) < 0) {
        perror("Failed to bind netlink socket");
        close(netlink_sock);
        netlink_sock = -1;
        return -1;
    }
    
    printf("TLSHub client initialized (netlink socket: %d)\n", netlink_sock);
    return 0;
}

/**
 * 清理 TLSHub 客户端
 */
void tlshub_client_cleanup(void) {
    if (netlink_sock >= 0) {
        close(netlink_sock);
        netlink_sock = -1;
        printf("TLSHub client cleaned up\n");
    }
}

/**
 * 根据四元组从 TLSHub 获取密钥
 */
int tlshub_fetch_key(struct flow_tuple *tuple, struct tls_key_info *key_info) {
    struct sockaddr_nl dest_addr;
    struct nlmsghdr *nlh = NULL;
    struct iovec iov;
    struct msghdr msg;
    int ret;
    
    if (netlink_sock < 0 || !tuple || !key_info) {
        fprintf(stderr, "Invalid parameters for tlshub_fetch_key\n");
        return -1;
    }
    
    /* 分配 Netlink 消息 */
    nlh = (struct nlmsghdr*)malloc(NLMSG_SPACE(sizeof(struct flow_tuple)));
    if (!nlh) {
        fprintf(stderr, "Failed to allocate netlink message\n");
        return -1;
    }
    
    memset(nlh, 0, NLMSG_SPACE(sizeof(struct flow_tuple)));
    nlh->nlmsg_len = NLMSG_SPACE(sizeof(struct flow_tuple));
    nlh->nlmsg_pid = getpid();
    nlh->nlmsg_flags = 0;
    nlh->nlmsg_type = NLMSG_TYPE_FETCH_KEY;
    
    /* 复制四元组信息 */
    memcpy(NLMSG_DATA(nlh), tuple, sizeof(struct flow_tuple));
    
    /* 设置目标地址（内核） */
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.nl_family = AF_NETLINK;
    dest_addr.nl_pid = 0;  /* 内核 */
    dest_addr.nl_groups = 0;
    
    /* 发送消息 */
    iov.iov_base = (void*)nlh;
    iov.iov_len = nlh->nlmsg_len;
    memset(&msg, 0, sizeof(msg));
    msg.msg_name = (void*)&dest_addr;
    msg.msg_namelen = sizeof(dest_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    
    ret = sendmsg(netlink_sock, &msg, 0);
    if (ret < 0) {
        perror("Failed to send fetch_key message");
        free(nlh);
        return -1;
    }
    
    /* 等待响应 */
    memset(nlh, 0, NLMSG_SPACE(sizeof(struct tls_key_info)));
    iov.iov_len = NLMSG_SPACE(sizeof(struct tls_key_info));
    ret = recvmsg(netlink_sock, &msg, 0);
    if (ret < 0) {
        perror("Failed to receive fetch_key response");
        free(nlh);
        return -1;
    }
    
    /* 复制密钥信息 */
    memcpy(key_info, NLMSG_DATA(nlh), sizeof(struct tls_key_info));
    
    free(nlh);
    printf("Fetched TLS key from TLSHub (key_len: %u)\n", key_info->key_len);
    return 0;
}

/**
 * 通过 TLSHub 发起握手
 */
int tlshub_handshake(struct flow_tuple *tuple) {
    struct sockaddr_nl dest_addr;
    struct nlmsghdr *nlh = NULL;
    struct iovec iov;
    struct msghdr msg;
    int ret;
    
    if (netlink_sock < 0 || !tuple) {
        fprintf(stderr, "Invalid parameters for tlshub_handshake\n");
        return -1;
    }
    
    /* 分配 Netlink 消息 */
    nlh = (struct nlmsghdr*)malloc(NLMSG_SPACE(sizeof(struct flow_tuple)));
    if (!nlh) {
        fprintf(stderr, "Failed to allocate netlink message\n");
        return -1;
    }
    
    memset(nlh, 0, NLMSG_SPACE(sizeof(struct flow_tuple)));
    nlh->nlmsg_len = NLMSG_SPACE(sizeof(struct flow_tuple));
    nlh->nlmsg_pid = getpid();
    nlh->nlmsg_flags = 0;
    nlh->nlmsg_type = NLMSG_TYPE_HANDSHAKE;
    
    /* 复制四元组信息 */
    memcpy(NLMSG_DATA(nlh), tuple, sizeof(struct flow_tuple));
    
    /* 设置目标地址（内核） */
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.nl_family = AF_NETLINK;
    dest_addr.nl_pid = 0;  /* 内核 */
    dest_addr.nl_groups = 0;
    
    /* 发送消息 */
    iov.iov_base = (void*)nlh;
    iov.iov_len = nlh->nlmsg_len;
    memset(&msg, 0, sizeof(msg));
    msg.msg_name = (void*)&dest_addr;
    msg.msg_namelen = sizeof(dest_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    
    ret = sendmsg(netlink_sock, &msg, 0);
    if (ret < 0) {
        perror("Failed to send handshake message");
        free(nlh);
        return -1;
    }
    
    /* 等待响应 */
    memset(nlh, 0, NLMSG_SPACE(sizeof(int)));
    iov.iov_len = NLMSG_SPACE(sizeof(int));
    ret = recvmsg(netlink_sock, &msg, 0);
    if (ret < 0) {
        perror("Failed to receive handshake response");
        free(nlh);
        return -1;
    }
    
    /* 检查握手结果 */
    ret = *(int*)NLMSG_DATA(nlh);
    free(nlh);
    
    if (ret == 0) {
        printf("TLSHub handshake completed successfully\n");
    } else {
        fprintf(stderr, "TLSHub handshake failed with error: %d\n", ret);
    }
    
    return ret;
}
