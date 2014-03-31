/* -*- mode: c; c-basic-offset: 8; -*-
 * vim: noexpandtab sw=8 ts=8 sts=0:
 *
 * uptodate.c
 *
 * Tracking the up-to-date-ness of a local buffer_head with respect to
 * the cluster.
 *
 * Copyright (C) 2002, 2004, 2005 Oracle.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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
 *
 * Standard buffer head caching flags (uptodate, etc) are insufficient
 * in a clustered environment - a buffer may be marked up to date on
 * our local node but could have been modified by another cluster
 * member. As a result an additional (and performant) caching scheme
 * is required. A further requirement is that we consume as little
 * memory as possible - we never pin buffer_head structures in order
 * to cache them.
 *
 * We track the existence of up to date buffers on the inodes which
 * are associated with them. Because we don't want to pin
 * buffer_heads, this is only a (strong) hint and several other checks
 * are made in the I/O path to ensure that we don't use a stale or
 * invalid buffer without going to disk:
 *	- buffer_jbd is used liberally - if a bh is in the journal on
 *	  this node then it *must* be up to date.
 *	- the standard buffer_uptodate() macro is used to detect buffers
 *	  which may be invalid (even if we have an up to date tracking
 * 	  item for them)
 *
 * For a full understanding of how this code works together, one
 * should read the callers in dlmglue.c, the I/O functions in
 * buffer_head_io.c and ocfs2_journal_access in journal.c
 */

#include <linux/fs.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/highmem.h>
#include <linux/buffer_head.h>
#include <linux/rbtree.h>

#include <cluster/masklog.h>

#include "ocfs2.h"

#include "inode.h"
#include "uptodate.h"
#include "ocfs2_trace.h"

struct ocfs2_meta_cache_item {
	struct rb_node	c_node;
	sector_t	c_block;
};

static struct kmem_cache *ocfs2_uptodate_cachep = NULL;

u64 ocfs2_metadata_cache_owner(struct ocfs2_caching_info *ci)
{
	BUG_ON(!ci || !ci->ci_ops);

	return ci->ci_ops->co_owner(ci);
}

struct super_block *ocfs2_metadata_cache_get_super(struct ocfs2_caching_info *ci)
{
	BUG_ON(!ci || !ci->ci_ops);

	return ci->ci_ops->co_get_super(ci);
}

static void ocfs2_metadata_cache_lock(struct ocfs2_caching_info *ci)
{
	BUG_ON(!ci || !ci->ci_ops);

	ci->ci_ops->co_cache_lock(ci);
}

static void ocfs2_metadata_cache_unlock(struct ocfs2_caching_info *ci)
{
	BUG_ON(!ci || !ci->ci_ops);

	ci->ci_ops->co_cache_unlock(ci);
}

void ocfs2_metadata_cache_io_lock(struct ocfs2_caching_info *ci)
{
	BUG_ON(!ci || !ci->ci_ops);

	ci->ci_ops->co_io_lock(ci);
}

void ocfs2_metadata_cache_io_unlock(struct ocfs2_caching_info *ci)
{
	BUG_ON(!ci || !ci->ci_ops);

	ci->ci_ops->co_io_unlock(ci);
}


static void ocfs2_metadata_cache_reset(struct ocfs2_caching_info *ci,
				       int clear)
{
	ci->ci_flags |= OCFS2_CACHE_FL_INLINE;
	ci->ci_num_cached = 0;

	if (clear) {
		ci->ci_created_trans = 0;
		ci->ci_last_trans = 0;
	}
}

void ocfs2_metadata_cache_init(struct ocfs2_caching_info *ci,
			       const struct ocfs2_caching_operations *ops)
{
	BUG_ON(!ops);

	ci->ci_ops = ops;
	ocfs2_metadata_cache_reset(ci, 1);
}

void ocfs2_metadata_cache_exit(struct ocfs2_caching_info *ci)
{
	ocfs2_metadata_cache_purge(ci);
	ocfs2_metadata_cache_reset(ci, 1);
}


static unsigned int ocfs2_purge_copied_metadata_tree(struct rb_root *root)
{
	unsigned int purged = 0;
	struct rb_node *node;
	struct ocfs2_meta_cache_item *item;

	while ((node = rb_last(root)) != NULL) {
		item = rb_entry(node, struct ocfs2_meta_cache_item, c_node);

		trace_ocfs2_purge_copied_metadata_tree(
					(unsigned long long) item->c_block);

		rb_erase(&item->c_node, root);
		kmem_cache_free(ocfs2_uptodate_cachep, item);

		purged++;
	}
	return purged;
}

