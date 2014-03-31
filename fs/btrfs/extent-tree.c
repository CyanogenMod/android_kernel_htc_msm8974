/*
 * Copyright (C) 2007 Oracle.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License v2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 */
#include <linux/sched.h>
#include <linux/pagemap.h>
#include <linux/writeback.h>
#include <linux/blkdev.h>
#include <linux/sort.h>
#include <linux/rcupdate.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/ratelimit.h>
#include "compat.h"
#include "hash.h"
#include "ctree.h"
#include "disk-io.h"
#include "print-tree.h"
#include "transaction.h"
#include "volumes.h"
#include "locking.h"
#include "free-space-cache.h"

enum {
	CHUNK_ALLOC_NO_FORCE = 0,
	CHUNK_ALLOC_LIMITED = 1,
	CHUNK_ALLOC_FORCE = 2,
};

enum {
	RESERVE_FREE = 0,
	RESERVE_ALLOC = 1,
	RESERVE_ALLOC_NO_ACCOUNT = 2,
};

static int update_block_group(struct btrfs_trans_handle *trans,
			      struct btrfs_root *root,
			      u64 bytenr, u64 num_bytes, int alloc);
static int __btrfs_free_extent(struct btrfs_trans_handle *trans,
				struct btrfs_root *root,
				u64 bytenr, u64 num_bytes, u64 parent,
				u64 root_objectid, u64 owner_objectid,
				u64 owner_offset, int refs_to_drop,
				struct btrfs_delayed_extent_op *extra_op);
static void __run_delayed_extent_op(struct btrfs_delayed_extent_op *extent_op,
				    struct extent_buffer *leaf,
				    struct btrfs_extent_item *ei);
static int alloc_reserved_file_extent(struct btrfs_trans_handle *trans,
				      struct btrfs_root *root,
				      u64 parent, u64 root_objectid,
				      u64 flags, u64 owner, u64 offset,
				      struct btrfs_key *ins, int ref_mod);
static int alloc_reserved_tree_block(struct btrfs_trans_handle *trans,
				     struct btrfs_root *root,
				     u64 parent, u64 root_objectid,
				     u64 flags, struct btrfs_disk_key *key,
				     int level, struct btrfs_key *ins);
static int do_chunk_alloc(struct btrfs_trans_handle *trans,
			  struct btrfs_root *extent_root, u64 alloc_bytes,
			  u64 flags, int force);
static int find_next_key(struct btrfs_path *path, int level,
			 struct btrfs_key *key);
static void dump_space_info(struct btrfs_space_info *info, u64 bytes,
			    int dump_block_groups);
static int btrfs_update_reserved_bytes(struct btrfs_block_group_cache *cache,
				       u64 num_bytes, int reserve);

static noinline int
block_group_cache_done(struct btrfs_block_group_cache *cache)
{
	smp_mb();
	return cache->cached == BTRFS_CACHE_FINISHED;
}

static int block_group_bits(struct btrfs_block_group_cache *cache, u64 bits)
{
	return (cache->flags & bits) == bits;
}

static void btrfs_get_block_group(struct btrfs_block_group_cache *cache)
{
	atomic_inc(&cache->count);
}

void btrfs_put_block_group(struct btrfs_block_group_cache *cache)
{
	if (atomic_dec_and_test(&cache->count)) {
		WARN_ON(cache->pinned > 0);
		WARN_ON(cache->reserved > 0);
		kfree(cache->free_space_ctl);
		kfree(cache);
	}
}

static int btrfs_add_block_group_cache(struct btrfs_fs_info *info,
				struct btrfs_block_group_cache *block_group)
{
	struct rb_node **p;
	struct rb_node *parent = NULL;
	struct btrfs_block_group_cache *cache;

	spin_lock(&info->block_group_cache_lock);
	p = &info->block_group_cache_tree.rb_node;

	while (*p) {
		parent = *p;
		cache = rb_entry(parent, struct btrfs_block_group_cache,
				 cache_node);
		if (block_group->key.objectid < cache->key.objectid) {
			p = &(*p)->rb_left;
		} else if (block_group->key.objectid > cache->key.objectid) {
			p = &(*p)->rb_right;
		} else {
			spin_unlock(&info->block_group_cache_lock);
			return -EEXIST;
		}
	}

	rb_link_node(&block_group->cache_node, parent, p);
	rb_insert_color(&block_group->cache_node,
			&info->block_group_cache_tree);
	spin_unlock(&info->block_group_cache_lock);

	return 0;
}

static struct btrfs_block_group_cache *
block_group_cache_tree_search(struct btrfs_fs_info *info, u64 bytenr,
			      int contains)
{
	struct btrfs_block_group_cache *cache, *ret = NULL;
	struct rb_node *n;
	u64 end, start;

	spin_lock(&info->block_group_cache_lock);
	n = info->block_group_cache_tree.rb_node;

	while (n) {
		cache = rb_entry(n, struct btrfs_block_group_cache,
				 cache_node);
		end = cache->key.objectid + cache->key.offset - 1;
		start = cache->key.objectid;

		if (bytenr < start) {
			if (!contains && (!ret || start < ret->key.objectid))
				ret = cache;
			n = n->rb_left;
		} else if (bytenr > start) {
			if (contains && bytenr <= end) {
				ret = cache;
				break;
			}
			n = n->rb_right;
		} else {
			ret = cache;
			break;
		}
	}
	if (ret)
		btrfs_get_block_group(ret);
	spin_unlock(&info->block_group_cache_lock);

	return ret;
}

static int add_excluded_extent(struct btrfs_root *root,
			       u64 start, u64 num_bytes)
{
	u64 end = start + num_bytes - 1;
	set_extent_bits(&root->fs_info->freed_extents[0],
			start, end, EXTENT_UPTODATE, GFP_NOFS);
	set_extent_bits(&root->fs_info->freed_extents[1],
			start, end, EXTENT_UPTODATE, GFP_NOFS);
	return 0;
}

static void free_excluded_extents(struct btrfs_root *root,
				  struct btrfs_block_group_cache *cache)
{
	u64 start, end;

	start = cache->key.objectid;
	end = start + cache->key.offset - 1;

	clear_extent_bits(&root->fs_info->freed_extents[0],
			  start, end, EXTENT_UPTODATE, GFP_NOFS);
	clear_extent_bits(&root->fs_info->freed_extents[1],
			  start, end, EXTENT_UPTODATE, GFP_NOFS);
}

static int exclude_super_stripes(struct btrfs_root *root,
				 struct btrfs_block_group_cache *cache)
{
	u64 bytenr;
	u64 *logical;
	int stripe_len;
	int i, nr, ret;

	if (cache->key.objectid < BTRFS_SUPER_INFO_OFFSET) {
		stripe_len = BTRFS_SUPER_INFO_OFFSET - cache->key.objectid;
		cache->bytes_super += stripe_len;
		ret = add_excluded_extent(root, cache->key.objectid,
					  stripe_len);
		BUG_ON(ret); 
	}

	for (i = 0; i < BTRFS_SUPER_MIRROR_MAX; i++) {
		bytenr = btrfs_sb_offset(i);
		ret = btrfs_rmap_block(&root->fs_info->mapping_tree,
				       cache->key.objectid, bytenr,
				       0, &logical, &nr, &stripe_len);
		BUG_ON(ret); 

		while (nr--) {
			cache->bytes_super += stripe_len;
			ret = add_excluded_extent(root, logical[nr],
						  stripe_len);
			BUG_ON(ret); 
		}

		kfree(logical);
	}
	return 0;
}

static struct btrfs_caching_control *
get_caching_control(struct btrfs_block_group_cache *cache)
{
	struct btrfs_caching_control *ctl;

	spin_lock(&cache->lock);
	if (cache->cached != BTRFS_CACHE_STARTED) {
		spin_unlock(&cache->lock);
		return NULL;
	}

	
	if (!cache->caching_ctl) {
		spin_unlock(&cache->lock);
		return NULL;
	}

	ctl = cache->caching_ctl;
	atomic_inc(&ctl->count);
	spin_unlock(&cache->lock);
	return ctl;
}

static void put_caching_control(struct btrfs_caching_control *ctl)
{
	if (atomic_dec_and_test(&ctl->count))
		kfree(ctl);
}

static u64 add_new_free_space(struct btrfs_block_group_cache *block_group,
			      struct btrfs_fs_info *info, u64 start, u64 end)
{
	u64 extent_start, extent_end, size, total_added = 0;
	int ret;

	while (start < end) {
		ret = find_first_extent_bit(info->pinned_extents, start,
					    &extent_start, &extent_end,
					    EXTENT_DIRTY | EXTENT_UPTODATE);
		if (ret)
			break;

		if (extent_start <= start) {
			start = extent_end + 1;
		} else if (extent_start > start && extent_start < end) {
			size = extent_start - start;
			total_added += size;
			ret = btrfs_add_free_space(block_group, start,
						   size);
			BUG_ON(ret); 
			start = extent_end + 1;
		} else {
			break;
		}
	}

	if (start < end) {
		size = end - start;
		total_added += size;
		ret = btrfs_add_free_space(block_group, start, size);
		BUG_ON(ret); 
	}

	return total_added;
}

static noinline void caching_thread(struct btrfs_work *work)
{
	struct btrfs_block_group_cache *block_group;
	struct btrfs_fs_info *fs_info;
	struct btrfs_caching_control *caching_ctl;
	struct btrfs_root *extent_root;
	struct btrfs_path *path;
	struct extent_buffer *leaf;
	struct btrfs_key key;
	u64 total_found = 0;
	u64 last = 0;
	u32 nritems;
	int ret = 0;

	caching_ctl = container_of(work, struct btrfs_caching_control, work);
	block_group = caching_ctl->block_group;
	fs_info = block_group->fs_info;
	extent_root = fs_info->extent_root;

	path = btrfs_alloc_path();
	if (!path)
		goto out;

	last = max_t(u64, block_group->key.objectid, BTRFS_SUPER_INFO_OFFSET);

	path->skip_locking = 1;
	path->search_commit_root = 1;
	path->reada = 1;

	key.objectid = last;
	key.offset = 0;
	key.type = BTRFS_EXTENT_ITEM_KEY;
again:
	mutex_lock(&caching_ctl->mutex);
	
	down_read(&fs_info->extent_commit_sem);

	ret = btrfs_search_slot(NULL, extent_root, &key, path, 0, 0);
	if (ret < 0)
		goto err;

	leaf = path->nodes[0];
	nritems = btrfs_header_nritems(leaf);

	while (1) {
		if (btrfs_fs_closing(fs_info) > 1) {
			last = (u64)-1;
			break;
		}

		if (path->slots[0] < nritems) {
			btrfs_item_key_to_cpu(leaf, &key, path->slots[0]);
		} else {
			ret = find_next_key(path, 0, &key);
			if (ret)
				break;

			if (need_resched() ||
			    btrfs_next_leaf(extent_root, path)) {
				caching_ctl->progress = last;
				btrfs_release_path(path);
				up_read(&fs_info->extent_commit_sem);
				mutex_unlock(&caching_ctl->mutex);
				cond_resched();
				goto again;
			}
			leaf = path->nodes[0];
			nritems = btrfs_header_nritems(leaf);
			continue;
		}

		if (key.objectid < block_group->key.objectid) {
			path->slots[0]++;
			continue;
		}

		if (key.objectid >= block_group->key.objectid +
		    block_group->key.offset)
			break;

		if (key.type == BTRFS_EXTENT_ITEM_KEY) {
			total_found += add_new_free_space(block_group,
							  fs_info, last,
							  key.objectid);
			last = key.objectid + key.offset;

			if (total_found > (1024 * 1024 * 2)) {
				total_found = 0;
				wake_up(&caching_ctl->wait);
			}
		}
		path->slots[0]++;
	}
	ret = 0;

	total_found += add_new_free_space(block_group, fs_info, last,
					  block_group->key.objectid +
					  block_group->key.offset);
	caching_ctl->progress = (u64)-1;

	spin_lock(&block_group->lock);
	block_group->caching_ctl = NULL;
	block_group->cached = BTRFS_CACHE_FINISHED;
	spin_unlock(&block_group->lock);

err:
	btrfs_free_path(path);
	up_read(&fs_info->extent_commit_sem);

	free_excluded_extents(extent_root, block_group);

	mutex_unlock(&caching_ctl->mutex);
out:
	wake_up(&caching_ctl->wait);

	put_caching_control(caching_ctl);
	btrfs_put_block_group(block_group);
}

static int cache_block_group(struct btrfs_block_group_cache *cache,
			     struct btrfs_trans_handle *trans,
			     struct btrfs_root *root,
			     int load_cache_only)
{
	DEFINE_WAIT(wait);
	struct btrfs_fs_info *fs_info = cache->fs_info;
	struct btrfs_caching_control *caching_ctl;
	int ret = 0;

	caching_ctl = kzalloc(sizeof(*caching_ctl), GFP_NOFS);
	if (!caching_ctl)
		return -ENOMEM;

	INIT_LIST_HEAD(&caching_ctl->list);
	mutex_init(&caching_ctl->mutex);
	init_waitqueue_head(&caching_ctl->wait);
	caching_ctl->block_group = cache;
	caching_ctl->progress = cache->key.objectid;
	atomic_set(&caching_ctl->count, 1);
	caching_ctl->work.func = caching_thread;

	spin_lock(&cache->lock);
	while (cache->cached == BTRFS_CACHE_FAST) {
		struct btrfs_caching_control *ctl;

		ctl = cache->caching_ctl;
		atomic_inc(&ctl->count);
		prepare_to_wait(&ctl->wait, &wait, TASK_UNINTERRUPTIBLE);
		spin_unlock(&cache->lock);

		schedule();

		finish_wait(&ctl->wait, &wait);
		put_caching_control(ctl);
		spin_lock(&cache->lock);
	}

	if (cache->cached != BTRFS_CACHE_NO) {
		spin_unlock(&cache->lock);
		kfree(caching_ctl);
		return 0;
	}
	WARN_ON(cache->caching_ctl);
	cache->caching_ctl = caching_ctl;
	cache->cached = BTRFS_CACHE_FAST;
	spin_unlock(&cache->lock);

	if (fs_info->mount_opt & BTRFS_MOUNT_SPACE_CACHE) {
		ret = load_free_space_cache(fs_info, cache);

		spin_lock(&cache->lock);
		if (ret == 1) {
			cache->caching_ctl = NULL;
			cache->cached = BTRFS_CACHE_FINISHED;
			cache->last_byte_to_unpin = (u64)-1;
		} else {
			if (load_cache_only) {
				cache->caching_ctl = NULL;
				cache->cached = BTRFS_CACHE_NO;
			} else {
				cache->cached = BTRFS_CACHE_STARTED;
			}
		}
		spin_unlock(&cache->lock);
		wake_up(&caching_ctl->wait);
		if (ret == 1) {
			put_caching_control(caching_ctl);
			free_excluded_extents(fs_info->extent_root, cache);
			return 0;
		}
	} else {
		spin_lock(&cache->lock);
		if (load_cache_only) {
			cache->caching_ctl = NULL;
			cache->cached = BTRFS_CACHE_NO;
		} else {
			cache->cached = BTRFS_CACHE_STARTED;
		}
		spin_unlock(&cache->lock);
		wake_up(&caching_ctl->wait);
	}

	if (load_cache_only) {
		put_caching_control(caching_ctl);
		return 0;
	}

	down_write(&fs_info->extent_commit_sem);
	atomic_inc(&caching_ctl->count);
	list_add_tail(&caching_ctl->list, &fs_info->caching_block_groups);
	up_write(&fs_info->extent_commit_sem);

	btrfs_get_block_group(cache);

	btrfs_queue_worker(&fs_info->caching_workers, &caching_ctl->work);

	return ret;
}

static struct btrfs_block_group_cache *
btrfs_lookup_first_block_group(struct btrfs_fs_info *info, u64 bytenr)
{
	struct btrfs_block_group_cache *cache;

	cache = block_group_cache_tree_search(info, bytenr, 0);

	return cache;
}

struct btrfs_block_group_cache *btrfs_lookup_block_group(
						 struct btrfs_fs_info *info,
						 u64 bytenr)
{
	struct btrfs_block_group_cache *cache;

	cache = block_group_cache_tree_search(info, bytenr, 1);

	return cache;
}

static struct btrfs_space_info *__find_space_info(struct btrfs_fs_info *info,
						  u64 flags)
{
	struct list_head *head = &info->space_info;
	struct btrfs_space_info *found;

	flags &= BTRFS_BLOCK_GROUP_TYPE_MASK;

	rcu_read_lock();
	list_for_each_entry_rcu(found, head, list) {
		if (found->flags & flags) {
			rcu_read_unlock();
			return found;
		}
	}
	rcu_read_unlock();
	return NULL;
}

void btrfs_clear_space_info_full(struct btrfs_fs_info *info)
{
	struct list_head *head = &info->space_info;
	struct btrfs_space_info *found;

	rcu_read_lock();
	list_for_each_entry_rcu(found, head, list)
		found->full = 0;
	rcu_read_unlock();
}

static u64 div_factor(u64 num, int factor)
{
	if (factor == 10)
		return num;
	num *= factor;
	do_div(num, 10);
	return num;
}

static u64 div_factor_fine(u64 num, int factor)
{
	if (factor == 100)
		return num;
	num *= factor;
	do_div(num, 100);
	return num;
}

u64 btrfs_find_block_group(struct btrfs_root *root,
			   u64 search_start, u64 search_hint, int owner)
{
	struct btrfs_block_group_cache *cache;
	u64 used;
	u64 last = max(search_hint, search_start);
	u64 group_start = 0;
	int full_search = 0;
	int factor = 9;
	int wrapped = 0;
again:
	while (1) {
		cache = btrfs_lookup_first_block_group(root->fs_info, last);
		if (!cache)
			break;

		spin_lock(&cache->lock);
		last = cache->key.objectid + cache->key.offset;
		used = btrfs_block_group_used(&cache->item);

		if ((full_search || !cache->ro) &&
		    block_group_bits(cache, BTRFS_BLOCK_GROUP_METADATA)) {
			if (used + cache->pinned + cache->reserved <
			    div_factor(cache->key.offset, factor)) {
				group_start = cache->key.objectid;
				spin_unlock(&cache->lock);
				btrfs_put_block_group(cache);
				goto found;
			}
		}
		spin_unlock(&cache->lock);
		btrfs_put_block_group(cache);
		cond_resched();
	}
	if (!wrapped) {
		last = search_start;
		wrapped = 1;
		goto again;
	}
	if (!full_search && factor < 10) {
		last = search_start;
		full_search = 1;
		factor = 10;
		goto again;
	}
found:
	return group_start;
}

int btrfs_lookup_extent(struct btrfs_root *root, u64 start, u64 len)
{
	int ret;
	struct btrfs_key key;
	struct btrfs_path *path;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	key.objectid = start;
	key.offset = len;
	btrfs_set_key_type(&key, BTRFS_EXTENT_ITEM_KEY);
	ret = btrfs_search_slot(NULL, root->fs_info->extent_root, &key, path,
				0, 0);
	btrfs_free_path(path);
	return ret;
}

int btrfs_lookup_extent_info(struct btrfs_trans_handle *trans,
			     struct btrfs_root *root, u64 bytenr,
			     u64 num_bytes, u64 *refs, u64 *flags)
{
	struct btrfs_delayed_ref_head *head;
	struct btrfs_delayed_ref_root *delayed_refs;
	struct btrfs_path *path;
	struct btrfs_extent_item *ei;
	struct extent_buffer *leaf;
	struct btrfs_key key;
	u32 item_size;
	u64 num_refs;
	u64 extent_flags;
	int ret;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	key.objectid = bytenr;
	key.type = BTRFS_EXTENT_ITEM_KEY;
	key.offset = num_bytes;
	if (!trans) {
		path->skip_locking = 1;
		path->search_commit_root = 1;
	}
again:
	ret = btrfs_search_slot(trans, root->fs_info->extent_root,
				&key, path, 0, 0);
	if (ret < 0)
		goto out_free;

	if (ret == 0) {
		leaf = path->nodes[0];
		item_size = btrfs_item_size_nr(leaf, path->slots[0]);
		if (item_size >= sizeof(*ei)) {
			ei = btrfs_item_ptr(leaf, path->slots[0],
					    struct btrfs_extent_item);
			num_refs = btrfs_extent_refs(leaf, ei);
			extent_flags = btrfs_extent_flags(leaf, ei);
		} else {
#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
			struct btrfs_extent_item_v0 *ei0;
			BUG_ON(item_size != sizeof(*ei0));
			ei0 = btrfs_item_ptr(leaf, path->slots[0],
					     struct btrfs_extent_item_v0);
			num_refs = btrfs_extent_refs_v0(leaf, ei0);
			
			extent_flags = BTRFS_BLOCK_FLAG_FULL_BACKREF;
#else
			BUG();
#endif
		}
		BUG_ON(num_refs == 0);
	} else {
		num_refs = 0;
		extent_flags = 0;
		ret = 0;
	}

	if (!trans)
		goto out;

	delayed_refs = &trans->transaction->delayed_refs;
	spin_lock(&delayed_refs->lock);
	head = btrfs_find_delayed_ref_head(trans, bytenr);
	if (head) {
		if (!mutex_trylock(&head->mutex)) {
			atomic_inc(&head->node.refs);
			spin_unlock(&delayed_refs->lock);

			btrfs_release_path(path);

			mutex_lock(&head->mutex);
			mutex_unlock(&head->mutex);
			btrfs_put_delayed_ref(&head->node);
			goto again;
		}
		if (head->extent_op && head->extent_op->update_flags)
			extent_flags |= head->extent_op->flags_to_set;
		else
			BUG_ON(num_refs == 0);

		num_refs += head->node.ref_mod;
		mutex_unlock(&head->mutex);
	}
	spin_unlock(&delayed_refs->lock);
out:
	WARN_ON(num_refs == 0);
	if (refs)
		*refs = num_refs;
	if (flags)
		*flags = extent_flags;
out_free:
	btrfs_free_path(path);
	return ret;
}


#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
static int convert_extent_item_v0(struct btrfs_trans_handle *trans,
				  struct btrfs_root *root,
				  struct btrfs_path *path,
				  u64 owner, u32 extra_size)
{
	struct btrfs_extent_item *item;
	struct btrfs_extent_item_v0 *ei0;
	struct btrfs_extent_ref_v0 *ref0;
	struct btrfs_tree_block_info *bi;
	struct extent_buffer *leaf;
	struct btrfs_key key;
	struct btrfs_key found_key;
	u32 new_size = sizeof(*item);
	u64 refs;
	int ret;

	leaf = path->nodes[0];
	BUG_ON(btrfs_item_size_nr(leaf, path->slots[0]) != sizeof(*ei0));

	btrfs_item_key_to_cpu(leaf, &key, path->slots[0]);
	ei0 = btrfs_item_ptr(leaf, path->slots[0],
			     struct btrfs_extent_item_v0);
	refs = btrfs_extent_refs_v0(leaf, ei0);

	if (owner == (u64)-1) {
		while (1) {
			if (path->slots[0] >= btrfs_header_nritems(leaf)) {
				ret = btrfs_next_leaf(root, path);
				if (ret < 0)
					return ret;
				BUG_ON(ret > 0); 
				leaf = path->nodes[0];
			}
			btrfs_item_key_to_cpu(leaf, &found_key,
					      path->slots[0]);
			BUG_ON(key.objectid != found_key.objectid);
			if (found_key.type != BTRFS_EXTENT_REF_V0_KEY) {
				path->slots[0]++;
				continue;
			}
			ref0 = btrfs_item_ptr(leaf, path->slots[0],
					      struct btrfs_extent_ref_v0);
			owner = btrfs_ref_objectid_v0(leaf, ref0);
			break;
		}
	}
	btrfs_release_path(path);

	if (owner < BTRFS_FIRST_FREE_OBJECTID)
		new_size += sizeof(*bi);

	new_size -= sizeof(*ei0);
	ret = btrfs_search_slot(trans, root, &key, path,
				new_size + extra_size, 1);
	if (ret < 0)
		return ret;
	BUG_ON(ret); 

	btrfs_extend_item(trans, root, path, new_size);

	leaf = path->nodes[0];
	item = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_extent_item);
	btrfs_set_extent_refs(leaf, item, refs);
	
	btrfs_set_extent_generation(leaf, item, 0);
	if (owner < BTRFS_FIRST_FREE_OBJECTID) {
		btrfs_set_extent_flags(leaf, item,
				       BTRFS_EXTENT_FLAG_TREE_BLOCK |
				       BTRFS_BLOCK_FLAG_FULL_BACKREF);
		bi = (struct btrfs_tree_block_info *)(item + 1);
		
		memset_extent_buffer(leaf, 0, (unsigned long)bi, sizeof(*bi));
		btrfs_set_tree_block_level(leaf, bi, (int)owner);
	} else {
		btrfs_set_extent_flags(leaf, item, BTRFS_EXTENT_FLAG_DATA);
	}
	btrfs_mark_buffer_dirty(leaf);
	return 0;
}
#endif

static u64 hash_extent_data_ref(u64 root_objectid, u64 owner, u64 offset)
{
	u32 high_crc = ~(u32)0;
	u32 low_crc = ~(u32)0;
	__le64 lenum;

	lenum = cpu_to_le64(root_objectid);
	high_crc = crc32c(high_crc, &lenum, sizeof(lenum));
	lenum = cpu_to_le64(owner);
	low_crc = crc32c(low_crc, &lenum, sizeof(lenum));
	lenum = cpu_to_le64(offset);
	low_crc = crc32c(low_crc, &lenum, sizeof(lenum));

	return ((u64)high_crc << 31) ^ (u64)low_crc;
}

static u64 hash_extent_data_ref_item(struct extent_buffer *leaf,
				     struct btrfs_extent_data_ref *ref)
{
	return hash_extent_data_ref(btrfs_extent_data_ref_root(leaf, ref),
				    btrfs_extent_data_ref_objectid(leaf, ref),
				    btrfs_extent_data_ref_offset(leaf, ref));
}

static int match_extent_data_ref(struct extent_buffer *leaf,
				 struct btrfs_extent_data_ref *ref,
				 u64 root_objectid, u64 owner, u64 offset)
{
	if (btrfs_extent_data_ref_root(leaf, ref) != root_objectid ||
	    btrfs_extent_data_ref_objectid(leaf, ref) != owner ||
	    btrfs_extent_data_ref_offset(leaf, ref) != offset)
		return 0;
	return 1;
}

static noinline int lookup_extent_data_ref(struct btrfs_trans_handle *trans,
					   struct btrfs_root *root,
					   struct btrfs_path *path,
					   u64 bytenr, u64 parent,
					   u64 root_objectid,
					   u64 owner, u64 offset)
{
	struct btrfs_key key;
	struct btrfs_extent_data_ref *ref;
	struct extent_buffer *leaf;
	u32 nritems;
	int ret;
	int recow;
	int err = -ENOENT;

	key.objectid = bytenr;
	if (parent) {
		key.type = BTRFS_SHARED_DATA_REF_KEY;
		key.offset = parent;
	} else {
		key.type = BTRFS_EXTENT_DATA_REF_KEY;
		key.offset = hash_extent_data_ref(root_objectid,
						  owner, offset);
	}
again:
	recow = 0;
	ret = btrfs_search_slot(trans, root, &key, path, -1, 1);
	if (ret < 0) {
		err = ret;
		goto fail;
	}

	if (parent) {
		if (!ret)
			return 0;
#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
		key.type = BTRFS_EXTENT_REF_V0_KEY;
		btrfs_release_path(path);
		ret = btrfs_search_slot(trans, root, &key, path, -1, 1);
		if (ret < 0) {
			err = ret;
			goto fail;
		}
		if (!ret)
			return 0;
#endif
		goto fail;
	}

	leaf = path->nodes[0];
	nritems = btrfs_header_nritems(leaf);
	while (1) {
		if (path->slots[0] >= nritems) {
			ret = btrfs_next_leaf(root, path);
			if (ret < 0)
				err = ret;
			if (ret)
				goto fail;

			leaf = path->nodes[0];
			nritems = btrfs_header_nritems(leaf);
			recow = 1;
		}

		btrfs_item_key_to_cpu(leaf, &key, path->slots[0]);
		if (key.objectid != bytenr ||
		    key.type != BTRFS_EXTENT_DATA_REF_KEY)
			goto fail;

		ref = btrfs_item_ptr(leaf, path->slots[0],
				     struct btrfs_extent_data_ref);

		if (match_extent_data_ref(leaf, ref, root_objectid,
					  owner, offset)) {
			if (recow) {
				btrfs_release_path(path);
				goto again;
			}
			err = 0;
			break;
		}
		path->slots[0]++;
	}
fail:
	return err;
}

