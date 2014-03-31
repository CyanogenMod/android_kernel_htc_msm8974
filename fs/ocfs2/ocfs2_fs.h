/* -*- mode: c; c-basic-offset: 8; -*-
 * vim: noexpandtab sw=8 ts=8 sts=0:
 *
 * ocfs2_fs.h
 *
 * On-disk structures for OCFS2.
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

#ifndef _OCFS2_FS_H
#define _OCFS2_FS_H

#define OCFS2_MAJOR_REV_LEVEL		0
#define OCFS2_MINOR_REV_LEVEL          	90

#define OCFS2_SUPER_BLOCK_BLKNO		2

#define OCFS2_MIN_CLUSTERSIZE		4096
#define OCFS2_MAX_CLUSTERSIZE		1048576

#define OCFS2_MIN_BLOCKSIZE		512
#define OCFS2_MAX_BLOCKSIZE		OCFS2_MIN_CLUSTERSIZE

#define OCFS2_SUPER_MAGIC		0x7461636f

#define OCFS2_SUPER_BLOCK_SIGNATURE	"OCFSV2"
#define OCFS2_INODE_SIGNATURE		"INODE01"
#define OCFS2_EXTENT_BLOCK_SIGNATURE	"EXBLK01"
#define OCFS2_GROUP_DESC_SIGNATURE      "GROUP01"
#define OCFS2_XATTR_BLOCK_SIGNATURE	"XATTR01"
#define OCFS2_DIR_TRAILER_SIGNATURE	"DIRTRL1"
#define OCFS2_DX_ROOT_SIGNATURE		"DXDIR01"
#define OCFS2_DX_LEAF_SIGNATURE		"DXLEAF1"
#define OCFS2_REFCOUNT_BLOCK_SIGNATURE	"REFCNT1"

#define OCFS2_HAS_COMPAT_FEATURE(sb,mask)			\
	( OCFS2_SB(sb)->s_feature_compat & (mask) )
#define OCFS2_HAS_RO_COMPAT_FEATURE(sb,mask)			\
	( OCFS2_SB(sb)->s_feature_ro_compat & (mask) )
#define OCFS2_HAS_INCOMPAT_FEATURE(sb,mask)			\
	( OCFS2_SB(sb)->s_feature_incompat & (mask) )
#define OCFS2_SET_COMPAT_FEATURE(sb,mask)			\
	OCFS2_SB(sb)->s_feature_compat |= (mask)
#define OCFS2_SET_RO_COMPAT_FEATURE(sb,mask)			\
	OCFS2_SB(sb)->s_feature_ro_compat |= (mask)
#define OCFS2_SET_INCOMPAT_FEATURE(sb,mask)			\
	OCFS2_SB(sb)->s_feature_incompat |= (mask)
#define OCFS2_CLEAR_COMPAT_FEATURE(sb,mask)			\
	OCFS2_SB(sb)->s_feature_compat &= ~(mask)
#define OCFS2_CLEAR_RO_COMPAT_FEATURE(sb,mask)			\
	OCFS2_SB(sb)->s_feature_ro_compat &= ~(mask)
#define OCFS2_CLEAR_INCOMPAT_FEATURE(sb,mask)			\
	OCFS2_SB(sb)->s_feature_incompat &= ~(mask)

#define OCFS2_FEATURE_COMPAT_SUPP	(OCFS2_FEATURE_COMPAT_BACKUP_SB	\
					 | OCFS2_FEATURE_COMPAT_JBD2_SB)
#define OCFS2_FEATURE_INCOMPAT_SUPP	(OCFS2_FEATURE_INCOMPAT_LOCAL_MOUNT \
					 | OCFS2_FEATURE_INCOMPAT_SPARSE_ALLOC \
					 | OCFS2_FEATURE_INCOMPAT_INLINE_DATA \
					 | OCFS2_FEATURE_INCOMPAT_EXTENDED_SLOT_MAP \
					 | OCFS2_FEATURE_INCOMPAT_USERSPACE_STACK \
					 | OCFS2_FEATURE_INCOMPAT_XATTR \
					 | OCFS2_FEATURE_INCOMPAT_META_ECC \
					 | OCFS2_FEATURE_INCOMPAT_INDEXED_DIRS \
					 | OCFS2_FEATURE_INCOMPAT_REFCOUNT_TREE \
					 | OCFS2_FEATURE_INCOMPAT_DISCONTIG_BG	\
					 | OCFS2_FEATURE_INCOMPAT_CLUSTERINFO)
#define OCFS2_FEATURE_RO_COMPAT_SUPP	(OCFS2_FEATURE_RO_COMPAT_UNWRITTEN \
					 | OCFS2_FEATURE_RO_COMPAT_USRQUOTA \
					 | OCFS2_FEATURE_RO_COMPAT_GRPQUOTA)

#define OCFS2_FEATURE_INCOMPAT_HEARTBEAT_DEV	0x0002

#define OCFS2_FEATURE_INCOMPAT_RESIZE_INPROG    0x0004

#define OCFS2_FEATURE_INCOMPAT_LOCAL_MOUNT	0x0008

#define OCFS2_FEATURE_INCOMPAT_SPARSE_ALLOC	0x0010

#define OCFS2_FEATURE_INCOMPAT_TUNEFS_INPROG	0x0020

#define OCFS2_FEATURE_INCOMPAT_INLINE_DATA	0x0040

#define OCFS2_FEATURE_INCOMPAT_USERSPACE_STACK	0x0080

#define OCFS2_FEATURE_INCOMPAT_EXTENDED_SLOT_MAP 0x100

#define OCFS2_FEATURE_INCOMPAT_XATTR		0x0200

#define OCFS2_FEATURE_INCOMPAT_INDEXED_DIRS	0x0400

#define OCFS2_FEATURE_INCOMPAT_META_ECC		0x0800

#define OCFS2_FEATURE_INCOMPAT_REFCOUNT_TREE	0x1000

#define OCFS2_FEATURE_INCOMPAT_DISCONTIG_BG	0x2000

#define OCFS2_FEATURE_INCOMPAT_CLUSTERINFO	0x4000

#define OCFS2_FEATURE_COMPAT_BACKUP_SB		0x0001

#define OCFS2_FEATURE_COMPAT_JBD2_SB		0x0002

/*
 * Unwritten extents support.
 */
