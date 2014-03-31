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
#include <linux/quotaops.h>
#include <linux/slab.h>
#include "jfs_incore.h"
#include "jfs_superblock.h"
#include "jfs_filsys.h"
#include "jfs_metapage.h"
#include "jfs_dmap.h"
#include "jfs_unicode.h"
#include "jfs_debug.h"

struct dtsplit {
	struct metapage *mp;
	s16 index;
	s16 nslot;
	struct component_name *key;
	ddata_t *data;
	struct pxdlist *pxdlist;
};

#define DT_PAGE(IP, MP) BT_PAGE(IP, MP, dtpage_t, i_dtroot)

#define DT_GETPAGE(IP, BN, MP, SIZE, P, RC)\
{\
	BT_GETPAGE(IP, BN, MP, dtpage_t, SIZE, P, RC, i_dtroot)\
	if (!(RC))\
	{\
		if (((P)->header.nextindex > (((BN)==0)?DTROOTMAXSLOT:(P)->header.maxslot)) ||\
		    ((BN) && ((P)->header.maxslot > DTPAGEMAXSLOT)))\
		{\
			BT_PUTPAGE(MP);\
			jfs_error((IP)->i_sb, "DT_GETPAGE: dtree page corrupt");\
			MP = NULL;\
			RC = -EIO;\
		}\
	}\
}

#define DT_PUTPAGE(MP) BT_PUTPAGE(MP)

#define DT_GETSEARCH(IP, LEAF, BN, MP, P, INDEX) \
	BT_GETSEARCH(IP, LEAF, BN, MP, dtpage_t, P, INDEX, i_dtroot)

static int dtSplitUp(tid_t tid, struct inode *ip,
		     struct dtsplit * split, struct btstack * btstack);

static int dtSplitPage(tid_t tid, struct inode *ip, struct dtsplit * split,
		       struct metapage ** rmpp, dtpage_t ** rpp, pxd_t * rxdp);

static int dtExtendPage(tid_t tid, struct inode *ip,
			struct dtsplit * split, struct btstack * btstack);

static int dtSplitRoot(tid_t tid, struct inode *ip,
		       struct dtsplit * split, struct metapage ** rmpp);

static int dtDeleteUp(tid_t tid, struct inode *ip, struct metapage * fmp,
		      dtpage_t * fp, struct btstack * btstack);

static int dtRelink(tid_t tid, struct inode *ip, dtpage_t * p);

static int dtReadFirst(struct inode *ip, struct btstack * btstack);

static int dtReadNext(struct inode *ip,
		      loff_t * offset, struct btstack * btstack);

static int dtCompare(struct component_name * key, dtpage_t * p, int si);

static int ciCompare(struct component_name * key, dtpage_t * p, int si,
		     int flag);

static void dtGetKey(dtpage_t * p, int i, struct component_name * key,
		     int flag);

static int ciGetLeafPrefixKey(dtpage_t * lp, int li, dtpage_t * rp,
			      int ri, struct component_name * key, int flag);

static void dtInsertEntry(dtpage_t * p, int index, struct component_name * key,
			  ddata_t * data, struct dt_lock **);

static void dtMoveEntry(dtpage_t * sp, int si, dtpage_t * dp,
			struct dt_lock ** sdtlock, struct dt_lock ** ddtlock,
			int do_index);

static void dtDeleteEntry(dtpage_t * p, int fi, struct dt_lock ** dtlock);

static void dtTruncateEntry(dtpage_t * p, int ti, struct dt_lock ** dtlock);

static void dtLinelockFreelist(dtpage_t * p, int m, struct dt_lock ** dtlock);

#define ciToUpper(c)	UniStrupr((c)->name)

static struct metapage *read_index_page(struct inode *inode, s64 blkno)
{
	int rc;
	s64 xaddr;
	int xflag;
	s32 xlen;

	rc = xtLookup(inode, blkno, 1, &xflag, &xaddr, &xlen, 1);
	if (rc || (xaddr == 0))
		return NULL;

	return read_metapage(inode, xaddr, PSIZE, 1);
}

static struct metapage *get_index_page(struct inode *inode, s64 blkno)
{
	int rc;
	s64 xaddr;
	int xflag;
	s32 xlen;

	rc = xtLookup(inode, blkno, 1, &xflag, &xaddr, &xlen, 1);
	if (rc || (xaddr == 0))
		return NULL;

	return get_metapage(inode, xaddr, PSIZE, 1);
}

static struct dir_table_slot *find_index(struct inode *ip, u32 index,
					 struct metapage ** mp, s64 *lblock)
{
	struct jfs_inode_info *jfs_ip = JFS_IP(ip);
	s64 blkno;
	s64 offset;
	int page_offset;
	struct dir_table_slot *slot;
	static int maxWarnings = 10;

	if (index < 2) {
		if (maxWarnings) {
			jfs_warn("find_entry called with index = %d", index);
			maxWarnings--;
		}
		return NULL;
	}

	if (index >= jfs_ip->next_index) {
		jfs_warn("find_entry called with index >= next_index");
		return NULL;
	}

	if (jfs_dirtable_inline(ip)) {
		*mp = NULL;
		slot = &jfs_ip->i_dirtable[index - 2];
	} else {
		offset = (index - 2) * sizeof(struct dir_table_slot);
		page_offset = offset & (PSIZE - 1);
		blkno = ((offset + 1) >> L2PSIZE) <<
		    JFS_SBI(ip->i_sb)->l2nbperpage;

		if (*mp && (*lblock != blkno)) {
			release_metapage(*mp);
			*mp = NULL;
		}
		if (!(*mp)) {
			*lblock = blkno;
			*mp = read_index_page(ip, blkno);
		}
		if (!(*mp)) {
			jfs_err("free_index: error reading directory table");
			return NULL;
		}

		slot =
		    (struct dir_table_slot *) ((char *) (*mp)->data +
					       page_offset);
	}
	return slot;
}

static inline void lock_index(tid_t tid, struct inode *ip, struct metapage * mp,
			      u32 index)
{
	struct tlock *tlck;
	struct linelock *llck;
	struct lv *lv;

	tlck = txLock(tid, ip, mp, tlckDATA);
	llck = (struct linelock *) tlck->lock;

	if (llck->index >= llck->maxcnt)
		llck = txLinelock(llck);
	lv = &llck->lv[llck->index];

	lv->offset = ((index - 2) & 511) >> 1;
	lv->length = 1;
	llck->index++;
}

static u32 add_index(tid_t tid, struct inode *ip, s64 bn, int slot)
{
	struct super_block *sb = ip->i_sb;
	struct jfs_sb_info *sbi = JFS_SBI(sb);
	struct jfs_inode_info *jfs_ip = JFS_IP(ip);
	u64 blkno;
	struct dir_table_slot *dirtab_slot;
	u32 index;
	struct linelock *llck;
	struct lv *lv;
	struct metapage *mp;
	s64 offset;
	uint page_offset;
	struct tlock *tlck;
	s64 xaddr;

	ASSERT(DO_INDEX(ip));

	if (jfs_ip->next_index < 2) {
		jfs_warn("add_index: next_index = %d.  Resetting!",
			   jfs_ip->next_index);
		jfs_ip->next_index = 2;
	}

	index = jfs_ip->next_index++;

	if (index <= MAX_INLINE_DIRTABLE_ENTRY) {
		ip->i_size = (loff_t) (index - 1) << 3;

		dirtab_slot = &jfs_ip->i_dirtable[index-2];
		dirtab_slot->flag = DIR_INDEX_VALID;
		dirtab_slot->slot = slot;
		DTSaddress(dirtab_slot, bn);

		set_cflag(COMMIT_Dirtable, ip);

		return index;
	}
	if (index == (MAX_INLINE_DIRTABLE_ENTRY + 1)) {
		struct dir_table_slot temp_table[12];

		if (dquot_alloc_block(ip, sbi->nbperpage))
			goto clean_up;
		if (dbAlloc(ip, 0, sbi->nbperpage, &xaddr)) {
			dquot_free_block(ip, sbi->nbperpage);
			goto clean_up;
		}

		memcpy(temp_table, &jfs_ip->i_dirtable, sizeof(temp_table));

		xtInitRoot(tid, ip);

		if (xtInsert(tid, ip, 0, 0, sbi->nbperpage, &xaddr, 0)) {
			
			jfs_warn("add_index: xtInsert failed!");
			memcpy(&jfs_ip->i_dirtable, temp_table,
			       sizeof (temp_table));
			dbFree(ip, xaddr, sbi->nbperpage);
			dquot_free_block(ip, sbi->nbperpage);
			goto clean_up;
		}
		ip->i_size = PSIZE;

		mp = get_index_page(ip, 0);
		if (!mp) {
			jfs_err("add_index: get_metapage failed!");
			xtTruncate(tid, ip, 0, COMMIT_PWMAP);
			memcpy(&jfs_ip->i_dirtable, temp_table,
			       sizeof (temp_table));
			goto clean_up;
		}
		tlck = txLock(tid, ip, mp, tlckDATA);
		llck = (struct linelock *) & tlck->lock;
		ASSERT(llck->index == 0);
		lv = &llck->lv[0];

		lv->offset = 0;
		lv->length = 6;	
		llck->index++;

		memcpy(mp->data, temp_table, sizeof(temp_table));

		mark_metapage_dirty(mp);
		release_metapage(mp);

		clear_cflag(COMMIT_Dirtable, ip);
	}

	offset = (index - 2) * sizeof(struct dir_table_slot);
	page_offset = offset & (PSIZE - 1);
	blkno = ((offset + 1) >> L2PSIZE) << sbi->l2nbperpage;
	if (page_offset == 0) {
		xaddr = 0;
		if (xtInsert(tid, ip, 0, blkno, sbi->nbperpage, &xaddr, 0)) {
			jfs_warn("add_index: xtInsert failed!");
			goto clean_up;
		}
		ip->i_size += PSIZE;

		if ((mp = get_index_page(ip, blkno)))
			memset(mp->data, 0, PSIZE);	
		else
			xtTruncate(tid, ip, offset, COMMIT_PWMAP);
	} else
		mp = read_index_page(ip, blkno);

	if (!mp) {
		jfs_err("add_index: get/read_metapage failed!");
		goto clean_up;
	}

	lock_index(tid, ip, mp, index);

	dirtab_slot =
	    (struct dir_table_slot *) ((char *) mp->data + page_offset);
	dirtab_slot->flag = DIR_INDEX_VALID;
	dirtab_slot->slot = slot;
	DTSaddress(dirtab_slot, bn);

	mark_metapage_dirty(mp);
	release_metapage(mp);

	return index;

      clean_up:

	jfs_ip->next_index--;

	return 0;
}

static void free_index(tid_t tid, struct inode *ip, u32 index, u32 next)
{
	struct dir_table_slot *dirtab_slot;
	s64 lblock;
	struct metapage *mp = NULL;

	dirtab_slot = find_index(ip, index, &mp, &lblock);

	if (!dirtab_slot)
		return;

	dirtab_slot->flag = DIR_INDEX_FREE;
	dirtab_slot->slot = dirtab_slot->addr1 = 0;
	dirtab_slot->addr2 = cpu_to_le32(next);

	if (mp) {
		lock_index(tid, ip, mp, index);
		mark_metapage_dirty(mp);
		release_metapage(mp);
	} else
		set_cflag(COMMIT_Dirtable, ip);
}

static void modify_index(tid_t tid, struct inode *ip, u32 index, s64 bn,
			 int slot, struct metapage ** mp, s64 *lblock)
{
	struct dir_table_slot *dirtab_slot;

	dirtab_slot = find_index(ip, index, mp, lblock);

	if (!dirtab_slot)
		return;

	DTSaddress(dirtab_slot, bn);
	dirtab_slot->slot = slot;

	if (*mp) {
		lock_index(tid, ip, *mp, index);
		mark_metapage_dirty(*mp);
	} else
		set_cflag(COMMIT_Dirtable, ip);
}

