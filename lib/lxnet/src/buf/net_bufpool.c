
/*
 * Copyright (C) lcinx
 * lcinx@163.com
*/

#include <string.h>
#include <assert.h>
#include "ossome.h"
#include "pool.h"
#include "net_bufpool.h"

struct bufpool {
	bool isinit;

	size_t bigpoolnum;
	size_t bigpoolsize;
	struct poolmgr *bigblockpool;
	spin_lock_struct big_lock;

	size_t smallpoolnum;
	size_t smallpoolsize;
	struct poolmgr *smallblockpool;
	spin_lock_struct small_lock;

	size_t bufnum;
	size_t bufsize;
	struct poolmgr *buf_pool;
	spin_lock_struct buf_lock;
};
static struct bufpool s_pool = {false};

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
 * */
bool bufpool_init(size_t bigblocknum, size_t bigblocksize, size_t smallblocknum, size_t smallblocksize, size_t bufnum, size_t bufsize) {
	if (s_pool.isinit)
		return false;
	if ((bigblocknum == 0) || (bigblocksize == 0) ||
		(smallblocknum == 0) || (smallblocksize == 0) ||
		(bufnum == 0) || (bufsize == 0))
		return false;
	s_pool.bigblockpool = poolmgr_create(bigblocksize, 8, bigblocknum, 1, "big_block_pools");
	s_pool.smallblockpool = poolmgr_create(smallblocksize, 8, smallblocknum, 1, "small_block_pools");
	s_pool.buf_pool = poolmgr_create(bufsize, 8, bufnum, 1, "bufpools");
	if (!s_pool.bigblockpool || !s_pool.smallblockpool || !s_pool.buf_pool) {
		poolmgr_release(s_pool.bigblockpool);
		poolmgr_release(s_pool.smallblockpool);
		poolmgr_release(s_pool.buf_pool);
		return false;
	}

	spin_lock_init(&s_pool.big_lock);
	spin_lock_init(&s_pool.small_lock);
	spin_lock_init(&s_pool.buf_lock);
	
	s_pool.bigpoolnum = bigblocknum;
	s_pool.bigpoolsize = bigblocksize;

	s_pool.smallpoolnum = smallblocknum;
	s_pool.smallpoolsize = smallblocksize;

	s_pool.bufnum = bufnum;
	s_pool.bufsize = bufsize;

	s_pool.isinit = true;
	return true;
}

/* release buf pool. */
void bufpool_release() {
	if (!s_pool.isinit)
		return;
	spin_lock_lock(&s_pool.big_lock);
	poolmgr_release(s_pool.bigblockpool);
	s_pool.bigblockpool = NULL;
	spin_lock_unlock(&s_pool.big_lock);
	spin_lock_delete(&s_pool.big_lock);

	spin_lock_lock(&s_pool.small_lock);
	poolmgr_release(s_pool.smallblockpool);
	s_pool.smallblockpool = NULL;
	spin_lock_unlock(&s_pool.small_lock);
	spin_lock_delete(&s_pool.small_lock);

	spin_lock_lock(&s_pool.buf_lock);
	poolmgr_release(s_pool.buf_pool);
	s_pool.buf_pool = NULL;
	spin_lock_unlock(&s_pool.buf_lock);
	spin_lock_delete(&s_pool.buf_lock);
	
	s_pool.isinit = false;
}

void *bufpool_createbigblock() {
	void *self = NULL;
	if (!s_pool.isinit)
		return NULL;
	spin_lock_lock(&s_pool.big_lock);
	self = poolmgr_getobject(s_pool.bigblockpool);
	spin_lock_unlock(&s_pool.big_lock);
	return self;
}

void bufpool_releasebigblock(void *self) {
	if (!self)
		return;
	spin_lock_lock(&s_pool.big_lock);
	poolmgr_freeobject(s_pool.bigblockpool, self);
	spin_lock_unlock(&s_pool.big_lock);
}

void *bufpool_createsmallblock() {
	void *self = NULL;
	if (!s_pool.isinit)
		return NULL;
	spin_lock_lock(&s_pool.small_lock);
	self = poolmgr_getobject(s_pool.smallblockpool);
	spin_lock_unlock(&s_pool.small_lock);
	return self;
}

void bufpool_releasesmallblock(void *self) {
	if (!self)
		return;
	spin_lock_lock(&s_pool.small_lock);
	poolmgr_freeobject(s_pool.smallblockpool, self);
	spin_lock_unlock(&s_pool.small_lock);
}

void *bufpool_createbuf() {
	void *self = NULL;
	if (!s_pool.isinit)
		return NULL;
	spin_lock_lock(&s_pool.buf_lock);
	self = poolmgr_getobject(s_pool.buf_pool);
	spin_lock_unlock(&s_pool.buf_lock);
	return self;
}

void bufpool_releasebuf(void *self) {
	if (!self)
		return;
	spin_lock_lock(&s_pool.buf_lock);
	poolmgr_freeobject(s_pool.buf_pool, self);
	spin_lock_unlock(&s_pool.buf_lock);
}

/* get buf pool memory info. */
void bufpool_meminfo(char *buf, size_t bufsize) {
	size_t index = 0;
	spin_lock_lock(&s_pool.big_lock);
	poolmgr_getinfo(s_pool.bigblockpool, buf, bufsize-1);
	spin_lock_unlock(&s_pool.big_lock);

	index = strlen(buf);

	spin_lock_lock(&s_pool.small_lock);
	poolmgr_getinfo(s_pool.smallblockpool, &buf[index], bufsize-1-index);
	spin_lock_unlock(&s_pool.small_lock);

	index = strlen(buf);

	spin_lock_lock(&s_pool.buf_lock);
	poolmgr_getinfo(s_pool.buf_pool, &buf[index], bufsize-1-index);
	spin_lock_unlock(&s_pool.buf_lock);

	buf[bufsize-1] = 0;
}