#define OCFS2_FEATURE_RO_COMPAT_UNWRITTEN	0x0001

#define OCFS2_FEATURE_RO_COMPAT_USRQUOTA	0x0002
#define OCFS2_FEATURE_RO_COMPAT_GRPQUOTA	0x0004

#define OCFS2_BACKUP_SB_START			1 << 30

#define OCFS2_MAX_BACKUP_SUPERBLOCKS	6

#define OCFS2_TUNEFS_INPROG_REMOVE_SLOT		0x0001	

#define OCFS2_VALID_FL		(0x00000001)	
#define OCFS2_UNUSED2_FL	(0x00000002)
#define OCFS2_ORPHANED_FL	(0x00000004)	
#define OCFS2_UNUSED3_FL	(0x00000008)
#define OCFS2_SYSTEM_FL		(0x00000010)	
#define OCFS2_SUPER_BLOCK_FL	(0x00000020)	
#define OCFS2_LOCAL_ALLOC_FL	(0x00000040)	
#define OCFS2_BITMAP_FL		(0x00000080)	
#define OCFS2_JOURNAL_FL	(0x00000100)	
#define OCFS2_HEARTBEAT_FL	(0x00000200)	
#define OCFS2_CHAIN_FL		(0x00000400)	
#define OCFS2_DEALLOC_FL	(0x00000800)	
#define OCFS2_QUOTA_FL		(0x00001000)	

#define OCFS2_INLINE_DATA_FL	(0x0001)	
#define OCFS2_HAS_XATTR_FL	(0x0002)
#define OCFS2_INLINE_XATTR_FL	(0x0004)
#define OCFS2_INDEXED_DIR_FL	(0x0008)
#define OCFS2_HAS_REFCOUNT_FL   (0x0010)

#define OCFS2_SECRM_FL			FS_SECRM_FL	
#define OCFS2_UNRM_FL			FS_UNRM_FL	
#define OCFS2_COMPR_FL			FS_COMPR_FL	
#define OCFS2_SYNC_FL			FS_SYNC_FL	
#define OCFS2_IMMUTABLE_FL		FS_IMMUTABLE_FL	
#define OCFS2_APPEND_FL			FS_APPEND_FL	
#define OCFS2_NODUMP_FL			FS_NODUMP_FL	
#define OCFS2_NOATIME_FL		FS_NOATIME_FL	
#define OCFS2_DIRTY_FL			FS_DIRTY_FL
#define OCFS2_COMPRBLK_FL		FS_COMPRBLK_FL	
#define OCFS2_NOCOMP_FL			FS_NOCOMP_FL	
#define OCFS2_ECOMPR_FL			FS_ECOMPR_FL	
#define OCFS2_BTREE_FL			FS_BTREE_FL	
#define OCFS2_INDEX_FL			FS_INDEX_FL	
#define OCFS2_IMAGIC_FL			FS_IMAGIC_FL	
#define OCFS2_JOURNAL_DATA_FL		FS_JOURNAL_DATA_FL 
#define OCFS2_NOTAIL_FL			FS_NOTAIL_FL	
#define OCFS2_DIRSYNC_FL		FS_DIRSYNC_FL	
#define OCFS2_TOPDIR_FL			FS_TOPDIR_FL	
#define OCFS2_RESERVED_FL		FS_RESERVED_FL	

#define OCFS2_FL_VISIBLE		FS_FL_USER_VISIBLE	
#define OCFS2_FL_MODIFIABLE		FS_FL_USER_MODIFIABLE	

#define OCFS2_EXT_UNWRITTEN		(0x01)	/* Extent is allocated but
						 * unwritten */
#define OCFS2_EXT_REFCOUNTED		(0x02)  

#define OCFS2_JOURNAL_DIRTY_FL	(0x00000001)	

#define OCFS2_ERROR_FS		(0x00000001)	

#define OCFS2_MAX_FILENAME_LEN		255

#define OCFS2_MAX_SLOTS			255

#define OCFS2_INVALID_SLOT		-1

#define OCFS2_VOL_UUID_LEN		16
#define OCFS2_MAX_VOL_LABEL_LEN		64

#define OCFS2_STACK_LABEL_LEN		4
#define OCFS2_CLUSTER_NAME_LEN		16

#define OCFS2_CLASSIC_CLUSTER_STACK	"o2cb"

#define OCFS2_MIN_JOURNAL_SIZE		(4 * 1024 * 1024)

#define OCFS2_MIN_XATTR_INLINE_SIZE     256

#define OCFS2_CLUSTER_O2CB_GLOBAL_HEARTBEAT	(0x01)

struct ocfs2_system_inode_info {
	char	*si_name;
	int	si_iflags;
	int	si_mode;
};

