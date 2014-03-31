/*
 * This file is part of UBIFS.
 *
 * Copyright (C) 2006-2008 Nokia Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Authors: Artem Bityutskiy (Битюцкий Артём)
 *          Adrian Hunter
 */


#include <linux/sort.h>
#include "ubifs.h"

struct scan_data {
	int min_space;
	int pick_free;
	int lnum;
	int exclude_index;
};

static int valuable(struct ubifs_info *c, const struct ubifs_lprops *lprops)
{
	int n, cat = lprops->flags & LPROPS_CAT_MASK;
	struct ubifs_lpt_heap *heap;

	switch (cat) {
	case LPROPS_DIRTY:
	case LPROPS_DIRTY_IDX:
	case LPROPS_FREE:
		heap = &c->lpt_heap[cat - 1];
		if (heap->cnt < heap->max_cnt)
			return 1;
		if (lprops->free + lprops->dirty >= c->dark_wm)
			return 1;
		return 0;
	case LPROPS_EMPTY:
		n = c->lst.empty_lebs + c->freeable_cnt -
		    c->lst.taken_empty_lebs;
		if (n < c->lsave_cnt)
			return 1;
		return 0;
	case LPROPS_FREEABLE:
		return 1;
	case LPROPS_FRDI_IDX:
		return 1;
	}
	return 0;
}

static int scan_for_dirty_cb(struct ubifs_info *c,
			     const struct ubifs_lprops *lprops, int in_tree,
			     struct scan_data *data)
{
	int ret = LPT_SCAN_CONTINUE;

	
	if (lprops->flags & LPROPS_TAKEN)
		return LPT_SCAN_CONTINUE;
	
	if (!in_tree && valuable(c, lprops))
		ret |= LPT_SCAN_ADD;
	
	if (lprops->free + lprops->dirty < data->min_space)
		return ret;
	
	if (data->exclude_index && lprops->flags & LPROPS_INDEX)
		return ret;
	
	if (lprops->free + lprops->dirty == c->leb_size) {
		if (!data->pick_free)
			return ret;
	
	} else if (lprops->dirty < c->dead_wm)
		return ret;
	
	data->lnum = lprops->lnum;
	return LPT_SCAN_ADD | LPT_SCAN_STOP;
}

static const struct ubifs_lprops *scan_for_dirty(struct ubifs_info *c,
						 int min_space, int pick_free,
						 int exclude_index)
{
	const struct ubifs_lprops *lprops;
	struct ubifs_lpt_heap *heap;
	struct scan_data data;
	int err, i;

	
	heap = &c->lpt_heap[LPROPS_FREE - 1];
	for (i = 0; i < heap->cnt; i++) {
		lprops = heap->arr[i];
		if (lprops->free + lprops->dirty < min_space)
			continue;
		if (lprops->dirty < c->dead_wm)
			continue;
		return lprops;
	}
	list_for_each_entry(lprops, &c->uncat_list, list) {
		if (lprops->flags & LPROPS_TAKEN)
			continue;
		if (lprops->free + lprops->dirty < min_space)
			continue;
		if (exclude_index && (lprops->flags & LPROPS_INDEX))
			continue;
		if (lprops->dirty < c->dead_wm)
			continue;
		return lprops;
	}
	
	if (c->pnodes_have >= c->pnode_cnt)
		
		return ERR_PTR(-ENOSPC);
	data.min_space = min_space;
	data.pick_free = pick_free;
	data.lnum = -1;
	data.exclude_index = exclude_index;
	err = ubifs_lpt_scan_nolock(c, -1, c->lscan_lnum,
				    (ubifs_lpt_scan_callback)scan_for_dirty_cb,
				    &data);
	if (err)
		return ERR_PTR(err);
	ubifs_assert(data.lnum >= c->main_first && data.lnum < c->leb_cnt);
	c->lscan_lnum = data.lnum;
	lprops = ubifs_lpt_lookup_dirty(c, data.lnum);
	if (IS_ERR(lprops))
		return lprops;
	ubifs_assert(lprops->lnum == data.lnum);
	ubifs_assert(lprops->free + lprops->dirty >= min_space);
	ubifs_assert(lprops->dirty >= c->dead_wm ||
		     (pick_free &&
		      lprops->free + lprops->dirty == c->leb_size));
	ubifs_assert(!(lprops->flags & LPROPS_TAKEN));
	ubifs_assert(!exclude_index || !(lprops->flags & LPROPS_INDEX));
	return lprops;
}

