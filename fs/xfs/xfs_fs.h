/*
 * Copyright (c) 1995-2005 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write the Free Software Foundation,
 * Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef __XFS_FS_H__
#define __XFS_FS_H__


#ifndef HAVE_DIOATTR
struct dioattr {
	__u32		d_mem;		
	__u32		d_miniosz;	
	__u32		d_maxiosz;	
};
#endif

#ifndef HAVE_FSXATTR
struct fsxattr {
	__u32		fsx_xflags;	
	__u32		fsx_extsize;	
	__u32		fsx_nextents;	
	__u32		fsx_projid;	
	unsigned char	fsx_pad[12];
};
#endif

#define XFS_XFLAG_REALTIME	0x00000001	
#define XFS_XFLAG_PREALLOC	0x00000002	
#define XFS_XFLAG_IMMUTABLE	0x00000008	
#define XFS_XFLAG_APPEND	0x00000010	
#define XFS_XFLAG_SYNC		0x00000020	
#define XFS_XFLAG_NOATIME	0x00000040	
#define XFS_XFLAG_NODUMP	0x00000080	
#define XFS_XFLAG_RTINHERIT	0x00000100	
#define XFS_XFLAG_PROJINHERIT	0x00000200	
#define XFS_XFLAG_NOSYMLINKS	0x00000400	
#define XFS_XFLAG_EXTSIZE	0x00000800	
#define XFS_XFLAG_EXTSZINHERIT	0x00001000	
#define XFS_XFLAG_NODEFRAG	0x00002000  	
#define XFS_XFLAG_FILESTREAM	0x00004000	
#define XFS_XFLAG_HASATTR	0x80000000	

#ifndef HAVE_GETBMAP
struct getbmap {
	__s64		bmv_offset;	
	__s64		bmv_block;	
	__s64		bmv_length;	
	__s32		bmv_count;	
	__s32		bmv_entries;	
};
#endif

#ifndef HAVE_GETBMAPX
struct getbmapx {
	__s64		bmv_offset;	
	__s64		bmv_block;	
	__s64		bmv_length;	
	__s32		bmv_count;	
	__s32		bmv_entries;	
	__s32		bmv_iflags;	
	__s32		bmv_oflags;	
	__s32		bmv_unused1;	
	__s32		bmv_unused2;	
};
#endif

#define BMV_IF_ATTRFORK		0x1	
#define BMV_IF_NO_DMAPI_READ	0x2	
#define BMV_IF_PREALLOC		0x4	
#define BMV_IF_DELALLOC		0x8	
#define BMV_IF_NO_HOLES		0x10	
#define BMV_IF_VALID	\
	(BMV_IF_ATTRFORK|BMV_IF_NO_DMAPI_READ|BMV_IF_PREALLOC|	\
	 BMV_IF_DELALLOC|BMV_IF_NO_HOLES)

#define BMV_OF_PREALLOC		0x1	/* segment = unwritten pre-allocation */
#define BMV_OF_DELALLOC		0x2	
#define BMV_OF_LAST		0x4	

#ifndef HAVE_FSDMIDATA
struct fsdmidata {
	__u32		fsd_dmevmask;	
	__u16		fsd_padding;
	__u16		fsd_dmstate;	
};
#endif

typedef struct xfs_flock64 {
	__s16		l_type;
	__s16		l_whence;
	__s64		l_start;
	__s64		l_len;		
	__s32		l_sysid;
	__u32		l_pid;
	__s32		l_pad[4];	
} xfs_flock64_t;

typedef struct xfs_fsop_geom_v1 {
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
} xfs_fsop_geom_v1_t;

typedef struct xfs_fsop_geom {
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
	__u32		logsunit;	
} xfs_fsop_geom_t;

typedef struct xfs_fsop_counts {
	__u64	freedata;	
	__u64	freertx;	
	__u64	freeino;	
	__u64	allocino;	
} xfs_fsop_counts_t;