enum {
	BAD_BLOCK_SYSTEM_INODE = 0,
	GLOBAL_INODE_ALLOC_SYSTEM_INODE,
	SLOT_MAP_SYSTEM_INODE,
#define OCFS2_FIRST_ONLINE_SYSTEM_INODE SLOT_MAP_SYSTEM_INODE
	HEARTBEAT_SYSTEM_INODE,
	GLOBAL_BITMAP_SYSTEM_INODE,
	USER_QUOTA_SYSTEM_INODE,
	GROUP_QUOTA_SYSTEM_INODE,
#define OCFS2_LAST_GLOBAL_SYSTEM_INODE GROUP_QUOTA_SYSTEM_INODE
#define OCFS2_FIRST_LOCAL_SYSTEM_INODE ORPHAN_DIR_SYSTEM_INODE
	ORPHAN_DIR_SYSTEM_INODE,
	EXTENT_ALLOC_SYSTEM_INODE,
	INODE_ALLOC_SYSTEM_INODE,
	JOURNAL_SYSTEM_INODE,
	LOCAL_ALLOC_SYSTEM_INODE,
	TRUNCATE_LOG_SYSTEM_INODE,
	LOCAL_USER_QUOTA_SYSTEM_INODE,
	LOCAL_GROUP_QUOTA_SYSTEM_INODE,
#define OCFS2_LAST_LOCAL_SYSTEM_INODE LOCAL_GROUP_QUOTA_SYSTEM_INODE
	NUM_SYSTEM_INODES
};
#define NUM_GLOBAL_SYSTEM_INODES OCFS2_FIRST_LOCAL_SYSTEM_INODE
#define NUM_LOCAL_SYSTEM_INODES	\
		(NUM_SYSTEM_INODES - OCFS2_FIRST_LOCAL_SYSTEM_INODE)

static struct ocfs2_system_inode_info ocfs2_system_inodes[NUM_SYSTEM_INODES] = {
	
	
	[BAD_BLOCK_SYSTEM_INODE]		= { "bad_blocks", 0, S_IFREG | 0644 },
	[GLOBAL_INODE_ALLOC_SYSTEM_INODE] 	= { "global_inode_alloc", OCFS2_BITMAP_FL | OCFS2_CHAIN_FL, S_IFREG | 0644 },

	
	[SLOT_MAP_SYSTEM_INODE]			= { "slot_map", 0, S_IFREG | 0644 },
	[HEARTBEAT_SYSTEM_INODE]		= { "heartbeat", OCFS2_HEARTBEAT_FL, S_IFREG | 0644 },
	[GLOBAL_BITMAP_SYSTEM_INODE]		= { "global_bitmap", 0, S_IFREG | 0644 },
	[USER_QUOTA_SYSTEM_INODE]		= { "aquota.user", OCFS2_QUOTA_FL, S_IFREG | 0644 },
	[GROUP_QUOTA_SYSTEM_INODE]		= { "aquota.group", OCFS2_QUOTA_FL, S_IFREG | 0644 },

	
	[ORPHAN_DIR_SYSTEM_INODE]		= { "orphan_dir:%04d", 0, S_IFDIR | 0755 },
	[EXTENT_ALLOC_SYSTEM_INODE]		= { "extent_alloc:%04d", OCFS2_BITMAP_FL | OCFS2_CHAIN_FL, S_IFREG | 0644 },
	[INODE_ALLOC_SYSTEM_INODE]		= { "inode_alloc:%04d", OCFS2_BITMAP_FL | OCFS2_CHAIN_FL, S_IFREG | 0644 },
	[JOURNAL_SYSTEM_INODE]			= { "journal:%04d", OCFS2_JOURNAL_FL, S_IFREG | 0644 },
	[LOCAL_ALLOC_SYSTEM_INODE]		= { "local_alloc:%04d", OCFS2_BITMAP_FL | OCFS2_LOCAL_ALLOC_FL, S_IFREG | 0644 },
	[TRUNCATE_LOG_SYSTEM_INODE]		= { "truncate_log:%04d", OCFS2_DEALLOC_FL, S_IFREG | 0644 },
	[LOCAL_USER_QUOTA_SYSTEM_INODE]		= { "aquota.user:%04d", OCFS2_QUOTA_FL, S_IFREG | 0644 },
	[LOCAL_GROUP_QUOTA_SYSTEM_INODE]	= { "aquota.group:%04d", OCFS2_QUOTA_FL, S_IFREG | 0644 },
};

#define OCFS2_HB_NONE			"heartbeat=none"
#define OCFS2_HB_LOCAL			"heartbeat=local"
#define OCFS2_HB_GLOBAL			"heartbeat=global"

#define OCFS2_FT_UNKNOWN	0
#define OCFS2_FT_REG_FILE	1
#define OCFS2_FT_DIR		2
#define OCFS2_FT_CHRDEV		3
#define OCFS2_FT_BLKDEV		4
#define OCFS2_FT_FIFO		5
#define OCFS2_FT_SOCK		6
#define OCFS2_FT_SYMLINK	7

#define OCFS2_FT_MAX		8

#define OCFS2_DIR_PAD			4
#define OCFS2_DIR_ROUND			(OCFS2_DIR_PAD - 1)
#define OCFS2_DIR_MEMBER_LEN 		offsetof(struct ocfs2_dir_entry, name)
#define OCFS2_DIR_REC_LEN(name_len)	(((name_len) + OCFS2_DIR_MEMBER_LEN + \
                                          OCFS2_DIR_ROUND) & \
					 ~OCFS2_DIR_ROUND)
#define OCFS2_DIR_MIN_REC_LEN	OCFS2_DIR_REC_LEN(1)

