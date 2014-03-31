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
#ifndef	_H_JFS_IMAP
#define _H_JFS_IMAP

#include "jfs_txnmgr.h"


#define	EXTSPERIAG	128	
#define IMAPBLKNO	0	
#define SMAPSZ		4	
#define	EXTSPERSUM	32	
#define	L2EXTSPERSUM	5	
#define	PGSPERIEXT	4	
#define	MAXIAGS		((1<<20)-1)	
#define	MAXAG		128	

#define AMAPSIZE	512	
#define SMAPSIZE	16	

#define	INOTOIAG(ino)	((ino) >> L2INOSPERIAG)

#define IAGTOLBLK(iagno,l2nbperpg)	(((iagno) + 1) << (l2nbperpg))

#define INOPBLK(pxd,ino,l2nbperpg)	(addressPXD((pxd)) +		\
	((((ino) & (INOSPEREXT-1)) >> L2INOSPERPAGE) << (l2nbperpg)))

struct iag {
	__le64 agstart;		
	__le32 iagnum;		
	__le32 inofreefwd;	
	__le32 inofreeback;	
	__le32 extfreefwd;	
	__le32 extfreeback;	
	__le32 iagfree;		

	
	__le32 inosmap[SMAPSZ];	
	__le32 extsmap[SMAPSZ];	
	__le32 nfreeinos;	
	__le32 nfreeexts;	
	
	u8 pad[1976];		
	
	__le32 wmap[EXTSPERIAG];	
	__le32 pmap[EXTSPERIAG];	
	pxd_t inoext[EXTSPERIAG];	
};				

struct iagctl_disk {
	__le32 inofree;		
	__le32 extfree;		
	__le32 numinos;		
	__le32 numfree;		
};				

struct iagctl {
	int inofree;		
	int extfree;		
	int numinos;		
	int numfree;		
};

struct dinomap_disk {
	__le32 in_freeiag;	
	__le32 in_nextiag;	
	__le32 in_numinos;	
	__le32 in_numfree;	
	__le32 in_nbperiext;	
	__le32 in_l2nbperiext;	
	__le32 in_diskblock;	
	__le32 in_maxag;	
	u8 pad[2016];		
	struct iagctl_disk in_agctl[MAXAG]; 
};				

struct dinomap {
	int in_freeiag;		
	int in_nextiag;		
	int in_numinos;		
	int in_numfree;		
	int in_nbperiext;	
	int in_l2nbperiext;	
	int in_diskblock;	
	int in_maxag;		
	struct iagctl in_agctl[MAXAG];	
};

struct inomap {
	struct dinomap im_imap;		
	struct inode *im_ipimap;	
	struct mutex im_freelock;	
	struct mutex im_aglock[MAXAG];	
	u32 *im_DBGdimap;
	atomic_t im_numinos;	
	atomic_t im_numfree;	
};

#define	im_freeiag	im_imap.in_freeiag
#define	im_nextiag	im_imap.in_nextiag
#define	im_agctl	im_imap.in_agctl
#define	im_nbperiext	im_imap.in_nbperiext
#define	im_l2nbperiext	im_imap.in_l2nbperiext

#define	im_diskblock	im_imap.in_diskblock
#define	im_maxag	im_imap.in_maxag

extern int diFree(struct inode *);
extern int diAlloc(struct inode *, bool, struct inode *);
extern int diSync(struct inode *);
extern int diUpdatePMap(struct inode *ipimap, unsigned long inum,
			bool is_free, struct tblock * tblk);
extern int diExtendFS(struct inode *ipimap, struct inode *ipbmap);
extern int diMount(struct inode *);
extern int diUnmount(struct inode *, int);
extern int diRead(struct inode *);
extern struct inode *diReadSpecial(struct super_block *, ino_t, int);
extern void diWriteSpecial(struct inode *, int);
extern void diFreeSpecial(struct inode *);
extern int diWrite(tid_t tid, struct inode *);
#endif				
