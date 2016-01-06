
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#ifndef _H_BUF_BLOCK_LIST_H_
#define _H_BUF_BLOCK_LIST_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "catomic.h"
#include "cthread.h"
#include "buf/block.h"
#include "buf/block_list_func.h"

struct blocklist {
	struct block *head;
	struct block *tail;

	bool is_new_message;					/* is new message? */
	int message_len;						/* current message length. */

	int message_maxlen;						/* message max length. */
	put_message_func custom_put_func;		/* custom put message function. */
	get_message_func custom_get_func;		/* custom get message function. */

	int can_write_size;						/* can write size for pusher. */
	catomic datasize;						/* this block list data total, pusher add and getter dec. */

	create_block_func create_func;
	release_block_func release_func;
	void *func_arg;
	size_t block_size;

	cspin list_lock;
};


void blocklist_init(struct blocklist *self, create_block_func create_func, 
		release_block_func release_func, void *func_arg, size_t block_size);

void blocklist_release(struct blocklist *self);

void blocklist_set_message_custom_arg(struct blocklist *self, 
		int message_maxlen, put_message_func pfunc, get_message_func gfunc);

static inline int blocklist_get_message_maxlen(struct blocklist *self) {
	return self->message_maxlen;
}

static inline int64 blocklist_get_datasize(struct blocklist *self) {
	return catomic_read(&self->datasize);
}



/*
 * ================================================================================
 * writer interface.
 * ================================================================================
 */
struct buf_info blocklist_get_write_bufinfo(struct blocklist *self);

void blocklist_add_write(struct blocklist *self, int len);

bool blocklist_put_data(struct blocklist *self, const void *data, int data_len);

bool blocklist_put_message(struct blocklist *self, const void *data, int data_len);






/*
 * ================================================================================
 * reader interface.
 * ================================================================================
 */
struct buf_info blocklist_get_read_bufinfo(struct blocklist *self);

void blocklist_add_read(struct blocklist *self, int len);

bool blocklist_get_data(struct blocklist *self, char *buf, int buf_size, int *read_len);

/*
 * if get new message succeed, return message length.
 * if do not gather together enough for a message, return 0.
 * if error, return less than 0.
 */
int blocklist_get_message(struct blocklist *self, char *buf, int buf_size);

#ifdef __cplusplus
}
#endif
#endif

