
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
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

struct nodeflag {
	void *pooladdr;

#ifndef NDEBUG
	void *debug_addr;
#endif

};

struct node {
	struct nodeflag flag;
	struct node *next;
};

struct nodepool {
	/* raw addres for free. */
	void *raw;
	struct nodepool *next;
	struct nodepool *prev;
	size_t block_size;
	size_t nodenum;
	size_t freenum;
	
	char *current_pos;
	char *end;

	struct node *head;
	short type;
	bool need_free;		/* if need_free is true, then must call free function. */
};

struct listobj {
	size_t num;
	struct nodepool *head;
	short type;
};

struct poolmgr {
	const char *name;
	size_t base_block_size;
	size_t alignment;
	size_t block_size;
	size_t base_num;
	size_t current_maxnum;
	size_t next_multiple;

	/* node pool info. */
	size_t nodepool_num;

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

	/* on alloc, first from this nodepool. */
	struct nodepool *first;
};

static inline void listobj_init(struct listobj *self, short type) {
	self->num = 0;
	self->head = NULL;
	self->type = type;
}

#ifndef NOTUSE_POOL
static inline bool nodepool_isfull(struct nodepool *self) {
	return self->freenum == 0;
}

static inline bool nodepool_isportion(struct nodepool *self) {
	return (self->freenum != 0) && (self->freenum != self->nodenum);
}

static inline bool nodepool_isfree(struct nodepool *self) {
	return self->freenum == self->nodenum;
}

static inline struct node *nodepool_pop_node(struct poolmgr *mgr, struct nodepool *self) {
	struct node *nd = self->head;
	if (nd) {
		self->head = nd->next;
		goto ret;
	} else {
		assert(self->current_pos <= self->end);
		if (self->current_pos + self->block_size <= self->end) {
			nd = (struct node *)self->current_pos;
			self->current_pos += self->block_size;
			goto ret;
		}
	}

	return NULL;

ret:
	mgr->node_free_total--;

	self->freenum--;
	nd->flag.pooladdr = self;

#ifndef NDEBUG
	nd->flag.debug_addr = NODE_IS_USED_VALUE(mgr);
#endif

	return nd;
}

static inline void nodepool_push_node(struct poolmgr *mgr, struct nodepool *self, struct node *nd) {
	assert(self == nd->flag.pooladdr);

#ifndef NDEBUG
	assert(nd->flag.debug_addr == NODE_IS_USED_VALUE(mgr));
	nd->flag.debug_addr = NODE_IS_FREED_VALUE(mgr);
#endif

	mgr->node_free_total++;

	nd->next = self->head;
	self->head = nd;
	self->freenum++;
}

static inline struct nodepool *nodepool_create(void *mem, size_t mem_size, size_t block_size, size_t nodenum) {
	struct nodepool *self;
	assert(mem != NULL);
	assert(block_size > 0);
	assert(nodenum > 0);
	assert(mem_size == (sizeof(struct nodepool) + block_size * nodenum));

	self = (struct nodepool *)((char *)mem + (block_size * nodenum));

	self->raw = mem;
	self->next = NULL;
	self->prev = NULL;
	self->block_size = block_size;
	self->nodenum = nodenum;
	self->freenum = nodenum;
	self->current_pos = (char *)mem;
	self->end = self->current_pos + (block_size * nodenum);

	self->head = NULL;
	self->type = enum_unknow;
	self->need_free = true;

	assert(self->current_pos <= self->end && "nodepool_create need check next_multiple, or check block_size * nodenum is overflow");
	return self;
}

static inline void nodepool_release(struct nodepool *self) {
	if (self->need_free)
		free(self->raw);
}

static inline void poolmgr_push_to_list(struct poolmgr *mgr, struct listobj *lt, struct nodepool *self) {
	assert(self->type == enum_unknow);

	self->next = lt->head;
	self->prev = NULL;
	self->type = lt->type;

	if (lt->head) {
		lt->head->prev = self;
	}

	lt->head = self;

	lt->num++;

	mgr->nodepool_num++;
	mgr->node_total += self->nodenum;
	mgr->node_free_total += self->freenum;
}

static inline void poolmgr_remove_from_list(struct poolmgr *mgr, struct listobj *lt, struct nodepool *self) {
	assert(self->type == lt->type);

	self->type = enum_unknow;
	if (self->next)
		self->next->prev = self->prev;

	if (self->prev)
		self->prev->next = self->next;

	if (lt->head == self)
		lt->head = self->next;

	lt->num--;

	mgr->nodepool_num--;
	mgr->node_total -= self->nodenum;
	mgr->node_free_total -= self->freenum;

	self->next = NULL;
	self->prev = NULL;
}

