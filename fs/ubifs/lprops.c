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
 * Authors: Adrian Hunter
 *          Artem Bityutskiy (Битюцкий Артём)
 */


#include "ubifs.h"

static int get_heap_comp_val(struct ubifs_lprops *lprops, int cat)
{
	switch (cat) {
	case LPROPS_FREE:
		return lprops->free;
	case LPROPS_DIRTY_IDX:
		return lprops->free + lprops->dirty;
	default:
		return lprops->dirty;
	}
}

static void move_up_lpt_heap(struct ubifs_info *c, struct ubifs_lpt_heap *heap,
			     struct ubifs_lprops *lprops, int cat)
{
	int val1, val2, hpos;

	hpos = lprops->hpos;
	if (!hpos)
		return; 
	val1 = get_heap_comp_val(lprops, cat);
	
	do {
		int ppos = (hpos - 1) / 2;

		val2 = get_heap_comp_val(heap->arr[ppos], cat);
		if (val2 >= val1)
			return;
		
		heap->arr[ppos]->hpos = hpos;
		heap->arr[hpos] = heap->arr[ppos];
		heap->arr[ppos] = lprops;
		lprops->hpos = ppos;
		hpos = ppos;
	} while (hpos);
}

static void adjust_lpt_heap(struct ubifs_info *c, struct ubifs_lpt_heap *heap,
			    struct ubifs_lprops *lprops, int hpos, int cat)
{
	int val1, val2, val3, cpos;

	val1 = get_heap_comp_val(lprops, cat);
	
	if (hpos) {
		int ppos = (hpos - 1) / 2;

		val2 = get_heap_comp_val(heap->arr[ppos], cat);
		if (val1 > val2) {
			
			while (1) {
				heap->arr[ppos]->hpos = hpos;
				heap->arr[hpos] = heap->arr[ppos];
				heap->arr[ppos] = lprops;
				lprops->hpos = ppos;
				hpos = ppos;
				if (!hpos)
					return;
				ppos = (hpos - 1) / 2;
				val2 = get_heap_comp_val(heap->arr[ppos], cat);
				if (val1 <= val2)
					return;
				
			}
		}
	}

	
	while (1) {
		
		cpos = hpos * 2 + 1;
		if (cpos >= heap->cnt)
			return;
		val2 = get_heap_comp_val(heap->arr[cpos], cat);
		if (val1 < val2) {
			
			if (cpos + 1 < heap->cnt) {
				val3 = get_heap_comp_val(heap->arr[cpos + 1],
							 cat);
				if (val3 > val2)
					cpos += 1; 
			}
			heap->arr[cpos]->hpos = hpos;
			heap->arr[hpos] = heap->arr[cpos];
			heap->arr[cpos] = lprops;
			lprops->hpos = cpos;
			hpos = cpos;
			continue;
		}
		
		cpos += 1;
		if (cpos >= heap->cnt)
			return;
		val3 = get_heap_comp_val(heap->arr[cpos], cat);
		if (val1 < val3) {
			
			heap->arr[cpos]->hpos = hpos;
			heap->arr[hpos] = heap->arr[cpos];
			heap->arr[cpos] = lprops;
			lprops->hpos = cpos;
			hpos = cpos;
			continue;
		}
		return;
	}
}

static int add_to_lpt_heap(struct ubifs_info *c, struct ubifs_lprops *lprops,
			   int cat)
{
	struct ubifs_lpt_heap *heap = &c->lpt_heap[cat - 1];

	if (heap->cnt >= heap->max_cnt) {
		const int b = LPT_HEAP_SZ / 2 - 1;
		int cpos, val1, val2;

		
		
		cpos = (((size_t)lprops >> 4) & b) + b;
		ubifs_assert(cpos >= b);
		ubifs_assert(cpos < LPT_HEAP_SZ);
		ubifs_assert(cpos < heap->cnt);

		val1 = get_heap_comp_val(lprops, cat);
		val2 = get_heap_comp_val(heap->arr[cpos], cat);
		if (val1 > val2) {
			struct ubifs_lprops *lp;

			lp = heap->arr[cpos];
			lp->flags &= ~LPROPS_CAT_MASK;
			lp->flags |= LPROPS_UNCAT;
			list_add(&lp->list, &c->uncat_list);
			lprops->hpos = cpos;
			heap->arr[cpos] = lprops;
			move_up_lpt_heap(c, heap, lprops, cat);
			dbg_check_heap(c, heap, cat, lprops->hpos);
			return 1; 
		}
		dbg_check_heap(c, heap, cat, -1);
		return 0; 
	} else {
		lprops->hpos = heap->cnt++;
		heap->arr[lprops->hpos] = lprops;
		move_up_lpt_heap(c, heap, lprops, cat);
		dbg_check_heap(c, heap, cat, lprops->hpos);
		return 1; 
	}
}

