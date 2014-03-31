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

/*
 * UBI scanning sub-system.
 *
 * This sub-system is responsible for scanning the flash media, checking UBI
 * headers and providing complete information about the UBI flash image.
 *
 * The scanning information is represented by a &struct ubi_scan_info' object.
 * Information about found volumes is represented by &struct ubi_scan_volume
 * objects which are kept in volume RB-tree with root at the @volumes field.
 * The RB-tree is indexed by the volume ID.
 *
 * Scanned logical eraseblocks are represented by &struct ubi_scan_leb objects.
 * These objects are kept in per-volume RB-trees with the root at the
 * corresponding &struct ubi_scan_volume object. To put it differently, we keep
 * an RB-tree of per-volume objects and each of these objects is the root of
 * RB-tree of per-eraseblock objects.
 *
 * Corrupted physical eraseblocks are put to the @corr list, free physical
 * eraseblocks are put to the @free list and the physical eraseblock to be
 * erased are put to the @erase list.
 *
 * About corruptions
 * ~~~~~~~~~~~~~~~~~
 *
 * UBI protects EC and VID headers with CRC-32 checksums, so it can detect
 * whether the headers are corrupted or not. Sometimes UBI also protects the
 * data with CRC-32, e.g., when it executes the atomic LEB change operation, or
 * when it moves the contents of a PEB for wear-leveling purposes.
 *
 * UBI tries to distinguish between 2 types of corruptions.
 *
 * 1. Corruptions caused by power cuts. These are expected corruptions and UBI
 * tries to handle them gracefully, without printing too many warnings and
 * error messages. The idea is that we do not lose important data in these case
 * - we may lose only the data which was being written to the media just before
 * the power cut happened, and the upper layers (e.g., UBIFS) are supposed to
 * handle such data losses (e.g., by using the FS journal).
 *
 * When UBI detects a corruption (CRC-32 mismatch) in a PEB, and it looks like
 * the reason is a power cut, UBI puts this PEB to the @erase list, and all
 * PEBs in the @erase list are scheduled for erasure later.
 *
 * 2. Unexpected corruptions which are not caused by power cuts. During
 * scanning, such PEBs are put to the @corr list and UBI preserves them.
 * Obviously, this lessens the amount of available PEBs, and if at some  point
 * UBI runs out of free PEBs, it switches to R/O mode. UBI also loudly informs
 * about such PEBs every time the MTD device is attached.
 *
 * However, it is difficult to reliably distinguish between these types of
 * corruptions and UBI's strategy is as follows. UBI assumes corruption type 2
 * if the VID header is corrupted and the data area does not contain all 0xFFs,
 * and there were no bit-flips or integrity errors while reading the data area.
 * Otherwise UBI assumes corruption type 1. So the decision criteria are as
 * follows.
 *   o If the data area contains only 0xFFs, there is no data, and it is safe
 *     to just erase this PEB - this is corruption type 1.
 *   o If the data area has bit-flips or data integrity errors (ECC errors on
 *     NAND), it is probably a PEB which was being erased when power cut
 *     happened, so this is corruption type 1. However, this is just a guess,
 *     which might be wrong.
 *   o Otherwise this it corruption type 2.
 */

#include <linux/err.h>
#include <linux/slab.h>
#include <linux/crc32.h>
#include <linux/math64.h>
#include <linux/random.h>
#include "ubi.h"

#ifdef CONFIG_MTD_UBI_DEBUG
static int paranoid_check_si(struct ubi_device *ubi, struct ubi_scan_info *si);
#else
#define paranoid_check_si(ubi, si) 0
#endif

static struct ubi_ec_hdr *ech;
static struct ubi_vid_hdr *vidh;

static int add_to_list(struct ubi_scan_info *si, int pnum, int ec, int to_head,
		       struct list_head *list)
{
	struct ubi_scan_leb *seb;

	if (list == &si->free) {
		dbg_bld("add to free: PEB %d, EC %d", pnum, ec);
	} else if (list == &si->erase) {
		dbg_bld("add to erase: PEB %d, EC %d", pnum, ec);
	} else if (list == &si->alien) {
		dbg_bld("add to alien: PEB %d, EC %d", pnum, ec);
		si->alien_peb_count += 1;
	} else
		BUG();

	seb = kmem_cache_alloc(si->scan_leb_slab, GFP_KERNEL);
	if (!seb)
		return -ENOMEM;

	seb->pnum = pnum;
	seb->ec = ec;
	if (to_head)
		list_add(&seb->u.list, list);
	else
		list_add_tail(&seb->u.list, list);
	return 0;
}

static int add_corrupted(struct ubi_scan_info *si, int pnum, int ec)
{
	struct ubi_scan_leb *seb;

	dbg_bld("add to corrupted: PEB %d, EC %d", pnum, ec);

	seb = kmem_cache_alloc(si->scan_leb_slab, GFP_KERNEL);
	if (!seb)
		return -ENOMEM;

	si->corr_peb_count += 1;
	seb->pnum = pnum;
	seb->ec = ec;
	list_add(&seb->u.list, &si->corr);
	return 0;
}

