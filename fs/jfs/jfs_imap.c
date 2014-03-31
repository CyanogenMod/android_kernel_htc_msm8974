/*
 *   Copyright (C) International Business Machines Corp., 2000-2004
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

/*
 *	jfs_imap.c: inode allocation map manager
 *
 * Serialization:
 *   Each AG has a simple lock which is used to control the serialization of
 *	the AG level lists.  This lock should be taken first whenever an AG
 *	level list will be modified or accessed.
 *
 *   Each IAG is locked by obtaining the buffer for the IAG page.
 *
 *   There is also a inode lock for the inode map inode.  A read lock needs to
 *	be taken whenever an IAG is read from the map or the global level
 *	information is read.  A write lock needs to be taken whenever the global
 *	level information is modified or an atomic operation needs to be used.
 *
 *	If more than one IAG is read at one time, the read lock may not
 *	be given up until all of the IAG's are read.  Otherwise, a deadlock
 *	may occur when trying to obtain the read lock while another thread
 *	holding the read lock is waiting on the IAG already being held.
 *
 *   The control page of the inode map is read into memory by diMount().
 *	Thereafter it should only be modified in memory and then it will be
 *	written out when the filesystem is unmounted by diUnmount().
 */

#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/pagemap.h>
#include <linux/quotaops.h>
#include <linux/slab.h>

#include "jfs_incore.h"
#include "jfs_inode.h"
#include "jfs_filsys.h"
#include "jfs_dinode.h"
#include "jfs_dmap.h"
#include "jfs_imap.h"
#include "jfs_metapage.h"
#include "jfs_superblock.h"
#include "jfs_debug.h"

#define IAGFREE_LOCK_INIT(imap)		mutex_init(&imap->im_freelock)
#define IAGFREE_LOCK(imap)		mutex_lock(&imap->im_freelock)
#define IAGFREE_UNLOCK(imap)		mutex_unlock(&imap->im_freelock)

#define AG_LOCK_INIT(imap,index)	mutex_init(&(imap->im_aglock[index]))
#define AG_LOCK(imap,agno)		mutex_lock(&imap->im_aglock[agno])
#define AG_UNLOCK(imap,agno)		mutex_unlock(&imap->im_aglock[agno])

static int diAllocAG(struct inomap *, int, bool, struct inode *);
static int diAllocAny(struct inomap *, int, bool, struct inode *);
static int diAllocBit(struct inomap *, struct iag *, int);
static int diAllocExt(struct inomap *, int, struct inode *);
static int diAllocIno(struct inomap *, int, struct inode *);
static int diFindFree(u32, int);
static int diNewExt(struct inomap *, struct iag *, int);
static int diNewIAG(struct inomap *, int *, int, struct metapage **);
static void duplicateIXtree(struct super_block *, s64, int, s64 *);

static int diIAGRead(struct inomap * imap, int, struct metapage **);
static int copy_from_dinode(struct dinode *, struct inode *);
static void copy_to_dinode(struct dinode *, struct inode *);

int diMount(struct inode *ipimap)
{
	struct inomap *imap;
	struct metapage *mp;
	int index;
	struct dinomap_disk *dinom_le;

	
	imap = kmalloc(sizeof(struct inomap), GFP_KERNEL);
	if (imap == NULL) {
		jfs_err("diMount: kmalloc returned NULL!");
		return -ENOMEM;
	}

	

	mp = read_metapage(ipimap,
			   IMAPBLKNO << JFS_SBI(ipimap->i_sb)->l2nbperpage,
			   PSIZE, 0);
	if (mp == NULL) {
		kfree(imap);
		return -EIO;
	}

	
	dinom_le = (struct dinomap_disk *) mp->data;
	imap->im_freeiag = le32_to_cpu(dinom_le->in_freeiag);
	imap->im_nextiag = le32_to_cpu(dinom_le->in_nextiag);
	atomic_set(&imap->im_numinos, le32_to_cpu(dinom_le->in_numinos));
	atomic_set(&imap->im_numfree, le32_to_cpu(dinom_le->in_numfree));
	imap->im_nbperiext = le32_to_cpu(dinom_le->in_nbperiext);
	imap->im_l2nbperiext = le32_to_cpu(dinom_le->in_l2nbperiext);
	for (index = 0; index < MAXAG; index++) {
		imap->im_agctl[index].inofree =
		    le32_to_cpu(dinom_le->in_agctl[index].inofree);
		imap->im_agctl[index].extfree =
		    le32_to_cpu(dinom_le->in_agctl[index].extfree);
		imap->im_agctl[index].numinos =
		    le32_to_cpu(dinom_le->in_agctl[index].numinos);
		imap->im_agctl[index].numfree =
		    le32_to_cpu(dinom_le->in_agctl[index].numfree);
	}

	
	release_metapage(mp);

	
	IAGFREE_LOCK_INIT(imap);

	
	for (index = 0; index < MAXAG; index++) {
		AG_LOCK_INIT(imap, index);
	}

	imap->im_ipimap = ipimap;
	JFS_IP(ipimap)->i_imap = imap;

	return (0);
}


int diUnmount(struct inode *ipimap, int mounterror)
{
	struct inomap *imap = JFS_IP(ipimap)->i_imap;


	if (!(mounterror || isReadOnly(ipimap)))
		diSync(ipimap);

	truncate_inode_pages(ipimap->i_mapping, 0);

	kfree(imap);

	return (0);
}


int diSync(struct inode *ipimap)
{
	struct dinomap_disk *dinom_le;
	struct inomap *imp = JFS_IP(ipimap)->i_imap;
	struct metapage *mp;
	int index;

	
	mp = get_metapage(ipimap,
			  IMAPBLKNO << JFS_SBI(ipimap->i_sb)->l2nbperpage,
			  PSIZE, 0);
	if (mp == NULL) {
		jfs_err("diSync: get_metapage failed!");
		return -EIO;
	}

	
	dinom_le = (struct dinomap_disk *) mp->data;
	dinom_le->in_freeiag = cpu_to_le32(imp->im_freeiag);
	dinom_le->in_nextiag = cpu_to_le32(imp->im_nextiag);
	dinom_le->in_numinos = cpu_to_le32(atomic_read(&imp->im_numinos));
	dinom_le->in_numfree = cpu_to_le32(atomic_read(&imp->im_numfree));
	dinom_le->in_nbperiext = cpu_to_le32(imp->im_nbperiext);
	dinom_le->in_l2nbperiext = cpu_to_le32(imp->im_l2nbperiext);
	for (index = 0; index < MAXAG; index++) {
		dinom_le->in_agctl[index].inofree =
		    cpu_to_le32(imp->im_agctl[index].inofree);
		dinom_le->in_agctl[index].extfree =
		    cpu_to_le32(imp->im_agctl[index].extfree);
		dinom_le->in_agctl[index].numinos =
		    cpu_to_le32(imp->im_agctl[index].numinos);
		dinom_le->in_agctl[index].numfree =
		    cpu_to_le32(imp->im_agctl[index].numfree);
	}

	
	write_metapage(mp);

	filemap_write_and_wait(ipimap->i_mapping);

	diWriteSpecial(ipimap, 0);

	return (0);
}


int diRead(struct inode *ip)
{
	struct jfs_sb_info *sbi = JFS_SBI(ip->i_sb);
	int iagno, ino, extno, rc;
	struct inode *ipimap;
	struct dinode *dp;
	struct iag *iagp;
	struct metapage *mp;
	s64 blkno, agstart;
	struct inomap *imap;
	int block_offset;
	int inodes_left;
	unsigned long pageno;
	int rel_inode;

	jfs_info("diRead: ino = %ld", ip->i_ino);

	ipimap = sbi->ipimap;
	JFS_IP(ip)->ipimap = ipimap;

	
	iagno = INOTOIAG(ip->i_ino);

	
	imap = JFS_IP(ipimap)->i_imap;
	IREAD_LOCK(ipimap, RDWRLOCK_IMAP);
	rc = diIAGRead(imap, iagno, &mp);
	IREAD_UNLOCK(ipimap);
	if (rc) {
		jfs_err("diRead: diIAGRead returned %d", rc);
		return (rc);
	}

	iagp = (struct iag *) mp->data;

	
	ino = ip->i_ino & (INOSPERIAG - 1);
	extno = ino >> L2INOSPEREXT;

	if ((lengthPXD(&iagp->inoext[extno]) != imap->im_nbperiext) ||
	    (addressPXD(&iagp->inoext[extno]) == 0)) {
		release_metapage(mp);
		return -ESTALE;
	}

	blkno = INOPBLK(&iagp->inoext[extno], ino, sbi->l2nbperpage);

	
	agstart = le64_to_cpu(iagp->agstart);

	release_metapage(mp);

	rel_inode = (ino & (INOSPERPAGE - 1));
	pageno = blkno >> sbi->l2nbperpage;

	if ((block_offset = ((u32) blkno & (sbi->nbperpage - 1)))) {
		inodes_left =
		     (sbi->nbperpage - block_offset) << sbi->l2niperblk;

		if (rel_inode < inodes_left)
			rel_inode += block_offset << sbi->l2niperblk;
		else {
			pageno += 1;
			rel_inode -= inodes_left;
		}
	}

	
	mp = read_metapage(ipimap, pageno << sbi->l2nbperpage, PSIZE, 1);
	if (!mp) {
		jfs_err("diRead: read_metapage failed");
		return -EIO;
	}

	
	dp = (struct dinode *) mp->data;
	dp += rel_inode;

	if (ip->i_ino != le32_to_cpu(dp->di_number)) {
		jfs_error(ip->i_sb, "diRead: i_ino != di_number");
		rc = -EIO;
	} else if (le32_to_cpu(dp->di_nlink) == 0)
		rc = -ESTALE;
	else
		
		rc = copy_from_dinode(dp, ip);

	release_metapage(mp);

	
	JFS_IP(ip)->agstart = agstart;
	JFS_IP(ip)->active_ag = -1;

	return (rc);
}