static noinline int insert_extent_data_ref(struct btrfs_trans_handle *trans,
					   struct btrfs_root *root,
					   struct btrfs_path *path,
					   u64 bytenr, u64 parent,
					   u64 root_objectid, u64 owner,
					   u64 offset, int refs_to_add)
{
	struct btrfs_key key;
	struct extent_buffer *leaf;
	u32 size;
	u32 num_refs;
	int ret;

	key.objectid = bytenr;
	if (parent) {
		key.type = BTRFS_SHARED_DATA_REF_KEY;
		key.offset = parent;
		size = sizeof(struct btrfs_shared_data_ref);
	} else {
		key.type = BTRFS_EXTENT_DATA_REF_KEY;
		key.offset = hash_extent_data_ref(root_objectid,
						  owner, offset);
		size = sizeof(struct btrfs_extent_data_ref);
	}

	ret = btrfs_insert_empty_item(trans, root, path, &key, size);
	if (ret && ret != -EEXIST)
		goto fail;

	leaf = path->nodes[0];
	if (parent) {
		struct btrfs_shared_data_ref *ref;
		ref = btrfs_item_ptr(leaf, path->slots[0],
				     struct btrfs_shared_data_ref);
		if (ret == 0) {
			btrfs_set_shared_data_ref_count(leaf, ref, refs_to_add);
		} else {
			num_refs = btrfs_shared_data_ref_count(leaf, ref);
			num_refs += refs_to_add;
			btrfs_set_shared_data_ref_count(leaf, ref, num_refs);
		}
	} else {
		struct btrfs_extent_data_ref *ref;
		while (ret == -EEXIST) {
			ref = btrfs_item_ptr(leaf, path->slots[0],
					     struct btrfs_extent_data_ref);
			if (match_extent_data_ref(leaf, ref, root_objectid,
						  owner, offset))
				break;
			btrfs_release_path(path);
			key.offset++;
			ret = btrfs_insert_empty_item(trans, root, path, &key,
						      size);
			if (ret && ret != -EEXIST)
				goto fail;

			leaf = path->nodes[0];
		}
		ref = btrfs_item_ptr(leaf, path->slots[0],
				     struct btrfs_extent_data_ref);
		if (ret == 0) {
			btrfs_set_extent_data_ref_root(leaf, ref,
						       root_objectid);
			btrfs_set_extent_data_ref_objectid(leaf, ref, owner);
			btrfs_set_extent_data_ref_offset(leaf, ref, offset);
			btrfs_set_extent_data_ref_count(leaf, ref, refs_to_add);
		} else {
			num_refs = btrfs_extent_data_ref_count(leaf, ref);
			num_refs += refs_to_add;
			btrfs_set_extent_data_ref_count(leaf, ref, num_refs);
		}
	}
	btrfs_mark_buffer_dirty(leaf);
	ret = 0;
fail:
	btrfs_release_path(path);
	return ret;
}

static noinline int remove_extent_data_ref(struct btrfs_trans_handle *trans,
					   struct btrfs_root *root,
					   struct btrfs_path *path,
					   int refs_to_drop)
{
	struct btrfs_key key;
	struct btrfs_extent_data_ref *ref1 = NULL;
	struct btrfs_shared_data_ref *ref2 = NULL;
	struct extent_buffer *leaf;
	u32 num_refs = 0;
	int ret = 0;

	leaf = path->nodes[0];
	btrfs_item_key_to_cpu(leaf, &key, path->slots[0]);

	if (key.type == BTRFS_EXTENT_DATA_REF_KEY) {
		ref1 = btrfs_item_ptr(leaf, path->slots[0],
				      struct btrfs_extent_data_ref);
		num_refs = btrfs_extent_data_ref_count(leaf, ref1);
	} else if (key.type == BTRFS_SHARED_DATA_REF_KEY) {
		ref2 = btrfs_item_ptr(leaf, path->slots[0],
				      struct btrfs_shared_data_ref);
		num_refs = btrfs_shared_data_ref_count(leaf, ref2);
#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
	} else if (key.type == BTRFS_EXTENT_REF_V0_KEY) {
		struct btrfs_extent_ref_v0 *ref0;
		ref0 = btrfs_item_ptr(leaf, path->slots[0],
				      struct btrfs_extent_ref_v0);
		num_refs = btrfs_ref_count_v0(leaf, ref0);
#endif
	} else {
		BUG();
	}

	BUG_ON(num_refs < refs_to_drop);
	num_refs -= refs_to_drop;

	if (num_refs == 0) {
		ret = btrfs_del_item(trans, root, path);
	} else {
		if (key.type == BTRFS_EXTENT_DATA_REF_KEY)
			btrfs_set_extent_data_ref_count(leaf, ref1, num_refs);
		else if (key.type == BTRFS_SHARED_DATA_REF_KEY)
			btrfs_set_shared_data_ref_count(leaf, ref2, num_refs);
#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
		else {
			struct btrfs_extent_ref_v0 *ref0;
			ref0 = btrfs_item_ptr(leaf, path->slots[0],
					struct btrfs_extent_ref_v0);
			btrfs_set_ref_count_v0(leaf, ref0, num_refs);
		}
#endif
		btrfs_mark_buffer_dirty(leaf);
	}
	return ret;
}

static noinline u32 extent_data_ref_count(struct btrfs_root *root,
					  struct btrfs_path *path,
					  struct btrfs_extent_inline_ref *iref)
{
	struct btrfs_key key;
	struct extent_buffer *leaf;
	struct btrfs_extent_data_ref *ref1;
	struct btrfs_shared_data_ref *ref2;
	u32 num_refs = 0;

	leaf = path->nodes[0];
	btrfs_item_key_to_cpu(leaf, &key, path->slots[0]);
	if (iref) {
		if (btrfs_extent_inline_ref_type(leaf, iref) ==
		    BTRFS_EXTENT_DATA_REF_KEY) {
			ref1 = (struct btrfs_extent_data_ref *)(&iref->offset);
			num_refs = btrfs_extent_data_ref_count(leaf, ref1);
		} else {
			ref2 = (struct btrfs_shared_data_ref *)(iref + 1);
			num_refs = btrfs_shared_data_ref_count(leaf, ref2);
		}
	} else if (key.type == BTRFS_EXTENT_DATA_REF_KEY) {
		ref1 = btrfs_item_ptr(leaf, path->slots[0],
				      struct btrfs_extent_data_ref);
		num_refs = btrfs_extent_data_ref_count(leaf, ref1);
	} else if (key.type == BTRFS_SHARED_DATA_REF_KEY) {
		ref2 = btrfs_item_ptr(leaf, path->slots[0],
				      struct btrfs_shared_data_ref);
		num_refs = btrfs_shared_data_ref_count(leaf, ref2);
#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
	} else if (key.type == BTRFS_EXTENT_REF_V0_KEY) {
		struct btrfs_extent_ref_v0 *ref0;
		ref0 = btrfs_item_ptr(leaf, path->slots[0],
				      struct btrfs_extent_ref_v0);
		num_refs = btrfs_ref_count_v0(leaf, ref0);
#endif
	} else {
		WARN_ON(1);
	}
	return num_refs;
}

static noinline int lookup_tree_block_ref(struct btrfs_trans_handle *trans,
					  struct btrfs_root *root,
					  struct btrfs_path *path,
					  u64 bytenr, u64 parent,
					  u64 root_objectid)
{
	struct btrfs_key key;
	int ret;

	key.objectid = bytenr;
	if (parent) {
		key.type = BTRFS_SHARED_BLOCK_REF_KEY;
		key.offset = parent;
	} else {
		key.type = BTRFS_TREE_BLOCK_REF_KEY;
		key.offset = root_objectid;
	}

	ret = btrfs_search_slot(trans, root, &key, path, -1, 1);
	if (ret > 0)
		ret = -ENOENT;
#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
	if (ret == -ENOENT && parent) {
		btrfs_release_path(path);
		key.type = BTRFS_EXTENT_REF_V0_KEY;
		ret = btrfs_search_slot(trans, root, &key, path, -1, 1);
		if (ret > 0)
			ret = -ENOENT;
	}
#endif
	return ret;
}

static noinline int insert_tree_block_ref(struct btrfs_trans_handle *trans,
					  struct btrfs_root *root,
					  struct btrfs_path *path,
					  u64 bytenr, u64 parent,
					  u64 root_objectid)
{
	struct btrfs_key key;
	int ret;

	key.objectid = bytenr;
	if (parent) {
		key.type = BTRFS_SHARED_BLOCK_REF_KEY;
		key.offset = parent;
	} else {
		key.type = BTRFS_TREE_BLOCK_REF_KEY;
		key.offset = root_objectid;
	}

	ret = btrfs_insert_empty_item(trans, root, path, &key, 0);
	btrfs_release_path(path);
	return ret;
}

static inline int extent_ref_type(u64 parent, u64 owner)
{
	int type;
	if (owner < BTRFS_FIRST_FREE_OBJECTID) {
		if (parent > 0)
			type = BTRFS_SHARED_BLOCK_REF_KEY;
		else
			type = BTRFS_TREE_BLOCK_REF_KEY;
	} else {
		if (parent > 0)
			type = BTRFS_SHARED_DATA_REF_KEY;
		else
			type = BTRFS_EXTENT_DATA_REF_KEY;
	}
	return type;
}

static int find_next_key(struct btrfs_path *path, int level,
			 struct btrfs_key *key)

{
	for (; level < BTRFS_MAX_LEVEL; level++) {
		if (!path->nodes[level])
			break;
		if (path->slots[level] + 1 >=
		    btrfs_header_nritems(path->nodes[level]))
			continue;
		if (level == 0)
			btrfs_item_key_to_cpu(path->nodes[level], key,
					      path->slots[level] + 1);
		else
			btrfs_node_key_to_cpu(path->nodes[level], key,
					      path->slots[level] + 1);
		return 0;
	}
	return 1;
}

static noinline_for_stack
int lookup_inline_extent_backref(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root,
				 struct btrfs_path *path,
				 struct btrfs_extent_inline_ref **ref_ret,
				 u64 bytenr, u64 num_bytes,
				 u64 parent, u64 root_objectid,
				 u64 owner, u64 offset, int insert)
{
	struct btrfs_key key;
	struct extent_buffer *leaf;
	struct btrfs_extent_item *ei;
	struct btrfs_extent_inline_ref *iref;
	u64 flags;
	u64 item_size;
	unsigned long ptr;
	unsigned long end;
	int extra_size;
	int type;
	int want;
	int ret;
	int err = 0;

	key.objectid = bytenr;
	key.type = BTRFS_EXTENT_ITEM_KEY;
	key.offset = num_bytes;

	want = extent_ref_type(parent, owner);
	if (insert) {
		extra_size = btrfs_extent_inline_ref_size(want);
		path->keep_locks = 1;
	} else
		extra_size = -1;
	ret = btrfs_search_slot(trans, root, &key, path, extra_size, 1);
	if (ret < 0) {
		err = ret;
		goto out;
	}
	if (ret && !insert) {
		err = -ENOENT;
		goto out;
	}
	BUG_ON(ret); 

	leaf = path->nodes[0];
	item_size = btrfs_item_size_nr(leaf, path->slots[0]);
#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
	if (item_size < sizeof(*ei)) {
		if (!insert) {
			err = -ENOENT;
			goto out;
		}
		ret = convert_extent_item_v0(trans, root, path, owner,
					     extra_size);
		if (ret < 0) {
			err = ret;
			goto out;
		}
		leaf = path->nodes[0];
		item_size = btrfs_item_size_nr(leaf, path->slots[0]);
	}
#endif
	BUG_ON(item_size < sizeof(*ei));

	ei = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_extent_item);
	flags = btrfs_extent_flags(leaf, ei);

	ptr = (unsigned long)(ei + 1);
	end = (unsigned long)ei + item_size;

	if (flags & BTRFS_EXTENT_FLAG_TREE_BLOCK) {
		ptr += sizeof(struct btrfs_tree_block_info);
		BUG_ON(ptr > end);
	} else {
		BUG_ON(!(flags & BTRFS_EXTENT_FLAG_DATA));
	}

	err = -ENOENT;
	while (1) {
		if (ptr >= end) {
			WARN_ON(ptr > end);
			break;
		}
		iref = (struct btrfs_extent_inline_ref *)ptr;
		type = btrfs_extent_inline_ref_type(leaf, iref);
		if (want < type)
			break;
		if (want > type) {
			ptr += btrfs_extent_inline_ref_size(type);
			continue;
		}

		if (type == BTRFS_EXTENT_DATA_REF_KEY) {
			struct btrfs_extent_data_ref *dref;
			dref = (struct btrfs_extent_data_ref *)(&iref->offset);
			if (match_extent_data_ref(leaf, dref, root_objectid,
						  owner, offset)) {
				err = 0;
				break;
			}
			if (hash_extent_data_ref_item(leaf, dref) <
			    hash_extent_data_ref(root_objectid, owner, offset))
				break;
		} else {
			u64 ref_offset;
			ref_offset = btrfs_extent_inline_ref_offset(leaf, iref);
			if (parent > 0) {
				if (parent == ref_offset) {
					err = 0;
					break;
				}
				if (ref_offset < parent)
					break;
			} else {
				if (root_objectid == ref_offset) {
					err = 0;
					break;
				}
				if (ref_offset < root_objectid)
					break;
			}
		}
		ptr += btrfs_extent_inline_ref_size(type);
	}
	if (err == -ENOENT && insert) {
		if (item_size + extra_size >=
		    BTRFS_MAX_EXTENT_ITEM_SIZE(root)) {
			err = -EAGAIN;
			goto out;
		}
		if (find_next_key(path, 0, &key) == 0 &&
		    key.objectid == bytenr &&
		    key.type < BTRFS_BLOCK_GROUP_ITEM_KEY) {
			err = -EAGAIN;
			goto out;
		}
	}
	*ref_ret = (struct btrfs_extent_inline_ref *)ptr;
out:
	if (insert) {
		path->keep_locks = 0;
		btrfs_unlock_up_safe(path, 1);
	}
	return err;
}

static noinline_for_stack
void setup_inline_extent_backref(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root,
				 struct btrfs_path *path,
				 struct btrfs_extent_inline_ref *iref,
				 u64 parent, u64 root_objectid,
				 u64 owner, u64 offset, int refs_to_add,
				 struct btrfs_delayed_extent_op *extent_op)
{
	struct extent_buffer *leaf;
	struct btrfs_extent_item *ei;
	unsigned long ptr;
	unsigned long end;
	unsigned long item_offset;
	u64 refs;
	int size;
	int type;

	leaf = path->nodes[0];
	ei = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_extent_item);
	item_offset = (unsigned long)iref - (unsigned long)ei;

	type = extent_ref_type(parent, owner);
	size = btrfs_extent_inline_ref_size(type);

	btrfs_extend_item(trans, root, path, size);

	ei = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_extent_item);
	refs = btrfs_extent_refs(leaf, ei);
	refs += refs_to_add;
	btrfs_set_extent_refs(leaf, ei, refs);
	if (extent_op)
		__run_delayed_extent_op(extent_op, leaf, ei);

	ptr = (unsigned long)ei + item_offset;
	end = (unsigned long)ei + btrfs_item_size_nr(leaf, path->slots[0]);
	if (ptr < end - size)
		memmove_extent_buffer(leaf, ptr + size, ptr,
				      end - size - ptr);

	iref = (struct btrfs_extent_inline_ref *)ptr;
	btrfs_set_extent_inline_ref_type(leaf, iref, type);
	if (type == BTRFS_EXTENT_DATA_REF_KEY) {
		struct btrfs_extent_data_ref *dref;
		dref = (struct btrfs_extent_data_ref *)(&iref->offset);
		btrfs_set_extent_data_ref_root(leaf, dref, root_objectid);
		btrfs_set_extent_data_ref_objectid(leaf, dref, owner);
		btrfs_set_extent_data_ref_offset(leaf, dref, offset);
		btrfs_set_extent_data_ref_count(leaf, dref, refs_to_add);
	} else if (type == BTRFS_SHARED_DATA_REF_KEY) {
		struct btrfs_shared_data_ref *sref;
		sref = (struct btrfs_shared_data_ref *)(iref + 1);
		btrfs_set_shared_data_ref_count(leaf, sref, refs_to_add);
		btrfs_set_extent_inline_ref_offset(leaf, iref, parent);
	} else if (type == BTRFS_SHARED_BLOCK_REF_KEY) {
		btrfs_set_extent_inline_ref_offset(leaf, iref, parent);
	} else {
		btrfs_set_extent_inline_ref_offset(leaf, iref, root_objectid);
	}
	btrfs_mark_buffer_dirty(leaf);
}

static int lookup_extent_backref(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root,
				 struct btrfs_path *path,
				 struct btrfs_extent_inline_ref **ref_ret,
				 u64 bytenr, u64 num_bytes, u64 parent,
				 u64 root_objectid, u64 owner, u64 offset)
{
	int ret;

	ret = lookup_inline_extent_backref(trans, root, path, ref_ret,
					   bytenr, num_bytes, parent,
					   root_objectid, owner, offset, 0);
	if (ret != -ENOENT)
		return ret;

	btrfs_release_path(path);
	*ref_ret = NULL;

	if (owner < BTRFS_FIRST_FREE_OBJECTID) {
		ret = lookup_tree_block_ref(trans, root, path, bytenr, parent,
					    root_objectid);
	} else {
		ret = lookup_extent_data_ref(trans, root, path, bytenr, parent,
					     root_objectid, owner, offset);
	}
	return ret;
}

static noinline_for_stack
void update_inline_extent_backref(struct btrfs_trans_handle *trans,
				  struct btrfs_root *root,
				  struct btrfs_path *path,
				  struct btrfs_extent_inline_ref *iref,
				  int refs_to_mod,
				  struct btrfs_delayed_extent_op *extent_op)
{
	struct extent_buffer *leaf;
	struct btrfs_extent_item *ei;
	struct btrfs_extent_data_ref *dref = NULL;
	struct btrfs_shared_data_ref *sref = NULL;
	unsigned long ptr;
	unsigned long end;
	u32 item_size;
	int size;
	int type;
	u64 refs;

	leaf = path->nodes[0];
	ei = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_extent_item);
	refs = btrfs_extent_refs(leaf, ei);
	WARN_ON(refs_to_mod < 0 && refs + refs_to_mod <= 0);
	refs += refs_to_mod;
	btrfs_set_extent_refs(leaf, ei, refs);
	if (extent_op)
		__run_delayed_extent_op(extent_op, leaf, ei);

	type = btrfs_extent_inline_ref_type(leaf, iref);

	if (type == BTRFS_EXTENT_DATA_REF_KEY) {
		dref = (struct btrfs_extent_data_ref *)(&iref->offset);
		refs = btrfs_extent_data_ref_count(leaf, dref);
	} else if (type == BTRFS_SHARED_DATA_REF_KEY) {
		sref = (struct btrfs_shared_data_ref *)(iref + 1);
		refs = btrfs_shared_data_ref_count(leaf, sref);
	} else {
		refs = 1;
		BUG_ON(refs_to_mod != -1);
	}

	BUG_ON(refs_to_mod < 0 && refs < -refs_to_mod);
	refs += refs_to_mod;

	if (refs > 0) {
		if (type == BTRFS_EXTENT_DATA_REF_KEY)
			btrfs_set_extent_data_ref_count(leaf, dref, refs);
		else
			btrfs_set_shared_data_ref_count(leaf, sref, refs);
	} else {
		size =  btrfs_extent_inline_ref_size(type);
		item_size = btrfs_item_size_nr(leaf, path->slots[0]);
		ptr = (unsigned long)iref;
		end = (unsigned long)ei + item_size;
		if (ptr + size < end)
			memmove_extent_buffer(leaf, ptr, ptr + size,
					      end - ptr - size);
		item_size -= size;
		btrfs_truncate_item(trans, root, path, item_size, 1);
	}
	btrfs_mark_buffer_dirty(leaf);
}

static noinline_for_stack
int insert_inline_extent_backref(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root,
				 struct btrfs_path *path,
				 u64 bytenr, u64 num_bytes, u64 parent,
				 u64 root_objectid, u64 owner,
				 u64 offset, int refs_to_add,
				 struct btrfs_delayed_extent_op *extent_op)
{
	struct btrfs_extent_inline_ref *iref;
	int ret;

	ret = lookup_inline_extent_backref(trans, root, path, &iref,
					   bytenr, num_bytes, parent,
					   root_objectid, owner, offset, 1);
	if (ret == 0) {
		BUG_ON(owner < BTRFS_FIRST_FREE_OBJECTID);
		update_inline_extent_backref(trans, root, path, iref,
					     refs_to_add, extent_op);
	} else if (ret == -ENOENT) {
		setup_inline_extent_backref(trans, root, path, iref, parent,
					    root_objectid, owner, offset,
					    refs_to_add, extent_op);
		ret = 0;
	}
	return ret;
}

static int insert_extent_backref(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root,
				 struct btrfs_path *path,
				 u64 bytenr, u64 parent, u64 root_objectid,
				 u64 owner, u64 offset, int refs_to_add)
{
	int ret;
	if (owner < BTRFS_FIRST_FREE_OBJECTID) {
		BUG_ON(refs_to_add != 1);
		ret = insert_tree_block_ref(trans, root, path, bytenr,
					    parent, root_objectid);
	} else {
		ret = insert_extent_data_ref(trans, root, path, bytenr,
					     parent, root_objectid,
					     owner, offset, refs_to_add);
	}
	return ret;
}

static int remove_extent_backref(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root,
				 struct btrfs_path *path,
				 struct btrfs_extent_inline_ref *iref,
				 int refs_to_drop, int is_data)
{
	int ret = 0;

	BUG_ON(!is_data && refs_to_drop != 1);
	if (iref) {
		update_inline_extent_backref(trans, root, path, iref,
					     -refs_to_drop, NULL);
	} else if (is_data) {
		ret = remove_extent_data_ref(trans, root, path, refs_to_drop);
	} else {
		ret = btrfs_del_item(trans, root, path);
	}
	return ret;
}

static int btrfs_issue_discard(struct block_device *bdev,
				u64 start, u64 len)
{
	return blkdev_issue_discard(bdev, start >> 9, len >> 9, GFP_NOFS, 0);
}

static int btrfs_discard_extent(struct btrfs_root *root, u64 bytenr,
				u64 num_bytes, u64 *actual_bytes)
{
	int ret;
	u64 discarded_bytes = 0;
	struct btrfs_bio *bbio = NULL;


	
	ret = btrfs_map_block(&root->fs_info->mapping_tree, REQ_DISCARD,
			      bytenr, &num_bytes, &bbio, 0);
	
	if (!ret) {
		struct btrfs_bio_stripe *stripe = bbio->stripes;
		int i;


		for (i = 0; i < bbio->num_stripes; i++, stripe++) {
			if (!stripe->dev->can_discard)
				continue;

			ret = btrfs_issue_discard(stripe->dev->bdev,
						  stripe->physical,
						  stripe->length);
			if (!ret)
				discarded_bytes += stripe->length;
			else if (ret != -EOPNOTSUPP)
				break; 

			ret = 0;
		}
		kfree(bbio);
	}

	if (actual_bytes)
		*actual_bytes = discarded_bytes;


	return ret;
}

int btrfs_inc_extent_ref(struct btrfs_trans_handle *trans,
			 struct btrfs_root *root,
			 u64 bytenr, u64 num_bytes, u64 parent,
			 u64 root_objectid, u64 owner, u64 offset, int for_cow)
{
	int ret;
	struct btrfs_fs_info *fs_info = root->fs_info;

	BUG_ON(owner < BTRFS_FIRST_FREE_OBJECTID &&
	       root_objectid == BTRFS_TREE_LOG_OBJECTID);

	if (owner < BTRFS_FIRST_FREE_OBJECTID) {
		ret = btrfs_add_delayed_tree_ref(fs_info, trans, bytenr,
					num_bytes,
					parent, root_objectid, (int)owner,
					BTRFS_ADD_DELAYED_REF, NULL, for_cow);
	} else {
		ret = btrfs_add_delayed_data_ref(fs_info, trans, bytenr,
					num_bytes,
					parent, root_objectid, owner, offset,
					BTRFS_ADD_DELAYED_REF, NULL, for_cow);
	}
	return ret;
}

static int __btrfs_inc_extent_ref(struct btrfs_trans_handle *trans,
				  struct btrfs_root *root,
				  u64 bytenr, u64 num_bytes,
				  u64 parent, u64 root_objectid,
				  u64 owner, u64 offset, int refs_to_add,
				  struct btrfs_delayed_extent_op *extent_op)
{
	struct btrfs_path *path;
	struct extent_buffer *leaf;
	struct btrfs_extent_item *item;
	u64 refs;
	int ret;
	int err = 0;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	path->reada = 1;
	path->leave_spinning = 1;
	
	ret = insert_inline_extent_backref(trans, root->fs_info->extent_root,
					   path, bytenr, num_bytes, parent,
					   root_objectid, owner, offset,
					   refs_to_add, extent_op);
	if (ret == 0)
		goto out;

	if (ret != -EAGAIN) {
		err = ret;
		goto out;
	}

	leaf = path->nodes[0];
	item = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_extent_item);
	refs = btrfs_extent_refs(leaf, item);
	btrfs_set_extent_refs(leaf, item, refs + refs_to_add);
	if (extent_op)
		__run_delayed_extent_op(extent_op, leaf, item);

	btrfs_mark_buffer_dirty(leaf);
	btrfs_release_path(path);

	path->reada = 1;
	path->leave_spinning = 1;

	
	ret = insert_extent_backref(trans, root->fs_info->extent_root,
				    path, bytenr, parent, root_objectid,
				    owner, offset, refs_to_add);
	if (ret)
		btrfs_abort_transaction(trans, root, ret);
out:
	btrfs_free_path(path);
	return err;
}

static int run_delayed_data_ref(struct btrfs_trans_handle *trans,
				struct btrfs_root *root,
				struct btrfs_delayed_ref_node *node,
				struct btrfs_delayed_extent_op *extent_op,
				int insert_reserved)
{
	int ret = 0;
	struct btrfs_delayed_data_ref *ref;
	struct btrfs_key ins;
	u64 parent = 0;
	u64 ref_root = 0;
	u64 flags = 0;

	ins.objectid = node->bytenr;
	ins.offset = node->num_bytes;
	ins.type = BTRFS_EXTENT_ITEM_KEY;

	ref = btrfs_delayed_node_to_data_ref(node);
	if (node->type == BTRFS_SHARED_DATA_REF_KEY)
		parent = ref->parent;
	else
		ref_root = ref->root;

	if (node->action == BTRFS_ADD_DELAYED_REF && insert_reserved) {
		if (extent_op) {
			BUG_ON(extent_op->update_key);
			flags |= extent_op->flags_to_set;
		}
		ret = alloc_reserved_file_extent(trans, root,
						 parent, ref_root, flags,
						 ref->objectid, ref->offset,
						 &ins, node->ref_mod);
	} else if (node->action == BTRFS_ADD_DELAYED_REF) {
		ret = __btrfs_inc_extent_ref(trans, root, node->bytenr,
					     node->num_bytes, parent,
					     ref_root, ref->objectid,
					     ref->offset, node->ref_mod,
					     extent_op);
	} else if (node->action == BTRFS_DROP_DELAYED_REF) {
		ret = __btrfs_free_extent(trans, root, node->bytenr,
					  node->num_bytes, parent,
					  ref_root, ref->objectid,
					  ref->offset, node->ref_mod,
					  extent_op);
	} else {
		BUG();
	}
	return ret;
}

