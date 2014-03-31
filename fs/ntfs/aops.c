/**
 * aops.c - NTFS kernel address space operations and page cache handling.
 *	    Part of the Linux-NTFS project.
 *
 * Copyright (c) 2001-2007 Anton Altaparmakov
 * Copyright (c) 2002 Richard Russon
 *
 * This program/include file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program/include file is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the Linux-NTFS
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/gfp.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/swap.h>
#include <linux/buffer_head.h>
#include <linux/writeback.h>
#include <linux/bit_spinlock.h>

#include "aops.h"
#include "attrib.h"
#include "debug.h"
#include "inode.h"
#include "mft.h"
#include "runlist.h"
#include "types.h"
#include "ntfs.h"

static void ntfs_end_buffer_async_read(struct buffer_head *bh, int uptodate)
{
	unsigned long flags;
	struct buffer_head *first, *tmp;
	struct page *page;
	struct inode *vi;
	ntfs_inode *ni;
	int page_uptodate = 1;

	page = bh->b_page;
	vi = page->mapping->host;
	ni = NTFS_I(vi);

	if (likely(uptodate)) {
		loff_t i_size;
		s64 file_ofs, init_size;

		set_buffer_uptodate(bh);

		file_ofs = ((s64)page->index << PAGE_CACHE_SHIFT) +
				bh_offset(bh);
		read_lock_irqsave(&ni->size_lock, flags);
		init_size = ni->initialized_size;
		i_size = i_size_read(vi);
		read_unlock_irqrestore(&ni->size_lock, flags);
		if (unlikely(init_size > i_size)) {
			
			init_size = i_size;
		}
		
		if (unlikely(file_ofs + bh->b_size > init_size)) {
			int ofs;
			void *kaddr;

			ofs = 0;
			if (file_ofs < init_size)
				ofs = init_size - file_ofs;
			local_irq_save(flags);
			kaddr = kmap_atomic(page);
			memset(kaddr + bh_offset(bh) + ofs, 0,
					bh->b_size - ofs);
			flush_dcache_page(page);
			kunmap_atomic(kaddr);
			local_irq_restore(flags);
		}
	} else {
		clear_buffer_uptodate(bh);
		SetPageError(page);
		ntfs_error(ni->vol->sb, "Buffer I/O error, logical block "
				"0x%llx.", (unsigned long long)bh->b_blocknr);
	}
	first = page_buffers(page);
	local_irq_save(flags);
	bit_spin_lock(BH_Uptodate_Lock, &first->b_state);
	clear_buffer_async_read(bh);
	unlock_buffer(bh);
	tmp = bh;
	do {
		if (!buffer_uptodate(tmp))
			page_uptodate = 0;
		if (buffer_async_read(tmp)) {
			if (likely(buffer_locked(tmp)))
				goto still_busy;
			
			BUG();
		}
		tmp = tmp->b_this_page;
	} while (tmp != bh);
	bit_spin_unlock(BH_Uptodate_Lock, &first->b_state);
	local_irq_restore(flags);
	if (!NInoMstProtected(ni)) {
		if (likely(page_uptodate && !PageError(page)))
			SetPageUptodate(page);
	} else {
		u8 *kaddr;
		unsigned int i, recs;
		u32 rec_size;

		rec_size = ni->itype.index.block_size;
		recs = PAGE_CACHE_SIZE / rec_size;
		
		BUG_ON(!recs);
		local_irq_save(flags);
		kaddr = kmap_atomic(page);
		for (i = 0; i < recs; i++)
			post_read_mst_fixup((NTFS_RECORD*)(kaddr +
					i * rec_size), rec_size);
		kunmap_atomic(kaddr);
		local_irq_restore(flags);
		flush_dcache_page(page);
		if (likely(page_uptodate && !PageError(page)))
			SetPageUptodate(page);
	}
	unlock_page(page);
	return;
still_busy:
	bit_spin_unlock(BH_Uptodate_Lock, &first->b_state);
	local_irq_restore(flags);
	return;
}

static int ntfs_read_block(struct page *page)
{
	loff_t i_size;
	VCN vcn;
	LCN lcn;
	s64 init_size;
	struct inode *vi;
	ntfs_inode *ni;
	ntfs_volume *vol;
	runlist_element *rl;
	struct buffer_head *bh, *head, *arr[MAX_BUF_PER_PAGE];
	sector_t iblock, lblock, zblock;
	unsigned long flags;
	unsigned int blocksize, vcn_ofs;
	int i, nr;
	unsigned char blocksize_bits;

	vi = page->mapping->host;
	ni = NTFS_I(vi);
	vol = ni->vol;

	
	BUG_ON(!ni->runlist.rl && !ni->mft_no && !NInoAttr(ni));

	blocksize = vol->sb->s_blocksize;
	blocksize_bits = vol->sb->s_blocksize_bits;

	if (!page_has_buffers(page)) {
		create_empty_buffers(page, blocksize, 0);
		if (unlikely(!page_has_buffers(page))) {
			unlock_page(page);
			return -ENOMEM;
		}
	}
	bh = head = page_buffers(page);
	BUG_ON(!bh);

	iblock = (s64)page->index << (PAGE_CACHE_SHIFT - blocksize_bits);
	read_lock_irqsave(&ni->size_lock, flags);
	lblock = (ni->allocated_size + blocksize - 1) >> blocksize_bits;
	init_size = ni->initialized_size;
	i_size = i_size_read(vi);
	read_unlock_irqrestore(&ni->size_lock, flags);
	if (unlikely(init_size > i_size)) {
		
		init_size = i_size;
	}
	zblock = (init_size + blocksize - 1) >> blocksize_bits;

	
	rl = NULL;
	nr = i = 0;
	do {
		int err = 0;

		if (unlikely(buffer_uptodate(bh)))
			continue;
		if (unlikely(buffer_mapped(bh))) {
			arr[nr++] = bh;
			continue;
		}
		bh->b_bdev = vol->sb->s_bdev;
		
		if (iblock < lblock) {
			bool is_retry = false;

			
			vcn = (VCN)iblock << blocksize_bits >>
					vol->cluster_size_bits;
			vcn_ofs = ((VCN)iblock << blocksize_bits) &
					vol->cluster_size_mask;
			if (!rl) {
lock_retry_remap:
				down_read(&ni->runlist.lock);
				rl = ni->runlist.rl;
			}
			if (likely(rl != NULL)) {
				
				while (rl->length && rl[1].vcn <= vcn)
					rl++;
				lcn = ntfs_rl_vcn_to_lcn(rl, vcn);
			} else
				lcn = LCN_RL_NOT_MAPPED;
			
			if (lcn >= 0) {
				
				bh->b_blocknr = ((lcn << vol->cluster_size_bits)
						+ vcn_ofs) >> blocksize_bits;
				set_buffer_mapped(bh);
				
				if (iblock < zblock) {
					arr[nr++] = bh;
					continue;
				}
				
				goto handle_zblock;
			}
			
			if (lcn == LCN_HOLE)
				goto handle_hole;
			
			if (!is_retry && lcn == LCN_RL_NOT_MAPPED) {
				is_retry = true;
				up_read(&ni->runlist.lock);
				err = ntfs_map_runlist(ni, vcn);
				if (likely(!err))
					goto lock_retry_remap;
				rl = NULL;
			} else if (!rl)
				up_read(&ni->runlist.lock);
			if (err == -ENOENT || lcn == LCN_ENOENT) {
				err = 0;
				goto handle_hole;
			}
			
			if (!err)
				err = -EIO;
			bh->b_blocknr = -1;
			SetPageError(page);
			ntfs_error(vol->sb, "Failed to read from inode 0x%lx, "
					"attribute type 0x%x, vcn 0x%llx, "
					"offset 0x%x because its location on "
					"disk could not be determined%s "
					"(error code %i).", ni->mft_no,
					ni->type, (unsigned long long)vcn,
					vcn_ofs, is_retry ? " even after "
					"retrying" : "", err);
		}
handle_hole:
		bh->b_blocknr = -1UL;
		clear_buffer_mapped(bh);
handle_zblock:
		zero_user(page, i * blocksize, blocksize);
		if (likely(!err))
			set_buffer_uptodate(bh);
	} while (i++, iblock++, (bh = bh->b_this_page) != head);

	
	if (rl)
		up_read(&ni->runlist.lock);

	
	if (nr) {
		struct buffer_head *tbh;

		
		for (i = 0; i < nr; i++) {
			tbh = arr[i];
			lock_buffer(tbh);
			tbh->b_end_io = ntfs_end_buffer_async_read;
			set_buffer_async_read(tbh);
		}
		
		for (i = 0; i < nr; i++) {
			tbh = arr[i];
			if (likely(!buffer_uptodate(tbh)))
				submit_bh(READ, tbh);
			else
				ntfs_end_buffer_async_read(tbh, 1);
		}
		return 0;
	}
	
	if (likely(!PageError(page)))
		SetPageUptodate(page);
	else 
		nr = -EIO;
	unlock_page(page);
	return nr;
}

static int ntfs_readpage(struct file *file, struct page *page)
{
	loff_t i_size;
	struct inode *vi;
	ntfs_inode *ni, *base_ni;
	u8 *addr;
	ntfs_attr_search_ctx *ctx;
	MFT_RECORD *mrec;
	unsigned long flags;
	u32 attr_len;
	int err = 0;

retry_readpage:
	BUG_ON(!PageLocked(page));
	vi = page->mapping->host;
	i_size = i_size_read(vi);
	
	if (unlikely(page->index >= (i_size + PAGE_CACHE_SIZE - 1) >>
			PAGE_CACHE_SHIFT)) {
		zero_user(page, 0, PAGE_CACHE_SIZE);
		ntfs_debug("Read outside i_size - truncated?");
		goto done;
	}
	if (PageUptodate(page)) {
		unlock_page(page);
		return 0;
	}
	ni = NTFS_I(vi);
	if (ni->type != AT_INDEX_ALLOCATION) {
		
		if (NInoEncrypted(ni)) {
			BUG_ON(ni->type != AT_DATA);
			err = -EACCES;
			goto err_out;
		}
		
		if (NInoNonResident(ni) && NInoCompressed(ni)) {
			BUG_ON(ni->type != AT_DATA);
			BUG_ON(ni->name_len);
			return ntfs_read_compressed_block(page);
		}
	}
	
	if (NInoNonResident(ni)) {
		
		return ntfs_read_block(page);
	}
	if (unlikely(page->index > 0)) {
		zero_user(page, 0, PAGE_CACHE_SIZE);
		goto done;
	}
	if (!NInoAttr(ni))
		base_ni = ni;
	else
		base_ni = ni->ext.base_ntfs_ino;
	
	mrec = map_mft_record(base_ni);
	if (IS_ERR(mrec)) {
		err = PTR_ERR(mrec);
		goto err_out;
	}
	if (unlikely(NInoNonResident(ni))) {
		unmap_mft_record(base_ni);
		goto retry_readpage;
	}
	ctx = ntfs_attr_get_search_ctx(base_ni, mrec);
	if (unlikely(!ctx)) {
		err = -ENOMEM;
		goto unm_err_out;
	}
	err = ntfs_attr_lookup(ni->type, ni->name, ni->name_len,
			CASE_SENSITIVE, 0, NULL, 0, ctx);
	if (unlikely(err))
		goto put_unm_err_out;
	attr_len = le32_to_cpu(ctx->attr->data.resident.value_length);
	read_lock_irqsave(&ni->size_lock, flags);
	if (unlikely(attr_len > ni->initialized_size))
		attr_len = ni->initialized_size;
	i_size = i_size_read(vi);
	read_unlock_irqrestore(&ni->size_lock, flags);
	if (unlikely(attr_len > i_size)) {
		
		attr_len = i_size;
	}
	addr = kmap_atomic(page);
	
	memcpy(addr, (u8*)ctx->attr +
			le16_to_cpu(ctx->attr->data.resident.value_offset),
			attr_len);
	
	memset(addr + attr_len, 0, PAGE_CACHE_SIZE - attr_len);
	flush_dcache_page(page);
	kunmap_atomic(addr);
put_unm_err_out:
	ntfs_attr_put_search_ctx(ctx);
unm_err_out:
	unmap_mft_record(base_ni);
done:
	SetPageUptodate(page);
err_out:
	unlock_page(page);
	return err;
}

#ifdef NTFS_RW

static int ntfs_write_block(struct page *page, struct writeback_control *wbc)
{
	VCN vcn;
	LCN lcn;
	s64 initialized_size;
	loff_t i_size;
	sector_t block, dblock, iblock;
	struct inode *vi;
	ntfs_inode *ni;
	ntfs_volume *vol;
	runlist_element *rl;
	struct buffer_head *bh, *head;
	unsigned long flags;
	unsigned int blocksize, vcn_ofs;
	int err;
	bool need_end_writeback;
	unsigned char blocksize_bits;

	vi = page->mapping->host;
	ni = NTFS_I(vi);
	vol = ni->vol;

	ntfs_debug("Entering for inode 0x%lx, attribute type 0x%x, page index "
			"0x%lx.", ni->mft_no, ni->type, page->index);

	BUG_ON(!NInoNonResident(ni));
	BUG_ON(NInoMstProtected(ni));
	blocksize = vol->sb->s_blocksize;
	blocksize_bits = vol->sb->s_blocksize_bits;
	if (!page_has_buffers(page)) {
		BUG_ON(!PageUptodate(page));
		create_empty_buffers(page, blocksize,
				(1 << BH_Uptodate) | (1 << BH_Dirty));
		if (unlikely(!page_has_buffers(page))) {
			ntfs_warning(vol->sb, "Error allocating page "
					"buffers.  Redirtying page so we try "
					"again later.");
			redirty_page_for_writepage(wbc, page);
			unlock_page(page);
			return 0;
		}
	}
	bh = head = page_buffers(page);
	BUG_ON(!bh);

	

	
	block = (s64)page->index << (PAGE_CACHE_SHIFT - blocksize_bits);

	read_lock_irqsave(&ni->size_lock, flags);
	i_size = i_size_read(vi);
	initialized_size = ni->initialized_size;
	read_unlock_irqrestore(&ni->size_lock, flags);

	
	dblock = (i_size + blocksize - 1) >> blocksize_bits;

	
	iblock = initialized_size >> blocksize_bits;


	rl = NULL;
	err = 0;
	do {
		bool is_retry = false;

		if (unlikely(block >= dblock)) {
			clear_buffer_dirty(bh);
			set_buffer_uptodate(bh);
			continue;
		}

		/* Clean buffers are not written out, so no need to map them. */
		if (!buffer_dirty(bh))
			continue;

		
		if (unlikely((block >= iblock) &&
				(initialized_size < i_size))) {
			if (block > iblock) {
				
				
				
				
				
				
				
				
				
				
				
				
				
				
			}
			/*
			 * The current page straddles initialized size. Zero
			 * all non-uptodate buffers and set them uptodate (and
			 * dirty?). Note, there aren't any non-uptodate buffers
			 * if the page is uptodate.
			 * FIXME: For an uptodate page, the buffers may need to
			 * be written out because they were not initialized on
			 * disk before.
			 */
			if (!PageUptodate(page)) {
				
				
				
			}
			
			
			
			
			
			
			ntfs_error(vol->sb, "Writing beyond initialized size "
					"is not supported yet. Sorry.");
			err = -EOPNOTSUPP;
			break;
			
			
			
			
		}

		
		if (buffer_mapped(bh))
			continue;

		
		bh->b_bdev = vol->sb->s_bdev;

		
		vcn = (VCN)block << blocksize_bits;
		vcn_ofs = vcn & vol->cluster_size_mask;
		vcn >>= vol->cluster_size_bits;
		if (!rl) {
lock_retry_remap:
			down_read(&ni->runlist.lock);
			rl = ni->runlist.rl;
		}
		if (likely(rl != NULL)) {
			
			while (rl->length && rl[1].vcn <= vcn)
				rl++;
			lcn = ntfs_rl_vcn_to_lcn(rl, vcn);
		} else
			lcn = LCN_RL_NOT_MAPPED;
		
		if (lcn >= 0) {
			
			bh->b_blocknr = ((lcn << vol->cluster_size_bits) +
					vcn_ofs) >> blocksize_bits;
			set_buffer_mapped(bh);
			continue;
		}
		
		if (lcn == LCN_HOLE) {
			u8 *kaddr;
			unsigned long *bpos, *bend;

			
			kaddr = kmap_atomic(page);
			bpos = (unsigned long *)(kaddr + bh_offset(bh));
			bend = (unsigned long *)((u8*)bpos + blocksize);
			do {
				if (unlikely(*bpos))
					break;
			} while (likely(++bpos < bend));
			kunmap_atomic(kaddr);
			if (bpos == bend) {
				bh->b_blocknr = -1;
				clear_buffer_dirty(bh);
				continue;
			}
			
			
			
			ntfs_error(vol->sb, "Writing into sparse regions is "
					"not supported yet. Sorry.");
			err = -EOPNOTSUPP;
			break;
		}
		
		if (!is_retry && lcn == LCN_RL_NOT_MAPPED) {
			is_retry = true;
			up_read(&ni->runlist.lock);
			err = ntfs_map_runlist(ni, vcn);
			if (likely(!err))
				goto lock_retry_remap;
			rl = NULL;
		} else if (!rl)
			up_read(&ni->runlist.lock);
		if (err == -ENOENT || lcn == LCN_ENOENT) {
			bh->b_blocknr = -1;
			clear_buffer_dirty(bh);
			zero_user(page, bh_offset(bh), blocksize);
			set_buffer_uptodate(bh);
			err = 0;
			continue;
		}
		
		if (!err)
			err = -EIO;
		bh->b_blocknr = -1;
		ntfs_error(vol->sb, "Failed to write to inode 0x%lx, "
				"attribute type 0x%x, vcn 0x%llx, offset 0x%x "
				"because its location on disk could not be "
				"determined%s (error code %i).", ni->mft_no,
				ni->type, (unsigned long long)vcn,
				vcn_ofs, is_retry ? " even after "
				"retrying" : "", err);
		break;
	} while (block++, (bh = bh->b_this_page) != head);

	
	if (rl)
		up_read(&ni->runlist.lock);

	
	bh = head;

	
	if (unlikely(!PageUptodate(page))) {
		int uptodate = 1;
		do {
			if (!buffer_uptodate(bh)) {
				uptodate = 0;
				bh = head;
				break;
			}
		} while ((bh = bh->b_this_page) != head);
		if (uptodate)
			SetPageUptodate(page);
	}

	
	do {
		if (buffer_mapped(bh) && buffer_dirty(bh)) {
			lock_buffer(bh);
			if (test_clear_buffer_dirty(bh)) {
				BUG_ON(!buffer_uptodate(bh));
				mark_buffer_async_write(bh);
			} else
				unlock_buffer(bh);
		} else if (unlikely(err)) {
			if (err != -ENOMEM)
				clear_buffer_dirty(bh);
		}
	} while ((bh = bh->b_this_page) != head);

	if (unlikely(err)) {
		
		if (unlikely(err == -EOPNOTSUPP))
			err = 0;
		else if (err == -ENOMEM) {
			ntfs_warning(vol->sb, "Error allocating memory. "
					"Redirtying page so we try again "
					"later.");
			redirty_page_for_writepage(wbc, page);
			err = 0;
		} else
			SetPageError(page);
	}

	BUG_ON(PageWriteback(page));
	set_page_writeback(page);	

	
	need_end_writeback = true;
	do {
		struct buffer_head *next = bh->b_this_page;
		if (buffer_async_write(bh)) {
			submit_bh(WRITE, bh);
			need_end_writeback = false;
		}
		bh = next;
	} while (bh != head);
	unlock_page(page);

	
	if (unlikely(need_end_writeback))
		end_page_writeback(page);

	ntfs_debug("Done.");
	return err;
}