static int validate_vid_hdr(const struct ubi_vid_hdr *vid_hdr,
			    const struct ubi_scan_volume *sv, int pnum)
{
	int vol_type = vid_hdr->vol_type;
	int vol_id = be32_to_cpu(vid_hdr->vol_id);
	int used_ebs = be32_to_cpu(vid_hdr->used_ebs);
	int data_pad = be32_to_cpu(vid_hdr->data_pad);

	if (sv->leb_count != 0) {
		int sv_vol_type;


		if (vol_id != sv->vol_id) {
			dbg_err("inconsistent vol_id");
			goto bad;
		}

		if (sv->vol_type == UBI_STATIC_VOLUME)
			sv_vol_type = UBI_VID_STATIC;
		else
			sv_vol_type = UBI_VID_DYNAMIC;

		if (vol_type != sv_vol_type) {
			dbg_err("inconsistent vol_type");
			goto bad;
		}

		if (used_ebs != sv->used_ebs) {
			dbg_err("inconsistent used_ebs");
			goto bad;
		}

		if (data_pad != sv->data_pad) {
			dbg_err("inconsistent data_pad");
			goto bad;
		}
	}

	return 0;

bad:
	ubi_err("inconsistent VID header at PEB %d", pnum);
	ubi_dbg_dump_vid_hdr(vid_hdr);
	ubi_dbg_dump_sv(sv);
	return -EINVAL;
}

static struct ubi_scan_volume *add_volume(struct ubi_scan_info *si, int vol_id,
					  int pnum,
					  const struct ubi_vid_hdr *vid_hdr)
{
	struct ubi_scan_volume *sv;
	struct rb_node **p = &si->volumes.rb_node, *parent = NULL;

	ubi_assert(vol_id == be32_to_cpu(vid_hdr->vol_id));

	
	while (*p) {
		parent = *p;
		sv = rb_entry(parent, struct ubi_scan_volume, rb);

		if (vol_id == sv->vol_id)
			return sv;

		if (vol_id > sv->vol_id)
			p = &(*p)->rb_left;
		else
			p = &(*p)->rb_right;
	}

	
	sv = kmalloc(sizeof(struct ubi_scan_volume), GFP_KERNEL);
	if (!sv)
		return ERR_PTR(-ENOMEM);

	sv->highest_lnum = sv->leb_count = 0;
	sv->vol_id = vol_id;
	sv->root = RB_ROOT;
	sv->used_ebs = be32_to_cpu(vid_hdr->used_ebs);
	sv->data_pad = be32_to_cpu(vid_hdr->data_pad);
	sv->compat = vid_hdr->compat;
	sv->vol_type = vid_hdr->vol_type == UBI_VID_DYNAMIC ? UBI_DYNAMIC_VOLUME
							    : UBI_STATIC_VOLUME;
	if (vol_id > si->highest_vol_id)
		si->highest_vol_id = vol_id;

	rb_link_node(&sv->rb, parent, p);
	rb_insert_color(&sv->rb, &si->volumes);
	si->vols_found += 1;
	dbg_bld("added volume %d", vol_id);
	return sv;
}

static int compare_lebs(struct ubi_device *ubi, const struct ubi_scan_leb *seb,
			int pnum, const struct ubi_vid_hdr *vid_hdr)
{
	void *buf;
	int len, err, second_is_newer, bitflips = 0, corrupted = 0;
	uint32_t data_crc, crc;
	struct ubi_vid_hdr *vh = NULL;
	unsigned long long sqnum2 = be64_to_cpu(vid_hdr->sqnum);

	if (sqnum2 == seb->sqnum) {
		ubi_err("unsupported on-flash UBI format\n");
		return -EINVAL;
	}

	
	second_is_newer = !!(sqnum2 > seb->sqnum);


	if (second_is_newer) {
		if (!vid_hdr->copy_flag) {
			
			dbg_bld("second PEB %d is newer, copy_flag is unset",
				pnum);
			return 1;
		}
	} else {
		if (!seb->copy_flag) {
			
			dbg_bld("first PEB %d is newer, copy_flag is unset",
				pnum);
			return bitflips << 1;
		}

		vh = ubi_zalloc_vid_hdr(ubi, GFP_KERNEL);
		if (!vh)
			return -ENOMEM;

		pnum = seb->pnum;
		err = ubi_io_read_vid_hdr(ubi, pnum, vh, 0);
		if (err) {
			if (err == UBI_IO_BITFLIPS)
				bitflips = 1;
			else {
				dbg_err("VID of PEB %d header is bad, but it "
					"was OK earlier, err %d", pnum, err);
				if (err > 0)
					err = -EIO;

				goto out_free_vidh;
			}
		}

		vid_hdr = vh;
	}

	

	len = be32_to_cpu(vid_hdr->data_size);
	buf = vmalloc(len);
	if (!buf) {
		err = -ENOMEM;
		goto out_free_vidh;
	}

	err = ubi_io_read_data(ubi, buf, pnum, 0, len);
	if (err && err != UBI_IO_BITFLIPS && !mtd_is_eccerr(err))
		goto out_free_buf;

	data_crc = be32_to_cpu(vid_hdr->data_crc);
	crc = crc32(UBI_CRC32_INIT, buf, len);
	if (crc != data_crc) {
		dbg_bld("PEB %d CRC error: calculated %#08x, must be %#08x",
			pnum, crc, data_crc);
		corrupted = 1;
		bitflips = 0;
		second_is_newer = !second_is_newer;
	} else {
		dbg_bld("PEB %d CRC is OK", pnum);
		bitflips = !!err;
	}

	vfree(buf);
	ubi_free_vid_hdr(ubi, vh);

	if (second_is_newer)
		dbg_bld("second PEB %d is newer, copy_flag is set", pnum);
	else
		dbg_bld("first PEB %d is newer, copy_flag is set", pnum);

	return second_is_newer | (bitflips << 1) | (corrupted << 2);

out_free_buf:
	vfree(buf);
out_free_vidh:
	ubi_free_vid_hdr(ubi, vh);
	return err;
}

