/**
 * mft.c - NTFS kernel mft record operations. Part of the Linux-NTFS project.
 *
 * Copyright (c) 2001-2012 Anton Altaparmakov and Tuxera Inc.
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

#include <linux/buffer_head.h>
#include <linux/slab.h>
#include <linux/swap.h>

#include "attrib.h"
#include "aops.h"
#include "bitmap.h"
#include "debug.h"
#include "dir.h"
#include "lcnalloc.h"
#include "malloc.h"
#include "mft.h"
#include "ntfs.h"

static inline MFT_RECORD *map_mft_record_page(ntfs_inode *ni)
{
	loff_t i_size;
	ntfs_volume *vol = ni->vol;
	struct inode *mft_vi = vol->mft_ino;
	struct page *page;
	unsigned long index, end_index;
	unsigned ofs;

	BUG_ON(ni->page);
	index = (u64)ni->mft_no << vol->mft_record_size_bits >>
			PAGE_CACHE_SHIFT;
	ofs = (ni->mft_no << vol->mft_record_size_bits) & ~PAGE_CACHE_MASK;

	i_size = i_size_read(mft_vi);
	
	end_index = i_size >> PAGE_CACHE_SHIFT;

	
	if (unlikely(index >= end_index)) {
		if (index > end_index || (i_size & ~PAGE_CACHE_MASK) < ofs +
				vol->mft_record_size) {
			page = ERR_PTR(-ENOENT);
			ntfs_error(vol->sb, "Attempt to read mft record 0x%lx, "
					"which is beyond the end of the mft.  "
					"This is probably a bug in the ntfs "
					"driver.", ni->mft_no);
			goto err_out;
		}
	}
	
	page = ntfs_map_page(mft_vi->i_mapping, index);
	if (likely(!IS_ERR(page))) {
		
		if (likely(ntfs_is_mft_recordp((le32*)(page_address(page) +
				ofs)))) {
			ni->page = page;
			ni->page_ofs = ofs;
			return page_address(page) + ofs;
		}
		ntfs_error(vol->sb, "Mft record 0x%lx is corrupt.  "
				"Run chkdsk.", ni->mft_no);
		ntfs_unmap_page(page);
		page = ERR_PTR(-EIO);
		NVolSetErrors(vol);
	}
err_out:
	ni->page = NULL;
	ni->page_ofs = 0;
	return (void*)page;
}

MFT_RECORD *map_mft_record(ntfs_inode *ni)
{
	MFT_RECORD *m;

	ntfs_debug("Entering for mft_no 0x%lx.", ni->mft_no);

	
	atomic_inc(&ni->count);

	
	mutex_lock(&ni->mrec_lock);

	m = map_mft_record_page(ni);
	if (likely(!IS_ERR(m)))
		return m;

	mutex_unlock(&ni->mrec_lock);
	atomic_dec(&ni->count);
	ntfs_error(ni->vol->sb, "Failed with error code %lu.", -PTR_ERR(m));
	return m;
}

static inline void unmap_mft_record_page(ntfs_inode *ni)
{
	BUG_ON(!ni->page);

	
	ntfs_unmap_page(ni->page);
	ni->page = NULL;
	ni->page_ofs = 0;
	return;
}

void unmap_mft_record(ntfs_inode *ni)
{
	struct page *page = ni->page;

	BUG_ON(!page);

	ntfs_debug("Entering for mft_no 0x%lx.", ni->mft_no);

	unmap_mft_record_page(ni);
	mutex_unlock(&ni->mrec_lock);
	atomic_dec(&ni->count);
	return;
}

MFT_RECORD *map_extent_mft_record(ntfs_inode *base_ni, MFT_REF mref,
		ntfs_inode **ntfs_ino)
{
	MFT_RECORD *m;
	ntfs_inode *ni = NULL;
	ntfs_inode **extent_nis = NULL;
	int i;
	unsigned long mft_no = MREF(mref);
	u16 seq_no = MSEQNO(mref);
	bool destroy_ni = false;

	ntfs_debug("Mapping extent mft record 0x%lx (base mft record 0x%lx).",
			mft_no, base_ni->mft_no);
	
	atomic_inc(&base_ni->count);
	mutex_lock(&base_ni->extent_lock);
	if (base_ni->nr_extents > 0) {
		extent_nis = base_ni->ext.extent_ntfs_inos;
		for (i = 0; i < base_ni->nr_extents; i++) {
			if (mft_no != extent_nis[i]->mft_no)
				continue;
			ni = extent_nis[i];
			
			atomic_inc(&ni->count);
			break;
		}
	}
	if (likely(ni != NULL)) {
		mutex_unlock(&base_ni->extent_lock);
		atomic_dec(&base_ni->count);
		
		m = map_mft_record(ni);
		
		atomic_dec(&ni->count);
		if (likely(!IS_ERR(m))) {
			
			if (likely(le16_to_cpu(m->sequence_number) == seq_no)) {
				ntfs_debug("Done 1.");
				*ntfs_ino = ni;
				return m;
			}
			unmap_mft_record(ni);
			ntfs_error(base_ni->vol->sb, "Found stale extent mft "
					"reference! Corrupt filesystem. "
					"Run chkdsk.");
			return ERR_PTR(-EIO);
		}
map_err_out:
		ntfs_error(base_ni->vol->sb, "Failed to map extent "
				"mft record, error code %ld.", -PTR_ERR(m));
		return m;
	}
	
	ni = ntfs_new_extent_inode(base_ni->vol->sb, mft_no);
	if (unlikely(!ni)) {
		mutex_unlock(&base_ni->extent_lock);
		atomic_dec(&base_ni->count);
		return ERR_PTR(-ENOMEM);
	}
	ni->vol = base_ni->vol;
	ni->seq_no = seq_no;
	ni->nr_extents = -1;
	ni->ext.base_ntfs_ino = base_ni;
	
	m = map_mft_record(ni);
	if (IS_ERR(m)) {
		mutex_unlock(&base_ni->extent_lock);
		atomic_dec(&base_ni->count);
		ntfs_clear_extent_inode(ni);
		goto map_err_out;
	}
	
	if (seq_no && (le16_to_cpu(m->sequence_number) != seq_no)) {
		ntfs_error(base_ni->vol->sb, "Found stale extent mft "
				"reference! Corrupt filesystem. Run chkdsk.");
		destroy_ni = true;
		m = ERR_PTR(-EIO);
		goto unm_err_out;
	}
	
	if (!(base_ni->nr_extents & 3)) {
		ntfs_inode **tmp;
		int new_size = (base_ni->nr_extents + 4) * sizeof(ntfs_inode *);

		tmp = kmalloc(new_size, GFP_NOFS);
		if (unlikely(!tmp)) {
			ntfs_error(base_ni->vol->sb, "Failed to allocate "
					"internal buffer.");
			destroy_ni = true;
			m = ERR_PTR(-ENOMEM);
			goto unm_err_out;
		}
		if (base_ni->nr_extents) {
			BUG_ON(!base_ni->ext.extent_ntfs_inos);
			memcpy(tmp, base_ni->ext.extent_ntfs_inos, new_size -
					4 * sizeof(ntfs_inode *));
			kfree(base_ni->ext.extent_ntfs_inos);
		}
		base_ni->ext.extent_ntfs_inos = tmp;
	}
	base_ni->ext.extent_ntfs_inos[base_ni->nr_extents++] = ni;
	mutex_unlock(&base_ni->extent_lock);
	atomic_dec(&base_ni->count);
	ntfs_debug("Done 2.");
	*ntfs_ino = ni;
	return m;
unm_err_out:
	unmap_mft_record(ni);
	mutex_unlock(&base_ni->extent_lock);
	atomic_dec(&base_ni->count);
	if (destroy_ni)
		ntfs_clear_extent_inode(ni);
	return m;
}

#ifdef NTFS_RW

/**
 * __mark_mft_record_dirty - set the mft record and the page containing it dirty
 * @ni:		ntfs inode describing the mapped mft record
 *
 * Internal function.  Users should call mark_mft_record_dirty() instead.
 *
 * Set the mapped (extent) mft record of the (base or extent) ntfs inode @ni,
 * as well as the page containing the mft record, dirty.  Also, mark the base
 * vfs inode dirty.  This ensures that any changes to the mft record are
 * written out to disk.
 *
 * NOTE:  We only set I_DIRTY_SYNC and I_DIRTY_DATASYNC (and not I_DIRTY_PAGES)
 * on the base vfs inode, because even though file data may have been modified,
 * it is dirty in the inode meta data rather than the data page cache of the
 * inode, and thus there are no data pages that need writing out.  Therefore, a
 * full mark_inode_dirty() is overkill.  A mark_inode_dirty_sync(), on the
 * other hand, is not sufficient, because ->write_inode needs to be called even
 * in case of fdatasync. This needs to happen or the file data would not
 * necessarily hit the device synchronously, even though the vfs inode has the
 * O_SYNC flag set.  Also, I_DIRTY_DATASYNC simply "feels" better than just
 * I_DIRTY_SYNC, since the file data has not actually hit the block device yet,
 * which is not what I_DIRTY_SYNC on its own would suggest.
 */
void __mark_mft_record_dirty(ntfs_inode *ni)
{
	ntfs_inode *base_ni;

	ntfs_debug("Entering for inode 0x%lx.", ni->mft_no);
	BUG_ON(NInoAttr(ni));
	mark_ntfs_record_dirty(ni->page, ni->page_ofs);
	
	mutex_lock(&ni->extent_lock);
	if (likely(ni->nr_extents >= 0))
		base_ni = ni;
	else
		base_ni = ni->ext.base_ntfs_ino;
	mutex_unlock(&ni->extent_lock);
	__mark_inode_dirty(VFS_I(base_ni), I_DIRTY_SYNC | I_DIRTY_DATASYNC);
}

static const char *ntfs_please_email = "Please email "
		"linux-ntfs-dev@lists.sourceforge.net and say that you saw "
		"this message.  Thank you.";

static int ntfs_sync_mft_mirror_umount(ntfs_volume *vol,
		const unsigned long mft_no, MFT_RECORD *m)
{
	BUG_ON(vol->mftmirr_ino);
	ntfs_error(vol->sb, "Umount time mft mirror syncing is not "
			"implemented yet.  %s", ntfs_please_email);
	return -EOPNOTSUPP;
}