static int read_index(struct inode *ip, u32 index,
		     struct dir_table_slot * dirtab_slot)
{
	s64 lblock;
	struct metapage *mp = NULL;
	struct dir_table_slot *slot;

	slot = find_index(ip, index, &mp, &lblock);
	if (!slot) {
		return -EIO;
	}

	memcpy(dirtab_slot, slot, sizeof(struct dir_table_slot));

	if (mp)
		release_metapage(mp);

	return 0;
}

int dtSearch(struct inode *ip, struct component_name * key, ino_t * data,
	     struct btstack * btstack, int flag)
{
	int rc = 0;
	int cmp = 1;		
	s64 bn;
	struct metapage *mp;
	dtpage_t *p;
	s8 *stbl;
	int base, index, lim;
	struct btframe *btsp;
	pxd_t *pxd;
	int psize = 288;	
	ino_t inumber;
	struct component_name ciKey;
	struct super_block *sb = ip->i_sb;

	ciKey.name = kmalloc((JFS_NAME_MAX + 1) * sizeof(wchar_t), GFP_NOFS);
	if (!ciKey.name) {
		rc = -ENOMEM;
		goto dtSearch_Exit2;
	}


	
	UniStrcpy(ciKey.name, key->name);
	ciKey.namlen = key->namlen;

	
	if ((JFS_SBI(sb)->mntflag & JFS_OS2) == JFS_OS2) {
		ciToUpper(&ciKey);
	}
	BT_CLR(btstack);	

	
	btstack->nsplit = 1;

	for (bn = 0;;) {
		
		DT_GETPAGE(ip, bn, mp, psize, p, rc);
		if (rc)
			goto dtSearch_Exit1;

		
		stbl = DT_GETSTBL(p);

		for (base = 0, lim = p->header.nextindex; lim; lim >>= 1) {
			index = base + (lim >> 1);

			if (p->header.flag & BT_LEAF) {
				
				cmp =
				    ciCompare(&ciKey, p, stbl[index],
					      JFS_SBI(sb)->mntflag);
			} else {
				

				cmp = dtCompare(&ciKey, p, stbl[index]);


			}
			if (cmp == 0) {
				if (p->header.flag & BT_LEAF) {
					inumber = le32_to_cpu(
			((struct ldtentry *) & p->slot[stbl[index]])->inumber);

					if (flag == JFS_LOOKUP) {
						*data = inumber;
						rc = 0;
						goto out;
					}

					if (flag == JFS_CREATE) {
						*data = inumber;
						rc = -EEXIST;
						goto out;
					}

					if ((flag == JFS_REMOVE ||
					     flag == JFS_RENAME) &&
					    *data != inumber) {
						rc = -ESTALE;
						goto out;
					}

					
					*data = inumber;
					btsp = btstack->top;
					btsp->bn = bn;
					btsp->index = index;
					btsp->mp = mp;

					rc = 0;
					goto dtSearch_Exit1;
				}

				goto getChild;
			}

			if (cmp > 0) {
				base = index + 1;
				--lim;
			}
		}

		if (p->header.flag & BT_LEAF) {
			if (flag == JFS_LOOKUP || flag == JFS_REMOVE ||
			    flag == JFS_RENAME) {
				rc = -ENOENT;
				goto out;
			}

			*data = 0;
			btsp = btstack->top;
			btsp->bn = bn;
			btsp->index = base;
			btsp->mp = mp;

			rc = 0;
			goto dtSearch_Exit1;
		}

		index = base ? base - 1 : base;

	      getChild:
		
		if (BT_STACK_FULL(btstack)) {
			jfs_error(sb, "stack overrun in dtSearch!");
			BT_STACK_DUMP(btstack);
			rc = -EIO;
			goto out;
		}
		btstack->nsplit++;

		
		BT_PUSH(btstack, bn, index);

		
		pxd = (pxd_t *) & p->slot[stbl[index]];
		bn = addressPXD(pxd);
		psize = lengthPXD(pxd) << JFS_SBI(ip->i_sb)->l2bsize;

		
		DT_PUTPAGE(mp);
	}

      out:
	DT_PUTPAGE(mp);

      dtSearch_Exit1:

	kfree(ciKey.name);

      dtSearch_Exit2:

	return rc;
}


int dtInsert(tid_t tid, struct inode *ip,
	 struct component_name * name, ino_t * fsn, struct btstack * btstack)
{
	int rc = 0;
	struct metapage *mp;	
	dtpage_t *p;		
	s64 bn;
	int index;
	struct dtsplit split;	
	ddata_t data;
	struct dt_lock *dtlck;
	int n;
	struct tlock *tlck;
	struct lv *lv;

	DT_GETSEARCH(ip, btstack->top, bn, mp, p, index);

	if (DO_INDEX(ip)) {
		if (JFS_IP(ip)->next_index == DIREND) {
			DT_PUTPAGE(mp);
			return -EMLINK;
		}
		n = NDTLEAF(name->namlen);
		data.leaf.tid = tid;
		data.leaf.ip = ip;
	} else {
		n = NDTLEAF_LEGACY(name->namlen);
		data.leaf.ip = NULL;	
	}
	data.leaf.ino = *fsn;

	if (n > p->header.freecnt) {
		split.mp = mp;
		split.index = index;
		split.nslot = n;
		split.key = name;
		split.data = &data;
		rc = dtSplitUp(tid, ip, &split, btstack);
		return rc;
	}

	BT_MARK_DIRTY(mp, ip);
	tlck = txLock(tid, ip, mp, tlckDTREE | tlckENTRY);
	dtlck = (struct dt_lock *) & tlck->lock;
	ASSERT(dtlck->index == 0);
	lv = & dtlck->lv[0];

	
	lv->offset = 0;
	lv->length = 1;
	dtlck->index++;

	dtInsertEntry(p, index, name, &data, &dtlck);

	
	if (!(p->header.flag & BT_ROOT)) {
		if (dtlck->index >= dtlck->maxcnt)
			dtlck = (struct dt_lock *) txLinelock(dtlck);
		lv = & dtlck->lv[dtlck->index];
		n = index >> L2DTSLOTSIZE;
		lv->offset = p->header.stblindex + n;
		lv->length =
		    ((p->header.nextindex - 1) >> L2DTSLOTSIZE) - n + 1;
		dtlck->index++;
	}

	
	DT_PUTPAGE(mp);

	return 0;
}


static int dtSplitUp(tid_t tid,
	  struct inode *ip, struct dtsplit * split, struct btstack * btstack)
{
	struct jfs_sb_info *sbi = JFS_SBI(ip->i_sb);
	int rc = 0;
	struct metapage *smp;
	dtpage_t *sp;		
	struct metapage *rmp;
	dtpage_t *rp;		
	pxd_t rpxd;		
	struct metapage *lmp;
	dtpage_t *lp;		
	int skip;		
	struct btframe *parent;	
	s64 xaddr, nxaddr;
	int xlen, xsize;
	struct pxdlist pxdlist;
	pxd_t *pxd;
	struct component_name key = { 0, NULL };
	ddata_t *data = split->data;
	int n;
	struct dt_lock *dtlck;
	struct tlock *tlck;
	struct lv *lv;
	int quota_allocation = 0;

	
	smp = split->mp;
	sp = DT_PAGE(ip, smp);

	key.name = kmalloc((JFS_NAME_MAX + 2) * sizeof(wchar_t), GFP_NOFS);
	if (!key.name) {
		DT_PUTPAGE(smp);
		rc = -ENOMEM;
		goto dtSplitUp_Exit;
	}

	if (sp->header.flag & BT_ROOT) {
		xlen = 1;
		n = sbi->bsize >> L2DTSLOTSIZE;
		n -= (n + 31) >> L2DTSLOTSIZE;	
		n -= DTROOTMAXSLOT - sp->header.freecnt; 
		if (n <= split->nslot)
			xlen++;
		if ((rc = dbAlloc(ip, 0, (s64) xlen, &xaddr))) {
			DT_PUTPAGE(smp);
			goto freeKeyName;
		}

		pxdlist.maxnpxd = 1;
		pxdlist.npxd = 0;
		pxd = &pxdlist.pxd[0];
		PXDaddress(pxd, xaddr);
		PXDlength(pxd, xlen);
		split->pxdlist = &pxdlist;
		rc = dtSplitRoot(tid, ip, split, &rmp);

		if (rc)
			dbFree(ip, xaddr, xlen);
		else
			DT_PUTPAGE(rmp);

		DT_PUTPAGE(smp);

		if (!DO_INDEX(ip))
			ip->i_size = xlen << sbi->l2bsize;

		goto freeKeyName;
	}

	pxd = &sp->header.self;
	xlen = lengthPXD(pxd);
	xsize = xlen << sbi->l2bsize;
	if (xsize < PSIZE) {
		xaddr = addressPXD(pxd);
		n = xsize >> L2DTSLOTSIZE;
		n -= (n + 31) >> L2DTSLOTSIZE;	
		if ((n + sp->header.freecnt) <= split->nslot)
			n = xlen + (xlen << 1);
		else
			n = xlen;

		
		rc = dquot_alloc_block(ip, n);
		if (rc)
			goto extendOut;
		quota_allocation += n;

		if ((rc = dbReAlloc(sbi->ipbmap, xaddr, (s64) xlen,
				    (s64) n, &nxaddr)))
			goto extendOut;

		pxdlist.maxnpxd = 1;
		pxdlist.npxd = 0;
		pxd = &pxdlist.pxd[0];
		PXDaddress(pxd, nxaddr)
		    PXDlength(pxd, xlen + n);
		split->pxdlist = &pxdlist;
		if ((rc = dtExtendPage(tid, ip, split, btstack))) {
			nxaddr = addressPXD(pxd);
			if (xaddr != nxaddr) {
				
				xlen = lengthPXD(pxd);
				dbFree(ip, nxaddr, (s64) xlen);
			} else {
				
				xlen = lengthPXD(pxd) - n;
				xaddr = addressPXD(pxd) + xlen;
				dbFree(ip, xaddr, (s64) n);
			}
		} else if (!DO_INDEX(ip))
			ip->i_size = lengthPXD(pxd) << sbi->l2bsize;


	      extendOut:
		DT_PUTPAGE(smp);
		goto freeKeyName;
	}

	n = btstack->nsplit;
	pxdlist.maxnpxd = pxdlist.npxd = 0;
	xlen = sbi->nbperpage;
	for (pxd = pxdlist.pxd; n > 0; n--, pxd++) {
		if ((rc = dbAlloc(ip, 0, (s64) xlen, &xaddr)) == 0) {
			PXDaddress(pxd, xaddr);
			PXDlength(pxd, xlen);
			pxdlist.maxnpxd++;
			continue;
		}

		DT_PUTPAGE(smp);

		
		goto splitOut;
	}

	split->pxdlist = &pxdlist;
	if ((rc = dtSplitPage(tid, ip, split, &rmp, &rp, &rpxd))) {
		DT_PUTPAGE(smp);

		
		goto splitOut;
	}

	if (!DO_INDEX(ip))
		ip->i_size += PSIZE;

