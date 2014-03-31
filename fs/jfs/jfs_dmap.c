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

#include <linux/fs.h>
#include <linux/slab.h>
#include "jfs_incore.h"
#include "jfs_superblock.h"
#include "jfs_dmap.h"
#include "jfs_imap.h"
#include "jfs_lock.h"
#include "jfs_metapage.h"
#include "jfs_debug.h"


#define BMAP_LOCK_INIT(bmp)	mutex_init(&bmp->db_bmaplock)
#define BMAP_LOCK(bmp)		mutex_lock(&bmp->db_bmaplock)
#define BMAP_UNLOCK(bmp)	mutex_unlock(&bmp->db_bmaplock)

static void dbAllocBits(struct bmap * bmp, struct dmap * dp, s64 blkno,
			int nblocks);
static void dbSplit(dmtree_t * tp, int leafno, int splitsz, int newval);
static int dbBackSplit(dmtree_t * tp, int leafno);
static int dbJoin(dmtree_t * tp, int leafno, int newval);
static void dbAdjTree(dmtree_t * tp, int leafno, int newval);
static int dbAdjCtl(struct bmap * bmp, s64 blkno, int newval, int alloc,
		    int level);
static int dbAllocAny(struct bmap * bmp, s64 nblocks, int l2nb, s64 * results);
static int dbAllocNext(struct bmap * bmp, struct dmap * dp, s64 blkno,
		       int nblocks);
static int dbAllocNear(struct bmap * bmp, struct dmap * dp, s64 blkno,
		       int nblocks,
		       int l2nb, s64 * results);
static int dbAllocDmap(struct bmap * bmp, struct dmap * dp, s64 blkno,
		       int nblocks);
static int dbAllocDmapLev(struct bmap * bmp, struct dmap * dp, int nblocks,
			  int l2nb,
			  s64 * results);
static int dbAllocAG(struct bmap * bmp, int agno, s64 nblocks, int l2nb,
		     s64 * results);
static int dbAllocCtl(struct bmap * bmp, s64 nblocks, int l2nb, s64 blkno,
		      s64 * results);
static int dbExtend(struct inode *ip, s64 blkno, s64 nblocks, s64 addnblocks);
static int dbFindBits(u32 word, int l2nb);
static int dbFindCtl(struct bmap * bmp, int l2nb, int level, s64 * blkno);
static int dbFindLeaf(dmtree_t * tp, int l2nb, int *leafidx);
static int dbFreeBits(struct bmap * bmp, struct dmap * dp, s64 blkno,
		      int nblocks);
static int dbFreeDmap(struct bmap * bmp, struct dmap * dp, s64 blkno,
		      int nblocks);
static int dbMaxBud(u8 * cp);
s64 dbMapFileSizeToMapSize(struct inode *ipbmap);
static int blkstol2(s64 nb);

static int cntlz(u32 value);
static int cnttz(u32 word);

static int dbAllocDmapBU(struct bmap * bmp, struct dmap * dp, s64 blkno,
			 int nblocks);
static int dbInitDmap(struct dmap * dp, s64 blkno, int nblocks);
static int dbInitDmapTree(struct dmap * dp);
static int dbInitTree(struct dmaptree * dtp);
static int dbInitDmapCtl(struct dmapctl * dcp, int level, int i);
static int dbGetL2AGSize(s64 nblocks);

static const s8 budtab[256] = {
	3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	2, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0,
	2, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0,
	2, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0,
	2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	2, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0,
	2, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0,
	2, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0,
	2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	2, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0,
	2, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0,
	2, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, -1
};


int dbMount(struct inode *ipbmap)
{
	struct bmap *bmp;
	struct dbmap_disk *dbmp_le;
	struct metapage *mp;
	int i;

	
	bmp = kmalloc(sizeof(struct bmap), GFP_KERNEL);
	if (bmp == NULL)
		return -ENOMEM;

	
	mp = read_metapage(ipbmap,
			   BMAPBLKNO << JFS_SBI(ipbmap->i_sb)->l2nbperpage,
			   PSIZE, 0);
	if (mp == NULL) {
		kfree(bmp);
		return -EIO;
	}

	
	dbmp_le = (struct dbmap_disk *) mp->data;
	bmp->db_mapsize = le64_to_cpu(dbmp_le->dn_mapsize);
	bmp->db_nfree = le64_to_cpu(dbmp_le->dn_nfree);
	bmp->db_l2nbperpage = le32_to_cpu(dbmp_le->dn_l2nbperpage);
	bmp->db_numag = le32_to_cpu(dbmp_le->dn_numag);
	bmp->db_maxlevel = le32_to_cpu(dbmp_le->dn_maxlevel);
	bmp->db_maxag = le32_to_cpu(dbmp_le->dn_maxag);
	bmp->db_agpref = le32_to_cpu(dbmp_le->dn_agpref);
	bmp->db_aglevel = le32_to_cpu(dbmp_le->dn_aglevel);
	bmp->db_agheight = le32_to_cpu(dbmp_le->dn_agheight);
	bmp->db_agwidth = le32_to_cpu(dbmp_le->dn_agwidth);
	bmp->db_agstart = le32_to_cpu(dbmp_le->dn_agstart);
	bmp->db_agl2size = le32_to_cpu(dbmp_le->dn_agl2size);
	for (i = 0; i < MAXAG; i++)
		bmp->db_agfree[i] = le64_to_cpu(dbmp_le->dn_agfree[i]);
	bmp->db_agsize = le64_to_cpu(dbmp_le->dn_agsize);
	bmp->db_maxfreebud = dbmp_le->dn_maxfreebud;

	
	release_metapage(mp);

	
	bmp->db_ipbmap = ipbmap;
	JFS_SBI(ipbmap->i_sb)->bmap = bmp;

	memset(bmp->db_active, 0, sizeof(bmp->db_active));

	BMAP_LOCK_INIT(bmp);

	return (0);
}


/*
 * NAME:	dbUnmount()
 *
 * FUNCTION:	terminate the block allocation map in preparation for
 *		file system unmount.
 *
 *		the in-core bmap descriptor is written to disk and
 *		the memory for this descriptor is freed.
 *
 * PARAMETERS:
 *	ipbmap	- pointer to in-core inode for the block map.
 *
 * RETURN VALUES:
 *	0	- success
 *	-EIO	- i/o error
 */
int dbUnmount(struct inode *ipbmap, int mounterror)
{
	struct bmap *bmp = JFS_SBI(ipbmap->i_sb)->bmap;

	if (!(mounterror || isReadOnly(ipbmap)))
		dbSync(ipbmap);

	truncate_inode_pages(ipbmap->i_mapping, 0);

	
	kfree(bmp);

	return (0);
}

int dbSync(struct inode *ipbmap)
{
	struct dbmap_disk *dbmp_le;
	struct bmap *bmp = JFS_SBI(ipbmap->i_sb)->bmap;
	struct metapage *mp;
	int i;

	
	mp = read_metapage(ipbmap,
			   BMAPBLKNO << JFS_SBI(ipbmap->i_sb)->l2nbperpage,
			   PSIZE, 0);
	if (mp == NULL) {
		jfs_err("dbSync: read_metapage failed!");
		return -EIO;
	}
	
	dbmp_le = (struct dbmap_disk *) mp->data;
	dbmp_le->dn_mapsize = cpu_to_le64(bmp->db_mapsize);
	dbmp_le->dn_nfree = cpu_to_le64(bmp->db_nfree);
	dbmp_le->dn_l2nbperpage = cpu_to_le32(bmp->db_l2nbperpage);
	dbmp_le->dn_numag = cpu_to_le32(bmp->db_numag);
	dbmp_le->dn_maxlevel = cpu_to_le32(bmp->db_maxlevel);
	dbmp_le->dn_maxag = cpu_to_le32(bmp->db_maxag);
	dbmp_le->dn_agpref = cpu_to_le32(bmp->db_agpref);
	dbmp_le->dn_aglevel = cpu_to_le32(bmp->db_aglevel);
	dbmp_le->dn_agheight = cpu_to_le32(bmp->db_agheight);
	dbmp_le->dn_agwidth = cpu_to_le32(bmp->db_agwidth);
	dbmp_le->dn_agstart = cpu_to_le32(bmp->db_agstart);
	dbmp_le->dn_agl2size = cpu_to_le32(bmp->db_agl2size);
	for (i = 0; i < MAXAG; i++)
		dbmp_le->dn_agfree[i] = cpu_to_le64(bmp->db_agfree[i]);
	dbmp_le->dn_agsize = cpu_to_le64(bmp->db_agsize);
	dbmp_le->dn_maxfreebud = bmp->db_maxfreebud;

	
	write_metapage(mp);

	filemap_write_and_wait(ipbmap->i_mapping);

	diWriteSpecial(ipbmap, 0);

	return (0);
}


