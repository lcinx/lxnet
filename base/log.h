
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
	st_not_split_dir_and_every_day_split_file,
	st_not_split_dir_and_every_hour_split_file,
	st_not_split_dir_and_not_split_file,
};

#define _NUMBER_TO_STRING_REAL_(number_value)	#number_value
#define _NUMBER_TO_STRING_(number_value)		_NUMBER_TO_STRING_REAL_(number_value)
#define __LINE__STRING__						_NUMBER_TO_STRING_(__LINE__)

#define __END__ARG__FLAG__						g_filelog_obj_


struct filelog;
extern struct filelog *g_filelog_obj_;

void _format_prefix_string_(struct filelog *log, int type, 
		char *buf, size_t bufsize, const char *file, const char *function, int line);
void _log_printf_(int type, const char *fmt, ...);
void _log_printf_set_show(int type, bool flag);
void _filelog_write_(struct filelog *log, int type, const char *fmt, ...);
void _filelog_set_directory_(struct filelog *self, int type, const char *directory);
const char *_filelog_get_directory_(struct filelog *self, int type);
bool _filelog_set_save_type_(struct filelog *self, int type, int save_type);
int _filelog_set_split_file_interval_(struct filelog *self, int type, int split_file_interval);
bool _filelog_set_single_filename_(struct filelog *self, int type, const char *single_filename);
const char *_filelog_get_single_filename_(struct filelog *self, int type);
bool _filelog_set_prefix_(struct filelog *self, int type, const char *prefix);
const char *_filelog_get_prefix_(struct filelog *self, int type);
bool _filelog_set_append_time_(struct filelog *self, int type, bool flag);
bool _filelog_set_every_flush_(struct filelog *self, int type, bool flag);
void _filelog_flush_(struct filelog *self, int type);


struct filelog *filelog_create();
void filelog_release(struct filelog *self);

#define filelog_set_directory(self, directory)							\
	do {																\
		_filelog_set_directory_(self, enum_log_type_log, (directory));	\
		_filelog_set_directory_(self, enum_log_type_error, (directory));\
	} while (0)

#define filelog_set_directory_log(self, directory)						\
	_filelog_set_directory_(self, enum_log_type_log, (directory))

#define filelog_set_directory_error(self, directory)					\
	_filelog_set_directory_(self, enum_log_type_error, (directory))

#define filelog_get_directory_log(self)									\
	_filelog_get_directory_(self, enum_log_type_log)

#define filelog_get_directory_error(self)								\
	_filelog_get_directory_(self, enum_log_type_error)

#define filelog_set_save_type_log(self, save_type)						\
	_filelog_set_save_type_(self, enum_log_type_log, (save_type))

#define filelog_set_save_type_error(self, save_type)					\
	_filelog_set_save_type_(self, enum_log_type_error, (save_type))

#define filelog_set_split_file_interval_log(self, split_file_interval)	\
	_filelog_set_split_file_interval_(self, enum_log_type_log, (split_file_interval))

#define filelog_set_split_file_interval_error(self, split_file_interval)\
	_filelog_set_split_file_interval_(self, enum_log_type_error, (split_file_interval))

#define filelog_set_single_filename_log(self, single_filename)			\
	_filelog_set_single_filename_(self, enum_log_type_log, (single_filename))

#define filelog_set_single_filename_error(self, single_filename)		\
	_filelog_set_single_filename_(self, enum_log_type_error, (single_filename))

#define filelog_get_single_filename_log(self)							\
	_filelog_get_single_filename_(self, enum_log_type_log)

#define filelog_get_single_filename_error(self)							\
	_filelog_get_single_filename_(self, enum_log_type_error)

#define filelog_set_prefix_log(self, prefix)							\
	_filelog_set_prefix_(self, enum_log_type_log, (prefix))

#define filelog_set_prefix_error(self, prefix)							\
	_filelog_set_prefix_(self, enum_log_type_error, (prefix))

