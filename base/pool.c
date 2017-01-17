
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "pool.h"
#include "log.h"
#include "platform_config.h"

#ifndef NDEBUG
#define NODE_IS_USED_VALUE(mgr) ((mgr) - (0x000000AB))
#define NODE_IS_FREED_VALUE(mgr) (mgr)
#endif


enum {
	enum_unknow = 0,
	enum_full_use,
	enum_portion_use,
	enum_free,
};

struct node_flag {
	void *pool_addr;

#ifndef NDEBUG
	void *debug_addr;
#endif

};

/* set in memory block tail. */
struct node {
	struct node *next;
	struct node_flag flag;
};

struct node_pool {
	/* raw addres for free. */
	void *raw;
	struct node_pool *next;
	struct node_pool *prev;
	size_t block_size;
	size_t node_num;
	size_t free_num;

	char *current_pos;
	char *end;

	struct node *head;
	short type;
	bool need_free;		/* if need_free is true, then must call free function. */
};

struct listobj {
	size_t num;
	struct node_pool *head;
	short type;
};

struct poolmgr {
	/* raw addres for free. */
	void *raw;

	const char *name;
	size_t base_block_size;
	size_t alignment;
	size_t block_size;
	size_t base_num;
	size_t current_maxnum;
	size_t next_multiple;

	/* node pool info. */
	size_t node_pool_num;
	size_t max_node_pool_num;
	time_t max_node_pool_time;

	/* all node info. */
	size_t node_total;
	size_t node_free_total;

	/* for shrinkage */
	size_t free_pool_num_for_shrink;
	double free_node_ratio_for_shrink;

	/* full use node pool list. */
	struct listobj full_use_list;

	/* portion use node pool list. */
	struct listobj portion_use_list;

	/* free node pool list. */
	struct listobj free_list;

	/* on alloc, first from this node_pool. */
	struct node_pool *first;
};

#define F_MAKE_ALIGNMENT(num, align)	(((num) + ((align) - 1)) & (~((align) - 1)))
#define F_THIS_POOL_ALIGNMENT_SIZE		16
#define F_THIS_POOL_ALIGNMENT(num)		F_MAKE_ALIGNMENT(num, F_THIS_POOL_ALIGNMENT_SIZE)
static inline bool alignment_check(size_t alignment) {
	switch (alignment) {
	case 1:
	case 4:
	case 8:
	case 16:
	case 32:
	case 64:
	case 128:
		return true;
	}
	return false;
}

static inline void listobj_init(struct listobj *self, short type) {
	self->num = 0;
	self->head = NULL;
	self->type = type;
}

#ifndef NOTUSE_POOL
static inline bool node_pool_is_full(struct node_pool *self) {
	return self->free_num == 0;
}

static inline bool node_pool_is_portion(struct node_pool *self) {
	return (self->free_num != 0) && (self->free_num != self->node_num);
}

static inline bool node_pool_is_free(struct node_pool *self) {
	return self->free_num == self->node_num;
}

static inline struct node *node_pool_pop_node(struct poolmgr *mgr, struct node_pool *self) {
	struct node *nd = self->head;
	if (nd) {
		self->head = nd->next;
		goto ret;
	} else {
		assert(self->current_pos <= self->end);
		if (self->current_pos + self->block_size <= self->end) {
			nd = (struct node *)(self->current_pos + self->block_size - sizeof(struct node));
			self->current_pos += self->block_size;
			goto ret;
		}
	}

	return NULL;

ret:
	--mgr->node_free_total;

	--self->free_num;
	nd->flag.pool_addr = self;

#ifndef NDEBUG
	nd->flag.debug_addr = NODE_IS_USED_VALUE(mgr);
#endif

	return nd;
}

static inline void node_pool_push_node(struct poolmgr *mgr, struct node_pool *self, struct node *nd) {
	assert(self == nd->flag.pool_addr);

#ifndef NDEBUG
	assert(nd->flag.debug_addr == NODE_IS_USED_VALUE(mgr));
	nd->flag.debug_addr = NODE_IS_FREED_VALUE(mgr);
#endif

	++mgr->node_free_total;

	nd->next = self->head;
	self->head = nd;
	++self->free_num;
}

