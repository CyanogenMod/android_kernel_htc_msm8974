/* -*- mode: c; c-basic-offset: 8; -*-
 * vim: noexpandtab sw=8 ts=8 sts=0:
 *
 * ocfs1_fs_compat.h
 *
 * OCFS1 volume header definitions.  OCFS2 creates valid but unmountable
 * OCFS1 volume headers on the first two sectors of an OCFS2 volume.
 * This allows an OCFS1 volume to see the partition and cleanly fail to
 * mount it.
 *
 * Copyright (C) 2002, 2004 Oracle.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License, version 2,  as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 */

#ifndef _OCFS1_FS_COMPAT_H
#define _OCFS1_FS_COMPAT_H

#define OCFS1_MAX_VOL_SIGNATURE_LEN          128
#define OCFS1_MAX_MOUNT_POINT_LEN            128
#define OCFS1_MAX_VOL_ID_LENGTH               16
#define OCFS1_MAX_VOL_LABEL_LEN               64
#define OCFS1_MAX_CLUSTER_NAME_LEN            64

#define OCFS1_MAJOR_VERSION              (2)
#define OCFS1_MINOR_VERSION              (0)
#define OCFS1_VOLUME_SIGNATURE		 "OracleCFS"

struct ocfs1_vol_disk_hdr
{
	__u32 minor_version;
	__u32 major_version;
	__u8 signature[OCFS1_MAX_VOL_SIGNATURE_LEN];
	__u8 mount_point[OCFS1_MAX_MOUNT_POINT_LEN];
	__u64 serial_num;
	__u64 device_size;
	__u64 start_off;
	__u64 bitmap_off;
	__u64 publ_off;
	__u64 vote_off;
	__u64 root_bitmap_off;
	__u64 data_start_off;
	__u64 root_bitmap_size;
	__u64 root_off;
	__u64 root_size;
	__u64 cluster_size;
	__u64 num_nodes;
	__u64 num_clusters;
	__u64 dir_node_size;
	__u64 file_node_size;
	__u64 internal_off;
	__u64 node_cfg_off;
	__u64 node_cfg_size;
	__u64 new_cfg_off;
	__u32 prot_bits;
	__s32 excl_mount;
};


struct ocfs1_disk_lock
{
	__u32 curr_master;
	__u8 file_lock;
	__u8 compat_pad[3];  
	__u64 last_write_time;
	__u64 last_read_time;
	__u32 writer_node_num;
	__u32 reader_node_num;
	__u64 oin_node_map;
	__u64 dlock_seq_num;
};

struct ocfs1_vol_label
{
	struct ocfs1_disk_lock disk_lock;
	__u8 label[OCFS1_MAX_VOL_LABEL_LEN];
	__u16 label_len;
	__u8 vol_id[OCFS1_MAX_VOL_ID_LENGTH];
	__u16 vol_id_len;
	__u8 cluster_name[OCFS1_MAX_CLUSTER_NAME_LEN];
	__u16 cluster_name_len;
};


#endif 