int ubifs_find_dirty_leb(struct ubifs_info *c, struct ubifs_lprops *ret_lp,
			 int min_space, int pick_free)
{
	int err = 0, sum, exclude_index = pick_free == 2 ? 1 : 0;
	const struct ubifs_lprops *lp = NULL, *idx_lp = NULL;
	struct ubifs_lpt_heap *heap, *idx_heap;

	ubifs_get_lprops(c);

	if (pick_free) {
		int lebs, rsvd_idx_lebs = 0;

		spin_lock(&c->space_lock);
		lebs = c->lst.empty_lebs + c->idx_gc_cnt;
		lebs += c->freeable_cnt - c->lst.taken_empty_lebs;

		if (c->bi.min_idx_lebs >= c->lst.idx_lebs) {
			rsvd_idx_lebs = c->bi.min_idx_lebs -  c->lst.idx_lebs;
			exclude_index = 1;
		}
		spin_unlock(&c->space_lock);

		
		if (rsvd_idx_lebs < lebs) {
			
			lp = ubifs_fast_find_empty(c);
			if (lp)
				goto found;

			
			lp = ubifs_fast_find_freeable(c);
			if (lp)
				goto found;
		} else
			pick_free = 0;
	} else {
		spin_lock(&c->space_lock);
		exclude_index = (c->bi.min_idx_lebs >= c->lst.idx_lebs);
		spin_unlock(&c->space_lock);
	}

	
	heap = &c->lpt_heap[LPROPS_DIRTY - 1];
	idx_heap = &c->lpt_heap[LPROPS_DIRTY_IDX - 1];

	if (idx_heap->cnt && !exclude_index) {
		idx_lp = idx_heap->arr[0];
		sum = idx_lp->free + idx_lp->dirty;
		if (sum < min_space || sum < c->half_leb_size)
			idx_lp = NULL;
	}

	if (heap->cnt) {
		lp = heap->arr[0];
		if (lp->dirty + lp->free < min_space)
			lp = NULL;
	}

	
	if (idx_lp && lp) {
		if (idx_lp->free + idx_lp->dirty >= lp->free + lp->dirty)
			lp = idx_lp;
	} else if (idx_lp && !lp)
		lp = idx_lp;

	if (lp) {
		ubifs_assert(lp->free + lp->dirty >= c->dead_wm);
		goto found;
	}

	
	dbg_find("scanning LPT for a dirty LEB");
	lp = scan_for_dirty(c, min_space, pick_free, exclude_index);
	if (IS_ERR(lp)) {
		err = PTR_ERR(lp);
		goto out;
	}
	ubifs_assert(lp->dirty >= c->dead_wm ||
		     (pick_free && lp->free + lp->dirty == c->leb_size));

found:
	dbg_find("found LEB %d, free %d, dirty %d, flags %#x",
		 lp->lnum, lp->free, lp->dirty, lp->flags);

	lp = ubifs_change_lp(c, lp, LPROPS_NC, LPROPS_NC,
			     lp->flags | LPROPS_TAKEN, 0);
	if (IS_ERR(lp)) {
		err = PTR_ERR(lp);
		goto out;
	}

	memcpy(ret_lp, lp, sizeof(struct ubifs_lprops));

out:
	ubifs_release_lprops(c);
	return err;
}

static int scan_for_free_cb(struct ubifs_info *c,
			    const struct ubifs_lprops *lprops, int in_tree,
			    struct scan_data *data)
{
	int ret = LPT_SCAN_CONTINUE;

	
	if (lprops->flags & LPROPS_TAKEN)
		return LPT_SCAN_CONTINUE;
	
