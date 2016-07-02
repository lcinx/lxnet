
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
void eventmgr_add_socket(struct socketer *self);

/* remove socket from event manager. */
void eventmgr_remove_socket(struct socketer *self);

/* set recv event. */
void eventmgr_setup_socket_recv_event(struct socketer *self);

/* remove recv event. */
void eventmgr_remove_socket_recv_event(struct socketer *self);

/* set recv data. */
void eventmgr_setup_socket_recv_data_event(struct socketer *self, char *data, int len);

/* set send event. */
void eventmgr_setup_socket_send_event(struct socketer *self);

/* remove send event. */
void eventmgr_remove_socket_send_event(struct socketer *self);

/* set send data. */
void eventmgr_setup_socket_send_data_event(struct socketer *self, char *data, int len);

/*
 * initialize event manager. 
 * socketer_num --- socket total number. must greater than 1.
 * thread_num --- thread number, if less than 0, then start by the number of cpu threads 
 */
bool eventmgr_init(int socketer_num, int thread_num);

/*
 * release event manager.
 */
void eventmgr_release();

#ifdef __cplusplus
}
#endif
#endif

