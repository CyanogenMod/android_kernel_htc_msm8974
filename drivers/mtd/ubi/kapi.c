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


#include <linux/module.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/namei.h>
#include <linux/fs.h>
#include <asm/div64.h>
#include "ubi.h"

void ubi_do_get_device_info(struct ubi_device *ubi, struct ubi_device_info *di)
{
	di->ubi_num = ubi->ubi_num;
	di->leb_size = ubi->leb_size;
	di->leb_start = ubi->leb_start;
	di->min_io_size = ubi->min_io_size;
	di->max_write_size = ubi->max_write_size;
	di->ro_mode = ubi->ro_mode;
	di->cdev = ubi->cdev.dev;
}
EXPORT_SYMBOL_GPL(ubi_do_get_device_info);

int ubi_get_device_info(int ubi_num, struct ubi_device_info *di)
{
	struct ubi_device *ubi;

	if (ubi_num < 0 || ubi_num >= UBI_MAX_DEVICES)
		return -EINVAL;
	ubi = ubi_get_device(ubi_num);
	if (!ubi)
		return -ENODEV;
	ubi_do_get_device_info(ubi, di);
	ubi_put_device(ubi);
	return 0;
}
EXPORT_SYMBOL_GPL(ubi_get_device_info);

void ubi_do_get_volume_info(struct ubi_device *ubi, struct ubi_volume *vol,
			    struct ubi_volume_info *vi)
{
	vi->vol_id = vol->vol_id;
	vi->ubi_num = ubi->ubi_num;
	vi->size = vol->reserved_pebs;
	vi->used_bytes = vol->used_bytes;
	vi->vol_type = vol->vol_type;
	vi->corrupted = vol->corrupted;
	vi->upd_marker = vol->upd_marker;
	vi->alignment = vol->alignment;
	vi->usable_leb_size = vol->usable_leb_size;
	vi->name_len = vol->name_len;
	vi->name = vol->name;
	vi->cdev = vol->cdev.dev;
}

void ubi_get_volume_info(struct ubi_volume_desc *desc,
			 struct ubi_volume_info *vi)
{
	ubi_do_get_volume_info(desc->vol->ubi, desc->vol, vi);
}
EXPORT_SYMBOL_GPL(ubi_get_volume_info);

struct ubi_volume_desc *ubi_open_volume(int ubi_num, int vol_id, int mode)
{
	int err;
	struct ubi_volume_desc *desc;
	struct ubi_device *ubi;
	struct ubi_volume *vol;

	dbg_gen("open device %d, volume %d, mode %d", ubi_num, vol_id, mode);

	if (ubi_num < 0 || ubi_num >= UBI_MAX_DEVICES)
		return ERR_PTR(-EINVAL);

	if (mode != UBI_READONLY && mode != UBI_READWRITE &&
	    mode != UBI_EXCLUSIVE)
		return ERR_PTR(-EINVAL);

	ubi = ubi_get_device(ubi_num);
	if (!ubi)
		return ERR_PTR(-ENODEV);

	if (vol_id < 0 || vol_id >= ubi->vtbl_slots) {
		err = -EINVAL;
		goto out_put_ubi;
	}

	desc = kmalloc(sizeof(struct ubi_volume_desc), GFP_KERNEL);
	if (!desc) {
		err = -ENOMEM;
		goto out_put_ubi;
	}

	err = -ENODEV;
	if (!try_module_get(THIS_MODULE))
		goto out_free;

	spin_lock(&ubi->volumes_lock);
	vol = ubi->volumes[vol_id];
	if (!vol)
		goto out_unlock;

