/*
 * Copyright (c) 2004-2005 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write the Free Software Foundation,
 * Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef __XFS_IOCTL32_H__
#define __XFS_IOCTL32_H__

#include <linux/compat.h>


#define XFS_IOC_GETXFLAGS_32	FS_IOC32_GETFLAGS
#define XFS_IOC_SETXFLAGS_32	FS_IOC32_SETFLAGS
#define XFS_IOC_GETVERSION_32	FS_IOC32_GETVERSION

#if defined(CONFIG_IA64) || defined(CONFIG_X86_64)
#define BROKEN_X86_ALIGNMENT
#define __compat_packed __attribute__((packed))
#else
#define __compat_packed
#endif

typedef struct compat_xfs_bstime {
	compat_time_t	tv_sec;		
	__s32		tv_nsec;	
} compat_xfs_bstime_t;

typedef struct compat_xfs_bstat {
	__u64		bs_ino;		
	__u16		bs_mode;	
	__u16		bs_nlink;	
	__u32		bs_uid;		
	__u32		bs_gid;		
	__u32		bs_rdev;	
	__s32		bs_blksize;	
	__s64		bs_size;	
	compat_xfs_bstime_t bs_atime;	
	compat_xfs_bstime_t bs_mtime;	
	compat_xfs_bstime_t bs_ctime;	
	int64_t		bs_blocks;	
	__u32		bs_xflags;	
	__s32		bs_extsize;	
	__s32		bs_extents;	
	__u32		bs_gen;		
	__u16		bs_projid_lo;	
#define	bs_projid	bs_projid_lo	
	__u16		bs_projid_hi;	
	unsigned char	bs_pad[12];	
	__u32		bs_dmevmask;	
	__u16		bs_dmstate;	
	__u16		bs_aextents;	
} __compat_packed compat_xfs_bstat_t;

typedef struct compat_xfs_fsop_bulkreq {
	compat_uptr_t	lastip;		
	__s32		icount;		
	compat_uptr_t	ubuffer;	
	compat_uptr_t	ocount;		
} compat_xfs_fsop_bulkreq_t;

#define XFS_IOC_FSBULKSTAT_32 \
	_IOWR('X', 101, struct compat_xfs_fsop_bulkreq)
#define XFS_IOC_FSBULKSTAT_SINGLE_32 \
	_IOWR('X', 102, struct compat_xfs_fsop_bulkreq)
#define XFS_IOC_FSINUMBERS_32 \
	_IOWR('X', 103, struct compat_xfs_fsop_bulkreq)

typedef struct compat_xfs_fsop_handlereq {
	__u32		fd;		
	compat_uptr_t	path;		
	__u32		oflags;		
	compat_uptr_t	ihandle;	
	__u32		ihandlen;	
	compat_uptr_t	ohandle;	
	compat_uptr_t	ohandlen;	
} compat_xfs_fsop_handlereq_t;

#define XFS_IOC_PATH_TO_FSHANDLE_32 \
	_IOWR('X', 104, struct compat_xfs_fsop_handlereq)
#define XFS_IOC_PATH_TO_HANDLE_32 \
	_IOWR('X', 105, struct compat_xfs_fsop_handlereq)
#define XFS_IOC_FD_TO_HANDLE_32 \
	_IOWR('X', 106, struct compat_xfs_fsop_handlereq)
#define XFS_IOC_OPEN_BY_HANDLE_32 \
	_IOWR('X', 107, struct compat_xfs_fsop_handlereq)
#define XFS_IOC_READLINK_BY_HANDLE_32 \
	_IOWR('X', 108, struct compat_xfs_fsop_handlereq)

typedef struct compat_xfs_swapext {
	__int64_t		sx_version;	
	__int64_t		sx_fdtarget;	
	__int64_t		sx_fdtmp;	
	xfs_off_t		sx_offset;	
	xfs_off_t		sx_length;	
	char			sx_pad[16];	
	compat_xfs_bstat_t	sx_stat;	
} __compat_packed compat_xfs_swapext_t;

#define XFS_IOC_SWAPEXT_32	_IOWR('X', 109, struct compat_xfs_swapext)

typedef struct compat_xfs_fsop_attrlist_handlereq {
	struct compat_xfs_fsop_handlereq hreq; 
	struct xfs_attrlist_cursor	pos; 
	__u32				flags;	
	__u32				buflen;	
	compat_uptr_t			buffer;	
} __compat_packed compat_xfs_fsop_attrlist_handlereq_t;

#define XFS_IOC_ATTRLIST_BY_HANDLE_32 \
	_IOW('X', 122, struct compat_xfs_fsop_attrlist_handlereq)

typedef struct compat_xfs_attr_multiop {
	__u32		am_opcode;
	__s32		am_error;
	compat_uptr_t	am_attrname;
	compat_uptr_t	am_attrvalue;
	__u32		am_length;
	__u32		am_flags;
} compat_xfs_attr_multiop_t;

typedef struct compat_xfs_fsop_attrmulti_handlereq {
	struct compat_xfs_fsop_handlereq hreq; 
	__u32				opcount;
	
	compat_uptr_t			ops; 
} compat_xfs_fsop_attrmulti_handlereq_t;

#define XFS_IOC_ATTRMULTI_BY_HANDLE_32 \
	_IOW('X', 123, struct compat_xfs_fsop_attrmulti_handlereq)

typedef struct compat_xfs_fsop_setdm_handlereq {
	struct compat_xfs_fsop_handlereq hreq;	
	
	compat_uptr_t			data;	
} compat_xfs_fsop_setdm_handlereq_t;

#define XFS_IOC_FSSETDM_BY_HANDLE_32 \
	_IOW('X', 121, struct compat_xfs_fsop_setdm_handlereq)

#ifdef BROKEN_X86_ALIGNMENT
typedef struct compat_xfs_flock64 {
	__s16		l_type;
	__s16		l_whence;
	__s64		l_start	__attribute__((packed));
			
	__s64		l_len __attribute__((packed));
	__s32		l_sysid;
	__u32		l_pid;
	__s32		l_pad[4];	
} compat_xfs_flock64_t;

#define XFS_IOC_ALLOCSP_32	_IOW('X', 10, struct compat_xfs_flock64)
#define XFS_IOC_FREESP_32	_IOW('X', 11, struct compat_xfs_flock64)
#define XFS_IOC_ALLOCSP64_32	_IOW('X', 36, struct compat_xfs_flock64)
#define XFS_IOC_FREESP64_32	_IOW('X', 37, struct compat_xfs_flock64)
#define XFS_IOC_RESVSP_32	_IOW('X', 40, struct compat_xfs_flock64)
#define XFS_IOC_UNRESVSP_32	_IOW('X', 41, struct compat_xfs_flock64)
#define XFS_IOC_RESVSP64_32	_IOW('X', 42, struct compat_xfs_flock64)
#define XFS_IOC_UNRESVSP64_32	_IOW('X', 43, struct compat_xfs_flock64)
#define XFS_IOC_ZERO_RANGE_32	_IOW('X', 57, struct compat_xfs_flock64)

typedef struct compat_xfs_fsop_geom_v1 {
	__u32		blocksize;	
	__u32		rtextsize;	
	__u32		agblocks;	
	__u32		agcount;	
	__u32		logblocks;	
	__u32		sectsize;	
	__u32		inodesize;	
	__u32		imaxpct;	
	__u64		datablocks;	
	__u64		rtblocks;	
	__u64		rtextents;	
	__u64		logstart;	
	unsigned char	uuid[16];	
	__u32		sunit;		
	__u32		swidth;		
	__s32		version;	
	__u32		flags;		
	__u32		logsectsize;	
	__u32		rtsectsize;	
	__u32		dirblocksize;	
} __attribute__((packed)) compat_xfs_fsop_geom_v1_t;

#define XFS_IOC_FSGEOMETRY_V1_32  \
	_IOR('X', 100, struct compat_xfs_fsop_geom_v1)

typedef struct compat_xfs_inogrp {
	__u64		xi_startino;	
	__s32		xi_alloccount;	
	__u64		xi_allocmask;	
} __attribute__((packed)) compat_xfs_inogrp_t;

typedef struct compat_xfs_growfs_data {
	__u64		newblocks;	
	__u32		imaxpct;	
} __attribute__((packed)) compat_xfs_growfs_data_t;

typedef struct compat_xfs_growfs_rt {
	__u64		newblocks;	
	__u32		extsize;	
} __attribute__((packed)) compat_xfs_growfs_rt_t;

#define XFS_IOC_FSGROWFSDATA_32 _IOW('X', 110, struct compat_xfs_growfs_data)
#define XFS_IOC_FSGROWFSRT_32   _IOW('X', 112, struct compat_xfs_growfs_rt)

#endif 

#endif 
