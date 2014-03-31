/*
 * Copyright (c) 2000,2005 Silicon Graphics, Inc.
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
#ifndef __XFS_DFRAG_H__
#define	__XFS_DFRAG_H__


typedef struct xfs_swapext
{
	__int64_t	sx_version;	
	__int64_t	sx_fdtarget;	
	__int64_t	sx_fdtmp;	
	xfs_off_t	sx_offset;	
	xfs_off_t	sx_length;	
	char		sx_pad[16];	
	xfs_bstat_t	sx_stat;	
} xfs_swapext_t;

#define XFS_SX_VERSION		0

#ifdef __KERNEL__

int	xfs_swapext(struct xfs_swapext *sx);

#endif	

#endif	
