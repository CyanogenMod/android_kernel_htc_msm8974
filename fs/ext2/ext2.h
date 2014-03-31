/*
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
#include <linux/ext2_fs.h>
#include <linux/blockgroup_lock.h>
#include <linux/percpu_counter.h>
#include <linux/rbtree.h>


typedef int ext2_grpblk_t;

typedef unsigned long ext2_fsblk_t;

#define E2FSBLK "%lu"

struct ext2_reserve_window {
	ext2_fsblk_t		_rsv_start;	
	ext2_fsblk_t		_rsv_end;	
};

struct ext2_reserve_window_node {
	struct rb_node	 	rsv_node;
	__u32			rsv_goal_size;
	__u32			rsv_alloc_hit;
	struct ext2_reserve_window	rsv_window;
};

struct ext2_block_alloc_info {
	
	struct ext2_reserve_window_node	rsv_window_node;
	__u32			last_alloc_logical_block;
	ext2_fsblk_t		last_alloc_physical_block;
};

#define rsv_start rsv_window._rsv_start
#define rsv_end rsv_window._rsv_end

struct ext2_sb_info {
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
	struct ext2_super_block * s_es;	
	struct buffer_head ** s_group_desc;
	unsigned long  s_mount_opt;
	unsigned long s_sb_block;
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
	unsigned long s_dir_count;
	u8 *s_debts;
	struct percpu_counter s_freeblocks_counter;
	struct percpu_counter s_freeinodes_counter;
	struct percpu_counter s_dirs_counter;
	struct blockgroup_lock *s_blockgroup_lock;
	
	spinlock_t s_rsv_window_lock;
	struct rb_root s_rsv_window_root;
	struct ext2_reserve_window_node s_rsv_window_head;
	spinlock_t s_lock;
};

static inline spinlock_t *
sb_bgl_lock(struct ext2_sb_info *sbi, unsigned int block_group)
{
	return bgl_lock_ptr(sbi->s_blockgroup_lock, block_group);
}

#undef EXT2FS_DEBUG

#define EXT2_DEFAULT_RESERVE_BLOCKS     8
#define EXT2_MAX_RESERVE_BLOCKS         1027
#define EXT2_RESERVE_WINDOW_NOT_ALLOCATED 0
#define EXT2FS_DATE		"95/08/09"
#define EXT2FS_VERSION		"0.5b"

#ifdef EXT2FS_DEBUG
#	define ext2_debug(f, a...)	{ \
					printk ("EXT2-fs DEBUG (%s, %d): %s:", \
						__FILE__, __LINE__, __func__); \
				  	printk (f, ## a); \
					}
#else
#	define ext2_debug(f, a...)	
#endif

#define	EXT2_BAD_INO		 1	
#define EXT2_ROOT_INO		 2	
#define EXT2_BOOT_LOADER_INO	 5	
#define EXT2_UNDEL_DIR_INO	 6	

#define EXT2_GOOD_OLD_FIRST_INO	11

static inline struct ext2_sb_info *EXT2_SB(struct super_block *sb)
{
	return sb->s_fs_info;
}

#define EXT2_MIN_BLOCK_SIZE		1024
#define	EXT2_MAX_BLOCK_SIZE		4096
#define EXT2_MIN_BLOCK_LOG_SIZE		  10
#define EXT2_BLOCK_SIZE(s)		((s)->s_blocksize)
#define	EXT2_ADDR_PER_BLOCK(s)		(EXT2_BLOCK_SIZE(s) / sizeof (__u32))
#define EXT2_BLOCK_SIZE_BITS(s)		((s)->s_blocksize_bits)
#define	EXT2_ADDR_PER_BLOCK_BITS(s)	(EXT2_SB(s)->s_addr_per_block_bits)
#define EXT2_INODE_SIZE(s)		(EXT2_SB(s)->s_inode_size)
#define EXT2_FIRST_INO(s)		(EXT2_SB(s)->s_first_ino)

#define EXT2_MIN_FRAG_SIZE		1024
#define	EXT2_MAX_FRAG_SIZE		4096
#define EXT2_MIN_FRAG_LOG_SIZE		  10
#define EXT2_FRAG_SIZE(s)		(EXT2_SB(s)->s_frag_size)
#define EXT2_FRAGS_PER_BLOCK(s)		(EXT2_SB(s)->s_frags_per_block)

struct ext2_group_desc
{
	__le32	bg_block_bitmap;		
	__le32	bg_inode_bitmap;		
	__le32	bg_inode_table;		
	__le16	bg_free_blocks_count;	
	__le16	bg_free_inodes_count;	
	__le16	bg_used_dirs_count;	
	__le16	bg_pad;
	__le32	bg_reserved[3];
};

#define EXT2_BLOCKS_PER_GROUP(s)	(EXT2_SB(s)->s_blocks_per_group)
#define EXT2_DESC_PER_BLOCK(s)		(EXT2_SB(s)->s_desc_per_block)
#define EXT2_INODES_PER_GROUP(s)	(EXT2_SB(s)->s_inodes_per_group)
#define EXT2_DESC_PER_BLOCK_BITS(s)	(EXT2_SB(s)->s_desc_per_block_bits)

#define	EXT2_NDIR_BLOCKS		12
#define	EXT2_IND_BLOCK			EXT2_NDIR_BLOCKS
#define	EXT2_DIND_BLOCK			(EXT2_IND_BLOCK + 1)
#define	EXT2_TIND_BLOCK			(EXT2_DIND_BLOCK + 1)
#define	EXT2_N_BLOCKS			(EXT2_TIND_BLOCK + 1)

#define	EXT2_SECRM_FL			FS_SECRM_FL	
#define	EXT2_UNRM_FL			FS_UNRM_FL	
#define	EXT2_COMPR_FL			FS_COMPR_FL	
#define EXT2_SYNC_FL			FS_SYNC_FL	
#define EXT2_IMMUTABLE_FL		FS_IMMUTABLE_FL	
#define EXT2_APPEND_FL			FS_APPEND_FL	
#define EXT2_NODUMP_FL			FS_NODUMP_FL	
#define EXT2_NOATIME_FL			FS_NOATIME_FL	
#define EXT2_DIRTY_FL			FS_DIRTY_FL
#define EXT2_COMPRBLK_FL		FS_COMPRBLK_FL	
#define EXT2_NOCOMP_FL			FS_NOCOMP_FL	
#define EXT2_ECOMPR_FL			FS_ECOMPR_FL	
	
#define EXT2_BTREE_FL			FS_BTREE_FL	
#define EXT2_INDEX_FL			FS_INDEX_FL	
#define EXT2_IMAGIC_FL			FS_IMAGIC_FL	
#define EXT2_JOURNAL_DATA_FL		FS_JOURNAL_DATA_FL 
#define EXT2_NOTAIL_FL			FS_NOTAIL_FL	
#define EXT2_DIRSYNC_FL			FS_DIRSYNC_FL	
#define EXT2_TOPDIR_FL			FS_TOPDIR_FL	
#define EXT2_RESERVED_FL		FS_RESERVED_FL	

#define EXT2_FL_USER_VISIBLE		FS_FL_USER_VISIBLE	
#define EXT2_FL_USER_MODIFIABLE		FS_FL_USER_MODIFIABLE	

#define EXT2_FL_INHERITED (EXT2_SECRM_FL | EXT2_UNRM_FL | EXT2_COMPR_FL |\
			   EXT2_SYNC_FL | EXT2_NODUMP_FL |\
			   EXT2_NOATIME_FL | EXT2_COMPRBLK_FL |\
			   EXT2_NOCOMP_FL | EXT2_JOURNAL_DATA_FL |\
			   EXT2_NOTAIL_FL | EXT2_DIRSYNC_FL)

#define EXT2_REG_FLMASK (~(EXT2_DIRSYNC_FL | EXT2_TOPDIR_FL))

#define EXT2_OTHER_FLMASK (EXT2_NODUMP_FL | EXT2_NOATIME_FL)

static inline __u32 ext2_mask_flags(umode_t mode, __u32 flags)
{
	if (S_ISDIR(mode))
		return flags;
	else if (S_ISREG(mode))
		return flags & EXT2_REG_FLMASK;
	else
		return flags & EXT2_OTHER_FLMASK;
}

#define	EXT2_IOC_GETFLAGS		FS_IOC_GETFLAGS
#define	EXT2_IOC_SETFLAGS		FS_IOC_SETFLAGS
#define	EXT2_IOC_GETVERSION		FS_IOC_GETVERSION
#define	EXT2_IOC_SETVERSION		FS_IOC_SETVERSION
#define	EXT2_IOC_GETRSVSZ		_IOR('f', 5, long)
#define	EXT2_IOC_SETRSVSZ		_IOW('f', 6, long)

#define EXT2_IOC32_GETFLAGS		FS_IOC32_GETFLAGS
#define EXT2_IOC32_SETFLAGS		FS_IOC32_SETFLAGS
#define EXT2_IOC32_GETVERSION		FS_IOC32_GETVERSION
#define EXT2_IOC32_SETVERSION		FS_IOC32_SETVERSION

struct ext2_inode {
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
			__le32  l_i_reserved1;
		} linux1;
		struct {
			__le32  h_i_translator;
		} hurd1;
		struct {
			__le32  m_i_reserved1;
		} masix1;
	} osd1;				
	__le32	i_block[EXT2_N_BLOCKS];
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
			__le16	h_i_mode_high;
			__le16	h_i_uid_high;
			__le16	h_i_gid_high;
			__le32	h_i_author;
		} hurd2;
		struct {
			__u8	m_i_frag;	
			__u8	m_i_fsize;	
			__u16	m_pad1;
			__u32	m_i_reserved2[2];
		} masix2;
	} osd2;				
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

#define	EXT2_VALID_FS			0x0001	
#define	EXT2_ERROR_FS			0x0002	

#define EXT2_MOUNT_CHECK		0x000001  
#define EXT2_MOUNT_OLDALLOC		0x000002  
#define EXT2_MOUNT_GRPID		0x000004  
#define EXT2_MOUNT_DEBUG		0x000008  
#define EXT2_MOUNT_ERRORS_CONT		0x000010  
#define EXT2_MOUNT_ERRORS_RO		0x000020  
#define EXT2_MOUNT_ERRORS_PANIC		0x000040  
#define EXT2_MOUNT_MINIX_DF		0x000080  
#define EXT2_MOUNT_NOBH			0x000100  
#define EXT2_MOUNT_NO_UID32		0x000200  
#define EXT2_MOUNT_XATTR_USER		0x004000  
#define EXT2_MOUNT_POSIX_ACL		0x008000  
#define EXT2_MOUNT_XIP			0x010000  
#define EXT2_MOUNT_USRQUOTA		0x020000  
#define EXT2_MOUNT_GRPQUOTA		0x040000  
#define EXT2_MOUNT_RESERVATION		0x080000  


#define clear_opt(o, opt)		o &= ~EXT2_MOUNT_##opt
#define set_opt(o, opt)			o |= EXT2_MOUNT_##opt
#define test_opt(sb, opt)		(EXT2_SB(sb)->s_mount_opt & \
					 EXT2_MOUNT_##opt)
#define EXT2_DFL_MAX_MNT_COUNT		20	
#define EXT2_DFL_CHECKINTERVAL		0	

#define EXT2_ERRORS_CONTINUE		1	
#define EXT2_ERRORS_RO			2	
#define EXT2_ERRORS_PANIC		3	
#define EXT2_ERRORS_DEFAULT		EXT2_ERRORS_CONTINUE

struct ext2_super_block {
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
	__u16	s_padding1;
	__u8	s_journal_uuid[16];	
	__u32	s_journal_inum;		
	__u32	s_journal_dev;		
	__u32	s_last_orphan;		
	__u32	s_hash_seed[4];		
	__u8	s_def_hash_version;	
	__u8	s_reserved_char_pad;
	__u16	s_reserved_word_pad;
	__le32	s_default_mount_opts;
 	__le32	s_first_meta_bg; 	
	__u32	s_reserved[190];	
};

#define EXT2_OS_LINUX		0
#define EXT2_OS_HURD		1
#define EXT2_OS_MASIX		2
#define EXT2_OS_FREEBSD		3
#define EXT2_OS_LITES		4

#define EXT2_GOOD_OLD_REV	0	
#define EXT2_DYNAMIC_REV	1 	

#define EXT2_CURRENT_REV	EXT2_GOOD_OLD_REV
#define EXT2_MAX_SUPP_REV	EXT2_DYNAMIC_REV

#define EXT2_GOOD_OLD_INODE_SIZE 128


#define EXT2_HAS_COMPAT_FEATURE(sb,mask)			\
	( EXT2_SB(sb)->s_es->s_feature_compat & cpu_to_le32(mask) )
#define EXT2_HAS_RO_COMPAT_FEATURE(sb,mask)			\
	( EXT2_SB(sb)->s_es->s_feature_ro_compat & cpu_to_le32(mask) )
#define EXT2_HAS_INCOMPAT_FEATURE(sb,mask)			\
	( EXT2_SB(sb)->s_es->s_feature_incompat & cpu_to_le32(mask) )
#define EXT2_SET_COMPAT_FEATURE(sb,mask)			\
	EXT2_SB(sb)->s_es->s_feature_compat |= cpu_to_le32(mask)
#define EXT2_SET_RO_COMPAT_FEATURE(sb,mask)			\
	EXT2_SB(sb)->s_es->s_feature_ro_compat |= cpu_to_le32(mask)
#define EXT2_SET_INCOMPAT_FEATURE(sb,mask)			\
	EXT2_SB(sb)->s_es->s_feature_incompat |= cpu_to_le32(mask)
#define EXT2_CLEAR_COMPAT_FEATURE(sb,mask)			\
	EXT2_SB(sb)->s_es->s_feature_compat &= ~cpu_to_le32(mask)
#define EXT2_CLEAR_RO_COMPAT_FEATURE(sb,mask)			\
	EXT2_SB(sb)->s_es->s_feature_ro_compat &= ~cpu_to_le32(mask)
#define EXT2_CLEAR_INCOMPAT_FEATURE(sb,mask)			\
	EXT2_SB(sb)->s_es->s_feature_incompat &= ~cpu_to_le32(mask)

#define EXT2_FEATURE_COMPAT_DIR_PREALLOC	0x0001
#define EXT2_FEATURE_COMPAT_IMAGIC_INODES	0x0002
#define EXT3_FEATURE_COMPAT_HAS_JOURNAL		0x0004
#define EXT2_FEATURE_COMPAT_EXT_ATTR		0x0008
#define EXT2_FEATURE_COMPAT_RESIZE_INO		0x0010
#define EXT2_FEATURE_COMPAT_DIR_INDEX		0x0020
#define EXT2_FEATURE_COMPAT_ANY			0xffffffff

#define EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER	0x0001
#define EXT2_FEATURE_RO_COMPAT_LARGE_FILE	0x0002
#define EXT2_FEATURE_RO_COMPAT_BTREE_DIR	0x0004
#define EXT2_FEATURE_RO_COMPAT_ANY		0xffffffff

#define EXT2_FEATURE_INCOMPAT_COMPRESSION	0x0001
#define EXT2_FEATURE_INCOMPAT_FILETYPE		0x0002
#define EXT3_FEATURE_INCOMPAT_RECOVER		0x0004
#define EXT3_FEATURE_INCOMPAT_JOURNAL_DEV	0x0008
#define EXT2_FEATURE_INCOMPAT_META_BG		0x0010
#define EXT2_FEATURE_INCOMPAT_ANY		0xffffffff

#define EXT2_FEATURE_COMPAT_SUPP	EXT2_FEATURE_COMPAT_EXT_ATTR
#define EXT2_FEATURE_INCOMPAT_SUPP	(EXT2_FEATURE_INCOMPAT_FILETYPE| \
					 EXT2_FEATURE_INCOMPAT_META_BG)
#define EXT2_FEATURE_RO_COMPAT_SUPP	(EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER| \
					 EXT2_FEATURE_RO_COMPAT_LARGE_FILE| \
					 EXT2_FEATURE_RO_COMPAT_BTREE_DIR)
#define EXT2_FEATURE_RO_COMPAT_UNSUPPORTED	~EXT2_FEATURE_RO_COMPAT_SUPP
#define EXT2_FEATURE_INCOMPAT_UNSUPPORTED	~EXT2_FEATURE_INCOMPAT_SUPP

#define	EXT2_DEF_RESUID		0
#define	EXT2_DEF_RESGID		0

#define EXT2_DEFM_DEBUG		0x0001
#define EXT2_DEFM_BSDGROUPS	0x0002
#define EXT2_DEFM_XATTR_USER	0x0004
#define EXT2_DEFM_ACL		0x0008
#define EXT2_DEFM_UID16		0x0010
    
#define EXT3_DEFM_JMODE		0x0060 
#define EXT3_DEFM_JMODE_DATA	0x0020
#define EXT3_DEFM_JMODE_ORDERED	0x0040
#define EXT3_DEFM_JMODE_WBACK	0x0060


struct ext2_dir_entry {
	__le32	inode;			
	__le16	rec_len;		
	__le16	name_len;		
	char	name[];			
};

struct ext2_dir_entry_2 {
	__le32	inode;			
	__le16	rec_len;		
	__u8	name_len;		
	__u8	file_type;
	char	name[];			
};

enum {
	EXT2_FT_UNKNOWN		= 0,
	EXT2_FT_REG_FILE	= 1,
	EXT2_FT_DIR		= 2,
	EXT2_FT_CHRDEV		= 3,
	EXT2_FT_BLKDEV		= 4,
	EXT2_FT_FIFO		= 5,
	EXT2_FT_SOCK		= 6,
	EXT2_FT_SYMLINK		= 7,
	EXT2_FT_MAX
};

#define EXT2_DIR_PAD		 	4
#define EXT2_DIR_ROUND 			(EXT2_DIR_PAD - 1)
#define EXT2_DIR_REC_LEN(name_len)	(((name_len) + 8 + EXT2_DIR_ROUND) & \
					 ~EXT2_DIR_ROUND)
#define EXT2_MAX_REC_LEN		((1<<16)-1)

static inline void verify_offsets(void)
{
#define A(x,y) BUILD_BUG_ON(x != offsetof(struct ext2_super_block, y));
	A(EXT2_SB_MAGIC_OFFSET, s_magic);
	A(EXT2_SB_BLOCKS_OFFSET, s_blocks_count);
	A(EXT2_SB_BSIZE_OFFSET, s_log_block_size);
#undef A
}

struct ext2_mount_options {
	unsigned long s_mount_opt;
	uid_t s_resuid;
	gid_t s_resgid;
};

struct ext2_inode_info {
	__le32	i_data[15];
	__u32	i_flags;
	__u32	i_faddr;
	__u8	i_frag_no;
	__u8	i_frag_size;
	__u16	i_state;
	__u32	i_file_acl;
	__u32	i_dir_acl;
	__u32	i_dtime;

	__u32	i_block_group;

	
	struct ext2_block_alloc_info *i_block_alloc_info;

	__u32	i_dir_start_lookup;
#ifdef CONFIG_EXT2_FS_XATTR
	struct rw_semaphore xattr_sem;
#endif
	rwlock_t i_meta_lock;

	struct mutex truncate_mutex;
	struct inode	vfs_inode;
	struct list_head i_orphan;	
};

#define EXT2_STATE_NEW			0x00000001 




static inline struct ext2_inode_info *EXT2_I(struct inode *inode)
{
	return container_of(inode, struct ext2_inode_info, vfs_inode);
}

extern int ext2_bg_has_super(struct super_block *sb, int group);
extern unsigned long ext2_bg_num_gdb(struct super_block *sb, int group);
extern ext2_fsblk_t ext2_new_block(struct inode *, unsigned long, int *);
extern ext2_fsblk_t ext2_new_blocks(struct inode *, unsigned long,
				unsigned long *, int *);
extern void ext2_free_blocks (struct inode *, unsigned long,
			      unsigned long);
extern unsigned long ext2_count_free_blocks (struct super_block *);
extern unsigned long ext2_count_dirs (struct super_block *);
extern void ext2_check_blocks_bitmap (struct super_block *);
extern struct ext2_group_desc * ext2_get_group_desc(struct super_block * sb,
						    unsigned int block_group,
						    struct buffer_head ** bh);
extern void ext2_discard_reservation (struct inode *);
extern int ext2_should_retry_alloc(struct super_block *sb, int *retries);
extern void ext2_init_block_alloc_info(struct inode *);
extern void ext2_rsv_window_add(struct super_block *sb, struct ext2_reserve_window_node *rsv);

extern int ext2_add_link (struct dentry *, struct inode *);
extern ino_t ext2_inode_by_name(struct inode *, struct qstr *);
extern int ext2_make_empty(struct inode *, struct inode *);
extern struct ext2_dir_entry_2 * ext2_find_entry (struct inode *,struct qstr *, struct page **);
extern int ext2_delete_entry (struct ext2_dir_entry_2 *, struct page *);
extern int ext2_empty_dir (struct inode *);
extern struct ext2_dir_entry_2 * ext2_dotdot (struct inode *, struct page **);
extern void ext2_set_link(struct inode *, struct ext2_dir_entry_2 *, struct page *, struct inode *, int);

extern struct inode * ext2_new_inode (struct inode *, umode_t, const struct qstr *);
extern void ext2_free_inode (struct inode *);
extern unsigned long ext2_count_free_inodes (struct super_block *);
extern void ext2_check_inodes_bitmap (struct super_block *);
extern unsigned long ext2_count_free (struct buffer_head *, unsigned);

extern struct inode *ext2_iget (struct super_block *, unsigned long);
extern int ext2_write_inode (struct inode *, struct writeback_control *);
extern void ext2_evict_inode(struct inode *);
extern int ext2_get_block(struct inode *, sector_t, struct buffer_head *, int);
extern int ext2_setattr (struct dentry *, struct iattr *);
extern void ext2_set_inode_flags(struct inode *inode);
extern void ext2_get_inode_flags(struct ext2_inode_info *);
extern int ext2_fiemap(struct inode *inode, struct fiemap_extent_info *fieinfo,
		       u64 start, u64 len);

extern long ext2_ioctl(struct file *, unsigned int, unsigned long);
extern long ext2_compat_ioctl(struct file *, unsigned int, unsigned long);

struct dentry *ext2_get_parent(struct dentry *child);

extern __printf(3, 4)
void ext2_error(struct super_block *, const char *, const char *, ...);
extern __printf(3, 4)
void ext2_msg(struct super_block *, const char *, const char *, ...);
extern void ext2_update_dynamic_rev (struct super_block *sb);
extern void ext2_write_super (struct super_block *);


extern const struct file_operations ext2_dir_operations;

extern int ext2_fsync(struct file *file, loff_t start, loff_t end,
		      int datasync);
extern const struct inode_operations ext2_file_inode_operations;
extern const struct file_operations ext2_file_operations;
extern const struct file_operations ext2_xip_file_operations;

extern const struct address_space_operations ext2_aops;
extern const struct address_space_operations ext2_aops_xip;
extern const struct address_space_operations ext2_nobh_aops;

extern const struct inode_operations ext2_dir_inode_operations;
extern const struct inode_operations ext2_special_inode_operations;

extern const struct inode_operations ext2_fast_symlink_inode_operations;
extern const struct inode_operations ext2_symlink_inode_operations;

static inline ext2_fsblk_t
ext2_group_first_block_no(struct super_block *sb, unsigned long group_no)
{
	return group_no * (ext2_fsblk_t)EXT2_BLOCKS_PER_GROUP(sb) +
		le32_to_cpu(EXT2_SB(sb)->s_es->s_first_data_block);
}

#define ext2_set_bit	__test_and_set_bit_le
#define ext2_clear_bit	__test_and_clear_bit_le
#define ext2_test_bit	test_bit_le
#define ext2_find_first_zero_bit	find_first_zero_bit_le
#define ext2_find_next_zero_bit		find_next_zero_bit_le