void ocfs2_metadata_cache_purge(struct ocfs2_caching_info *ci)
{
	unsigned int tree, to_purge, purged;
	struct rb_root root = RB_ROOT;

	BUG_ON(!ci || !ci->ci_ops);

	ocfs2_metadata_cache_lock(ci);
	tree = !(ci->ci_flags & OCFS2_CACHE_FL_INLINE);
	to_purge = ci->ci_num_cached;

	trace_ocfs2_metadata_cache_purge(
		(unsigned long long)ocfs2_metadata_cache_owner(ci),
		to_purge, tree);

	if (tree)
		root = ci->ci_cache.ci_tree;

	ocfs2_metadata_cache_reset(ci, 0);
	ocfs2_metadata_cache_unlock(ci);

	purged = ocfs2_purge_copied_metadata_tree(&root);
	if (tree && purged != to_purge)
		mlog(ML_ERROR, "Owner %llu, count = %u, purged = %u\n",
		     (unsigned long long)ocfs2_metadata_cache_owner(ci),
		     to_purge, purged);
}

static int ocfs2_search_cache_array(struct ocfs2_caching_info *ci,
				    sector_t item)
{
	int i;

	for (i = 0; i < ci->ci_num_cached; i++) {
		if (item == ci->ci_cache.ci_array[i])
			return i;
	}

	return -1;
}

static struct ocfs2_meta_cache_item *
ocfs2_search_cache_tree(struct ocfs2_caching_info *ci,
			sector_t block)
{
	struct rb_node * n = ci->ci_cache.ci_tree.rb_node;
	struct ocfs2_meta_cache_item *item = NULL;

	while (n) {
		item = rb_entry(n, struct ocfs2_meta_cache_item, c_node);

		if (block < item->c_block)
			n = n->rb_left;
		else if (block > item->c_block)
			n = n->rb_right;
		else
			return item;
	}

	return NULL;
}

static int ocfs2_buffer_cached(struct ocfs2_caching_info *ci,
			       struct buffer_head *bh)
{
	int index = -1;
	struct ocfs2_meta_cache_item *item = NULL;

	ocfs2_metadata_cache_lock(ci);

	trace_ocfs2_buffer_cached_begin(
		(unsigned long long)ocfs2_metadata_cache_owner(ci),
		(unsigned long long) bh->b_blocknr,
		!!(ci->ci_flags & OCFS2_CACHE_FL_INLINE));

	if (ci->ci_flags & OCFS2_CACHE_FL_INLINE)
		index = ocfs2_search_cache_array(ci, bh->b_blocknr);
	else
		item = ocfs2_search_cache_tree(ci, bh->b_blocknr);

	ocfs2_metadata_cache_unlock(ci);

	trace_ocfs2_buffer_cached_end(index, item);

	return (index != -1) || (item != NULL);
}

int ocfs2_buffer_uptodate(struct ocfs2_caching_info *ci,
			  struct buffer_head *bh)
{
	if (!buffer_uptodate(bh))
		return 0;

	if (buffer_jbd(bh))
		return 1;

	return ocfs2_buffer_cached(ci, bh);
}

int ocfs2_buffer_read_ahead(struct ocfs2_caching_info *ci,
			    struct buffer_head *bh)
{
	return buffer_locked(bh) && ocfs2_buffer_cached(ci, bh);
}

static void ocfs2_append_cache_array(struct ocfs2_caching_info *ci,
				     sector_t block)
{
	BUG_ON(ci->ci_num_cached >= OCFS2_CACHE_INFO_MAX_ARRAY);

	trace_ocfs2_append_cache_array(
		(unsigned long long)ocfs2_metadata_cache_owner(ci),
		(unsigned long long)block, ci->ci_num_cached);

	ci->ci_cache.ci_array[ci->ci_num_cached] = block;
	ci->ci_num_cached++;
}

static void __ocfs2_insert_cache_tree(struct ocfs2_caching_info *ci,
				      struct ocfs2_meta_cache_item *new)
{
	sector_t block = new->c_block;
	struct rb_node *parent = NULL;
	struct rb_node **p = &ci->ci_cache.ci_tree.rb_node;
	struct ocfs2_meta_cache_item *tmp;

	trace_ocfs2_insert_cache_tree(
		(unsigned long long)ocfs2_metadata_cache_owner(ci),
		(unsigned long long)block, ci->ci_num_cached);