	while ((parent = BT_POP(btstack)) != NULL) {
		

		
		lmp = smp;
		lp = sp;

		
		DT_GETPAGE(ip, parent->bn, smp, PSIZE, sp, rc);
		if (rc) {
			DT_PUTPAGE(lmp);
			DT_PUTPAGE(rmp);
			goto splitOut;
		}

		skip = parent->index + 1;

		switch (rp->header.flag & BT_TYPE) {
		case BT_LEAF:
			if ((sp->header.flag & BT_ROOT && skip > 1) ||
			    sp->header.prev != 0 || skip > 1) {
				
				rc = ciGetLeafPrefixKey(lp,
							lp->header.nextindex-1,
							rp, 0, &key,
							sbi->mntflag);
				if (rc) {
					DT_PUTPAGE(lmp);
					DT_PUTPAGE(rmp);
					DT_PUTPAGE(smp);
					goto splitOut;
				}
			} else {

				
				dtGetKey(rp, 0, &key, sbi->mntflag);
				key.name[key.namlen] = 0;

				if ((sbi->mntflag & JFS_OS2) == JFS_OS2)
					ciToUpper(&key);
			}

			n = NDTINTERNAL(key.namlen);
			break;

		case BT_INTERNAL:
			dtGetKey(rp, 0, &key, sbi->mntflag);
			n = NDTINTERNAL(key.namlen);
			break;

		default:
			jfs_err("dtSplitUp(): UFO!");
			break;
		}

		
		DT_PUTPAGE(lmp);

		data->xd = rpxd;	

		if (n > sp->header.freecnt) {
			
			split->mp = smp;
			split->index = skip;	
			split->nslot = n;
			split->key = &key;
			

			
			DT_PUTPAGE(rmp);

			rc = (sp->header.flag & BT_ROOT) ?
			    dtSplitRoot(tid, ip, split, &rmp) :
			    dtSplitPage(tid, ip, split, &rmp, &rp, &rpxd);
			if (rc) {
				DT_PUTPAGE(smp);
				goto splitOut;
			}

			
		}
		else {
			BT_MARK_DIRTY(smp, ip);
			tlck = txLock(tid, ip, smp, tlckDTREE | tlckENTRY);
			dtlck = (struct dt_lock *) & tlck->lock;
			ASSERT(dtlck->index == 0);
			lv = & dtlck->lv[0];

			
			lv->offset = 0;
			lv->length = 1;
			dtlck->index++;

			
			if (!(sp->header.flag & BT_ROOT)) {
				lv++;
				n = skip >> L2DTSLOTSIZE;
				lv->offset = sp->header.stblindex + n;
				lv->length =
				    ((sp->header.nextindex -
				      1) >> L2DTSLOTSIZE) - n + 1;
				dtlck->index++;
			}

			dtInsertEntry(sp, skip, &key, data, &dtlck);

			
			break;
		}
	}

	
	DT_PUTPAGE(smp);
	DT_PUTPAGE(rmp);

      splitOut:
	n = pxdlist.npxd;
	pxd = &pxdlist.pxd[n];
	for (; n < pxdlist.maxnpxd; n++, pxd++)
		dbFree(ip, addressPXD(pxd), (s64) lengthPXD(pxd));

      freeKeyName:
	kfree(key.name);

	
	if (rc && quota_allocation)
		dquot_free_block(ip, quota_allocation);

      dtSplitUp_Exit:

	return rc;
}


static int dtSplitPage(tid_t tid, struct inode *ip, struct dtsplit * split,
	    struct metapage ** rmpp, dtpage_t ** rpp, pxd_t * rpxdp)
{
	int rc = 0;
	struct metapage *smp;
	dtpage_t *sp;
	struct metapage *rmp;
	dtpage_t *rp;		
	s64 rbn;		
	struct metapage *mp;
	dtpage_t *p;
	s64 nextbn;
	struct pxdlist *pxdlist;
	pxd_t *pxd;
	int skip, nextindex, half, left, nxt, off, si;
	struct ldtentry *ldtentry;
	struct idtentry *idtentry;
	u8 *stbl;
	struct dtslot *f;
	int fsi, stblsize;
	int n;
	struct dt_lock *sdtlck, *rdtlck;
	struct tlock *tlck;
	struct dt_lock *dtlck;
	struct lv *slv, *rlv, *lv;

	
	smp = split->mp;
	sp = DT_PAGE(ip, smp);

	pxdlist = split->pxdlist;
	pxd = &pxdlist->pxd[pxdlist->npxd];
	pxdlist->npxd++;
	rbn = addressPXD(pxd);
	rmp = get_metapage(ip, rbn, PSIZE, 1);
	if (rmp == NULL)
		return -EIO;

	
	rc = dquot_alloc_block(ip, lengthPXD(pxd));
	if (rc) {
		release_metapage(rmp);
		return rc;
	}

	jfs_info("dtSplitPage: ip:0x%p smp:0x%p rmp:0x%p", ip, smp, rmp);

	BT_MARK_DIRTY(rmp, ip);
	tlck = txLock(tid, ip, rmp, tlckDTREE | tlckNEW);
	rdtlck = (struct dt_lock *) & tlck->lock;

	rp = (dtpage_t *) rmp->data;
	*rpp = rp;
	rp->header.self = *pxd;

	BT_MARK_DIRTY(smp, ip);
	tlck = txLock(tid, ip, smp, tlckDTREE | tlckENTRY);
	sdtlck = (struct dt_lock *) & tlck->lock;

	
	ASSERT(sdtlck->index == 0);
	slv = & sdtlck->lv[0];
	slv->offset = 0;
	slv->length = 1;
	sdtlck->index++;

	nextbn = le64_to_cpu(sp->header.next);
	rp->header.next = cpu_to_le64(nextbn);
	rp->header.prev = cpu_to_le64(addressPXD(&sp->header.self));
	sp->header.next = cpu_to_le64(rbn);

	rp->header.flag = sp->header.flag;

	
	rp->header.nextindex = 0;
	rp->header.stblindex = 1;

	n = PSIZE >> L2DTSLOTSIZE;
	rp->header.maxslot = n;
	stblsize = (n + 31) >> L2DTSLOTSIZE;	

	
	fsi = rp->header.stblindex + stblsize;
	rp->header.freelist = fsi;
	rp->header.freecnt = rp->header.maxslot - fsi;

	if (nextbn == 0 && split->index == sp->header.nextindex) {
		
		rlv = & rdtlck->lv[rdtlck->index];
		rlv->offset = 0;
		rlv->length = 2;
		rdtlck->index++;

		f = &rp->slot[fsi];
		for (fsi++; fsi < rp->header.maxslot; f++, fsi++)
			f->next = fsi;
		f->next = -1;

		
		dtInsertEntry(rp, 0, split->key, split->data, &rdtlck);

		goto out;
	}


	if (nextbn != 0) {
		DT_GETPAGE(ip, nextbn, mp, PSIZE, p, rc);
		if (rc) {
			discard_metapage(rmp);
			return rc;
		}

		BT_MARK_DIRTY(mp, ip);
		tlck = txLock(tid, ip, mp, tlckDTREE | tlckRELINK);
		jfs_info("dtSplitPage: tlck = 0x%p, ip = 0x%p, mp=0x%p",
			tlck, ip, mp);
		dtlck = (struct dt_lock *) & tlck->lock;

		
		lv = & dtlck->lv[dtlck->index];
		lv->offset = 0;
		lv->length = 1;
		dtlck->index++;

		p->header.prev = cpu_to_le64(rbn);

		DT_PUTPAGE(mp);
	}

	skip = split->index;
	half = (PSIZE >> L2DTSLOTSIZE) >> 1;	
	left = 0;

	stbl = (u8 *) & sp->slot[sp->header.stblindex];
	nextindex = sp->header.nextindex;
	for (nxt = off = 0; nxt < nextindex; ++off) {
		if (off == skip)
			
			n = split->nslot;
		else {
			si = stbl[nxt];
			switch (sp->header.flag & BT_TYPE) {
			case BT_LEAF:
				ldtentry = (struct ldtentry *) & sp->slot[si];
				if (DO_INDEX(ip))
					n = NDTLEAF(ldtentry->namlen);
				else
					n = NDTLEAF_LEGACY(ldtentry->
							   namlen);
				break;

			case BT_INTERNAL:
				idtentry = (struct idtentry *) & sp->slot[si];
				n = NDTINTERNAL(idtentry->namlen);
				break;

			default:
				break;
			}

			++nxt;	
		}

		left += n;
		if (left >= half)
			break;
	}

	

	
	rlv = & rdtlck->lv[rdtlck->index];
	rlv->offset = 0;
	rlv->length = 5;
	rdtlck->index++;

	dtMoveEntry(sp, nxt, rp, &sdtlck, &rdtlck, DO_INDEX(ip));

	sp->header.nextindex = nxt;

	fsi = rp->header.freelist;
	f = &rp->slot[fsi];
	for (fsi++; fsi < rp->header.maxslot; f++, fsi++)
		f->next = fsi;
	f->next = -1;

	if ((rp->header.flag & BT_LEAF) && DO_INDEX(ip)) {
		s64 lblock;

		mp = NULL;
		stbl = DT_GETSTBL(rp);
		for (n = 0; n < rp->header.nextindex; n++) {
			ldtentry = (struct ldtentry *) & rp->slot[stbl[n]];
			modify_index(tid, ip, le32_to_cpu(ldtentry->index),
				     rbn, n, &mp, &lblock);
		}
		if (mp)
			release_metapage(mp);
	}

	if (skip <= off) {
		
		dtInsertEntry(sp, skip, split->key, split->data, &sdtlck);

		
		if (sdtlck->index >= sdtlck->maxcnt)
			sdtlck = (struct dt_lock *) txLinelock(sdtlck);
		slv = & sdtlck->lv[sdtlck->index];
		n = skip >> L2DTSLOTSIZE;
		slv->offset = sp->header.stblindex + n;
		slv->length =
		    ((sp->header.nextindex - 1) >> L2DTSLOTSIZE) - n + 1;
		sdtlck->index++;
	}
	else {
		
		skip -= nxt;

		
		dtInsertEntry(rp, skip, split->key, split->data, &rdtlck);
	}

      out:
	*rmpp = rmp;
	*rpxdp = *pxd;

	return rc;
}


static int dtExtendPage(tid_t tid,
	     struct inode *ip, struct dtsplit * split, struct btstack * btstack)
{
	struct super_block *sb = ip->i_sb;
	int rc;
	struct metapage *smp, *pmp, *mp;
	dtpage_t *sp, *pp;
	struct pxdlist *pxdlist;
	pxd_t *pxd, *tpxd;
	int xlen, xsize;
	int newstblindex, newstblsize;
	int oldstblindex, oldstblsize;
	int fsi, last;
	struct dtslot *f;
	struct btframe *parent;
	int n;
	struct dt_lock *dtlck;
	s64 xaddr, txaddr;
	struct tlock *tlck;
	struct pxd_lock *pxdlock;
	struct lv *lv;
	uint type;
	struct ldtentry *ldtentry;
	u8 *stbl;

	
	smp = split->mp;
	sp = DT_PAGE(ip, smp);

	
	parent = BT_POP(btstack);
	DT_GETPAGE(ip, parent->bn, pmp, PSIZE, pp, rc);
	if (rc)
		return (rc);

	pxdlist = split->pxdlist;
	pxd = &pxdlist->pxd[pxdlist->npxd];
	pxdlist->npxd++;

	xaddr = addressPXD(pxd);
	tpxd = &sp->header.self;
	txaddr = addressPXD(tpxd);
	
	if (xaddr == txaddr) {
		type = tlckEXTEND;
	}
	
	else {
		type = tlckNEW;

		
		tlck = txMaplock(tid, ip, tlckDTREE | tlckRELOCATE);
		pxdlock = (struct pxd_lock *) & tlck->lock;
		pxdlock->flag = mlckFREEPXD;
		pxdlock->pxd = sp->header.self;
		pxdlock->index = 1;

		if (DO_INDEX(ip)) {
			s64 lblock;

			mp = NULL;
			stbl = DT_GETSTBL(sp);
			for (n = 0; n < sp->header.nextindex; n++) {
				ldtentry =
				    (struct ldtentry *) & sp->slot[stbl[n]];
				modify_index(tid, ip,
					     le32_to_cpu(ldtentry->index),
					     xaddr, n, &mp, &lblock);
			}
			if (mp)
				release_metapage(mp);
		}
	}

	sp->header.self = *pxd;