static int ntfs_write_mst_block(struct page *page,
		struct writeback_control *wbc)
{
	sector_t block, dblock, rec_block;
	struct inode *vi = page->mapping->host;
	ntfs_inode *ni = NTFS_I(vi);
	ntfs_volume *vol = ni->vol;
	u8 *kaddr;
	unsigned int rec_size = ni->itype.index.block_size;
	ntfs_inode *locked_nis[PAGE_CACHE_SIZE / rec_size];
	struct buffer_head *bh, *head, *tbh, *rec_start_bh;
	struct buffer_head *bhs[MAX_BUF_PER_PAGE];
	runlist_element *rl;
	int i, nr_locked_nis, nr_recs, nr_bhs, max_bhs, bhs_per_rec, err, err2;
	unsigned bh_size, rec_size_bits;
	bool sync, is_mft, page_is_dirty, rec_is_dirty;
	unsigned char bh_size_bits;

	ntfs_debug("Entering for inode 0x%lx, attribute type 0x%x, page index "
			"0x%lx.", vi->i_ino, ni->type, page->index);
	BUG_ON(!NInoNonResident(ni));
	BUG_ON(!NInoMstProtected(ni));
	is_mft = (S_ISREG(vi->i_mode) && !vi->i_ino);
	BUG_ON(!(is_mft || S_ISDIR(vi->i_mode) ||
			(NInoAttr(ni) && ni->type == AT_INDEX_ALLOCATION)));
	bh_size = vol->sb->s_blocksize;
	bh_size_bits = vol->sb->s_blocksize_bits;
	max_bhs = PAGE_CACHE_SIZE / bh_size;
	BUG_ON(!max_bhs);
	BUG_ON(max_bhs > MAX_BUF_PER_PAGE);

	
	sync = (wbc->sync_mode == WB_SYNC_ALL);

	
	bh = head = page_buffers(page);
	BUG_ON(!bh);

