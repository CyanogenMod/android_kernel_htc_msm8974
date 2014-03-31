/* -*- mode: c; c-basic-offset: 8; -*-
 * vim: noexpandtab sw=8 ts=8 sts=0:
 *
 * reservations.c
 *
 * Allocation reservations implementation
 *
 * Some code borrowed from fs/ext3/balloc.c and is:
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 * The rest is copyright (C) 2010 Novell.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/fs.h>
#include <linux/types.h>
#include <linux/highmem.h>
#include <linux/bitops.h>
#include <linux/list.h>

#include <cluster/masklog.h>

#include "ocfs2.h"
#include "ocfs2_trace.h"

#ifdef CONFIG_OCFS2_DEBUG_FS
#define OCFS2_CHECK_RESERVATIONS
#endif

DEFINE_SPINLOCK(resv_lock);

#define	OCFS2_MIN_RESV_WINDOW_BITS	8
#define	OCFS2_MAX_RESV_WINDOW_BITS	1024

int ocfs2_dir_resv_allowed(struct ocfs2_super *osb)
{
	return (osb->osb_resv_level && osb->osb_dir_resv_level);
}

static unsigned int ocfs2_resv_window_bits(struct ocfs2_reservation_map *resmap,
					   struct ocfs2_alloc_reservation *resv)
{
	struct ocfs2_super *osb = resmap->m_osb;
	unsigned int bits;

	if (!(resv->r_flags & OCFS2_RESV_FLAG_DIR)) {
		
		bits = 4 << osb->osb_resv_level;
	} else {
		bits = 4 << osb->osb_dir_resv_level;
	}
	return bits;
}

static inline unsigned int ocfs2_resv_end(struct ocfs2_alloc_reservation *resv)
{
	if (resv->r_len)
		return resv->r_start + resv->r_len - 1;
	return resv->r_start;
}

static inline int ocfs2_resv_empty(struct ocfs2_alloc_reservation *resv)
{
	return !!(resv->r_len == 0);
}

static inline int ocfs2_resmap_disabled(struct ocfs2_reservation_map *resmap)
{
	if (resmap->m_osb->osb_resv_level == 0)
		return 1;
	return 0;
}

static void ocfs2_dump_resv(struct ocfs2_reservation_map *resmap)
{
	struct ocfs2_super *osb = resmap->m_osb;
	struct rb_node *node;
	struct ocfs2_alloc_reservation *resv;
	int i = 0;

	mlog(ML_NOTICE, "Dumping resmap for device %s. Bitmap length: %u\n",
	     osb->dev_str, resmap->m_bitmap_len);

	node = rb_first(&resmap->m_reservations);
	while (node) {
		resv = rb_entry(node, struct ocfs2_alloc_reservation, r_node);

		mlog(ML_NOTICE, "start: %u\tend: %u\tlen: %u\tlast_start: %u"
		     "\tlast_len: %u\n", resv->r_start,
		     ocfs2_resv_end(resv), resv->r_len, resv->r_last_start,
		     resv->r_last_len);

		node = rb_next(node);
		i++;
	}

	mlog(ML_NOTICE, "%d reservations found. LRU follows\n", i);

	i = 0;
	list_for_each_entry(resv, &resmap->m_lru, r_lru) {
		mlog(ML_NOTICE, "LRU(%d) start: %u\tend: %u\tlen: %u\t"
		     "last_start: %u\tlast_len: %u\n", i, resv->r_start,
		     ocfs2_resv_end(resv), resv->r_len, resv->r_last_start,
		     resv->r_last_len);

		i++;
	}
}

#ifdef OCFS2_CHECK_RESERVATIONS
static int ocfs2_validate_resmap_bits(struct ocfs2_reservation_map *resmap,
				      int i,
				      struct ocfs2_alloc_reservation *resv)
{
	char *disk_bitmap = resmap->m_disk_bitmap;
	unsigned int start = resv->r_start;
	unsigned int end = ocfs2_resv_end(resv);

	while (start <= end) {
		if (ocfs2_test_bit(start, disk_bitmap)) {
			mlog(ML_ERROR,
			     "reservation %d covers an allocated area "
			     "starting at bit %u!\n", i, start);
			return 1;
		}

		start++;
	}
	return 0;
}

static void ocfs2_check_resmap(struct ocfs2_reservation_map *resmap)
{
	unsigned int off = 0;
	int i = 0;
	struct rb_node *node;
	struct ocfs2_alloc_reservation *resv;

	node = rb_first(&resmap->m_reservations);
	while (node) {
		resv = rb_entry(node, struct ocfs2_alloc_reservation, r_node);

		if (i > 0 && resv->r_start <= off) {
			mlog(ML_ERROR, "reservation %d has bad start off!\n",
			     i);
			goto bad;
		}

		if (resv->r_len == 0) {
			mlog(ML_ERROR, "reservation %d has no length!\n",
			     i);
			goto bad;
		}

		if (resv->r_start > ocfs2_resv_end(resv)) {
			mlog(ML_ERROR, "reservation %d has invalid range!\n",
			     i);
			goto bad;
		}

		if (ocfs2_resv_end(resv) >= resmap->m_bitmap_len) {
			mlog(ML_ERROR, "reservation %d extends past bitmap!\n",
			     i);
			goto bad;
		}

		if (ocfs2_validate_resmap_bits(resmap, i, resv))
			goto bad;

		off = ocfs2_resv_end(resv);
		node = rb_next(node);

		i++;
	}
	return;

bad:
	ocfs2_dump_resv(resmap);
	BUG();
}
#else
static inline void ocfs2_check_resmap(struct ocfs2_reservation_map *resmap)
{

}
#endif

void ocfs2_resv_init_once(struct ocfs2_alloc_reservation *resv)
{
	memset(resv, 0, sizeof(*resv));
	INIT_LIST_HEAD(&resv->r_lru);
}

void ocfs2_resv_set_type(struct ocfs2_alloc_reservation *resv,
			 unsigned int flags)
{
	BUG_ON(flags & ~OCFS2_RESV_TYPES);

	resv->r_flags |= flags;
}

int ocfs2_resmap_init(struct ocfs2_super *osb,
		      struct ocfs2_reservation_map *resmap)
{
	memset(resmap, 0, sizeof(*resmap));

	resmap->m_osb = osb;
	resmap->m_reservations = RB_ROOT;
	
	INIT_LIST_HEAD(&resmap->m_lru);

	return 0;
}

static void ocfs2_resv_mark_lru(struct ocfs2_reservation_map *resmap,
				struct ocfs2_alloc_reservation *resv)
{
	assert_spin_locked(&resv_lock);

	if (!list_empty(&resv->r_lru))
		list_del_init(&resv->r_lru);

	list_add_tail(&resv->r_lru, &resmap->m_lru);
}

static void __ocfs2_resv_trunc(struct ocfs2_alloc_reservation *resv)
{
	resv->r_len = 0;
	resv->r_start = 0;
}

static void ocfs2_resv_remove(struct ocfs2_reservation_map *resmap,
			      struct ocfs2_alloc_reservation *resv)
{
	if (resv->r_flags & OCFS2_RESV_FLAG_INUSE) {
		list_del_init(&resv->r_lru);
		rb_erase(&resv->r_node, &resmap->m_reservations);
		resv->r_flags &= ~OCFS2_RESV_FLAG_INUSE;
	}
}

static void __ocfs2_resv_discard(struct ocfs2_reservation_map *resmap,
				 struct ocfs2_alloc_reservation *resv)
{
	assert_spin_locked(&resv_lock);

	__ocfs2_resv_trunc(resv);
	resv->r_last_len = resv->r_last_start = 0;

	ocfs2_resv_remove(resmap, resv);
}

void ocfs2_resv_discard(struct ocfs2_reservation_map *resmap,
			struct ocfs2_alloc_reservation *resv)
{
	if (resv) {
		spin_lock(&resv_lock);
		__ocfs2_resv_discard(resmap, resv);
		spin_unlock(&resv_lock);
	}
}

static void ocfs2_resmap_clear_all_resv(struct ocfs2_reservation_map *resmap)
{
	struct rb_node *node;
	struct ocfs2_alloc_reservation *resv;

	assert_spin_locked(&resv_lock);

	while ((node = rb_last(&resmap->m_reservations)) != NULL) {
		resv = rb_entry(node, struct ocfs2_alloc_reservation, r_node);

		__ocfs2_resv_discard(resmap, resv);
	}
}

void ocfs2_resmap_restart(struct ocfs2_reservation_map *resmap,
			  unsigned int clen, char *disk_bitmap)
{
	if (ocfs2_resmap_disabled(resmap))
		return;

	spin_lock(&resv_lock);

	ocfs2_resmap_clear_all_resv(resmap);
	resmap->m_bitmap_len = clen;
	resmap->m_disk_bitmap = disk_bitmap;

	spin_unlock(&resv_lock);
}

void ocfs2_resmap_uninit(struct ocfs2_reservation_map *resmap)
{
	
}

static void ocfs2_resv_insert(struct ocfs2_reservation_map *resmap,
			      struct ocfs2_alloc_reservation *new)
{
	struct rb_root *root = &resmap->m_reservations;
	struct rb_node *parent = NULL;
	struct rb_node **p = &root->rb_node;
	struct ocfs2_alloc_reservation *tmp;

	assert_spin_locked(&resv_lock);

	trace_ocfs2_resv_insert(new->r_start, new->r_len);

	while (*p) {
		parent = *p;

		tmp = rb_entry(parent, struct ocfs2_alloc_reservation, r_node);

		if (new->r_start < tmp->r_start) {
			p = &(*p)->rb_left;

			BUG_ON(ocfs2_resv_end(new) >= tmp->r_start);
		} else if (new->r_start > ocfs2_resv_end(tmp)) {
			p = &(*p)->rb_right;
		} else {
			
			mlog(ML_ERROR, "Duplicate reservation window!\n");
			BUG();
		}
	}

	rb_link_node(&new->r_node, parent, p);
	rb_insert_color(&new->r_node, root);
	new->r_flags |= OCFS2_RESV_FLAG_INUSE;

	ocfs2_resv_mark_lru(resmap, new);

	ocfs2_check_resmap(resmap);
}

static struct ocfs2_alloc_reservation *
ocfs2_find_resv_lhs(struct ocfs2_reservation_map *resmap, unsigned int goal)
{
	struct ocfs2_alloc_reservation *resv = NULL;
	struct ocfs2_alloc_reservation *prev_resv = NULL;
	struct rb_node *node = resmap->m_reservations.rb_node;

	assert_spin_locked(&resv_lock);

	if (!node)
		return NULL;

	node = rb_first(&resmap->m_reservations);
	while (node) {
		resv = rb_entry(node, struct ocfs2_alloc_reservation, r_node);

		if (resv->r_start <= goal && ocfs2_resv_end(resv) >= goal)
			break;

		
		if (resv->r_start > goal) {
			resv = prev_resv;
			break;
		}

		prev_resv = resv;
		node = rb_next(node);
	}

	return resv;
}

static int ocfs2_resmap_find_free_bits(struct ocfs2_reservation_map *resmap,
				       unsigned int wanted,
				       unsigned int search_start,
				       unsigned int search_len,
				       unsigned int *rstart,
				       unsigned int *rlen)
{
	void *bitmap = resmap->m_disk_bitmap;
	unsigned int best_start, best_len = 0;
	int offset, start, found;

	trace_ocfs2_resmap_find_free_bits_begin(search_start, search_len,
						wanted, resmap->m_bitmap_len);

	found = best_start = best_len = 0;

	start = search_start;
	while ((offset = ocfs2_find_next_zero_bit(bitmap, resmap->m_bitmap_len,
						 start)) != -1) {
		
		if (offset >= (search_start + search_len))
			break;

		if (offset == start) {
			
			found++;
			
			start++;
		} else {
			
			found = 1;
			start = offset + 1;
		}
		if (found > best_len) {
			best_len = found;
			best_start = start - found;
		}

		if (found >= wanted)
			break;
	}

	if (best_len == 0)
		return 0;

	if (best_len >= wanted)
		best_len = wanted;

	*rlen = best_len;
	*rstart = best_start;

	trace_ocfs2_resmap_find_free_bits_end(best_start, best_len);

	return *rlen;
}

static void __ocfs2_resv_find_window(struct ocfs2_reservation_map *resmap,
				     struct ocfs2_alloc_reservation *resv,
				     unsigned int goal, unsigned int wanted)
{
	struct rb_root *root = &resmap->m_reservations;
	unsigned int gap_start, gap_end, gap_len;
	struct ocfs2_alloc_reservation *prev_resv, *next_resv;
	struct rb_node *prev, *next;
	unsigned int cstart, clen;
	unsigned int best_start = 0, best_len = 0;

	trace_ocfs2_resv_find_window_begin(resv->r_start, ocfs2_resv_end(resv),
					   goal, wanted, RB_EMPTY_ROOT(root));

	assert_spin_locked(&resv_lock);

	if (RB_EMPTY_ROOT(root)) {
		clen = ocfs2_resmap_find_free_bits(resmap, wanted, goal,
						   resmap->m_bitmap_len - goal,
						   &cstart, &clen);

		BUG_ON(goal == 0 && clen == 0);

		if (clen == 0)
			return;

		resv->r_start = cstart;
		resv->r_len = clen;

		ocfs2_resv_insert(resmap, resv);
		return;
	}

	prev_resv = ocfs2_find_resv_lhs(resmap, goal);

	if (prev_resv == NULL) {

		next = rb_first(root);
		next_resv = rb_entry(next, struct ocfs2_alloc_reservation,
				     r_node);

		if (next_resv->r_start <= goal) {
			mlog(ML_ERROR, "goal: %u next_resv: start %u len %u\n",
			     goal, next_resv->r_start, next_resv->r_len);
			ocfs2_dump_resv(resmap);
			BUG();
		}

		clen = ocfs2_resmap_find_free_bits(resmap, wanted, goal,
						   next_resv->r_start - goal,
						   &cstart, &clen);
		if (clen) {
			best_len = clen;
			best_start = cstart;
			if (best_len == wanted)
				goto out_insert;
		}

		prev_resv = next_resv;
		next_resv = NULL;
	}

	trace_ocfs2_resv_find_window_prev(prev_resv->r_start,
					  ocfs2_resv_end(prev_resv));

	prev = &prev_resv->r_node;

	
	while (1) {
		next = rb_next(prev);
		if (next) {
			next_resv = rb_entry(next,
					     struct ocfs2_alloc_reservation,
					     r_node);

			gap_start = ocfs2_resv_end(prev_resv) + 1;
			gap_end = next_resv->r_start - 1;
			gap_len = gap_end - gap_start + 1;
		} else {
			gap_start = ocfs2_resv_end(prev_resv) + 1;
			gap_len = resmap->m_bitmap_len - gap_start;
			gap_end = resmap->m_bitmap_len - 1;
		}

		trace_ocfs2_resv_find_window_next(next ? next_resv->r_start: -1,
					next ? ocfs2_resv_end(next_resv) : -1);
		if (gap_len <= best_len)
			goto next_resv;

		clen = ocfs2_resmap_find_free_bits(resmap, wanted, gap_start,
						   gap_len, &cstart, &clen);
		if (clen == wanted) {
			best_len = clen;
			best_start = cstart;
			goto out_insert;
		} else if (clen > best_len) {
			best_len = clen;
			best_start = cstart;
		}

next_resv:
		if (!next)
			break;

		prev = next;
		prev_resv = rb_entry(prev, struct ocfs2_alloc_reservation,
				     r_node);
	}

out_insert:
	if (best_len) {
		resv->r_start = best_start;
		resv->r_len = best_len;
		ocfs2_resv_insert(resmap, resv);
	}
}

static void ocfs2_cannibalize_resv(struct ocfs2_reservation_map *resmap,
				   struct ocfs2_alloc_reservation *resv,
				   unsigned int wanted)
{
	struct ocfs2_alloc_reservation *lru_resv;
	int tmpwindow = !!(resv->r_flags & OCFS2_RESV_FLAG_TMP);
	unsigned int min_bits;

	if (!tmpwindow)
		min_bits = ocfs2_resv_window_bits(resmap, resv) >> 1;
	else
		min_bits = wanted; 

	lru_resv = list_first_entry(&resmap->m_lru,
				    struct ocfs2_alloc_reservation, r_lru);

	trace_ocfs2_cannibalize_resv_begin(lru_resv->r_start,
					   lru_resv->r_len,
					   ocfs2_resv_end(lru_resv));

	if (lru_resv->r_len <= min_bits) {
		resv->r_start = lru_resv->r_start;
		resv->r_len = lru_resv->r_len;

		__ocfs2_resv_discard(resmap, lru_resv);
	} else {
		unsigned int shrink;
		if (tmpwindow)
			shrink = min_bits;
		else
			shrink = lru_resv->r_len / 2;

		lru_resv->r_len -= shrink;

		resv->r_start = ocfs2_resv_end(lru_resv) + 1;
		resv->r_len = shrink;
	}

	trace_ocfs2_cannibalize_resv_end(resv->r_start, ocfs2_resv_end(resv),
					 resv->r_len, resv->r_last_start,
					 resv->r_last_len);

	ocfs2_resv_insert(resmap, resv);
}

static void ocfs2_resv_find_window(struct ocfs2_reservation_map *resmap,
				   struct ocfs2_alloc_reservation *resv,
				   unsigned int wanted)
{
	unsigned int goal = 0;

	BUG_ON(!ocfs2_resv_empty(resv));

	if (resv->r_last_len) {
		goal = resv->r_last_start + resv->r_last_len;
		if (goal >= resmap->m_bitmap_len)
			goal = 0;
	}

	__ocfs2_resv_find_window(resmap, resv, goal, wanted);

	
	if (ocfs2_resv_empty(resv) && goal != 0)
		__ocfs2_resv_find_window(resmap, resv, 0, wanted);

	if (ocfs2_resv_empty(resv)) {
		ocfs2_cannibalize_resv(resmap, resv, wanted);
	}

	BUG_ON(ocfs2_resv_empty(resv));
}

int ocfs2_resmap_resv_bits(struct ocfs2_reservation_map *resmap,
			   struct ocfs2_alloc_reservation *resv,
			   int *cstart, int *clen)
{
	if (resv == NULL || ocfs2_resmap_disabled(resmap))
		return -ENOSPC;

	spin_lock(&resv_lock);

	if (ocfs2_resv_empty(resv)) {
		unsigned int wanted = ocfs2_resv_window_bits(resmap, resv);

		if ((resv->r_flags & OCFS2_RESV_FLAG_TMP) || wanted < *clen)
			wanted = *clen;

		ocfs2_resv_find_window(resmap, resv, wanted);
		trace_ocfs2_resmap_resv_bits(resv->r_start, resv->r_len);
	}

	BUG_ON(ocfs2_resv_empty(resv));

	*cstart = resv->r_start;
	*clen = resv->r_len;

	spin_unlock(&resv_lock);
	return 0;
}

static void
	ocfs2_adjust_resv_from_alloc(struct ocfs2_reservation_map *resmap,
				     struct ocfs2_alloc_reservation *resv,
				     unsigned int start, unsigned int end)
{
	unsigned int rhs = 0;
	unsigned int old_end = ocfs2_resv_end(resv);

	BUG_ON(start != resv->r_start || old_end < end);

	if (old_end == end) {
		__ocfs2_resv_discard(resmap, resv);
		return;
	}

	rhs = old_end - end;

	BUG_ON(rhs == 0);

	resv->r_start = end + 1;
	resv->r_len = old_end - resv->r_start + 1;
}

void ocfs2_resmap_claimed_bits(struct ocfs2_reservation_map *resmap,
			       struct ocfs2_alloc_reservation *resv,
			       u32 cstart, u32 clen)
{
	unsigned int cend = cstart + clen - 1;

	if (resmap == NULL || ocfs2_resmap_disabled(resmap))
		return;

	if (resv == NULL)
		return;

	BUG_ON(cstart != resv->r_start);

	spin_lock(&resv_lock);

	trace_ocfs2_resmap_claimed_bits_begin(cstart, cend, clen, resv->r_start,
					      ocfs2_resv_end(resv), resv->r_len,
					      resv->r_last_start,
					      resv->r_last_len);

	BUG_ON(cstart < resv->r_start);
	BUG_ON(cstart > ocfs2_resv_end(resv));
	BUG_ON(cend > ocfs2_resv_end(resv));

	ocfs2_adjust_resv_from_alloc(resmap, resv, cstart, cend);
	resv->r_last_start = cstart;
	resv->r_last_len = clen;

	if (!ocfs2_resv_empty(resv))
		ocfs2_resv_mark_lru(resmap, resv);

	trace_ocfs2_resmap_claimed_bits_end(resv->r_start, ocfs2_resv_end(resv),
					    resv->r_len, resv->r_last_start,
					    resv->r_last_len);

	ocfs2_check_resmap(resmap);

	spin_unlock(&resv_lock);
}
