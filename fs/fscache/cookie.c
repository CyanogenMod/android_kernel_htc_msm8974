/* netfs cookie management
 *
 * Copyright (C) 2004-2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * See Documentation/filesystems/caching/netfs-api.txt for more information on
 * the netfs API.
 */

#define FSCACHE_DEBUG_LEVEL COOKIE
#include <linux/module.h>
#include <linux/slab.h>
#include "internal.h"

struct kmem_cache *fscache_cookie_jar;

static atomic_t fscache_object_debug_id = ATOMIC_INIT(0);

static int fscache_acquire_non_index_cookie(struct fscache_cookie *cookie);
static int fscache_alloc_object(struct fscache_cache *cache,
				struct fscache_cookie *cookie);
static int fscache_attach_object(struct fscache_cookie *cookie,
				 struct fscache_object *object);

void fscache_cookie_init_once(void *_cookie)
{
	struct fscache_cookie *cookie = _cookie;

	memset(cookie, 0, sizeof(*cookie));
	spin_lock_init(&cookie->lock);
	spin_lock_init(&cookie->stores_lock);
	INIT_HLIST_HEAD(&cookie->backing_objects);
}

struct fscache_cookie *__fscache_acquire_cookie(
	struct fscache_cookie *parent,
	const struct fscache_cookie_def *def,
	void *netfs_data)
{
	struct fscache_cookie *cookie;

	BUG_ON(!def);

	_enter("{%s},{%s},%p",
	       parent ? (char *) parent->def->name : "<no-parent>",
	       def->name, netfs_data);

	fscache_stat(&fscache_n_acquires);

	
	if (!parent) {
		fscache_stat(&fscache_n_acquires_null);
		_leave(" [no parent]");
		return NULL;
	}

	
	BUG_ON(!def->get_key);
	BUG_ON(!def->name[0]);

	BUG_ON(def->type == FSCACHE_COOKIE_TYPE_INDEX &&
	       parent->def->type != FSCACHE_COOKIE_TYPE_INDEX);

	
	cookie = kmem_cache_alloc(fscache_cookie_jar, GFP_KERNEL);
	if (!cookie) {
		fscache_stat(&fscache_n_acquires_oom);
		_leave(" [ENOMEM]");
		return NULL;
	}

	atomic_set(&cookie->usage, 1);
	atomic_set(&cookie->n_children, 0);

	atomic_inc(&parent->usage);
	atomic_inc(&parent->n_children);

	cookie->def		= def;
	cookie->parent		= parent;
	cookie->netfs_data	= netfs_data;
	cookie->flags		= 0;

	INIT_RADIX_TREE(&cookie->stores, GFP_NOFS & ~__GFP_WAIT);

	switch (cookie->def->type) {
	case FSCACHE_COOKIE_TYPE_INDEX:
		fscache_stat(&fscache_n_cookie_index);
		break;
	case FSCACHE_COOKIE_TYPE_DATAFILE:
		fscache_stat(&fscache_n_cookie_data);
		break;
	default:
		fscache_stat(&fscache_n_cookie_special);
		break;
	}

	if (cookie->def->type != FSCACHE_COOKIE_TYPE_INDEX) {
		if (fscache_acquire_non_index_cookie(cookie) < 0) {
			atomic_dec(&parent->n_children);
			__fscache_cookie_put(cookie);
			fscache_stat(&fscache_n_acquires_nobufs);
			_leave(" = NULL");
			return NULL;
		}
	}

	fscache_stat(&fscache_n_acquires_ok);
	_leave(" = %p", cookie);
	return cookie;
}
EXPORT_SYMBOL(__fscache_acquire_cookie);

