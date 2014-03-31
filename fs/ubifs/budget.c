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
#include <linux/writeback.h>
#include <linux/math64.h>

#define MAX_MKSPC_RETRIES 3

/*
 * The below constant defines amount of dirty pages which should be written
 * back at when trying to shrink the liability.
 */
#define NR_TO_WRITE 16

static void shrink_liability(struct ubifs_info *c, int nr_to_write)
{
	down_read(&c->vfs_sb->s_umount);
	writeback_inodes_sb(c->vfs_sb, WB_REASON_FS_FREE_SPACE);
	up_read(&c->vfs_sb->s_umount);
}

static int run_gc(struct ubifs_info *c)
{
	int err, lnum;

	
	down_read(&c->commit_sem);
	lnum = ubifs_garbage_collect(c, 1);
	up_read(&c->commit_sem);
	if (lnum < 0)
		return lnum;

	
	dbg_budg("GC freed LEB %d", lnum);
	err = ubifs_return_leb(c, lnum);
	if (err)
		return err;
	return 0;
}

static long long get_liability(struct ubifs_info *c)
{
	long long liab;

	spin_lock(&c->space_lock);
	liab = c->bi.idx_growth + c->bi.data_growth + c->bi.dd_growth;
	spin_unlock(&c->space_lock);
	return liab;
}

static int make_free_space(struct ubifs_info *c)
{
	int err, retries = 0;
	long long liab1, liab2;

	do {
		liab1 = get_liability(c);
		dbg_budg("liability %lld, run write-back", liab1);
		shrink_liability(c, NR_TO_WRITE);

		liab2 = get_liability(c);
		if (liab2 < liab1)
			return -EAGAIN;

		dbg_budg("new liability %lld (not shrunk)", liab2);

		
		dbg_budg("Run GC");
		err = run_gc(c);
		if (!err)
			return -EAGAIN;

		if (err != -EAGAIN && err != -ENOSPC)
			
			return err;

		dbg_budg("Run commit (retries %d)", retries);
		err = ubifs_run_commit(c);
		if (err)
			return err;
	} while (retries++ < MAX_MKSPC_RETRIES);

	return -ENOSPC;
}

int ubifs_calc_min_idx_lebs(struct ubifs_info *c)
{
	int idx_lebs;
	long long idx_size;

	idx_size = c->bi.old_idx_sz + c->bi.idx_growth + c->bi.uncommitted_idx;
	
	idx_size += idx_size << 1;
	idx_lebs = div_u64(idx_size + c->idx_leb_size - 1, c->idx_leb_size);
	idx_lebs += 1;
	if (idx_lebs < MIN_INDEX_LEBS)
		idx_lebs = MIN_INDEX_LEBS;
	return idx_lebs;
}

long long ubifs_calc_available(const struct ubifs_info *c, int min_idx_lebs)
{
	int subtract_lebs;
	long long available;

	available = c->main_bytes - c->lst.total_used;

	subtract_lebs = min_idx_lebs;

	
	subtract_lebs += 1;

	subtract_lebs += c->jhead_cnt - 1;

	
	subtract_lebs += 1;

	available -= (long long)subtract_lebs * c->leb_size;

	
	available -= c->lst.total_dead;

	/*
	 * Subtract dark space, which might or might not be usable - it depends
	 * on the data which we have on the media and which will be written. If
	 * this is a lot of uncompressed or not-compressible data, the dark
	 * space cannot be used.
	 */
	available -= c->lst.total_dark;

	if (c->lst.idx_lebs > min_idx_lebs) {
		subtract_lebs = c->lst.idx_lebs - min_idx_lebs;
		available -= subtract_lebs * c->dark_wm;
	}

	
	return available > 0 ? available : 0;
}

static int can_use_rp(struct ubifs_info *c)
{
	if (current_fsuid() == c->rp_uid || capable(CAP_SYS_RESOURCE) ||
	    (c->rp_gid != 0 && in_group_p(c->rp_gid)))
		return 1;
	return 0;
}

