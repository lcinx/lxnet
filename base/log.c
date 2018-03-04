
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <assert.h>
#include "log.h"

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define my_mkdir _mkdir
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define my_mkdir(a) mkdir((a), 0755)
#endif


struct logobj {
	FILE *fp;
	time_t last_time;
	time_t last_split_time;
	char last_filename[1024];

	char directory[512];
	int save_type;
	int split_file_interval;
	char single_filename[128];
	char prefix[256];
	bool append_time;
	bool every_flush;
};

struct filelog {
	bool is_init;
	struct logobj log_group[enum_log_type_max];
};

static struct filelog g_logobj = {false};
struct filelog *g_filelog_obj_ = &g_logobj;

static bool s_debug_print_show[enum_debug_max] = {true, true, true};



int mymkdir_r(const char *directory) {
	int i, len;
	char temp[1024 * 8] = {0};
	if (!directory || !strncpy(temp, directory, sizeof(temp) - 1))
		return -1;

	temp[sizeof(temp) - 1] = '\0';
	len = (int)strlen(temp);
	if (len <= 0 || len >= (int)sizeof(temp) - 2)
		return -2;

	if (temp[len - 1] != '/') {
		temp[len] = '/';
		temp[len + 1] = '\0';
		len += 1;
	}

	if (temp[0] == '.')
		i = 1;
	else
		i = 0;

	for (i += 1; i < len; ++i) {
		if (temp[i] != '/')
			continue;

		temp[i] = '\0';
#ifdef _WIN32
		if (_access(temp, 0) != 0)
#else
		if (access(temp, F_OK) != 0)
#endif
		{
			if (my_mkdir(temp) != 0)
				return -3;
		}
		temp[i] = '/';
	}

	return 0;
}


static void logobj_reset(struct logobj *self) {
	self->last_time = 0;
	self->last_split_time = 0;
	memset(self->last_filename, 0, sizeof(self->last_filename));
}

static void logobj_set_directory(struct logobj *self, const char *directory) {
	if (!directory)
		return;

	if (strcmp(directory, "") == 0 || strcmp(directory, self->directory) == 0)
		return;

	strncpy(self->directory, directory, sizeof(self->directory) - 1);
	self->directory[sizeof(self->directory) - 1] = '\0';

	{
		/* set last char to '\0'. */
		int end_idx = (int)strlen(self->directory) - 1;
		if (self->directory[end_idx] == '/' || self->directory[end_idx] == '\\')
			self->directory[end_idx] = '\0';
	}

	logobj_reset(self);
}

static inline const char *logobj_get_directory(struct logobj *self) {
	return self->directory;
}

static bool logobj_set_save_type(struct logobj *self, int save_type) {
	if (!(
		save_type == st_every_day_split_dir_and_every_hour_split_file || 
		save_type == st_every_month_split_dir_and_every_day_split_file || 
		save_type == st_not_split_dir_and_every_day_split_file || 
		save_type == st_not_split_dir_and_every_hour_split_file || 
		save_type == st_not_split_dir_and_not_split_file))
		return false;

	self->save_type = save_type;
	logobj_reset(self);
	return true;
}

static inline int logobj_set_split_file_interval(struct logobj *self, int split_file_interval) {
	int old = self->split_file_interval;

	if (split_file_interval < 0)
		split_file_interval = 0;

	self->split_file_interval = split_file_interval;
	logobj_reset(self);
	return old;
}

static inline bool logobj_set_single_filename(struct logobj *self, const char *single_filename) {
	if (strlen(single_filename) >= sizeof(self->single_filename))
		return false;

	strncpy(self->single_filename, single_filename, sizeof(self->single_filename) - 1);
	self->single_filename[sizeof(self->single_filename) - 1] = '\0';
	logobj_reset(self);
	return true;
}

static inline const char *logobj_get_single_filename(struct logobj *self) {
	return self->single_filename;
}

static bool logobj_set_prefix(struct logobj *self, const char *prefix) {
	if (strlen(prefix) >= sizeof(self->prefix))
		return false;

	strncpy(self->prefix, prefix, sizeof(self->prefix) - 1);
	self->prefix[sizeof(self->prefix) - 1] = '\0';
	return true;
}

static inline const char *logobj_get_prefix(struct logobj *self) {
	return self->prefix;
}