	if (!in_tree && valuable(c, lprops))
		ret |= LPT_SCAN_ADD;
	
	if (lprops->flags & LPROPS_INDEX)
		return ret;
	
	if (lprops->free < data->min_space)
		return ret;
	
	if (!data->pick_free && lprops->free == c->leb_size)
		return ret;
	if (lprops->free + lprops->dirty == c->leb_size && lprops->dirty > 0)
		return ret;
	
	data->lnum = lprops->lnum;
	return LPT_SCAN_ADD | LPT_SCAN_STOP;
}

static
const struct ubifs_lprops *do_find_free_space(struct ubifs_info *c,
					      int min_space, int pick_free,
					      int squeeze)
{
	const struct ubifs_lprops *lprops;
	struct ubifs_lpt_heap *heap;
	struct scan_data data;
	int err, i;

	if (squeeze) {
		lprops = ubifs_fast_find_free(c);
		if (lprops && lprops->free >= min_space)
			return lprops;
	}
	if (pick_free) {
		lprops = ubifs_fast_find_empty(c);
		if (lprops)
			return lprops;
	}
	if (!squeeze) {
		lprops = ubifs_fast_find_free(c);
		if (lprops && lprops->free >= min_space)
			return lprops;
	}
	
	heap = &c->lpt_heap[LPROPS_DIRTY - 1];
	for (i = 0; i < heap->cnt; i++) {
		lprops = heap->arr[i];
		if (lprops->free >= min_space)
			return lprops;
	}
	list_for_each_entry(lprops, &c->uncat_list, list) {
		if (lprops->flags & LPROPS_TAKEN)
			continue;
		if (lprops->flags & LPROPS_INDEX)
			continue;
		if (lprops->free >= min_space)
			return lprops;
	}
	
	if (c->pnodes_have >= c->pnode_cnt)
		
		return ERR_PTR(-ENOSPC);
	data.min_space = min_space;
	data.pick_free = pick_free;
	data.lnum = -1;
	err = ubifs_lpt_scan_nolock(c, -1, c->lscan_lnum,
				    (ubifs_lpt_scan_callback)scan_for_free_cb,
				    &data);
	if (err)
		return ERR_PTR(err);
	ubifs_assert(data.lnum >= c->main_first && data.lnum < c->leb_cnt);
	c->lscan_lnum = data.lnum;
	lprops = ubifs_lpt_lookup_dirty(c, data.lnum);
	if (IS_ERR(lprops))
		return lprops;
	ubifs_assert(lprops->lnum == data.lnum);
	ubifs_assert(lprops->free >= min_space);
	ubifs_assert(!(lprops->flags & LPROPS_TAKEN));
	ubifs_assert(!(lprops->flags & LPROPS_INDEX));
	return lprops;
}

