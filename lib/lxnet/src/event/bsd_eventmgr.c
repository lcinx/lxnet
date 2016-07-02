
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include "socket_internal.h"
#include "_netsocket.h"
#include "cthread.h"
#include "crosslib.h"
#include "cthread_pool.h"
#include "log.h"

#ifdef _DEBUG_NETWORK
#define debuglog debug_print_call
#else
#define debuglog(...) ((void) 0)
#endif

/* max events from kevent function. */
#define THREAD_EVENT_SIZE (4096)
struct kqueuemgr {
	int thread_num;
	struct cthread_pool *thread_pool;				/* thread pool. */
	volatile char need_exit;						/* exit flag. */

	catomic event_num;								/* current event number. */
	int kqueue_fd;									/* kqueue handle. */
	struct kevent ev_array[THREAD_EVENT_SIZE];		/* event array. */
};

static struct kqueuemgr *s_mgr = NULL;

/* add socket to event manager. */
void eventmgr_add_socket(struct socketer *self) {
	bool res = true;
	struct kevent ev;
	EV_SET(&ev, self->sockfd, EVFILT_READ, EV_ADD | EV_DISABLE, 0, 0, self);
	if (kevent(s_mgr->kqueue_fd, &ev, 1, NULL, 0, NULL) == -1) {
		res = false;
	}

	EV_SET(&ev, self->sockfd, EVFILT_WRITE, EV_ADD | EV_DISABLE, 0, 0, self);
	if (kevent(s_mgr->kqueue_fd, &ev, 1, NULL, 0, NULL) == -1) {
		res = false;
	}

	if (!res) {
		socketer_close(self);
	}

	debuglog("add read、write event to eventmgr.");
}