struct inode *diReadSpecial(struct super_block *sb, ino_t inum, int secondary)
{
	struct jfs_sb_info *sbi = JFS_SBI(sb);
	uint address;
	struct dinode *dp;
	struct inode *ip;
	struct metapage *mp;

	ip = new_inode(sb);
	if (ip == NULL) {
		jfs_err("diReadSpecial: new_inode returned NULL!");
		return ip;
	}

	if (secondary) {
		address = addressPXD(&sbi->ait2) >> sbi->l2nbperpage;
		JFS_IP(ip)->ipimap = sbi->ipaimap2;
	} else {
		address = AITBL_OFF >> L2PSIZE;
		JFS_IP(ip)->ipimap = sbi->ipaimap;
	}

	ASSERT(inum < INOSPEREXT);

	ip->i_ino = inum;

	address += inum >> 3;	

	
	mp = read_metapage(ip, address << sbi->l2nbperpage, PSIZE, 1);
	if (mp == NULL) {
		set_nlink(ip, 1);	
		iput(ip);
		return (NULL);
	}

	
	dp = (struct dinode *) (mp->data);
	dp += inum % 8;		

	
	if ((copy_from_dinode(dp, ip)) != 0) {
		
		set_nlink(ip, 1);	
		iput(ip);
		
		release_metapage(mp);
		return (NULL);

	}

	ip->i_mapping->a_ops = &jfs_metapage_aops;
	mapping_set_gfp_mask(ip->i_mapping, GFP_NOFS);

	
	ip->i_flags |= S_NOQUOTA;

	if ((inum == FILESYSTEM_I) && (JFS_IP(ip)->ipimap == sbi->ipaimap)) {
		sbi->gengen = le32_to_cpu(dp->di_gengen);
		sbi->inostamp = le32_to_cpu(dp->di_inostamp);
	}

	
	release_metapage(mp);

	hlist_add_fake(&ip->i_hash);

	return (ip);
}


void diWriteSpecial(struct inode *ip, int secondary)
{
	struct jfs_sb_info *sbi = JFS_SBI(ip->i_sb);
	uint address;
	struct dinode *dp;
	ino_t inum = ip->i_ino;
	struct metapage *mp;

	if (secondary)
		address = addressPXD(&sbi->ait2) >> sbi->l2nbperpage;
	else
		address = AITBL_OFF >> L2PSIZE;

	ASSERT(inum < INOSPEREXT);

	address += inum >> 3;	

	
	mp = read_metapage(ip, address << sbi->l2nbperpage, PSIZE, 1);
	if (mp == NULL) {
		jfs_err("diWriteSpecial: failed to read aggregate inode "
			"extent!");
		return;
	}

	
	dp = (struct dinode *) (mp->data);
	dp += inum % 8;		

	
	copy_to_dinode(dp, ip);
	memcpy(&dp->di_xtroot, &JFS_IP(ip)->i_xtroot, 288);

	if (inum == FILESYSTEM_I)
		dp->di_gengen = cpu_to_le32(sbi->gengen);

	
	write_metapage(mp);
}

void diFreeSpecial(struct inode *ip)
{
	if (ip == NULL) {
		jfs_err("diFreeSpecial called with NULL ip!");
		return;
	}
	filemap_write_and_wait(ip->i_mapping);
	truncate_inode_pages(ip->i_mapping, 0);
	iput(ip);
}



/*
 * NAME:	diWrite()
 *
 * FUNCTION:	write the on-disk inode portion of the in-memory inode
 *		to its corresponding on-disk inode.
 *
 *		on entry, the specifed incore inode should itself
 *		specify the disk inode number corresponding to the
 *		incore inode (i.e. i_number should be initialized).
 *
 *		the inode contains the inode extent address for the disk
 *		inode.  with the inode extent address in hand, the
 *		page of the extent that contains the disk inode is
 *		read and the disk inode portion of the incore inode
 *		is copied to the disk inode.
 *
 * PARAMETERS:
 *	tid -  transacation id
 *	ip  -  pointer to incore inode to be written to the inode extent.
 *
 * RETURN VALUES:
 *	0	- success
 *	-EIO	- i/o error.
 */
int diWrite(tid_t tid, struct inode *ip)
{
	struct jfs_sb_info *sbi = JFS_SBI(ip->i_sb);
	struct jfs_inode_info *jfs_ip = JFS_IP(ip);
	int rc = 0;
	s32 ino;
	struct dinode *dp;
	s64 blkno;
	int block_offset;
	int inodes_left;
	struct metapage *mp;
	unsigned long pageno;
	int rel_inode;
	int dioffset;
	struct inode *ipimap;
	uint type;
	lid_t lid;
	struct tlock *ditlck, *tlck;
	struct linelock *dilinelock, *ilinelock;
	struct lv *lv;
	int n;

	ipimap = jfs_ip->ipimap;

	ino = ip->i_ino & (INOSPERIAG - 1);

	if (!addressPXD(&(jfs_ip->ixpxd)) ||
	    (lengthPXD(&(jfs_ip->ixpxd)) !=
	     JFS_IP(ipimap)->i_imap->im_nbperiext)) {
		jfs_error(ip->i_sb, "diWrite: ixpxd invalid");
		return -EIO;
	}

	
	blkno = INOPBLK(&(jfs_ip->ixpxd), ino, sbi->l2nbperpage);

	rel_inode = (ino & (INOSPERPAGE - 1));
	pageno = blkno >> sbi->l2nbperpage;

	if ((block_offset = ((u32) blkno & (sbi->nbperpage - 1)))) {
		inodes_left =
		    (sbi->nbperpage - block_offset) << sbi->l2niperblk;

		if (rel_inode < inodes_left)
			rel_inode += block_offset << sbi->l2niperblk;
		else {
			pageno += 1;
			rel_inode -= inodes_left;
		}
	}
	
      retry:
	mp = read_metapage(ipimap, pageno << sbi->l2nbperpage, PSIZE, 1);
	if (!mp)
		return -EIO;

	
	dp = (struct dinode *) mp->data;
	dp += rel_inode;

	dioffset = (ino & (INOSPERPAGE - 1)) << L2DISIZE;

	if ((ditlck =
	     txLock(tid, ipimap, mp, tlckINODE | tlckENTRY)) == NULL)
		goto retry;
	dilinelock = (struct linelock *) & ditlck->lock;


	if (S_ISDIR(ip->i_mode) && (lid = jfs_ip->xtlid)) {
		xtpage_t *p, *xp;
		xad_t *xad;

		jfs_ip->xtlid = 0;
		tlck = lid_to_tlock(lid);
		assert(tlck->type & tlckXTREE);
		tlck->type |= tlckBTROOT;
		tlck->mp = mp;
		ilinelock = (struct linelock *) & tlck->lock;

		p = &jfs_ip->i_xtroot;
		xp = (xtpage_t *) &dp->di_dirtable;
		lv = ilinelock->lv;
		for (n = 0; n < ilinelock->index; n++, lv++) {
			memcpy(&xp->xad[lv->offset], &p->xad[lv->offset],
			       lv->length << L2XTSLOTSIZE);
		}

		
		xad = &xp->xad[XTENTRYSTART];
		for (n = XTENTRYSTART;
		     n < le16_to_cpu(xp->header.nextindex); n++, xad++)
			if (xad->flag & (XAD_NEW | XAD_EXTENDED))
				xad->flag &= ~(XAD_NEW | XAD_EXTENDED);
	}

	if ((lid = jfs_ip->blid) == 0)
		goto inlineData;
	jfs_ip->blid = 0;

	tlck = lid_to_tlock(lid);
	type = tlck->type;
	tlck->type |= tlckBTROOT;
	tlck->mp = mp;
	ilinelock = (struct linelock *) & tlck->lock;

	if (type & tlckXTREE) {
		xtpage_t *p, *xp;
		xad_t *xad;

		p = &jfs_ip->i_xtroot;
		xp = &dp->di_xtroot;
		lv = ilinelock->lv;
		for (n = 0; n < ilinelock->index; n++, lv++) {
			memcpy(&xp->xad[lv->offset], &p->xad[lv->offset],
			       lv->length << L2XTSLOTSIZE);
		}

		
		xad = &xp->xad[XTENTRYSTART];
		for (n = XTENTRYSTART;
		     n < le16_to_cpu(xp->header.nextindex); n++, xad++)
			if (xad->flag & (XAD_NEW | XAD_EXTENDED))
				xad->flag &= ~(XAD_NEW | XAD_EXTENDED);
	}
	else if (type & tlckDTREE) {
		dtpage_t *p, *xp;

		p = (dtpage_t *) &jfs_ip->i_dtroot;
		xp = (dtpage_t *) & dp->di_dtroot;
		lv = ilinelock->lv;
		for (n = 0; n < ilinelock->index; n++, lv++) {
			memcpy(&xp->slot[lv->offset], &p->slot[lv->offset],
			       lv->length << L2DTSLOTSIZE);
		}
	} else {
		jfs_err("diWrite: UFO tlock");
	}

      inlineData:
	if (S_ISLNK(ip->i_mode) && ip->i_size < IDATASIZE) {
		lv = & dilinelock->lv[dilinelock->index];
		lv->offset = (dioffset + 2 * 128) >> L2INODESLOTSIZE;
		lv->length = 2;
		memcpy(&dp->di_fastsymlink, jfs_ip->i_inline, IDATASIZE);
		dilinelock->index++;
	}
	if (test_cflag(COMMIT_Inlineea, ip)) {
		lv = & dilinelock->lv[dilinelock->index];
		lv->offset = (dioffset + 3 * 128) >> L2INODESLOTSIZE;
		lv->length = 1;
		memcpy(&dp->di_inlineea, jfs_ip->i_inline_ea, INODESLOTSIZE);
		dilinelock->index++;

		clear_cflag(COMMIT_Inlineea, ip);
	}

	lv = & dilinelock->lv[dilinelock->index];
	lv->offset = dioffset >> L2INODESLOTSIZE;
	copy_to_dinode(dp, ip);
	if (test_and_clear_cflag(COMMIT_Dirtable, ip)) {
		lv->length = 2;
		memcpy(&dp->di_dirtable, &jfs_ip->i_dirtable, 96);
	} else
		lv->length = 1;
	dilinelock->index++;

	/* release the buffer holding the updated on-disk inode.
	 * the buffer will be later written by commit processing.
	 */
	write_metapage(mp);

	return (rc);
}


