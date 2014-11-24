
/*
 * Copyright (C) lcinx
 * lcinx@163.com
*/

#include <assert.h>
#include <string.h>
#include "ossome.h"
#include "pool.h"
#include "net_pool.h"

struct netpool
{
	bool isinit;
	size_t socket_num;
	size_t socket_size;
	struct poolmgr *socket_pool;
	LOCK_struct socket_lock;

	size_t listen_num;
	size_t listen_size;
	struct poolmgr *listen_pool;
	LOCK_struct listen_lock;
};
static struct netpool s_netpool = {false};

/* 
 * create and init net some pool.
 * socketnum --- socket object num.
 * socketsize --- socket object size.
 *
 * listennum --- listen object num.
 * listensize --- listen object size.
 * */
bool netpool_init (size_t socketnum, size_t socketsize, size_t listennum, size_t listensize)
{
	if (s_netpool.isinit)
		return false;
	if ((socketnum == 0) || (socketsize == 0) ||
		(listennum == 0) || (listensize == 0))
		return false;
	s_netpool.socket_pool = poolmgr_create(socketsize, 8, socketnum, 1, "socket_pools");
	s_netpool.listen_pool = poolmgr_create(listensize, 8, listennum, 1, "listen_pools");
	if (!s_netpool.socket_pool || !s_netpool.listen_pool)
	{
		poolmgr_release(s_netpool.socket_pool);
		poolmgr_release(s_netpool.listen_pool);
		return false;
	}
	LOCK_INIT(&s_netpool.socket_lock);
	LOCK_INIT(&s_netpool.listen_lock);

	s_netpool.socket_num = socketnum;
	s_netpool.socket_size = socketsize;
	s_netpool.listen_num = listennum;
	s_netpool.listen_size = listensize;

	s_netpool.isinit = true;
	return true;
}

/* release net some pool. */
void netpool_release ()
{
	if (!s_netpool.isinit)
		return;
	LOCK_LOCK(&s_netpool.socket_lock);
	poolmgr_release(s_netpool.socket_pool);
	s_netpool.socket_pool = NULL;
	LOCK_UNLOCK(&s_netpool.socket_lock);
	LOCK_DELETE(&s_netpool.socket_lock);

	LOCK_LOCK(&s_netpool.listen_lock);
	poolmgr_release(s_netpool.listen_pool);
	s_netpool.listen_pool = NULL;
	LOCK_UNLOCK(&s_netpool.listen_lock);
	LOCK_DELETE(&s_netpool.listen_lock);

	s_netpool.isinit = false;
}

void *netpool_createsocket ()
{
	void *self;
	if (!s_netpool.isinit)
	{
		assert(false && "netpool_createsocket not init!");
		return NULL;
	}
	LOCK_LOCK(&s_netpool.socket_lock);
	self = poolmgr_getobject(s_netpool.socket_pool);
	LOCK_UNLOCK(&s_netpool.socket_lock);
	return self;
}

void netpool_releasesocket (void *self)
{
	if (!self)
		return;
	LOCK_LOCK(&s_netpool.socket_lock);
	poolmgr_freeobject(s_netpool.socket_pool, self);
	LOCK_UNLOCK(&s_netpool.socket_lock);
}

void *netpool_createlisten ()
{
	void *self;
	if (!s_netpool.isinit)
	{
		assert(false && "netpool_createlisten not init!");
		return NULL;
	}
	LOCK_LOCK(&s_netpool.listen_lock);
	self = poolmgr_getobject(s_netpool.listen_pool);
	LOCK_UNLOCK(&s_netpool.listen_lock);
	return self;
}

void netpool_releaselisten (void *self)
{
	if (!self)
		return;
	LOCK_LOCK(&s_netpool.listen_lock);
	poolmgr_freeobject(s_netpool.listen_pool, self);
	LOCK_UNLOCK(&s_netpool.listen_lock);
}

/* get net some pool info. */
void netpool_meminfo (char *buf, size_t bufsize)
{
	size_t index = 0;
	LOCK_LOCK(&s_netpool.socket_lock);
	poolmgr_getinfo(s_netpool.socket_pool, buf, bufsize-1);
	LOCK_UNLOCK(&s_netpool.socket_lock);

	index = strlen(buf);

	LOCK_LOCK(&s_netpool.listen_lock);
	poolmgr_getinfo(s_netpool.listen_pool, &buf[index], bufsize-1-index);
	LOCK_UNLOCK(&s_netpool.listen_lock);

	buf[bufsize-1] = 0;
}

