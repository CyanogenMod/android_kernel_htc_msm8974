/*
 * Copyright (c) 2006-2007 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write the Free Software Foundation,
 * Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include "xfs.h"
#include "xfs_mru_cache.h"


typedef struct xfs_mru_cache_elem
{
	struct list_head list_node;
	unsigned long	key;
	void		*value;
} xfs_mru_cache_elem_t;

static kmem_zone_t		*xfs_mru_elem_zone;
static struct workqueue_struct	*xfs_mru_reap_wq;

STATIC unsigned long
_xfs_mru_cache_migrate(
	xfs_mru_cache_t	*mru,
	unsigned long	now)
{
	unsigned int	grp;
	unsigned int	migrated = 0;
	struct list_head *lru_list;

	
	if (!mru->time_zero)
		return 0;

	
	while (mru->time_zero <= now - mru->grp_count * mru->grp_time) {

		lru_list = mru->lists + mru->lru_grp;
		if (!list_empty(lru_list))
			list_splice_init(lru_list, mru->reap_list.prev);

		mru->lru_grp = (mru->lru_grp + 1) % mru->grp_count;
		mru->time_zero += mru->grp_time;

		if (++migrated == mru->grp_count) {
			mru->lru_grp = 0;
			mru->time_zero = 0;
			return 0;
		}
	}

	
	for (grp = 0; grp < mru->grp_count; grp++) {

		
		lru_list = mru->lists + ((mru->lru_grp + grp) % mru->grp_count);
		if (!list_empty(lru_list))
			return mru->time_zero +
			       (mru->grp_count + grp) * mru->grp_time;
	}

	
	mru->lru_grp = 0;
	mru->time_zero = 0;
	return 0;
}

STATIC void
_xfs_mru_cache_list_insert(
	xfs_mru_cache_t		*mru,
	xfs_mru_cache_elem_t	*elem)
{
	unsigned int	grp = 0;
	unsigned long	now = jiffies;

	if (!_xfs_mru_cache_migrate(mru, now)) {
		mru->time_zero = now;
		if (!mru->queued) {
			mru->queued = 1;
			queue_delayed_work(xfs_mru_reap_wq, &mru->work,
			                   mru->grp_count * mru->grp_time);
		}
	} else {
		grp = (now - mru->time_zero) / mru->grp_time;
		grp = (mru->lru_grp + grp) % mru->grp_count;
	}

	
	list_add_tail(&elem->list_node, mru->lists + grp);
}

STATIC void
_xfs_mru_cache_clear_reap_list(
	xfs_mru_cache_t		*mru) __releases(mru->lock) __acquires(mru->lock)

{
	xfs_mru_cache_elem_t	*elem, *next;
	struct list_head	tmp;

	INIT_LIST_HEAD(&tmp);
	list_for_each_entry_safe(elem, next, &mru->reap_list, list_node) {

		
		radix_tree_delete(&mru->store, elem->key);

		list_move(&elem->list_node, &tmp);
	}
	spin_unlock(&mru->lock);

	list_for_each_entry_safe(elem, next, &tmp, list_node) {

		
		list_del_init(&elem->list_node);

		
		mru->free_func(elem->key, elem->value);

		
		kmem_zone_free(xfs_mru_elem_zone, elem);
	}

	spin_lock(&mru->lock);
}

STATIC void
_xfs_mru_cache_reap(
	struct work_struct	*work)
{
	xfs_mru_cache_t		*mru = container_of(work, xfs_mru_cache_t, work.work);
	unsigned long		now, next;

	ASSERT(mru && mru->lists);
	if (!mru || !mru->lists)
		return;

	spin_lock(&mru->lock);
	next = _xfs_mru_cache_migrate(mru, jiffies);
	_xfs_mru_cache_clear_reap_list(mru);

	mru->queued = next;
	if ((mru->queued > 0)) {
		now = jiffies;
		if (next <= now)
			next = 0;
		else
			next -= now;
		queue_delayed_work(xfs_mru_reap_wq, &mru->work, next);
	}

	spin_unlock(&mru->lock);
}

int
xfs_mru_cache_init(void)
{
	xfs_mru_elem_zone = kmem_zone_init(sizeof(xfs_mru_cache_elem_t),
	                                 "xfs_mru_cache_elem");
	if (!xfs_mru_elem_zone)
		goto out;

	xfs_mru_reap_wq = alloc_workqueue("xfs_mru_cache", WQ_MEM_RECLAIM, 1);
	if (!xfs_mru_reap_wq)
		goto out_destroy_mru_elem_zone;

	return 0;

 out_destroy_mru_elem_zone:
	kmem_zone_destroy(xfs_mru_elem_zone);
 out:
	return -ENOMEM;
}

void
xfs_mru_cache_uninit(void)
{
	destroy_workqueue(xfs_mru_reap_wq);
	kmem_zone_destroy(xfs_mru_elem_zone);
}

int
xfs_mru_cache_create(
	xfs_mru_cache_t		**mrup,
	unsigned int		lifetime_ms,
	unsigned int		grp_count,
	xfs_mru_cache_free_func_t free_func)
{
	xfs_mru_cache_t	*mru = NULL;
	int		err = 0, grp;
	unsigned int	grp_time;

	if (mrup)
		*mrup = NULL;

	if (!mrup || !grp_count || !lifetime_ms || !free_func)
		return EINVAL;

	if (!(grp_time = msecs_to_jiffies(lifetime_ms) / grp_count))
		return EINVAL;

	if (!(mru = kmem_zalloc(sizeof(*mru), KM_SLEEP)))
		return ENOMEM;

	
	mru->grp_count = grp_count + 1;
	mru->lists = kmem_zalloc(mru->grp_count * sizeof(*mru->lists), KM_SLEEP);

	if (!mru->lists) {
		err = ENOMEM;
		goto exit;
	}

	for (grp = 0; grp < mru->grp_count; grp++)
		INIT_LIST_HEAD(mru->lists + grp);

	INIT_RADIX_TREE(&mru->store, GFP_ATOMIC);
	INIT_LIST_HEAD(&mru->reap_list);
	spin_lock_init(&mru->lock);
	INIT_DELAYED_WORK(&mru->work, _xfs_mru_cache_reap);

	mru->grp_time  = grp_time;
	mru->free_func = free_func;

	*mrup = mru;

exit:
	if (err && mru && mru->lists)
		kmem_free(mru->lists);
	if (err && mru)
		kmem_free(mru);

	return err;
}

static void
xfs_mru_cache_flush(
	xfs_mru_cache_t		*mru)
{
	if (!mru || !mru->lists)
		return;

	spin_lock(&mru->lock);
	if (mru->queued) {
		spin_unlock(&mru->lock);
		cancel_delayed_work_sync(&mru->work);
		spin_lock(&mru->lock);
	}

	_xfs_mru_cache_migrate(mru, jiffies + mru->grp_count * mru->grp_time);
	_xfs_mru_cache_clear_reap_list(mru);

	spin_unlock(&mru->lock);
}

void
xfs_mru_cache_destroy(
	xfs_mru_cache_t		*mru)
{
	if (!mru || !mru->lists)
		return;

	xfs_mru_cache_flush(mru);

	kmem_free(mru->lists);
	kmem_free(mru);
}

int
xfs_mru_cache_insert(
	xfs_mru_cache_t	*mru,
	unsigned long	key,
	void		*value)
{
	xfs_mru_cache_elem_t *elem;

	ASSERT(mru && mru->lists);
	if (!mru || !mru->lists)
		return EINVAL;

	elem = kmem_zone_zalloc(xfs_mru_elem_zone, KM_SLEEP);
	if (!elem)
		return ENOMEM;

	if (radix_tree_preload(GFP_KERNEL)) {
		kmem_zone_free(xfs_mru_elem_zone, elem);
		return ENOMEM;
	}

	INIT_LIST_HEAD(&elem->list_node);
	elem->key = key;
	elem->value = value;

	spin_lock(&mru->lock);

	radix_tree_insert(&mru->store, key, elem);
	radix_tree_preload_end();
	_xfs_mru_cache_list_insert(mru, elem);

	spin_unlock(&mru->lock);

	return 0;
}

void *
xfs_mru_cache_remove(
	xfs_mru_cache_t	*mru,
	unsigned long	key)
{
	xfs_mru_cache_elem_t *elem;
	void		*value = NULL;

	ASSERT(mru && mru->lists);
	if (!mru || !mru->lists)
		return NULL;

	spin_lock(&mru->lock);
	elem = radix_tree_delete(&mru->store, key);
	if (elem) {
		value = elem->value;
		list_del(&elem->list_node);
	}

	spin_unlock(&mru->lock);

	if (elem)
		kmem_zone_free(xfs_mru_elem_zone, elem);

	return value;
}

void
xfs_mru_cache_delete(
	xfs_mru_cache_t	*mru,
	unsigned long	key)
{
	void		*value = xfs_mru_cache_remove(mru, key);

	if (value)
		mru->free_func(key, value);
}

void *
xfs_mru_cache_lookup(
	xfs_mru_cache_t	*mru,
	unsigned long	key)
{
	xfs_mru_cache_elem_t *elem;

	ASSERT(mru && mru->lists);
	if (!mru || !mru->lists)
		return NULL;

	spin_lock(&mru->lock);
	elem = radix_tree_lookup(&mru->store, key);
	if (elem) {
		list_del(&elem->list_node);
		_xfs_mru_cache_list_insert(mru, elem);
		__release(mru_lock); 
	} else
		spin_unlock(&mru->lock);

	return elem ? elem->value : NULL;
}

void
xfs_mru_cache_done(
	xfs_mru_cache_t	*mru) __releases(mru->lock)
{
	spin_unlock(&mru->lock);
}
