
/*
 * Copyright (C) lcinx
 * lcinx@163.com
*/

#ifndef WIN32
#include "ossome.h"
#include "net_eventmgr.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <signal.h>
#include <unistd.h>
#include "socket_internal.h"
#include "_netsocket.h"
#include "log.h"
#include "threadpool.h"

#ifdef _DEBUG_NETWORK
#define debuglog debug_log
#else
#define debuglog(...) ((void) 0)
#endif

/* create epoll handle parameter. */
#define SOCKET_HOLDER_SIZE (36000)
/* max events from epoll_wait function. */
#define THREAD_EVENT_SIZE (4096)
struct epollmgr
{
	int threadnum;
	struct threadmgr *threadpool;					/* thread pool. */
	volatile char need_exit;						/* exit flag. */

	volatile long event_num;						/* current event number. */
	int epoll_fd;									/* epoll handle. */
	struct epoll_event ev_array[THREAD_EVENT_SIZE];	/* event array. */
};
static struct epollmgr *s_mgr = NULL;

/* add socket to event manager. */
void socket_addto_eventmgr (struct socketer *self)
{
	struct epoll_event ev;

	/* add evnet ---EPOLLHUP event. */
	self->events = EPOLLHUP;

	memset(&ev, 0, sizeof(ev));
	ev.events = self->events;
	ev.data.ptr = self;
	if (epoll_ctl(s_mgr->epoll_fd, EPOLL_CTL_ADD, self->sockfd, &ev) == -1)
	{
		/*log_error("epoll, add event to epoll set on fd %d error!, errno:%d", ev.data.fd, NET_GetLastError());*/
		socketer_close(self);
	}
	debuglog("add hup event to eventmgr.");
}

/* remove socket from event manager. */
void socket_removefrom_eventmgr (struct socketer *self)
{
	struct epoll_event ev;
	memset(&ev, 0, sizeof(ev));
	self->events = 0;
	ev.events = EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP;
	if (epoll_ctl(s_mgr->epoll_fd, EPOLL_CTL_DEL, self->sockfd, &ev) == -1)
	{
		/*log_error("epoll, not remove fd %d from epoll set, error!, errno:%d", ev.data.fd, NET_GetLastError());*/
	}
	
	debuglog("remove all event from eventmgr.");
}

/* set recv event. */
void socket_setup_recvevent (struct socketer *self)
{
	struct epoll_event ev;
	memset(&ev, 0, sizeof(ev));

	assert(self->recvlock == 1);

	if (self->recvlock != 1)
	{
		log_error("%x socket recvlock:%d, sendlock:%d, fd:%d, ref:%d, thread_id:%d", self, (int)self->recvlock, (int)self->sendlock, self->sockfd, (int)self->ref, CURRENT_THREAD);
	}

	ev.events = atom_or_fetch(&self->events, EPOLLIN);
	ev.data.ptr = self;
	if (epoll_ctl(s_mgr->epoll_fd, EPOLL_CTL_MOD, self->sockfd, &ev) == -1)
	{
		/*log_error("epoll, setup recv event to epoll set on fd %d error!, errno:%d", self->sockfd, NET_GetLastError());*/
		socketer_close(self);
		if (atom_dec(&self->ref) < 1)
		{
			log_error("%x socket recvlock:%d, sendlock:%d, fd:%d, ref:%d, thread_id:%d, connect:%d, deleted:%d", self, (int)self->recvlock, (int)self->sendlock, self->sockfd, (int)self->ref, CURRENT_THREAD, self->connected, self->deleted);
		}
	}
	debuglog("setup recv event to eventmgr.");
}

/* remove recv event. */
void socket_remove_recvevent (struct socketer *self)
{
	struct epoll_event ev;
	memset(&ev, 0, sizeof(ev));

	assert(self->recvlock == 1);

	if (self->recvlock != 1)
	{
		log_error("%x socket recvlock:%d, sendlock:%d, fd:%d, ref:%d, thread_id:%d", self, (int)self->recvlock, (int)self->sendlock, self->sockfd, (int)self->ref, CURRENT_THREAD);
	}

	ev.events = atom_and_fetch(&self->events, ~(EPOLLIN));
	ev.data.ptr = self;
	if (epoll_ctl(s_mgr->epoll_fd, EPOLL_CTL_MOD, self->sockfd, &ev) == -1)
	{
		/*log_error("epoll, remove recv event from epoll set on fd %d error!, errno:%d", self->sockfd, NET_GetLastError());*/
		socketer_close(self);
	}
	debuglog("remove recv event from eventmgr.");
}

/* set send event. */
void socket_setup_sendevent (struct socketer *self)
{
	struct epoll_event ev;
	memset(&ev, 0, sizeof(ev));

	assert(self->sendlock == 1);
	if (self->sendlock != 1)
	{
		log_error("%x socket recvlock:%d, sendlock:%d, fd:%d, ref:%d, thread_id:%d", self, (int)self->recvlock, (int)self->sendlock, self->sockfd, (int)self->ref, CURRENT_THREAD);
	}

	ev.events = atom_or_fetch(&self->events, EPOLLOUT);
	ev.data.ptr = self;
	if (epoll_ctl(s_mgr->epoll_fd, EPOLL_CTL_MOD, self->sockfd, &ev) == -1)
	{
		/*log_error("epoll, setup send event to epoll set on fd %d error!, errno:%d", self->sockfd, NET_GetLastError());*/
		socketer_close(self);
		if (atom_dec(&self->ref) < 1)
		{
			log_error("%x socket recvlock:%d, sendlock:%d, fd:%d, ref:%d, thread_id:%d, connect:%d, deleted:%d", self, (int)self->recvlock, (int)self->sendlock, self->sockfd, (int)self->ref, CURRENT_THREAD, self->connected, self->deleted);
		}
	}
	debuglog("setup send event to eventmgr.");
}

