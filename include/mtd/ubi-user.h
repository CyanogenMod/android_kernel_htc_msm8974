/*
 * Copyright © International Business Machines Corp., 2006
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

#ifndef __UBI_USER_H__
#define __UBI_USER_H__

#include <linux/types.h>


#define UBI_VOL_NUM_AUTO (-1)
#define UBI_DEV_NUM_AUTO (-1)

#define UBI_MAX_VOLUME_NAME 127


#define UBI_IOC_MAGIC 'o'

#define UBI_IOCMKVOL _IOW(UBI_IOC_MAGIC, 0, struct ubi_mkvol_req)
#define UBI_IOCRMVOL _IOW(UBI_IOC_MAGIC, 1, __s32)
#define UBI_IOCRSVOL _IOW(UBI_IOC_MAGIC, 2, struct ubi_rsvol_req)
#define UBI_IOCRNVOL _IOW(UBI_IOC_MAGIC, 3, struct ubi_rnvol_req)


#define UBI_CTRL_IOC_MAGIC 'o'

#define UBI_IOCATT _IOW(UBI_CTRL_IOC_MAGIC, 64, struct ubi_attach_req)
#define UBI_IOCDET _IOW(UBI_CTRL_IOC_MAGIC, 65, __s32)


#define UBI_VOL_IOC_MAGIC 'O'

#define UBI_IOCVOLUP _IOW(UBI_VOL_IOC_MAGIC, 0, __s64)
#define UBI_IOCEBER _IOW(UBI_VOL_IOC_MAGIC, 1, __s32)
#define UBI_IOCEBCH _IOW(UBI_VOL_IOC_MAGIC, 2, __s32)
#define UBI_IOCEBMAP _IOW(UBI_VOL_IOC_MAGIC, 3, struct ubi_map_req)
#define UBI_IOCEBUNMAP _IOW(UBI_VOL_IOC_MAGIC, 4, __s32)
#define UBI_IOCEBISMAP _IOR(UBI_VOL_IOC_MAGIC, 5, __s32)
#define UBI_IOCSETVOLPROP _IOW(UBI_VOL_IOC_MAGIC, 6, \
			       struct ubi_set_vol_prop_req)

#define MAX_UBI_MTD_NAME_LEN 127

#define UBI_MAX_RNVOL 32

/*
 * UBI data type hint constants.
 *
 * UBI_LONGTERM: long-term data
 * UBI_SHORTTERM: short-term data
 * UBI_UNKNOWN: data persistence is unknown
 *
 * These constants are used when data is written to UBI volumes in order to
 * help the UBI wear-leveling unit to find more appropriate physical
 * eraseblocks.
 */
enum {
	UBI_LONGTERM  = 1,
	UBI_SHORTTERM = 2,
	UBI_UNKNOWN   = 3,
};

enum {
	UBI_DYNAMIC_VOLUME = 3,
	UBI_STATIC_VOLUME  = 4,
};

enum {
	UBI_VOL_PROP_DIRECT_WRITE = 1,
};

struct ubi_attach_req {
	__s32 ubi_num;
	__s32 mtd_num;
	__s32 vid_hdr_offset;
	__s8 padding[12];
};

struct ubi_mkvol_req {
	__s32 vol_id;
	__s32 alignment;
	__s64 bytes;
	__s8 vol_type;
	__s8 padding1;
	__s16 name_len;
	__s8 padding2[4];
	char name[UBI_MAX_VOLUME_NAME + 1];
} __packed;

struct ubi_rsvol_req {
	__s64 bytes;
	__s32 vol_id;
} __packed;

struct ubi_rnvol_req {
	__s32 count;
	__s8 padding1[12];
	struct {
		__s32 vol_id;
		__s16 name_len;
		__s8  padding2[2];
		char    name[UBI_MAX_VOLUME_NAME + 1];
	} ents[UBI_MAX_RNVOL];
} __packed;

/**
 * struct ubi_leb_change_req - a data structure used in atomic LEB change
 *                             requests.
 * @lnum: logical eraseblock number to change
 * @bytes: how many bytes will be written to the logical eraseblock
 * @dtype: data type (%UBI_LONGTERM, %UBI_SHORTTERM, %UBI_UNKNOWN)
 * @padding: reserved for future, not used, has to be zeroed
 */
struct ubi_leb_change_req {
	__s32 lnum;
	__s32 bytes;
	__s8  dtype;
	__s8  padding[7];
} __packed;

struct ubi_map_req {
	__s32 lnum;
	__s8  dtype;
	__s8  padding[3];
} __packed;


struct ubi_set_vol_prop_req {
	__u8  property;
	__u8  padding[7];
	__u64 value;
}  __packed;

#endif 