static void __run_delayed_extent_op(struct btrfs_delayed_extent_op *extent_op,
				    struct extent_buffer *leaf,
				    struct btrfs_extent_item *ei)
{
	u64 flags = btrfs_extent_flags(leaf, ei);
	if (extent_op->update_flags) {
		flags |= extent_op->flags_to_set;
		btrfs_set_extent_flags(leaf, ei, flags);
	}

	if (extent_op->update_key) {
		struct btrfs_tree_block_info *bi;
		BUG_ON(!(flags & BTRFS_EXTENT_FLAG_TREE_BLOCK));
		bi = (struct btrfs_tree_block_info *)(ei + 1);
		btrfs_set_tree_block_key(leaf, bi, &extent_op->key);
	}
}

static int run_delayed_extent_op(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root,
				 struct btrfs_delayed_ref_node *node,
				 struct btrfs_delayed_extent_op *extent_op)
{
	struct btrfs_key key;
	struct btrfs_path *path;
	struct btrfs_extent_item *ei;
	struct extent_buffer *leaf;
	u32 item_size;
	int ret;
	int err = 0;

	if (trans->aborted)
		return 0;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	key.objectid = node->bytenr;
	key.type = BTRFS_EXTENT_ITEM_KEY;
	key.offset = node->num_bytes;

	path->reada = 1;
	path->leave_spinning = 1;
	ret = btrfs_search_slot(trans, root->fs_info->extent_root, &key,
				path, 0, 1);
	if (ret < 0) {
		err = ret;
		goto out;
	}
	if (ret > 0) {
		err = -EIO;
		goto out;
	}

	leaf = path->nodes[0];
	item_size = btrfs_item_size_nr(leaf, path->slots[0]);
#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
	if (item_size < sizeof(*ei)) {
		ret = convert_extent_item_v0(trans, root->fs_info->extent_root,
					     path, (u64)-1, 0);
		if (ret < 0) {
			err = ret;
			goto out;
		}
		leaf = path->nodes[0];
		item_size = btrfs_item_size_nr(leaf, path->slots[0]);
	}
#endif
	BUG_ON(item_size < sizeof(*ei));
	ei = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_extent_item);
	__run_delayed_extent_op(extent_op, leaf, ei);

	btrfs_mark_buffer_dirty(leaf);
out:
	btrfs_free_path(path);
	return err;
}

static int run_delayed_tree_ref(struct btrfs_trans_handle *trans,
				struct btrfs_root *root,
				struct btrfs_delayed_ref_node *node,
				struct btrfs_delayed_extent_op *extent_op,
				int insert_reserved)
{
	int ret = 0;
	struct btrfs_delayed_tree_ref *ref;
	struct btrfs_key ins;
	u64 parent = 0;
	u64 ref_root = 0;

	ins.objectid = node->bytenr;
	ins.offset = node->num_bytes;
	ins.type = BTRFS_EXTENT_ITEM_KEY;

	ref = btrfs_delayed_node_to_tree_ref(node);
	if (node->type == BTRFS_SHARED_BLOCK_REF_KEY)
		parent = ref->parent;
	else
		ref_root = ref->root;

	BUG_ON(node->ref_mod != 1);
	if (node->action == BTRFS_ADD_DELAYED_REF && insert_reserved) {
		BUG_ON(!extent_op || !extent_op->update_flags ||
		       !extent_op->update_key);
		ret = alloc_reserved_tree_block(trans, root,
						parent, ref_root,
						extent_op->flags_to_set,
						&extent_op->key,
						ref->level, &ins);
	} else if (node->action == BTRFS_ADD_DELAYED_REF) {
		ret = __btrfs_inc_extent_ref(trans, root, node->bytenr,
					     node->num_bytes, parent, ref_root,
					     ref->level, 0, 1, extent_op);
	} else if (node->action == BTRFS_DROP_DELAYED_REF) {
		ret = __btrfs_free_extent(trans, root, node->bytenr,
					  node->num_bytes, parent, ref_root,
					  ref->level, 0, 1, extent_op);
	} else {
		BUG();
	}
	return ret;
}

static int run_one_delayed_ref(struct btrfs_trans_handle *trans,
			       struct btrfs_root *root,
			       struct btrfs_delayed_ref_node *node,
			       struct btrfs_delayed_extent_op *extent_op,
			       int insert_reserved)
{
	int ret = 0;

	if (trans->aborted)
		return 0;

	if (btrfs_delayed_ref_is_head(node)) {
		struct btrfs_delayed_ref_head *head;
		BUG_ON(extent_op);
		head = btrfs_delayed_node_to_head(node);
		if (insert_reserved) {
			btrfs_pin_extent(root, node->bytenr,
					 node->num_bytes, 1);
			if (head->is_data) {
				ret = btrfs_del_csums(trans, root,
						      node->bytenr,
						      node->num_bytes);
			}
		}
		mutex_unlock(&head->mutex);
		return ret;
	}

	if (node->type == BTRFS_TREE_BLOCK_REF_KEY ||
	    node->type == BTRFS_SHARED_BLOCK_REF_KEY)
		ret = run_delayed_tree_ref(trans, root, node, extent_op,
					   insert_reserved);
	else if (node->type == BTRFS_EXTENT_DATA_REF_KEY ||
		 node->type == BTRFS_SHARED_DATA_REF_KEY)
		ret = run_delayed_data_ref(trans, root, node, extent_op,
					   insert_reserved);
	else
		BUG();
	return ret;
}

static noinline struct btrfs_delayed_ref_node *
select_delayed_ref(struct btrfs_delayed_ref_head *head)
{
	struct rb_node *node;
	struct btrfs_delayed_ref_node *ref;
	int action = BTRFS_ADD_DELAYED_REF;
again:
	node = rb_prev(&head->node.rb_node);
	while (1) {
		if (!node)
			break;
		ref = rb_entry(node, struct btrfs_delayed_ref_node,
				rb_node);
		if (ref->bytenr != head->node.bytenr)
			break;
		if (ref->action == action)
			return ref;
		node = rb_prev(node);
	}
	if (action == BTRFS_ADD_DELAYED_REF) {
		action = BTRFS_DROP_DELAYED_REF;
		goto again;
	}
	return NULL;
}

static noinline int run_clustered_refs(struct btrfs_trans_handle *trans,
				       struct btrfs_root *root,
				       struct list_head *cluster)
{
	struct btrfs_delayed_ref_root *delayed_refs;
	struct btrfs_delayed_ref_node *ref;
	struct btrfs_delayed_ref_head *locked_ref = NULL;
	struct btrfs_delayed_extent_op *extent_op;
	int ret;
	int count = 0;
	int must_insert_reserved = 0;

	delayed_refs = &trans->transaction->delayed_refs;
	while (1) {
		if (!locked_ref) {
			
			if (list_empty(cluster))
				break;

			locked_ref = list_entry(cluster->next,
				     struct btrfs_delayed_ref_head, cluster);

			ret = btrfs_delayed_ref_lock(trans, locked_ref);

			if (ret == -EAGAIN) {
				locked_ref = NULL;
				count++;
				continue;
			}
		}

		ref = select_delayed_ref(locked_ref);

		if (ref && ref->seq &&
		    btrfs_check_delayed_seq(delayed_refs, ref->seq)) {
			list_del_init(&locked_ref->cluster);
			mutex_unlock(&locked_ref->mutex);
			locked_ref = NULL;
			delayed_refs->num_heads_ready++;
			spin_unlock(&delayed_refs->lock);
			cond_resched();
			spin_lock(&delayed_refs->lock);
			continue;
		}

		must_insert_reserved = locked_ref->must_insert_reserved;
		locked_ref->must_insert_reserved = 0;

		extent_op = locked_ref->extent_op;
		locked_ref->extent_op = NULL;

		if (!ref) {
			ref = &locked_ref->node;

			if (extent_op && must_insert_reserved) {
				kfree(extent_op);
				extent_op = NULL;
			}

			if (extent_op) {
				spin_unlock(&delayed_refs->lock);

				ret = run_delayed_extent_op(trans, root,
							    ref, extent_op);
				kfree(extent_op);

				if (ret) {
					printk(KERN_DEBUG "btrfs: run_delayed_extent_op returned %d\n", ret);
					spin_lock(&delayed_refs->lock);
					return ret;
				}

				goto next;
			}

			list_del_init(&locked_ref->cluster);
			locked_ref = NULL;
		}

		ref->in_tree = 0;
		rb_erase(&ref->rb_node, &delayed_refs->root);
		delayed_refs->num_entries--;
		spin_unlock(&delayed_refs->lock);

		ret = run_one_delayed_ref(trans, root, ref, extent_op,
					  must_insert_reserved);

		btrfs_put_delayed_ref(ref);
		kfree(extent_op);
		count++;

		if (ret) {
			printk(KERN_DEBUG "btrfs: run_one_delayed_ref returned %d\n", ret);
			spin_lock(&delayed_refs->lock);
			return ret;
		}

next:
		do_chunk_alloc(trans, root->fs_info->extent_root,
			       2 * 1024 * 1024,
			       btrfs_get_alloc_profile(root, 0),
			       CHUNK_ALLOC_NO_FORCE);
		cond_resched();
		spin_lock(&delayed_refs->lock);
	}
	return count;
}


static void wait_for_more_refs(struct btrfs_delayed_ref_root *delayed_refs,
			unsigned long num_refs)
{
	struct list_head *first_seq = delayed_refs->seq_head.next;

	spin_unlock(&delayed_refs->lock);
	pr_debug("waiting for more refs (num %ld, first %p)\n",
		 num_refs, first_seq);
	wait_event(delayed_refs->seq_wait,
		   num_refs != delayed_refs->num_entries ||
		   delayed_refs->seq_head.next != first_seq);
	pr_debug("done waiting for more refs (num %ld, first %p)\n",
		 delayed_refs->num_entries, delayed_refs->seq_head.next);
	spin_lock(&delayed_refs->lock);
}

int btrfs_run_delayed_refs(struct btrfs_trans_handle *trans,
			   struct btrfs_root *root, unsigned long count)
{
	struct rb_node *node;
	struct btrfs_delayed_ref_root *delayed_refs;
	struct btrfs_delayed_ref_node *ref;
	struct list_head cluster;
	int ret;
	u64 delayed_start;
	int run_all = count == (unsigned long)-1;
	int run_most = 0;
	unsigned long num_refs = 0;
	int consider_waiting;

	
	if (trans->aborted)
		return 0;

	if (root == root->fs_info->extent_root)
		root = root->fs_info->tree_root;

	do_chunk_alloc(trans, root->fs_info->extent_root,
		       2 * 1024 * 1024, btrfs_get_alloc_profile(root, 0),
		       CHUNK_ALLOC_NO_FORCE);

	delayed_refs = &trans->transaction->delayed_refs;
	INIT_LIST_HEAD(&cluster);
again:
	consider_waiting = 0;
	spin_lock(&delayed_refs->lock);
	if (count == 0) {
		count = delayed_refs->num_entries * 2;
		run_most = 1;
	}
	while (1) {
		if (!(run_all || run_most) &&
		    delayed_refs->num_heads_ready < 64)
			break;

		delayed_start = delayed_refs->run_delayed_start;
		ret = btrfs_find_ref_cluster(trans, &cluster,
					     delayed_refs->run_delayed_start);
		if (ret)
			break;

		if (delayed_start >= delayed_refs->run_delayed_start) {
			if (consider_waiting == 0) {
				consider_waiting = 1;
				num_refs = delayed_refs->num_entries;
			} else {
				wait_for_more_refs(delayed_refs, num_refs);
				consider_waiting = 0;
			}
		}

		ret = run_clustered_refs(trans, root, &cluster);
		if (ret < 0) {
			spin_unlock(&delayed_refs->lock);
			btrfs_abort_transaction(trans, root, ret);
			return ret;
		}

		count -= min_t(unsigned long, ret, count);

		if (count == 0)
			break;

		if (ret || delayed_refs->run_delayed_start == 0) {
			
			consider_waiting = 0;
		}
	}

	if (run_all) {
		node = rb_first(&delayed_refs->root);
		if (!node)
			goto out;
		count = (unsigned long)-1;

		while (node) {
			ref = rb_entry(node, struct btrfs_delayed_ref_node,
				       rb_node);
			if (btrfs_delayed_ref_is_head(ref)) {
				struct btrfs_delayed_ref_head *head;

				head = btrfs_delayed_node_to_head(ref);
				atomic_inc(&ref->refs);

				spin_unlock(&delayed_refs->lock);
				mutex_lock(&head->mutex);
				mutex_unlock(&head->mutex);

				btrfs_put_delayed_ref(ref);
				cond_resched();
				goto again;
			}
			node = rb_next(node);
		}
		spin_unlock(&delayed_refs->lock);
		schedule_timeout(1);
		goto again;
	}
out:
	spin_unlock(&delayed_refs->lock);
	return 0;
}

int btrfs_set_disk_extent_flags(struct btrfs_trans_handle *trans,
				struct btrfs_root *root,
				u64 bytenr, u64 num_bytes, u64 flags,
				int is_data)
{
	struct btrfs_delayed_extent_op *extent_op;
	int ret;

	extent_op = kmalloc(sizeof(*extent_op), GFP_NOFS);
	if (!extent_op)
		return -ENOMEM;

	extent_op->flags_to_set = flags;
	extent_op->update_flags = 1;
	extent_op->update_key = 0;
	extent_op->is_data = is_data ? 1 : 0;

	ret = btrfs_add_delayed_extent_op(root->fs_info, trans, bytenr,
					  num_bytes, extent_op);
	if (ret)
		kfree(extent_op);
	return ret;
}

static noinline int check_delayed_ref(struct btrfs_trans_handle *trans,
				      struct btrfs_root *root,
				      struct btrfs_path *path,
				      u64 objectid, u64 offset, u64 bytenr)
{
	struct btrfs_delayed_ref_head *head;
	struct btrfs_delayed_ref_node *ref;
	struct btrfs_delayed_data_ref *data_ref;
	struct btrfs_delayed_ref_root *delayed_refs;
	struct rb_node *node;
	int ret = 0;

	ret = -ENOENT;
	delayed_refs = &trans->transaction->delayed_refs;
	spin_lock(&delayed_refs->lock);
	head = btrfs_find_delayed_ref_head(trans, bytenr);
	if (!head)
		goto out;

	if (!mutex_trylock(&head->mutex)) {
		atomic_inc(&head->node.refs);
		spin_unlock(&delayed_refs->lock);

		btrfs_release_path(path);

		mutex_lock(&head->mutex);
		mutex_unlock(&head->mutex);
		btrfs_put_delayed_ref(&head->node);
		return -EAGAIN;
	}

	node = rb_prev(&head->node.rb_node);
	if (!node)
		goto out_unlock;

	ref = rb_entry(node, struct btrfs_delayed_ref_node, rb_node);

	if (ref->bytenr != bytenr)
		goto out_unlock;

	ret = 1;
	if (ref->type != BTRFS_EXTENT_DATA_REF_KEY)
		goto out_unlock;

	data_ref = btrfs_delayed_node_to_data_ref(ref);

	node = rb_prev(node);
	if (node) {
		ref = rb_entry(node, struct btrfs_delayed_ref_node, rb_node);
		if (ref->bytenr == bytenr)
			goto out_unlock;
	}

	if (data_ref->root != root->root_key.objectid ||
	    data_ref->objectid != objectid || data_ref->offset != offset)
		goto out_unlock;

	ret = 0;
out_unlock:
	mutex_unlock(&head->mutex);
out:
	spin_unlock(&delayed_refs->lock);
	return ret;
}

static noinline int check_committed_ref(struct btrfs_trans_handle *trans,
					struct btrfs_root *root,
					struct btrfs_path *path,
					u64 objectid, u64 offset, u64 bytenr)
{
	struct btrfs_root *extent_root = root->fs_info->extent_root;
	struct extent_buffer *leaf;
	struct btrfs_extent_data_ref *ref;
	struct btrfs_extent_inline_ref *iref;
	struct btrfs_extent_item *ei;
	struct btrfs_key key;
	u32 item_size;
	int ret;

	key.objectid = bytenr;
	key.offset = (u64)-1;
	key.type = BTRFS_EXTENT_ITEM_KEY;

	ret = btrfs_search_slot(NULL, extent_root, &key, path, 0, 0);
	if (ret < 0)
		goto out;
	BUG_ON(ret == 0); 

	ret = -ENOENT;
	if (path->slots[0] == 0)
		goto out;

	path->slots[0]--;
	leaf = path->nodes[0];
	btrfs_item_key_to_cpu(leaf, &key, path->slots[0]);

	if (key.objectid != bytenr || key.type != BTRFS_EXTENT_ITEM_KEY)
		goto out;

	ret = 1;
	item_size = btrfs_item_size_nr(leaf, path->slots[0]);
#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
	if (item_size < sizeof(*ei)) {
		WARN_ON(item_size != sizeof(struct btrfs_extent_item_v0));
		goto out;
	}
#endif
	ei = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_extent_item);

	if (item_size != sizeof(*ei) +
	    btrfs_extent_inline_ref_size(BTRFS_EXTENT_DATA_REF_KEY))
		goto out;

	if (btrfs_extent_generation(leaf, ei) <=
	    btrfs_root_last_snapshot(&root->root_item))
		goto out;

	iref = (struct btrfs_extent_inline_ref *)(ei + 1);
	if (btrfs_extent_inline_ref_type(leaf, iref) !=
	    BTRFS_EXTENT_DATA_REF_KEY)
		goto out;

	ref = (struct btrfs_extent_data_ref *)(&iref->offset);
	if (btrfs_extent_refs(leaf, ei) !=
	    btrfs_extent_data_ref_count(leaf, ref) ||
	    btrfs_extent_data_ref_root(leaf, ref) !=
	    root->root_key.objectid ||
	    btrfs_extent_data_ref_objectid(leaf, ref) != objectid ||
	    btrfs_extent_data_ref_offset(leaf, ref) != offset)
		goto out;

	ret = 0;
out:
	return ret;
}

int btrfs_cross_ref_exist(struct btrfs_trans_handle *trans,
			  struct btrfs_root *root,
			  u64 objectid, u64 offset, u64 bytenr)
{
	struct btrfs_path *path;
	int ret;
	int ret2;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOENT;

	do {
		ret = check_committed_ref(trans, root, path, objectid,
					  offset, bytenr);
		if (ret && ret != -ENOENT)
			goto out;

		ret2 = check_delayed_ref(trans, root, path, objectid,
					 offset, bytenr);
	} while (ret2 == -EAGAIN);

	if (ret2 && ret2 != -ENOENT) {
		ret = ret2;
		goto out;
	}

	if (ret != -ENOENT || ret2 != -ENOENT)
		ret = 0;
out:
	btrfs_free_path(path);
	if (root->root_key.objectid == BTRFS_DATA_RELOC_TREE_OBJECTID)
		WARN_ON(ret > 0);
	return ret;
}

static int __btrfs_mod_ref(struct btrfs_trans_handle *trans,
			   struct btrfs_root *root,
			   struct extent_buffer *buf,
			   int full_backref, int inc, int for_cow)
{
	u64 bytenr;
	u64 num_bytes;
	u64 parent;
	u64 ref_root;
	u32 nritems;
	struct btrfs_key key;
	struct btrfs_file_extent_item *fi;
	int i;
	int level;
	int ret = 0;
	int (*process_func)(struct btrfs_trans_handle *, struct btrfs_root *,
			    u64, u64, u64, u64, u64, u64, int);

	ref_root = btrfs_header_owner(buf);
	nritems = btrfs_header_nritems(buf);
	level = btrfs_header_level(buf);

	if (!root->ref_cows && level == 0)
		return 0;

	if (inc)
		process_func = btrfs_inc_extent_ref;
	else
		process_func = btrfs_free_extent;

	if (full_backref)
		parent = buf->start;
	else
		parent = 0;

	for (i = 0; i < nritems; i++) {
		if (level == 0) {
			btrfs_item_key_to_cpu(buf, &key, i);
			if (btrfs_key_type(&key) != BTRFS_EXTENT_DATA_KEY)
				continue;
			fi = btrfs_item_ptr(buf, i,
					    struct btrfs_file_extent_item);
			if (btrfs_file_extent_type(buf, fi) ==
			    BTRFS_FILE_EXTENT_INLINE)
				continue;
			bytenr = btrfs_file_extent_disk_bytenr(buf, fi);
			if (bytenr == 0)
				continue;

			num_bytes = btrfs_file_extent_disk_num_bytes(buf, fi);
			key.offset -= btrfs_file_extent_offset(buf, fi);
			ret = process_func(trans, root, bytenr, num_bytes,
					   parent, ref_root, key.objectid,
					   key.offset, for_cow);
			if (ret)
				goto fail;
		} else {
			bytenr = btrfs_node_blockptr(buf, i);
			num_bytes = btrfs_level_size(root, level - 1);
			ret = process_func(trans, root, bytenr, num_bytes,
					   parent, ref_root, level - 1, 0,
					   for_cow);
			if (ret)
				goto fail;
		}
	}
	return 0;
fail:
	return ret;
}

int btrfs_inc_ref(struct btrfs_trans_handle *trans, struct btrfs_root *root,
		  struct extent_buffer *buf, int full_backref, int for_cow)
{
	return __btrfs_mod_ref(trans, root, buf, full_backref, 1, for_cow);
}

int btrfs_dec_ref(struct btrfs_trans_handle *trans, struct btrfs_root *root,
		  struct extent_buffer *buf, int full_backref, int for_cow)
{
	return __btrfs_mod_ref(trans, root, buf, full_backref, 0, for_cow);
}

static int write_one_cache_group(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root,
				 struct btrfs_path *path,
				 struct btrfs_block_group_cache *cache)
{
	int ret;
	struct btrfs_root *extent_root = root->fs_info->extent_root;
	unsigned long bi;
	struct extent_buffer *leaf;

	ret = btrfs_search_slot(trans, extent_root, &cache->key, path, 0, 1);
	if (ret < 0)
		goto fail;
	BUG_ON(ret); 

	leaf = path->nodes[0];
	bi = btrfs_item_ptr_offset(leaf, path->slots[0]);
	write_extent_buffer(leaf, &cache->item, bi, sizeof(cache->item));
	btrfs_mark_buffer_dirty(leaf);
	btrfs_release_path(path);
fail:
	if (ret) {
		btrfs_abort_transaction(trans, root, ret);
		return ret;
	}
	return 0;

}

static struct btrfs_block_group_cache *
next_block_group(struct btrfs_root *root,
		 struct btrfs_block_group_cache *cache)
{
	struct rb_node *node;
	spin_lock(&root->fs_info->block_group_cache_lock);
	node = rb_next(&cache->cache_node);
	btrfs_put_block_group(cache);
	if (node) {
		cache = rb_entry(node, struct btrfs_block_group_cache,
				 cache_node);
		btrfs_get_block_group(cache);
	} else
		cache = NULL;
	spin_unlock(&root->fs_info->block_group_cache_lock);
	return cache;
}

static int cache_save_setup(struct btrfs_block_group_cache *block_group,
			    struct btrfs_trans_handle *trans,
			    struct btrfs_path *path)
{
	struct btrfs_root *root = block_group->fs_info->tree_root;
	struct inode *inode = NULL;
	u64 alloc_hint = 0;
	int dcs = BTRFS_DC_ERROR;
	int num_pages = 0;
	int retries = 0;
	int ret = 0;

	if (block_group->key.offset < (100 * 1024 * 1024)) {
		spin_lock(&block_group->lock);
		block_group->disk_cache_state = BTRFS_DC_WRITTEN;
		spin_unlock(&block_group->lock);
		return 0;
	}

again:
	inode = lookup_free_space_inode(root, block_group, path);
	if (IS_ERR(inode) && PTR_ERR(inode) != -ENOENT) {
		ret = PTR_ERR(inode);
		btrfs_release_path(path);
		goto out;
	}

	if (IS_ERR(inode)) {
		BUG_ON(retries);
		retries++;

		if (block_group->ro)
			goto out_free;

		ret = create_free_space_inode(root, trans, block_group, path);
		if (ret)
			goto out_free;
		goto again;
	}

	
	if (block_group->cache_generation == trans->transid &&
	    i_size_read(inode)) {
		dcs = BTRFS_DC_SETUP;
		goto out_put;
	}

	BTRFS_I(inode)->generation = 0;
	ret = btrfs_update_inode(trans, root, inode);
	WARN_ON(ret);

	if (i_size_read(inode) > 0) {
		ret = btrfs_truncate_free_space_cache(root, trans, path,
						      inode);
		if (ret)
			goto out_put;
	}

	spin_lock(&block_group->lock);
	if (block_group->cached != BTRFS_CACHE_FINISHED) {
		
		dcs = BTRFS_DC_WRITTEN;
		spin_unlock(&block_group->lock);
		goto out_put;
	}
	spin_unlock(&block_group->lock);

	num_pages = (int)div64_u64(block_group->key.offset, 1024 * 1024 * 1024);
	if (!num_pages)
		num_pages = 1;

	num_pages *= 16;
	num_pages *= PAGE_CACHE_SIZE;

	ret = btrfs_check_data_free_space(inode, num_pages);
	if (ret)
		goto out_put;

	ret = btrfs_prealloc_file_range_trans(inode, trans, 0, 0, num_pages,
					      num_pages, num_pages,
					      &alloc_hint);
	if (!ret)
		dcs = BTRFS_DC_SETUP;
	btrfs_free_reserved_data_space(inode, num_pages);

out_put:
	iput(inode);
out_free:
	btrfs_release_path(path);
out:
	spin_lock(&block_group->lock);
	if (!ret && dcs == BTRFS_DC_SETUP)
		block_group->cache_generation = trans->transid;
	block_group->disk_cache_state = dcs;
	spin_unlock(&block_group->lock);

	return ret;
}

int btrfs_write_dirty_block_groups(struct btrfs_trans_handle *trans,
				   struct btrfs_root *root)
{
	struct btrfs_block_group_cache *cache;
	int err = 0;
	struct btrfs_path *path;
	u64 last = 0;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

again:
	while (1) {
		cache = btrfs_lookup_first_block_group(root->fs_info, last);
		while (cache) {
			if (cache->disk_cache_state == BTRFS_DC_CLEAR)
				break;
			cache = next_block_group(root, cache);
		}
		if (!cache) {
			if (last == 0)
				break;
			last = 0;
			continue;
		}
		err = cache_save_setup(cache, trans, path);
		last = cache->key.objectid + cache->key.offset;
		btrfs_put_block_group(cache);
	}

	while (1) {
		if (last == 0) {
			err = btrfs_run_delayed_refs(trans, root,
						     (unsigned long)-1);
			if (err) 
				goto out;
		}

		cache = btrfs_lookup_first_block_group(root->fs_info, last);
		while (cache) {
			if (cache->disk_cache_state == BTRFS_DC_CLEAR) {
				btrfs_put_block_group(cache);
				goto again;
			}

			if (cache->dirty)
				break;
			cache = next_block_group(root, cache);
		}
		if (!cache) {
			if (last == 0)
				break;
			last = 0;
			continue;
		}

		if (cache->disk_cache_state == BTRFS_DC_SETUP)
			cache->disk_cache_state = BTRFS_DC_NEED_WRITE;
		cache->dirty = 0;
		last = cache->key.objectid + cache->key.offset;

		err = write_one_cache_group(trans, root, path, cache);
		if (err) 
			goto out;

		btrfs_put_block_group(cache);
	}

	while (1) {
		/*
		 * I don't think this is needed since we're just marking our
		 * preallocated extent as written, but just in case it can't
		 * hurt.
		 */
		if (last == 0) {
			err = btrfs_run_delayed_refs(trans, root,
						     (unsigned long)-1);
			if (err) 
				goto out;
		}

		cache = btrfs_lookup_first_block_group(root->fs_info, last);
		while (cache) {
			if (cache->dirty) {
				btrfs_put_block_group(cache);
				goto again;
			}
			if (cache->disk_cache_state == BTRFS_DC_NEED_WRITE)
				break;
			cache = next_block_group(root, cache);
		}
		if (!cache) {
			if (last == 0)
				break;
			last = 0;
			continue;
		}

		err = btrfs_write_out_cache(root, trans, cache, path);

		/*
		 * If we didn't have an error then the cache state is still
		 * NEED_WRITE, so we can set it to WRITTEN.
		 */
		if (!err && cache->disk_cache_state == BTRFS_DC_NEED_WRITE)
			cache->disk_cache_state = BTRFS_DC_WRITTEN;
		last = cache->key.objectid + cache->key.offset;
		btrfs_put_block_group(cache);
	}
out:

	btrfs_free_path(path);
	return err;
}

int btrfs_extent_readonly(struct btrfs_root *root, u64 bytenr)
{
	struct btrfs_block_group_cache *block_group;
	int readonly = 0;

	block_group = btrfs_lookup_block_group(root->fs_info, bytenr);
	if (!block_group || block_group->ro)
		readonly = 1;
	if (block_group)
		btrfs_put_block_group(block_group);
	return readonly;
}

static int update_space_info(struct btrfs_fs_info *info, u64 flags,
			     u64 total_bytes, u64 bytes_used,
			     struct btrfs_space_info **space_info)
{
	struct btrfs_space_info *found;
	int i;
	int factor;

	if (flags & (BTRFS_BLOCK_GROUP_DUP | BTRFS_BLOCK_GROUP_RAID1 |
		     BTRFS_BLOCK_GROUP_RAID10))
		factor = 2;
	else
		factor = 1;

	found = __find_space_info(info, flags);
	if (found) {
		spin_lock(&found->lock);
		found->total_bytes += total_bytes;
		found->disk_total += total_bytes * factor;
		found->bytes_used += bytes_used;
		found->disk_used += bytes_used * factor;
		found->full = 0;
		spin_unlock(&found->lock);
		*space_info = found;
		return 0;
	}
	found = kzalloc(sizeof(*found), GFP_NOFS);
	if (!found)
		return -ENOMEM;

	for (i = 0; i < BTRFS_NR_RAID_TYPES; i++)
		INIT_LIST_HEAD(&found->block_groups[i]);
	init_rwsem(&found->groups_sem);
	spin_lock_init(&found->lock);
	found->flags = flags & BTRFS_BLOCK_GROUP_TYPE_MASK;
	found->total_bytes = total_bytes;
	found->disk_total = total_bytes * factor;
	found->bytes_used = bytes_used;
	found->disk_used = bytes_used * factor;
	found->bytes_pinned = 0;
	found->bytes_reserved = 0;
	found->bytes_readonly = 0;
	found->bytes_may_use = 0;
	found->full = 0;
	found->force_alloc = CHUNK_ALLOC_NO_FORCE;
	found->chunk_alloc = 0;
	found->flush = 0;
	init_waitqueue_head(&found->wait);
	*space_info = found;
	list_add_rcu(&found->list, &info->space_info);
	return 0;
}

static void set_avail_alloc_bits(struct btrfs_fs_info *fs_info, u64 flags)
{
	u64 extra_flags = chunk_to_extended(flags) &
				BTRFS_EXTENDED_PROFILE_MASK;

	if (flags & BTRFS_BLOCK_GROUP_DATA)
		fs_info->avail_data_alloc_bits |= extra_flags;
	if (flags & BTRFS_BLOCK_GROUP_METADATA)
		fs_info->avail_metadata_alloc_bits |= extra_flags;
	if (flags & BTRFS_BLOCK_GROUP_SYSTEM)
		fs_info->avail_system_alloc_bits |= extra_flags;
}

static u64 get_restripe_target(struct btrfs_fs_info *fs_info, u64 flags)
{
	struct btrfs_balance_control *bctl = fs_info->balance_ctl;
	u64 target = 0;

	if (!bctl)
		return 0;

	if (flags & BTRFS_BLOCK_GROUP_DATA &&
	    bctl->data.flags & BTRFS_BALANCE_ARGS_CONVERT) {
		target = BTRFS_BLOCK_GROUP_DATA | bctl->data.target;
	} else if (flags & BTRFS_BLOCK_GROUP_SYSTEM &&
		   bctl->sys.flags & BTRFS_BALANCE_ARGS_CONVERT) {
		target = BTRFS_BLOCK_GROUP_SYSTEM | bctl->sys.target;
	} else if (flags & BTRFS_BLOCK_GROUP_METADATA &&
		   bctl->meta.flags & BTRFS_BALANCE_ARGS_CONVERT) {
		target = BTRFS_BLOCK_GROUP_METADATA | bctl->meta.target;
	}

	return target;
}

u64 btrfs_reduce_alloc_profile(struct btrfs_root *root, u64 flags)
{
	u64 num_devices = root->fs_info->fs_devices->rw_devices +
		root->fs_info->fs_devices->missing_devices;
	u64 target;

	spin_lock(&root->fs_info->balance_lock);
	target = get_restripe_target(root->fs_info, flags);
	if (target) {
		
		if ((flags & target) & BTRFS_EXTENDED_PROFILE_MASK) {
			spin_unlock(&root->fs_info->balance_lock);
			return extended_to_chunk(target);
		}
	}
	spin_unlock(&root->fs_info->balance_lock);

	if (num_devices == 1)
		flags &= ~(BTRFS_BLOCK_GROUP_RAID1 | BTRFS_BLOCK_GROUP_RAID0);
	if (num_devices < 4)
		flags &= ~BTRFS_BLOCK_GROUP_RAID10;

	if ((flags & BTRFS_BLOCK_GROUP_DUP) &&
	    (flags & (BTRFS_BLOCK_GROUP_RAID1 |
		      BTRFS_BLOCK_GROUP_RAID10))) {
		flags &= ~BTRFS_BLOCK_GROUP_DUP;
	}

	if ((flags & BTRFS_BLOCK_GROUP_RAID1) &&
	    (flags & BTRFS_BLOCK_GROUP_RAID10)) {
		flags &= ~BTRFS_BLOCK_GROUP_RAID1;
	}

	if ((flags & BTRFS_BLOCK_GROUP_RAID0) &&
	    ((flags & BTRFS_BLOCK_GROUP_RAID1) |
	     (flags & BTRFS_BLOCK_GROUP_RAID10) |
	     (flags & BTRFS_BLOCK_GROUP_DUP))) {
		flags &= ~BTRFS_BLOCK_GROUP_RAID0;
	}

	return extended_to_chunk(flags);
}

static u64 get_alloc_profile(struct btrfs_root *root, u64 flags)
{
	if (flags & BTRFS_BLOCK_GROUP_DATA)
		flags |= root->fs_info->avail_data_alloc_bits;
	else if (flags & BTRFS_BLOCK_GROUP_SYSTEM)
		flags |= root->fs_info->avail_system_alloc_bits;
	else if (flags & BTRFS_BLOCK_GROUP_METADATA)
		flags |= root->fs_info->avail_metadata_alloc_bits;

	return btrfs_reduce_alloc_profile(root, flags);
}

u64 btrfs_get_alloc_profile(struct btrfs_root *root, int data)
{
	u64 flags;

	if (data)
		flags = BTRFS_BLOCK_GROUP_DATA;
	else if (root == root->fs_info->chunk_root)
		flags = BTRFS_BLOCK_GROUP_SYSTEM;
	else
		flags = BTRFS_BLOCK_GROUP_METADATA;

	return get_alloc_profile(root, flags);
}

void btrfs_set_inode_space_info(struct btrfs_root *root, struct inode *inode)
{
	BTRFS_I(inode)->space_info = __find_space_info(root->fs_info,
						       BTRFS_BLOCK_GROUP_DATA);
}

int btrfs_check_data_free_space(struct inode *inode, u64 bytes)
{
	struct btrfs_space_info *data_sinfo;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	u64 used;
	int ret = 0, committed = 0, alloc_chunk = 1;

	
	bytes = (bytes + root->sectorsize - 1) & ~((u64)root->sectorsize - 1);

	if (root == root->fs_info->tree_root ||
	    BTRFS_I(inode)->location.objectid == BTRFS_FREE_INO_OBJECTID) {
		alloc_chunk = 0;
		committed = 1;
	}

	data_sinfo = BTRFS_I(inode)->space_info;
	if (!data_sinfo)
		goto alloc;

again:
	
	spin_lock(&data_sinfo->lock);
	used = data_sinfo->bytes_used + data_sinfo->bytes_reserved +
		data_sinfo->bytes_pinned + data_sinfo->bytes_readonly +
		data_sinfo->bytes_may_use;

	if (used + bytes > data_sinfo->total_bytes) {
		struct btrfs_trans_handle *trans;

		if (!data_sinfo->full && alloc_chunk) {
			u64 alloc_target;

			data_sinfo->force_alloc = CHUNK_ALLOC_FORCE;
			spin_unlock(&data_sinfo->lock);
alloc:
			alloc_target = btrfs_get_alloc_profile(root, 1);
			trans = btrfs_join_transaction(root);
			if (IS_ERR(trans))
				return PTR_ERR(trans);

			ret = do_chunk_alloc(trans, root->fs_info->extent_root,
					     bytes + 2 * 1024 * 1024,
					     alloc_target,
					     CHUNK_ALLOC_NO_FORCE);
			btrfs_end_transaction(trans, root);
			if (ret < 0) {
				if (ret != -ENOSPC)
					return ret;
				else
					goto commit_trans;
			}

			if (!data_sinfo) {
				btrfs_set_inode_space_info(root, inode);
				data_sinfo = BTRFS_I(inode)->space_info;
			}
			goto again;
		}

		if (data_sinfo->bytes_pinned < bytes)
			committed = 1;
		spin_unlock(&data_sinfo->lock);

		
commit_trans:
		if (!committed &&
		    !atomic_read(&root->fs_info->open_ioctl_trans)) {
			committed = 1;
			trans = btrfs_join_transaction(root);
			if (IS_ERR(trans))
				return PTR_ERR(trans);
			ret = btrfs_commit_transaction(trans, root);
			if (ret)
				return ret;
			goto again;
		}

		return -ENOSPC;
	}
	data_sinfo->bytes_may_use += bytes;
	trace_btrfs_space_reservation(root->fs_info, "space_info",
				      data_sinfo->flags, bytes, 1);
	spin_unlock(&data_sinfo->lock);

	return 0;
}

void btrfs_free_reserved_data_space(struct inode *inode, u64 bytes)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_space_info *data_sinfo;

	
	bytes = (bytes + root->sectorsize - 1) & ~((u64)root->sectorsize - 1);

	data_sinfo = BTRFS_I(inode)->space_info;
	spin_lock(&data_sinfo->lock);
	data_sinfo->bytes_may_use -= bytes;
	trace_btrfs_space_reservation(root->fs_info, "space_info",
				      data_sinfo->flags, bytes, 0);
	spin_unlock(&data_sinfo->lock);
}

static void force_metadata_allocation(struct btrfs_fs_info *info)
{
	struct list_head *head = &info->space_info;
	struct btrfs_space_info *found;

	rcu_read_lock();
	list_for_each_entry_rcu(found, head, list) {
		if (found->flags & BTRFS_BLOCK_GROUP_METADATA)
			found->force_alloc = CHUNK_ALLOC_FORCE;
	}
	rcu_read_unlock();
}

static int should_alloc_chunk(struct btrfs_root *root,
			      struct btrfs_space_info *sinfo, u64 alloc_bytes,
			      int force)
{
	struct btrfs_block_rsv *global_rsv = &root->fs_info->global_block_rsv;
	u64 num_bytes = sinfo->total_bytes - sinfo->bytes_readonly;
	u64 num_allocated = sinfo->bytes_used + sinfo->bytes_reserved;
	u64 thresh;

	if (force == CHUNK_ALLOC_FORCE)
		return 1;

	num_allocated += global_rsv->size;

	if (force == CHUNK_ALLOC_LIMITED) {
		thresh = btrfs_super_total_bytes(root->fs_info->super_copy);
		thresh = max_t(u64, 64 * 1024 * 1024,
			       div_factor_fine(thresh, 1));

		if (num_bytes - num_allocated < thresh)
			return 1;
	}
	thresh = btrfs_super_total_bytes(root->fs_info->super_copy);

	
	thresh = max_t(u64, 256 * 1024 * 1024, div_factor_fine(thresh, 2));
	
	if (sinfo->flags & BTRFS_BLOCK_GROUP_SYSTEM)
		thresh = 32 * 1024 * 1024;

	if (num_bytes > thresh && sinfo->bytes_used < div_factor(num_bytes, 8))
		return 0;
	return 1;
}

static u64 get_system_chunk_thresh(struct btrfs_root *root, u64 type)
{
	u64 num_dev;

	if (type & BTRFS_BLOCK_GROUP_RAID10 ||
	    type & BTRFS_BLOCK_GROUP_RAID0)
		num_dev = root->fs_info->fs_devices->rw_devices;
	else if (type & BTRFS_BLOCK_GROUP_RAID1)
		num_dev = 2;
	else
		num_dev = 1;	

	
	return btrfs_calc_trans_metadata_size(root, num_dev + 1);
}

static void check_system_chunk(struct btrfs_trans_handle *trans,
			       struct btrfs_root *root, u64 type)
{
	struct btrfs_space_info *info;
	u64 left;
	u64 thresh;

	info = __find_space_info(root->fs_info, BTRFS_BLOCK_GROUP_SYSTEM);
	spin_lock(&info->lock);
	left = info->total_bytes - info->bytes_used - info->bytes_pinned -
		info->bytes_reserved - info->bytes_readonly;
	spin_unlock(&info->lock);

	thresh = get_system_chunk_thresh(root, type);
	if (left < thresh && btrfs_test_opt(root, ENOSPC_DEBUG)) {
		printk(KERN_INFO "left=%llu, need=%llu, flags=%llu\n",
		       left, thresh, type);
		dump_space_info(info, 0, 0);
	}

	if (left < thresh) {
		u64 flags;

		flags = btrfs_get_alloc_profile(root->fs_info->chunk_root, 0);
		btrfs_alloc_chunk(trans, root, flags);
	}
}

static int do_chunk_alloc(struct btrfs_trans_handle *trans,
			  struct btrfs_root *extent_root, u64 alloc_bytes,
			  u64 flags, int force)
{
	struct btrfs_space_info *space_info;
	struct btrfs_fs_info *fs_info = extent_root->fs_info;
	int wait_for_alloc = 0;
	int ret = 0;

	space_info = __find_space_info(extent_root->fs_info, flags);
	if (!space_info) {
		ret = update_space_info(extent_root->fs_info, flags,
					0, 0, &space_info);
		BUG_ON(ret); 
	}
	BUG_ON(!space_info); 

again:
	spin_lock(&space_info->lock);
	if (force < space_info->force_alloc)
		force = space_info->force_alloc;
	if (space_info->full) {
		spin_unlock(&space_info->lock);
		return 0;
	}

	if (!should_alloc_chunk(extent_root, space_info, alloc_bytes, force)) {
		spin_unlock(&space_info->lock);
		return 0;
	} else if (space_info->chunk_alloc) {
		wait_for_alloc = 1;
	} else {
		space_info->chunk_alloc = 1;
	}

	spin_unlock(&space_info->lock);

	mutex_lock(&fs_info->chunk_mutex);

	if (wait_for_alloc) {
		mutex_unlock(&fs_info->chunk_mutex);
		wait_for_alloc = 0;
		goto again;
	}

	if (btrfs_mixed_space_info(space_info))
		flags |= (BTRFS_BLOCK_GROUP_DATA | BTRFS_BLOCK_GROUP_METADATA);

	if (flags & BTRFS_BLOCK_GROUP_DATA && fs_info->metadata_ratio) {
		fs_info->data_chunk_allocations++;
		if (!(fs_info->data_chunk_allocations %
		      fs_info->metadata_ratio))
			force_metadata_allocation(fs_info);
	}

	check_system_chunk(trans, extent_root, flags);

	ret = btrfs_alloc_chunk(trans, extent_root, flags);
	if (ret < 0 && ret != -ENOSPC)
		goto out;

	spin_lock(&space_info->lock);
	if (ret)
		space_info->full = 1;
	else
		ret = 1;

	space_info->force_alloc = CHUNK_ALLOC_NO_FORCE;
	space_info->chunk_alloc = 0;
	spin_unlock(&space_info->lock);
out:
	mutex_unlock(&extent_root->fs_info->chunk_mutex);
	return ret;
}

static int shrink_delalloc(struct btrfs_root *root, u64 to_reclaim,
			   bool wait_ordered)
{
	struct btrfs_block_rsv *block_rsv;
	struct btrfs_space_info *space_info;
	struct btrfs_trans_handle *trans;
	u64 reserved;
	u64 max_reclaim;
	u64 reclaimed = 0;
	long time_left;
	unsigned long nr_pages = (2 * 1024 * 1024) >> PAGE_CACHE_SHIFT;
	int loops = 0;
	unsigned long progress;

	trans = (struct btrfs_trans_handle *)current->journal_info;
	block_rsv = &root->fs_info->delalloc_block_rsv;
	space_info = block_rsv->space_info;

	smp_mb();
	reserved = space_info->bytes_may_use;
	progress = space_info->reservation_progress;

	if (reserved == 0)
		return 0;

	smp_mb();
	if (root->fs_info->delalloc_bytes == 0) {
		if (trans)
			return 0;
		btrfs_wait_ordered_extents(root, 0, 0);
		return 0;
	}

	max_reclaim = min(reserved, to_reclaim);
	nr_pages = max_t(unsigned long, nr_pages,
			 max_reclaim >> PAGE_CACHE_SHIFT);
	while (loops < 1024) {
		
		smp_mb();
		nr_pages = min_t(unsigned long, nr_pages,
		       root->fs_info->delalloc_bytes >> PAGE_CACHE_SHIFT);
		writeback_inodes_sb_nr_if_idle(root->fs_info->sb, nr_pages,
						WB_REASON_FS_FREE_SPACE);

		spin_lock(&space_info->lock);
		if (reserved > space_info->bytes_may_use)
			reclaimed += reserved - space_info->bytes_may_use;
		reserved = space_info->bytes_may_use;
		spin_unlock(&space_info->lock);

		loops++;

		if (reserved == 0 || reclaimed >= max_reclaim)
			break;

		if (trans && trans->transaction->blocked)
			return -EAGAIN;

		if (wait_ordered && !trans) {
			btrfs_wait_ordered_extents(root, 0, 0);
		} else {
			time_left = schedule_timeout_interruptible(1);

			
			if (time_left)
				break;
		}


		if (loops > 3) {
			smp_mb();
			if (progress != space_info->reservation_progress)
				break;
		}

	}

	return reclaimed >= to_reclaim;
}

static int may_commit_transaction(struct btrfs_root *root,
				  struct btrfs_space_info *space_info,
				  u64 bytes, int force)
{
	struct btrfs_block_rsv *delayed_rsv = &root->fs_info->delayed_block_rsv;
	struct btrfs_trans_handle *trans;

	trans = (struct btrfs_trans_handle *)current->journal_info;
	if (trans)
		return -EAGAIN;

	if (force)
		goto commit;

	
	spin_lock(&space_info->lock);
	if (space_info->bytes_pinned >= bytes) {
		spin_unlock(&space_info->lock);
		goto commit;
	}
	spin_unlock(&space_info->lock);

	if (space_info != delayed_rsv->space_info)
		return -ENOSPC;

	spin_lock(&space_info->lock);
	spin_lock(&delayed_rsv->lock);
	if (space_info->bytes_pinned + delayed_rsv->size < bytes) {
		spin_unlock(&delayed_rsv->lock);
		spin_unlock(&space_info->lock);
		return -ENOSPC;
	}
	spin_unlock(&delayed_rsv->lock);
	spin_unlock(&space_info->lock);

commit:
	trans = btrfs_join_transaction(root);
	if (IS_ERR(trans))
		return -ENOSPC;

	return btrfs_commit_transaction(trans, root);
}

static int reserve_metadata_bytes(struct btrfs_root *root,
				  struct btrfs_block_rsv *block_rsv,
				  u64 orig_bytes, int flush)
{
	struct btrfs_space_info *space_info = block_rsv->space_info;
	u64 used;
	u64 num_bytes = orig_bytes;
	int retries = 0;
	int ret = 0;
	bool committed = false;
	bool flushing = false;
	bool wait_ordered = false;

again:
	ret = 0;
	spin_lock(&space_info->lock);
	while (flush && !flushing && space_info->flush) {
		spin_unlock(&space_info->lock);
		if (current->journal_info)
			return -EAGAIN;
		ret = wait_event_killable(space_info->wait, !space_info->flush);
		
		if (ret)
			return -EINTR;

		spin_lock(&space_info->lock);
	}

	ret = -ENOSPC;
	used = space_info->bytes_used + space_info->bytes_reserved +
		space_info->bytes_pinned + space_info->bytes_readonly +
		space_info->bytes_may_use;

	if (used <= space_info->total_bytes) {
		if (used + orig_bytes <= space_info->total_bytes) {
			space_info->bytes_may_use += orig_bytes;
			trace_btrfs_space_reservation(root->fs_info,
				"space_info", space_info->flags, orig_bytes, 1);
			ret = 0;
		} else {
			num_bytes = orig_bytes;
		}
	} else {
		wait_ordered = true;
		num_bytes = used - space_info->total_bytes +
			(orig_bytes * (retries + 1));
	}

	if (ret) {
		u64 profile = btrfs_get_alloc_profile(root, 0);
		u64 avail;

		avail = (space_info->total_bytes - space_info->bytes_used) * 8;
		do_div(avail, 10);
		if (space_info->bytes_pinned >= avail && flush && !committed) {
			space_info->flush = 1;
			flushing = true;
			spin_unlock(&space_info->lock);
			ret = may_commit_transaction(root, space_info,
						     orig_bytes, 1);
			if (ret)
				goto out;
			committed = true;
			goto again;
		}

		spin_lock(&root->fs_info->free_chunk_lock);
		avail = root->fs_info->free_chunk_space;

		if (profile & (BTRFS_BLOCK_GROUP_DUP |
			       BTRFS_BLOCK_GROUP_RAID1 |
			       BTRFS_BLOCK_GROUP_RAID10))
			avail >>= 1;

		if (flush)
			avail >>= 3;
		else
			avail >>= 1;
		 spin_unlock(&root->fs_info->free_chunk_lock);

		if (used + num_bytes < space_info->total_bytes + avail) {
			space_info->bytes_may_use += orig_bytes;
			trace_btrfs_space_reservation(root->fs_info,
				"space_info", space_info->flags, orig_bytes, 1);
			ret = 0;
		} else {
			wait_ordered = true;
		}
	}

	if (ret && flush) {
		flushing = true;
		space_info->flush = 1;
	}

	spin_unlock(&space_info->lock);

	if (!ret || !flush)
		goto out;

	ret = shrink_delalloc(root, num_bytes, wait_ordered);
	if (ret < 0)
		goto out;

	ret = 0;

	if (retries < 2) {
		wait_ordered = true;
		retries++;
		goto again;
	}

	ret = -ENOSPC;
	if (committed)
		goto out;

	ret = may_commit_transaction(root, space_info, orig_bytes, 0);
	if (!ret) {
		committed = true;
		goto again;
	}

out:
	if (flushing) {
		spin_lock(&space_info->lock);
		space_info->flush = 0;
		wake_up_all(&space_info->wait);
		spin_unlock(&space_info->lock);
	}
	return ret;
}

static struct btrfs_block_rsv *get_block_rsv(
					const struct btrfs_trans_handle *trans,
					const struct btrfs_root *root)
{
	struct btrfs_block_rsv *block_rsv = NULL;

	if (root->ref_cows || root == root->fs_info->csum_root)
		block_rsv = trans->block_rsv;

	if (!block_rsv)
		block_rsv = root->block_rsv;

	if (!block_rsv)
		block_rsv = &root->fs_info->empty_block_rsv;

	return block_rsv;
}

static int block_rsv_use_bytes(struct btrfs_block_rsv *block_rsv,
			       u64 num_bytes)
{
	int ret = -ENOSPC;
	spin_lock(&block_rsv->lock);
	if (block_rsv->reserved >= num_bytes) {
		block_rsv->reserved -= num_bytes;
		if (block_rsv->reserved < block_rsv->size)
			block_rsv->full = 0;
		ret = 0;
	}
	spin_unlock(&block_rsv->lock);
	return ret;
}

static void block_rsv_add_bytes(struct btrfs_block_rsv *block_rsv,
				u64 num_bytes, int update_size)
{
	spin_lock(&block_rsv->lock);
	block_rsv->reserved += num_bytes;
	if (update_size)
		block_rsv->size += num_bytes;
	else if (block_rsv->reserved >= block_rsv->size)
		block_rsv->full = 1;
	spin_unlock(&block_rsv->lock);
}

static void block_rsv_release_bytes(struct btrfs_fs_info *fs_info,
				    struct btrfs_block_rsv *block_rsv,
				    struct btrfs_block_rsv *dest, u64 num_bytes)
{
	struct btrfs_space_info *space_info = block_rsv->space_info;

	spin_lock(&block_rsv->lock);
	if (num_bytes == (u64)-1)
		num_bytes = block_rsv->size;
	block_rsv->size -= num_bytes;
	if (block_rsv->reserved >= block_rsv->size) {
		num_bytes = block_rsv->reserved - block_rsv->size;
		block_rsv->reserved = block_rsv->size;
		block_rsv->full = 1;
	} else {
		num_bytes = 0;
	}
	spin_unlock(&block_rsv->lock);

	if (num_bytes > 0) {
		if (dest) {
			spin_lock(&dest->lock);
			if (!dest->full) {
				u64 bytes_to_add;

				bytes_to_add = dest->size - dest->reserved;
				bytes_to_add = min(num_bytes, bytes_to_add);
				dest->reserved += bytes_to_add;
				if (dest->reserved >= dest->size)
					dest->full = 1;
				num_bytes -= bytes_to_add;
			}
			spin_unlock(&dest->lock);
		}
		if (num_bytes) {
			spin_lock(&space_info->lock);
			space_info->bytes_may_use -= num_bytes;
			trace_btrfs_space_reservation(fs_info, "space_info",
					space_info->flags, num_bytes, 0);
			space_info->reservation_progress++;
			spin_unlock(&space_info->lock);
		}
	}
}

static int block_rsv_migrate_bytes(struct btrfs_block_rsv *src,
				   struct btrfs_block_rsv *dst, u64 num_bytes)
{
	int ret;

	ret = block_rsv_use_bytes(src, num_bytes);
	if (ret)
		return ret;

	block_rsv_add_bytes(dst, num_bytes, 1);
	return 0;
}

void btrfs_init_block_rsv(struct btrfs_block_rsv *rsv)
{
	memset(rsv, 0, sizeof(*rsv));
	spin_lock_init(&rsv->lock);
}

struct btrfs_block_rsv *btrfs_alloc_block_rsv(struct btrfs_root *root)
{
	struct btrfs_block_rsv *block_rsv;
	struct btrfs_fs_info *fs_info = root->fs_info;

	block_rsv = kmalloc(sizeof(*block_rsv), GFP_NOFS);
	if (!block_rsv)
		return NULL;

	btrfs_init_block_rsv(block_rsv);
	block_rsv->space_info = __find_space_info(fs_info,
						  BTRFS_BLOCK_GROUP_METADATA);
	return block_rsv;
}

void btrfs_free_block_rsv(struct btrfs_root *root,
			  struct btrfs_block_rsv *rsv)
{
	btrfs_block_rsv_release(root, rsv, (u64)-1);
	kfree(rsv);
}

static inline int __block_rsv_add(struct btrfs_root *root,
				  struct btrfs_block_rsv *block_rsv,
				  u64 num_bytes, int flush)
{
	int ret;

	if (num_bytes == 0)
		return 0;

	ret = reserve_metadata_bytes(root, block_rsv, num_bytes, flush);
	if (!ret) {
		block_rsv_add_bytes(block_rsv, num_bytes, 1);
		return 0;
	}

	return ret;
}

int btrfs_block_rsv_add(struct btrfs_root *root,
			struct btrfs_block_rsv *block_rsv,
			u64 num_bytes)
{
	return __block_rsv_add(root, block_rsv, num_bytes, 1);
}

int btrfs_block_rsv_add_noflush(struct btrfs_root *root,
				struct btrfs_block_rsv *block_rsv,
				u64 num_bytes)
{
	return __block_rsv_add(root, block_rsv, num_bytes, 0);
}

