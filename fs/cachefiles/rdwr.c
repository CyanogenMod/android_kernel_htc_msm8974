/* Storage object read/write
 *
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */

#include <linux/mount.h>
#include <linux/slab.h>
#include <linux/file.h>
#include "internal.h"

static int cachefiles_read_waiter(wait_queue_t *wait, unsigned mode,
				  int sync, void *_key)
{
	struct cachefiles_one_read *monitor =
		container_of(wait, struct cachefiles_one_read, monitor);
	struct cachefiles_object *object;
	struct wait_bit_key *key = _key;
	struct page *page = wait->private;

	ASSERT(key);

	_enter("{%lu},%u,%d,{%p,%u}",
	       monitor->netfs_page->index, mode, sync,
	       key->flags, key->bit_nr);

	if (key->flags != &page->flags ||
	    key->bit_nr != PG_locked)
		return 0;

	_debug("--- monitor %p %lx ---", page, page->flags);

	if (!PageUptodate(page) && !PageError(page)) {
		
		_debug("page probably truncated");
	}

	
	list_del(&wait->task_list);

	
	ASSERT(monitor->op);

	object = container_of(monitor->op->op.object,
			      struct cachefiles_object, fscache);

	spin_lock(&object->work_lock);
	list_add_tail(&monitor->op_link, &monitor->op->to_do);
	spin_unlock(&object->work_lock);

	fscache_enqueue_retrieval(monitor->op);
	return 0;
}

static int cachefiles_read_reissue(struct cachefiles_object *object,
				   struct cachefiles_one_read *monitor)
{
	struct address_space *bmapping = object->backer->d_inode->i_mapping;
	struct page *backpage = monitor->back_page, *backpage2;
	int ret;

	kenter("{ino=%lx},{%lx,%lx}",
	       object->backer->d_inode->i_ino,
	       backpage->index, backpage->flags);

	
	if (backpage->mapping != bmapping) {
		kleave(" = -ENODATA [mapping]");
		return -ENODATA;
	}

	backpage2 = find_get_page(bmapping, backpage->index);
	if (!backpage2) {
		kleave(" = -ENODATA [gone]");
		return -ENODATA;
	}

	if (backpage != backpage2) {
		put_page(backpage2);
		kleave(" = -ENODATA [different]");
		return -ENODATA;
	}

	put_page(backpage2);

	INIT_LIST_HEAD(&monitor->op_link);
	add_page_wait_queue(backpage, &monitor->monitor);

	if (trylock_page(backpage)) {
		ret = -EIO;
		if (PageError(backpage))
			goto unlock_discard;
		ret = 0;
		if (PageUptodate(backpage))
			goto unlock_discard;

		kdebug("reissue read");
		ret = bmapping->a_ops->readpage(NULL, backpage);
		if (ret < 0)
			goto unlock_discard;
	}

	if (trylock_page(backpage)) {
		_debug("jumpstart %p {%lx}", backpage, backpage->flags);
		unlock_page(backpage);
	}

	
	kleave(" = -EINPROGRESS");
	return -EINPROGRESS;

unlock_discard:
	unlock_page(backpage);
	spin_lock_irq(&object->work_lock);
	list_del(&monitor->op_link);
	spin_unlock_irq(&object->work_lock);
	kleave(" = %d", ret);
	return ret;
}