/**
 * do_budget_space - reserve flash space for index and data growth.
 * @c: UBIFS file-system description object
 *
 * This function makes sure UBIFS has enough free LEBs for index growth and
 * data.
 *
 * When budgeting index space, UBIFS reserves thrice as many LEBs as the index
 * would take if it was consolidated and written to the flash. This guarantees
 * that the "in-the-gaps" commit method always succeeds and UBIFS will always
 * be able to commit dirty index. So this function basically adds amount of
 * budgeted index space to the size of the current index, multiplies this by 3,
 * and makes sure this does not exceed the amount of free LEBs.
 *
 * Notes about @c->bi.min_idx_lebs and @c->lst.idx_lebs variables:
 * o @c->lst.idx_lebs is the number of LEBs the index currently uses. It might
 *    be large, because UBIFS does not do any index consolidation as long as
 *    there is free space. IOW, the index may take a lot of LEBs, but the LEBs
 *    will contain a lot of dirt.
 * o @c->bi.min_idx_lebs is the number of LEBS the index presumably takes. IOW,
 *    the index may be consolidated to take up to @c->bi.min_idx_lebs LEBs.
 *
 * This function returns zero in case of success, and %-ENOSPC in case of
 * failure.
 */
static int do_budget_space(struct ubifs_info *c)
{
	long long outstanding, available;
	int lebs, rsvd_idx_lebs, min_idx_lebs;

	
	min_idx_lebs = ubifs_calc_min_idx_lebs(c);

	
	if (min_idx_lebs > c->lst.idx_lebs)
		rsvd_idx_lebs = min_idx_lebs - c->lst.idx_lebs;
	else
		rsvd_idx_lebs = 0;

	lebs = c->lst.empty_lebs + c->freeable_cnt + c->idx_gc_cnt -
	       c->lst.taken_empty_lebs;
	if (unlikely(rsvd_idx_lebs > lebs)) {
		dbg_budg("out of indexing space: min_idx_lebs %d (old %d), "
			 "rsvd_idx_lebs %d", min_idx_lebs, c->bi.min_idx_lebs,
			 rsvd_idx_lebs);
		return -ENOSPC;
	}

	available = ubifs_calc_available(c, min_idx_lebs);
	outstanding = c->bi.data_growth + c->bi.dd_growth;

	if (unlikely(available < outstanding)) {
		dbg_budg("out of data space: available %lld, outstanding %lld",
			 available, outstanding);
		return -ENOSPC;
	}

	if (available - outstanding <= c->rp_size && !can_use_rp(c))
		return -ENOSPC;

	c->bi.min_idx_lebs = min_idx_lebs;
	return 0;
}

static int calc_idx_growth(const struct ubifs_info *c,
			   const struct ubifs_budget_req *req)
{
	int znodes;

	znodes = req->new_ino + (req->new_page << UBIFS_BLOCKS_PER_PAGE_SHIFT) +
		 req->new_dent;
	return znodes * c->max_idx_node_sz;
}

static int calc_data_growth(const struct ubifs_info *c,
			    const struct ubifs_budget_req *req)
{
	int data_growth;

	data_growth = req->new_ino  ? c->bi.inode_budget : 0;
	if (req->new_page)
		data_growth += c->bi.page_budget;
	if (req->new_dent)
		data_growth += c->bi.dent_budget;
	data_growth += req->new_ino_d;
	return data_growth;
}

static int calc_dd_growth(const struct ubifs_info *c,
			  const struct ubifs_budget_req *req)
{
	int dd_growth;

	dd_growth = req->dirtied_page ? c->bi.page_budget : 0;

	if (req->dirtied_ino)
		dd_growth += c->bi.inode_budget << (req->dirtied_ino - 1);
	if (req->mod_dent)
		dd_growth += c->bi.dent_budget;
	dd_growth += req->dirtied_ino_d;
	return dd_growth;
}