	err = -EBUSY;
	switch (mode) {
	case UBI_READONLY:
		if (vol->exclusive)
			goto out_unlock;
		vol->readers += 1;
		break;

	case UBI_READWRITE:
		if (vol->exclusive || vol->writers > 0)
			goto out_unlock;
		vol->writers += 1;
		break;

	case UBI_EXCLUSIVE:
		if (vol->exclusive || vol->writers || vol->readers)
			goto out_unlock;
		vol->exclusive = 1;
		break;
	}
	get_device(&vol->dev);
	vol->ref_count += 1;
	spin_unlock(&ubi->volumes_lock);

	desc->vol = vol;
	desc->mode = mode;

	mutex_lock(&ubi->ckvol_mutex);
	if (!vol->checked) {
		
		err = ubi_check_volume(ubi, vol_id);
		if (err < 0) {
			mutex_unlock(&ubi->ckvol_mutex);
			ubi_close_volume(desc);
			return ERR_PTR(err);
		}
		if (err == 1) {
			ubi_warn("volume %d on UBI device %d is corrupted",
				 vol_id, ubi->ubi_num);
			vol->corrupted = 1;
		}
		vol->checked = 1;
	}
	mutex_unlock(&ubi->ckvol_mutex);

	return desc;

out_unlock:
	spin_unlock(&ubi->volumes_lock);
	module_put(THIS_MODULE);
out_free:
	kfree(desc);
out_put_ubi:
	ubi_put_device(ubi);
	dbg_err("cannot open device %d, volume %d, error %d",
		ubi_num, vol_id, err);
	return ERR_PTR(err);
}
EXPORT_SYMBOL_GPL(ubi_open_volume);

struct ubi_volume_desc *ubi_open_volume_nm(int ubi_num, const char *name,
					   int mode)
{
	int i, vol_id = -1, len;
	struct ubi_device *ubi;
	struct ubi_volume_desc *ret;

	dbg_gen("open device %d, volume %s, mode %d", ubi_num, name, mode);

	if (!name)
		return ERR_PTR(-EINVAL);

	len = strnlen(name, UBI_VOL_NAME_MAX + 1);
	if (len > UBI_VOL_NAME_MAX)
		return ERR_PTR(-EINVAL);

	if (ubi_num < 0 || ubi_num >= UBI_MAX_DEVICES)
		return ERR_PTR(-EINVAL);

	ubi = ubi_get_device(ubi_num);
	if (!ubi)
		return ERR_PTR(-ENODEV);

	spin_lock(&ubi->volumes_lock);
	
	for (i = 0; i < ubi->vtbl_slots; i++) {
		struct ubi_volume *vol = ubi->volumes[i];

		if (vol && len == vol->name_len && !strcmp(name, vol->name)) {
			vol_id = i;
			break;
		}
	}
	spin_unlock(&ubi->volumes_lock);

	if (vol_id >= 0)
		ret = ubi_open_volume(ubi_num, vol_id, mode);
	else
		ret = ERR_PTR(-ENODEV);

	ubi_put_device(ubi);
	return ret;
}
EXPORT_SYMBOL_GPL(ubi_open_volume_nm);

struct ubi_volume_desc *ubi_open_volume_path(const char *pathname, int mode)
{
	int error, ubi_num, vol_id, mod;
	struct inode *inode;
	struct path path;

	dbg_gen("open volume %s, mode %d", pathname, mode);

	if (!pathname || !*pathname)
		return ERR_PTR(-EINVAL);

	error = kern_path(pathname, LOOKUP_FOLLOW, &path);
	if (error)
		return ERR_PTR(error);

	inode = path.dentry->d_inode;
	mod = inode->i_mode;
	ubi_num = ubi_major2num(imajor(inode));
	vol_id = iminor(inode) - 1;
	path_put(&path);

	if (!S_ISCHR(mod))
		return ERR_PTR(-EINVAL);
	if (vol_id >= 0 && ubi_num >= 0)
		return ubi_open_volume(ubi_num, vol_id, mode);
	return ERR_PTR(-ENODEV);
}
EXPORT_SYMBOL_GPL(ubi_open_volume_path);

