
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "cthread.h"
#include "crosslib.h"
#include "_netsocket.h"
#include "socket_internal.h"
#include "net_pool.h"
#include "net_buf.h"
#include "net_eventmgr.h"
#include "log.h"

#ifdef _DEBUG_NETWORK
#define debuglog debug_log
#else
#define debuglog(...)
#endif

#ifdef WIN32
static const int s_datalimit = 32*1024;
#endif

enum e_control_value {
	enum_list_run_delay = 300,

	enum_list_close_delaytime = 15000,
};

struct socketmgr {
	bool isinit;

	int64 currenttime;
	int64 lastrun;			/* last run time. */

	struct socketer *head;
	struct socketer *tail;
	cspin mgr_lock;
};

static struct socketmgr s_mgr = {false};

/* add to delay close list. */
static void socketmgr_add_to_waite(struct socketer *self) {
	cspin_lock(&s_mgr.mgr_lock);
	self->next = NULL;
	if (s_mgr.tail) {
		s_mgr.tail->next = self;
	} else {
		s_mgr.head = self;
	}
	s_mgr.tail = self;
	cspin_unlock(&s_mgr.mgr_lock);
	self->closetime = get_millisecond();
}

/* pop from close list. */
static struct socketer *socketmgr_pop_front() {
	struct socketer *so = NULL;
	cspin_lock(&s_mgr.mgr_lock);
	so = s_mgr.head;
	if (!so) {
		cspin_unlock(&s_mgr.mgr_lock);
		return NULL;
	}
	
	s_mgr.head = so->next;
	so->next = NULL;
	if (s_mgr.tail == so) {
		s_mgr.tail = NULL;
		assert(s_mgr.head == NULL);
	}
	cspin_unlock(&s_mgr.mgr_lock);
	return so;
}

/* get socket object size. */
size_t socketer_getsize() {
	return (sizeof(struct socketer));
}

/* default encrypt/decrypt function key. */
static const char chKey = 0xae;
static void default_decrypt_func(void *logicdata, char *buf, int len) {
	int i;
	for (i = 0; i < len; i++) {
		buf[i] ^= chKey;
	}
}

static void default_encrypt_func(void *logicdata, char *buf, int len) {
	int i;
	for (i = 0; i < len; i++) {
		buf[i] ^= chKey;
	}
}

static void socketer_initrecvbuf(struct socketer *self) {
	if (!self->recvbuf) {
		self->recvbuf = buf_create(self->bigbuf);
		buf_setdofunc(self->recvbuf, default_decrypt_func, NULL, NULL);
	}
}

static void socketer_initsendbuf(struct socketer *self) {
	if (!self->sendbuf) {
		self->sendbuf = buf_create(self->bigbuf);
		buf_setdofunc(self->sendbuf, default_encrypt_func, NULL, NULL);
	}
}

static bool socketer_init(struct socketer *self, bool bigbuf) {

#ifdef WIN32
	memset(&self->recv_event, 0, sizeof(self->recv_event));
	memset(&self->send_event, 0, sizeof(self->send_event));
#else
	catomic_set(&self->events, 0);
#endif

	self->sockfd = NET_INVALID_SOCKET;
	self->try_connect_time = 0;
	self->closetime = 0;
	self->next = NULL;
	self->recvbuf = NULL;
	self->sendbuf = NULL;

	catomic_set(&self->already_event, 0);
	catomic_set(&self->sendlock, 0);
	catomic_set(&self->recvlock, 0);
	self->deleted = false;
	self->connected = false;
	self->bigbuf = bigbuf;
	catomic_set(&self->ref, 1);
	return true;
}

static void socketer_addto_eventlist(struct socketer *self) {
	/* if 0, then set 1, and add to event manager. */
	if (catomic_compare_set(&self->already_event, 0, 1)) {
		self->connected = true;
		socket_addto_eventmgr(self);
	}
}

/* 
 * create socketer. 
 * bigbuf --- if is true, then is bigbuf; or else is smallbuf.
 */
struct socketer *socketer_create(bool bigbuf) {
	struct socketer *self = (struct socketer *)netpool_createsocket();
	if (!self) {
		log_error("	struct socketer *self = (struct socketer *)netpool_createsocket();");
		return NULL;
	}

	if (!socketer_init(self, bigbuf)) {
		netpool_releasesocket(self);
		log_error("	if (!socketer_init(self, bigbuf))");
		return NULL;
	}
	return self;
}

