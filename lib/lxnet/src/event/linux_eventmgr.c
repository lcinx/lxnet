
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <signal.h>
#include <unistd.h>
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

/* max events from epoll_wait function. */
#define THREAD_EVENT_SIZE (4096)
struct epollmgr {
	int thread_num;
	struct cthread_pool *thread_pool;				/* thread pool. */
	volatile char need_exit;						/* exit flag. */

	catomic event_num;								/* current event number. */
	int epoll_fd;									/* epoll handle. */
	struct epoll_event ev_array[THREAD_EVENT_SIZE];	/* event array. */
};

static struct epollmgr *s_mgr = NULL;

/* add socket to event manager. */
void eventmgr_add_socket(struct socketer *self) {
	struct epoll_event ev;

	/* add evnet ---EPOLLHUP event. */
	catomic_set(&self->events, EPOLLHUP);

	memset(&ev, 0, sizeof(ev));
	ev.events = (uint32)catomic_read(&self->events);
	ev.data.ptr = self;
	if (epoll_ctl(s_mgr->epoll_fd, EPOLL_CTL_ADD, self->sockfd, &ev) == -1) {
		/*log_error("epoll, add event to epoll set on fd %d error!, errno:%d", ev.data.fd, NET_GetLastError());*/
		socketer_close(self);
	}
	debuglog("add hup event to eventmgr.");
}

/* remove socket from event manager. */
void eventmgr_remove_socket(struct socketer *self) {
	struct epoll_event ev;
	memset(&ev, 0, sizeof(ev));
	catomic_set(&self->events, 0);
	ev.events = EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP;
	if (epoll_ctl(s_mgr->epoll_fd, EPOLL_CTL_DEL, self->sockfd, &ev) == -1) {
		/*log_error("epoll, not remove fd %d from epoll set, error!, errno:%d", ev.data.fd, NET_GetLastError());*/
	}

	debuglog("remove all event from eventmgr.");
}

