
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "net_buf.h"
#include "net_bufpool.h"
#include "buf/block_list.h"
#include "net_thread_buf.h"
#include "net_compress.h"
#include "log.h"


static bool s_enable_errorlog = false;

enum enum_some {
	enum_unknow = 0,
	enum_compress,
	enum_uncompress,
	enum_encrypt,
	enum_decrypt,
};

struct block_size {
	size_t big_block_size;
	size_t small_block_size;
};
static struct block_size s_block_info;


struct net_buf {
	bool is_bigbuf;				/* big or small flag. */
	char compress_falg;
	char crypt_falg;
	bool use_tgw;
	volatile bool already_do_tgw;

	size_t raw_size_for_encrypt;
	size_t raw_size_for_compress;

	dofunc_f dofunc;
	void (*release_logicdata)(void *logicdata);
	void *do_logicdata;

	int io_limit_size;			/* io handle limit size. */

	struct blocklist iolist;	/* io block list. */

	struct blocklist logiclist;	/* if use compress/uncompress, logic block list is can use. */
};

static inline bool buf_is_use_compress(struct net_buf *self) {
	return (self->compress_falg == enum_compress);
}

static inline bool buf_is_use_uncompress(struct net_buf *self) {
	return (self->compress_falg == enum_uncompress);
}

static inline bool buf_is_use_encrypt(struct net_buf *self) {
	return (self->crypt_falg == enum_encrypt);
}

static inline bool buf_is_use_decrypt(struct net_buf *self) {
	return (self->crypt_falg == enum_decrypt);
}

static void buf_real_release(struct net_buf *self) {

	if (self->release_logicdata && self->do_logicdata) {
		self->release_logicdata(self->do_logicdata);
	}

	self->release_logicdata = NULL;
	self->do_logicdata = NULL;

	blocklist_release(&self->iolist);
	blocklist_release(&self->logiclist);
}

static void *create_small_block_f(void *arg, size_t size) {
	return bufpool_create_small_block();
}

static void release_small_block_f(void *arg, void *bobj) {
	bufpool_release_small_block(bobj);
}

static void *create_big_block_f(void *arg, size_t size) {
	return bufpool_create_big_block();
}

static void release_big_block_f(void *arg, void *bobj) {
	bufpool_release_big_block(bobj);
}


static void buf_init(struct net_buf *self, bool is_bigbuf) {
	assert(self != NULL);

	self->is_bigbuf = is_bigbuf;
	self->compress_falg = enum_unknow;
	self->crypt_falg = enum_unknow;
	self->use_tgw = false;
	self->already_do_tgw = false;

	self->raw_size_for_encrypt = 0;
	self->raw_size_for_compress = 0;

	self->dofunc = NULL;
	self->release_logicdata = NULL;
	self->do_logicdata = NULL;

	self->io_limit_size = 0;

	if (is_bigbuf) {
		blocklist_init(&self->iolist, 
				create_big_block_f, release_big_block_f, 
					NULL, s_block_info.big_block_size);
		blocklist_init(&self->logiclist, 
				create_big_block_f, release_big_block_f, 
					NULL, s_block_info.big_block_size);
	} else {
		blocklist_init(&self->iolist, 
				create_small_block_f, release_small_block_f, 
					NULL, s_block_info.small_block_size);
		blocklist_init(&self->logiclist, 
				create_small_block_f, release_small_block_f, 
					NULL, s_block_info.small_block_size);
	}
}

/*
 * create buf.
 * bigbuf --- big or small buf, if is true, then is big buf, or else is small buf.
 */
struct net_buf *buf_create(bool bigbuf) {
	struct net_buf *self = (struct net_buf *)bufpool_create_net_buf();
	if (!self) {
		log_error("if (!self)");
		return NULL;
	}
	buf_init(self, bigbuf);
	return self;
}

/*
 * set buf encrypt function or decrypt function, and some logic data.
 */
void buf_set_do_func(struct net_buf *self, dofunc_f func, 
		void (*release_logicdata)(void *logicdata), void *logicdata) {

	assert(func != NULL);
	if (!self || !func)
		return;
	self->dofunc = func;
	self->release_logicdata = release_logicdata;
	self->do_logicdata = logicdata;
}

/* release buf. */
void buf_release(struct net_buf *self) {
	if (!self)
		return;
	buf_real_release(self);
	bufpool_release_net_buf(self);
}


/* set buf handle limit size. */
void buf_set_limit_size(struct net_buf *self, int limit_len) {
	assert(limit_len > 0);
	if (!self)
		return;
	if (limit_len <= 0)
		self->io_limit_size = 0;
	else
		self->io_limit_size = limit_len;
}