static inline bool logobj_set_append_time(struct logobj *self, bool flag) {
	bool old = self->append_time;
	self->append_time = flag;
	return old;
}

static inline bool logobj_set_every_flush(struct logobj *self, bool flag) {
	bool old = self->every_flush;
	self->every_flush = flag;
	return old;
}

static inline void logobj_flush(struct logobj *self) {
	if (self->fp) {
		fflush(self->fp);
	}
}

static inline void logobj_close(struct logobj *self) {
	if (self->fp) {
		fclose(self->fp);
		self->fp = NULL;
	}

	logobj_reset(self);
}

static void logobj_init(struct logobj *self) {
	self->fp = NULL;
	self->last_time = 0;
	self->last_split_time = 0;
	memset(self->last_filename, 0, sizeof(self->last_filename));


	logobj_set_directory(self, "log_log");
	logobj_set_save_type(self, st_every_day_split_dir_and_every_hour_split_file);
	logobj_set_split_file_interval(self, 0);
	logobj_set_single_filename(self, "log");
	logobj_set_prefix(self, "");
	logobj_set_append_time(self, true);
	logobj_set_every_flush(self, true);
}


static void filelog_init(struct filelog *self) {
	int i;
	for (i = 0; i < enum_log_type_max; ++i) {
		logobj_init(&self->log_group[i]);
	}

	logobj_set_save_type(&self->log_group[enum_log_type_error], st_not_split_dir_and_not_split_file);
	logobj_set_single_filename(&self->log_group[enum_log_type_error], "error_log");
	self->is_init = true;
}


static bool inline check_type(int type) {
	return (type >= enum_log_type_log && type < enum_log_type_max);
}



#define FILELOG_CHECK_INIT(obj)											\
	do {																\
		if (!obj->is_init) {											\
			if (obj == g_filelog_obj_) {								\
				filelog_init(obj);										\
			}															\
		}																\
	} while (0)


#define __LOCALTIME__FORMAT__			"[%04d-%02d-%02d %02d:%02d:%02d] "
#define __LOCALTIME__ARG(now)											\
	now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,					\
	now->tm_hour, now->tm_min, now->tm_sec

#define __CALL__FROM__FORMAT__			"[file:%s, function:%s, line:%d] "
#define __CALL_FROM__ARG				file, function, line

#define __CUSTOM_PREFIX__FORMAT__		"%s"

void _format_prefix_string_(struct filelog *log, int type, 
		char *buf, size_t bufsize, const char *file, const char *function, int line) {

	time_t tval;
	struct tm tm_result;
	struct tm *now = NULL;
	const char *custom_prefix = "";
	bool append_time = true;
	if (!buf || bufsize <= 1)
		return;

	if (log) {
		struct logobj *info;

		if (!check_type(type))
			return;

		FILELOG_CHECK_INIT(log);

		info = &log->log_group[type];
		assert(bufsize >= 512);
		custom_prefix = info->prefix;
		append_time = info->append_time;
	}

	if (append_time) {
		time(&tval);
		now = safe_localtime(&tval, &tm_result);
	}

	if (file) {
		assert(bufsize >= 1024 * 8);

		if (append_time) {
			snprintf(buf, bufsize - 1, 
					__LOCALTIME__FORMAT__ __CALL__FROM__FORMAT__ __CUSTOM_PREFIX__FORMAT__, 
					__LOCALTIME__ARG(now), 
					__CALL_FROM__ARG, 
					custom_prefix);
		} else {
			snprintf(buf, bufsize - 1, 
					__CALL__FROM__FORMAT__ __CUSTOM_PREFIX__FORMAT__, 
					__CALL_FROM__ARG, 
					custom_prefix);
		}
	} else {
		assert(bufsize >= 32);

		if (append_time) {
			snprintf(buf, bufsize - 1, __LOCALTIME__FORMAT__ __CUSTOM_PREFIX__FORMAT__, 
					__LOCALTIME__ARG(now), 
					custom_prefix);
		} else {
			snprintf(buf, bufsize - 1, __CUSTOM_PREFIX__FORMAT__, 
					custom_prefix);
		}
	}

	buf[bufsize - 1] = '\0';
}



