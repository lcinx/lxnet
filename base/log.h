
/*
 * Copyright (C) lcinx
 * lcinx@163.com
*/

#ifndef _H_C_LOG_C_H_
#define _H_C_LOG_C_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "alway_inline.h"
#include <assert.h>

enum logtypelog_
{
	enum_log_type_assert = 0,
	enum_log_type_error,
	enum_log_type_log,
	enum_log_type_max,
};

enum debugtypelog_
{
	enum_debug_for_assert = 0,
	enum_debug_for_log,	
	enum_debug_for_debug,
	enum_debug_for_time_debug,
	enum_debug_max,
};

#ifndef NDEBUG
#define Assert(a, ...) \
	do{if (!(a))\
		{_log_printf_(enum_debug_for_assert, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__);\
		assert(false);\
		}\
	}while(0)
#else
#define Assert(a, ...) \
	do{if (!(a))\
		{_log_write_(((struct filelog *) 0), enum_log_type_assert, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__);}\
	}while(0)
#endif

struct filelog;

int mymkdir (const char *directname);

void _log_printf_ (unsigned int type, const char *filename, const char *func, long line, const char *fmt, ...);

/* print debug info and code file line */
#define debug_log(...) _log_printf_(enum_debug_for_assert, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__);

/* print debug info to console */
#define log_debug(...) _log_printf_(enum_debug_for_debug, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__);

/* print log info to console */
#define log_showlog(...) _log_printf_(enum_debug_for_log, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__);

/* print log info to console and time */
#define log_timelog(...) _log_printf_(enum_debug_for_time_debug, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__);

/* show/hide log_debug function out*/
void log_setdebug (bool show);

/* show/hide log_showlog function out*/
void log_setshow (bool show);

/* show/hide log_timelog function out*/
void log_settimelog (bool show);

void _log_write_ (struct filelog *log, unsigned int type, const char *filename, const char *func, long line, const char *fmt, ...);

void _log_setdirect_ (struct filelog *log, const char *directname);

#define log_setdirect(name) _log_setdirect_(((struct filelog *) 0), (name))

bool log_logtime (bool flag);

void log_everyflush (bool flag);

#define log_writelog(...) _log_write_(((struct filelog *) 0), enum_log_type_log, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)

#define log_error(...) _log_write_(((struct filelog *) 0), enum_log_type_error, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)


struct filelog *filelog_create ();

void filelog_release (struct filelog *self);

bool filelog_logtime (struct filelog *self, bool flag);

void filelog_everyflush (struct filelog *self, bool flag);

#define filelog_setdirect(log, name) _log_setdirect_((log), (name))

#define filelog_writelog(log, ...) _log_write_((log), enum_log_type_log, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)

#define filelog_error(log, ...) _log_write_((log), enum_log_type_error, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)

#ifdef __cplusplus
}
#endif
#endif