	rec_size_bits = ni->itype.index.block_size_bits;
	BUG_ON(!(PAGE_CACHE_SIZE >> rec_size_bits));
	bhs_per_rec = rec_size >> bh_size_bits;
	BUG_ON(!bhs_per_rec);

	
	rec_block = block = (sector_t)page->index <<
			(PAGE_CACHE_SHIFT - bh_size_bits);

	
	dblock = (i_size_read(vi) + bh_size - 1) >> bh_size_bits;

	rl = NULL;
	err = err2 = nr_bhs = nr_recs = nr_locked_nis = 0;
	page_is_dirty = rec_is_dirty = false;
	rec_start_bh = NULL;
	do {
		bool is_retry = false;

		if (likely(block < rec_block)) {
			if (unlikely(block >= dblock)) {
				clear_buffer_dirty(bh);
				set_buffer_uptodate(bh);
				continue;
			}
			if (!rec_is_dirty)
				continue;
			if (unlikely(err2)) {
				if (err2 != -ENOMEM)
					clear_buffer_dirty(bh);
				continue;
			}
		} else  {
			BUG_ON(block > rec_block);
			
			rec_block += bhs_per_rec;
			err2 = 0;
			if (unlikely(block >= dblock)) {
				clear_buffer_dirty(bh);
				continue;
			}
			if (!buffer_dirty(bh)) {
				/* Clean records are not written out. */
				rec_is_dirty = false;
				continue;
			}
			rec_is_dirty = true;
			rec_start_bh = bh;
		}
		
		if (unlikely(!buffer_mapped(bh))) {
			VCN vcn;
			LCN lcn;
			unsigned int vcn_ofs;

			bh->b_bdev = vol->sb->s_bdev;
			
			vcn = (VCN)block << bh_size_bits;
			vcn_ofs = vcn & vol->cluster_size_mask;
			vcn >>= vol->cluster_size_bits;
			if (!rl) {
lock_retry_remap:
				down_read(&ni->runlist.lock);
				rl = ni->runlist.rl;
			}
			if (likely(rl != NULL)) {
				
				while (rl->length && rl[1].vcn <= vcn)
					rl++;
				lcn = ntfs_rl_vcn_to_lcn(rl, vcn);
			} else
				lcn = LCN_RL_NOT_MAPPED;
			
			if (likely(lcn >= 0)) {
				
				bh->b_blocknr = ((lcn <<
						vol->cluster_size_bits) +
						vcn_ofs) >> bh_size_bits;
				set_buffer_mapped(bh);
			} else {
				if (!is_mft && !is_retry &&
						lcn == LCN_RL_NOT_MAPPED) {
					is_retry = true;
					up_read(&ni->runlist.lock);
					err2 = ntfs_map_runlist(ni, vcn);
					if (likely(!err2))
						goto lock_retry_remap;
					if (err2 == -ENOMEM)
						page_is_dirty = true;
					lcn = err2;
				} else {
					err2 = -EIO;
					if (!rl)
						up_read(&ni->runlist.lock);
				}
				
				if (!err || err == -ENOMEM)
					err = err2;
				bh->b_blocknr = -1;
				ntfs_error(vol->sb, "Cannot write ntfs record "
						"0x%llx (inode 0x%lx, "
						"attribute type 0x%x) because "
						"its location on disk could "
						"not be determined (error "
						"code %lli).",
						(long long)block <<
						bh_size_bits >>
						vol->mft_record_size_bits,
						ni->mft_no, ni->type,
						(long long)lcn);
				if (rec_start_bh != bh) {
					while (bhs[--nr_bhs] != rec_start_bh)
						;
					if (err2 != -ENOMEM) {
						do {
							clear_buffer_dirty(
								rec_start_bh);
						} while ((rec_start_bh =
								rec_start_bh->
								b_this_page) !=
								bh);
					}
				}
				continue;
			}
		}
		BUG_ON(!buffer_uptodate(bh));
		BUG_ON(nr_bhs >= max_bhs);
		bhs[nr_bhs++] = bh;
	} while (block++, (bh = bh->b_this_page) != head);
	if (unlikely(rl))
		up_read(&ni->runlist.lock);
	