typedef struct xfs_fsop_resblks {
	__u64  resblks;
	__u64  resblks_avail;
} xfs_fsop_resblks_t;

#define XFS_FSOP_GEOM_VERSION	0

#define XFS_FSOP_GEOM_FLAGS_ATTR	0x0001	
#define XFS_FSOP_GEOM_FLAGS_NLINK	0x0002	
#define XFS_FSOP_GEOM_FLAGS_QUOTA	0x0004	
#define XFS_FSOP_GEOM_FLAGS_IALIGN	0x0008	
#define XFS_FSOP_GEOM_FLAGS_DALIGN	0x0010	
#define XFS_FSOP_GEOM_FLAGS_SHARED	0x0020	
#define XFS_FSOP_GEOM_FLAGS_EXTFLG	0x0040	
#define XFS_FSOP_GEOM_FLAGS_DIRV2	0x0080	
#define XFS_FSOP_GEOM_FLAGS_LOGV2	0x0100	
#define XFS_FSOP_GEOM_FLAGS_SECTOR	0x0200	
#define XFS_FSOP_GEOM_FLAGS_ATTR2	0x0400	
#define XFS_FSOP_GEOM_FLAGS_DIRV2CI	0x1000	
#define XFS_FSOP_GEOM_FLAGS_LAZYSB	0x4000	


#define XFS_MIN_AG_BLOCKS	64
#define XFS_MIN_LOG_BLOCKS	512ULL
#define XFS_MAX_LOG_BLOCKS	(1024 * 1024ULL)
#define XFS_MIN_LOG_BYTES	(10 * 1024 * 1024ULL)

#define XFS_MAX_LOG_BYTES \
	((2 * 1024 * 1024 * 1024ULL) - XFS_MIN_LOG_BYTES)

#define XFS_MAX_DBLOCKS(s) ((xfs_drfsbno_t)(s)->sb_agcount * (s)->sb_agblocks)
#define XFS_MIN_DBLOCKS(s) ((xfs_drfsbno_t)((s)->sb_agcount - 1) *	\
			 (s)->sb_agblocks + XFS_MIN_AG_BLOCKS)

typedef struct xfs_growfs_data {
	__u64		newblocks;	
	__u32		imaxpct;	
} xfs_growfs_data_t;

typedef struct xfs_growfs_log {
	__u32		newblocks;	
	__u32		isint;		
} xfs_growfs_log_t;

typedef struct xfs_growfs_rt {
	__u64		newblocks;	
	__u32		extsize;	
} xfs_growfs_rt_t;


typedef struct xfs_bstime {
	time_t		tv_sec;		
	__s32		tv_nsec;	
} xfs_bstime_t;

typedef struct xfs_bstat {
	__u64		bs_ino;		
	__u16		bs_mode;	
	__u16		bs_nlink;	
	__u32		bs_uid;		
	__u32		bs_gid;		
	__u32		bs_rdev;	
	__s32		bs_blksize;	
	__s64		bs_size;	
	xfs_bstime_t	bs_atime;	
	xfs_bstime_t	bs_mtime;	
	xfs_bstime_t	bs_ctime;	
	int64_t		bs_blocks;	
	__u32		bs_xflags;	
	__s32		bs_extsize;	
	__s32		bs_extents;	
	__u32		bs_gen;		
	__u16		bs_projid_lo;	
#define	bs_projid	bs_projid_lo	
	__u16		bs_forkoff;	
	__u16		bs_projid_hi;	
	unsigned char	bs_pad[10];	
	__u32		bs_dmevmask;	
	__u16		bs_dmstate;	
	__u16		bs_aextents;	
} xfs_bstat_t;

typedef struct xfs_fsop_bulkreq {
	__u64		__user *lastip;	
	__s32		icount;		
	void		__user *ubuffer;
	__s32		__user *ocount;	
} xfs_fsop_bulkreq_t;


typedef struct xfs_inogrp {
	__u64		xi_startino;	
	__s32		xi_alloccount;	
	__u64		xi_allocmask;	
} xfs_inogrp_t;