int ubi_scan_add_used(struct ubi_device *ubi, struct ubi_scan_info *si,
		      int pnum, int ec, const struct ubi_vid_hdr *vid_hdr,
		      int bitflips)
{
	int err, vol_id, lnum;
	unsigned long long sqnum;
	struct ubi_scan_volume *sv;
	struct ubi_scan_leb *seb;
	struct rb_node **p, *parent = NULL;

	vol_id = be32_to_cpu(vid_hdr->vol_id);
	lnum = be32_to_cpu(vid_hdr->lnum);
	sqnum = be64_to_cpu(vid_hdr->sqnum);

	dbg_bld("PEB %d, LEB %d:%d, EC %d, sqnum %llu, bitflips %d",
		pnum, vol_id, lnum, ec, sqnum, bitflips);

	sv = add_volume(si, vol_id, pnum, vid_hdr);
	if (IS_ERR(sv))
		return PTR_ERR(sv);

	if (si->max_sqnum < sqnum)
		si->max_sqnum = sqnum;

	p = &sv->root.rb_node;
	while (*p) {
		int cmp_res;

		parent = *p;
		seb = rb_entry(parent, struct ubi_scan_leb, u.rb);
		if (lnum != seb->lnum) {
			if (lnum < seb->lnum)
				p = &(*p)->rb_left;
			else
				p = &(*p)->rb_right;
			continue;
		}


		dbg_bld("this LEB already exists: PEB %d, sqnum %llu, "
			"EC %d", seb->pnum, seb->sqnum, seb->ec);

		if (seb->sqnum == sqnum && sqnum != 0) {
			ubi_err("two LEBs with same sequence number %llu",
				sqnum);
			ubi_dbg_dump_seb(seb, 0);
			ubi_dbg_dump_vid_hdr(vid_hdr);
			return -EINVAL;
		}

		cmp_res = compare_lebs(ubi, seb, pnum, vid_hdr);
		if (cmp_res < 0)
			return cmp_res;

		if (cmp_res & 1) {
			err = validate_vid_hdr(vid_hdr, sv, pnum);
			if (err)
				return err;

			err = add_to_list(si, seb->pnum, seb->ec, cmp_res & 4,
					  &si->erase);
			if (err)
				return err;

			seb->ec = ec;
			seb->pnum = pnum;
			seb->scrub = ((cmp_res & 2) || bitflips);
			seb->copy_flag = vid_hdr->copy_flag;
			seb->sqnum = sqnum;

			if (sv->highest_lnum == lnum)
				sv->last_data_size =
					be32_to_cpu(vid_hdr->data_size);

			return 0;
		} else {
			return add_to_list(si, pnum, ec, cmp_res & 4,
					   &si->erase);
		}
	}


	err = validate_vid_hdr(vid_hdr, sv, pnum);
	if (err)
		return err;

	seb = kmem_cache_alloc(si->scan_leb_slab, GFP_KERNEL);
	if (!seb)
		return -ENOMEM;

	seb->ec = ec;
	seb->pnum = pnum;
	seb->lnum = lnum;
	seb->scrub = bitflips;
	seb->copy_flag = vid_hdr->copy_flag;
	seb->sqnum = sqnum;

	if (sv->highest_lnum <= lnum) {
		sv->highest_lnum = lnum;
		sv->last_data_size = be32_to_cpu(vid_hdr->data_size);
	}

	sv->leb_count += 1;
	rb_link_node(&seb->u.rb, parent, p);
	rb_insert_color(&seb->u.rb, &sv->root);
	return 0;
}

