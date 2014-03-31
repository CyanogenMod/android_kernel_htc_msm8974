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


#include <linux/slab.h>
#include <linux/crc32.h>
#include <linux/err.h>
#include "ubi.h"

#define EBA_RESERVED_PEBS 1

static unsigned long long next_sqnum(struct ubi_device *ubi)
{
	unsigned long long sqnum;

	spin_lock(&ubi->ltree_lock);
	sqnum = ubi->global_sqnum++;
	spin_unlock(&ubi->ltree_lock);

	return sqnum;
}

static int ubi_get_compat(const struct ubi_device *ubi, int vol_id)
{
	if (vol_id == UBI_LAYOUT_VOLUME_ID)
		return UBI_LAYOUT_VOLUME_COMPAT;
	return 0;
}

static struct ubi_ltree_entry *ltree_lookup(struct ubi_device *ubi, int vol_id,
					    int lnum)
{
	struct rb_node *p;

	p = ubi->ltree.rb_node;
	while (p) {
		struct ubi_ltree_entry *le;

		le = rb_entry(p, struct ubi_ltree_entry, rb);

		if (vol_id < le->vol_id)
			p = p->rb_left;
		else if (vol_id > le->vol_id)
			p = p->rb_right;
		else {
			if (lnum < le->lnum)
				p = p->rb_left;
			else if (lnum > le->lnum)
				p = p->rb_right;
			else
				return le;
		}
	}

	return NULL;
}

static struct ubi_ltree_entry *ltree_add_entry(struct ubi_device *ubi,
					       int vol_id, int lnum)
{
	struct ubi_ltree_entry *le, *le1, *le_free;

	le = kmalloc(sizeof(struct ubi_ltree_entry), GFP_NOFS);
	if (!le)
		return ERR_PTR(-ENOMEM);

	le->users = 0;
	init_rwsem(&le->mutex);
	le->vol_id = vol_id;
	le->lnum = lnum;

	spin_lock(&ubi->ltree_lock);
	le1 = ltree_lookup(ubi, vol_id, lnum);

	if (le1) {
		le_free = le;
		le = le1;
	} else {
		struct rb_node **p, *parent = NULL;

		le_free = NULL;

		p = &ubi->ltree.rb_node;
		while (*p) {
			parent = *p;
			le1 = rb_entry(parent, struct ubi_ltree_entry, rb);

			if (vol_id < le1->vol_id)
				p = &(*p)->rb_left;
			else if (vol_id > le1->vol_id)
				p = &(*p)->rb_right;
			else {
				ubi_assert(lnum != le1->lnum);
				if (lnum < le1->lnum)
					p = &(*p)->rb_left;
				else
					p = &(*p)->rb_right;
			}
		}

		rb_link_node(&le->rb, parent, p);
		rb_insert_color(&le->rb, &ubi->ltree);
	}
	le->users += 1;
	spin_unlock(&ubi->ltree_lock);

	kfree(le_free);
	return le;
}

static int leb_read_lock(struct ubi_device *ubi, int vol_id, int lnum)
{
	struct ubi_ltree_entry *le;

	le = ltree_add_entry(ubi, vol_id, lnum);
	if (IS_ERR(le))
		return PTR_ERR(le);
	down_read(&le->mutex);
	return 0;
}

static void leb_read_unlock(struct ubi_device *ubi, int vol_id, int lnum)
{
	struct ubi_ltree_entry *le;

	spin_lock(&ubi->ltree_lock);
	le = ltree_lookup(ubi, vol_id, lnum);
	le->users -= 1;
	ubi_assert(le->users >= 0);
	up_read(&le->mutex);
	if (le->users == 0) {
		rb_erase(&le->rb, &ubi->ltree);
		kfree(le);
	}
	spin_unlock(&ubi->ltree_lock);
}

static int leb_write_lock(struct ubi_device *ubi, int vol_id, int lnum)
{
	struct ubi_ltree_entry *le;

	le = ltree_add_entry(ubi, vol_id, lnum);
	if (IS_ERR(le))
		return PTR_ERR(le);
	down_write(&le->mutex);
	return 0;
}