int diFree(struct inode *ip)
{
	int rc;
	ino_t inum = ip->i_ino;
	struct iag *iagp, *aiagp, *biagp, *ciagp, *diagp;
	struct metapage *mp, *amp, *bmp, *cmp, *dmp;
	int iagno, ino, extno, bitno, sword, agno;
	int back, fwd;
	u32 bitmap, mask;
	struct inode *ipimap = JFS_SBI(ip->i_sb)->ipimap;
	struct inomap *imap = JFS_IP(ipimap)->i_imap;
	pxd_t freepxd;
	tid_t tid;
	struct inode *iplist[3];
	struct tlock *tlck;
	struct pxd_lock *pxdlock;

	aiagp = biagp = ciagp = diagp = NULL;

	iagno = INOTOIAG(inum);

	if (iagno >= imap->im_nextiag) {
		print_hex_dump(KERN_ERR, "imap: ", DUMP_PREFIX_ADDRESS, 16, 4,
			       imap, 32, 0);
		jfs_error(ip->i_sb,
			  "diFree: inum = %d, iagno = %d, nextiag = %d",
			  (uint) inum, iagno, imap->im_nextiag);
		return -EIO;
	}

	agno = BLKTOAG(JFS_IP(ip)->agstart, JFS_SBI(ip->i_sb));

	AG_LOCK(imap, agno);

	IREAD_LOCK(ipimap, RDWRLOCK_IMAP);

	if ((rc = diIAGRead(imap, iagno, &mp))) {
		IREAD_UNLOCK(ipimap);
		AG_UNLOCK(imap, agno);
		return (rc);
	}
	iagp = (struct iag *) mp->data;

	ino = inum & (INOSPERIAG - 1);
	extno = ino >> L2INOSPEREXT;
	bitno = ino & (INOSPEREXT - 1);
	mask = HIGHORDER >> bitno;

	if (!(le32_to_cpu(iagp->wmap[extno]) & mask)) {
		jfs_error(ip->i_sb,
			  "diFree: wmap shows inode already free");
	}

	if (!addressPXD(&iagp->inoext[extno])) {
		release_metapage(mp);
		IREAD_UNLOCK(ipimap);
		AG_UNLOCK(imap, agno);
		jfs_error(ip->i_sb, "diFree: invalid inoext");
		return -EIO;
	}

	bitmap = le32_to_cpu(iagp->wmap[extno]) & ~mask;

	if (imap->im_agctl[agno].numfree > imap->im_agctl[agno].numinos) {
		release_metapage(mp);
		IREAD_UNLOCK(ipimap);
		AG_UNLOCK(imap, agno);
		jfs_error(ip->i_sb, "diFree: numfree > numinos");
		return -EIO;
	}
	if (bitmap ||
	    imap->im_agctl[agno].numfree < 96 ||
	    (imap->im_agctl[agno].numfree < 288 &&
	     (((imap->im_agctl[agno].numfree * 100) /
	       imap->im_agctl[agno].numinos) <= 25))) {
		if (iagp->nfreeinos == 0) {
			if ((fwd = imap->im_agctl[agno].inofree) >= 0) {
				if ((rc = diIAGRead(imap, fwd, &amp))) {
					IREAD_UNLOCK(ipimap);
					AG_UNLOCK(imap, agno);
					release_metapage(mp);
					return (rc);
				}
				aiagp = (struct iag *) amp->data;

				aiagp->inofreeback = cpu_to_le32(iagno);

				write_metapage(amp);
			}

			iagp->inofreefwd =
			    cpu_to_le32(imap->im_agctl[agno].inofree);
			iagp->inofreeback = cpu_to_le32(-1);
			imap->im_agctl[agno].inofree = iagno;
		}
		IREAD_UNLOCK(ipimap);

		if (iagp->wmap[extno] == cpu_to_le32(ONES)) {
			sword = extno >> L2EXTSPERSUM;
			bitno = extno & (EXTSPERSUM - 1);
			iagp->inosmap[sword] &=
			    cpu_to_le32(~(HIGHORDER >> bitno));
		}

		iagp->wmap[extno] = cpu_to_le32(bitmap);

		le32_add_cpu(&iagp->nfreeinos, 1);
		imap->im_agctl[agno].numfree += 1;
		atomic_inc(&imap->im_numfree);

		AG_UNLOCK(imap, agno);

		
		write_metapage(mp);

		return (0);
	}



	amp = bmp = cmp = dmp = NULL;
	fwd = back = -1;

	if (iagp->nfreeexts == 0) {
		if ((fwd = imap->im_agctl[agno].extfree) >= 0) {
			if ((rc = diIAGRead(imap, fwd, &amp)))
				goto error_out;
			aiagp = (struct iag *) amp->data;
		}
	} else {
		if (iagp->nfreeexts == cpu_to_le32(EXTSPERIAG - 1)) {
			if ((fwd = le32_to_cpu(iagp->extfreefwd)) >= 0) {
				if ((rc = diIAGRead(imap, fwd, &amp)))
					goto error_out;
				aiagp = (struct iag *) amp->data;
			}

			if ((back = le32_to_cpu(iagp->extfreeback)) >= 0) {
				if ((rc = diIAGRead(imap, back, &bmp)))
					goto error_out;
				biagp = (struct iag *) bmp->data;
			}
		}
	}

	if (iagp->nfreeinos == cpu_to_le32(INOSPEREXT - 1)) {
		int inofreeback = le32_to_cpu(iagp->inofreeback);
		int inofreefwd = le32_to_cpu(iagp->inofreefwd);

		if (inofreefwd >= 0) {

			if (inofreefwd == fwd)
				ciagp = (struct iag *) amp->data;
			else if (inofreefwd == back)
				ciagp = (struct iag *) bmp->data;
			else {
				if ((rc =
				     diIAGRead(imap, inofreefwd, &cmp)))
					goto error_out;
				ciagp = (struct iag *) cmp->data;
			}
			assert(ciagp != NULL);
		}

		if (inofreeback >= 0) {
			if (inofreeback == fwd)
				diagp = (struct iag *) amp->data;
			else if (inofreeback == back)
				diagp = (struct iag *) bmp->data;
			else {
				if ((rc =
				     diIAGRead(imap, inofreeback, &dmp)))
					goto error_out;
				diagp = (struct iag *) dmp->data;
			}
			assert(diagp != NULL);
		}
	}

	IREAD_UNLOCK(ipimap);

	freepxd = iagp->inoext[extno];
	invalidate_pxd_metapages(ip, freepxd);

	if (iagp->nfreeexts == 0) {
		if (fwd >= 0)
			aiagp->extfreeback = cpu_to_le32(iagno);

		iagp->extfreefwd =
		    cpu_to_le32(imap->im_agctl[agno].extfree);
		iagp->extfreeback = cpu_to_le32(-1);
		imap->im_agctl[agno].extfree = iagno;
	} else {
		if (iagp->nfreeexts == cpu_to_le32(EXTSPERIAG - 1)) {
			if (fwd >= 0)
				aiagp->extfreeback = iagp->extfreeback;

			if (back >= 0)
				biagp->extfreefwd = iagp->extfreefwd;
			else
				imap->im_agctl[agno].extfree =
				    le32_to_cpu(iagp->extfreefwd);

			iagp->extfreefwd = iagp->extfreeback = cpu_to_le32(-1);

			IAGFREE_LOCK(imap);
			iagp->iagfree = cpu_to_le32(imap->im_freeiag);
			imap->im_freeiag = iagno;
			IAGFREE_UNLOCK(imap);
		}
	}

	if (iagp->nfreeinos == cpu_to_le32(INOSPEREXT - 1)) {
		if ((int) le32_to_cpu(iagp->inofreefwd) >= 0)
			ciagp->inofreeback = iagp->inofreeback;

		if ((int) le32_to_cpu(iagp->inofreeback) >= 0)
			diagp->inofreefwd = iagp->inofreefwd;
		else
			imap->im_agctl[agno].inofree =
			    le32_to_cpu(iagp->inofreefwd);

		iagp->inofreefwd = iagp->inofreeback = cpu_to_le32(-1);
	}

	if (iagp->pmap[extno] != 0) {
		jfs_error(ip->i_sb, "diFree: the pmap does not show inode free");
	}
	iagp->wmap[extno] = 0;
	PXDlength(&iagp->inoext[extno], 0);
	PXDaddress(&iagp->inoext[extno], 0);

	sword = extno >> L2EXTSPERSUM;
	bitno = extno & (EXTSPERSUM - 1);
	mask = HIGHORDER >> bitno;
	iagp->inosmap[sword] |= cpu_to_le32(mask);
	iagp->extsmap[sword] &= cpu_to_le32(~mask);

	le32_add_cpu(&iagp->nfreeinos, -(INOSPEREXT - 1));
	le32_add_cpu(&iagp->nfreeexts, 1);

	imap->im_agctl[agno].numfree -= (INOSPEREXT - 1);
	imap->im_agctl[agno].numinos -= INOSPEREXT;
	atomic_sub(INOSPEREXT - 1, &imap->im_numfree);
	atomic_sub(INOSPEREXT, &imap->im_numinos);

	if (amp)
		write_metapage(amp);
	if (bmp)
		write_metapage(bmp);
	if (cmp)
		write_metapage(cmp);
	if (dmp)
		write_metapage(dmp);

	tid = txBegin(ipimap->i_sb, COMMIT_FORCE);
	mutex_lock(&JFS_IP(ipimap)->commit_mutex);

	tlck = txLock(tid, ipimap, mp, tlckINODE | tlckFREE);
	pxdlock = (struct pxd_lock *) & tlck->lock;
	pxdlock->flag = mlckFREEPXD;
	pxdlock->pxd = freepxd;
	pxdlock->index = 1;

	write_metapage(mp);

	iplist[0] = ipimap;

	iplist[1] = (struct inode *) (size_t)iagno;
	iplist[2] = (struct inode *) (size_t)extno;

	rc = txCommit(tid, 1, &iplist[0], COMMIT_FORCE);

	txEnd(tid);
	mutex_unlock(&JFS_IP(ipimap)->commit_mutex);

	
	AG_UNLOCK(imap, agno);

	return (0);

      error_out:
	IREAD_UNLOCK(ipimap);

	if (amp)
		release_metapage(amp);
	if (bmp)
		release_metapage(bmp);
	if (cmp)
		release_metapage(cmp);
	if (dmp)
		release_metapage(dmp);

	AG_UNLOCK(imap, agno);

	release_metapage(mp);

	return (rc);
}