void ubi_close_volume(struct ubi_volume_desc *desc)
{
	struct ubi_volume *vol = desc->vol;
	struct ubi_device *ubi = vol->ubi;

	dbg_gen("close device %d, volume %d, mode %d",
		ubi->ubi_num, vol->vol_id, desc->mode);

	spin_lock(&ubi->volumes_lock);
	switch (desc->mode) {
	case UBI_READONLY:
		vol->readers -= 1;
		break;
	case UBI_READWRITE:
		vol->writers -= 1;
		break;
	case UBI_EXCLUSIVE:
		vol->exclusive = 0;
	}
	vol->ref_count -= 1;
	spin_unlock(&ubi->volumes_lock);

	kfree(desc);
	put_device(&vol->dev);
	ubi_put_device(ubi);
	module_put(THIS_MODULE);
}
EXPORT_SYMBOL_GPL(ubi_close_volume);

int ubi_leb_read(struct ubi_volume_desc *desc, int lnum, char *buf, int offset,
		 int len, int check)
{
	struct ubi_volume *vol = desc->vol;
	struct ubi_device *ubi = vol->ubi;
	int err, vol_id = vol->vol_id;

	dbg_gen("read %d bytes from LEB %d:%d:%d", len, vol_id, lnum, offset);

	if (vol_id < 0 || vol_id >= ubi->vtbl_slots || lnum < 0 ||
	    lnum >= vol->used_ebs || offset < 0 || len < 0 ||
	    offset + len > vol->usable_leb_size)
		return -EINVAL;

	if (vol->vol_type == UBI_STATIC_VOLUME) {
		if (vol->used_ebs == 0)
			
			return 0;
		if (lnum == vol->used_ebs - 1 &&
		    offset + len > vol->last_eb_bytes)
			return -EINVAL;
	}

	if (vol->upd_marker)
		return -EBADF;
	if (len == 0)
		return 0;

	err = ubi_eba_read_leb(ubi, vol, lnum, buf, offset, len, check);
	if (err && mtd_is_eccerr(err) && vol->vol_type == UBI_STATIC_VOLUME) {
		ubi_warn("mark volume %d as corrupted", vol_id);
		vol->corrupted = 1;
	}

	return err;
}
EXPORT_SYMBOL_GPL(ubi_leb_read);

/**
 * ubi_leb_write - write data.
 * @desc: volume descriptor
 * @lnum: logical eraseblock number to write to
 * @buf: data to write
 * @offset: offset within the logical eraseblock where to write
 * @len: how many bytes to write
 * @dtype: expected data type
 *
 * This function writes @len bytes of data from @buf to offset @offset of
 * logical eraseblock @lnum. The @dtype argument describes expected lifetime of
 * the data.
 *
 * This function takes care of physical eraseblock write failures. If write to
 * the physical eraseblock write operation fails, the logical eraseblock is
 * re-mapped to another physical eraseblock, the data is recovered, and the
 * write finishes. UBI has a pool of reserved physical eraseblocks for this.
 *
 * If all the data were successfully written, zero is returned. If an error
 * occurred and UBI has not been able to recover from it, this function returns
 * a negative error code. Note, in case of an error, it is possible that
 * something was still written to the flash media, but that may be some
 * garbage.
 *
 * If the volume is damaged because of an interrupted update this function just
 * returns immediately with %-EBADF code.
 */
