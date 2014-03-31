/*
 * Copyright (c) International Business Machines Corp., 2006
 * Copyright (c) Nokia Corporation, 2007
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
 * Author: Artem Bityutskiy (Битюцкий Артём),
 *         Frank Haverkamp
 */


#include <linux/err.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/stringify.h>
#include <linux/namei.h>
#include <linux/stat.h>
#include <linux/miscdevice.h>
#include <linux/log2.h>
#include <linux/kthread.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include "ubi.h"

#define MTD_PARAM_LEN_MAX 64

#ifdef CONFIG_MTD_UBI_MODULE
#define ubi_is_module() 1
#else
#define ubi_is_module() 0
#endif

struct mtd_dev_param {
	char name[MTD_PARAM_LEN_MAX];
	int vid_hdr_offs;
};

static int __initdata mtd_devs;

static struct mtd_dev_param __initdata mtd_dev_param[UBI_MAX_DEVICES];

struct class *ubi_class;

struct kmem_cache *ubi_wl_entry_slab;

static struct miscdevice ubi_ctrl_cdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ubi_ctrl",
	.fops = &ubi_ctrl_cdev_operations,
};

static struct ubi_device *ubi_devices[UBI_MAX_DEVICES];

DEFINE_MUTEX(ubi_devices_mutex);

static DEFINE_SPINLOCK(ubi_devices_lock);

static ssize_t ubi_version_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", UBI_VERSION);
}

static struct class_attribute ubi_version =
	__ATTR(version, S_IRUGO, ubi_version_show, NULL);

static ssize_t dev_attribute_show(struct device *dev,
				  struct device_attribute *attr, char *buf);

static struct device_attribute dev_eraseblock_size =
	__ATTR(eraseblock_size, S_IRUGO, dev_attribute_show, NULL);
static struct device_attribute dev_avail_eraseblocks =
	__ATTR(avail_eraseblocks, S_IRUGO, dev_attribute_show, NULL);
static struct device_attribute dev_total_eraseblocks =
	__ATTR(total_eraseblocks, S_IRUGO, dev_attribute_show, NULL);
static struct device_attribute dev_volumes_count =
	__ATTR(volumes_count, S_IRUGO, dev_attribute_show, NULL);
static struct device_attribute dev_max_ec =
	__ATTR(max_ec, S_IRUGO, dev_attribute_show, NULL);
static struct device_attribute dev_reserved_for_bad =
	__ATTR(reserved_for_bad, S_IRUGO, dev_attribute_show, NULL);
static struct device_attribute dev_bad_peb_count =
	__ATTR(bad_peb_count, S_IRUGO, dev_attribute_show, NULL);
static struct device_attribute dev_max_vol_count =
	__ATTR(max_vol_count, S_IRUGO, dev_attribute_show, NULL);
static struct device_attribute dev_min_io_size =
	__ATTR(min_io_size, S_IRUGO, dev_attribute_show, NULL);
static struct device_attribute dev_bgt_enabled =
	__ATTR(bgt_enabled, S_IRUGO, dev_attribute_show, NULL);
static struct device_attribute dev_mtd_num =
	__ATTR(mtd_num, S_IRUGO, dev_attribute_show, NULL);

int ubi_volume_notify(struct ubi_device *ubi, struct ubi_volume *vol, int ntype)
{
	struct ubi_notification nt;

	ubi_do_get_device_info(ubi, &nt.di);
	ubi_do_get_volume_info(ubi, vol, &nt.vi);
	return blocking_notifier_call_chain(&ubi_notifiers, ntype, &nt);
}

int ubi_notify_all(struct ubi_device *ubi, int ntype, struct notifier_block *nb)
{
	struct ubi_notification nt;
	int i, count = 0;

	ubi_do_get_device_info(ubi, &nt.di);

	mutex_lock(&ubi->device_mutex);
	for (i = 0; i < ubi->vtbl_slots; i++) {
		if (!ubi->volumes[i])
			continue;

		ubi_do_get_volume_info(ubi, ubi->volumes[i], &nt.vi);
		if (nb)
			nb->notifier_call(nb, ntype, &nt);
		else
			blocking_notifier_call_chain(&ubi_notifiers, ntype,
						     &nt);
		count += 1;
	}
	mutex_unlock(&ubi->device_mutex);

	return count;
}

int ubi_enumerate_volumes(struct notifier_block *nb)
{
	int i, count = 0;

	for (i = 0; i < UBI_MAX_DEVICES; i++) {
		struct ubi_device *ubi = ubi_devices[i];

		if (!ubi)
			continue;
		count += ubi_notify_all(ubi, UBI_VOLUME_ADDED, nb);
	}

	return count;
}

