
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#ifndef _H_POOL_H_
#define _H_POOL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

struct poolmgr;

/*
 * create poolmgr.
 * size is block size,
 * alignment is align number,
 * num is initialize block num,
 * next_multiple is next num,
 *		the next num is num * next_multiple, if next_multiple is zero, then only has one sub pool.
 * name is poolmgr name.
 */
struct poolmgr *poolmgr_create(size_t size, size_t alignment, 
		size_t num, size_t next_multiple, const char *name);

void poolmgr_release(struct poolmgr *self);

void poolmgr_set_shrink(struct poolmgr *self, size_t free_pool_num, double free_node_ratio);

void *poolmgr_alloc_object(struct poolmgr *self);

void poolmgr_free_object(struct poolmgr *self, void *bk);

void poolmgr_get_info(struct poolmgr *self, char *buf, size_t bufsize);

#ifdef __cplusplus
}
#endif
#endif

