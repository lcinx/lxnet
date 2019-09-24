
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
 * big_block_num --- is big block num.
 * big_block_size --- is big block size.
 *
 * small_block_num --- is small block num.
 * small_block_size --- is small block size.
 *
 * buf_num --- is buf num.
 * buf_size --- is buf size.
 */
bool bufpool_init(size_t big_block_num, size_t big_block_size, 
		size_t small_block_num, size_t small_block_size, size_t buf_num, size_t buf_size);

/* release buf pool. */
void bufpool_release();

void *bufpool_create_big_block();

void bufpool_release_big_block(void *self);

void *bufpool_create_small_block();

void bufpool_release_small_block(void *self);

void *bufpool_create_net_buf();

void bufpool_release_net_buf(void *self);


struct poolmgr_info;

/* get buf pool memory info. */
size_t bufpool_get_memory_info(struct poolmgr_info *array, size_t num);

#ifdef __cplusplus
}
#endif
#endif

