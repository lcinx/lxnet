
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#include <string.h>
#include "net_module.h"
#include "net_buf.h"
#include "net_eventmgr.h"
#include "net_pool.h"

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
					size_t listener_num, size_t socketer_num, int thread_num) {

	if ((!bufmgr_init(big_buf_num, big_buf_size, small_buf_num, small_buf_size, socketer_num)) ||
		(!eventmgr_init(socketer_num, thread_num)) || (!socketmgr_init()) ||
		(!netpool_init(socketer_num, socketer_get_size(), listener_num, listener_get_size()))) {
		net_module_release();
		return false;
	}
	return true;
}

/* release network. */
void net_module_release() {
	eventmgr_release();
	socketmgr_release();
	bufmgr_release();
	netpool_release();
}

/* network run. */
void net_module_run() {
	socketmgr_run();
}

/* get network memory info. */
void net_module_get_memory_info(char *buf, size_t bufsize) {
	size_t index = 0;
	bufmgr_get_memory_info(buf, bufsize - 1);
	index = strlen(buf);
	netpool_get_memory_info(&buf[index], bufsize - 1 - index);
	buf[bufsize - 1] = 0;
}

