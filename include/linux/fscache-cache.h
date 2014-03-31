/* General filesystem caching backing cache interface
 *
 * Copyright (C) 2004-2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * NOTE!!! See:
 *
 *	Documentation/filesystems/caching/backend-api.txt
 *
 * for a description of the cache backend interface declared here.
 */

#ifndef _LINUX_FSCACHE_CACHE_H
#define _LINUX_FSCACHE_CACHE_H

#include <linux/fscache.h>
#include <linux/sched.h>
#include <linux/workqueue.h>

#define NR_MAXCACHES BITS_PER_LONG

struct fscache_cache;
struct fscache_cache_ops;
struct fscache_object;
struct fscache_operation;

struct fscache_cache_tag {
	struct list_head	link;
	struct fscache_cache	*cache;		
	unsigned long		flags;
#define FSCACHE_TAG_RESERVED	0		
	atomic_t		usage;
	char			name[0];	
};

struct fscache_cache {
	const struct fscache_cache_ops *ops;
	struct fscache_cache_tag *tag;		
	struct kobject		*kobj;		
	struct list_head	link;		
	size_t			max_index_size;	
	char			identifier[36];	

	
	struct work_struct	op_gc;		
	struct list_head	object_list;	
	struct list_head	op_gc_list;	
	spinlock_t		object_list_lock;
	spinlock_t		op_gc_list_lock;
	atomic_t		object_count;	
	struct fscache_object	*fsdef;		
	unsigned long		flags;
#define FSCACHE_IOERROR		0	
#define FSCACHE_CACHE_WITHDRAWN	1	
};

extern wait_queue_head_t fscache_cache_cleared_wq;

typedef void (*fscache_operation_release_t)(struct fscache_operation *op);
typedef void (*fscache_operation_processor_t)(struct fscache_operation *op);

struct fscache_operation {
	struct work_struct	work;		
	struct list_head	pend_link;	
	struct fscache_object	*object;	

	unsigned long		flags;
#define FSCACHE_OP_TYPE		0x000f	
#define FSCACHE_OP_ASYNC	0x0001	
#define FSCACHE_OP_MYTHREAD	0x0002	
#define FSCACHE_OP_WAITING	4	
#define FSCACHE_OP_EXCLUSIVE	5	
#define FSCACHE_OP_DEAD		6	
#define FSCACHE_OP_DEC_READ_CNT	7	
#define FSCACHE_OP_KEEP_FLAGS	0xc0	

	atomic_t		usage;
	unsigned		debug_id;	

	fscache_operation_processor_t processor;

	
	fscache_operation_release_t release;
};

extern atomic_t fscache_op_debug_id;
extern void fscache_op_work_func(struct work_struct *work);

extern void fscache_enqueue_operation(struct fscache_operation *);
extern void fscache_put_operation(struct fscache_operation *);

static inline void fscache_operation_init(struct fscache_operation *op,
					fscache_operation_processor_t processor,
					fscache_operation_release_t release)
{
	INIT_WORK(&op->work, fscache_op_work_func);
	atomic_set(&op->usage, 1);
	op->debug_id = atomic_inc_return(&fscache_op_debug_id);
	op->processor = processor;
	op->release = release;
	INIT_LIST_HEAD(&op->pend_link);
}

struct fscache_retrieval {
	struct fscache_operation op;
	struct address_space	*mapping;	
	fscache_rw_complete_t	end_io_func;	
	void			*context;	
	struct list_head	to_do;		
	unsigned long		start_time;	
};

typedef int (*fscache_page_retrieval_func_t)(struct fscache_retrieval *op,
					     struct page *page,
					     gfp_t gfp);

typedef int (*fscache_pages_retrieval_func_t)(struct fscache_retrieval *op,
					      struct list_head *pages,
					      unsigned *nr_pages,
					      gfp_t gfp);

static inline
struct fscache_retrieval *fscache_get_retrieval(struct fscache_retrieval *op)
{
	atomic_inc(&op->op.usage);
	return op;
}

static inline void fscache_enqueue_retrieval(struct fscache_retrieval *op)
{
	fscache_enqueue_operation(&op->op);
}

static inline void fscache_put_retrieval(struct fscache_retrieval *op)
{
	fscache_put_operation(&op->op);
}

struct fscache_storage {
	struct fscache_operation op;
	pgoff_t			store_limit;	
};

struct fscache_cache_ops {
	