struct ubi_device *ubi_get_device(int ubi_num)
{
	struct ubi_device *ubi;

	spin_lock(&ubi_devices_lock);
	ubi = ubi_devices[ubi_num];
	if (ubi) {
		ubi_assert(ubi->ref_count >= 0);
		ubi->ref_count += 1;
		get_device(&ubi->dev);
	}
	spin_unlock(&ubi_devices_lock);

	return ubi;
}

void ubi_put_device(struct ubi_device *ubi)
{
	spin_lock(&ubi_devices_lock);
	ubi->ref_count -= 1;
	put_device(&ubi->dev);
	spin_unlock(&ubi_devices_lock);
}

struct ubi_device *ubi_get_by_major(int major)
{
	int i;
	struct ubi_device *ubi;

	spin_lock(&ubi_devices_lock);
	for (i = 0; i < UBI_MAX_DEVICES; i++) {
		ubi = ubi_devices[i];
		if (ubi && MAJOR(ubi->cdev.dev) == major) {
			ubi_assert(ubi->ref_count >= 0);
			ubi->ref_count += 1;
			get_device(&ubi->dev);
			spin_unlock(&ubi_devices_lock);
			return ubi;
		}
	}
	spin_unlock(&ubi_devices_lock);

	return NULL;
}

int ubi_major2num(int major)
{
	int i, ubi_num = -ENODEV;

	spin_lock(&ubi_devices_lock);
	for (i = 0; i < UBI_MAX_DEVICES; i++) {
		struct ubi_device *ubi = ubi_devices[i];

		if (ubi && MAJOR(ubi->cdev.dev) == major) {
			ubi_num = ubi->ubi_num;
			break;
		}
	}
	spin_unlock(&ubi_devices_lock);

	return ubi_num;
}

static ssize_t dev_attribute_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	struct ubi_device *ubi;

	ubi = container_of(dev, struct ubi_device, dev);
	ubi = ubi_get_device(ubi->ubi_num);
	if (!ubi)
		return -ENODEV;

	if (attr == &dev_eraseblock_size)
		ret = sprintf(buf, "%d\n", ubi->leb_size);
	else if (attr == &dev_avail_eraseblocks)
		ret = sprintf(buf, "%d\n", ubi->avail_pebs);
	else if (attr == &dev_total_eraseblocks)
		ret = sprintf(buf, "%d\n", ubi->good_peb_count);
	else if (attr == &dev_volumes_count)
		ret = sprintf(buf, "%d\n", ubi->vol_count - UBI_INT_VOL_COUNT);
	else if (attr == &dev_max_ec)
		ret = sprintf(buf, "%d\n", ubi->max_ec);
	else if (attr == &dev_reserved_for_bad)
		ret = sprintf(buf, "%d\n", ubi->beb_rsvd_pebs);
	else if (attr == &dev_bad_peb_count)
		ret = sprintf(buf, "%d\n", ubi->bad_peb_count);
	else if (attr == &dev_max_vol_count)
		ret = sprintf(buf, "%d\n", ubi->vtbl_slots);
	else if (attr == &dev_min_io_size)
		ret = sprintf(buf, "%d\n", ubi->min_io_size);
	else if (attr == &dev_bgt_enabled)
		ret = sprintf(buf, "%d\n", ubi->thread_enabled);
	else if (attr == &dev_mtd_num)
		ret = sprintf(buf, "%d\n", ubi->mtd->index);
	else
		ret = -EINVAL;

	ubi_put_device(ubi);
	return ret;
}

static void dev_release(struct device *dev)
{
	struct ubi_device *ubi = container_of(dev, struct ubi_device, dev);

	kfree(ubi);
}

static int ubi_sysfs_init(struct ubi_device *ubi, int *ref)
{
	int err;

	ubi->dev.release = dev_release;
	ubi->dev.devt = ubi->cdev.dev;
	ubi->dev.class = ubi_class;
	dev_set_name(&ubi->dev, UBI_NAME_STR"%d", ubi->ubi_num);
	err = device_register(&ubi->dev);
	if (err)
		return err;

	*ref = 1;
	err = device_create_file(&ubi->dev, &dev_eraseblock_size);
	if (err)
		return err;
	err = device_create_file(&ubi->dev, &dev_avail_eraseblocks);
	if (err)
		return err;
	err = device_create_file(&ubi->dev, &dev_total_eraseblocks);
	if (err)
		return err;
	err = device_create_file(&ubi->dev, &dev_volumes_count);
	if (err)
		return err;
	err = device_create_file(&ubi->dev, &dev_max_ec);
	if (err)
		return err;
	err = device_create_file(&ubi->dev, &dev_reserved_for_bad);
	if (err)
		return err;
	err = device_create_file(&ubi->dev, &dev_bad_peb_count);
	if (err)
		return err;
	err = device_create_file(&ubi->dev, &dev_max_vol_count);
	if (err)
		return err;
	err = device_create_file(&ubi->dev, &dev_min_io_size);
	if (err)
		return err;
	err = device_create_file(&ubi->dev, &dev_bgt_enabled);
	if (err)
		return err;
	err = device_create_file(&ubi->dev, &dev_mtd_num);
	return err;
}