int ubifs_budget_space(struct ubifs_info *c, struct ubifs_budget_req *req)
{
	int uninitialized_var(cmt_retries), uninitialized_var(wb_retries);
	int err, idx_growth, data_growth, dd_growth, retried = 0;

	ubifs_assert(req->new_page <= 1);
	ubifs_assert(req->dirtied_page <= 1);
	ubifs_assert(req->new_dent <= 1);
	ubifs_assert(req->mod_dent <= 1);
	ubifs_assert(req->new_ino <= 1);
	ubifs_assert(req->new_ino_d <= UBIFS_MAX_INO_DATA);
	ubifs_assert(req->dirtied_ino <= 4);
	ubifs_assert(req->dirtied_ino_d <= UBIFS_MAX_INO_DATA * 4);
	ubifs_assert(!(req->new_ino_d & 7));
	ubifs_assert(!(req->dirtied_ino_d & 7));

	data_growth = calc_data_growth(c, req);
	dd_growth = calc_dd_growth(c, req);
	if (!data_growth && !dd_growth)
		return 0;
	idx_growth = calc_idx_growth(c, req);

again:
	spin_lock(&c->space_lock);
	ubifs_assert(c->bi.idx_growth >= 0);
	ubifs_assert(c->bi.data_growth >= 0);
	ubifs_assert(c->bi.dd_growth >= 0);

	if (unlikely(c->bi.nospace) && (c->bi.nospace_rp || !can_use_rp(c))) {
		dbg_budg("no space");
		spin_unlock(&c->space_lock);
		return -ENOSPC;
	}

	c->bi.idx_growth += idx_growth;
	c->bi.data_growth += data_growth;
	c->bi.dd_growth += dd_growth;

	err = do_budget_space(c);
	if (likely(!err)) {
		req->idx_growth = idx_growth;
		req->data_growth = data_growth;
		req->dd_growth = dd_growth;
		spin_unlock(&c->space_lock);
		return 0;
	}

	
	c->bi.idx_growth -= idx_growth;
	c->bi.data_growth -= data_growth;
	c->bi.dd_growth -= dd_growth;
	spin_unlock(&c->space_lock);

	if (req->fast) {
		dbg_budg("no space for fast budgeting");
		return err;
	}

	err = make_free_space(c);
	cond_resched();
	if (err == -EAGAIN) {
		dbg_budg("try again");
		goto again;
	} else if (err == -ENOSPC) {
		if (!retried) {
			retried = 1;
			dbg_budg("-ENOSPC, but anyway try once again");
			goto again;
		}
		dbg_budg("FS is full, -ENOSPC");
		c->bi.nospace = 1;
		if (can_use_rp(c) || c->rp_size == 0)
			c->bi.nospace_rp = 1;
		smp_wmb();
	} else
		ubifs_err("cannot budget space, error %d", err);
	return err;
}

/**
 * ubifs_release_budget - release budgeted free space.
 * @c: UBIFS file-system description object
 * @req: budget request
 *
 * This function releases the space budgeted by 'ubifs_budget_space()'. Note,
 * since the index changes (which were budgeted for in @req->idx_growth) will
 * only be written to the media on commit, this function moves the index budget
 * from @c->bi.idx_growth to @c->bi.uncommitted_idx. The latter will be zeroed
 * by the commit operation.
 */