	while(*p) {
		parent = *p;

		tmp = rb_entry(parent, struct ocfs2_meta_cache_item, c_node);

		if (block < tmp->c_block)
			p = &(*p)->rb_left;
		else if (block > tmp->c_block)
			p = &(*p)->rb_right;
		else {
			
			mlog(ML_ERROR, "Duplicate block %llu cached!\n",
			     (unsigned long long) block);
			BUG();
		}
	}

	rb_link_node(&new->c_node, parent, p);
	rb_insert_color(&new->c_node, &ci->ci_cache.ci_tree);
	ci->ci_num_cached++;
}

static inline int ocfs2_insert_can_use_array(struct ocfs2_caching_info *ci)
{
	return (ci->ci_flags & OCFS2_CACHE_FL_INLINE) &&
		(ci->ci_num_cached < OCFS2_CACHE_INFO_MAX_ARRAY);
}

static void ocfs2_expand_cache(struct ocfs2_caching_info *ci,
			       struct ocfs2_meta_cache_item **tree)
{
	int i;

	mlog_bug_on_msg(ci->ci_num_cached != OCFS2_CACHE_INFO_MAX_ARRAY,
			"Owner %llu, num cached = %u, should be %u\n",
			(unsigned long long)ocfs2_metadata_cache_owner(ci),
			ci->ci_num_cached, OCFS2_CACHE_INFO_MAX_ARRAY);
	mlog_bug_on_msg(!(ci->ci_flags & OCFS2_CACHE_FL_INLINE),
			"Owner %llu not marked as inline anymore!\n",
			(unsigned long long)ocfs2_metadata_cache_owner(ci));

	for (i = 0; i < OCFS2_CACHE_INFO_MAX_ARRAY; i++)
		tree[i]->c_block = ci->ci_cache.ci_array[i];

	ci->ci_flags &= ~OCFS2_CACHE_FL_INLINE;
	ci->ci_cache.ci_tree = RB_ROOT;
	
	ci->ci_num_cached = 0;

	for (i = 0; i < OCFS2_CACHE_INFO_MAX_ARRAY; i++) {
		__ocfs2_insert_cache_tree(ci, tree[i]);
		tree[i] = NULL;
	}

	trace_ocfs2_expand_cache(
		(unsigned long long)ocfs2_metadata_cache_owner(ci),
		ci->ci_flags, ci->ci_num_cached);
}

static void __ocfs2_set_buffer_uptodate(struct ocfs2_caching_info *ci,
					sector_t block,
					int expand_tree)
{
	int i;
	struct ocfs2_meta_cache_item *new = NULL;
	struct ocfs2_meta_cache_item *tree[OCFS2_CACHE_INFO_MAX_ARRAY] =
		{ NULL, };

	trace_ocfs2_set_buffer_uptodate(
		(unsigned long long)ocfs2_metadata_cache_owner(ci),
		(unsigned long long)block, expand_tree);

	new = kmem_cache_alloc(ocfs2_uptodate_cachep, GFP_NOFS);
	if (!new) {
		mlog_errno(-ENOMEM);
		return;
	}
	new->c_block = block;

	if (expand_tree) {
		for (i = 0; i < OCFS2_CACHE_INFO_MAX_ARRAY; i++) {
			tree[i] = kmem_cache_alloc(ocfs2_uptodate_cachep,
						   GFP_NOFS);
			if (!tree[i]) {
				mlog_errno(-ENOMEM);
				goto out_free;
			}

			
		}
	}

	ocfs2_metadata_cache_lock(ci);
	if (ocfs2_insert_can_use_array(ci)) {
		ocfs2_append_cache_array(ci, block);
		ocfs2_metadata_cache_unlock(ci);
		goto out_free;
	}

	if (expand_tree)
		ocfs2_expand_cache(ci, tree);

	__ocfs2_insert_cache_tree(ci, new);
	ocfs2_metadata_cache_unlock(ci);

	new = NULL;
out_free:
	if (new)
		kmem_cache_free(ocfs2_uptodate_cachep, new);

	if (tree[0]) {
		for (i = 0; i < OCFS2_CACHE_INFO_MAX_ARRAY; i++)
			if (tree[i])
				kmem_cache_free(ocfs2_uptodate_cachep,
						tree[i]);
	}
}

void ocfs2_set_buffer_uptodate(struct ocfs2_caching_info *ci,
			       struct buffer_head *bh)
{
	int expand;

	if (ocfs2_buffer_cached(ci, bh))
		return;

	trace_ocfs2_set_buffer_uptodate_begin(
		(unsigned long long)ocfs2_metadata_cache_owner(ci),
		(unsigned long long)bh->b_blocknr);

