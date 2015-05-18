
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
 * bigbufsize --- big block size. 
 * bigbufnum --- big block num.
 *
 * smallbufsize --- small block size. 
 * smallbufnum --- small block num.
 * listen num --- listener object num. 
 * socket num --- socketer object num.
 * threadnum --- network thread num, if less than 0, then start by the number of cpu threads .
 */
bool netinit(size_t bigbufsize, size_t bigbufnum, size_t smallbufsize, size_t smallbufnum,
		size_t listenernum, size_t socketnum, int threadnum) {
	if ((!bufmgr_init(bigbufnum, bigbufsize, smallbufnum, smallbufsize, socketnum)) ||
		(!eventmgr_init(socketnum, threadnum)) || (!socketmgr_init()) ||
		(!netpool_init(socketnum, socketer_getsize(), listenernum, listener_getsize()))) {
		netrelease();
		return false;
	}
	return true;
}

/* release network. */
void netrelease() {
	eventmgr_release();
	socketmgr_release();
	bufmgr_release();
	netpool_release();
}

/* network run. */
void netrun() {
	socketmgr_run();
}

/* get network memory info. */
void netmemory_info(char *buf, size_t bufsize) {
	size_t index = 0;
	bufmgr_meminfo(buf, bufsize-1);
	index = strlen(buf);
	netpool_meminfo(&buf[index], bufsize-1-index);
	buf[bufsize-1] = 0;
}


