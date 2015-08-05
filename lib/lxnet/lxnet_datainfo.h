#ifndef _H_LXNET_DATAINFO_H_
#define _H_LXNET_DATAINFO_H_
#include <time.h>
#include "platform_config.h"

enum {
	enum_netdata_total = 0,
	enum_netdata_now,
	enum_netdata_max,
	enum_netdata_end,
};

struct datainfo {
	int64 sendmsgnum;
	int64 recvmsgnum;
	int64 sendbytes;
	int64 recvbytes;
	time_t tm_sendmsgnum;
	time_t tm_recvmsgnum;
	time_t tm_sendbytes;
	time_t tm_recvbytes;
};

struct datainfomgr {
	int64 lasttime;
	struct datainfo datatable[enum_netdata_end];
};

#endif