static void cachefiles_read_copier(struct fscache_operation *_op)
{
	struct cachefiles_one_read *monitor;
	struct cachefiles_object *object;
	struct fscache_retrieval *op;
	struct pagevec pagevec;
	int error, max;

	op = container_of(_op, struct fscache_retrieval, op);
	object = container_of(op->op.object,
			      struct cachefiles_object, fscache);

	_enter("{ino=%lu}", object->backer->d_inode->i_ino);

	pagevec_init(&pagevec, 0);

	max = 8;
	spin_lock_irq(&object->work_lock);

	while (!list_empty(&op->to_do)) {
		monitor = list_entry(op->to_do.next,
				     struct cachefiles_one_read, op_link);
		list_del(&monitor->op_link);

		spin_unlock_irq(&object->work_lock);

		_debug("- copy {%lu}", monitor->back_page->index);

	recheck:
		if (PageUptodate(monitor->back_page)) {
			copy_highpage(monitor->netfs_page, monitor->back_page);

			pagevec_add(&pagevec, monitor->netfs_page);
			fscache_mark_pages_cached(monitor->op, &pagevec);
			error = 0;
		} else if (!PageError(monitor->back_page)) {
			
			error = cachefiles_read_reissue(object, monitor);
			if (error == -EINPROGRESS)
				goto next;
			goto recheck;
		} else {
			cachefiles_io_error_obj(
				object,
				"Readpage failed on backing file %lx",
				(unsigned long) monitor->back_page->flags);
			error = -EIO;
		}

		page_cache_release(monitor->back_page);

		fscache_end_io(op, monitor->netfs_page, error);
		page_cache_release(monitor->netfs_page);
		fscache_put_retrieval(op);
		kfree(monitor);

	next:
		
		max--;
		if (max < 0 || need_resched()) {
			if (!list_empty(&op->to_do))
				fscache_enqueue_retrieval(op);
			_leave(" [maxed out]");
			return;
		}

		spin_lock_irq(&object->work_lock);
	}

	spin_unlock_irq(&object->work_lock);
	_leave("");
}

static int cachefiles_read_backing_file_one(struct cachefiles_object *object,
					    struct fscache_retrieval *op,
					    struct page *netpage,
					    struct pagevec *pagevec)
{
	struct cachefiles_one_read *monitor;
	struct address_space *bmapping;
	struct page *newpage, *backpage;
	int ret;

	_enter("");

	pagevec_reinit(pagevec);

	_debug("read back %p{%lu,%d}",
	       netpage, netpage->index, page_count(netpage));

	monitor = kzalloc(sizeof(*monitor), GFP_KERNEL);
	if (!monitor)
		goto nomem;

	monitor->netfs_page = netpage;
	monitor->op = fscache_get_retrieval(op);

	init_waitqueue_func_entry(&monitor->monitor, cachefiles_read_waiter);

	
	bmapping = object->backer->d_inode->i_mapping;
	newpage = NULL;

	for (;;) {
		backpage = find_get_page(bmapping, netpage->index);
		if (backpage)
			goto backing_page_already_present;

		if (!newpage) {
			newpage = page_cache_alloc_cold(bmapping);
			if (!newpage)
				goto nomem_monitor;
		}

		ret = add_to_page_cache(newpage, bmapping,
					netpage->index, GFP_KERNEL);
		if (ret == 0)
			goto installed_new_backing_page;
		if (ret != -EEXIST)
			goto nomem_page;
	}

installed_new_backing_page:
	_debug("- new %p", newpage);

	backpage = newpage;
	newpage = NULL;

	page_cache_get(backpage);
	pagevec_add(pagevec, backpage);
	__pagevec_lru_add_file(pagevec);

read_backing_page:
	ret = bmapping->a_ops->readpage(NULL, backpage);
	if (ret < 0)
		goto read_error;

	
monitor_backing_page:
	_debug("- monitor add");

	
	page_cache_get(monitor->netfs_page);
	page_cache_get(backpage);
	monitor->back_page = backpage;
	monitor->monitor.private = backpage;
	add_page_wait_queue(backpage, &monitor->monitor);
	monitor = NULL;

	if (trylock_page(backpage)) {
		_debug("jumpstart %p {%lx}", backpage, backpage->flags);
		unlock_page(backpage);
	}
	goto success;

backing_page_already_present:
	_debug("- present");

	if (newpage) {
		page_cache_release(newpage);
		newpage = NULL;
	}

	if (PageError(backpage))
		goto io_error;

	if (PageUptodate(backpage))
		goto backing_page_already_uptodate;

