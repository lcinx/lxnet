
/*
 * Copyright (C) lcinx
 * lcinx@163.com
*/

#ifndef WIN32
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include "net_threadlist.h"
#include "threadpool.h"
#include "log.h"

#ifdef _DEBUG_NETWORK
#define debuglog log_writelog
#else
#define debuglog(...) ((void) 0)
#endif


#ifndef min
#define min(a, b) (((a) < (b))? (a) : (b))
#endif

struct threadmgr {
	struct threadlist suspendlist;	/* suspend thread list. */
	pthread_mutex_t setup_mutex;	/* Initialized using locks. */
	do_func task_f;					/* task function. */
	do_func lead_f;					/* leader function. */
	void *argv;						/* function parameters. */
	long threadnum;					/* thread num. */
	volatile long exit_num;			/* exit thread number. */
	volatile long has_leader;		/* if 0, not has leader, if 1, has leader. */
	volatile long activitynum;		/* activity thread number. */
};

struct tempinfo {
	struct threadmgr *mgr;
	struct threadinfo *th;
};

static inline void threadlist_resume_numthread(struct threadmgr *self, int num) {
	struct threadinfo *th;
	while (num > 0) {
		th = threadlist_popfront(&self->suspendlist);
		if (!th)
			break;
		if (pthread_mutex_trylock(&th->mutex) == 0) {
			atom_inc(&self->activitynum);
			thread_resume(th);
			pthread_mutex_unlock(&th->mutex);
		} else {
			threadlist_pushback(&self->suspendlist, th);
		}
		--num;
	}
}

static inline void threadlist_resume_allthread(struct threadmgr *self) {
	struct threadinfo *th;
	int num = 0;
	usleep(300000);
	for (;;) {
		th = threadlist_popfront(&self->suspendlist);
		if (!th)
			break;
		atom_inc(&self->activitynum);
		thread_resume(th);
		++num;
	}
}

/* called when the thread needs to exit. */
static inline void thread_do_exit(struct tempinfo *self) {
	/* if is leader. */
	if (thread_is_header(self->th)) {
		debuglog("[leader as exit] threadid:%d exit...", self->th->threadid);

		/* change to followers. */
		thread_change_to_henchman(self->th);

		/* change leader falg. */
		atom_set(&self->mgr->has_leader, 0);

		/* pop all the suspend thread that resume it. */
		threadlist_resume_allthread(self->mgr);
	}

	/* exit thread number increase 1. */
	atom_inc(&self->mgr->exit_num);

	atom_dec(&self->mgr->activitynum);

#ifdef _SHOW_EXIT_STATE
	printf("suspendlist thread num:%d, activitynum:%d, has_leader:%d, exit num:%d\n", threadlist_threadnum(&self->mgr->suspendlist), self->mgr->activitynum, self->mgr->has_leader, self->mgr->exit_num);
#endif

	/* release own. */
	thread_release(self->th);
	free(self->th);
}

static void *thread_proc(void *param) {
	int tasknum;
	int num;
	struct tempinfo info;
	memcpy(&info, param, sizeof(info));
	free(param);

	thread_set(info.th, CURRENT_THREAD);
	pthread_mutex_lock(&info.th->mutex);

	pthread_mutex_lock(&info.mgr->setup_mutex);
	debuglog("start thread:%x, argv thread hand:%x", CURRENT_THREAD, info.th->handle);
	pthread_mutex_unlock(&info.mgr->setup_mutex);
	for (;;) {
		/* if is leader. */
		if (thread_is_header(info.th)) {
			/* do leader function, 
			 * if return value less than 0, then exit. 
			 * if return value greater than 0, then is need resume thread num.
			 * if return value is equal 0, then overtime.
			 */
			tasknum = info.mgr->lead_f(info.mgr->argv);
			if (tasknum > 0) {
				debuglog("[new task list] leader:%d, tasknum:%d, suspendlist num:%d, activitynum:%d, has_leader:%d", info.th->threadid,\
						tasknum, threadlist_threadnum(&info.mgr->suspendlist),\
						info.mgr->activitynum, info.mgr->has_leader);
				/* resume thread to do the task. */

				/* change to followers. */
				thread_change_to_henchman(info.th);

				/* change leader falg. */
				atom_set(&info.mgr->has_leader, 0);

				/* tasknum--, because leader will also do task. */
				tasknum--;
				num = min(tasknum, threadlist_threadnum(&info.mgr->suspendlist));
				threadlist_resume_numthread(info.mgr, num);
			} else if (tasknum < 0) {
				debuglog("[thread pool exit] leader:%d exit... suspendlist num:%d, activitynum:%d, has_leader:%d", info.th->threadid, \
						threadlist_threadnum(&info.mgr->suspendlist), \
						info.mgr->activitynum, info.mgr->has_leader);
				
				thread_do_exit(&info);
				/* return. */
				return NULL;
			}

		} else {
			/* do task function, the return value is not equal to 0, then exit. */
			if (info.mgr->task_f(info.mgr->argv) != 0) {
				debuglog("[thread exit] threadi:%d exit", info.th->threadid);
				thread_do_exit(&info);
				/* return. */
				return NULL;
			}

			/* If own is the last activity of the followers, set own to leader. */
			if (atom_dec(&info.mgr->activitynum) == 0) {
				
				/* competition leader. */
				/* if old is 0, then set 1, return old value. */
				if (atom_compare_and_swap(&info.mgr->has_leader, 0, 1) == 0) {
					debuglog("[change to leader] threadid:%d, suspendlist num:%d, activitynum:%d, has_leader:%d", \
							info.th->threadid, threadlist_threadnum(&info.mgr->suspendlist), \
							info.mgr->activitynum, info.mgr->has_leader);

					/*  change own to leader. */
					thread_change_to_header(info.th);
					atom_inc(&info.mgr->activitynum);
				} else {
					debuglog("thread change leader failed:%x, activitynum:%ld, has_leader:%ld\n", CURRENT_THREAD, info.mgr->activitynum, info.mgr->has_leader);
					assert(false && "is last activitynum, but not change leader..., error!");
					log_error("is last activitynum, but not change leader..., error!");
				}

			} else {
				threadlist_pushback(&info.mgr->suspendlist, info.th);

				debuglog("[suspend thread] threadid:%d, self suspend. suspendlist num:%d, activitynum:%d, has_leader:%d", \
						info.th->threadid, threadlist_threadnum(&info.mgr->suspendlist), \
						info.mgr->activitynum, info.mgr->has_leader);

				/* suspend. */
				thread_suspend(info.th);
				debuglog("[for resume] threadid:%d", info.th->threadid);
			}
		}
	}
}

