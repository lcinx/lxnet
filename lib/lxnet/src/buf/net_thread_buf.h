
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#ifndef _H_NET_THREAD_BUF_H_
#define _H_NET_THREAD_BUF_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "platform_config.h"
#include "buf/buf_info.h"

/* get temp packet buf. */
struct buf_info threadbuf_get_msg_buf();

/* get temp compress/uncompress buf. */
struct buf_info threadbuf_get_compress_buf();

/* get quicklz compress lib buf. */
void *threadbuf_get_quicklz_buf();

/* 
 * Initialize thread private buffer set, for getmsg and compress, uncompress etc temp buf.
 *
 * msgmaxsize --- max packet size.
 * compressmaxsize --- max compress/uncompress buffer size.
 */
bool threadbuf_init(size_t msgmaxsize, size_t compressmaxsize);

/* release thread private buffer set. */
void threadbuf_release();

#ifdef __cplusplus
}
#endif
#endif

