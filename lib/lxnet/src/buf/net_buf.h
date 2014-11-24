
/*
 * Copyright (C) lcinx
 * lcinx@163.com
*/

#ifndef _H_NET_BUF_H_
#define _H_NET_BUF_H_

#ifdef __cplusplus
extern "C"{
#endif


#include <stddef.h>
#include "alway_inline.h"
#include "buf_info.h"
#include "net_crypt.h"

/* max packet size --- 256K. */
#define _MAX_MSG_LEN (1024*256)
struct net_buf;
/* 
 * create buf.
 * bigbuf --- big or small buf, if is true, then is big buf, or else is small buf.
 * */
struct net_buf *buf_create (bool bigbuf);

/*
 * set buf encrypt function or decrypt function, and some logic data.
 * */
void buf_setdofunc (struct net_buf *self, dofunc_f func, void *logicdata);

/* release buf. */
void buf_release (struct net_buf *self);

/* set buf handle limit size. */
void buf_set_limitsize (struct net_buf *self, int limit_len);

void buf_usecompress (struct net_buf *self);

void buf_useuncompress (struct net_buf *self);

void buf_useencrypt (struct net_buf *self);

void buf_usedecrypt (struct net_buf *self);

void buf_use_tgw (struct net_buf *self);

void buf_set_raw_datasize (struct net_buf *self, size_t size);

/* push len, if is more than the limit, return true.*/
bool buf_add_islimit (struct net_buf *self, size_t len);

/* test can recv data, if not recv, return true. */
bool buf_can_not_recv (struct net_buf *self);

/* test has data for send, if not has data,  return true. */
bool buf_can_not_send (struct net_buf *self);

/*
 * some recv interface.
 *
 * */

/* get write buffer info. */
struct bufinfo buf_getwritebufinfo (struct net_buf *self);

/* add write position. */
void buf_addwrite (struct net_buf *self, char *buf, int len);

/* 
 * recv end, do something, if return flase, then close connect.
 * */
bool buf_recv_end_do (struct net_buf *self);

/*
 * some send interface.
 *
 * */

/* get read buffer info. */
struct bufinfo buf_getreadbufinfo (struct net_buf *self);

/* add read positon. */
void buf_addread (struct net_buf *self, int len);

/* before send, do something. */
void buf_send_before_do (struct net_buf *self);


/* push packet into the buffer. */
bool buf_pushmessage (struct net_buf *self, const char *msgbuf, int len);

/* get packet from the buffer, if error, then needclose is true. */
char *buf_getmessage (struct net_buf *self, bool *needclose, char *buf, size_t bufsize, int sockfd);


/* 
 * create and init buf pool.
 * bigbufnum --- is bigbuf num.
 * bigbufsize --- is bigbuf size.
 * smallbufnum --- is small buf num.
 * smallbufsize --- is small buf size.
 * bufnum --- is buf num.
 * 
 * because a socket need 2 buf, so bufnum is *2, in init function.
 *
 * this function be able to call private thread buffer etc.
 * */
bool bufmgr_init (size_t bigbufnum, size_t bigbufsize, size_t smallbufnum, size_t smallbufsize, size_t bufnum);

/* release some buf. */
void bufmgr_release ();

/* get some buf memroy info. */
void bufmgr_meminfo (char *buf, size_t bufsize);

/* enable/disable errorlog, and return before value. */
bool buf_set_enable_errorlog (bool flag);

/* get now enable or disable errorlog. */
bool buf_get_enable_errorlog ();

#ifdef __cplusplus
}
#endif
#endif

