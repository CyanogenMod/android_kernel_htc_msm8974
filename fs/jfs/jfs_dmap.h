/*
 *   Copyright (C) International Business Machines Corp., 2000-2002
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
#ifndef	_H_JFS_DMAP
#define _H_JFS_DMAP

#include "jfs_txnmgr.h"

#define BMAPVERSION	1	
#define	TREESIZE	(256+64+16+4+1)	
#define	LEAFIND		(64+16+4+1)	
#define LPERDMAP	256	
#define L2LPERDMAP	8	
#define	DBWORD		32	
#define	L2DBWORD	5	
#define BUDMIN		L2DBWORD	
#define BPERDMAP	(LPERDMAP * DBWORD)	
#define L2BPERDMAP	13	
#define CTLTREESIZE	(1024+256+64+16+4+1)	
#define CTLLEAFIND	(256+64+16+4+1)	
#define LPERCTL		1024	
#define L2LPERCTL	10	
#define	ROOT		0	
#define	NOFREE		((s8) -1)	
#define	MAXAG		128	
#define L2MAXAG		7	
#define L2MINAGSZ	25	
#define	BMAPBLKNO	0	

#define	L2MAXL0SIZE	(L2BPERDMAP + 1 * L2LPERCTL)
#define	L2MAXL1SIZE	(L2BPERDMAP + 2 * L2LPERCTL)
#define	L2MAXL2SIZE	(L2BPERDMAP + 3 * L2LPERCTL)

#define	MAXL0SIZE	((s64)1 << L2MAXL0SIZE)
#define	MAXL1SIZE	((s64)1 << L2MAXL1SIZE)
#define	MAXL2SIZE	((s64)1 << L2MAXL2SIZE)

#define	MAXMAPSIZE	MAXL2SIZE	

static inline signed char TREEMAX(signed char *cp)
{
	signed char tmp1, tmp2;

	tmp1 = max(*(cp+2), *(cp+3));
	tmp2 = max(*(cp), *(cp+1));

	return max(tmp1, tmp2);
}

#define BLKTODMAP(b,s)    \
	((((b) >> 13) + ((b) >> 23) + ((b) >> 33) + 3 + 1) << (s))

#define BLKTOL0(b,s)      \
	(((((b) >> 23) << 10) + ((b) >> 23) + ((b) >> 33) + 2 + 1) << (s))

#define BLKTOL1(b,s)      \
     (((((b) >> 33) << 20) + (((b) >> 33) << 10) + ((b) >> 33) + 1 + 1) << (s))

#define BLKTOCTL(b,s,l)   \
	(((l) == 2) ? 1 : ((l) == 1) ? BLKTOL1((b),(s)) : BLKTOL0((b),(s)))

#define	BMAPSZTOLEV(size)	\
	(((size) <= MAXL0SIZE) ? 0 : ((size) <= MAXL1SIZE) ? 1 : 2)

#define BLKTOAG(b,sbi)	((b) >> ((sbi)->bmap->db_agl2size))

#define AGTOBLK(a,ip)	\
	((s64)(a) << (JFS_SBI((ip)->i_sb)->bmap->db_agl2size))

struct dmaptree {
	__le32 nleafs;		
	__le32 l2nleafs;	
	__le32 leafidx;		
	__le32 height;		
	s8 budmin;		
	s8 stree[TREESIZE];	
	u8 pad[2];		
};				

struct dmap {
	__le32 nblocks;		
	__le32 nfree;		
	__le64 start;		
	struct dmaptree tree;	
	u8 pad[1672];		
	__le32 wmap[LPERDMAP];	
	__le32 pmap[LPERDMAP];	
};				

struct dmapctl {
	__le32 nleafs;		
	__le32 l2nleafs;	
	__le32 leafidx;		
	__le32 height;		
	s8 budmin;		
	s8 stree[CTLTREESIZE];	
	u8 pad[2714];		
};				

typedef union dmtree {
	struct dmaptree t1;
	struct dmapctl t2;
} dmtree_t;

#define	dmt_nleafs	t1.nleafs
#define	dmt_l2nleafs	t1.l2nleafs
#define	dmt_leafidx	t1.leafidx
#define	dmt_height	t1.height
#define	dmt_budmin	t1.budmin
#define	dmt_stree	t1.stree

struct dbmap_disk {
	__le64 dn_mapsize;	
	__le64 dn_nfree;	
	__le32 dn_l2nbperpage;	
	__le32 dn_numag;	
	__le32 dn_maxlevel;	
	__le32 dn_maxag;	
	__le32 dn_agpref;	
	__le32 dn_aglevel;	
	__le32 dn_agheight;	
	__le32 dn_agwidth;	
	__le32 dn_agstart;	
	__le32 dn_agl2size;	
	__le64 dn_agfree[MAXAG];
	__le64 dn_agsize;	
	s8 dn_maxfreebud;	
	u8 pad[3007];		
};				

struct dbmap {
	s64 dn_mapsize;		
	s64 dn_nfree;		
	int dn_l2nbperpage;	
	int dn_numag;		
	int dn_maxlevel;	
	int dn_maxag;		
	int dn_agpref;		
	int dn_aglevel;		
	int dn_agheight;	
	int dn_agwidth;		
	int dn_agstart;		
	int dn_agl2size;	
	s64 dn_agfree[MAXAG];	
	s64 dn_agsize;		
	signed char dn_maxfreebud;	
};				
struct bmap {
	struct dbmap db_bmap;		
	struct inode *db_ipbmap;	
	struct mutex db_bmaplock;	
	atomic_t db_active[MAXAG];	
	u32 *db_DBmap;
};

#define	db_mapsize	db_bmap.dn_mapsize
#define	db_nfree	db_bmap.dn_nfree
#define	db_agfree	db_bmap.dn_agfree
#define	db_agsize	db_bmap.dn_agsize
#define	db_agl2size	db_bmap.dn_agl2size
#define	db_agwidth	db_bmap.dn_agwidth
#define	db_agheight	db_bmap.dn_agheight
#define	db_agstart	db_bmap.dn_agstart
#define	db_numag	db_bmap.dn_numag
#define	db_maxlevel	db_bmap.dn_maxlevel
#define	db_aglevel	db_bmap.dn_aglevel
#define	db_agpref	db_bmap.dn_agpref
#define	db_maxag	db_bmap.dn_maxag
#define	db_maxfreebud	db_bmap.dn_maxfreebud
#define	db_l2nbperpage	db_bmap.dn_l2nbperpage

#define	BLKSTOL2(d)		(blkstol2(d))

#define	NLSTOL2BSZ(n)		(31 - cntlz((n)) + BUDMIN)

#define	LITOL2BSZ(n,m,b)	((((n) == 0) ? (m) : cnttz((n))) + (b))

#define BLKTOCTLLEAF(b,m)	\
	(((b) & (((s64)1 << ((m) + L2LPERCTL)) - 1)) >> (m))

#define	BUDSIZE(s,m)		(1 << ((s) - (m)))

extern int dbMount(struct inode *ipbmap);

extern int dbUnmount(struct inode *ipbmap, int mounterror);

extern int dbFree(struct inode *ipbmap, s64 blkno, s64 nblocks);

extern int dbUpdatePMap(struct inode *ipbmap,
			int free, s64 blkno, s64 nblocks, struct tblock * tblk);

extern int dbNextAG(struct inode *ipbmap);

extern int dbAlloc(struct inode *ipbmap, s64 hint, s64 nblocks, s64 * results);

extern int dbReAlloc(struct inode *ipbmap,
		     s64 blkno, s64 nblocks, s64 addnblocks, s64 * results);

extern int dbSync(struct inode *ipbmap);
extern int dbAllocBottomUp(struct inode *ip, s64 blkno, s64 nblocks);
extern int dbExtendFS(struct inode *ipbmap, s64 blkno, s64 nblocks);
extern void dbFinalizeBmap(struct inode *ipbmap);
extern s64 dbMapFileSizeToMapSize(struct inode *ipbmap);
#endif				