	const char *name;

	
	struct fscache_object *(*alloc_object)(struct fscache_cache *cache,
					       struct fscache_cookie *cookie);

	int (*lookup_object)(struct fscache_object *object);

	
	void (*lookup_complete)(struct fscache_object *object);

	
	struct fscache_object *(*grab_object)(struct fscache_object *object);

	
	int (*pin_object)(struct fscache_object *object);

	
	void (*unpin_object)(struct fscache_object *object);

	
	void (*update_object)(struct fscache_object *object);

	void (*drop_object)(struct fscache_object *object);

	
	void (*put_object)(struct fscache_object *object);

	
	void (*sync_cache)(struct fscache_cache *cache);

	int (*attr_changed)(struct fscache_object *object);

	
	int (*reserve_space)(struct fscache_object *object, loff_t i_size);

	fscache_page_retrieval_func_t read_or_alloc_page;

	fscache_pages_retrieval_func_t read_or_alloc_pages;

	/* request a backing block for a page be allocated in the cache so that
	 * it can be written directly */
	fscache_page_retrieval_func_t allocate_page;

	/* request backing blocks for pages be allocated in the cache so that
	 * they can be written directly */
	fscache_pages_retrieval_func_t allocate_pages;

	
	int (*write_page)(struct fscache_storage *op, struct page *page);

	void (*uncache_page)(struct fscache_object *object,
			     struct page *page);

	
	void (*dissociate_pages)(struct fscache_cache *cache);
};

struct fscache_cookie {
	atomic_t			usage;		
	atomic_t			n_children;	
	spinlock_t			lock;
	spinlock_t			stores_lock;	
	struct hlist_head		backing_objects; 
	const struct fscache_cookie_def	*def;		
	struct fscache_cookie		*parent;	
	void				*netfs_data;	
	struct radix_tree_root		stores;		
#define FSCACHE_COOKIE_PENDING_TAG	0		
#define FSCACHE_COOKIE_STORING_TAG	1		

	unsigned long			flags;
#define FSCACHE_COOKIE_LOOKING_UP	0	
#define FSCACHE_COOKIE_CREATING		1	
#define FSCACHE_COOKIE_NO_DATA_YET	2	
#define FSCACHE_COOKIE_PENDING_FILL	3	
#define FSCACHE_COOKIE_FILLING		4	
#define FSCACHE_COOKIE_UNAVAILABLE	5	
};

extern struct fscache_cookie fscache_fsdef_index;

struct fscache_object {
	enum fscache_object_state {
		FSCACHE_OBJECT_INIT,		
		FSCACHE_OBJECT_LOOKING_UP,	
		FSCACHE_OBJECT_CREATING,	

		
		FSCACHE_OBJECT_AVAILABLE,	
		FSCACHE_OBJECT_ACTIVE,		
		FSCACHE_OBJECT_UPDATING,	

		
		FSCACHE_OBJECT_DYING,		
		FSCACHE_OBJECT_LC_DYING,	
		FSCACHE_OBJECT_ABORT_INIT,	
		FSCACHE_OBJECT_RELEASING,	
		FSCACHE_OBJECT_RECYCLING,	
		FSCACHE_OBJECT_WITHDRAWING,	
		FSCACHE_OBJECT_DEAD,		
		FSCACHE_OBJECT__NSTATES
	} state;

	int			debug_id;	
	int			n_children;	
	int			n_ops;		
	int			n_obj_ops;	
	int			n_in_progress;	
	int			n_exclusive;	
	atomic_t		n_reads;	
	spinlock_t		lock;		

	unsigned long		lookup_jif;	
	unsigned long		event_mask;	
	unsigned long		events;		
#define FSCACHE_OBJECT_EV_REQUEUE	0	
#define FSCACHE_OBJECT_EV_UPDATE	1	
#define FSCACHE_OBJECT_EV_CLEARED	2	
#define FSCACHE_OBJECT_EV_ERROR		3	
#define FSCACHE_OBJECT_EV_RELEASE	4	
#define FSCACHE_OBJECT_EV_RETIRE	5	
#define FSCACHE_OBJECT_EV_WITHDRAW	6	
#define FSCACHE_OBJECT_EVENTS_MASK	0x7f	

	unsigned long		flags;
#define FSCACHE_OBJECT_LOCK		0	
#define FSCACHE_OBJECT_PENDING_WRITE	1	
#define FSCACHE_OBJECT_WAITING		2	

