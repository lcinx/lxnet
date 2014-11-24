
/*
 * Copyright (C) lcinx
 * lcinx@163.com
*/

#ifndef _H_NET_POOL_H_
#define _H_NET_POOL_H_

#ifdef __cplusplus
extern "C"{
#endif

#include <stddef.h>
#include "alway_inline.h"

/* 
 * create and init net some pool.
 * socketnum --- socket object num.
 * socketsize --- socket object size.
 *
 * listennum --- listen object num.
 * listensize --- listen object size.
 * */
bool netpool_init (size_t socketnum, size_t socketsize, size_t listennum, size_t listensize);

/* release net some pool. */
void netpool_release ();

void *netpool_createsocket ();

void netpool_releasesocket (void *self);

void *netpool_createlisten ();

void netpool_releaselisten (void *self);

/* get net some pool info. */
void netpool_meminfo (char *buf, size_t bufsize);

#ifdef __cplusplus
}
#endif
#endif

