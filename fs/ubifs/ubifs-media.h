/*
 * This file is part of UBIFS.
 *
 * Copyright (C) 2006-2008 Nokia Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Authors: Artem Bityutskiy (Битюцкий Артём)
 *          Adrian Hunter
 */


#ifndef __UBIFS_MEDIA_H__
#define __UBIFS_MEDIA_H__

#define UBIFS_NODE_MAGIC  0x06101831

#define UBIFS_FORMAT_VERSION 4

#define UBIFS_RO_COMPAT_VERSION 0

#define UBIFS_MIN_LEB_SZ (15*1024)

#define UBIFS_CRC32_INIT 0xFFFFFFFFU

#define UBIFS_MIN_COMPR_LEN 128

#define UBIFS_MIN_COMPRESS_DIFF 64

#define UBIFS_ROOT_INO 1

#define UBIFS_FIRST_INO 64

#define UBIFS_MAX_NLEN 255

#define UBIFS_MAX_JHEADS 1

#define UBIFS_BLOCK_SIZE  4096
#define UBIFS_BLOCK_SHIFT 12

#define UBIFS_PADDING_BYTE 0xCE

#define UBIFS_MAX_KEY_LEN 16

#define UBIFS_SK_LEN 8

#define UBIFS_MIN_FANOUT 3

#define UBIFS_MAX_LEVELS 512

#define UBIFS_MAX_INO_DATA UBIFS_BLOCK_SIZE

#define UBIFS_LPT_FANOUT 4
#define UBIFS_LPT_FANOUT_SHIFT 2

#define UBIFS_LPT_CRC_BITS 16
#define UBIFS_LPT_CRC_BYTES 2
#define UBIFS_LPT_TYPE_BITS 4

#define UBIFS_KEY_OFFSET offsetof(struct ubifs_ino_node, key)

#define UBIFS_GC_HEAD   0
#define UBIFS_BASE_HEAD 1
#define UBIFS_DATA_HEAD 2

enum {
	UBIFS_LPT_PNODE,
	UBIFS_LPT_NNODE,
	UBIFS_LPT_LTAB,
	UBIFS_LPT_LSAVE,
	UBIFS_LPT_NODE_CNT,
	UBIFS_LPT_NOT_A_NODE = (1 << UBIFS_LPT_TYPE_BITS) - 1,
};

enum {
	UBIFS_ITYPE_REG,
	UBIFS_ITYPE_DIR,
	UBIFS_ITYPE_LNK,
	UBIFS_ITYPE_BLK,
	UBIFS_ITYPE_CHR,
	UBIFS_ITYPE_FIFO,
	UBIFS_ITYPE_SOCK,
	UBIFS_ITYPES_CNT,
};

enum {
	UBIFS_KEY_HASH_R5,
	UBIFS_KEY_HASH_TEST,
};

enum {
	UBIFS_SIMPLE_KEY_FMT,
};

#define UBIFS_S_KEY_BLOCK_BITS 29
#define UBIFS_S_KEY_BLOCK_MASK 0x1FFFFFFF
#define UBIFS_S_KEY_HASH_BITS  UBIFS_S_KEY_BLOCK_BITS
#define UBIFS_S_KEY_HASH_MASK  UBIFS_S_KEY_BLOCK_MASK

enum {
	UBIFS_INO_KEY,
	UBIFS_DATA_KEY,
	UBIFS_DENT_KEY,
	UBIFS_XENT_KEY,
	UBIFS_KEY_TYPES_CNT,
};

#define UBIFS_SB_LEBS 1
#define UBIFS_MST_LEBS 2

#define UBIFS_SB_LNUM 0
#define UBIFS_MST_LNUM (UBIFS_SB_LNUM + UBIFS_SB_LEBS)
#define UBIFS_LOG_LNUM (UBIFS_MST_LNUM + UBIFS_MST_LEBS)


#define UBIFS_MIN_LOG_LEBS 2
#define UBIFS_MIN_BUD_LEBS 3
#define UBIFS_MIN_JNL_LEBS (UBIFS_MIN_LOG_LEBS + UBIFS_MIN_BUD_LEBS)
#define UBIFS_MIN_LPT_LEBS 2
#define UBIFS_MIN_ORPH_LEBS 1
#define UBIFS_MIN_MAIN_LEBS (UBIFS_MIN_BUD_LEBS + 6)

