/* General filesystem caching interface
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
 *	Documentation/filesystems/caching/netfs-api.txt
 *
 * for a description of the network filesystem interface declared here.
 */

#ifndef _LINUX_FSCACHE_H
#define _LINUX_FSCACHE_H

#include <linux/fs.h>
#include <linux/list.h>
#include <linux/pagemap.h>
#include <linux/pagevec.h>

#if defined(CONFIG_FSCACHE) || defined(CONFIG_FSCACHE_MODULE)
#define fscache_available() (1)
#define fscache_cookie_valid(cookie) (cookie)
#else
#define fscache_available() (0)
#define fscache_cookie_valid(cookie) (0)
#endif


#define PageFsCache(page)		PagePrivate2((page))
#define SetPageFsCache(page)		SetPagePrivate2((page))
#define ClearPageFsCache(page)		ClearPagePrivate2((page))
#define TestSetPageFsCache(page)	TestSetPagePrivate2((page))
#define TestClearPageFsCache(page)	TestClearPagePrivate2((page))

#define FSCACHE_INDEX_DEADFILL_PATTERN 0x79

struct pagevec;
struct fscache_cache_tag;
struct fscache_cookie;
struct fscache_netfs;

typedef void (*fscache_rw_complete_t)(struct page *page,
				      void *context,
				      int error);

enum fscache_checkaux {
	FSCACHE_CHECKAUX_OKAY,		
	FSCACHE_CHECKAUX_NEEDS_UPDATE,	
	FSCACHE_CHECKAUX_OBSOLETE,	
};

struct fscache_cookie_def {
	
	char name[16];

	
	uint8_t type;
#define FSCACHE_COOKIE_TYPE_INDEX	0
#define FSCACHE_COOKIE_TYPE_DATAFILE	1

	struct fscache_cache_tag *(*select_cache)(
		const void *parent_netfs_data,
		const void *cookie_netfs_data);

	uint16_t (*get_key)(const void *cookie_netfs_data,
			    void *buffer,
			    uint16_t bufmax);

	void (*get_attr)(const void *cookie_netfs_data, uint64_t *size);

	uint16_t (*get_aux)(const void *cookie_netfs_data,
			    void *buffer,
			    uint16_t bufmax);

	enum fscache_checkaux (*check_aux)(void *cookie_netfs_data,
					   const void *data,
					   uint16_t datalen);

	void (*get_context)(void *cookie_netfs_data, void *context);

	void (*put_context)(void *cookie_netfs_data, void *context);

	void (*mark_pages_cached)(void *cookie_netfs_data,
				  struct address_space *mapping,
				  struct pagevec *cached_pvec);

	void (*now_uncached)(void *cookie_netfs_data);
};

struct fscache_netfs {
	uint32_t			version;	
	const char			*name;		
	struct fscache_cookie		*primary_index;
	struct list_head		link;		
};

extern int __fscache_register_netfs(struct fscache_netfs *);
extern void __fscache_unregister_netfs(struct fscache_netfs *);
extern struct fscache_cache_tag *__fscache_lookup_cache_tag(const char *);
extern void __fscache_release_cache_tag(struct fscache_cache_tag *);

extern struct fscache_cookie *__fscache_acquire_cookie(
	struct fscache_cookie *,
	const struct fscache_cookie_def *,
	void *);
extern void __fscache_relinquish_cookie(struct fscache_cookie *, int);
extern void __fscache_update_cookie(struct fscache_cookie *);
extern int __fscache_attr_changed(struct fscache_cookie *);
extern int __fscache_read_or_alloc_page(struct fscache_cookie *,
					struct page *,
					fscache_rw_complete_t,
					void *,
					gfp_t);
extern int __fscache_read_or_alloc_pages(struct fscache_cookie *,
					 struct address_space *,
					 struct list_head *,
					 unsigned *,
					 fscache_rw_complete_t,
					 void *,
					 gfp_t);
extern int __fscache_alloc_page(struct fscache_cookie *, struct page *, gfp_t);
extern int __fscache_write_page(struct fscache_cookie *, struct page *, gfp_t);
extern void __fscache_uncache_page(struct fscache_cookie *, struct page *);
extern bool __fscache_check_page_write(struct fscache_cookie *, struct page *);
extern void __fscache_wait_on_page_write(struct fscache_cookie *, struct page *);
extern bool __fscache_maybe_release_page(struct fscache_cookie *, struct page *,
					 gfp_t);
extern void __fscache_uncache_all_inode_pages(struct fscache_cookie *,
					      struct inode *);

static inline
int fscache_register_netfs(struct fscache_netfs *netfs)
{
	if (fscache_available())
		return __fscache_register_netfs(netfs);
	else
		return 0;
}

static inline
void fscache_unregister_netfs(struct fscache_netfs *netfs)
{
	if (fscache_available())
		__fscache_unregister_netfs(netfs);
}

static inline
struct fscache_cache_tag *fscache_lookup_cache_tag(const char *name)
{
	if (fscache_available())
		return __fscache_lookup_cache_tag(name);
	else
		return NULL;
}

static inline
void fscache_release_cache_tag(struct fscache_cache_tag *tag)
{
	if (fscache_available())
		__fscache_release_cache_tag(tag);
}

static inline
struct fscache_cookie *fscache_acquire_cookie(
	struct fscache_cookie *parent,
	const struct fscache_cookie_def *def,
	void *netfs_data)
{
	if (fscache_cookie_valid(parent))
		return __fscache_acquire_cookie(parent, def, netfs_data);
	else
		return NULL;
}

