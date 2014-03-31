/*
 *   Copyright (C) International Business Machines Corp., 2000-2003
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
#ifndef _H_JFS_FILSYS
#define _H_JFS_FILSYS



#define JFS_UNICODE	0x00000001	

#define JFS_ERR_REMOUNT_RO 0x00000002	
#define JFS_ERR_CONTINUE   0x00000004	
#define JFS_ERR_PANIC      0x00000008	

#define	JFS_USRQUOTA	0x00000010
#define	JFS_GRPQUOTA	0x00000020

#define JFS_NOINTEGRITY 0x00000040

#define	JFS_COMMIT	0x00000f00	
#define	JFS_GROUPCOMMIT	0x00000100	
#define	JFS_LAZYCOMMIT	0x00000200	
#define	JFS_TMPFS	0x00000400	

#define	JFS_INLINELOG	0x00000800	
#define JFS_INLINEMOVE	0x00001000	

#define JFS_BAD_SAIT	0x00010000	

#define JFS_SPARSE	0x00020000	

#define JFS_DASD_ENABLED 0x00040000	
#define	JFS_DASD_PRIME	0x00080000	

#define	JFS_SWAP_BYTES	0x00100000	

#define JFS_DIR_INDEX	0x00200000	

#define JFS_LINUX	0x10000000	
#define JFS_DFS		0x20000000	

#define JFS_OS2		0x40000000	

#define JFS_AIX		0x80000000	

#ifdef PSIZE
#undef PSIZE
#endif
#define	PSIZE		4096	
#define	L2PSIZE		12	
#define	POFFSET		4095	

#define BPSIZE	PSIZE

#define	PBSIZE		512	
#define	L2PBSIZE	9	

#define DISIZE		512	
#define L2DISIZE	9	

#define IDATASIZE	256	
#define	IXATTRSIZE	128	

#define XTPAGE_SIZE	4096
#define log2_PAGESIZE	12

#define IAG_SIZE	4096
#define IAG_EXTENT_SIZE 4096
#define	INOSPERIAG	4096	
#define	L2INOSPERIAG	12	
#define INOSPEREXT	32	
#define L2INOSPEREXT	5	
#define	IXSIZE		(DISIZE * INOSPEREXT)	
#define	INOSPERPAGE	8	
#define	L2INOSPERPAGE	3	

#define	IAGFREELIST_LWM	64

#define INODE_EXTENT_SIZE	IXSIZE	
#define NUM_INODE_PER_EXTENT	INOSPEREXT
#define NUM_INODE_PER_IAG	INOSPERIAG

#define MINBLOCKSIZE		512
#define MAXBLOCKSIZE		4096
#define	MAXFILESIZE		((s64)1 << 52)

#define JFS_LINK_MAX		0xffffffff

#define MINJFS			(0x1000000)
#define MINJFSTEXT		"16"

#define LBOFFSET(x)	((x) & (PBSIZE - 1))
#define LBNUMBER(x)	((x) >> L2PBSIZE)
#define	LBLK2PBLK(sb,b)	((b) << (sb->s_blocksize_bits - L2PBSIZE))
#define	PBLK2LBLK(sb,b)	((b) >> (sb->s_blocksize_bits - L2PBSIZE))
#define	SIZE2PN(size)	( ((s64)((size) - 1)) >> (L2PSIZE) )
#define	SIZE2BN(size, l2bsize) ( ((s64)((size) - 1)) >> (l2bsize) )

#define SUPER1_B	64	
#define	AIMAP_B		(SUPER1_B + 8)	
#define	AITBL_B		(AIMAP_B + 16)	
#define	SUPER2_B	(AITBL_B + 32)	
#define	BMAP_B		(SUPER2_B + 8)	

#define SIZE_OF_SUPER	PSIZE

#define SIZE_OF_AG_TABLE	PSIZE

#define SIZE_OF_MAP_PAGE	PSIZE

#define SUPER1_OFF	0x8000	
#define AIMAP_OFF	(SUPER1_OFF + SIZE_OF_SUPER)
#define AITBL_OFF	(AIMAP_OFF + (SIZE_OF_MAP_PAGE << 1))
#define SUPER2_OFF	(AITBL_OFF + INODE_EXTENT_SIZE)
#define BMAP_OFF	(SUPER2_OFF + SIZE_OF_SUPER)

#define AGGR_RSVD_BLOCKS	SUPER1_B

#define AGGR_RSVD_BYTES	SUPER1_OFF

#define AGGR_INODE_TABLE_START	AITBL_OFF

#define AGGR_RESERVED_I	0	
#define	AGGREGATE_I	1	
#define	BMAP_I		2	
#define	LOG_I		3	
#define BADBLOCK_I	4	
#define	FILESYSTEM_I	16	

#define FILESET_RSVD_I	0	
#define FILESET_EXT_I	1	
#define	ROOT_I		2	
#define ACL_I		3	

#define FILESET_OBJECT_I 4	
#define FIRST_FILESET_INO 16	

#define JFS_NAME_MAX	255
#define JFS_PATH_MAX	BPSIZE


#define FM_CLEAN 0x00000000	
#define FM_MOUNT 0x00000001	
#define FM_DIRTY 0x00000002	
#define	FM_LOGREDO 0x00000004	
#define	FM_EXTENDFS 0x00000008	

#endif				
