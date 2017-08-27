
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#include <limits.h>
#include "buf/block_list.h"


void blocklist_init(struct blocklist *self, create_block_func create_func, 
		release_block_func release_func, void *func_arg, size_t block_size) {

	assert(create_func != NULL);
	assert(release_func != NULL);
	assert(block_size > sizeof(struct block));
	assert(block_size < INT_MAX && "the size of the block need less than INT_MAX");

	self->head = NULL;
	self->tail = NULL;

	self->is_new_message = false;
	self->message_len = 0;

	self->message_maxlen = 128 * 1024;
	self->custom_put_func = NULL;
	self->custom_get_func = NULL;

	self->can_write_size = 0;
	catomic_set(&self->datasize, 0);

	self->create_func = create_func;
	self->release_func = release_func;
	self->func_arg = func_arg;
	self->block_size = block_size;

	cspin_init(&self->list_lock);
}

static inline struct block *blocklist_pop_front(struct blocklist *self) {
	struct block *bk;
	cspin_lock(&self->list_lock);
	bk = self->head;
	if (self->head) {
		self->head = self->head->next;
		if (self->tail == bk) {
			self->tail = NULL;
			assert(self->head == NULL);
			assert(bk->next == NULL);
		}
	}
	cspin_unlock(&self->list_lock);
	return bk;
}

static inline void blocklist_push_back(struct blocklist *self, struct block *bk) {
	cspin_lock(&self->list_lock);
	bk->next = NULL;
	if (self->tail) {
		self->tail->next = bk;
	} else {
		assert(self->head == NULL);
		self->head = bk;
	}
	self->tail = bk;
	cspin_unlock(&self->list_lock);
}

void blocklist_release(struct blocklist *self) {
	while (true) {
		struct block *bk = blocklist_pop_front(self);
		if (!bk)
			break;

		self->release_func(self->func_arg, bk);
	}

	self->head = NULL;
	self->tail = NULL;

	self->is_new_message = false;
	self->message_len = 0;

	self->message_maxlen = 0;
	self->custom_put_func = NULL;
	self->custom_get_func = NULL;

	self->can_write_size = 0;
	catomic_set(&self->datasize, 0);

	self->create_func = NULL;
	self->release_func = NULL;
	self->func_arg = NULL;
	self->block_size = 0;

	cspin_destroy(&self->list_lock);
}

void blocklist_set_message_custom_arg(struct blocklist *self, 
		int message_maxlen, put_message_func pfunc, get_message_func gfunc) {

	assert(message_maxlen > 0);
	if (message_maxlen <= 0)
		return;

	self->message_maxlen = message_maxlen;
	self->custom_put_func = pfunc;
	self->custom_get_func = gfunc;
}

static inline struct block *blocklist_create_block(struct blocklist *self) {
	struct block *bk = (struct block *)self->create_func(self->func_arg, self->block_size);
	if (bk) {
		block_init(bk, (int)self->block_size);
	}
	return bk;
}

static inline void blocklist_check_free_block(struct blocklist *self) {
	/* check is need free. */
	if (block_is_read_over(self->head) && block_is_write_over(self->head)) {
		struct block *bk = blocklist_pop_front(self);
		self->release_func(self->func_arg, bk);
	}
}

static inline bool blocklist_check_alloc_block(struct blocklist *self) {
	assert(self->can_write_size >= 0);
	if (self->can_write_size == 0) {
		struct block *bk = blocklist_create_block(self);
		if (!bk)
			return false;

		blocklist_push_back(self, bk);
		self->can_write_size = block_get_writesize(bk);
	}

	return true;
}



/*
 * ================================================================================
 * writer interface.
 * ================================================================================
 */
struct buf_info blocklist_get_write_bufinfo(struct blocklist *self) {
	struct buf_info writebuf;
	writebuf.buf = NULL;
	writebuf.len = 0;

	if (blocklist_check_alloc_block(self)) {
		writebuf.buf = block_get_writebuf(self->tail);
		writebuf.len = block_get_writesize(self->tail);
	}

	assert(self->can_write_size == writebuf.len);
	return writebuf;
}

void blocklist_add_write(struct blocklist *self, int len) {
	assert(self != NULL);
	assert(len > 0);
	assert(self->can_write_size >= len);

	self->can_write_size -= len;

	/*
	 * add block write position first,
	 * then add datasize!
	 */
	block_add_write(self->tail, len);

	catomic_fetch_add(&self->datasize, len);
}

bool blocklist_put_data(struct blocklist *self, const void *data, int datalen) {
	int writesize, putsize;
	const char *data_str = (const char *)data;
	assert(self != NULL);
	assert(data != NULL);
	assert(datalen > 0);
	assert(self->can_write_size >= 0);
	if (datalen <= 0)
		return false;

	writesize = 0;
	putsize = 0;
	while (writesize < datalen) {
		if (!blocklist_check_alloc_block(self)) {
			assert(false && "why create new block failed? error!");
			return false;
		}

		putsize = block_put(self->tail, (void *)&data_str[writesize], datalen - writesize);
		assert(putsize > 0);

		assert(self->can_write_size >= putsize);
		self->can_write_size -= putsize;

		writesize += putsize;

		catomic_fetch_add(&self->datasize, putsize);
	}

	assert(writesize == datalen);
	return true;
}

bool blocklist_put_message(struct blocklist *self, const void *data, int datalen) {
	assert(self != NULL);
	assert(data != NULL);
	assert(datalen > 0);
	assert(datalen <= self->message_maxlen && 
			"need put data length greater than message max length, error!");

	if (datalen > self->message_maxlen)
		return false;

	if (!self->custom_put_func) {
		return blocklist_put_data(self, data, datalen);
	} else {
		return self->custom_put_func(blocklist_put_data, self, data, datalen);
	}
}






