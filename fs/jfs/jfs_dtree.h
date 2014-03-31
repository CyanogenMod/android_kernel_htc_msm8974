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
#ifndef _H_JFS_DTREE
#define	_H_JFS_DTREE


#include "jfs_btree.h"

typedef union {
	struct {
		tid_t tid;
		struct inode *ip;
		u32 ino;
	} leaf;
	pxd_t xd;
} ddata_t;


struct dtslot {
	s8 next;		
	s8 cnt;			
	__le16 name[15];	
};				


#define DATASLOTSIZE	16
#define L2DATASLOTSIZE	4
#define	DTSLOTSIZE	32
#define	L2DTSLOTSIZE	5
#define DTSLOTHDRSIZE	2
#define DTSLOTDATASIZE	30
#define DTSLOTDATALEN	15

struct idtentry {
	pxd_t xd;		

	s8 next;		
	u8 namlen;		
	__le16 name[11];	
};				

#define DTIHDRSIZE	10
#define DTIHDRDATALEN	11

#define	NDTINTERNAL(klen) (DIV_ROUND_UP((4 + (klen)), 15))


struct ldtentry {
	__le32 inumber;		
	s8 next;		
	u8 namlen;		
	__le16 name[11];	
	__le32 index;		
};				

#define DTLHDRSIZE	6
#define DTLHDRDATALEN_LEGACY	13	
#define DTLHDRDATALEN	11


#define DO_INDEX(INODE) (JFS_SBI((INODE)->i_sb)->mntflag & JFS_DIR_INDEX)

#define MAX_INLINE_DIRTABLE_ENTRY 13

struct dir_table_slot {
	u8 rsrvd;		
	u8 flag;		
	u8 slot;		
	u8 addr1;		
	__le32 addr2;		
};				

#define DIR_INDEX_VALID 1
#define DIR_INDEX_FREE 0

#define DTSaddress(dir_table_slot, address64)\
{\
	(dir_table_slot)->addr1 = ((u64)address64) >> 32;\
	(dir_table_slot)->addr2 = __cpu_to_le32((address64) & 0xffffffff);\
}

#define addressDTS(dts)\
	( ((s64)((dts)->addr1)) << 32 | __le32_to_cpu((dts)->addr2) )

#define	NDTLEAF_LEGACY(klen)	(DIV_ROUND_UP((2 + (klen)), 15))
#define	NDTLEAF	NDTINTERNAL


typedef union {
	struct {
		struct dasd DASD; 

		u8 flag;	
		u8 nextindex;	
		s8 freecnt;	
		s8 freelist;	

		__le32 idotdot;	

		s8 stbl[8];	
	} header;		

	struct dtslot slot[9];
} dtroot_t;

#define PARENT(IP) \
	(le32_to_cpu(JFS_IP(IP)->i_dtroot.header.idotdot))

#define DTROOTMAXSLOT	9

#define	dtEmpty(IP) (JFS_IP(IP)->i_dtroot.header.nextindex == 0)


typedef union {
	struct {
		__le64 next;	
		__le64 prev;	

		u8 flag;	
		u8 nextindex;	
		s8 freecnt;	
		s8 freelist;	

		u8 maxslot;	
		u8 stblindex;	
		u8 rsrvd[2];	

		pxd_t self;	
	} header;		

	struct dtslot slot[128];
} dtpage_t;

#define DTPAGEMAXSLOT        128

#define DT8THPGNODEBYTES     512
#define DT8THPGNODETSLOTS      1
#define DT8THPGNODESLOTS      16

#define DTQTRPGNODEBYTES    1024
#define DTQTRPGNODETSLOTS      1
#define DTQTRPGNODESLOTS      32

#define DTHALFPGNODEBYTES   2048
#define DTHALFPGNODETSLOTS     2
#define DTHALFPGNODESLOTS     64

#define DTFULLPGNODEBYTES   4096
#define DTFULLPGNODETSLOTS     4
#define DTFULLPGNODESLOTS    128

#define DTENTRYSTART	1

#define DT_GETSTBL(p) ( ((p)->header.flag & BT_ROOT) ?\
	((dtroot_t *)(p))->header.stbl : \
	(s8 *)&(p)->slot[(p)->header.stblindex] )

#define JFS_CREATE 1
#define JFS_LOOKUP 2
#define JFS_REMOVE 3
#define JFS_RENAME 4

#define DIREND	INT_MAX

extern void dtInitRoot(tid_t tid, struct inode *ip, u32 idotdot);

extern int dtSearch(struct inode *ip, struct component_name * key,
		    ino_t * data, struct btstack * btstack, int flag);

extern int dtInsert(tid_t tid, struct inode *ip, struct component_name * key,
		    ino_t * ino, struct btstack * btstack);

extern int dtDelete(tid_t tid, struct inode *ip, struct component_name * key,
		    ino_t * data, int flag);

extern int dtModify(tid_t tid, struct inode *ip, struct component_name * key,
		    ino_t * orig_ino, ino_t new_ino, int flag);

extern int jfs_readdir(struct file *filp, void *dirent, filldir_t filldir);
#endif				