void buf_use_compress(struct net_buf *self) {
	if (!self)
		return;
	self->compress_falg = enum_compress;
}

void buf_use_uncompress(struct net_buf *self) {
	if (!self)
		return;
	self->compress_falg = enum_uncompress;
}

void buf_use_encrypt(struct net_buf *self) {
	if (!self)
		return;
	self->crypt_falg = enum_encrypt;
}

void buf_use_decrypt(struct net_buf *self) {
	if (!self)
		return;
	self->crypt_falg = enum_decrypt;
}

void buf_use_tgw(struct net_buf *self) {
	if (!self)
		return;
	self->use_tgw = true;
}

void buf_set_raw_datasize(struct net_buf *self, size_t size) {
	if (!self)
		return;
	self->raw_size_for_encrypt = size;
	self->raw_size_for_compress = size;
}

int buf_get_now_data_size(struct net_buf *self) {
	if (!self)
		return 0;

	return (int)blocklist_get_datasize(&self->logiclist);
}

int buf_get_data_size(struct net_buf *self) {
	if (!self)
		return 0;

	return (int)(blocklist_get_datasize(&self->iolist) + blocklist_get_datasize(&self->logiclist));
}

/* push len, if is more than the limit, return true. */
bool buf_add_is_limit(struct net_buf *self, size_t len) {
	assert(len < _MAX_MSG_LEN);
	if (!self)
		return true;
	if (self->io_limit_size == 0)
		return false;
	/* limit compare as io datasize or logic datasize. */
	if ((self->io_limit_size <= blocklist_get_datasize(&self->iolist)) || 
			(self->io_limit_size <= (blocklist_get_datasize(&self->logiclist) + len)))
		return true;
	return false;
}

/* test limit, buffer data as limit */
static bool buf_islimit(struct net_buf *self) {
	if (!self)
		return true;
	if (self->io_limit_size == 0)
		return false;
	/* limit compare as io datasize or logic datasize. */
	if (self->io_limit_size <= blocklist_get_datasize(&self->logiclist) || 
			self->io_limit_size <= blocklist_get_datasize(&self->iolist))
		return true;
	return false;
}

/* test can recv data, if not recv, return true. */
bool buf_can_not_recv(struct net_buf *self) {
	return buf_islimit(self);
}

/* test has data for send, if not has data, return true. */
bool buf_can_not_send(struct net_buf *self) {
	if (!self)
		return true;
	return ((blocklist_get_datasize(&self->iolist) <= 0) &&
			(blocklist_get_datasize(&self->logiclist) <= 0));
}

/*
 * ================================================================================
 * some recv interface.
 * ================================================================================
 */

/* get write buffer info. */
struct buf_info buf_get_write_bufinfo(struct net_buf *self) {
	struct buf_info writebuf;
	writebuf.buf = NULL;
	writebuf.len = 0;

	if (buf_islimit(self))
		return writebuf;
	if (buf_is_use_uncompress(self))
		return blocklist_get_write_bufinfo(&self->iolist);
	else
		return blocklist_get_write_bufinfo(&self->logiclist);
}

static bool buf_try_parse_tgw(struct blocklist *lst, char **buf, int *len) {
	const int max_check_size = 256;
	char tgw_buf[] = "\r\n\r\n";
	struct block *bk = NULL;
	int find_idx = 0;
	int num = 0;

	*buf = NULL;
	*len = 0;
	for (bk = lst->head; bk; bk = bk->next) {
		char *f = block_get_readbuf(bk);
		int can_read_size = block_get_readsize(bk);
		int i = 0;
		for (i = 0; i < can_read_size; ++i) {
			if (num >= max_check_size)
				return false;

			if (f[i] == tgw_buf[find_idx])
				++find_idx;
			else
				find_idx = 0;

			++num;
			if (find_idx == 4) {
				blocklist_add_read(lst, num);
				if (i + 1 < can_read_size) {
					*buf = &f[i + 1];
					*len = can_read_size - (i + 1);
				}
				return true;
			}
		}
	}

	return false;
}

/* add write position. */
void buf_add_write(struct net_buf *self, char *buf, int len) {
	char *temp_buf = buf;
	int newlen = len;
	struct blocklist *lst;
	assert(len > 0);
	if (!self)
		return;

	if (buf_is_use_uncompress(self))
		lst = &self->iolist;
	else
		lst = &self->logiclist;

	if (self->use_tgw && (!self->already_do_tgw)) {
		if (buf_try_parse_tgw(lst, &temp_buf, &newlen))
			self->already_do_tgw = true;
	}

	/* decrypt opt. */
	if (buf_is_use_decrypt(self)) {
		if (temp_buf && (newlen > 0))
			self->dofunc(self->do_logicdata, temp_buf, newlen);
	}

	/* end change data size. */
	blocklist_add_write(lst, len);
}

