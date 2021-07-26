
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#ifndef _H_NET_SOCKET_H_
#define _H_NET_SOCKET_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "platform_config.h"
#include "net_crypt.h"

struct socketer;

/* get socket object size. */
size_t socketer_get_size();

/*
 * create socketer.
 * bigbuf --- if is true, then is bigbuf; or else is smallbuf.
 */
struct socketer *socketer_create(bool bigbuf);

struct socketer *socketer_create_for_accept(bool bigbuf, void *sockfd);

/* release socketer */
void socketer_release(struct socketer *self);

bool socketer_connect(struct socketer *self, const char *ip, short port);

void socketer_close(struct socketer *self);

bool socketer_is_close(struct socketer *self);

void socketer_get_ip(struct socketer *self, char *ip, size_t len);

int socketer_get_send_buffer_byte_size(struct socketer *self);

int socketer_get_recv_buffer_byte_size(struct socketer *self);

bool socketer_get_hostname(char *buf, size_t len);

bool socketer_get_host_ip_by_name(const char *name, char *buf, size_t len, bool ipv6);

bool socketer_send_msg(struct socketer *self, void *data, int len);

bool socketer_send_data(struct socketer *self, void *data, int len);

/*
 * when sending data. test send limit as len.
 * if return true, close this connect.
 */
bool socketer_send_is_limit(struct socketer *self, size_t len);

/* set send event. */
void socketer_check_send(struct socketer *self);

void *socketer_get_msg(struct socketer *self, char *buf, size_t bufsize);

void *socketer_get_data(struct socketer *self, char *buf, size_t bufsize, int *datalen);

int socketer_find_data_end_size(struct socketer *self, const char *data, int datalen);

/* set recv event. */
void socketer_check_recv(struct socketer *self);



/* set recv data limit. */
void socketer_set_recv_limit(struct socketer *self, int size);

/* set send data limit. */
void socketer_set_send_limit(struct socketer *self, int size);

void socketer_use_compress(struct socketer *self);

void socketer_use_uncompress(struct socketer *self);

/* set encrypt function and logic data. */
void socketer_set_encrypt_function(struct socketer *self, 
		dofunc_f encrypt_func, void (*release_logicdata)(void *), void *logicdata);

/* set encrypt function and logic data. */
void socketer_set_decrypt_function(struct socketer *self, 
		dofunc_f decrypt_func, void (*release_logicdata)(void *), void *logicdata);

void socketer_use_encrypt(struct socketer *self);

void socketer_use_decrypt(struct socketer *self);

void socketer_use_proxy(struct socketer *self, bool flag);

void socketer_set_proxy_param(struct socketer *self, 
		const char *proxy_end_char, size_t proxy_end_char_len, char *proxy_buff, size_t proxy_buff_len);

void socketer_set_raw_datasize(struct socketer *self, int size);

/*
 * ================================================================================
 * interface for event mgr.
 * ================================================================================
 */

void socketer_on_recv(struct socketer *self, int len);

void socketer_on_send(struct socketer *self, int len);

/* create and init socketer manager. */
bool socketmgr_init();

/* run socketer manager. */
void socketmgr_run();

/* release socketer manager. */
void socketmgr_release();

#ifdef __cplusplus
}
#endif
#endif