static inline struct node_pool *node_pool_create(void *mem, size_t mem_size, 
		size_t block_size, size_t node_num) {

	struct node_pool *self;
	assert(mem != NULL);
	assert(block_size > 0);
	assert(node_num > 0);
	assert(mem_size == (F_THIS_POOL_ALIGNMENT_SIZE + 
						F_THIS_POOL_ALIGNMENT(sizeof(struct node_pool)) + 
						block_size * node_num));

	self = (struct node_pool *)F_THIS_POOL_ALIGNMENT((uintptr_t)mem);

	self->raw = mem;
	self->next = NULL;
	self->prev = NULL;
	self->block_size = block_size;
	self->node_num = node_num;
	self->free_num = node_num;
	self->current_pos = (char *)self + F_THIS_POOL_ALIGNMENT(sizeof(struct node_pool));
	self->end = self->current_pos + (block_size * node_num);

	self->head = NULL;
	self->type = enum_unknow;
	self->need_free = true;

	assert(self->end <= (char *)mem + mem_size);
	assert(self->current_pos <= self->end && 
			"node_pool_create need check next_multiple, or check block_size * node_num is overflow");
	return self;
}

static inline void node_pool_release(struct node_pool *self) {
	if (self->need_free)
		free(self->raw);
}

static inline void poolmgr_push_to_list(struct poolmgr *mgr, 
		struct listobj *lt, struct node_pool *self) {

	assert(self->type == enum_unknow);

	self->next = lt->head;
	self->prev = NULL;
	self->type = lt->type;

	if (lt->head) {
		lt->head->prev = self;
	}

	lt->head = self;

	++lt->num;

	++mgr->node_pool_num;
	mgr->node_total += self->node_num;
	mgr->node_free_total += self->free_num;

	if (mgr->node_pool_num > mgr->max_node_pool_num) {
		mgr->max_node_pool_num = mgr->node_pool_num;
		mgr->max_node_pool_time = time(NULL);
	}
}

static inline void poolmgr_remove_from_list(struct poolmgr *mgr, 
		struct listobj *lt, struct node_pool *self) {

	assert(self->type == lt->type);

	self->type = enum_unknow;
	if (self->next)
		self->next->prev = self->prev;

	if (self->prev)
		self->prev->next = self->next;

	if (lt->head == self)
		lt->head = self->next;

	--lt->num;

	--mgr->node_pool_num;
	mgr->node_total -= self->node_num;
	mgr->node_free_total -= self->free_num;

	self->next = NULL;
	self->prev = NULL;
}

static inline void poolmgr_release_node_pool_from_list(struct poolmgr *mgr, struct listobj *lt) {
	struct node_pool *np, *next;
	for (np = lt->head; np; np = next) {
		next = np->next;
		if (np->need_free) {
			poolmgr_remove_from_list(mgr, lt, np);
			node_pool_release(np);
		}
	}
}

static inline struct node_pool *poolmgr_create_node_pool(struct poolmgr *self) {
	struct node_pool *np;
	size_t current_maxnum, total_mem_size;
	void *mem;

	/* if next_multiple is zero, then only has one sub pool. */
	if (self->next_multiple == 0)
		return NULL;

	current_maxnum = self->current_maxnum * self->next_multiple;

	assert(((self->block_size * current_maxnum) / self->block_size) == current_maxnum && 
			"poolmgr_create_node_pool exist overflow!");

	total_mem_size = F_THIS_POOL_ALIGNMENT_SIZE + 
					 F_THIS_POOL_ALIGNMENT(sizeof(struct node_pool)) + 
					 self->block_size * current_maxnum;

	mem = malloc(total_mem_size);
	if (!mem) {
		assert(false && "poolmgr_create_node_pool malloc memory failed!");
		log_error("malloc " _FORMAT_64U_NUM " byte memory error!", (uint64)total_mem_size);
		return NULL;
	}

	self->current_maxnum = current_maxnum;

	np = node_pool_create(mem, total_mem_size, self->block_size, current_maxnum);
	poolmgr_push_to_list(self, &self->free_list, np);
	return np;
}

static inline struct node_pool *poolmgr_get_node_pool_for_alloc_node(struct poolmgr *self) {
	if (self->portion_use_list.num > 0)
		return self->portion_use_list.head;

	if (self->free_list.num > 0)
		return self->free_list.head;

	return poolmgr_create_node_pool(self);
}