#define UBIFS_MIN_LEB_CNT (UBIFS_SB_LEBS + UBIFS_MST_LEBS + \
			   UBIFS_MIN_LOG_LEBS + UBIFS_MIN_LPT_LEBS + \
			   UBIFS_MIN_ORPH_LEBS + UBIFS_MIN_MAIN_LEBS)

#define UBIFS_CH_SZ        sizeof(struct ubifs_ch)
#define UBIFS_INO_NODE_SZ  sizeof(struct ubifs_ino_node)
#define UBIFS_DATA_NODE_SZ sizeof(struct ubifs_data_node)
#define UBIFS_DENT_NODE_SZ sizeof(struct ubifs_dent_node)
#define UBIFS_TRUN_NODE_SZ sizeof(struct ubifs_trun_node)
#define UBIFS_PAD_NODE_SZ  sizeof(struct ubifs_pad_node)
#define UBIFS_SB_NODE_SZ   sizeof(struct ubifs_sb_node)
#define UBIFS_MST_NODE_SZ  sizeof(struct ubifs_mst_node)
#define UBIFS_REF_NODE_SZ  sizeof(struct ubifs_ref_node)
#define UBIFS_IDX_NODE_SZ  sizeof(struct ubifs_idx_node)
#define UBIFS_CS_NODE_SZ   sizeof(struct ubifs_cs_node)
#define UBIFS_ORPH_NODE_SZ sizeof(struct ubifs_orph_node)
#define UBIFS_XENT_NODE_SZ UBIFS_DENT_NODE_SZ
#define UBIFS_BRANCH_SZ    sizeof(struct ubifs_branch)

#define UBIFS_MAX_DATA_NODE_SZ  (UBIFS_DATA_NODE_SZ + UBIFS_BLOCK_SIZE)
#define UBIFS_MAX_INO_NODE_SZ   (UBIFS_INO_NODE_SZ + UBIFS_MAX_INO_DATA)
#define UBIFS_MAX_DENT_NODE_SZ  (UBIFS_DENT_NODE_SZ + UBIFS_MAX_NLEN + 1)
#define UBIFS_MAX_XENT_NODE_SZ  UBIFS_MAX_DENT_NODE_SZ

#define UBIFS_MAX_NODE_SZ UBIFS_MAX_INO_NODE_SZ

enum {
	UBIFS_COMPR_FL     = 0x01,
	UBIFS_SYNC_FL      = 0x02,
	UBIFS_IMMUTABLE_FL = 0x04,
	UBIFS_APPEND_FL    = 0x08,
	UBIFS_DIRSYNC_FL   = 0x10,
	UBIFS_XATTR_FL     = 0x20,
};

#define UBIFS_FL_MASK 0x0000001F

enum {
	UBIFS_COMPR_NONE,
	UBIFS_COMPR_LZO,
	UBIFS_COMPR_ZLIB,
	UBIFS_COMPR_TYPES_CNT,
};

enum {
	UBIFS_INO_NODE,
	UBIFS_DATA_NODE,
	UBIFS_DENT_NODE,
	UBIFS_XENT_NODE,
	UBIFS_TRUN_NODE,
	UBIFS_PAD_NODE,
	UBIFS_SB_NODE,
	UBIFS_MST_NODE,
	UBIFS_REF_NODE,
	UBIFS_IDX_NODE,
	UBIFS_CS_NODE,
	UBIFS_ORPH_NODE,
	UBIFS_NODE_TYPES_CNT,
};

/*
 * Master node flags.
 *
 * UBIFS_MST_DIRTY: rebooted uncleanly - master node is dirty
 * UBIFS_MST_NO_ORPHS: no orphan inodes present
 * UBIFS_MST_RCVRY: written by recovery
 */
enum {
	UBIFS_MST_DIRTY = 1,
	UBIFS_MST_NO_ORPHS = 2,
	UBIFS_MST_RCVRY = 4,
};

enum {
	UBIFS_NO_NODE_GROUP = 0,
	UBIFS_IN_NODE_GROUP,
	UBIFS_LAST_OF_NODE_GROUP,
};

enum {
	UBIFS_FLG_BIGLPT = 0x02,
	UBIFS_FLG_SPACE_FIXUP = 0x04,
};

struct ubifs_ch {
	__le32 magic;
	__le32 crc;
	__le64 sqnum;
	__le32 len;
	__u8 node_type;
	__u8 group_type;
	__u8 padding[2];
} __packed;

