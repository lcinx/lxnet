
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#ifndef _H_NET_BUF_H_
#define _H_NET_BUF_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "platform_config.h"
#include "buf/buf_info.h"
#include "net_crypt.h"

/* max packet size --- 136K. */
#define _MAX_MSG_LEN (136 * 1024)
struct net_buf;

/*
 * create buf.
 * bigbuf --- big or small buf, if is true, then is big buf, or else is small buf.
 */
struct net_buf *buf_create(bool bigbuf);

/*
 * set buf encrypt function or decrypt function, and some logic data.
 */
void buf_set_do_func(struct net_buf *self, dofunc_f func, 
		void (*release_logicdata)(void *logicdata), void *logicdata);

/* release buf. */
void buf_release(struct net_buf *self);

/* set buf handle limit size. */
void buf_set_limit_size(struct net_buf *self, int limit_len);

void buf_use_compress(struct net_buf *self);

void buf_use_uncompress(struct net_buf *self);

void buf_use_encrypt(struct net_buf *self);

void buf_use_decrypt(struct net_buf *self);

void buf_use_tgw(struct net_buf *self);

void buf_set_raw_datasize(struct net_buf *self, size_t size);

int buf_get_now_data_size(struct net_buf *self);

int buf_get_data_size(struct net_buf *self);

/* push len, if is more than the limit, return true.*/
bool buf_add_is_limit(struct net_buf *self, size_t len);

/* test can recv data, if not recv, return true. */
bool buf_can_not_recv(struct net_buf *self);

/* test has data for send, if not has data, return true. */
bool buf_can_not_send(struct net_buf *self);

/*
 * ================================================================================
 * some recv interface.
 * ================================================================================
 */

/* get write buffer info. */
struct buf_info buf_get_write_bufinfo(struct net_buf *self);

/* add write position. */
void buf_add_write(struct net_buf *self, char *buf, int len);

/*
 * recv end, do something, if return flase, then close connect.
 */
bool buf_recv_end_do(struct net_buf *self);

/*
 * ================================================================================
 * some send interface.
 * ================================================================================
 */

/* get read buffer info. */
struct buf_info buf_get_read_bufinfo(struct net_buf *self);

/* add read positon. */
void buf_add_read(struct net_buf *self, int len);

/* before send, do something. */
void buf_send_before_do(struct net_buf *self);


/* push packet into the buffer. */
bool buf_put_message(struct net_buf *self, const void *msg_data, int len);

/* push data into the buffer. */
bool buf_put_data(struct net_buf *self, const void *data, int len);

/* get packet from the buffer, if error, then need_close is true. */
char *buf_get_message(struct net_buf *self, bool *need_close, char *buf, size_t bufsize);

/* get data from the buffer, if error, then need_close is true. */
char *buf_get_data(struct net_buf *self, bool *need_close, char *buf, int bufsize, int *datalen);

/* find data end size from the buffer. */
int buf_find_data_end_size(struct net_buf *self, const char *data, int datalen);


/*
 * create and init buf pool.
 * big_buf_num --- is bigbuf num.
 * big_buf_size --- is bigbuf size.
 * small_buf_num --- is small buf num.
 * small_buf_size --- is small buf size.
 * buf_num --- is buf num.
 *
 * because a socket need 2 buf, so buf_num is * 2, in init function.
 *
 * this function be able to call private thread buffer etc.
 */
bool bufmgr_init(size_t big_buf_num, size_t big_buf_size, 
		size_t small_buf_num, size_t small_buf_size, size_t buf_num);

/* release some buf. */
void bufmgr_release();


struct poolmgr_info;

/* get some buf memroy info. */
size_t bufmgr_get_memory_info(struct poolmgr_info *array, size_t num);

/* enable/disable errorlog, and return before value. */
bool buf_set_enable_errorlog(bool flag);

/* get now enable or disable errorlog. */
bool buf_get_enable_errorlog();

#ifdef __cplusplus
}
#endif
#endif