static int leb_write_trylock(struct ubi_device *ubi, int vol_id, int lnum)
{
	struct ubi_ltree_entry *le;

	le = ltree_add_entry(ubi, vol_id, lnum);
	if (IS_ERR(le))
		return PTR_ERR(le);
	if (down_write_trylock(&le->mutex))
		return 0;

	
	spin_lock(&ubi->ltree_lock);
	le->users -= 1;
	ubi_assert(le->users >= 0);
	if (le->users == 0) {
		rb_erase(&le->rb, &ubi->ltree);
		kfree(le);
	}
	spin_unlock(&ubi->ltree_lock);

	return 1;
}

static void leb_write_unlock(struct ubi_device *ubi, int vol_id, int lnum)
{
	struct ubi_ltree_entry *le;

	spin_lock(&ubi->ltree_lock);
	le = ltree_lookup(ubi, vol_id, lnum);
	le->users -= 1;
	ubi_assert(le->users >= 0);
	up_write(&le->mutex);
	if (le->users == 0) {
		rb_erase(&le->rb, &ubi->ltree);
		kfree(le);
	}
	spin_unlock(&ubi->ltree_lock);
}

int ubi_eba_unmap_leb(struct ubi_device *ubi, struct ubi_volume *vol,
		      int lnum)
{
	int err, pnum, vol_id = vol->vol_id;

	if (ubi->ro_mode)
		return -EROFS;

	err = leb_write_lock(ubi, vol_id, lnum);
	if (err)
		return err;

	pnum = vol->eba_tbl[lnum];
	if (pnum < 0)
		
		goto out_unlock;

	dbg_eba("erase LEB %d:%d, PEB %d", vol_id, lnum, pnum);

	vol->eba_tbl[lnum] = UBI_LEB_UNMAPPED;
	err = ubi_wl_put_peb(ubi, pnum, 0);

out_unlock:
	leb_write_unlock(ubi, vol_id, lnum);
	return err;
}

int ubi_eba_read_leb(struct ubi_device *ubi, struct ubi_volume *vol, int lnum,
		     void *buf, int offset, int len, int check)
{
	int err, pnum, scrub = 0, vol_id = vol->vol_id;
	struct ubi_vid_hdr *vid_hdr;
	uint32_t uninitialized_var(crc);

	err = leb_read_lock(ubi, vol_id, lnum);
	if (err)
		return err;

	pnum = vol->eba_tbl[lnum];
	if (pnum < 0) {
		dbg_eba("read %d bytes from offset %d of LEB %d:%d (unmapped)",
			len, offset, vol_id, lnum);
		leb_read_unlock(ubi, vol_id, lnum);
		ubi_assert(vol->vol_type != UBI_STATIC_VOLUME);
		memset(buf, 0xFF, len);
		return 0;
	}

	dbg_eba("read %d bytes from offset %d of LEB %d:%d, PEB %d",
		len, offset, vol_id, lnum, pnum);

	if (vol->vol_type == UBI_DYNAMIC_VOLUME)
		check = 0;

retry:
	if (check) {
		vid_hdr = ubi_zalloc_vid_hdr(ubi, GFP_NOFS);
		if (!vid_hdr) {
			err = -ENOMEM;
			goto out_unlock;
		}

		err = ubi_io_read_vid_hdr(ubi, pnum, vid_hdr, 1);
		if (err && err != UBI_IO_BITFLIPS) {
			if (err > 0) {
				if (err == UBI_IO_BAD_HDR_EBADMSG ||
				    err == UBI_IO_BAD_HDR) {
					ubi_warn("corrupted VID header at PEB "
						 "%d, LEB %d:%d", pnum, vol_id,
						 lnum);
					err = -EBADMSG;
				} else
					ubi_ro_mode(ubi);
			}
			goto out_free;
		} else if (err == UBI_IO_BITFLIPS)
			scrub = 1;

		ubi_assert(lnum < be32_to_cpu(vid_hdr->used_ebs));
		ubi_assert(len == be32_to_cpu(vid_hdr->data_size));

		crc = be32_to_cpu(vid_hdr->data_crc);
		ubi_free_vid_hdr(ubi, vid_hdr);
	}

	err = ubi_io_read_data(ubi, buf, pnum, offset, len);
	if (err) {
		if (err == UBI_IO_BITFLIPS) {
			scrub = 1;
			err = 0;
		} else if (mtd_is_eccerr(err)) {
			if (vol->vol_type == UBI_DYNAMIC_VOLUME)
				goto out_unlock;
			scrub = 1;
			if (!check) {
				ubi_msg("force data checking");
				check = 1;
				goto retry;
			}
		} else
			goto out_unlock;
	}

	if (check) {
		uint32_t crc1 = crc32(UBI_CRC32_INIT, buf, len);
		if (crc1 != crc) {
			ubi_warn("CRC error: calculated %#08x, must be %#08x",
				 crc1, crc);
			err = -EBADMSG;
			goto out_unlock;
		}
	}

	if (scrub)
		err = ubi_wl_scrub_peb(ubi, pnum);

	leb_read_unlock(ubi, vol_id, lnum);
	return err;

out_free:
	ubi_free_vid_hdr(ubi, vid_hdr);
out_unlock:
	leb_read_unlock(ubi, vol_id, lnum);
	return err;
}

