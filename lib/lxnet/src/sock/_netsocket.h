
/*
 * Copyright (C) lcinx
 * lcinx@163.com
*/

#ifndef _H__NETSOCKET_H_
#define _H__NETSOCKET_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "platform_config.h"
#include "net_crypt.h"

struct socketer;

/* get socket object size. */
size_t socketer_getsize();

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

bool socketer_isclose(struct socketer *self);

void socketer_getip(struct socketer *self, char *ip, size_t len);

bool socketer_gethostname(char *name, size_t len);

bool socketer_gethostbyname(const char *name, char *buf, size_t len);

bool socketer_sendmsg(struct socketer *self, void *data, int len);

/* 
 * when sending data. test send limit as len.
 * if return true, close this connect.
 */
bool socketer_send_islimit(struct socketer *self, size_t len);

/* set send event. */
void socketer_checksend(struct socketer *self);

void *socketer_getmsg(struct socketer *self, char *buf, size_t bufsize);

void *socketer_getdata(struct socketer *self, char *buf, size_t bufsize, int *datalen);

/* set recv event. */
void socketer_checkrecv(struct socketer *self);



/* set recv data limit. */
void socketer_set_recv_critical(struct socketer *self, long size);

/* set send data limit.*/
void socketer_set_send_critical(struct socketer *self, long size);

void socketer_use_compress(struct socketer *self);

void socketer_use_uncompress(struct socketer *self);

/* set encrypt function and logic data. */
void socketer_set_encrypt_function(struct socketer *self, dofunc_f encrypt_func, void (*release_logicdata) (void *), void *logicdata);

/* set encrypt function and logic data. */
void socketer_set_decrypt_function(struct socketer *self, dofunc_f decrypt_func, void (*release_logicdata) (void *), void *logicdata);

void socketer_use_encrypt(struct socketer *self);

void socketer_use_decrypt(struct socketer *self);

void socketer_use_tgw(struct socketer *self);

void socketer_set_raw_datasize(struct socketer *self, size_t size);

/* interface for event mgr. */

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

