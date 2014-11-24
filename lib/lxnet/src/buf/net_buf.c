
/*
 * Copyright (C) lcinx
 * lcinx@163.com
*/

#include "net_buf.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "net_bufpool.h"
#include "net_blocklist.h"
#include "log.h"
#include "net_thread_buf.h"
#include "net_compress.h"
#include "ossome.h"

#ifndef min
#define min(a, b) (((a) < (b))? (a) : (b))
#endif
#ifndef max
#define max(a, b) (((a) > (b))? (a) : (b))
#endif

static bool s_enable_errorlog = false;

enum enum_some
{
	enum_unknow = 0,
	enum_compress,
	enum_uncompress,
	enum_encrypt,
	enum_decrypt,
};

struct block_size
{
	size_t bigblocksize;
	size_t smallblocksize;
};
static struct block_size s_blockinfo;



struct net_buf
{
	bool isbigbuf;				/* big or small flag. */
	char compress_falg;
	char crypt_falg;
	bool use_tgw;
	bool already_do_tgw;

	size_t raw_size_for_encrypt;
	size_t raw_size_for_compress;

	dofunc_f dofunc;
	void *do_logicdata;

	long io_limitsize;			/* io handle limit size. */

	struct blocklist iolist;	/* io block list. */

	struct blocklist logiclist;	/* if use compress/uncompress, logic block list is can use. */
};

static inline bool buf_is_use_compress (struct net_buf *self)
{
	return (self->compress_falg == enum_compress);
}

static inline bool buf_is_use_uncompress (struct net_buf *self)
{
	return (self->compress_falg == enum_uncompress);
}

static inline bool buf_is_use_encrypt (struct net_buf *self)
{
	return (self->crypt_falg == enum_encrypt);
}

static inline bool buf_is_use_decrypt (struct net_buf *self)
{
	return (self->crypt_falg == enum_decrypt);
}

static void blocklist_release (struct blocklist *self, struct blocklist *lst)
{
	self->canusesize = 0;
	self->datasize = 0;

	LOCK_LOCK(&self->list_lock);
	lst->head = self->head;
	lst->tail = self->tail;
	self->head = NULL;
	self->tail = NULL;
	LOCK_UNLOCK(&self->list_lock);
	LOCK_DELETE(&self->list_lock);
}

static void buf_real_release (struct net_buf *self)
{
	struct block *bk, *next;
	struct blocklist lst, temp;
	lst.head = NULL;
	lst.tail = NULL;
	temp.head = NULL;
	temp.tail = NULL;
	blocklist_release(&self->iolist, &lst);
	blocklist_release(&self->logiclist, &temp);
	
	/* unite */
	if (lst.tail)
	{
		if (temp.head)
		{
			lst.tail->next = temp.head;
			lst.tail = temp.tail;
		}
	}
	else
	{
		assert(lst.head == NULL);
		lst.head = temp.head;
		lst.tail = temp.tail;
	}

	if (!lst.head)
		return;

	if (self->isbigbuf)
	{
		bufpool_bigblock_lock();
		for (bk = lst.head; bk; bk = next)
		{
			next = bk->next;
			bufpool_releasebigblock_notlock(bk);
		}
		bufpool_bigblock_unlock();
	}
	else
	{
		bufpool_smallblock_lock();
		for (bk = lst.head; bk; bk = next)
		{
			next = bk->next;
			bufpool_releasesmallblock_notlock(bk);
		}
		bufpool_smallblock_unlock();
	}
}

#define _GET_BUF_SIZE(a) ((a->isbigblock) ? (s_blockinfo.bigblocksize) : (s_blockinfo.smallblocksize))

static struct block *buf_createblock (struct blocklist *lst)
{
	struct block *newbl = NULL;
	if (!lst->isbigblock)
	{
		newbl = (struct block *)bufpool_createsmallblock();
	}
	else
	{
		newbl = (struct block *)bufpool_createbigblock();
	}

	if (newbl)
	{
		if (!block_init(newbl, _GET_BUF_SIZE(lst)))
		{
			log_error("block size is too little!");
			exit(1);
		}
	}
	else
	{
		log_error("buf_createblock is null!, error!");
	}
	return newbl;
}

