
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#include <assert.h>
#include <stdio.h>
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

struct listener {
	net_socket sockfd;
	bool is_free;
};

/* get listen object size. */
size_t listener_get_size() {
	return (sizeof(struct listener));
}

static void listener_init(struct listener *self) {
	self->sockfd = NET_INVALID_SOCKET;
	self->is_free = false;
}

struct listener *listener_create() {
	struct listener *self = (struct listener *)netpool_create_listener();
	if (!self) {
		log_error("	struct listener *self = (struct listener *)netpool_create_listener();");
		return NULL;
	}
	listener_init(self);
	return self;
}

void listener_release(struct listener *self) {
	assert(self != NULL);
	assert(!self->is_free);
	if (!self)
		return;
	socket_close(&self->sockfd);
	self->is_free = true;
	netpool_release_listener(self);
}

/*
 * port --- listen port.
 * backlog --- listen queue, max wait connect. 
 */
bool listener_listen(struct listener *self, unsigned short port, int backlog) {
	struct addrinfo hints;
	struct addrinfo *ai_list, *cur;
	int status;
	char port_buf[16];
	assert(self != NULL);
	assert(!self->is_free);
	if (!self)
		return false;

	if (self->sockfd != NET_INVALID_SOCKET)
		socket_close(&self->sockfd);

	ai_list = NULL;

	snprintf(port_buf, sizeof(port_buf), "%d", (int)port);
	port_buf[sizeof(port_buf) - 1] = '\0';

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_PASSIVE;
#ifdef WIN32
	hints.ai_family = AF_INET;
#else
	hints.ai_family = AF_UNSPEC;
#endif
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	status = getaddrinfo(NULL, port_buf, &hints, &ai_list);
	if (status != 0)
		return false;

	cur = ai_list;
	do {
		self->sockfd = socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol);
		if (self->sockfd == NET_INVALID_SOCKET)
			continue;

		if (!socket_setopt_for_listen(self->sockfd)) {
			socket_close(&self->sockfd);
			continue;
		}

		if (bind(self->sockfd, cur->ai_addr, cur->ai_addrlen) == 0)
			break;

		socket_close(&self->sockfd);

	} while ((cur = cur->ai_next) != NULL);

	freeaddrinfo(ai_list);

	if (cur == NULL) {
		socket_close(&self->sockfd);
		return false;
	}

	if (listen(self->sockfd, backlog) != 0) {
		socket_close(&self->sockfd);
		return false;
	}

	return true;
}

bool listener_isclose(struct listener *self) {
	assert(self != NULL);
	assert(!self->is_free);
	if (!self)
		return true;
	return (self->sockfd == NET_INVALID_SOCKET);
}

void listener_close(struct listener *self) {
	assert(self != NULL);
	assert(!self->is_free);
	if (!self)
		return;
	socket_close(&self->sockfd);
}

bool listener_can_accept(struct listener *self) {
	assert(self != NULL);
	assert(!self->is_free);
	if (!self)
		return false;
	if (self->sockfd != NET_INVALID_SOCKET) {
		if (socket_can_read(self->sockfd) > 0)
			return true;
	}
	return false;
}

/*
 * accept new connect.
 * bigbuf --- accept after, create bigbuf or smallbuf. 
 */
struct socketer *listener_accept(struct listener *self, bool bigbuf) {
	int e;
	assert(self != NULL);
	assert(!self->is_free);
	if (!self)
		return NULL;
	if (self->sockfd == NET_INVALID_SOCKET)
		return NULL;

	e = socket_can_read(self->sockfd);

	/* can accept new connect. */
	if (1 == e) {
		struct sockaddr_storage client_addr;		/* connect address info. */
		net_sock_len size = sizeof(client_addr);
		struct socketer *temp;
		net_socket new_sock = accept(self->sockfd, (struct sockaddr *)&client_addr, &size);
		if (new_sock == NET_INVALID_SOCKET)
			return NULL;

		temp = socketer_create_for_accept(bigbuf, (void *)&new_sock);
		if (!temp) {
			socket_close(&new_sock);
			return NULL;
		}
		return temp;
	}
#ifndef NDEBUG
	else
	{
		if (e == -1) {
			/* select function error. */
			debuglog("select socket error!\n");
			return NULL;
		} else if (e == 0) {
			/* over time. */
			debuglog("select socket overtime...error:%ld\n", GetLastError());
			return NULL;
		}
	}
	debuglog("end so return null...\n");
#endif

	return NULL;
}