static inline void poolmgr_release_nodepool_fromlist(struct poolmgr *mgr, struct listobj *lt) {
	struct nodepool *np, *next;
	for (np = lt->head; np; np = next) {
		next = np->next;
		if (np->need_free) {
			poolmgr_remove_from_list(mgr, lt, np);
			nodepool_release(np);
		}
	}
}

static inline struct nodepool *poolmgr_create_nodepool(struct poolmgr *self) {
	struct nodepool *np;
	size_t current_maxnum;
	void *mem;

	/* if next_multiple is zero, then only has one sub pool. */
	if (self->next_multiple == 0)
		return NULL;

	current_maxnum = self->current_maxnum * self->next_multiple;
	
	assert(((self->block_size * current_maxnum) / self->block_size) == current_maxnum && "poolmgr_create_nodepool exist overflow!");
	mem = malloc(sizeof(struct nodepool) + self->block_size * current_maxnum);
	if (!mem) {
		assert(false && "poolmgr_create_nodepool malloc memory failed!");
		log_error("malloc %lu byte memory error!", sizeof(struct nodepool) + self->block_size * current_maxnum);
		return NULL;
	}

	self->current_maxnum = current_maxnum;

	np = nodepool_create(mem, sizeof(struct nodepool) + self->block_size * current_maxnum, self->block_size, current_maxnum);
	poolmgr_push_to_list(self, &self->free_list, np);
	return np;
}

static inline struct nodepool *poolmgr_get_nodepool_for_alloc_node(struct poolmgr *self) {
	if (self->portion_use_list.num > 0)
		return self->portion_use_list.head;

	if (self->free_list.num > 0)
		return self->free_list.head;

	return poolmgr_create_nodepool(self);
}

static inline bool poolmgr_check_release_nodepool(struct poolmgr *self, struct nodepool *np) {
	if (np->need_free) {
		if (self->nodepool_num > 1 && 
			(self->free_list.num > self->free_pool_num_for_shrink ||
			 ((double)self->node_free_total / (double)self->node_total) > self->free_node_ratio_for_shrink)) {
			if (self->first == np) {
				self->first = NULL;
			}

			nodepool_release(np);
			return true;
		}
	}

	return false;
}

static inline void poolmgr_check_list_state(struct poolmgr *self, struct nodepool *np) {
	switch (np->type) {
	case enum_full_use:
		if (nodepool_isportion(np)) {
			poolmgr_remove_from_list(self, &self->full_use_list, np);
			poolmgr_push_to_list(self, &self->portion_use_list, np);
		} else if (nodepool_isfree(np)) {
			poolmgr_remove_from_list(self, &self->full_use_list, np);
			if (!poolmgr_check_release_nodepool(self, np))
				poolmgr_push_to_list(self, &self->free_list, np);
		} else {
			assert(false);
		}
		break;
	case enum_portion_use:
		if (nodepool_isfull(np)) {
			poolmgr_remove_from_list(self, &self->portion_use_list, np);
			poolmgr_push_to_list(self, &self->full_use_list, np);
		} else if (nodepool_isfree(np)) {
			poolmgr_remove_from_list(self, &self->portion_use_list, np);
			if (!poolmgr_check_release_nodepool(self, np))
				poolmgr_push_to_list(self, &self->free_list, np);
		}
		break;
	case enum_free:
		if (nodepool_isportion(np)) {
			poolmgr_remove_from_list(self, &self->free_list, np);
			poolmgr_push_to_list(self, &self->portion_use_list, np);
		} else if (nodepool_isfull(np)) {
			poolmgr_remove_from_list(self, &self->free_list, np);
			poolmgr_push_to_list(self, &self->full_use_list, np);
		} else {
			assert(false);
		}
		break;
	default:
		assert(false && "nodepool error in list type.");
		log_error("nodepool error in list type. type:%d", (int)np->type);
		break;
	}
}

static inline struct node *poolmgr_nodepool_alloc_node(struct poolmgr *self) {
	struct node *nd;
	struct nodepool *np = self->first;
	if (np) {
		nd = nodepool_pop_node(self, np);
		if (nd) {
			poolmgr_check_list_state(self, np);
			return nd;
		}
	}
	
	np = poolmgr_get_nodepool_for_alloc_node(self);

	self->first = np;

	if (np) {
		nd = nodepool_pop_node(self, np);
		if (nd) {
			poolmgr_check_list_state(self, np);
			return nd;
		}
	}

	return NULL;
}
#endif