	if (!trylock_page(backpage))
		goto monitor_backing_page;
	_debug("read %p {%lx}", backpage, backpage->flags);
	goto read_backing_page;

backing_page_already_uptodate:
	_debug("- uptodate");

	pagevec_add(pagevec, netpage);
	fscache_mark_pages_cached(op, pagevec);

	copy_highpage(netpage, backpage);
	fscache_end_io(op, netpage, 0);

success:
	_debug("success");
	ret = 0;

out:
	if (backpage)
		page_cache_release(backpage);
	if (monitor) {
		fscache_put_retrieval(monitor->op);
		kfree(monitor);
	}
	_leave(" = %d", ret);
	return ret;

read_error:
	_debug("read error %d", ret);
	if (ret == -ENOMEM)
		goto out;
io_error:
	cachefiles_io_error_obj(object, "Page read error on backing file");
	ret = -ENOBUFS;
	goto out;

nomem_page:
	page_cache_release(newpage);
nomem_monitor:
	fscache_put_retrieval(monitor->op);
	kfree(monitor);
nomem:
	_leave(" = -ENOMEM");
	return -ENOMEM;
}

int cachefiles_read_or_alloc_page(struct fscache_retrieval *op,
				  struct page *page,
				  gfp_t gfp)
{
	struct cachefiles_object *object;
	struct cachefiles_cache *cache;
	struct pagevec pagevec;
	struct inode *inode;
	sector_t block0, block;
	unsigned shift;
	int ret;

	object = container_of(op->op.object,
			      struct cachefiles_object, fscache);
	cache = container_of(object->fscache.cache,
			     struct cachefiles_cache, cache);

	_enter("{%p},{%lx},,,", object, page->index);

	if (!object->backer)
		return -ENOBUFS;

	inode = object->backer->d_inode;
	ASSERT(S_ISREG(inode->i_mode));
	ASSERT(inode->i_mapping->a_ops->bmap);
	ASSERT(inode->i_mapping->a_ops->readpages);

	
	if (inode->i_sb->s_blocksize > PAGE_SIZE)
		return -ENOBUFS;

	shift = PAGE_SHIFT - inode->i_sb->s_blocksize_bits;

	op->op.flags &= FSCACHE_OP_KEEP_FLAGS;
	op->op.flags |= FSCACHE_OP_ASYNC;
	op->op.processor = cachefiles_read_copier;

	pagevec_init(&pagevec, 0);

	block0 = page->index;
	block0 <<= shift;

	block = inode->i_mapping->a_ops->bmap(inode->i_mapping, block0);
	_debug("%llx -> %llx",
	       (unsigned long long) block0,
	       (unsigned long long) block);

	if (block) {
		ret = cachefiles_read_backing_file_one(object, op, page,
						       &pagevec);
	} else if (cachefiles_has_space(cache, 0, 1) == 0) {
		
		pagevec_add(&pagevec, page);
		fscache_mark_pages_cached(op, &pagevec);
		ret = -ENODATA;
	} else {
		ret = -ENOBUFS;
	}

	_leave(" = %d", ret);
	return ret;
}

static int cachefiles_read_backing_file(struct cachefiles_object *object,
					struct fscache_retrieval *op,
					struct list_head *list,
					struct pagevec *mark_pvec)
{
	struct cachefiles_one_read *monitor = NULL;
	struct address_space *bmapping = object->backer->d_inode->i_mapping;
	struct pagevec lru_pvec;
	struct page *newpage = NULL, *netpage, *_n, *backpage = NULL;
	int ret = 0;

	_enter("");

	pagevec_init(&lru_pvec, 0);

