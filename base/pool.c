
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
#include "alway_inline.h"

#ifndef NDEBUG
#define NODE_IS_USED_VALUE(a) ((a) - (0x000000AB))
#define NODE_IS_FREED_VALUE(a) (a)

struct debugflag
{
	void *pooladdr;
};
#endif

struct node
{
#ifndef NDEBUG
	struct debugflag debug;
#endif

	struct node *next;
};

struct nodepool
{
	struct nodepool *next;	/* if next is null, then not call free function. */
	size_t block_size;
	size_t nodenum;
	
	char *current_pos;
	char *end;
};

struct poolmgr
{
	const char *name;
	size_t base_block_size;
	size_t alignment;
	size_t block_size;
	size_t base_num;
	size_t current_maxnum;
	size_t next_multiple;

	/* node pool list. */
	size_t nodepool_num;
	struct nodepool *pool_head;

	/* node list. */
	size_t node_total;
	size_t freenode_num;
	struct node *f_head;
};

static inline struct node *nodepool_pop_from_head (struct poolmgr *self)
{
	struct nodepool *np = self->pool_head;
	assert(np->current_pos <= np->end);
	if (np->current_pos + np->block_size <= np->end)
	{
		struct node *nd = (struct node *)np->current_pos;
		np->current_pos += np->block_size;
		self->freenode_num--;

#ifndef NDEBUG
		nd->debug.pooladdr = NODE_IS_USED_VALUE(self);
#endif

		return nd;
	}
	else
	{
		return NULL;
	}
}

static inline void poolmgr_push_pool (struct poolmgr *self, struct nodepool *np)
{
	self->nodepool_num++;

	self->node_total += np->nodenum;
	self->freenode_num += np->nodenum;

	np->next = self->pool_head;
	self->pool_head = np;
}

#ifndef NOTUSE_POOL
static struct nodepool *nodepool_create (void *mem, size_t mem_size, size_t block_size, size_t nodenum)
{
	struct nodepool *self;
	assert(mem != NULL);
	assert(block_size > 0);
	assert(nodenum > 0);
	assert(mem_size == (sizeof(struct nodepool) + block_size * nodenum));

	self = (struct nodepool *)mem;

	self->next = NULL;
	self->block_size = block_size;
	self->nodenum = nodenum;
	self->current_pos = (char *)mem + sizeof(struct nodepool);
	self->end = self->current_pos + (block_size * nodenum);

	assert(self->current_pos <= self->end && "nodepool_create need check next_multiple, or check block_size * nodenum is overflow");
	return self;
}
#endif

static inline void nodepool_release (struct nodepool *self)
{
	if (self->next)
		free(self);
}