#define F_MAKE_ALIGNMENT(num, align) (((num) + ((align) - 1)) & (~((align) - 1)))
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

/*
 * create poolmgr.
 * size is block size,
 * alignment is align number,
 * num is initialize block num,
 * next_multiple is next num, the next num is num * next_multiple, if next_multiple is zero, then only has one sub pool. 
 * name is poolmgr name.
 */
struct poolmgr *poolmgr_create(size_t size, size_t alignment, size_t num, size_t next_multiple, const char *name) {
	size_t oldsize;
	char *mem;
	struct poolmgr *self;
#ifndef NOTUSE_POOL
	struct nodepool *np;
#endif
	assert(size != 0);
	assert(alignment_check(alignment) && "poolmgr_create alignment is error!");
	assert(num != 0);
	assert(name != NULL);
	if (size == 0 || !alignment_check(alignment) || num == 0 || name == NULL)
		return NULL;

	oldsize = size;

	size += sizeof(struct nodeflag);

	size = F_MAKE_ALIGNMENT(size, alignment);
	if (size < sizeof(struct node))
		size = sizeof(struct node);

	assert(((size * num) / num) == size && "poolmgr_create exist overflow!");

#ifndef NOTUSE_POOL
	mem = (char *)malloc(sizeof(struct poolmgr) + sizeof(struct nodepool) + size * num);
#else
	mem = (char *)malloc(sizeof(struct poolmgr));
#endif

	if (!mem) {
		assert(false && "poolmgr_create malloc memory failed!");

#ifndef NOTUSE_POOL
		log_error("malloc %lu byte memory error!", sizeof(struct poolmgr) + sizeof(struct nodepool) + size * num);
#else
		log_error("malloc %lu byte memory error!", sizeof(struct poolmgr));
#endif

		return NULL;
	}

	self = (struct poolmgr *)mem;

	/* init poolmgr struct. */
	self->name = name;
	self->base_block_size = oldsize;
	self->alignment = alignment;
	self->block_size = size;
	self->base_num = num;
	self->current_maxnum = num;
	self->next_multiple = next_multiple;

	self->nodepool_num = 0;

	self->node_total = 0;
	self->node_free_total = 0;

	self->free_pool_num_for_shrink = 1;
	self->free_node_ratio_for_shrink = 0.618f;

	listobj_init(&self->full_use_list, enum_full_use);
	listobj_init(&self->portion_use_list, enum_portion_use);
	listobj_init(&self->free_list, enum_free);

	self->first = NULL;

#ifndef NOTUSE_POOL
	/* create nodepool and push it. */
	mem += sizeof(struct poolmgr);
	np = nodepool_create(mem, sizeof(struct nodepool) + size * num, size, num);

	/* set free flag. */
	np->need_free = false;

	poolmgr_push_to_list(self, &self->free_list, np);
#endif

	return self;
}

void poolmgr_set_shrink(struct poolmgr *self, size_t free_pool_num, double free_node_ratio) {
	if (!self)
		return;

	self->free_pool_num_for_shrink = free_pool_num;
	self->free_node_ratio_for_shrink = free_node_ratio;
}

void *poolmgr_getobject(struct poolmgr *self) {
#ifndef NOTUSE_POOL
	struct node *nd;
#endif
	if (!self)
		return NULL;

#ifndef NOTUSE_POOL
	nd = poolmgr_nodepool_alloc_node(self);
	if (nd)
		return &nd->next;

	return NULL;

#else
	return malloc(self->base_block_size);
#endif

}

#ifndef NOTUSE_POOL
#ifndef NDEBUG
static inline bool nodepool_check_in(struct poolmgr *mgr, struct listobj *lt, struct nodepool *self) {
	struct nodepool *np, *next;
	for (np = lt->head; np; np = next) {
		next = np->next;
		if (np == self)
			return true;
	}

	return false;
}

static bool nodepool_isin(struct poolmgr *mgr, struct nodepool *np, char *nd) {
	char *begin;
	bool has = false;
	if (nodepool_check_in(mgr, &mgr->full_use_list, np) ||
		nodepool_check_in(mgr, &mgr->portion_use_list, np) ||
		nodepool_check_in(mgr, &mgr->free_list, np)) {
		has = true;
	}

	if (!has)
		return false;

	begin = np->end - (np->block_size * np->nodenum);
	if ((nd >= begin) &&  nd < np->end)
		return true;
	else
		return false;
}

