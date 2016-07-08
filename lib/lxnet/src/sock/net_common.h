
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#ifndef _H_NET_COMMON_H_
#define _H_NET_COMMON_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "platform_config.h"

#ifdef _WIN32

#ifdef __GNUC__
#if (_WIN32_WINNT < 0x0501)
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#define net_socket SOCKET
#define net_sock_len int
#define NET_INVALID_SOCKET	INVALID_SOCKET

#define NET_GetLastError()	WSAGetLastError()
/* retry. */
#define SOCKET_ERR_CONNECT_RETRIABLE(e)	\
	((e) == WSAEWOULDBLOCK ||			\
	 (e) == WSAEINTR ||					\
	 (e) == WSAEINPROGRESS ||			\
	 (e) == WSAEALREADY)

/* refused connect. */
#define SOCKET_ERR_CONNECT_REFUSED(e)	\
	((e) == WSAECONNREFUSED)

/* already connect. */
#define SOCKET_ERR_CONNECT_ALREADY(e)	\
	((e) == WSAEISCONN)

/* if is false, then disconnect. */
#define SOCKET_ERR_RW_RETRIABLE(e)		\
	((e) == WSAEWOULDBLOCK ||			\
	 (e) == WSAEINTR)

#else

#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#define net_socket int
#define net_sock_len socklen_t
#define NET_INVALID_SOCKET ((net_socket)(~0))

#define NET_GetLastError()	errno
/* retry. */
#define SOCKET_ERR_CONNECT_RETRIABLE(e)	\
	 ((e) == EINTR || (e) == EINPROGRESS)

/* refused connect. */
#define SOCKET_ERR_CONNECT_REFUSED(e)	\
	((e) == ECONNREFUSED)

/* already connect. */
#define SOCKET_ERR_CONNECT_ALREADY(e)	\
	((e) == EISCONN)

/* if is false, then disconnect. */
#define SOCKET_ERR_RW_RETRIABLE(e)		\
	((e) == EINTR ||					\
	 (e) == EAGAIN)

#endif


int socket_close(net_socket *sockfd);

bool socket_setopt_for_connect(net_socket sockfd);

bool socket_setopt_for_listen(net_socket sockfd);

int socket_can_read(net_socket fd);

int socket_can_write(net_socket fd);

#ifdef __cplusplus
}
#endif
#endif