struct ubi_scan_volume *ubi_scan_find_sv(const struct ubi_scan_info *si,
					 int vol_id)
{
	struct ubi_scan_volume *sv;
	struct rb_node *p = si->volumes.rb_node;

	while (p) {
		sv = rb_entry(p, struct ubi_scan_volume, rb);

		if (vol_id == sv->vol_id)
			return sv;

		if (vol_id > sv->vol_id)
			p = p->rb_left;
		else
			p = p->rb_right;
	}

	return NULL;
}

struct ubi_scan_leb *ubi_scan_find_seb(const struct ubi_scan_volume *sv,
				       int lnum)
{
	struct ubi_scan_leb *seb;
	struct rb_node *p = sv->root.rb_node;

	while (p) {
		seb = rb_entry(p, struct ubi_scan_leb, u.rb);

		if (lnum == seb->lnum)
			return seb;

		if (lnum > seb->lnum)
			p = p->rb_left;
		else
			p = p->rb_right;
	}

	return NULL;
}

void ubi_scan_rm_volume(struct ubi_scan_info *si, struct ubi_scan_volume *sv)
{
	struct rb_node *rb;
	struct ubi_scan_leb *seb;

	dbg_bld("remove scanning information about volume %d", sv->vol_id);

	while ((rb = rb_first(&sv->root))) {
		seb = rb_entry(rb, struct ubi_scan_leb, u.rb);
		rb_erase(&seb->u.rb, &sv->root);
		list_add_tail(&seb->u.list, &si->erase);
	}

	rb_erase(&sv->rb, &si->volumes);
	kfree(sv);
	si->vols_found -= 1;
}

int ubi_scan_erase_peb(struct ubi_device *ubi, const struct ubi_scan_info *si,
		       int pnum, int ec)
{
	int err;
	struct ubi_ec_hdr *ec_hdr;

	if ((long long)ec >= UBI_MAX_ERASECOUNTER) {
		ubi_err("erase counter overflow at PEB %d, EC %d", pnum, ec);
		return -EINVAL;
	}

	ec_hdr = kzalloc(ubi->ec_hdr_alsize, GFP_KERNEL);
	if (!ec_hdr)
		return -ENOMEM;

	ec_hdr->ec = cpu_to_be64(ec);

	err = ubi_io_sync_erase(ubi, pnum, 0);
	if (err < 0)
		goto out_free;

	err = ubi_io_write_ec_hdr(ubi, pnum, ec_hdr);

out_free:
	kfree(ec_hdr);
	return err;
}

struct ubi_scan_leb *ubi_scan_get_free_peb(struct ubi_device *ubi,
					   struct ubi_scan_info *si)
{
	int err = 0;
	struct ubi_scan_leb *seb, *tmp_seb;

	if (!list_empty(&si->free)) {
		seb = list_entry(si->free.next, struct ubi_scan_leb, u.list);
		list_del(&seb->u.list);
		dbg_bld("return free PEB %d, EC %d", seb->pnum, seb->ec);
		return seb;
	}

	list_for_each_entry_safe(seb, tmp_seb, &si->erase, u.list) {
		if (seb->ec == UBI_SCAN_UNKNOWN_EC)
			seb->ec = si->mean_ec;

		err = ubi_scan_erase_peb(ubi, si, seb->pnum, seb->ec+1);
		if (err)
			continue;

		seb->ec += 1;
		list_del(&seb->u.list);
		dbg_bld("return PEB %d, EC %d", seb->pnum, seb->ec);
		return seb;
	}

	ubi_err("no free eraseblocks");
	return ERR_PTR(-ENOSPC);
}

static int check_corruption(struct ubi_device *ubi, struct ubi_vid_hdr *vid_hdr,
			    int pnum)
{
	int err;

	mutex_lock(&ubi->buf_mutex);
	memset(ubi->peb_buf, 0x00, ubi->leb_size);

	err = ubi_io_read(ubi, ubi->peb_buf, pnum, ubi->leb_start,
			  ubi->leb_size);
	if (err == UBI_IO_BITFLIPS || mtd_is_eccerr(err)) {
		err = 0;
		goto out_unlock;
	}

	if (err)
		goto out_unlock;

	if (ubi_check_pattern(ubi->peb_buf, 0xFF, ubi->leb_size))
		goto out_unlock;

	ubi_err("PEB %d contains corrupted VID header, and the data does not "
		"contain all 0xFF, this may be a non-UBI PEB or a severe VID "
		"header corruption which requires manual inspection", pnum);
	ubi_dbg_dump_vid_hdr(vid_hdr);
	dbg_msg("hexdump of PEB %d offset %d, length %d",
		pnum, ubi->leb_start, ubi->leb_size);
	ubi_dbg_print_hex_dump(KERN_DEBUG, "", DUMP_PREFIX_OFFSET, 32, 1,
			       ubi->peb_buf, ubi->leb_size, 1);
	err = 1;

out_unlock:
	mutex_unlock(&ubi->buf_mutex);
	return err;
}