struct socketer *socketer_create_for_accept(bool bigbuf, void *sockfd) {
	struct socketer *self;
	if (!socket_setopt_for_connect(*((net_socket *)sockfd)))
		return NULL;

	self = socketer_create(bigbuf);
	if (!self)
		return NULL;

	self->sockfd = *((net_socket *)sockfd);
	socketer_addto_eventlist(self);
	return self;
}

static void socketer_real_release(struct socketer *self) {
	self->next = NULL;
	buf_release(self->recvbuf);
	buf_release(self->sendbuf);
	self->recvbuf = NULL;
	self->sendbuf = NULL;
	netpool_releasesocket(self);
}

/* release socketer */
void socketer_release(struct socketer *self) {
	assert(self != NULL);
	if (!self)
		return;

	assert(!self->deleted);
	if (self->deleted)
		return;

	self->deleted = true;

	socketer_close(self);
	
	socketmgr_add_to_waite(self);
}

bool socketer_connect(struct socketer *self, const char *ip, short port) {
	assert(self != NULL);
	assert(ip != NULL);
	if (!self || !ip || 0 == port)
		return false;

	assert(!self->connected);
	if (self->connected)
		return false;

	if (self->sockfd == NET_INVALID_SOCKET) {
		struct addrinfo hints;
		struct addrinfo *ai_list, *cur;
		int status;
		char port_buf[16];
		int lasterror;

		snprintf(port_buf, sizeof(port_buf), "%d", (int)port);
		port_buf[sizeof(port_buf) - 1] = '\0';

		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;

		status = getaddrinfo(ip, port_buf, &hints, &ai_list);
		if (status != 0)
			return false;
	
		cur = ai_list;
		do {
			self->sockfd = socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol);
			if (self->sockfd == NET_INVALID_SOCKET)
				continue;

			socket_setopt_for_connect(self->sockfd);

			if (connect(self->sockfd, cur->ai_addr, cur->ai_addrlen) == 0)
				break;

			lasterror = NET_GetLastError();
			if (SOCKET_ERR_CONNECT_RETRIABLE(lasterror) || SOCKET_ERR_CONNECT_ALREADY(lasterror))
				break;
			
			socket_close(&self->sockfd);
		} while ((cur = cur->ai_next) != NULL);

		freeaddrinfo(ai_list);

		if (self->sockfd == NET_INVALID_SOCKET) {
			log_error("socketer connect, but is invalid socket!, error!");
			return false;
		}

		self->try_connect_time = s_mgr.currenttime;
	}

	{
		int error;
		socklen_t len = sizeof(error);
		int code;

		bool is_connect = false;	
		if (socket_can_write(self->sockfd) == 1)
			is_connect = true;

		code = getsockopt(self->sockfd, SOL_SOCKET, SO_ERROR, (void *)&error, &len);
		if (code < 0 || SOCKET_ERR_CONNECT_REFUSED(error)) {
			socket_close(&self->sockfd);
			return false;
		}

		if (is_connect) {
			socketer_addto_eventlist(self);
			return true;
		}

		if (s_mgr.currenttime - self->try_connect_time > 1000) {
			socket_close(&self->sockfd);
		}
	}

	return false;
}

void socketer_close(struct socketer *self) {
	assert(self != NULL);
	if (!self)
		return;

	if (self->sockfd != NET_INVALID_SOCKET) {
		/*if 1, then set 0, and remove from event manager. */
		if (catomic_compare_set(&self->already_event, 1, 0)) {
			socket_removefrom_eventmgr(self);
		}
		socket_close(&self->sockfd);
	}

	self->connected = false;
}

bool socketer_isclose(struct socketer *self) {
	assert(self != NULL);
	if (!self)
		return true;

	return (self->sockfd == NET_INVALID_SOCKET);
}

void socketer_getip(struct socketer *self, char *ip, size_t len) {
	struct sockaddr_storage localaddr;
	net_sock_len isize = sizeof(localaddr);
	assert(self != NULL);
	assert(ip != NULL);
	if (!self || !ip || len < 1)
		return;

	ip[0] = '\0';
	if (getpeername(self->sockfd, (struct sockaddr *)&localaddr, &isize) == 0) {
		if (getnameinfo((struct sockaddr *)&localaddr, isize, ip, len, 0, 0, NI_NUMERICHOST) != 0) {
			ip[0] = '\0';
			return;
		}
	}
	ip[len - 1] = '\0';
}