/**
 * recover_peb - recover from write failure.
 * @ubi: UBI device description object
 * @pnum: the physical eraseblock to recover
 * @vol_id: volume ID
 * @lnum: logical eraseblock number
 * @buf: data which was not written because of the write failure
 * @offset: offset of the failed write
 * @len: how many bytes should have been written
 *
 * This function is called in case of a write failure and moves all good data
 * from the potentially bad physical eraseblock to a good physical eraseblock.
 * This function also writes the data which was not written due to the failure.
 * Returns new physical eraseblock number in case of success, and a negative
 * error code in case of failure.
 */
static int recover_peb(struct ubi_device *ubi, int pnum, int vol_id, int lnum,
		       const void *buf, int offset, int len)
{
	int err, idx = vol_id2idx(ubi, vol_id), new_pnum, data_size, tries = 0;
	struct ubi_volume *vol = ubi->volumes[idx];
	struct ubi_vid_hdr *vid_hdr;

	vid_hdr = ubi_zalloc_vid_hdr(ubi, GFP_NOFS);
	if (!vid_hdr)
		return -ENOMEM;

retry:
	new_pnum = ubi_wl_get_peb(ubi, UBI_UNKNOWN);
	if (new_pnum < 0) {
		ubi_free_vid_hdr(ubi, vid_hdr);
		return new_pnum;
	}

	ubi_msg("recover PEB %d, move data to PEB %d", pnum, new_pnum);

	err = ubi_io_read_vid_hdr(ubi, pnum, vid_hdr, 1);
	if (err && err != UBI_IO_BITFLIPS) {
		if (err > 0)
			err = -EIO;
		goto out_put;
	}

	vid_hdr->sqnum = cpu_to_be64(next_sqnum(ubi));
	err = ubi_io_write_vid_hdr(ubi, new_pnum, vid_hdr);
	if (err)
		goto write_error;

	data_size = offset + len;
	mutex_lock(&ubi->buf_mutex);
	memset(ubi->peb_buf + offset, 0xFF, len);

	
	if (offset > 0) {
		err = ubi_io_read_data(ubi, ubi->peb_buf, pnum, 0, offset);
		if (err && err != UBI_IO_BITFLIPS)
			goto out_unlock;
	}

	memcpy(ubi->peb_buf + offset, buf, len);

	err = ubi_io_write_data(ubi, ubi->peb_buf, new_pnum, 0, data_size);
	if (err) {
		mutex_unlock(&ubi->buf_mutex);
		goto write_error;
	}

	mutex_unlock(&ubi->buf_mutex);
	ubi_free_vid_hdr(ubi, vid_hdr);

	vol->eba_tbl[lnum] = new_pnum;
	ubi_wl_put_peb(ubi, pnum, 1);

	ubi_msg("data was successfully recovered");
	return 0;

out_unlock:
	mutex_unlock(&ubi->buf_mutex);
out_put:
	ubi_wl_put_peb(ubi, new_pnum, 1);
	ubi_free_vid_hdr(ubi, vid_hdr);
	return err;

write_error:
	ubi_warn("failed to write to PEB %d", new_pnum);
	ubi_wl_put_peb(ubi, new_pnum, 1);
	if (++tries > UBI_IO_RETRIES) {
		ubi_free_vid_hdr(ubi, vid_hdr);
		return err;
	}
	ubi_msg("try again");
	goto retry;
}