static int process_eb(struct ubi_device *ubi, struct ubi_scan_info *si,
		      int pnum)
{
	long long uninitialized_var(ec);
	int err, bitflips = 0, vol_id, ec_err = 0;

	dbg_bld("scan PEB %d", pnum);

	
	err = ubi_io_is_bad(ubi, pnum);
	if (err < 0)
		return err;
	else if (err) {
		si->bad_peb_count += 1;
		return 0;
	}

	err = ubi_io_read_ec_hdr(ubi, pnum, ech, 0);
	if (err < 0)
		return err;
	switch (err) {
	case 0:
		break;
	case UBI_IO_BITFLIPS:
		bitflips = 1;
		break;
	case UBI_IO_FF:
		si->empty_peb_count += 1;
		return add_to_list(si, pnum, UBI_SCAN_UNKNOWN_EC, 0,
				   &si->erase);
	case UBI_IO_FF_BITFLIPS:
		si->empty_peb_count += 1;
		return add_to_list(si, pnum, UBI_SCAN_UNKNOWN_EC, 1,
				   &si->erase);
	case UBI_IO_BAD_HDR_EBADMSG:
	case UBI_IO_BAD_HDR:
		ec_err = err;
		ec = UBI_SCAN_UNKNOWN_EC;
		bitflips = 1;
		break;
	default:
		ubi_err("'ubi_io_read_ec_hdr()' returned unknown code %d", err);
		return -EINVAL;
	}

	if (!ec_err) {
		int image_seq;

		
		if (ech->version != UBI_VERSION) {
			ubi_err("this UBI version is %d, image version is %d",
				UBI_VERSION, (int)ech->version);
			return -EINVAL;
		}

		ec = be64_to_cpu(ech->ec);
		if (ec > UBI_MAX_ERASECOUNTER) {
			ubi_err("erase counter overflow, max is %d",
				UBI_MAX_ERASECOUNTER);
			ubi_dbg_dump_ec_hdr(ech);
			return -EINVAL;
		}

		image_seq = be32_to_cpu(ech->image_seq);
		if (!ubi->image_seq && image_seq)
			ubi->image_seq = image_seq;
		if (ubi->image_seq && image_seq &&
		    ubi->image_seq != image_seq) {
			ubi_err("bad image sequence number %d in PEB %d, "
				"expected %d", image_seq, pnum, ubi->image_seq);
			ubi_dbg_dump_ec_hdr(ech);
			return -EINVAL;
		}
	}

	

	err = ubi_io_read_vid_hdr(ubi, pnum, vidh, 0);
	if (err < 0)
		return err;
	switch (err) {
	case 0:
		break;
	case UBI_IO_BITFLIPS:
		bitflips = 1;
		break;
	case UBI_IO_BAD_HDR_EBADMSG:
		if (ec_err == UBI_IO_BAD_HDR_EBADMSG)
			si->maybe_bad_peb_count += 1;
	case UBI_IO_BAD_HDR:
		if (ec_err)
			err = 0;
		else
			err = check_corruption(ubi, vidh, pnum);

		if (err < 0)
			return err;
		else if (!err)
			
			err = add_to_list(si, pnum, ec, 1, &si->erase);
		else
			
			err = add_corrupted(si, pnum, ec);
		if (err)
			return err;
		goto adjust_mean_ec;
	case UBI_IO_FF_BITFLIPS:
		err = add_to_list(si, pnum, ec, 1, &si->erase);
		if (err)
			return err;
		goto adjust_mean_ec;
	case UBI_IO_FF:
		if (ec_err)
			err = add_to_list(si, pnum, ec, 1, &si->erase);
		else
			err = add_to_list(si, pnum, ec, 0, &si->free);
		if (err)
			return err;
		goto adjust_mean_ec;
	default:
		ubi_err("'ubi_io_read_vid_hdr()' returned unknown code %d",
			err);
		return -EINVAL;
	}

	vol_id = be32_to_cpu(vidh->vol_id);
	if (vol_id > UBI_MAX_VOLUMES && vol_id != UBI_LAYOUT_VOLUME_ID) {
		int lnum = be32_to_cpu(vidh->lnum);

		
		switch (vidh->compat) {
		case UBI_COMPAT_DELETE:
			ubi_msg("\"delete\" compatible internal volume %d:%d"
				" found, will remove it", vol_id, lnum);
			err = add_to_list(si, pnum, ec, 1, &si->erase);
			if (err)
				return err;
			return 0;

		case UBI_COMPAT_RO:
			ubi_msg("read-only compatible internal volume %d:%d"
				" found, switch to read-only mode",
				vol_id, lnum);
			ubi->ro_mode = 1;
			break;

		case UBI_COMPAT_PRESERVE:
			ubi_msg("\"preserve\" compatible internal volume %d:%d"
				" found", vol_id, lnum);
			err = add_to_list(si, pnum, ec, 0, &si->alien);
			if (err)
				return err;
			return 0;

		case UBI_COMPAT_REJECT:
			ubi_err("incompatible internal volume %d:%d found",
				vol_id, lnum);
			return -EINVAL;
		}
	}

	if (ec_err)
		ubi_warn("valid VID header but corrupted EC header at PEB %d",
			 pnum);
	err = ubi_scan_add_used(ubi, si, pnum, ec, vidh, bitflips);
	if (err)
		return err;

adjust_mean_ec:
	if (!ec_err) {
		si->ec_sum += ec;
		si->ec_count += 1;
		if (ec > si->max_ec)
			si->max_ec = ec;
		if (ec < si->min_ec)
			si->min_ec = ec;
	}

	return 0;
}