	jfs_info("dtExtendPage: ip:0x%p smp:0x%p sp:0x%p", ip, smp, sp);

	BT_MARK_DIRTY(smp, ip);
	tlck = txLock(tid, ip, smp, tlckDTREE | type);
	dtlck = (struct dt_lock *) & tlck->lock;
	lv = & dtlck->lv[0];

	
	xlen = lengthPXD(pxd);
	xsize = xlen << JFS_SBI(sb)->l2bsize;

	oldstblindex = sp->header.stblindex;
	oldstblsize = (sp->header.maxslot + 31) >> L2DTSLOTSIZE;
	newstblindex = sp->header.maxslot;
	n = xsize >> L2DTSLOTSIZE;
	newstblsize = (n + 31) >> L2DTSLOTSIZE;
	memcpy(&sp->slot[newstblindex], &sp->slot[oldstblindex],
	       sp->header.nextindex);

	if (type == tlckEXTEND) {
		
		lv->offset = 0;
		lv->length = 1;
		dtlck->index++;
		lv++;

		
		lv->offset = newstblindex;
		lv->length = newstblsize;
	}
	else {
		lv->offset = 0;
		lv->length = sp->header.maxslot + newstblsize;
	}

	dtlck->index++;

	sp->header.maxslot = n;
	sp->header.stblindex = newstblindex;
	

	fsi = oldstblindex;
	f = &sp->slot[fsi];
	last = sp->header.freelist;
	for (n = 0; n < oldstblsize; n++, fsi++, f++) {
		f->next = last;
		last = fsi;
	}
	sp->header.freelist = last;
	sp->header.freecnt += oldstblsize;

	
	fsi = n = newstblindex + newstblsize;
	f = &sp->slot[fsi];
	for (fsi++; fsi < sp->header.maxslot; f++, fsi++)
		f->next = fsi;
	f->next = -1;

	
	fsi = sp->header.freelist;
	if (fsi == -1)
		sp->header.freelist = n;
	else {
		do {
			f = &sp->slot[fsi];
			fsi = f->next;
		} while (fsi != -1);

		f->next = n;
	}

	sp->header.freecnt += sp->header.maxslot - n;

	dtInsertEntry(sp, split->index, split->key, split->data, &dtlck);

	BT_MARK_DIRTY(pmp, ip);
	if (type == tlckEXTEND) {
		n = sp->header.maxslot >> 2;
		if (sp->header.freelist < n)
			dtLinelockFreelist(sp, n, &dtlck);
	}

	tlck = txLock(tid, ip, pmp, tlckDTREE | tlckENTRY);
	dtlck = (struct dt_lock *) & tlck->lock;
	lv = & dtlck->lv[dtlck->index];

	
	lv->offset = 1;
	lv->length = 1;
	dtlck->index++;

	
	tpxd = (pxd_t *) & pp->slot[1];
	*tpxd = *pxd;

	DT_PUTPAGE(pmp);
	return 0;
}


static int dtSplitRoot(tid_t tid,
	    struct inode *ip, struct dtsplit * split, struct metapage ** rmpp)
{
	struct super_block *sb = ip->i_sb;
	struct metapage *smp;
	dtroot_t *sp;
	struct metapage *rmp;
	dtpage_t *rp;
	s64 rbn;
	int xlen;
	int xsize;
	struct dtslot *f;
	s8 *stbl;
	int fsi, stblsize, n;
	struct idtentry *s;
	pxd_t *ppxd;
	struct pxdlist *pxdlist;
	pxd_t *pxd;
	struct dt_lock *dtlck;
	struct tlock *tlck;
	struct lv *lv;
	int rc;

	
	smp = split->mp;
	sp = &JFS_IP(ip)->i_dtroot;

	pxdlist = split->pxdlist;
	pxd = &pxdlist->pxd[pxdlist->npxd];
	pxdlist->npxd++;
	rbn = addressPXD(pxd);
	xlen = lengthPXD(pxd);
	xsize = xlen << JFS_SBI(sb)->l2bsize;
	rmp = get_metapage(ip, rbn, xsize, 1);
	if (!rmp)
		return -EIO;

	rp = rmp->data;

	
	rc = dquot_alloc_block(ip, lengthPXD(pxd));
	if (rc) {
		release_metapage(rmp);
		return rc;
	}

	BT_MARK_DIRTY(rmp, ip);
	tlck = txLock(tid, ip, rmp, tlckDTREE | tlckNEW);
	dtlck = (struct dt_lock *) & tlck->lock;

	rp->header.flag =
	    (sp->header.flag & BT_LEAF) ? BT_LEAF : BT_INTERNAL;
	rp->header.self = *pxd;

	
	rp->header.next = 0;
	rp->header.prev = 0;

	
	ASSERT(dtlck->index == 0);
	lv = & dtlck->lv[0];
	lv->offset = 0;
	lv->length = 10;	
	dtlck->index++;

	n = xsize >> L2DTSLOTSIZE;
	rp->header.maxslot = n;
	stblsize = (n + 31) >> L2DTSLOTSIZE;

	
	rp->header.stblindex = DTROOTMAXSLOT;
	stbl = (s8 *) & rp->slot[DTROOTMAXSLOT];
	memcpy(stbl, sp->header.stbl, sp->header.nextindex);
	rp->header.nextindex = sp->header.nextindex;

	
	memcpy(&rp->slot[1], &sp->slot[1], IDATASIZE);

	
	fsi = n = DTROOTMAXSLOT + stblsize;
	f = &rp->slot[fsi];
	for (fsi++; fsi < rp->header.maxslot; f++, fsi++)
		f->next = fsi;
	f->next = -1;

	
	fsi = sp->header.freelist;
	if (fsi == -1)
		rp->header.freelist = n;
	else {
		rp->header.freelist = fsi;

		do {
			f = &rp->slot[fsi];
			fsi = f->next;
		} while (fsi != -1);

		f->next = n;
	}

	rp->header.freecnt = sp->header.freecnt + rp->header.maxslot - n;

	if ((rp->header.flag & BT_LEAF) && DO_INDEX(ip)) {
		s64 lblock;
		struct metapage *mp = NULL;
		struct ldtentry *ldtentry;

		stbl = DT_GETSTBL(rp);
		for (n = 0; n < rp->header.nextindex; n++) {
			ldtentry = (struct ldtentry *) & rp->slot[stbl[n]];
			modify_index(tid, ip, le32_to_cpu(ldtentry->index),
				     rbn, n, &mp, &lblock);
		}
		if (mp)
			release_metapage(mp);
	}
	dtInsertEntry(rp, split->index, split->key, split->data, &dtlck);

	BT_MARK_DIRTY(smp, ip);
	tlck = txLock(tid, ip, smp, tlckDTREE | tlckNEW | tlckBTROOT);
	dtlck = (struct dt_lock *) & tlck->lock;

	
	ASSERT(dtlck->index == 0);
	lv = & dtlck->lv[0];
	lv->offset = 0;
	lv->length = DTROOTMAXSLOT;
	dtlck->index++;

	
	if (sp->header.flag & BT_LEAF) {
		sp->header.flag &= ~BT_LEAF;
		sp->header.flag |= BT_INTERNAL;
	}

	
	s = (struct idtentry *) & sp->slot[DTENTRYSTART];
	ppxd = (pxd_t *) s;
	*ppxd = *pxd;
	s->next = -1;
	s->namlen = 0;

	stbl = sp->header.stbl;
	stbl[0] = DTENTRYSTART;
	sp->header.nextindex = 1;

	
	fsi = DTENTRYSTART + 1;
	f = &sp->slot[fsi];

	
	for (fsi++; fsi < DTROOTMAXSLOT; f++, fsi++)
		f->next = fsi;
	f->next = -1;

	sp->header.freelist = DTENTRYSTART + 1;
	sp->header.freecnt = DTROOTMAXSLOT - (DTENTRYSTART + 1);

	*rmpp = rmp;

	return 0;
}


int dtDelete(tid_t tid,
	 struct inode *ip, struct component_name * key, ino_t * ino, int flag)
{
	int rc = 0;
	s64 bn;
	struct metapage *mp, *imp;
	dtpage_t *p;
	int index;
	struct btstack btstack;
	struct dt_lock *dtlck;
	struct tlock *tlck;
	struct lv *lv;
	int i;
	struct ldtentry *ldtentry;
	u8 *stbl;
	u32 table_index, next_index;
	struct metapage *nmp;
	dtpage_t *np;

	if ((rc = dtSearch(ip, key, ino, &btstack, flag)))
		return rc;

	
	DT_GETSEARCH(ip, btstack.top, bn, mp, p, index);

	if (DO_INDEX(ip)) {
		stbl = DT_GETSTBL(p);
		ldtentry = (struct ldtentry *) & p->slot[stbl[index]];
		table_index = le32_to_cpu(ldtentry->index);
		if (index == (p->header.nextindex - 1)) {
			if ((p->header.flag & BT_ROOT)
			    || (p->header.next == 0))
				next_index = -1;
			else {
				
				DT_GETPAGE(ip, le64_to_cpu(p->header.next),
					   nmp, PSIZE, np, rc);
				if (rc)
					next_index = -1;
				else {
					stbl = DT_GETSTBL(np);
					ldtentry =
					    (struct ldtentry *) & np->
					    slot[stbl[0]];
					next_index =
					    le32_to_cpu(ldtentry->index);
					DT_PUTPAGE(nmp);
				}
			}
		} else {
			ldtentry =
			    (struct ldtentry *) & p->slot[stbl[index + 1]];
			next_index = le32_to_cpu(ldtentry->index);
		}
		free_index(tid, ip, table_index, next_index);
	}
	if (p->header.nextindex == 1) {
		
		rc = dtDeleteUp(tid, ip, mp, p, &btstack);
	}
	else {
		BT_MARK_DIRTY(mp, ip);
		tlck = txLock(tid, ip, mp, tlckDTREE | tlckENTRY);
		dtlck = (struct dt_lock *) & tlck->lock;


		
		if (dtlck->index >= dtlck->maxcnt)
			dtlck = (struct dt_lock *) txLinelock(dtlck);
		lv = & dtlck->lv[dtlck->index];
		lv->offset = 0;
		lv->length = 1;
		dtlck->index++;

		
		if (!(p->header.flag & BT_ROOT)) {
			if (dtlck->index >= dtlck->maxcnt)
				dtlck = (struct dt_lock *) txLinelock(dtlck);
			lv = & dtlck->lv[dtlck->index];
			i = index >> L2DTSLOTSIZE;
			lv->offset = p->header.stblindex + i;
			lv->length =
			    ((p->header.nextindex - 1) >> L2DTSLOTSIZE) -
			    i + 1;
			dtlck->index++;
		}

		
		dtDeleteEntry(p, index, &dtlck);

		if (DO_INDEX(ip) && index < p->header.nextindex) {
			s64 lblock;

			imp = NULL;
			stbl = DT_GETSTBL(p);
			for (i = index; i < p->header.nextindex; i++) {
				ldtentry =
				    (struct ldtentry *) & p->slot[stbl[i]];
				modify_index(tid, ip,
					     le32_to_cpu(ldtentry->index),
					     bn, i, &imp, &lblock);
			}
			if (imp)
				release_metapage(imp);
		}

		DT_PUTPAGE(mp);
	}

	return rc;
}


static int dtDeleteUp(tid_t tid, struct inode *ip,
	   struct metapage * fmp, dtpage_t * fp, struct btstack * btstack)
{
	int rc = 0;
	struct metapage *mp;
	dtpage_t *p;
	int index, nextindex;
	int xlen;
	struct btframe *parent;
	struct dt_lock *dtlck;
	struct tlock *tlck;
	struct lv *lv;
	struct pxd_lock *pxdlock;
	int i;

	if (BT_IS_ROOT(fmp)) {
		dtInitRoot(tid, ip, PARENT(ip));

		DT_PUTPAGE(fmp);

		return 0;
	}