/**
 * ubi_eba_write_leb - write data to dynamic volume.
 * @ubi: UBI device description object
 * @vol: volume description object
 * @lnum: logical eraseblock number
 * @buf: the data to write
 * @offset: offset within the logical eraseblock where to write
 * @len: how many bytes to write
 * @dtype: data type
 *
 * This function writes data to logical eraseblock @lnum of a dynamic volume
 * @vol. Returns zero in case of success and a negative error code in case
 * of failure. In case of error, it is possible that something was still
 * written to the flash media, but may be some garbage.
 */
int ubi_eba_write_leb(struct ubi_device *ubi, struct ubi_volume *vol, int lnum,
		      const void *buf, int offset, int len, int dtype)
{
	int err, pnum, tries = 0, vol_id = vol->vol_id;
	struct ubi_vid_hdr *vid_hdr;

	if (ubi->ro_mode)
		return -EROFS;

	err = leb_write_lock(ubi, vol_id, lnum);
	if (err)
		return err;

	pnum = vol->eba_tbl[lnum];
	if (pnum >= 0) {
		dbg_eba("write %d bytes at offset %d of LEB %d:%d, PEB %d",
			len, offset, vol_id, lnum, pnum);

		err = ubi_io_write_data(ubi, buf, pnum, offset, len);
		if (err) {
			ubi_warn("failed to write data to PEB %d", pnum);
			if (err == -EIO && ubi->bad_allowed)
				err = recover_peb(ubi, pnum, vol_id, lnum, buf,
						  offset, len);
			if (err)
				ubi_ro_mode(ubi);
		}
		leb_write_unlock(ubi, vol_id, lnum);
		return err;
	}

	vid_hdr = ubi_zalloc_vid_hdr(ubi, GFP_NOFS);
	if (!vid_hdr) {
		leb_write_unlock(ubi, vol_id, lnum);
		return -ENOMEM;
	}

	vid_hdr->vol_type = UBI_VID_DYNAMIC;
	vid_hdr->sqnum = cpu_to_be64(next_sqnum(ubi));
	vid_hdr->vol_id = cpu_to_be32(vol_id);
	vid_hdr->lnum = cpu_to_be32(lnum);
	vid_hdr->compat = ubi_get_compat(ubi, vol_id);
	vid_hdr->data_pad = cpu_to_be32(vol->data_pad);

retry:
	pnum = ubi_wl_get_peb(ubi, dtype);
	if (pnum < 0) {
		ubi_free_vid_hdr(ubi, vid_hdr);
		leb_write_unlock(ubi, vol_id, lnum);
		return pnum;
	}

	dbg_eba("write VID hdr and %d bytes at offset %d of LEB %d:%d, PEB %d",
		len, offset, vol_id, lnum, pnum);

	err = ubi_io_write_vid_hdr(ubi, pnum, vid_hdr);
	if (err) {
		ubi_warn("failed to write VID header to LEB %d:%d, PEB %d",
			 vol_id, lnum, pnum);
		goto write_error;
	}

	if (len) {
		err = ubi_io_write_data(ubi, buf, pnum, offset, len);
		if (err) {
			ubi_warn("failed to write %d bytes at offset %d of "
				 "LEB %d:%d, PEB %d", len, offset, vol_id,
				 lnum, pnum);
			goto write_error;
		}
	}

	vol->eba_tbl[lnum] = pnum;

	leb_write_unlock(ubi, vol_id, lnum);
	ubi_free_vid_hdr(ubi, vid_hdr);
	return 0;

write_error:
	if (err != -EIO || !ubi->bad_allowed) {
		ubi_ro_mode(ubi);
		leb_write_unlock(ubi, vol_id, lnum);
		ubi_free_vid_hdr(ubi, vid_hdr);
		return err;
	}

	err = ubi_wl_put_peb(ubi, pnum, 1);
	if (err || ++tries > UBI_IO_RETRIES) {
		ubi_ro_mode(ubi);
		leb_write_unlock(ubi, vol_id, lnum);
		ubi_free_vid_hdr(ubi, vid_hdr);
		return err;
	}

	vid_hdr->sqnum = cpu_to_be64(next_sqnum(ubi));
	ubi_msg("try another PEB");
	goto retry;
}

