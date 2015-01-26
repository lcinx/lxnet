
/*
 * Copyright (C) lcinx
 * lcinx@163.com
*/

#ifndef _H_THREAD_POOL_H_
#define _H_THREAD_POOL_H_
#ifndef WIN32

#ifdef __cplusplus
extern "C" {
#endif

struct threadmgr;
typedef int (*do_func) (void *argv);

/* 
 * Create a thread pool that has threadnum threads, and suspend all.
 * tfunc --- is task function, if return 0, then not task to do. or else is to exit.
 * lfunc --- is leader function,  return need to resume threads num. if less than 0, then exit.
 * argv --- is function parameters, normally , is task manager pointer.
 */
struct threadmgr *threadmgr_create(int threadnum, do_func tfunc, do_func lfunc, void *argv);


/* release thread pool. */
void threadmgr_release(struct threadmgr *self);

#ifdef __cplusplus
}
#endif

#endif	/*WIN32*/
#endif