/* set recv event. */
void eventmgr_setup_socket_recv_event(struct socketer *self) {
	struct epoll_event ev;
	memset(&ev, 0, sizeof(ev));

	assert(catomic_read(&self->recvlock) == 1);

	if (catomic_read(&self->recvlock) != 1) {
		log_error("%x socket recvlock:%d, sendlock:%d, fd:%d, ref:%d, thread_id:%d", 
				self, (int)catomic_read(&self->recvlock), (int)catomic_read(&self->sendlock), 
				self->sockfd, (int)catomic_read(&self->ref), cthread_self_id());
	}

	ev.events = (uint32)catomic_or_fetch(&self->events, EPOLLIN);
	ev.data.ptr = self;
	if (epoll_ctl(s_mgr->epoll_fd, EPOLL_CTL_MOD, self->sockfd, &ev) == -1) {
		/*log_error("epoll, setup recv event to epoll set on fd %d error!, errno:%d", self->sockfd, NET_GetLastError());*/
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
	struct epoll_event ev;
	memset(&ev, 0, sizeof(ev));

	assert(catomic_read(&self->recvlock) == 1);

	if (catomic_read(&self->recvlock) != 1) {
		log_error("%x socket recvlock:%d, sendlock:%d, fd:%d, ref:%d, thread_id:%d", 
				self, (int)catomic_read(&self->recvlock), (int)catomic_read(&self->sendlock), 
				self->sockfd, (int)catomic_read(&self->ref), cthread_self_id());
	}

	ev.events = (uint32)catomic_and_fetch(&self->events, ~(EPOLLIN));
	ev.data.ptr = self;
	if (epoll_ctl(s_mgr->epoll_fd, EPOLL_CTL_MOD, self->sockfd, &ev) == -1) {
		/*log_error("epoll, remove recv event from epoll set on fd %d error!, errno:%d", self->sockfd, NET_GetLastError());*/
		socketer_close(self);
	}
	debuglog("remove recv event from eventmgr.");
}

/* set send event. */
void eventmgr_setup_socket_send_event(struct socketer *self) {
	struct epoll_event ev;
	memset(&ev, 0, sizeof(ev));

	assert(catomic_read(&self->sendlock) == 1);
	if (catomic_read(&self->sendlock) != 1) {
		log_error("%x socket recvlock:%d, sendlock:%d, fd:%d, ref:%d, thread_id:%d", 
				self, (int)catomic_read(&self->recvlock), (int)catomic_read(&self->sendlock), 
				self->sockfd, (int)catomic_read(&self->ref), cthread_self_id());
	}

	ev.events = (uint32)catomic_or_fetch(&self->events, EPOLLOUT);
	ev.data.ptr = self;
	if (epoll_ctl(s_mgr->epoll_fd, EPOLL_CTL_MOD, self->sockfd, &ev) == -1) {
		/*log_error("epoll, setup send event to epoll set on fd %d error!, errno:%d", self->sockfd, NET_GetLastError());*/
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
	struct epoll_event ev;
	memset(&ev, 0, sizeof(ev));

	assert(catomic_read(&self->sendlock) == 1);
	if (catomic_read(&self->sendlock) != 1) {
		log_error("%x socket recvlock:%d, sendlock:%d, fd:%d, ref:%d, thread_id:%d", 
				self, (int)catomic_read(&self->recvlock), (int)catomic_read(&self->sendlock), 
				self->sockfd, (int)catomic_read(&self->ref), cthread_self_id());
	}

	ev.events = (uint32)catomic_and_fetch(&self->events, ~(EPOLLOUT));
	ev.data.ptr = self;
	if (epoll_ctl(s_mgr->epoll_fd, EPOLL_CTL_MOD, self->sockfd, &ev) == -1) {
		/*log_error("epoll, remove send event from epoll set on fd %d error!, errno:%d", self->sockfd, NET_GetLastError());*/
		socketer_close(self);
	}
	debuglog("remove send event from eventmgr.");
}

static struct epoll_event *pop_event(struct epollmgr *self) {
	int index = (int)catomic_dec(&self->event_num);
	if (index < 0)
		return NULL;
	else
		return &self->ev_array[index];
}

/* execute the task callback function. */
static int task_func(void *argv) {
	struct epollmgr *mgr = (struct epollmgr *)argv;
	struct epoll_event *ev;
	struct socketer *sock;
	for (;;) {
		if (mgr->need_exit)
			return -1;
		ev = pop_event(mgr);
		if (!ev)
			return 0;
		assert(ev->data.ptr != NULL);
		sock = (struct socketer *)ev->data.ptr;

		/* error event. */
		if (ev->events & EPOLLHUP || ev->events & EPOLLERR) {
			socketer_close(sock);
			continue;
		}

		/* can read event. */
		if (ev->events & EPOLLIN) {
			if (catomic_compare_set(&sock->recvlock, 0, 1)) {
				catomic_inc(&sock->ref);
			}
			socketer_on_recv(sock, 0);
		}

		/* can write event. */
		if (ev->events & EPOLLOUT) {
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
	struct epollmgr *mgr = (struct epollmgr *)argv;

	/* wait event. */
	if (mgr->need_exit) {
		return -1;
	} else {
		int num = epoll_wait(mgr->epoll_fd, mgr->ev_array, THREAD_EVENT_SIZE, 50);
		if (num > 0) {
			catomic_set(&mgr->event_num, num);
			num = (num + (int)(EVERY_THREAD_PROCESS_EVENT_NUM) - 1) / (int)(EVERY_THREAD_PROCESS_EVENT_NUM);
		} else if (num < 0) {
			if (num == -1 && NET_GetLastError() == EINTR)
				return 0;
			log_error("epoll_wait return value < 0, error, return value:%d, errno:%d", num, NET_GetLastError());
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

	{
		struct sigaction sa;
		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = SIG_IGN;
		sigemptyset(&sa.sa_mask);
		if (sigaction(SIGPIPE, &sa, NULL) == -1)
			return false;
	}

	s_mgr = (struct epollmgr *)malloc(sizeof(struct epollmgr));
	if (!s_mgr)
		return false;

	/* initialize. */
	catomic_set(&s_mgr->event_num, 0);
	s_mgr->epoll_fd = epoll_create(1024);
	if (s_mgr->epoll_fd == -1) {
		free(s_mgr);
		s_mgr = NULL;
		return false;
	}

	s_mgr->thread_num = thread_num;
	s_mgr->need_exit = false;

	/* first building epoll module, and then create thread pool. */
	s_mgr->thread_pool = cthread_pool_create(thread_num, s_mgr, leader_func, task_func);
	if (!s_mgr->thread_pool) {
		close(s_mgr->epoll_fd);
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

	/* close epoll some. */
	close(s_mgr->epoll_fd);
	free(s_mgr);
	s_mgr = NULL;
}