int socketer_get_send_buffer_byte_size(struct socketer *self) {
	if (!self)
		return 0;

	if (!self->sendbuf)
		return 0;

	return buf_get_data_size(self->sendbuf);
}

bool socketer_gethostname(char *name, size_t len) {
	if (gethostname(name, len) == 0) {
		name[len - 1] = '\0';
		return true;
	}

	name[0] = '\0';
	return false;
}

bool socketer_gethostbyname(const char *name, char *buf, size_t len, bool ipv6) {
	struct addrinfo hints;
	struct addrinfo *ai_list, *cur;
	int status;
	if (!name || !buf || len < 64)
		return false;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = (ipv6 ? AF_INET6 : AF_INET);
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	status = getaddrinfo(name, NULL, &hints, &ai_list);
	if (status != 0)
		return false;
	
	cur = ai_list;
	do {
		if (getnameinfo(cur->ai_addr, cur->ai_addrlen, buf, len, 0, 0, NI_NUMERICHOST) == 0)
			break;

		buf[0] = '\0';
	} while ((cur = cur->ai_next) != NULL);

	freeaddrinfo(ai_list);

	if (cur == NULL)
		return false;

	return true;
}

bool socketer_sendmsg(struct socketer *self, void *data, int len) {
	assert(self != NULL);
	assert(data != NULL);
	assert(len > 0);
	if (!self || !data || len <= 0)
		return false;

	if (self->deleted || !self->connected)
		return false;

	socketer_initsendbuf(self);
	return buf_pushmessage(self->sendbuf, (char *)data, len);
}

/* 
 * when sending data. test send limit as len.
 * if return true, close this connect.
 */
bool socketer_send_islimit(struct socketer *self, size_t len) {
	assert(self != NULL);
	assert(len < _MAX_MSG_LEN);
	if (!self)
		return true;

	socketer_initsendbuf(self);
	return buf_add_islimit(self->sendbuf, len);
}

/* set send event. */
void socketer_checksend(struct socketer *self) {
	assert(self != NULL);
	if (!self)
		return;

	if (self->deleted || !self->connected)
		return;

	socketer_initsendbuf(self);

	/* if not has data for send. */
	if (buf_can_not_send(self->sendbuf))
		return;

	/* if 0, then set 1, and set sendevent. */
	if (catomic_compare_set(&self->sendlock, 0, 1)) {
		if (catomic_inc(&self->ref) <= 1) {
			log_error("%x socket recvlock:%d, sendlock:%d, fd:%d, ref:%d, thread_id:%d, connect:%d, deleted:%d", 
					self, (int)catomic_read(&self->recvlock), (int)catomic_read(&self->sendlock), self->sockfd, 
					(int)catomic_read(&self->ref), cthread_self_id(), self->connected, self->deleted);
		}
		socket_setup_sendevent(self);
	}
}

void *socketer_getmsg(struct socketer *self, char *buf, size_t bufsize) {
	void *msg;
	bool needclose = false;
	assert(self != NULL);
	if (!self)
		return NULL;

	socketer_initrecvbuf(self);
	msg = buf_getmessage(self->recvbuf, &needclose, buf, bufsize);
	if (needclose)
		socketer_close(self);
	return msg;
}

void *socketer_getdata(struct socketer *self, char *buf, size_t bufsize, int *datalen) {
	void *data;
	bool needclose = false;
	assert(self != NULL);
	if (!self)
		return NULL;

	socketer_initrecvbuf(self);
	data = buf_getdata(self->recvbuf, &needclose, buf, (int)bufsize, datalen);
	if (needclose)
		socketer_close(self);
	return data;
}

/* set recv event. */
void socketer_checkrecv(struct socketer *self) {
	assert(self != NULL);
	if (!self)
		return;

	if (self->deleted || !self->connected)
		return;

	socketer_initrecvbuf(self);

	/* if not recv, because limit. */
	if (buf_can_not_recv(self->recvbuf))
		return;

	/* if 0, then set 1, and set recvevent.*/
	if (catomic_compare_set(&self->recvlock, 0, 1)) {
		if (catomic_inc(&self->ref) <= 1) {
			log_error("%x socket recvlock:%d, sendlock:%d, fd:%d, ref:%d, thread_id:%d, connect:%d, deleted:%d", 
					self, (int)catomic_read(&self->recvlock), (int)catomic_read(&self->sendlock), self->sockfd, 
					(int)catomic_read(&self->ref), cthread_self_id(), self->connected, self->deleted);
		}

		socket_setup_recvevent(self);
	}
}