#define OCFS2_LINK_MAX		32000
#define	OCFS2_DX_LINK_MAX	((1U << 31) - 1U)
#define	OCFS2_LINKS_HI_SHIFT	16
#define	OCFS2_DX_ENTRIES_MAX	(0xffffffffU)

#define S_SHIFT			12
static unsigned char ocfs2_type_by_mode[S_IFMT >> S_SHIFT] = {
	[S_IFREG >> S_SHIFT]  = OCFS2_FT_REG_FILE,
	[S_IFDIR >> S_SHIFT]  = OCFS2_FT_DIR,
	[S_IFCHR >> S_SHIFT]  = OCFS2_FT_CHRDEV,
	[S_IFBLK >> S_SHIFT]  = OCFS2_FT_BLKDEV,
	[S_IFIFO >> S_SHIFT]  = OCFS2_FT_FIFO,
	[S_IFSOCK >> S_SHIFT] = OCFS2_FT_SOCK,
	[S_IFLNK >> S_SHIFT]  = OCFS2_FT_SYMLINK,
};


#define OCFS2_RAW_SB(dinode)		(&((dinode)->id2.i_super))

struct ocfs2_block_check {
	__le32 bc_crc32e;	
	__le16 bc_ecc;		
	__le16 bc_reserved1;
};

struct ocfs2_extent_rec {
	__le32 e_cpos;		
	union {
		__le32 e_int_clusters; 
		struct {
			__le16 e_leaf_clusters; 
			__u8 e_reserved1;
			__u8 e_flags; 
		};
	};
	__le64 e_blkno;		
};

struct ocfs2_chain_rec {
	__le32 c_free;	
	__le32 c_total;	
	__le64 c_blkno;	
};

struct ocfs2_truncate_rec {
	__le32 t_start;		
	__le32 t_clusters;	
};

struct ocfs2_extent_list {
	__le16 l_tree_depth;		
	__le16 l_count;			
	__le16 l_next_free_rec;		
	__le16 l_reserved1;
	__le64 l_reserved2;		
	struct ocfs2_extent_rec l_recs[0];	
};

struct ocfs2_chain_list {
	__le16 cl_cpg;			
	__le16 cl_bpc;			
	__le16 cl_count;		
	__le16 cl_next_free_rec;	
	__le64 cl_reserved1;
	struct ocfs2_chain_rec cl_recs[0];	
};

struct ocfs2_truncate_log {
	__le16 tl_count;		
	__le16 tl_used;			
	__le32 tl_reserved1;
	struct ocfs2_truncate_rec tl_recs[0];	
};

struct ocfs2_extent_block
{
	__u8 h_signature[8];		
	struct ocfs2_block_check h_check;	
	__le16 h_suballoc_slot;		
	__le16 h_suballoc_bit;		
	__le32 h_fs_generation;		
	__le64 h_blkno;			
	__le64 h_suballoc_loc;		
	__le64 h_next_leaf_blk;		
	struct ocfs2_extent_list h_list;	
};

struct ocfs2_slot_map {
	__le16 sm_slots[0];
};

struct ocfs2_extended_slot {
	__u8	es_valid;
	__u8	es_reserved1[3];
	__le32	es_node_num;
};

struct ocfs2_slot_map_extended {
	struct ocfs2_extended_slot se_slots[0];
};

struct ocfs2_cluster_info {
	__u8   ci_stack[OCFS2_STACK_LABEL_LEN];
	union {
		__le32 ci_reserved;
		struct {
			__u8 ci_stackflags;
			__u8 ci_reserved1;
			__u8 ci_reserved2;
			__u8 ci_reserved3;
		};
	};
	__u8   ci_cluster[OCFS2_CLUSTER_NAME_LEN];
};

struct ocfs2_super_block {
	__le16 s_major_rev_level;
	__le16 s_minor_rev_level;
	__le16 s_mnt_count;
	__le16 s_max_mnt_count;
	__le16 s_state;			
	__le16 s_errors;			
	__le32 s_checkinterval;		
	__le64 s_lastcheck;		
	__le32 s_creator_os;		
	__le32 s_feature_compat;		
	__le32 s_feature_incompat;	
	__le32 s_feature_ro_compat;	
	__le64 s_root_blkno;		
	__le64 s_system_dir_blkno;	
	__le32 s_blocksize_bits;		
	__le32 s_clustersize_bits;	
	__le16 s_max_slots;		
	__le16 s_tunefs_flag;
	__le32 s_uuid_hash;		
	__le64 s_first_cluster_group;	
	__u8  s_label[OCFS2_MAX_VOL_LABEL_LEN];	
	__u8  s_uuid[OCFS2_VOL_UUID_LEN];	
  struct ocfs2_cluster_info s_cluster_info; 
	__le16 s_xattr_inline_size;	
	__le16 s_reserved0;
	__le32 s_dx_seed[3];		
  __le64 s_reserved2[15];		

};

struct ocfs2_local_alloc
{
	__le32 la_bm_off;	
	__le16 la_size;		
	__le16 la_reserved1;
	__le64 la_reserved2;
	__u8   la_bitmap[0];
};

struct ocfs2_inline_data
{
	__le16	id_count;	
	__le16	id_reserved0;
	__le32	id_reserved1;
	__u8	id_data[0];	
};