/*
 * ================================================================================
 * reader interface.
 * ================================================================================
 */
struct buf_info blocklist_get_read_bufinfo(struct blocklist *self) {
	struct buf_info readbuf;
	int max_readsize = (int)blocklist_get_datasize(self);

	readbuf.buf = NULL;
	readbuf.len = 0;

	if (max_readsize > 0) {
		readbuf.buf = block_get_readbuf(self->head);
		readbuf.len = min(block_get_readsize(self->head), max_readsize);
	}

	return readbuf;
}

void blocklist_add_read(struct blocklist *self, int len) {
	assert(self != NULL);
	assert(len > 0);
	assert(blocklist_get_datasize(self) >= len);

	/* add block read position. */
	block_add_read(self->head, len);

	catomic_fetch_add(&self->datasize, (-len));

	blocklist_check_free_block(self);
}

static int blocklist_get_data_by_size(struct blocklist *self, 
		char *buf, int buf_size, int needread) {

	int readsize, getsize;
	assert(self != NULL);
	assert(buf != NULL);
	assert(buf_size > 0);
	assert(buf_size >= needread);
	if (buf_size < needread || needread < 0)
		return -1;

	if (blocklist_get_datasize(self) < needread)
		return 0;

	readsize = 0;
	while (readsize < needread) {
		blocklist_check_free_block(self);
		if (!self->head) {
			assert(false && "why head node is null? error!");
			return -1;
		}

		getsize = block_get(self->head, &buf[readsize], needread - readsize);
		assert(getsize > 0);
		readsize += getsize;

		catomic_fetch_add(&self->datasize, (-getsize));
	}

	assert(readsize == needread);

	blocklist_check_free_block(self);
	return readsize;
}

bool blocklist_get_data(struct blocklist *self, char *buf, int buf_size, int *read_len) {
	int needread;
	assert(self != NULL);
	assert(buf != NULL);
	assert(buf_size > 0);
	assert(blocklist_get_datasize(self) > 0);

	*read_len = 0;
	needread = (int)min(buf_size, blocklist_get_datasize(self));
	assert(needread > 0);

	if (blocklist_get_data_by_size(self, buf, buf_size, needread) == needread) {
		*read_len = needread;
		return true;
	}

	return false;
}

static inline bool find_data_from_all_block(struct block *bk, 
					int idx_offset, const char *data, int datalen) {

	for (; bk; bk = bk->next) {
		char *temp = block_get_readbuf(bk);
		int can_check_size = block_get_readsize(bk) - idx_offset;

		if (can_check_size >= datalen) {
			return memcmp(&temp[idx_offset], data, datalen) == 0;
		}

		if (can_check_size > 0) {
			if (memcmp(&temp[idx_offset], data, can_check_size) != 0)
				return false;
		}

		idx_offset = 0;
		data += can_check_size;
		datalen -= can_check_size;
	}

	return false;
}

int blocklist_find_data_end_size(struct blocklist *self, const char *data, int datalen) {
	struct block *bk;
	int check_size = 0;
	int max_check = (int)blocklist_get_datasize(self);
	assert(self != NULL);
	assert(data != NULL);
	assert(datalen > 0);
	if (!data || datalen <= 0)
		return -1;

	if (max_check < datalen)
		return 0;

	for (bk = self->head; bk; bk = bk->next) {
		int idx_offset = 0;
		int can_check_size = min(block_get_readsize(bk), max_check - check_size);
		for (; idx_offset < can_check_size; ++idx_offset) {
			if (find_data_from_all_block(bk, idx_offset, data, datalen)) {
				return check_size + datalen;
			}

			++check_size;

			if (max_check - check_size < datalen) {
				return 0;
			}

			assert(max_check > check_size);
		}
	}

	return 0;
}

/*
 * if get new message succeed, return message length.
 * if do not gather together enough for a message, return 0.
 * if error, return less than 0.
 */
int blocklist_get_message(struct blocklist *self, char *buf, int buf_size) {
	assert(self != NULL);
	assert(buf != NULL);
	assert(buf_size >= self->message_maxlen && 
			"get message need greater than message max length buffer, error!");

	if (!self->custom_get_func) {
		/* check new message. */
		const int length_len = 4;
		int res = 0;
		assert((int)sizeof(self->message_len) >= length_len);
		if (!self->is_new_message) {
			res = blocklist_get_data_by_size(self, 
					(char *)&self->message_len, length_len, length_len);

			if (res == 0) {
				return 0;
			} else if (res < 0) {
				assert(false && "why get new message length failed? error!");
				return res;
			}

			assert(res == length_len);
			self->is_new_message = true;
		}

		/* check message length. */
		if (self->message_len < length_len || self->message_len > buf_size) {
			assert(false && "new message length is invalid, error!");
			return -1;
		}

		/* first load message length. */
		memcpy(&buf[0], &self->message_len, length_len);
		res = blocklist_get_data_by_size(self, &buf[length_len], 
				(self->message_len - length_len), (self->message_len - length_len));

		if (res == 0) {
			return 0;
		} else if (res < 0) {
			assert(false && "why get new message data failed? error!");
			return res;
		}

		assert(res == (self->message_len - length_len));

		if (self->message_len - length_len != res) {
			assert(false && "why get new message data length is not need read len?");
			return -1;
		}

		res = self->message_len;
		self->is_new_message = false;
		self->message_len = 0;
		return res;

	} else {
		return self->custom_get_func(blocklist_get_data_by_size, self, 
									blocklist_get_datasize(self), 
									&self->is_new_message, &self->message_len, 
									buf, buf_size);
	}
}

