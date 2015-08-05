
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "cthread_pool.h"
#include "cthread.h"
#include "catomic.h"
#include "platform_config.h"
#include "log.h"

#ifdef _USE_THREAD_POOL_DEBUG_
#define threadpool_debuglog			log_writelog
#else
#define threadpool_debuglog(...)	((void) 0)
#endif


#ifndef min
#define min(a, b)	(((a) < (b)) ? (a) : (b))
#endif

enum {
	ecState_None = 0,
	ecState_Activity,
	ecState_Suspend,
	ecState_Exit,
};

struct cthread_info {
	cthread handle;
	int state;
	bool is_leader;

	struct cthread_info *next;

	struct cthread_pool *mgr;
};

struct cthread_list {
	struct cthread_info *head;
};

struct cthread_pool {
	struct cthread_list all_list;

	catomic run;
	catomic resume_num;
	catomic suspend_num;
	catomic activity_num;
	catomic need_exit_num;
	catomic exit_num;
	catomic has_leader;

	int thread_num;
	void *udata;
	int (*func_leader)(void *);
	int (*func_task)(void *);
};


/*
 * ================================================================================
 * cthread_info method. 
 * ================================================================================
 */
static struct cthread_info *cthread_info_create(struct cthread_pool *mgr, void (*func)(cthread *)) {
	struct cthread_info *self = (struct cthread_info *)malloc(sizeof(struct cthread_info));
	if (!self)
		return NULL;

	self->handle = NULL;
	self->is_leader = false;
	self->state = ecState_None;
	self->next = NULL;

	self->mgr = mgr;

	if (cthread_create(&self->handle, self, func) != 0) {
		free(self);
		return NULL;
	}

	return self;
}

static void cthread_info_release(struct cthread_info *self) {
	if (!self)
		return;

	if (self->handle) {
		cthread_release(&self->handle);
	}

	self->mgr = NULL;
	free(self);
}

static int cthread_info_get_id(struct cthread_info *self) {
	return cthread_thread_id(&self->handle);
}

static cthread *cthread_info_get_handle_ptr(struct cthread_info *self) {
	return &self->handle;
}

static bool cthread_info_is_header(struct cthread_info *self) {
	return self->is_leader;
}

static void cthread_info_change_to_header(struct cthread_info *self) {
	self->is_leader = true;
}

static void cthread_info_change_to_henchman(struct cthread_info *self) {
	self->is_leader = false;
}

static void cthread_info_state_to_activity(struct cthread_info *self) {
	assert(self->state == ecState_None || self->state == ecState_Suspend);
	self->state = ecState_Activity;
}

static void cthread_info_state_to_suspend(struct cthread_info *self) {
	assert(self->state == ecState_Activity);
	self->state = ecState_Suspend;
}

static void cthread_info_state_to_exit(struct cthread_info *self) {
	assert(self->state == ecState_Activity);
	self->state = ecState_Exit;
}

static bool cthread_info_state_is_exit(struct cthread_info *self) {
	return self->state == ecState_Exit;
}


/*
 * ================================================================================
 * cthread_list method.
 * ================================================================================
 */
static void cthread_list_init(struct cthread_list *self) {
	self->head = NULL;
}

static void cthread_list_push_back(struct cthread_list *self, struct cthread_info *th) {
	th->next = self->head;
	self->head = th;
}

static void cthread_list_destroy(struct cthread_list *self) {
	struct cthread_info *node, *cur;
	for (node = self->head; node;) {
		cur = node;
		node = node->next;
		cthread_info_release(cur);
	}

	self->head = NULL;
}


/*
 * ================================================================================
 * cthread_pool method.
 * ================================================================================
 */
static void cthread_pool_resume_some_thread(struct cthread_pool *self, 
		int num, struct cthread_info *skip) {

	struct cthread_info *node;
	for (node = self->all_list.head; node; node = node->next) {
		if (num <= 0)
			break;

		if (node != skip && (!cthread_info_state_is_exit(node))) {
			cthread_resume(cthread_info_get_handle_ptr(node));

			threadpool_debuglog("func:[%s] thread id:%d", 
				__FUNCTION__, cthread_info_get_id(node));
			--num;
		}
	}
}