void ubifs_release_budget(struct ubifs_info *c, struct ubifs_budget_req *req)
{
	ubifs_assert(req->new_page <= 1);
	ubifs_assert(req->dirtied_page <= 1);
	ubifs_assert(req->new_dent <= 1);
	ubifs_assert(req->mod_dent <= 1);
	ubifs_assert(req->new_ino <= 1);
	ubifs_assert(req->new_ino_d <= UBIFS_MAX_INO_DATA);
	ubifs_assert(req->dirtied_ino <= 4);
	ubifs_assert(req->dirtied_ino_d <= UBIFS_MAX_INO_DATA * 4);
	ubifs_assert(!(req->new_ino_d & 7));
	ubifs_assert(!(req->dirtied_ino_d & 7));
	if (!req->recalculate) {
		ubifs_assert(req->idx_growth >= 0);
		ubifs_assert(req->data_growth >= 0);
		ubifs_assert(req->dd_growth >= 0);
	}

	if (req->recalculate) {
		req->data_growth = calc_data_growth(c, req);
		req->dd_growth = calc_dd_growth(c, req);
		req->idx_growth = calc_idx_growth(c, req);
	}

	if (!req->data_growth && !req->dd_growth)
		return;

	c->bi.nospace = c->bi.nospace_rp = 0;
	smp_wmb();

	spin_lock(&c->space_lock);
	c->bi.idx_growth -= req->idx_growth;
	c->bi.uncommitted_idx += req->idx_growth;
	c->bi.data_growth -= req->data_growth;
	c->bi.dd_growth -= req->dd_growth;
	c->bi.min_idx_lebs = ubifs_calc_min_idx_lebs(c);

	ubifs_assert(c->bi.idx_growth >= 0);
	ubifs_assert(c->bi.data_growth >= 0);
	ubifs_assert(c->bi.dd_growth >= 0);
	ubifs_assert(c->bi.min_idx_lebs < c->main_lebs);
	ubifs_assert(!(c->bi.idx_growth & 7));
	ubifs_assert(!(c->bi.data_growth & 7));
	ubifs_assert(!(c->bi.dd_growth & 7));
	spin_unlock(&c->space_lock);
}

void ubifs_convert_page_budget(struct ubifs_info *c)
{
	spin_lock(&c->space_lock);
	
	c->bi.idx_growth -= c->max_idx_node_sz << UBIFS_BLOCKS_PER_PAGE_SHIFT;
	
	c->bi.data_growth -= c->bi.page_budget;
	
	c->bi.dd_growth += c->bi.page_budget;
	
	c->bi.min_idx_lebs = ubifs_calc_min_idx_lebs(c);
	spin_unlock(&c->space_lock);
}

/**
 * ubifs_release_dirty_inode_budget - release dirty inode budget.
 * @c: UBIFS file-system description object
 * @ui: UBIFS inode to release the budget for
 *
 * This function releases budget corresponding to a dirty inode. It is usually
 * called when after the inode has been written to the media and marked as
 * clean. It also causes the "no space" flags to be cleared.
 */
void ubifs_release_dirty_inode_budget(struct ubifs_info *c,
				      struct ubifs_inode *ui)
{
	struct ubifs_budget_req req;

	memset(&req, 0, sizeof(struct ubifs_budget_req));
	
	req.dd_growth = c->bi.inode_budget + ALIGN(ui->data_len, 8);
	ubifs_release_budget(c, &req);
}

long long ubifs_reported_space(const struct ubifs_info *c, long long free)
{
	int divisor, factor, f;

	f = c->fanout > 3 ? c->fanout >> 1 : 2;
	factor = UBIFS_BLOCK_SIZE;
	divisor = UBIFS_MAX_DATA_NODE_SZ;
	divisor += (c->max_idx_node_sz * 3) / (f - 1);
	free *= factor;
	return div_u64(free, divisor);
}

long long ubifs_get_free_space_nolock(struct ubifs_info *c)
{
	int rsvd_idx_lebs, lebs;
	long long available, outstanding, free;

	ubifs_assert(c->bi.min_idx_lebs == ubifs_calc_min_idx_lebs(c));
	outstanding = c->bi.data_growth + c->bi.dd_growth;
	available = ubifs_calc_available(c, c->bi.min_idx_lebs);

	if (c->bi.min_idx_lebs > c->lst.idx_lebs)
		rsvd_idx_lebs = c->bi.min_idx_lebs - c->lst.idx_lebs;
	else
		rsvd_idx_lebs = 0;
	lebs = c->lst.empty_lebs + c->freeable_cnt + c->idx_gc_cnt -
	       c->lst.taken_empty_lebs;
	lebs -= rsvd_idx_lebs;
	available += lebs * (c->dark_wm - c->leb_overhead);

	if (available > outstanding)
		free = ubifs_reported_space(c, available - outstanding);
	else
		free = 0;
	return free;
}

long long ubifs_get_free_space(struct ubifs_info *c)
{
	long long free;

	spin_lock(&c->space_lock);
	free = ubifs_get_free_space_nolock(c);
	spin_unlock(&c->space_lock);

	return free;
}