#define F_MAKE_ALIGNMENT(num, align) (((num) + ((align)-1)) & (~((align)-1)))
static inline bool alignment_check (size_t alignment)
{
	switch (alignment)
	{
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
struct poolmgr *poolmgr_create (size_t size, size_t alignment, size_t num, size_t next_multiple, const char *name)
{
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

#ifndef NDEBUG
	size += sizeof(struct debugflag);
#endif

	size = F_MAKE_ALIGNMENT(size, alignment);
	if (size < sizeof(struct node))
		size = sizeof(struct node);

	assert(((size * num)/num) == size && "poolmgr_create exist overflow!");

#ifndef NOTUSE_POOL
	mem = (char *)malloc(sizeof(struct poolmgr) + sizeof(struct nodepool) + size * num);
#else
	mem = (char *)malloc(sizeof(struct poolmgr));
#endif

	if (!mem)
	{
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
	self->pool_head = NULL;

	self->node_total = 0;
	self->freenode_num = 0;
	self->f_head = NULL;

#ifndef NOTUSE_POOL
	/* create nodepool and push it. */
	mem += sizeof(struct poolmgr);
	np = nodepool_create(mem, sizeof(struct nodepool) + size * num, size, num);

	poolmgr_push_pool(self, np);
#endif

	return self;
}

static inline void *poolmgr_node_pop (struct poolmgr *self)
{
	struct node *nd = self->f_head;
	if (nd)
	{
		self->f_head = nd->next;
		self->freenode_num--;

#ifndef NDEBUG
		assert(nd->debug.pooladdr == NODE_IS_FREED_VALUE(self));
		nd->debug.pooladdr = NODE_IS_USED_VALUE(self);
#endif

	}
	else
	{
		nd = nodepool_pop_from_head(self);
	}
#ifndef NDEBUG
	if (nd)
		return &nd->next;
	else
		return NULL;
#else
	return nd;
#endif
}

#ifndef NOTUSE_POOL
static void *poolmgr_create_nodepool (struct poolmgr *self)
{
	struct nodepool *np;
	size_t current_maxnum;
	void *mem;

	/* if next_multiple is zero, then only has one sub pool. */
	if (0 == self->next_multiple)
		return NULL;

	current_maxnum= self->current_maxnum * self->next_multiple;
	
	assert(((self->block_size * current_maxnum)/self->block_size) == current_maxnum && "poolmgr_create_nodepool exist overflow!");
	mem = malloc(sizeof(struct nodepool) + self->block_size * current_maxnum);
	if (!mem)
	{
		assert(false && "poolmgr_create_nodepool malloc memory failed!");
		log_error("malloc %lu byte memory error!", sizeof(struct nodepool) + self->block_size * current_maxnum);
		return NULL;
	}
	self->current_maxnum = current_maxnum;

	np = nodepool_create(mem, sizeof(struct nodepool) + self->block_size * current_maxnum, self->block_size, current_maxnum);
	poolmgr_push_pool(self, np);
	return poolmgr_node_pop(self);
}
#endif

void *poolmgr_getobject (struct poolmgr *self)
{
#ifndef NOTUSE_POOL
	void *bk;
#endif
	assert(self != NULL);
	if (!self)
		return NULL;

#ifndef NOTUSE_POOL
	bk = poolmgr_node_pop(self);
	if (bk)
		return bk;
	else
		return poolmgr_create_nodepool(self);
#else
	return malloc(self->base_block_size);
#endif

}

static inline void poolmgr_node_push (struct poolmgr *self, struct node *nd)
{
#ifndef NDEBUG
	nd->debug.pooladdr = NODE_IS_FREED_VALUE(self);
#endif

	nd->next = self->f_head;
	self->f_head = nd;

	self->freenode_num++;
}

#ifndef NDEBUG
#ifndef NOTUSE_POOL
static bool freenode_is_in_poolmgr (struct poolmgr *self, void *bk)
{
	struct node *nd = (struct node *)bk;
	if (nd->debug.pooladdr == NODE_IS_FREED_VALUE(self))
		return true;
	else if (nd->debug.pooladdr != NODE_IS_USED_VALUE(self))
	{
		assert(false && "freenode_is_in_poolmgr debug flag's pooladdr is not equal!, bk address be destroyed!");
		return false;
	}
	else
		return false;
}

static bool nodepool_isin (struct nodepool *np, void *bk)
{
	void *begin = np->end - (np->block_size * np->nodenum);
	if ((bk >= begin) &&  ((char *)bk < np->end))
		return true;
	else
		return false;
}

static bool nodepool_isbad_point (struct nodepool *np, char *bk)
{
	char *begin = np->end - (np->block_size * np->nodenum);
	bk -= (size_t)begin;
	if ((size_t )bk%np->block_size != 0)
		return true;
	else
		return false;
}

static bool nodepool_is_not_alloc (struct nodepool *np, void *bk)
{
	if (np->current_pos <= (char *)bk)
		return true;
	else
		return false;
}

static bool poolmgr_check_is_using (struct poolmgr *self, void *bk)
{
	struct nodepool *np, *next;
	if (freenode_is_in_poolmgr(self, bk))
	{
		assert(false && "poolmgr_freeobject repeat free!");
		return false;
	}
	for (np = self->pool_head; np; np = next)
	{
		next = np->next;
		if (nodepool_isin(np, bk))
		{
			if (nodepool_isbad_point(np, bk))
			{
				assert(false && "poolmgr_freeobject block is in pool, but is bad pointer!");
				return false;
			}
			else
			{
				if (nodepool_is_not_alloc(np, bk))
				{
					assert(false && "poolmgr_freeobject free the undistributed of block!");
					return false;
				}
				else
					return true;
			}

		}
	}
	assert(false && "poolmgr_freeobject this address is not in poolmgr!");
	return false;
}
#endif
#endif

void poolmgr_freeobject (struct poolmgr *self, void *bk)
{

#ifndef NOTUSE_POOL

#ifndef NDEBUG
	struct node *nd;
#endif

	assert(self != NULL);
	assert(bk != NULL);
	if (!self || !bk)
		return;

#ifndef NDEBUG
	nd = (struct node *)((char *)bk - (size_t)(&((struct node *)0)->next));

	/* check bk is in this free list ? */
	assert(poolmgr_check_is_using(self, nd));

	poolmgr_node_push(self, nd);
#else

	poolmgr_node_push(self, bk);

#endif

#else
	free(bk);
#endif

}

void poolmgr_release (struct poolmgr *self)
{
#ifndef NOTUSE_POOL
	struct nodepool *np, *next;
#endif
	assert(self != NULL);
	if (!self)
		return;

#ifndef NOTUSE_POOL
	/* check memory leak. */
	assert(self->node_total == self->freenode_num && "poolmgr_release has memory not free!");

	for (np = self->pool_head; np; np = next)
	{
		next = np->next;
		nodepool_release(np);
	}
	self->pool_head = NULL;
	self->f_head = NULL;
#endif

	free(self);
}

#ifdef WIN32
#define snprintf _snprintf
#endif
#define _STR_HEAD "%s:\n<<<<<<<<<<<<<<<<<< poolmgr info begin <<<<<<<<<<<<<<<<<\n\
pools have pool num:%lu\n\
base alignment:%lu\n\
base object size:%lu\tobject size:%lu\n\
object total num: [%lu]\tobject current num: [%lu]\n\
base num:%lu\tcurrent max num:%lu\tnext_multiple:%lu\n\
memory total: %lu(byte), %lu(kb), %lu(mb)\n\
>>>>>>>>>>>>>>>>>> poolmgr info end >>>>>>>>>>>>>>>>>>\n"
void poolmgr_getinfo (struct poolmgr *self, char *buf, size_t bufsize)
{

#ifndef NOTUSE_POOL
	size_t totalsize;
	assert(self != NULL);
	assert(buf != NULL);
	assert(bufsize > 0);
	if (!self || !buf || bufsize == 0)
		return;
	totalsize = self->block_size * self->node_total;
	snprintf(buf, bufsize, _STR_HEAD, self->name, \
	(unsigned long)self->nodepool_num, \
	(unsigned long)self->alignment, 
	(unsigned long)self->base_block_size, (unsigned long)self->block_size, \
	(unsigned long)self->node_total, (unsigned long)self->freenode_num, \
	(unsigned long)self->base_num, (unsigned long)self->current_maxnum, (unsigned long)self->next_multiple, \
	(unsigned long)totalsize, (unsigned long)totalsize/1024, (unsigned long)totalsize/(1024*1024));
#else
	snprintf(buf, bufsize, "not use pools!");
#endif

	buf[bufsize-1] = 0;
}