/* remove send event. */
void socket_remove_sendevent (struct socketer *self)
{
	struct epoll_event ev;
	memset(&ev, 0, sizeof(ev));

	assert(self->sendlock == 1);
	if (self->sendlock != 1)
	{
		log_error("%x socket recvlock:%d, sendlock:%d, fd:%d, ref:%d, thread_id:%d", self, (int)self->recvlock, (int)self->sendlock, self->sockfd, (int)self->ref, CURRENT_THREAD);
	}

	ev.events = atom_and_fetch(&self->events, ~(EPOLLOUT));
	ev.data.ptr = self;
	if (epoll_ctl(s_mgr->epoll_fd, EPOLL_CTL_MOD, self->sockfd, &ev) == -1)
	{
		/*log_error("epoll, remove send event from epoll set on fd %d error!, errno:%d", self->sockfd, NET_GetLastError());*/
		socketer_close(self);
	}
	debuglog("remove send event from eventmgr.");
}

static struct epoll_event *pop_event (struct epollmgr *self)
{
	int index = atom_dec(&self->event_num);
	if (index < 0)
		return NULL;
	else
		return &self->ev_array[index];
}

/* execute the task callback function. */
static int task_func (void *argv)
{
	struct epollmgr *mgr = (struct epollmgr *)argv;
	struct epoll_event *ev;
	struct socketer *sock;
	for (;;)
	{
		if (mgr->need_exit)
			return -1;
		ev = pop_event(mgr);
		if (!ev)
			return 0;
		assert(ev->data.ptr != NULL);
		sock = (struct socketer *)ev->data.ptr;

		/* error event. */
		if (ev->events & EPOLLHUP || ev->events & EPOLLERR)
		{
			socketer_close(sock);
			continue;
		}

		/* can read event. */
		if (ev->events & EPOLLIN)
		{
			if (atom_compare_and_swap(&sock->recvlock, 0, 1) == 0)
			{
				atom_inc(&sock->ref);
			}
			socketer_on_recv(sock, 0);
		}

		/* can write event. */
		if (ev->events & EPOLLOUT)
		{
			if (atom_compare_and_swap(&sock->sendlock, 0, 1) == 0)
			{
				atom_inc(&sock->ref);
			}
			socketer_on_send(sock, 0);
		}
	}
}

#define EVERY_THREAD_PROCESS_EVENT_NUM 128
/* get need resume thread number. */
static int leader_func (void *argv)
{
	struct epollmgr *mgr = (struct epollmgr *)argv;

	/* wait event. */
	if (mgr->need_exit)
	{
		return -1;
	}
	else
	{
		int num = epoll_wait(mgr->epoll_fd, mgr->ev_array, THREAD_EVENT_SIZE, 100);
		if (num > 0)
		{
			atom_set(&mgr->event_num, num);
			num = (num+(int)(EVERY_THREAD_PROCESS_EVENT_NUM)-1)/(int)(EVERY_THREAD_PROCESS_EVENT_NUM);
		}
		else if (num < 0)
		{
			if (num == -1 && NET_GetLastError() == EINTR)
				return 0;
			log_error("epoll_wait return value < 0, error, return value:%d, errno:%d", num, NET_GetLastError());
		}
		return num;
	}
}


/* 
 * initialize event manager. 
 * socketnum --- socket total number. must greater than 1.
 * threadnum --- thread number, if less than 0, then start by the number of cpu threads 
 * */
bool eventmgr_init (int socketnum, int threadnum)
{
	if (s_mgr)
		return false;
	if (socketnum < 1)
		return false;
	if (socketnum > (SOCKET_HOLDER_SIZE - 6000))
		return false;
	if (threadnum <= 0)
		threadnum = get_cpunum();
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
		return false;
	s_mgr = (struct epollmgr *)malloc(sizeof(struct epollmgr));
	if (!s_mgr)
		return false;

	/* initialize. */
	s_mgr->event_num = 0;
	s_mgr->epoll_fd = epoll_create(SOCKET_HOLDER_SIZE);
	if (s_mgr->epoll_fd == -1)
	{
		free(s_mgr);
		s_mgr = NULL;
		return false;
	}

	s_mgr->threadnum = threadnum;
	s_mgr->need_exit = false;

	/*  first building epoll module, and then create thread pool. */
	s_mgr->threadpool = threadmgr_create(threadnum, task_func, leader_func, s_mgr);
	if (!s_mgr->threadpool)
	{
		close(s_mgr->epoll_fd);
		free(s_mgr);
		s_mgr = NULL;
		return false;
	}
	return true;
}

/* 
 * release event manager.
 * */
void eventmgr_release ()
{
	if (!s_mgr)
		return;
	/* set exit flag. */
	s_mgr->need_exit = true;

	usleep(300000);	/* 300 ms, this value need greate than epoll_wait function last parameters. */

	/* release thread pool. */
	threadmgr_release(s_mgr->threadpool);

	/* close epoll some. */
	close(s_mgr->epoll_fd);
	free(s_mgr);
	s_mgr = NULL;
}

#endif

