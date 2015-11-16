
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#ifndef _H_LOG_H_
#define _H_LOG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "platform_config.h"

int mymkdir_r(const char *directory);



enum write_log_type_ {
	enum_log_type_log = 0,
	enum_log_type_error,
	enum_log_type_max,
};

enum debug_print_type_ {
	enum_debug_print = 0,
	enum_debug_print_call,
	enum_debug_print_time,
	enum_debug_max,
};

enum logfile_save_type {
	st_every_day_split_dir_and_every_hour_split_file = 0,
	st_every_month_split_dir_and_every_day_split_file,
	st_no_split_dir_and_every_day_split_file,
	st_no_split_dir_and_not_split_file,
};

#define _NUMBER_TO_STRING_REAL_(number_value)	#number_value
#define _NUMBER_TO_STRING_(number_value)		_NUMBER_TO_STRING_REAL_(number_value)
#define __LINE__STRING__						_NUMBER_TO_STRING_(__LINE__)


struct filelog;
extern struct filelog *g_filelog_obj_;

void _log_printf_(int type, const char *filename, const char *func, 
		int line, const char *fmt, ...);
void _log_printf_set_show(int type, bool flag);
void _filelog_write_(struct filelog *log, int type, const char *filename, 
		const char *func, int line, const char *fmt, ...);
void _filelog_setdirectory_(struct filelog *self, int type, const char *directory);
const char *_filelog_getdirectory_(struct filelog *self, int type);
bool _filelog_set_save_type_(struct filelog *self, int type, int save_type);
bool _filelog_append_time_(struct filelog *self, int type, bool flag);
bool _filelog_everyflush_(struct filelog *self, int type, bool flag);
void _filelog_flush_(struct filelog *self, int type);


struct filelog *filelog_create();
void filelog_release(struct filelog *self);

#define filelog_set_directory(self, directory)							\
	_filelog_setdirectory_(self, enum_log_type_log, (directory));		\
	_filelog_setdirectory_(self, enum_log_type_error, (directory))

#define filelog_set_log_directory(self, directory)						\
	_filelog_setdirectory_(self, enum_log_type_log, (directory))

#define filelog_set_error_directory(self, directory)					\
	_filelog_setdirectory_(self, enum_log_type_error, (directory))

#define filelog_get_log_directory(self)									\
	_filelog_getdirectory_(self, enum_log_type_log)

#define filelog_get_error_directory(self)								\
	_filelog_getdirectory_(self, enum_log_type_error)

#define filelog_set_log_save_type(self, save_type)						\
	_filelog_set_save_type_(self, enum_log_type_log, (save_type))

#define filelog_set_error_save_type(self, save_type)					\
	_filelog_set_save_type_(self, enum_log_type_error, (save_type))

#define filelog_append_time(self, flag)									\
	_filelog_append_time_(self, enum_log_type_log, (flag))

#define filelog_everyflush(self, flag)									\
	_filelog_everyflush_(self, enum_log_type_log, (flag))

#define filelog_flush(self)												\
	_filelog_flush_(self, enum_log_type_log)

#define filelog_writelog(self, ...)										\
	_filelog_write_(self, enum_log_type_log, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)

#define filelog_error(self, ...)										\
	_filelog_write_(self, enum_log_type_error, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)





#define log_set_directory(directory)									\
	_filelog_setdirectory_(g_filelog_obj_, enum_log_type_log, (directory));\
	_filelog_setdirectory_(g_filelog_obj_, enum_log_type_error, (directory))

#define log_get_directory()												\
	_filelog_getdirectory_(g_filelog_obj_, enum_log_type_log)

#define log_append_time(flag)											\
	_filelog_append_time_(g_filelog_obj_, enum_log_type_log, flag)

#define log_everyflush(flag)											\
	_filelog_everyflush_(g_filelog_obj_, enum_log_type_log, flag)

#define log_writelog(...)												\
	_filelog_write_(g_filelog_obj_, enum_log_type_log, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)

#define log_error(...)													\
	_filelog_write_(g_filelog_obj_, enum_log_type_error, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)



#define debug_print(...)												\
	_log_printf_(enum_debug_print, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)

#define debug_print_call(...)											\
	_log_printf_(enum_debug_print_call, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)

#define debug_print_time(...)											\
	_log_printf_(enum_debug_print_time, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)


#define debug_enable_print(flag)										\
	_log_printf_set_show(enum_debug_print, flag)

#define debug_enable_print_call(flag)									\
	_log_printf_set_show(enum_debug_print_call, flag)

#define debug_enable_print_time(flag)									\
	_log_printf_set_show(enum_debug_print_time, flag)


#ifdef __cplusplus
}
#endif
#endif

