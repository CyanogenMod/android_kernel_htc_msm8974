/**
 * compress.c - NTFS kernel compressed attributes handling.
 *		Part of the Linux-NTFS project.
 *
 * Copyright (c) 2001-2004 Anton Altaparmakov
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

#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/blkdev.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>

#include "attrib.h"
#include "inode.h"
#include "debug.h"
#include "ntfs.h"

typedef enum {
	
	NTFS_SYMBOL_TOKEN	=	0,
	NTFS_PHRASE_TOKEN	=	1,
	NTFS_TOKEN_MASK		=	1,

	
	NTFS_SB_SIZE_MASK	=	0x0fff,
	NTFS_SB_SIZE		=	0x1000,
	NTFS_SB_IS_COMPRESSED	=	0x8000,

	NTFS_MAX_CB_SIZE	= 64 * 1024,
} ntfs_compression_constants;

static u8 *ntfs_compression_buffer = NULL;

static DEFINE_SPINLOCK(ntfs_cb_lock);

int allocate_compression_buffers(void)
{
	BUG_ON(ntfs_compression_buffer);

	ntfs_compression_buffer = vmalloc(NTFS_MAX_CB_SIZE);
	if (!ntfs_compression_buffer)
		return -ENOMEM;
	return 0;
}

void free_compression_buffers(void)
{
	BUG_ON(!ntfs_compression_buffer);
	vfree(ntfs_compression_buffer);
	ntfs_compression_buffer = NULL;
}

static void zero_partial_compressed_page(struct page *page,
		const s64 initialized_size)
{
	u8 *kp = page_address(page);
	unsigned int kp_ofs;

	ntfs_debug("Zeroing page region outside initialized size.");
	if (((s64)page->index << PAGE_CACHE_SHIFT) >= initialized_size) {
		clear_page(kp);
		return;
	}
	kp_ofs = initialized_size & ~PAGE_CACHE_MASK;
	memset(kp + kp_ofs, 0, PAGE_CACHE_SIZE - kp_ofs);
	return;
}

static inline void handle_bounds_compressed_page(struct page *page,
		const loff_t i_size, const s64 initialized_size)
{
	if ((page->index >= (initialized_size >> PAGE_CACHE_SHIFT)) &&
			(initialized_size < i_size))
		zero_partial_compressed_page(page, initialized_size);
	return;
}

static int ntfs_decompress(struct page *dest_pages[], int *dest_index,
		int *dest_ofs, const int dest_max_index, const int dest_max_ofs,
		const int xpage, char *xpage_done, u8 *const cb_start,
		const u32 cb_size, const loff_t i_size,
		const s64 initialized_size)
{
	u8 *cb_end = cb_start + cb_size; 
	u8 *cb = cb_start;	
	u8 *cb_sb_start = cb;	
	u8 *cb_sb_end;		

	
	struct page *dp;	
	u8 *dp_addr;		
	u8 *dp_sb_start;	
	u8 *dp_sb_end;		
	u16 do_sb_start;	
	u16 do_sb_end;		

	
	u8 tag;			
	int token;		

	
	int completed_pages[dest_max_index - *dest_index + 1];
	int nr_completed_pages = 0;

	
	int err = -EOVERFLOW;

	ntfs_debug("Entering, cb_size = 0x%x.", cb_size);
do_next_sb:
	ntfs_debug("Beginning sub-block at offset = 0x%zx in the cb.",
			cb - cb_start);
	if (cb == cb_end || !le16_to_cpup((le16*)cb) ||
			(*dest_index == dest_max_index &&
			*dest_ofs == dest_max_ofs)) {
		int i;

		ntfs_debug("Completed. Returning success (0).");
		err = 0;
return_error:
		
		spin_unlock(&ntfs_cb_lock);
		
		if (nr_completed_pages > 0) {
			for (i = 0; i < nr_completed_pages; i++) {
				int di = completed_pages[i];

				dp = dest_pages[di];
				handle_bounds_compressed_page(dp, i_size,
						initialized_size);
				flush_dcache_page(dp);
				kunmap(dp);
				SetPageUptodate(dp);
				unlock_page(dp);
				if (di == xpage)
					*xpage_done = 1;
				else
					page_cache_release(dp);
				dest_pages[di] = NULL;
			}
		}
		return err;
	}

	
	do_sb_start = *dest_ofs;
	do_sb_end = do_sb_start + NTFS_SB_SIZE;

	
	if (*dest_index == dest_max_index && do_sb_end > dest_max_ofs)
		goto return_overflow;

	
	if (cb + 6 > cb_end)
		goto return_overflow;

	
	cb_sb_start = cb;
	cb_sb_end = cb_sb_start + (le16_to_cpup((le16*)cb) & NTFS_SB_SIZE_MASK)
			+ 3;
	if (cb_sb_end > cb_end)
		goto return_overflow;

	
	dp = dest_pages[*dest_index];
	if (!dp) {
		
		cb = cb_sb_end;

		
		*dest_ofs = (*dest_ofs + NTFS_SB_SIZE) & ~PAGE_CACHE_MASK;
		if (!*dest_ofs && (++*dest_index > dest_max_index))
			goto return_overflow;
		goto do_next_sb;
	}

	
	dp_addr = (u8*)page_address(dp) + do_sb_start;

	
	if (!(le16_to_cpup((le16*)cb) & NTFS_SB_IS_COMPRESSED)) {
		ntfs_debug("Found uncompressed sub-block.");
		

		
		cb += 2;

		
		if (cb_sb_end - cb != NTFS_SB_SIZE)
			goto return_overflow;

		
		memcpy(dp_addr, cb, NTFS_SB_SIZE);
		cb += NTFS_SB_SIZE;

		
		*dest_ofs += NTFS_SB_SIZE;
		if (!(*dest_ofs &= ~PAGE_CACHE_MASK)) {
finalize_page:
			completed_pages[nr_completed_pages++] = *dest_index;
			if (++*dest_index > dest_max_index)
				goto return_overflow;
		}
		goto do_next_sb;
	}
	ntfs_debug("Found compressed sub-block.");
	

	
	dp_sb_start = dp_addr;
	dp_sb_end = dp_sb_start + NTFS_SB_SIZE;

	
	cb += 2;
do_next_tag:
	if (cb == cb_sb_end) {
		
		if (dp_addr < dp_sb_end) {
			int nr_bytes = do_sb_end - *dest_ofs;

			ntfs_debug("Filling incomplete sub-block with "
					"zeroes.");
			
			memset(dp_addr, 0, nr_bytes);
			*dest_ofs += nr_bytes;
		}
		
		if (!(*dest_ofs &= ~PAGE_CACHE_MASK))
			goto finalize_page;
		goto do_next_sb;
	}

	
	if (cb > cb_sb_end || dp_addr > dp_sb_end)
		goto return_overflow;

	
	tag = *cb++;

	
	for (token = 0; token < 8; token++, tag >>= 1) {
		u16 lg, pt, length, max_non_overlap;
		register u16 i;
		u8 *dp_back_addr;

		
		if (cb >= cb_sb_end || dp_addr > dp_sb_end)
			break;

		
		if ((tag & NTFS_TOKEN_MASK) == NTFS_SYMBOL_TOKEN) {
			*dp_addr++ = *cb++;
			++*dest_ofs;

			
			continue;
		}

		if (dp_addr == dp_sb_start)
			goto return_overflow;

		lg = 0;
		for (i = *dest_ofs - do_sb_start - 1; i >= 0x10; i >>= 1)
			lg++;

		
		pt = le16_to_cpup((le16*)cb);

		dp_back_addr = dp_addr - (pt >> (12 - lg)) - 1;
		if (dp_back_addr < dp_sb_start)
			goto return_overflow;

		
		length = (pt & (0xfff >> lg)) + 3;

		
		*dest_ofs += length;
		if (*dest_ofs > do_sb_end)
			goto return_overflow;

		
		max_non_overlap = dp_addr - dp_back_addr;

		if (length <= max_non_overlap) {
			
			memcpy(dp_addr, dp_back_addr, length);

			
			dp_addr += length;
		} else {
			memcpy(dp_addr, dp_back_addr, max_non_overlap);
			dp_addr += max_non_overlap;
			dp_back_addr += max_non_overlap;
			length -= max_non_overlap;
			while (length--)
				*dp_addr++ = *dp_back_addr++;
		}

		
		cb += 2;
	}

	
	goto do_next_tag;

return_overflow:
	ntfs_error(NULL, "Failed. Returning -EOVERFLOW.");
	goto return_error;
}

/**
 * ntfs_read_compressed_block - read a compressed block into the page cache
 * @page:	locked page in the compression block(s) we need to read
 *
 * When we are called the page has already been verified to be locked and the
 * attribute is known to be non-resident, not encrypted, but compressed.
 *
 * 1. Determine which compression block(s) @page is in.
 * 2. Get hold of all pages corresponding to this/these compression block(s).
 * 3. Read the (first) compression block.
 * 4. Decompress it into the corresponding pages.
 * 5. Throw the compressed data away and proceed to 3. for the next compression
 *    block or return success if no more compression blocks left.
 *
 * Warning: We have to be careful what we do about existing pages. They might
 * have been written to so that we would lose data if we were to just overwrite
 * them with the out-of-date uncompressed data.
 *
 * FIXME: For PAGE_CACHE_SIZE > cb_size we are not doing the Right Thing(TM) at
 * the end of the file I think. We need to detect this case and zero the out
 * of bounds remainder of the page in question and mark it as handled. At the
 * moment we would just return -EIO on such a page. This bug will only become
 * apparent if pages are above 8kiB and the NTFS volume only uses 512 byte
 * clusters so is probably not going to be seen by anyone. Still this should
 * be fixed. (AIA)
 *
 * FIXME: Again for PAGE_CACHE_SIZE > cb_size we are screwing up both in
 * handling sparse and compressed cbs. (AIA)
 *
 * FIXME: At the moment we don't do any zeroing out in the case that
 * initialized_size is less than data_size. This should be safe because of the
 * nature of the compression algorithm used. Just in case we check and output
 * an error message in read inode if the two sizes are not equal for a
 * compressed file. (AIA)
 */
