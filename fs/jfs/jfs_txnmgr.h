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
#ifndef _H_JFS_TXNMGR
#define _H_JFS_TXNMGR

#include "jfs_logmgr.h"

#define tid_to_tblock(tid) (&TxBlock[tid])

#define lid_to_tlock(lid) (&TxLock[lid])

struct tblock {
	u16 xflag;		
	u16 flag;		
	lid_t dummy;		
	s32 lsn;		
	struct list_head synclist;	

	
	struct super_block *sb;	
	lid_t next;		
	lid_t last;		
	wait_queue_head_t waitor;	

	
	u32 logtid;		

	
	struct list_head cqueue;	
	s32 clsn;		
	struct lbuf *bp;
	s32 pn;			
	s32 eor;		
	wait_queue_head_t gcwait;	
	union {
		struct inode *ip; 
		pxd_t ixpxd;	
	} u;
	u32 ino;		
};

extern struct tblock *TxBlock;	

#define	COMMIT_SYNC	0x0001	
#define	COMMIT_FORCE	0x0002	
#define	COMMIT_FLUSH	0x0004	
#define COMMIT_MAP	0x00f0
#define	COMMIT_PMAP	0x0010	
#define	COMMIT_WMAP	0x0020	
#define	COMMIT_PWMAP	0x0040	
#define	COMMIT_FREE	0x0f00
#define	COMMIT_DELETE	0x0100	
#define	COMMIT_TRUNCATE	0x0200	
#define	COMMIT_CREATE	0x0400	
#define	COMMIT_LAZY	0x0800	
#define COMMIT_PAGE	0x1000	
#define COMMIT_INODE	0x2000	


struct tlock {
	lid_t next;		
	tid_t tid;		

	u16 flag;		
	u16 type;		

	struct metapage *mp;	
	struct inode *ip;	
	

	s16 lock[24];		
};				

extern struct tlock *TxLock;	

#define tlckPAGELOCK		0x8000
#define tlckINODELOCK		0x4000
#define tlckLINELOCK		0x2000
#define tlckINLINELOCK		0x1000
#define tlckLOG			0x0800
#define	tlckUPDATEMAP		0x0080
#define	tlckDIRECTORY		0x0040
#define tlckFREELOCK		0x0008
#define tlckWRITEPAGE		0x0004
#define tlckFREEPAGE		0x0002

#define	tlckTYPE		0xfe00
#define	tlckINODE		0x8000
#define	tlckXTREE		0x4000
#define	tlckDTREE		0x2000
#define	tlckMAP			0x1000
#define	tlckEA			0x0800
#define	tlckACL			0x0400
#define	tlckDATA		0x0200
#define	tlckBTROOT		0x0100

#define	tlckOPERATION		0x00ff
#define tlckGROW		0x0001	
#define tlckREMOVE		0x0002	
#define tlckTRUNCATE		0x0004	
#define tlckRELOCATE		0x0008	
#define tlckENTRY		0x0001	
#define tlckEXTEND		0x0002	
#define tlckSPLIT		0x0010	
#define tlckNEW			0x0020	
#define tlckFREE		0x0040	
#define tlckRELINK		0x0080	

struct lv {
	u8 offset;		
	u8 length;		
};				

#define	TLOCKSHORT	20
#define	TLOCKLONG	28

struct linelock {
	lid_t next;		

	s8 maxcnt;		
	s8 index;		

	u16 flag;		
	u8 type;		
	u8 l2linesize;		
	

	struct lv lv[20];	
};				

#define dt_lock	linelock

struct xtlock {
	lid_t next;		

	s8 maxcnt;		
	s8 index;		

	u16 flag;		
	u8 type;		
	u8 l2linesize;		
				

	struct lv header;	
	struct lv lwm;		
	struct lv hwm;		
	struct lv twm;		
				

	s32 pxdlock[8];		
};				


struct maplock {
	lid_t next;		

	u8 maxcnt;		
	u8 index;		

	u16 flag;		
	u8 type;		
	u8 count;		
				

	pxd_t pxd;		
};				

#define	mlckALLOC		0x00f0
#define	mlckALLOCXADLIST	0x0080
#define	mlckALLOCPXDLIST	0x0040
#define	mlckALLOCXAD		0x0020
#define	mlckALLOCPXD		0x0010
#define	mlckFREE		0x000f
#define	mlckFREEXADLIST		0x0008
#define	mlckFREEPXDLIST		0x0004
#define	mlckFREEXAD		0x0002
#define	mlckFREEPXD		0x0001

#define	pxd_lock	maplock

struct xdlistlock {
	lid_t next;		

	u8 maxcnt;		
	u8 index;		

	u16 flag;		
	u8 type;		
	u8 count;		
				

	union {
		void *_xdlist;	
		s64 pad;	
	} union64;
};				

#define xdlist union64._xdlist

struct commit {
	tid_t tid;		
	int flag;		
	struct jfs_log *log;	
	struct super_block *sb;	

	int nip;		
	struct inode **iplist;	

	
	struct lrd lrd;		
};

extern int jfs_tlocks_low;

extern int txInit(void);
extern void txExit(void);
extern struct tlock *txLock(tid_t, struct inode *, struct metapage *, int);
extern struct tlock *txMaplock(tid_t, struct inode *, int);
extern int txCommit(tid_t, int, struct inode **, int);
extern tid_t txBegin(struct super_block *, int);
extern void txBeginAnon(struct super_block *);
extern void txEnd(tid_t);
extern void txAbort(tid_t, int);
extern struct linelock *txLinelock(struct linelock *);
extern void txFreeMap(struct inode *, struct maplock *, struct tblock *, int);
extern void txEA(tid_t, struct inode *, dxd_t *, dxd_t *);
extern void txFreelock(struct inode *);
extern int lmLog(struct jfs_log *, struct tblock *, struct lrd *,
		 struct tlock *);
extern void txQuiesce(struct super_block *);
extern void txResume(struct super_block *);
extern void txLazyUnlock(struct tblock *);
extern int jfs_lazycommit(void *);
extern int jfs_sync(void *);
#endif				
