
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

#ifdef WIN32
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
	int save_type;
	bool append_time;
	bool every_flush;
	char directory[512];
	char single_filename[64];
	char last_filename[1024];
};

struct filelog {
	bool is_init;
	struct logobj log_group[enum_log_type_max];
};

static struct filelog g_logobj = {false};
struct filelog *g_filelog_obj_ = &g_logobj;

static bool s_debug_print_show[enum_debug_max] = {true, true, true};



static inline void logobj_set_single_filename(struct logobj *self, const char *single_filename) {
	strncpy(self->single_filename, single_filename, sizeof(self->single_filename) - 1);
	self->single_filename[sizeof(self->single_filename) - 1] = '\0';
}

static inline void logobj_close(struct logobj *self) {
	if (self->fp) {
		fclose(self->fp);
		self->fp = NULL;
	}
}

static inline void logobj_flush(struct logobj *self) {
	if (self->fp) {
		fflush(self->fp);
	}
}

static bool logobj_set_save_type(struct logobj *self, int save_type) {
	if (!(
		save_type == st_every_day_split_dir_and_every_hour_split_file ||
		save_type == st_every_month_split_dir_and_every_day_split_file ||
		save_type == st_no_split_dir_and_every_day_split_file ||
		save_type == st_no_split_dir_and_not_split_file))
		return false;

	self->save_type = save_type;
	return true;
}

static inline bool logobj_append_time(struct logobj *self, bool flag) {
	bool old = self->append_time;
	self->append_time = flag;
	return old;
}

static inline bool logobj_every_flush(struct logobj *self, bool flag) {
	bool old = self->every_flush;
	self->every_flush = flag;
	return old;
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

	if (self->fp) {
		fclose(self->fp);
		self->fp = NULL;
	}

	memset(self->last_filename, 0, sizeof(self->last_filename));
}

static inline const char *logobj_get_directory(struct logobj *self) {
	return self->directory;
}

static void logobj_init(struct logobj *self) {
	const char *default_directory_name = "log_log";
	const char *default_single_filename = "log";
	self->fp = NULL;
	self->save_type = st_every_day_split_dir_and_every_hour_split_file;
	self->append_time = true;
	self->every_flush = true;

	logobj_set_directory(self, default_directory_name);
	logobj_set_single_filename(self, default_single_filename);
	memset(self->last_filename, 0, sizeof(self->last_filename));
}



int mymkdir_r(const char *directory) {
	int i, len;
	char tmp[1024 * 8] = {0};
	if (!strncpy(tmp, directory, sizeof(tmp) - 1))
		return -1;

	tmp[sizeof(tmp) - 1] = '\0';
	len = (int)strlen(tmp);
	if (len >= (int)sizeof(tmp) - 1)
		return -2;

	if (tmp[len - 1] != '/') {
		tmp[len] = '/';
		tmp[len + 1] = '\0';
		len += 1;
	}

	if (tmp[0] == '.')
		i = 1;
	else
		i = 0;

	for (i += 1; i < len; ++i) {
		if (tmp[i] != '/')
			continue;

		tmp[i] = '\0';
#ifdef WIN32
		if (access(tmp, 0) != 0)
#else
		if (access(tmp, F_OK) != 0)
#endif
		{
			if (my_mkdir(tmp) != 0)
				return -3;
		}
		tmp[i] = '/';
	}

	return 0;
}


void _log_printf_(int type, const char *filename, const char *func, 
		int line, const char *fmt, ...) {

	va_list args;
	assert(type >= enum_debug_print);
	assert(type < enum_debug_max);
	assert(fmt != NULL);

	if (!s_debug_print_show[type] || !fmt)
		return;

	if (enum_debug_print_call == type) {
		printf("file:%s, function:%s, line:%d ", filename, func, line);
	} else if (enum_debug_print_time == type) {
		time_t tval;
		struct tm tm_result;
		struct tm *currTM;
		time(&tval);
		currTM = safe_localtime(&tval, &tm_result);
		printf("[%04d-%02d-%02d %02d:%02d:%02d] ", 
				currTM->tm_year + 1900, currTM->tm_mon + 1, currTM->tm_mday, 
				currTM->tm_hour, currTM->tm_min, currTM->tm_sec);
	} else {

	}

	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
	printf("\n");
	fflush(stdout);
}

void _log_printf_set_show(int type, bool flag) {
	assert(type >= enum_debug_print && type < enum_debug_max);
	if (type < enum_debug_print || type >= enum_debug_max)
		return;

	s_debug_print_show[type] = flag;
}


static void filelog_init(struct filelog *self) {
	int i;
	for (i = 0; i < enum_log_type_max; ++i) {
		logobj_init(&self->log_group[i]);
	}

	logobj_set_save_type(&self->log_group[enum_log_type_error], st_no_split_dir_and_not_split_file);
	logobj_set_single_filename(&self->log_group[enum_log_type_error], "_error_log_");
	self->is_init = true;
}

#define LOG_CHECK_TYPE(type)											\
	assert(type == enum_log_type_log || type == enum_log_type_error)

#define FILELOG_CHECK_INIT(obj)											\
	do {																\
		if (!obj->is_init) {											\
			if (obj == g_filelog_obj_) {								\
				filelog_init(obj);										\
			}															\
		}																\
	} while (0)

/* log write file spend time */
/*#define _TEST_WRITE_LOG_NEED_TIME*/

#ifdef _TEST_WRITE_LOG_NEED_TIME
#include "crosslib.h"
#endif