int btrfs_block_rsv_check(struct btrfs_root *root,
			  struct btrfs_block_rsv *block_rsv, int min_factor)
{
	u64 num_bytes = 0;
	int ret = -ENOSPC;

	if (!block_rsv)
		return 0;

	spin_lock(&block_rsv->lock);
	num_bytes = div_factor(block_rsv->size, min_factor);
	if (block_rsv->reserved >= num_bytes)
		ret = 0;
	spin_unlock(&block_rsv->lock);

	return ret;
}

static inline int __btrfs_block_rsv_refill(struct btrfs_root *root,
					   struct btrfs_block_rsv *block_rsv,
					   u64 min_reserved, int flush)
{
	u64 num_bytes = 0;
	int ret = -ENOSPC;

	if (!block_rsv)
		return 0;

	spin_lock(&block_rsv->lock);
	num_bytes = min_reserved;
	if (block_rsv->reserved >= num_bytes)
		ret = 0;
	else
		num_bytes -= block_rsv->reserved;
	spin_unlock(&block_rsv->lock);

	if (!ret)
		return 0;

	ret = reserve_metadata_bytes(root, block_rsv, num_bytes, flush);
	if (!ret) {
		block_rsv_add_bytes(block_rsv, num_bytes, 0);
		return 0;
	}

	return ret;
}

int btrfs_block_rsv_refill(struct btrfs_root *root,
			   struct btrfs_block_rsv *block_rsv,
			   u64 min_reserved)
{
	return __btrfs_block_rsv_refill(root, block_rsv, min_reserved, 1);
}

int btrfs_block_rsv_refill_noflush(struct btrfs_root *root,
				   struct btrfs_block_rsv *block_rsv,
				   u64 min_reserved)
{
	return __btrfs_block_rsv_refill(root, block_rsv, min_reserved, 0);
}

int btrfs_block_rsv_migrate(struct btrfs_block_rsv *src_rsv,
			    struct btrfs_block_rsv *dst_rsv,
			    u64 num_bytes)
{
	return block_rsv_migrate_bytes(src_rsv, dst_rsv, num_bytes);
}

void btrfs_block_rsv_release(struct btrfs_root *root,
			     struct btrfs_block_rsv *block_rsv,
			     u64 num_bytes)
{
	struct btrfs_block_rsv *global_rsv = &root->fs_info->global_block_rsv;
	if (global_rsv->full || global_rsv == block_rsv ||
	    block_rsv->space_info != global_rsv->space_info)
		global_rsv = NULL;
	block_rsv_release_bytes(root->fs_info, block_rsv, global_rsv,
				num_bytes);
}

static u64 calc_global_metadata_size(struct btrfs_fs_info *fs_info)
{
	struct btrfs_space_info *sinfo;
	u64 num_bytes;
	u64 meta_used;
	u64 data_used;
	int csum_size = btrfs_super_csum_size(fs_info->super_copy);

	sinfo = __find_space_info(fs_info, BTRFS_BLOCK_GROUP_DATA);
	spin_lock(&sinfo->lock);
	data_used = sinfo->bytes_used;
	spin_unlock(&sinfo->lock);

	sinfo = __find_space_info(fs_info, BTRFS_BLOCK_GROUP_METADATA);
	spin_lock(&sinfo->lock);
	if (sinfo->flags & BTRFS_BLOCK_GROUP_DATA)
		data_used = 0;
	meta_used = sinfo->bytes_used;
	spin_unlock(&sinfo->lock);

	num_bytes = (data_used >> fs_info->sb->s_blocksize_bits) *
		    csum_size * 2;
	num_bytes += div64_u64(data_used + meta_used, 50);

	if (num_bytes * 3 > meta_used)
		num_bytes = div64_u64(meta_used, 3);

	return ALIGN(num_bytes, fs_info->extent_root->leafsize << 10);
}

static void update_global_block_rsv(struct btrfs_fs_info *fs_info)
{
	struct btrfs_block_rsv *block_rsv = &fs_info->global_block_rsv;
	struct btrfs_space_info *sinfo = block_rsv->space_info;
	u64 num_bytes;

	num_bytes = calc_global_metadata_size(fs_info);

	spin_lock(&sinfo->lock);
	spin_lock(&block_rsv->lock);

	block_rsv->size = num_bytes;

	num_bytes = sinfo->bytes_used + sinfo->bytes_pinned +
		    sinfo->bytes_reserved + sinfo->bytes_readonly +
		    sinfo->bytes_may_use;

	if (sinfo->total_bytes > num_bytes) {
		num_bytes = sinfo->total_bytes - num_bytes;
		block_rsv->reserved += num_bytes;
		sinfo->bytes_may_use += num_bytes;
		trace_btrfs_space_reservation(fs_info, "space_info",
				      sinfo->flags, num_bytes, 1);
	}

	if (block_rsv->reserved >= block_rsv->size) {
		num_bytes = block_rsv->reserved - block_rsv->size;
		sinfo->bytes_may_use -= num_bytes;
		trace_btrfs_space_reservation(fs_info, "space_info",
				      sinfo->flags, num_bytes, 0);
		sinfo->reservation_progress++;
		block_rsv->reserved = block_rsv->size;
		block_rsv->full = 1;
	}

	spin_unlock(&block_rsv->lock);
	spin_unlock(&sinfo->lock);
}

static void init_global_block_rsv(struct btrfs_fs_info *fs_info)
{
	struct btrfs_space_info *space_info;

	space_info = __find_space_info(fs_info, BTRFS_BLOCK_GROUP_SYSTEM);
	fs_info->chunk_block_rsv.space_info = space_info;

	space_info = __find_space_info(fs_info, BTRFS_BLOCK_GROUP_METADATA);
	fs_info->global_block_rsv.space_info = space_info;
	fs_info->delalloc_block_rsv.space_info = space_info;
	fs_info->trans_block_rsv.space_info = space_info;
	fs_info->empty_block_rsv.space_info = space_info;
	fs_info->delayed_block_rsv.space_info = space_info;

	fs_info->extent_root->block_rsv = &fs_info->global_block_rsv;
	fs_info->csum_root->block_rsv = &fs_info->global_block_rsv;
	fs_info->dev_root->block_rsv = &fs_info->global_block_rsv;
	fs_info->tree_root->block_rsv = &fs_info->global_block_rsv;
	fs_info->chunk_root->block_rsv = &fs_info->chunk_block_rsv;

	update_global_block_rsv(fs_info);
}

static void release_global_block_rsv(struct btrfs_fs_info *fs_info)
{
	block_rsv_release_bytes(fs_info, &fs_info->global_block_rsv, NULL,
				(u64)-1);
	WARN_ON(fs_info->delalloc_block_rsv.size > 0);
	WARN_ON(fs_info->delalloc_block_rsv.reserved > 0);
	WARN_ON(fs_info->trans_block_rsv.size > 0);
	WARN_ON(fs_info->trans_block_rsv.reserved > 0);
	WARN_ON(fs_info->chunk_block_rsv.size > 0);
	WARN_ON(fs_info->chunk_block_rsv.reserved > 0);
	WARN_ON(fs_info->delayed_block_rsv.size > 0);
	WARN_ON(fs_info->delayed_block_rsv.reserved > 0);
}

void btrfs_trans_release_metadata(struct btrfs_trans_handle *trans,
				  struct btrfs_root *root)
{
	if (!trans->bytes_reserved)
		return;

	trace_btrfs_space_reservation(root->fs_info, "transaction",
				      trans->transid, trans->bytes_reserved, 0);
	btrfs_block_rsv_release(root, trans->block_rsv, trans->bytes_reserved);
	trans->bytes_reserved = 0;
}

int btrfs_orphan_reserve_metadata(struct btrfs_trans_handle *trans,
				  struct inode *inode)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_block_rsv *src_rsv = get_block_rsv(trans, root);
	struct btrfs_block_rsv *dst_rsv = root->orphan_block_rsv;

	u64 num_bytes = btrfs_calc_trans_metadata_size(root, 1);
	trace_btrfs_space_reservation(root->fs_info, "orphan",
				      btrfs_ino(inode), num_bytes, 1);
	return block_rsv_migrate_bytes(src_rsv, dst_rsv, num_bytes);
}

void btrfs_orphan_release_metadata(struct inode *inode)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	u64 num_bytes = btrfs_calc_trans_metadata_size(root, 1);
	trace_btrfs_space_reservation(root->fs_info, "orphan",
				      btrfs_ino(inode), num_bytes, 0);
	btrfs_block_rsv_release(root, root->orphan_block_rsv, num_bytes);
}

int btrfs_snap_reserve_metadata(struct btrfs_trans_handle *trans,
				struct btrfs_pending_snapshot *pending)
{
	struct btrfs_root *root = pending->root;
	struct btrfs_block_rsv *src_rsv = get_block_rsv(trans, root);
	struct btrfs_block_rsv *dst_rsv = &pending->block_rsv;
	u64 num_bytes = btrfs_calc_trans_metadata_size(root, 5);
	dst_rsv->space_info = src_rsv->space_info;
	return block_rsv_migrate_bytes(src_rsv, dst_rsv, num_bytes);
}

/**
 * drop_outstanding_extent - drop an outstanding extent
 * @inode: the inode we're dropping the extent for
 *
 * This is called when we are freeing up an outstanding extent, either called
 * after an error or after an extent is written.  This will return the number of
 * reserved extents that need to be freed.  This must be called with
 * BTRFS_I(inode)->lock held.
 */
static unsigned drop_outstanding_extent(struct inode *inode)
{
	unsigned drop_inode_space = 0;
	unsigned dropped_extents = 0;

	BUG_ON(!BTRFS_I(inode)->outstanding_extents);
	BTRFS_I(inode)->outstanding_extents--;

	if (BTRFS_I(inode)->outstanding_extents == 0 &&
	    BTRFS_I(inode)->delalloc_meta_reserved) {
		drop_inode_space = 1;
		BTRFS_I(inode)->delalloc_meta_reserved = 0;
	}

	if (BTRFS_I(inode)->outstanding_extents >=
	    BTRFS_I(inode)->reserved_extents)
		return drop_inode_space;

	dropped_extents = BTRFS_I(inode)->reserved_extents -
		BTRFS_I(inode)->outstanding_extents;
	BTRFS_I(inode)->reserved_extents -= dropped_extents;
	return dropped_extents + drop_inode_space;
}

static u64 calc_csum_metadata_size(struct inode *inode, u64 num_bytes,
				   int reserve)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	u64 csum_size;
	int num_csums_per_leaf;
	int num_csums;
	int old_csums;

	if (BTRFS_I(inode)->flags & BTRFS_INODE_NODATASUM &&
	    BTRFS_I(inode)->csum_bytes == 0)
		return 0;

	old_csums = (int)div64_u64(BTRFS_I(inode)->csum_bytes, root->sectorsize);
	if (reserve)
		BTRFS_I(inode)->csum_bytes += num_bytes;
	else
		BTRFS_I(inode)->csum_bytes -= num_bytes;
	csum_size = BTRFS_LEAF_DATA_SIZE(root) - sizeof(struct btrfs_item);
	num_csums_per_leaf = (int)div64_u64(csum_size,
					    sizeof(struct btrfs_csum_item) +
					    sizeof(struct btrfs_disk_key));
	num_csums = (int)div64_u64(BTRFS_I(inode)->csum_bytes, root->sectorsize);
	num_csums = num_csums + num_csums_per_leaf - 1;
	num_csums = num_csums / num_csums_per_leaf;

	old_csums = old_csums + num_csums_per_leaf - 1;
	old_csums = old_csums / num_csums_per_leaf;

	
	if (old_csums == num_csums)
		return 0;

	if (reserve)
		return btrfs_calc_trans_metadata_size(root,
						      num_csums - old_csums);

	return btrfs_calc_trans_metadata_size(root, old_csums - num_csums);
}

int btrfs_delalloc_reserve_metadata(struct inode *inode, u64 num_bytes)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_block_rsv *block_rsv = &root->fs_info->delalloc_block_rsv;
	u64 to_reserve = 0;
	u64 csum_bytes;
	unsigned nr_extents = 0;
	int extra_reserve = 0;
	int flush = 1;
	int ret;

	
	if (btrfs_is_free_space_inode(root, inode))
		flush = 0;

	if (flush && btrfs_transaction_in_commit(root->fs_info))
		schedule_timeout(1);

	mutex_lock(&BTRFS_I(inode)->delalloc_mutex);
	num_bytes = ALIGN(num_bytes, root->sectorsize);

	spin_lock(&BTRFS_I(inode)->lock);
	BTRFS_I(inode)->outstanding_extents++;

	if (BTRFS_I(inode)->outstanding_extents >
	    BTRFS_I(inode)->reserved_extents)
		nr_extents = BTRFS_I(inode)->outstanding_extents -
			BTRFS_I(inode)->reserved_extents;

	if (!BTRFS_I(inode)->delalloc_meta_reserved) {
		nr_extents++;
		extra_reserve = 1;
	}

	to_reserve = btrfs_calc_trans_metadata_size(root, nr_extents);
	to_reserve += calc_csum_metadata_size(inode, num_bytes, 1);
	csum_bytes = BTRFS_I(inode)->csum_bytes;
	spin_unlock(&BTRFS_I(inode)->lock);

	ret = reserve_metadata_bytes(root, block_rsv, to_reserve, flush);
	if (ret) {
		u64 to_free = 0;
		unsigned dropped;

		spin_lock(&BTRFS_I(inode)->lock);
		dropped = drop_outstanding_extent(inode);
		if (BTRFS_I(inode)->csum_bytes == csum_bytes)
			calc_csum_metadata_size(inode, num_bytes, 0);
		else
			to_free = calc_csum_metadata_size(inode, num_bytes, 0);
		spin_unlock(&BTRFS_I(inode)->lock);
		if (dropped)
			to_free += btrfs_calc_trans_metadata_size(root, dropped);

		if (to_free) {
			btrfs_block_rsv_release(root, block_rsv, to_free);
			trace_btrfs_space_reservation(root->fs_info,
						      "delalloc",
						      btrfs_ino(inode),
						      to_free, 0);
		}
		mutex_unlock(&BTRFS_I(inode)->delalloc_mutex);
		return ret;
	}

	spin_lock(&BTRFS_I(inode)->lock);
	if (extra_reserve) {
		BTRFS_I(inode)->delalloc_meta_reserved = 1;
		nr_extents--;
	}
	BTRFS_I(inode)->reserved_extents += nr_extents;
	spin_unlock(&BTRFS_I(inode)->lock);
	mutex_unlock(&BTRFS_I(inode)->delalloc_mutex);

	if (to_reserve)
		trace_btrfs_space_reservation(root->fs_info,"delalloc",
					      btrfs_ino(inode), to_reserve, 1);
	block_rsv_add_bytes(block_rsv, to_reserve, 1);

	return 0;
}

void btrfs_delalloc_release_metadata(struct inode *inode, u64 num_bytes)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	u64 to_free = 0;
	unsigned dropped;

	num_bytes = ALIGN(num_bytes, root->sectorsize);
	spin_lock(&BTRFS_I(inode)->lock);
	dropped = drop_outstanding_extent(inode);

	to_free = calc_csum_metadata_size(inode, num_bytes, 0);
	spin_unlock(&BTRFS_I(inode)->lock);
	if (dropped > 0)
		to_free += btrfs_calc_trans_metadata_size(root, dropped);

	trace_btrfs_space_reservation(root->fs_info, "delalloc",
				      btrfs_ino(inode), to_free, 0);
	btrfs_block_rsv_release(root, &root->fs_info->delalloc_block_rsv,
				to_free);
}

int btrfs_delalloc_reserve_space(struct inode *inode, u64 num_bytes)
{
	int ret;

	ret = btrfs_check_data_free_space(inode, num_bytes);
	if (ret)
		return ret;

	ret = btrfs_delalloc_reserve_metadata(inode, num_bytes);
	if (ret) {
		btrfs_free_reserved_data_space(inode, num_bytes);
		return ret;
	}

	return 0;
}

void btrfs_delalloc_release_space(struct inode *inode, u64 num_bytes)
{
	btrfs_delalloc_release_metadata(inode, num_bytes);
	btrfs_free_reserved_data_space(inode, num_bytes);
}

static int update_block_group(struct btrfs_trans_handle *trans,
			      struct btrfs_root *root,
			      u64 bytenr, u64 num_bytes, int alloc)
{
	struct btrfs_block_group_cache *cache = NULL;
	struct btrfs_fs_info *info = root->fs_info;
	u64 total = num_bytes;
	u64 old_val;
	u64 byte_in_group;
	int factor;

	
	spin_lock(&info->delalloc_lock);
	old_val = btrfs_super_bytes_used(info->super_copy);
	if (alloc)
		old_val += num_bytes;
	else
		old_val -= num_bytes;
	btrfs_set_super_bytes_used(info->super_copy, old_val);
	spin_unlock(&info->delalloc_lock);

	while (total) {
		cache = btrfs_lookup_block_group(info, bytenr);
		if (!cache)
			return -ENOENT;
		if (cache->flags & (BTRFS_BLOCK_GROUP_DUP |
				    BTRFS_BLOCK_GROUP_RAID1 |
				    BTRFS_BLOCK_GROUP_RAID10))
			factor = 2;
		else
			factor = 1;
		/*
		 * If this block group has free space cache written out, we
		 * need to make sure to load it if we are removing space.  This
		 * is because we need the unpinning stage to actually add the
		 * space back to the block group, otherwise we will leak space.
		 */
		if (!alloc && cache->cached == BTRFS_CACHE_NO)
			cache_block_group(cache, trans, NULL, 1);

		byte_in_group = bytenr - cache->key.objectid;
		WARN_ON(byte_in_group > cache->key.offset);

		spin_lock(&cache->space_info->lock);
		spin_lock(&cache->lock);

		if (btrfs_test_opt(root, SPACE_CACHE) &&
		    cache->disk_cache_state < BTRFS_DC_CLEAR)
			cache->disk_cache_state = BTRFS_DC_CLEAR;

		cache->dirty = 1;
		old_val = btrfs_block_group_used(&cache->item);
		num_bytes = min(total, cache->key.offset - byte_in_group);
		if (alloc) {
			old_val += num_bytes;
			btrfs_set_block_group_used(&cache->item, old_val);
			cache->reserved -= num_bytes;
			cache->space_info->bytes_reserved -= num_bytes;
			cache->space_info->bytes_used += num_bytes;
			cache->space_info->disk_used += num_bytes * factor;
			spin_unlock(&cache->lock);
			spin_unlock(&cache->space_info->lock);
		} else {
			old_val -= num_bytes;
			btrfs_set_block_group_used(&cache->item, old_val);
			cache->pinned += num_bytes;
			cache->space_info->bytes_pinned += num_bytes;
			cache->space_info->bytes_used -= num_bytes;
			cache->space_info->disk_used -= num_bytes * factor;
			spin_unlock(&cache->lock);
			spin_unlock(&cache->space_info->lock);

			set_extent_dirty(info->pinned_extents,
					 bytenr, bytenr + num_bytes - 1,
					 GFP_NOFS | __GFP_NOFAIL);
		}
		btrfs_put_block_group(cache);
		total -= num_bytes;
		bytenr += num_bytes;
	}
	return 0;
}

static u64 first_logical_byte(struct btrfs_root *root, u64 search_start)
{
	struct btrfs_block_group_cache *cache;
	u64 bytenr;

	cache = btrfs_lookup_first_block_group(root->fs_info, search_start);
	if (!cache)
		return 0;

	bytenr = cache->key.objectid;
	btrfs_put_block_group(cache);

	return bytenr;
}

static int pin_down_extent(struct btrfs_root *root,
			   struct btrfs_block_group_cache *cache,
			   u64 bytenr, u64 num_bytes, int reserved)
{
	spin_lock(&cache->space_info->lock);
	spin_lock(&cache->lock);
	cache->pinned += num_bytes;
	cache->space_info->bytes_pinned += num_bytes;
	if (reserved) {
		cache->reserved -= num_bytes;
		cache->space_info->bytes_reserved -= num_bytes;
	}
	spin_unlock(&cache->lock);
	spin_unlock(&cache->space_info->lock);

	set_extent_dirty(root->fs_info->pinned_extents, bytenr,
			 bytenr + num_bytes - 1, GFP_NOFS | __GFP_NOFAIL);
	return 0;
}

int btrfs_pin_extent(struct btrfs_root *root,
		     u64 bytenr, u64 num_bytes, int reserved)
{
	struct btrfs_block_group_cache *cache;

	cache = btrfs_lookup_block_group(root->fs_info, bytenr);
	BUG_ON(!cache); 

	pin_down_extent(root, cache, bytenr, num_bytes, reserved);

	btrfs_put_block_group(cache);
	return 0;
}

int btrfs_pin_extent_for_log_replay(struct btrfs_trans_handle *trans,
				    struct btrfs_root *root,
				    u64 bytenr, u64 num_bytes)
{
	struct btrfs_block_group_cache *cache;

	cache = btrfs_lookup_block_group(root->fs_info, bytenr);
	BUG_ON(!cache); 

	cache_block_group(cache, trans, root, 1);

	pin_down_extent(root, cache, bytenr, num_bytes, 0);

	
	btrfs_remove_free_space(cache, bytenr, num_bytes);
	btrfs_put_block_group(cache);
	return 0;
}

static int btrfs_update_reserved_bytes(struct btrfs_block_group_cache *cache,
				       u64 num_bytes, int reserve)
{
	struct btrfs_space_info *space_info = cache->space_info;
	int ret = 0;

	spin_lock(&space_info->lock);
	spin_lock(&cache->lock);
	if (reserve != RESERVE_FREE) {
		if (cache->ro) {
			ret = -EAGAIN;
		} else {
			cache->reserved += num_bytes;
			space_info->bytes_reserved += num_bytes;
			if (reserve == RESERVE_ALLOC) {
				trace_btrfs_space_reservation(cache->fs_info,
						"space_info", space_info->flags,
						num_bytes, 0);
				space_info->bytes_may_use -= num_bytes;
			}
		}
	} else {
		if (cache->ro)
			space_info->bytes_readonly += num_bytes;
		cache->reserved -= num_bytes;
		space_info->bytes_reserved -= num_bytes;
		space_info->reservation_progress++;
	}
	spin_unlock(&cache->lock);
	spin_unlock(&space_info->lock);
	return ret;
}

void btrfs_prepare_extent_commit(struct btrfs_trans_handle *trans,
				struct btrfs_root *root)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_caching_control *next;
	struct btrfs_caching_control *caching_ctl;
	struct btrfs_block_group_cache *cache;

	down_write(&fs_info->extent_commit_sem);

	list_for_each_entry_safe(caching_ctl, next,
				 &fs_info->caching_block_groups, list) {
		cache = caching_ctl->block_group;
		if (block_group_cache_done(cache)) {
			cache->last_byte_to_unpin = (u64)-1;
			list_del_init(&caching_ctl->list);
			put_caching_control(caching_ctl);
		} else {
			cache->last_byte_to_unpin = caching_ctl->progress;
		}
	}

	if (fs_info->pinned_extents == &fs_info->freed_extents[0])
		fs_info->pinned_extents = &fs_info->freed_extents[1];
	else
		fs_info->pinned_extents = &fs_info->freed_extents[0];

	up_write(&fs_info->extent_commit_sem);

	update_global_block_rsv(fs_info);
}

static int unpin_extent_range(struct btrfs_root *root, u64 start, u64 end)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_block_group_cache *cache = NULL;
	u64 len;

	while (start <= end) {
		if (!cache ||
		    start >= cache->key.objectid + cache->key.offset) {
			if (cache)
				btrfs_put_block_group(cache);
			cache = btrfs_lookup_block_group(fs_info, start);
			BUG_ON(!cache); 
		}

		len = cache->key.objectid + cache->key.offset - start;
		len = min(len, end + 1 - start);

		if (start < cache->last_byte_to_unpin) {
			len = min(len, cache->last_byte_to_unpin - start);
			btrfs_add_free_space(cache, start, len);
		}

		start += len;

		spin_lock(&cache->space_info->lock);
		spin_lock(&cache->lock);
		cache->pinned -= len;
		cache->space_info->bytes_pinned -= len;
		if (cache->ro)
			cache->space_info->bytes_readonly += len;
		spin_unlock(&cache->lock);
		spin_unlock(&cache->space_info->lock);
	}

	if (cache)
		btrfs_put_block_group(cache);
	return 0;
}

int btrfs_finish_extent_commit(struct btrfs_trans_handle *trans,
			       struct btrfs_root *root)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct extent_io_tree *unpin;
	u64 start;
	u64 end;
	int ret;

	if (trans->aborted)
		return 0;

	if (fs_info->pinned_extents == &fs_info->freed_extents[0])
		unpin = &fs_info->freed_extents[1];
	else
		unpin = &fs_info->freed_extents[0];

	while (1) {
		ret = find_first_extent_bit(unpin, 0, &start, &end,
					    EXTENT_DIRTY);
		if (ret)
			break;

		if (btrfs_test_opt(root, DISCARD))
			ret = btrfs_discard_extent(root, start,
						   end + 1 - start, NULL);

		clear_extent_dirty(unpin, start, end, GFP_NOFS);
		unpin_extent_range(root, start, end);
		cond_resched();
	}

	return 0;
}

static int __btrfs_free_extent(struct btrfs_trans_handle *trans,
				struct btrfs_root *root,
				u64 bytenr, u64 num_bytes, u64 parent,
				u64 root_objectid, u64 owner_objectid,
				u64 owner_offset, int refs_to_drop,
				struct btrfs_delayed_extent_op *extent_op)
{
	struct btrfs_key key;
	struct btrfs_path *path;
	struct btrfs_fs_info *info = root->fs_info;
	struct btrfs_root *extent_root = info->extent_root;
	struct extent_buffer *leaf;
	struct btrfs_extent_item *ei;
	struct btrfs_extent_inline_ref *iref;
	int ret;
	int is_data;
	int extent_slot = 0;
	int found_extent = 0;
	int num_to_del = 1;
	u32 item_size;
	u64 refs;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	path->reada = 1;
	path->leave_spinning = 1;

	is_data = owner_objectid >= BTRFS_FIRST_FREE_OBJECTID;
	BUG_ON(!is_data && refs_to_drop != 1);

	ret = lookup_extent_backref(trans, extent_root, path, &iref,
				    bytenr, num_bytes, parent,
				    root_objectid, owner_objectid,
				    owner_offset);
	if (ret == 0) {
		extent_slot = path->slots[0];
		while (extent_slot >= 0) {
			btrfs_item_key_to_cpu(path->nodes[0], &key,
					      extent_slot);
			if (key.objectid != bytenr)
				break;
			if (key.type == BTRFS_EXTENT_ITEM_KEY &&
			    key.offset == num_bytes) {
				found_extent = 1;
				break;
			}
			if (path->slots[0] - extent_slot > 5)
				break;
			extent_slot--;
		}
#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
		item_size = btrfs_item_size_nr(path->nodes[0], extent_slot);
		if (found_extent && item_size < sizeof(*ei))
			found_extent = 0;
#endif
		if (!found_extent) {
			BUG_ON(iref);
			ret = remove_extent_backref(trans, extent_root, path,
						    NULL, refs_to_drop,
						    is_data);
			if (ret)
				goto abort;
			btrfs_release_path(path);
			path->leave_spinning = 1;

			key.objectid = bytenr;
			key.type = BTRFS_EXTENT_ITEM_KEY;
			key.offset = num_bytes;

			ret = btrfs_search_slot(trans, extent_root,
						&key, path, -1, 1);
			if (ret) {
				printk(KERN_ERR "umm, got %d back from search"
				       ", was looking for %llu\n", ret,
				       (unsigned long long)bytenr);
				if (ret > 0)
					btrfs_print_leaf(extent_root,
							 path->nodes[0]);
			}
			if (ret < 0)
				goto abort;
			extent_slot = path->slots[0];
		}
	} else if (ret == -ENOENT) {
		btrfs_print_leaf(extent_root, path->nodes[0]);
		WARN_ON(1);
		printk(KERN_ERR "btrfs unable to find ref byte nr %llu "
		       "parent %llu root %llu  owner %llu offset %llu\n",
		       (unsigned long long)bytenr,
		       (unsigned long long)parent,
		       (unsigned long long)root_objectid,
		       (unsigned long long)owner_objectid,
		       (unsigned long long)owner_offset);
	} else {
		goto abort;
	}

