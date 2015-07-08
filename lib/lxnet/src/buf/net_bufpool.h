
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#ifndef _H_NET_BUFPOOL_H_
#define _H_NET_BUFPOOL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "platform_config.h"

/*
 * create and init buf pool.
 * bigblocknum --- is big block num.
 * bigblocksize --- is big block size.
 *
 * smallblocknum --- is small block num.
 * smallblocksize --- is small block size.
 * 
 * bufnum --- is buf num.
 * bufsize --- is buf size.
 */
bool bufpool_init(size_t bigblocknum, size_t bigblocksize, size_t smallblocknum, size_t smallblocksize, size_t bufnum, size_t bufsize);

/* release buf pool. */
void bufpool_release();

void *bufpool_createbigblock();

void bufpool_releasebigblock(void *self);

void *bufpool_createsmallblock();

void bufpool_releasesmallblock(void *self);

void *bufpool_createbuf();

void bufpool_releasebuf(void *self);

/* get buf pool memory info. */
void bufpool_meminfo(char *buf, size_t bufsize);

#ifdef __cplusplus
}
#endif
#endif