int ubi_leb_write(struct ubi_volume_desc *desc, int lnum, const void *buf,
		  int offset, int len, int dtype)
{
	struct ubi_volume *vol = desc->vol;
	struct ubi_device *ubi = vol->ubi;
	int vol_id = vol->vol_id;

	dbg_gen("write %d bytes to LEB %d:%d:%d", len, vol_id, lnum, offset);

	if (vol_id < 0 || vol_id >= ubi->vtbl_slots)
		return -EINVAL;

	if (desc->mode == UBI_READONLY || vol->vol_type == UBI_STATIC_VOLUME)
		return -EROFS;

	if (lnum < 0 || lnum >= vol->reserved_pebs || offset < 0 || len < 0 ||
	    offset + len > vol->usable_leb_size ||
	    offset & (ubi->min_io_size - 1) || len & (ubi->min_io_size - 1))
		return -EINVAL;

	if (dtype != UBI_LONGTERM && dtype != UBI_SHORTTERM &&
	    dtype != UBI_UNKNOWN)
		return -EINVAL;

	if (vol->upd_marker)
		return -EBADF;

	if (len == 0)
		return 0;

	return ubi_eba_write_leb(ubi, vol, lnum, buf, offset, len, dtype);
}
EXPORT_SYMBOL_GPL(ubi_leb_write);

int ubi_leb_change(struct ubi_volume_desc *desc, int lnum, const void *buf,
		   int len, int dtype)
{
	struct ubi_volume *vol = desc->vol;
	struct ubi_device *ubi = vol->ubi;
	int vol_id = vol->vol_id;

	dbg_gen("atomically write %d bytes to LEB %d:%d", len, vol_id, lnum);

	if (vol_id < 0 || vol_id >= ubi->vtbl_slots)
		return -EINVAL;

	if (desc->mode == UBI_READONLY || vol->vol_type == UBI_STATIC_VOLUME)
		return -EROFS;

	if (lnum < 0 || lnum >= vol->reserved_pebs || len < 0 ||
	    len > vol->usable_leb_size || len & (ubi->min_io_size - 1))
		return -EINVAL;

	if (dtype != UBI_LONGTERM && dtype != UBI_SHORTTERM &&
	    dtype != UBI_UNKNOWN)
		return -EINVAL;

	if (vol->upd_marker)
		return -EBADF;

	if (len == 0)
		return 0;

	return ubi_eba_atomic_leb_change(ubi, vol, lnum, buf, len, dtype);
}
EXPORT_SYMBOL_GPL(ubi_leb_change);

int ubi_leb_erase(struct ubi_volume_desc *desc, int lnum)
{
	struct ubi_volume *vol = desc->vol;
	struct ubi_device *ubi = vol->ubi;
	int err;

	dbg_gen("erase LEB %d:%d", vol->vol_id, lnum);

	if (desc->mode == UBI_READONLY || vol->vol_type == UBI_STATIC_VOLUME)
		return -EROFS;

	if (lnum < 0 || lnum >= vol->reserved_pebs)
		return -EINVAL;

	if (vol->upd_marker)
		return -EBADF;

	err = ubi_eba_unmap_leb(ubi, vol, lnum);
	if (err)
		return err;

	return ubi_wl_flush(ubi);
}
EXPORT_SYMBOL_GPL(ubi_leb_erase);

/**
 * ubi_leb_unmap - un-map logical eraseblock.
 * @desc: volume descriptor
 * @lnum: logical eraseblock number
 *
 * This function un-maps logical eraseblock @lnum and schedules the
 * corresponding physical eraseblock for erasure, so that it will eventually be
 * physically erased in background. This operation is much faster than the
 * erase operation.
 *
 * Unlike erase, the un-map operation does not guarantee that the logical
 * eraseblock will contain all 0xFF bytes when UBI is initialized again. For
 * example, if several logical eraseblocks are un-mapped, and an unclean reboot
 * happens after this, the logical eraseblocks will not necessarily be
 * un-mapped again when this MTD device is attached. They may actually be
 * mapped to the same physical eraseblocks again. So, this function has to be
 * used with care.
 *
 * In other words, when un-mapping a logical eraseblock, UBI does not store
 * any information about this on the flash media, it just marks the logical
 * eraseblock as "un-mapped" in RAM. If UBI is detached before the physical
 * eraseblock is physically erased, it will be mapped again to the same logical
 * eraseblock when the MTD device is attached again.
 *
 * The main and obvious use-case of this function is when the contents of a
 * logical eraseblock has to be re-written. Then it is much more efficient to
 * first un-map it, then write new data, rather than first erase it, then write
 * new data. Note, once new data has been written to the logical eraseblock,
 * UBI guarantees that the old contents has gone forever. In other words, if an
 * unclean reboot happens after the logical eraseblock has been un-mapped and
 * then written to, it will contain the last written data.
 *
 * This function returns zero in case of success and a negative error code in
 * case of failure. If the volume is damaged because of an interrupted update
 * this function just returns immediately with %-EBADF code.
 */