typedef struct xfs_error_injection {
	__s32		fd;
	__s32		errtag;
} xfs_error_injection_t;


typedef struct xfs_fsop_handlereq {
	__u32		fd;		
	void		__user *path;	
	__u32		oflags;		
	void		__user *ihandle;
	__u32		ihandlen;	
	void		__user *ohandle;
	__u32		__user *ohandlen;
} xfs_fsop_handlereq_t;


typedef struct xfs_fsop_setdm_handlereq {
	struct xfs_fsop_handlereq	hreq;	
	struct fsdmidata		__user *data;	
} xfs_fsop_setdm_handlereq_t;

typedef struct xfs_attrlist_cursor {
	__u32		opaque[4];
} xfs_attrlist_cursor_t;

typedef struct xfs_fsop_attrlist_handlereq {
	struct xfs_fsop_handlereq	hreq; 
	struct xfs_attrlist_cursor	pos; 
	__u32				flags;	
	__u32				buflen;	
	void				__user *buffer;	
} xfs_fsop_attrlist_handlereq_t;

typedef struct xfs_attr_multiop {
	__u32		am_opcode;
#define ATTR_OP_GET	1	
#define ATTR_OP_SET	2	
#define ATTR_OP_REMOVE	3	
	__s32		am_error;
	void		__user *am_attrname;
	void		__user *am_attrvalue;
	__u32		am_length;
	__u32		am_flags;
} xfs_attr_multiop_t;

typedef struct xfs_fsop_attrmulti_handlereq {
	struct xfs_fsop_handlereq	hreq; 
	__u32				opcount;
	struct xfs_attr_multiop		__user *ops; 
} xfs_fsop_attrmulti_handlereq_t;

typedef struct { __u32 val[2]; } xfs_fsid_t; 

typedef struct xfs_fid {
	__u16	fid_len;		
	__u16	fid_pad;
	__u32	fid_gen;		
	__u64	fid_ino;		
} xfs_fid_t;

typedef struct xfs_handle {
	union {
		__s64	    align;	
		xfs_fsid_t  _ha_fsid;	
	} ha_u;
	xfs_fid_t	ha_fid;		
} xfs_handle_t;
#define ha_fsid ha_u._ha_fsid

#define XFS_HSIZE(handle)	(((char *) &(handle).ha_fid.fid_pad	 \
				 - (char *) &(handle))			  \
				 + (handle).ha_fid.fid_len)

#define XFS_FSOP_GOING_FLAGS_DEFAULT		0x0	
#define XFS_FSOP_GOING_FLAGS_LOGFLUSH		0x1	
#define XFS_FSOP_GOING_FLAGS_NOLOGFLUSH		0x2	

#define XFS_IOC_GETXFLAGS	FS_IOC_GETFLAGS
#define XFS_IOC_SETXFLAGS	FS_IOC_SETFLAGS
#define XFS_IOC_GETVERSION	FS_IOC_GETVERSION

#define XFS_IOC_ALLOCSP		_IOW ('X', 10, struct xfs_flock64)
#define XFS_IOC_FREESP		_IOW ('X', 11, struct xfs_flock64)
#define XFS_IOC_DIOINFO		_IOR ('X', 30, struct dioattr)
#define XFS_IOC_FSGETXATTR	_IOR ('X', 31, struct fsxattr)
#define XFS_IOC_FSSETXATTR	_IOW ('X', 32, struct fsxattr)
#define XFS_IOC_ALLOCSP64	_IOW ('X', 36, struct xfs_flock64)
#define XFS_IOC_FREESP64	_IOW ('X', 37, struct xfs_flock64)
#define XFS_IOC_GETBMAP		_IOWR('X', 38, struct getbmap)
#define XFS_IOC_FSSETDM		_IOW ('X', 39, struct fsdmidata)
#define XFS_IOC_RESVSP		_IOW ('X', 40, struct xfs_flock64)
#define XFS_IOC_UNRESVSP	_IOW ('X', 41, struct xfs_flock64)
#define XFS_IOC_RESVSP64	_IOW ('X', 42, struct xfs_flock64)
#define XFS_IOC_UNRESVSP64	_IOW ('X', 43, struct xfs_flock64)
#define XFS_IOC_GETBMAPA	_IOWR('X', 44, struct getbmap)
#define XFS_IOC_FSGETXATTRA	_IOR ('X', 45, struct fsxattr)
#define XFS_IOC_GETBMAPX	_IOWR('X', 56, struct getbmap)
#define XFS_IOC_ZERO_RANGE	_IOW ('X', 57, struct xfs_flock64)

