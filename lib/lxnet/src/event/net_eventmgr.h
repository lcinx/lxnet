
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#ifndef _H_NET_EVENTMGR_H_
#define _H_NET_EVENTMGR_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "platform_config.h"

struct socketer;

/* add socket to event manager. */
void socket_addto_eventmgr(struct socketer *self);

/* remove socket from event manager. */
void socket_removefrom_eventmgr(struct socketer *self);

/* set recv event. */
void socket_setup_recvevent(struct socketer *self);

/* remove recv event. */
void socket_remove_recvevent(struct socketer *self);

/* set recv data. */
void socket_recvdata(struct socketer *self, char *data, int len);

/* set send event. */
void socket_setup_sendevent(struct socketer *self);

/* remove send event. */
void socket_remove_sendevent(struct socketer *self);

/* set send data. */
void socket_senddata(struct socketer *self, char *data, int len);

/* 
 * initialize event manager. 
 * socketnum --- socket total number. must greater than 1.
 * threadnum --- thread number, if less than 0, then start by the number of cpu threads 
 * */
bool eventmgr_init(int socketnum, int threadnum);

/* 
 * release event manager.
 * */
void eventmgr_release();

#ifdef __cplusplus
}
#endif
#endif