static int check_what_we_have(struct ubi_device *ubi, struct ubi_scan_info *si)
{
	struct ubi_scan_leb *seb;
	int max_corr, peb_count;

	peb_count = ubi->peb_count - si->bad_peb_count - si->alien_peb_count;
	max_corr = peb_count / 20 ?: 8;

	if (si->corr_peb_count) {
		ubi_err("%d PEBs are corrupted and preserved",
			si->corr_peb_count);
		printk(KERN_ERR "Corrupted PEBs are:");
		list_for_each_entry(seb, &si->corr, u.list)
			printk(KERN_CONT " %d", seb->pnum);
		printk(KERN_CONT "\n");

		if (si->corr_peb_count >= max_corr) {
			ubi_err("too many corrupted PEBs, refusing");
			return -EINVAL;
		}
	}

	if (si->empty_peb_count + si->maybe_bad_peb_count == peb_count) {
		if (si->maybe_bad_peb_count <= 2) {
			si->is_empty = 1;
			ubi_msg("empty MTD device detected");
			get_random_bytes(&ubi->image_seq,
					 sizeof(ubi->image_seq));
		} else {
			ubi_err("MTD device is not UBI-formatted and possibly "
				"contains non-UBI data - refusing it");
			return -EINVAL;
		}

	}

	return 0;
}

struct ubi_scan_info *ubi_scan(struct ubi_device *ubi)
{
	int err, pnum;
	struct rb_node *rb1, *rb2;
	struct ubi_scan_volume *sv;
	struct ubi_scan_leb *seb;
	struct ubi_scan_info *si;

	si = kzalloc(sizeof(struct ubi_scan_info), GFP_KERNEL);
	if (!si)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&si->corr);
	INIT_LIST_HEAD(&si->free);
	INIT_LIST_HEAD(&si->erase);
	INIT_LIST_HEAD(&si->alien);
	si->volumes = RB_ROOT;

	err = -ENOMEM;
	si->scan_leb_slab = kmem_cache_create("ubi_scan_leb_slab",
					      sizeof(struct ubi_scan_leb),
					      0, 0, NULL);
	if (!si->scan_leb_slab)
		goto out_si;

	ech = kzalloc(ubi->ec_hdr_alsize, GFP_KERNEL);
	if (!ech)
		goto out_si;

	vidh = ubi_zalloc_vid_hdr(ubi, GFP_KERNEL);
	if (!vidh)
		goto out_ech;

	for (pnum = 0; pnum < ubi->peb_count; pnum++) {
		cond_resched();

		dbg_gen("process PEB %d", pnum);
		err = process_eb(ubi, si, pnum);
		if (err < 0)
			goto out_vidh;
	}

	dbg_msg("scanning is finished");

	
	if (si->ec_count)
		si->mean_ec = div_u64(si->ec_sum, si->ec_count);

	err = check_what_we_have(ubi, si);
	if (err)
		goto out_vidh;

	ubi_rb_for_each_entry(rb1, sv, &si->volumes, rb) {
		ubi_rb_for_each_entry(rb2, seb, &sv->root, u.rb)
			if (seb->ec == UBI_SCAN_UNKNOWN_EC)
				seb->ec = si->mean_ec;
	}

	list_for_each_entry(seb, &si->free, u.list) {
		if (seb->ec == UBI_SCAN_UNKNOWN_EC)
			seb->ec = si->mean_ec;
	}

	list_for_each_entry(seb, &si->corr, u.list)
		if (seb->ec == UBI_SCAN_UNKNOWN_EC)
			seb->ec = si->mean_ec;

	list_for_each_entry(seb, &si->erase, u.list)
		if (seb->ec == UBI_SCAN_UNKNOWN_EC)
			seb->ec = si->mean_ec;

	err = paranoid_check_si(ubi, si);
	if (err)
		goto out_vidh;

	ubi_free_vid_hdr(ubi, vidh);
	kfree(ech);

	return si;

out_vidh:
	ubi_free_vid_hdr(ubi, vidh);
out_ech:
	kfree(ech);
out_si:
	ubi_scan_destroy_si(si);
	return ERR_PTR(err);
}