int ubi_eba_write_leb_st(struct ubi_device *ubi, struct ubi_volume *vol,
			 int lnum, const void *buf, int len, int dtype,
			 int used_ebs)
{
	int err, pnum, tries = 0, data_size = len, vol_id = vol->vol_id;
	struct ubi_vid_hdr *vid_hdr;
	uint32_t crc;

	if (ubi->ro_mode)
		return -EROFS;

	if (lnum == used_ebs - 1)
		
		len = ALIGN(data_size, ubi->min_io_size);
	else
		ubi_assert(!(len & (ubi->min_io_size - 1)));

	vid_hdr = ubi_zalloc_vid_hdr(ubi, GFP_NOFS);
	if (!vid_hdr)
		return -ENOMEM;

	err = leb_write_lock(ubi, vol_id, lnum);
	if (err) {
		ubi_free_vid_hdr(ubi, vid_hdr);
		return err;
	}

	vid_hdr->sqnum = cpu_to_be64(next_sqnum(ubi));
	vid_hdr->vol_id = cpu_to_be32(vol_id);
	vid_hdr->lnum = cpu_to_be32(lnum);
	vid_hdr->compat = ubi_get_compat(ubi, vol_id);
	vid_hdr->data_pad = cpu_to_be32(vol->data_pad);

	crc = crc32(UBI_CRC32_INIT, buf, data_size);
	vid_hdr->vol_type = UBI_VID_STATIC;
	vid_hdr->data_size = cpu_to_be32(data_size);
	vid_hdr->used_ebs = cpu_to_be32(used_ebs);
	vid_hdr->data_crc = cpu_to_be32(crc);

retry:
	pnum = ubi_wl_get_peb(ubi, dtype);
	if (pnum < 0) {
		ubi_free_vid_hdr(ubi, vid_hdr);
		leb_write_unlock(ubi, vol_id, lnum);
		return pnum;
	}

	dbg_eba("write VID hdr and %d bytes at LEB %d:%d, PEB %d, used_ebs %d",
		len, vol_id, lnum, pnum, used_ebs);

	err = ubi_io_write_vid_hdr(ubi, pnum, vid_hdr);
	if (err) {
		ubi_warn("failed to write VID header to LEB %d:%d, PEB %d",
			 vol_id, lnum, pnum);
		goto write_error;
	}

	err = ubi_io_write_data(ubi, buf, pnum, 0, len);
	if (err) {
		ubi_warn("failed to write %d bytes of data to PEB %d",
			 len, pnum);
		goto write_error;
	}

	ubi_assert(vol->eba_tbl[lnum] < 0);
	vol->eba_tbl[lnum] = pnum;

	leb_write_unlock(ubi, vol_id, lnum);
	ubi_free_vid_hdr(ubi, vid_hdr);
	return 0;

write_error:
	if (err != -EIO || !ubi->bad_allowed) {
		ubi_ro_mode(ubi);
		leb_write_unlock(ubi, vol_id, lnum);
		ubi_free_vid_hdr(ubi, vid_hdr);
		return err;
	}

	err = ubi_wl_put_peb(ubi, pnum, 1);
	if (err || ++tries > UBI_IO_RETRIES) {
		ubi_ro_mode(ubi);
		leb_write_unlock(ubi, vol_id, lnum);
		ubi_free_vid_hdr(ubi, vid_hdr);
		return err;
	}

	vid_hdr->sqnum = cpu_to_be64(next_sqnum(ubi));
	ubi_msg("try another PEB");
	goto retry;
}