int dbFree(struct inode *ip, s64 blkno, s64 nblocks)
{
	struct metapage *mp;
	struct dmap *dp;
	int nb, rc;
	s64 lblkno, rem;
	struct inode *ipbmap = JFS_SBI(ip->i_sb)->ipbmap;
	struct bmap *bmp = JFS_SBI(ip->i_sb)->bmap;

	IREAD_LOCK(ipbmap, RDWRLOCK_DMAP);

	
	if (unlikely((blkno == 0) || (blkno + nblocks > bmp->db_mapsize))) {
		IREAD_UNLOCK(ipbmap);
		printk(KERN_ERR "blkno = %Lx, nblocks = %Lx\n",
		       (unsigned long long) blkno,
		       (unsigned long long) nblocks);
		jfs_error(ip->i_sb,
			  "dbFree: block to be freed is outside the map");
		return -EIO;
	}

	mp = NULL;
	for (rem = nblocks; rem > 0; rem -= nb, blkno += nb) {
		
		if (mp) {
			write_metapage(mp);
		}

		
		lblkno = BLKTODMAP(blkno, bmp->db_l2nbperpage);
		mp = read_metapage(ipbmap, lblkno, PSIZE, 0);
		if (mp == NULL) {
			IREAD_UNLOCK(ipbmap);
			return -EIO;
		}
		dp = (struct dmap *) mp->data;

		nb = min(rem, BPERDMAP - (blkno & (BPERDMAP - 1)));

		
		if ((rc = dbFreeDmap(bmp, dp, blkno, nb))) {
			jfs_error(ip->i_sb, "dbFree: error in block map\n");
			release_metapage(mp);
			IREAD_UNLOCK(ipbmap);
			return (rc);
		}
	}

	
	write_metapage(mp);

	IREAD_UNLOCK(ipbmap);

	return (0);
}