static inline void
diInitInode(struct inode *ip, int iagno, int ino, int extno, struct iag * iagp)
{
	struct jfs_inode_info *jfs_ip = JFS_IP(ip);

	ip->i_ino = (iagno << L2INOSPERIAG) + ino;
	jfs_ip->ixpxd = iagp->inoext[extno];
	jfs_ip->agstart = le64_to_cpu(iagp->agstart);
	jfs_ip->active_ag = -1;
}


int diAlloc(struct inode *pip, bool dir, struct inode *ip)
{
	int rc, ino, iagno, addext, extno, bitno, sword;
	int nwords, rem, i, agno;
	u32 mask, inosmap, extsmap;
	struct inode *ipimap;
	struct metapage *mp;
	ino_t inum;
	struct iag *iagp;
	struct inomap *imap;

	ipimap = JFS_SBI(pip->i_sb)->ipimap;
	imap = JFS_IP(ipimap)->i_imap;
	JFS_IP(ip)->ipimap = ipimap;
	JFS_IP(ip)->fileset = FILESYSTEM_I;

	if (dir) {
		agno = dbNextAG(JFS_SBI(pip->i_sb)->ipbmap);
		AG_LOCK(imap, agno);
		goto tryag;
	}


	
	agno = BLKTOAG(JFS_IP(pip)->agstart, JFS_SBI(pip->i_sb));

	if (atomic_read(&JFS_SBI(pip->i_sb)->bmap->db_active[agno])) {
		agno = dbNextAG(JFS_SBI(pip->i_sb)->ipbmap);
		AG_LOCK(imap, agno);
		goto tryag;
	}

	inum = pip->i_ino + 1;
	ino = inum & (INOSPERIAG - 1);

	
	if (ino == 0)
		inum = pip->i_ino;

	
	AG_LOCK(imap, agno);

	
	IREAD_LOCK(ipimap, RDWRLOCK_IMAP);

	
	iagno = INOTOIAG(inum);
	if ((rc = diIAGRead(imap, iagno, &mp))) {
		IREAD_UNLOCK(ipimap);
		AG_UNLOCK(imap, agno);
		return (rc);
	}
	iagp = (struct iag *) mp->data;

	addext = (imap->im_agctl[agno].numfree < 32 && iagp->nfreeexts);

	if (iagp->nfreeinos || addext) {
		extno = ino >> L2INOSPEREXT;

		if (addressPXD(&iagp->inoext[extno])) {
			bitno = ino & (INOSPEREXT - 1);
			if ((bitno =
			     diFindFree(le32_to_cpu(iagp->wmap[extno]),
					bitno))
			    < INOSPEREXT) {
				ino = (extno << L2INOSPEREXT) + bitno;

				rc = diAllocBit(imap, iagp, ino);
				IREAD_UNLOCK(ipimap);
				if (rc) {
					assert(rc == -EIO);
				} else {
					diInitInode(ip, iagno, ino, extno,
						    iagp);
					mark_metapage_dirty(mp);
				}
				release_metapage(mp);

				AG_UNLOCK(imap, agno);
				return (rc);
			}

			if (!addext)
				extno =
				    (extno ==
				     EXTSPERIAG - 1) ? 0 : extno + 1;
		}

		bitno = extno & (EXTSPERSUM - 1);
		nwords = (bitno == 0) ? SMAPSZ : SMAPSZ + 1;
		sword = extno >> L2EXTSPERSUM;

		mask = ONES << (EXTSPERSUM - bitno);
		inosmap = le32_to_cpu(iagp->inosmap[sword]) | mask;
		extsmap = le32_to_cpu(iagp->extsmap[sword]) | mask;

		for (i = 0; i < nwords; i++) {
			if (~inosmap) {
				rem = diFindFree(inosmap, 0);
				extno = (sword << L2EXTSPERSUM) + rem;
				rem = diFindFree(le32_to_cpu(iagp->wmap[extno]),
						 0);
				if (rem >= INOSPEREXT) {
					IREAD_UNLOCK(ipimap);
					release_metapage(mp);
					AG_UNLOCK(imap, agno);
					jfs_error(ip->i_sb,
						  "diAlloc: can't find free bit "
						  "in wmap");
					return -EIO;
				}

				ino = (extno << L2INOSPEREXT) + rem;
				rc = diAllocBit(imap, iagp, ino);
				IREAD_UNLOCK(ipimap);
				if (rc)
					assert(rc == -EIO);
				else {
					diInitInode(ip, iagno, ino, extno,
						    iagp);
					mark_metapage_dirty(mp);
				}
				release_metapage(mp);

				AG_UNLOCK(imap, agno);
				return (rc);

			}

			if (addext && ~extsmap) {
				rem = diFindFree(extsmap, 0);
				extno = (sword << L2EXTSPERSUM) + rem;

				if ((rc = diNewExt(imap, iagp, extno))) {
					if (rc == -ENOSPC)
						break;

					assert(rc == -EIO);
				} else {
					diInitInode(ip, iagno,
						    extno << L2INOSPEREXT,
						    extno, iagp);
					mark_metapage_dirty(mp);
				}
				release_metapage(mp);
				IREAD_UNLOCK(ipimap);
				AG_UNLOCK(imap, agno);
				return (rc);
			}

			sword = (sword == SMAPSZ - 1) ? 0 : sword + 1;
			inosmap = le32_to_cpu(iagp->inosmap[sword]);
			extsmap = le32_to_cpu(iagp->extsmap[sword]);
		}
	}
	
	IREAD_UNLOCK(ipimap);

	
	release_metapage(mp);

      tryag:
	rc = diAllocAG(imap, agno, dir, ip);

	AG_UNLOCK(imap, agno);

	if (rc != -ENOSPC)
		return (rc);

	return (diAllocAny(imap, agno, dir, ip));
}


