
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#include "net_compress.h"
#include <assert.h>
#include "quicklz.h"
#include "log.h"

/*#define print_debug debug_log*/
#define print_debug(...) ((void) 0)

/* 
 * uncompress data.
 * uncompressbuf --- is uncompress buffer.
 * uncompresslen --- is uncompress buffer len.
 * quicklzbuf --- is quicklz lib need buffer.
 * data --- is source data.
 * len --- is source data len.
 *
 * return uncompress result data info.
 *
 * Attention: Will remove the original header length, and then uncompress, because the header length is the compressed added.
 * */
struct buf_info compressmgr_uncompressdata(char *uncompressbuf, int uncompresslen, char *quicklzbuf, char *data, int len) {
	size_t needsize;
	struct buf_info resbuf;
	resbuf.buf= NULL;
	resbuf.len = 0;
	
	print_debug("un compress before, msg len:%d\n", *(int *)data);

	assert(data != NULL);
	assert(len > 0);
	needsize = qlz_size_decompressed(&data[sizeof(int)]);
	if (needsize > uncompresslen)
	{
		return resbuf;
	}
	resbuf.buf = uncompressbuf;
	resbuf.len = (int)qlz_decompress(&data[sizeof(int)], resbuf.buf, (qlz_state_decompress *)quicklzbuf);

	print_debug("un compress end, msg len:%d\n", resbuf.len);
	return resbuf;
}

/* compress data.
 * compressbuf --- is compress buffer.
 * quicklzbuf --- is quicklz lib need buffer.
 * data --- is source data.
 * len --- is source data len.
 *
 * return compress result data info.
 * 
 * Attention: Will form a compressed data packet, plus the header length.
 * */
struct buf_info compressmgr_do_compressdata(char *compressbuf, char *quicklzbuf, char *data, int len) {
	struct buf_info resbuf;
	assert(data != NULL);
	assert(len > 0);
	print_debug("compress before, msg len:%d\n", len);

	resbuf.buf = compressbuf;
	resbuf.len = (int)qlz_compress(data, &resbuf.buf[sizeof(int)], len, (qlz_state_compress *)quicklzbuf);

	resbuf.len += sizeof(int);
	*(int *)resbuf.buf = resbuf.len;
	assert(resbuf.len != 0);

	print_debug("compress end, msg len:%d\n", *(int *)resbuf.buf);
	return resbuf;
}