static void ubi_sysfs_close(struct ubi_device *ubi)
{
	device_remove_file(&ubi->dev, &dev_mtd_num);
	device_remove_file(&ubi->dev, &dev_bgt_enabled);
	device_remove_file(&ubi->dev, &dev_min_io_size);
	device_remove_file(&ubi->dev, &dev_max_vol_count);
	device_remove_file(&ubi->dev, &dev_bad_peb_count);
	device_remove_file(&ubi->dev, &dev_reserved_for_bad);
	device_remove_file(&ubi->dev, &dev_max_ec);
	device_remove_file(&ubi->dev, &dev_volumes_count);
	device_remove_file(&ubi->dev, &dev_total_eraseblocks);
	device_remove_file(&ubi->dev, &dev_avail_eraseblocks);
	device_remove_file(&ubi->dev, &dev_eraseblock_size);
	device_unregister(&ubi->dev);
}

static void kill_volumes(struct ubi_device *ubi)
{
	int i;

	for (i = 0; i < ubi->vtbl_slots; i++)
		if (ubi->volumes[i])
			ubi_free_volume(ubi, ubi->volumes[i]);
}

static int uif_init(struct ubi_device *ubi, int *ref)
{
	int i, err;
	dev_t dev;

	*ref = 0;
	sprintf(ubi->ubi_name, UBI_NAME_STR "%d", ubi->ubi_num);

	err = alloc_chrdev_region(&dev, 0, ubi->vtbl_slots + 1, ubi->ubi_name);
	if (err) {
		ubi_err("cannot register UBI character devices");
		return err;
	}

	ubi_assert(MINOR(dev) == 0);
	cdev_init(&ubi->cdev, &ubi_cdev_operations);
	dbg_gen("%s major is %u", ubi->ubi_name, MAJOR(dev));
	ubi->cdev.owner = THIS_MODULE;

	err = cdev_add(&ubi->cdev, dev, 1);
	if (err) {
		ubi_err("cannot add character device");
		goto out_unreg;
	}

	err = ubi_sysfs_init(ubi, ref);
	if (err)
		goto out_sysfs;

	for (i = 0; i < ubi->vtbl_slots; i++)
		if (ubi->volumes[i]) {
			err = ubi_add_volume(ubi, ubi->volumes[i]);
			if (err) {
				ubi_err("cannot add volume %d", i);
				goto out_volumes;
			}
		}

	return 0;

out_volumes:
	kill_volumes(ubi);
out_sysfs:
	if (*ref)
		get_device(&ubi->dev);
	ubi_sysfs_close(ubi);
	cdev_del(&ubi->cdev);
out_unreg:
	unregister_chrdev_region(ubi->cdev.dev, ubi->vtbl_slots + 1);
	ubi_err("cannot initialize UBI %s, error %d", ubi->ubi_name, err);
	return err;
}

static void uif_close(struct ubi_device *ubi)
{
	kill_volumes(ubi);
	ubi_sysfs_close(ubi);
	cdev_del(&ubi->cdev);
	unregister_chrdev_region(ubi->cdev.dev, ubi->vtbl_slots + 1);
}

static void free_internal_volumes(struct ubi_device *ubi)
{
	int i;

	for (i = ubi->vtbl_slots;
	     i < ubi->vtbl_slots + UBI_INT_VOL_COUNT; i++) {
		kfree(ubi->volumes[i]->eba_tbl);
		kfree(ubi->volumes[i]);
	}
}

static int attach_by_scanning(struct ubi_device *ubi)
{
	int err;
	struct ubi_scan_info *si;

	si = ubi_scan(ubi);
	if (IS_ERR(si))
		return PTR_ERR(si);

	ubi->bad_peb_count = si->bad_peb_count;
	ubi->good_peb_count = ubi->peb_count - ubi->bad_peb_count;
	ubi->corr_peb_count = si->corr_peb_count;
	ubi->max_ec = si->max_ec;
	ubi->mean_ec = si->mean_ec;
	ubi_msg("max. sequence number:       %llu", si->max_sqnum);

	err = ubi_read_volume_table(ubi, si);
	if (err)
		goto out_si;

	err = ubi_wl_init_scan(ubi, si);
	if (err)
		goto out_vtbl;

	err = ubi_eba_init_scan(ubi, si);
	if (err)
		goto out_wl;

	ubi_scan_destroy_si(si);
	return 0;

out_wl:
	ubi_wl_close(ubi);
out_vtbl:
	free_internal_volumes(ubi);
	vfree(ubi->vtbl);
out_si:
	ubi_scan_destroy_si(si);
	return err;
}