static int
diAllocAG(struct inomap * imap, int agno, bool dir, struct inode *ip)
{
	int rc, addext, numfree, numinos;

	numfree = imap->im_agctl[agno].numfree;
	numinos = imap->im_agctl[agno].numinos;

	if (numfree > numinos) {
		jfs_error(ip->i_sb, "diAllocAG: numfree > numinos");
		return -EIO;
	}

	if (dir)
		addext = (numfree < 64 ||
			  (numfree < 256
			   && ((numfree * 100) / numinos) <= 20));
	else
		addext = (numfree == 0);

	if (addext) {
		if ((rc = diAllocExt(imap, agno, ip)) != -ENOSPC)
			return (rc);
	}

	return (diAllocIno(imap, agno, ip));
}


static int
diAllocAny(struct inomap * imap, int agno, bool dir, struct inode *ip)
{
	int ag, rc;
	int maxag = JFS_SBI(imap->im_ipimap->i_sb)->bmap->db_maxag;


	for (ag = agno + 1; ag <= maxag; ag++) {
		AG_LOCK(imap, ag);

		rc = diAllocAG(imap, ag, dir, ip);

		AG_UNLOCK(imap, ag);

		if (rc != -ENOSPC)
			return (rc);
	}

	for (ag = 0; ag < agno; ag++) {
		AG_LOCK(imap, ag);

		rc = diAllocAG(imap, ag, dir, ip);

		AG_UNLOCK(imap, ag);

		if (rc != -ENOSPC)
			return (rc);
	}

	return -ENOSPC;
}


static int diAllocIno(struct inomap * imap, int agno, struct inode *ip)
{
	int iagno, ino, rc, rem, extno, sword;
	struct metapage *mp;
	struct iag *iagp;

	if ((iagno = imap->im_agctl[agno].inofree) < 0)
		return -ENOSPC;

	
	IREAD_LOCK(imap->im_ipimap, RDWRLOCK_IMAP);

	if ((rc = diIAGRead(imap, iagno, &mp))) {
		IREAD_UNLOCK(imap->im_ipimap);
		return (rc);
	}
	iagp = (struct iag *) mp->data;

	if (!iagp->nfreeinos) {
		IREAD_UNLOCK(imap->im_ipimap);
		release_metapage(mp);
		jfs_error(ip->i_sb,
			  "diAllocIno: nfreeinos = 0, but iag on freelist");
		return -EIO;
	}

	for (sword = 0;; sword++) {
		if (sword >= SMAPSZ) {
			IREAD_UNLOCK(imap->im_ipimap);
			release_metapage(mp);
			jfs_error(ip->i_sb,
				  "diAllocIno: free inode not found in summary map");
			return -EIO;
		}

		if (~iagp->inosmap[sword])
			break;
	}

	rem = diFindFree(le32_to_cpu(iagp->inosmap[sword]), 0);
	if (rem >= EXTSPERSUM) {
		IREAD_UNLOCK(imap->im_ipimap);
		release_metapage(mp);
		jfs_error(ip->i_sb, "diAllocIno: no free extent found");
		return -EIO;
	}
	extno = (sword << L2EXTSPERSUM) + rem;

	rem = diFindFree(le32_to_cpu(iagp->wmap[extno]), 0);
	if (rem >= INOSPEREXT) {
		IREAD_UNLOCK(imap->im_ipimap);
		release_metapage(mp);
		jfs_error(ip->i_sb, "diAllocIno: free inode not found");
		return -EIO;
	}

	ino = (extno << L2INOSPEREXT) + rem;

	rc = diAllocBit(imap, iagp, ino);
	IREAD_UNLOCK(imap->im_ipimap);
	if (rc) {
		release_metapage(mp);
		return (rc);
	}

	diInitInode(ip, iagno, ino, extno, iagp);
	write_metapage(mp);

	return (0);
}


static int diAllocExt(struct inomap * imap, int agno, struct inode *ip)
{
	int rem, iagno, sword, extno, rc;
	struct metapage *mp;
	struct iag *iagp;

	if ((iagno = imap->im_agctl[agno].extfree) < 0) {
		if ((rc = diNewIAG(imap, &iagno, agno, &mp))) {
			return (rc);
		}
		iagp = (struct iag *) mp->data;

		iagp->agstart =
		    cpu_to_le64(AGTOBLK(agno, imap->im_ipimap));
	} else {
		IREAD_LOCK(imap->im_ipimap, RDWRLOCK_IMAP);
		if ((rc = diIAGRead(imap, iagno, &mp))) {
			IREAD_UNLOCK(imap->im_ipimap);
			jfs_error(ip->i_sb, "diAllocExt: error reading iag");
			return rc;
		}
		iagp = (struct iag *) mp->data;
	}

	for (sword = 0;; sword++) {
		if (sword >= SMAPSZ) {
			release_metapage(mp);
			IREAD_UNLOCK(imap->im_ipimap);
			jfs_error(ip->i_sb,
				  "diAllocExt: free ext summary map not found");
			return -EIO;
		}
		if (~iagp->extsmap[sword])
			break;
	}

	rem = diFindFree(le32_to_cpu(iagp->extsmap[sword]), 0);
	if (rem >= EXTSPERSUM) {
		release_metapage(mp);
		IREAD_UNLOCK(imap->im_ipimap);
		jfs_error(ip->i_sb, "diAllocExt: free extent not found");
		return -EIO;
	}
	extno = (sword << L2EXTSPERSUM) + rem;

	rc = diNewExt(imap, iagp, extno);
	IREAD_UNLOCK(imap->im_ipimap);
	if (rc) {
		if (iagp->nfreeexts == cpu_to_le32(EXTSPERIAG)) {
			IAGFREE_LOCK(imap);
			iagp->iagfree = cpu_to_le32(imap->im_freeiag);
			imap->im_freeiag = iagno;
			IAGFREE_UNLOCK(imap);
		}
		write_metapage(mp);
		return (rc);
	}

	diInitInode(ip, iagno, extno << L2INOSPEREXT, extno, iagp);

	write_metapage(mp);

	return (0);
}


static int diAllocBit(struct inomap * imap, struct iag * iagp, int ino)
{
	int extno, bitno, agno, sword, rc;
	struct metapage *amp = NULL, *bmp = NULL;
	struct iag *aiagp = NULL, *biagp = NULL;
	u32 mask;

	if (iagp->nfreeinos == cpu_to_le32(1)) {
		if ((int) le32_to_cpu(iagp->inofreefwd) >= 0) {
			if ((rc =
			     diIAGRead(imap, le32_to_cpu(iagp->inofreefwd),
				       &amp)))
				return (rc);
			aiagp = (struct iag *) amp->data;
		}

		if ((int) le32_to_cpu(iagp->inofreeback) >= 0) {
			if ((rc =
			     diIAGRead(imap,
				       le32_to_cpu(iagp->inofreeback),
				       &bmp))) {
				if (amp)
					release_metapage(amp);
				return (rc);
			}
			biagp = (struct iag *) bmp->data;
		}
	}

	agno = BLKTOAG(le64_to_cpu(iagp->agstart), JFS_SBI(imap->im_ipimap->i_sb));
	extno = ino >> L2INOSPEREXT;
	bitno = ino & (INOSPEREXT - 1);

	mask = HIGHORDER >> bitno;

	if (((le32_to_cpu(iagp->pmap[extno]) & mask) != 0) ||
	    ((le32_to_cpu(iagp->wmap[extno]) & mask) != 0) ||
	    (addressPXD(&iagp->inoext[extno]) == 0)) {
		if (amp)
			release_metapage(amp);
		if (bmp)
			release_metapage(bmp);

		jfs_error(imap->im_ipimap->i_sb,
			  "diAllocBit: iag inconsistent");
		return -EIO;
	}

	iagp->wmap[extno] |= cpu_to_le32(mask);

	if (iagp->wmap[extno] == cpu_to_le32(ONES)) {
		sword = extno >> L2EXTSPERSUM;
		bitno = extno & (EXTSPERSUM - 1);
		iagp->inosmap[sword] |= cpu_to_le32(HIGHORDER >> bitno);
	}

	if (iagp->nfreeinos == cpu_to_le32(1)) {
		if (amp) {
			aiagp->inofreeback = iagp->inofreeback;
			write_metapage(amp);
		}

		if (bmp) {
			biagp->inofreefwd = iagp->inofreefwd;
			write_metapage(bmp);
		} else {
			imap->im_agctl[agno].inofree =
			    le32_to_cpu(iagp->inofreefwd);
		}
		iagp->inofreefwd = iagp->inofreeback = cpu_to_le32(-1);
	}

	le32_add_cpu(&iagp->nfreeinos, -1);
	imap->im_agctl[agno].numfree -= 1;
	atomic_dec(&imap->im_numfree);

	return (0);
}


