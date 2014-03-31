/*
 * lcnalloc.h - Exports for NTFS kernel cluster (de)allocation.  Part of the
 *		Linux-NTFS project.
 *
 * Copyright (c) 2004-2005 Anton Altaparmakov
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

#ifndef _LINUX_NTFS_LCNALLOC_H
#define _LINUX_NTFS_LCNALLOC_H

#ifdef NTFS_RW

#include <linux/fs.h>

#include "attrib.h"
#include "types.h"
#include "inode.h"
#include "runlist.h"
#include "volume.h"

typedef enum {
	FIRST_ZONE	= 0,	
	MFT_ZONE	= 0,	
	DATA_ZONE	= 1,	
	LAST_ZONE	= 1,	
} NTFS_CLUSTER_ALLOCATION_ZONES;

extern runlist_element *ntfs_cluster_alloc(ntfs_volume *vol,
		const VCN start_vcn, const s64 count, const LCN start_lcn,
		const NTFS_CLUSTER_ALLOCATION_ZONES zone,
		const bool is_extension);

extern s64 __ntfs_cluster_free(ntfs_inode *ni, const VCN start_vcn,
		s64 count, ntfs_attr_search_ctx *ctx, const bool is_rollback);

static inline s64 ntfs_cluster_free(ntfs_inode *ni, const VCN start_vcn,
		s64 count, ntfs_attr_search_ctx *ctx)
{
	return __ntfs_cluster_free(ni, start_vcn, count, ctx, false);
}

extern int ntfs_cluster_free_from_rl_nolock(ntfs_volume *vol,
		const runlist_element *rl);

static inline int ntfs_cluster_free_from_rl(ntfs_volume *vol,
		const runlist_element *rl)
{
	int ret;

	down_write(&vol->lcnbmp_lock);
	ret = ntfs_cluster_free_from_rl_nolock(vol, rl);
	up_write(&vol->lcnbmp_lock);
	return ret;
}

#endif 

#endif 