	tlck = txMaplock(tid, ip, tlckDTREE | tlckFREE);
	pxdlock = (struct pxd_lock *) & tlck->lock;
	pxdlock->flag = mlckFREEPXD;
	pxdlock->pxd = fp->header.self;
	pxdlock->index = 1;

	
	if ((rc = dtRelink(tid, ip, fp))) {
		BT_PUTPAGE(fmp);
		return rc;
	}

	xlen = lengthPXD(&fp->header.self);

	
	dquot_free_block(ip, xlen);

	
	discard_metapage(fmp);

	while ((parent = BT_POP(btstack)) != NULL) {
		
		DT_GETPAGE(ip, parent->bn, mp, PSIZE, p, rc);
		if (rc)
			return rc;

		index = parent->index;

		nextindex = p->header.nextindex;

		if (nextindex == 1) {
			if (p->header.flag & BT_ROOT) {
				dtInitRoot(tid, ip, PARENT(ip));

				DT_PUTPAGE(mp);

				return 0;
			}
			else {
				tlck =
				    txMaplock(tid, ip,
					      tlckDTREE | tlckFREE);
				pxdlock = (struct pxd_lock *) & tlck->lock;
				pxdlock->flag = mlckFREEPXD;
				pxdlock->pxd = p->header.self;
				pxdlock->index = 1;

				
				if ((rc = dtRelink(tid, ip, p))) {
					DT_PUTPAGE(mp);
					return rc;
				}

				xlen = lengthPXD(&p->header.self);

				
				dquot_free_block(ip, xlen);

				
				discard_metapage(mp);

				
				continue;
			}
		}

		BT_MARK_DIRTY(mp, ip);
		tlck = txLock(tid, ip, mp, tlckDTREE | tlckENTRY);
		dtlck = (struct dt_lock *) & tlck->lock;

		
		if (dtlck->index >= dtlck->maxcnt)
			dtlck = (struct dt_lock *) txLinelock(dtlck);
		lv = & dtlck->lv[dtlck->index];
		lv->offset = 0;
		lv->length = 1;
		dtlck->index++;

		
		if (!(p->header.flag & BT_ROOT)) {
			if (dtlck->index < dtlck->maxcnt)
				lv++;
			else {
				dtlck = (struct dt_lock *) txLinelock(dtlck);
				lv = & dtlck->lv[0];
			}
			i = index >> L2DTSLOTSIZE;
			lv->offset = p->header.stblindex + i;
			lv->length =
			    ((p->header.nextindex - 1) >> L2DTSLOTSIZE) -
			    i + 1;
			dtlck->index++;
		}

		
		dtDeleteEntry(p, index, &dtlck);

		
		if (index == 0 &&
		    ((p->header.flag & BT_ROOT) || p->header.prev == 0))
			dtTruncateEntry(p, 0, &dtlck);

		
		DT_PUTPAGE(mp);

		
		break;
	}

	if (!DO_INDEX(ip))
		ip->i_size -= PSIZE;

	return 0;
}

#ifdef _NOTYET
int dtRelocate(tid_t tid, struct inode *ip, s64 lmxaddr, pxd_t * opxd,
	       s64 nxaddr)
{
	int rc = 0;
	struct metapage *mp, *pmp, *lmp, *rmp;
	dtpage_t *p, *pp, *rp = 0, *lp= 0;
	s64 bn;
	int index;
	struct btstack btstack;
	pxd_t *pxd;
	s64 oxaddr, nextbn, prevbn;
	int xlen, xsize;
	struct tlock *tlck;
	struct dt_lock *dtlck;
	struct pxd_lock *pxdlock;
	s8 *stbl;
	struct lv *lv;

	oxaddr = addressPXD(opxd);
	xlen = lengthPXD(opxd);

	jfs_info("dtRelocate: lmxaddr:%Ld xaddr:%Ld:%Ld xlen:%d",
		   (long long)lmxaddr, (long long)oxaddr, (long long)nxaddr,
		   xlen);

	rc = dtSearchNode(ip, lmxaddr, opxd, &btstack);
	if (rc)
		return rc;

	
	DT_GETSEARCH(ip, btstack.top, bn, pmp, pp, index);
	jfs_info("dtRelocate: parent router entry validated.");

	
	DT_GETPAGE(ip, oxaddr, mp, PSIZE, p, rc);
	if (rc) {
		
		DT_PUTPAGE(pmp);
		return rc;
	}

	rmp = NULL;
	if (p->header.next) {
		nextbn = le64_to_cpu(p->header.next);
		DT_GETPAGE(ip, nextbn, rmp, PSIZE, rp, rc);
		if (rc) {
			DT_PUTPAGE(mp);
			DT_PUTPAGE(pmp);
			return (rc);
		}
	}

	lmp = NULL;
	if (p->header.prev) {
		prevbn = le64_to_cpu(p->header.prev);
		DT_GETPAGE(ip, prevbn, lmp, PSIZE, lp, rc);
		if (rc) {
			DT_PUTPAGE(mp);
			DT_PUTPAGE(pmp);
			if (rmp)
				DT_PUTPAGE(rmp);
			return (rc);
		}
	}

	

	if (lmp) {
		tlck = txLock(tid, ip, lmp, tlckDTREE | tlckRELINK);
		dtlck = (struct dt_lock *) & tlck->lock;
		
		ASSERT(dtlck->index == 0);
		lv = & dtlck->lv[0];
		lv->offset = 0;
		lv->length = 1;
		dtlck->index++;

		lp->header.next = cpu_to_le64(nxaddr);
		DT_PUTPAGE(lmp);
	}

	if (rmp) {
		tlck = txLock(tid, ip, rmp, tlckDTREE | tlckRELINK);
		dtlck = (struct dt_lock *) & tlck->lock;
		
		ASSERT(dtlck->index == 0);
		lv = & dtlck->lv[0];
		lv->offset = 0;
		lv->length = 1;
		dtlck->index++;

		rp->header.prev = cpu_to_le64(nxaddr);
		DT_PUTPAGE(rmp);
	}

	tlck = txLock(tid, ip, mp, tlckDTREE | tlckNEW);
	dtlck = (struct dt_lock *) & tlck->lock;
	
	ASSERT(dtlck->index == 0);
	lv = & dtlck->lv[0];

	
	pxd = &p->header.self;
	PXDaddress(pxd, nxaddr);

	lv->offset = 0;
	lv->length = p->header.maxslot;
	dtlck->index++;

	
	xsize = xlen << JFS_SBI(ip->i_sb)->l2bsize;

	
	DT_PUTPAGE(mp);
	jfs_info("dtRelocate: target dtpage relocated.");

	/* the moved extent is dtpage, then a LOG_NOREDOPAGE log rec
	 * needs to be written (in logredo(), the LOG_NOREDOPAGE log rec
	 * will also force a bmap update ).
	 */

	tlck = txMaplock(tid, ip, tlckDTREE | tlckFREE);
	pxdlock = (struct pxd_lock *) & tlck->lock;
	pxdlock->flag = mlckFREEPXD;
	PXDaddress(&pxdlock->pxd, oxaddr);
	PXDlength(&pxdlock->pxd, xlen);
	pxdlock->index = 1;

	jfs_info("dtRelocate: update parent router entry.");
	tlck = txLock(tid, ip, pmp, tlckDTREE | tlckENTRY);
	dtlck = (struct dt_lock *) & tlck->lock;
	lv = & dtlck->lv[dtlck->index];

	
	stbl = DT_GETSTBL(pp);
	pxd = (pxd_t *) & pp->slot[stbl[index]];
	PXDaddress(pxd, nxaddr);
	lv->offset = stbl[index];
	lv->length = 1;
	dtlck->index++;

	
	DT_PUTPAGE(pmp);

	return rc;
}

static int dtSearchNode(struct inode *ip, s64 lmxaddr, pxd_t * kpxd,
			struct btstack * btstack)
{
	int rc = 0;
	s64 bn;
	struct metapage *mp;
	dtpage_t *p;
	int psize = 288;	
	s8 *stbl;
	int i;
	pxd_t *pxd;
	struct btframe *btsp;

	BT_CLR(btstack);	

	for (bn = 0;;) {
		
		DT_GETPAGE(ip, bn, mp, psize, p, rc);
		if (rc)
			return rc;

		if (p->header.flag & BT_ROOT) {
			if (lmxaddr == 0)
				break;
		} else if (addressPXD(&p->header.self) == lmxaddr)
			break;

		if (p->header.flag & BT_LEAF) {
			DT_PUTPAGE(mp);
			return -ESTALE;
		}

		
		stbl = DT_GETSTBL(p);
		pxd = (pxd_t *) & p->slot[stbl[0]];

		
		bn = addressPXD(pxd);
		psize = lengthPXD(pxd) << JFS_SBI(ip->i_sb)->l2bsize;
		
		DT_PUTPAGE(mp);
	}

      loop:
	stbl = DT_GETSTBL(p);
	for (i = 0; i < p->header.nextindex; i++) {
		pxd = (pxd_t *) & p->slot[stbl[i]];

		
		if (addressPXD(pxd) == addressPXD(kpxd) &&
		    lengthPXD(pxd) == lengthPXD(kpxd)) {
			btsp = btstack->top;
			btsp->bn = bn;
			btsp->index = i;
			btsp->mp = mp;

			return 0;
		}
	}

	
	if (p->header.next)
		bn = le64_to_cpu(p->header.next);
	else {
		DT_PUTPAGE(mp);
		return -ESTALE;
	}

	
	DT_PUTPAGE(mp);

	
	DT_GETPAGE(ip, bn, mp, PSIZE, p, rc);
	if (rc)
		return rc;

	goto loop;
}
#endif 

static int dtRelink(tid_t tid, struct inode *ip, dtpage_t * p)
{
	int rc;
	struct metapage *mp;
	s64 nextbn, prevbn;
	struct tlock *tlck;
	struct dt_lock *dtlck;
	struct lv *lv;

	nextbn = le64_to_cpu(p->header.next);
	prevbn = le64_to_cpu(p->header.prev);

	
	if (nextbn != 0) {
		DT_GETPAGE(ip, nextbn, mp, PSIZE, p, rc);
		if (rc)
			return rc;

		BT_MARK_DIRTY(mp, ip);
		tlck = txLock(tid, ip, mp, tlckDTREE | tlckRELINK);
		jfs_info("dtRelink nextbn: tlck = 0x%p, ip = 0x%p, mp=0x%p",
			tlck, ip, mp);
		dtlck = (struct dt_lock *) & tlck->lock;

		
		if (dtlck->index >= dtlck->maxcnt)
			dtlck = (struct dt_lock *) txLinelock(dtlck);
		lv = & dtlck->lv[dtlck->index];
		lv->offset = 0;
		lv->length = 1;
		dtlck->index++;

		p->header.prev = cpu_to_le64(prevbn);
		DT_PUTPAGE(mp);
	}

	
	if (prevbn != 0) {
		DT_GETPAGE(ip, prevbn, mp, PSIZE, p, rc);
		if (rc)
			return rc;

		BT_MARK_DIRTY(mp, ip);
		tlck = txLock(tid, ip, mp, tlckDTREE | tlckRELINK);
		jfs_info("dtRelink prevbn: tlck = 0x%p, ip = 0x%p, mp=0x%p",
			tlck, ip, mp);
		dtlck = (struct dt_lock *) & tlck->lock;

		
		if (dtlck->index >= dtlck->maxcnt)
			dtlck = (struct dt_lock *) txLinelock(dtlck);
		lv = & dtlck->lv[dtlck->index];
		lv->offset = 0;
		lv->length = 1;
		dtlck->index++;

		p->header.next = cpu_to_le64(nextbn);
		DT_PUTPAGE(mp);
	}

	return 0;
}


