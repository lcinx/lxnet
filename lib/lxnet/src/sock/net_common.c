
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#include "net_common.h"

#ifdef WIN32

#ifdef _MSC_VER
#pragma comment(lib, "ws2_32")
#endif

#else

#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#endif


int socket_close(net_socket *sockfd) {
	int res;
	net_socket temp = *sockfd;
	*sockfd = NET_INVALID_SOCKET;
#ifdef WIN32
	res = closesocket(temp);
#else
	res = close(temp);
#endif
	return res;
}

static bool socket_set_nonblock(net_socket sockfd) {
#ifdef WIN32
	{
		u_long nonblocking = 1;
		ioctlsocket(sockfd, FIONBIO, &nonblocking);
	}
#else
	{
		int flags;
		if ((flags = fcntl(sockfd, F_GETFL, NULL)) < 0)
			return false;

		if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1)
			return false;
	}
#endif
	return true;
}

bool socket_setopt_for_connect(net_socket sockfd) {
	if (!socket_set_nonblock(sockfd))
		return false;

	/* prohibit nagle. */
	{
		int bNoDelay = 1;
		setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char*)&bNoDelay, sizeof(int));
	}

	{
		struct linger ling;
		ling.l_onoff = 1;
		ling.l_linger = 0;
		setsockopt(sockfd, SOL_SOCKET, SO_LINGER, (char *)&ling, sizeof(ling));
	}

	{
		int bKeepAlive = 1;
		setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (char*)&bKeepAlive, sizeof(int));
	}

	return true;
}

static bool set_reuseaddr(net_socket fd) {
#ifdef WIN32
	return true;
#else
	int reuseaddr = 1;
	return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const void *) &reuseaddr, sizeof(int)) == 0;
#endif
}

bool socket_setopt_for_listen(net_socket sockfd) {
	return socket_set_nonblock(sockfd) && set_reuseaddr(sockfd);
}

int socket_can_read(net_socket fd) {
#ifdef WIN32

	fd_set set;
	struct timeval tout;
	tout.tv_sec = 0;
	tout.tv_usec = 0;

	FD_ZERO(&set);
	FD_SET(fd, &set);
	return select((int)fd + 1, &set, NULL, NULL, &tout);

#else

	struct pollfd set;
	set.fd = fd;
	set.events = POLLIN;
	return poll(&set, 1, 0);
#endif
}

int socket_can_write(net_socket fd) {
	#ifdef WIN32

	fd_set set;
	struct timeval tout;
	tout.tv_sec = 0;
	tout.tv_usec = 0;

	FD_ZERO(&set);
	FD_SET(fd, &set);
	return select((int)fd + 1, NULL, &set, NULL, &tout);

#else

	struct pollfd set;
	set.fd = fd;
	set.events = POLLOUT;
	return poll(&set, 1, 0);
#endif
}

