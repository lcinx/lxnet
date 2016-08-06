
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#ifndef _H_NET_COMPRESS_H_
#define _H_NET_COMPRESS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "buf/buf_info.h"

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
 */
struct buf_info compressmgr_uncompressdata(char *uncompressbuf, int uncompresslen, char *quicklzbuf, char *data, int len);

/*
 * compress data.
 * compressbuf --- is compress buffer.
 * quicklzbuf --- is quicklz lib need buffer.
 * data --- is source data.
 * len --- is source data len.
 *
 * return compress result data info.
 *
 * Attention: Will form a compressed data packet, plus the header length.
 */
struct buf_info compressmgr_do_compressdata(char *compressbuf, char *quicklzbuf, char *data, int len);

#ifdef __cplusplus
}
#endif
#endif