static void remove_from_lpt_heap(struct ubifs_info *c,
				 struct ubifs_lprops *lprops, int cat)
{
	struct ubifs_lpt_heap *heap;
	int hpos = lprops->hpos;

	heap = &c->lpt_heap[cat - 1];
	ubifs_assert(hpos >= 0 && hpos < heap->cnt);
	ubifs_assert(heap->arr[hpos] == lprops);
	heap->cnt -= 1;
	if (hpos < heap->cnt) {
		heap->arr[hpos] = heap->arr[heap->cnt];
		heap->arr[hpos]->hpos = hpos;
		adjust_lpt_heap(c, heap, heap->arr[hpos], hpos, cat);
	}
	dbg_check_heap(c, heap, cat, -1);
}

static void lpt_heap_replace(struct ubifs_info *c,
			     struct ubifs_lprops *old_lprops,
			     struct ubifs_lprops *new_lprops, int cat)
{
	struct ubifs_lpt_heap *heap;
	int hpos = new_lprops->hpos;

	heap = &c->lpt_heap[cat - 1];
	heap->arr[hpos] = new_lprops;
}

void ubifs_add_to_cat(struct ubifs_info *c, struct ubifs_lprops *lprops,
		      int cat)
{
	switch (cat) {
	case LPROPS_DIRTY:
	case LPROPS_DIRTY_IDX:
	case LPROPS_FREE:
		if (add_to_lpt_heap(c, lprops, cat))
			break;
		
		cat = LPROPS_UNCAT;
		
	case LPROPS_UNCAT:
		list_add(&lprops->list, &c->uncat_list);
		break;
	case LPROPS_EMPTY:
		list_add(&lprops->list, &c->empty_list);
		break;
	case LPROPS_FREEABLE:
		list_add(&lprops->list, &c->freeable_list);
		c->freeable_cnt += 1;
		break;
	case LPROPS_FRDI_IDX:
		list_add(&lprops->list, &c->frdi_idx_list);
		break;
	default:
		ubifs_assert(0);
	}
	lprops->flags &= ~LPROPS_CAT_MASK;
	lprops->flags |= cat;
}

static void ubifs_remove_from_cat(struct ubifs_info *c,
				  struct ubifs_lprops *lprops, int cat)
{
	switch (cat) {
	case LPROPS_DIRTY:
	case LPROPS_DIRTY_IDX:
	case LPROPS_FREE:
		remove_from_lpt_heap(c, lprops, cat);
		break;
	case LPROPS_FREEABLE:
		c->freeable_cnt -= 1;
		ubifs_assert(c->freeable_cnt >= 0);
		
	case LPROPS_UNCAT:
	case LPROPS_EMPTY:
	case LPROPS_FRDI_IDX:
		ubifs_assert(!list_empty(&lprops->list));
		list_del(&lprops->list);
		break;
	default:
		ubifs_assert(0);
	}
}

void ubifs_replace_cat(struct ubifs_info *c, struct ubifs_lprops *old_lprops,
		       struct ubifs_lprops *new_lprops)
{
	int cat;

	cat = new_lprops->flags & LPROPS_CAT_MASK;
	switch (cat) {
	case LPROPS_DIRTY:
	case LPROPS_DIRTY_IDX:
	case LPROPS_FREE:
		lpt_heap_replace(c, old_lprops, new_lprops, cat);
		break;
	case LPROPS_UNCAT:
	case LPROPS_EMPTY:
	case LPROPS_FREEABLE:
	case LPROPS_FRDI_IDX:
		list_replace(&old_lprops->list, &new_lprops->list);
		break;
	default:
		ubifs_assert(0);
	}
}