	leaf = path->nodes[0];
	item_size = btrfs_item_size_nr(leaf, extent_slot);
#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
	if (item_size < sizeof(*ei)) {
		BUG_ON(found_extent || extent_slot != path->slots[0]);
		ret = convert_extent_item_v0(trans, extent_root, path,
					     owner_objectid, 0);
		if (ret < 0)
			goto abort;

		btrfs_release_path(path);
		path->leave_spinning = 1;

		key.objectid = bytenr;
		key.type = BTRFS_EXTENT_ITEM_KEY;
		key.offset = num_bytes;

		ret = btrfs_search_slot(trans, extent_root, &key, path,
					-1, 1);
		if (ret) {
			printk(KERN_ERR "umm, got %d back from search"
			       ", was looking for %llu\n", ret,
			       (unsigned long long)bytenr);
			btrfs_print_leaf(extent_root, path->nodes[0]);
		}
		if (ret < 0)
			goto abort;
		extent_slot = path->slots[0];
		leaf = path->nodes[0];
		item_size = btrfs_item_size_nr(leaf, extent_slot);
	}
#endif
	BUG_ON(item_size < sizeof(*ei));
	ei = btrfs_item_ptr(leaf, extent_slot,
			    struct btrfs_extent_item);
	if (owner_objectid < BTRFS_FIRST_FREE_OBJECTID) {
		struct btrfs_tree_block_info *bi;
		BUG_ON(item_size < sizeof(*ei) + sizeof(*bi));
		bi = (struct btrfs_tree_block_info *)(ei + 1);
		WARN_ON(owner_objectid != btrfs_tree_block_level(leaf, bi));
	}

	refs = btrfs_extent_refs(leaf, ei);
	BUG_ON(refs < refs_to_drop);
	refs -= refs_to_drop;

	if (refs > 0) {
		if (extent_op)
			__run_delayed_extent_op(extent_op, leaf, ei);
		if (iref) {
			BUG_ON(!found_extent);
		} else {
			btrfs_set_extent_refs(leaf, ei, refs);
			btrfs_mark_buffer_dirty(leaf);
		}
		if (found_extent) {
			ret = remove_extent_backref(trans, extent_root, path,
						    iref, refs_to_drop,
						    is_data);
			if (ret)
				goto abort;
		}
	} else {
		if (found_extent) {
			BUG_ON(is_data && refs_to_drop !=
			       extent_data_ref_count(root, path, iref));
			if (iref) {
				BUG_ON(path->slots[0] != extent_slot);
			} else {
				BUG_ON(path->slots[0] != extent_slot + 1);
				path->slots[0] = extent_slot;
				num_to_del = 2;
			}
		}

		ret = btrfs_del_items(trans, extent_root, path, path->slots[0],
				      num_to_del);
		if (ret)
			goto abort;
		btrfs_release_path(path);

		if (is_data) {
			ret = btrfs_del_csums(trans, root, bytenr, num_bytes);
			if (ret)
				goto abort;
		}

		ret = update_block_group(trans, root, bytenr, num_bytes, 0);
		if (ret)
			goto abort;
	}
out:
	btrfs_free_path(path);
	return ret;

abort:
	btrfs_abort_transaction(trans, extent_root, ret);
	goto out;
}

static noinline int check_ref_cleanup(struct btrfs_trans_handle *trans,
				      struct btrfs_root *root, u64 bytenr)
{
	struct btrfs_delayed_ref_head *head;
	struct btrfs_delayed_ref_root *delayed_refs;
	struct btrfs_delayed_ref_node *ref;
	struct rb_node *node;
	int ret = 0;

	delayed_refs = &trans->transaction->delayed_refs;
	spin_lock(&delayed_refs->lock);
	head = btrfs_find_delayed_ref_head(trans, bytenr);
	if (!head)
		goto out;

	node = rb_prev(&head->node.rb_node);
	if (!node)
		goto out;

	ref = rb_entry(node, struct btrfs_delayed_ref_node, rb_node);

	
	if (ref->bytenr == bytenr)
		goto out;

	if (head->extent_op) {
		if (!head->must_insert_reserved)
			goto out;
		kfree(head->extent_op);
		head->extent_op = NULL;
	}

	if (!mutex_trylock(&head->mutex))
		goto out;

	head->node.in_tree = 0;
	rb_erase(&head->node.rb_node, &delayed_refs->root);

	delayed_refs->num_entries--;
	if (waitqueue_active(&delayed_refs->seq_wait))
		wake_up(&delayed_refs->seq_wait);

	delayed_refs->num_heads--;
	if (list_empty(&head->cluster))
		delayed_refs->num_heads_ready--;

	list_del_init(&head->cluster);
	spin_unlock(&delayed_refs->lock);

	BUG_ON(head->extent_op);
	if (head->must_insert_reserved)
		ret = 1;

	mutex_unlock(&head->mutex);
	btrfs_put_delayed_ref(&head->node);
	return ret;
out:
	spin_unlock(&delayed_refs->lock);
	return 0;
}

void btrfs_free_tree_block(struct btrfs_trans_handle *trans,
			   struct btrfs_root *root,
			   struct extent_buffer *buf,
			   u64 parent, int last_ref, int for_cow)
{
	struct btrfs_block_group_cache *cache = NULL;
	int ret;

	if (root->root_key.objectid != BTRFS_TREE_LOG_OBJECTID) {
		ret = btrfs_add_delayed_tree_ref(root->fs_info, trans,
					buf->start, buf->len,
					parent, root->root_key.objectid,
					btrfs_header_level(buf),
					BTRFS_DROP_DELAYED_REF, NULL, for_cow);
		BUG_ON(ret); 
	}

	if (!last_ref)
		return;

	cache = btrfs_lookup_block_group(root->fs_info, buf->start);

	if (btrfs_header_generation(buf) == trans->transid) {
		if (root->root_key.objectid != BTRFS_TREE_LOG_OBJECTID) {
			ret = check_ref_cleanup(trans, root, buf->start);
			if (!ret)
				goto out;
		}

		if (btrfs_header_flag(buf, BTRFS_HEADER_FLAG_WRITTEN)) {
			pin_down_extent(root, cache, buf->start, buf->len, 1);
			goto out;
		}

		WARN_ON(test_bit(EXTENT_BUFFER_DIRTY, &buf->bflags));

		btrfs_add_free_space(cache, buf->start, buf->len);
		btrfs_update_reserved_bytes(cache, buf->len, RESERVE_FREE);
	}
out:
	clear_bit(EXTENT_BUFFER_CORRUPT, &buf->bflags);
	btrfs_put_block_group(cache);
}

int btrfs_free_extent(struct btrfs_trans_handle *trans, struct btrfs_root *root,
		      u64 bytenr, u64 num_bytes, u64 parent, u64 root_objectid,
		      u64 owner, u64 offset, int for_cow)
{
	int ret;
	struct btrfs_fs_info *fs_info = root->fs_info;

	if (root_objectid == BTRFS_TREE_LOG_OBJECTID) {
		WARN_ON(owner >= BTRFS_FIRST_FREE_OBJECTID);
		
		btrfs_pin_extent(root, bytenr, num_bytes, 1);
		ret = 0;
	} else if (owner < BTRFS_FIRST_FREE_OBJECTID) {
		ret = btrfs_add_delayed_tree_ref(fs_info, trans, bytenr,
					num_bytes,
					parent, root_objectid, (int)owner,
					BTRFS_DROP_DELAYED_REF, NULL, for_cow);
	} else {
		ret = btrfs_add_delayed_data_ref(fs_info, trans, bytenr,
						num_bytes,
						parent, root_objectid, owner,
						offset, BTRFS_DROP_DELAYED_REF,
						NULL, for_cow);
	}
	return ret;
}

static u64 stripe_align(struct btrfs_root *root, u64 val)
{
	u64 mask = ((u64)root->stripesize - 1);
	u64 ret = (val + mask) & ~mask;
	return ret;
}

static noinline int
wait_block_group_cache_progress(struct btrfs_block_group_cache *cache,
				u64 num_bytes)
{
	struct btrfs_caching_control *caching_ctl;
	DEFINE_WAIT(wait);

	caching_ctl = get_caching_control(cache);
	if (!caching_ctl)
		return 0;

	wait_event(caching_ctl->wait, block_group_cache_done(cache) ||
		   (cache->free_space_ctl->free_space >= num_bytes));

	put_caching_control(caching_ctl);
	return 0;
}

static noinline int
wait_block_group_cache_done(struct btrfs_block_group_cache *cache)
{
	struct btrfs_caching_control *caching_ctl;
	DEFINE_WAIT(wait);

	caching_ctl = get_caching_control(cache);
	if (!caching_ctl)
		return 0;

	wait_event(caching_ctl->wait, block_group_cache_done(cache));

	put_caching_control(caching_ctl);
	return 0;
}

static int __get_block_group_index(u64 flags)
{
	int index;

	if (flags & BTRFS_BLOCK_GROUP_RAID10)
		index = 0;
	else if (flags & BTRFS_BLOCK_GROUP_RAID1)
		index = 1;
	else if (flags & BTRFS_BLOCK_GROUP_DUP)
		index = 2;
	else if (flags & BTRFS_BLOCK_GROUP_RAID0)
		index = 3;
	else
		index = 4;

	return index;
}

static int get_block_group_index(struct btrfs_block_group_cache *cache)
{
	return __get_block_group_index(cache->flags);
}

enum btrfs_loop_type {
	LOOP_CACHING_NOWAIT = 0,
	LOOP_CACHING_WAIT = 1,
	LOOP_ALLOC_CHUNK = 2,
	LOOP_NO_EMPTY_SIZE = 3,
};

static noinline int find_free_extent(struct btrfs_trans_handle *trans,
				     struct btrfs_root *orig_root,
				     u64 num_bytes, u64 empty_size,
				     u64 hint_byte, struct btrfs_key *ins,
				     u64 data)
{
	int ret = 0;
	struct btrfs_root *root = orig_root->fs_info->extent_root;
	struct btrfs_free_cluster *last_ptr = NULL;
	struct btrfs_block_group_cache *block_group = NULL;
	struct btrfs_block_group_cache *used_block_group;
	u64 search_start = 0;
	int empty_cluster = 2 * 1024 * 1024;
	int allowed_chunk_alloc = 0;
	int done_chunk_alloc = 0;
	struct btrfs_space_info *space_info;
	int loop = 0;
	int index = 0;
	int alloc_type = (data & BTRFS_BLOCK_GROUP_DATA) ?
		RESERVE_ALLOC_NO_ACCOUNT : RESERVE_ALLOC;
	bool found_uncached_bg = false;
	bool failed_cluster_refill = false;
	bool failed_alloc = false;
	bool use_cluster = true;
	bool have_caching_bg = false;

	WARN_ON(num_bytes < root->sectorsize);
	btrfs_set_key_type(ins, BTRFS_EXTENT_ITEM_KEY);
	ins->objectid = 0;
	ins->offset = 0;

	trace_find_free_extent(orig_root, num_bytes, empty_size, data);

	space_info = __find_space_info(root->fs_info, data);
	if (!space_info) {
		printk(KERN_ERR "No space info for %llu\n", data);
		return -ENOSPC;
	}

	if (btrfs_mixed_space_info(space_info))
		use_cluster = false;

	if (orig_root->ref_cows || empty_size)
		allowed_chunk_alloc = 1;

	if (data & BTRFS_BLOCK_GROUP_METADATA && use_cluster) {
		last_ptr = &root->fs_info->meta_alloc_cluster;
		if (!btrfs_test_opt(root, SSD))
			empty_cluster = 64 * 1024;
	}

	if ((data & BTRFS_BLOCK_GROUP_DATA) && use_cluster &&
	    btrfs_test_opt(root, SSD)) {
		last_ptr = &root->fs_info->data_alloc_cluster;
	}

	if (last_ptr) {
		spin_lock(&last_ptr->lock);
		if (last_ptr->block_group)
			hint_byte = last_ptr->window_start;
		spin_unlock(&last_ptr->lock);
	}

	search_start = max(search_start, first_logical_byte(root, 0));
	search_start = max(search_start, hint_byte);

	if (!last_ptr)
		empty_cluster = 0;

	if (search_start == hint_byte) {
		block_group = btrfs_lookup_block_group(root->fs_info,
						       search_start);
		used_block_group = block_group;
		if (block_group && block_group_bits(block_group, data) &&
		    block_group->cached != BTRFS_CACHE_NO) {
			down_read(&space_info->groups_sem);
			if (list_empty(&block_group->list) ||
			    block_group->ro) {
				btrfs_put_block_group(block_group);
				up_read(&space_info->groups_sem);
			} else {
				index = get_block_group_index(block_group);
				goto have_block_group;
			}
		} else if (block_group) {
			btrfs_put_block_group(block_group);
		}
	}
search:
	have_caching_bg = false;
	down_read(&space_info->groups_sem);
	list_for_each_entry(block_group, &space_info->block_groups[index],
			    list) {
		u64 offset;
		int cached;

		used_block_group = block_group;
		btrfs_get_block_group(block_group);
		search_start = block_group->key.objectid;

		if (!block_group_bits(block_group, data)) {
		    u64 extra = BTRFS_BLOCK_GROUP_DUP |
				BTRFS_BLOCK_GROUP_RAID1 |
				BTRFS_BLOCK_GROUP_RAID10;

			if ((data & extra) && !(block_group->flags & extra))
				goto loop;
		}

have_block_group:
		cached = block_group_cache_done(block_group);
		if (unlikely(!cached)) {
			found_uncached_bg = true;
			ret = cache_block_group(block_group, trans,
						orig_root, 0);
			BUG_ON(ret < 0);
			ret = 0;
		}

		if (unlikely(block_group->ro))
			goto loop;

		if (last_ptr) {
			spin_lock(&last_ptr->refill_lock);
			used_block_group = last_ptr->block_group;
			if (used_block_group != block_group &&
			    (!used_block_group ||
			     used_block_group->ro ||
			     !block_group_bits(used_block_group, data))) {
				used_block_group = block_group;
				goto refill_cluster;
			}

			if (used_block_group != block_group)
				btrfs_get_block_group(used_block_group);

			offset = btrfs_alloc_from_cluster(used_block_group,
			  last_ptr, num_bytes, used_block_group->key.objectid);
			if (offset) {
				
				spin_unlock(&last_ptr->refill_lock);
				trace_btrfs_reserve_extent_cluster(root,
					block_group, search_start, num_bytes);
				goto checks;
			}

			WARN_ON(last_ptr->block_group != used_block_group);
			if (used_block_group != block_group) {
				btrfs_put_block_group(used_block_group);
				used_block_group = block_group;
			}
refill_cluster:
			BUG_ON(used_block_group != block_group);
			if (loop >= LOOP_NO_EMPTY_SIZE &&
			    last_ptr->block_group != block_group) {
				spin_unlock(&last_ptr->refill_lock);
				goto unclustered_alloc;
			}

			btrfs_return_cluster_to_free_space(NULL, last_ptr);

			if (loop >= LOOP_NO_EMPTY_SIZE) {
				spin_unlock(&last_ptr->refill_lock);
				goto unclustered_alloc;
			}

			
			ret = btrfs_find_space_cluster(trans, root,
					       block_group, last_ptr,
					       search_start, num_bytes,
					       empty_cluster + empty_size);
			if (ret == 0) {
				offset = btrfs_alloc_from_cluster(block_group,
						  last_ptr, num_bytes,
						  search_start);
				if (offset) {
					
					spin_unlock(&last_ptr->refill_lock);
					trace_btrfs_reserve_extent_cluster(root,
						block_group, search_start,
						num_bytes);
					goto checks;
				}
			} else if (!cached && loop > LOOP_CACHING_NOWAIT
				   && !failed_cluster_refill) {
				spin_unlock(&last_ptr->refill_lock);

				failed_cluster_refill = true;
				wait_block_group_cache_progress(block_group,
				       num_bytes + empty_cluster + empty_size);
				goto have_block_group;
			}

			btrfs_return_cluster_to_free_space(NULL, last_ptr);
			spin_unlock(&last_ptr->refill_lock);
			goto loop;
		}

unclustered_alloc:
		spin_lock(&block_group->free_space_ctl->tree_lock);
		if (cached &&
		    block_group->free_space_ctl->free_space <
		    num_bytes + empty_cluster + empty_size) {
			spin_unlock(&block_group->free_space_ctl->tree_lock);
			goto loop;
		}
		spin_unlock(&block_group->free_space_ctl->tree_lock);

		offset = btrfs_find_space_for_alloc(block_group, search_start,
						    num_bytes, empty_size);
		if (!offset && !failed_alloc && !cached &&
		    loop > LOOP_CACHING_NOWAIT) {
			wait_block_group_cache_progress(block_group,
						num_bytes + empty_size);
			failed_alloc = true;
			goto have_block_group;
		} else if (!offset) {
			if (!cached)
				have_caching_bg = true;
			goto loop;
		}
checks:
		search_start = stripe_align(root, offset);

		
		if (search_start + num_bytes >
		    used_block_group->key.objectid + used_block_group->key.offset) {
			btrfs_add_free_space(used_block_group, offset, num_bytes);
			goto loop;
		}

		if (offset < search_start)
			btrfs_add_free_space(used_block_group, offset,
					     search_start - offset);
		BUG_ON(offset > search_start);

		ret = btrfs_update_reserved_bytes(used_block_group, num_bytes,
						  alloc_type);
		if (ret == -EAGAIN) {
			btrfs_add_free_space(used_block_group, offset, num_bytes);
			goto loop;
		}

		
		ins->objectid = search_start;
		ins->offset = num_bytes;

		trace_btrfs_reserve_extent(orig_root, block_group,
					   search_start, num_bytes);
		if (offset < search_start)
			btrfs_add_free_space(used_block_group, offset,
					     search_start - offset);
		BUG_ON(offset > search_start);
		if (used_block_group != block_group)
			btrfs_put_block_group(used_block_group);
		btrfs_put_block_group(block_group);
		break;
loop:
		failed_cluster_refill = false;
		failed_alloc = false;
		BUG_ON(index != get_block_group_index(block_group));
		if (used_block_group != block_group)
			btrfs_put_block_group(used_block_group);
		btrfs_put_block_group(block_group);
	}
	up_read(&space_info->groups_sem);

	if (!ins->objectid && loop >= LOOP_CACHING_WAIT && have_caching_bg)
		goto search;

	if (!ins->objectid && ++index < BTRFS_NR_RAID_TYPES)
		goto search;

	if (!ins->objectid && loop < LOOP_NO_EMPTY_SIZE) {
		index = 0;
		loop++;
		if (loop == LOOP_ALLOC_CHUNK) {
		       if (allowed_chunk_alloc) {
				ret = do_chunk_alloc(trans, root, num_bytes +
						     2 * 1024 * 1024, data,
						     CHUNK_ALLOC_LIMITED);
				if (ret < 0) {
					btrfs_abort_transaction(trans,
								root, ret);
					goto out;
				}
				allowed_chunk_alloc = 0;
				if (ret == 1)
					done_chunk_alloc = 1;
			} else if (!done_chunk_alloc &&
				   space_info->force_alloc ==
				   CHUNK_ALLOC_NO_FORCE) {
				space_info->force_alloc = CHUNK_ALLOC_LIMITED;
			}

		       if (!done_chunk_alloc)
			       loop = LOOP_NO_EMPTY_SIZE;
		}

		if (loop == LOOP_NO_EMPTY_SIZE) {
			empty_size = 0;
			empty_cluster = 0;
		}

		goto search;
	} else if (!ins->objectid) {
		ret = -ENOSPC;
	} else if (ins->objectid) {
		ret = 0;
	}
out:

	return ret;
}

static void dump_space_info(struct btrfs_space_info *info, u64 bytes,
			    int dump_block_groups)
{
	struct btrfs_block_group_cache *cache;
	int index = 0;

	spin_lock(&info->lock);
	printk(KERN_INFO "space_info %llu has %llu free, is %sfull\n",
	       (unsigned long long)info->flags,
	       (unsigned long long)(info->total_bytes - info->bytes_used -
				    info->bytes_pinned - info->bytes_reserved -
				    info->bytes_readonly),
	       (info->full) ? "" : "not ");
	printk(KERN_INFO "space_info total=%llu, used=%llu, pinned=%llu, "
	       "reserved=%llu, may_use=%llu, readonly=%llu\n",
	       (unsigned long long)info->total_bytes,
	       (unsigned long long)info->bytes_used,
	       (unsigned long long)info->bytes_pinned,
	       (unsigned long long)info->bytes_reserved,
	       (unsigned long long)info->bytes_may_use,
	       (unsigned long long)info->bytes_readonly);
	spin_unlock(&info->lock);

	if (!dump_block_groups)
		return;

	down_read(&info->groups_sem);
again:
	list_for_each_entry(cache, &info->block_groups[index], list) {
		spin_lock(&cache->lock);
		printk(KERN_INFO "block group %llu has %llu bytes, %llu used "
		       "%llu pinned %llu reserved\n",
		       (unsigned long long)cache->key.objectid,
		       (unsigned long long)cache->key.offset,
		       (unsigned long long)btrfs_block_group_used(&cache->item),
		       (unsigned long long)cache->pinned,
		       (unsigned long long)cache->reserved);
		btrfs_dump_free_space(cache, bytes);
		spin_unlock(&cache->lock);
	}
	if (++index < BTRFS_NR_RAID_TYPES)
		goto again;
	up_read(&info->groups_sem);
}

int btrfs_reserve_extent(struct btrfs_trans_handle *trans,
			 struct btrfs_root *root,
			 u64 num_bytes, u64 min_alloc_size,
			 u64 empty_size, u64 hint_byte,
			 struct btrfs_key *ins, u64 data)
{
	bool final_tried = false;
	int ret;

	data = btrfs_get_alloc_profile(root, data);
again:
	if (empty_size || root->ref_cows) {
		ret = do_chunk_alloc(trans, root->fs_info->extent_root,
				     num_bytes + 2 * 1024 * 1024, data,
				     CHUNK_ALLOC_NO_FORCE);
		if (ret < 0 && ret != -ENOSPC) {
			btrfs_abort_transaction(trans, root, ret);
			return ret;
		}
	}

	WARN_ON(num_bytes < root->sectorsize);
	ret = find_free_extent(trans, root, num_bytes, empty_size,
			       hint_byte, ins, data);

	if (ret == -ENOSPC) {
		if (!final_tried) {
			num_bytes = num_bytes >> 1;
			num_bytes = num_bytes & ~(root->sectorsize - 1);
			num_bytes = max(num_bytes, min_alloc_size);
			ret = do_chunk_alloc(trans, root->fs_info->extent_root,
				       num_bytes, data, CHUNK_ALLOC_FORCE);
			if (ret < 0 && ret != -ENOSPC) {
				btrfs_abort_transaction(trans, root, ret);
				return ret;
			}
			if (num_bytes == min_alloc_size)
				final_tried = true;
			goto again;
		} else if (btrfs_test_opt(root, ENOSPC_DEBUG)) {
			struct btrfs_space_info *sinfo;

			sinfo = __find_space_info(root->fs_info, data);
			printk(KERN_ERR "btrfs allocation failed flags %llu, "
			       "wanted %llu\n", (unsigned long long)data,
			       (unsigned long long)num_bytes);
			if (sinfo)
				dump_space_info(sinfo, num_bytes, 1);
		}
	}

	trace_btrfs_reserved_extent_alloc(root, ins->objectid, ins->offset);

	return ret;
}

static int __btrfs_free_reserved_extent(struct btrfs_root *root,
					u64 start, u64 len, int pin)
{
	struct btrfs_block_group_cache *cache;
	int ret = 0;

	cache = btrfs_lookup_block_group(root->fs_info, start);
	if (!cache) {
		printk(KERN_ERR "Unable to find block group for %llu\n",
		       (unsigned long long)start);
		return -ENOSPC;
	}

	if (btrfs_test_opt(root, DISCARD))
		ret = btrfs_discard_extent(root, start, len, NULL);

	if (pin)
		pin_down_extent(root, cache, start, len, 1);
	else {
		btrfs_add_free_space(cache, start, len);
		btrfs_update_reserved_bytes(cache, len, RESERVE_FREE);
	}
	btrfs_put_block_group(cache);

	trace_btrfs_reserved_extent_free(root, start, len);

	return ret;
}

int btrfs_free_reserved_extent(struct btrfs_root *root,
					u64 start, u64 len)
{
	return __btrfs_free_reserved_extent(root, start, len, 0);
}

int btrfs_free_and_pin_reserved_extent(struct btrfs_root *root,
				       u64 start, u64 len)
{
	return __btrfs_free_reserved_extent(root, start, len, 1);
}

static int alloc_reserved_file_extent(struct btrfs_trans_handle *trans,
				      struct btrfs_root *root,
				      u64 parent, u64 root_objectid,
				      u64 flags, u64 owner, u64 offset,
				      struct btrfs_key *ins, int ref_mod)
{
	int ret;
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_extent_item *extent_item;
	struct btrfs_extent_inline_ref *iref;
	struct btrfs_path *path;
	struct extent_buffer *leaf;
	int type;
	u32 size;

	if (parent > 0)
		type = BTRFS_SHARED_DATA_REF_KEY;
	else
		type = BTRFS_EXTENT_DATA_REF_KEY;

	size = sizeof(*extent_item) + btrfs_extent_inline_ref_size(type);

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	path->leave_spinning = 1;
	ret = btrfs_insert_empty_item(trans, fs_info->extent_root, path,
				      ins, size);
	if (ret) {
		btrfs_free_path(path);
		return ret;
	}

	leaf = path->nodes[0];
	extent_item = btrfs_item_ptr(leaf, path->slots[0],
				     struct btrfs_extent_item);
	btrfs_set_extent_refs(leaf, extent_item, ref_mod);
	btrfs_set_extent_generation(leaf, extent_item, trans->transid);
	btrfs_set_extent_flags(leaf, extent_item,
			       flags | BTRFS_EXTENT_FLAG_DATA);

	iref = (struct btrfs_extent_inline_ref *)(extent_item + 1);
	btrfs_set_extent_inline_ref_type(leaf, iref, type);
	if (parent > 0) {
		struct btrfs_shared_data_ref *ref;
		ref = (struct btrfs_shared_data_ref *)(iref + 1);
		btrfs_set_extent_inline_ref_offset(leaf, iref, parent);
		btrfs_set_shared_data_ref_count(leaf, ref, ref_mod);
	} else {
		struct btrfs_extent_data_ref *ref;
		ref = (struct btrfs_extent_data_ref *)(&iref->offset);
		btrfs_set_extent_data_ref_root(leaf, ref, root_objectid);
		btrfs_set_extent_data_ref_objectid(leaf, ref, owner);
		btrfs_set_extent_data_ref_offset(leaf, ref, offset);
		btrfs_set_extent_data_ref_count(leaf, ref, ref_mod);
	}

	btrfs_mark_buffer_dirty(path->nodes[0]);
	btrfs_free_path(path);

	ret = update_block_group(trans, root, ins->objectid, ins->offset, 1);
	if (ret) { 
		printk(KERN_ERR "btrfs update block group failed for %llu "
		       "%llu\n", (unsigned long long)ins->objectid,
		       (unsigned long long)ins->offset);
		BUG();
	}
	return ret;
}

