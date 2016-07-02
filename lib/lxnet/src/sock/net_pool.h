
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#ifndef _H_NET_POOL_H_
#define _H_NET_POOL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "platform_config.h"

/*
 * create and init net some pool.
 * socketer_num --- socketer object num.
 * socketer_size --- socketer object size.
 *
 * listener_num --- listener object num.
 * listener_size --- listener object size.
 */
bool netpool_init(size_t socketer_num, size_t socketer_size, size_t listener_num, size_t listener_size);

/* release net some pool. */
void netpool_release();

void *netpool_create_socketer();

void netpool_release_socketer(void *self);

void *netpool_create_listener();

void netpool_release_listener(void *self);

/* get net some pool info. */
void netpool_get_memory_info(char *buf, size_t bufsize);

#ifdef __cplusplus
}
#endif
#endif

