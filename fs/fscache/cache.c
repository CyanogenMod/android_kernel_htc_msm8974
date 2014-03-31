/* FS-Cache cache handling
 *
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#define FSCACHE_DEBUG_LEVEL CACHE
#include <linux/module.h>
#include <linux/slab.h>
#include "internal.h"

LIST_HEAD(fscache_cache_list);
DECLARE_RWSEM(fscache_addremove_sem);
DECLARE_WAIT_QUEUE_HEAD(fscache_cache_cleared_wq);
EXPORT_SYMBOL(fscache_cache_cleared_wq);

static LIST_HEAD(fscache_cache_tag_list);

struct fscache_cache_tag *__fscache_lookup_cache_tag(const char *name)
{
	struct fscache_cache_tag *tag, *xtag;

	
	down_read(&fscache_addremove_sem);

	list_for_each_entry(tag, &fscache_cache_tag_list, link) {
		if (strcmp(tag->name, name) == 0) {
			atomic_inc(&tag->usage);
			up_read(&fscache_addremove_sem);
			return tag;
		}
	}

	up_read(&fscache_addremove_sem);

	
	xtag = kzalloc(sizeof(*xtag) + strlen(name) + 1, GFP_KERNEL);
	if (!xtag)
		
		return ERR_PTR(-ENOMEM);

	atomic_set(&xtag->usage, 1);
	strcpy(xtag->name, name);

	
	down_write(&fscache_addremove_sem);

	list_for_each_entry(tag, &fscache_cache_tag_list, link) {
		if (strcmp(tag->name, name) == 0) {
			atomic_inc(&tag->usage);
			up_write(&fscache_addremove_sem);
			kfree(xtag);
			return tag;
		}
	}

	list_add_tail(&xtag->link, &fscache_cache_tag_list);
	up_write(&fscache_addremove_sem);
	return xtag;
}

void __fscache_release_cache_tag(struct fscache_cache_tag *tag)
{
	if (tag != ERR_PTR(-ENOMEM)) {
		down_write(&fscache_addremove_sem);

		if (atomic_dec_and_test(&tag->usage))
			list_del_init(&tag->link);
		else
			tag = NULL;

		up_write(&fscache_addremove_sem);

		kfree(tag);
	}
}

struct fscache_cache *fscache_select_cache_for_object(
	struct fscache_cookie *cookie)
{
	struct fscache_cache_tag *tag;
	struct fscache_object *object;
	struct fscache_cache *cache;

	_enter("");

	if (list_empty(&fscache_cache_list)) {
		_leave(" = NULL [no cache]");
		return NULL;
	}

	
	spin_lock(&cookie->lock);

	if (!hlist_empty(&cookie->backing_objects)) {
		object = hlist_entry(cookie->backing_objects.first,
				     struct fscache_object, cookie_link);

		cache = object->cache;
		if (object->state >= FSCACHE_OBJECT_DYING ||
		    test_bit(FSCACHE_IOERROR, &cache->flags))
			cache = NULL;

		spin_unlock(&cookie->lock);
		_leave(" = %p [parent]", cache);
		return cache;
	}

	
	if (cookie->def->type != FSCACHE_COOKIE_TYPE_INDEX) {
		
		spin_unlock(&cookie->lock);
		_leave(" = NULL [cookie ub,ni]");
		return NULL;
	}

	spin_unlock(&cookie->lock);

	if (!cookie->def->select_cache)
		goto no_preference;

	
	tag = cookie->def->select_cache(cookie->parent->netfs_data,
					cookie->netfs_data);
	if (!tag)
		goto no_preference;

	if (tag == ERR_PTR(-ENOMEM)) {
		_leave(" = NULL [nomem tag]");
		return NULL;
	}

	if (!tag->cache) {
		_leave(" = NULL [unbacked tag]");
		return NULL;
	}

	if (test_bit(FSCACHE_IOERROR, &tag->cache->flags))
		return NULL;

	_leave(" = %p [specific]", tag->cache);
	return tag->cache;

no_preference:
	
	cache = list_entry(fscache_cache_list.next,
			   struct fscache_cache, link);
	_leave(" = %p [first]", cache);
	return cache;
}

void fscache_init_cache(struct fscache_cache *cache,
			const struct fscache_cache_ops *ops,
			const char *idfmt,
			...)
{
	va_list va;

	memset(cache, 0, sizeof(*cache));

	cache->ops = ops;

	va_start(va, idfmt);
	vsnprintf(cache->identifier, sizeof(cache->identifier), idfmt, va);
	va_end(va);

	INIT_WORK(&cache->op_gc, fscache_operation_gc);
	INIT_LIST_HEAD(&cache->link);
	INIT_LIST_HEAD(&cache->object_list);
	INIT_LIST_HEAD(&cache->op_gc_list);
	spin_lock_init(&cache->object_list_lock);
	spin_lock_init(&cache->op_gc_list_lock);
}
EXPORT_SYMBOL(fscache_init_cache);

int fscache_add_cache(struct fscache_cache *cache,
		      struct fscache_object *ifsdef,
		      const char *tagname)
{
	struct fscache_cache_tag *tag;

	BUG_ON(!cache->ops);
	BUG_ON(!ifsdef);

	cache->flags = 0;
	ifsdef->event_mask = ULONG_MAX & ~(1 << FSCACHE_OBJECT_EV_CLEARED);
	ifsdef->state = FSCACHE_OBJECT_ACTIVE;

	if (!tagname)
		tagname = cache->identifier;

	BUG_ON(!tagname[0]);

	_enter("{%s.%s},,%s", cache->ops->name, cache->identifier, tagname);

	
	tag = __fscache_lookup_cache_tag(tagname);
	if (IS_ERR(tag))
		goto nomem;

	if (test_and_set_bit(FSCACHE_TAG_RESERVED, &tag->flags))
		goto tag_in_use;

	cache->kobj = kobject_create_and_add(tagname, fscache_root);
	if (!cache->kobj)
		goto error;

	ifsdef->cookie = &fscache_fsdef_index;
	ifsdef->cache = cache;
	cache->fsdef = ifsdef;

	down_write(&fscache_addremove_sem);

	tag->cache = cache;
	cache->tag = tag;

	
	list_add(&cache->link, &fscache_cache_list);

	spin_lock(&cache->object_list_lock);
	list_add_tail(&ifsdef->cache_link, &cache->object_list);
	spin_unlock(&cache->object_list_lock);
	fscache_objlist_add(ifsdef);

	spin_lock(&fscache_fsdef_index.lock);

	hlist_add_head(&ifsdef->cookie_link,
		       &fscache_fsdef_index.backing_objects);

	atomic_inc(&fscache_fsdef_index.usage);

	
	spin_unlock(&fscache_fsdef_index.lock);
	up_write(&fscache_addremove_sem);

	printk(KERN_NOTICE "FS-Cache: Cache \"%s\" added (type %s)\n",
	       cache->tag->name, cache->ops->name);
	kobject_uevent(cache->kobj, KOBJ_ADD);

	_leave(" = 0 [%s]", cache->identifier);
	return 0;

tag_in_use:
	printk(KERN_ERR "FS-Cache: Cache tag '%s' already in use\n", tagname);
	__fscache_release_cache_tag(tag);
	_leave(" = -EXIST");
	return -EEXIST;

error:
	__fscache_release_cache_tag(tag);
	_leave(" = -EINVAL");
	return -EINVAL;

nomem:
	_leave(" = -ENOMEM");
	return -ENOMEM;
}
EXPORT_SYMBOL(fscache_add_cache);

void fscache_io_error(struct fscache_cache *cache)
{
	set_bit(FSCACHE_IOERROR, &cache->flags);

	printk(KERN_ERR "FS-Cache: Cache %s stopped due to I/O error\n",
	       cache->ops->name);
}
EXPORT_SYMBOL(fscache_io_error);

static void fscache_withdraw_all_objects(struct fscache_cache *cache,
					 struct list_head *dying_objects)
{
	struct fscache_object *object;

	spin_lock(&cache->object_list_lock);

	while (!list_empty(&cache->object_list)) {
		object = list_entry(cache->object_list.next,
				    struct fscache_object, cache_link);
		list_move_tail(&object->cache_link, dying_objects);

		_debug("withdraw %p", object->cookie);

		spin_lock(&object->lock);
		spin_unlock(&cache->object_list_lock);
		fscache_raise_event(object, FSCACHE_OBJECT_EV_WITHDRAW);
		spin_unlock(&object->lock);

		cond_resched();
		spin_lock(&cache->object_list_lock);
	}

	spin_unlock(&cache->object_list_lock);
}

void fscache_withdraw_cache(struct fscache_cache *cache)
{
	LIST_HEAD(dying_objects);

	_enter("");

	printk(KERN_NOTICE "FS-Cache: Withdrawing cache \"%s\"\n",
	       cache->tag->name);

	
	if (test_and_set_bit(FSCACHE_CACHE_WITHDRAWN, &cache->flags))
		BUG();

	down_write(&fscache_addremove_sem);
	list_del_init(&cache->link);
	cache->tag->cache = NULL;
	up_write(&fscache_addremove_sem);

	/* make sure all pages pinned by operations on behalf of the netfs are
	 * written to disk */
	fscache_stat(&fscache_n_cop_sync_cache);
	cache->ops->sync_cache(cache);
	fscache_stat_d(&fscache_n_cop_sync_cache);

	fscache_stat(&fscache_n_cop_dissociate_pages);
	cache->ops->dissociate_pages(cache);
	fscache_stat_d(&fscache_n_cop_dissociate_pages);

	_debug("destroy");

	fscache_withdraw_all_objects(cache, &dying_objects);

	_debug("wait for finish");
	wait_event(fscache_cache_cleared_wq,
		   atomic_read(&cache->object_count) == 0);
	_debug("wait for clearance");
	wait_event(fscache_cache_cleared_wq,
		   list_empty(&cache->object_list));
	_debug("cleared");
	ASSERT(list_empty(&dying_objects));

	kobject_put(cache->kobj);

	clear_bit(FSCACHE_TAG_RESERVED, &cache->tag->flags);
	fscache_release_cache_tag(cache->tag);
	cache->tag = NULL;

	_leave("");
}
EXPORT_SYMBOL(fscache_withdraw_cache);
