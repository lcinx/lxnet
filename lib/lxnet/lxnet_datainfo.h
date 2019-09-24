
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

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
	int64 send_msg_num;
	int64 recv_msg_num;
	int64 send_bytes;
	int64 recv_bytes;
	time_t send_msg_num_time;
	time_t recv_msg_num_time;
	time_t send_bytes_time;
	time_t recv_bytes_time;
};

struct datainfomgr {
	int64 last_time;
	struct datainfo data_table[enum_netdata_end];
};

#endif