int ubi_leb_unmap(struct ubi_volume_desc *desc, int lnum)
{
	struct ubi_volume *vol = desc->vol;
	struct ubi_device *ubi = vol->ubi;

	dbg_gen("unmap LEB %d:%d", vol->vol_id, lnum);

	if (desc->mode == UBI_READONLY || vol->vol_type == UBI_STATIC_VOLUME)
		return -EROFS;

	if (lnum < 0 || lnum >= vol->reserved_pebs)
		return -EINVAL;

	if (vol->upd_marker)
		return -EBADF;

	return ubi_eba_unmap_leb(ubi, vol, lnum);
}
EXPORT_SYMBOL_GPL(ubi_leb_unmap);

int ubi_leb_map(struct ubi_volume_desc *desc, int lnum, int dtype)
{
	struct ubi_volume *vol = desc->vol;
	struct ubi_device *ubi = vol->ubi;

	dbg_gen("unmap LEB %d:%d", vol->vol_id, lnum);

	if (desc->mode == UBI_READONLY || vol->vol_type == UBI_STATIC_VOLUME)
		return -EROFS;

	if (lnum < 0 || lnum >= vol->reserved_pebs)
		return -EINVAL;

	if (dtype != UBI_LONGTERM && dtype != UBI_SHORTTERM &&
	    dtype != UBI_UNKNOWN)
		return -EINVAL;

	if (vol->upd_marker)
		return -EBADF;

	if (vol->eba_tbl[lnum] >= 0)
		return -EBADMSG;

	return ubi_eba_write_leb(ubi, vol, lnum, NULL, 0, 0, dtype);
}
EXPORT_SYMBOL_GPL(ubi_leb_map);

int ubi_is_mapped(struct ubi_volume_desc *desc, int lnum)
{
	struct ubi_volume *vol = desc->vol;

	dbg_gen("test LEB %d:%d", vol->vol_id, lnum);

	if (lnum < 0 || lnum >= vol->reserved_pebs)
		return -EINVAL;

	if (vol->upd_marker)
		return -EBADF;

	return vol->eba_tbl[lnum] >= 0;
}
EXPORT_SYMBOL_GPL(ubi_is_mapped);

int ubi_sync(int ubi_num)
{
	struct ubi_device *ubi;

	ubi = ubi_get_device(ubi_num);
	if (!ubi)
		return -ENODEV;

	mtd_sync(ubi->mtd);
	ubi_put_device(ubi);
	return 0;
}
EXPORT_SYMBOL_GPL(ubi_sync);

BLOCKING_NOTIFIER_HEAD(ubi_notifiers);

int ubi_register_volume_notifier(struct notifier_block *nb,
				 int ignore_existing)
{
	int err;

	err = blocking_notifier_chain_register(&ubi_notifiers, nb);
	if (err != 0)
		return err;
	if (ignore_existing)
		return 0;

	mutex_lock(&ubi_devices_mutex);
	ubi_enumerate_volumes(nb);
	mutex_unlock(&ubi_devices_mutex);

	return err;
}
EXPORT_SYMBOL_GPL(ubi_register_volume_notifier);

int ubi_unregister_volume_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&ubi_notifiers, nb);
}
EXPORT_SYMBOL_GPL(ubi_unregister_volume_notifier);
