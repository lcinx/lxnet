#ifndef _H_LXNET_DATAINFO_H_
#define _H_LXNET_DATAINFO_H_
#include <time.h>
#include "catomic.h"

enum {
	enum_netdata_total = 0,
	enum_netdata_now,
	enum_netdata_max,
	enum_netdata_end,
};

struct datainfo {
	catomic sendmsgnum;
	catomic recvmsgnum;
	catomic sendbytes;
	catomic recvbytes;
	time_t tm_sendmsgnum;
	time_t tm_recvmsgnum;
	time_t tm_sendbytes;
	time_t tm_recvbytes;
};

#endif