static void buf_init (struct net_buf *self, bool isbigbuf)
{
	assert(self != NULL);

	self->isbigbuf = isbigbuf;
	self->compress_falg = enum_unknow;
	self->crypt_falg = enum_unknow;
	self->use_tgw = false;
	self->already_do_tgw = false;

	self->raw_size_for_encrypt = 0;
	self->raw_size_for_compress = 0;

	self->dofunc = NULL;
	self->do_logicdata = NULL;

	self->io_limitsize = 0;

	blocklist_init(&self->iolist, isbigbuf);
	blocklist_init(&self->logiclist, isbigbuf);
}

/* 
 * create buf.
 * bigbuf --- big or small buf, if is true, then is big buf, or else is small buf.
 * */
struct net_buf *buf_create (bool bigbuf)
{
	struct net_buf *self = (struct net_buf *)bufpool_createbuf();
	if (!self)
	{
		log_error("	if (!self)");
		return NULL;
	}
	buf_init(self, bigbuf);
	return self;
}

/*
 * set buf encrypt function or decrypt function, and some logic data.
 * */
void buf_setdofunc (struct net_buf *self, dofunc_f func, void *logicdata)
{
	assert(func != NULL);
	if (!self || !func)
		return;
	self->dofunc = func;
	self->do_logicdata = logicdata;
}

/* release buf. */
void buf_release (struct net_buf *self)
{
	if (!self)
		return;
	buf_real_release(self);
	bufpool_releasebuf(self);
}


/* set buf handle limit size. */
void buf_set_limitsize (struct net_buf *self, int limit_len)
{
	assert(limit_len > 0);
	if (!self)
		return;
	if (limit_len <= 0)
		self->io_limitsize = 0;
	else
		self->io_limitsize = limit_len;
}

void buf_usecompress (struct net_buf *self)
{
	if (!self)
		return;
	self->compress_falg = enum_compress;
}

void buf_useuncompress (struct net_buf *self)
{
	if (!self)
		return;
	self->compress_falg = enum_uncompress;
}

void buf_useencrypt (struct net_buf *self)
{
	if (!self)
		return;
	self->crypt_falg = enum_encrypt;
}

void buf_usedecrypt (struct net_buf *self)
{
	if (!self)
		return;
	self->crypt_falg = enum_decrypt;
}

void buf_use_tgw (struct net_buf *self)
{
	if (!self)
		return;
	self->use_tgw = true;
}

void buf_set_raw_datasize (struct net_buf *self, size_t size)
{
	if (!self)
		return;
	self->raw_size_for_encrypt = size;
	self->raw_size_for_compress = size;
}

/* push len, if is more than the limit, return true.*/
bool buf_add_islimit (struct net_buf *self, size_t len)
{
	assert(len < _MAX_MSG_LEN);
	if (!self)
		return true;
	if (self->io_limitsize == 0)
		return false;
	/* limit compare as io datasize or logic datasize.*/
	if ((self->io_limitsize <= self->iolist.datasize) || 
			(self->io_limitsize <= (self->logiclist.datasize + len)))
		return true;
	return false;
}

/* test limit, buffer data as limit */
static bool buf_islimit (struct net_buf *self)
{
	if (!self)
		return true;
	if (self->io_limitsize == 0)
		return false;
	/* limit compare as io datasize or logic datasize.*/
	if (self->io_limitsize <= self->logiclist.datasize || 
			self->io_limitsize <= self->iolist.datasize)
		return true;
	return false;
}

/* test can recv data, if not recv, return true. */
bool buf_can_not_recv (struct net_buf *self)
{
	return buf_islimit(self);
}

/* test has data for send, if not has data,  return true. */
bool buf_can_not_send (struct net_buf *self)
{
	if (!self)
		return true;
	return ((self->iolist.datasize <= 0) && (self->logiclist.datasize <= 0));
}

/* test and free block*/
static void buf_test_and_freeblock (struct blocklist *lt)
{
	/*check is need free. */
	if (block_is_full(lt->head))
	{
		struct block *frbk = blocklist_popfront(lt);
		if (frbk)
		{
			if (lt->isbigblock)
				bufpool_releasebigblock(frbk);
			else
				bufpool_releasesmallblock(frbk);
		}
		else
		{
			assert(false && "struct block *frbk = blocklist_popfront(lt);");
			log_error("struct block *frbk = blocklist_popfront(lt);");
		}
	}
}

