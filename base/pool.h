
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
#include <time.h>

struct poolmgr;
struct poolmgr_info {
	char name[64];

	size_t pool_num;
	size_t max_pool_num;
	time_t max_pool_num_time;

	size_t base_num;
	size_t current_max_num;
	size_t next_multiple;

	size_t alignment;
	size_t base_object_size;
	size_t object_size;

	size_t object_total_num;
	size_t object_current_num;

	size_t memory_total_bytes;

	size_t shrink_free_pool_num;
	double shrink_free_object_ratio;
};

/*
 * create poolmgr.
 * size is object size,
 * alignment is align number,
 * num is initialize object num,
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

void poolmgr_get_info(struct poolmgr *self, struct poolmgr_info *info);

#ifdef __cplusplus
}
#endif
#endif