static int io_init(struct ubi_device *ubi)
{
	if (ubi->mtd->numeraseregions != 0) {
		ubi_err("multiple regions, not implemented");
		return -EINVAL;
	}

	if (ubi->vid_hdr_offset < 0)
		return -EINVAL;


	ubi->peb_size   = ubi->mtd->erasesize;
	ubi->peb_count  = mtd_div_by_eb(ubi->mtd->size, ubi->mtd);
	ubi->flash_size = ubi->mtd->size;

	if (mtd_can_have_bb(ubi->mtd))
		ubi->bad_allowed = 1;

	if (ubi->mtd->type == MTD_NORFLASH) {
		ubi_assert(ubi->mtd->writesize == 1);
		ubi->nor_flash = 1;
	}

	ubi->min_io_size = ubi->mtd->writesize;
	ubi->hdrs_min_io_size = ubi->mtd->writesize >> ubi->mtd->subpage_sft;

	if (!is_power_of_2(ubi->min_io_size)) {
		ubi_err("min. I/O unit (%d) is not power of 2",
			ubi->min_io_size);
		return -EINVAL;
	}

	ubi_assert(ubi->hdrs_min_io_size > 0);
	ubi_assert(ubi->hdrs_min_io_size <= ubi->min_io_size);
	ubi_assert(ubi->min_io_size % ubi->hdrs_min_io_size == 0);

	ubi->max_write_size = ubi->mtd->writebufsize;
	if (ubi->max_write_size < ubi->min_io_size ||
	    ubi->max_write_size % ubi->min_io_size ||
	    !is_power_of_2(ubi->max_write_size)) {
		ubi_err("bad write buffer size %d for %d min. I/O unit",
			ubi->max_write_size, ubi->min_io_size);
		return -EINVAL;
	}

	
	ubi->ec_hdr_alsize = ALIGN(UBI_EC_HDR_SIZE, ubi->hdrs_min_io_size);
	ubi->vid_hdr_alsize = ALIGN(UBI_VID_HDR_SIZE, ubi->hdrs_min_io_size);

	dbg_msg("min_io_size      %d", ubi->min_io_size);
	dbg_msg("max_write_size   %d", ubi->max_write_size);
	dbg_msg("hdrs_min_io_size %d", ubi->hdrs_min_io_size);
	dbg_msg("ec_hdr_alsize    %d", ubi->ec_hdr_alsize);
	dbg_msg("vid_hdr_alsize   %d", ubi->vid_hdr_alsize);

	if (ubi->vid_hdr_offset == 0)
		
		ubi->vid_hdr_offset = ubi->vid_hdr_aloffset =
				      ubi->ec_hdr_alsize;
	else {
		ubi->vid_hdr_aloffset = ubi->vid_hdr_offset &
						~(ubi->hdrs_min_io_size - 1);
		ubi->vid_hdr_shift = ubi->vid_hdr_offset -
						ubi->vid_hdr_aloffset;
	}

	
	ubi->leb_start = ubi->vid_hdr_offset + UBI_VID_HDR_SIZE;
	ubi->leb_start = ALIGN(ubi->leb_start, ubi->min_io_size);

	dbg_msg("vid_hdr_offset   %d", ubi->vid_hdr_offset);
	dbg_msg("vid_hdr_aloffset %d", ubi->vid_hdr_aloffset);
	dbg_msg("vid_hdr_shift    %d", ubi->vid_hdr_shift);
	dbg_msg("leb_start        %d", ubi->leb_start);

	
	if (ubi->vid_hdr_shift % 4) {
		ubi_err("unaligned VID header shift %d",
			ubi->vid_hdr_shift);
		return -EINVAL;
	}

	
	if (ubi->vid_hdr_offset < UBI_EC_HDR_SIZE ||
	    ubi->leb_start < ubi->vid_hdr_offset + UBI_VID_HDR_SIZE ||
	    ubi->leb_start > ubi->peb_size - UBI_VID_HDR_SIZE ||
	    ubi->leb_start & (ubi->min_io_size - 1)) {
		ubi_err("bad VID header (%d) or data offsets (%d)",
			ubi->vid_hdr_offset, ubi->leb_start);
		return -EINVAL;
	}

	ubi->max_erroneous = ubi->peb_count / 10;
	if (ubi->max_erroneous < 16)
		ubi->max_erroneous = 16;
	dbg_msg("max_erroneous    %d", ubi->max_erroneous);

	if (ubi->vid_hdr_offset + UBI_VID_HDR_SIZE <= ubi->hdrs_min_io_size) {
		ubi_warn("EC and VID headers are in the same minimal I/O unit, "
			 "switch to read-only mode");
		ubi->ro_mode = 1;
	}

	ubi->leb_size = ubi->peb_size - ubi->leb_start;

	if (!(ubi->mtd->flags & MTD_WRITEABLE)) {
		ubi_msg("MTD device %d is write-protected, attach in "
			"read-only mode", ubi->mtd->index);
		ubi->ro_mode = 1;
	}

	ubi_msg("physical eraseblock size:   %d bytes (%d KiB)",
		ubi->peb_size, ubi->peb_size >> 10);
	ubi_msg("logical eraseblock size:    %d bytes", ubi->leb_size);
	ubi_msg("smallest flash I/O unit:    %d", ubi->min_io_size);
	if (ubi->hdrs_min_io_size != ubi->min_io_size)
		ubi_msg("sub-page size:              %d",
			ubi->hdrs_min_io_size);
	ubi_msg("VID header offset:          %d (aligned %d)",
		ubi->vid_hdr_offset, ubi->vid_hdr_aloffset);
	ubi_msg("data offset:                %d", ubi->leb_start);


	return 0;
}