struct ocfs2_dinode {
	__u8 i_signature[8];		
	__le32 i_generation;		
	__le16 i_suballoc_slot;		
	__le16 i_suballoc_bit;		
	__le16 i_links_count_hi;	
	__le16 i_xattr_inline_size;
	__le32 i_clusters;		
	__le32 i_uid;			
	__le32 i_gid;			
	__le64 i_size;			
	__le16 i_mode;			
	__le16 i_links_count;		
	__le32 i_flags;			
	__le64 i_atime;			
	__le64 i_ctime;			
	__le64 i_mtime;			
	__le64 i_dtime;			
	__le64 i_blkno;			
	__le64 i_last_eb_blk;		
	__le32 i_fs_generation;		
	__le32 i_atime_nsec;
	__le32 i_ctime_nsec;
	__le32 i_mtime_nsec;
	__le32 i_attr;
	__le16 i_orphaned_slot;		
	__le16 i_dyn_features;
	__le64 i_xattr_loc;
	struct ocfs2_block_check i_check;	
	__le64 i_dx_root;		
	__le64 i_refcount_loc;
	__le64 i_suballoc_loc;		
	__le64 i_reserved2[3];
	union {
		__le64 i_pad1;		
		struct {
			__le64 i_rdev;	
		} dev1;
		struct {		
			__le32 i_used;	
			__le32 i_total;	
		} bitmap1;
		struct {		
			__le32 ij_flags;	
			__le32 ij_recovery_generation; 
		} journal1;
	} id1;				
	union {
		struct ocfs2_super_block	i_super;
		struct ocfs2_local_alloc	i_lab;
		struct ocfs2_chain_list		i_chain;
		struct ocfs2_extent_list	i_list;
		struct ocfs2_truncate_log	i_dealloc;
		struct ocfs2_inline_data	i_data;
		__u8               		i_symlink[0];
	} id2;
};

struct ocfs2_dir_entry {
	__le64   inode;                  
	__le16   rec_len;                
	__u8    name_len;               
	__u8    file_type;
	char    name[OCFS2_MAX_FILENAME_LEN];   
} __attribute__ ((packed));

struct ocfs2_dir_block_trailer {
	__le64		db_compat_inode;	

	__le16		db_compat_rec_len;	
	__u8		db_compat_name_len;	
	__u8		db_reserved0;
	__le16		db_reserved1;
	__le16		db_free_rec_len;	
	__u8		db_signature[8];	
	__le64		db_reserved2;
	__le64		db_free_next;		
	__le64		db_blkno;		
	__le64		db_parent_dinode;	
	struct ocfs2_block_check db_check;	
};

struct ocfs2_dx_entry {
	__le32		dx_major_hash;	
	__le32		dx_minor_hash;	
	__le64		dx_dirent_blk;	
};

struct ocfs2_dx_entry_list {
	__le32		de_reserved;
	__le16		de_count;	
	__le16		de_num_used;	
	struct	ocfs2_dx_entry		de_entries[0];	
};

#define OCFS2_DX_FLAG_INLINE	0x01

struct ocfs2_dx_root_block {
	__u8		dr_signature[8];	
	struct ocfs2_block_check dr_check;	
	__le16		dr_suballoc_slot;	
	__le16		dr_suballoc_bit;	
	__le32		dr_fs_generation;	
	__le64		dr_blkno;		
	__le64		dr_last_eb_blk;		
	__le32		dr_clusters;		
	__u8		dr_flags;		
	__u8		dr_reserved0;
	__le16		dr_reserved1;
	__le64		dr_dir_blkno;		
	__le32		dr_num_entries;		
	__le32		dr_reserved2;
	__le64		dr_free_blk;		
	__le64		dr_suballoc_loc;	
	__le64		dr_reserved3[14];
	union {
		struct ocfs2_extent_list dr_list; 
		struct ocfs2_dx_entry_list dr_entries; 
	};
};

struct ocfs2_dx_leaf {
	__u8		dl_signature[8];
	struct ocfs2_block_check dl_check;	
	__le64		dl_blkno;	
	__le32		dl_fs_generation;
	__le32		dl_reserved0;
	__le64		dl_reserved1;
	struct ocfs2_dx_entry_list	dl_list;
};

#define OCFS2_MAX_BG_BITMAP_SIZE	256

struct ocfs2_group_desc
{
	__u8    bg_signature[8];        
	__le16   bg_size;                
	__le16   bg_bits;                
	__le16	bg_free_bits_count;     
	__le16   bg_chain;               
	__le32   bg_generation;
	__le32	bg_reserved1;
	__le64   bg_next_group;          
	__le64   bg_parent_dinode;       
	__le64   bg_blkno;               
	struct ocfs2_block_check bg_check;	
	__le64   bg_reserved2;
	union {
		__u8    bg_bitmap[0];
		struct {
			__u8 bg_bitmap_filler[OCFS2_MAX_BG_BITMAP_SIZE];
			struct ocfs2_extent_list bg_list;
		};
	};
};

struct ocfs2_refcount_rec {
	__le64 r_cpos;		
	__le32 r_clusters;	
	__le32 r_refcount;	
};
#define OCFS2_32BIT_POS_MASK		(0xffffffffULL)

#define OCFS2_REFCOUNT_LEAF_FL          (0x00000001)
#define OCFS2_REFCOUNT_TREE_FL          (0x00000002)

struct ocfs2_refcount_list {
	__le16 rl_count;	
	__le16 rl_used;		
	__le32 rl_reserved2;
	__le64 rl_reserved1;	
	struct ocfs2_refcount_rec rl_recs[0];	
};