int ntfs_sync_mft_mirror(ntfs_volume *vol, const unsigned long mft_no,
		MFT_RECORD *m, int sync)
{
	struct page *page;
	unsigned int blocksize = vol->sb->s_blocksize;
	int max_bhs = vol->mft_record_size / blocksize;
	struct buffer_head *bhs[max_bhs];
	struct buffer_head *bh, *head;
	u8 *kmirr;
	runlist_element *rl;
	unsigned int block_start, block_end, m_start, m_end, page_ofs;
	int i_bhs, nr_bhs, err = 0;
	unsigned char blocksize_bits = vol->sb->s_blocksize_bits;

	ntfs_debug("Entering for inode 0x%lx.", mft_no);
	BUG_ON(!max_bhs);
	if (unlikely(!vol->mftmirr_ino)) {
		
		err = ntfs_sync_mft_mirror_umount(vol, mft_no, m);
		if (likely(!err))
			return err;
		goto err_out;
	}
	
	page = ntfs_map_page(vol->mftmirr_ino->i_mapping, mft_no >>
			(PAGE_CACHE_SHIFT - vol->mft_record_size_bits));
	if (IS_ERR(page)) {
		ntfs_error(vol->sb, "Failed to map mft mirror page.");
		err = PTR_ERR(page);
		goto err_out;
	}
	lock_page(page);
	BUG_ON(!PageUptodate(page));
	ClearPageUptodate(page);
	
	page_ofs = (mft_no << vol->mft_record_size_bits) & ~PAGE_CACHE_MASK;
	
	kmirr = page_address(page) + page_ofs;
	
	memcpy(kmirr, m, vol->mft_record_size);
	
	if (unlikely(!page_has_buffers(page))) {
		struct buffer_head *tail;

		bh = head = alloc_page_buffers(page, blocksize, 1);
		do {
			set_buffer_uptodate(bh);
			tail = bh;
			bh = bh->b_this_page;
		} while (bh);
		tail->b_this_page = head;
		attach_page_buffers(page, head);
	}
	bh = head = page_buffers(page);
	BUG_ON(!bh);
	rl = NULL;
	nr_bhs = 0;
	block_start = 0;
	m_start = kmirr - (u8*)page_address(page);
	m_end = m_start + vol->mft_record_size;
	do {
		block_end = block_start + blocksize;
		
		if (block_end <= m_start)
			continue;
		if (unlikely(block_start >= m_end))
			break;
		
		if (unlikely(!buffer_mapped(bh))) {
			VCN vcn;
			LCN lcn;
			unsigned int vcn_ofs;

			bh->b_bdev = vol->sb->s_bdev;
			
			vcn = ((VCN)mft_no << vol->mft_record_size_bits) +
					(block_start - m_start);
			vcn_ofs = vcn & vol->cluster_size_mask;
			vcn >>= vol->cluster_size_bits;
			if (!rl) {
				down_read(&NTFS_I(vol->mftmirr_ino)->
						runlist.lock);
				rl = NTFS_I(vol->mftmirr_ino)->runlist.rl;
				BUG_ON(!rl);
			}
			
			while (rl->length && rl[1].vcn <= vcn)
				rl++;
			lcn = ntfs_rl_vcn_to_lcn(rl, vcn);
			
			if (likely(lcn >= 0)) {
				
				bh->b_blocknr = ((lcn <<
						vol->cluster_size_bits) +
						vcn_ofs) >> blocksize_bits;
				set_buffer_mapped(bh);
			} else {
				bh->b_blocknr = -1;
				ntfs_error(vol->sb, "Cannot write mft mirror "
						"record 0x%lx because its "
						"location on disk could not "
						"be determined (error code "
						"%lli).", mft_no,
						(long long)lcn);
				err = -EIO;
			}
		}
		BUG_ON(!buffer_uptodate(bh));
		BUG_ON(!nr_bhs && (m_start != block_start));
		BUG_ON(nr_bhs >= max_bhs);
		bhs[nr_bhs++] = bh;
		BUG_ON((nr_bhs >= max_bhs) && (m_end != block_end));
	} while (block_start = block_end, (bh = bh->b_this_page) != head);
	if (unlikely(rl))
		up_read(&NTFS_I(vol->mftmirr_ino)->runlist.lock);
	if (likely(!err)) {
		
		for (i_bhs = 0; i_bhs < nr_bhs; i_bhs++) {
			struct buffer_head *tbh = bhs[i_bhs];

			if (!trylock_buffer(tbh))
				BUG();
			BUG_ON(!buffer_uptodate(tbh));
			clear_buffer_dirty(tbh);
			get_bh(tbh);
			tbh->b_end_io = end_buffer_write_sync;
			submit_bh(WRITE, tbh);
		}
		
		for (i_bhs = 0; i_bhs < nr_bhs; i_bhs++) {
			struct buffer_head *tbh = bhs[i_bhs];

			wait_on_buffer(tbh);
			if (unlikely(!buffer_uptodate(tbh))) {
				err = -EIO;
				set_buffer_uptodate(tbh);
			}
		}
	} else  {
		
		for (i_bhs = 0; i_bhs < nr_bhs; i_bhs++)
			clear_buffer_dirty(bhs[i_bhs]);
	}
	
	
	post_write_mst_fixup((NTFS_RECORD*)kmirr);
	flush_dcache_page(page);
	SetPageUptodate(page);
	unlock_page(page);
	ntfs_unmap_page(page);
	if (likely(!err)) {
		ntfs_debug("Done.");
	} else {
		ntfs_error(vol->sb, "I/O error while writing mft mirror "
				"record 0x%lx!", mft_no);
err_out:
		ntfs_error(vol->sb, "Failed to synchronize $MFTMirr (error "
				"code %i).  Volume will be left marked dirty "
				"on umount.  Run ntfsfix on the partition "
				"after umounting to correct this.", -err);
		NVolSetErrors(vol);
	}
	return err;
}

int write_mft_record_nolock(ntfs_inode *ni, MFT_RECORD *m, int sync)
{
	ntfs_volume *vol = ni->vol;
	struct page *page = ni->page;
	unsigned int blocksize = vol->sb->s_blocksize;
	unsigned char blocksize_bits = vol->sb->s_blocksize_bits;
	int max_bhs = vol->mft_record_size / blocksize;
	struct buffer_head *bhs[max_bhs];
	struct buffer_head *bh, *head;
	runlist_element *rl;
	unsigned int block_start, block_end, m_start, m_end;
	int i_bhs, nr_bhs, err = 0;

	ntfs_debug("Entering for inode 0x%lx.", ni->mft_no);
	BUG_ON(NInoAttr(ni));
	BUG_ON(!max_bhs);
	BUG_ON(!PageLocked(page));
	if (!NInoTestClearDirty(ni))
		goto done;
	bh = head = page_buffers(page);
	BUG_ON(!bh);
	rl = NULL;
	nr_bhs = 0;
	block_start = 0;
	m_start = ni->page_ofs;
	m_end = m_start + vol->mft_record_size;
	do {
		block_end = block_start + blocksize;
		
		if (block_end <= m_start)
			continue;
		if (unlikely(block_start >= m_end))
			break;
		if (block_start == m_start) {
			
			if (!buffer_dirty(bh)) {
				BUG_ON(nr_bhs);
				/* Clean records are not written out. */
				break;
			}
		}
		
		if (unlikely(!buffer_mapped(bh))) {
			VCN vcn;
			LCN lcn;
			unsigned int vcn_ofs;

			bh->b_bdev = vol->sb->s_bdev;
			
			vcn = ((VCN)ni->mft_no << vol->mft_record_size_bits) +
					(block_start - m_start);
			vcn_ofs = vcn & vol->cluster_size_mask;
			vcn >>= vol->cluster_size_bits;
			if (!rl) {
				down_read(&NTFS_I(vol->mft_ino)->runlist.lock);
				rl = NTFS_I(vol->mft_ino)->runlist.rl;
				BUG_ON(!rl);
			}
			
			while (rl->length && rl[1].vcn <= vcn)
				rl++;
			lcn = ntfs_rl_vcn_to_lcn(rl, vcn);
			
			if (likely(lcn >= 0)) {
				
				bh->b_blocknr = ((lcn <<
						vol->cluster_size_bits) +
						vcn_ofs) >> blocksize_bits;
				set_buffer_mapped(bh);
			} else {
				bh->b_blocknr = -1;
				ntfs_error(vol->sb, "Cannot write mft record "
						"0x%lx because its location "
						"on disk could not be "
						"determined (error code %lli).",
						ni->mft_no, (long long)lcn);
				err = -EIO;
			}
		}
		BUG_ON(!buffer_uptodate(bh));
		BUG_ON(!nr_bhs && (m_start != block_start));
		BUG_ON(nr_bhs >= max_bhs);
		bhs[nr_bhs++] = bh;
		BUG_ON((nr_bhs >= max_bhs) && (m_end != block_end));
	} while (block_start = block_end, (bh = bh->b_this_page) != head);
	if (unlikely(rl))
		up_read(&NTFS_I(vol->mft_ino)->runlist.lock);
	if (!nr_bhs)
		goto done;
	if (unlikely(err))
		goto cleanup_out;
	
	err = pre_write_mst_fixup((NTFS_RECORD*)m, vol->mft_record_size);
	if (err) {
		ntfs_error(vol->sb, "Failed to apply mst fixups!");
		goto cleanup_out;
	}
	flush_dcache_mft_record_page(ni);
	
	for (i_bhs = 0; i_bhs < nr_bhs; i_bhs++) {
		struct buffer_head *tbh = bhs[i_bhs];

		if (!trylock_buffer(tbh))
			BUG();
		BUG_ON(!buffer_uptodate(tbh));
		clear_buffer_dirty(tbh);
		get_bh(tbh);
		tbh->b_end_io = end_buffer_write_sync;
		submit_bh(WRITE, tbh);
	}
	
	if (!sync && ni->mft_no < vol->mftmirr_size)
		ntfs_sync_mft_mirror(vol, ni->mft_no, m, sync);
	
	for (i_bhs = 0; i_bhs < nr_bhs; i_bhs++) {
		struct buffer_head *tbh = bhs[i_bhs];

		wait_on_buffer(tbh);
		if (unlikely(!buffer_uptodate(tbh))) {
			err = -EIO;
			if (PageUptodate(page))
				set_buffer_uptodate(tbh);
		}
	}
	
	if (sync && ni->mft_no < vol->mftmirr_size)
		ntfs_sync_mft_mirror(vol, ni->mft_no, m, sync);
	
	post_write_mst_fixup((NTFS_RECORD*)m);
	flush_dcache_mft_record_page(ni);
	if (unlikely(err)) {
		
		ntfs_error(vol->sb, "I/O error while writing mft record "
				"0x%lx!  Marking base inode as bad.  You "
				"should unmount the volume and run chkdsk.",
				ni->mft_no);
		goto err_out;
	}
done:
	ntfs_debug("Done.");
	return 0;
cleanup_out:
	
	for (i_bhs = 0; i_bhs < nr_bhs; i_bhs++)
		clear_buffer_dirty(bhs[i_bhs]);
err_out:
	if (err == -ENOMEM) {
		ntfs_error(vol->sb, "Not enough memory to write mft record.  "
				"Redirtying so the write is retried later.");
		mark_mft_record_dirty(ni);
		err = 0;
	} else
		NVolSetErrors(vol);
	return err;
}