static inline bool poolmgr_check_release_node_pool(struct poolmgr *self, struct node_pool *np) {
	if (np->need_free) {
		if (self->node_pool_num > 1) {
			double now_free_ratio = (double)self->node_free_total / (double)self->node_total;
			if (self->free_list.num > self->free_pool_num_for_shrink ||
				now_free_ratio > self->free_node_ratio_for_shrink) {

				if (self->first == np)
					self->first = NULL;

				node_pool_release(np);
				return true;
			}
		}
	}

	return false;
}

static inline void poolmgr_check_list_state(struct poolmgr *self, struct node_pool *np) {
	switch (np->type) {
	case enum_full_use:
		if (node_pool_is_portion(np)) {
			poolmgr_remove_from_list(self, &self->full_use_list, np);
			poolmgr_push_to_list(self, &self->portion_use_list, np);
		} else if (node_pool_is_free(np)) {
			poolmgr_remove_from_list(self, &self->full_use_list, np);
			if (!poolmgr_check_release_node_pool(self, np))
				poolmgr_push_to_list(self, &self->free_list, np);
		} else {
			assert(false);
		}
		break;
	case enum_portion_use:
		if (node_pool_is_full(np)) {
			poolmgr_remove_from_list(self, &self->portion_use_list, np);
			poolmgr_push_to_list(self, &self->full_use_list, np);
		} else if (node_pool_is_free(np)) {
			poolmgr_remove_from_list(self, &self->portion_use_list, np);
			if (!poolmgr_check_release_node_pool(self, np))
				poolmgr_push_to_list(self, &self->free_list, np);
		}
		break;
	case enum_free:
		if (node_pool_is_portion(np)) {
			poolmgr_remove_from_list(self, &self->free_list, np);
			poolmgr_push_to_list(self, &self->portion_use_list, np);
		} else if (node_pool_is_full(np)) {
			poolmgr_remove_from_list(self, &self->free_list, np);
			poolmgr_push_to_list(self, &self->full_use_list, np);
		} else {
			assert(false);
		}
		break;
	default:
		assert(false && "node_pool error in list type.");
		log_error("node_pool error in list type. type:%d", (int)np->type);
		break;
	}
}

static inline struct node *poolmgr_node_pool_alloc_node(struct poolmgr *self) {
	struct node *nd;
	struct node_pool *np = self->first;
	if (np) {
		nd = node_pool_pop_node(self, np);
		if (nd) {
			poolmgr_check_list_state(self, np);
			return nd;
		}
	}

	np = poolmgr_get_node_pool_for_alloc_node(self);

	self->first = np;

	if (np) {
		nd = node_pool_pop_node(self, np);
		if (nd) {
			poolmgr_check_list_state(self, np);
			return nd;
		}
	}

	return NULL;
}
#endif

/*
 * create poolmgr.
 * size is block size,
 * alignment is align number,
 * num is initialize block num,
 * next_multiple is next num,
 *		the next num is num * next_multiple, if next_multiple is zero, then only has one sub pool.
 * name is poolmgr name.
 */