/*
 * some recv interface.
 *
 * */

static inline struct bufinfo blocklist_getwritebufinfo (struct blocklist *lst)
{
	struct bufinfo writebuf;
	struct block *bk = NULL;

	writebuf.buf = NULL;
	writebuf.len = 0;
	assert(lst != NULL);
	assert(lst->canusesize >= 0);
	if (lst->canusesize == 0)
	{
		bk = buf_createblock(lst);
		if (!bk)
			return writebuf;
		lst->canusesize = block_getwritesize(bk);
		blocklist_pushback(lst, bk);
	}
	else
	{
		bk = lst->tail;
	}
	writebuf.buf = block_getwritebuf(bk);
	writebuf.len = block_getwritesize(bk);
	assert(lst->canusesize == writebuf.len);
	return writebuf;
}

/* get write buffer info. */
struct bufinfo buf_getwritebufinfo (struct net_buf *self)
{
	struct bufinfo writebuf;
	writebuf.buf = NULL;
	writebuf.len = 0;

	if (buf_islimit(self))
		return writebuf;
	if (buf_is_use_uncompress(self))
		return blocklist_getwritebufinfo(&self->iolist);
	else
		return blocklist_getwritebufinfo(&self->logiclist);
}

static inline void blocklist_addwrite (struct blocklist *lst, int len)
{
	assert(lst != NULL);
	assert(len > 0);
	assert(lst->canusesize >= len);
	lst->canusesize -= len;

	/* add block write position first,
	 * then add datasize !.*/
	block_addwrite(lst->tail, len);

	atom_fetch_add(&lst->datasize, len);
}

static inline void blocklist_addread (struct blocklist *lst, int len)
{
	int tmpsize;
	assert(lst != NULL);

	/* first change datasize. */
	atom_fetch_add(&lst->datasize, (-len));

	while (len > 0)
	{
		tmpsize = min(block_getreadsize(lst->head), len);

		/* add block read position. */
		block_addread(lst->head, tmpsize);

		/* test free. */
		buf_test_and_freeblock(lst);

		len -= tmpsize;
	}
}

static bool buf_try_parse_tgw (struct blocklist *lst, char **buf, int *len)
{
	const int maxchecksize = 256;
	char tgwbuf[] = "\r\n\r\n";
	int findidx = 0;
	struct block *bk;
	int num = 0;
	int i;
	int canreadsize;
	char *f;
	
	*buf = NULL;
	*len = 0;
	for (bk = lst->head; bk; bk = bk->next)
	{
		f = block_getreadbuf(bk);
		canreadsize = block_getreadsize(bk);
		for (i = 0; i < canreadsize; ++i)
		{
			if (num >= maxchecksize)
				return false;

			if (f[i] == tgwbuf[findidx])
				findidx++;
			else
				findidx = 0;

			num++;
			if (findidx == 4)
			{
				blocklist_addread(lst, num);
				if (i + 1 < canreadsize)
				{
					*buf = &f[i + 1];
					*len = canreadsize - (i + 1);
				}
				return true;
			}
		}
	}

	return false;
}

/* add write position. */
void buf_addwrite (struct net_buf *self, char *buf, int len)
{
	char *tmpbuf = buf;
	int newlen = len;
	struct blocklist *lst;
	assert(len > 0);
	if (!self)
		return;

	if (buf_is_use_uncompress(self))
		lst = &self->iolist;
	else
		lst = &self->logiclist;

	if (self->use_tgw && (!self->already_do_tgw))
	{
		if (buf_try_parse_tgw(lst, &tmpbuf, &newlen))
			self->already_do_tgw = true;
	}

	/* decrypt opt. */
	if (buf_is_use_decrypt(self))
	{
		if (tmpbuf && (newlen > 0))
			self->dofunc(self->do_logicdata, tmpbuf, newlen);
	}

	/* end change data size. */
	blocklist_addwrite(lst, len);
}