int ubi_eba_atomic_leb_change(struct ubi_device *ubi, struct ubi_volume *vol,
			      int lnum, const void *buf, int len, int dtype)
{
	int err, pnum, tries = 0, vol_id = vol->vol_id;
	struct ubi_vid_hdr *vid_hdr;
	uint32_t crc;

	if (ubi->ro_mode)
		return -EROFS;

	if (len == 0) {
		err = ubi_eba_unmap_leb(ubi, vol, lnum);
		if (err)
			return err;
		return ubi_eba_write_leb(ubi, vol, lnum, NULL, 0, 0, dtype);
	}

	vid_hdr = ubi_zalloc_vid_hdr(ubi, GFP_NOFS);
	if (!vid_hdr)
		return -ENOMEM;

	mutex_lock(&ubi->alc_mutex);
	err = leb_write_lock(ubi, vol_id, lnum);
	if (err)
		goto out_mutex;

	vid_hdr->sqnum = cpu_to_be64(next_sqnum(ubi));
	vid_hdr->vol_id = cpu_to_be32(vol_id);
	vid_hdr->lnum = cpu_to_be32(lnum);
	vid_hdr->compat = ubi_get_compat(ubi, vol_id);
	vid_hdr->data_pad = cpu_to_be32(vol->data_pad);

	crc = crc32(UBI_CRC32_INIT, buf, len);
	vid_hdr->vol_type = UBI_VID_DYNAMIC;
	vid_hdr->data_size = cpu_to_be32(len);
	vid_hdr->copy_flag = 1;
	vid_hdr->data_crc = cpu_to_be32(crc);

retry:
	pnum = ubi_wl_get_peb(ubi, dtype);
	if (pnum < 0) {
		err = pnum;
		goto out_leb_unlock;
	}

	dbg_eba("change LEB %d:%d, PEB %d, write VID hdr to PEB %d",
		vol_id, lnum, vol->eba_tbl[lnum], pnum);

	err = ubi_io_write_vid_hdr(ubi, pnum, vid_hdr);
	if (err) {
		ubi_warn("failed to write VID header to LEB %d:%d, PEB %d",
			 vol_id, lnum, pnum);
		goto write_error;
	}

	err = ubi_io_write_data(ubi, buf, pnum, 0, len);
	if (err) {
		ubi_warn("failed to write %d bytes of data to PEB %d",
			 len, pnum);
		goto write_error;
	}

	if (vol->eba_tbl[lnum] >= 0) {
		err = ubi_wl_put_peb(ubi, vol->eba_tbl[lnum], 0);
		if (err)
			goto out_leb_unlock;
	}

	vol->eba_tbl[lnum] = pnum;

out_leb_unlock:
	leb_write_unlock(ubi, vol_id, lnum);
out_mutex:
	mutex_unlock(&ubi->alc_mutex);
	ubi_free_vid_hdr(ubi, vid_hdr);
	return err;

write_error:
	if (err != -EIO || !ubi->bad_allowed) {
		ubi_ro_mode(ubi);
		goto out_leb_unlock;
	}

	err = ubi_wl_put_peb(ubi, pnum, 1);
	if (err || ++tries > UBI_IO_RETRIES) {
		ubi_ro_mode(ubi);
		goto out_leb_unlock;
	}

	vid_hdr->sqnum = cpu_to_be64(next_sqnum(ubi));
	ubi_msg("try another PEB");
	goto retry;
}

/**
 * is_error_sane - check whether a read error is sane.
 * @err: code of the error happened during reading
 *
 * This is a helper function for 'ubi_eba_copy_leb()' which is called when we
 * cannot read data from the target PEB (an error @err happened). If the error
 * code is sane, then we treat this error as non-fatal. Otherwise the error is
 * fatal and UBI will be switched to R/O mode later.
 *
 * The idea is that we try not to switch to R/O mode if the read error is
 * something which suggests there was a real read problem. E.g., %-EIO. Or a
 * memory allocation failed (-%ENOMEM). Otherwise, it is safer to switch to R/O
 * mode, simply because we do not know what happened at the MTD level, and we
 * cannot handle this. E.g., the underlying driver may have become crazy, and
 * it is safer to switch to R/O mode to preserve the data.
 *
 * And bear in mind, this is about reading from the target PEB, i.e. the PEB
 * which we have just written.
 */
static int is_error_sane(int err)
{
	if (err == -EIO || err == -ENOMEM || err == UBI_IO_BAD_HDR ||
	    err == UBI_IO_BAD_HDR_EBADMSG || err == -ETIMEDOUT)
		return 0;
	return 1;
}

int ubi_eba_copy_leb(struct ubi_device *ubi, int from, int to,
		     struct ubi_vid_hdr *vid_hdr)
{
	int err, vol_id, lnum, data_size, aldata_size, idx;
	struct ubi_volume *vol;
	uint32_t crc;