/* 
 * Create a thread pool that has threadnum threads, and suspend all.
 * tfunc --- is task function, if return 0, then not task to do. or else is to exit.
 * lfunc --- is leader function,  return need to resume threads num. if less than 0, then exit.
 * argv --- is function parameters, normally , is task manager pointer.
 */
struct threadmgr *threadmgr_create(int threadnum, do_func tfunc, do_func lfunc, void *argv) {
	struct threadmgr *mgr;
	int i;
	pthread_t thandle;
	struct threadinfo *tinfo;
	struct tempinfo *info;
	assert(threadnum > 0 && tfunc != NULL && lfunc != NULL && argv != NULL);
	if (threadnum <= 0 || !tfunc || !lfunc || !argv)
		return NULL;
	
	mgr = (struct threadmgr *)malloc(sizeof(struct threadmgr));
	if (!mgr)
		return NULL;

	/* initialize */
	threadlist_init(&mgr->suspendlist);
	pthread_mutex_init(&mgr->setup_mutex, NULL);
	mgr->task_f = tfunc;
	mgr->lead_f = lfunc;
	mgr->argv = argv;
	mgr->threadnum = threadnum;
	mgr->exit_num = 0;
	mgr->has_leader = 0;
	mgr->activitynum = threadnum;

	pthread_mutex_lock(&mgr->setup_mutex);

	/* create thread. */
	for (i = 0; i < threadnum; ++i) {
		tinfo = (struct threadinfo *)malloc(sizeof(struct threadinfo));
		if (!tinfo) {
			log_error("create threadinfo failed, function malloc return null!");
			mgr = NULL;
			goto end;
		}

		info = (struct tempinfo *)malloc(sizeof(struct tempinfo));
		if (!info) {
			log_error("create tempinfo failed, function malloc return null!");
			mgr = NULL;
			goto end;
		}
		info->mgr = mgr;
		info->th = tinfo;
		thread_init(tinfo);
		tinfo->threadid = i;
		if (pthread_create(&thandle, NULL, &thread_proc, (void *)info) != 0) {
			log_error("pthread_create create thread error!, errno:%d", errno);
			mgr = NULL;
			goto end;
		}
	}
end:
	pthread_mutex_unlock(&mgr->setup_mutex);
	debuglog("exit..");
	return mgr;
}


/* release thread pool. */
void threadmgr_release(struct threadmgr *self) {
	if (!self)
		return;
	/* pop all the suspend thread that resume it. */
	threadlist_resume_allthread(self);

	while (self->exit_num != self->threadnum) {
		assert(self->exit_num >= 0);
		assert(self->exit_num <= self->threadnum);
		usleep(100000);	/* 100 ms */
	}

#ifdef _SHOW_EXIT_STATE
	printf("thread num:%d, suspend num:%d, activitynum:%d, has_leader:%d, exit num:%d\n", self->threadnum, threadlist_threadnum(&self->suspendlist), self->activitynum, self->has_leader, self->exit_num);
#endif
	
	assert(self->activitynum == 0);
	assert(self->has_leader == 0);
	assert(self->exit_num == self->threadnum);

	threadlist_release(&self->suspendlist);
	pthread_mutex_destroy(&self->setup_mutex);
	free(self);
}

#endif

