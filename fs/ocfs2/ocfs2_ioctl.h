/* -*- mode: c; c-basic-offset: 8; -*-
 * vim: noexpandtab sw=8 ts=8 sts=0:
 *
 * ocfs2_ioctl.h
 *
 * Defines OCFS2 ioctls.
 *
 * Copyright (C) 2010 Oracle.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#ifndef OCFS2_IOCTL_H
#define OCFS2_IOCTL_H

#define OCFS2_IOC_GETFLAGS	FS_IOC_GETFLAGS
#define OCFS2_IOC_SETFLAGS	FS_IOC_SETFLAGS
#define OCFS2_IOC32_GETFLAGS	FS_IOC32_GETFLAGS
#define OCFS2_IOC32_SETFLAGS	FS_IOC32_SETFLAGS

struct ocfs2_space_resv {
	__s16		l_type;
	__s16		l_whence;
	__s64		l_start;
	__s64		l_len;		
	__s32		l_sysid;
	__u32		l_pid;
	__s32		l_pad[4];	
};

#define OCFS2_IOC_ALLOCSP		_IOW ('X', 10, struct ocfs2_space_resv)
#define OCFS2_IOC_FREESP		_IOW ('X', 11, struct ocfs2_space_resv)
#define OCFS2_IOC_RESVSP		_IOW ('X', 40, struct ocfs2_space_resv)
#define OCFS2_IOC_UNRESVSP	_IOW ('X', 41, struct ocfs2_space_resv)
#define OCFS2_IOC_ALLOCSP64	_IOW ('X', 36, struct ocfs2_space_resv)
#define OCFS2_IOC_FREESP64	_IOW ('X', 37, struct ocfs2_space_resv)
#define OCFS2_IOC_RESVSP64	_IOW ('X', 42, struct ocfs2_space_resv)
#define OCFS2_IOC_UNRESVSP64	_IOW ('X', 43, struct ocfs2_space_resv)

struct ocfs2_new_group_input {
	__u64 group;		
	__u32 clusters;		
	__u32 frees;		
	__u16 chain;		
	__u16 reserved1;
	__u32 reserved2;
};

#define OCFS2_IOC_GROUP_EXTEND	_IOW('o', 1, int)
#define OCFS2_IOC_GROUP_ADD	_IOW('o', 2,struct ocfs2_new_group_input)
#define OCFS2_IOC_GROUP_ADD64	_IOW('o', 3,struct ocfs2_new_group_input)

struct reflink_arguments {
	__u64 old_path;
	__u64 new_path;
	__u64 preserve;
};
#define OCFS2_IOC_REFLINK	_IOW('o', 4, struct reflink_arguments)

#define OCFS2_INFO_MAX_REQUEST		(50)
#define OCFS2_TEXT_UUID_LEN		(OCFS2_VOL_UUID_LEN * 2)

#define OCFS2_INFO_MAGIC		(0x4F32494E)

struct ocfs2_info {
	__u64 oi_requests;	
	__u32 oi_count;		
	__u32 oi_pad;
};

struct ocfs2_info_request {
	__u32 ir_magic;	
	__u32 ir_code;	
	__u32 ir_size;	
	__u32 ir_flags;	
			
};

struct ocfs2_info_clustersize {
	struct ocfs2_info_request ic_req;
	__u32 ic_clustersize;
	__u32 ic_pad;
};

struct ocfs2_info_blocksize {
	struct ocfs2_info_request ib_req;
	__u32 ib_blocksize;
	__u32 ib_pad;
};

struct ocfs2_info_maxslots {
	struct ocfs2_info_request im_req;
	__u32 im_max_slots;
	__u32 im_pad;
};

struct ocfs2_info_label {
	struct ocfs2_info_request il_req;
	__u8	il_label[OCFS2_MAX_VOL_LABEL_LEN];
} __attribute__ ((packed));

struct ocfs2_info_uuid {
	struct ocfs2_info_request iu_req;
	__u8	iu_uuid_str[OCFS2_TEXT_UUID_LEN + 1];
} __attribute__ ((packed));

struct ocfs2_info_fs_features {
	struct ocfs2_info_request if_req;
	__u32 if_compat_features;
	__u32 if_incompat_features;
	__u32 if_ro_compat_features;
	__u32 if_pad;
};

struct ocfs2_info_journal_size {
	struct ocfs2_info_request ij_req;
	__u64 ij_journal_size;
};

struct ocfs2_info_freeinode {
	struct ocfs2_info_request ifi_req;
	struct ocfs2_info_local_freeinode {
		__u64 lfi_total;
		__u64 lfi_free;
	} ifi_stat[OCFS2_MAX_SLOTS];
	__u32 ifi_slotnum; 
	__u32 ifi_pad;
};

#define OCFS2_INFO_MAX_HIST     (32)

struct ocfs2_info_freefrag {
	struct ocfs2_info_request iff_req;
	struct ocfs2_info_freefrag_stats { 
		struct ocfs2_info_free_chunk_list {
			__u32 fc_chunks[OCFS2_INFO_MAX_HIST];
			__u32 fc_clusters[OCFS2_INFO_MAX_HIST];
		} ffs_fc_hist;
		__u32 ffs_clusters;
		__u32 ffs_free_clusters;
		__u32 ffs_free_chunks;
		__u32 ffs_free_chunks_real;
		__u32 ffs_min; 
		__u32 ffs_max;
		__u32 ffs_avg;
		__u32 ffs_pad;
	} iff_ffs;
	__u32 iff_chunksize; 
	__u32 iff_pad;
};

enum ocfs2_info_type {
	OCFS2_INFO_CLUSTERSIZE = 1,
	OCFS2_INFO_BLOCKSIZE,
	OCFS2_INFO_MAXSLOTS,
	OCFS2_INFO_LABEL,
	OCFS2_INFO_UUID,
	OCFS2_INFO_FS_FEATURES,
	OCFS2_INFO_JOURNAL_SIZE,
	OCFS2_INFO_FREEINODE,
	OCFS2_INFO_FREEFRAG,
	OCFS2_INFO_NUM_TYPES
};

#define OCFS2_INFO_FL_NON_COHERENT	(0x00000001)	
#define OCFS2_INFO_FL_FILLED		(0x40000000)	

#define OCFS2_INFO_FL_ERROR		(0x80000000)	

#define OCFS2_IOC_INFO		_IOR('o', 5, struct ocfs2_info)

struct ocfs2_move_extents {
	
	__u64 me_start;		
	__u64 me_len;		
	__u64 me_goal;		
	__u64 me_threshold;	
	__u64 me_flags;		
	
	__u64 me_moved_len;	
	__u64 me_new_offset;	
	__u32 me_reserved[2];	
};

#define OCFS2_MOVE_EXT_FL_AUTO_DEFRAG	(0x00000001)	
#define OCFS2_MOVE_EXT_FL_PART_DEFRAG	(0x00000002)	
#define OCFS2_MOVE_EXT_FL_COMPLETE	(0x00000004)	

#define OCFS2_IOC_MOVE_EXT	_IOW('o', 6, struct ocfs2_move_extents)

#endif 
