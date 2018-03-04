
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#include <assert.h>
#include <stdlib.h>
#include "catomic.h"
#include "cthread.h"
#include "net_thread_buf.h"
#include "quicklz.h"
#include "log.h"

#if QLZ_STREAMING_BUFFER != 0 || QLZ_COMPRESSION_LEVEL != 1
	#error define QLZ_STREAMING_BUFFER to the zero value and define QLZ_COMPRESSION_LEVEL to the 1 for this lib
#endif

#ifndef max
#define max(a, b) (((a) > (b))? (a) : (b))
#endif

/* max thread num. */
#define _MAX_SAFE_THREAD_NUM 64

struct thread_localuse {
	unsigned int thread_id;	/* thread id */
	char *buf;				/* buffer */
};

struct threadinfo {
	bool is_init;
	size_t msg_maxsize;
	size_t compress_maxsize;
	size_t quicklz_size;

	struct thread_localuse msgbuf[_MAX_SAFE_THREAD_NUM];		/* for getmsg temp buf */
	struct thread_localuse compressbuf[_MAX_SAFE_THREAD_NUM];	/* compress/uncompress. */
	struct thread_localuse quicklzbuf[_MAX_SAFE_THREAD_NUM];	/* for quicklz lib buffer. */
	catomic msgbuf_freeindex;
	catomic compressbuf_freeindex;
	catomic quicklzbuf_freeindex;
};

static struct threadinfo s_threadlock = {false};

static void *threadlocal_getbuf(struct thread_localuse self[_MAX_SAFE_THREAD_NUM], size_t need_size, catomic *free_index) {
	unsigned int current_thread_id = cthread_self_id();
	int index;
	for (index = 0; index < _MAX_SAFE_THREAD_NUM; ++index) {
		if (self[index].thread_id == current_thread_id) {
			if (self[index].buf == NULL) {
				log_error("if (self[index].buf == NULL)");
				exit(1);
			}
			return self[index].buf;
		}
	}

	/* check index and max thread num. */
	index = (int)catomic_fetch_add(free_index, 1);
	if (index < 0 || index >= _MAX_SAFE_THREAD_NUM) {
		log_error("if (index < 0 || index >= _MAX_SAFE_THREAD_NUM) index:%d, _MAX_SAFE_THREAD_NUM:%d", index, _MAX_SAFE_THREAD_NUM);
		exit(1);
	}

	/* if index can use, then create new thread buffer. */
	self[index].buf = (char *)malloc(need_size);
	if (!self[index].buf) {
		log_error("if (!self[index])");
		exit(1);
	}

	self[index].thread_id = current_thread_id;
	return self[index].buf;
}

/* get temp packet buf. */
struct buf_info threadbuf_get_msg_buf() {
	struct buf_info tempbuf;
	if (!s_threadlock.is_init) {
		log_error("if (!s_threadlock.is_init)");
		exit(1);
	}

	tempbuf.buf = threadlocal_getbuf(s_threadlock.msgbuf, s_threadlock.msg_maxsize, &s_threadlock.msgbuf_freeindex);
	tempbuf.len = s_threadlock.msg_maxsize;
	return tempbuf;
}

/* get temp compress/uncompress buf. */
struct buf_info threadbuf_get_compress_buf() {
	struct buf_info tempbuf;
	if (!s_threadlock.is_init) {
		log_error("if (!s_threadlock.is_init)");
		exit(1);
	}

	tempbuf.buf = threadlocal_getbuf(s_threadlock.compressbuf, s_threadlock.compress_maxsize, &s_threadlock.compressbuf_freeindex);
	tempbuf.len = s_threadlock.compress_maxsize;
	return tempbuf;
}

/* get quicklz compress lib buf. */
void *threadbuf_get_quicklz_buf() {
	if (!s_threadlock.is_init) {
		log_error("if (!s_threadlock.is_init)");
		exit(1);
	}

	return threadlocal_getbuf(s_threadlock.quicklzbuf, s_threadlock.quicklz_size, &s_threadlock.quicklzbuf_freeindex);
}


static void threadlocal_init(struct thread_localuse self[_MAX_SAFE_THREAD_NUM]) {
	int i;
	for (i = 0; i < _MAX_SAFE_THREAD_NUM; ++i) {
		self[i].thread_id = 0;
		self[i].buf = NULL;
	}
}

static void threadlocal_release(struct thread_localuse self[_MAX_SAFE_THREAD_NUM]) {
	int i;
	for (i = 0; i < _MAX_SAFE_THREAD_NUM; ++i) {
		self[i].thread_id = 0;
		if (self[i].buf) {
			free(self[i].buf);
			self[i].buf = NULL;
		}
	}
}

/*
 * Initialize thread private buffer set, for getmsg and compress, uncompress etc temp buf.
 *
 * msg_maxsize --- max packet size.
 * compress_maxsize --- max compress/uncompress buffer size.
 */
bool threadbuf_init(size_t msg_maxsize, size_t compress_maxsize) {
	if (s_threadlock.is_init)
		return false;

	if ((msg_maxsize == 0) || (compress_maxsize == 0))
		return false;

	s_threadlock.msg_maxsize = msg_maxsize;
	s_threadlock.compress_maxsize = compress_maxsize;
	s_threadlock.quicklz_size = max(sizeof(qlz_state_compress), sizeof(qlz_state_decompress));
	threadlocal_init(s_threadlock.msgbuf);
	threadlocal_init(s_threadlock.compressbuf);
	threadlocal_init(s_threadlock.quicklzbuf);

	catomic_set(&s_threadlock.msgbuf_freeindex, 0);
	catomic_set(&s_threadlock.compressbuf_freeindex, 0);
	catomic_set(&s_threadlock.quicklzbuf_freeindex, 0);
	s_threadlock.is_init = true;
	return true;
}

/* release thread private buffer set. */
void threadbuf_release() {
	if (!s_threadlock.is_init)
		return;

	s_threadlock.is_init = false;
	threadlocal_release(s_threadlock.msgbuf);
	threadlocal_release(s_threadlock.compressbuf);
	threadlocal_release(s_threadlock.quicklzbuf);
}