	if (!nr_bhs)
		goto done;
	
	kaddr = kmap(page);
	
	BUG_ON(!PageUptodate(page));
	ClearPageUptodate(page);
	for (i = 0; i < nr_bhs; i++) {
		unsigned int ofs;

		
		if (i % bhs_per_rec)
			continue;
		tbh = bhs[i];
		ofs = bh_offset(tbh);
		if (is_mft) {
			ntfs_inode *tni;
			unsigned long mft_no;

			
			mft_no = (((s64)page->index << PAGE_CACHE_SHIFT) + ofs)
					>> rec_size_bits;
			
			tni = NULL;
			if (!ntfs_may_write_mft_record(vol, mft_no,
					(MFT_RECORD*)(kaddr + ofs), &tni)) {
				/*
				 * The record should not be written.  This
				 * means we need to redirty the page before
				 * returning.
				 */
				page_is_dirty = true;
				do {
					bhs[i] = NULL;
				} while (++i % bhs_per_rec);
				continue;
			}
			/*
			 * The record should be written.  If a locked ntfs
			 * inode was returned, add it to the array of locked
			 * ntfs inodes.
			 */
			if (tni)
				locked_nis[nr_locked_nis++] = tni;
		}
		
		err2 = pre_write_mst_fixup((NTFS_RECORD*)(kaddr + ofs),
				rec_size);
		if (unlikely(err2)) {
			if (!err || err == -ENOMEM)
				err = -EIO;
			ntfs_error(vol->sb, "Failed to apply mst fixups "
					"(inode 0x%lx, attribute type 0x%x, "
					"page index 0x%lx, page offset 0x%x)!"
					"  Unmount and run chkdsk.", vi->i_ino,
					ni->type, page->index, ofs);
			do {
				clear_buffer_dirty(bhs[i]);
				bhs[i] = NULL;
			} while (++i % bhs_per_rec);
			continue;
		}
		nr_recs++;
	}
	/* If no records are to be written out, we are done. */
	if (!nr_recs)
		goto unm_done;
	flush_dcache_page(page);
	