int ubifs_find_free_space(struct ubifs_info *c, int min_space, int *offs,
			  int squeeze)
{
	const struct ubifs_lprops *lprops;
	int lebs, rsvd_idx_lebs, pick_free = 0, err, lnum, flags;

	dbg_find("min_space %d", min_space);
	ubifs_get_lprops(c);

	
	spin_lock(&c->space_lock);
	if (c->bi.min_idx_lebs > c->lst.idx_lebs)
		rsvd_idx_lebs = c->bi.min_idx_lebs -  c->lst.idx_lebs;
	else
		rsvd_idx_lebs = 0;
	lebs = c->lst.empty_lebs + c->freeable_cnt + c->idx_gc_cnt -
	       c->lst.taken_empty_lebs;
	if (rsvd_idx_lebs < lebs)
		if (c->lst.empty_lebs - c->lst.taken_empty_lebs > 0) {
			pick_free = 1;
			c->lst.taken_empty_lebs += 1;
		}
	spin_unlock(&c->space_lock);

	lprops = do_find_free_space(c, min_space, pick_free, squeeze);
	if (IS_ERR(lprops)) {
		err = PTR_ERR(lprops);
		goto out;
	}

	lnum = lprops->lnum;
	flags = lprops->flags | LPROPS_TAKEN;

	lprops = ubifs_change_lp(c, lprops, LPROPS_NC, LPROPS_NC, flags, 0);
	if (IS_ERR(lprops)) {
		err = PTR_ERR(lprops);
		goto out;
	}

	if (pick_free) {
		spin_lock(&c->space_lock);
		c->lst.taken_empty_lebs -= 1;
		spin_unlock(&c->space_lock);
	}

	*offs = c->leb_size - lprops->free;
	ubifs_release_lprops(c);

	if (*offs == 0) {
		err = ubifs_leb_unmap(c, lnum);
		if (err)
			return err;
	}

	dbg_find("found LEB %d, free %d", lnum, c->leb_size - *offs);
	ubifs_assert(*offs <= c->leb_size - min_space);
	return lnum;

out:
	if (pick_free) {
		spin_lock(&c->space_lock);
		c->lst.taken_empty_lebs -= 1;
		spin_unlock(&c->space_lock);
	}
	ubifs_release_lprops(c);
	return err;
}

static int scan_for_idx_cb(struct ubifs_info *c,
			   const struct ubifs_lprops *lprops, int in_tree,
			   struct scan_data *data)
{
	int ret = LPT_SCAN_CONTINUE;

	
	if (lprops->flags & LPROPS_TAKEN)
		return LPT_SCAN_CONTINUE;
	
	if (!in_tree && valuable(c, lprops))
		ret |= LPT_SCAN_ADD;
	
	if (lprops->flags & LPROPS_INDEX)
		return ret;
	
	if (lprops->free + lprops->dirty != c->leb_size)
		return ret;
	data->lnum = lprops->lnum;
	return LPT_SCAN_ADD | LPT_SCAN_STOP;
}

static const struct ubifs_lprops *scan_for_leb_for_idx(struct ubifs_info *c)
{
	struct ubifs_lprops *lprops;
	struct scan_data data;
	int err;

	data.lnum = -1;
	err = ubifs_lpt_scan_nolock(c, -1, c->lscan_lnum,
				    (ubifs_lpt_scan_callback)scan_for_idx_cb,
				    &data);
	if (err)
		return ERR_PTR(err);
	ubifs_assert(data.lnum >= c->main_first && data.lnum < c->leb_cnt);
	c->lscan_lnum = data.lnum;
	lprops = ubifs_lpt_lookup_dirty(c, data.lnum);
	if (IS_ERR(lprops))
		return lprops;
	ubifs_assert(lprops->lnum == data.lnum);
	ubifs_assert(lprops->free + lprops->dirty == c->leb_size);
	ubifs_assert(!(lprops->flags & LPROPS_TAKEN));
	ubifs_assert(!(lprops->flags & LPROPS_INDEX));
	return lprops;
}

int ubifs_find_free_leb_for_idx(struct ubifs_info *c)
{
	const struct ubifs_lprops *lprops;
	int lnum = -1, err, flags;

	ubifs_get_lprops(c);

	lprops = ubifs_fast_find_empty(c);
	if (!lprops) {
		lprops = ubifs_fast_find_freeable(c);
		if (!lprops) {
			ubifs_assert(c->freeable_cnt == 0);
			if (c->lst.empty_lebs - c->lst.taken_empty_lebs > 0) {
				lprops = scan_for_leb_for_idx(c);
				if (IS_ERR(lprops)) {
					err = PTR_ERR(lprops);
					goto out;
				}
			}
		}
	}

	if (!lprops) {
		err = -ENOSPC;
		goto out;
	}

	lnum = lprops->lnum;

	dbg_find("found LEB %d, free %d, dirty %d, flags %#x",
		 lnum, lprops->free, lprops->dirty, lprops->flags);

	flags = lprops->flags | LPROPS_TAKEN | LPROPS_INDEX;
	lprops = ubifs_change_lp(c, lprops, c->leb_size, 0, flags, 0);
	if (IS_ERR(lprops)) {
		err = PTR_ERR(lprops);
		goto out;
	}

	ubifs_release_lprops(c);

	err = ubifs_leb_unmap(c, lnum);
	if (err) {
		ubifs_change_one_lp(c, lnum, LPROPS_NC, LPROPS_NC, 0,
				    LPROPS_TAKEN | LPROPS_INDEX, 0);
		return err;
	}

	return lnum;

out:
	ubifs_release_lprops(c);
	return err;
}