void _log_printf_(int type, const char *fmt, ...) {
	assert(type >= enum_debug_print);
	assert(type < enum_debug_max);
	assert(fmt != NULL);

	if (!s_debug_print_show[type] || !fmt)
		return;

	{
		char temp_buf[1024 * 32] = {0};
		va_list args, temp;
		const char *prefix;
		va_start(args, fmt);
		prefix = va_arg(args, const char *);
		va_copy(temp, args);
		if (va_arg(args, void *) == __END__ARG__FLAG__) {
			printf("%s%s\n", prefix, fmt);
			return;
		}

		snprintf(temp_buf, sizeof(temp_buf) - 1, "%s%s\n", prefix, fmt);
		fmt = temp_buf;
		va_copy(args, temp);

		vprintf(fmt, args);
		va_end(args);
	}
}

void _log_printf_set_show(int type, bool flag) {
	assert(type >= enum_debug_print && type < enum_debug_max);
	if (type < enum_debug_print || type >= enum_debug_max)
		return;

	s_debug_print_show[type] = flag;
}



void _filelog_write_(struct filelog *self, int type, const char *fmt, ...) {
	time_t tval;
	struct logobj *info;

	if (!self || !fmt)
		return;

	if (!check_type(type))
		return;

	FILELOG_CHECK_INIT(self);
	info = &self->log_group[type];

	time(&tval);

	if (info->last_time != tval) {
		char file_path[1024] = {0};
		const char *now_filename = file_path;

		struct tm tm_result;
		struct tm *now = safe_localtime(&tval, &tm_result);

		switch(info->save_type) {
		case st_every_day_split_dir_and_every_hour_split_file:
			snprintf(file_path, sizeof(file_path) - 1, "%s/%04d-%02d-%02d/%02d.log", 
					info->directory, 
					now->tm_year + 1900, now->tm_mon + 1, now->tm_mday, 
					now->tm_hour);
			break;
		case st_every_month_split_dir_and_every_day_split_file:
			snprintf(file_path, sizeof(file_path) - 1, "%s/%04d-%02d/%02d.log", 
					info->directory, 
					now->tm_year + 1900, now->tm_mon + 1, 
					now->tm_mday);
			break;
		case st_not_split_dir_and_every_day_split_file:
			snprintf(file_path, sizeof(file_path) - 1, "%s/%04d-%02d-%02d.log", 
					info->directory, 
					now->tm_year + 1900, now->tm_mon + 1, now->tm_mday);
			break;
		case st_not_split_dir_and_every_hour_split_file:
			snprintf(file_path, sizeof(file_path) - 1, "%s/%04d-%02d-%02d-%02d.log", 
					info->directory, 
					now->tm_year + 1900, now->tm_mon + 1, now->tm_mday, 
					now->tm_hour);
			break;
		case st_not_split_dir_and_not_split_file:
			if (info->split_file_interval > 0) {
				if (tval - info->last_split_time >= info->split_file_interval) {
					snprintf(file_path, sizeof(file_path) - 1, 
							"%s/%s%04d-%02d-%02d-%02d-%02d-%02d.log", 
							info->directory, 
							info->single_filename, 
							now->tm_year + 1900, now->tm_mon + 1, now->tm_mday, 
							now->tm_hour, now->tm_min, now->tm_sec);

					info->last_split_time = tval;
				} else {
					now_filename = info->last_filename;
				}
			} else {
				snprintf(file_path, sizeof(file_path) - 1, "%s/%s.log", 
						info->directory, 
						info->single_filename);
			}
			break;
		default:
			assert(false && "unknow save type...");
			return;
		}


		if (strcmp(info->last_filename, now_filename) != 0) {
			strncpy(info->last_filename, file_path, sizeof(info->last_filename) - 1);
			info->last_filename[sizeof(info->last_filename) - 1] = '\0';
			if (info->fp) {
				fclose(info->fp);
				info->fp = NULL;
			}

			{
				/* Most try 8 times. */
				const int max_times = 8;
				int i = 0;
				int res = 1;
				int len = strlen(file_path);
				for (i = len - 1; i >= 0; --i) {
					if (file_path[i] == '/') {
						file_path[i + 1] = '\0';
						break;
					}
				}

				i = 0;
				while (res != 0 && i < max_times) {
					res = mymkdir_r(file_path);
					++i;
				}
				assert(res == 0 && "mymkdir_r failed!");
			}
		}

		info->last_time = tval;
	}

	if (!info->fp) {
		info->fp = fopen(info->last_filename, "a");
	}

	if (info->fp) {
		char temp_buf[1024 * 32] = {0};
		va_list args, temp;
		const char *prefix;
		va_start(args, fmt);
		prefix = va_arg(args, const char *);
		va_copy(temp, args);
		if (va_arg(args, void *) == __END__ARG__FLAG__) {
			fprintf(info->fp, "%s%s\n", prefix, fmt);
		} else {
			snprintf(temp_buf, sizeof(temp_buf) - 1, "%s%s\n", prefix, fmt);
			fmt = temp_buf;
			va_copy(args, temp);

			vfprintf(info->fp, fmt, args);
			va_end(args);
		}

		if (info->every_flush)
			fflush(info->fp);
	}
}