static int fscache_acquire_non_index_cookie(struct fscache_cookie *cookie)
{
	struct fscache_object *object;
	struct fscache_cache *cache;
	uint64_t i_size;
	int ret;

	_enter("");

	cookie->flags = 1 << FSCACHE_COOKIE_UNAVAILABLE;

	down_read(&fscache_addremove_sem);

	if (list_empty(&fscache_cache_list)) {
		up_read(&fscache_addremove_sem);
		_leave(" = 0 [no caches]");
		return 0;
	}

	
	cache = fscache_select_cache_for_object(cookie->parent);
	if (!cache) {
		up_read(&fscache_addremove_sem);
		fscache_stat(&fscache_n_acquires_no_cache);
		_leave(" = -ENOMEDIUM [no cache]");
		return -ENOMEDIUM;
	}

	_debug("cache %s", cache->tag->name);

	cookie->flags =
		(1 << FSCACHE_COOKIE_LOOKING_UP) |
		(1 << FSCACHE_COOKIE_CREATING) |
		(1 << FSCACHE_COOKIE_NO_DATA_YET);

	ret = fscache_alloc_object(cache, cookie);
	if (ret < 0) {
		up_read(&fscache_addremove_sem);
		_leave(" = %d", ret);
		return ret;
	}

	
	cookie->def->get_attr(cookie->netfs_data, &i_size);

	spin_lock(&cookie->lock);
	if (hlist_empty(&cookie->backing_objects)) {
		spin_unlock(&cookie->lock);
		goto unavailable;
	}

	object = hlist_entry(cookie->backing_objects.first,
			     struct fscache_object, cookie_link);

	fscache_set_store_limit(object, i_size);

	fscache_enqueue_object(object);

	spin_unlock(&cookie->lock);

	
	if (!fscache_defer_lookup) {
		_debug("non-deferred lookup %p", &cookie->flags);
		wait_on_bit(&cookie->flags, FSCACHE_COOKIE_LOOKING_UP,
			    fscache_wait_bit, TASK_UNINTERRUPTIBLE);
		_debug("complete");
		if (test_bit(FSCACHE_COOKIE_UNAVAILABLE, &cookie->flags))
			goto unavailable;
	}

	up_read(&fscache_addremove_sem);
	_leave(" = 0 [deferred]");
	return 0;

unavailable:
	up_read(&fscache_addremove_sem);
	_leave(" = -ENOBUFS");
	return -ENOBUFS;
}

static int fscache_alloc_object(struct fscache_cache *cache,
				struct fscache_cookie *cookie)
{
	struct fscache_object *object;
	struct hlist_node *_n;
	int ret;

	_enter("%p,%p{%s}", cache, cookie, cookie->def->name);

	spin_lock(&cookie->lock);
	hlist_for_each_entry(object, _n, &cookie->backing_objects,
			     cookie_link) {
		if (object->cache == cache)
			goto object_already_extant;
	}
	spin_unlock(&cookie->lock);

	fscache_stat(&fscache_n_cop_alloc_object);
	object = cache->ops->alloc_object(cache, cookie);
	fscache_stat_d(&fscache_n_cop_alloc_object);
	if (IS_ERR(object)) {
		fscache_stat(&fscache_n_object_no_alloc);
		ret = PTR_ERR(object);
		goto error;
	}

	fscache_stat(&fscache_n_object_alloc);

	object->debug_id = atomic_inc_return(&fscache_object_debug_id);

	_debug("ALLOC OBJ%x: %s {%lx}",
	       object->debug_id, cookie->def->name, object->events);

	ret = fscache_alloc_object(cache, cookie->parent);
	if (ret < 0)
		goto error_put;

	if (fscache_attach_object(cookie, object) < 0) {
		fscache_stat(&fscache_n_cop_put_object);
		cache->ops->put_object(object);
		fscache_stat_d(&fscache_n_cop_put_object);
	}

	_leave(" = 0");
	return 0;

object_already_extant:
	ret = -ENOBUFS;
	if (object->state >= FSCACHE_OBJECT_DYING) {
		spin_unlock(&cookie->lock);
		goto error;
	}
	spin_unlock(&cookie->lock);
	_leave(" = 0 [found]");
	return 0;

error_put:
	fscache_stat(&fscache_n_cop_put_object);
	cache->ops->put_object(object);
	fscache_stat_d(&fscache_n_cop_put_object);
error:
	_leave(" = %d", ret);
	return ret;
}

static int fscache_attach_object(struct fscache_cookie *cookie,
				 struct fscache_object *object)
{
	struct fscache_object *p;
	struct fscache_cache *cache = object->cache;
	struct hlist_node *_n;
	int ret;

	_enter("{%s},{OBJ%x}", cookie->def->name, object->debug_id);

	spin_lock(&cookie->lock);

	ret = -EEXIST;
	hlist_for_each_entry(p, _n, &cookie->backing_objects, cookie_link) {
		if (p->cache == object->cache) {
			if (p->state >= FSCACHE_OBJECT_DYING)
				ret = -ENOBUFS;
			goto cant_attach_object;
		}
	}

	
	spin_lock_nested(&cookie->parent->lock, 1);
	hlist_for_each_entry(p, _n, &cookie->parent->backing_objects,
			     cookie_link) {
		if (p->cache == object->cache) {
			if (p->state >= FSCACHE_OBJECT_DYING) {
				ret = -ENOBUFS;
				spin_unlock(&cookie->parent->lock);
				goto cant_attach_object;
			}
			object->parent = p;
			spin_lock(&p->lock);
			p->n_children++;
			spin_unlock(&p->lock);
			break;
		}
	}
	spin_unlock(&cookie->parent->lock);

	
	if (list_empty(&object->cache_link)) {
		spin_lock(&cache->object_list_lock);
		list_add(&object->cache_link, &cache->object_list);
		spin_unlock(&cache->object_list_lock);
	}

	
	object->cookie = cookie;
	atomic_inc(&cookie->usage);
	hlist_add_head(&object->cookie_link, &cookie->backing_objects);