	for (i = 0; i < nr_bhs; i++) {
		tbh = bhs[i];
		if (!tbh)
			continue;
		if (!trylock_buffer(tbh))
			BUG();
		
		clear_buffer_dirty(tbh);
		BUG_ON(!buffer_uptodate(tbh));
		BUG_ON(!buffer_mapped(tbh));
		get_bh(tbh);
		tbh->b_end_io = end_buffer_write_sync;
		submit_bh(WRITE, tbh);
	}
	
	if (is_mft && !sync)
		goto do_mirror;
do_wait:
	
	for (i = 0; i < nr_bhs; i++) {
		tbh = bhs[i];
		if (!tbh)
			continue;
		wait_on_buffer(tbh);
		if (unlikely(!buffer_uptodate(tbh))) {
			ntfs_error(vol->sb, "I/O error while writing ntfs "
					"record buffer (inode 0x%lx, "
					"attribute type 0x%x, page index "
					"0x%lx, page offset 0x%lx)!  Unmount "
					"and run chkdsk.", vi->i_ino, ni->type,
					page->index, bh_offset(tbh));
			if (!err || err == -ENOMEM)
				err = -EIO;
			set_buffer_uptodate(tbh);
		}
	}
	
	if (is_mft && sync) {
do_mirror:
		for (i = 0; i < nr_bhs; i++) {
			unsigned long mft_no;
			unsigned int ofs;

			if (i % bhs_per_rec)
				continue;
			tbh = bhs[i];
			
			if (!tbh)
				continue;
			ofs = bh_offset(tbh);
			
			mft_no = (((s64)page->index << PAGE_CACHE_SHIFT) + ofs)
					>> rec_size_bits;
			if (mft_no < vol->mftmirr_size)
				ntfs_sync_mft_mirror(vol, mft_no,
						(MFT_RECORD*)(kaddr + ofs),
						sync);
		}
		if (!sync)
			goto do_wait;
	}
	