static int cmp_dirty_idx(const struct ubifs_lprops **a,
			 const struct ubifs_lprops **b)
{
	const struct ubifs_lprops *lpa = *a;
	const struct ubifs_lprops *lpb = *b;

	return lpa->dirty + lpa->free - lpb->dirty - lpb->free;
}

static void swap_dirty_idx(struct ubifs_lprops **a, struct ubifs_lprops **b,
			   int size)
{
	struct ubifs_lprops *t = *a;

	*a = *b;
	*b = t;
}

int ubifs_save_dirty_idx_lnums(struct ubifs_info *c)
{
	int i;

	ubifs_get_lprops(c);
	
	c->dirty_idx.cnt = c->lpt_heap[LPROPS_DIRTY_IDX - 1].cnt;
	memcpy(c->dirty_idx.arr, c->lpt_heap[LPROPS_DIRTY_IDX - 1].arr,
	       sizeof(void *) * c->dirty_idx.cnt);
	
	sort(c->dirty_idx.arr, c->dirty_idx.cnt, sizeof(void *),
	     (int (*)(const void *, const void *))cmp_dirty_idx,
	     (void (*)(void *, void *, int))swap_dirty_idx);
	dbg_find("found %d dirty index LEBs", c->dirty_idx.cnt);
	if (c->dirty_idx.cnt)
		dbg_find("dirtiest index LEB is %d with dirty %d and free %d",
			 c->dirty_idx.arr[c->dirty_idx.cnt - 1]->lnum,
			 c->dirty_idx.arr[c->dirty_idx.cnt - 1]->dirty,
			 c->dirty_idx.arr[c->dirty_idx.cnt - 1]->free);
	
	for (i = 0; i < c->dirty_idx.cnt; i++)
		c->dirty_idx.arr[i] = (void *)(size_t)c->dirty_idx.arr[i]->lnum;
	ubifs_release_lprops(c);
	return 0;
}

static int scan_dirty_idx_cb(struct ubifs_info *c,
			   const struct ubifs_lprops *lprops, int in_tree,
			   struct scan_data *data)
{
	int ret = LPT_SCAN_CONTINUE;

	
	if (lprops->flags & LPROPS_TAKEN)
		return LPT_SCAN_CONTINUE;
	
	if (!in_tree && valuable(c, lprops))
		ret |= LPT_SCAN_ADD;
	
	if (!(lprops->flags & LPROPS_INDEX))
		return ret;
	
	if (lprops->free + lprops->dirty < c->min_idx_node_sz)
		return ret;
	
	data->lnum = lprops->lnum;
	return LPT_SCAN_ADD | LPT_SCAN_STOP;
}