/**
 * ntfs_may_write_mft_record - check if an mft record may be written out
 * @vol:	[IN]  ntfs volume on which the mft record to check resides
 * @mft_no:	[IN]  mft record number of the mft record to check
 * @m:		[IN]  mapped mft record to check
 * @locked_ni:	[OUT] caller has to unlock this ntfs inode if one is returned
 *
 * Check if the mapped (base or extent) mft record @m with mft record number
 * @mft_no belonging to the ntfs volume @vol may be written out.  If necessary
 * and possible the ntfs inode of the mft record is locked and the base vfs
 * inode is pinned.  The locked ntfs inode is then returned in @locked_ni.  The
 * caller is responsible for unlocking the ntfs inode and unpinning the base
 * vfs inode.
 *
 * Return 'true' if the mft record may be written out and 'false' if not.
 *
 * The caller has locked the page and cleared the uptodate flag on it which
 * means that we can safely write out any dirty mft records that do not have
 * their inodes in icache as determined by ilookup5() as anyone
 * opening/creating such an inode would block when attempting to map the mft
 * record in read_cache_page() until we are finished with the write out.
 *
 * Here is a description of the tests we perform:
 *
 * If the inode is found in icache we know the mft record must be a base mft
 * record.  If it is dirty, we do not write it and return 'false' as the vfs
 * inode write paths will result in the access times being updated which would
 * cause the base mft record to be redirtied and written out again.  (We know
 * the access time update will modify the base mft record because Windows
 * chkdsk complains if the standard information attribute is not in the base
 * mft record.)
 *
 * If the inode is in icache and not dirty, we attempt to lock the mft record
 * and if we find the lock was already taken, it is not safe to write the mft
 * record and we return 'false'.
 *
 * If we manage to obtain the lock we have exclusive access to the mft record,
 * which also allows us safe writeout of the mft record.  We then set
 * @locked_ni to the locked ntfs inode and return 'true'.
 *
 * Note we cannot just lock the mft record and sleep while waiting for the lock
 * because this would deadlock due to lock reversal (normally the mft record is
 * locked before the page is locked but we already have the page locked here
 * when we try to lock the mft record).
 *
 * If the inode is not in icache we need to perform further checks.
 *
 * If the mft record is not a FILE record or it is a base mft record, we can
 * safely write it and return 'true'.
 *
 * We now know the mft record is an extent mft record.  We check if the inode
 * corresponding to its base mft record is in icache and obtain a reference to
 * it if it is.  If it is not, we can safely write it and return 'true'.
 *
 * We now have the base inode for the extent mft record.  We check if it has an
 * ntfs inode for the extent mft record attached and if not it is safe to write
 * the extent mft record and we return 'true'.
 *
 * The ntfs inode for the extent mft record is attached to the base inode so we
 * attempt to lock the extent mft record and if we find the lock was already
 * taken, it is not safe to write the extent mft record and we return 'false'.
 *
 * If we manage to obtain the lock we have exclusive access to the extent mft
 * record, which also allows us safe writeout of the extent mft record.  We
 * set the ntfs inode of the extent mft record clean and then set @locked_ni to
 * the now locked ntfs inode and return 'true'.
 *
 * Note, the reason for actually writing dirty mft records here and not just
 * relying on the vfs inode dirty code paths is that we can have mft records
 * modified without them ever having actual inodes in memory.  Also we can have
 * dirty mft records with clean ntfs inodes in memory.  None of the described
 * cases would result in the dirty mft records being written out if we only
 * relied on the vfs inode dirty code paths.  And these cases can really occur
 * during allocation of new mft records and in particular when the
 * initialized_size of the $MFT/$DATA attribute is extended and the new space
 * is initialized using ntfs_mft_record_format().  The clean inode can then
 * appear if the mft record is reused for a new inode before it got written
 * out.
 */
bool ntfs_may_write_mft_record(ntfs_volume *vol, const unsigned long mft_no,
		const MFT_RECORD *m, ntfs_inode **locked_ni)
{
	struct super_block *sb = vol->sb;
	struct inode *mft_vi = vol->mft_ino;
	struct inode *vi;
	ntfs_inode *ni, *eni, **extent_nis;
	int i;
	ntfs_attr na;

	ntfs_debug("Entering for inode 0x%lx.", mft_no);
	BUG_ON(!locked_ni);
	*locked_ni = NULL;
	ntfs_debug("Looking for inode 0x%lx in icache.", mft_no);
	na.mft_no = mft_no;
	na.name = NULL;
	na.name_len = 0;
	na.type = AT_UNUSED;
	if (!mft_no) {
		
		vi = igrab(mft_vi);
		BUG_ON(vi != mft_vi);
	} else {
		vi = ilookup5_nowait(sb, mft_no, (test_t)ntfs_test_inode, &na);
	}
	if (vi) {
		ntfs_debug("Base inode 0x%lx is in icache.", mft_no);
		
		ni = NTFS_I(vi);
		
		atomic_inc(&ni->count);
		
		if (NInoDirty(ni)) {
			ntfs_debug("Inode 0x%lx is dirty, do not write it.",
					mft_no);
			atomic_dec(&ni->count);
			iput(vi);
			return false;
		}
		ntfs_debug("Inode 0x%lx is not dirty.", mft_no);
		
		if (unlikely(!mutex_trylock(&ni->mrec_lock))) {
			ntfs_debug("Mft record 0x%lx is already locked, do "
					"not write it.", mft_no);
			atomic_dec(&ni->count);
			iput(vi);
			return false;
		}
		ntfs_debug("Managed to lock mft record 0x%lx, write it.",
				mft_no);
		*locked_ni = ni;
		return true;
	}
	ntfs_debug("Inode 0x%lx is not in icache.", mft_no);
	
	
	if (!ntfs_is_mft_record(m->magic)) {
		ntfs_debug("Mft record 0x%lx is not a FILE record, write it.",
				mft_no);
		return true;
	}
	
	if (!m->base_mft_record) {
		ntfs_debug("Mft record 0x%lx is a base record, write it.",
				mft_no);
		return true;
	}
	na.mft_no = MREF_LE(m->base_mft_record);
	ntfs_debug("Mft record 0x%lx is an extent record.  Looking for base "
			"inode 0x%lx in icache.", mft_no, na.mft_no);
	if (!na.mft_no) {
		
		vi = igrab(mft_vi);
		BUG_ON(vi != mft_vi);
	} else
		vi = ilookup5_nowait(sb, na.mft_no, (test_t)ntfs_test_inode,
				&na);
	if (!vi) {
		ntfs_debug("Base inode 0x%lx is not in icache, write the "
				"extent record.", na.mft_no);
		return true;
	}
	ntfs_debug("Base inode 0x%lx is in icache.", na.mft_no);
	ni = NTFS_I(vi);
	mutex_lock(&ni->extent_lock);
	if (ni->nr_extents <= 0) {
		mutex_unlock(&ni->extent_lock);
		iput(vi);
		ntfs_debug("Base inode 0x%lx has no attached extent inodes, "
				"write the extent record.", na.mft_no);
		return true;
	}
	
	extent_nis = ni->ext.extent_ntfs_inos;
	for (eni = NULL, i = 0; i < ni->nr_extents; ++i) {
		if (mft_no == extent_nis[i]->mft_no) {
			eni = extent_nis[i];
			break;
		}
	}
	if (!eni) {
		mutex_unlock(&ni->extent_lock);
		iput(vi);
		ntfs_debug("Extent inode 0x%lx is not attached to its base "
				"inode 0x%lx, write the extent record.",
				mft_no, na.mft_no);
		return true;
	}
	ntfs_debug("Extent inode 0x%lx is attached to its base inode 0x%lx.",
			mft_no, na.mft_no);
	
	atomic_inc(&eni->count);
	mutex_unlock(&ni->extent_lock);
	if (unlikely(!mutex_trylock(&eni->mrec_lock))) {
		atomic_dec(&eni->count);
		iput(vi);
		ntfs_debug("Extent mft record 0x%lx is already locked, do "
				"not write it.", mft_no);
		return false;
	}
	ntfs_debug("Managed to lock extent mft record 0x%lx, write it.",
			mft_no);
	if (NInoTestClearDirty(eni))
		ntfs_debug("Extent inode 0x%lx is dirty, marking it clean.",
				mft_no);
	*locked_ni = eni;
	return true;
}

static const char *es = "  Leaving inconsistent metadata.  Unmount and run "
		"chkdsk.";

static int ntfs_mft_bitmap_find_and_alloc_free_rec_nolock(ntfs_volume *vol,
		ntfs_inode *base_ni)
{
	s64 pass_end, ll, data_pos, pass_start, ofs, bit;
	unsigned long flags;
	struct address_space *mftbmp_mapping;
	u8 *buf, *byte;
	struct page *page;
	unsigned int page_ofs, size;
	u8 pass, b;

	ntfs_debug("Searching for free mft record in the currently "
			"initialized mft bitmap.");
	mftbmp_mapping = vol->mftbmp_ino->i_mapping;
	read_lock_irqsave(&NTFS_I(vol->mft_ino)->size_lock, flags);
	pass_end = NTFS_I(vol->mft_ino)->allocated_size >>
			vol->mft_record_size_bits;
	read_unlock_irqrestore(&NTFS_I(vol->mft_ino)->size_lock, flags);
	read_lock_irqsave(&NTFS_I(vol->mftbmp_ino)->size_lock, flags);
	ll = NTFS_I(vol->mftbmp_ino)->initialized_size << 3;
	read_unlock_irqrestore(&NTFS_I(vol->mftbmp_ino)->size_lock, flags);
	if (pass_end > ll)
		pass_end = ll;
	pass = 1;
	if (!base_ni)
		data_pos = vol->mft_data_pos;
	else
		data_pos = base_ni->mft_no + 1;
	if (data_pos < 24)
		data_pos = 24;
	if (data_pos >= pass_end) {
		data_pos = 24;
		pass = 2;
		
		if (data_pos >= pass_end)
			return -ENOSPC;
	}
	pass_start = data_pos;
	ntfs_debug("Starting bitmap search: pass %u, pass_start 0x%llx, "
			"pass_end 0x%llx, data_pos 0x%llx.", pass,
			(long long)pass_start, (long long)pass_end,
			(long long)data_pos);
	
	for (; pass <= 2;) {
		
		ofs = data_pos >> 3;
		page_ofs = ofs & ~PAGE_CACHE_MASK;
		size = PAGE_CACHE_SIZE - page_ofs;
		ll = ((pass_end + 7) >> 3) - ofs;
		if (size > ll)
			size = ll;
		size <<= 3;
		if (size) {
			page = ntfs_map_page(mftbmp_mapping,
					ofs >> PAGE_CACHE_SHIFT);
			if (IS_ERR(page)) {
				ntfs_error(vol->sb, "Failed to read mft "
						"bitmap, aborting.");
				return PTR_ERR(page);
			}
			buf = (u8*)page_address(page) + page_ofs;
			bit = data_pos & 7;
			data_pos &= ~7ull;
			ntfs_debug("Before inner for loop: size 0x%x, "
					"data_pos 0x%llx, bit 0x%llx", size,
					(long long)data_pos, (long long)bit);
			for (; bit < size && data_pos + bit < pass_end;
					bit &= ~7ull, bit += 8) {
				byte = buf + (bit >> 3);
				if (*byte == 0xff)
					continue;
				b = ffz((unsigned long)*byte);
				if (b < 8 && b >= (bit & 7)) {
					ll = data_pos + (bit & ~7ull) + b;
					if (unlikely(ll > (1ll << 32))) {
						ntfs_unmap_page(page);
						return -ENOSPC;
					}
					*byte |= 1 << b;
					flush_dcache_page(page);
					set_page_dirty(page);
					ntfs_unmap_page(page);
					ntfs_debug("Done.  (Found and "
							"allocated mft record "
							"0x%llx.)",
							(long long)ll);
					return ll;
				}
			}
			ntfs_debug("After inner for loop: size 0x%x, "
					"data_pos 0x%llx, bit 0x%llx", size,
					(long long)data_pos, (long long)bit);
			data_pos += size;
			ntfs_unmap_page(page);
			if (data_pos < pass_end)
				continue;
		}
		
		if (++pass == 2) {
			pass_end = pass_start;
			data_pos = pass_start = 24;
			ntfs_debug("pass %i, pass_start 0x%llx, pass_end "
					"0x%llx.", pass, (long long)pass_start,
					(long long)pass_end);
			if (data_pos >= pass_end)
				break;
		}
	}
	
	ntfs_debug("Done.  (No free mft records left in currently initialized "
			"mft bitmap.)");
	return -ENOSPC;
}