static inline bool blocklist_pushdata (struct blocklist *lst, const char *msgbuf, int len)
{
	int writesize = 0;
	int pushsize = 0;
	struct block *bk = NULL;
	assert(lst != NULL);
	assert(msgbuf != NULL);
	assert(len > 0);
	assert(lst->canusesize >= 0);
	if (lst->canusesize == 0)
		bk = NULL;
	else
		bk = lst->tail;

	while (writesize < len)
	{
		if (lst->canusesize == 0)
		{
			assert((bk == NULL) || (block_getwritesize(bk) == 0));
			bk = buf_createblock(lst);
			if (!bk)
				return false;
			assert(lst->canusesize == 0);
			blocklist_pushback(lst, bk);
			lst->canusesize = block_getwritesize(bk);
		}
		else
		{
			pushsize = block_push(bk, (void *)&msgbuf[writesize], len - writesize);
			assert(pushsize > 0);

			/* push data first, then amend datasize. */
			atom_fetch_add(&lst->datasize, pushsize);

			assert(lst->canusesize >= pushsize);
			lst->canusesize -= pushsize;

			writesize += pushsize;
		}
	}
	assert(writesize == len);
	return true;
}

static inline char *blocklist_getmessage (struct blocklist *lst, char *buf, bool *needclose, int sockfd, int fromline)
{
	int readsize, needread, read_s;
	assert(lst != NULL);
	assert(buf != NULL);
	assert(needclose != NULL);
	
	readsize = 0;
	*needclose = false;

	if (!lst->isnewmsg)
	{
		/* first get the msg length. */
		if (lst->datasize < sizeof(lst->msglen))
			return NULL;
		buf_test_and_freeblock(lst);
		readsize = block_get(lst->head, &lst->msglen, sizeof(lst->msglen));
		assert(readsize > 0);
		assert(readsize <= sizeof(lst->msglen));

		if (readsize < sizeof(lst->msglen))
		{
			int lastsize = 0;
			buf_test_and_freeblock(lst);

			/* read next block. */
			assert(lst->head != NULL);
			lastsize = block_get(lst->head, &((char *)&lst->msglen)[readsize], sizeof(lst->msglen)-readsize);
			assert(lastsize > 0);
			assert(lastsize <= sizeof(lst->msglen));
			assert((lastsize + readsize) == sizeof(lst->msglen));
		}
		readsize = sizeof(lst->msglen);
		/* change datasize. */
		atom_fetch_add(&lst->datasize, (-readsize));

		lst->isnewmsg = true;
	}
	/* check packet length. */
	if ((lst->msglen < (int)sizeof(lst->msglen)) || (lst->msglen >= (int)((_MAX_MSG_LEN) - (int)sizeof(lst->msglen))))
	{
		*needclose = true;
		assert(false && "if ((lst->msglen < sizeof(lst->msglen)) || (lst->msglen >= _MAX_MSG_LEN))");
		if (s_enable_errorlog)
		{
			log_error("if ((lst->msglen < sizeof(lst->msglen)) || (lst->msglen >= _MAX_MSG_LEN)), msglen:%d, sockfd:%d, fromline:%d", lst->msglen, sockfd, fromline);
		}
		return NULL;
	}

	needread = lst->msglen - (int)(sizeof(lst->msglen));
	if (lst->datasize < needread)
		return NULL;
	assert(lst->head != NULL);
	readsize = 0;
	read_s = 0;
	
	assert((needread >= 0) && (needread < (_MAX_MSG_LEN-sizeof(lst->msglen))));	
	
	/* first load packet length. */
	*(int *)&buf[0] = lst->msglen;

	while (readsize < needread)
	{
		if (!lst->head)
		{
			assert(false && "if (!lst->head)");
			log_error("if (!lst->head)");
			*needclose = true;
			return NULL;
		}
		buf_test_and_freeblock(lst);

		read_s = block_get(lst->head, &buf[readsize+sizeof(lst->msglen)], needread - readsize);
		assert(read_s > 0);
		assert(read_s < _GET_BUF_SIZE(lst));
		readsize += read_s;
	}
	atom_fetch_add(&lst->datasize, (-needread));

	/* At this point should be a complete packet of! */
	lst->isnewmsg = false;
	lst->msglen = 0;
	return buf;
}