static int diNewExt(struct inomap * imap, struct iag * iagp, int extno)
{
	int agno, iagno, fwd, back, freei = 0, sword, rc;
	struct iag *aiagp = NULL, *biagp = NULL, *ciagp = NULL;
	struct metapage *amp, *bmp, *cmp, *dmp;
	struct inode *ipimap;
	s64 blkno, hint;
	int i, j;
	u32 mask;
	ino_t ino;
	struct dinode *dp;
	struct jfs_sb_info *sbi;

	if (!iagp->nfreeexts) {
		jfs_error(imap->im_ipimap->i_sb, "diNewExt: no free extents");
		return -EIO;
	}

	ipimap = imap->im_ipimap;
	sbi = JFS_SBI(ipimap->i_sb);

	amp = bmp = cmp = NULL;

	agno = BLKTOAG(le64_to_cpu(iagp->agstart), sbi);
	iagno = le32_to_cpu(iagp->iagnum);

	if (iagp->nfreeexts == cpu_to_le32(1)) {
		if ((fwd = le32_to_cpu(iagp->extfreefwd)) >= 0) {
			if ((rc = diIAGRead(imap, fwd, &amp)))
				return (rc);
			aiagp = (struct iag *) amp->data;
		}

		if ((back = le32_to_cpu(iagp->extfreeback)) >= 0) {
			if ((rc = diIAGRead(imap, back, &bmp)))
				goto error_out;
			biagp = (struct iag *) bmp->data;
		}
	} else {
		fwd = back = -1;
		if (iagp->nfreeexts == cpu_to_le32(EXTSPERIAG)) {
			if ((fwd = imap->im_agctl[agno].extfree) >= 0) {
				if ((rc = diIAGRead(imap, fwd, &amp)))
					goto error_out;
				aiagp = (struct iag *) amp->data;
			}
		}
	}

	if (iagp->nfreeinos == 0) {
		freei = imap->im_agctl[agno].inofree;

		if (freei >= 0) {
			if (freei == fwd) {
				ciagp = aiagp;
			} else if (freei == back) {
				ciagp = biagp;
			} else {
				if ((rc = diIAGRead(imap, freei, &cmp)))
					goto error_out;
				ciagp = (struct iag *) cmp->data;
			}
			if (ciagp == NULL) {
				jfs_error(imap->im_ipimap->i_sb,
					  "diNewExt: ciagp == NULL");
				rc = -EIO;
				goto error_out;
			}
		}
	}

	if ((extno == 0) || (addressPXD(&iagp->inoext[extno - 1]) == 0))
		hint = ((s64) agno << sbi->bmap->db_agl2size) - 1;
	else
		hint = addressPXD(&iagp->inoext[extno - 1]) +
		    lengthPXD(&iagp->inoext[extno - 1]) - 1;

	if ((rc = dbAlloc(ipimap, hint, (s64) imap->im_nbperiext, &blkno)))
		goto error_out;

	ino = (iagno << L2INOSPERIAG) + (extno << L2INOSPEREXT);

	for (i = 0; i < imap->im_nbperiext; i += sbi->nbperpage) {
		dmp = get_metapage(ipimap, blkno + i, PSIZE, 1);
		if (dmp == NULL) {
			rc = -EIO;
			goto error_out;
		}
		dp = (struct dinode *) dmp->data;

		for (j = 0; j < INOSPERPAGE; j++, dp++, ino++) {
			dp->di_inostamp = cpu_to_le32(sbi->inostamp);
			dp->di_number = cpu_to_le32(ino);
			dp->di_fileset = cpu_to_le32(FILESYSTEM_I);
			dp->di_mode = 0;
			dp->di_nlink = 0;
			PXDaddress(&(dp->di_ixpxd), blkno);
			PXDlength(&(dp->di_ixpxd), imap->im_nbperiext);
		}
		write_metapage(dmp);
	}

	if (iagp->nfreeexts == cpu_to_le32(1)) {
		if (fwd >= 0)
			aiagp->extfreeback = iagp->extfreeback;

		if (back >= 0)
			biagp->extfreefwd = iagp->extfreefwd;
		else
			imap->im_agctl[agno].extfree =
			    le32_to_cpu(iagp->extfreefwd);

		iagp->extfreefwd = iagp->extfreeback = cpu_to_le32(-1);
	} else {
		if (iagp->nfreeexts == cpu_to_le32(EXTSPERIAG)) {
			if (fwd >= 0)
				aiagp->extfreeback = cpu_to_le32(iagno);

			iagp->extfreefwd = cpu_to_le32(fwd);
			iagp->extfreeback = cpu_to_le32(-1);
			imap->im_agctl[agno].extfree = iagno;
		}
	}

	if (iagp->nfreeinos == 0) {
		if (freei >= 0)
			ciagp->inofreeback = cpu_to_le32(iagno);

		iagp->inofreefwd =
		    cpu_to_le32(imap->im_agctl[agno].inofree);
		iagp->inofreeback = cpu_to_le32(-1);
		imap->im_agctl[agno].inofree = iagno;
	}

	
	PXDlength(&iagp->inoext[extno], imap->im_nbperiext);
	PXDaddress(&iagp->inoext[extno], blkno);

	iagp->wmap[extno] = cpu_to_le32(HIGHORDER);
	iagp->pmap[extno] = 0;

	sword = extno >> L2EXTSPERSUM;
	mask = HIGHORDER >> (extno & (EXTSPERSUM - 1));
	iagp->extsmap[sword] |= cpu_to_le32(mask);
	iagp->inosmap[sword] &= cpu_to_le32(~mask);

	le32_add_cpu(&iagp->nfreeinos, (INOSPEREXT - 1));
	le32_add_cpu(&iagp->nfreeexts, -1);

	imap->im_agctl[agno].numfree += (INOSPEREXT - 1);
	imap->im_agctl[agno].numinos += INOSPEREXT;

	atomic_add(INOSPEREXT - 1, &imap->im_numfree);
	atomic_add(INOSPEREXT, &imap->im_numinos);

	if (amp)
		write_metapage(amp);
	if (bmp)
		write_metapage(bmp);
	if (cmp)
		write_metapage(cmp);

	return (0);

      error_out:

	if (amp)
		release_metapage(amp);
	if (bmp)
		release_metapage(bmp);
	if (cmp)
		release_metapage(cmp);

	return (rc);
}


static int
diNewIAG(struct inomap * imap, int *iagnop, int agno, struct metapage ** mpp)
{
	int rc;
	int iagno, i, xlen;
	struct inode *ipimap;
	struct super_block *sb;
	struct jfs_sb_info *sbi;
	struct metapage *mp;
	struct iag *iagp;
	s64 xaddr = 0;
	s64 blkno;
	tid_t tid;
	struct inode *iplist[1];

	
	ipimap = imap->im_ipimap;
	sb = ipimap->i_sb;
	sbi = JFS_SBI(sb);

	
	IAGFREE_LOCK(imap);

	if (imap->im_freeiag >= 0) {
		
		iagno = imap->im_freeiag;

		
		blkno = IAGTOLBLK(iagno, sbi->l2nbperpage);
	} else {

		
		IWRITE_LOCK(ipimap, RDWRLOCK_IMAP);

		if (ipimap->i_size >> L2PSIZE != imap->im_nextiag + 1) {
			IWRITE_UNLOCK(ipimap);
			IAGFREE_UNLOCK(imap);
			jfs_error(imap->im_ipimap->i_sb,
				  "diNewIAG: ipimap->i_size is wrong");
			return -EIO;
		}


		
		iagno = imap->im_nextiag;

		if (iagno > (MAXIAGS - 1)) {
			
			IWRITE_UNLOCK(ipimap);

			rc = -ENOSPC;
			goto out;
		}

		
		blkno = IAGTOLBLK(iagno, sbi->l2nbperpage);

		
		xlen = sbi->nbperpage;
		if ((rc = dbAlloc(ipimap, 0, (s64) xlen, &xaddr))) {
			
			IWRITE_UNLOCK(ipimap);

			goto out;
		}

		tid = txBegin(sb, COMMIT_FORCE);
		mutex_lock(&JFS_IP(ipimap)->commit_mutex);

		
		if ((rc =
		     xtInsert(tid, ipimap, 0, blkno, xlen, &xaddr, 0))) {
			txEnd(tid);
			mutex_unlock(&JFS_IP(ipimap)->commit_mutex);
			dbFree(ipimap, xaddr, (s64) xlen);

			
			IWRITE_UNLOCK(ipimap);

			goto out;
		}

		
		ipimap->i_size += PSIZE;
		inode_add_bytes(ipimap, PSIZE);

		
		mp = get_metapage(ipimap, blkno, PSIZE, 0);
		if (!mp) {
			xtTruncate(tid, ipimap, ipimap->i_size - PSIZE,
				   COMMIT_PWMAP);

			txAbort(tid, 0);
			txEnd(tid);
			mutex_unlock(&JFS_IP(ipimap)->commit_mutex);

			
			IWRITE_UNLOCK(ipimap);

			rc = -EIO;
			goto out;
		}
		iagp = (struct iag *) mp->data;

		
		memset(iagp, 0, sizeof(struct iag));
		iagp->iagnum = cpu_to_le32(iagno);
		iagp->inofreefwd = iagp->inofreeback = cpu_to_le32(-1);
		iagp->extfreefwd = iagp->extfreeback = cpu_to_le32(-1);
		iagp->iagfree = cpu_to_le32(-1);
		iagp->nfreeinos = 0;
		iagp->nfreeexts = cpu_to_le32(EXTSPERIAG);

		for (i = 0; i < SMAPSZ; i++)
			iagp->inosmap[i] = cpu_to_le32(ONES);

		flush_metapage(mp);

		iplist[0] = ipimap;
		rc = txCommit(tid, 1, &iplist[0], COMMIT_FORCE);

		txEnd(tid);
		mutex_unlock(&JFS_IP(ipimap)->commit_mutex);

		duplicateIXtree(sb, blkno, xlen, &xaddr);

		
		imap->im_nextiag += 1;

		imap->im_freeiag = iagno;

		diSync(ipimap);

		
		IWRITE_UNLOCK(ipimap);
	}

	
	IREAD_LOCK(ipimap, RDWRLOCK_IMAP);

	
	if ((rc = diIAGRead(imap, iagno, &mp))) {
		IREAD_UNLOCK(ipimap);
		rc = -EIO;
		goto out;
	}
	iagp = (struct iag *) mp->data;

	
	imap->im_freeiag = le32_to_cpu(iagp->iagfree);
	iagp->iagfree = cpu_to_le32(-1);

	
	*iagnop = iagno;
	*mpp = mp;

      out:
	
	IAGFREE_UNLOCK(imap);

	return (rc);
}

