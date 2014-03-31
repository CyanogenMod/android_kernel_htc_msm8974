/*
 *   Copyright (C) International Business Machines Corp., 2000-2005
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
#include <linux/module.h>
#include <linux/quotaops.h>
#include <linux/seq_file.h>
#include "jfs_incore.h"
#include "jfs_filsys.h"
#include "jfs_metapage.h"
#include "jfs_dmap.h"
#include "jfs_dinode.h"
#include "jfs_superblock.h"
#include "jfs_debug.h"

#define XT_INSERT	0x00000001

#define XT_CMP(CMP, K, X, OFFSET64)\
{\
	OFFSET64 = offsetXAD(X);\
	(CMP) = ((K) >= OFFSET64 + lengthXAD(X)) ? 1 :\
		((K) < OFFSET64) ? -1 : 0;\
}

#define XT_PUTENTRY(XAD, FLAG, OFF, LEN, ADDR)\
{\
	(XAD)->flag = (FLAG);\
	XADoffset((XAD), (OFF));\
	XADlength((XAD), (LEN));\
	XADaddress((XAD), (ADDR));\
}

#define XT_PAGE(IP, MP) BT_PAGE(IP, MP, xtpage_t, i_xtroot)

#define XT_GETPAGE(IP, BN, MP, SIZE, P, RC)\
{\
	BT_GETPAGE(IP, BN, MP, xtpage_t, SIZE, P, RC, i_xtroot)\
	if (!(RC))\
	{\
		if ((le16_to_cpu((P)->header.nextindex) < XTENTRYSTART) ||\
		    (le16_to_cpu((P)->header.nextindex) > le16_to_cpu((P)->header.maxentry)) ||\
		    (le16_to_cpu((P)->header.maxentry) > (((BN)==0)?XTROOTMAXSLOT:PSIZE>>L2XTSLOTSIZE)))\
		{\
			jfs_error((IP)->i_sb, "XT_GETPAGE: xtree page corrupt");\
			BT_PUTPAGE(MP);\
			MP = NULL;\
			RC = -EIO;\
		}\
	}\
}

#define XT_PUTPAGE(MP) BT_PUTPAGE(MP)

#define XT_GETSEARCH(IP, LEAF, BN, MP, P, INDEX) \
	BT_GETSEARCH(IP, LEAF, BN, MP, xtpage_t, P, INDEX, i_xtroot)
struct xtsplit {
	struct metapage *mp;
	s16 index;
	u8 flag;
	s64 off;
	s64 addr;
	int len;
	struct pxdlist *pxdlist;
};


#ifdef CONFIG_JFS_STATISTICS
static struct {
	uint search;
	uint fastSearch;
	uint split;
} xtStat;
#endif


static int xtSearch(struct inode *ip, s64 xoff, s64 *next, int *cmpp,
		    struct btstack * btstack, int flag);

static int xtSplitUp(tid_t tid,
		     struct inode *ip,
		     struct xtsplit * split, struct btstack * btstack);

static int xtSplitPage(tid_t tid, struct inode *ip, struct xtsplit * split,
		       struct metapage ** rmpp, s64 * rbnp);

static int xtSplitRoot(tid_t tid, struct inode *ip,
		       struct xtsplit * split, struct metapage ** rmpp);

#ifdef _STILL_TO_PORT
static int xtDeleteUp(tid_t tid, struct inode *ip, struct metapage * fmp,
		      xtpage_t * fp, struct btstack * btstack);

static int xtSearchNode(struct inode *ip,
			xad_t * xad,
			int *cmpp, struct btstack * btstack, int flag);

static int xtRelink(tid_t tid, struct inode *ip, xtpage_t * fp);
#endif				

int xtLookup(struct inode *ip, s64 lstart,
	     s64 llen, int *pflag, s64 * paddr, s32 * plen, int no_check)
{
	int rc = 0;
	struct btstack btstack;
	int cmp;
	s64 bn;
	struct metapage *mp;
	xtpage_t *p;
	int index;
	xad_t *xad;
	s64 next, size, xoff, xend;
	int xlen;
	s64 xaddr;

	*paddr = 0;
	*plen = llen;

	if (!no_check) {
		
		size = ((u64) ip->i_size + (JFS_SBI(ip->i_sb)->bsize - 1)) >>
		    JFS_SBI(ip->i_sb)->l2bsize;
		if (lstart >= size)
			return 0;
	}

	if ((rc = xtSearch(ip, lstart, &next, &cmp, &btstack, 0))) {
		jfs_err("xtLookup: xtSearch returned %d", rc);
		return rc;
	}

	
	XT_GETSEARCH(ip, btstack.top, bn, mp, p, index);

	if (cmp) {
		if (next)
			*plen = min(next - lstart, llen);
		goto out;
	}

	xad = &p->xad[index];
	xoff = offsetXAD(xad);
	xlen = lengthXAD(xad);
	xend = xoff + xlen;
	xaddr = addressXAD(xad);

	
	*pflag = xad->flag;
	*paddr = xaddr + (lstart - xoff);
	
	*plen = min(xend - lstart, llen);

      out:
	XT_PUTPAGE(mp);

	return rc;
}

static int xtSearch(struct inode *ip, s64 xoff,	s64 *nextp,
		    int *cmpp, struct btstack * btstack, int flag)
{
	struct jfs_inode_info *jfs_ip = JFS_IP(ip);
	int rc = 0;
	int cmp = 1;		
	s64 bn;			
	struct metapage *mp;	
	xtpage_t *p;		
	xad_t *xad;
	int base, index, lim, btindex;
	struct btframe *btsp;
	int nsplit = 0;		
	s64 t64;
	s64 next = 0;

	INCREMENT(xtStat.search);

	BT_CLR(btstack);

	btstack->nsplit = 0;

	for (bn = 0;;) {
		
		XT_GETPAGE(ip, bn, mp, PSIZE, p, rc);
		if (rc)
			return rc;

		if ((jfs_ip->btorder & BT_SEQUENTIAL) &&
		    (p->header.flag & BT_LEAF) &&
		    (index = jfs_ip->btindex) <
		    le16_to_cpu(p->header.nextindex)) {
			xad = &p->xad[index];
			t64 = offsetXAD(xad);
			if (xoff < t64 + lengthXAD(xad)) {
				if (xoff >= t64) {
					*cmpp = 0;
					goto out;
				}

				
				goto binarySearch;
			} else {	

				
				index++;
				if (index <
				    le16_to_cpu(p->header.nextindex)) {
					xad++;
					t64 = offsetXAD(xad);
					if (xoff < t64 + lengthXAD(xad)) {
						if (xoff >= t64) {
							*cmpp = 0;
							goto out;
						}

						*cmpp = 1;
						next = t64;
						goto out;
					}

					
					goto binarySearch;
				}

				*cmpp = 1;
				goto out;
			}

		      out:
			
			if (flag & XT_INSERT) {
				if (p->header.nextindex ==	
				    p->header.maxentry)
					nsplit++;
				else
					nsplit = 0;
				btstack->nsplit = nsplit;
			}

			
			btsp = btstack->top;
			btsp->bn = bn;
			btsp->index = index;
			btsp->mp = mp;

			
			jfs_ip->btindex = index;

			if (nextp)
				*nextp = next;

			INCREMENT(xtStat.fastSearch);
			return 0;
		}

		
	      binarySearch:
		lim = le16_to_cpu(p->header.nextindex) - XTENTRYSTART;

		for (base = XTENTRYSTART; lim; lim >>= 1) {
			index = base + (lim >> 1);

			XT_CMP(cmp, xoff, &p->xad[index], t64);
			if (cmp == 0) {
				if (p->header.flag & BT_LEAF) {
					*cmpp = cmp;

					
					if (flag & XT_INSERT) {
						if (p->header.nextindex ==
						    p->header.maxentry)
							nsplit++;
						else
							nsplit = 0;
						btstack->nsplit = nsplit;
					}

					
					btsp = btstack->top;
					btsp->bn = bn;
					btsp->index = index;
					btsp->mp = mp;

					
					btindex = jfs_ip->btindex;
					if (index == btindex ||
					    index == btindex + 1)
						jfs_ip->btorder = BT_SEQUENTIAL;
					else
						jfs_ip->btorder = BT_RANDOM;
					jfs_ip->btindex = index;

					return 0;
				}
				if (index < le16_to_cpu(p->header.nextindex)-1)
					next = offsetXAD(&p->xad[index + 1]);
				goto next;
			}

			if (cmp > 0) {
				base = index + 1;
				--lim;
			}
		}

		if (base < le16_to_cpu(p->header.nextindex))
			next = offsetXAD(&p->xad[base]);
		if (p->header.flag & BT_LEAF) {
			*cmpp = cmp;

			
			if (flag & XT_INSERT) {
				if (p->header.nextindex ==
				    p->header.maxentry)
					nsplit++;
				else
					nsplit = 0;
				btstack->nsplit = nsplit;
			}

			
			btsp = btstack->top;
			btsp->bn = bn;
			btsp->index = base;
			btsp->mp = mp;

			
			btindex = jfs_ip->btindex;
			if (base == btindex || base == btindex + 1)
				jfs_ip->btorder = BT_SEQUENTIAL;
			else
				jfs_ip->btorder = BT_RANDOM;
			jfs_ip->btindex = base;

			if (nextp)
				*nextp = next;

			return 0;
		}

		index = base ? base - 1 : base;

	      next:
		
		if (p->header.nextindex == p->header.maxentry)
			nsplit++;
		else
			nsplit = 0;

		
		if (BT_STACK_FULL(btstack)) {
			jfs_error(ip->i_sb, "stack overrun in xtSearch!");
			XT_PUTPAGE(mp);
			return -EIO;
		}
		BT_PUSH(btstack, bn, index);

		
		bn = addressXAD(&p->xad[index]);

		
		XT_PUTPAGE(mp);
	}
}

int xtInsert(tid_t tid,		
	     struct inode *ip, int xflag, s64 xoff, s32 xlen, s64 * xaddrp,
	     int flag)
{
	int rc = 0;
	s64 xaddr, hint;
	struct metapage *mp;	
	xtpage_t *p;		
	s64 bn;
	int index, nextindex;
	struct btstack btstack;	
	struct xtsplit split;	
	xad_t *xad;
	int cmp;
	s64 next;
	struct tlock *tlck;
	struct xtlock *xtlck;

	jfs_info("xtInsert: nxoff:0x%lx nxlen:0x%x", (ulong) xoff, xlen);

	if ((rc = xtSearch(ip, xoff, &next, &cmp, &btstack, XT_INSERT)))
		return rc;

	
	XT_GETSEARCH(ip, btstack.top, bn, mp, p, index);

	if ((cmp == 0) || (next && (xlen > next - xoff))) {
		rc = -EEXIST;
		goto out;
	}

	if ((xaddr = *xaddrp) == 0) {
		if (index > XTENTRYSTART) {
			xad = &p->xad[index - 1];
			hint = addressXAD(xad) + lengthXAD(xad) - 1;
		} else
			hint = 0;
		if ((rc = dquot_alloc_block(ip, xlen)))
			goto out;
		if ((rc = dbAlloc(ip, hint, (s64) xlen, &xaddr))) {
			dquot_free_block(ip, xlen);
			goto out;
		}
	}

	xflag |= XAD_NEW;

	nextindex = le16_to_cpu(p->header.nextindex);
	if (nextindex == le16_to_cpu(p->header.maxentry)) {
		split.mp = mp;
		split.index = index;
		split.flag = xflag;
		split.off = xoff;
		split.len = xlen;
		split.addr = xaddr;
		split.pxdlist = NULL;
		if ((rc = xtSplitUp(tid, ip, &split, &btstack))) {
			
			if (*xaddrp == 0) {
				dbFree(ip, xaddr, (s64) xlen);
				dquot_free_block(ip, xlen);
			}
			return rc;
		}

		*xaddrp = xaddr;
		return 0;
	}

	BT_MARK_DIRTY(mp, ip);

	
	if (index < nextindex)
		memmove(&p->xad[index + 1], &p->xad[index],
			(nextindex - index) * sizeof(xad_t));

	
	xad = &p->xad[index];
	XT_PUTENTRY(xad, xflag, xoff, xlen, xaddr);

	
	le16_add_cpu(&p->header.nextindex, 1);

	
	if (!test_cflag(COMMIT_Nolink, ip)) {
		tlck = txLock(tid, ip, mp, tlckXTREE | tlckGROW);
		xtlck = (struct xtlock *) & tlck->lock;
		xtlck->lwm.offset =
		    (xtlck->lwm.offset) ? min(index,
					      (int)xtlck->lwm.offset) : index;
		xtlck->lwm.length =
		    le16_to_cpu(p->header.nextindex) - xtlck->lwm.offset;
	}

	*xaddrp = xaddr;

      out:
	
	XT_PUTPAGE(mp);

	return rc;
}


static int
xtSplitUp(tid_t tid,
	  struct inode *ip, struct xtsplit * split, struct btstack * btstack)
{
	int rc = 0;
	struct metapage *smp;
	xtpage_t *sp;		
	struct metapage *rmp;
	s64 rbn;		
	struct metapage *rcmp;
	xtpage_t *rcp;		
	s64 rcbn;		
	int skip;		
	int nextindex;		
	struct btframe *parent;	
	xad_t *xad;
	s64 xaddr;
	int xlen;
	int nsplit;		
	struct pxdlist pxdlist;
	pxd_t *pxd;
	struct tlock *tlck;
	struct xtlock *xtlck;

	smp = split->mp;
	sp = XT_PAGE(ip, smp);

	
	if ((sp->header.flag & BT_ROOT) && (!S_ISDIR(ip->i_mode)) &&
	    (le16_to_cpu(sp->header.maxentry) < XTROOTMAXSLOT) &&
	    (JFS_IP(ip)->mode2 & INLINEEA)) {
		sp->header.maxentry = cpu_to_le16(XTROOTMAXSLOT);
		JFS_IP(ip)->mode2 &= ~INLINEEA;

		BT_MARK_DIRTY(smp, ip);

		
		skip = split->index;
		nextindex = le16_to_cpu(sp->header.nextindex);
		if (skip < nextindex)
			memmove(&sp->xad[skip + 1], &sp->xad[skip],
				(nextindex - skip) * sizeof(xad_t));

		
		xad = &sp->xad[skip];
		XT_PUTENTRY(xad, split->flag, split->off, split->len,
			    split->addr);

		
		le16_add_cpu(&sp->header.nextindex, 1);

		
		if (!test_cflag(COMMIT_Nolink, ip)) {
			tlck = txLock(tid, ip, smp, tlckXTREE | tlckGROW);
			xtlck = (struct xtlock *) & tlck->lock;
			xtlck->lwm.offset = (xtlck->lwm.offset) ?
			    min(skip, (int)xtlck->lwm.offset) : skip;
			xtlck->lwm.length =
			    le16_to_cpu(sp->header.nextindex) -
			    xtlck->lwm.offset;
		}

		return 0;
	}

	if (split->pxdlist == NULL) {
		nsplit = btstack->nsplit;
		split->pxdlist = &pxdlist;
		pxdlist.maxnpxd = pxdlist.npxd = 0;
		pxd = &pxdlist.pxd[0];
		xlen = JFS_SBI(ip->i_sb)->nbperpage;
		for (; nsplit > 0; nsplit--, pxd++) {
			if ((rc = dbAlloc(ip, (s64) 0, (s64) xlen, &xaddr))
			    == 0) {
				PXDaddress(pxd, xaddr);
				PXDlength(pxd, xlen);

				pxdlist.maxnpxd++;

				continue;
			}

			

			XT_PUTPAGE(smp);
			return rc;
		}
	}

	rc = (sp->header.flag & BT_ROOT) ?
	    xtSplitRoot(tid, ip, split, &rmp) :
	    xtSplitPage(tid, ip, split, &rmp, &rbn);

	XT_PUTPAGE(smp);

	if (rc)
		return -EIO;
	while ((parent = BT_POP(btstack)) != NULL) {
		

		
		rcmp = rmp;
		rcbn = rbn;
		rcp = XT_PAGE(ip, rcmp);

		
		XT_GETPAGE(ip, parent->bn, smp, PSIZE, sp, rc);
		if (rc) {
			XT_PUTPAGE(rcmp);
			return rc;
		}

		skip = parent->index + 1;

		nextindex = le16_to_cpu(sp->header.nextindex);
		if (nextindex == le16_to_cpu(sp->header.maxentry)) {
			
			split->mp = smp;
			split->index = skip;	
			split->flag = XAD_NEW;
			split->off = offsetXAD(&rcp->xad[XTENTRYSTART]);
			split->len = JFS_SBI(ip->i_sb)->nbperpage;
			split->addr = rcbn;

			
			XT_PUTPAGE(rcmp);

			rc = (sp->header.flag & BT_ROOT) ?
			    xtSplitRoot(tid, ip, split, &rmp) :
			    xtSplitPage(tid, ip, split, &rmp, &rbn);
			if (rc) {
				XT_PUTPAGE(smp);
				return rc;
			}

			XT_PUTPAGE(smp);
			
		}
		else {
			BT_MARK_DIRTY(smp, ip);

			if (skip < nextindex)
				memmove(&sp->xad[skip + 1], &sp->xad[skip],
					(nextindex -
					 skip) << L2XTSLOTSIZE);

			
			xad = &sp->xad[skip];
			XT_PUTENTRY(xad, XAD_NEW,
				    offsetXAD(&rcp->xad[XTENTRYSTART]),
				    JFS_SBI(ip->i_sb)->nbperpage, rcbn);

			
			le16_add_cpu(&sp->header.nextindex, 1);

			
			if (!test_cflag(COMMIT_Nolink, ip)) {
				tlck = txLock(tid, ip, smp,
					      tlckXTREE | tlckGROW);
				xtlck = (struct xtlock *) & tlck->lock;
				xtlck->lwm.offset = (xtlck->lwm.offset) ?
				    min(skip, (int)xtlck->lwm.offset) : skip;
				xtlck->lwm.length =
				    le16_to_cpu(sp->header.nextindex) -
				    xtlck->lwm.offset;
			}

			
			XT_PUTPAGE(smp);

			
			break;
		}
	}

	
	XT_PUTPAGE(rmp);

	return 0;
}


static int
xtSplitPage(tid_t tid, struct inode *ip,
	    struct xtsplit * split, struct metapage ** rmpp, s64 * rbnp)
{
	int rc = 0;
	struct metapage *smp;
	xtpage_t *sp;
	struct metapage *rmp;
	xtpage_t *rp;		
	s64 rbn;		
	struct metapage *mp;
	xtpage_t *p;
	s64 nextbn;
	int skip, maxentry, middle, righthalf, n;
	xad_t *xad;
	struct pxdlist *pxdlist;
	pxd_t *pxd;
	struct tlock *tlck;
	struct xtlock *sxtlck = NULL, *rxtlck = NULL;
	int quota_allocation = 0;

	smp = split->mp;
	sp = XT_PAGE(ip, smp);

	INCREMENT(xtStat.split);

	pxdlist = split->pxdlist;
	pxd = &pxdlist->pxd[pxdlist->npxd];
	pxdlist->npxd++;
	rbn = addressPXD(pxd);

	
	rc = dquot_alloc_block(ip, lengthPXD(pxd));
	if (rc)
		goto clean_up;

	quota_allocation += lengthPXD(pxd);

	rmp = get_metapage(ip, rbn, PSIZE, 1);
	if (rmp == NULL) {
		rc = -EIO;
		goto clean_up;
	}

	jfs_info("xtSplitPage: ip:0x%p smp:0x%p rmp:0x%p", ip, smp, rmp);

	BT_MARK_DIRTY(rmp, ip);

	rp = (xtpage_t *) rmp->data;
	rp->header.self = *pxd;
	rp->header.flag = sp->header.flag & BT_TYPE;
	rp->header.maxentry = sp->header.maxentry;	
	rp->header.nextindex = cpu_to_le16(XTENTRYSTART);

	BT_MARK_DIRTY(smp, ip);
	
	if (!test_cflag(COMMIT_Nolink, ip)) {
		tlck = txLock(tid, ip, rmp, tlckXTREE | tlckNEW);
		rxtlck = (struct xtlock *) & tlck->lock;
		rxtlck->lwm.offset = XTENTRYSTART;
		tlck = txLock(tid, ip, smp, tlckXTREE | tlckGROW);
		sxtlck = (struct xtlock *) & tlck->lock;
	}

	nextbn = le64_to_cpu(sp->header.next);
	rp->header.next = cpu_to_le64(nextbn);
	rp->header.prev = cpu_to_le64(addressPXD(&sp->header.self));
	sp->header.next = cpu_to_le64(rbn);

	skip = split->index;

	if (nextbn == 0 && skip == le16_to_cpu(sp->header.maxentry)) {
		
		xad = &rp->xad[XTENTRYSTART];
		XT_PUTENTRY(xad, split->flag, split->off, split->len,
			    split->addr);

		rp->header.nextindex = cpu_to_le16(XTENTRYSTART + 1);

		if (!test_cflag(COMMIT_Nolink, ip)) {
			
			rxtlck->lwm.length = 1;
		}

		*rmpp = rmp;
		*rbnp = rbn;

		jfs_info("xtSplitPage: sp:0x%p rp:0x%p", sp, rp);
		return 0;
	}


	if (nextbn != 0) {
		XT_GETPAGE(ip, nextbn, mp, PSIZE, p, rc);
		if (rc) {
			XT_PUTPAGE(rmp);
			goto clean_up;
		}

		BT_MARK_DIRTY(mp, ip);
		if (!test_cflag(COMMIT_Nolink, ip))
			tlck = txLock(tid, ip, mp, tlckXTREE | tlckRELINK);

		p->header.prev = cpu_to_le64(rbn);


		XT_PUTPAGE(mp);
	}

	maxentry = le16_to_cpu(sp->header.maxentry);
	middle = maxentry >> 1;
	righthalf = maxentry - middle;

	if (skip <= middle) {
		
		memmove(&rp->xad[XTENTRYSTART], &sp->xad[middle],
			righthalf << L2XTSLOTSIZE);

		
		if (skip < middle)
			memmove(&sp->xad[skip + 1], &sp->xad[skip],
				(middle - skip) << L2XTSLOTSIZE);

		
		xad = &sp->xad[skip];
		XT_PUTENTRY(xad, split->flag, split->off, split->len,
			    split->addr);

		
		sp->header.nextindex = cpu_to_le16(middle + 1);
		if (!test_cflag(COMMIT_Nolink, ip)) {
			sxtlck->lwm.offset = (sxtlck->lwm.offset) ?
			    min(skip, (int)sxtlck->lwm.offset) : skip;
		}

		rp->header.nextindex =
		    cpu_to_le16(XTENTRYSTART + righthalf);
	}
	else {
		
		n = skip - middle;
		memmove(&rp->xad[XTENTRYSTART], &sp->xad[middle],
			n << L2XTSLOTSIZE);

		
		n += XTENTRYSTART;
		xad = &rp->xad[n];
		XT_PUTENTRY(xad, split->flag, split->off, split->len,
			    split->addr);

		
		if (skip < maxentry)
			memmove(&rp->xad[n + 1], &sp->xad[skip],
				(maxentry - skip) << L2XTSLOTSIZE);

		
		sp->header.nextindex = cpu_to_le16(middle);
		if (!test_cflag(COMMIT_Nolink, ip)) {
			sxtlck->lwm.offset = (sxtlck->lwm.offset) ?
			    min(middle, (int)sxtlck->lwm.offset) : middle;
		}

		rp->header.nextindex = cpu_to_le16(XTENTRYSTART +
						   righthalf + 1);
	}

	if (!test_cflag(COMMIT_Nolink, ip)) {
		sxtlck->lwm.length = le16_to_cpu(sp->header.nextindex) -
		    sxtlck->lwm.offset;

		
		rxtlck->lwm.length = le16_to_cpu(rp->header.nextindex) -
		    XTENTRYSTART;
	}

	*rmpp = rmp;
	*rbnp = rbn;

	jfs_info("xtSplitPage: sp:0x%p rp:0x%p", sp, rp);
	return rc;

      clean_up:

	
	if (quota_allocation)
		dquot_free_block(ip, quota_allocation);

	return (rc);
}


static int
xtSplitRoot(tid_t tid,
	    struct inode *ip, struct xtsplit * split, struct metapage ** rmpp)
{
	xtpage_t *sp;
	struct metapage *rmp;
	xtpage_t *rp;
	s64 rbn;
	int skip, nextindex;
	xad_t *xad;
	pxd_t *pxd;
	struct pxdlist *pxdlist;
	struct tlock *tlck;
	struct xtlock *xtlck;
	int rc;

	sp = &JFS_IP(ip)->i_xtroot;

	INCREMENT(xtStat.split);

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

	jfs_info("xtSplitRoot: ip:0x%p rmp:0x%p", ip, rmp);

	BT_MARK_DIRTY(rmp, ip);

	rp = (xtpage_t *) rmp->data;
	rp->header.flag =
	    (sp->header.flag & BT_LEAF) ? BT_LEAF : BT_INTERNAL;
	rp->header.self = *pxd;
	rp->header.nextindex = cpu_to_le16(XTENTRYSTART);
	rp->header.maxentry = cpu_to_le16(PSIZE >> L2XTSLOTSIZE);

	
	rp->header.next = 0;
	rp->header.prev = 0;

	nextindex = le16_to_cpu(sp->header.maxentry);
	memmove(&rp->xad[XTENTRYSTART], &sp->xad[XTENTRYSTART],
		(nextindex - XTENTRYSTART) << L2XTSLOTSIZE);

	skip = split->index;
	
	if (skip != nextindex)
		memmove(&rp->xad[skip + 1], &rp->xad[skip],
			(nextindex - skip) * sizeof(xad_t));

	xad = &rp->xad[skip];
	XT_PUTENTRY(xad, split->flag, split->off, split->len, split->addr);

	
	rp->header.nextindex = cpu_to_le16(nextindex + 1);

	if (!test_cflag(COMMIT_Nolink, ip)) {
		tlck = txLock(tid, ip, rmp, tlckXTREE | tlckNEW);
		xtlck = (struct xtlock *) & tlck->lock;
		xtlck->lwm.offset = XTENTRYSTART;
		xtlck->lwm.length = le16_to_cpu(rp->header.nextindex) -
		    XTENTRYSTART;
	}

	BT_MARK_DIRTY(split->mp, ip);

	xad = &sp->xad[XTENTRYSTART];
	XT_PUTENTRY(xad, XAD_NEW, 0, JFS_SBI(ip->i_sb)->nbperpage, rbn);

	
	sp->header.flag &= ~BT_LEAF;
	sp->header.flag |= BT_INTERNAL;

	sp->header.nextindex = cpu_to_le16(XTENTRYSTART + 1);

	if (!test_cflag(COMMIT_Nolink, ip)) {
		tlck = txLock(tid, ip, split->mp, tlckXTREE | tlckGROW);
		xtlck = (struct xtlock *) & tlck->lock;
		xtlck->lwm.offset = XTENTRYSTART;
		xtlck->lwm.length = 1;
	}

	*rmpp = rmp;

	jfs_info("xtSplitRoot: sp:0x%p rp:0x%p", sp, rp);
	return 0;
}


int xtExtend(tid_t tid,		
	     struct inode *ip, s64 xoff,	
	     s32 xlen,		
	     int flag)
{
	int rc = 0;
	int cmp;
	struct metapage *mp;	
	xtpage_t *p;		
	s64 bn;
	int index, nextindex, len;
	struct btstack btstack;	
	struct xtsplit split;	
	xad_t *xad;
	s64 xaddr;
	struct tlock *tlck;
	struct xtlock *xtlck = NULL;

	jfs_info("xtExtend: nxoff:0x%lx nxlen:0x%x", (ulong) xoff, xlen);

	
	if ((rc = xtSearch(ip, xoff - 1, NULL, &cmp, &btstack, XT_INSERT)))
		return rc;

	
	XT_GETSEARCH(ip, btstack.top, bn, mp, p, index);

	if (cmp != 0) {
		XT_PUTPAGE(mp);
		jfs_error(ip->i_sb, "xtExtend: xtSearch did not find extent");
		return -EIO;
	}

	
	xad = &p->xad[index];
	if ((offsetXAD(xad) + lengthXAD(xad)) != xoff) {
		XT_PUTPAGE(mp);
		jfs_error(ip->i_sb, "xtExtend: extension is not contiguous");
		return -EIO;
	}

	BT_MARK_DIRTY(mp, ip);
	if (!test_cflag(COMMIT_Nolink, ip)) {
		tlck = txLock(tid, ip, mp, tlckXTREE | tlckGROW);
		xtlck = (struct xtlock *) & tlck->lock;
	}

	
	xlen = lengthXAD(xad) + xlen;
	if ((len = xlen - MAXXLEN) <= 0)
		goto extendOld;

	xoff = offsetXAD(xad) + MAXXLEN;
	xaddr = addressXAD(xad) + MAXXLEN;
	nextindex = le16_to_cpu(p->header.nextindex);

	if (nextindex == le16_to_cpu(p->header.maxentry)) {
		
		split.mp = mp;
		split.index = index + 1;
		split.flag = XAD_NEW;
		split.off = xoff;	
		split.len = len;
		split.addr = xaddr;
		split.pxdlist = NULL;
		if ((rc = xtSplitUp(tid, ip, &split, &btstack)))
			return rc;

		
		XT_GETPAGE(ip, bn, mp, PSIZE, p, rc);
		if (rc)
			return rc;
		if (p->header.flag & BT_INTERNAL) {
			ASSERT(p->header.nextindex ==
			       cpu_to_le16(XTENTRYSTART + 1));
			xad = &p->xad[XTENTRYSTART];
			bn = addressXAD(xad);
			XT_PUTPAGE(mp);

			
			XT_GETPAGE(ip, bn, mp, PSIZE, p, rc);
			if (rc)
				return rc;

			BT_MARK_DIRTY(mp, ip);
			if (!test_cflag(COMMIT_Nolink, ip)) {
				tlck = txLock(tid, ip, mp, tlckXTREE|tlckGROW);
				xtlck = (struct xtlock *) & tlck->lock;
			}
		}
	}
	else {
		
		xad = &p->xad[index + 1];
		XT_PUTENTRY(xad, XAD_NEW, xoff, len, xaddr);

		
		le16_add_cpu(&p->header.nextindex, 1);
	}

	
	xad = &p->xad[index];
	xlen = MAXXLEN;

      extendOld:
	XADlength(xad, xlen);
	if (!(xad->flag & XAD_NEW))
		xad->flag |= XAD_EXTENDED;

	if (!test_cflag(COMMIT_Nolink, ip)) {
		xtlck->lwm.offset =
		    (xtlck->lwm.offset) ? min(index,
					      (int)xtlck->lwm.offset) : index;
		xtlck->lwm.length =
		    le16_to_cpu(p->header.nextindex) - xtlck->lwm.offset;
	}

	
	XT_PUTPAGE(mp);

	return rc;
}

#ifdef _NOTYET
int xtTailgate(tid_t tid,		
	       struct inode *ip, s64 xoff,	
	       s32 xlen,	
	       s64 xaddr,	
	       int flag)
{
	int rc = 0;
	int cmp;
	struct metapage *mp;	
	xtpage_t *p;		
	s64 bn;
	int index, nextindex, llen, rlen;
	struct btstack btstack;	
	struct xtsplit split;	
	xad_t *xad;
	struct tlock *tlck;
	struct xtlock *xtlck = 0;
	struct tlock *mtlck;
	struct maplock *pxdlock;


	
	if ((rc = xtSearch(ip, xoff, NULL, &cmp, &btstack, XT_INSERT)))
		return rc;

	
	XT_GETSEARCH(ip, btstack.top, bn, mp, p, index);

	if (cmp != 0) {
		XT_PUTPAGE(mp);
		jfs_error(ip->i_sb, "xtTailgate: couldn't find extent");
		return -EIO;
	}

	
	nextindex = le16_to_cpu(p->header.nextindex);
	if (index != nextindex - 1) {
		XT_PUTPAGE(mp);
		jfs_error(ip->i_sb,
			  "xtTailgate: the entry found is not the last entry");
		return -EIO;
	}

	BT_MARK_DIRTY(mp, ip);
	if (!test_cflag(COMMIT_Nolink, ip)) {
		tlck = txLock(tid, ip, mp, tlckXTREE | tlckGROW);
		xtlck = (struct xtlock *) & tlck->lock;
	}

	
	xad = &p->xad[index];
	if ((llen = xoff - offsetXAD(xad)) == 0)
		goto updateOld;

	if (nextindex == le16_to_cpu(p->header.maxentry)) {
		
		split.mp = mp;
		split.index = index + 1;
		split.flag = XAD_NEW;
		split.off = xoff;	
		split.len = xlen;
		split.addr = xaddr;
		split.pxdlist = NULL;
		if ((rc = xtSplitUp(tid, ip, &split, &btstack)))
			return rc;

		
		XT_GETPAGE(ip, bn, mp, PSIZE, p, rc);
		if (rc)
			return rc;
		if (p->header.flag & BT_INTERNAL) {
			ASSERT(p->header.nextindex ==
			       cpu_to_le16(XTENTRYSTART + 1));
			xad = &p->xad[XTENTRYSTART];
			bn = addressXAD(xad);
			XT_PUTPAGE(mp);

			
			XT_GETPAGE(ip, bn, mp, PSIZE, p, rc);
			if (rc)
				return rc;

			BT_MARK_DIRTY(mp, ip);
			if (!test_cflag(COMMIT_Nolink, ip)) {
				tlck = txLock(tid, ip, mp, tlckXTREE|tlckGROW);
				xtlck = (struct xtlock *) & tlck->lock;
			}
		}
	}
	else {
		
		xad = &p->xad[index + 1];
		XT_PUTENTRY(xad, XAD_NEW, xoff, xlen, xaddr);

		
		le16_add_cpu(&p->header.nextindex, 1);
	}

	
	xad = &p->xad[index];

      updateOld:
	
	rlen = lengthXAD(xad) - llen;
	if (!(xad->flag & XAD_NEW)) {
		
		if (!test_cflag(COMMIT_Nolink, ip)) {
			mtlck = txMaplock(tid, ip, tlckMAP);
			pxdlock = (struct maplock *) & mtlck->lock;
			pxdlock->flag = mlckFREEPXD;
			PXDaddress(&pxdlock->pxd, addressXAD(xad) + llen);
			PXDlength(&pxdlock->pxd, rlen);
			pxdlock->index = 1;
		}
	} else
		
		dbFree(ip, addressXAD(xad) + llen, (s64) rlen);

	if (llen)
		
		XADlength(xad, llen);
	else
		
		XT_PUTENTRY(xad, XAD_NEW, xoff, xlen, xaddr);

	if (!test_cflag(COMMIT_Nolink, ip)) {
		xtlck->lwm.offset = (xtlck->lwm.offset) ?
		    min(index, (int)xtlck->lwm.offset) : index;
		xtlck->lwm.length = le16_to_cpu(p->header.nextindex) -
		    xtlck->lwm.offset;
	}

	
	XT_PUTPAGE(mp);

	return rc;
}
#endif 

int xtUpdate(tid_t tid, struct inode *ip, xad_t * nxad)
{				
	int rc = 0;
	int cmp;
	struct metapage *mp;	
	xtpage_t *p;		
	s64 bn;
	int index0, index, newindex, nextindex;
	struct btstack btstack;	
	struct xtsplit split;	
	xad_t *xad, *lxad, *rxad;
	int xflag;
	s64 nxoff, xoff;
	int nxlen, xlen, lxlen, rxlen;
	s64 nxaddr, xaddr;
	struct tlock *tlck;
	struct xtlock *xtlck = NULL;
	int newpage = 0;

	
	nxoff = offsetXAD(nxad);
	nxlen = lengthXAD(nxad);
	nxaddr = addressXAD(nxad);

	if ((rc = xtSearch(ip, nxoff, NULL, &cmp, &btstack, XT_INSERT)))
		return rc;

	
	XT_GETSEARCH(ip, btstack.top, bn, mp, p, index0);

	if (cmp != 0) {
		XT_PUTPAGE(mp);
		jfs_error(ip->i_sb, "xtUpdate: Could not find extent");
		return -EIO;
	}

	BT_MARK_DIRTY(mp, ip);
	if (!test_cflag(COMMIT_Nolink, ip)) {
		tlck = txLock(tid, ip, mp, tlckXTREE | tlckGROW);
		xtlck = (struct xtlock *) & tlck->lock;
	}

	xad = &p->xad[index0];
	xflag = xad->flag;
	xoff = offsetXAD(xad);
	xlen = lengthXAD(xad);
	xaddr = addressXAD(xad);

	
	if ((xoff > nxoff) ||
	    (nxoff + nxlen > xoff + xlen)) {
		XT_PUTPAGE(mp);
		jfs_error(ip->i_sb,
			  "xtUpdate: nXAD in not completely contained within XAD");
		return -EIO;
	}

	index = index0;
	newindex = index + 1;
	nextindex = le16_to_cpu(p->header.nextindex);

#ifdef  _JFS_WIP_NOCOALESCE
	if (xoff < nxoff)
		goto updateRight;

      replace:			
	if (nxlen == xlen) {
		
		*xad = *nxad;
		xad->flag = xflag & ~XAD_NOTRECORDED;

		goto out;
	} else			
		goto updateLeft;
#endif				

	if (xoff < nxoff)
		goto coalesceRight;

	
	if (index == XTENTRYSTART)
		goto replace;

	
	lxad = &p->xad[index - 1];
	lxlen = lengthXAD(lxad);
	if (!(lxad->flag & XAD_NOTRECORDED) &&
	    (nxoff == offsetXAD(lxad) + lxlen) &&
	    (nxaddr == addressXAD(lxad) + lxlen) &&
	    (lxlen + nxlen < MAXXLEN)) {
		
		index0 = index - 1;
		XADlength(lxad, lxlen + nxlen);

		if (!(lxad->flag & XAD_NEW))
			lxad->flag |= XAD_EXTENDED;

		if (xlen > nxlen) {
			
			XADoffset(xad, xoff + nxlen);
			XADlength(xad, xlen - nxlen);
			XADaddress(xad, xaddr + nxlen);
			goto out;
		} else {	

			
			if (index < nextindex - 1)
				memmove(&p->xad[index], &p->xad[index + 1],
					(nextindex - index -
					 1) << L2XTSLOTSIZE);

			p->header.nextindex =
			    cpu_to_le16(le16_to_cpu(p->header.nextindex) -
					1);

			index = index0;
			newindex = index + 1;
			nextindex = le16_to_cpu(p->header.nextindex);
			xoff = nxoff = offsetXAD(lxad);
			xlen = nxlen = lxlen + nxlen;
			xaddr = nxaddr = addressXAD(lxad);
			goto coalesceRight;
		}
	}

      replace:			
	if (nxlen == xlen) {
		
		*xad = *nxad;
		xad->flag = xflag & ~XAD_NOTRECORDED;

		goto coalesceRight;
	} else			
		goto updateLeft;

      coalesceRight:		
	
	if (newindex == nextindex) {
		if (xoff == nxoff)
			goto out;
		goto updateRight;
	}

	
	rxad = &p->xad[index + 1];
	rxlen = lengthXAD(rxad);
	if (!(rxad->flag & XAD_NOTRECORDED) &&
	    (nxoff + nxlen == offsetXAD(rxad)) &&
	    (nxaddr + nxlen == addressXAD(rxad)) &&
	    (rxlen + nxlen < MAXXLEN)) {
		
		XADoffset(rxad, nxoff);
		XADlength(rxad, rxlen + nxlen);
		XADaddress(rxad, nxaddr);

		if (!(rxad->flag & XAD_NEW))
			rxad->flag |= XAD_EXTENDED;

		if (xlen > nxlen)
			
			XADlength(xad, xlen - nxlen);
		else {		

			
			memmove(&p->xad[index], &p->xad[index + 1],
				(nextindex - index - 1) << L2XTSLOTSIZE);

			p->header.nextindex =
			    cpu_to_le16(le16_to_cpu(p->header.nextindex) -
					1);
		}

		goto out;
	} else if (xoff == nxoff)
		goto out;

	if (xoff >= nxoff) {
		XT_PUTPAGE(mp);
		jfs_error(ip->i_sb, "xtUpdate: xoff >= nxoff");
		return -EIO;
	}

      updateRight:		
	
	xad = &p->xad[index];
	XADlength(xad, nxoff - xoff);

	
	if (nextindex == le16_to_cpu(p->header.maxentry)) {

		
		split.mp = mp;
		split.index = newindex;
		split.flag = xflag & ~XAD_NOTRECORDED;
		split.off = nxoff;
		split.len = nxlen;
		split.addr = nxaddr;
		split.pxdlist = NULL;
		if ((rc = xtSplitUp(tid, ip, &split, &btstack)))
			return rc;

		
		XT_GETPAGE(ip, bn, mp, PSIZE, p, rc);
		if (rc)
			return rc;
		if (p->header.flag & BT_INTERNAL) {
			ASSERT(p->header.nextindex ==
			       cpu_to_le16(XTENTRYSTART + 1));
			xad = &p->xad[XTENTRYSTART];
			bn = addressXAD(xad);
			XT_PUTPAGE(mp);

			
			XT_GETPAGE(ip, bn, mp, PSIZE, p, rc);
			if (rc)
				return rc;

			BT_MARK_DIRTY(mp, ip);
			if (!test_cflag(COMMIT_Nolink, ip)) {
				tlck = txLock(tid, ip, mp, tlckXTREE|tlckGROW);
				xtlck = (struct xtlock *) & tlck->lock;
			}
		} else {
			
			if (newindex >
			    (le16_to_cpu(p->header.maxentry) >> 1)) {
				newindex =
				    newindex -
				    le16_to_cpu(p->header.nextindex) +
				    XTENTRYSTART;
				newpage = 1;
			}
		}
	} else {
		
		if (newindex < nextindex)
			memmove(&p->xad[newindex + 1], &p->xad[newindex],
				(nextindex - newindex) << L2XTSLOTSIZE);

		
		xad = &p->xad[newindex];
		*xad = *nxad;
		xad->flag = xflag & ~XAD_NOTRECORDED;

		
		p->header.nextindex =
		    cpu_to_le16(le16_to_cpu(p->header.nextindex) + 1);
	}

	if (nxoff + nxlen == xoff + xlen)
		goto out;

	
	if (newpage) {
		
		if (!test_cflag(COMMIT_Nolink, ip)) {
			xtlck->lwm.offset = (xtlck->lwm.offset) ?
			    min(index0, (int)xtlck->lwm.offset) : index0;
			xtlck->lwm.length =
			    le16_to_cpu(p->header.nextindex) -
			    xtlck->lwm.offset;
		}

		bn = le64_to_cpu(p->header.next);
		XT_PUTPAGE(mp);

		
		XT_GETPAGE(ip, bn, mp, PSIZE, p, rc);
		if (rc)
			return rc;

		BT_MARK_DIRTY(mp, ip);
		if (!test_cflag(COMMIT_Nolink, ip)) {
			tlck = txLock(tid, ip, mp, tlckXTREE | tlckGROW);
			xtlck = (struct xtlock *) & tlck->lock;
		}

		index0 = index = newindex;
	} else
		index++;

	newindex = index + 1;
	nextindex = le16_to_cpu(p->header.nextindex);
	xlen = xlen - (nxoff - xoff);
	xoff = nxoff;
	xaddr = nxaddr;

	
	if (nextindex == le16_to_cpu(p->header.maxentry)) {
		XT_PUTPAGE(mp);

		if ((rc = xtSearch(ip, nxoff, NULL, &cmp, &btstack, XT_INSERT)))
			return rc;

		
		XT_GETSEARCH(ip, btstack.top, bn, mp, p, index0);

		if (cmp != 0) {
			XT_PUTPAGE(mp);
			jfs_error(ip->i_sb, "xtUpdate: xtSearch failed");
			return -EIO;
		}

		if (index0 != index) {
			XT_PUTPAGE(mp);
			jfs_error(ip->i_sb,
				  "xtUpdate: unexpected value of index");
			return -EIO;
		}
	}

      updateLeft:		
	
	xad = &p->xad[index];
	*xad = *nxad;
	xad->flag = xflag & ~XAD_NOTRECORDED;

	
	xoff = xoff + nxlen;
	xlen = xlen - nxlen;
	xaddr = xaddr + nxlen;
	if (nextindex == le16_to_cpu(p->header.maxentry)) {
		
		split.mp = mp;
		split.index = newindex;
		split.flag = xflag;
		split.off = xoff;
		split.len = xlen;
		split.addr = xaddr;
		split.pxdlist = NULL;
		if ((rc = xtSplitUp(tid, ip, &split, &btstack)))
			return rc;

		
		XT_GETPAGE(ip, bn, mp, PSIZE, p, rc);
		if (rc)
			return rc;

		if (p->header.flag & BT_INTERNAL) {
			ASSERT(p->header.nextindex ==
			       cpu_to_le16(XTENTRYSTART + 1));
			xad = &p->xad[XTENTRYSTART];
			bn = addressXAD(xad);
			XT_PUTPAGE(mp);

			
			XT_GETPAGE(ip, bn, mp, PSIZE, p, rc);
			if (rc)
				return rc;

			BT_MARK_DIRTY(mp, ip);
			if (!test_cflag(COMMIT_Nolink, ip)) {
				tlck = txLock(tid, ip, mp, tlckXTREE|tlckGROW);
				xtlck = (struct xtlock *) & tlck->lock;
			}
		}
	} else {
		
		if (newindex < nextindex)
			memmove(&p->xad[newindex + 1], &p->xad[newindex],
				(nextindex - newindex) << L2XTSLOTSIZE);

		
		xad = &p->xad[newindex];
		XT_PUTENTRY(xad, xflag, xoff, xlen, xaddr);

		
		p->header.nextindex =
		    cpu_to_le16(le16_to_cpu(p->header.nextindex) + 1);
	}

      out:
	if (!test_cflag(COMMIT_Nolink, ip)) {
		xtlck->lwm.offset = (xtlck->lwm.offset) ?
		    min(index0, (int)xtlck->lwm.offset) : index0;
		xtlck->lwm.length = le16_to_cpu(p->header.nextindex) -
		    xtlck->lwm.offset;
	}

	
	XT_PUTPAGE(mp);

	return rc;
}


int xtAppend(tid_t tid,		
	     struct inode *ip, int xflag, s64 xoff, s32 maxblocks,
	     s32 * xlenp,	
	     s64 * xaddrp,	
	     int flag)
{
	int rc = 0;
	struct metapage *mp;	
	xtpage_t *p;		
	s64 bn, xaddr;
	int index, nextindex;
	struct btstack btstack;	
	struct xtsplit split;	
	xad_t *xad;
	int cmp;
	struct tlock *tlck;
	struct xtlock *xtlck;
	int nsplit, nblocks, xlen;
	struct pxdlist pxdlist;
	pxd_t *pxd;
	s64 next;

	xaddr = *xaddrp;
	xlen = *xlenp;
	jfs_info("xtAppend: xoff:0x%lx maxblocks:%d xlen:%d xaddr:0x%lx",
		 (ulong) xoff, maxblocks, xlen, (ulong) xaddr);

	if ((rc = xtSearch(ip, xoff, &next, &cmp, &btstack, XT_INSERT)))
		return rc;

	
	XT_GETSEARCH(ip, btstack.top, bn, mp, p, index);

	if (cmp == 0) {
		rc = -EEXIST;
		goto out;
	}

	if (next)
		xlen = min(xlen, (int)(next - xoff));
	xflag |= XAD_NEW;

	nextindex = le16_to_cpu(p->header.nextindex);
	if (nextindex < le16_to_cpu(p->header.maxentry))
		goto insertLeaf;

	nsplit = btstack.nsplit;
	split.pxdlist = &pxdlist;
	pxdlist.maxnpxd = pxdlist.npxd = 0;
	pxd = &pxdlist.pxd[0];
	nblocks = JFS_SBI(ip->i_sb)->nbperpage;
	for (; nsplit > 0; nsplit--, pxd++, xaddr += nblocks, maxblocks -= nblocks) {
		if ((rc = dbAllocBottomUp(ip, xaddr, (s64) nblocks)) == 0) {
			PXDaddress(pxd, xaddr);
			PXDlength(pxd, nblocks);

			pxdlist.maxnpxd++;

			continue;
		}

		

		goto out;
	}

	xlen = min(xlen, maxblocks);

	if ((rc = dbAllocBottomUp(ip, xaddr, (s64) xlen)))
		goto out;

	split.mp = mp;
	split.index = index;
	split.flag = xflag;
	split.off = xoff;
	split.len = xlen;
	split.addr = xaddr;
	if ((rc = xtSplitUp(tid, ip, &split, &btstack))) {
		
		dbFree(ip, *xaddrp, (s64) * xlenp);

		return rc;
	}

	*xaddrp = xaddr;
	*xlenp = xlen;
	return 0;

      insertLeaf:
	if ((rc = dbAllocBottomUp(ip, xaddr, (s64) xlen)))
		goto out;

	BT_MARK_DIRTY(mp, ip);
	tlck = txLock(tid, ip, mp, tlckXTREE | tlckGROW);
	xtlck = (struct xtlock *) & tlck->lock;

	
	xad = &p->xad[index];
	XT_PUTENTRY(xad, xflag, xoff, xlen, xaddr);

	
	le16_add_cpu(&p->header.nextindex, 1);

	xtlck->lwm.offset =
	    (xtlck->lwm.offset) ? min(index,(int) xtlck->lwm.offset) : index;
	xtlck->lwm.length = le16_to_cpu(p->header.nextindex) -
	    xtlck->lwm.offset;

	*xaddrp = xaddr;
	*xlenp = xlen;

      out:
	
	XT_PUTPAGE(mp);

	return rc;
}
#ifdef _STILL_TO_PORT

int xtDelete(tid_t tid, struct inode *ip, s64 xoff, s32 xlen, int flag)
{
	int rc = 0;
	struct btstack btstack;
	int cmp;
	s64 bn;
	struct metapage *mp;
	xtpage_t *p;
	int index, nextindex;
	struct tlock *tlck;
	struct xtlock *xtlck;

	if ((rc = xtSearch(ip, xoff, NULL, &cmp, &btstack, 0)))
		return rc;

	XT_GETSEARCH(ip, btstack.top, bn, mp, p, index);
	if (cmp) {
		
		XT_PUTPAGE(mp);
		return -ENOENT;
	}

	nextindex = le16_to_cpu(p->header.nextindex);
	le16_add_cpu(&p->header.nextindex, -1);

	if (p->header.nextindex == cpu_to_le16(XTENTRYSTART))
		return (xtDeleteUp(tid, ip, mp, p, &btstack));

	BT_MARK_DIRTY(mp, ip);
	tlck = txLock(tid, ip, mp, tlckXTREE);
	xtlck = (struct xtlock *) & tlck->lock;
	xtlck->lwm.offset =
	    (xtlck->lwm.offset) ? min(index, xtlck->lwm.offset) : index;

	
	if (index < nextindex - 1)
		memmove(&p->xad[index], &p->xad[index + 1],
			(nextindex - index - 1) * sizeof(xad_t));

	XT_PUTPAGE(mp);

	return 0;
}


static int
xtDeleteUp(tid_t tid, struct inode *ip,
	   struct metapage * fmp, xtpage_t * fp, struct btstack * btstack)
{
	int rc = 0;
	struct metapage *mp;
	xtpage_t *p;
	int index, nextindex;
	s64 xaddr;
	int xlen;
	struct btframe *parent;
	struct tlock *tlck;
	struct xtlock *xtlck;

	if (fp->header.flag & BT_ROOT) {
		
		fp->header.flag &= ~BT_INTERNAL;
		fp->header.flag |= BT_LEAF;
		fp->header.nextindex = cpu_to_le16(XTENTRYSTART);

		

		return 0;
	}

	if ((rc = xtRelink(tid, ip, fp))) {
		XT_PUTPAGE(fmp);
		return rc;
	}

	xaddr = addressPXD(&fp->header.self);
	xlen = lengthPXD(&fp->header.self);
	
	dbFree(ip, xaddr, (s64) xlen);

	
	discard_metapage(fmp);

	while ((parent = BT_POP(btstack)) != NULL) {
		
		XT_GETPAGE(ip, parent->bn, mp, PSIZE, p, rc);
		if (rc)
			return rc;

		index = parent->index;

		nextindex = le16_to_cpu(p->header.nextindex);

		if (nextindex == 1) {
			if (p->header.flag & BT_ROOT) {
				
				p->header.flag &= ~BT_INTERNAL;
				p->header.flag |= BT_LEAF;
				p->header.nextindex =
				    cpu_to_le16(XTENTRYSTART);

				

				break;
			} else {
				
				if ((rc = xtRelink(tid, ip, p)))
					return rc;

				xaddr = addressPXD(&p->header.self);
				
				dbFree(ip, xaddr,
				       (s64) JFS_SBI(ip->i_sb)->nbperpage);

				
				discard_metapage(mp);

				
				continue;
			}
		}
		else {
			BT_MARK_DIRTY(mp, ip);
			tlck = txLock(tid, ip, mp, tlckXTREE);
			xtlck = (struct xtlock *) & tlck->lock;
			xtlck->lwm.offset =
			    (xtlck->lwm.offset) ? min(index,
						      xtlck->lwm.
						      offset) : index;

			if (index < nextindex - 1)
				memmove(&p->xad[index], &p->xad[index + 1],
					(nextindex - index -
					 1) << L2XTSLOTSIZE);

			le16_add_cpu(&p->header.nextindex, -1);
			jfs_info("xtDeleteUp(entry): 0x%lx[%d]",
				 (ulong) parent->bn, index);
		}

		
		XT_PUTPAGE(mp);

		
		break;
	}

	return 0;
}


int
xtRelocate(tid_t tid, struct inode * ip, xad_t * oxad,	
	   s64 nxaddr,		
	   int xtype)
{				
	int rc = 0;
	struct tblock *tblk;
	struct tlock *tlck;
	struct xtlock *xtlck;
	struct metapage *mp, *pmp, *lmp, *rmp;	
	xtpage_t *p, *pp, *rp, *lp;	
	xad_t *xad;
	pxd_t *pxd;
	s64 xoff, xsize;
	int xlen;
	s64 oxaddr, sxaddr, dxaddr, nextbn, prevbn;
	cbuf_t *cp;
	s64 offset, nbytes, nbrd, pno;
	int nb, npages, nblks;
	s64 bn;
	int cmp;
	int index;
	struct pxd_lock *pxdlock;
	struct btstack btstack;	

	xtype = xtype & EXTENT_TYPE;

	xoff = offsetXAD(oxad);
	oxaddr = addressXAD(oxad);
	xlen = lengthXAD(oxad);

	
	offset = xoff << JFS_SBI(ip->i_sb)->l2bsize;
	if (offset >= ip->i_size)
		return -ESTALE;	

	jfs_info("xtRelocate: xtype:%d xoff:0x%lx xlen:0x%x xaddr:0x%lx:0x%lx",
		 xtype, (ulong) xoff, xlen, (ulong) oxaddr, (ulong) nxaddr);

	if (xtype == DATAEXT) {
		
		rc = xtSearch(ip, xoff, NULL, &cmp, &btstack, 0);
		if (rc)
			return rc;

		
		XT_GETSEARCH(ip, btstack.top, bn, pmp, pp, index);

		if (cmp) {
			XT_PUTPAGE(pmp);
			return -ESTALE;
		}

		
		xad = &pp->xad[index];
		if (addressXAD(xad) != oxaddr || lengthXAD(xad) != xlen) {
			XT_PUTPAGE(pmp);
			return -ESTALE;
		}
	} else {		

		
		rc = xtSearchNode(ip, oxad, &cmp, &btstack, 0);
		if (rc)
			return rc;

		
		XT_GETSEARCH(ip, btstack.top, bn, pmp, pp, index);

		if (cmp) {
			XT_PUTPAGE(pmp);
			return -ESTALE;
		}

		xad = &pp->xad[index];
	}
	jfs_info("xtRelocate: parent xad entry validated.");

	if (xtype == DATAEXT) {
		if (xad->flag & XAD_NOTRECORDED)
			goto out;
		else
			
			XT_PUTPAGE(pmp);

		offset = xoff << JFS_SBI(ip->i_sb)->l2bsize;
		assert((offset & CM_OFFSET) == 0);
		nbytes = xlen << JFS_SBI(ip->i_sb)->l2bsize;
		pno = offset >> CM_L2BSIZE;
		npages = (nbytes + (CM_BSIZE - 1)) >> CM_L2BSIZE;
		sxaddr = oxaddr;
		dxaddr = nxaddr;

		
		for (nbrd = 0; nbrd < nbytes; nbrd += nb,
		     offset += nb, pno++, npages--) {
			
			nb = min(nbytes - nbrd, CM_BSIZE);

			
			if (rc = cmRead(ip, offset, npages, &cp))
				break;

			assert(addressPXD(&cp->cm_pxd) == sxaddr);
			assert(!cp->cm_modified);

			
			nblks = nb >> JFS_IP(ip->i_sb)->l2bsize;
			cmSetXD(ip, cp, pno, dxaddr, nblks);

			
			cmPut(cp, true);

			dxaddr += nblks;
			sxaddr += nblks;
		}

		
		if ((rc = xtSearch(ip, xoff, NULL, &cmp, &btstack, 0)))
			return rc;

		XT_GETSEARCH(ip, btstack.top, bn, pmp, pp, index);
		jfs_info("xtRelocate: target data extent relocated.");
	} else {		

		XT_GETPAGE(ip, oxaddr, mp, PSIZE, p, rc);
		if (rc) {
			XT_PUTPAGE(pmp);
			return rc;
		}

		rmp = NULL;
		if (p->header.next) {
			nextbn = le64_to_cpu(p->header.next);
			XT_GETPAGE(ip, nextbn, rmp, PSIZE, rp, rc);
			if (rc) {
				XT_PUTPAGE(pmp);
				XT_PUTPAGE(mp);
				return (rc);
			}
		}

		lmp = NULL;
		if (p->header.prev) {
			prevbn = le64_to_cpu(p->header.prev);
			XT_GETPAGE(ip, prevbn, lmp, PSIZE, lp, rc);
			if (rc) {
				XT_PUTPAGE(pmp);
				XT_PUTPAGE(mp);
				if (rmp)
					XT_PUTPAGE(rmp);
				return (rc);
			}
		}

		

		if (lmp) {
			BT_MARK_DIRTY(lmp, ip);
			tlck = txLock(tid, ip, lmp, tlckXTREE | tlckRELINK);
			lp->header.next = cpu_to_le64(nxaddr);
			XT_PUTPAGE(lmp);
		}

		if (rmp) {
			BT_MARK_DIRTY(rmp, ip);
			tlck = txLock(tid, ip, rmp, tlckXTREE | tlckRELINK);
			rp->header.prev = cpu_to_le64(nxaddr);
			XT_PUTPAGE(rmp);
		}

		BT_MARK_DIRTY(mp, ip);
		
		tlck = txLock(tid, ip, mp, tlckXTREE | tlckNEW);
		xtlck = (struct xtlock *) & tlck->lock;

		
		pxd = &p->header.self;
		PXDaddress(pxd, nxaddr);

		
		xtlck->lwm.length =
		    le16_to_cpu(p->header.nextindex) - xtlck->lwm.offset;

		
		xsize = xlen << JFS_SBI(ip->i_sb)->l2bsize;
		bmSetXD(mp, nxaddr, xsize);

		
		XT_PUTPAGE(mp);
		jfs_info("xtRelocate: target xtpage relocated.");
	}

      out:
	if (xtype == DATAEXT)
		tlck = txMaplock(tid, ip, tlckMAP);
	else			
		tlck = txMaplock(tid, ip, tlckMAP | tlckRELOCATE);

	pxdlock = (struct pxd_lock *) & tlck->lock;
	pxdlock->flag = mlckFREEPXD;
	PXDaddress(&pxdlock->pxd, oxaddr);
	PXDlength(&pxdlock->pxd, xlen);
	pxdlock->index = 1;

	jfs_info("xtRelocate: update parent xad entry.");
	BT_MARK_DIRTY(pmp, ip);
	tlck = txLock(tid, ip, pmp, tlckXTREE | tlckGROW);
	xtlck = (struct xtlock *) & tlck->lock;

	
	xad = &pp->xad[index];
	xad->flag |= XAD_NEW;
	XADaddress(xad, nxaddr);

	xtlck->lwm.offset = min(index, xtlck->lwm.offset);
	xtlck->lwm.length = le16_to_cpu(pp->header.nextindex) -
	    xtlck->lwm.offset;

	
	XT_PUTPAGE(pmp);

	return rc;
}


static int xtSearchNode(struct inode *ip, xad_t * xad,	
			int *cmpp, struct btstack * btstack, int flag)
{
	int rc = 0;
	s64 xoff, xaddr;
	int xlen;
	int cmp = 1;		
	s64 bn;			
	struct metapage *mp;	
	xtpage_t *p;		
	int base, index, lim;
	struct btframe *btsp;
	s64 t64;

	BT_CLR(btstack);

	xoff = offsetXAD(xad);
	xlen = lengthXAD(xad);
	xaddr = addressXAD(xad);

	for (bn = 0;;) {
		
		XT_GETPAGE(ip, bn, mp, PSIZE, p, rc);
		if (rc)
			return rc;
		if (p->header.flag & BT_LEAF) {
			XT_PUTPAGE(mp);
			return -ESTALE;
		}

		lim = le16_to_cpu(p->header.nextindex) - XTENTRYSTART;

		for (base = XTENTRYSTART; lim; lim >>= 1) {
			index = base + (lim >> 1);

			XT_CMP(cmp, xoff, &p->xad[index], t64);
			if (cmp == 0) {
				if (xaddr == addressXAD(&p->xad[index]) &&
				    xoff == offsetXAD(&p->xad[index])) {
					*cmpp = cmp;

					
					btsp = btstack->top;
					btsp->bn = bn;
					btsp->index = index;
					btsp->mp = mp;

					return 0;
				}

				
				goto next;
			}

			if (cmp > 0) {
				base = index + 1;
				--lim;
			}
		}

		index = base ? base - 1 : base;

	      next:
		
		bn = addressXAD(&p->xad[index]);

		
		XT_PUTPAGE(mp);
	}
}


static int xtRelink(tid_t tid, struct inode *ip, xtpage_t * p)
{
	int rc = 0;
	struct metapage *mp;
	s64 nextbn, prevbn;
	struct tlock *tlck;

	nextbn = le64_to_cpu(p->header.next);
	prevbn = le64_to_cpu(p->header.prev);

	
	if (nextbn != 0) {
		XT_GETPAGE(ip, nextbn, mp, PSIZE, p, rc);
		if (rc)
			return rc;

		BT_MARK_DIRTY(mp, ip);
		tlck = txLock(tid, ip, mp, tlckXTREE | tlckRELINK);

		

		p->header.prev = cpu_to_le64(prevbn);

		XT_PUTPAGE(mp);
	}

	
	if (prevbn != 0) {
		XT_GETPAGE(ip, prevbn, mp, PSIZE, p, rc);
		if (rc)
			return rc;

		BT_MARK_DIRTY(mp, ip);
		tlck = txLock(tid, ip, mp, tlckXTREE | tlckRELINK);

		

		p->header.next = le64_to_cpu(nextbn);

		XT_PUTPAGE(mp);
	}

	return 0;
}
#endif				


void xtInitRoot(tid_t tid, struct inode *ip)
{
	xtpage_t *p;

	txLock(tid, ip, (struct metapage *) &JFS_IP(ip)->bxflag,
		      tlckXTREE | tlckNEW);
	p = &JFS_IP(ip)->i_xtroot;

	p->header.flag = DXD_INDEX | BT_ROOT | BT_LEAF;
	p->header.nextindex = cpu_to_le16(XTENTRYSTART);

	if (S_ISDIR(ip->i_mode))
		p->header.maxentry = cpu_to_le16(XTROOTINITSLOT_DIR);
	else {
		p->header.maxentry = cpu_to_le16(XTROOTINITSLOT);
		ip->i_size = 0;
	}


	return;
}


#define MAX_TRUNCATE_LEAVES 50

s64 xtTruncate(tid_t tid, struct inode *ip, s64 newsize, int flag)
{
	int rc = 0;
	s64 teof;
	struct metapage *mp;
	xtpage_t *p;
	s64 bn;
	int index, nextindex;
	xad_t *xad;
	s64 xoff, xaddr;
	int xlen, len, freexlen;
	struct btstack btstack;
	struct btframe *parent;
	struct tblock *tblk = NULL;
	struct tlock *tlck = NULL;
	struct xtlock *xtlck = NULL;
	struct xdlistlock xadlock;	
	struct pxd_lock *pxdlock;		
	s64 nfreed;
	int freed, log;
	int locked_leaves = 0;

	
	if (tid) {
		tblk = tid_to_tblock(tid);
		tblk->xflag |= flag;
	}

	nfreed = 0;

	flag &= COMMIT_MAP;
	assert(flag != COMMIT_PMAP);

	if (flag == COMMIT_PWMAP)
		log = 1;
	else {
		log = 0;
		xadlock.flag = mlckFREEXADLIST;
		xadlock.index = 1;
	}


	teof = (newsize + (JFS_SBI(ip->i_sb)->bsize - 1)) >>
	    JFS_SBI(ip->i_sb)->l2bsize;

	
	BT_CLR(&btstack);

	bn = 0;

      getPage:
	XT_GETPAGE(ip, bn, mp, PSIZE, p, rc);
	if (rc)
		return rc;

	
	index = le16_to_cpu(p->header.nextindex) - 1;


	if (p->header.next) {
		if (log)
			tlck = txLock(tid, ip, mp, tlckXTREE|tlckGROW);
		BT_MARK_DIRTY(mp, ip);
		p->header.next = 0;
	}

	if (p->header.flag & BT_INTERNAL)
		goto getChild;

	freed = 0;

	
	xad = &p->xad[index];
	xoff = offsetXAD(xad);
	xlen = lengthXAD(xad);
	if (teof >= xoff + xlen) {
		XT_PUTPAGE(mp);
		goto getParent;
	}

	
	if (log) {
		if (++locked_leaves > MAX_TRUNCATE_LEAVES) {
			XT_PUTPAGE(mp);
			newsize = (xoff + xlen) << JFS_SBI(ip->i_sb)->l2bsize;
			goto getParent;
		}
		tlck = txLock(tid, ip, mp, tlckXTREE);
		tlck->type = tlckXTREE | tlckTRUNCATE;
		xtlck = (struct xtlock *) & tlck->lock;
		xtlck->hwm.offset = le16_to_cpu(p->header.nextindex) - 1;
	}
	BT_MARK_DIRTY(mp, ip);

	for (; index >= XTENTRYSTART; index--) {
		xad = &p->xad[index];
		xoff = offsetXAD(xad);
		xlen = lengthXAD(xad);
		xaddr = addressXAD(xad);

		if (S_ISDIR(ip->i_mode) && (teof == 0))
			invalidate_xad_metapages(ip, *xad);
		if (teof < xoff) {
			nfreed += xlen;
			continue;
		}


		if (teof == xoff) {
			nfreed += xlen;

			if (index == XTENTRYSTART)
				break;

			nextindex = index;
		}
		else if (teof < xoff + xlen) {
			
			len = teof - xoff;
			freexlen = xlen - len;
			XADlength(xad, len);

			
			xaddr += len;
			if (log) {	
				xtlck->lwm.offset = (xtlck->lwm.offset) ?
				    min(index, (int)xtlck->lwm.offset) : index;
				xtlck->lwm.length = index + 1 -
				    xtlck->lwm.offset;
				xtlck->twm.offset = index;
				pxdlock = (struct pxd_lock *) & xtlck->pxdlock;
				pxdlock->flag = mlckFREEPXD;
				PXDaddress(&pxdlock->pxd, xaddr);
				PXDlength(&pxdlock->pxd, freexlen);
			}
			
			else {	

				pxdlock = (struct pxd_lock *) & xadlock;
				pxdlock->flag = mlckFREEPXD;
				PXDaddress(&pxdlock->pxd, xaddr);
				PXDlength(&pxdlock->pxd, freexlen);
				txFreeMap(ip, pxdlock, NULL, COMMIT_WMAP);

				
				xadlock.flag = mlckFREEXADLIST;
			}

			
			nextindex = index + 1;

			nfreed += freexlen;
		}
		else {		

			nextindex = index + 1;
		}

		if (nextindex < le16_to_cpu(p->header.nextindex)) {
			if (!log) {	
				xadlock.xdlist = &p->xad[nextindex];
				xadlock.count =
				    le16_to_cpu(p->header.nextindex) -
				    nextindex;
				txFreeMap(ip, (struct maplock *) & xadlock,
					  NULL, COMMIT_WMAP);
			}
			p->header.nextindex = cpu_to_le16(nextindex);
		}

		XT_PUTPAGE(mp);

		
		goto getParent;
	}			

	freed = 1;

	if (log) {		
		tlck->type = tlckXTREE | tlckFREE;
	} else {		

		
		xadlock.xdlist = &p->xad[XTENTRYSTART];
		xadlock.count =
		    le16_to_cpu(p->header.nextindex) - XTENTRYSTART;
		txFreeMap(ip, (struct maplock *) & xadlock, NULL, COMMIT_WMAP);
	}

	if (p->header.flag & BT_ROOT) {
		p->header.flag &= ~BT_INTERNAL;
		p->header.flag |= BT_LEAF;
		p->header.nextindex = cpu_to_le16(XTENTRYSTART);

		XT_PUTPAGE(mp);	
		goto out;
	} else {
		if (log) {	
			XT_PUTPAGE(mp);
		} else {	

			if (mp->lid)
				lid_to_tlock(mp->lid)->flag |= tlckFREELOCK;

			
			discard_metapage(mp);
		}
	}


      getParent:
	
	if ((parent = BT_POP(&btstack)) == NULL)
		
		goto out;

	
	bn = parent->bn;
	XT_GETPAGE(ip, bn, mp, PSIZE, p, rc);
	if (rc)
		return rc;

	index = parent->index;

	if (freed == 0) {
		
		if (index < le16_to_cpu(p->header.nextindex) - 1) {
			
			if (log) {	
				tlck = txLock(tid, ip, mp, tlckXTREE);
				xtlck = (struct xtlock *) & tlck->lock;
				if (!(tlck->type & tlckTRUNCATE)) {
					xtlck->hwm.offset =
					    le16_to_cpu(p->header.
							nextindex) - 1;
					tlck->type =
					    tlckXTREE | tlckTRUNCATE;
				}
			} else {	

				
				xadlock.xdlist = &p->xad[index + 1];
				xadlock.count =
				    le16_to_cpu(p->header.nextindex) -
				    index - 1;
				txFreeMap(ip, (struct maplock *) & xadlock,
					  NULL, COMMIT_WMAP);
			}
			BT_MARK_DIRTY(mp, ip);

			p->header.nextindex = cpu_to_le16(index + 1);
		}
		XT_PUTPAGE(mp);
		goto getParent;
	}

	nfreed += lengthXAD(&p->xad[index]);


	if (log && mp->lid && (tblk->last != mp->lid) &&
	    lid_to_tlock(mp->lid)->tid) {
		lid_t lid = mp->lid;
		struct tlock *prev;

		tlck = lid_to_tlock(lid);

		if (tblk->next == lid)
			tblk->next = tlck->next;
		else {
			for (prev = lid_to_tlock(tblk->next);
			     prev->next != lid;
			     prev = lid_to_tlock(prev->next)) {
				assert(prev->next);
			}
			prev->next = tlck->next;
		}
		lid_to_tlock(tblk->last)->next = lid;
		tlck->next = 0;
		tblk->last = lid;
	}

	if (index == XTENTRYSTART) {
		if (log) {	
			tlck = txLock(tid, ip, mp, tlckXTREE);
			xtlck = (struct xtlock *) & tlck->lock;
			xtlck->hwm.offset =
			    le16_to_cpu(p->header.nextindex) - 1;
			tlck->type = tlckXTREE | tlckFREE;
		} else {	

			
			xadlock.xdlist = &p->xad[XTENTRYSTART];
			xadlock.count =
			    le16_to_cpu(p->header.nextindex) -
			    XTENTRYSTART;
			txFreeMap(ip, (struct maplock *) & xadlock, NULL,
				  COMMIT_WMAP);
		}
		BT_MARK_DIRTY(mp, ip);

		if (p->header.flag & BT_ROOT) {
			p->header.flag &= ~BT_INTERNAL;
			p->header.flag |= BT_LEAF;
			p->header.nextindex = cpu_to_le16(XTENTRYSTART);
			if (le16_to_cpu(p->header.maxentry) == XTROOTMAXSLOT) {
				p->header.maxentry =
				    cpu_to_le16(XTROOTINITSLOT);
				JFS_IP(ip)->mode2 |= INLINEEA;
			}

			XT_PUTPAGE(mp);	
			goto out;
		} else {
			if (log) {	
				XT_PUTPAGE(mp);
			} else {	

				if (mp->lid)
					lid_to_tlock(mp->lid)->flag |=
						tlckFREELOCK;

				
				discard_metapage(mp);
			}

			
			goto getParent;
		}
	}
	else {
		index--;

		goto getChild;
	}

      getChild:
	
	if (BT_STACK_FULL(&btstack)) {
		jfs_error(ip->i_sb, "stack overrun in xtTruncate!");
		XT_PUTPAGE(mp);
		return -EIO;
	}
	BT_PUSH(&btstack, bn, index);

	
	xad = &p->xad[index];
	bn = addressXAD(xad);

	
	XT_PUTPAGE(mp);

	
	goto getPage;

      out:
	if (S_ISDIR(ip->i_mode) && !newsize)
		ip->i_size = 1;	
	else
		ip->i_size = newsize;

	
	dquot_free_block(ip, nfreed);

	if (flag == COMMIT_WMAP)
		txFreelock(ip);

	return newsize;
}


s64 xtTruncate_pmap(tid_t tid, struct inode *ip, s64 committed_size)
{
	s64 bn;
	struct btstack btstack;
	int cmp;
	int index;
	int locked_leaves = 0;
	struct metapage *mp;
	xtpage_t *p;
	struct btframe *parent;
	int rc;
	struct tblock *tblk;
	struct tlock *tlck = NULL;
	xad_t *xad;
	int xlen;
	s64 xoff;
	struct xtlock *xtlck = NULL;

	
	tblk = tid_to_tblock(tid);
	tblk->xflag |= COMMIT_PMAP;

	
	BT_CLR(&btstack);

	if (committed_size) {
		xoff = (committed_size >> JFS_SBI(ip->i_sb)->l2bsize) - 1;
		rc = xtSearch(ip, xoff, NULL, &cmp, &btstack, 0);
		if (rc)
			return rc;

		XT_GETSEARCH(ip, btstack.top, bn, mp, p, index);

		if (cmp != 0) {
			XT_PUTPAGE(mp);
			jfs_error(ip->i_sb,
				  "xtTruncate_pmap: did not find extent");
			return -EIO;
		}
	} else {
		bn = 0;

      getPage:
		XT_GETPAGE(ip, bn, mp, PSIZE, p, rc);
		if (rc)
			return rc;

		
		index = le16_to_cpu(p->header.nextindex) - 1;

		if (p->header.flag & BT_INTERNAL)
			goto getChild;
	}


	if (++locked_leaves > MAX_TRUNCATE_LEAVES) {
		xad = &p->xad[index];
		xoff = offsetXAD(xad);
		xlen = lengthXAD(xad);
		XT_PUTPAGE(mp);
		return (xoff + xlen) << JFS_SBI(ip->i_sb)->l2bsize;
	}
	tlck = txLock(tid, ip, mp, tlckXTREE);
	tlck->type = tlckXTREE | tlckFREE;
	xtlck = (struct xtlock *) & tlck->lock;
	xtlck->hwm.offset = index;


	XT_PUTPAGE(mp);

      getParent:
	
	if ((parent = BT_POP(&btstack)) == NULL)
		
		goto out;

	
	bn = parent->bn;
	XT_GETPAGE(ip, bn, mp, PSIZE, p, rc);
	if (rc)
		return rc;

	index = parent->index;

	if (index == XTENTRYSTART) {
		tlck = txLock(tid, ip, mp, tlckXTREE);
		xtlck = (struct xtlock *) & tlck->lock;
		xtlck->hwm.offset = le16_to_cpu(p->header.nextindex) - 1;
		tlck->type = tlckXTREE | tlckFREE;

		XT_PUTPAGE(mp);

		if (p->header.flag & BT_ROOT) {

			goto out;
		} else {
			goto getParent;
		}
	}
	else
		index--;
      getChild:
	
	if (BT_STACK_FULL(&btstack)) {
		jfs_error(ip->i_sb, "stack overrun in xtTruncate_pmap!");
		XT_PUTPAGE(mp);
		return -EIO;
	}
	BT_PUSH(&btstack, bn, index);

	
	xad = &p->xad[index];
	bn = addressXAD(xad);

	
	XT_PUTPAGE(mp);

	
	goto getPage;

      out:

	return 0;
}

#ifdef CONFIG_JFS_STATISTICS
static int jfs_xtstat_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m,
		       "JFS Xtree statistics\n"
		       "====================\n"
		       "searches = %d\n"
		       "fast searches = %d\n"
		       "splits = %d\n",
		       xtStat.search,
		       xtStat.fastSearch,
		       xtStat.split);
	return 0;
}

static int jfs_xtstat_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, jfs_xtstat_proc_show, NULL);
}

const struct file_operations jfs_xtstat_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= jfs_xtstat_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
#endif
