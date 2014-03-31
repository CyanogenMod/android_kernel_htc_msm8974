/* General netfs cache on cache files internal defs
 *
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */

#include <linux/fscache-cache.h>
#include <linux/timer.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/security.h>

struct cachefiles_cache;
struct cachefiles_object;

extern unsigned cachefiles_debug;
#define CACHEFILES_DEBUG_KENTER	1
#define CACHEFILES_DEBUG_KLEAVE	2
#define CACHEFILES_DEBUG_KDEBUG	4

struct cachefiles_object {
	struct fscache_object		fscache;	
	struct cachefiles_lookup_data	*lookup_data;	
	struct dentry			*dentry;	
	struct dentry			*backer;	
	loff_t				i_size;		
	unsigned long			flags;
#define CACHEFILES_OBJECT_ACTIVE	0		
#define CACHEFILES_OBJECT_BURIED	1		
	atomic_t			usage;		
	uint8_t				type;		
	uint8_t				new;		
	spinlock_t			work_lock;
	struct rb_node			active_node;	
};

extern struct kmem_cache *cachefiles_object_jar;

struct cachefiles_cache {
	struct fscache_cache		cache;		
	struct vfsmount			*mnt;		
	struct dentry			*graveyard;	
	struct file			*cachefilesd;	
	const struct cred		*cache_cred;	
	struct mutex			daemon_mutex;	
	wait_queue_head_t		daemon_pollwq;	
	struct rb_root			active_nodes;	
	rwlock_t			active_lock;	
	atomic_t			gravecounter;	
	unsigned			frun_percent;	
	unsigned			fcull_percent;	
	unsigned			fstop_percent;	
	unsigned			brun_percent;	
	unsigned			bcull_percent;	
	unsigned			bstop_percent;	
	unsigned			bsize;		
	unsigned			bshift;		
	uint64_t			frun;		
	uint64_t			fcull;		
	uint64_t			fstop;		
	sector_t			brun;		
	sector_t			bcull;		
	sector_t			bstop;		
	unsigned long			flags;
#define CACHEFILES_READY		0	
#define CACHEFILES_DEAD			1	
#define CACHEFILES_CULLING		2	
#define CACHEFILES_STATE_CHANGED	3	
	char				*rootdirname;	
	char				*secctx;	
	char				*tag;		
};

struct cachefiles_one_read {
	wait_queue_t			monitor;	
	struct page			*back_page;	
	struct page			*netfs_page;	
	struct fscache_retrieval	*op;		
	struct list_head		op_link;	
};

struct cachefiles_one_write {
	struct page			*netfs_page;	
	struct cachefiles_object	*object;
	struct list_head		obj_link;	
	fscache_rw_complete_t		end_io_func;
	void				*context;
};

struct cachefiles_xattr {
	uint16_t			len;
	uint8_t				type;
	uint8_t				data[];
};

static inline void cachefiles_state_changed(struct cachefiles_cache *cache)
{
	set_bit(CACHEFILES_STATE_CHANGED, &cache->flags);
	wake_up_all(&cache->daemon_pollwq);
}

extern int cachefiles_daemon_bind(struct cachefiles_cache *cache, char *args);
extern void cachefiles_daemon_unbind(struct cachefiles_cache *cache);

extern const struct file_operations cachefiles_daemon_fops;

extern int cachefiles_has_space(struct cachefiles_cache *cache,
				unsigned fnr, unsigned bnr);

extern const struct fscache_cache_ops cachefiles_cache_ops;

extern char *cachefiles_cook_key(const u8 *raw, int keylen, uint8_t type);

extern int cachefiles_delete_object(struct cachefiles_cache *cache,
				    struct cachefiles_object *object);
extern int cachefiles_walk_to_object(struct cachefiles_object *parent,
				     struct cachefiles_object *object,
				     const char *key,
				     struct cachefiles_xattr *auxdata);
extern struct dentry *cachefiles_get_directory(struct cachefiles_cache *cache,
					       struct dentry *dir,
					       const char *name);

extern int cachefiles_cull(struct cachefiles_cache *cache, struct dentry *dir,
			   char *filename);

extern int cachefiles_check_in_use(struct cachefiles_cache *cache,
				   struct dentry *dir, char *filename);

#ifdef CONFIG_CACHEFILES_HISTOGRAM
extern atomic_t cachefiles_lookup_histogram[HZ];
extern atomic_t cachefiles_mkdir_histogram[HZ];
extern atomic_t cachefiles_create_histogram[HZ];

extern int __init cachefiles_proc_init(void);
extern void cachefiles_proc_cleanup(void);
static inline
void cachefiles_hist(atomic_t histogram[], unsigned long start_jif)
{
	unsigned long jif = jiffies - start_jif;
	if (jif >= HZ)
		jif = HZ - 1;
	atomic_inc(&histogram[jif]);
}

#else
#define cachefiles_proc_init()		(0)
#define cachefiles_proc_cleanup()	do {} while (0)
#define cachefiles_hist(hist, start_jif) do {} while (0)
#endif

extern int cachefiles_read_or_alloc_page(struct fscache_retrieval *,
					 struct page *, gfp_t);
extern int cachefiles_read_or_alloc_pages(struct fscache_retrieval *,
					  struct list_head *, unsigned *,
					  gfp_t);
extern int cachefiles_allocate_page(struct fscache_retrieval *, struct page *,
				    gfp_t);
extern int cachefiles_allocate_pages(struct fscache_retrieval *,
				     struct list_head *, unsigned *, gfp_t);
