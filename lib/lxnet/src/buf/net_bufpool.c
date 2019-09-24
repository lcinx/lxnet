
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#include <assert.h>
#include "net_bufpool.h"
#include "cthread.h"
#include "pool.h"

struct bufpool {
	bool is_init;

	size_t big_pool_num;
	size_t big_pool_size;
	struct poolmgr *big_block_pool;
	cspin big_lock;

	size_t small_pool_num;
	size_t small_pool_size;
	struct poolmgr *small_block_pool;
	cspin small_lock;

	size_t buf_num;
	size_t buf_size;
	struct poolmgr *buf_pool;
	cspin buf_lock;
};
static struct bufpool s_pool = {false};

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
		size_t small_block_num, size_t small_block_size, size_t buf_num, size_t buf_size) {

	if (s_pool.is_init)
		return false;

	if ((big_block_num == 0) || (big_block_size == 0) ||
		(small_block_num == 0) || (small_block_size == 0) ||
		(buf_num == 0) || (buf_size == 0))
		return false;

	s_pool.big_block_pool = poolmgr_create(big_block_size, 8, big_block_num, 1, 
																"big block pools");
	s_pool.small_block_pool = poolmgr_create(small_block_size, 8, small_block_num, 1, 
																"small block pools");

	s_pool.buf_pool = poolmgr_create(buf_size, 8, buf_num, 1, "buf pools");
	if (!s_pool.big_block_pool || !s_pool.small_block_pool || !s_pool.buf_pool) {
		poolmgr_release(s_pool.big_block_pool);
		poolmgr_release(s_pool.small_block_pool);
		poolmgr_release(s_pool.buf_pool);
		return false;
	}

	cspin_init(&s_pool.big_lock);
	cspin_init(&s_pool.small_lock);
	cspin_init(&s_pool.buf_lock);

	s_pool.big_pool_num = big_block_num;
	s_pool.big_pool_size = big_block_size;

	s_pool.small_pool_num = small_block_num;
	s_pool.small_pool_size = small_block_size;

	s_pool.buf_num = buf_num;
	s_pool.buf_size = buf_size;

	s_pool.is_init = true;
	return true;
}

/* release buf pool. */
void bufpool_release() {
	if (!s_pool.is_init)
		return;

	cspin_lock(&s_pool.big_lock);
	poolmgr_release(s_pool.big_block_pool);
	s_pool.big_block_pool = NULL;
	cspin_unlock(&s_pool.big_lock);
	cspin_destroy(&s_pool.big_lock);

	cspin_lock(&s_pool.small_lock);
	poolmgr_release(s_pool.small_block_pool);
	s_pool.small_block_pool = NULL;
	cspin_unlock(&s_pool.small_lock);
	cspin_destroy(&s_pool.small_lock);

	cspin_lock(&s_pool.buf_lock);
	poolmgr_release(s_pool.buf_pool);
	s_pool.buf_pool = NULL;
	cspin_unlock(&s_pool.buf_lock);
	cspin_destroy(&s_pool.buf_lock);

	s_pool.is_init = false;
}

void *bufpool_create_big_block() {
	void *self = NULL;
	if (!s_pool.is_init)
		return NULL;

	cspin_lock(&s_pool.big_lock);
	self = poolmgr_alloc_object(s_pool.big_block_pool);
	cspin_unlock(&s_pool.big_lock);
	return self;
}

void bufpool_release_big_block(void *self) {
	if (!self)
		return;

	cspin_lock(&s_pool.big_lock);
	poolmgr_free_object(s_pool.big_block_pool, self);
	cspin_unlock(&s_pool.big_lock);
}

void *bufpool_create_small_block() {
	void *self = NULL;
	if (!s_pool.is_init)
		return NULL;

	cspin_lock(&s_pool.small_lock);
	self = poolmgr_alloc_object(s_pool.small_block_pool);
	cspin_unlock(&s_pool.small_lock);
	return self;
}

void bufpool_release_small_block(void *self) {
	if (!self)
		return;

	cspin_lock(&s_pool.small_lock);
	poolmgr_free_object(s_pool.small_block_pool, self);
	cspin_unlock(&s_pool.small_lock);
}

void *bufpool_create_net_buf() {
	void *self = NULL;
	if (!s_pool.is_init)
		return NULL;

	cspin_lock(&s_pool.buf_lock);
	self = poolmgr_alloc_object(s_pool.buf_pool);
	cspin_unlock(&s_pool.buf_lock);
	return self;
}

void bufpool_release_net_buf(void *self) {
	if (!self)
		return;

	cspin_lock(&s_pool.buf_lock);
	poolmgr_free_object(s_pool.buf_pool, self);
	cspin_unlock(&s_pool.buf_lock);
}

/* get buf pool memory info. */
size_t bufpool_get_memory_info(struct poolmgr_info *array, size_t num) {
	size_t index = 0;
	if (!array || num < 3)
		return 0;

	cspin_lock(&s_pool.big_lock);
	poolmgr_get_info(s_pool.big_block_pool, &array[index]);
	cspin_unlock(&s_pool.big_lock);
	++index;

	cspin_lock(&s_pool.small_lock);
	poolmgr_get_info(s_pool.small_block_pool, &array[index]);
	cspin_unlock(&s_pool.small_lock);
	++index;

	cspin_lock(&s_pool.buf_lock);
	poolmgr_get_info(s_pool.buf_pool, &array[index]);
	cspin_unlock(&s_pool.buf_lock);
	++index;

	return index;
}

