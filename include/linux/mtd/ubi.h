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

#ifndef __LINUX_UBI_H__
#define __LINUX_UBI_H__

#include <linux/ioctl.h>
#include <linux/types.h>
#include <mtd/ubi-user.h>

enum {
	UBI_READONLY = 1,
	UBI_READWRITE,
	UBI_EXCLUSIVE
};

struct ubi_volume_info {
	int ubi_num;
	int vol_id;
	int size;
	long long used_bytes;
	int used_ebs;
	int vol_type;
	int corrupted;
	int upd_marker;
	int alignment;
	int usable_leb_size;
	int name_len;
	const char *name;
	dev_t cdev;
};

struct ubi_device_info {
	int ubi_num;
	int leb_size;
	int leb_start;
	int min_io_size;
	int max_write_size;
	int ro_mode;
	dev_t cdev;
};

/*
 * Volume notification types.
 * @UBI_VOLUME_ADDED: a volume has been added (an UBI device was attached or a
 *                    volume was created)
 * @UBI_VOLUME_REMOVED: a volume has been removed (an UBI device was detached
 *			or a volume was removed)
 * @UBI_VOLUME_RESIZED: a volume has been re-sized
 * @UBI_VOLUME_RENAMED: a volume has been re-named
 * @UBI_VOLUME_UPDATED: data has been written to a volume
 *
 * These constants define which type of event has happened when a volume
 * notification function is invoked.
 */
enum {
	UBI_VOLUME_ADDED,
	UBI_VOLUME_REMOVED,
	UBI_VOLUME_RESIZED,
	UBI_VOLUME_RENAMED,
	UBI_VOLUME_UPDATED,
};

struct ubi_notification {
	struct ubi_device_info di;
	struct ubi_volume_info vi;
};

struct ubi_volume_desc;

int ubi_get_device_info(int ubi_num, struct ubi_device_info *di);
void ubi_get_volume_info(struct ubi_volume_desc *desc,
			 struct ubi_volume_info *vi);
struct ubi_volume_desc *ubi_open_volume(int ubi_num, int vol_id, int mode);
struct ubi_volume_desc *ubi_open_volume_nm(int ubi_num, const char *name,
					   int mode);
struct ubi_volume_desc *ubi_open_volume_path(const char *pathname, int mode);

int ubi_register_volume_notifier(struct notifier_block *nb,
				 int ignore_existing);
int ubi_unregister_volume_notifier(struct notifier_block *nb);

void ubi_close_volume(struct ubi_volume_desc *desc);
int ubi_leb_read(struct ubi_volume_desc *desc, int lnum, char *buf, int offset,
		 int len, int check);
int ubi_leb_write(struct ubi_volume_desc *desc, int lnum, const void *buf,
		  int offset, int len, int dtype);
int ubi_leb_change(struct ubi_volume_desc *desc, int lnum, const void *buf,
		   int len, int dtype);
int ubi_leb_erase(struct ubi_volume_desc *desc, int lnum);
int ubi_leb_unmap(struct ubi_volume_desc *desc, int lnum);
int ubi_leb_map(struct ubi_volume_desc *desc, int lnum, int dtype);
int ubi_is_mapped(struct ubi_volume_desc *desc, int lnum);
int ubi_sync(int ubi_num);

static inline int ubi_read(struct ubi_volume_desc *desc, int lnum, char *buf,
			   int offset, int len)
{
	return ubi_leb_read(desc, lnum, buf, offset, len, 0);
}

static inline int ubi_write(struct ubi_volume_desc *desc, int lnum,
			    const void *buf, int offset, int len)
{
	return ubi_leb_write(desc, lnum, buf, offset, len, UBI_UNKNOWN);
}

static inline int ubi_change(struct ubi_volume_desc *desc, int lnum,
				    const void *buf, int len)
{
	return ubi_leb_change(desc, lnum, buf, len, UBI_UNKNOWN);
}

#endif 