/* set recv data limit. */
void socketer_set_recv_critical(struct socketer *self, int size) {
	assert(self != NULL);
	if (!self)
		return;

	if (size > 0) {
		socketer_initrecvbuf(self);
		buf_set_limitsize(self->recvbuf, size);
	}
}

/* set send data limit.*/
void socketer_set_send_critical(struct socketer *self, int size) {
	assert(self != NULL);
	if (!self)
		return;

	if (size > 0) {
		socketer_initsendbuf(self);
		buf_set_limitsize(self->sendbuf, size);
	}
}

void socketer_use_compress(struct socketer *self) {
	assert(self != NULL);
	if (!self)
		return;

	socketer_initsendbuf(self);
	buf_usecompress(self->sendbuf);
}

void socketer_use_uncompress(struct socketer *self) {
	assert(self != NULL);
	if (!self)
		return;

	socketer_initrecvbuf(self);
	buf_useuncompress(self->recvbuf);
}

/* set encrypt function and logic data. */
void socketer_set_encrypt_function(struct socketer *self, dofunc_f encrypt_func, void (*release_logicdata)(void *), void *logicdata) {
	assert(self != NULL);
	if (!self || !encrypt_func)
		return;

	socketer_initsendbuf(self);
	buf_setdofunc(self->sendbuf, encrypt_func, release_logicdata, logicdata);
}

/* set encrypt function and logic data. */
void socketer_set_decrypt_function(struct socketer *self, dofunc_f decrypt_func, void (*release_logicdata)(void *), void *logicdata) {
	assert(self != NULL);
	if (!self || !decrypt_func)
		return;

	socketer_initrecvbuf(self);
	buf_setdofunc(self->recvbuf, decrypt_func, release_logicdata, logicdata);
}

void socketer_use_encrypt(struct socketer *self) {
	assert(self != NULL);
	if (!self)
		return;

	socketer_initsendbuf(self);
	buf_useencrypt(self->sendbuf);
}

void socketer_use_decrypt(struct socketer *self) {
	assert(self != NULL);
	if (!self)
		return;

	socketer_initrecvbuf(self);
	buf_usedecrypt(self->recvbuf);
}

void socketer_use_tgw(struct socketer *self) {
	assert(self != NULL);
	if (!self)
		return;

	socketer_initrecvbuf(self);
	buf_use_tgw(self->recvbuf);
}

void socketer_set_raw_datasize(struct socketer *self, size_t size) {
	assert(self != NULL);
	if (!self)
		return;

	socketer_initsendbuf(self);
	buf_set_raw_datasize(self->sendbuf, size);
}

/* interface for event mgr. */