static int find_dirty_idx_leb(struct ubifs_info *c)
{
	const struct ubifs_lprops *lprops;
	struct ubifs_lpt_heap *heap;
	struct scan_data data;
	int err, i, ret;

	
	data.lnum = -1;
	heap = &c->lpt_heap[LPROPS_DIRTY_IDX - 1];
	for (i = 0; i < heap->cnt; i++) {
		lprops = heap->arr[i];
		ret = scan_dirty_idx_cb(c, lprops, 1, &data);
		if (ret & LPT_SCAN_STOP)
			goto found;
	}
	list_for_each_entry(lprops, &c->frdi_idx_list, list) {
		ret = scan_dirty_idx_cb(c, lprops, 1, &data);
		if (ret & LPT_SCAN_STOP)
			goto found;
	}
	list_for_each_entry(lprops, &c->uncat_list, list) {
		ret = scan_dirty_idx_cb(c, lprops, 1, &data);
		if (ret & LPT_SCAN_STOP)
			goto found;
	}
	if (c->pnodes_have >= c->pnode_cnt)
		
		return -ENOSPC;
	err = ubifs_lpt_scan_nolock(c, -1, c->lscan_lnum,
				    (ubifs_lpt_scan_callback)scan_dirty_idx_cb,
				    &data);
	if (err)
		return err;
found:
	ubifs_assert(data.lnum >= c->main_first && data.lnum < c->leb_cnt);
	c->lscan_lnum = data.lnum;
	lprops = ubifs_lpt_lookup_dirty(c, data.lnum);
	if (IS_ERR(lprops))
		return PTR_ERR(lprops);
	ubifs_assert(lprops->lnum == data.lnum);
	ubifs_assert(lprops->free + lprops->dirty >= c->min_idx_node_sz);
	ubifs_assert(!(lprops->flags & LPROPS_TAKEN));
	ubifs_assert((lprops->flags & LPROPS_INDEX));

	dbg_find("found dirty LEB %d, free %d, dirty %d, flags %#x",
		 lprops->lnum, lprops->free, lprops->dirty, lprops->flags);

	lprops = ubifs_change_lp(c, lprops, LPROPS_NC, LPROPS_NC,
				 lprops->flags | LPROPS_TAKEN, 0);
	if (IS_ERR(lprops))
		return PTR_ERR(lprops);

	return lprops->lnum;
}

static int get_idx_gc_leb(struct ubifs_info *c)
{
	const struct ubifs_lprops *lp;
	int err, lnum;

	err = ubifs_get_idx_gc_leb(c);
	if (err < 0)
		return err;
	lnum = err;
	lp = ubifs_lpt_lookup_dirty(c, lnum);
	if (IS_ERR(lp))
		return PTR_ERR(lp);
	lp = ubifs_change_lp(c, lp, LPROPS_NC, LPROPS_NC,
			     lp->flags | LPROPS_INDEX, -1);
	if (IS_ERR(lp))
		return PTR_ERR(lp);
	dbg_find("LEB %d, dirty %d and free %d flags %#x",
		 lp->lnum, lp->dirty, lp->free, lp->flags);
	return lnum;
}

static int find_dirtiest_idx_leb(struct ubifs_info *c)
{
	const struct ubifs_lprops *lp;
	int lnum;

	while (1) {
		if (!c->dirty_idx.cnt)
			return -ENOSPC;
		
		lnum = (size_t)c->dirty_idx.arr[--c->dirty_idx.cnt];
		lp = ubifs_lpt_lookup(c, lnum);
		if (IS_ERR(lp))
			return PTR_ERR(lp);
		if ((lp->flags & LPROPS_TAKEN) || !(lp->flags & LPROPS_INDEX))
			continue;
		lp = ubifs_change_lp(c, lp, LPROPS_NC, LPROPS_NC,
				     lp->flags | LPROPS_TAKEN, 0);
		if (IS_ERR(lp))
			return PTR_ERR(lp);
		break;
	}
	dbg_find("LEB %d, dirty %d and free %d flags %#x", lp->lnum, lp->dirty,
		 lp->free, lp->flags);
	ubifs_assert(lp->flags | LPROPS_TAKEN);
	ubifs_assert(lp->flags | LPROPS_INDEX);
	return lnum;
}

int ubifs_find_dirty_idx_leb(struct ubifs_info *c)
{
	int err;

	ubifs_get_lprops(c);

	err = find_dirtiest_idx_leb(c);

	
	if (err == -ENOSPC)
		err = find_dirty_idx_leb(c);

	
	if (err == -ENOSPC)
		err = get_idx_gc_leb(c);

	ubifs_release_lprops(c);
	return err;
}