struct ocfs2_refcount_block {
	__u8 rf_signature[8];		
	__le16 rf_suballoc_slot;	
	__le16 rf_suballoc_bit;		
	__le32 rf_fs_generation;	
	__le64 rf_blkno;		
	__le64 rf_parent;		
	struct ocfs2_block_check rf_check;	
	__le64 rf_last_eb_blk;		
	__le32 rf_count;		
	__le32 rf_flags;		
	__le32 rf_clusters;		
	__le32 rf_cpos;			
	__le32 rf_generation;		
	__le32 rf_reserved0;
	__le64 rf_suballoc_loc;		
	__le64 rf_reserved1[6];
	union {
		struct ocfs2_refcount_list rf_records;  
		struct ocfs2_extent_list rf_list;	
	};
};


struct ocfs2_xattr_entry {
	__le32	xe_name_hash;    
	__le16	xe_name_offset;  
	__u8	xe_name_len;	 
	__u8	xe_type;         
	__le64	xe_value_size;	 
};

struct ocfs2_xattr_header {
	__le16	xh_count;                       
	__le16	xh_free_start;                  
	__le16	xh_name_value_len;              
	__le16	xh_num_buckets;                 
	struct ocfs2_block_check xh_check;	
	struct ocfs2_xattr_entry xh_entries[0]; 
};

struct ocfs2_xattr_value_root {
	__le32	xr_clusters;              
	__le32	xr_reserved0;
	__le64	xr_last_eb_blk;           
	struct ocfs2_extent_list xr_list; 
};

struct ocfs2_xattr_tree_root {
	__le32	xt_clusters;              
	__le32	xt_reserved0;
	__le64	xt_last_eb_blk;           
	struct ocfs2_extent_list xt_list; 
};

#define OCFS2_XATTR_INDEXED	0x1
#define OCFS2_HASH_SHIFT	5
#define OCFS2_XATTR_ROUND	3
#define OCFS2_XATTR_SIZE(size)	(((size) + OCFS2_XATTR_ROUND) & \
				~(OCFS2_XATTR_ROUND))

#define OCFS2_XATTR_BUCKET_SIZE			4096
#define OCFS2_XATTR_MAX_BLOCKS_PER_BUCKET 	(OCFS2_XATTR_BUCKET_SIZE \
						 / OCFS2_MIN_BLOCKSIZE)

struct ocfs2_xattr_block {
	__u8	xb_signature[8];     
	__le16	xb_suballoc_slot;    
	__le16	xb_suballoc_bit;     
	__le32	xb_fs_generation;    
	__le64	xb_blkno;            
	struct ocfs2_block_check xb_check;	
	__le16	xb_flags;            
	__le16	xb_reserved0;
	__le32  xb_reserved1;
	__le64	xb_suballoc_loc;	
	union {
		struct ocfs2_xattr_header xb_header; 
		struct ocfs2_xattr_tree_root xb_root;
	} xb_attrs;
};

#define OCFS2_XATTR_ENTRY_LOCAL		0x80
#define OCFS2_XATTR_TYPE_MASK		0x7F
static inline void ocfs2_xattr_set_local(struct ocfs2_xattr_entry *xe,
					 int local)
{
	if (local)
		xe->xe_type |= OCFS2_XATTR_ENTRY_LOCAL;
	else
		xe->xe_type &= ~OCFS2_XATTR_ENTRY_LOCAL;
}

static inline int ocfs2_xattr_is_local(struct ocfs2_xattr_entry *xe)
{
	return xe->xe_type & OCFS2_XATTR_ENTRY_LOCAL;
}

static inline void ocfs2_xattr_set_type(struct ocfs2_xattr_entry *xe, int type)
{
	xe->xe_type |= type & OCFS2_XATTR_TYPE_MASK;
}

static inline int ocfs2_xattr_get_type(struct ocfs2_xattr_entry *xe)
{
	return xe->xe_type & OCFS2_XATTR_TYPE_MASK;
}


#define OCFS2_GLOBAL_QMAGICS {\
	0x0cf52470,  \
	0x0cf52471   \
}

#define OCFS2_GLOBAL_QVERSIONS {\
	0, \
	0, \
}


#define OCFS2_QBLK_RESERVED_SPACE 8

struct ocfs2_disk_dqheader {
	__le32 dqh_magic;	
	__le32 dqh_version;	
};

#define OCFS2_GLOBAL_INFO_OFF (sizeof(struct ocfs2_disk_dqheader))

struct ocfs2_global_disk_dqinfo {
	__le32 dqi_bgrace;	
	__le32 dqi_igrace;	
	__le32 dqi_syncms;	
	__le32 dqi_blocks;	
	__le32 dqi_free_blk;	
	__le32 dqi_free_entry;	
};

struct ocfs2_global_disk_dqblk {
	__le32 dqb_id;          
	__le32 dqb_use_count;   
	__le64 dqb_ihardlimit;  
	__le64 dqb_isoftlimit;  
	__le64 dqb_curinodes;   
	__le64 dqb_bhardlimit;  
	__le64 dqb_bsoftlimit;  
	__le64 dqb_curspace;    
	__le64 dqb_btime;       
	__le64 dqb_itime;       
	__le64 dqb_pad1;
	__le64 dqb_pad2;
};


#define OCFS2_LOCAL_QMAGICS {\
	0x0cf524c0,  \
	0x0cf524c1   \
}

#define OCFS2_LOCAL_QVERSIONS {\
	0, \
	0, \
}

#define OLQF_CLEAN	0x0001	

#define OCFS2_LOCAL_INFO_OFF (sizeof(struct ocfs2_disk_dqheader))

struct ocfs2_local_disk_dqinfo {
	__le32 dqi_flags;	
	__le32 dqi_chunks;	
	__le32 dqi_blocks;	
};

