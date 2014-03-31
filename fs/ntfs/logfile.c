/*
 * logfile.c - NTFS kernel journal handling. Part of the Linux-NTFS project.
 *
 * Copyright (c) 2002-2007 Anton Altaparmakov
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

#ifdef NTFS_RW

#include <linux/types.h>
#include <linux/fs.h>
#include <linux/highmem.h>
#include <linux/buffer_head.h>
#include <linux/bitops.h>
#include <linux/log2.h>

#include "attrib.h"
#include "aops.h"
#include "debug.h"
#include "logfile.h"
#include "malloc.h"
#include "volume.h"
#include "ntfs.h"

static bool ntfs_check_restart_page_header(struct inode *vi,
		RESTART_PAGE_HEADER *rp, s64 pos)
{
	u32 logfile_system_page_size, logfile_log_page_size;
	u16 ra_ofs, usa_count, usa_ofs, usa_end = 0;
	bool have_usa = true;

	ntfs_debug("Entering.");
	logfile_system_page_size = le32_to_cpu(rp->system_page_size);
	logfile_log_page_size = le32_to_cpu(rp->log_page_size);
	if (logfile_system_page_size < NTFS_BLOCK_SIZE ||
			logfile_log_page_size < NTFS_BLOCK_SIZE ||
			logfile_system_page_size &
			(logfile_system_page_size - 1) ||
			!is_power_of_2(logfile_log_page_size)) {
		ntfs_error(vi->i_sb, "$LogFile uses unsupported page size.");
		return false;
	}
	if (pos && pos != logfile_system_page_size) {
		ntfs_error(vi->i_sb, "Found restart area in incorrect "
				"position in $LogFile.");
		return false;
	}
	
	if (sle16_to_cpu(rp->major_ver) != 1 ||
			sle16_to_cpu(rp->minor_ver) != 1) {
		ntfs_error(vi->i_sb, "$LogFile version %i.%i is not "
				"supported.  (This driver supports version "
				"1.1 only.)", (int)sle16_to_cpu(rp->major_ver),
				(int)sle16_to_cpu(rp->minor_ver));
		return false;
	}
	if (ntfs_is_chkd_record(rp->magic) && !le16_to_cpu(rp->usa_count)) {
		have_usa = false;
		goto skip_usa_checks;
	}
	
	usa_count = 1 + (logfile_system_page_size >> NTFS_BLOCK_SIZE_BITS);
	if (usa_count != le16_to_cpu(rp->usa_count)) {
		ntfs_error(vi->i_sb, "$LogFile restart page specifies "
				"inconsistent update sequence array count.");
		return false;
	}
	
	usa_ofs = le16_to_cpu(rp->usa_ofs);
	usa_end = usa_ofs + usa_count * sizeof(u16);
	if (usa_ofs < sizeof(RESTART_PAGE_HEADER) ||
			usa_end > NTFS_BLOCK_SIZE - sizeof(u16)) {
		ntfs_error(vi->i_sb, "$LogFile restart page specifies "
				"inconsistent update sequence array offset.");
		return false;
	}
skip_usa_checks:
	ra_ofs = le16_to_cpu(rp->restart_area_offset);
	if (ra_ofs & 7 || (have_usa ? ra_ofs < usa_end :
			ra_ofs < sizeof(RESTART_PAGE_HEADER)) ||
			ra_ofs > logfile_system_page_size) {
		ntfs_error(vi->i_sb, "$LogFile restart page specifies "
				"inconsistent restart area offset.");
		return false;
	}
	if (!ntfs_is_chkd_record(rp->magic) && sle64_to_cpu(rp->chkdsk_lsn)) {
		ntfs_error(vi->i_sb, "$LogFile restart page is not modified "
				"by chkdsk but a chkdsk LSN is specified.");
		return false;
	}
	ntfs_debug("Done.");
	return true;
}

static bool ntfs_check_restart_area(struct inode *vi, RESTART_PAGE_HEADER *rp)
{
	u64 file_size;
	RESTART_AREA *ra;
	u16 ra_ofs, ra_len, ca_ofs;
	u8 fs_bits;

	ntfs_debug("Entering.");
	ra_ofs = le16_to_cpu(rp->restart_area_offset);
	ra = (RESTART_AREA*)((u8*)rp + ra_ofs);
	if (ra_ofs + offsetof(RESTART_AREA, file_size) >
			NTFS_BLOCK_SIZE - sizeof(u16)) {
		ntfs_error(vi->i_sb, "$LogFile restart area specifies "
				"inconsistent file offset.");
		return false;
	}
	ca_ofs = le16_to_cpu(ra->client_array_offset);
	if (((ca_ofs + 7) & ~7) != ca_ofs ||
			ra_ofs + ca_ofs > NTFS_BLOCK_SIZE - sizeof(u16)) {
		ntfs_error(vi->i_sb, "$LogFile restart area specifies "
				"inconsistent client array offset.");
		return false;
	}
	ra_len = ca_ofs + le16_to_cpu(ra->log_clients) *
			sizeof(LOG_CLIENT_RECORD);
	if (ra_ofs + ra_len > le32_to_cpu(rp->system_page_size) ||
			ra_ofs + le16_to_cpu(ra->restart_area_length) >
			le32_to_cpu(rp->system_page_size) ||
			ra_len > le16_to_cpu(ra->restart_area_length)) {
		ntfs_error(vi->i_sb, "$LogFile restart area is out of bounds "
				"of the system page size specified by the "
				"restart page header and/or the specified "
				"restart area length is inconsistent.");
		return false;
	}
	if ((ra->client_free_list != LOGFILE_NO_CLIENT &&
			le16_to_cpu(ra->client_free_list) >=
			le16_to_cpu(ra->log_clients)) ||
			(ra->client_in_use_list != LOGFILE_NO_CLIENT &&
			le16_to_cpu(ra->client_in_use_list) >=
			le16_to_cpu(ra->log_clients))) {
		ntfs_error(vi->i_sb, "$LogFile restart area specifies "
				"overflowing client free and/or in use lists.");
		return false;
	}
	file_size = (u64)sle64_to_cpu(ra->file_size);
	fs_bits = 0;
	while (file_size) {
		file_size >>= 1;
		fs_bits++;
	}
	if (le32_to_cpu(ra->seq_number_bits) != 67 - fs_bits) {
		ntfs_error(vi->i_sb, "$LogFile restart area specifies "
				"inconsistent sequence number bits.");
		return false;
	}
	
	if (((le16_to_cpu(ra->log_record_header_length) + 7) & ~7) !=
			le16_to_cpu(ra->log_record_header_length)) {
		ntfs_error(vi->i_sb, "$LogFile restart area specifies "
				"inconsistent log record header length.");
		return false;
	}
	
	if (((le16_to_cpu(ra->log_page_data_offset) + 7) & ~7) !=
			le16_to_cpu(ra->log_page_data_offset)) {
		ntfs_error(vi->i_sb, "$LogFile restart area specifies "
				"inconsistent log page data offset.");
		return false;
	}
	ntfs_debug("Done.");
	return true;
}

static bool ntfs_check_log_client_array(struct inode *vi,
		RESTART_PAGE_HEADER *rp)
{
	RESTART_AREA *ra;
	LOG_CLIENT_RECORD *ca, *cr;
	u16 nr_clients, idx;
	bool in_free_list, idx_is_first;

	ntfs_debug("Entering.");
	ra = (RESTART_AREA*)((u8*)rp + le16_to_cpu(rp->restart_area_offset));
	ca = (LOG_CLIENT_RECORD*)((u8*)ra +
			le16_to_cpu(ra->client_array_offset));
	nr_clients = le16_to_cpu(ra->log_clients);
	idx = le16_to_cpu(ra->client_free_list);
	in_free_list = true;
check_list:
	for (idx_is_first = true; idx != LOGFILE_NO_CLIENT_CPU; nr_clients--,
			idx = le16_to_cpu(cr->next_client)) {
		if (!nr_clients || idx >= le16_to_cpu(ra->log_clients))
			goto err_out;
		
		cr = ca + idx;
		
		if (idx_is_first) {
			if (cr->prev_client != LOGFILE_NO_CLIENT)
				goto err_out;
			idx_is_first = false;
		}
	}
	
	if (in_free_list) {
		in_free_list = false;
		idx = le16_to_cpu(ra->client_in_use_list);
		goto check_list;
	}
	ntfs_debug("Done.");
	return true;
err_out:
	ntfs_error(vi->i_sb, "$LogFile log client array is corrupt.");
	return false;
}

static int ntfs_check_and_load_restart_page(struct inode *vi,
		RESTART_PAGE_HEADER *rp, s64 pos, RESTART_PAGE_HEADER **wrp,
		LSN *lsn)
{
	RESTART_AREA *ra;
	RESTART_PAGE_HEADER *trp;
	int size, err;

	ntfs_debug("Entering.");
	
	if (!ntfs_check_restart_page_header(vi, rp, pos)) {
		
		return -EINVAL;
	}
	
	if (!ntfs_check_restart_area(vi, rp)) {
		
		return -EINVAL;
	}
	ra = (RESTART_AREA*)((u8*)rp + le16_to_cpu(rp->restart_area_offset));
	trp = ntfs_malloc_nofs(le32_to_cpu(rp->system_page_size));
	if (!trp) {
		ntfs_error(vi->i_sb, "Failed to allocate memory for $LogFile "
				"restart page buffer.");
		return -ENOMEM;
	}
	size = PAGE_CACHE_SIZE - (pos & ~PAGE_CACHE_MASK);
	if (size >= le32_to_cpu(rp->system_page_size)) {
		memcpy(trp, rp, le32_to_cpu(rp->system_page_size));
	} else {
		pgoff_t idx;
		struct page *page;
		int have_read, to_read;

		
		memcpy(trp, rp, size);
		
		have_read = size;
		to_read = le32_to_cpu(rp->system_page_size) - size;
		idx = (pos + size) >> PAGE_CACHE_SHIFT;
		BUG_ON((pos + size) & ~PAGE_CACHE_MASK);
		do {
			page = ntfs_map_page(vi->i_mapping, idx);
			if (IS_ERR(page)) {
				ntfs_error(vi->i_sb, "Error mapping $LogFile "
						"page (index %lu).", idx);
				err = PTR_ERR(page);
				if (err != -EIO && err != -ENOMEM)
					err = -EIO;
				goto err_out;
			}
			size = min_t(int, to_read, PAGE_CACHE_SIZE);
			memcpy((u8*)trp + have_read, page_address(page), size);
			ntfs_unmap_page(page);
			have_read += size;
			to_read -= size;
			idx++;
		} while (to_read > 0);
	}
	if ((!ntfs_is_chkd_record(trp->magic) || le16_to_cpu(trp->usa_count))
			&& post_read_mst_fixup((NTFS_RECORD*)trp,
			le32_to_cpu(rp->system_page_size))) {
		if (le16_to_cpu(rp->restart_area_offset) +
				le16_to_cpu(ra->restart_area_length) >
				NTFS_BLOCK_SIZE - sizeof(u16)) {
			ntfs_error(vi->i_sb, "Multi sector transfer error "
					"detected in $LogFile restart page.");
			err = -EINVAL;
			goto err_out;
		}
	}
	err = 0;
	if (ntfs_is_rstr_record(rp->magic) &&
			ra->client_in_use_list != LOGFILE_NO_CLIENT) {
		if (!ntfs_check_log_client_array(vi, trp)) {
			err = -EINVAL;
			goto err_out;
		}
	}
	if (lsn) {
		if (ntfs_is_rstr_record(rp->magic))
			*lsn = sle64_to_cpu(ra->current_lsn);
		else 
			*lsn = sle64_to_cpu(rp->chkdsk_lsn);
	}
	ntfs_debug("Done.");
	if (wrp)
		*wrp = trp;
	else {
err_out:
		ntfs_free(trp);
	}
	return err;
}

bool ntfs_check_logfile(struct inode *log_vi, RESTART_PAGE_HEADER **rp)
{
	s64 size, pos;
	LSN rstr1_lsn, rstr2_lsn;
	ntfs_volume *vol = NTFS_SB(log_vi->i_sb);
	struct address_space *mapping = log_vi->i_mapping;
	struct page *page = NULL;
	u8 *kaddr = NULL;
	RESTART_PAGE_HEADER *rstr1_ph = NULL;
	RESTART_PAGE_HEADER *rstr2_ph = NULL;
	int log_page_size, log_page_mask, err;
	bool logfile_is_empty = true;
	u8 log_page_bits;

	ntfs_debug("Entering.");
	
	if (NVolLogFileEmpty(vol))
		goto is_empty;
	size = i_size_read(log_vi);
	
	if (size > MaxLogFileSize)
		size = MaxLogFileSize;
	if (PAGE_CACHE_SIZE >= DefaultLogPageSize && PAGE_CACHE_SIZE <=
			DefaultLogPageSize * 2)
		log_page_size = DefaultLogPageSize;
	else
		log_page_size = PAGE_CACHE_SIZE;
	log_page_mask = log_page_size - 1;
	log_page_bits = ntfs_ffs(log_page_size) - 1;
	size &= ~(s64)(log_page_size - 1);
	if (size < log_page_size * 2 || (size - log_page_size * 2) >>
			log_page_bits < MinLogRecordPages) {
		ntfs_error(vol->sb, "$LogFile is too small.");
		return false;
	}
	for (pos = 0; pos < size; pos <<= 1) {
		pgoff_t idx = pos >> PAGE_CACHE_SHIFT;
		if (!page || page->index != idx) {
			if (page)
				ntfs_unmap_page(page);
			page = ntfs_map_page(mapping, idx);
			if (IS_ERR(page)) {
				ntfs_error(vol->sb, "Error mapping $LogFile "
						"page (index %lu).", idx);
				goto err_out;
			}
		}
		kaddr = (u8*)page_address(page) + (pos & ~PAGE_CACHE_MASK);
		if (!ntfs_is_empty_recordp((le32*)kaddr))
			logfile_is_empty = false;
		else if (!logfile_is_empty)
			break;
		if (ntfs_is_rcrd_recordp((le32*)kaddr))
			break;
		
		if (!ntfs_is_rstr_recordp((le32*)kaddr) &&
				!ntfs_is_chkd_recordp((le32*)kaddr)) {
			if (!pos)
				pos = NTFS_BLOCK_SIZE >> 1;
			continue;
		}
		err = ntfs_check_and_load_restart_page(log_vi,
				(RESTART_PAGE_HEADER*)kaddr, pos,
				!rstr1_ph ? &rstr1_ph : &rstr2_ph,
				!rstr1_ph ? &rstr1_lsn : &rstr2_lsn);
		if (!err) {
			if (!pos) {
				pos = NTFS_BLOCK_SIZE >> 1;
				continue;
			}
			break;
		}
		if (err != -EINVAL) {
			ntfs_unmap_page(page);
			goto err_out;
		}
		
		if (!pos)
			pos = NTFS_BLOCK_SIZE >> 1;
	}
	if (page)
		ntfs_unmap_page(page);
	if (logfile_is_empty) {
		NVolSetLogFileEmpty(vol);
is_empty:
		ntfs_debug("Done.  ($LogFile is empty.)");
		return true;
	}
	if (!rstr1_ph) {
		BUG_ON(rstr2_ph);
		ntfs_error(vol->sb, "Did not find any restart pages in "
				"$LogFile and it was not empty.");
		return false;
	}
	
	if (rstr2_ph) {
		if (rstr2_lsn > rstr1_lsn) {
			ntfs_debug("Using second restart page as it is more "
					"recent.");
			ntfs_free(rstr1_ph);
			rstr1_ph = rstr2_ph;
			
		} else {
			ntfs_debug("Using first restart page as it is more "
					"recent.");
			ntfs_free(rstr2_ph);
		}
		rstr2_ph = NULL;
	}
	
	if (rp)
		*rp = rstr1_ph;
	else
		ntfs_free(rstr1_ph);
	ntfs_debug("Done.");
	return true;
err_out:
	if (rstr1_ph)
		ntfs_free(rstr1_ph);
	return false;
}

bool ntfs_is_logfile_clean(struct inode *log_vi, const RESTART_PAGE_HEADER *rp)
{
	ntfs_volume *vol = NTFS_SB(log_vi->i_sb);
	RESTART_AREA *ra;

	ntfs_debug("Entering.");
	
	if (NVolLogFileEmpty(vol)) {
		ntfs_debug("Done.  ($LogFile is empty.)");
		return true;
	}
	BUG_ON(!rp);
	if (!ntfs_is_rstr_record(rp->magic) &&
			!ntfs_is_chkd_record(rp->magic)) {
		ntfs_error(vol->sb, "Restart page buffer is invalid.  This is "
				"probably a bug in that the $LogFile should "
				"have been consistency checked before calling "
				"this function.");
		return false;
	}
	ra = (RESTART_AREA*)((u8*)rp + le16_to_cpu(rp->restart_area_offset));
	if (ra->client_in_use_list != LOGFILE_NO_CLIENT &&
			!(ra->flags & RESTART_VOLUME_IS_CLEAN)) {
		ntfs_debug("Done.  $LogFile indicates a dirty shutdown.");
		return false;
	}
	
	ntfs_debug("Done.  $LogFile indicates a clean shutdown.");
	return true;
}

bool ntfs_empty_logfile(struct inode *log_vi)
{
	VCN vcn, end_vcn;
	ntfs_inode *log_ni = NTFS_I(log_vi);
	ntfs_volume *vol = log_ni->vol;
	struct super_block *sb = vol->sb;
	runlist_element *rl;
	unsigned long flags;
	unsigned block_size, block_size_bits;
	int err;
	bool should_wait = true;

	ntfs_debug("Entering.");
	if (NVolLogFileEmpty(vol)) {
		ntfs_debug("Done.");
		return true;
	}
	block_size = sb->s_blocksize;
	block_size_bits = sb->s_blocksize_bits;
	vcn = 0;
	read_lock_irqsave(&log_ni->size_lock, flags);
	end_vcn = (log_ni->initialized_size + vol->cluster_size_mask) >>
			vol->cluster_size_bits;
	read_unlock_irqrestore(&log_ni->size_lock, flags);
	truncate_inode_pages(log_vi->i_mapping, 0);
	down_write(&log_ni->runlist.lock);
	rl = log_ni->runlist.rl;
	if (unlikely(!rl || vcn < rl->vcn || !rl->length)) {
map_vcn:
		err = ntfs_map_runlist_nolock(log_ni, vcn, NULL);
		if (err) {
			ntfs_error(sb, "Failed to map runlist fragment (error "
					"%d).", -err);
			goto err;
		}
		rl = log_ni->runlist.rl;
		BUG_ON(!rl || vcn < rl->vcn || !rl->length);
	}
	
	while (rl->length && vcn >= rl[1].vcn)
		rl++;
	do {
		LCN lcn;
		sector_t block, end_block;
		s64 len;

		lcn = rl->lcn;
		if (unlikely(lcn == LCN_RL_NOT_MAPPED)) {
			vcn = rl->vcn;
			goto map_vcn;
		}
		
		if (unlikely(!rl->length || lcn < LCN_HOLE))
			goto rl_err;
		
		if (lcn == LCN_HOLE)
			continue;
		block = lcn << vol->cluster_size_bits >> block_size_bits;
		len = rl->length;
		if (rl[1].vcn > end_vcn)
			len = end_vcn - rl->vcn;
		end_block = (lcn + len) << vol->cluster_size_bits >>
				block_size_bits;
		
		do {
			struct buffer_head *bh;

			
			bh = sb_getblk(sb, block);
			BUG_ON(!bh);
			
			lock_buffer(bh);
			bh->b_end_io = end_buffer_write_sync;
			get_bh(bh);
			
			memset(bh->b_data, -1, block_size);
			if (!buffer_uptodate(bh))
				set_buffer_uptodate(bh);
			if (buffer_dirty(bh))
				clear_buffer_dirty(bh);
			submit_bh(WRITE, bh);
			if (should_wait) {
				should_wait = false;
				wait_on_buffer(bh);
				if (unlikely(!buffer_uptodate(bh)))
					goto io_err;
			}
			brelse(bh);
		} while (++block < end_block);
	} while ((++rl)->vcn < end_vcn);
	up_write(&log_ni->runlist.lock);
	truncate_inode_pages(log_vi->i_mapping, 0);
	
	NVolSetLogFileEmpty(vol);
	ntfs_debug("Done.");
	return true;
io_err:
	ntfs_error(sb, "Failed to write buffer.  Unmount and run chkdsk.");
	goto dirty_err;
rl_err:
	ntfs_error(sb, "Runlist is corrupt.  Unmount and run chkdsk.");
dirty_err:
	NVolSetErrors(vol);
	err = -EIO;
err:
	up_write(&log_ni->runlist.lock);
	ntfs_error(sb, "Failed to fill $LogFile with 0xff bytes (error %d).",
			-err);
	return false;
}

#endif 