static int alloc_reserved_tree_block(struct btrfs_trans_handle *trans,
				     struct btrfs_root *root,
				     u64 parent, u64 root_objectid,
				     u64 flags, struct btrfs_disk_key *key,
				     int level, struct btrfs_key *ins)
{
	int ret;
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_extent_item *extent_item;
	struct btrfs_tree_block_info *block_info;
	struct btrfs_extent_inline_ref *iref;
	struct btrfs_path *path;
	struct extent_buffer *leaf;
	u32 size = sizeof(*extent_item) + sizeof(*block_info) + sizeof(*iref);

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	path->leave_spinning = 1;
	ret = btrfs_insert_empty_item(trans, fs_info->extent_root, path,
				      ins, size);
	if (ret) {
		btrfs_free_path(path);
		return ret;
	}

	leaf = path->nodes[0];
	extent_item = btrfs_item_ptr(leaf, path->slots[0],
				     struct btrfs_extent_item);
	btrfs_set_extent_refs(leaf, extent_item, 1);
	btrfs_set_extent_generation(leaf, extent_item, trans->transid);
	btrfs_set_extent_flags(leaf, extent_item,
			       flags | BTRFS_EXTENT_FLAG_TREE_BLOCK);
	block_info = (struct btrfs_tree_block_info *)(extent_item + 1);

	btrfs_set_tree_block_key(leaf, block_info, key);
	btrfs_set_tree_block_level(leaf, block_info, level);

	iref = (struct btrfs_extent_inline_ref *)(block_info + 1);
	if (parent > 0) {
		BUG_ON(!(flags & BTRFS_BLOCK_FLAG_FULL_BACKREF));
		btrfs_set_extent_inline_ref_type(leaf, iref,
						 BTRFS_SHARED_BLOCK_REF_KEY);
		btrfs_set_extent_inline_ref_offset(leaf, iref, parent);
	} else {
		btrfs_set_extent_inline_ref_type(leaf, iref,
						 BTRFS_TREE_BLOCK_REF_KEY);
		btrfs_set_extent_inline_ref_offset(leaf, iref, root_objectid);
	}

	btrfs_mark_buffer_dirty(leaf);
	btrfs_free_path(path);

	ret = update_block_group(trans, root, ins->objectid, ins->offset, 1);
	if (ret) { 
		printk(KERN_ERR "btrfs update block group failed for %llu "
		       "%llu\n", (unsigned long long)ins->objectid,
		       (unsigned long long)ins->offset);
		BUG();
	}
	return ret;
}

int btrfs_alloc_reserved_file_extent(struct btrfs_trans_handle *trans,
				     struct btrfs_root *root,
				     u64 root_objectid, u64 owner,
				     u64 offset, struct btrfs_key *ins)
{
	int ret;

	BUG_ON(root_objectid == BTRFS_TREE_LOG_OBJECTID);

	ret = btrfs_add_delayed_data_ref(root->fs_info, trans, ins->objectid,
					 ins->offset, 0,
					 root_objectid, owner, offset,
					 BTRFS_ADD_DELAYED_EXTENT, NULL, 0);
	return ret;
}

int btrfs_alloc_logged_file_extent(struct btrfs_trans_handle *trans,
				   struct btrfs_root *root,
				   u64 root_objectid, u64 owner, u64 offset,
				   struct btrfs_key *ins)
{
	int ret;
	struct btrfs_block_group_cache *block_group;
	struct btrfs_caching_control *caching_ctl;
	u64 start = ins->objectid;
	u64 num_bytes = ins->offset;

	block_group = btrfs_lookup_block_group(root->fs_info, ins->objectid);
	cache_block_group(block_group, trans, NULL, 0);
	caching_ctl = get_caching_control(block_group);

	if (!caching_ctl) {
		BUG_ON(!block_group_cache_done(block_group));
		ret = btrfs_remove_free_space(block_group, start, num_bytes);
		BUG_ON(ret); 
	} else {
		mutex_lock(&caching_ctl->mutex);

		if (start >= caching_ctl->progress) {
			ret = add_excluded_extent(root, start, num_bytes);
			BUG_ON(ret); 
		} else if (start + num_bytes <= caching_ctl->progress) {
			ret = btrfs_remove_free_space(block_group,
						      start, num_bytes);
			BUG_ON(ret); 
		} else {
			num_bytes = caching_ctl->progress - start;
			ret = btrfs_remove_free_space(block_group,
						      start, num_bytes);
			BUG_ON(ret); 

			start = caching_ctl->progress;
			num_bytes = ins->objectid + ins->offset -
				    caching_ctl->progress;
			ret = add_excluded_extent(root, start, num_bytes);
			BUG_ON(ret); 
		}

		mutex_unlock(&caching_ctl->mutex);
		put_caching_control(caching_ctl);
	}

	ret = btrfs_update_reserved_bytes(block_group, ins->offset,
					  RESERVE_ALLOC_NO_ACCOUNT);
	BUG_ON(ret); 
	btrfs_put_block_group(block_group);
	ret = alloc_reserved_file_extent(trans, root, 0, root_objectid,
					 0, owner, offset, ins, 1);
	return ret;
}

struct extent_buffer *btrfs_init_new_buffer(struct btrfs_trans_handle *trans,
					    struct btrfs_root *root,
					    u64 bytenr, u32 blocksize,
					    int level)
{
	struct extent_buffer *buf;

	buf = btrfs_find_create_tree_block(root, bytenr, blocksize);
	if (!buf)
		return ERR_PTR(-ENOMEM);
	btrfs_set_header_generation(buf, trans->transid);
	btrfs_set_buffer_lockdep_class(root->root_key.objectid, buf, level);
	btrfs_tree_lock(buf);
	clean_tree_block(trans, root, buf);
	clear_bit(EXTENT_BUFFER_STALE, &buf->bflags);

	btrfs_set_lock_blocking(buf);
	btrfs_set_buffer_uptodate(buf);

	if (root->root_key.objectid == BTRFS_TREE_LOG_OBJECTID) {
		if (root->log_transid % 2 == 0)
			set_extent_dirty(&root->dirty_log_pages, buf->start,
					buf->start + buf->len - 1, GFP_NOFS);
		else
			set_extent_new(&root->dirty_log_pages, buf->start,
					buf->start + buf->len - 1, GFP_NOFS);
	} else {
		set_extent_dirty(&trans->transaction->dirty_pages, buf->start,
			 buf->start + buf->len - 1, GFP_NOFS);
	}
	trans->blocks_used++;
	
	return buf;
}

static struct btrfs_block_rsv *
use_block_rsv(struct btrfs_trans_handle *trans,
	      struct btrfs_root *root, u32 blocksize)
{
	struct btrfs_block_rsv *block_rsv;
	struct btrfs_block_rsv *global_rsv = &root->fs_info->global_block_rsv;
	int ret;

	block_rsv = get_block_rsv(trans, root);

	if (block_rsv->size == 0) {
		ret = reserve_metadata_bytes(root, block_rsv, blocksize, 0);
		if (ret && block_rsv != global_rsv) {
			ret = block_rsv_use_bytes(global_rsv, blocksize);
			if (!ret)
				return global_rsv;
			return ERR_PTR(ret);
		} else if (ret) {
			return ERR_PTR(ret);
		}
		return block_rsv;
	}

	ret = block_rsv_use_bytes(block_rsv, blocksize);
	if (!ret)
		return block_rsv;
	if (ret) {
		static DEFINE_RATELIMIT_STATE(_rs,
				DEFAULT_RATELIMIT_INTERVAL,
				 2);
		if (__ratelimit(&_rs)) {
			printk(KERN_DEBUG "btrfs: block rsv returned %d\n", ret);
			WARN_ON(1);
		}
		ret = reserve_metadata_bytes(root, block_rsv, blocksize, 0);
		if (!ret) {
			return block_rsv;
		} else if (ret && block_rsv != global_rsv) {
			ret = block_rsv_use_bytes(global_rsv, blocksize);
			if (!ret)
				return global_rsv;
		}
	}

	return ERR_PTR(-ENOSPC);
}

static void unuse_block_rsv(struct btrfs_fs_info *fs_info,
			    struct btrfs_block_rsv *block_rsv, u32 blocksize)
{
	block_rsv_add_bytes(block_rsv, blocksize, 0);
	block_rsv_release_bytes(fs_info, block_rsv, NULL, 0);
}

struct extent_buffer *btrfs_alloc_free_block(struct btrfs_trans_handle *trans,
					struct btrfs_root *root, u32 blocksize,
					u64 parent, u64 root_objectid,
					struct btrfs_disk_key *key, int level,
					u64 hint, u64 empty_size, int for_cow)
{
	struct btrfs_key ins;
	struct btrfs_block_rsv *block_rsv;
	struct extent_buffer *buf;
	u64 flags = 0;
	int ret;


	block_rsv = use_block_rsv(trans, root, blocksize);
	if (IS_ERR(block_rsv))
		return ERR_CAST(block_rsv);

	ret = btrfs_reserve_extent(trans, root, blocksize, blocksize,
				   empty_size, hint, &ins, 0);
	if (ret) {
		unuse_block_rsv(root->fs_info, block_rsv, blocksize);
		return ERR_PTR(ret);
	}

	buf = btrfs_init_new_buffer(trans, root, ins.objectid,
				    blocksize, level);
	BUG_ON(IS_ERR(buf)); 

	if (root_objectid == BTRFS_TREE_RELOC_OBJECTID) {
		if (parent == 0)
			parent = ins.objectid;
		flags |= BTRFS_BLOCK_FLAG_FULL_BACKREF;
	} else
		BUG_ON(parent > 0);

	if (root_objectid != BTRFS_TREE_LOG_OBJECTID) {
		struct btrfs_delayed_extent_op *extent_op;
		extent_op = kmalloc(sizeof(*extent_op), GFP_NOFS);
		BUG_ON(!extent_op); 
		if (key)
			memcpy(&extent_op->key, key, sizeof(extent_op->key));
		else
			memset(&extent_op->key, 0, sizeof(extent_op->key));
		extent_op->flags_to_set = flags;
		extent_op->update_key = 1;
		extent_op->update_flags = 1;
		extent_op->is_data = 0;

		ret = btrfs_add_delayed_tree_ref(root->fs_info, trans,
					ins.objectid,
					ins.offset, parent, root_objectid,
					level, BTRFS_ADD_DELAYED_EXTENT,
					extent_op, for_cow);
		BUG_ON(ret); 
	}
	return buf;
}

struct walk_control {
	u64 refs[BTRFS_MAX_LEVEL];
	u64 flags[BTRFS_MAX_LEVEL];
	struct btrfs_key update_progress;
	int stage;
	int level;
	int shared_level;
	int update_ref;
	int keep_locks;
	int reada_slot;
	int reada_count;
	int for_reloc;
};

#define DROP_REFERENCE	1
#define UPDATE_BACKREF	2

static noinline void reada_walk_down(struct btrfs_trans_handle *trans,
				     struct btrfs_root *root,
				     struct walk_control *wc,
				     struct btrfs_path *path)
{
	u64 bytenr;
	u64 generation;
	u64 refs;
	u64 flags;
	u32 nritems;
	u32 blocksize;
	struct btrfs_key key;
	struct extent_buffer *eb;
	int ret;
	int slot;
	int nread = 0;

	if (path->slots[wc->level] < wc->reada_slot) {
		wc->reada_count = wc->reada_count * 2 / 3;
		wc->reada_count = max(wc->reada_count, 2);
	} else {
		wc->reada_count = wc->reada_count * 3 / 2;
		wc->reada_count = min_t(int, wc->reada_count,
					BTRFS_NODEPTRS_PER_BLOCK(root));
	}

	eb = path->nodes[wc->level];
	nritems = btrfs_header_nritems(eb);
	blocksize = btrfs_level_size(root, wc->level - 1);

	for (slot = path->slots[wc->level]; slot < nritems; slot++) {
		if (nread >= wc->reada_count)
			break;

		cond_resched();
		bytenr = btrfs_node_blockptr(eb, slot);
		generation = btrfs_node_ptr_generation(eb, slot);

		if (slot == path->slots[wc->level])
			goto reada;

		if (wc->stage == UPDATE_BACKREF &&
		    generation <= root->root_key.offset)
			continue;

		
		ret = btrfs_lookup_extent_info(trans, root, bytenr, blocksize,
					       &refs, &flags);
		
		if (ret < 0)
			continue;
		BUG_ON(refs == 0);

		if (wc->stage == DROP_REFERENCE) {
			if (refs == 1)
				goto reada;

			if (wc->level == 1 &&
			    (flags & BTRFS_BLOCK_FLAG_FULL_BACKREF))
				continue;
			if (!wc->update_ref ||
			    generation <= root->root_key.offset)
				continue;
			btrfs_node_key_to_cpu(eb, &key, slot);
			ret = btrfs_comp_cpu_keys(&key,
						  &wc->update_progress);
			if (ret < 0)
				continue;
		} else {
			if (wc->level == 1 &&
			    (flags & BTRFS_BLOCK_FLAG_FULL_BACKREF))
				continue;
		}
reada:
		ret = readahead_tree_block(root, bytenr, blocksize,
					   generation);
		if (ret)
			break;
		nread++;
	}
	wc->reada_slot = slot;
}

static noinline int walk_down_proc(struct btrfs_trans_handle *trans,
				   struct btrfs_root *root,
				   struct btrfs_path *path,
				   struct walk_control *wc, int lookup_info)
{
	int level = wc->level;
	struct extent_buffer *eb = path->nodes[level];
	u64 flag = BTRFS_BLOCK_FLAG_FULL_BACKREF;
	int ret;

	if (wc->stage == UPDATE_BACKREF &&
	    btrfs_header_owner(eb) != root->root_key.objectid)
		return 1;

	if (lookup_info &&
	    ((wc->stage == DROP_REFERENCE && wc->refs[level] != 1) ||
	     (wc->stage == UPDATE_BACKREF && !(wc->flags[level] & flag)))) {
		BUG_ON(!path->locks[level]);
		ret = btrfs_lookup_extent_info(trans, root,
					       eb->start, eb->len,
					       &wc->refs[level],
					       &wc->flags[level]);
		BUG_ON(ret == -ENOMEM);
		if (ret)
			return ret;
		BUG_ON(wc->refs[level] == 0);
	}

	if (wc->stage == DROP_REFERENCE) {
		if (wc->refs[level] > 1)
			return 1;

		if (path->locks[level] && !wc->keep_locks) {
			btrfs_tree_unlock_rw(eb, path->locks[level]);
			path->locks[level] = 0;
		}
		return 0;
	}

	
	if (!(wc->flags[level] & flag)) {
		BUG_ON(!path->locks[level]);
		ret = btrfs_inc_ref(trans, root, eb, 1, wc->for_reloc);
		BUG_ON(ret); 
		ret = btrfs_dec_ref(trans, root, eb, 0, wc->for_reloc);
		BUG_ON(ret); 
		ret = btrfs_set_disk_extent_flags(trans, root, eb->start,
						  eb->len, flag, 0);
		BUG_ON(ret); 
		wc->flags[level] |= flag;
	}

	if (path->locks[level] && level > 0) {
		btrfs_tree_unlock_rw(eb, path->locks[level]);
		path->locks[level] = 0;
	}
	return 0;
}

static noinline int do_walk_down(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root,
				 struct btrfs_path *path,
				 struct walk_control *wc, int *lookup_info)
{
	u64 bytenr;
	u64 generation;
	u64 parent;
	u32 blocksize;
	struct btrfs_key key;
	struct extent_buffer *next;
	int level = wc->level;
	int reada = 0;
	int ret = 0;

	generation = btrfs_node_ptr_generation(path->nodes[level],
					       path->slots[level]);
	if (wc->stage == UPDATE_BACKREF &&
	    generation <= root->root_key.offset) {
		*lookup_info = 1;
		return 1;
	}

	bytenr = btrfs_node_blockptr(path->nodes[level], path->slots[level]);
	blocksize = btrfs_level_size(root, level - 1);

	next = btrfs_find_tree_block(root, bytenr, blocksize);
	if (!next) {
		next = btrfs_find_create_tree_block(root, bytenr, blocksize);
		if (!next)
			return -ENOMEM;
		reada = 1;
	}
	btrfs_tree_lock(next);
	btrfs_set_lock_blocking(next);

	ret = btrfs_lookup_extent_info(trans, root, bytenr, blocksize,
				       &wc->refs[level - 1],
				       &wc->flags[level - 1]);
	if (ret < 0) {
		btrfs_tree_unlock(next);
		return ret;
	}

	BUG_ON(wc->refs[level - 1] == 0);
	*lookup_info = 0;

	if (wc->stage == DROP_REFERENCE) {
		if (wc->refs[level - 1] > 1) {
			if (level == 1 &&
			    (wc->flags[0] & BTRFS_BLOCK_FLAG_FULL_BACKREF))
				goto skip;

			if (!wc->update_ref ||
			    generation <= root->root_key.offset)
				goto skip;

			btrfs_node_key_to_cpu(path->nodes[level], &key,
					      path->slots[level]);
			ret = btrfs_comp_cpu_keys(&key, &wc->update_progress);
			if (ret < 0)
				goto skip;

			wc->stage = UPDATE_BACKREF;
			wc->shared_level = level - 1;
		}
	} else {
		if (level == 1 &&
		    (wc->flags[0] & BTRFS_BLOCK_FLAG_FULL_BACKREF))
			goto skip;
	}

	if (!btrfs_buffer_uptodate(next, generation, 0)) {
		btrfs_tree_unlock(next);
		free_extent_buffer(next);
		next = NULL;
		*lookup_info = 1;
	}

	if (!next) {
		if (reada && level == 1)
			reada_walk_down(trans, root, wc, path);
		next = read_tree_block(root, bytenr, blocksize, generation);
		if (!next)
			return -EIO;
		btrfs_tree_lock(next);
		btrfs_set_lock_blocking(next);
	}

	level--;
	BUG_ON(level != btrfs_header_level(next));
	path->nodes[level] = next;
	path->slots[level] = 0;
	path->locks[level] = BTRFS_WRITE_LOCK_BLOCKING;
	wc->level = level;
	if (wc->level == 1)
		wc->reada_slot = 0;
	return 0;
skip:
	wc->refs[level - 1] = 0;
	wc->flags[level - 1] = 0;
	if (wc->stage == DROP_REFERENCE) {
		if (wc->flags[level] & BTRFS_BLOCK_FLAG_FULL_BACKREF) {
			parent = path->nodes[level]->start;
		} else {
			BUG_ON(root->root_key.objectid !=
			       btrfs_header_owner(path->nodes[level]));
			parent = 0;
		}

		ret = btrfs_free_extent(trans, root, bytenr, blocksize, parent,
				root->root_key.objectid, level - 1, 0, 0);
		BUG_ON(ret); 
	}
	btrfs_tree_unlock(next);
	free_extent_buffer(next);
	*lookup_info = 1;
	return 1;
}

static noinline int walk_up_proc(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root,
				 struct btrfs_path *path,
				 struct walk_control *wc)
{
	int ret;
	int level = wc->level;
	struct extent_buffer *eb = path->nodes[level];
	u64 parent = 0;

	if (wc->stage == UPDATE_BACKREF) {
		BUG_ON(wc->shared_level < level);
		if (level < wc->shared_level)
			goto out;

		ret = find_next_key(path, level + 1, &wc->update_progress);
		if (ret > 0)
			wc->update_ref = 0;

		wc->stage = DROP_REFERENCE;
		wc->shared_level = -1;
		path->slots[level] = 0;

		if (!path->locks[level]) {
			BUG_ON(level == 0);
			btrfs_tree_lock(eb);
			btrfs_set_lock_blocking(eb);
			path->locks[level] = BTRFS_WRITE_LOCK_BLOCKING;

			ret = btrfs_lookup_extent_info(trans, root,
						       eb->start, eb->len,
						       &wc->refs[level],
						       &wc->flags[level]);
			if (ret < 0) {
				btrfs_tree_unlock_rw(eb, path->locks[level]);
				return ret;
			}
			BUG_ON(wc->refs[level] == 0);
			if (wc->refs[level] == 1) {
				btrfs_tree_unlock_rw(eb, path->locks[level]);
				return 1;
			}
		}
	}

	
	BUG_ON(wc->refs[level] > 1 && !path->locks[level]);

	if (wc->refs[level] == 1) {
		if (level == 0) {
			if (wc->flags[level] & BTRFS_BLOCK_FLAG_FULL_BACKREF)
				ret = btrfs_dec_ref(trans, root, eb, 1,
						    wc->for_reloc);
			else
				ret = btrfs_dec_ref(trans, root, eb, 0,
						    wc->for_reloc);
			BUG_ON(ret); 
		}
		
		if (!path->locks[level] &&
		    btrfs_header_generation(eb) == trans->transid) {
			btrfs_tree_lock(eb);
			btrfs_set_lock_blocking(eb);
			path->locks[level] = BTRFS_WRITE_LOCK_BLOCKING;
		}
		clean_tree_block(trans, root, eb);
	}

	if (eb == root->node) {
		if (wc->flags[level] & BTRFS_BLOCK_FLAG_FULL_BACKREF)
			parent = eb->start;
		else
			BUG_ON(root->root_key.objectid !=
			       btrfs_header_owner(eb));
	} else {
		if (wc->flags[level + 1] & BTRFS_BLOCK_FLAG_FULL_BACKREF)
			parent = path->nodes[level + 1]->start;
		else
			BUG_ON(root->root_key.objectid !=
			       btrfs_header_owner(path->nodes[level + 1]));
	}

	btrfs_free_tree_block(trans, root, eb, parent, wc->refs[level] == 1, 0);
out:
	wc->refs[level] = 0;
	wc->flags[level] = 0;
	return 0;
}

static noinline int walk_down_tree(struct btrfs_trans_handle *trans,
				   struct btrfs_root *root,
				   struct btrfs_path *path,
				   struct walk_control *wc)
{
	int level = wc->level;
	int lookup_info = 1;
	int ret;

	while (level >= 0) {
		ret = walk_down_proc(trans, root, path, wc, lookup_info);
		if (ret > 0)
			break;

		if (level == 0)
			break;

		if (path->slots[level] >=
		    btrfs_header_nritems(path->nodes[level]))
			break;

		ret = do_walk_down(trans, root, path, wc, &lookup_info);
		if (ret > 0) {
			path->slots[level]++;
			continue;
		} else if (ret < 0)
			return ret;
		level = wc->level;
	}
	return 0;
}

static noinline int walk_up_tree(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root,
				 struct btrfs_path *path,
				 struct walk_control *wc, int max_level)
{
	int level = wc->level;
	int ret;

	path->slots[level] = btrfs_header_nritems(path->nodes[level]);
	while (level < max_level && path->nodes[level]) {
		wc->level = level;
		if (path->slots[level] + 1 <
		    btrfs_header_nritems(path->nodes[level])) {
			path->slots[level]++;
			return 0;
		} else {
			ret = walk_up_proc(trans, root, path, wc);
			if (ret > 0)
				return 0;

			if (path->locks[level]) {
				btrfs_tree_unlock_rw(path->nodes[level],
						     path->locks[level]);
				path->locks[level] = 0;
			}
			free_extent_buffer(path->nodes[level]);
			path->nodes[level] = NULL;
			level++;
		}
	}
	return 1;
}

int btrfs_drop_snapshot(struct btrfs_root *root,
			 struct btrfs_block_rsv *block_rsv, int update_ref,
			 int for_reloc)
{
	struct btrfs_path *path;
	struct btrfs_trans_handle *trans;
	struct btrfs_root *tree_root = root->fs_info->tree_root;
	struct btrfs_root_item *root_item = &root->root_item;
	struct walk_control *wc;
	struct btrfs_key key;
	int err = 0;
	int ret;
	int level;

	path = btrfs_alloc_path();
	if (!path) {
		err = -ENOMEM;
		goto out;
	}

	wc = kzalloc(sizeof(*wc), GFP_NOFS);
	if (!wc) {
		btrfs_free_path(path);
		err = -ENOMEM;
		goto out;
	}

	trans = btrfs_start_transaction(tree_root, 0);
	if (IS_ERR(trans)) {
		err = PTR_ERR(trans);
		goto out_free;
	}

	if (block_rsv)
		trans->block_rsv = block_rsv;

	if (btrfs_disk_key_objectid(&root_item->drop_progress) == 0) {
		level = btrfs_header_level(root->node);
		path->nodes[level] = btrfs_lock_root_node(root);
		btrfs_set_lock_blocking(path->nodes[level]);
		path->slots[level] = 0;
		path->locks[level] = BTRFS_WRITE_LOCK_BLOCKING;
		memset(&wc->update_progress, 0,
		       sizeof(wc->update_progress));
	} else {
		btrfs_disk_key_to_cpu(&key, &root_item->drop_progress);
		memcpy(&wc->update_progress, &key,
		       sizeof(wc->update_progress));

		level = root_item->drop_level;
		BUG_ON(level == 0);
		path->lowest_level = level;
		ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
		path->lowest_level = 0;
		if (ret < 0) {
			err = ret;
			goto out_end_trans;
		}
		WARN_ON(ret > 0);

		btrfs_unlock_up_safe(path, 0);

		level = btrfs_header_level(root->node);
		while (1) {
			btrfs_tree_lock(path->nodes[level]);
			btrfs_set_lock_blocking(path->nodes[level]);

			ret = btrfs_lookup_extent_info(trans, root,
						path->nodes[level]->start,
						path->nodes[level]->len,
						&wc->refs[level],
						&wc->flags[level]);
			if (ret < 0) {
				err = ret;
				goto out_end_trans;
			}
			BUG_ON(wc->refs[level] == 0);

			if (level == root_item->drop_level)
				break;

			btrfs_tree_unlock(path->nodes[level]);
			WARN_ON(wc->refs[level] != 1);
			level--;
		}
	}

	wc->level = level;
	wc->shared_level = -1;
	wc->stage = DROP_REFERENCE;
	wc->update_ref = update_ref;
	wc->keep_locks = 0;
	wc->for_reloc = for_reloc;
	wc->reada_count = BTRFS_NODEPTRS_PER_BLOCK(root);

	while (1) {
		ret = walk_down_tree(trans, root, path, wc);
		if (ret < 0) {
			err = ret;
			break;
		}

		ret = walk_up_tree(trans, root, path, wc, BTRFS_MAX_LEVEL);
		if (ret < 0) {
			err = ret;
			break;
		}

		if (ret > 0) {
			BUG_ON(wc->stage != DROP_REFERENCE);
			break;
		}

		if (wc->stage == DROP_REFERENCE) {
			level = wc->level;
			btrfs_node_key(path->nodes[level],
				       &root_item->drop_progress,
				       path->slots[level]);
			root_item->drop_level = level;
		}

		BUG_ON(wc->level == 0);
		if (btrfs_should_end_transaction(trans, tree_root)) {
			ret = btrfs_update_root(trans, tree_root,
						&root->root_key,
						root_item);
			if (ret) {
				btrfs_abort_transaction(trans, tree_root, ret);
				err = ret;
				goto out_end_trans;
			}

			btrfs_end_transaction_throttle(trans, tree_root);
			trans = btrfs_start_transaction(tree_root, 0);
			if (IS_ERR(trans)) {
				err = PTR_ERR(trans);
				goto out_free;
			}
			if (block_rsv)
				trans->block_rsv = block_rsv;
		}
	}
	btrfs_release_path(path);
	if (err)
		goto out_end_trans;

	ret = btrfs_del_root(trans, tree_root, &root->root_key);
	if (ret) {
		btrfs_abort_transaction(trans, tree_root, ret);
		goto out_end_trans;
	}

	if (root->root_key.objectid != BTRFS_TREE_RELOC_OBJECTID) {
		ret = btrfs_find_last_root(tree_root, root->root_key.objectid,
					   NULL, NULL);
		if (ret < 0) {
			btrfs_abort_transaction(trans, tree_root, ret);
			err = ret;
			goto out_end_trans;
		} else if (ret > 0) {
			btrfs_del_orphan_item(trans, tree_root,
					      root->root_key.objectid);
		}
	}

	if (root->in_radix) {
		btrfs_free_fs_root(tree_root->fs_info, root);
	} else {
		free_extent_buffer(root->node);
		free_extent_buffer(root->commit_root);
		kfree(root);
	}
out_end_trans:
	btrfs_end_transaction_throttle(trans, tree_root);
out_free:
	kfree(wc);
	btrfs_free_path(path);
out:
	if (err)
		btrfs_std_error(root->fs_info, err);
	return err;
}

int btrfs_drop_subtree(struct btrfs_trans_handle *trans,
			struct btrfs_root *root,
			struct extent_buffer *node,
			struct extent_buffer *parent)
{
	struct btrfs_path *path;
	struct walk_control *wc;
	int level;
	int parent_level;
	int ret = 0;
	int wret;

	BUG_ON(root->root_key.objectid != BTRFS_TREE_RELOC_OBJECTID);

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	wc = kzalloc(sizeof(*wc), GFP_NOFS);
	if (!wc) {
		btrfs_free_path(path);
		return -ENOMEM;
	}