	for (i = 0; i < nr_bhs; i++) {
		if (!(i % bhs_per_rec)) {
			tbh = bhs[i];
			if (!tbh)
				continue;
			post_write_mst_fixup((NTFS_RECORD*)(kaddr +
					bh_offset(tbh)));
		}
	}
	flush_dcache_page(page);
unm_done:
	
	while (nr_locked_nis-- > 0) {
		ntfs_inode *tni, *base_tni;
		
		tni = locked_nis[nr_locked_nis];
		
		mutex_lock(&tni->extent_lock);
		if (tni->nr_extents >= 0)
			base_tni = tni;
		else {
			base_tni = tni->ext.base_ntfs_ino;
			BUG_ON(!base_tni);
		}
		mutex_unlock(&tni->extent_lock);
		ntfs_debug("Unlocking %s inode 0x%lx.",
				tni == base_tni ? "base" : "extent",
				tni->mft_no);
		mutex_unlock(&tni->mrec_lock);
		atomic_dec(&tni->count);
		iput(VFS_I(base_tni));
	}
	SetPageUptodate(page);
	kunmap(page);
done:
	if (unlikely(err && err != -ENOMEM)) {
		if (ni->itype.index.block_size == PAGE_CACHE_SIZE)
			SetPageError(page);
		NVolSetErrors(vol);
	}
	if (page_is_dirty) {
		ntfs_debug("Page still contains one or more dirty ntfs "
				"records.  Redirtying the page starting at "
				"record 0x%lx.", page->index <<
				(PAGE_CACHE_SHIFT - rec_size_bits));
		redirty_page_for_writepage(wbc, page);
		unlock_page(page);
	} else {
		BUG_ON(PageWriteback(page));
		set_page_writeback(page);
		unlock_page(page);
		end_page_writeback(page);
	}
	if (likely(!err))
		ntfs_debug("Done.");
	return err;
}