	list_for_each_entry_safe(netpage, _n, list, lru) {
		list_del(&netpage->lru);

		_debug("read back %p{%lu,%d}",
		       netpage, netpage->index, page_count(netpage));

		if (!monitor) {
			monitor = kzalloc(sizeof(*monitor), GFP_KERNEL);
			if (!monitor)
				goto nomem;

			monitor->op = fscache_get_retrieval(op);
			init_waitqueue_func_entry(&monitor->monitor,
						  cachefiles_read_waiter);
		}

		for (;;) {
			backpage = find_get_page(bmapping, netpage->index);
			if (backpage)
				goto backing_page_already_present;

			if (!newpage) {
				newpage = page_cache_alloc_cold(bmapping);
				if (!newpage)
					goto nomem;
			}

			ret = add_to_page_cache(newpage, bmapping,
						netpage->index, GFP_KERNEL);
			if (ret == 0)
				goto installed_new_backing_page;
			if (ret != -EEXIST)
				goto nomem;
		}

	installed_new_backing_page:
		_debug("- new %p", newpage);

		backpage = newpage;
		newpage = NULL;

		page_cache_get(backpage);
		if (!pagevec_add(&lru_pvec, backpage))
			__pagevec_lru_add_file(&lru_pvec);

	reread_backing_page:
		ret = bmapping->a_ops->readpage(NULL, backpage);
		if (ret < 0)
			goto read_error;

	monitor_backing_page:
		_debug("- monitor add");

		ret = add_to_page_cache(netpage, op->mapping, netpage->index,
					GFP_KERNEL);
		if (ret < 0) {
			if (ret == -EEXIST) {
				page_cache_release(netpage);
				continue;
			}
			goto nomem;
		}

		page_cache_get(netpage);
		if (!pagevec_add(&lru_pvec, netpage))
			__pagevec_lru_add_file(&lru_pvec);

		
		page_cache_get(netpage);
		monitor->netfs_page = netpage;

		page_cache_get(backpage);
		monitor->back_page = backpage;
		monitor->monitor.private = backpage;
		add_page_wait_queue(backpage, &monitor->monitor);
		monitor = NULL;

		if (trylock_page(backpage)) {
			_debug("2unlock %p {%lx}", backpage, backpage->flags);
			unlock_page(backpage);
		}

		page_cache_release(backpage);
		backpage = NULL;

		page_cache_release(netpage);
		netpage = NULL;
		continue;

	backing_page_already_present:
		_debug("- present %p", backpage);

		if (PageError(backpage))
			goto io_error;

		if (PageUptodate(backpage))
			goto backing_page_already_uptodate;

		_debug("- not ready %p{%lx}", backpage, backpage->flags);

		if (!trylock_page(backpage))
			goto monitor_backing_page;

		if (PageError(backpage)) {
			_debug("error %lx", backpage->flags);
			unlock_page(backpage);
			goto io_error;
		}

		if (PageUptodate(backpage))
			goto backing_page_already_uptodate_unlock;

		goto reread_backing_page;

	backing_page_already_uptodate_unlock:
		_debug("uptodate %lx", backpage->flags);
		unlock_page(backpage);
	backing_page_already_uptodate:
		_debug("- uptodate");

		ret = add_to_page_cache(netpage, op->mapping, netpage->index,
					GFP_KERNEL);
		if (ret < 0) {
			if (ret == -EEXIST) {
				page_cache_release(netpage);
				continue;
			}
			goto nomem;
		}

		copy_highpage(netpage, backpage);

		page_cache_release(backpage);
		backpage = NULL;

		if (!pagevec_add(mark_pvec, netpage))
			fscache_mark_pages_cached(op, mark_pvec);

		page_cache_get(netpage);
		if (!pagevec_add(&lru_pvec, netpage))
			__pagevec_lru_add_file(&lru_pvec);

		fscache_end_io(op, netpage, 0);
		page_cache_release(netpage);
		netpage = NULL;
		continue;
	}

	netpage = NULL;

	_debug("out");

out:
	
	pagevec_lru_add_file(&lru_pvec);

	if (newpage)
		page_cache_release(newpage);
	if (netpage)
		page_cache_release(netpage);
	if (backpage)
		page_cache_release(backpage);
	if (monitor) {
		fscache_put_retrieval(op);
		kfree(monitor);
	}