union ubifs_dev_desc {
	__le32 new;
	__le64 huge;
} __packed;

struct ubifs_ino_node {
	struct ubifs_ch ch;
	__u8 key[UBIFS_MAX_KEY_LEN];
	__le64 creat_sqnum;
	__le64 size;
	__le64 atime_sec;
	__le64 ctime_sec;
	__le64 mtime_sec;
	__le32 atime_nsec;
	__le32 ctime_nsec;
	__le32 mtime_nsec;
	__le32 nlink;
	__le32 uid;
	__le32 gid;
	__le32 mode;
	__le32 flags;
	__le32 data_len;
	__le32 xattr_cnt;
	__le32 xattr_size;
	__u8 padding1[4]; 
	__le32 xattr_names;
	__le16 compr_type;
	__u8 padding2[26]; 
	__u8 data[];
} __packed;

struct ubifs_dent_node {
	struct ubifs_ch ch;
	__u8 key[UBIFS_MAX_KEY_LEN];
	__le64 inum;
	__u8 padding1;
	__u8 type;
	__le16 nlen;
	__u8 padding2[4]; 
	__u8 name[];
} __packed;

struct ubifs_data_node {
	struct ubifs_ch ch;
	__u8 key[UBIFS_MAX_KEY_LEN];
	__le32 size;
	__le16 compr_type;
	__u8 padding[2]; 
	__u8 data[];
} __packed;

struct ubifs_trun_node {
	struct ubifs_ch ch;
	__le32 inum;
	__u8 padding[12]; 
	__le64 old_size;
	__le64 new_size;
} __packed;

struct ubifs_pad_node {
	struct ubifs_ch ch;
	__le32 pad_len;
} __packed;

struct ubifs_sb_node {
	struct ubifs_ch ch;
	__u8 padding[2];
	__u8 key_hash;
	__u8 key_fmt;
	__le32 flags;
	__le32 min_io_size;
	__le32 leb_size;
	__le32 leb_cnt;
	__le32 max_leb_cnt;
	__le64 max_bud_bytes;
	__le32 log_lebs;
	__le32 lpt_lebs;
	__le32 orph_lebs;
	__le32 jhead_cnt;
	__le32 fanout;
	__le32 lsave_cnt;
	__le32 fmt_version;
	__le16 default_compr;
	__u8 padding1[2];
	__le32 rp_uid;
	__le32 rp_gid;
	__le64 rp_size;
	__le32 time_gran;
	__u8 uuid[16];
	__le32 ro_compat_version;
	__u8 padding2[3968];
} __packed;

struct ubifs_mst_node {
	struct ubifs_ch ch;
	__le64 highest_inum;
	__le64 cmt_no;
	__le32 flags;
	__le32 log_lnum;
	__le32 root_lnum;
	__le32 root_offs;
	__le32 root_len;
	__le32 gc_lnum;
	__le32 ihead_lnum;
	__le32 ihead_offs;
	__le64 index_size;
	__le64 total_free;
	__le64 total_dirty;
	__le64 total_used;
	__le64 total_dead;
	__le64 total_dark;
	__le32 lpt_lnum;
	__le32 lpt_offs;
	__le32 nhead_lnum;
	__le32 nhead_offs;
	__le32 ltab_lnum;
	__le32 ltab_offs;
	__le32 lsave_lnum;
	__le32 lsave_offs;
	__le32 lscan_lnum;
	__le32 empty_lebs;
	__le32 idx_lebs;
	__le32 leb_cnt;
	__u8 padding[344];
} __packed;

struct ubifs_ref_node {
	struct ubifs_ch ch;
	__le32 lnum;
	__le32 offs;
	__le32 jhead;
	__u8 padding[28];
} __packed;

struct ubifs_branch {
	__le32 lnum;
	__le32 offs;
	__le32 len;
	__u8 key[];
} __packed;

struct ubifs_idx_node {
	struct ubifs_ch ch;
	__le16 child_cnt;
	__le16 level;
	__u8 branches[];
} __packed;

struct ubifs_cs_node {
	struct ubifs_ch ch;
	__le64 cmt_no;
} __packed;

struct ubifs_orph_node {
	struct ubifs_ch ch;
	__le64 cmt_no;
	__le64 inos[];
} __packed;

#endif 
