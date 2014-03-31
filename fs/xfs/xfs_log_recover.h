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
#ifndef	__XFS_LOG_RECOVER_H__
#define __XFS_LOG_RECOVER_H__


#define XLOG_RHASH_BITS  4
#define XLOG_RHASH_SIZE	16
#define XLOG_RHASH_SHIFT 2
#define XLOG_RHASH(tid)	\
	((((__uint32_t)tid)>>XLOG_RHASH_SHIFT) & (XLOG_RHASH_SIZE-1))

#define XLOG_MAX_REGIONS_IN_ITEM   (XFS_MAX_BLOCKSIZE / XFS_BLF_CHUNK / 2 + 1)


typedef struct xlog_recover_item {
	struct list_head	ri_list;
	int			ri_type;
	int			ri_cnt;	
	int			ri_total;	
	xfs_log_iovec_t		*ri_buf;	
} xlog_recover_item_t;

struct xlog_tid;
typedef struct xlog_recover {
	struct hlist_node	r_list;
	xlog_tid_t		r_log_tid;	
	xfs_trans_header_t	r_theader;	
	int			r_state;	
	xfs_lsn_t		r_lsn;		
	struct list_head	r_itemq;	
} xlog_recover_t;

#define ITEM_TYPE(i)	(*(ushort *)(i)->ri_buf[0].i_addr)

#define	XLOG_BC_TABLE_SIZE	64

#define	XLOG_RECOVER_PASS1	1
#define	XLOG_RECOVER_PASS2	2

#endif	