static int ntfs_mft_bitmap_extend_allocation_nolock(ntfs_volume *vol)
{
	LCN lcn;
	s64 ll;
	unsigned long flags;
	struct page *page;
	ntfs_inode *mft_ni, *mftbmp_ni;
	runlist_element *rl, *rl2 = NULL;
	ntfs_attr_search_ctx *ctx = NULL;
	MFT_RECORD *mrec;
	ATTR_RECORD *a = NULL;
	int ret, mp_size;
	u32 old_alen = 0;
	u8 *b, tb;
	struct {
		u8 added_cluster:1;
		u8 added_run:1;
		u8 mp_rebuilt:1;
	} status = { 0, 0, 0 };

	ntfs_debug("Extending mft bitmap allocation.");
	mft_ni = NTFS_I(vol->mft_ino);
	mftbmp_ni = NTFS_I(vol->mftbmp_ino);
	down_write(&mftbmp_ni->runlist.lock);
	read_lock_irqsave(&mftbmp_ni->size_lock, flags);
	ll = mftbmp_ni->allocated_size;
	read_unlock_irqrestore(&mftbmp_ni->size_lock, flags);
	rl = ntfs_attr_find_vcn_nolock(mftbmp_ni,
			(ll - 1) >> vol->cluster_size_bits, NULL);
	if (unlikely(IS_ERR(rl) || !rl->length || rl->lcn < 0)) {
		up_write(&mftbmp_ni->runlist.lock);
		ntfs_error(vol->sb, "Failed to determine last allocated "
				"cluster of mft bitmap attribute.");
		if (!IS_ERR(rl))
			ret = -EIO;
		else
			ret = PTR_ERR(rl);
		return ret;
	}
	lcn = rl->lcn + rl->length;
	ntfs_debug("Last lcn of mft bitmap attribute is 0x%llx.",
			(long long)lcn);
	ll = lcn >> 3;
	page = ntfs_map_page(vol->lcnbmp_ino->i_mapping,
			ll >> PAGE_CACHE_SHIFT);
	if (IS_ERR(page)) {
		up_write(&mftbmp_ni->runlist.lock);
		ntfs_error(vol->sb, "Failed to read from lcn bitmap.");
		return PTR_ERR(page);
	}
	b = (u8*)page_address(page) + (ll & ~PAGE_CACHE_MASK);
	tb = 1 << (lcn & 7ull);
	down_write(&vol->lcnbmp_lock);
	if (*b != 0xff && !(*b & tb)) {
		
		*b |= tb;
		flush_dcache_page(page);
		set_page_dirty(page);
		up_write(&vol->lcnbmp_lock);
		ntfs_unmap_page(page);
		
		rl->length++;
		rl[1].vcn++;
		status.added_cluster = 1;
		ntfs_debug("Appending one cluster to mft bitmap.");
	} else {
		up_write(&vol->lcnbmp_lock);
		ntfs_unmap_page(page);
		
		rl2 = ntfs_cluster_alloc(vol, rl[1].vcn, 1, lcn, DATA_ZONE,
				true);
		if (IS_ERR(rl2)) {
			up_write(&mftbmp_ni->runlist.lock);
			ntfs_error(vol->sb, "Failed to allocate a cluster for "
					"the mft bitmap.");
			return PTR_ERR(rl2);
		}
		rl = ntfs_runlists_merge(mftbmp_ni->runlist.rl, rl2);
		if (IS_ERR(rl)) {
			up_write(&mftbmp_ni->runlist.lock);
			ntfs_error(vol->sb, "Failed to merge runlists for mft "
					"bitmap.");
			if (ntfs_cluster_free_from_rl(vol, rl2)) {
				ntfs_error(vol->sb, "Failed to deallocate "
						"allocated cluster.%s", es);
				NVolSetErrors(vol);
			}
			ntfs_free(rl2);
			return PTR_ERR(rl);
		}
		mftbmp_ni->runlist.rl = rl;
		status.added_run = 1;
		ntfs_debug("Adding one run to mft bitmap.");
		
		for (; rl[1].length; rl++)
			;
	}
	mrec = map_mft_record(mft_ni);
	if (IS_ERR(mrec)) {
		ntfs_error(vol->sb, "Failed to map mft record.");
		ret = PTR_ERR(mrec);
		goto undo_alloc;
	}
	ctx = ntfs_attr_get_search_ctx(mft_ni, mrec);
	if (unlikely(!ctx)) {
		ntfs_error(vol->sb, "Failed to get search context.");
		ret = -ENOMEM;
		goto undo_alloc;
	}
	ret = ntfs_attr_lookup(mftbmp_ni->type, mftbmp_ni->name,
			mftbmp_ni->name_len, CASE_SENSITIVE, rl[1].vcn, NULL,
			0, ctx);
	if (unlikely(ret)) {
		ntfs_error(vol->sb, "Failed to find last attribute extent of "
				"mft bitmap attribute.");
		if (ret == -ENOENT)
			ret = -EIO;
		goto undo_alloc;
	}
	a = ctx->attr;
	ll = sle64_to_cpu(a->data.non_resident.lowest_vcn);
	
	for (rl2 = rl; rl2 > mftbmp_ni->runlist.rl; rl2--) {
		if (ll >= rl2->vcn)
			break;
	}
	BUG_ON(ll < rl2->vcn);
	BUG_ON(ll >= rl2->vcn + rl2->length);
	
	mp_size = ntfs_get_size_for_mapping_pairs(vol, rl2, ll, -1);
	if (unlikely(mp_size <= 0)) {
		ntfs_error(vol->sb, "Get size for mapping pairs failed for "
				"mft bitmap attribute extent.");
		ret = mp_size;
		if (!ret)
			ret = -EIO;
		goto undo_alloc;
	}
	
	old_alen = le32_to_cpu(a->length);
	ret = ntfs_attr_record_resize(ctx->mrec, a, mp_size +
			le16_to_cpu(a->data.non_resident.mapping_pairs_offset));
	if (unlikely(ret)) {
		if (ret != -ENOSPC) {
			ntfs_error(vol->sb, "Failed to resize attribute "
					"record for mft bitmap attribute.");
			goto undo_alloc;
		}
		
		
		
		
		
		ntfs_error(vol->sb, "Not enough space in this mft record to "
				"accommodate extended mft bitmap attribute "
				"extent.  Cannot handle this yet.");
		ret = -EOPNOTSUPP;
		goto undo_alloc;
	}
	status.mp_rebuilt = 1;
	
	ret = ntfs_mapping_pairs_build(vol, (u8*)a +
			le16_to_cpu(a->data.non_resident.mapping_pairs_offset),
			mp_size, rl2, ll, -1, NULL);
	if (unlikely(ret)) {
		ntfs_error(vol->sb, "Failed to build mapping pairs array for "
				"mft bitmap attribute.");
		goto undo_alloc;
	}
	
	a->data.non_resident.highest_vcn = cpu_to_sle64(rl[1].vcn - 1);
	if (a->data.non_resident.lowest_vcn) {
		flush_dcache_mft_record_page(ctx->ntfs_ino);
		mark_mft_record_dirty(ctx->ntfs_ino);
		ntfs_attr_reinit_search_ctx(ctx);
		ret = ntfs_attr_lookup(mftbmp_ni->type, mftbmp_ni->name,
				mftbmp_ni->name_len, CASE_SENSITIVE, 0, NULL,
				0, ctx);
		if (unlikely(ret)) {
			ntfs_error(vol->sb, "Failed to find first attribute "
					"extent of mft bitmap attribute.");
			goto restore_undo_alloc;
		}
		a = ctx->attr;
	}
	write_lock_irqsave(&mftbmp_ni->size_lock, flags);
	mftbmp_ni->allocated_size += vol->cluster_size;
	a->data.non_resident.allocated_size =
			cpu_to_sle64(mftbmp_ni->allocated_size);
	write_unlock_irqrestore(&mftbmp_ni->size_lock, flags);
	
	flush_dcache_mft_record_page(ctx->ntfs_ino);
	mark_mft_record_dirty(ctx->ntfs_ino);
	ntfs_attr_put_search_ctx(ctx);
	unmap_mft_record(mft_ni);
	up_write(&mftbmp_ni->runlist.lock);
	ntfs_debug("Done.");
	return 0;
restore_undo_alloc:
	ntfs_attr_reinit_search_ctx(ctx);
	if (ntfs_attr_lookup(mftbmp_ni->type, mftbmp_ni->name,
			mftbmp_ni->name_len, CASE_SENSITIVE, rl[1].vcn, NULL,
			0, ctx)) {
		ntfs_error(vol->sb, "Failed to find last attribute extent of "
				"mft bitmap attribute.%s", es);
		write_lock_irqsave(&mftbmp_ni->size_lock, flags);
		mftbmp_ni->allocated_size += vol->cluster_size;
		write_unlock_irqrestore(&mftbmp_ni->size_lock, flags);
		ntfs_attr_put_search_ctx(ctx);
		unmap_mft_record(mft_ni);
		up_write(&mftbmp_ni->runlist.lock);
		NVolSetErrors(vol);
		return ret;
	}
	a = ctx->attr;
	a->data.non_resident.highest_vcn = cpu_to_sle64(rl[1].vcn - 2);
undo_alloc:
	if (status.added_cluster) {
		
		rl->length--;
		rl[1].vcn--;
	} else if (status.added_run) {
		lcn = rl->lcn;
		
		rl->lcn = rl[1].lcn;
		rl->length = 0;
	}
	
	down_write(&vol->lcnbmp_lock);
	if (ntfs_bitmap_clear_bit(vol->lcnbmp_ino, lcn)) {
		ntfs_error(vol->sb, "Failed to free allocated cluster.%s", es);
		NVolSetErrors(vol);
	}
	up_write(&vol->lcnbmp_lock);
	if (status.mp_rebuilt) {
		if (ntfs_mapping_pairs_build(vol, (u8*)a + le16_to_cpu(
				a->data.non_resident.mapping_pairs_offset),
				old_alen - le16_to_cpu(
				a->data.non_resident.mapping_pairs_offset),
				rl2, ll, -1, NULL)) {
			ntfs_error(vol->sb, "Failed to restore mapping pairs "
					"array.%s", es);
			NVolSetErrors(vol);
		}
		if (ntfs_attr_record_resize(ctx->mrec, a, old_alen)) {
			ntfs_error(vol->sb, "Failed to restore attribute "
					"record.%s", es);
			NVolSetErrors(vol);
		}
		flush_dcache_mft_record_page(ctx->ntfs_ino);
		mark_mft_record_dirty(ctx->ntfs_ino);
	}
	if (ctx)
		ntfs_attr_put_search_ctx(ctx);
	if (!IS_ERR(mrec))
		unmap_mft_record(mft_ni);
	up_write(&mftbmp_ni->runlist.lock);
	return ret;
}