static int diIAGRead(struct inomap * imap, int iagno, struct metapage ** mpp)
{
	struct inode *ipimap = imap->im_ipimap;
	s64 blkno;

	
	blkno = IAGTOLBLK(iagno, JFS_SBI(ipimap->i_sb)->l2nbperpage);

	
	*mpp = read_metapage(ipimap, blkno, PSIZE, 0);
	if (*mpp == NULL) {
		return -EIO;
	}

	return (0);
}

static int diFindFree(u32 word, int start)
{
	int bitno;
	assert(start < 32);
	
	for (word <<= start, bitno = start; bitno < 32;
	     bitno++, word <<= 1) {
		if ((word & HIGHORDER) == 0)
			break;
	}
	return (bitno);
}

int
diUpdatePMap(struct inode *ipimap,
	     unsigned long inum, bool is_free, struct tblock * tblk)
{
	int rc;
	struct iag *iagp;
	struct metapage *mp;
	int iagno, ino, extno, bitno;
	struct inomap *imap;
	u32 mask;
	struct jfs_log *log;
	int lsn, difft, diffp;
	unsigned long flags;

	imap = JFS_IP(ipimap)->i_imap;
	
	iagno = INOTOIAG(inum);
	
	if (iagno >= imap->im_nextiag) {
		jfs_error(ipimap->i_sb,
			  "diUpdatePMap: the iag is outside the map");
		return -EIO;
	}
	
	IREAD_LOCK(ipimap, RDWRLOCK_IMAP);
	rc = diIAGRead(imap, iagno, &mp);
	IREAD_UNLOCK(ipimap);
	if (rc)
		return (rc);
	metapage_wait_for_io(mp);
	iagp = (struct iag *) mp->data;
	ino = inum & (INOSPERIAG - 1);
	extno = ino >> L2INOSPEREXT;
	bitno = ino & (INOSPEREXT - 1);
	mask = HIGHORDER >> bitno;
	if (is_free) {
		if (!(le32_to_cpu(iagp->wmap[extno]) & mask)) {
			jfs_error(ipimap->i_sb,
				  "diUpdatePMap: inode %ld not marked as "
				  "allocated in wmap!", inum);
		}
		if (!(le32_to_cpu(iagp->pmap[extno]) & mask)) {
			jfs_error(ipimap->i_sb,
				  "diUpdatePMap: inode %ld not marked as "
				  "allocated in pmap!", inum);
		}
		
		iagp->pmap[extno] &= cpu_to_le32(~mask);
	}
	else {
		if (!(le32_to_cpu(iagp->wmap[extno]) & mask)) {
			release_metapage(mp);
			jfs_error(ipimap->i_sb,
				  "diUpdatePMap: the inode is not allocated in "
				  "the working map");
			return -EIO;
		}
		if ((le32_to_cpu(iagp->pmap[extno]) & mask) != 0) {
			release_metapage(mp);
			jfs_error(ipimap->i_sb,
				  "diUpdatePMap: the inode is not free in the "
				  "persistent map");
			return -EIO;
		}
		
		iagp->pmap[extno] |= cpu_to_le32(mask);
	}
	lsn = tblk->lsn;
	log = JFS_SBI(tblk->sb)->log;
	LOGSYNC_LOCK(log, flags);
	if (mp->lsn != 0) {
		
		logdiff(difft, lsn, log);
		logdiff(diffp, mp->lsn, log);
		if (difft < diffp) {
			mp->lsn = lsn;
			
			list_move(&mp->synclist, &tblk->synclist);
		}
		
		assert(mp->clsn);
		logdiff(difft, tblk->clsn, log);
		logdiff(diffp, mp->clsn, log);
		if (difft > diffp)
			mp->clsn = tblk->clsn;
	} else {
		mp->log = log;
		mp->lsn = lsn;
		
		log->count++;
		list_add(&mp->synclist, &tblk->synclist);
		mp->clsn = tblk->clsn;
	}
	LOGSYNC_UNLOCK(log, flags);
	write_metapage(mp);
	return (0);
}

int diExtendFS(struct inode *ipimap, struct inode *ipbmap)
{
	int rc, rcx = 0;
	struct inomap *imap = JFS_IP(ipimap)->i_imap;
	struct iag *iagp = NULL, *hiagp = NULL;
	struct bmap *mp = JFS_SBI(ipbmap->i_sb)->bmap;
	struct metapage *bp, *hbp;
	int i, n, head;
	int numinos, xnuminos = 0, xnumfree = 0;
	s64 agstart;

	jfs_info("diExtendFS: nextiag:%d numinos:%d numfree:%d",
		   imap->im_nextiag, atomic_read(&imap->im_numinos),
		   atomic_read(&imap->im_numfree));


	
	for (i = 0; i < MAXAG; i++) {
		imap->im_agctl[i].inofree = -1;
		imap->im_agctl[i].extfree = -1;
		imap->im_agctl[i].numinos = 0;	
		imap->im_agctl[i].numfree = 0;	
	}

	for (i = 0; i < imap->im_nextiag; i++) {
		if ((rc = diIAGRead(imap, i, &bp))) {
			rcx = rc;
			continue;
		}
		iagp = (struct iag *) bp->data;
		if (le32_to_cpu(iagp->iagnum) != i) {
			release_metapage(bp);
			jfs_error(ipimap->i_sb,
				  "diExtendFs: unexpected value of iagnum");
			return -EIO;
		}

		
		if (iagp->nfreeexts == cpu_to_le32(EXTSPERIAG)) {
			release_metapage(bp);
			continue;
		}

		agstart = le64_to_cpu(iagp->agstart);
		n = agstart >> mp->db_agl2size;
		iagp->agstart = cpu_to_le64((s64)n << mp->db_agl2size);

		
		numinos = (EXTSPERIAG - le32_to_cpu(iagp->nfreeexts))
		    << L2INOSPEREXT;
		if (numinos > 0) {
			
			imap->im_agctl[n].numinos += numinos;
			xnuminos += numinos;
		}

		
		if ((int) le32_to_cpu(iagp->nfreeinos) > 0) {
			if ((head = imap->im_agctl[n].inofree) == -1) {
				iagp->inofreefwd = cpu_to_le32(-1);
				iagp->inofreeback = cpu_to_le32(-1);
			} else {
				if ((rc = diIAGRead(imap, head, &hbp))) {
					rcx = rc;
					goto nextiag;
				}
				hiagp = (struct iag *) hbp->data;
				hiagp->inofreeback = iagp->iagnum;
				iagp->inofreefwd = cpu_to_le32(head);
				iagp->inofreeback = cpu_to_le32(-1);
				write_metapage(hbp);
			}

			imap->im_agctl[n].inofree =
			    le32_to_cpu(iagp->iagnum);

			
			imap->im_agctl[n].numfree +=
			    le32_to_cpu(iagp->nfreeinos);
			xnumfree += le32_to_cpu(iagp->nfreeinos);
		}

		
		if (le32_to_cpu(iagp->nfreeexts) > 0) {
			if ((head = imap->im_agctl[n].extfree) == -1) {
				iagp->extfreefwd = cpu_to_le32(-1);
				iagp->extfreeback = cpu_to_le32(-1);
			} else {
				if ((rc = diIAGRead(imap, head, &hbp))) {
					rcx = rc;
					goto nextiag;
				}
				hiagp = (struct iag *) hbp->data;
				hiagp->extfreeback = iagp->iagnum;
				iagp->extfreefwd = cpu_to_le32(head);
				iagp->extfreeback = cpu_to_le32(-1);
				write_metapage(hbp);
			}

			imap->im_agctl[n].extfree =
			    le32_to_cpu(iagp->iagnum);
		}

	      nextiag:
		write_metapage(bp);
	}

	if (xnuminos != atomic_read(&imap->im_numinos) ||
	    xnumfree != atomic_read(&imap->im_numfree)) {
		jfs_error(ipimap->i_sb,
			  "diExtendFs: numinos or numfree incorrect");
		return -EIO;
	}

	return rcx;
}