void ubifs_ensure_cat(struct ubifs_info *c, struct ubifs_lprops *lprops)
{
	int cat = lprops->flags & LPROPS_CAT_MASK;

	if (cat != LPROPS_UNCAT)
		return;
	cat = ubifs_categorize_lprops(c, lprops);
	if (cat == LPROPS_UNCAT)
		return;
	ubifs_remove_from_cat(c, lprops, LPROPS_UNCAT);
	ubifs_add_to_cat(c, lprops, cat);
}

int ubifs_categorize_lprops(const struct ubifs_info *c,
			    const struct ubifs_lprops *lprops)
{
	if (lprops->flags & LPROPS_TAKEN)
		return LPROPS_UNCAT;

	if (lprops->free == c->leb_size) {
		ubifs_assert(!(lprops->flags & LPROPS_INDEX));
		return LPROPS_EMPTY;
	}

	if (lprops->free + lprops->dirty == c->leb_size) {
		if (lprops->flags & LPROPS_INDEX)
			return LPROPS_FRDI_IDX;
		else
			return LPROPS_FREEABLE;
	}

	if (lprops->flags & LPROPS_INDEX) {
		if (lprops->dirty + lprops->free >= c->min_idx_node_sz)
			return LPROPS_DIRTY_IDX;
	} else {
		if (lprops->dirty >= c->dead_wm &&
		    lprops->dirty > lprops->free)
			return LPROPS_DIRTY;
		if (lprops->free > 0)
			return LPROPS_FREE;
	}

	return LPROPS_UNCAT;
}

static void change_category(struct ubifs_info *c, struct ubifs_lprops *lprops)
{
	int old_cat = lprops->flags & LPROPS_CAT_MASK;
	int new_cat = ubifs_categorize_lprops(c, lprops);

	if (old_cat == new_cat) {
		struct ubifs_lpt_heap *heap = &c->lpt_heap[new_cat - 1];

		
		if (new_cat < 1 || new_cat > LPROPS_HEAP_CNT)
			return; 
		heap = &c->lpt_heap[new_cat - 1];
		adjust_lpt_heap(c, heap, lprops, lprops->hpos, new_cat);
	} else {
		ubifs_remove_from_cat(c, lprops, old_cat);
		ubifs_add_to_cat(c, lprops, new_cat);
	}
}

int ubifs_calc_dark(const struct ubifs_info *c, int spc)
{
	ubifs_assert(!(spc & 7));

	if (spc < c->dark_wm)
		return spc;

	if (spc - c->dark_wm < MIN_WRITE_SZ)
		return spc - MIN_WRITE_SZ;

	return c->dark_wm;
}

static int is_lprops_dirty(struct ubifs_info *c, struct ubifs_lprops *lprops)
{
	struct ubifs_pnode *pnode;
	int pos;

	pos = (lprops->lnum - c->main_first) & (UBIFS_LPT_FANOUT - 1);
	pnode = (struct ubifs_pnode *)container_of(lprops - pos,
						   struct ubifs_pnode,
						   lprops[0]);
	return !test_bit(COW_CNODE, &pnode->flags) &&
	       test_bit(DIRTY_CNODE, &pnode->flags);
}