static int ntfs_mft_bitmap_extend_initialized_nolock(ntfs_volume *vol)
{
	s64 old_data_size, old_initialized_size;
	unsigned long flags;
	struct inode *mftbmp_vi;
	ntfs_inode *mft_ni, *mftbmp_ni;
	ntfs_attr_search_ctx *ctx;
	MFT_RECORD *mrec;
	ATTR_RECORD *a;
	int ret;

	ntfs_debug("Extending mft bitmap initiailized (and data) size.");
	mft_ni = NTFS_I(vol->mft_ino);
	mftbmp_vi = vol->mftbmp_ino;
	mftbmp_ni = NTFS_I(mftbmp_vi);
	
	mrec = map_mft_record(mft_ni);
	if (IS_ERR(mrec)) {
		ntfs_error(vol->sb, "Failed to map mft record.");
		return PTR_ERR(mrec);
	}
	ctx = ntfs_attr_get_search_ctx(mft_ni, mrec);
	if (unlikely(!ctx)) {
		ntfs_error(vol->sb, "Failed to get search context.");
		ret = -ENOMEM;
		goto unm_err_out;
	}
	ret = ntfs_attr_lookup(mftbmp_ni->type, mftbmp_ni->name,
			mftbmp_ni->name_len, CASE_SENSITIVE, 0, NULL, 0, ctx);
	if (unlikely(ret)) {
		ntfs_error(vol->sb, "Failed to find first attribute extent of "
				"mft bitmap attribute.");
		if (ret == -ENOENT)
			ret = -EIO;
		goto put_err_out;
	}
	a = ctx->attr;
	write_lock_irqsave(&mftbmp_ni->size_lock, flags);
	old_data_size = i_size_read(mftbmp_vi);
	old_initialized_size = mftbmp_ni->initialized_size;
	mftbmp_ni->initialized_size += 8;
	a->data.non_resident.initialized_size =
			cpu_to_sle64(mftbmp_ni->initialized_size);
	if (mftbmp_ni->initialized_size > old_data_size) {
		i_size_write(mftbmp_vi, mftbmp_ni->initialized_size);
		a->data.non_resident.data_size =
				cpu_to_sle64(mftbmp_ni->initialized_size);
	}
	write_unlock_irqrestore(&mftbmp_ni->size_lock, flags);
	
	flush_dcache_mft_record_page(ctx->ntfs_ino);
	mark_mft_record_dirty(ctx->ntfs_ino);
	ntfs_attr_put_search_ctx(ctx);
	unmap_mft_record(mft_ni);
	
	ret = ntfs_attr_set(mftbmp_ni, old_initialized_size, 8, 0);
	if (likely(!ret)) {
		ntfs_debug("Done.  (Wrote eight initialized bytes to mft "
				"bitmap.");
		return 0;
	}
	ntfs_error(vol->sb, "Failed to write to mft bitmap.");
	
	mrec = map_mft_record(mft_ni);
	if (IS_ERR(mrec)) {
		ntfs_error(vol->sb, "Failed to map mft record.%s", es);
		NVolSetErrors(vol);
		return ret;
	}
	ctx = ntfs_attr_get_search_ctx(mft_ni, mrec);
	if (unlikely(!ctx)) {
		ntfs_error(vol->sb, "Failed to get search context.%s", es);
		NVolSetErrors(vol);
		goto unm_err_out;
	}
	if (ntfs_attr_lookup(mftbmp_ni->type, mftbmp_ni->name,
			mftbmp_ni->name_len, CASE_SENSITIVE, 0, NULL, 0, ctx)) {
		ntfs_error(vol->sb, "Failed to find first attribute extent of "
				"mft bitmap attribute.%s", es);
		NVolSetErrors(vol);
put_err_out:
		ntfs_attr_put_search_ctx(ctx);
unm_err_out:
		unmap_mft_record(mft_ni);
		goto err_out;
	}
	a = ctx->attr;
	write_lock_irqsave(&mftbmp_ni->size_lock, flags);
	mftbmp_ni->initialized_size = old_initialized_size;
	a->data.non_resident.initialized_size =
			cpu_to_sle64(old_initialized_size);
	if (i_size_read(mftbmp_vi) != old_data_size) {
		i_size_write(mftbmp_vi, old_data_size);
		a->data.non_resident.data_size = cpu_to_sle64(old_data_size);
	}
	write_unlock_irqrestore(&mftbmp_ni->size_lock, flags);
	flush_dcache_mft_record_page(ctx->ntfs_ino);
	mark_mft_record_dirty(ctx->ntfs_ino);
	ntfs_attr_put_search_ctx(ctx);
	unmap_mft_record(mft_ni);
#ifdef DEBUG
	read_lock_irqsave(&mftbmp_ni->size_lock, flags);
	ntfs_debug("Restored status of mftbmp: allocated_size 0x%llx, "
			"data_size 0x%llx, initialized_size 0x%llx.",
			(long long)mftbmp_ni->allocated_size,
			(long long)i_size_read(mftbmp_vi),
			(long long)mftbmp_ni->initialized_size);
	read_unlock_irqrestore(&mftbmp_ni->size_lock, flags);
#endif 
err_out:
	return ret;
}

static int ntfs_mft_data_extend_allocation_nolock(ntfs_volume *vol)
{
	LCN lcn;
	VCN old_last_vcn;
	s64 min_nr, nr, ll;
	unsigned long flags;
	ntfs_inode *mft_ni;
	runlist_element *rl, *rl2;
	ntfs_attr_search_ctx *ctx = NULL;
	MFT_RECORD *mrec;
	ATTR_RECORD *a = NULL;
	int ret, mp_size;
	u32 old_alen = 0;
	bool mp_rebuilt = false;

	ntfs_debug("Extending mft data allocation.");
	mft_ni = NTFS_I(vol->mft_ino);
	down_write(&mft_ni->runlist.lock);
	read_lock_irqsave(&mft_ni->size_lock, flags);
	ll = mft_ni->allocated_size;
	read_unlock_irqrestore(&mft_ni->size_lock, flags);
	rl = ntfs_attr_find_vcn_nolock(mft_ni,
			(ll - 1) >> vol->cluster_size_bits, NULL);
	if (unlikely(IS_ERR(rl) || !rl->length || rl->lcn < 0)) {
		up_write(&mft_ni->runlist.lock);
		ntfs_error(vol->sb, "Failed to determine last allocated "
				"cluster of mft data attribute.");
		if (!IS_ERR(rl))
			ret = -EIO;
		else
			ret = PTR_ERR(rl);
		return ret;
	}
	lcn = rl->lcn + rl->length;
	ntfs_debug("Last lcn of mft data attribute is 0x%llx.", (long long)lcn);
	
	min_nr = vol->mft_record_size >> vol->cluster_size_bits;
	if (!min_nr)
		min_nr = 1;
	
	nr = vol->mft_record_size << 4 >> vol->cluster_size_bits;
	if (!nr)
		nr = min_nr;
	
	read_lock_irqsave(&mft_ni->size_lock, flags);
	ll = mft_ni->allocated_size;
	read_unlock_irqrestore(&mft_ni->size_lock, flags);
	if (unlikely((ll + (nr << vol->cluster_size_bits)) >>
			vol->mft_record_size_bits >= (1ll << 32))) {
		nr = min_nr;
		if (unlikely((ll + (nr << vol->cluster_size_bits)) >>
				vol->mft_record_size_bits >= (1ll << 32))) {
			ntfs_warning(vol->sb, "Cannot allocate mft record "
					"because the maximum number of inodes "
					"(2^32) has already been reached.");
			up_write(&mft_ni->runlist.lock);
			return -ENOSPC;
		}
	}
	ntfs_debug("Trying mft data allocation with %s cluster count %lli.",
			nr > min_nr ? "default" : "minimal", (long long)nr);
	old_last_vcn = rl[1].vcn;
	do {
		rl2 = ntfs_cluster_alloc(vol, old_last_vcn, nr, lcn, MFT_ZONE,
				true);
		if (likely(!IS_ERR(rl2)))
			break;
		if (PTR_ERR(rl2) != -ENOSPC || nr == min_nr) {
			ntfs_error(vol->sb, "Failed to allocate the minimal "
					"number of clusters (%lli) for the "
					"mft data attribute.", (long long)nr);
			up_write(&mft_ni->runlist.lock);
			return PTR_ERR(rl2);
		}
		nr = min_nr;
		ntfs_debug("Retrying mft data allocation with minimal cluster "
				"count %lli.", (long long)nr);
	} while (1);
	rl = ntfs_runlists_merge(mft_ni->runlist.rl, rl2);
	if (IS_ERR(rl)) {
		up_write(&mft_ni->runlist.lock);
		ntfs_error(vol->sb, "Failed to merge runlists for mft data "
				"attribute.");
		if (ntfs_cluster_free_from_rl(vol, rl2)) {
			ntfs_error(vol->sb, "Failed to deallocate clusters "
					"from the mft data attribute.%s", es);
			NVolSetErrors(vol);
		}
		ntfs_free(rl2);
		return PTR_ERR(rl);
	}
	mft_ni->runlist.rl = rl;
	ntfs_debug("Allocated %lli clusters.", (long long)nr);
	
	for (; rl[1].length; rl++)
		;
	
	mrec = map_mft_record(mft_ni);
	if (IS_ERR(mrec)) {
		ntfs_error(vol->sb, "Failed to map mft record.");
		ret = PTR_ERR(mrec);
		goto undo_alloc;
	}
	ctx = ntfs_attr_get_search_ctx(mft_ni, mrec);
	if (unlikely(!ctx)) {
		ntfs_error(vol->sb, "Failed to get search context.");
		ret = -ENOMEM;
		goto undo_alloc;
	}
	ret = ntfs_attr_lookup(mft_ni->type, mft_ni->name, mft_ni->name_len,
			CASE_SENSITIVE, rl[1].vcn, NULL, 0, ctx);
	if (unlikely(ret)) {
		ntfs_error(vol->sb, "Failed to find last attribute extent of "
				"mft data attribute.");
		if (ret == -ENOENT)
			ret = -EIO;
		goto undo_alloc;
	}
	a = ctx->attr;
	ll = sle64_to_cpu(a->data.non_resident.lowest_vcn);
	
	for (rl2 = rl; rl2 > mft_ni->runlist.rl; rl2--) {
		if (ll >= rl2->vcn)
			break;
	}
	BUG_ON(ll < rl2->vcn);
	BUG_ON(ll >= rl2->vcn + rl2->length);
	
	mp_size = ntfs_get_size_for_mapping_pairs(vol, rl2, ll, -1);
	if (unlikely(mp_size <= 0)) {
		ntfs_error(vol->sb, "Get size for mapping pairs failed for "
				"mft data attribute extent.");
		ret = mp_size;
		if (!ret)
			ret = -EIO;
		goto undo_alloc;
	}
	
	old_alen = le32_to_cpu(a->length);
	ret = ntfs_attr_record_resize(ctx->mrec, a, mp_size +
			le16_to_cpu(a->data.non_resident.mapping_pairs_offset));
	if (unlikely(ret)) {
		if (ret != -ENOSPC) {
			ntfs_error(vol->sb, "Failed to resize attribute "
					"record for mft data attribute.");
			goto undo_alloc;
		}
		
		
		
		
		
		
		
		
		
		
		ntfs_error(vol->sb, "Not enough space in this mft record to "
				"accommodate extended mft data attribute "
				"extent.  Cannot handle this yet.");
		ret = -EOPNOTSUPP;
		goto undo_alloc;
	}
	mp_rebuilt = true;
	
	ret = ntfs_mapping_pairs_build(vol, (u8*)a +
			le16_to_cpu(a->data.non_resident.mapping_pairs_offset),
			mp_size, rl2, ll, -1, NULL);
	if (unlikely(ret)) {
		ntfs_error(vol->sb, "Failed to build mapping pairs array of "
				"mft data attribute.");
		goto undo_alloc;
	}
	
	a->data.non_resident.highest_vcn = cpu_to_sle64(rl[1].vcn - 1);
	if (a->data.non_resident.lowest_vcn) {
		flush_dcache_mft_record_page(ctx->ntfs_ino);
		mark_mft_record_dirty(ctx->ntfs_ino);
		ntfs_attr_reinit_search_ctx(ctx);
		ret = ntfs_attr_lookup(mft_ni->type, mft_ni->name,
				mft_ni->name_len, CASE_SENSITIVE, 0, NULL, 0,
				ctx);
		if (unlikely(ret)) {
			ntfs_error(vol->sb, "Failed to find first attribute "
					"extent of mft data attribute.");
			goto restore_undo_alloc;
		}
		a = ctx->attr;
	}
	write_lock_irqsave(&mft_ni->size_lock, flags);
	mft_ni->allocated_size += nr << vol->cluster_size_bits;
	a->data.non_resident.allocated_size =
			cpu_to_sle64(mft_ni->allocated_size);
	write_unlock_irqrestore(&mft_ni->size_lock, flags);
	
	flush_dcache_mft_record_page(ctx->ntfs_ino);
	mark_mft_record_dirty(ctx->ntfs_ino);
	ntfs_attr_put_search_ctx(ctx);
	unmap_mft_record(mft_ni);
	up_write(&mft_ni->runlist.lock);
	ntfs_debug("Done.");
	return 0;
restore_undo_alloc:
	ntfs_attr_reinit_search_ctx(ctx);
	if (ntfs_attr_lookup(mft_ni->type, mft_ni->name, mft_ni->name_len,
			CASE_SENSITIVE, rl[1].vcn, NULL, 0, ctx)) {
		ntfs_error(vol->sb, "Failed to find last attribute extent of "
				"mft data attribute.%s", es);
		write_lock_irqsave(&mft_ni->size_lock, flags);
		mft_ni->allocated_size += nr << vol->cluster_size_bits;
		write_unlock_irqrestore(&mft_ni->size_lock, flags);
		ntfs_attr_put_search_ctx(ctx);
		unmap_mft_record(mft_ni);
		up_write(&mft_ni->runlist.lock);
		NVolSetErrors(vol);
		return ret;
	}
	ctx->attr->data.non_resident.highest_vcn =
			cpu_to_sle64(old_last_vcn - 1);
undo_alloc:
	if (ntfs_cluster_free(mft_ni, old_last_vcn, -1, ctx) < 0) {
		ntfs_error(vol->sb, "Failed to free clusters from mft data "
				"attribute.%s", es);
		NVolSetErrors(vol);
	}
	a = ctx->attr;
	if (ntfs_rl_truncate_nolock(vol, &mft_ni->runlist, old_last_vcn)) {
		ntfs_error(vol->sb, "Failed to truncate mft data attribute "
				"runlist.%s", es);
		NVolSetErrors(vol);
	}
	if (mp_rebuilt && !IS_ERR(ctx->mrec)) {
		if (ntfs_mapping_pairs_build(vol, (u8*)a + le16_to_cpu(
				a->data.non_resident.mapping_pairs_offset),
				old_alen - le16_to_cpu(
				a->data.non_resident.mapping_pairs_offset),
				rl2, ll, -1, NULL)) {
			ntfs_error(vol->sb, "Failed to restore mapping pairs "
					"array.%s", es);
			NVolSetErrors(vol);
		}
		if (ntfs_attr_record_resize(ctx->mrec, a, old_alen)) {
			ntfs_error(vol->sb, "Failed to restore attribute "
					"record.%s", es);
			NVolSetErrors(vol);
		}
		flush_dcache_mft_record_page(ctx->ntfs_ino);
		mark_mft_record_dirty(ctx->ntfs_ino);
	} else if (IS_ERR(ctx->mrec)) {
		ntfs_error(vol->sb, "Failed to restore attribute search "
				"context.%s", es);
		NVolSetErrors(vol);
	}
	if (ctx)
		ntfs_attr_put_search_ctx(ctx);
	if (!IS_ERR(mrec))
		unmap_mft_record(mft_ni);
	up_write(&mft_ni->runlist.lock);
	return ret;
}