int
dbUpdatePMap(struct inode *ipbmap,
	     int free, s64 blkno, s64 nblocks, struct tblock * tblk)
{
	int nblks, dbitno, wbitno, rbits;
	int word, nbits, nwords;
	struct bmap *bmp = JFS_SBI(ipbmap->i_sb)->bmap;
	s64 lblkno, rem, lastlblkno;
	u32 mask;
	struct dmap *dp;
	struct metapage *mp;
	struct jfs_log *log;
	int lsn, difft, diffp;
	unsigned long flags;

	
	if (blkno + nblocks > bmp->db_mapsize) {
		printk(KERN_ERR "blkno = %Lx, nblocks = %Lx\n",
		       (unsigned long long) blkno,
		       (unsigned long long) nblocks);
		jfs_error(ipbmap->i_sb,
			  "dbUpdatePMap: blocks are outside the map");
		return -EIO;
	}

	
	lsn = tblk->lsn;
	log = (struct jfs_log *) JFS_SBI(tblk->sb)->log;
	logdiff(difft, lsn, log);

	mp = NULL;
	lastlblkno = 0;
	for (rem = nblocks; rem > 0; rem -= nblks, blkno += nblks) {
		
		lblkno = BLKTODMAP(blkno, bmp->db_l2nbperpage);
		if (lblkno != lastlblkno) {
			if (mp) {
				write_metapage(mp);
			}

			mp = read_metapage(bmp->db_ipbmap, lblkno, PSIZE,
					   0);
			if (mp == NULL)
				return -EIO;
			metapage_wait_for_io(mp);
		}
		dp = (struct dmap *) mp->data;

		dbitno = blkno & (BPERDMAP - 1);
		word = dbitno >> L2DBWORD;
		nblks = min(rem, (s64)BPERDMAP - dbitno);

		for (rbits = nblks; rbits > 0;
		     rbits -= nbits, dbitno += nbits) {
			wbitno = dbitno & (DBWORD - 1);
			nbits = min(rbits, DBWORD - wbitno);

			
			if (nbits < DBWORD) {
				mask =
				    (ONES << (DBWORD - nbits) >> wbitno);
				if (free)
					dp->pmap[word] &=
					    cpu_to_le32(~mask);
				else
					dp->pmap[word] |=
					    cpu_to_le32(mask);

				word += 1;
			} else {
				nwords = rbits >> L2DBWORD;
				nbits = nwords << L2DBWORD;

				if (free)
					memset(&dp->pmap[word], 0,
					       nwords * 4);
				else
					memset(&dp->pmap[word], (int) ONES,
					       nwords * 4);

				word += nwords;
			}
		}

		if (lblkno == lastlblkno)
			continue;

		lastlblkno = lblkno;

		LOGSYNC_LOCK(log, flags);
		if (mp->lsn != 0) {
			
			logdiff(diffp, mp->lsn, log);
			if (difft < diffp) {
				mp->lsn = lsn;

				
				list_move(&mp->synclist, &tblk->synclist);
			}

			
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
	}

	
	if (mp) {
		write_metapage(mp);
	}

	return (0);
}


int dbNextAG(struct inode *ipbmap)
{
	s64 avgfree;
	int agpref;
	s64 hwm = 0;
	int i;
	int next_best = -1;
	struct bmap *bmp = JFS_SBI(ipbmap->i_sb)->bmap;

	BMAP_LOCK(bmp);

	
	avgfree = (u32)bmp->db_nfree / bmp->db_numag;

	agpref = bmp->db_agpref;
	if ((atomic_read(&bmp->db_active[agpref]) == 0) &&
	    (bmp->db_agfree[agpref] >= avgfree))
		goto unlock;

	for (i = 0 ; i < bmp->db_numag; i++, agpref++) {
		if (agpref == bmp->db_numag)
			agpref = 0;

		if (atomic_read(&bmp->db_active[agpref]))
			
			continue;
		if (bmp->db_agfree[agpref] >= avgfree) {
			
			bmp->db_agpref = agpref;
			goto unlock;
		} else if (bmp->db_agfree[agpref] > hwm) {
			
			hwm = bmp->db_agfree[agpref];
			next_best = agpref;
		}
	}

	if (next_best != -1)
		bmp->db_agpref = next_best;
	
unlock:
	BMAP_UNLOCK(bmp);

	return (bmp->db_agpref);
}

int dbAlloc(struct inode *ip, s64 hint, s64 nblocks, s64 * results)
{
	int rc, agno;
	struct inode *ipbmap = JFS_SBI(ip->i_sb)->ipbmap;
	struct bmap *bmp;
	struct metapage *mp;
	s64 lblkno, blkno;
	struct dmap *dp;
	int l2nb;
	s64 mapSize;
	int writers;

	
	assert(nblocks > 0);

	l2nb = BLKSTOL2(nblocks);

	bmp = JFS_SBI(ip->i_sb)->bmap;

	mapSize = bmp->db_mapsize;

	
	if (hint >= mapSize) {
		jfs_error(ip->i_sb, "dbAlloc: the hint is outside the map");
		return -EIO;
	}

	if (l2nb > bmp->db_agl2size) {
		IWRITE_LOCK(ipbmap, RDWRLOCK_DMAP);

		rc = dbAllocAny(bmp, nblocks, l2nb, results);

		goto write_unlock;
	}

	if (hint == 0)
		goto pref_ag;

	blkno = hint + 1;

	if (blkno >= bmp->db_mapsize)
		goto pref_ag;

	agno = blkno >> bmp->db_agl2size;

	if ((blkno & (bmp->db_agsize - 1)) == 0)
		/* check if the AG is currently being written to.
		 * if so, call dbNextAG() to find a non-busy
		 * AG with sufficient free space.
		 */
		if (atomic_read(&bmp->db_active[agno]))
			goto pref_ag;

	if (nblocks <= BPERDMAP) {
		IREAD_LOCK(ipbmap, RDWRLOCK_DMAP);

		rc = -EIO;
		lblkno = BLKTODMAP(blkno, bmp->db_l2nbperpage);
		mp = read_metapage(ipbmap, lblkno, PSIZE, 0);
		if (mp == NULL)
			goto read_unlock;

		dp = (struct dmap *) mp->data;

		if ((rc = dbAllocNext(bmp, dp, blkno, (int) nblocks))
		    != -ENOSPC) {
			if (rc == 0) {
				*results = blkno;
				mark_metapage_dirty(mp);
			}

			release_metapage(mp);
			goto read_unlock;
		}

		writers = atomic_read(&bmp->db_active[agno]);
		if ((writers > 1) ||
		    ((writers == 1) && (JFS_IP(ip)->active_ag != agno))) {
			release_metapage(mp);
			IREAD_UNLOCK(ipbmap);
			goto pref_ag;
		}

		if ((rc =
		     dbAllocNear(bmp, dp, blkno, (int) nblocks, l2nb, results))
		    != -ENOSPC) {
			if (rc == 0)
				mark_metapage_dirty(mp);

			release_metapage(mp);
			goto read_unlock;
		}

		if ((rc = dbAllocDmapLev(bmp, dp, (int) nblocks, l2nb, results))
		    != -ENOSPC) {
			if (rc == 0)
				mark_metapage_dirty(mp);

			release_metapage(mp);
			goto read_unlock;
		}

		release_metapage(mp);
		IREAD_UNLOCK(ipbmap);
	}

	IWRITE_LOCK(ipbmap, RDWRLOCK_DMAP);
	if ((rc = dbAllocAG(bmp, agno, nblocks, l2nb, results)) != -ENOSPC)
		goto write_unlock;

	IWRITE_UNLOCK(ipbmap);


      pref_ag:
	agno = dbNextAG(ipbmap);
	IWRITE_LOCK(ipbmap, RDWRLOCK_DMAP);

	if ((rc = dbAllocAG(bmp, agno, nblocks, l2nb, results)) == -ENOSPC)
		rc = dbAllocAny(bmp, nblocks, l2nb, results);

      write_unlock:
	IWRITE_UNLOCK(ipbmap);

	return (rc);

      read_unlock:
	IREAD_UNLOCK(ipbmap);

	return (rc);
}

#ifdef _NOTYET
int dbAllocExact(struct inode *ip, s64 blkno, int nblocks)
{
	int rc;
	struct inode *ipbmap = JFS_SBI(ip->i_sb)->ipbmap;
	struct bmap *bmp = JFS_SBI(ip->i_sb)->bmap;
	struct dmap *dp;
	s64 lblkno;
	struct metapage *mp;

	IREAD_LOCK(ipbmap, RDWRLOCK_DMAP);

	if (nblocks <= 0 || nblocks > BPERDMAP || blkno >= bmp->db_mapsize) {
		IREAD_UNLOCK(ipbmap);
		return -EINVAL;
	}

	if (nblocks > ((s64) 1 << bmp->db_maxfreebud)) {
		
		IREAD_UNLOCK(ipbmap);
		return -ENOSPC;
	}

	
	lblkno = BLKTODMAP(blkno, bmp->db_l2nbperpage);
	mp = read_metapage(ipbmap, lblkno, PSIZE, 0);
	if (mp == NULL) {
		IREAD_UNLOCK(ipbmap);
		return -EIO;
	}
	dp = (struct dmap *) mp->data;

	
	rc = dbAllocNext(bmp, dp, blkno, nblocks);

	IREAD_UNLOCK(ipbmap);

	if (rc == 0)
		mark_metapage_dirty(mp);

	release_metapage(mp);

	return (rc);
}
#endif 

int
dbReAlloc(struct inode *ip,
	  s64 blkno, s64 nblocks, s64 addnblocks, s64 * results)
{
	int rc;

	if ((rc = dbExtend(ip, blkno, nblocks, addnblocks)) == 0) {
		*results = blkno;
		return (0);
	} else {
		if (rc != -ENOSPC)
			return (rc);
	}

	return (dbAlloc
		(ip, blkno + nblocks - 1, addnblocks + nblocks, results));
}


static int dbExtend(struct inode *ip, s64 blkno, s64 nblocks, s64 addnblocks)
{
	struct jfs_sb_info *sbi = JFS_SBI(ip->i_sb);
	s64 lblkno, lastblkno, extblkno;
	uint rel_block;
	struct metapage *mp;
	struct dmap *dp;
	int rc;
	struct inode *ipbmap = sbi->ipbmap;
	struct bmap *bmp;

	if (((rel_block = blkno & (sbi->nbperpage - 1))) &&
	    (rel_block + nblocks + addnblocks > sbi->nbperpage))
		return -ENOSPC;

	
	lastblkno = blkno + nblocks - 1;

	extblkno = lastblkno + 1;

	IREAD_LOCK(ipbmap, RDWRLOCK_DMAP);

	
	bmp = sbi->bmap;
	if (lastblkno < 0 || lastblkno >= bmp->db_mapsize) {
		IREAD_UNLOCK(ipbmap);
		jfs_error(ip->i_sb,
			  "dbExtend: the block is outside the filesystem");
		return -EIO;
	}

	if (addnblocks > BPERDMAP || extblkno >= bmp->db_mapsize ||
	    (extblkno & (bmp->db_agsize - 1)) == 0) {
		IREAD_UNLOCK(ipbmap);
		return -ENOSPC;
	}

	lblkno = BLKTODMAP(extblkno, bmp->db_l2nbperpage);
	mp = read_metapage(ipbmap, lblkno, PSIZE, 0);
	if (mp == NULL) {
		IREAD_UNLOCK(ipbmap);
		return -EIO;
	}

	dp = (struct dmap *) mp->data;

	rc = dbAllocNext(bmp, dp, extblkno, (int) addnblocks);

	IREAD_UNLOCK(ipbmap);

	
	if (rc == 0)
		write_metapage(mp);
	else
		
		release_metapage(mp);


	return (rc);
}


static int dbAllocNext(struct bmap * bmp, struct dmap * dp, s64 blkno,
		       int nblocks)
{
	int dbitno, word, rembits, nb, nwords, wbitno, nw;
	int l2size;
	s8 *leaf;
	u32 mask;

	if (dp->tree.leafidx != cpu_to_le32(LEAFIND)) {
		jfs_error(bmp->db_ipbmap->i_sb,
			  "dbAllocNext: Corrupt dmap page");
		return -EIO;
	}

	leaf = dp->tree.stree + le32_to_cpu(dp->tree.leafidx);

	dbitno = blkno & (BPERDMAP - 1);
	word = dbitno >> L2DBWORD;

	if (dbitno + nblocks > BPERDMAP)
		return -ENOSPC;

	if (leaf[word] == NOFREE)
		return -ENOSPC;

	for (rembits = nblocks; rembits > 0; rembits -= nb, dbitno += nb) {
		wbitno = dbitno & (DBWORD - 1);
		nb = min(rembits, DBWORD - wbitno);

		if (nb < DBWORD) {
			mask = (ONES << (DBWORD - nb) >> wbitno);
			if ((mask & ~le32_to_cpu(dp->wmap[word])) != mask)
				return -ENOSPC;

			word += 1;
		} else {
			nwords = rembits >> L2DBWORD;
			nb = nwords << L2DBWORD;

			while (nwords > 0) {
				if (leaf[word] < BUDMIN)
					return -ENOSPC;

				l2size =
				    min((int)leaf[word], NLSTOL2BSZ(nwords));

				nw = BUDSIZE(l2size, BUDMIN);

				nwords -= nw;
				word += nw;
			}
		}
	}

	return (dbAllocDmap(bmp, dp, blkno, nblocks));
}


static int
dbAllocNear(struct bmap * bmp,
	    struct dmap * dp, s64 blkno, int nblocks, int l2nb, s64 * results)
{
	int word, lword, rc;
	s8 *leaf;

	if (dp->tree.leafidx != cpu_to_le32(LEAFIND)) {
		jfs_error(bmp->db_ipbmap->i_sb,
			  "dbAllocNear: Corrupt dmap page");
		return -EIO;
	}

	leaf = dp->tree.stree + le32_to_cpu(dp->tree.leafidx);

	word = (blkno & (BPERDMAP - 1)) >> L2DBWORD;
	lword = min(word + 4, LPERDMAP);

	for (; word < lword; word++) {
		if (leaf[word] < l2nb)
			continue;

		blkno = le64_to_cpu(dp->start) + (word << L2DBWORD);

		if (leaf[word] < BUDMIN)
			blkno +=
			    dbFindBits(le32_to_cpu(dp->wmap[word]), l2nb);

		if ((rc = dbAllocDmap(bmp, dp, blkno, nblocks)) == 0)
			*results = blkno;

		return (rc);
	}

	return -ENOSPC;
}


static int
dbAllocAG(struct bmap * bmp, int agno, s64 nblocks, int l2nb, s64 * results)
{
	struct metapage *mp;
	struct dmapctl *dcp;
	int rc, ti, i, k, m, n, agperlev;
	s64 blkno, lblkno;
	int budmin;

	if (l2nb > bmp->db_agl2size) {
		jfs_error(bmp->db_ipbmap->i_sb,
			  "dbAllocAG: allocation request is larger than the "
			  "allocation group size");
		return -EIO;
	}

	blkno = (s64) agno << bmp->db_agl2size;

	if (bmp->db_agsize == BPERDMAP
	    || bmp->db_agfree[agno] == bmp->db_agsize) {
		rc = dbAllocCtl(bmp, nblocks, l2nb, blkno, results);
		if ((rc == -ENOSPC) &&
		    (bmp->db_agfree[agno] == bmp->db_agsize)) {
			printk(KERN_ERR "blkno = %Lx, blocks = %Lx\n",
			       (unsigned long long) blkno,
			       (unsigned long long) nblocks);
			jfs_error(bmp->db_ipbmap->i_sb,
				  "dbAllocAG: dbAllocCtl failed in free AG");
		}
		return (rc);
	}

	lblkno = BLKTOCTL(blkno, bmp->db_l2nbperpage, bmp->db_aglevel);
	mp = read_metapage(bmp->db_ipbmap, lblkno, PSIZE, 0);
	if (mp == NULL)
		return -EIO;
	dcp = (struct dmapctl *) mp->data;
	budmin = dcp->budmin;

	if (dcp->leafidx != cpu_to_le32(CTLLEAFIND)) {
		jfs_error(bmp->db_ipbmap->i_sb,
			  "dbAllocAG: Corrupt dmapctl page");
		release_metapage(mp);
		return -EIO;
	}

	agperlev =
	    (1 << (L2LPERCTL - (bmp->db_agheight << 1))) / bmp->db_agwidth;
	ti = bmp->db_agstart + bmp->db_agwidth * (agno & (agperlev - 1));

	for (i = 0; i < bmp->db_agwidth; i++, ti++) {
		if (l2nb > dcp->stree[ti])
			continue;

		for (k = bmp->db_agheight; k > 0; k--) {
			for (n = 0, m = (ti << 2) + 1; n < 4; n++) {
				if (l2nb <= dcp->stree[m + n]) {
					ti = m + n;
					break;
				}
			}
			if (n == 4) {
				jfs_error(bmp->db_ipbmap->i_sb,
					  "dbAllocAG: failed descending stree");
				release_metapage(mp);
				return -EIO;
			}
		}

		if (bmp->db_aglevel == 2)
			blkno = 0;
		else if (bmp->db_aglevel == 1)
			blkno &= ~(MAXL1SIZE - 1);
		else		
			blkno &= ~(MAXL0SIZE - 1);

		blkno +=
		    ((s64) (ti - le32_to_cpu(dcp->leafidx))) << budmin;

		release_metapage(mp);

		if (l2nb < budmin) {

			if ((rc =
			     dbFindCtl(bmp, l2nb, bmp->db_aglevel - 1,
				       &blkno))) {
				if (rc == -ENOSPC) {
					jfs_error(bmp->db_ipbmap->i_sb,
						  "dbAllocAG: control page "
						  "inconsistent");
					return -EIO;
				}
				return (rc);
			}
		}

		rc = dbAllocCtl(bmp, nblocks, l2nb, blkno, results);
		if (rc == -ENOSPC) {
			jfs_error(bmp->db_ipbmap->i_sb,
				  "dbAllocAG: unable to allocate blocks");
			rc = -EIO;
		}
		return (rc);
	}

	release_metapage(mp);

	return -ENOSPC;
}


static int dbAllocAny(struct bmap * bmp, s64 nblocks, int l2nb, s64 * results)
{
	int rc;
	s64 blkno = 0;

	if ((rc = dbFindCtl(bmp, l2nb, bmp->db_maxlevel, &blkno)))
		return (rc);

	rc = dbAllocCtl(bmp, nblocks, l2nb, blkno, results);
	if (rc == -ENOSPC) {
		jfs_error(bmp->db_ipbmap->i_sb,
			  "dbAllocAny: unable to allocate blocks");
		return -EIO;
	}
	return (rc);
}


static int dbFindCtl(struct bmap * bmp, int l2nb, int level, s64 * blkno)
{
	int rc, leafidx, lev;
	s64 b, lblkno;
	struct dmapctl *dcp;
	int budmin;
	struct metapage *mp;

	for (lev = level, b = *blkno; lev >= 0; lev--) {
		lblkno = BLKTOCTL(b, bmp->db_l2nbperpage, lev);
		mp = read_metapage(bmp->db_ipbmap, lblkno, PSIZE, 0);
		if (mp == NULL)
			return -EIO;
		dcp = (struct dmapctl *) mp->data;
		budmin = dcp->budmin;

		if (dcp->leafidx != cpu_to_le32(CTLLEAFIND)) {
			jfs_error(bmp->db_ipbmap->i_sb,
				  "dbFindCtl: Corrupt dmapctl page");
			release_metapage(mp);
			return -EIO;
		}

		rc = dbFindLeaf((dmtree_t *) dcp, l2nb, &leafidx);

		release_metapage(mp);

		if (rc) {
			if (lev != level) {
				jfs_error(bmp->db_ipbmap->i_sb,
					  "dbFindCtl: dmap inconsistent");
				return -EIO;
			}
			return -ENOSPC;
		}

		b += (((s64) leafidx) << budmin);

		if (l2nb >= budmin)
			break;
	}

	*blkno = b;
	return (0);
}


static int
dbAllocCtl(struct bmap * bmp, s64 nblocks, int l2nb, s64 blkno, s64 * results)
{
	int rc, nb;
	s64 b, lblkno, n;
	struct metapage *mp;
	struct dmap *dp;

	if (l2nb <= L2BPERDMAP) {
		lblkno = BLKTODMAP(blkno, bmp->db_l2nbperpage);
		mp = read_metapage(bmp->db_ipbmap, lblkno, PSIZE, 0);
		if (mp == NULL)
			return -EIO;
		dp = (struct dmap *) mp->data;

		rc = dbAllocDmapLev(bmp, dp, (int) nblocks, l2nb, results);
		if (rc == 0)
			mark_metapage_dirty(mp);

		release_metapage(mp);

		return (rc);
	}

	assert((blkno & (BPERDMAP - 1)) == 0);

	for (n = nblocks, b = blkno; n > 0; n -= nb, b += nb) {
		lblkno = BLKTODMAP(b, bmp->db_l2nbperpage);
		mp = read_metapage(bmp->db_ipbmap, lblkno, PSIZE, 0);
		if (mp == NULL) {
			rc = -EIO;
			goto backout;
		}
		dp = (struct dmap *) mp->data;

		if (dp->tree.stree[ROOT] != L2BPERDMAP) {
			release_metapage(mp);
			jfs_error(bmp->db_ipbmap->i_sb,
				  "dbAllocCtl: the dmap is not all free");
			rc = -EIO;
			goto backout;
		}

		nb = min(n, (s64)BPERDMAP);

		if ((rc = dbAllocDmap(bmp, dp, b, nb))) {
			release_metapage(mp);
			goto backout;
		}

		write_metapage(mp);
	}

	*results = blkno;
	return (0);

      backout:

	for (n = nblocks - n, b = blkno; n > 0;
	     n -= BPERDMAP, b += BPERDMAP) {
		lblkno = BLKTODMAP(b, bmp->db_l2nbperpage);
		mp = read_metapage(bmp->db_ipbmap, lblkno, PSIZE, 0);
		if (mp == NULL) {
			jfs_error(bmp->db_ipbmap->i_sb,
				  "dbAllocCtl: I/O Error: Block Leakage.");
			continue;
		}
		dp = (struct dmap *) mp->data;

		if (dbFreeDmap(bmp, dp, b, BPERDMAP)) {
			release_metapage(mp);
			jfs_error(bmp->db_ipbmap->i_sb,
				  "dbAllocCtl: Block Leakage.");
			continue;
		}

		write_metapage(mp);
	}

	return (rc);
}


static int
dbAllocDmapLev(struct bmap * bmp,
	       struct dmap * dp, int nblocks, int l2nb, s64 * results)
{
	s64 blkno;
	int leafidx, rc;

	
	assert(l2nb <= L2BPERDMAP);

	if (dbFindLeaf((dmtree_t *) & dp->tree, l2nb, &leafidx))
		return -ENOSPC;

	blkno = le64_to_cpu(dp->start) + (leafidx << L2DBWORD);

	if (dp->tree.stree[leafidx + LEAFIND] < BUDMIN)
		blkno += dbFindBits(le32_to_cpu(dp->wmap[leafidx]), l2nb);

	
	if ((rc = dbAllocDmap(bmp, dp, blkno, nblocks)) == 0)
		*results = blkno;

	return (rc);
}


static int dbAllocDmap(struct bmap * bmp, struct dmap * dp, s64 blkno,
		       int nblocks)
{
	s8 oldroot;
	int rc;

	oldroot = dp->tree.stree[ROOT];

	
	dbAllocBits(bmp, dp, blkno, nblocks);

	
	if (dp->tree.stree[ROOT] == oldroot)
		return (0);

	if ((rc = dbAdjCtl(bmp, blkno, dp->tree.stree[ROOT], 1, 0)))
		dbFreeBits(bmp, dp, blkno, nblocks);

	return (rc);
}


static int dbFreeDmap(struct bmap * bmp, struct dmap * dp, s64 blkno,
		      int nblocks)
{
	s8 oldroot;
	int rc = 0, word;

	oldroot = dp->tree.stree[ROOT];

	
	rc = dbFreeBits(bmp, dp, blkno, nblocks);

	
	if (rc || (dp->tree.stree[ROOT] == oldroot))
		return (rc);

	if ((rc = dbAdjCtl(bmp, blkno, dp->tree.stree[ROOT], 0, 0))) {
		word = (blkno & (BPERDMAP - 1)) >> L2DBWORD;

		if (dp->tree.stree[word] == NOFREE)
			dbBackSplit((dmtree_t *) & dp->tree, word);

		dbAllocBits(bmp, dp, blkno, nblocks);
	}

	return (rc);
}


static void dbAllocBits(struct bmap * bmp, struct dmap * dp, s64 blkno,
			int nblocks)
{
	int dbitno, word, rembits, nb, nwords, wbitno, nw, agno;
	dmtree_t *tp = (dmtree_t *) & dp->tree;
	int size;
	s8 *leaf;

	
	leaf = dp->tree.stree + LEAFIND;

	dbitno = blkno & (BPERDMAP - 1);
	word = dbitno >> L2DBWORD;

	
	assert(dbitno + nblocks <= BPERDMAP);

	for (rembits = nblocks; rembits > 0; rembits -= nb, dbitno += nb) {
		wbitno = dbitno & (DBWORD - 1);
		nb = min(rembits, DBWORD - wbitno);

		if (nb < DBWORD) {
			dp->wmap[word] |= cpu_to_le32(ONES << (DBWORD - nb)
						      >> wbitno);

			dbSplit(tp, word, BUDMIN,
				dbMaxBud((u8 *) & dp->wmap[word]));

			word += 1;
		} else {
			nwords = rembits >> L2DBWORD;
			memset(&dp->wmap[word], (int) ONES, nwords * 4);

			nb = nwords << L2DBWORD;

			for (; nwords > 0; nwords -= nw) {
				if (leaf[word] < BUDMIN) {
					jfs_error(bmp->db_ipbmap->i_sb,
						  "dbAllocBits: leaf page "
						  "corrupt");
					break;
				}

				size = min((int)leaf[word], NLSTOL2BSZ(nwords));

				dbSplit(tp, word, size, NOFREE);

				
				nw = BUDSIZE(size, BUDMIN);
				word += nw;
			}
		}
	}

	
	le32_add_cpu(&dp->nfree, -nblocks);

	BMAP_LOCK(bmp);

	agno = blkno >> bmp->db_agl2size;
	if (agno > bmp->db_maxag)
		bmp->db_maxag = agno;

	
	bmp->db_agfree[agno] -= nblocks;
	bmp->db_nfree -= nblocks;

	BMAP_UNLOCK(bmp);
}


static int dbFreeBits(struct bmap * bmp, struct dmap * dp, s64 blkno,
		       int nblocks)
{
	int dbitno, word, rembits, nb, nwords, wbitno, nw, agno;
	dmtree_t *tp = (dmtree_t *) & dp->tree;
	int rc = 0;
	int size;

	dbitno = blkno & (BPERDMAP - 1);
	word = dbitno >> L2DBWORD;

	assert(dbitno + nblocks <= BPERDMAP);

	for (rembits = nblocks; rembits > 0; rembits -= nb, dbitno += nb) {
		wbitno = dbitno & (DBWORD - 1);
		nb = min(rembits, DBWORD - wbitno);

		if (nb < DBWORD) {
			dp->wmap[word] &=
			    cpu_to_le32(~(ONES << (DBWORD - nb)
					  >> wbitno));

			rc = dbJoin(tp, word,
				    dbMaxBud((u8 *) & dp->wmap[word]));
			if (rc)
				return rc;

			word += 1;
		} else {
			nwords = rembits >> L2DBWORD;
			memset(&dp->wmap[word], 0, nwords * 4);

			nb = nwords << L2DBWORD;

			for (; nwords > 0; nwords -= nw) {
				size =
				    min(LITOL2BSZ
					(word, L2LPERDMAP, BUDMIN),
					NLSTOL2BSZ(nwords));

				rc = dbJoin(tp, word, size);
				if (rc)
					return rc;

				nw = BUDSIZE(size, BUDMIN);
				word += nw;
			}
		}
	}

	le32_add_cpu(&dp->nfree, nblocks);

	BMAP_LOCK(bmp);

	agno = blkno >> bmp->db_agl2size;
	bmp->db_nfree += nblocks;
	bmp->db_agfree[agno] += nblocks;

	if ((bmp->db_agfree[agno] == bmp->db_agsize && agno == bmp->db_maxag) ||
	    (agno == bmp->db_numag - 1 &&
	     bmp->db_agfree[agno] == (bmp-> db_mapsize & (BPERDMAP - 1)))) {
		while (bmp->db_maxag > 0) {
			bmp->db_maxag -= 1;
			if (bmp->db_agfree[bmp->db_maxag] !=
			    bmp->db_agsize)
				break;
		}

		if (bmp->db_agpref > bmp->db_maxag)
			bmp->db_agpref = bmp->db_maxag;
	}

	BMAP_UNLOCK(bmp);

	return 0;
}


static int
dbAdjCtl(struct bmap * bmp, s64 blkno, int newval, int alloc, int level)
{
	struct metapage *mp;
	s8 oldroot;
	int oldval;
	s64 lblkno;
	struct dmapctl *dcp;
	int rc, leafno, ti;

	lblkno = BLKTOCTL(blkno, bmp->db_l2nbperpage, level);
	mp = read_metapage(bmp->db_ipbmap, lblkno, PSIZE, 0);
	if (mp == NULL)
		return -EIO;
	dcp = (struct dmapctl *) mp->data;

	if (dcp->leafidx != cpu_to_le32(CTLLEAFIND)) {
		jfs_error(bmp->db_ipbmap->i_sb,
			  "dbAdjCtl: Corrupt dmapctl page");
		release_metapage(mp);
		return -EIO;
	}

	leafno = BLKTOCTLLEAF(blkno, dcp->budmin);
	ti = leafno + le32_to_cpu(dcp->leafidx);

	oldval = dcp->stree[ti];
	oldroot = dcp->stree[ROOT];

	if (alloc) {
		if (oldval == NOFREE) {
			rc = dbBackSplit((dmtree_t *) dcp, leafno);
			if (rc)
				return rc;
			oldval = dcp->stree[ti];
		}
		dbSplit((dmtree_t *) dcp, leafno, dcp->budmin, newval);
	} else {
		rc = dbJoin((dmtree_t *) dcp, leafno, newval);
		if (rc)
			return rc;
	}

	if (dcp->stree[ROOT] != oldroot) {
		if (level < bmp->db_maxlevel) {
			if ((rc =
			     dbAdjCtl(bmp, blkno, dcp->stree[ROOT], alloc,
				      level + 1))) {
				if (alloc) {
					dbJoin((dmtree_t *) dcp, leafno,
					       oldval);
				} else {
					if (dcp->stree[ti] == NOFREE)
						dbBackSplit((dmtree_t *)
							    dcp, leafno);
					dbSplit((dmtree_t *) dcp, leafno,
						dcp->budmin, oldval);
				}

				release_metapage(mp);
				return (rc);
			}
		} else {
			assert(level == bmp->db_maxlevel);
			if (bmp->db_maxfreebud != oldroot) {
				jfs_error(bmp->db_ipbmap->i_sb,
					  "dbAdjCtl: the maximum free buddy is "
					  "not the old root");
			}
			bmp->db_maxfreebud = dcp->stree[ROOT];
		}
	}

	write_metapage(mp);

	return (0);
}


static void dbSplit(dmtree_t * tp, int leafno, int splitsz, int newval)
{
	int budsz;
	int cursz;
	s8 *leaf = tp->dmt_stree + le32_to_cpu(tp->dmt_leafidx);

	if (leaf[leafno] > tp->dmt_budmin) {
		cursz = leaf[leafno] - 1;
		budsz = BUDSIZE(cursz, tp->dmt_budmin);

		while (cursz >= splitsz) {
			dbAdjTree(tp, leafno ^ budsz, cursz);

			cursz -= 1;
			budsz >>= 1;
		}
	}

	dbAdjTree(tp, leafno, newval);
}


static int dbBackSplit(dmtree_t * tp, int leafno)
{
	int budsz, bud, w, bsz, size;
	int cursz;
	s8 *leaf = tp->dmt_stree + le32_to_cpu(tp->dmt_leafidx);

	assert(leaf[leafno] == NOFREE);

	size =
	    LITOL2BSZ(leafno, le32_to_cpu(tp->dmt_l2nleafs),
		      tp->dmt_budmin);

	budsz = BUDSIZE(size, tp->dmt_budmin);

	while (leaf[leafno] == NOFREE) {
		for (w = leafno, bsz = budsz;; bsz <<= 1,
		     w = (w < bud) ? w : bud) {
			if (bsz >= le32_to_cpu(tp->dmt_nleafs)) {
				jfs_err("JFS: block map error in dbBackSplit");
				return -EIO;
			}

			bud = w ^ bsz;

			if (leaf[bud] != NOFREE) {
				cursz = leaf[bud] - 1;
				dbSplit(tp, bud, cursz, cursz);
				break;
			}
		}
	}

	if (leaf[leafno] != size) {
		jfs_err("JFS: wrong leaf value in dbBackSplit");
		return -EIO;
	}
	return 0;
}


static int dbJoin(dmtree_t * tp, int leafno, int newval)
{
	int budsz, buddy;
	s8 *leaf;

	if (newval >= tp->dmt_budmin) {
		leaf = tp->dmt_stree + le32_to_cpu(tp->dmt_leafidx);

		budsz = BUDSIZE(newval, tp->dmt_budmin);

		while (budsz < le32_to_cpu(tp->dmt_nleafs)) {
			buddy = leafno ^ budsz;

			if (newval > leaf[buddy])
				break;

			
			if (newval < leaf[buddy])
				return -EIO;

			if (leafno < buddy) {
				dbAdjTree(tp, buddy, NOFREE);
			} else {
				dbAdjTree(tp, leafno, NOFREE);
				leafno = buddy;
			}

			newval += 1;
			budsz <<= 1;
		}
	}

	dbAdjTree(tp, leafno, newval);

	return 0;
}


static void dbAdjTree(dmtree_t * tp, int leafno, int newval)
{
	int lp, pp, k;
	int max;

	lp = leafno + le32_to_cpu(tp->dmt_leafidx);

	if (tp->dmt_stree[lp] == newval)
		return;

	tp->dmt_stree[lp] = newval;

	for (k = 0; k < le32_to_cpu(tp->dmt_height); k++) {
		lp = ((lp - 1) & ~0x03) + 1;

		pp = (lp - 1) >> 2;

		max = TREEMAX(&tp->dmt_stree[lp]);

		if (tp->dmt_stree[pp] == max)
			break;

		tp->dmt_stree[pp] = max;

		lp = pp;
	}
}


static int dbFindLeaf(dmtree_t * tp, int l2nb, int *leafidx)
{
	int ti, n = 0, k, x = 0;

	if (l2nb > tp->dmt_stree[ROOT])
		return -ENOSPC;

	for (k = le32_to_cpu(tp->dmt_height), ti = 1;
	     k > 0; k--, ti = ((ti + n) << 2) + 1) {
		for (x = ti, n = 0; n < 4; n++) {
			if (l2nb <= tp->dmt_stree[x + n])
				break;
		}

		assert(n < 4);
	}

	*leafidx = x + n - le32_to_cpu(tp->dmt_leafidx);

	return (0);
}


static int dbFindBits(u32 word, int l2nb)
{
	int bitno, nb;
	u32 mask;

	nb = 1 << l2nb;
	assert(nb <= DBWORD);

	word = ~word;
	mask = ONES << (DBWORD - nb);

	for (bitno = 0; mask != 0; bitno += nb, mask >>= nb) {
		if ((mask & word) == mask)
			break;
	}

	ASSERT(bitno < 32);

	return (bitno);
}


static int dbMaxBud(u8 * cp)
{
	signed char tmp1, tmp2;

	if (*((uint *) cp) == 0)
		return (BUDMIN);

	if (*((u16 *) cp) == 0 || *((u16 *) cp + 1) == 0)
		return (BUDMIN - 1);

	tmp1 = max(budtab[cp[2]], budtab[cp[3]]);
	tmp2 = max(budtab[cp[0]], budtab[cp[1]]);
	return (max(tmp1, tmp2));
}


static int cnttz(u32 word)
{
	int n;

	for (n = 0; n < 32; n++, word >>= 1) {
		if (word & 0x01)
			break;
	}

	return (n);
}


static int cntlz(u32 value)
{
	int n;

	for (n = 0; n < 32; n++, value <<= 1) {
		if (value & HIGHORDER)
			break;
	}
	return (n);
}


static int blkstol2(s64 nb)
{
	int l2nb;
	s64 mask;		

	mask = (s64) 1 << (64 - 1);

	for (l2nb = 0; l2nb < 64; l2nb++, mask >>= 1) {
		if (nb & mask) {
			l2nb = (64 - 1) - l2nb;

			if (~mask & nb)
				l2nb++;

			return (l2nb);
		}
	}
	assert(0);
	return 0;		
}


int dbAllocBottomUp(struct inode *ip, s64 blkno, s64 nblocks)
{
	struct metapage *mp;
	struct dmap *dp;
	int nb, rc;
	s64 lblkno, rem;
	struct inode *ipbmap = JFS_SBI(ip->i_sb)->ipbmap;
	struct bmap *bmp = JFS_SBI(ip->i_sb)->bmap;

	IREAD_LOCK(ipbmap, RDWRLOCK_DMAP);

	
	ASSERT(nblocks <= bmp->db_mapsize - blkno);

	mp = NULL;
	for (rem = nblocks; rem > 0; rem -= nb, blkno += nb) {
		
		if (mp) {
			write_metapage(mp);
		}

		
		lblkno = BLKTODMAP(blkno, bmp->db_l2nbperpage);
		mp = read_metapage(ipbmap, lblkno, PSIZE, 0);
		if (mp == NULL) {
			IREAD_UNLOCK(ipbmap);
			return -EIO;
		}
		dp = (struct dmap *) mp->data;

		nb = min(rem, BPERDMAP - (blkno & (BPERDMAP - 1)));

		
		if ((rc = dbAllocDmapBU(bmp, dp, blkno, nb))) {
			release_metapage(mp);
			IREAD_UNLOCK(ipbmap);
			return (rc);
		}
	}

	
	write_metapage(mp);

	IREAD_UNLOCK(ipbmap);

	return (0);
}


static int dbAllocDmapBU(struct bmap * bmp, struct dmap * dp, s64 blkno,
			 int nblocks)
{
	int rc;
	int dbitno, word, rembits, nb, nwords, wbitno, agno;
	s8 oldroot;
	struct dmaptree *tp = (struct dmaptree *) & dp->tree;

	oldroot = tp->stree[ROOT];

	dbitno = blkno & (BPERDMAP - 1);
	word = dbitno >> L2DBWORD;

	
	assert(dbitno + nblocks <= BPERDMAP);

	for (rembits = nblocks; rembits > 0; rembits -= nb, dbitno += nb) {
		wbitno = dbitno & (DBWORD - 1);
		nb = min(rembits, DBWORD - wbitno);

		if (nb < DBWORD) {
			dp->wmap[word] |= cpu_to_le32(ONES << (DBWORD - nb)
						      >> wbitno);

			word++;
		} else {
			nwords = rembits >> L2DBWORD;
			memset(&dp->wmap[word], (int) ONES, nwords * 4);

			
			nb = nwords << L2DBWORD;
			word += nwords;
		}
	}

	
	le32_add_cpu(&dp->nfree, -nblocks);

	
	dbInitDmapTree(dp);

	BMAP_LOCK(bmp);

	agno = blkno >> bmp->db_agl2size;
	if (agno > bmp->db_maxag)
		bmp->db_maxag = agno;

	
	bmp->db_agfree[agno] -= nblocks;
	bmp->db_nfree -= nblocks;

	BMAP_UNLOCK(bmp);

	
	if (tp->stree[ROOT] == oldroot)
		return (0);

	if ((rc = dbAdjCtl(bmp, blkno, tp->stree[ROOT], 1, 0)))
		dbFreeBits(bmp, dp, blkno, nblocks);

	return (rc);
}


int dbExtendFS(struct inode *ipbmap, s64 blkno,	s64 nblocks)
{
	struct jfs_sb_info *sbi = JFS_SBI(ipbmap->i_sb);
	int nbperpage = sbi->nbperpage;
	int i, i0 = true, j, j0 = true, k, n;
	s64 newsize;
	s64 p;
	struct metapage *mp, *l2mp, *l1mp = NULL, *l0mp = NULL;
	struct dmapctl *l2dcp, *l1dcp, *l0dcp;
	struct dmap *dp;
	s8 *l0leaf, *l1leaf, *l2leaf;
	struct bmap *bmp = sbi->bmap;
	int agno, l2agsize, oldl2agsize;
	s64 ag_rem;

	newsize = blkno + nblocks;

	jfs_info("dbExtendFS: blkno:%Ld nblocks:%Ld newsize:%Ld",
		 (long long) blkno, (long long) nblocks, (long long) newsize);


	
	bmp->db_mapsize = newsize;
	bmp->db_maxlevel = BMAPSZTOLEV(bmp->db_mapsize);

	
	l2agsize = dbGetL2AGSize(newsize);
	oldl2agsize = bmp->db_agl2size;

	bmp->db_agl2size = l2agsize;
	bmp->db_agsize = 1 << l2agsize;

	
	agno = bmp->db_numag;
	bmp->db_numag = newsize >> l2agsize;
	bmp->db_numag += ((u32) newsize % (u32) bmp->db_agsize) ? 1 : 0;

	if (l2agsize == oldl2agsize)
		goto extend;
	k = 1 << (l2agsize - oldl2agsize);
	ag_rem = bmp->db_agfree[0];	
	for (i = 0, n = 0; i < agno; n++) {
		bmp->db_agfree[n] = 0;	

		
		for (j = 0; j < k && i < agno; j++, i++) {
			
			bmp->db_agfree[n] += bmp->db_agfree[i];
		}
	}
	bmp->db_agfree[0] += ag_rem;	

	for (; n < MAXAG; n++)
		bmp->db_agfree[n] = 0;


	bmp->db_maxag = bmp->db_maxag / k;

      extend:
	
	p = BMAPBLKNO + nbperpage;	
	l2mp = read_metapage(ipbmap, p, PSIZE, 0);
	if (!l2mp) {
		jfs_error(ipbmap->i_sb, "dbExtendFS: L2 page could not be read");
		return -EIO;
	}
	l2dcp = (struct dmapctl *) l2mp->data;

	
	k = blkno >> L2MAXL1SIZE;
	l2leaf = l2dcp->stree + CTLLEAFIND + k;
	p = BLKTOL1(blkno, sbi->l2nbperpage);	

	for (; k < LPERCTL; k++, p += nbperpage) {
		
		if (j0) {
			
			l1mp = read_metapage(ipbmap, p, PSIZE, 0);
			if (l1mp == NULL)
				goto errout;
			l1dcp = (struct dmapctl *) l1mp->data;

			
			j = (blkno & (MAXL1SIZE - 1)) >> L2MAXL0SIZE;
			l1leaf = l1dcp->stree + CTLLEAFIND + j;
			p = BLKTOL0(blkno, sbi->l2nbperpage);
			j0 = false;
		} else {
			
			l1mp = get_metapage(ipbmap, p, PSIZE, 0);
			if (l1mp == NULL)
				goto errout;

			l1dcp = (struct dmapctl *) l1mp->data;

			
			j = 0;
			l1leaf = l1dcp->stree + CTLLEAFIND;
			p += nbperpage;	
		}

		for (; j < LPERCTL; j++) {
			
			if (i0) {
				

				l0mp = read_metapage(ipbmap, p, PSIZE, 0);
				if (l0mp == NULL)
					goto errout;
				l0dcp = (struct dmapctl *) l0mp->data;

				
				i = (blkno & (MAXL0SIZE - 1)) >>
				    L2BPERDMAP;
				l0leaf = l0dcp->stree + CTLLEAFIND + i;
				p = BLKTODMAP(blkno,
					      sbi->l2nbperpage);
				i0 = false;
			} else {
				
				l0mp = get_metapage(ipbmap, p, PSIZE, 0);
				if (l0mp == NULL)
					goto errout;

				l0dcp = (struct dmapctl *) l0mp->data;

				
				i = 0;
				l0leaf = l0dcp->stree + CTLLEAFIND;
				p += nbperpage;	
			}

			for (; i < LPERCTL; i++) {
				if ((n = blkno & (BPERDMAP - 1))) {
					
					mp = read_metapage(ipbmap, p,
							   PSIZE, 0);
					if (mp == NULL)
						goto errout;
					n = min(nblocks, (s64)BPERDMAP - n);
				} else {
					
					mp = read_metapage(ipbmap, p,
							   PSIZE, 0);
					if (mp == NULL)
						goto errout;

					n = min(nblocks, (s64)BPERDMAP);
				}

				dp = (struct dmap *) mp->data;
				*l0leaf = dbInitDmap(dp, blkno, n);

				bmp->db_nfree += n;
				agno = le64_to_cpu(dp->start) >> l2agsize;
				bmp->db_agfree[agno] += n;

				write_metapage(mp);

				l0leaf++;
				p += nbperpage;

				blkno += n;
				nblocks -= n;
				if (nblocks == 0)
					break;
			}	

			*l1leaf = dbInitDmapCtl(l0dcp, 0, ++i);
			write_metapage(l0mp);
			l0mp = NULL;

			if (nblocks)
				l1leaf++;	
			else {
				
				if (j > 0)
					break;	
				else {
					
					bmp->db_maxfreebud = *l1leaf;
					release_metapage(l1mp);
					release_metapage(l2mp);
					goto finalize;
				}
			}
		}		

		*l2leaf = dbInitDmapCtl(l1dcp, 1, ++j);
		write_metapage(l1mp);
		l1mp = NULL;

		if (nblocks)
			l2leaf++;	
		else {
			
			if (k > 0)
				break;	
			else {
				
				bmp->db_maxfreebud = *l2leaf;
				release_metapage(l2mp);
				goto finalize;
			}
		}
	}			

	jfs_error(ipbmap->i_sb,
		  "dbExtendFS: function has not returned as expected");
errout:
	if (l0mp)
		release_metapage(l0mp);
	if (l1mp)
		release_metapage(l1mp);
	release_metapage(l2mp);
	return -EIO;

finalize:

	return 0;
}


void dbFinalizeBmap(struct inode *ipbmap)
{
	struct bmap *bmp = JFS_SBI(ipbmap->i_sb)->bmap;
	int actags, inactags, l2nl;
	s64 ag_rem, actfree, inactfree, avgfree;
	int i, n;

	
	actags = bmp->db_maxag + 1;
	inactags = bmp->db_numag - actags;
	ag_rem = bmp->db_mapsize & (bmp->db_agsize - 1);	

	inactfree = (inactags && ag_rem) ?
	    ((inactags - 1) << bmp->db_agl2size) + ag_rem
	    : inactags << bmp->db_agl2size;

	actfree = bmp->db_nfree - inactfree;
	avgfree = (u32) actfree / (u32) actags;

	if (bmp->db_agfree[bmp->db_agpref] < avgfree) {
		for (bmp->db_agpref = 0; bmp->db_agpref < actags;
		     bmp->db_agpref++) {
			if (bmp->db_agfree[bmp->db_agpref] >= avgfree)
				break;
		}
		if (bmp->db_agpref >= bmp->db_numag) {
			jfs_error(ipbmap->i_sb,
				  "cannot find ag with average freespace");
		}
	}

	bmp->db_aglevel = BMAPSZTOLEV(bmp->db_agsize);
	l2nl =
	    bmp->db_agl2size - (L2BPERDMAP + bmp->db_aglevel * L2LPERCTL);
	bmp->db_agheight = l2nl >> 1;
	bmp->db_agwidth = 1 << (l2nl - (bmp->db_agheight << 1));
	for (i = 5 - bmp->db_agheight, bmp->db_agstart = 0, n = 1; i > 0;
	     i--) {
		bmp->db_agstart += n;
		n <<= 2;
	}

}


static int dbInitDmap(struct dmap * dp, s64 Blkno, int nblocks)
{
	int blkno, w, b, r, nw, nb, i;

	
	blkno = Blkno & (BPERDMAP - 1);

	if (blkno == 0) {
		dp->nblocks = dp->nfree = cpu_to_le32(nblocks);
		dp->start = cpu_to_le64(Blkno);

		if (nblocks == BPERDMAP) {
			memset(&dp->wmap[0], 0, LPERDMAP * 4);
			memset(&dp->pmap[0], 0, LPERDMAP * 4);
			goto initTree;
		}
	} else {
		le32_add_cpu(&dp->nblocks, nblocks);
		le32_add_cpu(&dp->nfree, nblocks);
	}

	
	w = blkno >> L2DBWORD;

	for (r = nblocks; r > 0; r -= nb, blkno += nb) {
		
		b = blkno & (DBWORD - 1);
		
		nb = min(r, DBWORD - b);

		
		if (nb < DBWORD) {
			
			dp->wmap[w] &= cpu_to_le32(~(ONES << (DBWORD - nb)
						     >> b));
			dp->pmap[w] &= cpu_to_le32(~(ONES << (DBWORD - nb)
						     >> b));

			
			w++;
		} else {
			
			nw = r >> L2DBWORD;
			memset(&dp->wmap[w], 0, nw * 4);
			memset(&dp->pmap[w], 0, nw * 4);

			
			nb = nw << L2DBWORD;
			w += nw;
		}
	}


	if (blkno == BPERDMAP)
		goto initTree;

	
	w = blkno >> L2DBWORD;

	
	b = blkno & (DBWORD - 1);
	if (b) {
		
		dp->wmap[w] = dp->pmap[w] = cpu_to_le32(ONES >> b);
		w++;
	}

	
	for (i = w; i < LPERDMAP; i++)
		dp->pmap[i] = dp->wmap[i] = cpu_to_le32(ONES);

      initTree:
	return (dbInitDmapTree(dp));
}


static int dbInitDmapTree(struct dmap * dp)
{
	struct dmaptree *tp;
	s8 *cp;
	int i;

	
	tp = &dp->tree;
	tp->nleafs = cpu_to_le32(LPERDMAP);
	tp->l2nleafs = cpu_to_le32(L2LPERDMAP);
	tp->leafidx = cpu_to_le32(LEAFIND);
	tp->height = cpu_to_le32(4);
	tp->budmin = BUDMIN;

	cp = tp->stree + le32_to_cpu(tp->leafidx);
	for (i = 0; i < LPERDMAP; i++)
		*cp++ = dbMaxBud((u8 *) & dp->wmap[i]);

	
	return (dbInitTree(tp));
}


static int dbInitTree(struct dmaptree * dtp)
{
	int l2max, l2free, bsize, nextb, i;
	int child, parent, nparent;
	s8 *tp, *cp, *cp1;

	tp = dtp->stree;

	
	l2max = le32_to_cpu(dtp->l2nleafs) + dtp->budmin;

	for (l2free = dtp->budmin, bsize = 1; l2free < l2max;
	     l2free++, bsize = nextb) {
		
		nextb = bsize << 1;

		
		for (i = 0, cp = tp + le32_to_cpu(dtp->leafidx);
		     i < le32_to_cpu(dtp->nleafs);
		     i += nextb, cp += nextb) {
			
			if (*cp == l2free && *(cp + bsize) == l2free) {
				*cp = l2free + 1;	
				*(cp + bsize) = -1;	
			}
		}
	}

	for (child = le32_to_cpu(dtp->leafidx),
	     nparent = le32_to_cpu(dtp->nleafs) >> 2;
	     nparent > 0; nparent >>= 2, child = parent) {
		
		parent = (child - 1) >> 2;

		for (i = 0, cp = tp + child, cp1 = tp + parent;
		     i < nparent; i++, cp += 4, cp1++)
			*cp1 = TREEMAX(cp);
	}

	return (*tp);
}


static int dbInitDmapCtl(struct dmapctl * dcp, int level, int i)
{				
	s8 *cp;

	dcp->nleafs = cpu_to_le32(LPERCTL);
	dcp->l2nleafs = cpu_to_le32(L2LPERCTL);
	dcp->leafidx = cpu_to_le32(CTLLEAFIND);
	dcp->height = cpu_to_le32(5);
	dcp->budmin = L2BPERDMAP + L2LPERCTL * level;

	cp = &dcp->stree[CTLLEAFIND + i];
	for (; i < LPERCTL; i++)
		*cp++ = NOFREE;

	
	return (dbInitTree((struct dmaptree *) dcp));
}


static int dbGetL2AGSize(s64 nblocks)
{
	s64 sz;
	s64 m;
	int l2sz;

	if (nblocks < BPERDMAP * MAXAG)
		return (L2BPERDMAP);

	
	m = ((u64) 1 << (64 - 1));
	for (l2sz = 64; l2sz >= 0; l2sz--, m >>= 1) {
		if (m & nblocks)
			break;
	}

	sz = (s64) 1 << l2sz;
	if (sz < nblocks)
		l2sz += 1;

	
	return (l2sz - L2MAXAG);
}



#define MAXL0PAGES	(1 + LPERCTL)
#define MAXL1PAGES	(1 + LPERCTL * MAXL0PAGES)
#define MAXL2PAGES	(1 + LPERCTL * MAXL1PAGES)

#define BMAPPGTOLEV(npages)	\
	(((npages) <= 3 + MAXL0PAGES) ? 0 : \
	 ((npages) <= 2 + MAXL1PAGES) ? 1 : 2)

s64 dbMapFileSizeToMapSize(struct inode * ipbmap)
{
	struct super_block *sb = ipbmap->i_sb;
	s64 nblocks;
	s64 npages, ndmaps;
	int level, i;
	int complete, factor;

	nblocks = ipbmap->i_size >> JFS_SBI(sb)->l2bsize;
	npages = nblocks >> JFS_SBI(sb)->l2nbperpage;
	level = BMAPPGTOLEV(npages);

	ndmaps = 0;
	npages--;		
	
	npages -= (2 - level);
	npages--;		
	for (i = level; i >= 0; i--) {
		factor =
		    (i == 2) ? MAXL1PAGES : ((i == 1) ? MAXL0PAGES : 1);
		complete = (u32) npages / factor;
		ndmaps += complete * ((i == 2) ? LPERCTL * LPERCTL :
				      ((i == 1) ? LPERCTL : 1));

		
		npages = (u32) npages % factor;
		
		npages--;
	}

	nblocks = ndmaps << L2BPERDMAP;

	return (nblocks);
}
