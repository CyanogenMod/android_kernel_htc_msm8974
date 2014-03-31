/*
 * volume.h - Defines for volume structures in NTFS Linux kernel driver. Part
 *	      of the Linux-NTFS project.
 *
 * Copyright (c) 2001-2006 Anton Altaparmakov
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

#ifndef _LINUX_NTFS_VOLUME_H
#define _LINUX_NTFS_VOLUME_H

#include <linux/rwsem.h>

#include "types.h"
#include "layout.h"

typedef struct {
	
	struct super_block *sb;		
	LCN nr_blocks;			
	
	unsigned long flags;		
	uid_t uid;			
	gid_t gid;			
	umode_t fmask;			
	umode_t dmask;			
	u8 mft_zone_multiplier;		
	u8 on_errors;			
	
	u16 sector_size;		
	u8 sector_size_bits;		
	u32 cluster_size;		
	u32 cluster_size_mask;		
	u8 cluster_size_bits;		
	u32 mft_record_size;		
	u32 mft_record_size_mask;	
	u8 mft_record_size_bits;	
	u32 index_record_size;		
	u32 index_record_size_mask;	
	u8 index_record_size_bits;	
	LCN nr_clusters;		
	LCN mft_lcn;			
	LCN mftmirr_lcn;		
	u64 serial_no;			
	
	u32 upcase_len;			
	ntfschar *upcase;		

	s32 attrdef_size;		
	ATTR_DEF *attrdef;		

#ifdef NTFS_RW
	
	s64 mft_data_pos;		
	LCN mft_zone_start;		
	LCN mft_zone_end;		
	LCN mft_zone_pos;		
	LCN data1_zone_pos;		
	LCN data2_zone_pos;		
#endif 

	struct inode *mft_ino;		

	struct inode *mftbmp_ino;	
	struct rw_semaphore mftbmp_lock; 
#ifdef NTFS_RW
	struct inode *mftmirr_ino;	
	int mftmirr_size;		

	struct inode *logfile_ino;	
#endif 

	struct inode *lcnbmp_ino;	
	struct rw_semaphore lcnbmp_lock; 

	struct inode *vol_ino;		
	VOLUME_FLAGS vol_flags;		
	u8 major_ver;			
	u8 minor_ver;			

	struct inode *root_ino;		
	struct inode *secure_ino;	
	struct inode *extend_ino;	
#ifdef NTFS_RW
	
	struct inode *quota_ino;	
	struct inode *quota_q_ino;	
	
	struct inode *usnjrnl_ino;	
	struct inode *usnjrnl_max_ino;	
	struct inode *usnjrnl_j_ino;	
#endif 
	struct nls_table *nls_map;
} ntfs_volume;

typedef enum {
	NV_Errors,		
	NV_ShowSystemFiles,	
	NV_CaseSensitive,	
	NV_LogFileEmpty,	
	NV_QuotaOutOfDate,	
	NV_UsnJrnlStamped,	
	NV_SparseEnabled,	
} ntfs_volume_flags;

#define DEFINE_NVOL_BIT_OPS(flag)					\
static inline int NVol##flag(ntfs_volume *vol)		\
{							\
	return test_bit(NV_##flag, &(vol)->flags);	\
}							\
static inline void NVolSet##flag(ntfs_volume *vol)	\
{							\
	set_bit(NV_##flag, &(vol)->flags);		\
}							\
static inline void NVolClear##flag(ntfs_volume *vol)	\
{							\
	clear_bit(NV_##flag, &(vol)->flags);		\
}

DEFINE_NVOL_BIT_OPS(Errors)
DEFINE_NVOL_BIT_OPS(ShowSystemFiles)
DEFINE_NVOL_BIT_OPS(CaseSensitive)
DEFINE_NVOL_BIT_OPS(LogFileEmpty)
DEFINE_NVOL_BIT_OPS(QuotaOutOfDate)
DEFINE_NVOL_BIT_OPS(UsnJrnlStamped)
DEFINE_NVOL_BIT_OPS(SparseEnabled)

#endif 