static int ntfs_mft_record_layout(const ntfs_volume *vol, const s64 mft_no,
		MFT_RECORD *m)
{
	ATTR_RECORD *a;

	ntfs_debug("Entering for mft record 0x%llx.", (long long)mft_no);
	if (mft_no >= (1ll << 32)) {
		ntfs_error(vol->sb, "Mft record number 0x%llx exceeds "
				"maximum of 2^32.", (long long)mft_no);
		return -ERANGE;
	}
	
	memset(m, 0, vol->mft_record_size);
	
	if (vol->major_ver < 3 || (vol->major_ver == 3 && !vol->minor_ver))
		m->usa_ofs = cpu_to_le16((sizeof(MFT_RECORD_OLD) + 1) & ~1);
	else {
		m->usa_ofs = cpu_to_le16((sizeof(MFT_RECORD) + 1) & ~1);
		m->reserved = 0;
		m->mft_record_number = cpu_to_le32((u32)mft_no);
	}
	m->magic = magic_FILE;
	if (vol->mft_record_size >= NTFS_BLOCK_SIZE)
		m->usa_count = cpu_to_le16(vol->mft_record_size /
				NTFS_BLOCK_SIZE + 1);
	else {
		m->usa_count = cpu_to_le16(1);
		ntfs_warning(vol->sb, "Sector size is bigger than mft record "
				"size.  Setting usa_count to 1.  If chkdsk "
				"reports this as corruption, please email "
				"linux-ntfs-dev@lists.sourceforge.net stating "
				"that you saw this message and that the "
				"modified filesystem created was corrupt.  "
				"Thank you.");
	}
	
	*(le16*)((u8*)m + le16_to_cpu(m->usa_ofs)) = cpu_to_le16(1);
	m->lsn = 0;
	m->sequence_number = cpu_to_le16(1);
	m->link_count = 0;
	m->attrs_offset = cpu_to_le16((le16_to_cpu(m->usa_ofs) +
			(le16_to_cpu(m->usa_count) << 1) + 7) & ~7);
	m->flags = 0;
	m->bytes_in_use = cpu_to_le32(le16_to_cpu(m->attrs_offset) + 8);
	m->bytes_allocated = cpu_to_le32(vol->mft_record_size);
	m->base_mft_record = 0;
	m->next_attr_instance = 0;
	
	a = (ATTR_RECORD*)((u8*)m + le16_to_cpu(m->attrs_offset));
	a->type = AT_END;
	a->length = 0;
	ntfs_debug("Done.");
	return 0;
}

static int ntfs_mft_record_format(const ntfs_volume *vol, const s64 mft_no)
{
	loff_t i_size;
	struct inode *mft_vi = vol->mft_ino;
	struct page *page;
	MFT_RECORD *m;
	pgoff_t index, end_index;
	unsigned int ofs;
	int err;

	ntfs_debug("Entering for mft record 0x%llx.", (long long)mft_no);
	index = mft_no << vol->mft_record_size_bits >> PAGE_CACHE_SHIFT;
	ofs = (mft_no << vol->mft_record_size_bits) & ~PAGE_CACHE_MASK;
	
	i_size = i_size_read(mft_vi);
	end_index = i_size >> PAGE_CACHE_SHIFT;
	if (unlikely(index >= end_index)) {
		if (unlikely(index > end_index || ofs + vol->mft_record_size >=
				(i_size & ~PAGE_CACHE_MASK))) {
			ntfs_error(vol->sb, "Tried to format non-existing mft "
					"record 0x%llx.", (long long)mft_no);
			return -ENOENT;
		}
	}
	
	page = ntfs_map_page(mft_vi->i_mapping, index);
	if (IS_ERR(page)) {
		ntfs_error(vol->sb, "Failed to map page containing mft record "
				"to format 0x%llx.", (long long)mft_no);
		return PTR_ERR(page);
	}
	lock_page(page);
	BUG_ON(!PageUptodate(page));
	ClearPageUptodate(page);
	m = (MFT_RECORD*)((u8*)page_address(page) + ofs);
	err = ntfs_mft_record_layout(vol, mft_no, m);
	if (unlikely(err)) {
		ntfs_error(vol->sb, "Failed to layout mft record 0x%llx.",
				(long long)mft_no);
		SetPageUptodate(page);
		unlock_page(page);
		ntfs_unmap_page(page);
		return err;
	}
	flush_dcache_page(page);
	SetPageUptodate(page);
	unlock_page(page);
	/*
	 * Make sure the mft record is written out to disk.  We could use
	 * ilookup5() to check if an inode is in icache and so on but this is
	 * unnecessary as ntfs_writepage() will write the dirty record anyway.
	 */
	mark_ntfs_record_dirty(page, ofs);
	ntfs_unmap_page(page);
	ntfs_debug("Done.");
	return 0;
}

/**
 * ntfs_mft_record_alloc - allocate an mft record on an ntfs volume
 * @vol:	[IN]  volume on which to allocate the mft record
 * @mode:	[IN]  mode if want a file or directory, i.e. base inode or 0
 * @base_ni:	[IN]  open base inode if allocating an extent mft record or NULL
 * @mrec:	[OUT] on successful return this is the mapped mft record
 *
 * Allocate an mft record in $MFT/$DATA of an open ntfs volume @vol.
 *
 * If @base_ni is NULL make the mft record a base mft record, i.e. a file or
 * direvctory inode, and allocate it at the default allocator position.  In
 * this case @mode is the file mode as given to us by the caller.  We in
 * particular use @mode to distinguish whether a file or a directory is being
 * created (S_IFDIR(mode) and S_IFREG(mode), respectively).
 *
 * If @base_ni is not NULL make the allocated mft record an extent record,
 * allocate it starting at the mft record after the base mft record and attach
 * the allocated and opened ntfs inode to the base inode @base_ni.  In this
 * case @mode must be 0 as it is meaningless for extent inodes.
 *
 * You need to check the return value with IS_ERR().  If false, the function
 * was successful and the return value is the now opened ntfs inode of the
 * allocated mft record.  *@mrec is then set to the allocated, mapped, pinned,
 * and locked mft record.  If IS_ERR() is true, the function failed and the
 * error code is obtained from PTR_ERR(return value).  *@mrec is undefined in
 * this case.
 *
 * Allocation strategy:
 *
 * To find a free mft record, we scan the mft bitmap for a zero bit.  To
 * optimize this we start scanning at the place specified by @base_ni or if
 * @base_ni is NULL we start where we last stopped and we perform wrap around
 * when we reach the end.  Note, we do not try to allocate mft records below
 * number 24 because numbers 0 to 15 are the defined system files anyway and 16
 * to 24 are special in that they are used for storing extension mft records
 * for the $DATA attribute of $MFT.  This is required to avoid the possibility
 * of creating a runlist with a circular dependency which once written to disk
 * can never be read in again.  Windows will only use records 16 to 24 for
 * normal files if the volume is completely out of space.  We never use them
 * which means that when the volume is really out of space we cannot create any
 * more files while Windows can still create up to 8 small files.  We can start
 * doing this at some later time, it does not matter much for now.
 *
 * When scanning the mft bitmap, we only search up to the last allocated mft
 * record.  If there are no free records left in the range 24 to number of
 * allocated mft records, then we extend the $MFT/$DATA attribute in order to
 * create free mft records.  We extend the allocated size of $MFT/$DATA by 16
 * records at a time or one cluster, if cluster size is above 16kiB.  If there
 * is not sufficient space to do this, we try to extend by a single mft record
 * or one cluster, if cluster size is above the mft record size.
 *
 * No matter how many mft records we allocate, we initialize only the first
 * allocated mft record, incrementing mft data size and initialized size
 * accordingly, open an ntfs_inode for it and return it to the caller, unless
 * there are less than 24 mft records, in which case we allocate and initialize
 * mft records until we reach record 24 which we consider as the first free mft
 * record for use by normal files.
 *
 * If during any stage we overflow the initialized data in the mft bitmap, we
 * extend the initialized size (and data size) by 8 bytes, allocating another
 * cluster if required.  The bitmap data size has to be at least equal to the
 * number of mft records in the mft, but it can be bigger, in which case the
 * superflous bits are padded with zeroes.
 *
 * Thus, when we return successfully (IS_ERR() is false), we will have:
 *	- initialized / extended the mft bitmap if necessary,
 *	- initialized / extended the mft data if necessary,
 *	- set the bit corresponding to the mft record being allocated in the
 *	  mft bitmap,
 *	- opened an ntfs_inode for the allocated mft record, and we will have
 *	- returned the ntfs_inode as well as the allocated mapped, pinned, and
 *	  locked mft record.
 *
 * On error, the volume will be left in a consistent state and no record will
 * be allocated.  If rolling back a partial operation fails, we may leave some
 * inconsistent metadata in which case we set NVolErrors() so the volume is
 * left dirty when unmounted.
 *
 * Note, this function cannot make use of most of the normal functions, like
 * for example for attribute resizing, etc, because when the run list overflows
 * the base mft record and an attribute list is used, it is very important that
 * the extension mft records used to store the $DATA attribute of $MFT can be
 * reached without having to read the information contained inside them, as
 * this would make it impossible to find them in the first place after the
 * volume is unmounted.  $MFT/$BITMAP probably does not need to follow this
 * rule because the bitmap is not essential for finding the mft records, but on
 * the other hand, handling the bitmap in this special way would make life
 * easier because otherwise there might be circular invocations of functions
 * when reading the bitmap.
 */
