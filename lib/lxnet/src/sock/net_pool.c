
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#include <assert.h>
#include "net_pool.h"
#include "cthread.h"
#include "pool.h"

struct netpool {
	bool is_init;
	size_t socketer_num;
	size_t socketer_size;
	struct poolmgr *socketer_pool;
	cspin socketer_lock;

	size_t listener_num;
	size_t listener_size;
	struct poolmgr *listener_pool;
	cspin listener_lock;
};

static struct netpool s_netpool = {false};

/*
 * create and init net some pool.
 * socketer_num --- socketer object num.
 * socketer_size --- socketer object size.
 *
 * listener_num --- listener object num.
 * listener_size --- listener object size.
 */
bool netpool_init(size_t socketer_num, size_t socketer_size, size_t listener_num, size_t listener_size) {
	if (s_netpool.is_init)
		return false;

	if ((socketer_num == 0) || (socketer_size == 0) ||
		(listener_num == 0) || (listener_size == 0))
		return false;

	s_netpool.socketer_pool = poolmgr_create(socketer_size, 8, socketer_num, 1, "socketer pools");
	s_netpool.listener_pool = poolmgr_create(listener_size, 8, listener_num, 1, "listener pools");
	if (!s_netpool.socketer_pool || !s_netpool.listener_pool) {
		poolmgr_release(s_netpool.socketer_pool);
		poolmgr_release(s_netpool.listener_pool);
		return false;
	}

	cspin_init(&s_netpool.socketer_lock);
	cspin_init(&s_netpool.listener_lock);

	s_netpool.socketer_num = socketer_num;
	s_netpool.socketer_size = socketer_size;
	s_netpool.listener_num = listener_num;
	s_netpool.listener_size = listener_size;

	s_netpool.is_init = true;
	return true;
}

/* release net some pool. */
void netpool_release() {
	if (!s_netpool.is_init)
		return;

	cspin_lock(&s_netpool.socketer_lock);
	poolmgr_release(s_netpool.socketer_pool);
	s_netpool.socketer_pool = NULL;
	cspin_unlock(&s_netpool.socketer_lock);
	cspin_destroy(&s_netpool.socketer_lock);

	cspin_lock(&s_netpool.listener_lock);
	poolmgr_release(s_netpool.listener_pool);
	s_netpool.listener_pool = NULL;
	cspin_unlock(&s_netpool.listener_lock);
	cspin_destroy(&s_netpool.listener_lock);

	s_netpool.is_init = false;
}

void *netpool_create_socketer() {
	void *self;
	if (!s_netpool.is_init) {
		assert(false && "netpool_create_socketer not init!");
		return NULL;
	}

	cspin_lock(&s_netpool.socketer_lock);
	self = poolmgr_alloc_object(s_netpool.socketer_pool);
	cspin_unlock(&s_netpool.socketer_lock);
	return self;
}

void netpool_release_socketer(void *self) {
	if (!self)
		return;

	cspin_lock(&s_netpool.socketer_lock);
	poolmgr_free_object(s_netpool.socketer_pool, self);
	cspin_unlock(&s_netpool.socketer_lock);
}

void *netpool_create_listener() {
	void *self;
	if (!s_netpool.is_init) {
		assert(false && "netpool_create_listener not init!");
		return NULL;
	}

	cspin_lock(&s_netpool.listener_lock);
	self = poolmgr_alloc_object(s_netpool.listener_pool);
	cspin_unlock(&s_netpool.listener_lock);
	return self;
}

void netpool_release_listener(void *self) {
	if (!self)
		return;

	cspin_lock(&s_netpool.listener_lock);
	poolmgr_free_object(s_netpool.listener_pool, self);
	cspin_unlock(&s_netpool.listener_lock);
}

/* get net some pool info. */
size_t netpool_get_memory_info(struct poolmgr_info *array, size_t num) {
	size_t index = 0;
	if (!array || num < 2)
		return 0;

	cspin_lock(&s_netpool.socketer_lock);
	poolmgr_get_info(s_netpool.socketer_pool, &array[index]);
	cspin_unlock(&s_netpool.socketer_lock);
	++index;

	cspin_lock(&s_netpool.listener_lock);
	poolmgr_get_info(s_netpool.listener_pool, &array[index]);
	cspin_unlock(&s_netpool.listener_lock);
	++index;

	return index;
}

