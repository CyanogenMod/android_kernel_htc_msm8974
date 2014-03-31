/*
 * Copyright (c) 2000,2002-2003,2005 Silicon Graphics, Inc.
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
#ifndef __XFS_ATTR_H__
#define	__XFS_ATTR_H__

struct xfs_inode;
struct xfs_da_args;
struct xfs_attr_list_context;




#define ATTR_DONTFOLLOW	0x0001	
#define ATTR_ROOT	0x0002	
#define ATTR_TRUST	0x0004	
#define ATTR_SECURE	0x0008	
#define ATTR_CREATE	0x0010	
#define ATTR_REPLACE	0x0020	

#define ATTR_KERNOTIME	0x1000	
#define ATTR_KERNOVAL	0x2000	

#define XFS_ATTR_FLAGS \
	{ ATTR_DONTFOLLOW, 	"DONTFOLLOW" }, \
	{ ATTR_ROOT,		"ROOT" }, \
	{ ATTR_TRUST,		"TRUST" }, \
	{ ATTR_SECURE,		"SECURE" }, \
	{ ATTR_CREATE,		"CREATE" }, \
	{ ATTR_REPLACE,		"REPLACE" }, \
	{ ATTR_KERNOTIME,	"KERNOTIME" }, \
	{ ATTR_KERNOVAL,	"KERNOVAL" }

#define	ATTR_MAX_VALUELEN	(64*1024)	

typedef struct attrlist {
	__s32	al_count;	
	__s32	al_more;	
	__s32	al_offset[1];	
} attrlist_t;

typedef struct attrlist_ent {	
	__u32	a_valuelen;	
	char	a_name[1];	
} attrlist_ent_t;

#define	ATTR_ENTRY(buffer, index)		\
	((attrlist_ent_t *)			\
	 &((char *)buffer)[ ((attrlist_t *)(buffer))->al_offset[index] ])

typedef struct attrlist_cursor_kern {
	__u32	hashval;	
	__u32	blkno;		
	__u32	offset;		
	__u16	pad1;		
	__u8	pad2;		
	__u8	initted;	
} attrlist_cursor_kern_t;




typedef int (*put_listent_func_t)(struct xfs_attr_list_context *, int,
			      unsigned char *, int, int, unsigned char *);

typedef struct xfs_attr_list_context {
	struct xfs_inode		*dp;		
	struct attrlist_cursor_kern	*cursor;	
	char				*alist;		
	int				seen_enough;	
	ssize_t				count;		
	int				dupcnt;		
	int				bufsize;	
	int				firstu;		
	int				flags;		
	int				resynch;	
	int				put_value;	
	put_listent_func_t		put_listent;	
	int				index;		
} xfs_attr_list_context_t;



int xfs_attr_inactive(struct xfs_inode *dp);
int xfs_attr_rmtval_get(struct xfs_da_args *args);
int xfs_attr_list_int(struct xfs_attr_list_context *);

#endif	