static void destroy_sv(struct ubi_scan_info *si, struct ubi_scan_volume *sv)
{
	struct ubi_scan_leb *seb;
	struct rb_node *this = sv->root.rb_node;

	while (this) {
		if (this->rb_left)
			this = this->rb_left;
		else if (this->rb_right)
			this = this->rb_right;
		else {
			seb = rb_entry(this, struct ubi_scan_leb, u.rb);
			this = rb_parent(this);
			if (this) {
				if (this->rb_left == &seb->u.rb)
					this->rb_left = NULL;
				else
					this->rb_right = NULL;
			}

			kmem_cache_free(si->scan_leb_slab, seb);
		}
	}
	kfree(sv);
}

void ubi_scan_destroy_si(struct ubi_scan_info *si)
{
	struct ubi_scan_leb *seb, *seb_tmp;
	struct ubi_scan_volume *sv;
	struct rb_node *rb;

	list_for_each_entry_safe(seb, seb_tmp, &si->alien, u.list) {
		list_del(&seb->u.list);
		kmem_cache_free(si->scan_leb_slab, seb);
	}
	list_for_each_entry_safe(seb, seb_tmp, &si->erase, u.list) {
		list_del(&seb->u.list);
		kmem_cache_free(si->scan_leb_slab, seb);
	}
	list_for_each_entry_safe(seb, seb_tmp, &si->corr, u.list) {
		list_del(&seb->u.list);
		kmem_cache_free(si->scan_leb_slab, seb);
	}
	list_for_each_entry_safe(seb, seb_tmp, &si->free, u.list) {
		list_del(&seb->u.list);
		kmem_cache_free(si->scan_leb_slab, seb);
	}

	
	rb = si->volumes.rb_node;
	while (rb) {
		if (rb->rb_left)
			rb = rb->rb_left;
		else if (rb->rb_right)
			rb = rb->rb_right;
		else {
			sv = rb_entry(rb, struct ubi_scan_volume, rb);

			rb = rb_parent(rb);
			if (rb) {
				if (rb->rb_left == &sv->rb)
					rb->rb_left = NULL;
				else
					rb->rb_right = NULL;
			}

			destroy_sv(si, sv);
		}
	}

	if (si->scan_leb_slab)
		kmem_cache_destroy(si->scan_leb_slab);

	kfree(si);
}

#ifdef CONFIG_MTD_UBI_DEBUG

