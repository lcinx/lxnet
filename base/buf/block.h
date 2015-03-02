
/*
 * Copyright (C) lcinx
 * lcinx@163.com
*/

#ifndef _H_BUF_BLOCK_H_
#define _H_BUF_BLOCK_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include "platform_config.h"
#include "buf_info.h"

#ifndef min
#define min(a, b)	(((a) < (b))? (a) : (b))
#endif

/* block buffer. */
struct block {
	int read;
	volatile int write;
	int process_pos;		/* process pos. */
	int maxsize;
	struct block *next;
	char buf[0];
};

static inline void block_init(struct block *self, int size) {
	assert(self != NULL);
	assert(size > (int)sizeof(struct block));
	self->read = 0;
	self->write = 0;
	self->process_pos = 0;
	self->maxsize = size - (int)sizeof(struct block);
	self->next = NULL;
}

static inline struct buf_info block_get_do_process(struct block *self) {
	struct buf_info pinfo;
	
	assert(self != NULL);
	assert(self->maxsize >= self->write);
	assert(self->write >= self->read);
	assert(self->write >= self->process_pos);

	pinfo.buf = NULL;
	pinfo.len = 0;
	if (self->write > self->process_pos) {
		pinfo.len = self->write - self->process_pos;
		pinfo.buf = &self->buf[self->process_pos];
		assert(pinfo.len > 0);
		assert(self->write >= pinfo.len);

		self->process_pos += pinfo.len;
		assert(self->write >= self->process_pos);
	}
	return pinfo;
}


static inline bool block_is_read_over(struct block *self) {
	assert(self != NULL);
	return (self->read == self->maxsize);
}

static inline bool block_is_write_over(struct block *self) {
	assert(self != NULL);
	return (self->write == self->maxsize);
}

static inline int block_getreadsize(struct block *self) {
	assert(self != NULL);
	assert(self->write >= self->read);
	return (self->write - self->read);
}

static inline char *block_getreadbuf(struct block *self) {
	assert(self != NULL);
	assert(self->write >= self->read);
	assert(self->maxsize > self->read);
	return &self->buf[self->read];
}

static inline void block_addread(struct block *self, int len) {
	assert(self != NULL);
	assert(len >= 0);
	assert(self->write >= (self->read + len));
	self->read += len;
}

static inline int block_getwritesize(struct block *self) {
	assert(self != NULL);
	assert(self->maxsize >= self->write);
	return (self->maxsize - self->write);
}

static inline char *block_getwritebuf(struct block *self) {
	assert(self != NULL);
	assert(self->maxsize > self->write);
	return &self->buf[self->write];
}

static inline void block_addwrite(struct block *self, int len) {
	assert(self != NULL);
	assert(len >= 0);
	assert(self->maxsize >= (self->write + len));
	self->write += len;
}

static inline int block_get(struct block *self, void *data, int len) {
	int readsize;
	assert(self != NULL);
	assert(self->write >= self->read);
	assert(data != NULL);
	assert(len != 0);
	readsize = min(block_getreadsize(self), len);
	memcpy(data, &self->buf[self->read], readsize);
	self->read += readsize;
	assert(self->read <= self->write);
	return readsize;
}

static inline int block_put(struct block *self, void *data, int len) {
	int writesize;
	assert(self != NULL);
	assert(self->maxsize > self->write);
	assert(data != NULL);
	assert(len != 0);
	writesize = min(block_getwritesize(self), len);
	memcpy(&self->buf[self->write], data, writesize);
	self->write += writesize;
	assert(self->write <= self->maxsize);
	return writesize;
}

#ifdef __cplusplus
}
#endif
#endif