const struct ubifs_lprops *ubifs_change_lp(struct ubifs_info *c,
					   const struct ubifs_lprops *lp,
					   int free, int dirty, int flags,
					   int idx_gc_cnt)
{
	struct ubifs_lprops *lprops = (struct ubifs_lprops *)lp;

	dbg_lp("LEB %d, free %d, dirty %d, flags %d",
	       lprops->lnum, free, dirty, flags);

	ubifs_assert(mutex_is_locked(&c->lp_mutex));
	ubifs_assert(c->lst.empty_lebs >= 0 &&
		     c->lst.empty_lebs <= c->main_lebs);
	ubifs_assert(c->freeable_cnt >= 0);
	ubifs_assert(c->freeable_cnt <= c->main_lebs);
	ubifs_assert(c->lst.taken_empty_lebs >= 0);
	ubifs_assert(c->lst.taken_empty_lebs <= c->lst.empty_lebs);
	ubifs_assert(!(c->lst.total_free & 7) && !(c->lst.total_dirty & 7));
	ubifs_assert(!(c->lst.total_dead & 7) && !(c->lst.total_dark & 7));
	ubifs_assert(!(c->lst.total_used & 7));
	ubifs_assert(free == LPROPS_NC || free >= 0);
	ubifs_assert(dirty == LPROPS_NC || dirty >= 0);

	if (!is_lprops_dirty(c, lprops)) {
		lprops = ubifs_lpt_lookup_dirty(c, lprops->lnum);
		if (IS_ERR(lprops))
			return lprops;
	} else
		ubifs_assert(lprops == ubifs_lpt_lookup_dirty(c, lprops->lnum));

	ubifs_assert(!(lprops->free & 7) && !(lprops->dirty & 7));

	spin_lock(&c->space_lock);
	if ((lprops->flags & LPROPS_TAKEN) && lprops->free == c->leb_size)
		c->lst.taken_empty_lebs -= 1;

	if (!(lprops->flags & LPROPS_INDEX)) {
		int old_spc;

		old_spc = lprops->free + lprops->dirty;
		if (old_spc < c->dead_wm)
			c->lst.total_dead -= old_spc;
		else
			c->lst.total_dark -= ubifs_calc_dark(c, old_spc);

		c->lst.total_used -= c->leb_size - old_spc;
	}

	if (free != LPROPS_NC) {
		free = ALIGN(free, 8);
		c->lst.total_free += free - lprops->free;

		
		if (free == c->leb_size) {
			if (lprops->free != c->leb_size)
				c->lst.empty_lebs += 1;
		} else if (lprops->free == c->leb_size)
			c->lst.empty_lebs -= 1;
		lprops->free = free;
	}

	if (dirty != LPROPS_NC) {
		dirty = ALIGN(dirty, 8);
		c->lst.total_dirty += dirty - lprops->dirty;
		lprops->dirty = dirty;
	}

	if (flags != LPROPS_NC) {
		
		if ((lprops->flags & LPROPS_INDEX)) {
			if (!(flags & LPROPS_INDEX))
				c->lst.idx_lebs -= 1;
		} else if (flags & LPROPS_INDEX)
			c->lst.idx_lebs += 1;
		lprops->flags = flags;
	}

	if (!(lprops->flags & LPROPS_INDEX)) {
		int new_spc;

		new_spc = lprops->free + lprops->dirty;
		if (new_spc < c->dead_wm)
			c->lst.total_dead += new_spc;
		else
			c->lst.total_dark += ubifs_calc_dark(c, new_spc);

		c->lst.total_used += c->leb_size - new_spc;
	}

	if ((lprops->flags & LPROPS_TAKEN) && lprops->free == c->leb_size)
		c->lst.taken_empty_lebs += 1;

	change_category(c, lprops);
	c->idx_gc_cnt += idx_gc_cnt;
	spin_unlock(&c->space_lock);
	return lprops;
}

void ubifs_get_lp_stats(struct ubifs_info *c, struct ubifs_lp_stats *lst)
{
	spin_lock(&c->space_lock);
	memcpy(lst, &c->lst, sizeof(struct ubifs_lp_stats));
	spin_unlock(&c->space_lock);
}

int ubifs_change_one_lp(struct ubifs_info *c, int lnum, int free, int dirty,
			int flags_set, int flags_clean, int idx_gc_cnt)
{
	int err = 0, flags;
	const struct ubifs_lprops *lp;

	ubifs_get_lprops(c);

	lp = ubifs_lpt_lookup_dirty(c, lnum);
	if (IS_ERR(lp)) {
		err = PTR_ERR(lp);
		goto out;
	}

	flags = (lp->flags | flags_set) & ~flags_clean;
	lp = ubifs_change_lp(c, lp, free, dirty, flags, idx_gc_cnt);
	if (IS_ERR(lp))
		err = PTR_ERR(lp);

out:
	ubifs_release_lprops(c);
	if (err)
		ubifs_err("cannot change properties of LEB %d, error %d",
			  lnum, err);
	return err;
}

int ubifs_update_one_lp(struct ubifs_info *c, int lnum, int free, int dirty,
			int flags_set, int flags_clean)
{
	int err = 0, flags;
	const struct ubifs_lprops *lp;

	ubifs_get_lprops(c);

	lp = ubifs_lpt_lookup_dirty(c, lnum);
	if (IS_ERR(lp)) {
		err = PTR_ERR(lp);
		goto out;
	}

	flags = (lp->flags | flags_set) & ~flags_clean;
	lp = ubifs_change_lp(c, lp, free, lp->dirty + dirty, flags, 0);
	if (IS_ERR(lp))
		err = PTR_ERR(lp);

out:
	ubifs_release_lprops(c);
	if (err)
		ubifs_err("cannot update properties of LEB %d, error %d",
			  lnum, err);
	return err;
}

