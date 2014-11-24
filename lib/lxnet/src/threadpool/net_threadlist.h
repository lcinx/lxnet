
/*
 * Copyright (C) lcinx
 * lcinx@163.com
*/

#ifndef _H_NET_THREADLIST_H_
#define _H_NET_THREADLIST_H_

#ifdef __cplusplus
extern "C"{
#endif
#ifndef WIN32
#include "net_thread.h"
#include "ossome.h"
#include "alway_inline.h"

struct threadlist
{
	struct threadinfo *head;
	struct threadinfo *tail;
	LOCK_struct listlock;
	volatile long threadnum;
};

static inline void threadlist_init (struct threadlist *self)
{
	self->head = NULL;
	self->tail = NULL;
	LOCK_INIT(&self->listlock);
	self->threadnum = 0;
}

static inline void threadlist_release (struct threadlist *self)
{
	self->head = NULL;
	self->tail = NULL;
	LOCK_DELETE(&self->listlock);
}

static inline int threadlist_threadnum (struct threadlist *self)
{
	return self->threadnum;
}

static inline struct threadinfo *threadlist_popfront (struct threadlist *self)
{
	struct threadinfo *th;
	LOCK_LOCK(&self->listlock);
	th = self->head;
	if (self->head)
	{
		self->head = self->head->next;
		if (th == self->tail)
		{
			self->tail = NULL;
			assert(self->head == NULL);
		}
	}
	LOCK_UNLOCK(&self->listlock);
	if (th)
	{
		atom_dec(&self->threadnum);
		th->next = NULL;
	}
	return th;
}

static inline void threadlist_pushback (struct threadlist *self, struct threadinfo *th)
{
	LOCK_LOCK(&self->listlock);
	th->next = NULL;
	if (self->tail)
	{
		self->tail->next = th;
	}
	else
	{
		assert(self->head == NULL);
		self->head = th;
	}
	self->tail = th;
	LOCK_UNLOCK(&self->listlock);
	atom_inc(&self->threadnum);
}

#endif
#ifdef __cplusplus
}
#endif
#endif

