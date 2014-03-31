/*
 * Copyright (c) 2000-2005 Silicon Graphics, Inc.
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
#ifndef __XFS_TYPES_H__
#define	__XFS_TYPES_H__

#ifdef __KERNEL__

typedef signed char		__int8_t;
typedef unsigned char		__uint8_t;
typedef signed short int	__int16_t;
typedef unsigned short int	__uint16_t;
typedef signed int		__int32_t;
typedef unsigned int		__uint32_t;
typedef signed long long int	__int64_t;
typedef unsigned long long int	__uint64_t;

typedef enum { B_FALSE,B_TRUE }	boolean_t;
typedef __uint32_t		prid_t;		
typedef __uint32_t		inst_t;		

typedef __s64			xfs_off_t;	
typedef unsigned long long	xfs_ino_t;	
typedef __s64			xfs_daddr_t;	
typedef char *			xfs_caddr_t;	
typedef __u32			xfs_dev_t;
typedef __u32			xfs_nlink_t;

#if (BITS_PER_LONG == 32)
typedef __int32_t __psint_t;
typedef __uint32_t __psunsigned_t;
#elif (BITS_PER_LONG == 64)
typedef __int64_t __psint_t;
typedef __uint64_t __psunsigned_t;
#else
#error BITS_PER_LONG must be 32 or 64
#endif

#endif	

typedef __uint32_t	xfs_agblock_t;	
typedef	__uint32_t	xfs_extlen_t;	
typedef	__uint32_t	xfs_agnumber_t;	
typedef __int32_t	xfs_extnum_t;	
typedef __int16_t	xfs_aextnum_t;	
typedef	__int64_t	xfs_fsize_t;	
typedef __uint64_t	xfs_ufsize_t;	

typedef	__int32_t	xfs_suminfo_t;	
typedef	__int32_t	xfs_rtword_t;	

typedef	__int64_t	xfs_lsn_t;	
typedef	__int32_t	xfs_tid_t;	

typedef	__uint32_t	xfs_dablk_t;	
typedef	__uint32_t	xfs_dahash_t;	

typedef __uint64_t	xfs_dfsbno_t;	
typedef __uint64_t	xfs_drfsbno_t;	
typedef	__uint64_t	xfs_drtbno_t;	
typedef	__uint64_t	xfs_dfiloff_t;	
typedef	__uint64_t	xfs_dfilblks_t;	

#if XFS_BIG_BLKNOS
typedef	__uint64_t	xfs_fsblock_t;	
typedef __uint64_t	xfs_rfsblock_t;	
typedef __uint64_t	xfs_rtblock_t;	
typedef	__int64_t	xfs_srtblock_t;	
#else
typedef	__uint32_t	xfs_fsblock_t;	
typedef __uint32_t	xfs_rfsblock_t;	
typedef __uint32_t	xfs_rtblock_t;	
typedef	__int32_t	xfs_srtblock_t;	
#endif
typedef __uint64_t	xfs_fileoff_t;	
typedef __int64_t	xfs_sfiloff_t;	
typedef __uint64_t	xfs_filblks_t;	

#define	NULLDFSBNO	((xfs_dfsbno_t)-1)
#define	NULLDRFSBNO	((xfs_drfsbno_t)-1)
#define	NULLDRTBNO	((xfs_drtbno_t)-1)
#define	NULLDFILOFF	((xfs_dfiloff_t)-1)

#define	NULLFSBLOCK	((xfs_fsblock_t)-1)
#define	NULLRFSBLOCK	((xfs_rfsblock_t)-1)
#define	NULLRTBLOCK	((xfs_rtblock_t)-1)
#define	NULLFILEOFF	((xfs_fileoff_t)-1)

#define	NULLAGBLOCK	((xfs_agblock_t)-1)
#define	NULLAGNUMBER	((xfs_agnumber_t)-1)
#define	NULLEXTNUM	((xfs_extnum_t)-1)

#define NULLCOMMITLSN	((xfs_lsn_t)-1)

#define	MAXEXTLEN	((xfs_extlen_t)0x001fffff)	
#define	MAXEXTNUM	((xfs_extnum_t)0x7fffffff)	
#define	MAXAEXTNUM	((xfs_aextnum_t)0x7fff)		

#define MINDBTPTRS	3
#define MINABTPTRS	2

#define MAXNAMELEN	256

typedef enum {
	XFS_LOOKUP_EQi, XFS_LOOKUP_LEi, XFS_LOOKUP_GEi
} xfs_lookup_t;

typedef enum {
	XFS_BTNUM_BNOi, XFS_BTNUM_CNTi, XFS_BTNUM_BMAPi, XFS_BTNUM_INOi,
	XFS_BTNUM_MAX
} xfs_btnum_t;

struct xfs_name {
	const unsigned char	*name;
	int			len;
};

#endif	