	btrfs_assert_tree_locked(parent);
	parent_level = btrfs_header_level(parent);
	extent_buffer_get(parent);
	path->nodes[parent_level] = parent;
	path->slots[parent_level] = btrfs_header_nritems(parent);

	btrfs_assert_tree_locked(node);
	level = btrfs_header_level(node);
	path->nodes[level] = node;
	path->slots[level] = 0;
	path->locks[level] = BTRFS_WRITE_LOCK_BLOCKING;

	wc->refs[parent_level] = 1;
	wc->flags[parent_level] = BTRFS_BLOCK_FLAG_FULL_BACKREF;
	wc->level = level;
	wc->shared_level = -1;
	wc->stage = DROP_REFERENCE;
	wc->update_ref = 0;
	wc->keep_locks = 1;
	wc->for_reloc = 1;
	wc->reada_count = BTRFS_NODEPTRS_PER_BLOCK(root);

	while (1) {
		wret = walk_down_tree(trans, root, path, wc);
		if (wret < 0) {
			ret = wret;
			break;
		}

		wret = walk_up_tree(trans, root, path, wc, parent_level);
		if (wret < 0)
			ret = wret;
		if (wret != 0)
			break;
	}

	kfree(wc);
	btrfs_free_path(path);
	return ret;
}

static u64 update_block_group_flags(struct btrfs_root *root, u64 flags)
{
	u64 num_devices;
	u64 stripped;

	stripped = get_restripe_target(root->fs_info, flags);
	if (stripped)
		return extended_to_chunk(stripped);

	num_devices = root->fs_info->fs_devices->rw_devices +
		root->fs_info->fs_devices->missing_devices;

	stripped = BTRFS_BLOCK_GROUP_RAID0 |
		BTRFS_BLOCK_GROUP_RAID1 | BTRFS_BLOCK_GROUP_RAID10;

	if (num_devices == 1) {
		stripped |= BTRFS_BLOCK_GROUP_DUP;
		stripped = flags & ~stripped;

		
		if (flags & BTRFS_BLOCK_GROUP_RAID0)
			return stripped;

		
		if (flags & (BTRFS_BLOCK_GROUP_RAID1 |
			     BTRFS_BLOCK_GROUP_RAID10))
			return stripped | BTRFS_BLOCK_GROUP_DUP;
	} else {
		
		if (flags & stripped)
			return flags;

		stripped |= BTRFS_BLOCK_GROUP_DUP;
		stripped = flags & ~stripped;

		
		if (flags & BTRFS_BLOCK_GROUP_DUP)
			return stripped | BTRFS_BLOCK_GROUP_RAID1;

		
	}

	return flags;
}

static int set_block_group_ro(struct btrfs_block_group_cache *cache, int force)
{
	struct btrfs_space_info *sinfo = cache->space_info;
	u64 num_bytes;
	u64 min_allocable_bytes;
	int ret = -ENOSPC;


	if ((sinfo->flags &
	     (BTRFS_BLOCK_GROUP_SYSTEM | BTRFS_BLOCK_GROUP_METADATA)) &&
	    !force)
		min_allocable_bytes = 1 * 1024 * 1024;
	else
		min_allocable_bytes = 0;

	spin_lock(&sinfo->lock);
	spin_lock(&cache->lock);

	if (cache->ro) {
		ret = 0;
		goto out;
	}

	num_bytes = cache->key.offset - cache->reserved - cache->pinned -
		    cache->bytes_super - btrfs_block_group_used(&cache->item);

	if (sinfo->bytes_used + sinfo->bytes_reserved + sinfo->bytes_pinned +
	    sinfo->bytes_may_use + sinfo->bytes_readonly + num_bytes +
	    min_allocable_bytes <= sinfo->total_bytes) {
		sinfo->bytes_readonly += num_bytes;
		cache->ro = 1;
		ret = 0;
	}
out:
	spin_unlock(&cache->lock);
	spin_unlock(&sinfo->lock);
	return ret;
}

int btrfs_set_block_group_ro(struct btrfs_root *root,
			     struct btrfs_block_group_cache *cache)

{
	struct btrfs_trans_handle *trans;
	u64 alloc_flags;
	int ret;

	BUG_ON(cache->ro);

	trans = btrfs_join_transaction(root);
	if (IS_ERR(trans))
		return PTR_ERR(trans);

	alloc_flags = update_block_group_flags(root, cache->flags);
	if (alloc_flags != cache->flags) {
		ret = do_chunk_alloc(trans, root, 2 * 1024 * 1024, alloc_flags,
				     CHUNK_ALLOC_FORCE);
		if (ret < 0)
			goto out;
	}

	ret = set_block_group_ro(cache, 0);
	if (!ret)
		goto out;
	alloc_flags = get_alloc_profile(root, cache->space_info->flags);
	ret = do_chunk_alloc(trans, root, 2 * 1024 * 1024, alloc_flags,
			     CHUNK_ALLOC_FORCE);
	if (ret < 0)
		goto out;
	ret = set_block_group_ro(cache, 0);
out:
	btrfs_end_transaction(trans, root);
	return ret;
}

int btrfs_force_chunk_alloc(struct btrfs_trans_handle *trans,
			    struct btrfs_root *root, u64 type)
{
	u64 alloc_flags = get_alloc_profile(root, type);
	return do_chunk_alloc(trans, root, 2 * 1024 * 1024, alloc_flags,
			      CHUNK_ALLOC_FORCE);
}

static u64 __btrfs_get_ro_block_group_free_space(struct list_head *groups_list)
{
	struct btrfs_block_group_cache *block_group;
	u64 free_bytes = 0;
	int factor;

	list_for_each_entry(block_group, groups_list, list) {
		spin_lock(&block_group->lock);

		if (!block_group->ro) {
			spin_unlock(&block_group->lock);
			continue;
		}

		if (block_group->flags & (BTRFS_BLOCK_GROUP_RAID1 |
					  BTRFS_BLOCK_GROUP_RAID10 |
					  BTRFS_BLOCK_GROUP_DUP))
			factor = 2;
		else
			factor = 1;

		free_bytes += (block_group->key.offset -
			       btrfs_block_group_used(&block_group->item)) *
			       factor;

		spin_unlock(&block_group->lock);
	}

	return free_bytes;
}

u64 btrfs_account_ro_block_groups_free_space(struct btrfs_space_info *sinfo)
{
	int i;
	u64 free_bytes = 0;

	spin_lock(&sinfo->lock);

	for(i = 0; i < BTRFS_NR_RAID_TYPES; i++)
		if (!list_empty(&sinfo->block_groups[i]))
			free_bytes += __btrfs_get_ro_block_group_free_space(
						&sinfo->block_groups[i]);

	spin_unlock(&sinfo->lock);

	return free_bytes;
}

void btrfs_set_block_group_rw(struct btrfs_root *root,
			      struct btrfs_block_group_cache *cache)
{
	struct btrfs_space_info *sinfo = cache->space_info;
	u64 num_bytes;

	BUG_ON(!cache->ro);

	spin_lock(&sinfo->lock);
	spin_lock(&cache->lock);
	num_bytes = cache->key.offset - cache->reserved - cache->pinned -
		    cache->bytes_super - btrfs_block_group_used(&cache->item);
	sinfo->bytes_readonly -= num_bytes;
	cache->ro = 0;
	spin_unlock(&cache->lock);
	spin_unlock(&sinfo->lock);
}

int btrfs_can_relocate(struct btrfs_root *root, u64 bytenr)
{
	struct btrfs_block_group_cache *block_group;
	struct btrfs_space_info *space_info;
	struct btrfs_fs_devices *fs_devices = root->fs_info->fs_devices;
	struct btrfs_device *device;
	u64 min_free;
	u64 dev_min = 1;
	u64 dev_nr = 0;
	u64 target;
	int index;
	int full = 0;
	int ret = 0;

	block_group = btrfs_lookup_block_group(root->fs_info, bytenr);

	
	if (!block_group)
		return -1;

	min_free = btrfs_block_group_used(&block_group->item);

	
	if (!min_free)
		goto out;

	space_info = block_group->space_info;
	spin_lock(&space_info->lock);

	full = space_info->full;

	if ((space_info->total_bytes != block_group->key.offset) &&
	    (space_info->bytes_used + space_info->bytes_reserved +
	     space_info->bytes_pinned + space_info->bytes_readonly +
	     min_free < space_info->total_bytes)) {
		spin_unlock(&space_info->lock);
		goto out;
	}
	spin_unlock(&space_info->lock);

	ret = -1;

	target = get_restripe_target(root->fs_info, block_group->flags);
	if (target) {
		index = __get_block_group_index(extended_to_chunk(target));
	} else {
		if (full)
			goto out;

		index = get_block_group_index(block_group);
	}

	if (index == 0) {
		dev_min = 4;
		
		min_free >>= 1;
	} else if (index == 1) {
		dev_min = 2;
	} else if (index == 2) {
		
		min_free <<= 1;
	} else if (index == 3) {
		dev_min = fs_devices->rw_devices;
		do_div(min_free, dev_min);
	}

	mutex_lock(&root->fs_info->chunk_mutex);
	list_for_each_entry(device, &fs_devices->alloc_list, dev_alloc_list) {
		u64 dev_offset;

		if (device->total_bytes > device->bytes_used + min_free) {
			ret = find_free_dev_extent(device, min_free,
						   &dev_offset, NULL);
			if (!ret)
				dev_nr++;

			if (dev_nr >= dev_min)
				break;

			ret = -1;
		}
	}
	mutex_unlock(&root->fs_info->chunk_mutex);
out:
	btrfs_put_block_group(block_group);
	return ret;
}

static int find_first_block_group(struct btrfs_root *root,
		struct btrfs_path *path, struct btrfs_key *key)
{
	int ret = 0;
	struct btrfs_key found_key;
	struct extent_buffer *leaf;
	int slot;

	ret = btrfs_search_slot(NULL, root, key, path, 0, 0);
	if (ret < 0)
		goto out;

	while (1) {
		slot = path->slots[0];
		leaf = path->nodes[0];
		if (slot >= btrfs_header_nritems(leaf)) {
			ret = btrfs_next_leaf(root, path);
			if (ret == 0)
				continue;
			if (ret < 0)
				goto out;
			break;
		}
		btrfs_item_key_to_cpu(leaf, &found_key, slot);

		if (found_key.objectid >= key->objectid &&
		    found_key.type == BTRFS_BLOCK_GROUP_ITEM_KEY) {
			ret = 0;
			goto out;
		}
		path->slots[0]++;
	}
out:
	return ret;
}

void btrfs_put_block_group_cache(struct btrfs_fs_info *info)
{
	struct btrfs_block_group_cache *block_group;
	u64 last = 0;

	while (1) {
		struct inode *inode;

		block_group = btrfs_lookup_first_block_group(info, last);
		while (block_group) {
			spin_lock(&block_group->lock);
			if (block_group->iref)
				break;
			spin_unlock(&block_group->lock);
			block_group = next_block_group(info->tree_root,
						       block_group);
		}
		if (!block_group) {
			if (last == 0)
				break;
			last = 0;
			continue;
		}

		inode = block_group->inode;
		block_group->iref = 0;
		block_group->inode = NULL;
		spin_unlock(&block_group->lock);
		iput(inode);
		last = block_group->key.objectid + block_group->key.offset;
		btrfs_put_block_group(block_group);
	}
}

int btrfs_free_block_groups(struct btrfs_fs_info *info)
{
	struct btrfs_block_group_cache *block_group;
	struct btrfs_space_info *space_info;
	struct btrfs_caching_control *caching_ctl;
	struct rb_node *n;

	down_write(&info->extent_commit_sem);
	while (!list_empty(&info->caching_block_groups)) {
		caching_ctl = list_entry(info->caching_block_groups.next,
					 struct btrfs_caching_control, list);
		list_del(&caching_ctl->list);
		put_caching_control(caching_ctl);
	}
	up_write(&info->extent_commit_sem);

	spin_lock(&info->block_group_cache_lock);
	while ((n = rb_last(&info->block_group_cache_tree)) != NULL) {
		block_group = rb_entry(n, struct btrfs_block_group_cache,
				       cache_node);
		rb_erase(&block_group->cache_node,
			 &info->block_group_cache_tree);
		spin_unlock(&info->block_group_cache_lock);

		down_write(&block_group->space_info->groups_sem);
		list_del(&block_group->list);
		up_write(&block_group->space_info->groups_sem);

		if (block_group->cached == BTRFS_CACHE_STARTED)
			wait_block_group_cache_done(block_group);

		if (block_group->cached == BTRFS_CACHE_NO)
			free_excluded_extents(info->extent_root, block_group);

		btrfs_remove_free_space_cache(block_group);
		btrfs_put_block_group(block_group);

		spin_lock(&info->block_group_cache_lock);
	}
	spin_unlock(&info->block_group_cache_lock);

	synchronize_rcu();

	release_global_block_rsv(info);

	while(!list_empty(&info->space_info)) {
		space_info = list_entry(info->space_info.next,
					struct btrfs_space_info,
					list);
		if (space_info->bytes_pinned > 0 ||
		    space_info->bytes_reserved > 0 ||
		    space_info->bytes_may_use > 0) {
			WARN_ON(1);
			dump_space_info(space_info, 0, 0);
		}
		list_del(&space_info->list);
		kfree(space_info);
	}
	return 0;
}

static void __link_block_group(struct btrfs_space_info *space_info,
			       struct btrfs_block_group_cache *cache)
{
	int index = get_block_group_index(cache);

	down_write(&space_info->groups_sem);
	list_add_tail(&cache->list, &space_info->block_groups[index]);
	up_write(&space_info->groups_sem);
}

int btrfs_read_block_groups(struct btrfs_root *root)
{
	struct btrfs_path *path;
	int ret;
	struct btrfs_block_group_cache *cache;
	struct btrfs_fs_info *info = root->fs_info;
	struct btrfs_space_info *space_info;
	struct btrfs_key key;
	struct btrfs_key found_key;
	struct extent_buffer *leaf;
	int need_clear = 0;
	u64 cache_gen;

	root = info->extent_root;
	key.objectid = 0;
	key.offset = 0;
	btrfs_set_key_type(&key, BTRFS_BLOCK_GROUP_ITEM_KEY);
	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;
	path->reada = 1;

	cache_gen = btrfs_super_cache_generation(root->fs_info->super_copy);
	if (btrfs_test_opt(root, SPACE_CACHE) &&
	    btrfs_super_generation(root->fs_info->super_copy) != cache_gen)
		need_clear = 1;
	if (btrfs_test_opt(root, CLEAR_CACHE))
		need_clear = 1;

	while (1) {
		ret = find_first_block_group(root, path, &key);
		if (ret > 0)
			break;
		if (ret != 0)
			goto error;
		leaf = path->nodes[0];
		btrfs_item_key_to_cpu(leaf, &found_key, path->slots[0]);
		cache = kzalloc(sizeof(*cache), GFP_NOFS);
		if (!cache) {
			ret = -ENOMEM;
			goto error;
		}
		cache->free_space_ctl = kzalloc(sizeof(*cache->free_space_ctl),
						GFP_NOFS);
		if (!cache->free_space_ctl) {
			kfree(cache);
			ret = -ENOMEM;
			goto error;
		}

		atomic_set(&cache->count, 1);
		spin_lock_init(&cache->lock);
		cache->fs_info = info;
		INIT_LIST_HEAD(&cache->list);
		INIT_LIST_HEAD(&cache->cluster_list);

		if (need_clear)
			cache->disk_cache_state = BTRFS_DC_CLEAR;

		read_extent_buffer(leaf, &cache->item,
				   btrfs_item_ptr_offset(leaf, path->slots[0]),
				   sizeof(cache->item));
		memcpy(&cache->key, &found_key, sizeof(found_key));

		key.objectid = found_key.objectid + found_key.offset;
		btrfs_release_path(path);
		cache->flags = btrfs_block_group_flags(&cache->item);
		cache->sectorsize = root->sectorsize;

		btrfs_init_free_space_ctl(cache);

		exclude_super_stripes(root, cache);

		if (found_key.offset == btrfs_block_group_used(&cache->item)) {
			cache->last_byte_to_unpin = (u64)-1;
			cache->cached = BTRFS_CACHE_FINISHED;
			free_excluded_extents(root, cache);
		} else if (btrfs_block_group_used(&cache->item) == 0) {
			cache->last_byte_to_unpin = (u64)-1;
			cache->cached = BTRFS_CACHE_FINISHED;
			add_new_free_space(cache, root->fs_info,
					   found_key.objectid,
					   found_key.objectid +
					   found_key.offset);
			free_excluded_extents(root, cache);
		}

		ret = update_space_info(info, cache->flags, found_key.offset,
					btrfs_block_group_used(&cache->item),
					&space_info);
		BUG_ON(ret); 
		cache->space_info = space_info;
		spin_lock(&cache->space_info->lock);
		cache->space_info->bytes_readonly += cache->bytes_super;
		spin_unlock(&cache->space_info->lock);

		__link_block_group(space_info, cache);

		ret = btrfs_add_block_group_cache(root->fs_info, cache);
		BUG_ON(ret); 

		set_avail_alloc_bits(root->fs_info, cache->flags);
		if (btrfs_chunk_readonly(root, cache->key.objectid))
			set_block_group_ro(cache, 1);
	}

	list_for_each_entry_rcu(space_info, &root->fs_info->space_info, list) {
		if (!(get_alloc_profile(root, space_info->flags) &
		      (BTRFS_BLOCK_GROUP_RAID10 |
		       BTRFS_BLOCK_GROUP_RAID1 |
		       BTRFS_BLOCK_GROUP_DUP)))
			continue;
		list_for_each_entry(cache, &space_info->block_groups[3], list)
			set_block_group_ro(cache, 1);
		list_for_each_entry(cache, &space_info->block_groups[4], list)
			set_block_group_ro(cache, 1);
	}

	init_global_block_rsv(info);
	ret = 0;
error:
	btrfs_free_path(path);
	return ret;
}

int btrfs_make_block_group(struct btrfs_trans_handle *trans,
			   struct btrfs_root *root, u64 bytes_used,
			   u64 type, u64 chunk_objectid, u64 chunk_offset,
			   u64 size)
{
	int ret;
	struct btrfs_root *extent_root;
	struct btrfs_block_group_cache *cache;

	extent_root = root->fs_info->extent_root;

	root->fs_info->last_trans_log_full_commit = trans->transid;

	cache = kzalloc(sizeof(*cache), GFP_NOFS);
	if (!cache)
		return -ENOMEM;
	cache->free_space_ctl = kzalloc(sizeof(*cache->free_space_ctl),
					GFP_NOFS);
	if (!cache->free_space_ctl) {
		kfree(cache);
		return -ENOMEM;
	}

	cache->key.objectid = chunk_offset;
	cache->key.offset = size;
	cache->key.type = BTRFS_BLOCK_GROUP_ITEM_KEY;
	cache->sectorsize = root->sectorsize;
	cache->fs_info = root->fs_info;

	atomic_set(&cache->count, 1);
	spin_lock_init(&cache->lock);
	INIT_LIST_HEAD(&cache->list);
	INIT_LIST_HEAD(&cache->cluster_list);

	btrfs_init_free_space_ctl(cache);

	btrfs_set_block_group_used(&cache->item, bytes_used);
	btrfs_set_block_group_chunk_objectid(&cache->item, chunk_objectid);
	cache->flags = type;
	btrfs_set_block_group_flags(&cache->item, type);

	cache->last_byte_to_unpin = (u64)-1;
	cache->cached = BTRFS_CACHE_FINISHED;
	exclude_super_stripes(root, cache);

	add_new_free_space(cache, root->fs_info, chunk_offset,
			   chunk_offset + size);

	free_excluded_extents(root, cache);

	ret = update_space_info(root->fs_info, cache->flags, size, bytes_used,
				&cache->space_info);
	BUG_ON(ret); 
	update_global_block_rsv(root->fs_info);

	spin_lock(&cache->space_info->lock);
	cache->space_info->bytes_readonly += cache->bytes_super;
	spin_unlock(&cache->space_info->lock);

	__link_block_group(cache->space_info, cache);

	ret = btrfs_add_block_group_cache(root->fs_info, cache);
	BUG_ON(ret); 

	ret = btrfs_insert_item(trans, extent_root, &cache->key, &cache->item,
				sizeof(cache->item));
	if (ret) {
		btrfs_abort_transaction(trans, extent_root, ret);
		return ret;
	}

	set_avail_alloc_bits(extent_root->fs_info, type);

	return 0;
}

static void clear_avail_alloc_bits(struct btrfs_fs_info *fs_info, u64 flags)
{
	u64 extra_flags = chunk_to_extended(flags) &
				BTRFS_EXTENDED_PROFILE_MASK;

	if (flags & BTRFS_BLOCK_GROUP_DATA)
		fs_info->avail_data_alloc_bits &= ~extra_flags;
	if (flags & BTRFS_BLOCK_GROUP_METADATA)
		fs_info->avail_metadata_alloc_bits &= ~extra_flags;
	if (flags & BTRFS_BLOCK_GROUP_SYSTEM)
		fs_info->avail_system_alloc_bits &= ~extra_flags;
}

int btrfs_remove_block_group(struct btrfs_trans_handle *trans,
			     struct btrfs_root *root, u64 group_start)
{
	struct btrfs_path *path;
	struct btrfs_block_group_cache *block_group;
	struct btrfs_free_cluster *cluster;
	struct btrfs_root *tree_root = root->fs_info->tree_root;
	struct btrfs_key key;
	struct inode *inode;
	int ret;
	int index;
	int factor;

	root = root->fs_info->extent_root;

	block_group = btrfs_lookup_block_group(root->fs_info, group_start);
	BUG_ON(!block_group);
	BUG_ON(!block_group->ro);

	free_excluded_extents(root, block_group);

	memcpy(&key, &block_group->key, sizeof(key));
	index = get_block_group_index(block_group);
	if (block_group->flags & (BTRFS_BLOCK_GROUP_DUP |
				  BTRFS_BLOCK_GROUP_RAID1 |
				  BTRFS_BLOCK_GROUP_RAID10))
		factor = 2;
	else
		factor = 1;

	
	cluster = &root->fs_info->data_alloc_cluster;
	spin_lock(&cluster->refill_lock);
	btrfs_return_cluster_to_free_space(block_group, cluster);
	spin_unlock(&cluster->refill_lock);

	cluster = &root->fs_info->meta_alloc_cluster;
	spin_lock(&cluster->refill_lock);
	btrfs_return_cluster_to_free_space(block_group, cluster);
	spin_unlock(&cluster->refill_lock);

	path = btrfs_alloc_path();
	if (!path) {
		ret = -ENOMEM;
		goto out;
	}

	inode = lookup_free_space_inode(tree_root, block_group, path);
	if (!IS_ERR(inode)) {
		ret = btrfs_orphan_add(trans, inode);
		if (ret) {
			btrfs_add_delayed_iput(inode);
			goto out;
		}
		clear_nlink(inode);
		
		spin_lock(&block_group->lock);
		if (block_group->iref) {
			block_group->iref = 0;
			block_group->inode = NULL;
			spin_unlock(&block_group->lock);
			iput(inode);
		} else {
			spin_unlock(&block_group->lock);
		}
		
		btrfs_add_delayed_iput(inode);
	}

	key.objectid = BTRFS_FREE_SPACE_OBJECTID;
	key.offset = block_group->key.objectid;
	key.type = 0;

	ret = btrfs_search_slot(trans, tree_root, &key, path, -1, 1);
	if (ret < 0)
		goto out;
	if (ret > 0)
		btrfs_release_path(path);
	if (ret == 0) {
		ret = btrfs_del_item(trans, tree_root, path);
		if (ret)
			goto out;
		btrfs_release_path(path);
	}

	spin_lock(&root->fs_info->block_group_cache_lock);
	rb_erase(&block_group->cache_node,
		 &root->fs_info->block_group_cache_tree);
	spin_unlock(&root->fs_info->block_group_cache_lock);

	down_write(&block_group->space_info->groups_sem);
	list_del_init(&block_group->list);
	if (list_empty(&block_group->space_info->block_groups[index]))
		clear_avail_alloc_bits(root->fs_info, block_group->flags);
	up_write(&block_group->space_info->groups_sem);

	if (block_group->cached == BTRFS_CACHE_STARTED)
		wait_block_group_cache_done(block_group);

	btrfs_remove_free_space_cache(block_group);

	spin_lock(&block_group->space_info->lock);
	block_group->space_info->total_bytes -= block_group->key.offset;
	block_group->space_info->bytes_readonly -= block_group->key.offset;
	block_group->space_info->disk_total -= block_group->key.offset * factor;
	spin_unlock(&block_group->space_info->lock);

	memcpy(&key, &block_group->key, sizeof(key));

	btrfs_clear_space_info_full(root->fs_info);

	btrfs_put_block_group(block_group);
	btrfs_put_block_group(block_group);

	ret = btrfs_search_slot(trans, root, &key, path, -1, 1);
	if (ret > 0)
		ret = -EIO;
	if (ret < 0)
		goto out;

	ret = btrfs_del_item(trans, root, path);
out:
	btrfs_free_path(path);
	return ret;
}

int btrfs_init_space_info(struct btrfs_fs_info *fs_info)
{
	struct btrfs_space_info *space_info;
	struct btrfs_super_block *disk_super;
	u64 features;
	u64 flags;
	int mixed = 0;
	int ret;

	disk_super = fs_info->super_copy;
	if (!btrfs_super_root(disk_super))
		return 1;

	features = btrfs_super_incompat_flags(disk_super);
	if (features & BTRFS_FEATURE_INCOMPAT_MIXED_GROUPS)
		mixed = 1;

	flags = BTRFS_BLOCK_GROUP_SYSTEM;
	ret = update_space_info(fs_info, flags, 0, 0, &space_info);
	if (ret)
		goto out;

	if (mixed) {
		flags = BTRFS_BLOCK_GROUP_METADATA | BTRFS_BLOCK_GROUP_DATA;
		ret = update_space_info(fs_info, flags, 0, 0, &space_info);
	} else {
		flags = BTRFS_BLOCK_GROUP_METADATA;
		ret = update_space_info(fs_info, flags, 0, 0, &space_info);
		if (ret)
			goto out;

		flags = BTRFS_BLOCK_GROUP_DATA;
		ret = update_space_info(fs_info, flags, 0, 0, &space_info);
	}
out:
	return ret;
}

int btrfs_error_unpin_extent_range(struct btrfs_root *root, u64 start, u64 end)
{
	return unpin_extent_range(root, start, end);
}

int btrfs_error_discard_extent(struct btrfs_root *root, u64 bytenr,
			       u64 num_bytes, u64 *actual_bytes)
{
	return btrfs_discard_extent(root, bytenr, num_bytes, actual_bytes);
}

int btrfs_trim_fs(struct btrfs_root *root, struct fstrim_range *range)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_block_group_cache *cache = NULL;
	u64 group_trimmed;
	u64 start;
	u64 end;
	u64 trimmed = 0;
	u64 total_bytes = btrfs_super_total_bytes(fs_info->super_copy);
	int ret = 0;

	if (range->len == total_bytes)
		cache = btrfs_lookup_first_block_group(fs_info, range->start);
	else
		cache = btrfs_lookup_block_group(fs_info, range->start);

	while (cache) {
		if (cache->key.objectid >= (range->start + range->len)) {
			btrfs_put_block_group(cache);
			break;
		}

		start = max(range->start, cache->key.objectid);
		end = min(range->start + range->len,
				cache->key.objectid + cache->key.offset);

		if (end - start >= range->minlen) {
			if (!block_group_cache_done(cache)) {
				ret = cache_block_group(cache, NULL, root, 0);
				if (!ret)
					wait_block_group_cache_done(cache);
			}
			ret = btrfs_trim_block_group(cache,
						     &group_trimmed,
						     start,
						     end,
						     range->minlen);

			trimmed += group_trimmed;
			if (ret) {
				btrfs_put_block_group(cache);
				break;
			}
		}

		cache = next_block_group(fs_info->tree_root, cache);
	}

	range->len = trimmed;
	return ret;
}
