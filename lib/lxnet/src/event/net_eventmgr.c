
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#include "net_eventmgr.h"

#if defined(_WIN32)
	#include "win_eventmgr.c"
#elif defined(__linux__)
	#include "linux_eventmgr.c"
#else
	#include "bsd_eventmgr.c"
#endif

