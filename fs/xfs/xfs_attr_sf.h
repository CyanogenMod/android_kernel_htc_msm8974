/*
 * Copyright (c) 2000,2002,2005 Silicon Graphics, Inc.
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
#ifndef __XFS_ATTR_SF_H__
#define	__XFS_ATTR_SF_H__


typedef struct xfs_attr_shortform {
	struct xfs_attr_sf_hdr {	
		__be16	totsize;	
		__u8	count;	
	} hdr;
	struct xfs_attr_sf_entry {
		__uint8_t namelen;	
		__uint8_t valuelen;	
		__uint8_t flags;	
		__uint8_t nameval[1];	
	} list[1];			
} xfs_attr_shortform_t;
typedef struct xfs_attr_sf_hdr xfs_attr_sf_hdr_t;
typedef struct xfs_attr_sf_entry xfs_attr_sf_entry_t;

typedef struct xfs_attr_sf_sort {
	__uint8_t	entno;		
	__uint8_t	namelen;	
	__uint8_t	valuelen;	
	__uint8_t	flags;		
	xfs_dahash_t	hash;		
	unsigned char	*name;		
} xfs_attr_sf_sort_t;

#define XFS_ATTR_SF_ENTSIZE_BYNAME(nlen,vlen)	 \
	(((int)sizeof(xfs_attr_sf_entry_t)-1 + (nlen)+(vlen)))
#define XFS_ATTR_SF_ENTSIZE_MAX			 \
	((1 << (NBBY*(int)sizeof(__uint8_t))) - 1)
#define XFS_ATTR_SF_ENTSIZE(sfep)		 \
	((int)sizeof(xfs_attr_sf_entry_t)-1 + (sfep)->namelen+(sfep)->valuelen)
#define XFS_ATTR_SF_NEXTENTRY(sfep)		 \
	((xfs_attr_sf_entry_t *)((char *)(sfep) + XFS_ATTR_SF_ENTSIZE(sfep)))
#define XFS_ATTR_SF_TOTSIZE(dp)			 \
	(be16_to_cpu(((xfs_attr_shortform_t *)	\
		((dp)->i_afp->if_u1.if_data))->hdr.totsize))

#endif	