struct ocfs2_local_disk_chunk {
	__le32 dqc_free;	
	__u8 dqc_bitmap[0];	
};

struct ocfs2_local_disk_dqblk {
	__le64 dqb_id;		
	__le64 dqb_spacemod;	
	__le64 dqb_inodemod;	
};



struct ocfs2_disk_dqtrailer {
	struct ocfs2_block_check dq_check;	
	
};

static inline struct ocfs2_disk_dqtrailer *ocfs2_block_dqtrailer(int blocksize,
								 void *buf)
{
	char *ptr = buf;
	ptr += blocksize - OCFS2_QBLK_RESERVED_SPACE;

	return (struct ocfs2_disk_dqtrailer *)ptr;
}

#ifdef __KERNEL__
static inline int ocfs2_fast_symlink_chars(struct super_block *sb)
{
	return  sb->s_blocksize -
		 offsetof(struct ocfs2_dinode, id2.i_symlink);
}

static inline int ocfs2_max_inline_data_with_xattr(struct super_block *sb,
						   struct ocfs2_dinode *di)
{
	unsigned int xattrsize = le16_to_cpu(di->i_xattr_inline_size);

	if (le16_to_cpu(di->i_dyn_features) & OCFS2_INLINE_XATTR_FL)
		return sb->s_blocksize -
			offsetof(struct ocfs2_dinode, id2.i_data.id_data) -
			xattrsize;
	else
		return sb->s_blocksize -
			offsetof(struct ocfs2_dinode, id2.i_data.id_data);
}

static inline int ocfs2_extent_recs_per_inode(struct super_block *sb)
{
	int size;

	size = sb->s_blocksize -
		offsetof(struct ocfs2_dinode, id2.i_list.l_recs);

	return size / sizeof(struct ocfs2_extent_rec);
}

static inline int ocfs2_extent_recs_per_inode_with_xattr(
						struct super_block *sb,
						struct ocfs2_dinode *di)
{
	int size;
	unsigned int xattrsize = le16_to_cpu(di->i_xattr_inline_size);

	if (le16_to_cpu(di->i_dyn_features) & OCFS2_INLINE_XATTR_FL)
		size = sb->s_blocksize -
			offsetof(struct ocfs2_dinode, id2.i_list.l_recs) -
			xattrsize;
	else
		size = sb->s_blocksize -
			offsetof(struct ocfs2_dinode, id2.i_list.l_recs);

	return size / sizeof(struct ocfs2_extent_rec);
}

static inline int ocfs2_extent_recs_per_dx_root(struct super_block *sb)
{
	int size;

	size = sb->s_blocksize -
		offsetof(struct ocfs2_dx_root_block, dr_list.l_recs);

	return size / sizeof(struct ocfs2_extent_rec);
}

static inline int ocfs2_chain_recs_per_inode(struct super_block *sb)
{
	int size;

	size = sb->s_blocksize -
		offsetof(struct ocfs2_dinode, id2.i_chain.cl_recs);

	return size / sizeof(struct ocfs2_chain_rec);
}

static inline u16 ocfs2_extent_recs_per_eb(struct super_block *sb)
{
	int size;

	size = sb->s_blocksize -
		offsetof(struct ocfs2_extent_block, h_list.l_recs);

	return size / sizeof(struct ocfs2_extent_rec);
}

static inline u16 ocfs2_extent_recs_per_gd(struct super_block *sb)
{
	int size;

	size = sb->s_blocksize -
		offsetof(struct ocfs2_group_desc, bg_list.l_recs);

	return size / sizeof(struct ocfs2_extent_rec);
}

static inline int ocfs2_dx_entries_per_leaf(struct super_block *sb)
{
	int size;

	size = sb->s_blocksize -
		offsetof(struct ocfs2_dx_leaf, dl_list.de_entries);

	return size / sizeof(struct ocfs2_dx_entry);
}

static inline int ocfs2_dx_entries_per_root(struct super_block *sb)
{
	int size;

	size = sb->s_blocksize -
		offsetof(struct ocfs2_dx_root_block, dr_entries.de_entries);

	return size / sizeof(struct ocfs2_dx_entry);
}

static inline u16 ocfs2_local_alloc_size(struct super_block *sb)
{
	u16 size;

	size = sb->s_blocksize -
		offsetof(struct ocfs2_dinode, id2.i_lab.la_bitmap);

	return size;
}

static inline int ocfs2_group_bitmap_size(struct super_block *sb,
					  int suballocator,
					  u32 feature_incompat)
{
	int size = sb->s_blocksize -
		offsetof(struct ocfs2_group_desc, bg_bitmap);

	if (suballocator &&
	    (feature_incompat & OCFS2_FEATURE_INCOMPAT_DISCONTIG_BG))
		size = OCFS2_MAX_BG_BITMAP_SIZE;

	return size;
}

static inline int ocfs2_truncate_recs_per_inode(struct super_block *sb)
{
	int size;

	size = sb->s_blocksize -
		offsetof(struct ocfs2_dinode, id2.i_dealloc.tl_recs);

	return size / sizeof(struct ocfs2_truncate_rec);
}

static inline u64 ocfs2_backup_super_blkno(struct super_block *sb, int index)
{
	u64 offset = OCFS2_BACKUP_SB_START;

	if (index >= 0 && index < OCFS2_MAX_BACKUP_SUPERBLOCKS) {
		offset <<= (2 * index);
		offset >>= sb->s_blocksize_bits;
		return offset;
	}

	return 0;

}

