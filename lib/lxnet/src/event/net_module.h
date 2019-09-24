
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#ifndef _H_NET_MODULE_H_
#define _H_NET_MODULE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "_netlisten.h"
#include "_netsocket.h"


/*
 * initialize network.
 * big_buf_size --- big block size.
 * big_buf_num --- big block num.
 *
 * small_buf_size --- small block size.
 * small_buf_num --- small block num.
 * listener_num --- listener object num.
 * socketer_num --- socketer object num.
 * thread_num --- network thread num, if less than 0, then start by the number of cpu threads .
 */
bool net_module_init(size_t big_buf_size, size_t big_buf_num, 
					size_t small_buf_size, size_t small_buf_num, 
					size_t listener_num, size_t socketer_num, int thread_num);

/* release network. */
void net_module_release();

/* network run. */
void net_module_run();


struct poolmgr_info;

/* get network memory info. */
size_t net_module_get_memory_info(struct poolmgr_info *array, size_t num);

#ifdef __cplusplus
}
#endif
#endif