static void duplicateIXtree(struct super_block *sb, s64 blkno,
			    int xlen, s64 *xaddr)
{
	struct jfs_superblock *j_sb;
	struct buffer_head *bh;
	struct inode *ip;
	tid_t tid;

	
	if (JFS_SBI(sb)->mntflag & JFS_BAD_SAIT)	
		return;
	ip = diReadSpecial(sb, FILESYSTEM_I, 1);
	if (ip == NULL) {
		JFS_SBI(sb)->mntflag |= JFS_BAD_SAIT;
		if (readSuper(sb, &bh))
			return;
		j_sb = (struct jfs_superblock *)bh->b_data;
		j_sb->s_flag |= cpu_to_le32(JFS_BAD_SAIT);

		mark_buffer_dirty(bh);
		sync_dirty_buffer(bh);
		brelse(bh);
		return;
	}

	
	tid = txBegin(sb, COMMIT_FORCE);
	
	if (xtInsert(tid, ip, 0, blkno, xlen, xaddr, 0)) {
		JFS_SBI(sb)->mntflag |= JFS_BAD_SAIT;
		txAbort(tid, 1);
		goto cleanup;

	}
	
	ip->i_size += PSIZE;
	inode_add_bytes(ip, PSIZE);
	txCommit(tid, 1, &ip, COMMIT_FORCE);
      cleanup:
	txEnd(tid);
	diFreeSpecial(ip);
}

static int copy_from_dinode(struct dinode * dip, struct inode *ip)
{
	struct jfs_inode_info *jfs_ip = JFS_IP(ip);
	struct jfs_sb_info *sbi = JFS_SBI(ip->i_sb);

	jfs_ip->fileset = le32_to_cpu(dip->di_fileset);
	jfs_ip->mode2 = le32_to_cpu(dip->di_mode);
	jfs_set_inode_flags(ip);

	ip->i_mode = le32_to_cpu(dip->di_mode) & 0xffff;
	if (sbi->umask != -1) {
		ip->i_mode = (ip->i_mode & ~0777) | (0777 & ~sbi->umask);
		
		if (S_ISDIR(ip->i_mode)) {
			if (ip->i_mode & 0400)
				ip->i_mode |= 0100;
			if (ip->i_mode & 0040)
				ip->i_mode |= 0010;
			if (ip->i_mode & 0004)
				ip->i_mode |= 0001;
		}
	}
	set_nlink(ip, le32_to_cpu(dip->di_nlink));

	jfs_ip->saved_uid = le32_to_cpu(dip->di_uid);
	if (sbi->uid == -1)
		ip->i_uid = jfs_ip->saved_uid;
	else {
		ip->i_uid = sbi->uid;
	}

	jfs_ip->saved_gid = le32_to_cpu(dip->di_gid);
	if (sbi->gid == -1)
		ip->i_gid = jfs_ip->saved_gid;
	else {
		ip->i_gid = sbi->gid;
	}

	ip->i_size = le64_to_cpu(dip->di_size);
	ip->i_atime.tv_sec = le32_to_cpu(dip->di_atime.tv_sec);
	ip->i_atime.tv_nsec = le32_to_cpu(dip->di_atime.tv_nsec);
	ip->i_mtime.tv_sec = le32_to_cpu(dip->di_mtime.tv_sec);
	ip->i_mtime.tv_nsec = le32_to_cpu(dip->di_mtime.tv_nsec);
	ip->i_ctime.tv_sec = le32_to_cpu(dip->di_ctime.tv_sec);
	ip->i_ctime.tv_nsec = le32_to_cpu(dip->di_ctime.tv_nsec);
	ip->i_blocks = LBLK2PBLK(ip->i_sb, le64_to_cpu(dip->di_nblocks));
	ip->i_generation = le32_to_cpu(dip->di_gen);

	jfs_ip->ixpxd = dip->di_ixpxd;	
	jfs_ip->acl = dip->di_acl;	
	jfs_ip->ea = dip->di_ea;
	jfs_ip->next_index = le32_to_cpu(dip->di_next_index);
	jfs_ip->otime = le32_to_cpu(dip->di_otime.tv_sec);
	jfs_ip->acltype = le32_to_cpu(dip->di_acltype);

	if (S_ISCHR(ip->i_mode) || S_ISBLK(ip->i_mode)) {
		jfs_ip->dev = le32_to_cpu(dip->di_rdev);
		ip->i_rdev = new_decode_dev(jfs_ip->dev);
	}

	if (S_ISDIR(ip->i_mode)) {
		memcpy(&jfs_ip->i_dirtable, &dip->di_dirtable, 384);
	} else if (S_ISREG(ip->i_mode) || S_ISLNK(ip->i_mode)) {
		memcpy(&jfs_ip->i_xtroot, &dip->di_xtroot, 288);
	} else
		memcpy(&jfs_ip->i_inline_ea, &dip->di_inlineea, 128);

	
	jfs_ip->cflag = 0;
	jfs_ip->btindex = 0;
	jfs_ip->btorder = 0;
	jfs_ip->bxflag = 0;
	jfs_ip->blid = 0;
	jfs_ip->atlhead = 0;
	jfs_ip->atltail = 0;
	jfs_ip->xtlid = 0;
	return (0);
}

static void copy_to_dinode(struct dinode * dip, struct inode *ip)
{
	struct jfs_inode_info *jfs_ip = JFS_IP(ip);
	struct jfs_sb_info *sbi = JFS_SBI(ip->i_sb);

	dip->di_fileset = cpu_to_le32(jfs_ip->fileset);
	dip->di_inostamp = cpu_to_le32(sbi->inostamp);
	dip->di_number = cpu_to_le32(ip->i_ino);
	dip->di_gen = cpu_to_le32(ip->i_generation);
	dip->di_size = cpu_to_le64(ip->i_size);
	dip->di_nblocks = cpu_to_le64(PBLK2LBLK(ip->i_sb, ip->i_blocks));
	dip->di_nlink = cpu_to_le32(ip->i_nlink);
	if (sbi->uid == -1)
		dip->di_uid = cpu_to_le32(ip->i_uid);
	else
		dip->di_uid = cpu_to_le32(jfs_ip->saved_uid);
	if (sbi->gid == -1)
		dip->di_gid = cpu_to_le32(ip->i_gid);
	else
		dip->di_gid = cpu_to_le32(jfs_ip->saved_gid);
	jfs_get_inode_flags(jfs_ip);
	if (sbi->umask == -1)
		dip->di_mode = cpu_to_le32((jfs_ip->mode2 & 0xffff0000) |
					   ip->i_mode);
	else 
		dip->di_mode = cpu_to_le32(jfs_ip->mode2);

	dip->di_atime.tv_sec = cpu_to_le32(ip->i_atime.tv_sec);
	dip->di_atime.tv_nsec = cpu_to_le32(ip->i_atime.tv_nsec);
	dip->di_ctime.tv_sec = cpu_to_le32(ip->i_ctime.tv_sec);
	dip->di_ctime.tv_nsec = cpu_to_le32(ip->i_ctime.tv_nsec);
	dip->di_mtime.tv_sec = cpu_to_le32(ip->i_mtime.tv_sec);
	dip->di_mtime.tv_nsec = cpu_to_le32(ip->i_mtime.tv_nsec);
	dip->di_ixpxd = jfs_ip->ixpxd;	
	dip->di_acl = jfs_ip->acl;	
	dip->di_ea = jfs_ip->ea;
	dip->di_next_index = cpu_to_le32(jfs_ip->next_index);
	dip->di_otime.tv_sec = cpu_to_le32(jfs_ip->otime);
	dip->di_otime.tv_nsec = 0;
	dip->di_acltype = cpu_to_le32(jfs_ip->acltype);
	if (S_ISCHR(ip->i_mode) || S_ISBLK(ip->i_mode))
		dip->di_rdev = cpu_to_le32(jfs_ip->dev);
}