int ubifs_read_one_lp(struct ubifs_info *c, int lnum, struct ubifs_lprops *lp)
{
	int err = 0;
	const struct ubifs_lprops *lpp;

	ubifs_get_lprops(c);

	lpp = ubifs_lpt_lookup(c, lnum);
	if (IS_ERR(lpp)) {
		err = PTR_ERR(lpp);
		ubifs_err("cannot read properties of LEB %d, error %d",
			  lnum, err);
		goto out;
	}

	memcpy(lp, lpp, sizeof(struct ubifs_lprops));

out:
	ubifs_release_lprops(c);
	return err;
}

const struct ubifs_lprops *ubifs_fast_find_free(struct ubifs_info *c)
{
	struct ubifs_lprops *lprops;
	struct ubifs_lpt_heap *heap;

	ubifs_assert(mutex_is_locked(&c->lp_mutex));

	heap = &c->lpt_heap[LPROPS_FREE - 1];
	if (heap->cnt == 0)
		return NULL;

	lprops = heap->arr[0];
	ubifs_assert(!(lprops->flags & LPROPS_TAKEN));
	ubifs_assert(!(lprops->flags & LPROPS_INDEX));
	return lprops;
}

const struct ubifs_lprops *ubifs_fast_find_empty(struct ubifs_info *c)
{
	struct ubifs_lprops *lprops;

	ubifs_assert(mutex_is_locked(&c->lp_mutex));

	if (list_empty(&c->empty_list))
		return NULL;

	lprops = list_entry(c->empty_list.next, struct ubifs_lprops, list);
	ubifs_assert(!(lprops->flags & LPROPS_TAKEN));
	ubifs_assert(!(lprops->flags & LPROPS_INDEX));
	ubifs_assert(lprops->free == c->leb_size);
	return lprops;
}

const struct ubifs_lprops *ubifs_fast_find_freeable(struct ubifs_info *c)
{
	struct ubifs_lprops *lprops;

	ubifs_assert(mutex_is_locked(&c->lp_mutex));

	if (list_empty(&c->freeable_list))
		return NULL;

	lprops = list_entry(c->freeable_list.next, struct ubifs_lprops, list);
	ubifs_assert(!(lprops->flags & LPROPS_TAKEN));
	ubifs_assert(!(lprops->flags & LPROPS_INDEX));
	ubifs_assert(lprops->free + lprops->dirty == c->leb_size);
	ubifs_assert(c->freeable_cnt > 0);
	return lprops;
}

const struct ubifs_lprops *ubifs_fast_find_frdi_idx(struct ubifs_info *c)
{
	struct ubifs_lprops *lprops;

	ubifs_assert(mutex_is_locked(&c->lp_mutex));

	if (list_empty(&c->frdi_idx_list))
		return NULL;

	lprops = list_entry(c->frdi_idx_list.next, struct ubifs_lprops, list);
	ubifs_assert(!(lprops->flags & LPROPS_TAKEN));
	ubifs_assert((lprops->flags & LPROPS_INDEX));
	ubifs_assert(lprops->free + lprops->dirty == c->leb_size);
	return lprops;
}

#ifdef CONFIG_UBIFS_FS_DEBUG

