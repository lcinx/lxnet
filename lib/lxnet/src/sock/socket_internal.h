
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#ifndef _H_SOCKET_INTERNAL_H_
#define _H_SOCKET_INTERNAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "net_common.h"
#include "catomic.h"

#ifdef WIN32
struct overlappedstruct {
	OVERLAPPED m_overlap;				/* overlap struct. */
	int m_event;						/* event type for iocp. */
};
#endif

struct net_buf;
struct socketer {
#ifdef WIN32
	struct overlappedstruct recv_event;
	struct overlappedstruct send_event;
#else
	catomic events;						/* for epoll event. */
#endif

	net_socket sockfd;					/* socket fd. */
	int64 try_connect_time;				/* the one fd try connect 1000 ms, after close it. */
	int64 close_time;					/* close time. */
	struct socketer *next;
	struct net_buf *recvbuf;
	struct net_buf *sendbuf;

	catomic already_event;				/* if 0, then do not join. if 1, is added. */

	catomic sendlock;					/* if 0, then not set send event. if 1, already set. */
	catomic recvlock;					/* if 0, then not set recv event. if 1, already set. */
	volatile bool deleted;				/* delete flag. */
	volatile bool connected;			/* connect flag. */
	bool bigbuf;						/* if true, then is bigbuf */

	catomic ref;						/* the socketer object reference number */
};

#ifdef __cplusplus
}
#endif
#endif