void dtInitRoot(tid_t tid, struct inode *ip, u32 idotdot)
{
	struct jfs_inode_info *jfs_ip = JFS_IP(ip);
	dtroot_t *p;
	int fsi;
	struct dtslot *f;
	struct tlock *tlck;
	struct dt_lock *dtlck;
	struct lv *lv;
	u16 xflag_save;

	if (DO_INDEX(ip)) {
		if (!jfs_dirtable_inline(ip)) {
			struct tblock *tblk = tid_to_tblock(tid);
			xflag_save = tblk->xflag;
			tblk->xflag = 0;
			xtTruncate(tid, ip, 0, COMMIT_PWMAP);
			set_cflag(COMMIT_Stale, ip);

			tblk->xflag = xflag_save;
		} else
			ip->i_size = 1;

		jfs_ip->next_index = 2;
	} else
		ip->i_size = IDATASIZE;

	tlck = txLock(tid, ip, (struct metapage *) & jfs_ip->bxflag,
		      tlckDTREE | tlckENTRY | tlckBTROOT);
	dtlck = (struct dt_lock *) & tlck->lock;

	
	ASSERT(dtlck->index == 0);
	lv = & dtlck->lv[0];
	lv->offset = 0;
	lv->length = DTROOTMAXSLOT;
	dtlck->index++;

	p = &jfs_ip->i_dtroot;

	p->header.flag = DXD_INDEX | BT_ROOT | BT_LEAF;

	p->header.nextindex = 0;

	
	fsi = 1;
	f = &p->slot[fsi];

	
	for (fsi++; fsi < DTROOTMAXSLOT; f++, fsi++)
		f->next = fsi;
	f->next = -1;

	p->header.freelist = 1;
	p->header.freecnt = 8;

	
	p->header.idotdot = cpu_to_le32(idotdot);

	return;
}

static void add_missing_indices(struct inode *inode, s64 bn)
{
	struct ldtentry *d;
	struct dt_lock *dtlck;
	int i;
	uint index;
	struct lv *lv;
	struct metapage *mp;
	dtpage_t *p;
	int rc;
	s8 *stbl;
	tid_t tid;
	struct tlock *tlck;

	tid = txBegin(inode->i_sb, 0);

	DT_GETPAGE(inode, bn, mp, PSIZE, p, rc);

	if (rc) {
		printk(KERN_ERR "DT_GETPAGE failed!\n");
		goto end;
	}
	BT_MARK_DIRTY(mp, inode);

	ASSERT(p->header.flag & BT_LEAF);

	tlck = txLock(tid, inode, mp, tlckDTREE | tlckENTRY);
	if (BT_IS_ROOT(mp))
		tlck->type |= tlckBTROOT;

	dtlck = (struct dt_lock *) &tlck->lock;

	stbl = DT_GETSTBL(p);
	for (i = 0; i < p->header.nextindex; i++) {
		d = (struct ldtentry *) &p->slot[stbl[i]];
		index = le32_to_cpu(d->index);
		if ((index < 2) || (index >= JFS_IP(inode)->next_index)) {
			d->index = cpu_to_le32(add_index(tid, inode, bn, i));
			if (dtlck->index >= dtlck->maxcnt)
				dtlck = (struct dt_lock *) txLinelock(dtlck);
			lv = &dtlck->lv[dtlck->index];
			lv->offset = stbl[i];
			lv->length = 1;
			dtlck->index++;
		}
	}

	DT_PUTPAGE(mp);
	(void) txCommit(tid, 1, &inode, 0);
end:
	txEnd(tid);
}

struct jfs_dirent {
	loff_t position;
	int ino;
	u16 name_len;
	char name[0];
};

static inline struct jfs_dirent *next_jfs_dirent(struct jfs_dirent *dirent)
{
	return (struct jfs_dirent *)
		((char *)dirent +
		 ((sizeof (struct jfs_dirent) + dirent->name_len + 1 +
		   sizeof (loff_t) - 1) &
		  ~(sizeof (loff_t) - 1)));
}

int jfs_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct inode *ip = filp->f_path.dentry->d_inode;
	struct nls_table *codepage = JFS_SBI(ip->i_sb)->nls_tab;
	int rc = 0;
	loff_t dtpos;	
	struct dtoffset {
		s16 pn;
		s16 index;
		s32 unused;
	} *dtoffset = (struct dtoffset *) &dtpos;
	s64 bn;
	struct metapage *mp;
	dtpage_t *p;
	int index;
	s8 *stbl;
	struct btstack btstack;
	int i, next;
	struct ldtentry *d;
	struct dtslot *t;
	int d_namleft, len, outlen;
	unsigned long dirent_buf;
	char *name_ptr;
	u32 dir_index;
	int do_index = 0;
	uint loop_count = 0;
	struct jfs_dirent *jfs_dirent;
	int jfs_dirents;
	int overflow, fix_page, page_fixed = 0;
	static int unique_pos = 2;	

	if (filp->f_pos == DIREND)
		return 0;

	if (DO_INDEX(ip)) {
		do_index = 1;

		dir_index = (u32) filp->f_pos;

		if (dir_index > 1) {
			struct dir_table_slot dirtab_slot;

			if (dtEmpty(ip) ||
			    (dir_index >= JFS_IP(ip)->next_index)) {
				
				filp->f_pos = DIREND;
				return 0;
			}
		      repeat:
			rc = read_index(ip, dir_index, &dirtab_slot);
			if (rc) {
				filp->f_pos = DIREND;
				return rc;
			}
			if (dirtab_slot.flag == DIR_INDEX_FREE) {
				if (loop_count++ > JFS_IP(ip)->next_index) {
					jfs_err("jfs_readdir detected "
						   "infinite loop!");
					filp->f_pos = DIREND;
					return 0;
				}
				dir_index = le32_to_cpu(dirtab_slot.addr2);
				if (dir_index == -1) {
					filp->f_pos = DIREND;
					return 0;
				}
				goto repeat;
			}
			bn = addressDTS(&dirtab_slot);
			index = dirtab_slot.slot;
			DT_GETPAGE(ip, bn, mp, PSIZE, p, rc);
			if (rc) {
				filp->f_pos = DIREND;
				return 0;
			}
			if (p->header.flag & BT_INTERNAL) {
				jfs_err("jfs_readdir: bad index table");
				DT_PUTPAGE(mp);
				filp->f_pos = -1;
				return 0;
			}
		} else {
			if (dir_index == 0) {
				filp->f_pos = 0;
				if (filldir(dirent, ".", 1, 0, ip->i_ino,
					    DT_DIR))
					return 0;
			}
			filp->f_pos = 1;
			if (filldir(dirent, "..", 2, 1, PARENT(ip), DT_DIR))
				return 0;

			if (dtEmpty(ip)) {
				filp->f_pos = DIREND;
				return 0;
			}

			if ((rc = dtReadFirst(ip, &btstack)))
				return rc;

			DT_GETSEARCH(ip, btstack.top, bn, mp, p, index);
		}
	} else {
		dtpos = filp->f_pos;
		if (dtpos == 0) {
			

			if (filldir(dirent, ".", 1, filp->f_pos, ip->i_ino,
				    DT_DIR))
				return 0;
			dtoffset->index = 1;
			filp->f_pos = dtpos;
		}

		if (dtoffset->pn == 0) {
			if (dtoffset->index == 1) {
				

				if (filldir(dirent, "..", 2, filp->f_pos,
					    PARENT(ip), DT_DIR))
					return 0;
			} else {
				jfs_err("jfs_readdir called with "
					"invalid offset!");
			}
			dtoffset->pn = 1;
			dtoffset->index = 0;
			filp->f_pos = dtpos;
		}

		if (dtEmpty(ip)) {
			filp->f_pos = DIREND;
			return 0;
		}

		if ((rc = dtReadNext(ip, &filp->f_pos, &btstack))) {
			jfs_err("jfs_readdir: unexpected rc = %d "
				"from dtReadNext", rc);
			filp->f_pos = DIREND;
			return 0;
		}
		
		DT_GETSEARCH(ip, btstack.top, bn, mp, p, index);

		
		if (bn < 0) {
			filp->f_pos = DIREND;
			return 0;
		}
	}

	dirent_buf = __get_free_page(GFP_KERNEL);
	if (dirent_buf == 0) {
		DT_PUTPAGE(mp);
		jfs_warn("jfs_readdir: __get_free_page failed!");
		filp->f_pos = DIREND;
		return -ENOMEM;
	}

	while (1) {
		jfs_dirent = (struct jfs_dirent *) dirent_buf;
		jfs_dirents = 0;
		overflow = fix_page = 0;

		stbl = DT_GETSTBL(p);

		for (i = index; i < p->header.nextindex; i++) {
			d = (struct ldtentry *) & p->slot[stbl[i]];

			if (((long) jfs_dirent + d->namlen + 1) >
			    (dirent_buf + PAGE_SIZE)) {
				
				index = i;
				overflow = 1;
				break;
			}

			d_namleft = d->namlen;
			name_ptr = jfs_dirent->name;
			jfs_dirent->ino = le32_to_cpu(d->inumber);

			if (do_index) {
				len = min(d_namleft, DTLHDRDATALEN);
				jfs_dirent->position = le32_to_cpu(d->index);
				if ((jfs_dirent->position < 2) ||
				    (jfs_dirent->position >=
				     JFS_IP(ip)->next_index)) {
					if (!page_fixed && !isReadOnly(ip)) {
						fix_page = 1;
						overflow = 1;
						index = i;
						break;
					}
					jfs_dirent->position = unique_pos++;
				}
			} else {
				jfs_dirent->position = dtpos;
				len = min(d_namleft, DTLHDRDATALEN_LEGACY);
			}

			
			outlen = jfs_strfromUCS_le(name_ptr, d->name, len,
						   codepage);
			jfs_dirent->name_len = outlen;

			
			next = d->next;
			while (next >= 0) {
				t = (struct dtslot *) & p->slot[next];
				name_ptr += outlen;
				d_namleft -= len;
				
				if (d_namleft == 0) {
					jfs_error(ip->i_sb,
						  "JFS:Dtree error: ino = "
						  "%ld, bn=%Ld, index = %d",
						  (long)ip->i_ino,
						  (long long)bn,
						  i);
					goto skip_one;
				}
				len = min(d_namleft, DTSLOTDATALEN);
				outlen = jfs_strfromUCS_le(name_ptr, t->name,
							   len, codepage);
				jfs_dirent->name_len += outlen;

				next = t->next;
			}

			jfs_dirents++;
			jfs_dirent = next_jfs_dirent(jfs_dirent);
skip_one:
			if (!do_index)
				dtoffset->index++;
		}

		if (!overflow) {
			
			if (p->header.flag & BT_ROOT)
				bn = 0;
			else {
				bn = le64_to_cpu(p->header.next);
				index = 0;
				
				if (!do_index) {
					dtoffset->pn++;
					dtoffset->index = 0;
				}
			}
			page_fixed = 0;
		}

		
		DT_PUTPAGE(mp);

		jfs_dirent = (struct jfs_dirent *) dirent_buf;
		while (jfs_dirents--) {
			filp->f_pos = jfs_dirent->position;
			if (filldir(dirent, jfs_dirent->name,
				    jfs_dirent->name_len, filp->f_pos,
				    jfs_dirent->ino, DT_UNKNOWN))
				goto out;
			jfs_dirent = next_jfs_dirent(jfs_dirent);
		}

		if (fix_page) {
			add_missing_indices(ip, bn);
			page_fixed = 1;
		}

		if (!overflow && (bn == 0)) {
			filp->f_pos = DIREND;
			break;
		}

		DT_GETPAGE(ip, bn, mp, PSIZE, p, rc);
		if (rc) {
			free_page(dirent_buf);
			return rc;
		}
	}

      out:
	free_page(dirent_buf);

	return rc;
}