struct filelog *filelog_create() {
	struct filelog *self = (struct filelog *)malloc(sizeof(struct filelog));
	if (!self)
		return NULL;

	filelog_init(self);
	return self;
}

void filelog_release(struct filelog *self) {
	int i;
	if (!self)
		return;

	for (i = 0; i < enum_log_type_max; ++i) {
		logobj_close(&self->log_group[i]);
	}

	if (self != g_filelog_obj_) {
		free(self);
	}
}

void _filelog_set_directory_(struct filelog *self, int type, const char *directory) {
	if (!self)
		return;

	if (!check_type(type))
		return;

	FILELOG_CHECK_INIT(self);
	logobj_set_directory(&self->log_group[type], directory);
}

const char *_filelog_get_directory_(struct filelog *self, int type) {
	if (!self)
		return NULL;

	if (!check_type(type))
		return NULL;

	FILELOG_CHECK_INIT(self);
	return logobj_get_directory(&self->log_group[type]);
}

bool _filelog_set_save_type_(struct filelog *self, int type, int save_type) {
	if (!self)
		return false;

	if (!check_type(type))
		return false;

	FILELOG_CHECK_INIT(self);
	return logobj_set_save_type(&self->log_group[type], save_type);
}

int _filelog_set_split_file_interval_(struct filelog *self, int type, int split_file_interval) {
	if (!self)
		return 0;

	if (!check_type(type))
		return 0;

	FILELOG_CHECK_INIT(self);
	return logobj_set_split_file_interval(&self->log_group[type], split_file_interval);
}

bool _filelog_set_single_filename_(struct filelog *self, int type, const char *single_filename) {
	if (!self || !single_filename)
		return false;

	if (!check_type(type))
		return false;

	FILELOG_CHECK_INIT(self);
	return logobj_set_single_filename(&self->log_group[type], single_filename);
}

const char *_filelog_get_single_filename_(struct filelog *self, int type) {
	if (!self)
		return NULL;

	if (!check_type(type))
		return NULL;

	FILELOG_CHECK_INIT(self);
	return logobj_get_single_filename(&self->log_group[type]);
}

bool _filelog_set_prefix_(struct filelog *self, int type, const char *prefix) {
	if (!self || !prefix)
		return false;

	if (!check_type(type))
		return false;

	FILELOG_CHECK_INIT(self);
	return logobj_set_prefix(&self->log_group[type], prefix);
}

const char *_filelog_get_prefix_(struct filelog *self, int type) {
	if (!self)
		return NULL;

	if (!check_type(type))
		return NULL;

	FILELOG_CHECK_INIT(self);
	return logobj_get_prefix(&self->log_group[type]);
}

bool _filelog_set_append_time_(struct filelog *self, int type, bool flag) {
	if (!self)
		return true;

	if (!check_type(type))
		return true;

	FILELOG_CHECK_INIT(self);
	return logobj_set_append_time(&self->log_group[type], flag);
}

bool _filelog_set_every_flush_(struct filelog *self, int type, bool flag) {
	if (!self)
		return true;

	if (!check_type(type))
		return true;

	FILELOG_CHECK_INIT(self);
	return logobj_set_every_flush(&self->log_group[type], flag);
}

void _filelog_flush_(struct filelog *self, int type) {
	if (!self)
		return;

	if (!check_type(type))
		return;

	FILELOG_CHECK_INIT(self);
	logobj_flush(&self->log_group[type]);
}

