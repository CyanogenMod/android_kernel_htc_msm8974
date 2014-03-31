/*
 * Written by Stephen C. Tweedie <sct@redhat.com>, 1999
 *
 * Copyright 1998--1999 Red Hat corp --- All Rights Reserved
 *
 * This file is part of the Linux kernel and is made available under
 * the terms of the GNU General Public License, version 2, or at your
 * option, any later version, incorporated herein by reference.
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 *  from
 *
 *  linux/include/linux/minix_fs.h
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

#include <linux/fs.h>
#include <linux/jbd.h>
#include <linux/magic.h>
#include <linux/bug.h>
#include <linux/blockgroup_lock.h>


#undef EXT3FS_DEBUG

#define EXT3_DEFAULT_RESERVE_BLOCKS     8
#define EXT3_MAX_RESERVE_BLOCKS         1027
#define EXT3_RESERVE_WINDOW_NOT_ALLOCATED 0

#ifdef EXT3FS_DEBUG
#define ext3_debug(f, a...)						\
	do {								\
		printk (KERN_DEBUG "EXT3-fs DEBUG (%s, %d): %s:",	\
			__FILE__, __LINE__, __func__);		\
		printk (KERN_DEBUG f, ## a);				\
	} while (0)
#else
#define ext3_debug(f, a...)	do {} while (0)
#endif

#define	EXT3_BAD_INO		 1	
#define EXT3_ROOT_INO		 2	
#define EXT3_BOOT_LOADER_INO	 5	
#define EXT3_UNDEL_DIR_INO	 6	
#define EXT3_RESIZE_INO		 7	
#define EXT3_JOURNAL_INO	 8	

#define EXT3_GOOD_OLD_FIRST_INO	11

#define EXT3_LINK_MAX		32000

#define EXT3_MIN_BLOCK_SIZE		1024
#define	EXT3_MAX_BLOCK_SIZE		65536
#define EXT3_MIN_BLOCK_LOG_SIZE		10
#define EXT3_BLOCK_SIZE(s)		((s)->s_blocksize)
#define	EXT3_ADDR_PER_BLOCK(s)		(EXT3_BLOCK_SIZE(s) / sizeof (__u32))
#define EXT3_BLOCK_SIZE_BITS(s)	((s)->s_blocksize_bits)
#define	EXT3_ADDR_PER_BLOCK_BITS(s)	(EXT3_SB(s)->s_addr_per_block_bits)
#define EXT3_INODE_SIZE(s)		(EXT3_SB(s)->s_inode_size)
#define EXT3_FIRST_INO(s)		(EXT3_SB(s)->s_first_ino)

#define EXT3_MIN_FRAG_SIZE		1024
#define	EXT3_MAX_FRAG_SIZE		4096
#define EXT3_MIN_FRAG_LOG_SIZE		  10
#define EXT3_FRAG_SIZE(s)		(EXT3_SB(s)->s_frag_size)
#define EXT3_FRAGS_PER_BLOCK(s)		(EXT3_SB(s)->s_frags_per_block)

struct ext3_group_desc
{
	__le32	bg_block_bitmap;		
	__le32	bg_inode_bitmap;		
	__le32	bg_inode_table;		
	__le16	bg_free_blocks_count;	
	__le16	bg_free_inodes_count;	
	__le16	bg_used_dirs_count;	
	__u16	bg_pad;
	__le32	bg_reserved[3];
};

#define EXT3_BLOCKS_PER_GROUP(s)	(EXT3_SB(s)->s_blocks_per_group)
#define EXT3_DESC_PER_BLOCK(s)		(EXT3_SB(s)->s_desc_per_block)
#define EXT3_INODES_PER_GROUP(s)	(EXT3_SB(s)->s_inodes_per_group)
#define EXT3_DESC_PER_BLOCK_BITS(s)	(EXT3_SB(s)->s_desc_per_block_bits)

#define	EXT3_NDIR_BLOCKS		12
#define	EXT3_IND_BLOCK			EXT3_NDIR_BLOCKS
#define	EXT3_DIND_BLOCK			(EXT3_IND_BLOCK + 1)
#define	EXT3_TIND_BLOCK			(EXT3_DIND_BLOCK + 1)
#define	EXT3_N_BLOCKS			(EXT3_TIND_BLOCK + 1)

#define	EXT3_SECRM_FL			0x00000001 
#define	EXT3_UNRM_FL			0x00000002 
#define	EXT3_COMPR_FL			0x00000004 
#define EXT3_SYNC_FL			0x00000008 
#define EXT3_IMMUTABLE_FL		0x00000010 
#define EXT3_APPEND_FL			0x00000020 
#define EXT3_NODUMP_FL			0x00000040 
#define EXT3_NOATIME_FL			0x00000080 
#define EXT3_DIRTY_FL			0x00000100
#define EXT3_COMPRBLK_FL		0x00000200 
#define EXT3_NOCOMPR_FL			0x00000400 
#define EXT3_ECOMPR_FL			0x00000800 
#define EXT3_INDEX_FL			0x00001000 
#define EXT3_IMAGIC_FL			0x00002000 
#define EXT3_JOURNAL_DATA_FL		0x00004000 
#define EXT3_NOTAIL_FL			0x00008000 
#define EXT3_DIRSYNC_FL			0x00010000 
#define EXT3_TOPDIR_FL			0x00020000 
#define EXT3_RESERVED_FL		0x80000000 

#define EXT3_FL_USER_VISIBLE		0x0003DFFF 
#define EXT3_FL_USER_MODIFIABLE		0x000380FF 

#define EXT3_FL_INHERITED (EXT3_SECRM_FL | EXT3_UNRM_FL | EXT3_COMPR_FL |\
			   EXT3_SYNC_FL | EXT3_NODUMP_FL |\
			   EXT3_NOATIME_FL | EXT3_COMPRBLK_FL |\
			   EXT3_NOCOMPR_FL | EXT3_JOURNAL_DATA_FL |\
			   EXT3_NOTAIL_FL | EXT3_DIRSYNC_FL)

#define EXT3_REG_FLMASK (~(EXT3_DIRSYNC_FL | EXT3_TOPDIR_FL))

#define EXT3_OTHER_FLMASK (EXT3_NODUMP_FL | EXT3_NOATIME_FL)

static inline __u32 ext3_mask_flags(umode_t mode, __u32 flags)
{
	if (S_ISDIR(mode))
		return flags;
	else if (S_ISREG(mode))
		return flags & EXT3_REG_FLMASK;
	else
		return flags & EXT3_OTHER_FLMASK;
}

struct ext3_new_group_input {
	__u32 group;            
	__u32 block_bitmap;     
	__u32 inode_bitmap;     
	__u32 inode_table;      
	__u32 blocks_count;     
	__u16 reserved_blocks;  
	__u16 unused;
};

struct ext3_new_group_data {
	__u32 group;
	__u32 block_bitmap;
	__u32 inode_bitmap;
	__u32 inode_table;
	__u32 blocks_count;
	__u16 reserved_blocks;
	__u16 unused;
	__u32 free_blocks_count;
};


#define	EXT3_IOC_GETFLAGS		FS_IOC_GETFLAGS
#define	EXT3_IOC_SETFLAGS		FS_IOC_SETFLAGS
#define	EXT3_IOC_GETVERSION		_IOR('f', 3, long)
#define	EXT3_IOC_SETVERSION		_IOW('f', 4, long)
#define EXT3_IOC_GROUP_EXTEND		_IOW('f', 7, unsigned long)
#define EXT3_IOC_GROUP_ADD		_IOW('f', 8,struct ext3_new_group_input)
#define	EXT3_IOC_GETVERSION_OLD		FS_IOC_GETVERSION
#define	EXT3_IOC_SETVERSION_OLD		FS_IOC_SETVERSION
#ifdef CONFIG_JBD_DEBUG
#define EXT3_IOC_WAIT_FOR_READONLY	_IOR('f', 99, long)
#endif
#define EXT3_IOC_GETRSVSZ		_IOR('f', 5, long)
#define EXT3_IOC_SETRSVSZ		_IOW('f', 6, long)

#define EXT3_IOC32_GETFLAGS		FS_IOC32_GETFLAGS
#define EXT3_IOC32_SETFLAGS		FS_IOC32_SETFLAGS
#define EXT3_IOC32_GETVERSION		_IOR('f', 3, int)
#define EXT3_IOC32_SETVERSION		_IOW('f', 4, int)
#define EXT3_IOC32_GETRSVSZ		_IOR('f', 5, int)
#define EXT3_IOC32_SETRSVSZ		_IOW('f', 6, int)
#define EXT3_IOC32_GROUP_EXTEND		_IOW('f', 7, unsigned int)
#ifdef CONFIG_JBD_DEBUG
#define EXT3_IOC32_WAIT_FOR_READONLY	_IOR('f', 99, int)
#endif
#define EXT3_IOC32_GETVERSION_OLD	FS_IOC32_GETVERSION
#define EXT3_IOC32_SETVERSION_OLD	FS_IOC32_SETVERSION


struct ext3_mount_options {
	unsigned long s_mount_opt;
	uid_t s_resuid;
	gid_t s_resgid;
	unsigned long s_commit_interval;
#ifdef CONFIG_QUOTA
	int s_jquota_fmt;
	char *s_qf_names[MAXQUOTAS];
#endif
};

struct ext3_inode {
	__le16	i_mode;		
	__le16	i_uid;		
	__le32	i_size;		
	__le32	i_atime;	
	__le32	i_ctime;	
	__le32	i_mtime;	
	__le32	i_dtime;	
	__le16	i_gid;		
	__le16	i_links_count;	
	__le32	i_blocks;	
	__le32	i_flags;	
	union {
		struct {
			__u32  l_i_reserved1;
		} linux1;
		struct {
			__u32  h_i_translator;
		} hurd1;
		struct {
			__u32  m_i_reserved1;
		} masix1;
	} osd1;				
	__le32	i_block[EXT3_N_BLOCKS];
	__le32	i_generation;	
	__le32	i_file_acl;	
	__le32	i_dir_acl;	
	__le32	i_faddr;	
	union {
		struct {
			__u8	l_i_frag;	
			__u8	l_i_fsize;	
			__u16	i_pad1;
			__le16	l_i_uid_high;	
			__le16	l_i_gid_high;	
			__u32	l_i_reserved2;
		} linux2;
		struct {
			__u8	h_i_frag;	
			__u8	h_i_fsize;	
			__u16	h_i_mode_high;
			__u16	h_i_uid_high;
			__u16	h_i_gid_high;
			__u32	h_i_author;
		} hurd2;
		struct {
			__u8	m_i_frag;	
			__u8	m_i_fsize;	
			__u16	m_pad1;
			__u32	m_i_reserved2[2];
		} masix2;
	} osd2;				
	__le16	i_extra_isize;
	__le16	i_pad1;
};

#define i_size_high	i_dir_acl

#define i_reserved1	osd1.linux1.l_i_reserved1
#define i_frag		osd2.linux2.l_i_frag
#define i_fsize		osd2.linux2.l_i_fsize
#define i_uid_low	i_uid
#define i_gid_low	i_gid
#define i_uid_high	osd2.linux2.l_i_uid_high
#define i_gid_high	osd2.linux2.l_i_gid_high
#define i_reserved2	osd2.linux2.l_i_reserved2

#define	EXT3_VALID_FS			0x0001	
#define	EXT3_ERROR_FS			0x0002	
#define	EXT3_ORPHAN_FS			0x0004	

#define EXT2_FLAGS_SIGNED_HASH		0x0001  
#define EXT2_FLAGS_UNSIGNED_HASH	0x0002  
#define EXT2_FLAGS_TEST_FILESYS		0x0004	

#define EXT3_MOUNT_CHECK		0x00001	
#define EXT3_MOUNT_GRPID		0x00004	
#define EXT3_MOUNT_DEBUG		0x00008	
#define EXT3_MOUNT_ERRORS_CONT		0x00010	
#define EXT3_MOUNT_ERRORS_RO		0x00020	
#define EXT3_MOUNT_ERRORS_PANIC		0x00040	
#define EXT3_MOUNT_MINIX_DF		0x00080	
#define EXT3_MOUNT_NOLOAD		0x00100	
#define EXT3_MOUNT_ABORT		0x00200	
#define EXT3_MOUNT_DATA_FLAGS		0x00C00	
#define EXT3_MOUNT_JOURNAL_DATA		0x00400	
#define EXT3_MOUNT_ORDERED_DATA		0x00800	
#define EXT3_MOUNT_WRITEBACK_DATA	0x00C00	
#define EXT3_MOUNT_UPDATE_JOURNAL	0x01000	
#define EXT3_MOUNT_NO_UID32		0x02000  
#define EXT3_MOUNT_XATTR_USER		0x04000	
#define EXT3_MOUNT_POSIX_ACL		0x08000	
#define EXT3_MOUNT_RESERVATION		0x10000	
#define EXT3_MOUNT_BARRIER		0x20000 
#define EXT3_MOUNT_QUOTA		0x80000 
#define EXT3_MOUNT_USRQUOTA		0x100000 
#define EXT3_MOUNT_GRPQUOTA		0x200000 
#define EXT3_MOUNT_DATA_ERR_ABORT	0x400000 

#ifndef _LINUX_EXT2_FS_H
#define clear_opt(o, opt)		o &= ~EXT3_MOUNT_##opt
#define set_opt(o, opt)			o |= EXT3_MOUNT_##opt
#define test_opt(sb, opt)		(EXT3_SB(sb)->s_mount_opt & \
					 EXT3_MOUNT_##opt)
#else
#define EXT2_MOUNT_NOLOAD		EXT3_MOUNT_NOLOAD
#define EXT2_MOUNT_ABORT		EXT3_MOUNT_ABORT
#define EXT2_MOUNT_DATA_FLAGS		EXT3_MOUNT_DATA_FLAGS
#endif

#define ext3_set_bit			__set_bit_le
#define ext3_set_bit_atomic		ext2_set_bit_atomic
#define ext3_clear_bit			__clear_bit_le
#define ext3_clear_bit_atomic		ext2_clear_bit_atomic
#define ext3_test_bit			test_bit_le
#define ext3_find_next_zero_bit		find_next_zero_bit_le

#define EXT3_DFL_MAX_MNT_COUNT		20	
#define EXT3_DFL_CHECKINTERVAL		0	

#define EXT3_ERRORS_CONTINUE		1	
#define EXT3_ERRORS_RO			2	
#define EXT3_ERRORS_PANIC		3	
#define EXT3_ERRORS_DEFAULT		EXT3_ERRORS_CONTINUE

struct ext3_super_block {
	__le32	s_inodes_count;		
	__le32	s_blocks_count;		
	__le32	s_r_blocks_count;	
	__le32	s_free_blocks_count;	
	__le32	s_free_inodes_count;	
	__le32	s_first_data_block;	
	__le32	s_log_block_size;	
	__le32	s_log_frag_size;	
	__le32	s_blocks_per_group;	
	__le32	s_frags_per_group;	
	__le32	s_inodes_per_group;	
	__le32	s_mtime;		
	__le32	s_wtime;		
	__le16	s_mnt_count;		
	__le16	s_max_mnt_count;	
	__le16	s_magic;		
	__le16	s_state;		
	__le16	s_errors;		
	__le16	s_minor_rev_level;	
	__le32	s_lastcheck;		
	__le32	s_checkinterval;	
	__le32	s_creator_os;		
	__le32	s_rev_level;		
	__le16	s_def_resuid;		
	__le16	s_def_resgid;		
	__le32	s_first_ino;		
	__le16   s_inode_size;		
	__le16	s_block_group_nr;	
	__le32	s_feature_compat;	
	__le32	s_feature_incompat;	
	__le32	s_feature_ro_compat;	
	__u8	s_uuid[16];		
	char	s_volume_name[16];	
	char	s_last_mounted[64];	
	__le32	s_algorithm_usage_bitmap; 
	__u8	s_prealloc_blocks;	
	__u8	s_prealloc_dir_blocks;	
	__le16	s_reserved_gdt_blocks;	
	__u8	s_journal_uuid[16];	
	__le32	s_journal_inum;		
	__le32	s_journal_dev;		
	__le32	s_last_orphan;		
	__le32	s_hash_seed[4];		
	__u8	s_def_hash_version;	
	__u8	s_reserved_char_pad;
	__u16	s_reserved_word_pad;
	__le32	s_default_mount_opts;
	__le32	s_first_meta_bg;	
	__le32	s_mkfs_time;		
	__le32	s_jnl_blocks[17];	
	
	__le32	s_blocks_count_hi;	
	__le32	s_r_blocks_count_hi;	
	__le32	s_free_blocks_count_hi;	
	__le16	s_min_extra_isize;	
	__le16	s_want_extra_isize; 	
	__le32	s_flags;		
	__le16  s_raid_stride;		
	__le16  s_mmp_interval;         
	__le64  s_mmp_block;            
	__le32  s_raid_stripe_width;    
	__u8	s_log_groups_per_flex;  
	__u8	s_reserved_char_pad2;
	__le16  s_reserved_pad;
	__u32   s_reserved[162];        
};

typedef int ext3_grpblk_t;

typedef unsigned long ext3_fsblk_t;

#define E3FSBLK "%lu"

struct ext3_reserve_window {
	ext3_fsblk_t	_rsv_start;	
	ext3_fsblk_t	_rsv_end;	
};

struct ext3_reserve_window_node {
	struct rb_node		rsv_node;
	__u32			rsv_goal_size;
	__u32			rsv_alloc_hit;
	struct ext3_reserve_window	rsv_window;
};

struct ext3_block_alloc_info {
	
	struct ext3_reserve_window_node	rsv_window_node;
	__u32                   last_alloc_logical_block;
	ext3_fsblk_t		last_alloc_physical_block;
};

#define rsv_start rsv_window._rsv_start
#define rsv_end rsv_window._rsv_end

struct ext3_inode_info {
	__le32	i_data[15];	
	__u32	i_flags;
#ifdef EXT3_FRAGMENTS
	__u32	i_faddr;
	__u8	i_frag_no;
	__u8	i_frag_size;
#endif
	ext3_fsblk_t	i_file_acl;
	__u32	i_dir_acl;
	__u32	i_dtime;

	__u32	i_block_group;
	unsigned long	i_state_flags;	

	
	struct ext3_block_alloc_info *i_block_alloc_info;

	__u32	i_dir_start_lookup;
#ifdef CONFIG_EXT3_FS_XATTR
	struct rw_semaphore xattr_sem;
#endif

	struct list_head i_orphan;	

	loff_t	i_disksize;

	
	__u16 i_extra_isize;

	struct mutex truncate_mutex;

	atomic_t i_sync_tid;
	atomic_t i_datasync_tid;

	struct inode vfs_inode;
};

struct ext3_sb_info {
	unsigned long s_frag_size;	
	unsigned long s_frags_per_block;
	unsigned long s_inodes_per_block;
	unsigned long s_frags_per_group;
	unsigned long s_blocks_per_group;
	unsigned long s_inodes_per_group;
	unsigned long s_itb_per_group;	
	unsigned long s_gdb_count;	
	unsigned long s_desc_per_block;	
	unsigned long s_groups_count;	
	unsigned long s_overhead_last;  
	unsigned long s_blocks_last;    
	struct buffer_head * s_sbh;	
	struct ext3_super_block * s_es;	
	struct buffer_head ** s_group_desc;
	unsigned long  s_mount_opt;
	ext3_fsblk_t s_sb_block;
	uid_t s_resuid;
	gid_t s_resgid;
	unsigned short s_mount_state;
	unsigned short s_pad;
	int s_addr_per_block_bits;
	int s_desc_per_block_bits;
	int s_inode_size;
	int s_first_ino;
	spinlock_t s_next_gen_lock;
	u32 s_next_generation;
	u32 s_hash_seed[4];
	int s_def_hash_version;
	int s_hash_unsigned;	
	struct percpu_counter s_freeblocks_counter;
	struct percpu_counter s_freeinodes_counter;
	struct percpu_counter s_dirs_counter;
	struct blockgroup_lock *s_blockgroup_lock;

	
	spinlock_t s_rsv_window_lock;
	struct rb_root s_rsv_window_root;
	struct ext3_reserve_window_node s_rsv_window_head;

	
	struct inode * s_journal_inode;
	struct journal_s * s_journal;
	struct list_head s_orphan;
	struct mutex s_orphan_lock;
	struct mutex s_resize_lock;
	unsigned long s_commit_interval;
	struct block_device *journal_bdev;
#ifdef CONFIG_QUOTA
	char *s_qf_names[MAXQUOTAS];		
	int s_jquota_fmt;			
#endif
};

static inline spinlock_t *
sb_bgl_lock(struct ext3_sb_info *sbi, unsigned int block_group)
{
	return bgl_lock_ptr(sbi->s_blockgroup_lock, block_group);
}

static inline struct ext3_sb_info * EXT3_SB(struct super_block *sb)
{
	return sb->s_fs_info;
}
static inline struct ext3_inode_info *EXT3_I(struct inode *inode)
{
	return container_of(inode, struct ext3_inode_info, vfs_inode);
}

static inline int ext3_valid_inum(struct super_block *sb, unsigned long ino)
{
	return ino == EXT3_ROOT_INO ||
		ino == EXT3_JOURNAL_INO ||
		ino == EXT3_RESIZE_INO ||
		(ino >= EXT3_FIRST_INO(sb) &&
		 ino <= le32_to_cpu(EXT3_SB(sb)->s_es->s_inodes_count));
}

enum {
	EXT3_STATE_JDATA,		
	EXT3_STATE_NEW,			
	EXT3_STATE_XATTR,		
	EXT3_STATE_FLUSH_ON_CLOSE,	
};

static inline int ext3_test_inode_state(struct inode *inode, int bit)
{
	return test_bit(bit, &EXT3_I(inode)->i_state_flags);
}

static inline void ext3_set_inode_state(struct inode *inode, int bit)
{
	set_bit(bit, &EXT3_I(inode)->i_state_flags);
}

static inline void ext3_clear_inode_state(struct inode *inode, int bit)
{
	clear_bit(bit, &EXT3_I(inode)->i_state_flags);
}

#define NEXT_ORPHAN(inode) EXT3_I(inode)->i_dtime

#define EXT3_OS_LINUX		0
#define EXT3_OS_HURD		1
#define EXT3_OS_MASIX		2
#define EXT3_OS_FREEBSD		3
#define EXT3_OS_LITES		4

#define EXT3_GOOD_OLD_REV	0	
#define EXT3_DYNAMIC_REV	1	

#define EXT3_CURRENT_REV	EXT3_GOOD_OLD_REV
#define EXT3_MAX_SUPP_REV	EXT3_DYNAMIC_REV

#define EXT3_GOOD_OLD_INODE_SIZE 128


#define EXT3_HAS_COMPAT_FEATURE(sb,mask)			\
	( EXT3_SB(sb)->s_es->s_feature_compat & cpu_to_le32(mask) )
#define EXT3_HAS_RO_COMPAT_FEATURE(sb,mask)			\
	( EXT3_SB(sb)->s_es->s_feature_ro_compat & cpu_to_le32(mask) )
#define EXT3_HAS_INCOMPAT_FEATURE(sb,mask)			\
	( EXT3_SB(sb)->s_es->s_feature_incompat & cpu_to_le32(mask) )
#define EXT3_SET_COMPAT_FEATURE(sb,mask)			\
	EXT3_SB(sb)->s_es->s_feature_compat |= cpu_to_le32(mask)
#define EXT3_SET_RO_COMPAT_FEATURE(sb,mask)			\
	EXT3_SB(sb)->s_es->s_feature_ro_compat |= cpu_to_le32(mask)
#define EXT3_SET_INCOMPAT_FEATURE(sb,mask)			\
	EXT3_SB(sb)->s_es->s_feature_incompat |= cpu_to_le32(mask)
#define EXT3_CLEAR_COMPAT_FEATURE(sb,mask)			\
	EXT3_SB(sb)->s_es->s_feature_compat &= ~cpu_to_le32(mask)
#define EXT3_CLEAR_RO_COMPAT_FEATURE(sb,mask)			\
	EXT3_SB(sb)->s_es->s_feature_ro_compat &= ~cpu_to_le32(mask)
#define EXT3_CLEAR_INCOMPAT_FEATURE(sb,mask)			\
	EXT3_SB(sb)->s_es->s_feature_incompat &= ~cpu_to_le32(mask)

#define EXT3_FEATURE_COMPAT_DIR_PREALLOC	0x0001
#define EXT3_FEATURE_COMPAT_IMAGIC_INODES	0x0002
#define EXT3_FEATURE_COMPAT_HAS_JOURNAL		0x0004
#define EXT3_FEATURE_COMPAT_EXT_ATTR		0x0008
#define EXT3_FEATURE_COMPAT_RESIZE_INODE	0x0010
#define EXT3_FEATURE_COMPAT_DIR_INDEX		0x0020

#define EXT3_FEATURE_RO_COMPAT_SPARSE_SUPER	0x0001
#define EXT3_FEATURE_RO_COMPAT_LARGE_FILE	0x0002
#define EXT3_FEATURE_RO_COMPAT_BTREE_DIR	0x0004

#define EXT3_FEATURE_INCOMPAT_COMPRESSION	0x0001
#define EXT3_FEATURE_INCOMPAT_FILETYPE		0x0002
#define EXT3_FEATURE_INCOMPAT_RECOVER		0x0004 
#define EXT3_FEATURE_INCOMPAT_JOURNAL_DEV	0x0008 
#define EXT3_FEATURE_INCOMPAT_META_BG		0x0010

#define EXT3_FEATURE_COMPAT_SUPP	EXT2_FEATURE_COMPAT_EXT_ATTR
#define EXT3_FEATURE_INCOMPAT_SUPP	(EXT3_FEATURE_INCOMPAT_FILETYPE| \
					 EXT3_FEATURE_INCOMPAT_RECOVER| \
					 EXT3_FEATURE_INCOMPAT_META_BG)
#define EXT3_FEATURE_RO_COMPAT_SUPP	(EXT3_FEATURE_RO_COMPAT_SPARSE_SUPER| \
					 EXT3_FEATURE_RO_COMPAT_LARGE_FILE| \
					 EXT3_FEATURE_RO_COMPAT_BTREE_DIR)

#define	EXT3_DEF_RESUID		0
#define	EXT3_DEF_RESGID		0

#define EXT3_DEFM_DEBUG		0x0001
#define EXT3_DEFM_BSDGROUPS	0x0002
#define EXT3_DEFM_XATTR_USER	0x0004
#define EXT3_DEFM_ACL		0x0008
#define EXT3_DEFM_UID16		0x0010
#define EXT3_DEFM_JMODE		0x0060
#define EXT3_DEFM_JMODE_DATA	0x0020
#define EXT3_DEFM_JMODE_ORDERED	0x0040
#define EXT3_DEFM_JMODE_WBACK	0x0060

#define EXT3_NAME_LEN 255

struct ext3_dir_entry {
	__le32	inode;			
	__le16	rec_len;		
	__le16	name_len;		
	char	name[EXT3_NAME_LEN];	
};

struct ext3_dir_entry_2 {
	__le32	inode;			
	__le16	rec_len;		
	__u8	name_len;		
	__u8	file_type;
	char	name[EXT3_NAME_LEN];	
};

#define EXT3_FT_UNKNOWN		0
#define EXT3_FT_REG_FILE	1
#define EXT3_FT_DIR		2
#define EXT3_FT_CHRDEV		3
#define EXT3_FT_BLKDEV		4
#define EXT3_FT_FIFO		5
#define EXT3_FT_SOCK		6
#define EXT3_FT_SYMLINK		7

#define EXT3_FT_MAX		8

#define EXT3_DIR_PAD			4
#define EXT3_DIR_ROUND			(EXT3_DIR_PAD - 1)
#define EXT3_DIR_REC_LEN(name_len)	(((name_len) + 8 + EXT3_DIR_ROUND) & \
					 ~EXT3_DIR_ROUND)
#define EXT3_MAX_REC_LEN		((1<<16)-1)

static inline unsigned ext3_rec_len_from_disk(__le16 dlen)
{
	unsigned len = le16_to_cpu(dlen);

#if (PAGE_CACHE_SIZE >= 65536)
	if (len == EXT3_MAX_REC_LEN)
		return 1 << 16;
#endif
	return len;
}

static inline __le16 ext3_rec_len_to_disk(unsigned len)
{
#if (PAGE_CACHE_SIZE >= 65536)
	if (len == (1 << 16))
		return cpu_to_le16(EXT3_MAX_REC_LEN);
	else if (len > (1 << 16))
		BUG();
#endif
	return cpu_to_le16(len);
}


#define is_dx(dir) (EXT3_HAS_COMPAT_FEATURE(dir->i_sb, \
				      EXT3_FEATURE_COMPAT_DIR_INDEX) && \
		      (EXT3_I(dir)->i_flags & EXT3_INDEX_FL))
#define EXT3_DIR_LINK_MAX(dir) (!is_dx(dir) && (dir)->i_nlink >= EXT3_LINK_MAX)
#define EXT3_DIR_LINK_EMPTY(dir) ((dir)->i_nlink == 2 || (dir)->i_nlink == 1)


#define DX_HASH_LEGACY		0
#define DX_HASH_HALF_MD4	1
#define DX_HASH_TEA		2
#define DX_HASH_LEGACY_UNSIGNED	3
#define DX_HASH_HALF_MD4_UNSIGNED	4
#define DX_HASH_TEA_UNSIGNED		5

struct dx_hash_info
{
	u32		hash;
	u32		minor_hash;
	int		hash_version;
	u32		*seed;
};

#define EXT3_HTREE_EOF	0x7fffffff

#define HASH_NB_ALWAYS		1


struct ext3_iloc
{
	struct buffer_head *bh;
	unsigned long offset;
	unsigned long block_group;
};

static inline struct ext3_inode *ext3_raw_inode(struct ext3_iloc *iloc)
{
	return (struct ext3_inode *) (iloc->bh->b_data + iloc->offset);
}

struct dir_private_info {
	struct rb_root	root;
	struct rb_node	*curr_node;
	struct fname	*extra_fname;
	loff_t		last_pos;
	__u32		curr_hash;
	__u32		curr_minor_hash;
	__u32		next_hash;
};

static inline ext3_fsblk_t
ext3_group_first_block_no(struct super_block *sb, unsigned long group_no)
{
	return group_no * (ext3_fsblk_t)EXT3_BLOCKS_PER_GROUP(sb) +
		le32_to_cpu(EXT3_SB(sb)->s_es->s_first_data_block);
}

#define ERR_BAD_DX_DIR	-75000


# define NORET_TYPE    
# define ATTRIB_NORET  __attribute__((noreturn))
# define NORET_AND     noreturn,

extern int ext3_bg_has_super(struct super_block *sb, int group);
extern unsigned long ext3_bg_num_gdb(struct super_block *sb, int group);
extern ext3_fsblk_t ext3_new_block (handle_t *handle, struct inode *inode,
			ext3_fsblk_t goal, int *errp);
extern ext3_fsblk_t ext3_new_blocks (handle_t *handle, struct inode *inode,
			ext3_fsblk_t goal, unsigned long *count, int *errp);
extern void ext3_free_blocks (handle_t *handle, struct inode *inode,
			ext3_fsblk_t block, unsigned long count);
extern void ext3_free_blocks_sb (handle_t *handle, struct super_block *sb,
				 ext3_fsblk_t block, unsigned long count,
				unsigned long *pdquot_freed_blocks);
extern ext3_fsblk_t ext3_count_free_blocks (struct super_block *);
extern void ext3_check_blocks_bitmap (struct super_block *);
extern struct ext3_group_desc * ext3_get_group_desc(struct super_block * sb,
						    unsigned int block_group,
						    struct buffer_head ** bh);
extern int ext3_should_retry_alloc(struct super_block *sb, int *retries);
extern void ext3_init_block_alloc_info(struct inode *);
extern void ext3_rsv_window_add(struct super_block *sb, struct ext3_reserve_window_node *rsv);
extern int ext3_trim_fs(struct super_block *sb, struct fstrim_range *range);

extern int ext3_check_dir_entry(const char *, struct inode *,
				struct ext3_dir_entry_2 *,
				struct buffer_head *, unsigned long);
extern int ext3_htree_store_dirent(struct file *dir_file, __u32 hash,
				    __u32 minor_hash,
				    struct ext3_dir_entry_2 *dirent);
extern void ext3_htree_free_dir_info(struct dir_private_info *p);

extern int ext3_sync_file(struct file *, loff_t, loff_t, int);

extern int ext3fs_dirhash(const char *name, int len, struct
			  dx_hash_info *hinfo);

extern struct inode * ext3_new_inode (handle_t *, struct inode *,
				      const struct qstr *, umode_t);
extern void ext3_free_inode (handle_t *, struct inode *);
extern struct inode * ext3_orphan_get (struct super_block *, unsigned long);
extern unsigned long ext3_count_free_inodes (struct super_block *);
extern unsigned long ext3_count_dirs (struct super_block *);
extern void ext3_check_inodes_bitmap (struct super_block *);
extern unsigned long ext3_count_free (struct buffer_head *, unsigned);


int ext3_forget(handle_t *handle, int is_metadata, struct inode *inode,
		struct buffer_head *bh, ext3_fsblk_t blocknr);
struct buffer_head * ext3_getblk (handle_t *, struct inode *, long, int, int *);
struct buffer_head * ext3_bread (handle_t *, struct inode *, int, int, int *);
int ext3_get_blocks_handle(handle_t *handle, struct inode *inode,
	sector_t iblock, unsigned long maxblocks, struct buffer_head *bh_result,
	int create);

extern struct inode *ext3_iget(struct super_block *, unsigned long);
extern int  ext3_write_inode (struct inode *, struct writeback_control *);
extern int  ext3_setattr (struct dentry *, struct iattr *);
extern void ext3_evict_inode (struct inode *);
extern int  ext3_sync_inode (handle_t *, struct inode *);
extern void ext3_discard_reservation (struct inode *);
extern void ext3_dirty_inode(struct inode *, int);
extern int ext3_change_inode_journal_flag(struct inode *, int);
extern int ext3_get_inode_loc(struct inode *, struct ext3_iloc *);
extern int ext3_can_truncate(struct inode *inode);
extern void ext3_truncate(struct inode *inode);
extern void ext3_set_inode_flags(struct inode *);
extern void ext3_get_inode_flags(struct ext3_inode_info *);
extern void ext3_set_aops(struct inode *inode);
extern int ext3_fiemap(struct inode *inode, struct fiemap_extent_info *fieinfo,
		       u64 start, u64 len);

extern long ext3_ioctl(struct file *, unsigned int, unsigned long);
extern long ext3_compat_ioctl(struct file *, unsigned int, unsigned long);

extern int ext3_orphan_add(handle_t *, struct inode *);
extern int ext3_orphan_del(handle_t *, struct inode *);
extern int ext3_htree_fill_tree(struct file *dir_file, __u32 start_hash,
				__u32 start_minor_hash, __u32 *next_hash);

extern int ext3_group_add(struct super_block *sb,
				struct ext3_new_group_data *input);
extern int ext3_group_extend(struct super_block *sb,
				struct ext3_super_block *es,
				ext3_fsblk_t n_blocks_count);

extern __printf(3, 4)
void ext3_error(struct super_block *, const char *, const char *, ...);
extern void __ext3_std_error (struct super_block *, const char *, int);
extern __printf(3, 4)
void ext3_abort(struct super_block *, const char *, const char *, ...);
extern __printf(3, 4)
void ext3_warning(struct super_block *, const char *, const char *, ...);
extern __printf(3, 4)
void ext3_msg(struct super_block *, const char *, const char *, ...);
extern void ext3_update_dynamic_rev (struct super_block *sb);

#define ext3_std_error(sb, errno)				\
do {								\
	if ((errno))						\
		__ext3_std_error((sb), __func__, (errno));	\
} while (0)


extern const struct file_operations ext3_dir_operations;

extern const struct inode_operations ext3_file_inode_operations;
extern const struct file_operations ext3_file_operations;

extern const struct inode_operations ext3_dir_inode_operations;
extern const struct inode_operations ext3_special_inode_operations;

extern const struct inode_operations ext3_symlink_inode_operations;
extern const struct inode_operations ext3_fast_symlink_inode_operations;

#define EXT3_JOURNAL(inode)	(EXT3_SB((inode)->i_sb)->s_journal)


#define EXT3_SINGLEDATA_TRANS_BLOCKS	8U


#define EXT3_XATTR_TRANS_BLOCKS		6U


#define EXT3_DATA_TRANS_BLOCKS(sb)	(EXT3_SINGLEDATA_TRANS_BLOCKS + \
					 EXT3_XATTR_TRANS_BLOCKS - 2 + \
					 EXT3_MAXQUOTAS_TRANS_BLOCKS(sb))


#define EXT3_DELETE_TRANS_BLOCKS(sb)   (EXT3_MAXQUOTAS_TRANS_BLOCKS(sb) + 64)


#define EXT3_MAX_TRANS_DATA		64U


#define EXT3_RESERVE_TRANS_BLOCKS	12U

#define EXT3_INDEX_EXTRA_TRANS_BLOCKS	8

#ifdef CONFIG_QUOTA
#define EXT3_QUOTA_TRANS_BLOCKS(sb) (test_opt(sb, QUOTA) ? 2 : 0)
#define EXT3_QUOTA_INIT_BLOCKS(sb) (test_opt(sb, QUOTA) ? (DQUOT_INIT_ALLOC*\
		(EXT3_SINGLEDATA_TRANS_BLOCKS-3)+3+DQUOT_INIT_REWRITE) : 0)
#define EXT3_QUOTA_DEL_BLOCKS(sb) (test_opt(sb, QUOTA) ? (DQUOT_DEL_ALLOC*\
		(EXT3_SINGLEDATA_TRANS_BLOCKS-3)+3+DQUOT_DEL_REWRITE) : 0)
#else
#define EXT3_QUOTA_TRANS_BLOCKS(sb) 0
#define EXT3_QUOTA_INIT_BLOCKS(sb) 0
#define EXT3_QUOTA_DEL_BLOCKS(sb) 0
#endif
#define EXT3_MAXQUOTAS_TRANS_BLOCKS(sb) (MAXQUOTAS*EXT3_QUOTA_TRANS_BLOCKS(sb))
#define EXT3_MAXQUOTAS_INIT_BLOCKS(sb) (MAXQUOTAS*EXT3_QUOTA_INIT_BLOCKS(sb))
#define EXT3_MAXQUOTAS_DEL_BLOCKS(sb) (MAXQUOTAS*EXT3_QUOTA_DEL_BLOCKS(sb))

int
ext3_mark_iloc_dirty(handle_t *handle,
		     struct inode *inode,
		     struct ext3_iloc *iloc);


int ext3_reserve_inode_write(handle_t *handle, struct inode *inode,
			struct ext3_iloc *iloc);

int ext3_mark_inode_dirty(handle_t *handle, struct inode *inode);


static inline void ext3_journal_release_buffer(handle_t *handle,
						struct buffer_head *bh)
{
	journal_release_buffer(handle, bh);
}

void ext3_journal_abort_handle(const char *caller, const char *err_fn,
		struct buffer_head *bh, handle_t *handle, int err);

int __ext3_journal_get_undo_access(const char *where, handle_t *handle,
				struct buffer_head *bh);

int __ext3_journal_get_write_access(const char *where, handle_t *handle,
				struct buffer_head *bh);

int __ext3_journal_forget(const char *where, handle_t *handle,
				struct buffer_head *bh);

int __ext3_journal_revoke(const char *where, handle_t *handle,
				unsigned long blocknr, struct buffer_head *bh);

int __ext3_journal_get_create_access(const char *where,
				handle_t *handle, struct buffer_head *bh);

int __ext3_journal_dirty_metadata(const char *where,
				handle_t *handle, struct buffer_head *bh);

#define ext3_journal_get_undo_access(handle, bh) \
	__ext3_journal_get_undo_access(__func__, (handle), (bh))
#define ext3_journal_get_write_access(handle, bh) \
	__ext3_journal_get_write_access(__func__, (handle), (bh))
#define ext3_journal_revoke(handle, blocknr, bh) \
	__ext3_journal_revoke(__func__, (handle), (blocknr), (bh))
#define ext3_journal_get_create_access(handle, bh) \
	__ext3_journal_get_create_access(__func__, (handle), (bh))
#define ext3_journal_dirty_metadata(handle, bh) \
	__ext3_journal_dirty_metadata(__func__, (handle), (bh))
#define ext3_journal_forget(handle, bh) \
	__ext3_journal_forget(__func__, (handle), (bh))

int ext3_journal_dirty_data(handle_t *handle, struct buffer_head *bh);

handle_t *ext3_journal_start_sb(struct super_block *sb, int nblocks);
int __ext3_journal_stop(const char *where, handle_t *handle);

static inline handle_t *ext3_journal_start(struct inode *inode, int nblocks)
{
	return ext3_journal_start_sb(inode->i_sb, nblocks);
}

#define ext3_journal_stop(handle) \
	__ext3_journal_stop(__func__, (handle))

static inline handle_t *ext3_journal_current_handle(void)
{
	return journal_current_handle();
}

static inline int ext3_journal_extend(handle_t *handle, int nblocks)
{
	return journal_extend(handle, nblocks);
}

static inline int ext3_journal_restart(handle_t *handle, int nblocks)
{
	return journal_restart(handle, nblocks);
}

static inline int ext3_journal_blocks_per_page(struct inode *inode)
{
	return journal_blocks_per_page(inode);
}

static inline int ext3_journal_force_commit(journal_t *journal)
{
	return journal_force_commit(journal);
}

int ext3_force_commit(struct super_block *sb);

static inline int ext3_should_journal_data(struct inode *inode)
{
	if (!S_ISREG(inode->i_mode))
		return 1;
	if (test_opt(inode->i_sb, DATA_FLAGS) == EXT3_MOUNT_JOURNAL_DATA)
		return 1;
	if (EXT3_I(inode)->i_flags & EXT3_JOURNAL_DATA_FL)
		return 1;
	return 0;
}

static inline int ext3_should_order_data(struct inode *inode)
{
	if (!S_ISREG(inode->i_mode))
		return 0;
	if (EXT3_I(inode)->i_flags & EXT3_JOURNAL_DATA_FL)
		return 0;
	if (test_opt(inode->i_sb, DATA_FLAGS) == EXT3_MOUNT_ORDERED_DATA)
		return 1;
	return 0;
}

static inline int ext3_should_writeback_data(struct inode *inode)
{
	if (!S_ISREG(inode->i_mode))
		return 0;
	if (EXT3_I(inode)->i_flags & EXT3_JOURNAL_DATA_FL)
		return 0;
	if (test_opt(inode->i_sb, DATA_FLAGS) == EXT3_MOUNT_WRITEBACK_DATA)
		return 1;
	return 0;
}

#include <trace/events/ext3.h>