static int dtReadFirst(struct inode *ip, struct btstack * btstack)
{
	int rc = 0;
	s64 bn;
	int psize = 288;	
	struct metapage *mp;
	dtpage_t *p;
	s8 *stbl;
	struct btframe *btsp;
	pxd_t *xd;

	BT_CLR(btstack);	

	for (bn = 0;;) {
		DT_GETPAGE(ip, bn, mp, psize, p, rc);
		if (rc)
			return rc;

		if (p->header.flag & BT_LEAF) {
			
			btsp = btstack->top;
			btsp->bn = bn;
			btsp->index = 0;
			btsp->mp = mp;

			return 0;
		}

		if (BT_STACK_FULL(btstack)) {
			DT_PUTPAGE(mp);
			jfs_error(ip->i_sb, "dtReadFirst: btstack overrun");
			BT_STACK_DUMP(btstack);
			return -EIO;
		}
		
		BT_PUSH(btstack, bn, 0);

		
		stbl = DT_GETSTBL(p);
		xd = (pxd_t *) & p->slot[stbl[0]];

		
		bn = addressPXD(xd);
		psize = lengthPXD(xd) << JFS_SBI(ip->i_sb)->l2bsize;

		
		DT_PUTPAGE(mp);
	}
}


static int dtReadNext(struct inode *ip, loff_t * offset,
		      struct btstack * btstack)
{
	int rc = 0;
	struct dtoffset {
		s16 pn;
		s16 index;
		s32 unused;
	} *dtoffset = (struct dtoffset *) offset;
	s64 bn;
	struct metapage *mp;
	dtpage_t *p;
	int index;
	int pn;
	s8 *stbl;
	struct btframe *btsp, *parent;
	pxd_t *xd;

	if ((rc = dtReadFirst(ip, btstack)))
		return rc;

	
	DT_GETSEARCH(ip, btstack->top, bn, mp, p, index);

	
	pn = dtoffset->pn - 1;	
	index = dtoffset->index;

	
	if (pn == 0) {
		
		if (index < p->header.nextindex)
			goto out;

		if (p->header.flag & BT_ROOT) {
			bn = -1;
			goto out;
		}

		
		dtoffset->pn++;
		dtoffset->index = index = 0;
		goto a;
	}

	
	if (p->header.flag & BT_ROOT) {
		bn = -1;
		goto out;
	}

	
	if (pn > 1)
		goto b;

	
      a:
	bn = le64_to_cpu(p->header.next);

	
	DT_PUTPAGE(mp);

	
	if (bn == 0) {
		bn = -1;
		goto out;
	}

	goto c;

      b:
	
	DT_PUTPAGE(mp);

	
	btsp = btstack->top;
	parent = btsp - 1;
	bn = parent->bn;
	DT_GETPAGE(ip, bn, mp, PSIZE, p, rc);
	if (rc)
		return rc;

	
	while (pn >= p->header.nextindex) {
		pn -= p->header.nextindex;

		
		bn = le64_to_cpu(p->header.next);

		
		DT_PUTPAGE(mp);

		
		if (bn == 0) {
			bn = -1;
			goto out;
		}

		
		DT_GETPAGE(ip, bn, mp, PSIZE, p, rc);
		if (rc)
			return rc;

		
		parent->bn = bn;
	}

	
	stbl = DT_GETSTBL(p);
	xd = (pxd_t *) & p->slot[stbl[pn]];
	bn = addressPXD(xd);

	
	DT_PUTPAGE(mp);

      c:
	DT_GETPAGE(ip, bn, mp, PSIZE, p, rc);
	if (rc)
		return rc;

	if (index >= p->header.nextindex) {
		bn = le64_to_cpu(p->header.next);

		
		DT_PUTPAGE(mp);

		
		if (bn == 0) {
			bn = -1;
			goto out;
		}

		
		DT_GETPAGE(ip, bn, mp, PSIZE, p, rc);
		if (rc)
			return rc;

		
		dtoffset->pn++;
		dtoffset->index = 0;
	}

      out:
	
	btsp = btstack->top;
	btsp->bn = bn;
	btsp->index = dtoffset->index;
	btsp->mp = mp;

	return 0;
}


static int dtCompare(struct component_name * key,	
		     dtpage_t * p,	
		     int si)
{				
	wchar_t *kname;
	__le16 *name;
	int klen, namlen, len, rc;
	struct idtentry *ih;
	struct dtslot *t;


	kname = key->name;
	klen = key->namlen;

	ih = (struct idtentry *) & p->slot[si];
	si = ih->next;
	name = ih->name;
	namlen = ih->namlen;
	len = min(namlen, DTIHDRDATALEN);

	
	len = min(klen, len);
	if ((rc = UniStrncmp_le(kname, name, len)))
		return rc;

	klen -= len;
	namlen -= len;

	
	kname += len;
	while (klen > 0 && namlen > 0) {
		
		t = (struct dtslot *) & p->slot[si];
		len = min(namlen, DTSLOTDATALEN);
		len = min(klen, len);
		name = t->name;
		if ((rc = UniStrncmp_le(kname, name, len)))
			return rc;

		klen -= len;
		namlen -= len;
		kname += len;
		si = t->next;
	}

	return (klen - namlen);
}




static int ciCompare(struct component_name * key,	
		     dtpage_t * p,	
		     int si,	
		     int flag)
{
	wchar_t *kname, x;
	__le16 *name;
	int klen, namlen, len, rc;
	struct ldtentry *lh;
	struct idtentry *ih;
	struct dtslot *t;
	int i;


	kname = key->name;
	klen = key->namlen;

	if (p->header.flag & BT_LEAF) {
		lh = (struct ldtentry *) & p->slot[si];
		si = lh->next;
		name = lh->name;
		namlen = lh->namlen;
		if (flag & JFS_DIR_INDEX)
			len = min(namlen, DTLHDRDATALEN);
		else
			len = min(namlen, DTLHDRDATALEN_LEGACY);
	}
	else {
		ih = (struct idtentry *) & p->slot[si];
		si = ih->next;
		name = ih->name;
		namlen = ih->namlen;
		len = min(namlen, DTIHDRDATALEN);
	}

	
	len = min(klen, len);
	for (i = 0; i < len; i++, kname++, name++) {
		
		if ((flag & JFS_OS2) == JFS_OS2)
			x = UniToupper(le16_to_cpu(*name));
		else
			x = le16_to_cpu(*name);
		if ((rc = *kname - x))
			return rc;
	}

	klen -= len;
	namlen -= len;

	
	while (klen > 0 && namlen > 0) {
		
		t = (struct dtslot *) & p->slot[si];
		len = min(namlen, DTSLOTDATALEN);
		len = min(klen, len);
		name = t->name;
		for (i = 0; i < len; i++, kname++, name++) {
			
			if ((flag & JFS_OS2) == JFS_OS2)
				x = UniToupper(le16_to_cpu(*name));
			else
				x = le16_to_cpu(*name);

			if ((rc = *kname - x))
				return rc;
		}

		klen -= len;
		namlen -= len;
		si = t->next;
	}

	return (klen - namlen);
}


static int ciGetLeafPrefixKey(dtpage_t * lp, int li, dtpage_t * rp,
			       int ri, struct component_name * key, int flag)
{
	int klen, namlen;
	wchar_t *pl, *pr, *kname;
	struct component_name lkey;
	struct component_name rkey;

	lkey.name = kmalloc((JFS_NAME_MAX + 1) * sizeof(wchar_t),
					GFP_KERNEL);
	if (lkey.name == NULL)
		return -ENOMEM;

	rkey.name = kmalloc((JFS_NAME_MAX + 1) * sizeof(wchar_t),
					GFP_KERNEL);
	if (rkey.name == NULL) {
		kfree(lkey.name);
		return -ENOMEM;
	}

	
	dtGetKey(lp, li, &lkey, flag);
	lkey.name[lkey.namlen] = 0;

	if ((flag & JFS_OS2) == JFS_OS2)
		ciToUpper(&lkey);

	dtGetKey(rp, ri, &rkey, flag);
	rkey.name[rkey.namlen] = 0;


	if ((flag & JFS_OS2) == JFS_OS2)
		ciToUpper(&rkey);

	
	klen = 0;
	kname = key->name;
	namlen = min(lkey.namlen, rkey.namlen);
	for (pl = lkey.name, pr = rkey.name;
	     namlen; pl++, pr++, namlen--, klen++, kname++) {
		*kname = *pr;
		if (*pl != *pr) {
			key->namlen = klen + 1;
			goto free_names;
		}
	}

	
	if (lkey.namlen < rkey.namlen) {
		*kname = *pr;
		key->namlen = klen + 1;
	} else			
		key->namlen = klen;

free_names:
	kfree(lkey.name);
	kfree(rkey.name);
	return 0;
}



static void dtGetKey(dtpage_t * p, int i,	
		     struct component_name * key, int flag)
{
	int si;
	s8 *stbl;
	struct ldtentry *lh;
	struct idtentry *ih;
	struct dtslot *t;
	int namlen, len;
	wchar_t *kname;
	__le16 *name;

	
	stbl = DT_GETSTBL(p);
	si = stbl[i];
	if (p->header.flag & BT_LEAF) {
		lh = (struct ldtentry *) & p->slot[si];
		si = lh->next;
		namlen = lh->namlen;
		name = lh->name;
		if (flag & JFS_DIR_INDEX)
			len = min(namlen, DTLHDRDATALEN);
		else
			len = min(namlen, DTLHDRDATALEN_LEGACY);
	} else {
		ih = (struct idtentry *) & p->slot[si];
		si = ih->next;
		namlen = ih->namlen;
		name = ih->name;
		len = min(namlen, DTIHDRDATALEN);
	}

	key->namlen = namlen;
	kname = key->name;

	UniStrncpy_from_le(kname, name, len);

	while (si >= 0) {
		
		t = &p->slot[si];
		kname += len;
		namlen -= len;
		len = min(namlen, DTSLOTDATALEN);
		UniStrncpy_from_le(kname, t->name, len);

		si = t->next;
	}
}


static void dtInsertEntry(dtpage_t * p, int index, struct component_name * key,
			  ddata_t * data, struct dt_lock ** dtlock)
{
	struct dtslot *h, *t;
	struct ldtentry *lh = NULL;
	struct idtentry *ih = NULL;
	int hsi, fsi, klen, len, nextindex;
	wchar_t *kname;
	__le16 *name;
	s8 *stbl;
	pxd_t *xd;
	struct dt_lock *dtlck = *dtlock;
	struct lv *lv;
	int xsi, n;
	s64 bn = 0;
	struct metapage *mp = NULL;

	klen = key->namlen;
	kname = key->name;

	
	hsi = fsi = p->header.freelist;
	h = &p->slot[fsi];
	p->header.freelist = h->next;
	--p->header.freecnt;

	
	if (dtlck->index >= dtlck->maxcnt)
		dtlck = (struct dt_lock *) txLinelock(dtlck);

	lv = & dtlck->lv[dtlck->index];
	lv->offset = hsi;

	
	if (p->header.flag & BT_LEAF) {
		lh = (struct ldtentry *) h;
		lh->next = h->next;
		lh->inumber = cpu_to_le32(data->leaf.ino);
		lh->namlen = klen;
		name = lh->name;
		if (data->leaf.ip) {
			len = min(klen, DTLHDRDATALEN);
			if (!(p->header.flag & BT_ROOT))
				bn = addressPXD(&p->header.self);
			lh->index = cpu_to_le32(add_index(data->leaf.tid,
							  data->leaf.ip,
							  bn, index));
		} else
			len = min(klen, DTLHDRDATALEN_LEGACY);
	} else {
		ih = (struct idtentry *) h;
		ih->next = h->next;
		xd = (pxd_t *) ih;
		*xd = data->xd;
		ih->namlen = klen;
		name = ih->name;
		len = min(klen, DTIHDRDATALEN);
	}

	UniStrncpy_to_le(name, kname, len);

	n = 1;
	xsi = hsi;

	
	t = h;
	klen -= len;
	while (klen) {
		
		fsi = p->header.freelist;
		t = &p->slot[fsi];
		p->header.freelist = t->next;
		--p->header.freecnt;

		
		if (fsi != xsi + 1) {
			
			lv->length = n;
			dtlck->index++;

			
			if (dtlck->index < dtlck->maxcnt)
				lv++;
			else {
				dtlck = (struct dt_lock *) txLinelock(dtlck);
				lv = & dtlck->lv[0];
			}

			lv->offset = fsi;
			n = 0;
		}

		kname += len;
		len = min(klen, DTSLOTDATALEN);
		UniStrncpy_to_le(t->name, kname, len);

		n++;
		xsi = fsi;
		klen -= len;
	}

	
	lv->length = n;
	dtlck->index++;

	*dtlock = dtlck;

	
	if (h == t) {
		
		if (p->header.flag & BT_LEAF)
			lh->next = -1;
		else
			ih->next = -1;
	} else
		
		t->next = -1;

	
	stbl = DT_GETSTBL(p);
	nextindex = p->header.nextindex;
	if (index < nextindex) {
		memmove(stbl + index + 1, stbl + index, nextindex - index);

		if ((p->header.flag & BT_LEAF) && data->leaf.ip) {
			s64 lblock;

			mp = NULL;
			for (n = index + 1; n <= nextindex; n++) {
				lh = (struct ldtentry *) & (p->slot[stbl[n]]);
				modify_index(data->leaf.tid, data->leaf.ip,
					     le32_to_cpu(lh->index), bn, n,
					     &mp, &lblock);
			}
			if (mp)
				release_metapage(mp);
		}
	}

	stbl[index] = hsi;

	
	++p->header.nextindex;
}