static bool nodepool_isbad_point(struct nodepool *np, char *bk) {
	char *begin = np->end - (np->block_size * np->nodenum);
	bk -= (size_t)begin;
	if ((size_t )bk % np->block_size != 0)
		return true;
	else
		return false;
}

static bool nodepool_is_not_alloc(struct nodepool *np, char *bk) {
	if (np->current_pos <= bk)
		return true;
	else
		return false;
}

static bool poolmgr_check_is_using(struct poolmgr *mgr, struct nodepool *np, char *nd) {
	if (nodepool_isin(mgr, np, nd)) {
		if (nodepool_isbad_point(np, nd)) {
			assert(false && "poolmgr_freeobject block is in pool, but is bad pointer!");
			return false;
		} else {
			if (nodepool_is_not_alloc(np, nd)) {
				assert(false && "poolmgr_freeobject free the undistributed of block!");
				return false;
			} else {
				if (((struct node *)nd)->flag.debug_addr == NODE_IS_FREED_VALUE(mgr)) {
					assert(false && "poolmgr_freeobject repeated free block!");
					return false;
				} else if (((struct node *)nd)->flag.debug_addr != NODE_IS_USED_VALUE(mgr)) {
					assert(false && "poolmgr_freeobject free the bad block!");
					return false;
				}
				return true;
			}
		}
	}

	assert(false && "poolmgr_freeobject this address is not in poolmgr!");
	return false;
}
#endif
#endif

void poolmgr_freeobject(struct poolmgr *self, void *bk) {

#ifndef NOTUSE_POOL

	struct node *nd;
	struct nodepool *np;
	if (!self || !bk)
		return;

	nd = (struct node *)((char *)bk - (size_t)(&((struct node *)0)->next));
	np = (struct nodepool *)nd->flag.pooladdr;

	/* check bk is in this free list ? */
	assert(np != NULL && poolmgr_check_is_using(self, np, (char *)nd));


	nodepool_push_node(self, np, nd);
	poolmgr_check_list_state(self, np);

#else
	free(bk);
#endif

}

void poolmgr_release(struct poolmgr *self) {
	if (!self)
		return;

#ifndef NOTUSE_POOL
	
	poolmgr_release_nodepool_fromlist(self, &self->full_use_list);
	poolmgr_release_nodepool_fromlist(self, &self->portion_use_list);
	poolmgr_release_nodepool_fromlist(self, &self->free_list);
	
	/* check memory leak. */
	assert(self->node_total == self->node_free_total && "poolmgr_release has memory not free!");

#endif

	free(self);
}

#define _STR_HEAD "\n%s:\n<<<<<<<<<<<<<<<<<< poolmgr info begin <<<<<<<<<<<<<<<<<\n\
pools have pool num:%lu\n\
base alignment:%lu\n\
base object size:%lu\tobject size:%lu\n\
object total num: [%lu]\tobject current num: [%lu]\n\
base num:%lu\tcurrent max num:%lu\tnext_multiple:%lu\n\
memory total: %lu(byte), %lu(kb), %lu(mb)\n\
shrink arg: free pool num:%lu, free node ratio:%.3f\n\
>>>>>>>>>>>>>>>>>> poolmgr info end >>>>>>>>>>>>>>>>>>\n"
void poolmgr_getinfo(struct poolmgr *self, char *buf, size_t bufsize) {

#ifndef NOTUSE_POOL
	size_t totalsize;
	if (!self || !buf || bufsize == 0)
		return;

	totalsize = self->block_size * self->node_total;
	snprintf(buf, bufsize, _STR_HEAD, self->name, \
	(unsigned long)self->nodepool_num, \
	(unsigned long)self->alignment, 
	(unsigned long)self->base_block_size, (unsigned long)self->block_size, \
	(unsigned long)self->node_total, (unsigned long)self->node_free_total, \
	(unsigned long)self->base_num, (unsigned long)self->current_maxnum, (unsigned long)self->next_multiple, \
	(unsigned long)totalsize, (unsigned long)totalsize / 1024, (unsigned long)totalsize / (1024 * 1024), \
	(unsigned long)self->free_pool_num_for_shrink, self->free_node_ratio_for_shrink);
#else
	snprintf(buf, bufsize, "%s not use pools!\n", self->name);
#endif

	buf[bufsize - 1] = 0;
}

