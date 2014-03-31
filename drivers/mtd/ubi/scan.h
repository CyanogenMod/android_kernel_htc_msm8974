/*
 * Copyright (c) International Business Machines Corp., 2006
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
 * the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Author: Artem Bityutskiy (Битюцкий Артём)
 */

#ifndef __UBI_SCAN_H__
#define __UBI_SCAN_H__

#define UBI_SCAN_UNKNOWN_EC (-1)

struct ubi_scan_leb {
	int ec;
	int pnum;
	int lnum;
	unsigned int scrub:1;
	unsigned int copy_flag:1;
	unsigned long long sqnum;
	union {
		struct rb_node rb;
		struct list_head list;
	} u;
};

struct ubi_scan_volume {
	int vol_id;
	int highest_lnum;
	int leb_count;
	int vol_type;
	int used_ebs;
	int last_data_size;
	int data_pad;
	int compat;
	struct rb_node rb;
	struct rb_root root;
};

struct ubi_scan_info {
	struct rb_root volumes;
	struct list_head corr;
	struct list_head free;
	struct list_head erase;
	struct list_head alien;
	int corr_peb_count;
	int empty_peb_count;
	int alien_peb_count;
	int bad_peb_count;
	int maybe_bad_peb_count;
	int vols_found;
	int highest_vol_id;
	int is_empty;
	int min_ec;
	int max_ec;
	unsigned long long max_sqnum;
	int mean_ec;
	uint64_t ec_sum;
	int ec_count;
	struct kmem_cache *scan_leb_slab;
};

struct ubi_device;
struct ubi_vid_hdr;

static inline void ubi_scan_move_to_list(struct ubi_scan_volume *sv,
					 struct ubi_scan_leb *seb,
					 struct list_head *list)
{
		rb_erase(&seb->u.rb, &sv->root);
		list_add_tail(&seb->u.list, list);
}

int ubi_scan_add_used(struct ubi_device *ubi, struct ubi_scan_info *si,
		      int pnum, int ec, const struct ubi_vid_hdr *vid_hdr,
		      int bitflips);
struct ubi_scan_volume *ubi_scan_find_sv(const struct ubi_scan_info *si,
					 int vol_id);
struct ubi_scan_leb *ubi_scan_find_seb(const struct ubi_scan_volume *sv,
				       int lnum);
void ubi_scan_rm_volume(struct ubi_scan_info *si, struct ubi_scan_volume *sv);
struct ubi_scan_leb *ubi_scan_get_free_peb(struct ubi_device *ubi,
					   struct ubi_scan_info *si);
int ubi_scan_erase_peb(struct ubi_device *ubi, const struct ubi_scan_info *si,
		       int pnum, int ec);
struct ubi_scan_info *ubi_scan(struct ubi_device *ubi);
void ubi_scan_destroy_si(struct ubi_scan_info *si);

#endif 
