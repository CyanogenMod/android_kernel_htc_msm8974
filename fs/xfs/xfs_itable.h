/*
 * Copyright (c) 2000-2001 Silicon Graphics, Inc.  All Rights Reserved.
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
#ifndef __XFS_ITABLE_H__
#define	__XFS_ITABLE_H__

typedef int (*bulkstat_one_pf)(struct xfs_mount	*mp,
			       xfs_ino_t	ino,
			       void		__user *buffer,
			       int		ubsize,
			       int		*ubused,
			       int		*stat);

#define BULKSTAT_RV_NOTHING	0
#define BULKSTAT_RV_DIDONE	1
#define BULKSTAT_RV_GIVEUP	2

int					
xfs_bulkstat(
	xfs_mount_t	*mp,		
	xfs_ino_t	*lastino,	
	int		*count,		
	bulkstat_one_pf formatter,	
	size_t		statstruct_size,
	char		__user *ubuffer,
	int		*done);		

int
xfs_bulkstat_single(
	xfs_mount_t		*mp,
	xfs_ino_t		*lastinop,
	char			__user *buffer,
	int			*done);

typedef int (*bulkstat_one_fmt_pf)(  
	void			__user *ubuffer, 
	int			ubsize,		 
	int			*ubused,	 
	const xfs_bstat_t	*buffer);        

int
xfs_bulkstat_one_int(
	xfs_mount_t		*mp,
	xfs_ino_t		ino,
	void			__user *buffer,
	int			ubsize,
	bulkstat_one_fmt_pf	formatter,
	int			*ubused,
	int			*stat);

int
xfs_bulkstat_one(
	xfs_mount_t		*mp,
	xfs_ino_t		ino,
	void			__user *buffer,
	int			ubsize,
	int			*ubused,
	int			*stat);

typedef int (*inumbers_fmt_pf)(
	void			__user *ubuffer, 
	const xfs_inogrp_t	*buffer,	
	long			count,		
	long			*written);	/* # of bytes written */

int
xfs_inumbers_fmt(
	void			__user *ubuffer, 
	const xfs_inogrp_t	*buffer,	
	long			count,		
	long			*written);	/* # of bytes written */

int					
xfs_inumbers(
	xfs_mount_t		*mp,	
	xfs_ino_t		*last,	
	int			*count,	
	void			__user *buffer, 
	inumbers_fmt_pf		formatter);

#endif	