static int autoresize(struct ubi_device *ubi, int vol_id)
{
	struct ubi_volume_desc desc;
	struct ubi_volume *vol = ubi->volumes[vol_id];
	int err, old_reserved_pebs = vol->reserved_pebs;

	ubi->vtbl[vol_id].flags &= ~UBI_VTBL_AUTORESIZE_FLG;

	if (ubi->avail_pebs == 0) {
		struct ubi_vtbl_record vtbl_rec;

		memcpy(&vtbl_rec, &ubi->vtbl[vol_id],
		       sizeof(struct ubi_vtbl_record));
		err = ubi_change_vtbl_record(ubi, vol_id, &vtbl_rec);
		if (err)
			ubi_err("cannot clean auto-resize flag for volume %d",
				vol_id);
	} else {
		desc.vol = vol;
		err = ubi_resize_volume(&desc,
					old_reserved_pebs + ubi->avail_pebs);
		if (err)
			ubi_err("cannot auto-resize volume %d", vol_id);
	}

	if (err)
		return err;

	ubi_msg("volume %d (\"%s\") re-sized from %d to %d LEBs", vol_id,
		vol->name, old_reserved_pebs, vol->reserved_pebs);
	return 0;
}

int ubi_attach_mtd_dev(struct mtd_info *mtd, int ubi_num, int vid_hdr_offset)
{
	struct ubi_device *ubi;
	int i, err, ref = 0;

	for (i = 0; i < UBI_MAX_DEVICES; i++) {
		ubi = ubi_devices[i];
		if (ubi && mtd->index == ubi->mtd->index) {
			dbg_err("mtd%d is already attached to ubi%d",
				mtd->index, i);
			return -EEXIST;
		}
	}

	if (mtd->type == MTD_UBIVOLUME) {
		ubi_err("refuse attaching mtd%d - it is already emulated on "
			"top of UBI", mtd->index);
		return -EINVAL;
	}

	if (ubi_num == UBI_DEV_NUM_AUTO) {
		
		for (ubi_num = 0; ubi_num < UBI_MAX_DEVICES; ubi_num++)
			if (!ubi_devices[ubi_num])
				break;
		if (ubi_num == UBI_MAX_DEVICES) {
			dbg_err("only %d UBI devices may be created",
				UBI_MAX_DEVICES);
			return -ENFILE;
		}
	} else {
		if (ubi_num >= UBI_MAX_DEVICES)
			return -EINVAL;

		
		if (ubi_devices[ubi_num]) {
			dbg_err("ubi%d already exists", ubi_num);
			return -EEXIST;
		}
	}

	ubi = kzalloc(sizeof(struct ubi_device), GFP_KERNEL);
	if (!ubi)
		return -ENOMEM;

	ubi->mtd = mtd;
	ubi->ubi_num = ubi_num;
	ubi->vid_hdr_offset = vid_hdr_offset;
	ubi->autoresize_vol_id = -1;

	mutex_init(&ubi->buf_mutex);
	mutex_init(&ubi->ckvol_mutex);
	mutex_init(&ubi->device_mutex);
	spin_lock_init(&ubi->volumes_lock);

	ubi_msg("attaching mtd%d to ubi%d", mtd->index, ubi_num);
	dbg_msg("sizeof(struct ubi_scan_leb) %zu", sizeof(struct ubi_scan_leb));
	dbg_msg("sizeof(struct ubi_wl_entry) %zu", sizeof(struct ubi_wl_entry));

	err = io_init(ubi);
	if (err)
		goto out_free;

	err = -ENOMEM;
	ubi->peb_buf = vmalloc(ubi->peb_size);
	if (!ubi->peb_buf)
		goto out_free;

	err = ubi_debugging_init_dev(ubi);
	if (err)
		goto out_free;

	err = attach_by_scanning(ubi);
	if (err) {
		dbg_err("failed to attach by scanning, error %d", err);
		goto out_debugging;
	}

	if (ubi->autoresize_vol_id != -1) {
		err = autoresize(ubi, ubi->autoresize_vol_id);
		if (err)
			goto out_detach;
	}

	err = uif_init(ubi, &ref);
	if (err)
		goto out_detach;

	err = ubi_debugfs_init_dev(ubi);
	if (err)
		goto out_uif;

	ubi->bgt_thread = kthread_create(ubi_thread, ubi, ubi->bgt_name);
	if (IS_ERR(ubi->bgt_thread)) {
		err = PTR_ERR(ubi->bgt_thread);
		ubi_err("cannot spawn \"%s\", error %d", ubi->bgt_name,
			err);
		goto out_debugfs;
	}

	ubi_msg("attached mtd%d to ubi%d", mtd->index, ubi_num);
	ubi_msg("MTD device name:            \"%s\"", mtd->name);
	ubi_msg("MTD device size:            %llu MiB", ubi->flash_size >> 20);
	ubi_msg("number of good PEBs:        %d", ubi->good_peb_count);
	ubi_msg("number of bad PEBs:         %d", ubi->bad_peb_count);
	ubi_msg("number of corrupted PEBs:   %d", ubi->corr_peb_count);
	ubi_msg("max. allowed volumes:       %d", ubi->vtbl_slots);
	ubi_msg("wear-leveling threshold:    %d", CONFIG_MTD_UBI_WL_THRESHOLD);
	ubi_msg("number of internal volumes: %d", UBI_INT_VOL_COUNT);
	ubi_msg("number of user volumes:     %d",
		ubi->vol_count - UBI_INT_VOL_COUNT);
	ubi_msg("available PEBs:             %d", ubi->avail_pebs);
	ubi_msg("total number of reserved PEBs: %d", ubi->rsvd_pebs);
	ubi_msg("number of PEBs reserved for bad PEB handling: %d",
		ubi->beb_rsvd_pebs);
	ubi_msg("max/mean erase counter: %d/%d", ubi->max_ec, ubi->mean_ec);
	ubi_msg("image sequence number:  %d", ubi->image_seq);

	spin_lock(&ubi->wl_lock);
	ubi->thread_enabled = 1;
	wake_up_process(ubi->bgt_thread);
	spin_unlock(&ubi->wl_lock);

	ubi_devices[ubi_num] = ubi;
	ubi_notify_all(ubi, UBI_VOLUME_ADDED, NULL);
	return ubi_num;

out_debugfs:
	ubi_debugfs_exit_dev(ubi);
out_uif:
	get_device(&ubi->dev);
	ubi_assert(ref);
	uif_close(ubi);
out_detach:
	ubi_wl_close(ubi);
	free_internal_volumes(ubi);
	vfree(ubi->vtbl);
out_debugging:
	ubi_debugging_exit_dev(ubi);
out_free:
	vfree(ubi->peb_buf);
	if (ref)
		put_device(&ubi->dev);
	else
		kfree(ubi);
	return err;
}