void _filelog_write_(struct filelog *self, int type, const char *filename, 
		const char *func, int line, const char *fmt, ...) {

	const char *directory;
	struct logobj *info;
	time_t tval;
	struct tm tm_result;
	struct tm *currTM;
	char szFile[1024] = {0};

#ifdef _TEST_WRITE_LOG_NEED_TIME
	int64 begin, end;
#endif

	LOG_CHECK_TYPE(type);
	if (!self || !fmt)
		return;

	FILELOG_CHECK_INIT(self);
	info = &self->log_group[type];
	directory = info->directory;


	time(&tval);
	currTM = safe_localtime(&tval, &tm_result);

	{
		char path_dir[512] = {0};
		char subdir[64] = {0};
		char save_name[64] = {0};
		const char *path_dir_format = "%s/%s";

		switch(info->save_type) {
		case st_every_day_split_dir_and_every_hour_split_file:
			snprintf(subdir, sizeof(subdir) - 1, "%04d-%02d-%02d", 
					currTM->tm_year + 1900, currTM->tm_mon + 1, currTM->tm_mday);
			snprintf(save_name, sizeof(save_name) - 1, "%02d", currTM->tm_hour);
			break;
		case st_every_month_split_dir_and_every_day_split_file:
			snprintf(subdir, sizeof(subdir) - 1, "%04d-%02d", 
					currTM->tm_year + 1900, currTM->tm_mon + 1);
			snprintf(save_name, sizeof(save_name) - 1, "%04d-%02d-%02d", 
					currTM->tm_year + 1900, currTM->tm_mon + 1, currTM->tm_mday);
			break;
		case st_no_split_dir_and_every_day_split_file:
			path_dir_format = "%s%s";
			snprintf(save_name, sizeof(save_name) - 1, "%04d-%02d-%02d", 
					currTM->tm_year + 1900, currTM->tm_mon + 1, currTM->tm_mday);
			break;
		case st_no_split_dir_and_not_split_file:
			path_dir_format = "%s%s";
			strncpy(save_name, info->single_filename, sizeof(save_name) - 1);
			save_name[sizeof(save_name) - 1] = '\0';
			break;
		default:
			assert(false && "unknow save type...");
			return;
		}

		snprintf(path_dir, sizeof(path_dir) - 1, path_dir_format, directory, subdir);
		snprintf(szFile, sizeof(szFile) - 1, "%s/%s.log", path_dir, save_name);

		if (strcmp(info->last_filename, szFile) != 0) {
			strncpy(info->last_filename, szFile, sizeof(info->last_filename) - 1);
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
				while (res != 0 && i < max_times) {
					res = mymkdir_r(path_dir);
					++i;
				}
				assert(res == 0 && "mymkdir_r failed!");
			}
		}
	}

	if (!info->fp)
		info->fp = fopen(szFile, "a");

#ifdef _TEST_WRITE_LOG_NEED_TIME
	begin = get_microsecond();
#endif

	if (info->fp) {
		va_list args;
		if (enum_log_type_log == type) {
			if (info->append_time) {
				fprintf(info->fp, "[%04d-%02d-%02d %02d:%02d:%02d] ", 
						currTM->tm_year + 1900, currTM->tm_mon + 1, currTM->tm_mday, 
						currTM->tm_hour, currTM->tm_min, currTM->tm_sec);
			}

		} else {
			fprintf(info->fp, "[%04d-%02d-%02d %02d:%02d:%02d] [file:%s, function:%s, line:%d] ", 
					currTM->tm_year + 1900, currTM->tm_mon + 1, currTM->tm_mday, 
					currTM->tm_hour, currTM->tm_min, currTM->tm_sec, filename, func, line);
		}

		va_start(args, fmt);
		vfprintf(info->fp, fmt, args);
		va_end(args);
		fprintf(info->fp, "\n");

#ifdef _TEST_WRITE_LOG_NEED_TIME
		end = get_microsecond();
		fprintf(info->fp, "need: %d us\n", (int)(end - begin));
		fflush(info->fp);
		begin = get_microsecond();
		fprintf(info->fp, "fflush need:%d us\n", (int)(begin - end));
#endif

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

	free(self);
}

void _filelog_set_directory_(struct filelog *self, int type, const char *directory) {
	LOG_CHECK_TYPE(type);
	if (!self)
		return;

	FILELOG_CHECK_INIT(self);
	logobj_set_directory(&self->log_group[type], directory);
}

const char *_filelog_get_directory_(struct filelog *self, int type) {
	LOG_CHECK_TYPE(type);
	if (!self)
		return NULL;

	FILELOG_CHECK_INIT(self);
	return logobj_get_directory(&self->log_group[type]);
}

bool _filelog_set_save_type_(struct filelog *self, int type, int save_type) {
	LOG_CHECK_TYPE(type);
	if (!self)
		return false;

	FILELOG_CHECK_INIT(self);
	return logobj_set_save_type(&self->log_group[type], save_type);
}

bool _filelog_append_time_(struct filelog *self, int type, bool flag) {
	LOG_CHECK_TYPE(type);
	if (!self)
		return true;

	FILELOG_CHECK_INIT(self);
	return logobj_append_time(&self->log_group[type], flag);
}

bool _filelog_every_flush_(struct filelog *self, int type, bool flag) {
	LOG_CHECK_TYPE(type);
	if (!self)
		return true;

	FILELOG_CHECK_INIT(self);
	return logobj_every_flush(&self->log_group[type], flag);
}

void _filelog_flush_(struct filelog *self, int type) {
	LOG_CHECK_TYPE(type);
	if (!self)
		return;

	FILELOG_CHECK_INIT(self);
	logobj_flush(&self->log_group[type]);
}