int dbg_check_cats(struct ubifs_info *c)
{
	struct ubifs_lprops *lprops;
	struct list_head *pos;
	int i, cat;

	if (!dbg_is_chk_gen(c) && !dbg_is_chk_lprops(c))
		return 0;

	list_for_each_entry(lprops, &c->empty_list, list) {
		if (lprops->free != c->leb_size) {
			ubifs_err("non-empty LEB %d on empty list "
				  "(free %d dirty %d flags %d)", lprops->lnum,
				  lprops->free, lprops->dirty, lprops->flags);
			return -EINVAL;
		}
		if (lprops->flags & LPROPS_TAKEN) {
			ubifs_err("taken LEB %d on empty list "
				  "(free %d dirty %d flags %d)", lprops->lnum,
				  lprops->free, lprops->dirty, lprops->flags);
			return -EINVAL;
		}
	}

	i = 0;
	list_for_each_entry(lprops, &c->freeable_list, list) {
		if (lprops->free + lprops->dirty != c->leb_size) {
			ubifs_err("non-freeable LEB %d on freeable list "
				  "(free %d dirty %d flags %d)", lprops->lnum,
				  lprops->free, lprops->dirty, lprops->flags);
			return -EINVAL;
		}
		if (lprops->flags & LPROPS_TAKEN) {
			ubifs_err("taken LEB %d on freeable list "
				  "(free %d dirty %d flags %d)", lprops->lnum,
				  lprops->free, lprops->dirty, lprops->flags);
			return -EINVAL;
		}
		i += 1;
	}
	if (i != c->freeable_cnt) {
		ubifs_err("freeable list count %d expected %d", i,
			  c->freeable_cnt);
		return -EINVAL;
	}

	i = 0;
	list_for_each(pos, &c->idx_gc)
		i += 1;
	if (i != c->idx_gc_cnt) {
		ubifs_err("idx_gc list count %d expected %d", i,
			  c->idx_gc_cnt);
		return -EINVAL;
	}

	list_for_each_entry(lprops, &c->frdi_idx_list, list) {
		if (lprops->free + lprops->dirty != c->leb_size) {
			ubifs_err("non-freeable LEB %d on frdi_idx list "
				  "(free %d dirty %d flags %d)", lprops->lnum,
				  lprops->free, lprops->dirty, lprops->flags);
			return -EINVAL;
		}
		if (lprops->flags & LPROPS_TAKEN) {
			ubifs_err("taken LEB %d on frdi_idx list "
				  "(free %d dirty %d flags %d)", lprops->lnum,
				  lprops->free, lprops->dirty, lprops->flags);
			return -EINVAL;
		}
		if (!(lprops->flags & LPROPS_INDEX)) {
			ubifs_err("non-index LEB %d on frdi_idx list "
				  "(free %d dirty %d flags %d)", lprops->lnum,
				  lprops->free, lprops->dirty, lprops->flags);
			return -EINVAL;
		}
	}

	for (cat = 1; cat <= LPROPS_HEAP_CNT; cat++) {
		struct ubifs_lpt_heap *heap = &c->lpt_heap[cat - 1];

		for (i = 0; i < heap->cnt; i++) {
			lprops = heap->arr[i];
			if (!lprops) {
				ubifs_err("null ptr in LPT heap cat %d", cat);
				return -EINVAL;
			}
			if (lprops->hpos != i) {
				ubifs_err("bad ptr in LPT heap cat %d", cat);
				return -EINVAL;
			}
			if (lprops->flags & LPROPS_TAKEN) {
				ubifs_err("taken LEB in LPT heap cat %d", cat);
				return -EINVAL;
			}
		}
	}

	return 0;
}

void dbg_check_heap(struct ubifs_info *c, struct ubifs_lpt_heap *heap, int cat,
		    int add_pos)
{
	int i = 0, j, err = 0;

	if (!dbg_is_chk_gen(c) && !dbg_is_chk_lprops(c))
		return;

	for (i = 0; i < heap->cnt; i++) {
		struct ubifs_lprops *lprops = heap->arr[i];
		struct ubifs_lprops *lp;

		if (i != add_pos)
			if ((lprops->flags & LPROPS_CAT_MASK) != cat) {
				err = 1;
				goto out;
			}
		if (lprops->hpos != i) {
			err = 2;
			goto out;
		}
		lp = ubifs_lpt_lookup(c, lprops->lnum);
		if (IS_ERR(lp)) {
			err = 3;
			goto out;
		}
		if (lprops != lp) {
			dbg_msg("lprops %zx lp %zx lprops->lnum %d lp->lnum %d",
				(size_t)lprops, (size_t)lp, lprops->lnum,
				lp->lnum);
			err = 4;
			goto out;
		}
		for (j = 0; j < i; j++) {
			lp = heap->arr[j];
			if (lp == lprops) {
				err = 5;
				goto out;
			}
			if (lp->lnum == lprops->lnum) {
				err = 6;
				goto out;
			}
		}
	}
out:
	if (err) {
		dbg_msg("failed cat %d hpos %d err %d", cat, i, err);
		dbg_dump_stack();
		dbg_dump_heap(c, heap, cat);
	}
}