static int paranoid_check_si(struct ubi_device *ubi, struct ubi_scan_info *si)
{
	int pnum, err, vols_found = 0;
	struct rb_node *rb1, *rb2;
	struct ubi_scan_volume *sv;
	struct ubi_scan_leb *seb, *last_seb;
	uint8_t *buf;

	if (!ubi->dbg->chk_gen)
		return 0;

	ubi_rb_for_each_entry(rb1, sv, &si->volumes, rb) {
		int leb_count = 0;

		cond_resched();

		vols_found += 1;

		if (si->is_empty) {
			ubi_err("bad is_empty flag");
			goto bad_sv;
		}

		if (sv->vol_id < 0 || sv->highest_lnum < 0 ||
		    sv->leb_count < 0 || sv->vol_type < 0 || sv->used_ebs < 0 ||
		    sv->data_pad < 0 || sv->last_data_size < 0) {
			ubi_err("negative values");
			goto bad_sv;
		}

		if (sv->vol_id >= UBI_MAX_VOLUMES &&
		    sv->vol_id < UBI_INTERNAL_VOL_START) {
			ubi_err("bad vol_id");
			goto bad_sv;
		}

		if (sv->vol_id > si->highest_vol_id) {
			ubi_err("highest_vol_id is %d, but vol_id %d is there",
				si->highest_vol_id, sv->vol_id);
			goto out;
		}

		if (sv->vol_type != UBI_DYNAMIC_VOLUME &&
		    sv->vol_type != UBI_STATIC_VOLUME) {
			ubi_err("bad vol_type");
			goto bad_sv;
		}

		if (sv->data_pad > ubi->leb_size / 2) {
			ubi_err("bad data_pad");
			goto bad_sv;
		}

		last_seb = NULL;
		ubi_rb_for_each_entry(rb2, seb, &sv->root, u.rb) {
			cond_resched();

			last_seb = seb;
			leb_count += 1;

			if (seb->pnum < 0 || seb->ec < 0) {
				ubi_err("negative values");
				goto bad_seb;
			}

			if (seb->ec < si->min_ec) {
				ubi_err("bad si->min_ec (%d), %d found",
					si->min_ec, seb->ec);
				goto bad_seb;
			}

			if (seb->ec > si->max_ec) {
				ubi_err("bad si->max_ec (%d), %d found",
					si->max_ec, seb->ec);
				goto bad_seb;
			}

			if (seb->pnum >= ubi->peb_count) {
				ubi_err("too high PEB number %d, total PEBs %d",
					seb->pnum, ubi->peb_count);
				goto bad_seb;
			}

			if (sv->vol_type == UBI_STATIC_VOLUME) {
				if (seb->lnum >= sv->used_ebs) {
					ubi_err("bad lnum or used_ebs");
					goto bad_seb;
				}
			} else {
				if (sv->used_ebs != 0) {
					ubi_err("non-zero used_ebs");
					goto bad_seb;
				}
			}

			if (seb->lnum > sv->highest_lnum) {
				ubi_err("incorrect highest_lnum or lnum");
				goto bad_seb;
			}
		}

		if (sv->leb_count != leb_count) {
			ubi_err("bad leb_count, %d objects in the tree",
				leb_count);
			goto bad_sv;
		}

		if (!last_seb)
			continue;

		seb = last_seb;

		if (seb->lnum != sv->highest_lnum) {
			ubi_err("bad highest_lnum");
			goto bad_seb;
		}
	}

	if (vols_found != si->vols_found) {
		ubi_err("bad si->vols_found %d, should be %d",
			si->vols_found, vols_found);
		goto out;
	}

	
	ubi_rb_for_each_entry(rb1, sv, &si->volumes, rb) {
		last_seb = NULL;
		ubi_rb_for_each_entry(rb2, seb, &sv->root, u.rb) {
			int vol_type;

			cond_resched();

			last_seb = seb;

			err = ubi_io_read_vid_hdr(ubi, seb->pnum, vidh, 1);
			if (err && err != UBI_IO_BITFLIPS) {
				ubi_err("VID header is not OK (%d)", err);
				if (err > 0)
					err = -EIO;
				return err;
			}

			vol_type = vidh->vol_type == UBI_VID_DYNAMIC ?
				   UBI_DYNAMIC_VOLUME : UBI_STATIC_VOLUME;
			if (sv->vol_type != vol_type) {
				ubi_err("bad vol_type");
				goto bad_vid_hdr;
			}

			if (seb->sqnum != be64_to_cpu(vidh->sqnum)) {
				ubi_err("bad sqnum %llu", seb->sqnum);
				goto bad_vid_hdr;
			}

			if (sv->vol_id != be32_to_cpu(vidh->vol_id)) {
				ubi_err("bad vol_id %d", sv->vol_id);
				goto bad_vid_hdr;
			}

			if (sv->compat != vidh->compat) {
				ubi_err("bad compat %d", vidh->compat);
				goto bad_vid_hdr;
			}

			if (seb->lnum != be32_to_cpu(vidh->lnum)) {
				ubi_err("bad lnum %d", seb->lnum);
				goto bad_vid_hdr;
			}

			if (sv->used_ebs != be32_to_cpu(vidh->used_ebs)) {
				ubi_err("bad used_ebs %d", sv->used_ebs);
				goto bad_vid_hdr;
			}

			if (sv->data_pad != be32_to_cpu(vidh->data_pad)) {
				ubi_err("bad data_pad %d", sv->data_pad);
				goto bad_vid_hdr;
			}
		}

		if (!last_seb)
			continue;

		if (sv->highest_lnum != be32_to_cpu(vidh->lnum)) {
			ubi_err("bad highest_lnum %d", sv->highest_lnum);
			goto bad_vid_hdr;
		}

		if (sv->last_data_size != be32_to_cpu(vidh->data_size)) {
			ubi_err("bad last_data_size %d", sv->last_data_size);
			goto bad_vid_hdr;
		}
	}

	buf = kzalloc(ubi->peb_count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	for (pnum = 0; pnum < ubi->peb_count; pnum++) {
		err = ubi_io_is_bad(ubi, pnum);
		if (err < 0) {
			kfree(buf);
			return err;
		} else if (err)
			buf[pnum] = 1;
	}

	ubi_rb_for_each_entry(rb1, sv, &si->volumes, rb)
		ubi_rb_for_each_entry(rb2, seb, &sv->root, u.rb)
			buf[seb->pnum] = 1;

	list_for_each_entry(seb, &si->free, u.list)
		buf[seb->pnum] = 1;

	list_for_each_entry(seb, &si->corr, u.list)
		buf[seb->pnum] = 1;

	list_for_each_entry(seb, &si->erase, u.list)
		buf[seb->pnum] = 1;

	list_for_each_entry(seb, &si->alien, u.list)
		buf[seb->pnum] = 1;

	err = 0;
	for (pnum = 0; pnum < ubi->peb_count; pnum++)
		if (!buf[pnum]) {
			ubi_err("PEB %d is not referred", pnum);
			err = 1;
		}

	kfree(buf);
	if (err)
		goto out;
	return 0;

bad_seb:
	ubi_err("bad scanning information about LEB %d", seb->lnum);
	ubi_dbg_dump_seb(seb, 0);
	ubi_dbg_dump_sv(sv);
	goto out;

bad_sv:
	ubi_err("bad scanning information about volume %d", sv->vol_id);
	ubi_dbg_dump_sv(sv);
	goto out;

bad_vid_hdr:
	ubi_err("bad scanning information about volume %d", sv->vol_id);
	ubi_dbg_dump_sv(sv);
	ubi_dbg_dump_vid_hdr(vidh);

out:
	ubi_dbg_dump_stack();
	return -EINVAL;
}

#endif 