	ocfs2_metadata_cache_lock(ci);
	if (ocfs2_insert_can_use_array(ci)) {
		ocfs2_append_cache_array(ci, bh->b_blocknr);
		ocfs2_metadata_cache_unlock(ci);
		return;
	}

	expand = 0;
	if (ci->ci_flags & OCFS2_CACHE_FL_INLINE) {
		
		expand = 1;
	}
	ocfs2_metadata_cache_unlock(ci);

	__ocfs2_set_buffer_uptodate(ci, bh->b_blocknr, expand);
}

void ocfs2_set_new_buffer_uptodate(struct ocfs2_caching_info *ci,
				   struct buffer_head *bh)
{
	
	BUG_ON(ocfs2_buffer_cached(ci, bh));

	set_buffer_uptodate(bh);

	ocfs2_metadata_cache_io_lock(ci);
	ocfs2_set_buffer_uptodate(ci, bh);
	ocfs2_metadata_cache_io_unlock(ci);
}

static void ocfs2_remove_metadata_array(struct ocfs2_caching_info *ci,
					int index)
{
	sector_t *array = ci->ci_cache.ci_array;
	int bytes;

	BUG_ON(index < 0 || index >= OCFS2_CACHE_INFO_MAX_ARRAY);
	BUG_ON(index >= ci->ci_num_cached);
	BUG_ON(!ci->ci_num_cached);

	trace_ocfs2_remove_metadata_array(
		(unsigned long long)ocfs2_metadata_cache_owner(ci),
		index, ci->ci_num_cached);

	ci->ci_num_cached--;

	if (ci->ci_num_cached && index < ci->ci_num_cached) {
		bytes = sizeof(sector_t) * (ci->ci_num_cached - index);
		memmove(&array[index], &array[index + 1], bytes);
	}
}

static void ocfs2_remove_metadata_tree(struct ocfs2_caching_info *ci,
				       struct ocfs2_meta_cache_item *item)
{
	trace_ocfs2_remove_metadata_tree(
		(unsigned long long)ocfs2_metadata_cache_owner(ci),
		(unsigned long long)item->c_block);

	rb_erase(&item->c_node, &ci->ci_cache.ci_tree);
	ci->ci_num_cached--;
}

static void ocfs2_remove_block_from_cache(struct ocfs2_caching_info *ci,
					  sector_t block)
{
	int index;
	struct ocfs2_meta_cache_item *item = NULL;

	ocfs2_metadata_cache_lock(ci);
	trace_ocfs2_remove_block_from_cache(
		(unsigned long long)ocfs2_metadata_cache_owner(ci),
		(unsigned long long) block, ci->ci_num_cached,
		ci->ci_flags);

	if (ci->ci_flags & OCFS2_CACHE_FL_INLINE) {
		index = ocfs2_search_cache_array(ci, block);
		if (index != -1)
			ocfs2_remove_metadata_array(ci, index);
	} else {
		item = ocfs2_search_cache_tree(ci, block);
		if (item)
			ocfs2_remove_metadata_tree(ci, item);
	}
	ocfs2_metadata_cache_unlock(ci);

	if (item)
		kmem_cache_free(ocfs2_uptodate_cachep, item);
}

void ocfs2_remove_from_cache(struct ocfs2_caching_info *ci,
			     struct buffer_head *bh)
{
	sector_t block = bh->b_blocknr;

	ocfs2_remove_block_from_cache(ci, block);
}

void ocfs2_remove_xattr_clusters_from_cache(struct ocfs2_caching_info *ci,
					    sector_t block,
					    u32 c_len)
{
	struct super_block *sb = ocfs2_metadata_cache_get_super(ci);
	unsigned int i, b_len = ocfs2_clusters_to_blocks(sb, 1) * c_len;

	for (i = 0; i < b_len; i++, block++)
		ocfs2_remove_block_from_cache(ci, block);
}

int __init init_ocfs2_uptodate_cache(void)
{
	ocfs2_uptodate_cachep = kmem_cache_create("ocfs2_uptodate",
				  sizeof(struct ocfs2_meta_cache_item),
				  0, SLAB_HWCACHE_ALIGN, NULL);
	if (!ocfs2_uptodate_cachep)
		return -ENOMEM;

	return 0;
}

void exit_ocfs2_uptodate_cache(void)
{
	if (ocfs2_uptodate_cachep)
		kmem_cache_destroy(ocfs2_uptodate_cachep);
}
