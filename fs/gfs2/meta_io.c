/*
 * Copyright (C) Sistina Software, Inc.  1997-2003 All rights reserved.
 * Copyright (C) 2004-2008 Red Hat, Inc.  All rights reserved.
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU General Public License version 2.
 */

#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/completion.h>
#include <linux/buffer_head.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/writeback.h>
#include <linux/swap.h>
#include <linux/delay.h>
#include <linux/bio.h>
#include <linux/gfs2_ondisk.h>

#include "gfs2.h"
#include "incore.h"
#include "glock.h"
#include "glops.h"
#include "inode.h"
#include "log.h"
#include "lops.h"
#include "meta_io.h"
#include "rgrp.h"
#include "trans.h"
#include "util.h"
#include "trace_gfs2.h"

static int gfs2_aspace_writepage(struct page *page, struct writeback_control *wbc)
{
	struct buffer_head *bh, *head;
	int nr_underway = 0;
	int write_op = REQ_META | REQ_PRIO |
		(wbc->sync_mode == WB_SYNC_ALL ? WRITE_SYNC : WRITE);

	BUG_ON(!PageLocked(page));
	BUG_ON(!page_has_buffers(page));

	head = page_buffers(page);
	bh = head;

	do {
		if (!buffer_mapped(bh))
			continue;
		if (wbc->sync_mode != WB_SYNC_NONE) {
			lock_buffer(bh);
		} else if (!trylock_buffer(bh)) {
			redirty_page_for_writepage(wbc, page);
			continue;
		}
		if (test_clear_buffer_dirty(bh)) {
			mark_buffer_async_write(bh);
		} else {
			unlock_buffer(bh);
		}
	} while ((bh = bh->b_this_page) != head);

	BUG_ON(PageWriteback(page));
	set_page_writeback(page);

	do {
		struct buffer_head *next = bh->b_this_page;
		if (buffer_async_write(bh)) {
			submit_bh(write_op, bh);
			nr_underway++;
		}
		bh = next;
	} while (bh != head);
	unlock_page(page);

	if (nr_underway == 0)
		end_page_writeback(page);

	return 0;
}

const struct address_space_operations gfs2_meta_aops = {
	.writepage = gfs2_aspace_writepage,
	.releasepage = gfs2_releasepage,
};


void gfs2_meta_sync(struct gfs2_glock *gl)
{
	struct address_space *mapping = gfs2_glock2aspace(gl);
	int error;

	filemap_fdatawrite(mapping);
	error = filemap_fdatawait(mapping);

	if (error)
		gfs2_io_error(gl->gl_sbd);
}


struct buffer_head *gfs2_getbuf(struct gfs2_glock *gl, u64 blkno, int create)
{
	struct address_space *mapping = gfs2_glock2aspace(gl);
	struct gfs2_sbd *sdp = gl->gl_sbd;
	struct page *page;
	struct buffer_head *bh;
	unsigned int shift;
	unsigned long index;
	unsigned int bufnum;

	shift = PAGE_CACHE_SHIFT - sdp->sd_sb.sb_bsize_shift;
	index = blkno >> shift;             
	bufnum = blkno - (index << shift);  

	if (create) {
		for (;;) {
			page = grab_cache_page(mapping, index);
			if (page)
				break;
			yield();
		}
	} else {
		page = find_lock_page(mapping, index);
		if (!page)
			return NULL;
	}

	if (!page_has_buffers(page))
		create_empty_buffers(page, sdp->sd_sb.sb_bsize, 0);

	
	for (bh = page_buffers(page); bufnum--; bh = bh->b_this_page)
		;
	get_bh(bh);

	if (!buffer_mapped(bh))
		map_bh(bh, sdp->sd_vfs, blkno);

	unlock_page(page);
	mark_page_accessed(page);
	page_cache_release(page);

	return bh;
}

static void meta_prep_new(struct buffer_head *bh)
{
	struct gfs2_meta_header *mh = (struct gfs2_meta_header *)bh->b_data;

	lock_buffer(bh);
	clear_buffer_dirty(bh);
	set_buffer_uptodate(bh);
	unlock_buffer(bh);

	mh->mh_magic = cpu_to_be32(GFS2_MAGIC);
}


struct buffer_head *gfs2_meta_new(struct gfs2_glock *gl, u64 blkno)
{
	struct buffer_head *bh;
	bh = gfs2_getbuf(gl, blkno, CREATE);
	meta_prep_new(bh);
	return bh;
}


int gfs2_meta_read(struct gfs2_glock *gl, u64 blkno, int flags,
		   struct buffer_head **bhp)
{
	struct gfs2_sbd *sdp = gl->gl_sbd;
	struct buffer_head *bh;

	if (unlikely(test_bit(SDF_SHUTDOWN, &sdp->sd_flags)))
		return -EIO;

	*bhp = bh = gfs2_getbuf(gl, blkno, CREATE);

	lock_buffer(bh);
	if (buffer_uptodate(bh)) {
		unlock_buffer(bh);
		return 0;
	}
	bh->b_end_io = end_buffer_read_sync;
	get_bh(bh);
	submit_bh(READ_SYNC | REQ_META | REQ_PRIO, bh);
	if (!(flags & DIO_WAIT))
		return 0;

	wait_on_buffer(bh);
	if (unlikely(!buffer_uptodate(bh))) {
		struct gfs2_trans *tr = current->journal_info;
		if (tr && tr->tr_touched)
			gfs2_io_error_bh(sdp, bh);
		brelse(bh);
		return -EIO;
	}

	return 0;
}


