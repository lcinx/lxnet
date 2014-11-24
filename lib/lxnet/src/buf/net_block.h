
/*
 * Copyright (C) lcinx
 * lcinx@163.com
*/

#ifndef _H_NET_BLOCK_H_
#define _H_NET_BLOCK_H_

#ifdef __cplusplus
extern "C"{
#endif

#include <stddef.h>
#include <assert.h>
#include "alway_inline.h"
#include "buf_info.h"

#ifndef min
#define min(a, b) (((a) < (b))? (a) : (b))
#endif

/* block buffer. */
struct block
{
	volatile size_t read;
	volatile size_t write;
	size_t encrypt_pos;	/* encrypt pos. */
	size_t maxsize;
	struct block *next;
	char buf[0];
};

static inline struct bufinfo block_get_encrypt (struct block *self)
{
	struct bufinfo encryptbuf;
	
	assert(self->maxsize > self->write);
	assert(self->write >= self->read);
	assert(self->write >= self->encrypt_pos);

	encryptbuf.buf = &self->buf[self->encrypt_pos];
	encryptbuf.len = 0;
	if (self->write > self->encrypt_pos)
	{
		encryptbuf.len = self->write - self->encrypt_pos;
		assert(encryptbuf.len > 0);
		assert(self->write >= encryptbuf.len);

		self->encrypt_pos += encryptbuf.len;
		assert(self->write >= self->encrypt_pos);
	}
	return encryptbuf;
}

static inline bool block_is_full (struct block *self)
{
	return ((self->read == (self->maxsize - 1)) && (self->write == (self->maxsize - 1)));
}

static inline size_t block_getreadsize (struct block *self)
{
	assert(self != NULL);
	assert(self->write >= self->read);
	return (self->write - self->read);
}

static inline void block_addwrite (struct block *self, int len)
{
	assert(self != NULL);
	assert(len >= 0);
	assert(self->maxsize > (self->write + len));
	self->write += len;
}

static inline void block_addread (struct block *self, int len)
{
	assert(self != NULL);
	assert(len >= 0);
	assert(self->write >= (self->read + len));
	self->read += len;
}

static inline size_t block_getwritesize (struct block *self)
{
	assert(self != NULL);
	assert(self->maxsize > self->write);
	return (self->maxsize - self->write - 1);
}

static inline char *block_getreadbuf (struct block *self)
{
	assert(self != NULL);
	assert(self->write >= self->read);
	assert(self->maxsize > self->read);
	return &self->buf[self->read];
}

static inline char *block_getwritebuf (struct block *self)
{
	assert(self != NULL);
	assert(self->maxsize > self->write);
	return &self->buf[self->write];
}

static inline size_t block_push (struct block *self, void *data, size_t len)
{
	int writesize;
	assert(self != NULL);
	assert(self->maxsize > self->write);
	assert(data != NULL);
	assert(len != 0);
	writesize = min(block_getwritesize(self), len);
	memcpy(&self->buf[self->write], data, writesize);
	self->write += writesize;
	assert(self->write < self->maxsize);
	return writesize;
}

static inline size_t block_get (struct block *self, void *data, size_t len)
{
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

static inline bool block_init (struct block *self, size_t size)
{
	if (size <= sizeof(struct block))
	{
		return false;
	}
	assert(self != NULL);
	self->read = 0;
	self->write = 0;
	self->encrypt_pos = 0;
	self->maxsize = size;
	self->next = NULL;
	return true;
}

#ifdef __cplusplus
}
#endif
#endif