	struct list_head	cache_link;	
	struct hlist_node	cookie_link;	
	struct fscache_cache	*cache;		
	struct fscache_cookie	*cookie;	
	struct fscache_object	*parent;	
	struct work_struct	work;		
	struct list_head	dependents;	
	struct list_head	dep_link;	
	struct list_head	pending_ops;	
#ifdef CONFIG_FSCACHE_OBJECT_LIST
	struct rb_node		objlist_link;	
#endif
	pgoff_t			store_limit;	
	loff_t			store_limit_l;	
};

extern const char *fscache_object_states[];

#define fscache_object_is_active(obj)			      \
	(!test_bit(FSCACHE_IOERROR, &(obj)->cache->flags) &&  \
	 (obj)->state >= FSCACHE_OBJECT_AVAILABLE &&	      \
	 (obj)->state < FSCACHE_OBJECT_DYING)

#define fscache_object_is_dead(obj)				\
	(test_bit(FSCACHE_IOERROR, &(obj)->cache->flags) &&	\
	 (obj)->state >= FSCACHE_OBJECT_DYING)

extern void fscache_object_work_func(struct work_struct *work);

static inline
void fscache_object_init(struct fscache_object *object,
			 struct fscache_cookie *cookie,
			 struct fscache_cache *cache)
{
	atomic_inc(&cache->object_count);

	object->state = FSCACHE_OBJECT_INIT;
	spin_lock_init(&object->lock);
	INIT_LIST_HEAD(&object->cache_link);
	INIT_HLIST_NODE(&object->cookie_link);
	INIT_WORK(&object->work, fscache_object_work_func);
	INIT_LIST_HEAD(&object->dependents);
	INIT_LIST_HEAD(&object->dep_link);
	INIT_LIST_HEAD(&object->pending_ops);
	object->n_children = 0;
	object->n_ops = object->n_in_progress = object->n_exclusive = 0;
	object->events = object->event_mask = 0;
	object->flags = 0;
	object->store_limit = 0;
	object->store_limit_l = 0;
	object->cache = cache;
	object->cookie = cookie;
	object->parent = NULL;
}

extern void fscache_object_lookup_negative(struct fscache_object *object);
extern void fscache_obtained_object(struct fscache_object *object);

#ifdef CONFIG_FSCACHE_OBJECT_LIST
extern void fscache_object_destroy(struct fscache_object *object);
#else
#define fscache_object_destroy(object) do {} while(0)
#endif

static inline void fscache_object_destroyed(struct fscache_cache *cache)
{
	if (atomic_dec_and_test(&cache->object_count))
		wake_up_all(&fscache_cache_cleared_wq);
}

static inline void fscache_object_lookup_error(struct fscache_object *object)
{
	set_bit(FSCACHE_OBJECT_EV_ERROR, &object->events);
}

/**
 * fscache_set_store_limit - Set the maximum size to be stored in an object
 * @object: The object to set the maximum on
 * @i_size: The limit to set in bytes
 *
 * Set the maximum size an object is permitted to reach, implying the highest
 * byte that may be written.  Intended to be called by the attr_changed() op.
 *
 * See Documentation/filesystems/caching/backend-api.txt for a complete
 * description.
 */
static inline
void fscache_set_store_limit(struct fscache_object *object, loff_t i_size)
{
	object->store_limit_l = i_size;
	object->store_limit = i_size >> PAGE_SHIFT;
	if (i_size & ~PAGE_MASK)
		object->store_limit++;
}

static inline void fscache_end_io(struct fscache_retrieval *op,
				  struct page *page, int error)
{
	op->end_io_func(page, op->context, error);
}

extern __printf(3, 4)
void fscache_init_cache(struct fscache_cache *cache,
			const struct fscache_cache_ops *ops,
			const char *idfmt, ...);

extern int fscache_add_cache(struct fscache_cache *cache,
			     struct fscache_object *fsdef,
			     const char *tagname);
extern void fscache_withdraw_cache(struct fscache_cache *cache);

extern void fscache_io_error(struct fscache_cache *cache);

extern void fscache_mark_pages_cached(struct fscache_retrieval *op,
				      struct pagevec *pagevec);

extern bool fscache_object_sleep_till_congested(signed long *timeoutp);

extern enum fscache_checkaux fscache_check_aux(struct fscache_object *object,
					       const void *data,
					       uint16_t datalen);

#endif 
