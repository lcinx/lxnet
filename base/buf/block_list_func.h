
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#ifndef _H_BUF_BLOCK_LIST_FUNC_H_
#define _H_BUF_BLOCK_LIST_FUNC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "platform_config.h"

struct blocklist;

typedef void *(*create_block_func)(void *arg, size_t size);
typedef void (*release_block_func)(void *arg, void *bobj);
typedef bool (*put_data_func)(struct blocklist *self, const void *data, int datalen);
typedef int (*get_data_func)(struct blocklist *self, char *buf, int buf_size, int needread);
typedef bool (*put_message_func)(put_data_func func, void *arg, const void *data, int datalen);
typedef int (*get_message_func)(get_data_func func, void *arg, int64 datasize, 
								bool *is_new_message, int *message_len, 
								char *buf, int buf_size);

#ifdef __cplusplus
}
#endif
#endif