/*
 * recv end, do something, if return flase, then close connect.
 */
bool buf_recv_end_do(struct net_buf *self) {
	if (!self)
		return false;
	if (buf_is_use_uncompress(self)) {
		/* get a compress packet, uncompress it, and then push the queue. */
		struct blocklist *lst = &self->iolist;
		int res;
		struct buf_info srcbuf;
		struct buf_info resbuf;
		bool pushresult;
		struct buf_info compressbuf = threadbuf_get_compress_buf();
		struct buf_info msgbuf = threadbuf_get_msg_buf();
		char *quicklzbuf = threadbuf_get_quicklz_buf();
		for (;;) {
			res = blocklist_get_message(lst, msgbuf.buf, msgbuf.len);
			if (res == 0)
				break;

			if (res < 0) {
				if (s_enable_errorlog) {
					log_error("msg length error. max message len:%d, message len:%d", (int)lst->message_maxlen, (int)lst->message_len);
				}
				return false;
			}

			srcbuf.buf = msgbuf.buf;
			srcbuf.len = res;

			/* uncompress function will be responsible for header length of removed. */
			resbuf = compressmgr_uncompressdata(compressbuf.buf, compressbuf.len, quicklzbuf, srcbuf.buf, srcbuf.len);

			/*
			 * if return null, then uncompress error,
			 * uncompress error, probably because the uncompress buffer is less than uncompress data length.
			 */
			if (!resbuf.buf) {
				log_error("uncompress buf is too small!");
				return false;
			}
			assert(resbuf.len > 0);
			pushresult = blocklist_put_data(&self->logiclist, resbuf.buf, resbuf.len);
			assert(pushresult);
			if (!pushresult) {
				log_error("if (!pushresult)");
				return false;
			}
		}
	}
	return true;
}

/*
 * ================================================================================
 * some send interface.
 * ================================================================================
 */

/* get read buffer info. */
struct buf_info buf_get_read_bufinfo(struct net_buf *self) {
	struct buf_info readbuf;
	struct blocklist *lst;

	readbuf.buf = NULL;
	readbuf.len = 0;

	if (!self)
		return readbuf;

	if (buf_is_use_compress(self))
		lst = &self->iolist;
	else
		lst = &self->logiclist;

	readbuf = blocklist_get_read_bufinfo(lst);
	if (readbuf.len > 0) {
		if (buf_is_use_encrypt(self)) {
			/* encrypt */
			struct buf_info encrybuf = block_get_do_process(lst->head);
			assert(encrybuf.len >= 0);
			if (self->raw_size_for_encrypt <= encrybuf.len) {
				encrybuf.len -= self->raw_size_for_encrypt;
				encrybuf.buf = &encrybuf.buf[self->raw_size_for_encrypt];
				self->raw_size_for_encrypt = 0;
				self->dofunc(self->do_logicdata, encrybuf.buf, encrybuf.len);
			} else {
				self->raw_size_for_encrypt -= encrybuf.len;
			}
		}
	}
	return readbuf;
}

/* add read position. */
void buf_add_read(struct net_buf *self, int len) {
	assert(len > 0);
	if (!self)
		return;
	if (buf_is_use_compress(self))
		blocklist_add_read(&self->iolist, len);
	else
		blocklist_add_read(&self->logiclist, len);
}

/* before send, do something. */
void buf_send_before_do(struct net_buf *self) {
	if (!self)
		return;
	if (buf_is_use_compress(self)) {
		/* get all can read data, compress it. (compress data header is compress function do.) */
		bool pushresult = false;
		struct buf_info resbuf;
		struct buf_info srcbuf;
		struct buf_info compressbuf = threadbuf_get_compress_buf();
		char *quicklzbuf = threadbuf_get_quicklz_buf();
		for (;;) {
			srcbuf = blocklist_get_read_bufinfo(&self->logiclist);
			srcbuf.len = min(srcbuf.len, self->logiclist.message_maxlen);
			assert(srcbuf.len >= 0);
			if ((srcbuf.len <= 0) || (!srcbuf.buf))
				break;
			if (self->raw_size_for_compress != 0) {
				if (self->raw_size_for_compress <= srcbuf.len) {
					srcbuf.len = self->raw_size_for_compress;
					self->raw_size_for_compress = 0;
				} else {
					self->raw_size_for_compress -= srcbuf.len;
				}

				resbuf.len = srcbuf.len;
				resbuf.buf = srcbuf.buf;
			} else {
				resbuf = compressmgr_do_compressdata(compressbuf.buf, quicklzbuf, srcbuf.buf, srcbuf.len);
			}

			assert(resbuf.len > 0);
			pushresult = blocklist_put_data(&self->iolist, resbuf.buf, resbuf.len);
			assert(pushresult);
			if (!pushresult)
				log_error("if (!pushresult)");
			blocklist_add_read(&self->logiclist, srcbuf.len);
		}
	}
}