static inline u16 ocfs2_xattr_recs_per_xb(struct super_block *sb)
{
	int size;

	size = sb->s_blocksize -
		offsetof(struct ocfs2_xattr_block,
			 xb_attrs.xb_root.xt_list.l_recs);

	return size / sizeof(struct ocfs2_extent_rec);
}

static inline u16 ocfs2_extent_recs_per_rb(struct super_block *sb)
{
	int size;

	size = sb->s_blocksize -
		offsetof(struct ocfs2_refcount_block, rf_list.l_recs);

	return size / sizeof(struct ocfs2_extent_rec);
}

static inline u16 ocfs2_refcount_recs_per_rb(struct super_block *sb)
{
	int size;

	size = sb->s_blocksize -
		offsetof(struct ocfs2_refcount_block, rf_records.rl_recs);

	return size / sizeof(struct ocfs2_refcount_rec);
}

static inline u32
ocfs2_get_ref_rec_low_cpos(const struct ocfs2_refcount_rec *rec)
{
	return le64_to_cpu(rec->r_cpos) & OCFS2_32BIT_POS_MASK;
}
#else
static inline int ocfs2_fast_symlink_chars(int blocksize)
{
	return blocksize - offsetof(struct ocfs2_dinode, id2.i_symlink);
}

static inline int ocfs2_max_inline_data_with_xattr(int blocksize,
						   struct ocfs2_dinode *di)
{
	if (di && (di->i_dyn_features & OCFS2_INLINE_XATTR_FL))
		return blocksize -
			offsetof(struct ocfs2_dinode, id2.i_data.id_data) -
			di->i_xattr_inline_size;
	else
		return blocksize -
			offsetof(struct ocfs2_dinode, id2.i_data.id_data);
}

static inline int ocfs2_extent_recs_per_inode(int blocksize)
{
	int size;

	size = blocksize -
		offsetof(struct ocfs2_dinode, id2.i_list.l_recs);

	return size / sizeof(struct ocfs2_extent_rec);
}

static inline int ocfs2_chain_recs_per_inode(int blocksize)
{
	int size;

	size = blocksize -
		offsetof(struct ocfs2_dinode, id2.i_chain.cl_recs);

	return size / sizeof(struct ocfs2_chain_rec);
}

static inline int ocfs2_extent_recs_per_eb(int blocksize)
{
	int size;

	size = blocksize -
		offsetof(struct ocfs2_extent_block, h_list.l_recs);

	return size / sizeof(struct ocfs2_extent_rec);
}

static inline int ocfs2_extent_recs_per_gd(int blocksize)
{
	int size;

	size = blocksize -
		offsetof(struct ocfs2_group_desc, bg_list.l_recs);

	return size / sizeof(struct ocfs2_extent_rec);
}

static inline int ocfs2_local_alloc_size(int blocksize)
{
	int size;

	size = blocksize -
		offsetof(struct ocfs2_dinode, id2.i_lab.la_bitmap);

	return size;
}

static inline int ocfs2_group_bitmap_size(int blocksize,
					  int suballocator,
					  uint32_t feature_incompat)
{
	int size = sb->s_blocksize -
		offsetof(struct ocfs2_group_desc, bg_bitmap);

	if (suballocator &&
	    (feature_incompat & OCFS2_FEATURE_INCOMPAT_DISCONTIG_BG))
		size = OCFS2_MAX_BG_BITMAP_SIZE;

	return size;
}

static inline int ocfs2_truncate_recs_per_inode(int blocksize)
{
	int size;

	size = blocksize -
		offsetof(struct ocfs2_dinode, id2.i_dealloc.tl_recs);

	return size / sizeof(struct ocfs2_truncate_rec);
}

static inline uint64_t ocfs2_backup_super_blkno(int blocksize, int index)
{
	uint64_t offset = OCFS2_BACKUP_SB_START;

	if (index >= 0 && index < OCFS2_MAX_BACKUP_SUPERBLOCKS) {
		offset <<= (2 * index);
		offset /= blocksize;
		return offset;
	}

	return 0;
}

static inline int ocfs2_xattr_recs_per_xb(int blocksize)
{
	int size;

	size = blocksize -
		offsetof(struct ocfs2_xattr_block,
			 xb_attrs.xb_root.xt_list.l_recs);

	return size / sizeof(struct ocfs2_extent_rec);
}
#endif  


static inline int ocfs2_system_inode_is_global(int type)
{
	return ((type >= 0) &&
		(type <= OCFS2_LAST_GLOBAL_SYSTEM_INODE));
}

static inline int ocfs2_sprintf_system_inode_name(char *buf, int len,
						  int type, int slot)
{
	int chars;

	if (type <= OCFS2_LAST_GLOBAL_SYSTEM_INODE)
		chars = snprintf(buf, len, "%s",
				 ocfs2_system_inodes[type].si_name);
	else
		chars = snprintf(buf, len,
				 ocfs2_system_inodes[type].si_name,
				 slot);

	return chars;
}

static inline void ocfs2_set_de_type(struct ocfs2_dir_entry *de,
				    umode_t mode)
{
	de->file_type = ocfs2_type_by_mode[(mode & S_IFMT)>>S_SHIFT];
}

static inline int ocfs2_gd_is_discontig(struct ocfs2_group_desc *gd)
{
	if ((offsetof(struct ocfs2_group_desc, bg_bitmap) +
	     le16_to_cpu(gd->bg_size)) !=
	    offsetof(struct ocfs2_group_desc, bg_list))
		return 0;
	if (!gd->bg_list.l_next_free_rec)
		return 0;
	return 1;
}
#endif  