#define XFS_IOC_FSGEOMETRY_V1	     _IOR ('X', 100, struct xfs_fsop_geom_v1)
#define XFS_IOC_FSBULKSTAT	     _IOWR('X', 101, struct xfs_fsop_bulkreq)
#define XFS_IOC_FSBULKSTAT_SINGLE    _IOWR('X', 102, struct xfs_fsop_bulkreq)
#define XFS_IOC_FSINUMBERS	     _IOWR('X', 103, struct xfs_fsop_bulkreq)
#define XFS_IOC_PATH_TO_FSHANDLE     _IOWR('X', 104, struct xfs_fsop_handlereq)
#define XFS_IOC_PATH_TO_HANDLE	     _IOWR('X', 105, struct xfs_fsop_handlereq)
#define XFS_IOC_FD_TO_HANDLE	     _IOWR('X', 106, struct xfs_fsop_handlereq)
#define XFS_IOC_OPEN_BY_HANDLE	     _IOWR('X', 107, struct xfs_fsop_handlereq)
#define XFS_IOC_READLINK_BY_HANDLE   _IOWR('X', 108, struct xfs_fsop_handlereq)
#define XFS_IOC_SWAPEXT		     _IOWR('X', 109, struct xfs_swapext)
#define XFS_IOC_FSGROWFSDATA	     _IOW ('X', 110, struct xfs_growfs_data)
#define XFS_IOC_FSGROWFSLOG	     _IOW ('X', 111, struct xfs_growfs_log)
#define XFS_IOC_FSGROWFSRT	     _IOW ('X', 112, struct xfs_growfs_rt)
#define XFS_IOC_FSCOUNTS	     _IOR ('X', 113, struct xfs_fsop_counts)
#define XFS_IOC_SET_RESBLKS	     _IOWR('X', 114, struct xfs_fsop_resblks)
#define XFS_IOC_GET_RESBLKS	     _IOR ('X', 115, struct xfs_fsop_resblks)
#define XFS_IOC_ERROR_INJECTION	     _IOW ('X', 116, struct xfs_error_injection)
#define XFS_IOC_ERROR_CLEARALL	     _IOW ('X', 117, struct xfs_error_injection)
#define XFS_IOC_FSSETDM_BY_HANDLE    _IOW ('X', 121, struct xfs_fsop_setdm_handlereq)
#define XFS_IOC_ATTRLIST_BY_HANDLE   _IOW ('X', 122, struct xfs_fsop_attrlist_handlereq)
#define XFS_IOC_ATTRMULTI_BY_HANDLE  _IOW ('X', 123, struct xfs_fsop_attrmulti_handlereq)
#define XFS_IOC_FSGEOMETRY	     _IOR ('X', 124, struct xfs_fsop_geom)
#define XFS_IOC_GOINGDOWN	     _IOR ('X', 125, __uint32_t)


#ifndef HAVE_BBMACROS
#define BBSHIFT		9
#define BBSIZE		(1<<BBSHIFT)
#define BBMASK		(BBSIZE-1)
#define BTOBB(bytes)	(((__u64)(bytes) + BBSIZE - 1) >> BBSHIFT)
#define BTOBBT(bytes)	((__u64)(bytes) >> BBSHIFT)
#define BBTOB(bbs)	((bbs) << BBSHIFT)
#endif

#endif	