static int scan_check_cb(struct ubifs_info *c,
			 const struct ubifs_lprops *lp, int in_tree,
			 struct ubifs_lp_stats *lst)
{
	struct ubifs_scan_leb *sleb;
	struct ubifs_scan_node *snod;
	int cat, lnum = lp->lnum, is_idx = 0, used = 0, free, dirty, ret;
	void *buf = NULL;

	cat = lp->flags & LPROPS_CAT_MASK;
	if (cat != LPROPS_UNCAT) {
		cat = ubifs_categorize_lprops(c, lp);
		if (cat != (lp->flags & LPROPS_CAT_MASK)) {
			ubifs_err("bad LEB category %d expected %d",
				  (lp->flags & LPROPS_CAT_MASK), cat);
			return -EINVAL;
		}
	}

	
	if (in_tree) {
		struct list_head *list = NULL;

		switch (cat) {
		case LPROPS_EMPTY:
			list = &c->empty_list;
			break;
		case LPROPS_FREEABLE:
			list = &c->freeable_list;
			break;
		case LPROPS_FRDI_IDX:
			list = &c->frdi_idx_list;
			break;
		case LPROPS_UNCAT:
			list = &c->uncat_list;
			break;
		}
		if (list) {
			struct ubifs_lprops *lprops;
			int found = 0;

			list_for_each_entry(lprops, list, list) {
				if (lprops == lp) {
					found = 1;
					break;
				}
			}
			if (!found) {
				ubifs_err("bad LPT list (category %d)", cat);
				return -EINVAL;
			}
		}
	}

	
	if (in_tree && cat > 0 && cat <= LPROPS_HEAP_CNT) {
		struct ubifs_lpt_heap *heap = &c->lpt_heap[cat - 1];

		if ((lp->hpos != -1 && heap->arr[lp->hpos]->lnum != lnum) ||
		    lp != heap->arr[lp->hpos]) {
			ubifs_err("bad LPT heap (category %d)", cat);
			return -EINVAL;
		}
	}

