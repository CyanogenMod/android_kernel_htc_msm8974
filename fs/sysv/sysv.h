#ifndef _SYSV_H
#define _SYSV_H

#include <linux/buffer_head.h>

typedef __u16 __bitwise __fs16;
typedef __u32 __bitwise __fs32;

#include <linux/sysv_fs.h>


struct sysv_sb_info {
	struct super_block *s_sb;	
	int	       s_type;		
	char	       s_bytesex;	
	char	       s_truncate;	
					
	unsigned int   s_inodes_per_block;	
	unsigned int   s_inodes_per_block_1;	
	unsigned int   s_inodes_per_block_bits;	
	unsigned int   s_ind_per_block;		
	unsigned int   s_ind_per_block_bits;	
	unsigned int   s_ind_per_block_2;	
	unsigned int   s_toobig_block;		
	unsigned int   s_block_base;	
	unsigned short s_fic_size;	
	unsigned short s_flc_size;	
	
	struct buffer_head *s_bh1;
	struct buffer_head *s_bh2;
	char *         s_sbd1;		
	char *         s_sbd2;		
	__fs16         *s_sb_fic_count;	
        sysv_ino_t     *s_sb_fic_inodes; 
	__fs16         *s_sb_total_free_inodes; 
	__fs16         *s_bcache_count;	
	sysv_zone_t    *s_bcache;	
	__fs32         *s_free_blocks;	
	__fs32         *s_sb_time;	
	__fs32         *s_sb_state;	
	u32            s_firstinodezone; 
	u32            s_firstdatazone;	
	u32            s_ninodes;	
	u32            s_ndatazones;	
	u32            s_nzones;	
	u16	       s_namelen;       
	int	       s_forced_ro;
};

struct sysv_inode_info {
	__fs32		i_data[13];
	u32		i_dir_start_lookup;
	struct inode	vfs_inode;
};


static inline struct sysv_inode_info *SYSV_I(struct inode *inode)
{
	return list_entry(inode, struct sysv_inode_info, vfs_inode);
}

static inline struct sysv_sb_info *SYSV_SB(struct super_block *sb)
{
	return sb->s_fs_info;
}


enum {
	FSTYPE_NONE = 0,
	FSTYPE_XENIX,
	FSTYPE_SYSV4,
	FSTYPE_SYSV2,
	FSTYPE_COH,
	FSTYPE_V7,
	FSTYPE_AFS,
	FSTYPE_END,
};

#define SYSV_MAGIC_BASE		0x012FF7B3

#define XENIX_SUPER_MAGIC	(SYSV_MAGIC_BASE+FSTYPE_XENIX)
#define SYSV4_SUPER_MAGIC	(SYSV_MAGIC_BASE+FSTYPE_SYSV4)
#define SYSV2_SUPER_MAGIC	(SYSV_MAGIC_BASE+FSTYPE_SYSV2)
#define COH_SUPER_MAGIC		(SYSV_MAGIC_BASE+FSTYPE_COH)


enum {
	XENIX_LINK_MAX	=	126,	
	SYSV_LINK_MAX	=	126,	
	V7_LINK_MAX     =	126,	
	COH_LINK_MAX	=	10000,
};


static inline void dirty_sb(struct super_block *sb)
{
	struct sysv_sb_info *sbi = SYSV_SB(sb);

	mark_buffer_dirty(sbi->s_bh1);
	if (sbi->s_bh1 != sbi->s_bh2)
		mark_buffer_dirty(sbi->s_bh2);
	sb->s_dirt = 1;
}


extern struct sysv_inode *sysv_raw_inode(struct super_block *, unsigned,
			struct buffer_head **);
extern struct inode * sysv_new_inode(const struct inode *, umode_t);
extern void sysv_free_inode(struct inode *);
extern unsigned long sysv_count_free_inodes(struct super_block *);

extern sysv_zone_t sysv_new_block(struct super_block *);
extern void sysv_free_block(struct super_block *, sysv_zone_t);
extern unsigned long sysv_count_free_blocks(struct super_block *);

extern void sysv_truncate(struct inode *);
extern int sysv_prepare_chunk(struct page *page, loff_t pos, unsigned len);