int ubi_detach_mtd_dev(int ubi_num, int anyway)
{
	struct ubi_device *ubi;

	if (ubi_num < 0 || ubi_num >= UBI_MAX_DEVICES)
		return -EINVAL;

	ubi = ubi_get_device(ubi_num);
	if (!ubi)
		return -EINVAL;

	spin_lock(&ubi_devices_lock);
	put_device(&ubi->dev);
	ubi->ref_count -= 1;
	if (ubi->ref_count) {
		if (!anyway) {
			spin_unlock(&ubi_devices_lock);
			return -EBUSY;
		}
		
		ubi_err("%s reference count %d, destroy anyway",
			ubi->ubi_name, ubi->ref_count);
	}
	ubi_devices[ubi_num] = NULL;
	spin_unlock(&ubi_devices_lock);

	ubi_assert(ubi_num == ubi->ubi_num);
	ubi_notify_all(ubi, UBI_VOLUME_REMOVED, NULL);
	dbg_msg("detaching mtd%d from ubi%d", ubi->mtd->index, ubi_num);

	if (ubi->bgt_thread)
		kthread_stop(ubi->bgt_thread);

	get_device(&ubi->dev);

	ubi_debugfs_exit_dev(ubi);
	uif_close(ubi);
	ubi_wl_close(ubi);
	free_internal_volumes(ubi);
	vfree(ubi->vtbl);
	put_mtd_device(ubi->mtd);
	ubi_debugging_exit_dev(ubi);
	vfree(ubi->peb_buf);
	ubi_msg("mtd%d is detached from ubi%d", ubi->mtd->index, ubi->ubi_num);
	put_device(&ubi->dev);
	return 0;
}