static void dtMoveEntry(dtpage_t * sp, int si, dtpage_t * dp,
			struct dt_lock ** sdtlock, struct dt_lock ** ddtlock,
			int do_index)
{
	int ssi, next;		
	int di;			
	int dsi;		
	s8 *sstbl, *dstbl;	
	int snamlen, len;
	struct ldtentry *slh, *dlh = NULL;
	struct idtentry *sih, *dih = NULL;
	struct dtslot *h, *s, *d;
	struct dt_lock *sdtlck = *sdtlock, *ddtlck = *ddtlock;
	struct lv *slv, *dlv;
	int xssi, ns, nd;
	int sfsi;

	sstbl = (s8 *) & sp->slot[sp->header.stblindex];
	dstbl = (s8 *) & dp->slot[dp->header.stblindex];

	dsi = dp->header.freelist;	
	sfsi = sp->header.freelist;

	
	dlv = & ddtlck->lv[ddtlck->index];
	dlv->offset = dsi;

	
	slv = & sdtlck->lv[sdtlck->index];
	slv->offset = sstbl[si];
	xssi = slv->offset - 1;

	ns = nd = 0;
	for (di = 0; si < sp->header.nextindex; si++, di++) {
		ssi = sstbl[si];
		dstbl[di] = dsi;

		
		if (ssi != xssi + 1) {
			
			slv->length = ns;
			sdtlck->index++;

			
			if (sdtlck->index < sdtlck->maxcnt)
				slv++;
			else {
				sdtlck = (struct dt_lock *) txLinelock(sdtlck);
				slv = & sdtlck->lv[0];
			}

			slv->offset = ssi;
			ns = 0;
		}

		
		h = d = &dp->slot[dsi];

		
		s = &sp->slot[ssi];
		if (sp->header.flag & BT_LEAF) {
			
			slh = (struct ldtentry *) s;
			dlh = (struct ldtentry *) h;
			snamlen = slh->namlen;

			if (do_index) {
				len = min(snamlen, DTLHDRDATALEN);
				dlh->index = slh->index; 
			} else
				len = min(snamlen, DTLHDRDATALEN_LEGACY);

			memcpy(dlh, slh, 6 + len * 2);

			next = slh->next;

			
			dsi++;
			dlh->next = dsi;
		} else {
			sih = (struct idtentry *) s;
			snamlen = sih->namlen;

			len = min(snamlen, DTIHDRDATALEN);
			dih = (struct idtentry *) h;
			memcpy(dih, sih, 10 + len * 2);
			next = sih->next;

			dsi++;
			dih->next = dsi;
		}

		
		s->next = sfsi;
		s->cnt = 1;
		sfsi = ssi;

		ns++;
		nd++;
		xssi = ssi;

		snamlen -= len;
		while ((ssi = next) >= 0) {
			
			if (ssi != xssi + 1) {
				
				slv->length = ns;
				sdtlck->index++;

				
				if (sdtlck->index < sdtlck->maxcnt)
					slv++;
				else {
					sdtlck =
					    (struct dt_lock *)
					    txLinelock(sdtlck);
					slv = & sdtlck->lv[0];
				}

				slv->offset = ssi;
				ns = 0;
			}

			
			s = &sp->slot[ssi];

			
			d++;

			len = min(snamlen, DTSLOTDATALEN);
			UniStrncpy_le(d->name, s->name, len);

			ns++;
			nd++;
			xssi = ssi;

			dsi++;
			d->next = dsi;

			
			next = s->next;
			s->next = sfsi;
			s->cnt = 1;
			sfsi = ssi;

			snamlen -= len;
		}		

		
		if (h == d) {
			
			if (dp->header.flag & BT_LEAF)
				dlh->next = -1;
			else
				dih->next = -1;
		} else
			
			d->next = -1;
	}			

	
	slv->length = ns;
	sdtlck->index++;
	*sdtlock = sdtlck;

	dlv->length = nd;
	ddtlck->index++;
	*ddtlock = ddtlck;

	
	sp->header.freelist = sfsi;
	sp->header.freecnt += nd;

	
	dp->header.nextindex = di;

	dp->header.freelist = dsi;
	dp->header.freecnt -= nd;
}


static void dtDeleteEntry(dtpage_t * p, int fi, struct dt_lock ** dtlock)
{
	int fsi;		
	s8 *stbl;
	struct dtslot *t;
	int si, freecnt;
	struct dt_lock *dtlck = *dtlock;
	struct lv *lv;
	int xsi, n;

	
	stbl = DT_GETSTBL(p);
	fsi = stbl[fi];

	
	if (dtlck->index >= dtlck->maxcnt)
		dtlck = (struct dt_lock *) txLinelock(dtlck);
	lv = & dtlck->lv[dtlck->index];

	lv->offset = fsi;

	
	t = &p->slot[fsi];
	if (p->header.flag & BT_LEAF)
		si = ((struct ldtentry *) t)->next;
	else
		si = ((struct idtentry *) t)->next;
	t->next = si;
	t->cnt = 1;

	n = freecnt = 1;
	xsi = fsi;

	
	while (si >= 0) {
		
		if (si != xsi + 1) {
			
			lv->length = n;
			dtlck->index++;

			
			if (dtlck->index < dtlck->maxcnt)
				lv++;
			else {
				dtlck = (struct dt_lock *) txLinelock(dtlck);
				lv = & dtlck->lv[0];
			}

			lv->offset = si;
			n = 0;
		}

		n++;
		xsi = si;
		freecnt++;

		t = &p->slot[si];
		t->cnt = 1;
		si = t->next;
	}

	
	lv->length = n;
	dtlck->index++;

	*dtlock = dtlck;

	
	t->next = p->header.freelist;
	p->header.freelist = fsi;
	p->header.freecnt += freecnt;

	si = p->header.nextindex;
	if (fi < si - 1)
		memmove(&stbl[fi], &stbl[fi + 1], si - fi - 1);

	p->header.nextindex--;
}


static void dtTruncateEntry(dtpage_t * p, int ti, struct dt_lock ** dtlock)
{
	int tsi;		
	s8 *stbl;
	struct dtslot *t;
	int si, freecnt;
	struct dt_lock *dtlck = *dtlock;
	struct lv *lv;
	int fsi, xsi, n;

	
	stbl = DT_GETSTBL(p);
	tsi = stbl[ti];

	
	if (dtlck->index >= dtlck->maxcnt)
		dtlck = (struct dt_lock *) txLinelock(dtlck);
	lv = & dtlck->lv[dtlck->index];

	lv->offset = tsi;

	
	t = &p->slot[tsi];
	ASSERT(p->header.flag & BT_INTERNAL);
	((struct idtentry *) t)->namlen = 0;
	si = ((struct idtentry *) t)->next;
	((struct idtentry *) t)->next = -1;

	n = 1;
	freecnt = 0;
	fsi = si;
	xsi = tsi;

	
	while (si >= 0) {
		
		if (si != xsi + 1) {
			
			lv->length = n;
			dtlck->index++;

			
			if (dtlck->index < dtlck->maxcnt)
				lv++;
			else {
				dtlck = (struct dt_lock *) txLinelock(dtlck);
				lv = & dtlck->lv[0];
			}

			lv->offset = si;
			n = 0;
		}

		n++;
		xsi = si;
		freecnt++;

		t = &p->slot[si];
		t->cnt = 1;
		si = t->next;
	}

	
	lv->length = n;
	dtlck->index++;

	*dtlock = dtlck;

	
	if (freecnt == 0)
		return;
	t->next = p->header.freelist;
	p->header.freelist = fsi;
	p->header.freecnt += freecnt;
}


static void dtLinelockFreelist(dtpage_t * p,	
			       int m,	
			       struct dt_lock ** dtlock)
{
	int fsi;		
	struct dtslot *t;
	int si;
	struct dt_lock *dtlck = *dtlock;
	struct lv *lv;
	int xsi, n;

	
	fsi = p->header.freelist;

	
	if (dtlck->index >= dtlck->maxcnt)
		dtlck = (struct dt_lock *) txLinelock(dtlck);
	lv = & dtlck->lv[dtlck->index];

	lv->offset = fsi;

	n = 1;
	xsi = fsi;

	t = &p->slot[fsi];
	si = t->next;

	
	while (si < m && si >= 0) {
		
		if (si != xsi + 1) {
			
			lv->length = n;
			dtlck->index++;

			
			if (dtlck->index < dtlck->maxcnt)
				lv++;
			else {
				dtlck = (struct dt_lock *) txLinelock(dtlck);
				lv = & dtlck->lv[0];
			}

			lv->offset = si;
			n = 0;
		}

		n++;
		xsi = si;

		t = &p->slot[si];
		si = t->next;
	}

	
	lv->length = n;
	dtlck->index++;

	*dtlock = dtlck;
}


int dtModify(tid_t tid, struct inode *ip,
	 struct component_name * key, ino_t * orig_ino, ino_t new_ino, int flag)
{
	int rc;
	s64 bn;
	struct metapage *mp;
	dtpage_t *p;
	int index;
	struct btstack btstack;
	struct tlock *tlck;
	struct dt_lock *dtlck;
	struct lv *lv;
	s8 *stbl;
	int entry_si;		
	struct ldtentry *entry;

	if ((rc = dtSearch(ip, key, orig_ino, &btstack, flag)))
		return rc;

	
	DT_GETSEARCH(ip, btstack.top, bn, mp, p, index);

	BT_MARK_DIRTY(mp, ip);
	tlck = txLock(tid, ip, mp, tlckDTREE | tlckENTRY);
	dtlck = (struct dt_lock *) & tlck->lock;

	
	stbl = DT_GETSTBL(p);
	entry_si = stbl[index];

	
	ASSERT(dtlck->index == 0);
	lv = & dtlck->lv[0];
	lv->offset = entry_si;
	lv->length = 1;
	dtlck->index++;

	
	entry = (struct ldtentry *) & p->slot[entry_si];

	
	entry->inumber = cpu_to_le32(new_ino);

	
	DT_PUTPAGE(mp);

	return 0;
}