#define filelog_get_prefix_log(self)									\
	_filelog_get_prefix_(self, enum_log_type_log)

#define filelog_get_prefix_error(self)									\
	_filelog_get_prefix_(self, enum_log_type_error)

#define filelog_set_append_time_log(self, flag)							\
	_filelog_set_append_time_(self, enum_log_type_log, (flag))

#define filelog_set_append_time_error(self, flag)						\
	_filelog_set_append_time_(self, enum_log_type_error, (flag))

#define filelog_set_every_flush_log(self, flag)							\
	_filelog_set_every_flush_(self, enum_log_type_log, (flag))

#define filelog_set_every_flush_error(self, flag)						\
	_filelog_set_every_flush_(self, enum_log_type_error, (flag))

#define filelog_flush_log(self)											\
	_filelog_flush_(self, enum_log_type_log)

#define filelog_flush_error(self)										\
	_filelog_flush_(self, enum_log_type_error)

#define _filelog_write_arg_(self, type, file, function, line, fmt, ...)	\
	do {																\
		char temp_buf[1024 * 8] = {0};									\
		_format_prefix_string_(self, type,								\
				temp_buf, sizeof(temp_buf), file, function, line);		\
		_filelog_write_(self, type,										\
				fmt, temp_buf, ##__VA_ARGS__, __END__ARG__FLAG__);		\
	} while (0)

#define filelog_write_log(self, fmt, ...)								\
	_filelog_write_arg_(self, enum_log_type_log, 0, 0, 0, fmt, ##__VA_ARGS__)

#define filelog_write_error_arg(self, file, function, line, fmt, ...)	\
	_filelog_write_arg_(self, enum_log_type_error,						\
			file, function, line, fmt, ##__VA_ARGS__)

#define filelog_write_error(self, fmt, ...)								\
	filelog_write_error_arg(self, __FILE__, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)






#define log_release()													\
		filelog_release(g_filelog_obj_)

#define log_set_directory(directory)									\
	do {																\
		_filelog_set_directory_(g_filelog_obj_,							\
				enum_log_type_log, (directory));						\
		_filelog_set_directory_(g_filelog_obj_,							\
				enum_log_type_error, (directory));						\
	} while (0)

#define log_get_directory()												\
	_filelog_get_directory_(g_filelog_obj_, enum_log_type_log)

#define log_append_time(flag)											\
	_filelog_set_append_time_(g_filelog_obj_, enum_log_type_log, flag)

#define log_every_flush(flag)											\
	_filelog_set_every_flush_(g_filelog_obj_, enum_log_type_log, flag)

#define log_writelog(fmt, ...)											\
	filelog_write_log(g_filelog_obj_, fmt, ##__VA_ARGS__)

#define log_error(fmt, ...)												\
	filelog_write_error(g_filelog_obj_, fmt, ##__VA_ARGS__)



#define debug_print(fmt, ...)											\
	_log_printf_(enum_debug_print, fmt, "", ##__VA_ARGS__, __END__ARG__FLAG__)

#define debug_print_call(fmt, ...)										\
	do {																\
		char temp_buf[1024 * 8] = {0};									\
		_format_prefix_string_(0, -1, temp_buf, sizeof(temp_buf),		\
				__FILE__, __FUNCTION__, __LINE__);						\
		_log_printf_(enum_debug_print_call,								\
				fmt, temp_buf, ##__VA_ARGS__, __END__ARG__FLAG__);		\
	} while (0)

#define debug_print_time(fmt, ...)										\
	do {																\
		char temp_buf[32] = {0};										\
		_format_prefix_string_(0, -1, temp_buf, sizeof(temp_buf),		\
				0, 0, 0);												\
		_log_printf_(enum_debug_print_time,								\
				fmt, temp_buf, ##__VA_ARGS__, __END__ARG__FLAG__);		\
	} while (0)


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