/* 
 * recv end, do something, if return flase, then close connect.
 * */
bool buf_recv_end_do (struct net_buf *self)
{
	if (!self)
		return false;
	if (buf_is_use_uncompress(self))
	{
		/* get a compress packet, uncompress it, and then push the queue. */
		struct blocklist *lst = &self->iolist;
		bool needclose = false;
		struct bufinfo srcbuf;
		struct bufinfo resbuf;
		bool pushresult;
		struct bufinfo compressbuf = threadbuf_get_compress_buf();
		char *quicklzbuf = threadbuf_get_quicklz_buf();
		char *msgbuf = threadbuf_get_msg_buf();
		for (;;)
		{
			srcbuf.buf = blocklist_getmessage(lst, msgbuf, &needclose, -99, __LINE__);
			if (needclose)
				return false;
			if (!srcbuf.buf)
				break;

			srcbuf.len = *(int *)srcbuf.buf;
			/* uncompress function will be responsible for header length of removed. */
			resbuf = compressmgr_uncompressdata(compressbuf.buf, compressbuf.len, quicklzbuf, srcbuf.buf, srcbuf.len);
			
			/* if return null, then uncompress error,
			 * uncompress error, probably because the uncompress buffer is less than uncompress data length. */
			if (!resbuf.buf)
			{
				log_error("uncompress buf is too small!");
				return false;
			}
			assert(resbuf.len > 0);
			pushresult = blocklist_pushdata(&self->logiclist, resbuf.buf, resbuf.len);
			assert(pushresult);
			if (!pushresult)
			{
				log_error("if (!pushresult)");
				return false;
			}
		}
	}
	return true;
}

/*
 * some send interface.
 *
 * */

static inline struct bufinfo blocklist_getreadbufinfo (struct blocklist *lst)
{
	struct bufinfo readbuf;
	readbuf.buf = NULL;
	readbuf.len = 0;
	assert(lst != NULL);

	/* if datasize > 0, so has data wait send. */
	if (lst->datasize > 0)
	{
		readbuf.buf = block_getreadbuf(lst->head);
		readbuf.len = block_getreadsize(lst->head);
	}
	return readbuf;
}

/* get read buffer info. */
struct bufinfo buf_getreadbufinfo (struct net_buf *self)
{
	struct bufinfo readbuf;
	struct blocklist *lst;

	readbuf.buf = NULL;
	readbuf.len = 0;

	if (!self)
		return readbuf;

	if (buf_is_use_compress(self))
		lst = &self->iolist;
	else
		lst = &self->logiclist;
	if (lst->datasize > 0)
	{
		struct block *bk = lst->head;
		readbuf.buf = block_getreadbuf(bk);
		readbuf.len = block_getreadsize(bk);
		if (buf_is_use_encrypt(self))
		{
			/* encrypt */
			struct bufinfo encrybuf = block_get_encrypt(bk);
			assert(encrybuf.len >= 0);
			if (self->raw_size_for_encrypt <= encrybuf.len)
			{
				encrybuf.len -= self->raw_size_for_encrypt;
				encrybuf.buf = &encrybuf.buf[self->raw_size_for_encrypt];
				self->raw_size_for_encrypt = 0;
				self->dofunc(self->do_logicdata, encrybuf.buf, encrybuf.len);
			}
			else
			{
				self->raw_size_for_encrypt -= encrybuf.len;
			}
		}
	}
	return readbuf;
}

/* add read position. */
void buf_addread (struct net_buf *self, int len)
{
	assert(len > 0);
	if (!self)
		return;
	if (buf_is_use_compress(self))
		blocklist_addread(&self->iolist, len);
	else
		blocklist_addread(&self->logiclist, len);
}