ntfs_inode *ntfs_mft_record_alloc(ntfs_volume *vol, const int mode,
		ntfs_inode *base_ni, MFT_RECORD **mrec)
{
	s64 ll, bit, old_data_initialized, old_data_size;
	unsigned long flags;
	struct inode *vi;
	struct page *page;
	ntfs_inode *mft_ni, *mftbmp_ni, *ni;
	ntfs_attr_search_ctx *ctx;
	MFT_RECORD *m;
	ATTR_RECORD *a;
	pgoff_t index;
	unsigned int ofs;
	int err;
	le16 seq_no, usn;
	bool record_formatted = false;

	if (base_ni) {
		ntfs_debug("Entering (allocating an extent mft record for "
				"base mft record 0x%llx).",
				(long long)base_ni->mft_no);
		
		BUG_ON(mode);
	} else
		ntfs_debug("Entering (allocating a base mft record).");
	if (mode) {
		
		BUG_ON(base_ni);
		
		if (!S_ISREG(mode) && !S_ISDIR(mode))
			return ERR_PTR(-EOPNOTSUPP);
	}
	BUG_ON(!mrec);
	mft_ni = NTFS_I(vol->mft_ino);
	mftbmp_ni = NTFS_I(vol->mftbmp_ino);
	down_write(&vol->mftbmp_lock);
	bit = ntfs_mft_bitmap_find_and_alloc_free_rec_nolock(vol, base_ni);
	if (bit >= 0) {
		ntfs_debug("Found and allocated free record (#1), bit 0x%llx.",
				(long long)bit);
		goto have_alloc_rec;
	}
	if (bit != -ENOSPC) {
		up_write(&vol->mftbmp_lock);
		return ERR_PTR(bit);
	}
	read_lock_irqsave(&mft_ni->size_lock, flags);
	ll = mft_ni->initialized_size >> vol->mft_record_size_bits;
	read_unlock_irqrestore(&mft_ni->size_lock, flags);
	read_lock_irqsave(&mftbmp_ni->size_lock, flags);
	old_data_initialized = mftbmp_ni->initialized_size;
	read_unlock_irqrestore(&mftbmp_ni->size_lock, flags);
	if (old_data_initialized << 3 > ll && old_data_initialized > 3) {
		bit = ll;
		if (bit < 24)
			bit = 24;
		if (unlikely(bit >= (1ll << 32)))
			goto max_err_out;
		ntfs_debug("Found free record (#2), bit 0x%llx.",
				(long long)bit);
		goto found_free_rec;
	}
	bit = old_data_initialized << 3;
	if (unlikely(bit >= (1ll << 32)))
		goto max_err_out;
	read_lock_irqsave(&mftbmp_ni->size_lock, flags);
	old_data_size = mftbmp_ni->allocated_size;
	ntfs_debug("Status of mftbmp before extension: allocated_size 0x%llx, "
			"data_size 0x%llx, initialized_size 0x%llx.",
			(long long)old_data_size,
			(long long)i_size_read(vol->mftbmp_ino),
			(long long)old_data_initialized);
	read_unlock_irqrestore(&mftbmp_ni->size_lock, flags);
	if (old_data_initialized + 8 > old_data_size) {
		
		ntfs_debug("mftbmp: initialized_size + 8 > allocated_size.");
		err = ntfs_mft_bitmap_extend_allocation_nolock(vol);
		if (unlikely(err)) {
			up_write(&vol->mftbmp_lock);
			goto err_out;
		}
#ifdef DEBUG
		read_lock_irqsave(&mftbmp_ni->size_lock, flags);
		ntfs_debug("Status of mftbmp after allocation extension: "
				"allocated_size 0x%llx, data_size 0x%llx, "
				"initialized_size 0x%llx.",
				(long long)mftbmp_ni->allocated_size,
				(long long)i_size_read(vol->mftbmp_ino),
				(long long)mftbmp_ni->initialized_size);
		read_unlock_irqrestore(&mftbmp_ni->size_lock, flags);
#endif 
	}
	err = ntfs_mft_bitmap_extend_initialized_nolock(vol);
	if (unlikely(err)) {
		up_write(&vol->mftbmp_lock);
		goto err_out;
	}
#ifdef DEBUG
	read_lock_irqsave(&mftbmp_ni->size_lock, flags);
	ntfs_debug("Status of mftbmp after initialized extension: "
			"allocated_size 0x%llx, data_size 0x%llx, "
			"initialized_size 0x%llx.",
			(long long)mftbmp_ni->allocated_size,
			(long long)i_size_read(vol->mftbmp_ino),
			(long long)mftbmp_ni->initialized_size);
	read_unlock_irqrestore(&mftbmp_ni->size_lock, flags);
#endif 
	ntfs_debug("Found free record (#3), bit 0x%llx.", (long long)bit);
found_free_rec:
	
	ntfs_debug("At found_free_rec.");
	err = ntfs_bitmap_set_bit(vol->mftbmp_ino, bit);
	if (unlikely(err)) {
		ntfs_error(vol->sb, "Failed to allocate bit in mft bitmap.");
		up_write(&vol->mftbmp_lock);
		goto err_out;
	}
	ntfs_debug("Set bit 0x%llx in mft bitmap.", (long long)bit);
have_alloc_rec:
	ll = (bit + 1) << vol->mft_record_size_bits;
	read_lock_irqsave(&mft_ni->size_lock, flags);
	old_data_initialized = mft_ni->initialized_size;
	read_unlock_irqrestore(&mft_ni->size_lock, flags);
	if (ll <= old_data_initialized) {
		ntfs_debug("Allocated mft record already initialized.");
		goto mft_rec_already_initialized;
	}
	ntfs_debug("Initializing allocated mft record.");
	/*
	 * The mft record is outside the initialized data.  Extend the mft data
	 * attribute until it covers the allocated record.  The loop is only
	 * actually traversed more than once when a freshly formatted volume is
	 * first written to so it optimizes away nicely in the common case.
	 */
	read_lock_irqsave(&mft_ni->size_lock, flags);
	ntfs_debug("Status of mft data before extension: "
			"allocated_size 0x%llx, data_size 0x%llx, "
			"initialized_size 0x%llx.",
			(long long)mft_ni->allocated_size,
			(long long)i_size_read(vol->mft_ino),
			(long long)mft_ni->initialized_size);
	while (ll > mft_ni->allocated_size) {
		read_unlock_irqrestore(&mft_ni->size_lock, flags);
		err = ntfs_mft_data_extend_allocation_nolock(vol);
		if (unlikely(err)) {
			ntfs_error(vol->sb, "Failed to extend mft data "
					"allocation.");
			goto undo_mftbmp_alloc_nolock;
		}
		read_lock_irqsave(&mft_ni->size_lock, flags);
		ntfs_debug("Status of mft data after allocation extension: "
				"allocated_size 0x%llx, data_size 0x%llx, "
				"initialized_size 0x%llx.",
				(long long)mft_ni->allocated_size,
				(long long)i_size_read(vol->mft_ino),
				(long long)mft_ni->initialized_size);
	}
	read_unlock_irqrestore(&mft_ni->size_lock, flags);
	write_lock_irqsave(&mft_ni->size_lock, flags);
	old_data_initialized = mft_ni->initialized_size;
	old_data_size = vol->mft_ino->i_size;
	while (ll > mft_ni->initialized_size) {
		s64 new_initialized_size, mft_no;
		
		new_initialized_size = mft_ni->initialized_size +
				vol->mft_record_size;
		mft_no = mft_ni->initialized_size >> vol->mft_record_size_bits;
		if (new_initialized_size > i_size_read(vol->mft_ino))
			i_size_write(vol->mft_ino, new_initialized_size);
		write_unlock_irqrestore(&mft_ni->size_lock, flags);
		ntfs_debug("Initializing mft record 0x%llx.",
				(long long)mft_no);
		err = ntfs_mft_record_format(vol, mft_no);
		if (unlikely(err)) {
			ntfs_error(vol->sb, "Failed to format mft record.");
			goto undo_data_init;
		}
		write_lock_irqsave(&mft_ni->size_lock, flags);
		mft_ni->initialized_size = new_initialized_size;
	}
	write_unlock_irqrestore(&mft_ni->size_lock, flags);
	record_formatted = true;
	
	m = map_mft_record(mft_ni);
	if (IS_ERR(m)) {
		ntfs_error(vol->sb, "Failed to map mft record.");
		err = PTR_ERR(m);
		goto undo_data_init;
	}
	ctx = ntfs_attr_get_search_ctx(mft_ni, m);
	if (unlikely(!ctx)) {
		ntfs_error(vol->sb, "Failed to get search context.");
		err = -ENOMEM;
		unmap_mft_record(mft_ni);
		goto undo_data_init;
	}
	err = ntfs_attr_lookup(mft_ni->type, mft_ni->name, mft_ni->name_len,
			CASE_SENSITIVE, 0, NULL, 0, ctx);
	if (unlikely(err)) {
		ntfs_error(vol->sb, "Failed to find first attribute extent of "
				"mft data attribute.");
		ntfs_attr_put_search_ctx(ctx);
		unmap_mft_record(mft_ni);
		goto undo_data_init;
	}
	a = ctx->attr;
	read_lock_irqsave(&mft_ni->size_lock, flags);
	a->data.non_resident.initialized_size =
			cpu_to_sle64(mft_ni->initialized_size);
	a->data.non_resident.data_size =
			cpu_to_sle64(i_size_read(vol->mft_ino));
	read_unlock_irqrestore(&mft_ni->size_lock, flags);
	
	flush_dcache_mft_record_page(ctx->ntfs_ino);
	mark_mft_record_dirty(ctx->ntfs_ino);
	ntfs_attr_put_search_ctx(ctx);
	unmap_mft_record(mft_ni);
	read_lock_irqsave(&mft_ni->size_lock, flags);
	ntfs_debug("Status of mft data after mft record initialization: "
			"allocated_size 0x%llx, data_size 0x%llx, "
			"initialized_size 0x%llx.",
			(long long)mft_ni->allocated_size,
			(long long)i_size_read(vol->mft_ino),
			(long long)mft_ni->initialized_size);
	BUG_ON(i_size_read(vol->mft_ino) > mft_ni->allocated_size);
	BUG_ON(mft_ni->initialized_size > i_size_read(vol->mft_ino));
	read_unlock_irqrestore(&mft_ni->size_lock, flags);
mft_rec_already_initialized:
	up_write(&vol->mftbmp_lock);
	index = bit << vol->mft_record_size_bits >> PAGE_CACHE_SHIFT;
	ofs = (bit << vol->mft_record_size_bits) & ~PAGE_CACHE_MASK;
	
	page = ntfs_map_page(vol->mft_ino->i_mapping, index);
	if (IS_ERR(page)) {
		ntfs_error(vol->sb, "Failed to map page containing allocated "
				"mft record 0x%llx.", (long long)bit);
		err = PTR_ERR(page);
		goto undo_mftbmp_alloc;
	}
	lock_page(page);
	BUG_ON(!PageUptodate(page));
	ClearPageUptodate(page);
	m = (MFT_RECORD*)((u8*)page_address(page) + ofs);
	
	if (!record_formatted) {
		
		if (ntfs_is_file_record(m->magic) &&
				(m->flags & MFT_RECORD_IN_USE)) {
			ntfs_error(vol->sb, "Mft record 0x%llx was marked "
					"free in mft bitmap but is marked "
					"used itself.  Corrupt filesystem.  "
					"Unmount and run chkdsk.",
					(long long)bit);
			err = -EIO;
			SetPageUptodate(page);
			unlock_page(page);
			ntfs_unmap_page(page);
			NVolSetErrors(vol);
			goto undo_mftbmp_alloc;
		}
		seq_no = m->sequence_number;
		usn = *(le16*)((u8*)m + le16_to_cpu(m->usa_ofs));
		err = ntfs_mft_record_layout(vol, bit, m);
		if (unlikely(err)) {
			ntfs_error(vol->sb, "Failed to layout allocated mft "
					"record 0x%llx.", (long long)bit);
			SetPageUptodate(page);
			unlock_page(page);
			ntfs_unmap_page(page);
			goto undo_mftbmp_alloc;
		}
		if (seq_no)
			m->sequence_number = seq_no;
		if (usn && le16_to_cpu(usn) != 0xffff)
			*(le16*)((u8*)m + le16_to_cpu(m->usa_ofs)) = usn;
	}
	
	m->flags |= MFT_RECORD_IN_USE;
	if (S_ISDIR(mode))
		m->flags |= MFT_RECORD_IS_DIRECTORY;
	flush_dcache_page(page);
	SetPageUptodate(page);
	if (base_ni) {
		MFT_RECORD *m_tmp;

		m->base_mft_record = MK_LE_MREF(base_ni->mft_no,
				base_ni->seq_no);
		m_tmp = map_extent_mft_record(base_ni, bit, &ni);
		if (IS_ERR(m_tmp)) {
			ntfs_error(vol->sb, "Failed to map allocated extent "
					"mft record 0x%llx.", (long long)bit);
			err = PTR_ERR(m_tmp);
			
			m->flags &= cpu_to_le16(
					~le16_to_cpu(MFT_RECORD_IN_USE));
			flush_dcache_page(page);
			/* Make sure the mft record is written out to disk. */
			mark_ntfs_record_dirty(page, ofs);
			unlock_page(page);
			ntfs_unmap_page(page);
			goto undo_mftbmp_alloc;
		}
		BUG_ON(m != m_tmp);
		/*
		 * Make sure the allocated mft record is written out to disk.
		 * No need to set the inode dirty because the caller is going
		 * to do that anyway after finishing with the new extent mft
		 * record (e.g. at a minimum a new attribute will be added to
		 * the mft record.
		 */
		mark_ntfs_record_dirty(page, ofs);
		unlock_page(page);
		ntfs_unmap_page(page);
	} else {
		vi = new_inode(vol->sb);
		if (unlikely(!vi)) {
			err = -ENOMEM;
			
			m->flags &= cpu_to_le16(
					~le16_to_cpu(MFT_RECORD_IN_USE));
			flush_dcache_page(page);
			/* Make sure the mft record is written out to disk. */
			mark_ntfs_record_dirty(page, ofs);
			unlock_page(page);
			ntfs_unmap_page(page);
			goto undo_mftbmp_alloc;
		}
		vi->i_ino = bit;
		vi->i_version = 1;

		
		vi->i_uid = vol->uid;
		vi->i_gid = vol->gid;

		
		ntfs_init_big_inode(vi);
		ni = NTFS_I(vi);
		if (S_ISDIR(mode)) {
			vi->i_mode = S_IFDIR | S_IRWXUGO;
			vi->i_mode &= ~vol->dmask;

			NInoSetMstProtected(ni);
			ni->type = AT_INDEX_ALLOCATION;
			ni->name = I30;
			ni->name_len = 4;

			ni->itype.index.block_size = 4096;
			ni->itype.index.block_size_bits = ntfs_ffs(4096) - 1;
			ni->itype.index.collation_rule = COLLATION_FILE_NAME;
			if (vol->cluster_size <= ni->itype.index.block_size) {
				ni->itype.index.vcn_size = vol->cluster_size;
				ni->itype.index.vcn_size_bits =
						vol->cluster_size_bits;
			} else {
				ni->itype.index.vcn_size = vol->sector_size;
				ni->itype.index.vcn_size_bits =
						vol->sector_size_bits;
			}
		} else {
			vi->i_mode = S_IFREG | S_IRWXUGO;
			vi->i_mode &= ~vol->fmask;

			ni->type = AT_DATA;
			ni->name = NULL;
			ni->name_len = 0;
		}
		if (IS_RDONLY(vi))
			vi->i_mode &= ~S_IWUGO;

		
		vi->i_atime = vi->i_mtime = vi->i_ctime =
			current_fs_time(vi->i_sb);
		vi->i_size = 0;
		vi->i_blocks = 0;

		
		vi->i_generation = ni->seq_no = le16_to_cpu(m->sequence_number);
		atomic_inc(&ni->count);
		mutex_lock(&ni->mrec_lock);
		ni->page = page;
		ni->page_ofs = ofs;
		/*
		 * Make sure the allocated mft record is written out to disk.
		 * NOTE: We do not set the ntfs inode dirty because this would
		 * fail in ntfs_write_inode() because the inode does not have a
		 * standard information attribute yet.  Also, there is no need
		 * to set the inode dirty because the caller is going to do
		 * that anyway after finishing with the new mft record (e.g. at
		 * a minimum some new attributes will be added to the mft
		 * record.
		 */
		mark_ntfs_record_dirty(page, ofs);
		unlock_page(page);

		
		insert_inode_hash(vi);

		
		vol->mft_data_pos = bit + 1;
	}
	ntfs_debug("Returning opened, allocated %sinode 0x%llx.",
			base_ni ? "extent " : "", (long long)bit);
	*mrec = m;
	return ni;
undo_data_init:
	write_lock_irqsave(&mft_ni->size_lock, flags);
	mft_ni->initialized_size = old_data_initialized;
	i_size_write(vol->mft_ino, old_data_size);
	write_unlock_irqrestore(&mft_ni->size_lock, flags);
	goto undo_mftbmp_alloc_nolock;
undo_mftbmp_alloc:
	down_write(&vol->mftbmp_lock);
undo_mftbmp_alloc_nolock:
	if (ntfs_bitmap_clear_bit(vol->mftbmp_ino, bit)) {
		ntfs_error(vol->sb, "Failed to clear bit in mft bitmap.%s", es);
		NVolSetErrors(vol);
	}
	up_write(&vol->mftbmp_lock);
err_out:
	return ERR_PTR(err);
max_err_out:
	ntfs_warning(vol->sb, "Cannot allocate mft record because the maximum "
			"number of inodes (2^32) has already been reached.");
	up_write(&vol->mftbmp_lock);
	return ERR_PTR(-ENOSPC);
}