void socketer_on_recv(struct socketer *self, int len) {
	int res;
	struct buf_info writebuf;
	debuglog("on recv\n");

	assert(catomic_read(&self->recvlock) == 1);
	assert(len >= 0);

#ifdef WIN32
	if (len > 0) {
		writebuf = buf_getwritebufinfo(self->recvbuf);
		if (writebuf.len < len || !writebuf.buf) {
			log_error("if (writebuf.len < len)  len:%d, writebuf.len:%d, writebuf.buf:%x", len, writebuf.len, writebuf.buf);
		}
		buf_addwrite(self->recvbuf, writebuf.buf, len);
	}
#endif

	for (;;) {
		writebuf = buf_getwritebufinfo(self->recvbuf);
		assert(writebuf.len >= 0);
		if (writebuf.len <= 0) {
			if (!buf_recv_end_do(self->recvbuf)) {
				/* uncompress error, close socket. */
				socketer_close(self);

				if (catomic_dec(&self->ref) < 1) {
					log_error("%x socket recvlock:%d, sendlock:%d, fd:%d, ref:%d, thread_id:%d, connect:%d, deleted:%d", 
							self, (int)catomic_read(&self->recvlock), (int)catomic_read(&self->sendlock), self->sockfd, 
							(int)catomic_read(&self->ref), cthread_self_id(), self->connected, self->deleted);
				}

				return;
			}

#ifndef WIN32
			/* remove recv event. */
			socket_remove_recvevent(self);
#endif

			if (catomic_dec(&self->ref) < 1) {
				log_error("%x socket recvlock:%d, sendlock:%d, fd:%d, ref:%d, thread_id:%d, connect:%d, deleted:%d", 
						self, (int)catomic_read(&self->recvlock), (int)catomic_read(&self->sendlock), self->sockfd, 
						(int)catomic_read(&self->ref), cthread_self_id(), self->connected, self->deleted);
			}

			if (catomic_dec(&self->recvlock) != 0) {
				log_error("%x socket recvlock:%d, sendlock:%d, fd:%d, ref:%d, thread_id:%d, connect:%d, deleted:%d", 
						self, (int)catomic_read(&self->recvlock), (int)catomic_read(&self->sendlock), self->sockfd, 
						(int)catomic_read(&self->ref), cthread_self_id(), self->connected, self->deleted);
			}
			return;
		}

		res = recv(self->sockfd, writebuf.buf, writebuf.len, 0);
		if (res > 0) {
			buf_addwrite(self->recvbuf, writebuf.buf, res);
			debuglog("recv :%d size\n", res);
		} else {
			if (!buf_recv_end_do(self->recvbuf)) {
				/* uncompress error, close socket. */
				socketer_close(self);

				if (catomic_dec(&self->ref) < 1) {
					log_error("%x socket recvlock:%d, sendlock:%d, fd:%d, ref:%d, thread_id:%d, connect:%d, deleted:%d", 
							self, (int)catomic_read(&self->recvlock), (int)catomic_read(&self->sendlock), self->sockfd, 
							(int)catomic_read(&self->ref), cthread_self_id(), self->connected, self->deleted);
				}

				return;
			}

			if ((!SOCKET_ERR_RW_RETRIABLE(NET_GetLastError())) || (res == 0)) {
				/* error, close socket. */
				socketer_close(self);

				if (catomic_dec(&self->ref) < 1) {
					log_error("%x socket recvlock:%d, sendlock:%d, fd:%d, ref:%d, thread_id:%d, connect:%d, deleted:%d", 
							self, (int)catomic_read(&self->recvlock), (int)catomic_read(&self->sendlock), self->sockfd, 
							(int)catomic_read(&self->ref), cthread_self_id(), self->connected, self->deleted);
				}

				debuglog("recv func, socket is error!, so close it!\n");
			} else {
#ifdef WIN32
				/* if > s_datalimit, then set is s_datalimit. */
				if (writebuf.len > s_datalimit)
					writebuf.len = s_datalimit;

				/* set recv event. */
				socket_recvdata(self, writebuf.buf, writebuf.len);
				debuglog("setup recv event...\n");
#endif
			}
			/* return. !!! */
			return;
		}
		
	}
}


void socketer_on_send(struct socketer *self, int len) {
	int res;
	struct buf_info readbuf;
	debuglog("on send\n");

	assert(catomic_read(&self->sendlock) == 1);
	assert(len >= 0);

#ifdef WIN32
	if (len > 0) {
		buf_addread(self->sendbuf, len);
		debuglog("send :%d size\n", len);
	}
#endif
	
	/* do something before real send. */
	buf_send_before_do(self->sendbuf);

	for (;;) {
		readbuf = buf_getreadbufinfo(self->sendbuf);
		assert(readbuf.len >= 0);
		if (readbuf.len <= 0) {

#ifndef WIN32
			/* remove send event. */
			socket_remove_sendevent(self);
#endif

			if (catomic_dec(&self->ref) < 1) {
				log_error("%x socket recvlock:%d, sendlock:%d, fd:%d, ref:%d, thread_id:%d, connect:%d, deleted:%d", 
						self, (int)catomic_read(&self->recvlock), (int)catomic_read(&self->sendlock), self->sockfd, 
						(int)catomic_read(&self->ref), cthread_self_id(), self->connected, self->deleted);
			}

			if (catomic_dec(&self->sendlock) != 0) {
				log_error("%x socket recvlock:%d, sendlock:%d, fd:%d, ref:%d, thread_id:%d, connect:%d, deleted:%d", 
						self, (int)catomic_read(&self->recvlock), (int)catomic_read(&self->sendlock), self->sockfd, 
						(int)catomic_read(&self->ref), cthread_self_id(), self->connected, self->deleted);
			}

			return;
		}

		res = send(self->sockfd, readbuf.buf, readbuf.len, 0);
		if (res > 0) {
			buf_addread(self->sendbuf, res);
			debuglog("send :%d size\n", res);
		} else {
			if (!SOCKET_ERR_RW_RETRIABLE(NET_GetLastError())) {
				/* error, close socket. */
				socketer_close(self);
				
				if (catomic_dec(&self->ref) < 1) {
					log_error("%x socket recvlock:%d, sendlock:%d, fd:%d, ref:%d, thread_id:%d, connect:%d, deleted:%d", 
							self, (int)catomic_read(&self->recvlock), (int)catomic_read(&self->sendlock), self->sockfd, 
							(int)catomic_read(&self->ref), cthread_self_id(), self->connected, self->deleted);
				}
				debuglog("send func, socket is error!, so close it!\n");
			} else {
#ifdef WIN32
				/* if > s_datalimit, then set is s_datalimit. */
				if (readbuf.len > s_datalimit)
					readbuf.len = s_datalimit;

				/* set send data, because WSASend 0 size data, not check can send. */
				socket_senddata(self, readbuf.buf, readbuf.len);
				debuglog("setup send event...\n");

#endif
			}
			/* return. !!! */
			return;
		}
	}
}