static inline
void fscache_relinquish_cookie(struct fscache_cookie *cookie, int retire)
{
	if (fscache_cookie_valid(cookie))
		__fscache_relinquish_cookie(cookie, retire);
}

static inline
void fscache_update_cookie(struct fscache_cookie *cookie)
{
	if (fscache_cookie_valid(cookie))
		__fscache_update_cookie(cookie);
}

static inline
int fscache_pin_cookie(struct fscache_cookie *cookie)
{
	return -ENOBUFS;
}

static inline
void fscache_unpin_cookie(struct fscache_cookie *cookie)
{
}

static inline
int fscache_attr_changed(struct fscache_cookie *cookie)
{
	if (fscache_cookie_valid(cookie))
		return __fscache_attr_changed(cookie);
	else
		return -ENOBUFS;
}

static inline
int fscache_reserve_space(struct fscache_cookie *cookie, loff_t size)
{
	return -ENOBUFS;
}

static inline
int fscache_read_or_alloc_page(struct fscache_cookie *cookie,
			       struct page *page,
			       fscache_rw_complete_t end_io_func,
			       void *context,
			       gfp_t gfp)
{
	if (fscache_cookie_valid(cookie))
		return __fscache_read_or_alloc_page(cookie, page, end_io_func,
						    context, gfp);
	else
		return -ENOBUFS;
}

static inline
int fscache_read_or_alloc_pages(struct fscache_cookie *cookie,
				struct address_space *mapping,
				struct list_head *pages,
				unsigned *nr_pages,
				fscache_rw_complete_t end_io_func,
				void *context,
				gfp_t gfp)
{
	if (fscache_cookie_valid(cookie))
		return __fscache_read_or_alloc_pages(cookie, mapping, pages,
						     nr_pages, end_io_func,
						     context, gfp);
	else
		return -ENOBUFS;
}

static inline
int fscache_alloc_page(struct fscache_cookie *cookie,
		       struct page *page,
		       gfp_t gfp)
{
	if (fscache_cookie_valid(cookie))
		return __fscache_alloc_page(cookie, page, gfp);
	else
		return -ENOBUFS;
}

/**
 * fscache_write_page - Request storage of a page in the cache
 * @cookie: The cookie representing the cache object
 * @page: The netfs page to store
 * @gfp: The conditions under which memory allocation should be made
 *
 * Request the contents of the netfs page be written into the cache.  This
 * request may be ignored if no cache block is currently allocated, in which
 * case it will return -ENOBUFS.
 *
 * If a cache block was already allocated, a write will be initiated and 0 will
 * be returned.  The PG_fscache_write page bit is set immediately and will then
 * be cleared at the completion of the write to indicate the success or failure
 * of the operation.  Note that the completion may happen before the return.
 *
 * See Documentation/filesystems/caching/netfs-api.txt for a complete
 * description.
 */
static inline
int fscache_write_page(struct fscache_cookie *cookie,
		       struct page *page,
		       gfp_t gfp)
{
	if (fscache_cookie_valid(cookie))
		return __fscache_write_page(cookie, page, gfp);
	else
		return -ENOBUFS;
}

static inline
void fscache_uncache_page(struct fscache_cookie *cookie,
			  struct page *page)
{
	if (fscache_cookie_valid(cookie))
		__fscache_uncache_page(cookie, page);
}

/**
 * fscache_check_page_write - Ask if a page is being writing to the cache
 * @cookie: The cookie representing the cache object
 * @page: The netfs page that is being cached.
 *
 * Ask the cache if a page is being written to the cache.
 *
 * See Documentation/filesystems/caching/netfs-api.txt for a complete
 * description.
 */
static inline
bool fscache_check_page_write(struct fscache_cookie *cookie,
			      struct page *page)
{
	if (fscache_cookie_valid(cookie))
		return __fscache_check_page_write(cookie, page);
	return false;
}

/**
 * fscache_wait_on_page_write - Wait for a page to complete writing to the cache
 * @cookie: The cookie representing the cache object
 * @page: The netfs page that is being cached.
 *
 * Ask the cache to wake us up when a page is no longer being written to the
 * cache.
 *
 * See Documentation/filesystems/caching/netfs-api.txt for a complete
 * description.
 */
static inline
void fscache_wait_on_page_write(struct fscache_cookie *cookie,
				struct page *page)
{
	if (fscache_cookie_valid(cookie))
		__fscache_wait_on_page_write(cookie, page);
}

static inline
bool fscache_maybe_release_page(struct fscache_cookie *cookie,
				struct page *page,
				gfp_t gfp)
{
	if (fscache_cookie_valid(cookie) && PageFsCache(page))
		return __fscache_maybe_release_page(cookie, page, gfp);
	return false;
}

/**
 * fscache_uncache_all_inode_pages - Uncache all an inode's pages
 * @cookie: The cookie representing the inode's cache object.
 * @inode: The inode to uncache pages from.
 *
 * Uncache all the pages in an inode that are marked PG_fscache, assuming them
 * to be associated with the given cookie.
 *
 * This function may sleep.  It will wait for pages that are being written out
 * and will wait whilst the PG_fscache mark is removed by the cache.
 */
static inline
void fscache_uncache_all_inode_pages(struct fscache_cookie *cookie,
				     struct inode *inode)
{
	if (fscache_cookie_valid(cookie))
		__fscache_uncache_all_inode_pages(cookie, inode);
}

#endif 