static struct mtd_info * __init open_mtd_by_chdev(const char *mtd_dev)
{
	int err, major, minor, mode;
	struct path path;

	
	err = kern_path(mtd_dev, LOOKUP_FOLLOW, &path);
	if (err)
		return ERR_PTR(err);

	
	major = imajor(path.dentry->d_inode);
	minor = iminor(path.dentry->d_inode);
	mode = path.dentry->d_inode->i_mode;
	path_put(&path);
	if (major != MTD_CHAR_MAJOR || !S_ISCHR(mode))
		return ERR_PTR(-EINVAL);

	if (minor & 1)
		return ERR_PTR(-EINVAL);

	return get_mtd_device(NULL, minor / 2);
}

static struct mtd_info * __init open_mtd_device(const char *mtd_dev)
{
	struct mtd_info *mtd;
	int mtd_num;
	char *endp;

	mtd_num = simple_strtoul(mtd_dev, &endp, 0);
	if (*endp != '\0' || mtd_dev == endp) {
		mtd = get_mtd_device_nm(mtd_dev);
		if (IS_ERR(mtd) && PTR_ERR(mtd) == -ENODEV)
			
			mtd = open_mtd_by_chdev(mtd_dev);
	} else
		mtd = get_mtd_device(NULL, mtd_num);

	return mtd;
}

static int __init ubi_init(void)
{
	int err, i, k;

	
	BUILD_BUG_ON(sizeof(struct ubi_ec_hdr) != 64);
	BUILD_BUG_ON(sizeof(struct ubi_vid_hdr) != 64);

	if (mtd_devs > UBI_MAX_DEVICES) {
		ubi_err("too many MTD devices, maximum is %d", UBI_MAX_DEVICES);
		return -EINVAL;
	}

	
	ubi_class = class_create(THIS_MODULE, UBI_NAME_STR);
	if (IS_ERR(ubi_class)) {
		err = PTR_ERR(ubi_class);
		ubi_err("cannot create UBI class");
		goto out;
	}

	err = class_create_file(ubi_class, &ubi_version);
	if (err) {
		ubi_err("cannot create sysfs file");
		goto out_class;
	}

	err = misc_register(&ubi_ctrl_cdev);
	if (err) {
		ubi_err("cannot register device");
		goto out_version;
	}

	ubi_wl_entry_slab = kmem_cache_create("ubi_wl_entry_slab",
					      sizeof(struct ubi_wl_entry),
					      0, 0, NULL);
	if (!ubi_wl_entry_slab)
		goto out_dev_unreg;

	err = ubi_debugfs_init();
	if (err)
		goto out_slab;


	
	for (i = 0; i < mtd_devs; i++) {
		struct mtd_dev_param *p = &mtd_dev_param[i];
		struct mtd_info *mtd;

		cond_resched();

		mtd = open_mtd_device(p->name);
		if (IS_ERR(mtd)) {
			err = PTR_ERR(mtd);
			goto out_detach;
		}

		mutex_lock(&ubi_devices_mutex);
		err = ubi_attach_mtd_dev(mtd, UBI_DEV_NUM_AUTO,
					 p->vid_hdr_offs);
		mutex_unlock(&ubi_devices_mutex);
		if (err < 0) {
			ubi_err("cannot attach mtd%d", mtd->index);
			put_mtd_device(mtd);

			if (ubi_is_module())
				goto out_detach;
		}
	}

	return 0;

out_detach:
	for (k = 0; k < i; k++)
		if (ubi_devices[k]) {
			mutex_lock(&ubi_devices_mutex);
			ubi_detach_mtd_dev(ubi_devices[k]->ubi_num, 1);
			mutex_unlock(&ubi_devices_mutex);
		}
	ubi_debugfs_exit();
out_slab:
	kmem_cache_destroy(ubi_wl_entry_slab);
out_dev_unreg:
	misc_deregister(&ubi_ctrl_cdev);
out_version:
	class_remove_file(ubi_class, &ubi_version);
out_class:
	class_destroy(ubi_class);
out:
	ubi_err("UBI error: cannot initialize UBI, error %d", err);
	return err;
}
module_init(ubi_init);