	vol_id = be32_to_cpu(vid_hdr->vol_id);
	lnum = be32_to_cpu(vid_hdr->lnum);

	dbg_wl("copy LEB %d:%d, PEB %d to PEB %d", vol_id, lnum, from, to);

	if (vid_hdr->vol_type == UBI_VID_STATIC) {
		data_size = be32_to_cpu(vid_hdr->data_size);
		aldata_size = ALIGN(data_size, ubi->min_io_size);
	} else
		data_size = aldata_size =
			    ubi->leb_size - be32_to_cpu(vid_hdr->data_pad);

	idx = vol_id2idx(ubi, vol_id);
	spin_lock(&ubi->volumes_lock);
	vol = ubi->volumes[idx];
	spin_unlock(&ubi->volumes_lock);
	if (!vol) {
		
		dbg_wl("volume %d is being removed, cancel", vol_id);
		return MOVE_CANCEL_RACE;
	}

	err = leb_write_trylock(ubi, vol_id, lnum);
	if (err) {
		dbg_wl("contention on LEB %d:%d, cancel", vol_id, lnum);
		return MOVE_RETRY;
	}

	if (vol->eba_tbl[lnum] != from) {
		dbg_wl("LEB %d:%d is no longer mapped to PEB %d, mapped to "
		       "PEB %d, cancel", vol_id, lnum, from,
		       vol->eba_tbl[lnum]);
		err = MOVE_CANCEL_RACE;
		goto out_unlock_leb;
	}

	mutex_lock(&ubi->buf_mutex);
	dbg_wl("read %d bytes of data", aldata_size);
	err = ubi_io_read_data(ubi, ubi->peb_buf, from, 0, aldata_size);
	if (err && err != UBI_IO_BITFLIPS) {
		ubi_warn("error %d while reading data from PEB %d",
			 err, from);
		err = MOVE_SOURCE_RD_ERR;
		goto out_unlock_buf;
	}

	if (vid_hdr->vol_type == UBI_VID_DYNAMIC)
		aldata_size = data_size =
			ubi_calc_data_len(ubi, ubi->peb_buf, data_size);

	cond_resched();
	crc = crc32(UBI_CRC32_INIT, ubi->peb_buf, data_size);
	cond_resched();

	if (data_size > 0) {
		vid_hdr->copy_flag = 1;
		vid_hdr->data_size = cpu_to_be32(data_size);
		vid_hdr->data_crc = cpu_to_be32(crc);
	}
	vid_hdr->sqnum = cpu_to_be64(next_sqnum(ubi));

	err = ubi_io_write_vid_hdr(ubi, to, vid_hdr);
	if (err) {
		if (err == -EIO)
			err = MOVE_TARGET_WR_ERR;
		goto out_unlock_buf;
	}

	cond_resched();

	/* Read the VID header back and check if it was written correctly */
	err = ubi_io_read_vid_hdr(ubi, to, vid_hdr, 1);
	if (err) {
		if (err != UBI_IO_BITFLIPS) {
			ubi_warn("error %d while reading VID header back from "
				  "PEB %d", err, to);
			if (is_error_sane(err))
				err = MOVE_TARGET_RD_ERR;
		} else
			err = MOVE_TARGET_BITFLIPS;
		goto out_unlock_buf;
	}

	if (data_size > 0) {
		err = ubi_io_write_data(ubi, ubi->peb_buf, to, 0, aldata_size);
		if (err) {
			if (err == -EIO)
				err = MOVE_TARGET_WR_ERR;
			goto out_unlock_buf;
		}

		cond_resched();

		/*
		 * We've written the data and are going to read it back to make
		 * sure it was written correctly.
		 */
		memset(ubi->peb_buf, 0xFF, aldata_size);
		err = ubi_io_read_data(ubi, ubi->peb_buf, to, 0, aldata_size);
		if (err) {
			if (err != UBI_IO_BITFLIPS) {
				ubi_warn("error %d while reading data back "
					 "from PEB %d", err, to);
				if (is_error_sane(err))
					err = MOVE_TARGET_RD_ERR;
			} else
				err = MOVE_TARGET_BITFLIPS;
			goto out_unlock_buf;
		}

		cond_resched();

		if (crc != crc32(UBI_CRC32_INIT, ubi->peb_buf, data_size)) {
			ubi_warn("read data back from PEB %d and it is "
				 "different", to);
			err = -EINVAL;
			goto out_unlock_buf;
		}
	}