/**
 * ntfs_writepage - write a @page to the backing store
 * @page:	page cache page to write out
 * @wbc:	writeback control structure
 *
 * This is called from the VM when it wants to have a dirty ntfs page cache
 * page cleaned.  The VM has already locked the page and marked it clean.
 *
 * For non-resident attributes, ntfs_writepage() writes the @page by calling
 * the ntfs version of the generic block_write_full_page() function,
 * ntfs_write_block(), which in turn if necessary creates and writes the
 * buffers associated with the page asynchronously.
 *
 * For resident attributes, OTOH, ntfs_writepage() writes the @page by copying
 * the data to the mft record (which at this stage is most likely in memory).
 * The mft record is then marked dirty and written out asynchronously via the
 * vfs inode dirty code path for the inode the mft record belongs to or via the
 * vm page dirty code path for the page the mft record is in.
 *
 * Based on ntfs_readpage() and fs/buffer.c::block_write_full_page().
 *
 * Return 0 on success and -errno on error.
 */
static int ntfs_writepage(struct page *page, struct writeback_control *wbc)
{
	loff_t i_size;
	struct inode *vi = page->mapping->host;
	ntfs_inode *base_ni = NULL, *ni = NTFS_I(vi);
	char *addr;
	ntfs_attr_search_ctx *ctx = NULL;
	MFT_RECORD *m = NULL;
	u32 attr_len;
	int err;

retry_writepage:
	BUG_ON(!PageLocked(page));
	i_size = i_size_read(vi);
	
	if (unlikely(page->index >= (i_size + PAGE_CACHE_SIZE - 1) >>
			PAGE_CACHE_SHIFT)) {
		block_invalidatepage(page, 0);
		unlock_page(page);
		ntfs_debug("Write outside i_size - truncated?");
		return 0;
	}
	if (ni->type != AT_INDEX_ALLOCATION) {
		
		if (NInoEncrypted(ni)) {
			unlock_page(page);
			BUG_ON(ni->type != AT_DATA);
			ntfs_debug("Denying write access to encrypted file.");
			return -EACCES;
		}
		
		if (NInoNonResident(ni) && NInoCompressed(ni)) {
			BUG_ON(ni->type != AT_DATA);
			BUG_ON(ni->name_len);
			
			
			unlock_page(page);
			ntfs_error(vi->i_sb, "Writing to compressed files is "
					"not supported yet.  Sorry.");
			return -EOPNOTSUPP;
		}
		
		if (NInoNonResident(ni) && NInoSparse(ni)) {
			unlock_page(page);
			ntfs_error(vi->i_sb, "Writing to sparse files is not "
					"supported yet.  Sorry.");
			return -EOPNOTSUPP;
		}
	}
	
	if (NInoNonResident(ni)) {
		
		if (page->index >= (i_size >> PAGE_CACHE_SHIFT)) {
			
			unsigned int ofs = i_size & ~PAGE_CACHE_MASK;
			zero_user_segment(page, ofs, PAGE_CACHE_SIZE);
		}
		
		if (NInoMstProtected(ni))
			return ntfs_write_mst_block(page, wbc);
		
		return ntfs_write_block(page, wbc);
	}
	BUG_ON(page_has_buffers(page));
	BUG_ON(!PageUptodate(page));
	if (unlikely(page->index > 0)) {
		ntfs_error(vi->i_sb, "BUG()! page->index (0x%lx) > 0.  "
				"Aborting write.", page->index);
		BUG_ON(PageWriteback(page));
		set_page_writeback(page);
		unlock_page(page);
		end_page_writeback(page);
		return -EIO;
	}
	if (!NInoAttr(ni))
		base_ni = ni;
	else
		base_ni = ni->ext.base_ntfs_ino;
	
	m = map_mft_record(base_ni);
	if (IS_ERR(m)) {
		err = PTR_ERR(m);
		m = NULL;
		ctx = NULL;
		goto err_out;
	}
	if (unlikely(NInoNonResident(ni))) {
		unmap_mft_record(base_ni);
		goto retry_writepage;
	}
	ctx = ntfs_attr_get_search_ctx(base_ni, m);
	if (unlikely(!ctx)) {
		err = -ENOMEM;
		goto err_out;
	}
	err = ntfs_attr_lookup(ni->type, ni->name, ni->name_len,
			CASE_SENSITIVE, 0, NULL, 0, ctx);
	if (unlikely(err))
		goto err_out;
	BUG_ON(PageWriteback(page));
	set_page_writeback(page);
	unlock_page(page);
	attr_len = le32_to_cpu(ctx->attr->data.resident.value_length);
	i_size = i_size_read(vi);
	if (unlikely(attr_len > i_size)) {
		
		attr_len = i_size;
		err = ntfs_resident_attr_value_resize(ctx->mrec, ctx->attr,
				attr_len);
		
		BUG_ON(err);
	}
	addr = kmap_atomic(page);
	
	memcpy((u8*)ctx->attr +
			le16_to_cpu(ctx->attr->data.resident.value_offset),
			addr, attr_len);
	
	memset(addr + attr_len, 0, PAGE_CACHE_SIZE - attr_len);
	kunmap_atomic(addr);
	flush_dcache_page(page);
	flush_dcache_mft_record_page(ctx->ntfs_ino);
	
	end_page_writeback(page);
	/* Finally, mark the mft record dirty, so it gets written back. */
	mark_mft_record_dirty(ctx->ntfs_ino);
	ntfs_attr_put_search_ctx(ctx);
	unmap_mft_record(base_ni);
	return 0;
err_out:
	if (err == -ENOMEM) {
		ntfs_warning(vi->i_sb, "Error allocating memory. Redirtying "
				"page so we try again later.");
		redirty_page_for_writepage(wbc, page);
		err = 0;
	} else {
		ntfs_error(vi->i_sb, "Resident attribute write failed with "
				"error %i.", err);
		SetPageError(page);
		NVolSetErrors(ni->vol);
	}
	unlock_page(page);
	if (ctx)
		ntfs_attr_put_search_ctx(ctx);
	if (m)
		unmap_mft_record(base_ni);
	return err;
}