	fscache_objlist_add(object);
	ret = 0;

cant_attach_object:
	spin_unlock(&cookie->lock);
	_leave(" = %d", ret);
	return ret;
}

void __fscache_update_cookie(struct fscache_cookie *cookie)
{
	struct fscache_object *object;
	struct hlist_node *_p;

	fscache_stat(&fscache_n_updates);

	if (!cookie) {
		fscache_stat(&fscache_n_updates_null);
		_leave(" [no cookie]");
		return;
	}

	_enter("{%s}", cookie->def->name);

	BUG_ON(!cookie->def->get_aux);

	spin_lock(&cookie->lock);

	
	hlist_for_each_entry(object, _p,
			     &cookie->backing_objects, cookie_link) {
		fscache_raise_event(object, FSCACHE_OBJECT_EV_UPDATE);
	}

	spin_unlock(&cookie->lock);
	_leave("");
}
EXPORT_SYMBOL(__fscache_update_cookie);

void __fscache_relinquish_cookie(struct fscache_cookie *cookie, int retire)
{
	struct fscache_cache *cache;
	struct fscache_object *object;
	unsigned long event;

	fscache_stat(&fscache_n_relinquishes);
	if (retire)
		fscache_stat(&fscache_n_relinquishes_retire);

	if (!cookie) {
		fscache_stat(&fscache_n_relinquishes_null);
		_leave(" [no cookie]");
		return;
	}

	_enter("%p{%s,%p},%d",
	       cookie, cookie->def->name, cookie->netfs_data, retire);

	if (atomic_read(&cookie->n_children) != 0) {
		printk(KERN_ERR "FS-Cache: Cookie '%s' still has children\n",
		       cookie->def->name);
		BUG();
	}

	
	if (test_bit(FSCACHE_COOKIE_CREATING, &cookie->flags)) {
		fscache_stat(&fscache_n_relinquishes_waitcrt);
		wait_on_bit(&cookie->flags, FSCACHE_COOKIE_CREATING,
			    fscache_wait_bit, TASK_UNINTERRUPTIBLE);
	}

	event = retire ? FSCACHE_OBJECT_EV_RETIRE : FSCACHE_OBJECT_EV_RELEASE;

	spin_lock(&cookie->lock);

	
	while (!hlist_empty(&cookie->backing_objects)) {
		object = hlist_entry(cookie->backing_objects.first,
				     struct fscache_object,
				     cookie_link);

		_debug("RELEASE OBJ%x", object->debug_id);

		
		spin_lock(&object->lock);
		hlist_del_init(&object->cookie_link);

		cache = object->cache;
		object->cookie = NULL;
		fscache_raise_event(object, event);
		spin_unlock(&object->lock);

		if (atomic_dec_and_test(&cookie->usage))
			
			BUG();
	}

	
	cookie->netfs_data	= NULL;
	cookie->def		= NULL;

	spin_unlock(&cookie->lock);

	if (cookie->parent) {
		ASSERTCMP(atomic_read(&cookie->parent->usage), >, 0);
		ASSERTCMP(atomic_read(&cookie->parent->n_children), >, 0);
		atomic_dec(&cookie->parent->n_children);
	}

	
	ASSERTCMP(atomic_read(&cookie->usage), >, 0);
	fscache_cookie_put(cookie);

	_leave("");
}
EXPORT_SYMBOL(__fscache_relinquish_cookie);

void __fscache_cookie_put(struct fscache_cookie *cookie)
{
	struct fscache_cookie *parent;

	_enter("%p", cookie);

	for (;;) {
		_debug("FREE COOKIE %p", cookie);
		parent = cookie->parent;
		BUG_ON(!hlist_empty(&cookie->backing_objects));
		kmem_cache_free(fscache_cookie_jar, cookie);

		if (!parent)
			break;

		cookie = parent;
		BUG_ON(atomic_read(&cookie->usage) <= 0);
		if (!atomic_dec_and_test(&cookie->usage))
			break;
	}

	_leave("");
}
