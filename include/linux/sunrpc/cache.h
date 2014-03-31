/*
 * include/linux/sunrpc/cache.h
 *
 * Generic code for various authentication-related caches
 * used by sunrpc clients and servers.
 *
 * Copyright (C) 2002 Neil Brown <neilb@cse.unsw.edu.au>
 *
 * Released under terms in GPL version 2.  See COPYING.
 *
 */

#ifndef _LINUX_SUNRPC_CACHE_H_
#define _LINUX_SUNRPC_CACHE_H_

#include <linux/kref.h>
#include <linux/slab.h>
#include <linux/atomic.h>
#include <linux/proc_fs.h>


struct cache_head {
	struct cache_head * next;
	time_t		expiry_time;	
	time_t		last_refresh;   
	struct kref	ref;
	unsigned long	flags;
};
#define	CACHE_VALID	0	
#define	CACHE_NEGATIVE	1	
#define	CACHE_PENDING	2	

#define	CACHE_NEW_EXPIRY 120	

struct cache_detail_procfs {
	struct proc_dir_entry	*proc_ent;
	struct proc_dir_entry   *flush_ent, *channel_ent, *content_ent;
};

struct cache_detail_pipefs {
	struct dentry *dir;
};

struct cache_detail {
	struct module *		owner;
	int			hash_size;
	struct cache_head **	hash_table;
	rwlock_t		hash_lock;

	atomic_t		inuse; 

	char			*name;
	void			(*cache_put)(struct kref *);

	int			(*cache_upcall)(struct cache_detail *,
						struct cache_head *);

	int			(*cache_parse)(struct cache_detail *,
					       char *buf, int len);

	int			(*cache_show)(struct seq_file *m,
					      struct cache_detail *cd,
					      struct cache_head *h);
	void			(*warn_no_listener)(struct cache_detail *cd,
					      int has_died);

	struct cache_head *	(*alloc)(void);
	int			(*match)(struct cache_head *orig, struct cache_head *new);
	void			(*init)(struct cache_head *orig, struct cache_head *new);
	void			(*update)(struct cache_head *orig, struct cache_head *new);

	time_t			flush_time;		
	struct list_head	others;
	time_t			nextcheck;
	int			entries;

	
	struct list_head	queue;

	atomic_t		readers;		
	time_t			last_close;		
	time_t			last_warn;		

	union {
		struct cache_detail_procfs procfs;
		struct cache_detail_pipefs pipefs;
	} u;
	struct net		*net;
};


struct cache_req {
	struct cache_deferred_req *(*defer)(struct cache_req *req);
	int thread_wait;  
};
struct cache_deferred_req {
	struct hlist_node	hash;	
	struct list_head	recent; 
	struct cache_head	*item;  
	void			*owner; 
	void			(*revisit)(struct cache_deferred_req *req,
					   int too_many);
};


extern const struct file_operations cache_file_operations_pipefs;
extern const struct file_operations content_file_operations_pipefs;
extern const struct file_operations cache_flush_operations_pipefs;

extern struct cache_head *
sunrpc_cache_lookup(struct cache_detail *detail,
		    struct cache_head *key, int hash);
extern struct cache_head *
sunrpc_cache_update(struct cache_detail *detail,
		    struct cache_head *new, struct cache_head *old, int hash);

extern int
sunrpc_cache_pipe_upcall(struct cache_detail *detail, struct cache_head *h,
		void (*cache_request)(struct cache_detail *,
				      struct cache_head *,
				      char **,
				      int *));


extern void cache_clean_deferred(void *owner);

static inline struct cache_head  *cache_get(struct cache_head *h)
{
	kref_get(&h->ref);
	return h;
}


static inline void cache_put(struct cache_head *h, struct cache_detail *cd)
{
	if (atomic_read(&h->ref.refcount) <= 2 &&
	    h->expiry_time < cd->nextcheck)
		cd->nextcheck = h->expiry_time;
	kref_put(&h->ref, cd->cache_put);
}

static inline int cache_valid(struct cache_head *h)
{
	return (h->expiry_time != 0 && test_bit(CACHE_VALID, &h->flags));
}

extern int cache_check(struct cache_detail *detail,
		       struct cache_head *h, struct cache_req *rqstp);
extern void cache_flush(void);
extern void cache_purge(struct cache_detail *detail);
#define NEVER (0x7FFFFFFF)
extern void __init cache_initialize(void);
extern int cache_register_net(struct cache_detail *cd, struct net *net);
extern void cache_unregister_net(struct cache_detail *cd, struct net *net);

extern struct cache_detail *cache_create_net(struct cache_detail *tmpl, struct net *net);
extern void cache_destroy_net(struct cache_detail *cd, struct net *net);

extern void sunrpc_init_cache_detail(struct cache_detail *cd);
extern void sunrpc_destroy_cache_detail(struct cache_detail *cd);
extern int sunrpc_cache_register_pipefs(struct dentry *parent, const char *,
					umode_t, struct cache_detail *);
extern void sunrpc_cache_unregister_pipefs(struct cache_detail *);

extern void qword_add(char **bpp, int *lp, char *str);
extern void qword_addhex(char **bpp, int *lp, char *buf, int blen);
extern int qword_get(char **bpp, char *dest, int bufsize);

static inline int get_int(char **bpp, int *anint)
{
	char buf[50];
	char *ep;
	int rv;
	int len = qword_get(bpp, buf, 50);
	if (len < 0) return -EINVAL;
	if (len ==0) return -ENOENT;
	rv = simple_strtol(buf, &ep, 0);
	if (*ep) return -EINVAL;
	*anint = rv;
	return 0;
}

static inline time_t seconds_since_boot(void)
{
	struct timespec boot;
	getboottime(&boot);
	return get_seconds() - boot.tv_sec;
}

static inline time_t convert_to_wallclock(time_t sinceboot)
{
	struct timespec boot;
	getboottime(&boot);
	return boot.tv_sec + sinceboot;
}

static inline time_t get_expiry(char **bpp)
{
	int rv;
	struct timespec boot;

	if (get_int(bpp, &rv))
		return 0;
	if (rv < 0)
		return 0;
	getboottime(&boot);
	return rv - boot.tv_sec;
}

#endif 