/* remove socket from event manager. */
void eventmgr_remove_socket(struct socketer *self) {
	struct kevent ev;
	EV_SET(&ev, self->sockfd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
	kevent(s_mgr->kqueue_fd, &ev, 1, NULL, 0, NULL);
	EV_SET(&ev, self->sockfd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
	kevent(s_mgr->kqueue_fd, &ev, 1, NULL, 0, NULL);

	debuglog("remove all event from eventmgr.");
}

/* set recv event. */
void eventmgr_setup_socket_recv_event(struct socketer *self) {
	struct kevent ev;

	assert(catomic_read(&self->recvlock) == 1);

	if (catomic_read(&self->recvlock) != 1) {
		log_error("%x socket recvlock:%d, sendlock:%d, fd:%d, ref:%d, thread_id:%d", 
				self, (int)catomic_read(&self->recvlock), (int)catomic_read(&self->sendlock), 
				self->sockfd, (int)catomic_read(&self->ref), cthread_self_id());
	}


	EV_SET(&ev, self->sockfd, EVFILT_READ, EV_ENABLE, 0, 0, self);
	if (kevent(s_mgr->kqueue_fd, &ev, 1, NULL, 0, NULL) == -1) {
		/*log_error("kqueue, setup recv event to kqueue set on fd %d error!, errno:%d", self->sockfd, NET_GetLastError());*/
		socketer_close(self);
		if (catomic_dec(&self->ref) < 1) {
			log_error("%x socket recvlock:%d, sendlock:%d, fd:%d, ref:%d, thread_id:%d, connect:%d, deleted:%d", 
					self, (int)catomic_read(&self->recvlock), (int)catomic_read(&self->sendlock), self->sockfd, 
					(int)catomic_read(&self->ref), cthread_self_id(), self->connected, self->deleted);
		}
	}
	debuglog("setup recv event to eventmgr.");
}

/* remove recv event. */
void eventmgr_remove_socket_recv_event(struct socketer *self) {
	struct kevent ev;

	assert(catomic_read(&self->recvlock) == 1);

	if (catomic_read(&self->recvlock) != 1) {
		log_error("%x socket recvlock:%d, sendlock:%d, fd:%d, ref:%d, thread_id:%d", 
				self, (int)catomic_read(&self->recvlock), (int)catomic_read(&self->sendlock), 
				self->sockfd, (int)catomic_read(&self->ref), cthread_self_id());
	}

	EV_SET(&ev, self->sockfd, EVFILT_READ, EV_DISABLE, 0, 0, self);
	if (kevent(s_mgr->kqueue_fd, &ev, 1, NULL, 0, NULL) == -1) {
		/*log_error("kqueue, remove recv event from kqueue set on fd %d error!, errno:%d", self->sockfd, NET_GetLastError());*/
		socketer_close(self);
	}
	debuglog("remove recv event from eventmgr.");
}

/* set send event. */
void eventmgr_setup_socket_send_event(struct socketer *self) {
	struct kevent ev;

	assert(catomic_read(&self->sendlock) == 1);
	if (catomic_read(&self->sendlock) != 1) {
		log_error("%x socket recvlock:%d, sendlock:%d, fd:%d, ref:%d, thread_id:%d", 
				self, (int)catomic_read(&self->recvlock), (int)catomic_read(&self->sendlock), 
				self->sockfd, (int)catomic_read(&self->ref), cthread_self_id());
	}

	EV_SET(&ev, self->sockfd, EVFILT_WRITE, EV_ENABLE, 0, 0, self);
	if (kevent(s_mgr->kqueue_fd, &ev, 1, NULL, 0, NULL) == -1) {
		/*log_error("kqueue, setup send event to kqueue set on fd %d error!, errno:%d", self->sockfd, NET_GetLastError());*/
		socketer_close(self);
		if (catomic_dec(&self->ref) < 1) {
			log_error("%x socket recvlock:%d, sendlock:%d, fd:%d, ref:%d, thread_id:%d, connect:%d, deleted:%d", 
					self, (int)catomic_read(&self->recvlock), (int)catomic_read(&self->sendlock), self->sockfd, 
					(int)catomic_read(&self->ref), cthread_self_id(), self->connected, self->deleted);
		}
	}
	debuglog("setup send event to eventmgr.");
}

/* remove send event. */
void eventmgr_remove_socket_send_event(struct socketer *self) {
	struct kevent ev;

	assert(catomic_read(&self->sendlock) == 1);
	if (catomic_read(&self->sendlock) != 1) {
		log_error("%x socket recvlock:%d, sendlock:%d, fd:%d, ref:%d, thread_id:%d", 
				self, (int)catomic_read(&self->recvlock), (int)catomic_read(&self->sendlock), 
				self->sockfd, (int)catomic_read(&self->ref), cthread_self_id());
	}

	EV_SET(&ev, self->sockfd, EVFILT_WRITE, EV_DISABLE, 0, 0, self);
	if (kevent(s_mgr->kqueue_fd, &ev, 1, NULL, 0, NULL) == -1) {
		/*log_error("kqueue, remove send event from kqueue set on fd %d error!, errno:%d", self->sockfd, NET_GetLastError());*/
		socketer_close(self);
	}
	debuglog("remove send event from eventmgr.");
}

static struct kevent *pop_event(struct kqueuemgr *self) {
	int index = (int)catomic_dec(&self->event_num);
	if (index < 0)
		return NULL;
	else
		return &self->ev_array[index];
}

/* execute the task callback function. */
static int task_func(void *argv) {
	struct kqueuemgr *mgr = (struct kqueuemgr *)argv;
	struct kevent *ev;
	struct socketer *sock;
	for (;;) {
		if (mgr->need_exit)
			return -1;
		ev = pop_event(mgr);
		if (!ev)
			return 0;
		assert(ev->udata != NULL);
		sock = (struct socketer *)ev->udata;

		/* error event. */
		if (ev->flags & EV_EOF || ev->flags & EV_ERROR) {
			socketer_close(sock);
			continue;
		}

		if (ev->filter == EVFILT_READ) {
			/* can read event. */
			if (catomic_compare_set(&sock->recvlock, 0, 1)) {
				catomic_inc(&sock->ref);
			}
			socketer_on_recv(sock, 0);
		} else 	if (ev->filter == EVFILT_WRITE) {
			/* can write event. */
			if (catomic_compare_set(&sock->sendlock, 0, 1)) {
				catomic_inc(&sock->ref);
			}
			socketer_on_send(sock, 0);
		}
	}
}

#define EVERY_THREAD_PROCESS_EVENT_NUM 8
/* get need resume thread number. */
static int leader_func(void *argv) {
	struct kqueuemgr *mgr = (struct kqueuemgr *)argv;

	/* wait event. */
	if (mgr->need_exit) {
		return -1;
	} else {
		struct timespec timeout;
		timeout.tv_sec = 0;
		timeout.tv_nsec = 50 * 1000000;
		int num = kevent(mgr->kqueue_fd, NULL, 0, mgr->ev_array, THREAD_EVENT_SIZE, &timeout);
		if (num > 0) {
			catomic_set(&mgr->event_num, num);
			num = (num + (int)(EVERY_THREAD_PROCESS_EVENT_NUM) - 1) / (int)(EVERY_THREAD_PROCESS_EVENT_NUM);
		} else if (num < 0) {
			if (num == -1 && NET_GetLastError() == EINTR)
				return 0;
			log_error("kevent return value < 0, error, return value:%d, errno:%d", num, NET_GetLastError());
		}
		return num;
	}
}


/*
 * initialize event manager. 
 * socketer_num --- socket total number. must greater than 1.
 * thread_num --- thread number, if less than 0, then start by the number of cpu threads 
 */
bool eventmgr_init(int socketer_num, int thread_num) {
	if (s_mgr || socketer_num < 1)
		return false;

	if (thread_num <= 0) {
		thread_num = get_cpu_num();
	}

	/* in kqueue, must only one thead. */
	thread_num = 1;

	{
		struct sigaction sa;
		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = SIG_IGN;
		sigemptyset(&sa.sa_mask);
		if (sigaction(SIGPIPE, &sa, NULL) == -1)
			return false;
	}

	s_mgr = (struct kqueuemgr *)malloc(sizeof(struct kqueuemgr));
	if (!s_mgr)
		return false;

	/* initialize. */
	catomic_set(&s_mgr->event_num, 0);
	s_mgr->kqueue_fd = kqueue();
	if (s_mgr->kqueue_fd == -1) {
		free(s_mgr);
		s_mgr = NULL;
		return false;
	}

	s_mgr->thread_num = thread_num;
	s_mgr->need_exit = false;

	/* first building kqueue module, and then create thread pool. */
	s_mgr->thread_pool = cthread_pool_create(thread_num, s_mgr, leader_func, task_func);
	if (!s_mgr->thread_pool) {
		close(s_mgr->kqueue_fd);
		free(s_mgr);
		s_mgr = NULL;
		return false;
	}
	return true;
}

/*
 * release event manager.
 */
void eventmgr_release() {
	if (!s_mgr)
		return;

	/* set exit flag. */
	s_mgr->need_exit = true;

	/* release thread pool. */
	cthread_pool_release(s_mgr->thread_pool);

	/* close kqueue some. */
	close(s_mgr->kqueue_fd);
	free(s_mgr);
	s_mgr = NULL;
}