struct poolmgr *poolmgr_create(size_t size, size_t alignment, 
		size_t num, size_t next_multiple, const char *name) {

	size_t oldsize;
	char *mem;
	struct poolmgr *self;
#ifndef NOTUSE_POOL
	struct node_pool *np;
	size_t poolmgr_mem_size, node_pool_mem_size, total_mem_size;
#endif
	assert(size != 0);
	assert(alignment_check(alignment) && "poolmgr_create alignment is error!");
	assert(num != 0);
	assert(name != NULL);
	if (size == 0 || !alignment_check(alignment) || num == 0 || name == NULL)
		return NULL;

	oldsize = size;

	size += sizeof(struct node_flag);

	size = F_MAKE_ALIGNMENT(size, alignment);
	if (size < sizeof(struct node))
		size = sizeof(struct node);

	assert(((size * num) / num) == size && "poolmgr_create exist overflow!");

#ifndef NOTUSE_POOL
	/* for memory address alignment. (in risc cpu memory address must alignment.) */
	poolmgr_mem_size = F_THIS_POOL_ALIGNMENT_SIZE + F_THIS_POOL_ALIGNMENT(sizeof(struct poolmgr));
	node_pool_mem_size = F_THIS_POOL_ALIGNMENT_SIZE + 
						F_THIS_POOL_ALIGNMENT(sizeof(struct node_pool)) + size * num;
	total_mem_size = poolmgr_mem_size + node_pool_mem_size;
	mem = (char *)malloc(total_mem_size);
#else
	mem = (char *)malloc(sizeof(struct poolmgr));
#endif

	if (!mem) {
		assert(false && "poolmgr_create malloc memory failed!");

#ifndef NOTUSE_POOL
		log_error("malloc " _FORMAT_64U_NUM " byte memory error!", (uint64)total_mem_size);
#else
		log_error("malloc " _FORMAT_64U_NUM " byte memory error!", (uint64)sizeof(struct poolmgr));
#endif

		return NULL;
	}

#ifndef NOTUSE_POOL
	self = (struct poolmgr *)F_THIS_POOL_ALIGNMENT((uintptr_t)mem);
#else
	self = (struct poolmgr *)mem;
#endif

	/* init poolmgr struct. */
	self->raw = mem;
	self->name = name;
	self->base_block_size = oldsize;
	self->alignment = alignment;
	self->block_size = size;
	self->base_num = num;
	self->current_maxnum = num;
	self->next_multiple = next_multiple;

	self->node_pool_num = 0;
	self->max_node_pool_num = 0;
	self->max_node_pool_time = 0;

	self->node_total = 0;
	self->node_free_total = 0;

	self->free_pool_num_for_shrink = 1;
	self->free_node_ratio_for_shrink = 0.618f;

	listobj_init(&self->full_use_list, enum_full_use);
	listobj_init(&self->portion_use_list, enum_portion_use);
	listobj_init(&self->free_list, enum_free);

	self->first = NULL;

#ifndef NOTUSE_POOL
	/* create node_pool and push it. */
	mem = (char *)self;
	mem += F_THIS_POOL_ALIGNMENT(sizeof(struct poolmgr));
	np = node_pool_create(mem, node_pool_mem_size, size, num);

	/* set free flag. */
	np->need_free = false;

	poolmgr_push_to_list(self, &self->free_list, np);
#endif

	return self;
}

void poolmgr_release(struct poolmgr *self) {
	if (!self)
		return;

#ifndef NOTUSE_POOL

	poolmgr_release_node_pool_from_list(self, &self->full_use_list);
	poolmgr_release_node_pool_from_list(self, &self->portion_use_list);
	poolmgr_release_node_pool_from_list(self, &self->free_list);

	/* check memory leak. */
	assert(self->node_total == self->node_free_total && "poolmgr_release has memory not free!");

#endif

	free(self->raw);
}

void poolmgr_set_shrink(struct poolmgr *self, size_t free_pool_num, double free_node_ratio) {
	if (!self)
		return;

	self->free_pool_num_for_shrink = free_pool_num;
	self->free_node_ratio_for_shrink = free_node_ratio;
}

void *poolmgr_alloc_object(struct poolmgr *self) {
#ifndef NOTUSE_POOL
	struct node *nd;
#endif
	if (!self)
		return NULL;

#ifndef NOTUSE_POOL
	nd = poolmgr_node_pool_alloc_node(self);
	if (nd) {
		return ((char *)nd) - (self->block_size - sizeof(struct node));
	}

	return NULL;

#else
	return malloc(self->base_block_size);
#endif

}

#ifndef NOTUSE_POOL
#ifndef NDEBUG
static inline bool node_pool_check_in(struct poolmgr *mgr, 
		struct listobj *lt, struct node_pool *self) {

	struct node_pool *np, *next;
	for (np = lt->head; np; np = next) {
		next = np->next;
		if (np == self)
			return true;
	}

	return false;
}

static bool node_pool_is_in(struct poolmgr *mgr, struct node_pool *np, char *begin_bk) {
	char *begin;
	bool has = false;
	if (node_pool_check_in(mgr, &mgr->full_use_list, np) ||
		node_pool_check_in(mgr, &mgr->portion_use_list, np) ||
		node_pool_check_in(mgr, &mgr->free_list, np)) {
		has = true;
	}

	if (!has)
		return false;

	begin = np->end - (np->block_size * np->node_num);
	if ((begin_bk >= begin) && begin_bk < np->end)
		return true;
	else
		return false;
}

static bool node_pool_isbad_point(struct node_pool *np, char *begin_bk) {
	char *begin = np->end - (np->block_size * np->node_num);
	begin_bk -= (size_t)begin;
	if ((size_t)begin_bk % np->block_size != 0)
		return true;
	else
		return false;
}

static bool node_pool_is_not_alloc(struct node_pool *np, char *begin_bk) {
	if (np->current_pos <= begin_bk)
		return true;
	else
		return false;
}

