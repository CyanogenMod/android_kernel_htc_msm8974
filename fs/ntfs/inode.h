/*
 * inode.h - Defines for inode structures NTFS Linux kernel driver. Part of
 *	     the Linux-NTFS project.
 *
 * Copyright (c) 2001-2007 Anton Altaparmakov
 * Copyright (c) 2002 Richard Russon
 *
 * This program/include file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program/include file is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the Linux-NTFS
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _LINUX_NTFS_INODE_H
#define _LINUX_NTFS_INODE_H

#include <linux/atomic.h>

#include <linux/fs.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/seq_file.h>

#include "layout.h"
#include "volume.h"
#include "types.h"
#include "runlist.h"
#include "debug.h"

typedef struct _ntfs_inode ntfs_inode;

struct _ntfs_inode {
	rwlock_t size_lock;	
	s64 initialized_size;	
	s64 allocated_size;	
	unsigned long state;	
	unsigned long mft_no;	
	u16 seq_no;		
	atomic_t count;		
	ntfs_volume *vol;	
	ATTR_TYPE type;	
	ntfschar *name;		
	u32 name_len;		
	runlist runlist;	
	struct mutex mrec_lock;	
	struct page *page;	
	int page_ofs;		
	u32 attr_list_size;	
	u8 *attr_list;		
	runlist attr_list_rl;	
	union {
		struct { 
			u32 block_size;		
			u32 vcn_size;		
			COLLATION_RULE collation_rule; 
			u8 block_size_bits; 	
			u8 vcn_size_bits;	
		} index;
		struct { 
			s64 size;		
			u32 block_size;		
			u8 block_size_bits;	
			u8 block_clusters;	
		} compressed;
	} itype;
	struct mutex extent_lock;	
	s32 nr_extents;	
	union {		
		ntfs_inode **extent_ntfs_inos;	
		ntfs_inode *base_ntfs_ino;	
	} ext;
};

typedef enum {
	NI_Dirty,		/* 1: Mft record needs to be written to disk. */
	NI_AttrList,		
	NI_AttrListNonResident,	

	NI_Attr,		

	NI_MstProtected,	
	NI_NonResident,		
	NI_IndexAllocPresent = NI_NonResident,	
	NI_Compressed,		
	NI_Encrypted,		
	NI_Sparse,		
	NI_SparseDisabled,	
	NI_TruncateFailed,	
} ntfs_inode_state_bits;


#define NINO_FNS(flag)					\
static inline int NIno##flag(ntfs_inode *ni)		\
{							\
	return test_bit(NI_##flag, &(ni)->state);	\
}							\
static inline void NInoSet##flag(ntfs_inode *ni)	\
{							\
	set_bit(NI_##flag, &(ni)->state);		\
}							\
static inline void NInoClear##flag(ntfs_inode *ni)	\
{							\
	clear_bit(NI_##flag, &(ni)->state);		\
}

#define TAS_NINO_FNS(flag)					\
static inline int NInoTestSet##flag(ntfs_inode *ni)		\
{								\
	return test_and_set_bit(NI_##flag, &(ni)->state);	\
}								\
static inline int NInoTestClear##flag(ntfs_inode *ni)		\
{								\
	return test_and_clear_bit(NI_##flag, &(ni)->state);	\
}

NINO_FNS(Dirty)
TAS_NINO_FNS(Dirty)
NINO_FNS(AttrList)
NINO_FNS(AttrListNonResident)
NINO_FNS(Attr)
NINO_FNS(MstProtected)
NINO_FNS(NonResident)
NINO_FNS(IndexAllocPresent)
NINO_FNS(Compressed)
NINO_FNS(Encrypted)
NINO_FNS(Sparse)
NINO_FNS(SparseDisabled)
NINO_FNS(TruncateFailed)

typedef struct {
	ntfs_inode ntfs_inode;
	struct inode vfs_inode;		
} big_ntfs_inode;

static inline ntfs_inode *NTFS_I(struct inode *inode)
{
	return (ntfs_inode *)list_entry(inode, big_ntfs_inode, vfs_inode);
}

static inline struct inode *VFS_I(ntfs_inode *ni)
{
	return &((big_ntfs_inode *)ni)->vfs_inode;
}

typedef struct {
	unsigned long mft_no;
	ntfschar *name;
	u32 name_len;
	ATTR_TYPE type;
} ntfs_attr;

typedef int (*test_t)(struct inode *, void *);

extern int ntfs_test_inode(struct inode *vi, ntfs_attr *na);

extern struct inode *ntfs_iget(struct super_block *sb, unsigned long mft_no);
extern struct inode *ntfs_attr_iget(struct inode *base_vi, ATTR_TYPE type,
		ntfschar *name, u32 name_len);
extern struct inode *ntfs_index_iget(struct inode *base_vi, ntfschar *name,
		u32 name_len);

extern struct inode *ntfs_alloc_big_inode(struct super_block *sb);
extern void ntfs_destroy_big_inode(struct inode *inode);
extern void ntfs_evict_big_inode(struct inode *vi);

extern void __ntfs_init_inode(struct super_block *sb, ntfs_inode *ni);

static inline void ntfs_init_big_inode(struct inode *vi)
{
	ntfs_inode *ni = NTFS_I(vi);

	ntfs_debug("Entering.");
	__ntfs_init_inode(vi->i_sb, ni);
	ni->mft_no = vi->i_ino;
}

extern ntfs_inode *ntfs_new_extent_inode(struct super_block *sb,
		unsigned long mft_no);
extern void ntfs_clear_extent_inode(ntfs_inode *ni);

extern int ntfs_read_inode_mount(struct inode *vi);

extern int ntfs_show_options(struct seq_file *sf, struct dentry *root);

#ifdef NTFS_RW

extern int ntfs_truncate(struct inode *vi);
extern void ntfs_truncate_vfs(struct inode *vi);

extern int ntfs_setattr(struct dentry *dentry, struct iattr *attr);

extern int __ntfs_write_inode(struct inode *vi, int sync);

static inline void ntfs_commit_inode(struct inode *vi)
{
	if (!is_bad_inode(vi))
		__ntfs_write_inode(vi, 1);
	return;
}

#endif 

#endif 