static void __exit ubi_exit(void)
{
	int i;

	for (i = 0; i < UBI_MAX_DEVICES; i++)
		if (ubi_devices[i]) {
			mutex_lock(&ubi_devices_mutex);
			ubi_detach_mtd_dev(ubi_devices[i]->ubi_num, 1);
			mutex_unlock(&ubi_devices_mutex);
		}
	ubi_debugfs_exit();
	kmem_cache_destroy(ubi_wl_entry_slab);
	misc_deregister(&ubi_ctrl_cdev);
	class_remove_file(ubi_class, &ubi_version);
	class_destroy(ubi_class);
}
module_exit(ubi_exit);

static int __init bytes_str_to_int(const char *str)
{
	char *endp;
	unsigned long result;

	result = simple_strtoul(str, &endp, 0);
	if (str == endp || result >= INT_MAX) {
		printk(KERN_ERR "UBI error: incorrect bytes count: \"%s\"\n",
		       str);
		return -EINVAL;
	}

	switch (*endp) {
	case 'G':
		result *= 1024;
	case 'M':
		result *= 1024;
	case 'K':
		result *= 1024;
		if (endp[1] == 'i' && endp[2] == 'B')
			endp += 2;
	case '\0':
		break;
	default:
		printk(KERN_ERR "UBI error: incorrect bytes count: \"%s\"\n",
		       str);
		return -EINVAL;
	}

	return result;
}

static int __init ubi_mtd_param_parse(const char *val, struct kernel_param *kp)
{
	int i, len;
	struct mtd_dev_param *p;
	char buf[MTD_PARAM_LEN_MAX];
	char *pbuf = &buf[0];
	char *tokens[2] = {NULL, NULL};

	if (!val)
		return -EINVAL;

	if (mtd_devs == UBI_MAX_DEVICES) {
		printk(KERN_ERR "UBI error: too many parameters, max. is %d\n",
		       UBI_MAX_DEVICES);
		return -EINVAL;
	}

	len = strnlen(val, MTD_PARAM_LEN_MAX);
	if (len == MTD_PARAM_LEN_MAX) {
		printk(KERN_ERR "UBI error: parameter \"%s\" is too long, "
		       "max. is %d\n", val, MTD_PARAM_LEN_MAX);
		return -EINVAL;
	}

	if (len == 0) {
		printk(KERN_WARNING "UBI warning: empty 'mtd=' parameter - "
		       "ignored\n");
		return 0;
	}

	strcpy(buf, val);

	
	if (buf[len - 1] == '\n')
		buf[len - 1] = '\0';

	for (i = 0; i < 2; i++)
		tokens[i] = strsep(&pbuf, ",");

	if (pbuf) {
		printk(KERN_ERR "UBI error: too many arguments at \"%s\"\n",
		       val);
		return -EINVAL;
	}

	p = &mtd_dev_param[mtd_devs];
	strcpy(&p->name[0], tokens[0]);

	if (tokens[1])
		p->vid_hdr_offs = bytes_str_to_int(tokens[1]);

	if (p->vid_hdr_offs < 0)
		return p->vid_hdr_offs;

	mtd_devs += 1;
	return 0;
}

module_param_call(mtd, ubi_mtd_param_parse, NULL, NULL, 000);
MODULE_PARM_DESC(mtd, "MTD devices to attach. Parameter format: "
		      "mtd=<name|num|path>[,<vid_hdr_offs>].\n"
		      "Multiple \"mtd\" parameters may be specified.\n"
		      "MTD devices may be specified by their number, name, or "
		      "path to the MTD character device node.\n"
		      "Optional \"vid_hdr_offs\" parameter specifies UBI VID "
		      "header position to be used by UBI.\n"
		      "Example 1: mtd=/dev/mtd0 - attach MTD device "
		      "/dev/mtd0.\n"
		      "Example 2: mtd=content,1984 mtd=4 - attach MTD device "
		      "with name \"content\" using VID header offset 1984, and "
		      "MTD device number 4 with default VID header offset.");

MODULE_VERSION(__stringify(UBI_VERSION));
MODULE_DESCRIPTION("UBI - Unsorted Block Images");
MODULE_AUTHOR("Artem Bityutskiy");
MODULE_LICENSE("GPL");
