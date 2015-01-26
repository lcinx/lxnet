
/*
 * Copyright (C) lcinx
 * lcinx@163.com
*/

#ifndef _H_NET_BLOCKLIST_H_
#define _H_NET_BLOCKLIST_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "ossome.h"
#include "net_block.h"

struct blocklist {
	bool isbigblock;		/* small or big block ? */
	bool isnewmsg;			/* is new packet ? */
	int msglen;				/* current packet length. */

	int canusesize;			/* can use size, pusher or getter, signle maintenance,
							   if is 0, then head or tail is full or null. or else, can use.
							   Scenes look at the specific meaning of. */
	struct block *head;
	struct block *tail;

	spin_lock_struct list_lock;

	volatile long datasize;	/* this block list data total, pusher and getter all maintenance. */
};

static inline void blocklist_init(struct blocklist *self, bool bigblock) {
	assert(self != NULL);
	spin_lock_init(&self->list_lock);
	self->isbigblock = bigblock;
	self->isnewmsg = false;
	self->msglen = 0;
	self->datasize = 0;
	self->canusesize = 0;

	self->head = NULL;
	self->tail = NULL;
}

static inline struct block *blocklist_popfront(struct blocklist *self) {
	struct block *bk;
	spin_lock_lock(&self->list_lock);
	bk = self->head;
	if (self->head) {
		self->head = self->head->next;
		if (self->tail == bk) {
			self->tail = NULL;
			assert(self->head == NULL);
		}
	}
	spin_lock_unlock(&self->list_lock);
	return bk;
}

static inline void blocklist_pushback(struct blocklist *self, struct block *bk) {
	spin_lock_lock(&self->list_lock);
	bk->next = NULL;
	if (self->tail) {
		self->tail->next = bk;
	} else {
		assert(self->head == NULL);
		self->head = bk;
	}
	self->tail = bk;
	spin_lock_unlock(&self->list_lock);
}

#ifdef __cplusplus
}
#endif
#endif