int gfs2_meta_wait(struct gfs2_sbd *sdp, struct buffer_head *bh)
{
	if (unlikely(test_bit(SDF_SHUTDOWN, &sdp->sd_flags)))
		return -EIO;

	wait_on_buffer(bh);

	if (!buffer_uptodate(bh)) {
		struct gfs2_trans *tr = current->journal_info;
		if (tr && tr->tr_touched)
			gfs2_io_error_bh(sdp, bh);
		return -EIO;
	}
	if (unlikely(test_bit(SDF_SHUTDOWN, &sdp->sd_flags)))
		return -EIO;

	return 0;
}


void gfs2_attach_bufdata(struct gfs2_glock *gl, struct buffer_head *bh,
			 int meta)
{
	struct gfs2_bufdata *bd;

	if (meta)
		lock_page(bh->b_page);

	if (bh->b_private) {
		if (meta)
			unlock_page(bh->b_page);
		return;
	}

	bd = kmem_cache_zalloc(gfs2_bufdata_cachep, GFP_NOFS | __GFP_NOFAIL);
	bd->bd_bh = bh;
	bd->bd_gl = gl;

	INIT_LIST_HEAD(&bd->bd_list_tr);
	if (meta)
		lops_init_le(&bd->bd_le, &gfs2_buf_lops);
	else
		lops_init_le(&bd->bd_le, &gfs2_databuf_lops);
	bh->b_private = bd;

	if (meta)
		unlock_page(bh->b_page);
}

void gfs2_remove_from_journal(struct buffer_head *bh, struct gfs2_trans *tr, int meta)
{
	struct address_space *mapping = bh->b_page->mapping;
	struct gfs2_sbd *sdp = gfs2_mapping2sbd(mapping);
	struct gfs2_bufdata *bd = bh->b_private;

	if (test_clear_buffer_pinned(bh)) {
		trace_gfs2_pin(bd, 0);
		atomic_dec(&sdp->sd_log_pinned);
		list_del_init(&bd->bd_le.le_list);
		if (meta) {
			gfs2_assert_warn(sdp, sdp->sd_log_num_buf);
			sdp->sd_log_num_buf--;
			tr->tr_num_buf_rm++;
		} else {
			gfs2_assert_warn(sdp, sdp->sd_log_num_databuf);
			sdp->sd_log_num_databuf--;
			tr->tr_num_databuf_rm++;
		}
		tr->tr_touched = 1;
		brelse(bh);
	}
	if (bd) {
		spin_lock(&sdp->sd_ail_lock);
		if (bd->bd_ail) {
			gfs2_remove_from_ail(bd);
			bh->b_private = NULL;
			bd->bd_bh = NULL;
			bd->bd_blkno = bh->b_blocknr;
			gfs2_trans_add_revoke(sdp, bd);
		}
		spin_unlock(&sdp->sd_ail_lock);
	}
	clear_buffer_dirty(bh);
	clear_buffer_uptodate(bh);
}


void gfs2_meta_wipe(struct gfs2_inode *ip, u64 bstart, u32 blen)
{
	struct gfs2_sbd *sdp = GFS2_SB(&ip->i_inode);
	struct buffer_head *bh;

	while (blen) {
		bh = gfs2_getbuf(ip->i_gl, bstart, NO_CREATE);
		if (bh) {
			lock_buffer(bh);
			gfs2_log_lock(sdp);
			gfs2_remove_from_journal(bh, current->journal_info, 1);
			gfs2_log_unlock(sdp);
			unlock_buffer(bh);
			brelse(bh);
		}

		bstart++;
		blen--;
	}
}


int gfs2_meta_indirect_buffer(struct gfs2_inode *ip, int height, u64 num,
			      int new, struct buffer_head **bhp)
{
	struct gfs2_sbd *sdp = GFS2_SB(&ip->i_inode);
	struct gfs2_glock *gl = ip->i_gl;
	struct buffer_head *bh;
	int ret = 0;

	if (new) {
		BUG_ON(height == 0);
		bh = gfs2_meta_new(gl, num);
		gfs2_trans_add_bh(ip->i_gl, bh, 1);
		gfs2_metatype_set(bh, GFS2_METATYPE_IN, GFS2_FORMAT_IN);
		gfs2_buffer_clear_tail(bh, sizeof(struct gfs2_meta_header));
	} else {
		u32 mtype = height ? GFS2_METATYPE_IN : GFS2_METATYPE_DI;
		ret = gfs2_meta_read(gl, num, DIO_WAIT, &bh);
		if (ret == 0 && gfs2_metatype_check(sdp, bh, mtype)) {
			brelse(bh);
			ret = -EIO;
		}
	}
	*bhp = bh;
	return ret;
}


struct buffer_head *gfs2_meta_ra(struct gfs2_glock *gl, u64 dblock, u32 extlen)
{
	struct gfs2_sbd *sdp = gl->gl_sbd;
	struct buffer_head *first_bh, *bh;
	u32 max_ra = gfs2_tune_get(sdp, gt_max_readahead) >>
			  sdp->sd_sb.sb_bsize_shift;

	BUG_ON(!extlen);

	if (max_ra < 1)
		max_ra = 1;
	if (extlen > max_ra)
		extlen = max_ra;

	first_bh = gfs2_getbuf(gl, dblock, CREATE);

	if (buffer_uptodate(first_bh))
		goto out;
	if (!buffer_locked(first_bh))
		ll_rw_block(READ_SYNC | REQ_META, 1, &first_bh);

	dblock++;
	extlen--;

	while (extlen) {
		bh = gfs2_getbuf(gl, dblock, CREATE);

		if (!buffer_uptodate(bh) && !buffer_locked(bh))
			ll_rw_block(READA | REQ_META, 1, &bh);
		brelse(bh);
		dblock++;
		extlen--;
		if (!buffer_locked(first_bh) && buffer_uptodate(first_bh))
			goto out;
	}

	wait_on_buffer(first_bh);
out:
	return first_bh;
}

