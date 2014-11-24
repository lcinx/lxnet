
/*
 * Copyright (C) lcinx
 * lcinx@163.com
*/

#include <assert.h>
#include <string.h>
#include "_netlisten.h"
#include "net_common.h"
#include "_netsocket.h"
#include "net_pool.h"
#include "log.h"

#define PT_DEBUG
#ifndef PT_DEBUG
#define debuglog(...) printf(__VA_ARGS__)
#else
#define debuglog(...)
#endif

struct listener
{
	net_socket sockfd;
	bool isfree;
};

/* get listen object size. */
size_t listener_getsize ()
{
	return (sizeof(struct listener));
}

static void listener_init (struct listener *self)
{
	self->sockfd = NET_INVALID_SOCKET;
	self->isfree = false;
}

struct listener *listener_create ()
{
	struct listener *self = (struct listener *)netpool_createlisten();
	if (!self)
	{
		log_error("	struct listener *self = (struct listener *)netpool_createlisten();");
		return NULL;
	}
	listener_init(self);
	return self;
}

void listener_release (struct listener *self)
{
	assert(self != NULL);
	assert(!self->isfree);
	if (!self)
		return;
	socket_close(&self->sockfd);
	self->isfree = true;
	netpool_releaselisten(self);
}

/* 
 * port --- listen port.
 * backlog --- listen queue, max wait connect. 
 * */
bool listener_listen (struct listener *self, unsigned short port, int backlog)
{
	struct sockaddr_in soaddr;
	assert(self != NULL);
	assert(!self->isfree);
	if (!self)
		return false;
	if (self->sockfd != NET_INVALID_SOCKET)
	{
		socket_close(&self->sockfd);
	}
	self->sockfd = socket_create();
	if (self->sockfd == NET_INVALID_SOCKET)
		return false;

	if (!socket_setopt_for_listen(self->sockfd))
	{
		socket_close(&self->sockfd);
		return false;
	}

	memset(&soaddr, 0, sizeof(soaddr));
	soaddr.sin_family = AF_INET;
	soaddr.sin_addr.s_addr = INADDR_ANY;
	soaddr.sin_port = htons(port);

	if (bind(self->sockfd, (struct sockaddr *)&soaddr, sizeof(soaddr)) != 0)
	{
		socket_close(&self->sockfd);
		return false;
	}

	if (listen(self->sockfd, backlog) != 0)
	{
		socket_close(&self->sockfd);
		return false;
	}
	return true;
}

bool listener_isclose (struct listener *self)
{
	assert(self != NULL);
	assert(!self->isfree);
	if (!self)
		return true;
	return (self->sockfd == NET_INVALID_SOCKET);
}

void listener_close (struct listener *self)
{
	assert(self != NULL);
	assert(!self->isfree);
	if (!self)
		return;
	socket_close(&self->sockfd);
}

bool listener_can_accept (struct listener *self)
{
	assert(self != NULL);
	assert(!self->isfree);
	if (!self)
		return false;
	if (self->sockfd != NET_INVALID_SOCKET)
	{
		if (socket_can_read(self->sockfd) > 0)
			return true;
	}
	return false;
}

/* 
 * accept new connect.
 * bigbuf --- accept after, create bigbuf or smallbuf. 
 * */
struct socketer *listener_accept (struct listener *self, bool bigbuf)
{
	int e;
	assert(self != NULL);
	assert(!self->isfree);
	if (!self)
		return NULL;
	if (self->sockfd == NET_INVALID_SOCKET)
		return NULL;

	e = socket_can_read(self->sockfd);
	if (1 == e)								/* can accept new connect. */
	{
		struct sockaddr_in client_addr;		/* connect address info. */
		net_sock_len size = sizeof(client_addr);
		struct socketer *temp;
		net_socket new_sock = accept(self->sockfd, (struct sockaddr *)&client_addr, &size);
		if (new_sock == NET_INVALID_SOCKET)
			return NULL;

		temp = socketer_create_for_accept(bigbuf, (void *)&new_sock);
		if (!temp)
		{
			socket_close(&new_sock);
			return NULL;
		}
		return temp;
	}
#ifndef NDEBUG
	else
	{
		if (e == -1)		/* select function error. */
		{
			debuglog("select socket error!\n");
			return NULL;
		}
		else if (e == 0)	/* over time. */
		{
			debuglog("select socket overtime...error:%ld\n", GetLastError());
			return NULL;
		}
	}
	debuglog("end so return null...\n");
#endif
	return NULL;
}