/* before send, do something. */
void buf_send_before_do (struct net_buf *self)
{
	if (!self)
		return;
	if (buf_is_use_compress(self))
	{
		/* get all can read data, compress it. (compress data header is compress function do.)*/
		bool pushresult = false;
		struct bufinfo resbuf;
		struct bufinfo srcbuf;
		struct bufinfo compressbuf = threadbuf_get_compress_buf();
		char *quicklzbuf = threadbuf_get_quicklz_buf();
		assert(compressbuf.len >= max((s_blockinfo.bigblocksize+sizeof(struct block)+(1024*16)), (s_blockinfo.smallblocksize+sizeof(struct block)+(1024*16))));
		for (;;)
		{
			srcbuf = blocklist_getreadbufinfo(&self->logiclist);
			assert(srcbuf.len >= 0);
			if ((srcbuf.len <= 0) || (!srcbuf.buf))
				break;
			if (self->raw_size_for_compress != 0)
			{
				if (self->raw_size_for_compress <= srcbuf.len)
				{
					srcbuf.len = self->raw_size_for_compress;
					self->raw_size_for_compress = 0;
				}
				else
				{
					self->raw_size_for_compress -= srcbuf.len;
				}

				resbuf.len = srcbuf.len;
				resbuf.buf = srcbuf.buf;
			}
			else
			{
				resbuf = compressmgr_do_compressdata(compressbuf.buf, quicklzbuf, srcbuf.buf, srcbuf.len);
			}

			assert(resbuf.len > 0);
			pushresult = blocklist_pushdata(&self->iolist, resbuf.buf, resbuf.len);
			assert(pushresult);
			if (!pushresult)
				log_error("if (!pushresult)");
			blocklist_addread(&self->logiclist, srcbuf.len);
		}
	}
}

/* push packet into the buffer. */
bool buf_pushmessage (struct net_buf *self, const char *msgbuf, int len)
{
	assert(msgbuf != NULL);
	assert(len > 0);
	if (!self || (len <= 0))
		return false;
	return blocklist_pushdata(&self->logiclist, msgbuf, len);
}

/* get packet from the buffer, if error, then needclose is true. */
char *buf_getmessage (struct net_buf *self, bool *needclose, char *buf, size_t bufsize, int sockfd)
{
	if (!self || !needclose)
		return NULL;
	if (self->use_tgw && (!self->already_do_tgw))
		return NULL;

	if (!buf || bufsize <= 0)
		return blocklist_getmessage(&self->logiclist, threadbuf_get_msg_buf(), needclose, sockfd, __LINE__);
	else
	{
		if (bufsize < _MAX_MSG_LEN)
		{
			assert(false && "why bufsize < _MAX_MSG_LEN");
			return NULL;
		}
		return blocklist_getmessage(&self->logiclist, buf, needclose, sockfd, __LINE__);
	}
}

/* 
 * create and init buf pool.
 * bigbufnum --- is bigbuf num.
 * bigbufsize --- is bigbuf size.
 * smallbufnum --- is small buf num.
 * smallbufsize --- is small buf size.
 * bufnum --- is buf num.
 * 
 * because a socket need 2 buf, so bufnum is *2, in init function.
 *
 * this function be able to call private thread buffer etc.
 * */
bool bufmgr_init (size_t bigbufnum, size_t bigbufsize, size_t smallbufnum, size_t smallbufsize, size_t bufnum)
{
	if ((bigbufnum == 0) || (bigbufsize == 0) || (smallbufnum == 0) || (smallbufsize == 0))
		return false;
	if (!threadbuf_init(_MAX_MSG_LEN, max((bigbufsize+sizeof(struct block)+(1024*16)), (smallbufsize+sizeof(struct block)+(1024*16)))))
		return false;
	if (bufpool_init(bigbufnum, bigbufsize+sizeof(struct block), smallbufnum, smallbufsize+sizeof(struct block), bufnum*2, sizeof(struct net_buf)))
	{
		s_blockinfo.bigblocksize = bigbufsize;
		s_blockinfo.smallblocksize = smallbufsize;
		return true;
	}
	return false;
}

/* release some buf. */
void bufmgr_release ()
{
	bufpool_release();
	threadbuf_release();
}

/* get some buf memroy info. */
void bufmgr_meminfo (char *buf, size_t bufsize)
{
	bufpool_meminfo(buf, bufsize);
}

/* enable/disable errorlog, and return before value. */
bool buf_set_enable_errorlog (bool flag)
{
	bool old = s_enable_errorlog;
	s_enable_errorlog = flag;
	return old;
}

/* get now enable or disable errorlog. */
bool buf_get_enable_errorlog ()
{
	return s_enable_errorlog;
}