	buf = __vmalloc(c->leb_size, GFP_NOFS, PAGE_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (lp->free == c->leb_size) {
		lst->empty_lebs += 1;
		lst->total_free += c->leb_size;
		lst->total_dark += ubifs_calc_dark(c, c->leb_size);
		return LPT_SCAN_CONTINUE;
	}
	if (lp->free + lp->dirty == c->leb_size &&
	    !(lp->flags & LPROPS_INDEX)) {
		lst->total_free  += lp->free;
		lst->total_dirty += lp->dirty;
		lst->total_dark  +=  ubifs_calc_dark(c, c->leb_size);
		return LPT_SCAN_CONTINUE;
	}

	sleb = ubifs_scan(c, lnum, 0, buf, 0);
	if (IS_ERR(sleb)) {
		ret = PTR_ERR(sleb);
		if (ret == -EUCLEAN) {
			dbg_dump_lprops(c);
			dbg_dump_budg(c, &c->bi);
		}
		goto out;
	}

	is_idx = -1;
	list_for_each_entry(snod, &sleb->nodes, list) {
		int found, level = 0;

		cond_resched();

		if (is_idx == -1)
			is_idx = (snod->type == UBIFS_IDX_NODE) ? 1 : 0;

		if (is_idx && snod->type != UBIFS_IDX_NODE) {
			ubifs_err("indexing node in data LEB %d:%d",
				  lnum, snod->offs);
			goto out_destroy;
		}

		if (snod->type == UBIFS_IDX_NODE) {
			struct ubifs_idx_node *idx = snod->node;

			key_read(c, ubifs_idx_key(c, idx), &snod->key);
			level = le16_to_cpu(idx->level);
		}

		found = ubifs_tnc_has_node(c, &snod->key, level, lnum,
					   snod->offs, is_idx);
		if (found) {
			if (found < 0)
				goto out_destroy;
			used += ALIGN(snod->len, 8);
		}
	}

	free = c->leb_size - sleb->endpt;
	dirty = sleb->endpt - used;

	if (free > c->leb_size || free < 0 || dirty > c->leb_size ||
	    dirty < 0) {
		ubifs_err("bad calculated accounting for LEB %d: "
			  "free %d, dirty %d", lnum, free, dirty);
		goto out_destroy;
	}

	if (lp->free + lp->dirty == c->leb_size &&
	    free + dirty == c->leb_size)
		if ((is_idx && !(lp->flags & LPROPS_INDEX)) ||
		    (!is_idx && free == c->leb_size) ||
		    lp->free == c->leb_size) {
			free = lp->free;
			dirty = lp->dirty;
			is_idx = 0;
		    }

	if (is_idx && lp->free + lp->dirty == free + dirty &&
	    lnum != c->ihead_lnum) {
		free = lp->free;
		dirty = lp->dirty;
	}

	if (lp->free != free || lp->dirty != dirty)
		goto out_print;

	if (is_idx && !(lp->flags & LPROPS_INDEX)) {
		if (free == c->leb_size)
			
			is_idx = 0;
		else {
			ubifs_err("indexing node without indexing "
				  "flag");
			goto out_print;
		}
	}

	if (!is_idx && (lp->flags & LPROPS_INDEX)) {
		ubifs_err("data node with indexing flag");
		goto out_print;
	}

	if (free == c->leb_size)
		lst->empty_lebs += 1;

	if (is_idx)
		lst->idx_lebs += 1;

	if (!(lp->flags & LPROPS_INDEX))
		lst->total_used += c->leb_size - free - dirty;
	lst->total_free += free;
	lst->total_dirty += dirty;

	if (!(lp->flags & LPROPS_INDEX)) {
		int spc = free + dirty;

		if (spc < c->dead_wm)
			lst->total_dead += spc;
		else
			lst->total_dark += ubifs_calc_dark(c, spc);
	}

	ubifs_scan_destroy(sleb);
	vfree(buf);
	return LPT_SCAN_CONTINUE;

out_print:
	ubifs_err("bad accounting of LEB %d: free %d, dirty %d flags %#x, "
		  "should be free %d, dirty %d",
		  lnum, lp->free, lp->dirty, lp->flags, free, dirty);
	dbg_dump_leb(c, lnum);
out_destroy:
	ubifs_scan_destroy(sleb);
	ret = -EINVAL;
out:
	vfree(buf);
	return ret;
}

int dbg_check_lprops(struct ubifs_info *c)
{
	int i, err;
	struct ubifs_lp_stats lst;

	if (!dbg_is_chk_lprops(c))
		return 0;

	for (i = 0; i < c->jhead_cnt; i++) {
		err = ubifs_wbuf_sync(&c->jheads[i].wbuf);
		if (err)
			return err;
	}

	memset(&lst, 0, sizeof(struct ubifs_lp_stats));
	err = ubifs_lpt_scan_nolock(c, c->main_first, c->leb_cnt - 1,
				    (ubifs_lpt_scan_callback)scan_check_cb,
				    &lst);
	if (err && err != -ENOSPC)
		goto out;

	if (lst.empty_lebs != c->lst.empty_lebs ||
	    lst.idx_lebs != c->lst.idx_lebs ||
	    lst.total_free != c->lst.total_free ||
	    lst.total_dirty != c->lst.total_dirty ||
	    lst.total_used != c->lst.total_used) {
		ubifs_err("bad overall accounting");
		ubifs_err("calculated: empty_lebs %d, idx_lebs %d, "
			  "total_free %lld, total_dirty %lld, total_used %lld",
			  lst.empty_lebs, lst.idx_lebs, lst.total_free,
			  lst.total_dirty, lst.total_used);
		ubifs_err("read from lprops: empty_lebs %d, idx_lebs %d, "
			  "total_free %lld, total_dirty %lld, total_used %lld",
			  c->lst.empty_lebs, c->lst.idx_lebs, c->lst.total_free,
			  c->lst.total_dirty, c->lst.total_used);
		err = -EINVAL;
		goto out;
	}

	if (lst.total_dead != c->lst.total_dead ||
	    lst.total_dark != c->lst.total_dark) {
		ubifs_err("bad dead/dark space accounting");
		ubifs_err("calculated: total_dead %lld, total_dark %lld",
			  lst.total_dead, lst.total_dark);
		ubifs_err("read from lprops: total_dead %lld, total_dark %lld",
			  c->lst.total_dead, c->lst.total_dark);
		err = -EINVAL;
		goto out;
	}

	err = dbg_check_cats(c);
out:
	return err;
}

#endif 