int ntfs_read_compressed_block(struct page *page)
{
	loff_t i_size;
	s64 initialized_size;
	struct address_space *mapping = page->mapping;
	ntfs_inode *ni = NTFS_I(mapping->host);
	ntfs_volume *vol = ni->vol;
	struct super_block *sb = vol->sb;
	runlist_element *rl;
	unsigned long flags, block_size = sb->s_blocksize;
	unsigned char block_size_bits = sb->s_blocksize_bits;
	u8 *cb, *cb_pos, *cb_end;
	struct buffer_head **bhs;
	unsigned long offset, index = page->index;
	u32 cb_size = ni->itype.compressed.block_size;
	u64 cb_size_mask = cb_size - 1UL;
	VCN vcn;
	LCN lcn;
	
	VCN start_vcn = (((s64)index << PAGE_CACHE_SHIFT) & ~cb_size_mask) >>
			vol->cluster_size_bits;
	VCN end_vcn = ((((s64)(index + 1UL) << PAGE_CACHE_SHIFT) + cb_size - 1)
			& ~cb_size_mask) >> vol->cluster_size_bits;
	
	unsigned int nr_cbs = (end_vcn - start_vcn) << vol->cluster_size_bits
			>> ni->itype.compressed.block_size_bits;
	unsigned int nr_pages = (end_vcn - start_vcn) <<
			vol->cluster_size_bits >> PAGE_CACHE_SHIFT;
	unsigned int xpage, max_page, cur_page, cur_ofs, i;
	unsigned int cb_clusters, cb_max_ofs;
	int block, max_block, cb_max_page, bhs_size, nr_bhs, err = 0;
	struct page **pages;
	unsigned char xpage_done = 0;

	ntfs_debug("Entering, page->index = 0x%lx, cb_size = 0x%x, nr_pages = "
			"%i.", index, cb_size, nr_pages);
	BUG_ON(ni->type != AT_DATA);
	BUG_ON(ni->name_len);

	pages = kmalloc(nr_pages * sizeof(struct page *), GFP_NOFS);

	
	bhs_size = cb_size / block_size * sizeof(struct buffer_head *);
	bhs = kmalloc(bhs_size, GFP_NOFS);

	if (unlikely(!pages || !bhs)) {
		kfree(bhs);
		kfree(pages);
		unlock_page(page);
		ntfs_error(vol->sb, "Failed to allocate internal buffers.");
		return -ENOMEM;
	}

	offset = start_vcn << vol->cluster_size_bits >> PAGE_CACHE_SHIFT;
	xpage = index - offset;
	pages[xpage] = page;
	read_lock_irqsave(&ni->size_lock, flags);
	i_size = i_size_read(VFS_I(ni));
	initialized_size = ni->initialized_size;
	read_unlock_irqrestore(&ni->size_lock, flags);
	max_page = ((i_size + PAGE_CACHE_SIZE - 1) >> PAGE_CACHE_SHIFT) -
			offset;
	
	if (xpage >= max_page) {
		kfree(bhs);
		kfree(pages);
		zero_user(page, 0, PAGE_CACHE_SIZE);
		ntfs_debug("Compressed read outside i_size - truncated?");
		SetPageUptodate(page);
		unlock_page(page);
		return 0;
	}
	if (nr_pages < max_page)
		max_page = nr_pages;
	for (i = 0; i < max_page; i++, offset++) {
		if (i != xpage)
			pages[i] = grab_cache_page_nowait(mapping, offset);
		page = pages[i];
		if (page) {
			if (!PageDirty(page) && (!PageUptodate(page) ||
					PageError(page))) {
				ClearPageError(page);
				kmap(page);
				continue;
			}
			unlock_page(page);
			page_cache_release(page);
			pages[i] = NULL;
		}
	}

	cur_page = 0;
	cur_ofs = 0;
	cb_clusters = ni->itype.compressed.block_clusters;
do_next_cb:
	nr_cbs--;
	nr_bhs = 0;

	
	rl = NULL;
	for (vcn = start_vcn, start_vcn += cb_clusters; vcn < start_vcn;
			vcn++) {
		bool is_retry = false;

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
		ntfs_debug("Reading vcn = 0x%llx, lcn = 0x%llx.",
				(unsigned long long)vcn,
				(unsigned long long)lcn);
		if (lcn < 0) {
			if (lcn == LCN_HOLE)
				break;
			if (is_retry || lcn != LCN_RL_NOT_MAPPED)
				goto rl_err;
			is_retry = true;
			up_read(&ni->runlist.lock);
			if (!ntfs_map_runlist(ni, vcn))
				goto lock_retry_remap;
			goto map_rl_err;
		}
		block = lcn << vol->cluster_size_bits >> block_size_bits;
		
		max_block = block + (vol->cluster_size >> block_size_bits);
		do {
			ntfs_debug("block = 0x%x.", block);
			if (unlikely(!(bhs[nr_bhs] = sb_getblk(sb, block))))
				goto getblk_err;
			nr_bhs++;
		} while (++block < max_block);
	}

	
	if (rl)
		up_read(&ni->runlist.lock);

	
	for (i = 0; i < nr_bhs; i++) {
		struct buffer_head *tbh = bhs[i];

		if (!trylock_buffer(tbh))
			continue;
		if (unlikely(buffer_uptodate(tbh))) {
			unlock_buffer(tbh);
			continue;
		}
		get_bh(tbh);
		tbh->b_end_io = end_buffer_read_sync;
		submit_bh(READ, tbh);
	}

	
	for (i = 0; i < nr_bhs; i++) {
		struct buffer_head *tbh = bhs[i];

		if (buffer_uptodate(tbh))
			continue;
		wait_on_buffer(tbh);
		barrier();
		if (unlikely(!buffer_uptodate(tbh))) {
			ntfs_warning(vol->sb, "Buffer is unlocked but not "
					"uptodate! Unplugging the disk queue "
					"and rescheduling.");
			get_bh(tbh);
			io_schedule();
			put_bh(tbh);
			if (unlikely(!buffer_uptodate(tbh)))
				goto read_err;
			ntfs_warning(vol->sb, "Buffer is now uptodate. Good.");
		}
	}

	spin_lock(&ntfs_cb_lock);
	cb = ntfs_compression_buffer;

	BUG_ON(!cb);

	cb_pos = cb;
	cb_end = cb + cb_size;

	
	for (i = 0; i < nr_bhs; i++) {
		memcpy(cb_pos, bhs[i]->b_data, block_size);
		cb_pos += block_size;
	}

	
	if (cb_pos + 2 <= cb + cb_size)
		*(u16*)cb_pos = 0;

	
	cb_pos = cb;

	
	ntfs_debug("Successfully read the compression block.");

	
	cb_max_page = (cur_page << PAGE_CACHE_SHIFT) + cur_ofs + cb_size;
	cb_max_ofs = cb_max_page & ~PAGE_CACHE_MASK;
	cb_max_page >>= PAGE_CACHE_SHIFT;

	
	if (cb_max_page > max_page)
		cb_max_page = max_page;

	if (vcn == start_vcn - cb_clusters) {
		
		ntfs_debug("Found sparse compression block.");
		
		spin_unlock(&ntfs_cb_lock);
		if (cb_max_ofs)
			cb_max_page--;
		for (; cur_page < cb_max_page; cur_page++) {
			page = pages[cur_page];
			if (page) {
				if (likely(!cur_ofs))
					clear_page(page_address(page));
				else
					memset(page_address(page) + cur_ofs, 0,
							PAGE_CACHE_SIZE -
							cur_ofs);
				flush_dcache_page(page);
				kunmap(page);
				SetPageUptodate(page);
				unlock_page(page);
				if (cur_page == xpage)
					xpage_done = 1;
				else
					page_cache_release(page);
				pages[cur_page] = NULL;
			}
			cb_pos += PAGE_CACHE_SIZE - cur_ofs;
			cur_ofs = 0;
			if (cb_pos >= cb_end)
				break;
		}
		
		if (cb_max_ofs && cb_pos < cb_end) {
			page = pages[cur_page];
			if (page)
				memset(page_address(page) + cur_ofs, 0,
						cb_max_ofs - cur_ofs);
			cur_ofs = cb_max_ofs;
		}
	} else if (vcn == start_vcn) {
		
		unsigned int cur2_page = cur_page;
		unsigned int cur_ofs2 = cur_ofs;
		u8 *cb_pos2 = cb_pos;

		ntfs_debug("Found uncompressed compression block.");
		
		if (cb_max_ofs)
			cb_max_page--;
		
		for (; cur_page < cb_max_page; cur_page++) {
			page = pages[cur_page];
			if (page)
				memcpy(page_address(page) + cur_ofs, cb_pos,
						PAGE_CACHE_SIZE - cur_ofs);
			cb_pos += PAGE_CACHE_SIZE - cur_ofs;
			cur_ofs = 0;
			if (cb_pos >= cb_end)
				break;
		}
		
		if (cb_max_ofs && cb_pos < cb_end) {
			page = pages[cur_page];
			if (page)
				memcpy(page_address(page) + cur_ofs, cb_pos,
						cb_max_ofs - cur_ofs);
			cb_pos += cb_max_ofs - cur_ofs;
			cur_ofs = cb_max_ofs;
		}
		
		spin_unlock(&ntfs_cb_lock);
		
		for (; cur2_page < cb_max_page; cur2_page++) {
			page = pages[cur2_page];
			if (page) {
				handle_bounds_compressed_page(page, i_size,
						initialized_size);
				flush_dcache_page(page);
				kunmap(page);
				SetPageUptodate(page);
				unlock_page(page);
				if (cur2_page == xpage)
					xpage_done = 1;
				else
					page_cache_release(page);
				pages[cur2_page] = NULL;
			}
			cb_pos2 += PAGE_CACHE_SIZE - cur_ofs2;
			cur_ofs2 = 0;
			if (cb_pos2 >= cb_end)
				break;
		}
	} else {
		
		unsigned int prev_cur_page = cur_page;

		ntfs_debug("Found compressed compression block.");
		err = ntfs_decompress(pages, &cur_page, &cur_ofs,
				cb_max_page, cb_max_ofs, xpage, &xpage_done,
				cb_pos,	cb_size - (cb_pos - cb), i_size,
				initialized_size);
		if (err) {
			ntfs_error(vol->sb, "ntfs_decompress() failed in inode "
					"0x%lx with error code %i. Skipping "
					"this compression block.",
					ni->mft_no, -err);
			
			for (; prev_cur_page < cur_page; prev_cur_page++) {
				page = pages[prev_cur_page];
				if (page) {
					flush_dcache_page(page);
					kunmap(page);
					unlock_page(page);
					if (prev_cur_page != xpage)
						page_cache_release(page);
					pages[prev_cur_page] = NULL;
				}
			}
		}
	}

	
	for (i = 0; i < nr_bhs; i++)
		brelse(bhs[i]);

	
	if (nr_cbs)
		goto do_next_cb;

	
	kfree(bhs);

	
	for (cur_page = 0; cur_page < max_page; cur_page++) {
		page = pages[cur_page];
		if (page) {
			ntfs_error(vol->sb, "Still have pages left! "
					"Terminating them with extreme "
					"prejudice.  Inode 0x%lx, page index "
					"0x%lx.", ni->mft_no, page->index);
			flush_dcache_page(page);
			kunmap(page);
			unlock_page(page);
			if (cur_page != xpage)
				page_cache_release(page);
			pages[cur_page] = NULL;
		}
	}

	
	kfree(pages);

	
	if (likely(xpage_done))
		return 0;

	ntfs_debug("Failed. Returning error code %s.", err == -EOVERFLOW ?
			"EOVERFLOW" : (!err ? "EIO" : "unknown error"));
	return err < 0 ? err : -EIO;

read_err:
	ntfs_error(vol->sb, "IO error while reading compressed data.");
	
	for (i = 0; i < nr_bhs; i++)
		brelse(bhs[i]);
	goto err_out;

map_rl_err:
	ntfs_error(vol->sb, "ntfs_map_runlist() failed. Cannot read "
			"compression block.");
	goto err_out;

rl_err:
	up_read(&ni->runlist.lock);
	ntfs_error(vol->sb, "ntfs_rl_vcn_to_lcn() failed. Cannot read "
			"compression block.");
	goto err_out;

getblk_err:
	up_read(&ni->runlist.lock);
	ntfs_error(vol->sb, "getblk() failed. Cannot read compression block.");

err_out:
	kfree(bhs);
	for (i = cur_page; i < max_page; i++) {
		page = pages[i];
		if (page) {
			flush_dcache_page(page);
			kunmap(page);
			unlock_page(page);
			if (i != xpage)
				page_cache_release(page);
		}
	}
	kfree(pages);
	return -EIO;
}