	ubi_assert(vol->eba_tbl[lnum] == from);
	vol->eba_tbl[lnum] = to;

out_unlock_buf:
	mutex_unlock(&ubi->buf_mutex);
out_unlock_leb:
	leb_write_unlock(ubi, vol_id, lnum);
	return err;
}

static void print_rsvd_warning(struct ubi_device *ubi,
			       struct ubi_scan_info *si)
{
	if (si->max_sqnum > (1 << 18)) {
		int min = ubi->beb_rsvd_level / 10;

		if (!min)
			min = 1;
		if (ubi->beb_rsvd_pebs > min)
			return;
	}

	ubi_warn("cannot reserve enough PEBs for bad PEB handling, reserved %d,"
		 " need %d", ubi->beb_rsvd_pebs, ubi->beb_rsvd_level);
	if (ubi->corr_peb_count)
		ubi_warn("%d PEBs are corrupted and not used",
			ubi->corr_peb_count);
}

int ubi_eba_init_scan(struct ubi_device *ubi, struct ubi_scan_info *si)
{
	int i, j, err, num_volumes;
	struct ubi_scan_volume *sv;
	struct ubi_volume *vol;
	struct ubi_scan_leb *seb;
	struct rb_node *rb;

	dbg_eba("initialize EBA sub-system");

	spin_lock_init(&ubi->ltree_lock);
	mutex_init(&ubi->alc_mutex);
	ubi->ltree = RB_ROOT;

	ubi->global_sqnum = si->max_sqnum + 1;
	num_volumes = ubi->vtbl_slots + UBI_INT_VOL_COUNT;

	for (i = 0; i < num_volumes; i++) {
		vol = ubi->volumes[i];
		if (!vol)
			continue;

		cond_resched();

		vol->eba_tbl = kmalloc(vol->reserved_pebs * sizeof(int),
				       GFP_KERNEL);
		if (!vol->eba_tbl) {
			err = -ENOMEM;
			goto out_free;
		}

		for (j = 0; j < vol->reserved_pebs; j++)
			vol->eba_tbl[j] = UBI_LEB_UNMAPPED;

		sv = ubi_scan_find_sv(si, idx2vol_id(ubi, i));
		if (!sv)
			continue;

		ubi_rb_for_each_entry(rb, seb, &sv->root, u.rb) {
			if (seb->lnum >= vol->reserved_pebs)
				ubi_scan_move_to_list(sv, seb, &si->erase);
			vol->eba_tbl[seb->lnum] = seb->pnum;
		}
	}

	if (ubi->avail_pebs < EBA_RESERVED_PEBS) {
		ubi_err("no enough physical eraseblocks (%d, need %d)",
			ubi->avail_pebs, EBA_RESERVED_PEBS);
		if (ubi->corr_peb_count)
			ubi_err("%d PEBs are corrupted and not used",
				ubi->corr_peb_count);
		err = -ENOSPC;
		goto out_free;
	}
	ubi->avail_pebs -= EBA_RESERVED_PEBS;
	ubi->rsvd_pebs += EBA_RESERVED_PEBS;

	if (ubi->bad_allowed) {
		ubi_calculate_reserved(ubi);

		if (ubi->avail_pebs < ubi->beb_rsvd_level) {
			
			ubi->beb_rsvd_pebs = ubi->avail_pebs;
			print_rsvd_warning(ubi, si);
		} else
			ubi->beb_rsvd_pebs = ubi->beb_rsvd_level;

		ubi->avail_pebs -= ubi->beb_rsvd_pebs;
		ubi->rsvd_pebs  += ubi->beb_rsvd_pebs;
	}

	dbg_eba("EBA sub-system is initialized");
	return 0;

out_free:
	for (i = 0; i < num_volumes; i++) {
		if (!ubi->volumes[i])
			continue;
		kfree(ubi->volumes[i]->eba_tbl);
		ubi->volumes[i]->eba_tbl = NULL;
	}
	return err;
}
