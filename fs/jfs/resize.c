/*
 *   Copyright (C) International Business Machines  Corp., 2000-2004
 *
 *   This program is free software;  you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY;  without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program;  if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/quotaops.h>
#include "jfs_incore.h"
#include "jfs_filsys.h"
#include "jfs_metapage.h"
#include "jfs_dinode.h"
#include "jfs_imap.h"
#include "jfs_dmap.h"
#include "jfs_superblock.h"
#include "jfs_txnmgr.h"
#include "jfs_debug.h"

#define BITSPERPAGE	(PSIZE << 3)
#define L2MEGABYTE	20
#define MEGABYTE	(1 << L2MEGABYTE)
#define MEGABYTE32	(MEGABYTE << 5)

#define BLKTODMAPN(b)\
	(((b) >> 13) + ((b) >> 23) + ((b) >> 33) + 3 + 1)

int jfs_extendfs(struct super_block *sb, s64 newLVSize, int newLogSize)
{
	int rc = 0;
	struct jfs_sb_info *sbi = JFS_SBI(sb);
	struct inode *ipbmap = sbi->ipbmap;
	struct inode *ipbmap2;
	struct inode *ipimap = sbi->ipimap;
	struct jfs_log *log = sbi->log;
	struct bmap *bmp = sbi->bmap;
	s64 newLogAddress, newFSCKAddress;
	int newFSCKSize;
	s64 newMapSize = 0, mapSize;
	s64 XAddress, XSize, nblocks, xoff, xaddr, t64;
	s64 oldLVSize;
	s64 newFSSize;
	s64 VolumeSize;
	int newNpages = 0, nPages, newPage, xlen, t32;
	int tid;
	int log_formatted = 0;
	struct inode *iplist[1];
	struct jfs_superblock *j_sb, *j_sb2;
	s64 old_agsize;
	int agsizechanged = 0;
	struct buffer_head *bh, *bh2;

	

	if (sbi->mntflag & JFS_INLINELOG)
		oldLVSize = addressPXD(&sbi->logpxd) + lengthPXD(&sbi->logpxd);
	else
		oldLVSize = addressPXD(&sbi->fsckpxd) +
		    lengthPXD(&sbi->fsckpxd);

	if (oldLVSize >= newLVSize) {
		printk(KERN_WARNING
		       "jfs_extendfs: volume hasn't grown, returning\n");
		goto out;
	}

	VolumeSize = sb->s_bdev->bd_inode->i_size >> sb->s_blocksize_bits;

	if (VolumeSize) {
		if (newLVSize > VolumeSize) {
			printk(KERN_WARNING "jfs_extendfs: invalid size\n");
			rc = -EINVAL;
			goto out;
		}
	} else {
		
		bh = sb_bread(sb, newLVSize - 1);
		if (!bh) {
			printk(KERN_WARNING "jfs_extendfs: invalid size\n");
			rc = -EINVAL;
			goto out;
		}
		bforget(bh);
	}

	

	if (isReadOnly(ipbmap)) {
		printk(KERN_WARNING "jfs_extendfs: read-only file system\n");
		rc = -EROFS;
		goto out;
	}


	if ((sbi->mntflag & JFS_INLINELOG)) {
		if (newLogSize == 0) {
			newLogSize = newLVSize >> 8;
			t32 = (1 << (20 - sbi->l2bsize)) - 1;
			newLogSize = (newLogSize + t32) & ~t32;
			newLogSize =
			    min(newLogSize, MEGABYTE32 >> sbi->l2bsize);
		} else {
			newLogSize = (newLogSize * MEGABYTE) >> sbi->l2bsize;
		}

	} else
		newLogSize = 0;

	newLogAddress = newLVSize - newLogSize;

	t64 = ((newLVSize - newLogSize + BPERDMAP - 1) >> L2BPERDMAP)
	    << L2BPERDMAP;
	t32 = DIV_ROUND_UP(t64, BITSPERPAGE) + 1 + 50;
	newFSCKSize = t32 << sbi->l2nbperpage;
	newFSCKAddress = newLogAddress - newFSCKSize;

	newFSSize = newLVSize - newLogSize - newFSCKSize;

	
	if (newFSSize < bmp->db_mapsize) {
		rc = -EINVAL;
		goto out;
	}

	if ((sbi->mntflag & JFS_INLINELOG) && (newLogAddress > oldLVSize)) {
		if ((rc = lmLogFormat(log, newLogAddress, newLogSize)))
			goto out;
		log_formatted = 1;
	}
	txQuiesce(sb);

	
	sbi->direct_inode->i_size =  sb->s_bdev->bd_inode->i_size;

	if (sbi->mntflag & JFS_INLINELOG) {
		lmLogShutdown(log);


		
		if ((rc = readSuper(sb, &bh)))
			goto error_out;
		j_sb = (struct jfs_superblock *)bh->b_data;

		
		j_sb->s_state |= cpu_to_le32(FM_EXTENDFS);
		j_sb->s_xsize = cpu_to_le64(newFSSize);
		PXDaddress(&j_sb->s_xfsckpxd, newFSCKAddress);
		PXDlength(&j_sb->s_xfsckpxd, newFSCKSize);
		PXDaddress(&j_sb->s_xlogpxd, newLogAddress);
		PXDlength(&j_sb->s_xlogpxd, newLogSize);

		
		mark_buffer_dirty(bh);
		sync_dirty_buffer(bh);
		brelse(bh);

		if (!log_formatted)
			if ((rc = lmLogFormat(log, newLogAddress, newLogSize)))
				goto error_out;

		log->base = newLogAddress;
		log->size = newLogSize >> (L2LOGPSIZE - sb->s_blocksize_bits);
		if ((rc = lmLogInit(log)))
			goto error_out;
	}

	newMapSize = newFSSize;
	t64 = (newMapSize - 1) + BPERDMAP;
	newNpages = BLKTODMAPN(t64) + 1;

      extendBmap:
	
	mapSize = bmp->db_mapsize;
	XAddress = mapSize;	
	XSize = newMapSize - mapSize;	
	old_agsize = bmp->db_agsize;	

	
	t64 = dbMapFileSizeToMapSize(ipbmap);
	if (mapSize > t64) {
		printk(KERN_ERR "jfs_extendfs: mapSize (0x%Lx) > t64 (0x%Lx)\n",
		       (long long) mapSize, (long long) t64);
		rc = -EIO;
		goto error_out;
	}
	nblocks = min(t64 - mapSize, XSize);

	if ((rc = dbExtendFS(ipbmap, XAddress, nblocks)))
		goto error_out;

	agsizechanged |= (bmp->db_agsize != old_agsize);

	
	XSize -= nblocks;

	
	nPages = ipbmap->i_size >> L2PSIZE;

	
	if (nPages == newNpages)
		goto finalizeBmap;

	/* synchronous write of data pages: bmap data pages are
	 * cached in meta-data cache, and not written out
	 * by txCommit();
	 */
	filemap_fdatawait(ipbmap->i_mapping);
	filemap_write_and_wait(ipbmap->i_mapping);
	diWriteSpecial(ipbmap, 0);

	newPage = nPages;	
	xoff = newPage << sbi->l2nbperpage;
	xlen = (newNpages - nPages) << sbi->l2nbperpage;
	xlen = min(xlen, (int) nblocks) & ~(sbi->nbperpage - 1);
	xaddr = XAddress;

	tid = txBegin(sb, COMMIT_FORCE);

	if ((rc = xtAppend(tid, ipbmap, 0, xoff, nblocks, &xlen, &xaddr, 0))) {
		txEnd(tid);
		goto error_out;
	}
	
	ipbmap->i_size += xlen << sbi->l2bsize;
	inode_add_bytes(ipbmap, xlen << sbi->l2bsize);

	iplist[0] = ipbmap;
	rc = txCommit(tid, 1, &iplist[0], COMMIT_FORCE);

	txEnd(tid);

	if (rc)
		goto error_out;

	
	if (XSize)
		goto extendBmap;

      finalizeBmap:
	
	dbFinalizeBmap(ipbmap);

	
	if (agsizechanged) {
		if ((rc = diExtendFS(ipimap, ipbmap)))
			goto error_out;

		
		if ((rc = diSync(ipimap)))
			goto error_out;
	}


	
	

	if ((rc = dbSync(ipbmap)))
		goto error_out;


	ipbmap2 = diReadSpecial(sb, BMAP_I, 1);
	if (ipbmap2 == NULL) {
		printk(KERN_ERR "jfs_extendfs: diReadSpecial(bmap) failed\n");
		goto error_out;
	}
	memcpy(&JFS_IP(ipbmap2)->i_xtroot, &JFS_IP(ipbmap)->i_xtroot, 288);
	ipbmap2->i_size = ipbmap->i_size;
	ipbmap2->i_blocks = ipbmap->i_blocks;

	diWriteSpecial(ipbmap2, 1);
	diFreeSpecial(ipbmap2);

	if ((rc = readSuper(sb, &bh)))
		goto error_out;
	j_sb = (struct jfs_superblock *)bh->b_data;

	
	j_sb->s_state &= cpu_to_le32(~FM_EXTENDFS);
	j_sb->s_size = cpu_to_le64(bmp->db_mapsize <<
				   le16_to_cpu(j_sb->s_l2bfactor));
	j_sb->s_agsize = cpu_to_le32(bmp->db_agsize);

	
	if (sbi->mntflag & JFS_INLINELOG) {
		PXDaddress(&(j_sb->s_logpxd), newLogAddress);
		PXDlength(&(j_sb->s_logpxd), newLogSize);
	}

	
	j_sb->s_logserial = cpu_to_le32(log->serial);

	
	PXDaddress(&(j_sb->s_fsckpxd), newFSCKAddress);
	PXDlength(&(j_sb->s_fsckpxd), newFSCKSize);
	j_sb->s_fscklog = 1;
	

	
	bh2 = sb_bread(sb, SUPER2_OFF >> sb->s_blocksize_bits);
	if (bh2) {
		j_sb2 = (struct jfs_superblock *)bh2->b_data;
		memcpy(j_sb2, j_sb, sizeof (struct jfs_superblock));

		mark_buffer_dirty(bh);
		sync_dirty_buffer(bh2);
		brelse(bh2);
	}

	
	mark_buffer_dirty(bh);
	sync_dirty_buffer(bh);
	brelse(bh);

	goto resume;

      error_out:
	jfs_error(sb, "jfs_extendfs");

      resume:
	txResume(sb);

      out:
	return rc;
}
