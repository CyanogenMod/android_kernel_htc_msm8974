/*
 * common.h - Common definitions for both Kernel and user-mode utilities
 *
 * Copyright (C) 2005, 2006
 * Avishay Traeger (avishay@gmail.com)
 * Copyright (C) 2008, 2009
 * Boaz Harrosh <bharrosh@panasas.com>
 *
 * Copyrights for code taken from ext2:
 *     Copyright (C) 1992, 1993, 1994, 1995
 *     Remy Card (card@masi.ibp.fr)
 *     Laboratoire MASI - Institut Blaise Pascal
 *     Universite Pierre et Marie Curie (Paris VI)
 *     from
 *     linux/fs/minix/inode.c
 *     Copyright (C) 1991, 1992  Linus Torvalds
 *
 * This file is part of exofs.
 *
 * exofs is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.  Since it is based on ext2, and the only
 * valid version of GPL for the Linux kernel is version 2, the only valid
 * version of GPL for exofs is version 2.
 *
 * exofs is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with exofs; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __EXOFS_COM_H__
#define __EXOFS_COM_H__

#include <linux/types.h>

#include <scsi/osd_attributes.h>
#include <scsi/osd_initiator.h>
#include <scsi/osd_sec.h>

#define EXOFS_MIN_PID   0x10000	
#define EXOFS_OBJ_OFF	0x10000	
#define EXOFS_SUPER_ID	0x10000	
#define EXOFS_DEVTABLE_ID 0x10001 
#define EXOFS_ROOT_ID	0x10002	

# define EXOFS_APAGE_FS_DATA	(OSD_APAGE_APP_DEFINED_FIRST + 3)
# define EXOFS_ATTR_INODE_DATA	1
# define EXOFS_ATTR_INODE_FILE_LAYOUT	2
# define EXOFS_ATTR_INODE_DIR_LAYOUT	3
# define EXOFS_APAGE_SB_DATA	(0xF0000000U + 3)
# define EXOFS_ATTR_SB_STATS	1

enum {
	EXOFS_MAX_INO_ID = (sizeof(ino_t) * 8 == 64) ? ULLONG_MAX :
					(1ULL << (sizeof(ino_t) * 8ULL - 1ULL)),
	EXOFS_MAX_ID	 = (EXOFS_MAX_INO_ID - 1 - EXOFS_OBJ_OFF),
};

#define EXOFS_BLKSHIFT	12
#define EXOFS_BLKSIZE	(1UL << EXOFS_BLKSHIFT)

#define EXOFS_SUPER_MAGIC	0x5DF5

enum {EXOFS_FSCB_VER = 1, EXOFS_DT_VER = 1};
struct exofs_fscb {
	__le64  s_nextid;	
	__le64  s_numfiles;	
	__le32	s_version;	
	__le16  s_magic;	
	__le16  s_newfs;	

	/* From here on it's a static part, only written by mkexofs */
	__le64	s_dev_table_oid;   
	__le64	s_dev_table_count; 
} __packed;

/*
 * This struct is set on the FS partition's attributes.
 * [EXOFS_APAGE_SB_DATA, EXOFS_ATTR_SB_STATS] and is written together
 * with the create command, to atomically persist the sb writeable information.
 */
struct exofs_sb_stats {
	__le64  s_nextid;	
	__le64  s_numfiles;	
} __packed;

struct exofs_dt_data_map {
	__le32	cb_num_comps;
	__le64	cb_stripe_unit;
	__le32	cb_group_width;
	__le32	cb_group_depth;
	__le32	cb_mirror_cnt;
	__le32	cb_raid_algorithm;
} __packed;

struct exofs_dt_device_info {
	__le32	systemid_len;
	u8	systemid[OSD_SYSTEMID_LEN];
	__le64	long_name_offset;	
	__le32	osdname_len;		
	u8	osdname[44];		
} __packed;

struct exofs_device_table {
	__le32				dt_version;	
	struct exofs_dt_data_map	dt_data_map;	

	__le64				__Resurved[4];

	__le64				dt_num_devices;	
	struct exofs_dt_device_info	dt_dev_table[];	
} __packed;

#define EXOFS_IDATA		5

struct exofs_fcb {
	__le64  i_size;			
	__le16  i_mode;         	
	__le16  i_links_count;  	
	__le32  i_uid;          	
	__le32  i_gid;          	
	__le32  i_atime;        	
	__le32  i_ctime;        	
	__le32  i_mtime;        	
	__le32  i_flags;        	
	__le32  i_generation;   	
	__le32  i_data[EXOFS_IDATA];	
};

#define EXOFS_INO_ATTR_SIZE	sizeof(struct exofs_fcb)

static const struct __weak osd_attr g_attr_inode_data = ATTR_DEF(
	EXOFS_APAGE_FS_DATA,
	EXOFS_ATTR_INODE_DATA,
	EXOFS_INO_ATTR_SIZE);

#define EXOFS_NAME_LEN	255

struct exofs_dir_entry {
	__le64		inode_no;		
	__le16		rec_len;		
	u8		name_len;		
	u8		file_type;		
	char		name[EXOFS_NAME_LEN];	
};

enum {
	EXOFS_FT_UNKNOWN,
	EXOFS_FT_REG_FILE,
	EXOFS_FT_DIR,
	EXOFS_FT_CHRDEV,
	EXOFS_FT_BLKDEV,
	EXOFS_FT_FIFO,
	EXOFS_FT_SOCK,
	EXOFS_FT_SYMLINK,
	EXOFS_FT_MAX
};

#define EXOFS_DIR_PAD			4
#define EXOFS_DIR_ROUND			(EXOFS_DIR_PAD - 1)
#define EXOFS_DIR_REC_LEN(name_len) \
	(((name_len) + offsetof(struct exofs_dir_entry, name)  + \
	  EXOFS_DIR_ROUND) & ~EXOFS_DIR_ROUND)


enum exofs_inode_layout_gen_functions {
	LAYOUT_MOVING_WINDOW = 0,
	LAYOUT_IMPLICT = 1,
};

struct exofs_on_disk_inode_layout {
	__le16 gen_func; 
	__le16 pad;
	union {
		
		struct exofs_layout_sliding_window {
			__le32 num_devices; 
		} sliding_window __packed;

		
		struct exofs_layout_implict_list {
			struct exofs_dt_data_map data_map;
			__le32 dev_indexes[];
		} implict __packed;
	};
} __packed;

static inline size_t exofs_on_disk_inode_layout_size(unsigned max_devs)
{
	return sizeof(struct exofs_on_disk_inode_layout) +
		max_devs * sizeof(__le32);
}

#endif 
