
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#ifndef _H_CROSS_THREAD_POOL_H_
#define _H_CROSS_THREAD_POOL_H_

#ifdef __cplusplus
extern "C" {
#endif


struct cthread_pool;

/*
 * create a thread pool, that has thread_num threads.
 * @param {int} thread_num			thread num.
 * @param {void *} udata			user data pointer.
 * @param {function} func_leader	leader function, need the one param. 
 *										return need to resume threads num, if less than 0, then exit.
 * @param {function} func_task		task function, need the one param.
 *										return 0 then not task to do. or else is to exit.
 */
struct cthread_pool *cthread_pool_create(int thread_num, void *udata, 
		int (*func_leader)(void *), int (*func_task)(void *));

void cthread_pool_release(struct cthread_pool *self);

#ifdef __cplusplus
}
#endif
#endif