	list_for_each_entry_safe(netpage, _n, list, lru) {
		list_del(&netpage->lru);
		page_cache_release(netpage);
	}

	_leave(" = %d", ret);
	return ret;

nomem:
	_debug("nomem");
	ret = -ENOMEM;
	goto out;

read_error:
	_debug("read error %d", ret);
	if (ret == -ENOMEM)
		goto out;
io_error:
	cachefiles_io_error_obj(object, "Page read error on backing file");
	ret = -ENOBUFS;
	goto out;
}

int cachefiles_read_or_alloc_pages(struct fscache_retrieval *op,
				   struct list_head *pages,
				   unsigned *nr_pages,
				   gfp_t gfp)
{
	struct cachefiles_object *object;
	struct cachefiles_cache *cache;
	struct list_head backpages;
	struct pagevec pagevec;
	struct inode *inode;
	struct page *page, *_n;
	unsigned shift, nrbackpages;
	int ret, ret2, space;

	object = container_of(op->op.object,
			      struct cachefiles_object, fscache);
	cache = container_of(object->fscache.cache,
			     struct cachefiles_cache, cache);

	_enter("{OBJ%x,%d},,%d,,",
	       object->fscache.debug_id, atomic_read(&op->op.usage),
	       *nr_pages);

	if (!object->backer)
		return -ENOBUFS;

	space = 1;
	if (cachefiles_has_space(cache, 0, *nr_pages) < 0)
		space = 0;

	inode = object->backer->d_inode;
	ASSERT(S_ISREG(inode->i_mode));
	ASSERT(inode->i_mapping->a_ops->bmap);
	ASSERT(inode->i_mapping->a_ops->readpages);

	
	if (inode->i_sb->s_blocksize > PAGE_SIZE)
		return -ENOBUFS;

	shift = PAGE_SHIFT - inode->i_sb->s_blocksize_bits;

	pagevec_init(&pagevec, 0);

	op->op.flags &= FSCACHE_OP_KEEP_FLAGS;
	op->op.flags |= FSCACHE_OP_ASYNC;
	op->op.processor = cachefiles_read_copier;

	INIT_LIST_HEAD(&backpages);
	nrbackpages = 0;

	ret = space ? -ENODATA : -ENOBUFS;
	list_for_each_entry_safe(page, _n, pages, lru) {
		sector_t block0, block;

		block0 = page->index;
		block0 <<= shift;

		block = inode->i_mapping->a_ops->bmap(inode->i_mapping,
						      block0);
		_debug("%llx -> %llx",
		       (unsigned long long) block0,
		       (unsigned long long) block);

		if (block) {
			list_move(&page->lru, &backpages);
			(*nr_pages)--;
			nrbackpages++;
		} else if (space && pagevec_add(&pagevec, page) == 0) {
			fscache_mark_pages_cached(op, &pagevec);
			ret = -ENODATA;
		}
	}

	if (pagevec_count(&pagevec) > 0)
		fscache_mark_pages_cached(op, &pagevec);

	if (list_empty(pages))
		ret = 0;

	if (nrbackpages > 0) {
		ret2 = cachefiles_read_backing_file(object, op, &backpages,
						    &pagevec);
		if (ret2 == -ENOMEM || ret2 == -EINTR)
			ret = ret2;
	}

	if (pagevec_count(&pagevec) > 0)
		fscache_mark_pages_cached(op, &pagevec);

	_leave(" = %d [nr=%u%s]",
	       ret, *nr_pages, list_empty(pages) ? " empty" : "");
	return ret;
}

int cachefiles_allocate_page(struct fscache_retrieval *op,
			     struct page *page,
			     gfp_t gfp)
{
	struct cachefiles_object *object;
	struct cachefiles_cache *cache;
	struct pagevec pagevec;
	int ret;

	object = container_of(op->op.object,
			      struct cachefiles_object, fscache);
	cache = container_of(object->fscache.cache,
			     struct cachefiles_cache, cache);

	_enter("%p,{%lx},", object, page->index);

	ret = cachefiles_has_space(cache, 0, 1);
	if (ret == 0) {
		pagevec_init(&pagevec, 0);
		pagevec_add(&pagevec, page);
		fscache_mark_pages_cached(op, &pagevec);
	} else {
		ret = -ENOBUFS;
	}

	_leave(" = %d", ret);
	return ret;
}

