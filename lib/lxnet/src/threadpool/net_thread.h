
/*
 * Copyright (C) lcinx
 * lcinx@163.com
*/

#ifndef _H_NET_THREAD_H_
#define _H_NET_THREAD_H_

#ifdef __cplusplus
extern "C" {
#endif
#ifndef WIN32
#include <pthread.h>
#include "platform_config.h"

struct threadinfo {
	pthread_t handle;					/* thread handle. */
	pthread_cond_t cond;				/* condition variable. */
	pthread_mutex_t mutex;				/* mutex lock. */
	bool isheader;						/* leader flag. */
	int threadid;
	struct threadinfo *next;
};
/* initialize. */
static inline void thread_init(struct threadinfo *self) {
	pthread_mutex_init(&self->mutex, NULL);
	pthread_cond_init(&self->cond, NULL);
	self->isheader = false;
	self->next = NULL;
}

/* set thread handle. */
static inline void thread_set(struct threadinfo *self, pthread_t h) {
	self->handle = h;
}

/* is leader? */
static inline bool thread_is_header(struct threadinfo *self) {
	return self->isheader;
}

/* change to leader. */
static inline void thread_change_to_header(struct threadinfo *self) {
	self->isheader = true;
}

/* change to follower. */
static inline void thread_change_to_henchman(struct threadinfo *self) {
	self->isheader = false;
}

/* release. */
static inline void thread_release(struct threadinfo *self) {
	pthread_mutex_destroy(&self->mutex);
	pthread_cond_destroy(&self->cond);
	self->next = NULL;
}

/* suspend. */
static inline void thread_suspend(struct threadinfo *self) {
	pthread_cond_wait(&self->cond, &self->mutex);
}

/* resume. */
static inline void thread_resume(struct threadinfo *self) {
	pthread_cond_signal(&self->cond);
}

static inline void thread_join(struct threadinfo *self) {
	pthread_join(self->handle, NULL);
}

#endif
#ifdef __cplusplus
}
#endif
#endif

