
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#ifndef _H_NET_LISTEN_H_
#define _H_NET_LISTEN_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "platform_config.h"

struct listener;
struct socketer;

/* get listen object size. */
size_t listener_getsize();

struct listener *listener_create();

void listener_release(struct listener *self);

/*
 * port --- listen port.
 * backlog --- listen queue, max wait connect. 
 */
bool listener_listen(struct listener *self, unsigned short port, int backlog);

bool listener_isclose(struct listener *self);

void listener_close(struct listener *self);

bool listener_can_accept(struct listener *self);

/*
 * accept new connect.
 * bigbuf --- accept after, create bigbuf or smallbuf. 
 */
struct socketer *listener_accept(struct listener *self, bool bigbuf);

#ifdef __cplusplus
}
#endif
#endif