/* push packet into the buffer. */
bool buf_put_message(struct net_buf *self, const void *msg_data, int len) {
	assert(msg_data != NULL);
	assert(len > 0);
	if (!self || (len <= 0))
		return false;
	return blocklist_put_message(&self->logiclist, msg_data, len);
}

/* push data into the buffer. */
bool buf_put_data(struct net_buf *self, const void *data, int len) {
	assert(data != NULL);
	assert(len > 0);
	if (!self || (len <= 0))
		return false;
	return blocklist_put_data(&self->logiclist, data, len);
}

/* get packet from the buffer, if error, then need_close is true. */
char *buf_get_message(struct net_buf *self, bool *need_close, char *buf, size_t bufsize) {
	struct buf_info dst;
	int res;
	if (!self || !need_close)
		return NULL;
	if (self->use_tgw && (!self->already_do_tgw))
		return NULL;

	if (!buf || bufsize <= 0) {
		dst = threadbuf_get_msg_buf();
	} else {
		if (bufsize < _MAX_MSG_LEN) {
			assert(false && "why bufsize < _MAX_MSG_LEN");
			return NULL;
		}

		dst.buf = buf;
		dst.len = (int)bufsize;
	}

	res = blocklist_get_message(&self->logiclist, dst.buf, dst.len);
	if (res == 0) {
		return NULL;
	} else if (res > 0) {
		return dst.buf;
	} else {
		*need_close = true;
		if (s_enable_errorlog) {
			log_error("msg length error. max message len:%d, message len:%d", (int)self->logiclist.message_maxlen, (int)self->logiclist.message_len);
		}
		return NULL;
	}
}

/* get data from the buffer, if error, then need_close is true. */
char *buf_get_data(struct net_buf *self, bool *need_close, char *buf, int bufsize, int *datalen) {
	struct blocklist *lst;
	if (!self || !need_close)
		return NULL;
	if (self->use_tgw && (!self->already_do_tgw))
		return NULL;

	if (!buf || bufsize <= 0 || !datalen)
		return NULL;

	lst = &self->logiclist;
	if (blocklist_get_datasize(lst) <= 0)
		return NULL;

	if (!blocklist_get_data(lst, buf, bufsize, datalen)) {
		*need_close = true;
		if (s_enable_errorlog) {
			log_error("buf_get_data error.");
		}
		return NULL;
	}

	return buf;
}

/* find data end size from the buffer. */
int buf_find_data_end_size(struct net_buf *self, const char *data, int datalen) {
	if (!self)
		return -1;

	return blocklist_find_data_end_size(&self->logiclist, data, datalen);
}


/*
 * create and init buf pool.
 * big_buf_num --- is bigbuf num.
 * big_buf_size --- is bigbuf size.
 * small_buf_num --- is small buf num.
 * small_buf_size --- is small buf size.
 * buf_num --- is buf num.
 *
 * because a socket need 2 buf, so buf_num is * 2, in init function.
 *
 * this function be able to call private thread buffer etc.
 */
bool bufmgr_init(size_t big_buf_num, size_t big_buf_size, 
		size_t small_buf_num, size_t small_buf_size, size_t buf_num) {

	if ((big_buf_num == 0) || (big_buf_size == 0) || (small_buf_num == 0) || (small_buf_size == 0))
		return false;

	if (!threadbuf_init(_MAX_MSG_LEN + 512, _MAX_MSG_LEN + 512))
		return false;

	big_buf_size += sizeof(struct block);
	small_buf_size += sizeof(struct block);

	if (!bufpool_init(big_buf_num, big_buf_size, 
					 small_buf_num, small_buf_size, 
					 buf_num * 2, sizeof(struct net_buf))) {
		return false;
	}

	s_block_info.big_block_size = big_buf_size;
	s_block_info.small_block_size = small_buf_size;
	return true;
}

/* release some buf. */
void bufmgr_release() {
	bufpool_release();
	threadbuf_release();
}

/* get some buf memroy info. */
void bufmgr_get_memory_info(char *buf, size_t bufsize) {
	bufpool_get_memory_info(buf, bufsize);
}

/* enable/disable errorlog, and return before value. */
bool buf_set_enable_errorlog(bool flag) {
	bool old = s_enable_errorlog;
	s_enable_errorlog = flag;
	return old;
}

/* get now enable or disable errorlog. */
bool buf_get_enable_errorlog() {
	return s_enable_errorlog;
}