int cachefiles_allocate_pages(struct fscache_retrieval *op,
			      struct list_head *pages,
			      unsigned *nr_pages,
			      gfp_t gfp)
{
	struct cachefiles_object *object;
	struct cachefiles_cache *cache;
	struct pagevec pagevec;
	struct page *page;
	int ret;

	object = container_of(op->op.object,
			      struct cachefiles_object, fscache);
	cache = container_of(object->fscache.cache,
			     struct cachefiles_cache, cache);

	_enter("%p,,,%d,", object, *nr_pages);

	ret = cachefiles_has_space(cache, 0, *nr_pages);
	if (ret == 0) {
		pagevec_init(&pagevec, 0);

		list_for_each_entry(page, pages, lru) {
			if (pagevec_add(&pagevec, page) == 0)
				fscache_mark_pages_cached(op, &pagevec);
		}

		if (pagevec_count(&pagevec) > 0)
			fscache_mark_pages_cached(op, &pagevec);
		ret = -ENODATA;
	} else {
		ret = -ENOBUFS;
	}

	_leave(" = %d", ret);
	return ret;
}

int cachefiles_write_page(struct fscache_storage *op, struct page *page)
{
	struct cachefiles_object *object;
	struct cachefiles_cache *cache;
	mm_segment_t old_fs;
	struct file *file;
	loff_t pos, eof;
	size_t len;
	void *data;
	int ret;

	ASSERT(op != NULL);
	ASSERT(page != NULL);

	object = container_of(op->op.object,
			      struct cachefiles_object, fscache);

	_enter("%p,%p{%lx},,,", object, page, page->index);

	if (!object->backer) {
		_leave(" = -ENOBUFS");
		return -ENOBUFS;
	}

	ASSERT(S_ISREG(object->backer->d_inode->i_mode));

	cache = container_of(object->fscache.cache,
			     struct cachefiles_cache, cache);

	dget(object->backer);
	mntget(cache->mnt);
	file = dentry_open(object->backer, cache->mnt, O_RDWR,
			   cache->cache_cred);
	if (IS_ERR(file)) {
		ret = PTR_ERR(file);
	} else {
		ret = -EIO;
		if (file->f_op->write) {
			pos = (loff_t) page->index << PAGE_SHIFT;

			eof = object->fscache.store_limit_l;
			len = PAGE_SIZE;
			if (eof & ~PAGE_MASK) {
				ASSERTCMP(pos, <, eof);
				if (eof - pos < PAGE_SIZE) {
					_debug("cut short %llx to %llx",
					       pos, eof);
					len = eof - pos;
					ASSERTCMP(pos + len, ==, eof);
				}
			}

			data = kmap(page);
			old_fs = get_fs();
			set_fs(KERNEL_DS);
			ret = file->f_op->write(
				file, (const void __user *) data, len, &pos);
			set_fs(old_fs);
			kunmap(page);
			if (ret != len)
				ret = -EIO;
		}
		fput(file);
	}

	if (ret < 0) {
		if (ret == -EIO)
			cachefiles_io_error_obj(
				object, "Write page to backing file failed");
		ret = -ENOBUFS;
	}

	_leave(" = %d", ret);
	return ret;
}

void cachefiles_uncache_page(struct fscache_object *_object, struct page *page)
{
	struct cachefiles_object *object;
	struct cachefiles_cache *cache;

	object = container_of(_object, struct cachefiles_object, fscache);
	cache = container_of(object->fscache.cache,
			     struct cachefiles_cache, cache);

	_enter("%p,{%lu}", object, page->index);

	spin_unlock(&object->fscache.cookie->lock);
}
