
/*
 * Copyright (C) lcinx
 * lcinx@163.com
*/

#ifndef _H_NET_MODULE_H_
#define _H_NET_MODULE_H_

#ifdef __cplusplus
extern "C"{
#endif

#include "alway_inline.h"
#include "_netlisten.h"
#include "_netsocket.h"


/* 
 * initialize network. 
 * bigbufsize --- big block size. 
 * bigbufnum --- big block num.
 *
 * smallbufsize --- small block size. 
 * smallbufnum --- small block num.
 * listen num --- listener object num. 
 * socket num --- socketer object num.
 * threadnum --- network thread num, if less than 0, then start by the number of cpu threads .
 */
bool netinit (size_t bigbufsize, size_t bigbufnum, size_t smallbufsize, size_t smallbufnum,
		size_t listenernum, size_t socketnum, int threadnum);

/* release network. */
void netrelease ();

/* network run. */
void netrun ();

/* get network memory info. */
void netmemory_info (char *buf, size_t bufsize);

#ifdef __cplusplus
}
#endif
#endif