extern int cachefiles_write_page(struct fscache_storage *, struct page *);
extern void cachefiles_uncache_page(struct fscache_object *, struct page *);

extern int cachefiles_get_security_ID(struct cachefiles_cache *cache);
extern int cachefiles_determine_cache_security(struct cachefiles_cache *cache,
					       struct dentry *root,
					       const struct cred **_saved_cred);

static inline void cachefiles_begin_secure(struct cachefiles_cache *cache,
					   const struct cred **_saved_cred)
{
	*_saved_cred = override_creds(cache->cache_cred);
}

static inline void cachefiles_end_secure(struct cachefiles_cache *cache,
					 const struct cred *saved_cred)
{
	revert_creds(saved_cred);
}

extern int cachefiles_check_object_type(struct cachefiles_object *object);
extern int cachefiles_set_object_xattr(struct cachefiles_object *object,
				       struct cachefiles_xattr *auxdata);
extern int cachefiles_update_object_xattr(struct cachefiles_object *object,
					  struct cachefiles_xattr *auxdata);
extern int cachefiles_check_object_xattr(struct cachefiles_object *object,
					 struct cachefiles_xattr *auxdata);
extern int cachefiles_remove_object_xattr(struct cachefiles_cache *cache,
					  struct dentry *dentry);


#define kerror(FMT, ...) printk(KERN_ERR "CacheFiles: "FMT"\n", ##__VA_ARGS__)

#define cachefiles_io_error(___cache, FMT, ...)		\
do {							\
	kerror("I/O Error: " FMT, ##__VA_ARGS__);	\
	fscache_io_error(&(___cache)->cache);		\
	set_bit(CACHEFILES_DEAD, &(___cache)->flags);	\
} while (0)

#define cachefiles_io_error_obj(object, FMT, ...)			\
do {									\
	struct cachefiles_cache *___cache;				\
									\
	___cache = container_of((object)->fscache.cache,		\
				struct cachefiles_cache, cache);	\
	cachefiles_io_error(___cache, FMT, ##__VA_ARGS__);		\
} while (0)


#define dbgprintk(FMT, ...) \
	printk(KERN_DEBUG "[%-6.6s] "FMT"\n", current->comm, ##__VA_ARGS__)

#define kenter(FMT, ...) dbgprintk("==> %s("FMT")", __func__, ##__VA_ARGS__)
#define kleave(FMT, ...) dbgprintk("<== %s()"FMT"", __func__, ##__VA_ARGS__)
#define kdebug(FMT, ...) dbgprintk(FMT, ##__VA_ARGS__)


#if defined(__KDEBUG)
#define _enter(FMT, ...) kenter(FMT, ##__VA_ARGS__)
#define _leave(FMT, ...) kleave(FMT, ##__VA_ARGS__)
#define _debug(FMT, ...) kdebug(FMT, ##__VA_ARGS__)

#elif defined(CONFIG_CACHEFILES_DEBUG)
#define _enter(FMT, ...)				\
do {							\
	if (cachefiles_debug & CACHEFILES_DEBUG_KENTER)	\
		kenter(FMT, ##__VA_ARGS__);		\
} while (0)

#define _leave(FMT, ...)				\
do {							\
	if (cachefiles_debug & CACHEFILES_DEBUG_KLEAVE)	\
		kleave(FMT, ##__VA_ARGS__);		\
} while (0)

#define _debug(FMT, ...)				\
do {							\
	if (cachefiles_debug & CACHEFILES_DEBUG_KDEBUG)	\
		kdebug(FMT, ##__VA_ARGS__);		\
} while (0)

#else
#define _enter(FMT, ...) no_printk("==> %s("FMT")", __func__, ##__VA_ARGS__)
#define _leave(FMT, ...) no_printk("<== %s()"FMT"", __func__, ##__VA_ARGS__)
#define _debug(FMT, ...) no_printk(FMT, ##__VA_ARGS__)
#endif

#if 1 

#define ASSERT(X)							\
do {									\
	if (unlikely(!(X))) {						\
		printk(KERN_ERR "\n");					\
		printk(KERN_ERR "CacheFiles: Assertion failed\n");	\
		BUG();							\
	}								\
} while (0)

#define ASSERTCMP(X, OP, Y)						\
do {									\
	if (unlikely(!((X) OP (Y)))) {					\
		printk(KERN_ERR "\n");					\
		printk(KERN_ERR "CacheFiles: Assertion failed\n");	\
		printk(KERN_ERR "%lx " #OP " %lx is false\n",		\
		       (unsigned long)(X), (unsigned long)(Y));		\
		BUG();							\
	}								\
} while (0)

#define ASSERTIF(C, X)							\
do {									\
	if (unlikely((C) && !(X))) {					\
		printk(KERN_ERR "\n");					\
		printk(KERN_ERR "CacheFiles: Assertion failed\n");	\
		BUG();							\
	}								\
} while (0)

#define ASSERTIFCMP(C, X, OP, Y)					\
do {									\
	if (unlikely((C) && !((X) OP (Y)))) {				\
		printk(KERN_ERR "\n");					\
		printk(KERN_ERR "CacheFiles: Assertion failed\n");	\
		printk(KERN_ERR "%lx " #OP " %lx is false\n",		\
		       (unsigned long)(X), (unsigned long)(Y));		\
		BUG();							\
	}								\
} while (0)

#else

#define ASSERT(X)			do {} while (0)
#define ASSERTCMP(X, OP, Y)		do {} while (0)
#define ASSERTIF(C, X)			do {} while (0)
#define ASSERTIFCMP(C, X, OP, Y)	do {} while (0)

#endif
