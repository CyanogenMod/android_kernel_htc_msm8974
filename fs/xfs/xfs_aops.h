/*
 * Copyright (c) 2005-2006 Silicon Graphics, Inc.
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
#ifndef __XFS_AOPS_H__
#define __XFS_AOPS_H__

extern mempool_t *xfs_ioend_pool;

enum {
	IO_DIRECT = 0,	
	IO_DELALLOC,	
	IO_UNWRITTEN,	
	IO_OVERWRITE,	
};

#define XFS_IO_TYPES \
	{ 0,			"" }, \
	{ IO_DELALLOC,		"delalloc" }, \
	{ IO_UNWRITTEN,		"unwritten" }, \
	{ IO_OVERWRITE,		"overwrite" }

typedef struct xfs_ioend {
	struct xfs_ioend	*io_list;	
	unsigned int		io_type;	/* delalloc / unwritten */
	int			io_error;	
	atomic_t		io_remaining;	
	unsigned int		io_isasync : 1;	
	unsigned int		io_isdirect : 1;
	struct inode		*io_inode;	/* file being written to */
	struct buffer_head	*io_buffer_head;
	struct buffer_head	*io_buffer_tail;
	size_t			io_size;	
	xfs_off_t		io_offset;	
	struct work_struct	io_work;	
	struct xfs_trans	*io_append_trans;
	struct kiocb		*io_iocb;
	int			io_result;
} xfs_ioend_t;

extern const struct address_space_operations xfs_address_space_operations;
extern int xfs_get_blocks(struct inode *, sector_t, struct buffer_head *, int);

extern void xfs_count_page_state(struct page *, int *, int *);

#endif 