static void cthread_pool_do_thread_exit(struct cthread_info *th) {
	catomic_dec(&th->mgr->activity_num);

	if (cthread_info_is_header(th)) {

		threadpool_debuglog("func:[%s][leader as exit] thread id:%d", 
				__FUNCTION__, cthread_info_get_id(th));

		cthread_pool_resume_some_thread(th->mgr, th->mgr->thread_num, NULL);
	}

	cthread_info_state_to_exit(th);
	catomic_inc(&th->mgr->exit_num);

	threadpool_debuglog("func:[%s] thread id:%d, activity num:%d, exit num:%d, has_leader:%d",
			__FUNCTION__, cthread_info_get_id(th),
			(int)catomic_read(&th->mgr->activity_num), (int)catomic_read(&th->mgr->exit_num), 
			(int)catomic_read(&th->mgr->has_leader));
}


static void th_pro_func(cthread *th) {
	int cinfo_id = 0;
	struct cthread_info *cinfo = (struct cthread_info *)cthread_get_udata(th);
	struct cthread_pool *mgr = cinfo->mgr;

	/* first suspend. */
	cthread_suspend(cthread_info_get_handle_ptr(cinfo));

	/* check need run. */
	if (catomic_read(&mgr->run) == 0)
		return;

	cinfo_id = (int)cthread_info_get_id(cinfo);
	(void )cinfo_id;

	cthread_info_state_to_activity(cinfo);
	catomic_inc(&mgr->resume_num);
	catomic_inc(&mgr->activity_num);
	catomic_inc(&mgr->need_exit_num);

	/* wait all run to here. */
	while (catomic_read(&mgr->need_exit_num) != (int64)mgr->thread_num) {
		cthread_self_sleep(0);
	}

	threadpool_debuglog("func:[%s][start thread] id:%d", 
			__FUNCTION__, cthread_info_get_id(cinfo));

	while (catomic_read(&mgr->run) != 0) {
		if (cthread_info_is_header(cinfo)) {
			/*
			 * do leader function, 
			 * if return value less than 0, then exit. 
			 * if return value greater than 0, then is need resume thread num.
			 */
			int resume_num = mgr->func_leader(mgr->udata);
			if (resume_num > 0) {
				int real_resume_num = (int)min(resume_num, catomic_read(&mgr->need_exit_num));

				threadpool_debuglog("func:[%s][leader func return] leader thread id:%d, "
						"resume_num:%d, activity num:%d, exit num:%d, has_leader:%d",
						__FUNCTION__, cthread_info_get_id(cinfo), resume_num, 
						(int)catomic_read(&mgr->activity_num), (int)catomic_read(&mgr->exit_num), 
						(int)catomic_read(&mgr->has_leader));

				cthread_info_change_to_henchman(cinfo);

				catomic_set(&mgr->has_leader, 0);
				catomic_set(&mgr->resume_num, real_resume_num);

				cthread_pool_resume_some_thread(mgr, real_resume_num - 1, cinfo);
			} else if (resume_num < 0) {
				break;
			}

		} else {

			/*
			 * do task function,
			 * the return value is not equal to 0, then exit.
			 */
			if (mgr->func_task(mgr->udata) != 0)
				break;

			/* If own is the last activity of the followers, set own to leader. */
			if (catomic_dec(&mgr->resume_num) == 0) {

				assert(catomic_read(&mgr->activity_num) >= 1);
				assert(catomic_read(&mgr->has_leader) == 0);

				/* competition leader. */
				if (catomic_compare_set(&mgr->has_leader, 0, 1)) {

					/* change own to leader. */
					cthread_info_change_to_header(cinfo);

					threadpool_debuglog("func:[%s][change to leader] task thread id:%d, "
							"activity num:%d, exit num:%d, has_leader:%d",
							__FUNCTION__, cthread_info_get_id(cinfo),
							(int)catomic_read(&mgr->activity_num), (int)catomic_read(&mgr->exit_num), 
							(int)catomic_read(&mgr->has_leader));

					continue;
				}

				assert(false && "is last activity thread, but not change to leader, error!");
				log_error("is last activity thread, but not change to leader, error!");

			}

			/* suspend. */
			cthread_info_state_to_suspend(cinfo);
			catomic_dec(&mgr->activity_num);
			catomic_inc(&mgr->suspend_num);

			threadpool_debuglog("func:[%s][suspend thread] thread id:%d, activity num:%d, "
					"exit num:%d, has_leader:%d", __FUNCTION__, cthread_info_get_id(cinfo),
					(int)catomic_read(&mgr->activity_num), (int)catomic_read(&mgr->exit_num), 
					(int)catomic_read(&mgr->has_leader));

			/* real do suspend. */
			cthread_suspend(cthread_info_get_handle_ptr(cinfo));

			/* from resume. */
			cthread_info_state_to_activity(cinfo);
			catomic_dec(&mgr->suspend_num);
			catomic_inc(&mgr->activity_num);

			threadpool_debuglog("func:[%s][thread for resume] thread id:%d, activity num:%d, "
					"exit num:%d, has_leader:%d", __FUNCTION__,	cthread_info_get_id(cinfo),
					(int)catomic_read(&mgr->activity_num), (int)catomic_read(&mgr->exit_num), 
					(int)catomic_read(&mgr->has_leader));
		}
	}

	cthread_pool_do_thread_exit(cinfo);
}



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
		int (*func_leader)(void *), int (*func_task)(void *)) {

	struct cthread_pool *self;
	assert(thread_num > 0 && func_leader != NULL && func_task != NULL);
	if (thread_num <= 0 || !func_leader || !func_task)
		return NULL;

	self = (struct cthread_pool *)malloc(sizeof(struct cthread_pool));
	if (!self)
		return NULL;

	cthread_list_init(&self->all_list);
	catomic_set(&self->run, 0);
	catomic_set(&self->resume_num, 0);
	catomic_set(&self->suspend_num, 0);
	catomic_set(&self->activity_num, 0);
	catomic_set(&self->need_exit_num, 0);
	catomic_set(&self->exit_num, 0);
	catomic_set(&self->has_leader, 0);

	self->thread_num = thread_num;
	self->udata = udata;
	self->func_leader = func_leader;
	self->func_task = func_task;

	while (thread_num > 0) {
		struct cthread_info *cinfo = cthread_info_create(self, th_pro_func);
		if (!cinfo)
			goto err_do;

		cthread_list_push_back(&self->all_list, cinfo);

		thread_num--;
	}

	catomic_set(&self->run, 1);

	threadpool_debuglog("func[%s] mgr:%p", __FUNCTION__, self);

	/* resume all. */
	cthread_pool_resume_some_thread(self, self->thread_num, NULL);

	/* wait thread run. */
	while (catomic_read(&self->need_exit_num) != (int64)self->thread_num) {
		cthread_self_sleep(0);
	}

	return self;

err_do:
	if (self) {
		cthread_list_destroy(&self->all_list);
		free(self);
	}

	return NULL;
}

void cthread_pool_release(struct cthread_pool *self) {
	if (!self)
		return;

	threadpool_debuglog("func[%s] mgr:%p", __FUNCTION__, self);

	catomic_set(&self->run, 0);

	while (catomic_read(&self->exit_num) != catomic_read(&self->need_exit_num)) {

		cthread_pool_resume_some_thread(self, self->thread_num, NULL);

		/* sleep 1 ms. */
		cthread_self_sleep(1);
	}

	assert(catomic_read(&self->suspend_num) == 0);
	assert(catomic_read(&self->activity_num) == 0);
	assert(catomic_read(&self->need_exit_num) == catomic_read(&self->exit_num));
	assert(catomic_read(&self->need_exit_num) <= self->thread_num);

	cthread_list_destroy(&self->all_list);

	free(self);
}