static bool poolmgr_check_is_using(struct poolmgr *mgr, 
		struct node_pool *np, char *begin_bk, struct node *nd) {

	if (node_pool_is_in(mgr, np, begin_bk)) {
		if (node_pool_isbad_point(np, begin_bk)) {
			assert(false && "poolmgr_free_object block is in pool, but is bad pointer!");
			return false;
		} else {
			if (node_pool_is_not_alloc(np, begin_bk)) {
				assert(false && "poolmgr_free_object free the undistributed of block!");
				return false;
			} else {
				if (nd->flag.debug_addr == NODE_IS_FREED_VALUE(mgr)) {
					assert(false && "poolmgr_free_object repeated free block!");
					return false;
				} else if (nd->flag.debug_addr != NODE_IS_USED_VALUE(mgr)) {
					assert(false && "poolmgr_free_object free the bad block!");
					return false;
				}
				return true;
			}
		}
	}

	assert(false && "poolmgr_free_object this address is not in poolmgr!");
	return false;
}
#endif
#endif

void poolmgr_free_object(struct poolmgr *self, void *bk) {

#ifndef NOTUSE_POOL

	struct node *nd;
	struct node_pool *np;
	if (!self || !bk)
		return;

	nd = (struct node *)((char *)bk + self->block_size - sizeof(struct node));
	np = (struct node_pool *)nd->flag.pool_addr;

	/* check bk is in this free list ? */
	assert(np != NULL && poolmgr_check_is_using(self, np, (char *)bk, nd));

	node_pool_push_node(self, np, nd);
	poolmgr_check_list_state(self, np);

#else
	free(bk);
#endif

}

#define _STR_HEAD "\n%s:\n\
<<<<<<<<<<<<<<<<<< poolmgr info begin <<<<<<<<<<<<<<<<<\n\
pools have pool num:" _FORMAT_64U_NUM "\n\
pools max pool num:" _FORMAT_64U_NUM "\ttime:%s\n\
base alignment:" _FORMAT_64U_NUM "\n\
base object size:" _FORMAT_64U_NUM "\tobject size:" _FORMAT_64U_NUM "\n\
object total num: [" _FORMAT_64U_NUM "]\tobject current num: [" _FORMAT_64U_NUM "]\n\
base num:" _FORMAT_64U_NUM "\tcurrent max num:" _FORMAT_64U_NUM "\tnext_multiple:" _FORMAT_64U_NUM "\n\
memory total: " _FORMAT_64U_NUM "(byte), " _FORMAT_64U_NUM "(kb), " _FORMAT_64U_NUM "(mb)\n\
shrink arg: free pool num:" _FORMAT_64U_NUM ", free node ratio:%.3f\n\
>>>>>>>>>>>>>>>>>> poolmgr info end >>>>>>>>>>>>>>>>>>>\n"
void poolmgr_get_info(struct poolmgr *self, char *buf, size_t bufsize) {

#ifndef NOTUSE_POOL
	size_t totalsize;
	char time_buf[64] = {0};
	struct tm tm_result;
	struct tm *currTM;
	if (!self || !buf || bufsize == 0)
		return;

	currTM = safe_localtime(&self->max_node_pool_time, &tm_result);
	snprintf(time_buf, sizeof(time_buf) - 1, "%d-%02d-%02d %02d:%02d:%02d", 
			currTM->tm_year + 1900, currTM->tm_mon + 1, currTM->tm_mday, 
			currTM->tm_hour, currTM->tm_min, currTM->tm_sec);
	time_buf[sizeof(time_buf) - 1] = '\0';

	totalsize = self->block_size * self->node_total;
	snprintf(buf, bufsize, _STR_HEAD, self->name, 
	(uint64)self->node_pool_num, (uint64)self->max_node_pool_num, time_buf, 
	(uint64)self->alignment, 
	(uint64)self->base_block_size, (uint64)self->block_size, 
	(uint64)self->node_total, (uint64)self->node_free_total, 
	(uint64)self->base_num, (uint64)self->current_maxnum, (uint64)self->next_multiple, 
	(uint64)totalsize, (uint64)totalsize / 1024, (uint64)totalsize / (1024 * 1024), 
	(uint64)self->free_pool_num_for_shrink, self->free_node_ratio_for_shrink);
#else
	snprintf(buf, bufsize, "%s not use pools!\n", self->name);
#endif

	buf[bufsize - 1] = 0;
}

