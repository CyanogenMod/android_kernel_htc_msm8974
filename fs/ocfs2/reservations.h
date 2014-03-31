/* -*- mode: c; c-basic-offset: 8; -*-
 * vim: noexpandtab sw=8 ts=8 sts=0:
 *
 * reservations.h
 *
 * Allocation reservations function prototypes and structures.
 *
 * Copyright (C) 2010 Novell.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#ifndef	OCFS2_RESERVATIONS_H
#define	OCFS2_RESERVATIONS_H

#include <linux/rbtree.h>

#define OCFS2_DEFAULT_RESV_LEVEL	2
#define OCFS2_MAX_RESV_LEVEL	9
#define OCFS2_MIN_RESV_LEVEL	0

struct ocfs2_alloc_reservation {
	struct rb_node	r_node;

	unsigned int	r_start;	
	unsigned int	r_len;		

	unsigned int	r_last_len;	
	unsigned int	r_last_start;	
	struct list_head	r_lru;	

	unsigned int	r_flags;
};

#define	OCFS2_RESV_FLAG_INUSE	0x01	
#define	OCFS2_RESV_FLAG_TMP	0x02	
#define	OCFS2_RESV_FLAG_DIR	0x04	

struct ocfs2_reservation_map {
	struct rb_root		m_reservations;
	char			*m_disk_bitmap;

	struct ocfs2_super	*m_osb;

	u32			m_bitmap_len;	

	struct list_head	m_lru;		

};

void ocfs2_resv_init_once(struct ocfs2_alloc_reservation *resv);

#define OCFS2_RESV_TYPES	(OCFS2_RESV_FLAG_TMP|OCFS2_RESV_FLAG_DIR)
void ocfs2_resv_set_type(struct ocfs2_alloc_reservation *resv,
			 unsigned int flags);

int ocfs2_dir_resv_allowed(struct ocfs2_super *osb);

void ocfs2_resv_discard(struct ocfs2_reservation_map *resmap,
			struct ocfs2_alloc_reservation *resv);


int ocfs2_resmap_init(struct ocfs2_super *osb,
		      struct ocfs2_reservation_map *resmap);

void ocfs2_resmap_restart(struct ocfs2_reservation_map *resmap,
			  unsigned int clen, char *disk_bitmap);

void ocfs2_resmap_uninit(struct ocfs2_reservation_map *resmap);

int ocfs2_resmap_resv_bits(struct ocfs2_reservation_map *resmap,
			   struct ocfs2_alloc_reservation *resv,
			   int *cstart, int *clen);

void ocfs2_resmap_claimed_bits(struct ocfs2_reservation_map *resmap,
			       struct ocfs2_alloc_reservation *resv,
			       u32 cstart, u32 clen);

#endif	