/* create and init socketer manager. */
bool socketmgr_init() {
	if (s_mgr.isinit)
		return false;

	s_mgr.isinit = true;
	s_mgr.currenttime = get_millisecond();
	s_mgr.lastrun = 0;
	s_mgr.head = NULL;
	s_mgr.tail = NULL;
	cspin_init(&s_mgr.mgr_lock);
	return true;
}

/* run socketer manager. */
void socketmgr_run() {
	int64 currenttime;
	if (!s_mgr.head)
		return;

	s_mgr.currenttime = get_millisecond();
	currenttime = s_mgr.currenttime;
	if (currenttime - s_mgr.lastrun < enum_list_run_delay)
		return;

	s_mgr.lastrun = currenttime;
	for (;;) {
		struct socketer *sock, *resock;
		sock = s_mgr.head;
		if (!sock)
			return;

		if (currenttime - sock->closetime < enum_list_close_delaytime)
			return;

#ifdef WIN32
		if (catomic_dec(&sock->ref) != 0) {
			log_error("%x socket recvlock:%d, sendlock:%d, fd:%d, ref:%d, thread_id:%d, connect:%d, deleted:%d", 
					sock, (int)catomic_read(&sock->recvlock), (int)catomic_read(&sock->sendlock), sock->sockfd, 
					(int)catomic_read(&sock->ref), cthread_self_id(), sock->connected, sock->deleted);
		}
#endif

		resock = socketmgr_pop_front();
		assert(resock == sock);
		if (sock != resock) {
			log_error(" if (sock != resock)");
			if (resock) {
#ifdef WIN32
				if (catomic_read(&resock->ref) != 0) {
					log_error("%x socket recvlock:%d, sendlock:%d, fd:%d, ref:%d, thread_id:%d, connect:%d, deleted:%d", 
							resock, (int)catomic_read(&resock->recvlock), (int)catomic_read(&resock->sendlock), resock->sockfd, 
							(int)catomic_read(&resock->ref), cthread_self_id(), resock->connected, resock->deleted);
				}
#endif
				socketer_real_release(resock);
			}
		}

#ifdef WIN32
		if (catomic_read(&sock->ref) != 0)
			log_error("%x socket recvlock:%d, sendlock:%d, fd:%d, ref:%d, thread_id:%d, connect:%d, deleted:%d", 
					sock, (int)catomic_read(&sock->recvlock), (int)catomic_read(&sock->sendlock), sock->sockfd, 
					(int)catomic_read(&sock->ref), cthread_self_id(), sock->connected, sock->deleted);
#endif
		socketer_real_release(sock);
	}
}

/* release socketer manager. */
void socketmgr_release() {
	struct socketer *sock, *next;
	if (!s_mgr.isinit)
		return;

	s_mgr.isinit = false;
	cspin_lock(&s_mgr.mgr_lock);
	for (sock = s_mgr.head; sock; sock = next) {
		next = sock->next;
		socketer_real_release(sock);
	}

	cspin_unlock(&s_mgr.mgr_lock);
	cspin_destroy(&s_mgr.mgr_lock);
	s_mgr.head = NULL;
	s_mgr.tail = NULL;
}