extern struct inode *sysv_iget(struct super_block *, unsigned int);
extern int sysv_write_inode(struct inode *, struct writeback_control *wbc);
extern int sysv_sync_inode(struct inode *);
extern void sysv_set_inode(struct inode *, dev_t);
extern int sysv_getattr(struct vfsmount *, struct dentry *, struct kstat *);
extern int sysv_init_icache(void);
extern void sysv_destroy_icache(void);


extern struct sysv_dir_entry *sysv_find_entry(struct dentry *, struct page **);
extern int sysv_add_link(struct dentry *, struct inode *);
extern int sysv_delete_entry(struct sysv_dir_entry *, struct page *);
extern int sysv_make_empty(struct inode *, struct inode *);
extern int sysv_empty_dir(struct inode *);
extern void sysv_set_link(struct sysv_dir_entry *, struct page *,
			struct inode *);
extern struct sysv_dir_entry *sysv_dotdot(struct inode *, struct page **);
extern ino_t sysv_inode_by_name(struct dentry *);


extern const struct inode_operations sysv_file_inode_operations;
extern const struct inode_operations sysv_dir_inode_operations;
extern const struct inode_operations sysv_fast_symlink_inode_operations;
extern const struct file_operations sysv_file_operations;
extern const struct file_operations sysv_dir_operations;
extern const struct address_space_operations sysv_aops;
extern const struct super_operations sysv_sops;
extern const struct dentry_operations sysv_dentry_operations;


enum {
	BYTESEX_LE,
	BYTESEX_PDP,
	BYTESEX_BE,
};

static inline u32 PDP_swab(u32 x)
{
#ifdef __LITTLE_ENDIAN
	return ((x & 0xffff) << 16) | ((x & 0xffff0000) >> 16);
#else
#ifdef __BIG_ENDIAN
	return ((x & 0xff00ff) << 8) | ((x & 0xff00ff00) >> 8);
#else
#error BYTESEX
#endif
#endif
}

static inline __u32 fs32_to_cpu(struct sysv_sb_info *sbi, __fs32 n)
{
	if (sbi->s_bytesex == BYTESEX_PDP)
		return PDP_swab((__force __u32)n);
	else if (sbi->s_bytesex == BYTESEX_LE)
		return le32_to_cpu((__force __le32)n);
	else
		return be32_to_cpu((__force __be32)n);
}

static inline __fs32 cpu_to_fs32(struct sysv_sb_info *sbi, __u32 n)
{
	if (sbi->s_bytesex == BYTESEX_PDP)
		return (__force __fs32)PDP_swab(n);
	else if (sbi->s_bytesex == BYTESEX_LE)
		return (__force __fs32)cpu_to_le32(n);
	else
		return (__force __fs32)cpu_to_be32(n);
}

static inline __fs32 fs32_add(struct sysv_sb_info *sbi, __fs32 *n, int d)
{
	if (sbi->s_bytesex == BYTESEX_PDP)
		*(__u32*)n = PDP_swab(PDP_swab(*(__u32*)n)+d);
	else if (sbi->s_bytesex == BYTESEX_LE)
		le32_add_cpu((__le32 *)n, d);
	else
		be32_add_cpu((__be32 *)n, d);
	return *n;
}

static inline __u16 fs16_to_cpu(struct sysv_sb_info *sbi, __fs16 n)
{
	if (sbi->s_bytesex != BYTESEX_BE)
		return le16_to_cpu((__force __le16)n);
	else
		return be16_to_cpu((__force __be16)n);
}

static inline __fs16 cpu_to_fs16(struct sysv_sb_info *sbi, __u16 n)
{
	if (sbi->s_bytesex != BYTESEX_BE)
		return (__force __fs16)cpu_to_le16(n);
	else
		return (__force __fs16)cpu_to_be16(n);
}

static inline __fs16 fs16_add(struct sysv_sb_info *sbi, __fs16 *n, int d)
{
	if (sbi->s_bytesex != BYTESEX_BE)
		le16_add_cpu((__le16 *)n, d);
	else
		be16_add_cpu((__be16 *)n, d);
	return *n;
}

#endif 