#endif	

const struct address_space_operations ntfs_aops = {
	.readpage	= ntfs_readpage,	
#ifdef NTFS_RW
	.writepage	= ntfs_writepage,	
#endif 
	.migratepage	= buffer_migrate_page,	
	.error_remove_page = generic_error_remove_page,
};

const struct address_space_operations ntfs_mst_aops = {
	.readpage	= ntfs_readpage,	
#ifdef NTFS_RW
	.writepage	= ntfs_writepage,	
	.set_page_dirty	= __set_page_dirty_nobuffers,	
#endif 
	.migratepage	= buffer_migrate_page,	
	.error_remove_page = generic_error_remove_page,
};

#ifdef NTFS_RW

void mark_ntfs_record_dirty(struct page *page, const unsigned int ofs) {
	struct address_space *mapping = page->mapping;
	ntfs_inode *ni = NTFS_I(mapping->host);
	struct buffer_head *bh, *head, *buffers_to_free = NULL;
	unsigned int end, bh_size, bh_ofs;

	BUG_ON(!PageUptodate(page));
	end = ofs + ni->itype.index.block_size;
	bh_size = VFS_I(ni)->i_sb->s_blocksize;
	spin_lock(&mapping->private_lock);
	if (unlikely(!page_has_buffers(page))) {
		spin_unlock(&mapping->private_lock);
		bh = head = alloc_page_buffers(page, bh_size, 1);
		spin_lock(&mapping->private_lock);
		if (likely(!page_has_buffers(page))) {
			struct buffer_head *tail;

			do {
				set_buffer_uptodate(bh);
				tail = bh;
				bh = bh->b_this_page;
			} while (bh);
			tail->b_this_page = head;
			attach_page_buffers(page, head);
		} else
			buffers_to_free = bh;
	}
	bh = head = page_buffers(page);
	BUG_ON(!bh);
	do {
		bh_ofs = bh_offset(bh);
		if (bh_ofs + bh_size <= ofs)
			continue;
		if (unlikely(bh_ofs >= end))
			break;
		set_buffer_dirty(bh);
	} while ((bh = bh->b_this_page) != head);
	spin_unlock(&mapping->private_lock);
	__set_page_dirty_nobuffers(page);
	if (unlikely(buffers_to_free)) {
		do {
			bh = buffers_to_free->b_this_page;
			free_buffer_head(buffers_to_free);
			buffers_to_free = bh;
		} while (buffers_to_free);
	}
}

#endif 