int ntfs_extent_mft_record_free(ntfs_inode *ni, MFT_RECORD *m)
{
	unsigned long mft_no = ni->mft_no;
	ntfs_volume *vol = ni->vol;
	ntfs_inode *base_ni;
	ntfs_inode **extent_nis;
	int i, err;
	le16 old_seq_no;
	u16 seq_no;
	
	BUG_ON(NInoAttr(ni));
	BUG_ON(ni->nr_extents != -1);

	mutex_lock(&ni->extent_lock);
	base_ni = ni->ext.base_ntfs_ino;
	mutex_unlock(&ni->extent_lock);

	BUG_ON(base_ni->nr_extents <= 0);

	ntfs_debug("Entering for extent inode 0x%lx, base inode 0x%lx.\n",
			mft_no, base_ni->mft_no);

	mutex_lock(&base_ni->extent_lock);

	
	if (atomic_read(&ni->count) > 2) {
		ntfs_error(vol->sb, "Tried to free busy extent inode 0x%lx, "
				"not freeing.", base_ni->mft_no);
		mutex_unlock(&base_ni->extent_lock);
		return -EBUSY;
	}

	
	extent_nis = base_ni->ext.extent_ntfs_inos;
	err = -ENOENT;
	for (i = 0; i < base_ni->nr_extents; i++) {
		if (ni != extent_nis[i])
			continue;
		extent_nis += i;
		base_ni->nr_extents--;
		memmove(extent_nis, extent_nis + 1, (base_ni->nr_extents - i) *
				sizeof(ntfs_inode*));
		err = 0;
		break;
	}

	mutex_unlock(&base_ni->extent_lock);

	if (unlikely(err)) {
		ntfs_error(vol->sb, "Extent inode 0x%lx is not attached to "
				"its base inode 0x%lx.", mft_no,
				base_ni->mft_no);
		BUG();
	}


	
	m->flags &= ~MFT_RECORD_IN_USE;

	
	old_seq_no = m->sequence_number;
	seq_no = le16_to_cpu(old_seq_no);
	if (seq_no == 0xffff)
		seq_no = 1;
	else if (seq_no)
		seq_no++;
	m->sequence_number = cpu_to_le16(seq_no);

	NInoSetDirty(ni);
	err = write_mft_record(ni, m, 0);
	if (unlikely(err)) {
		ntfs_error(vol->sb, "Failed to write mft record 0x%lx, not "
				"freeing.", mft_no);
		goto rollback;
	}
rollback_error:
	
	unmap_extent_mft_record(ni);
	ntfs_clear_extent_inode(ni);

	
	down_write(&vol->mftbmp_lock);
	err = ntfs_bitmap_clear_bit(vol->mftbmp_ino, mft_no);
	up_write(&vol->mftbmp_lock);
	if (unlikely(err)) {
		ntfs_error(vol->sb, "Failed to clear bit in mft bitmap.%s", es);
		NVolSetErrors(vol);
	}
	return 0;
rollback:
	
	mutex_lock(&base_ni->extent_lock);
	extent_nis = base_ni->ext.extent_ntfs_inos;
	if (!(base_ni->nr_extents & 3)) {
		int new_size = (base_ni->nr_extents + 4) * sizeof(ntfs_inode*);

		extent_nis = kmalloc(new_size, GFP_NOFS);
		if (unlikely(!extent_nis)) {
			ntfs_error(vol->sb, "Failed to allocate internal "
					"buffer during rollback.%s", es);
			mutex_unlock(&base_ni->extent_lock);
			NVolSetErrors(vol);
			goto rollback_error;
		}
		if (base_ni->nr_extents) {
			BUG_ON(!base_ni->ext.extent_ntfs_inos);
			memcpy(extent_nis, base_ni->ext.extent_ntfs_inos,
					new_size - 4 * sizeof(ntfs_inode*));
			kfree(base_ni->ext.extent_ntfs_inos);
		}
		base_ni->ext.extent_ntfs_inos = extent_nis;
	}
	m->flags |= MFT_RECORD_IN_USE;
	m->sequence_number = old_seq_no;
	extent_nis[base_ni->nr_extents++] = ni;
	mutex_unlock(&base_ni->extent_lock);
	mark_mft_record_dirty(ni);
	return err;
}
#endif 
