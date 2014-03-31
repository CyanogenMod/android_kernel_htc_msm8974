#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/exportfs.h>
#include <linux/iso_fs.h>
#include <asm/unaligned.h>

enum isofs_file_format {
	isofs_file_normal = 0,
	isofs_file_sparse = 1,
	isofs_file_compressed = 2,
};
	
struct iso_inode_info {
	unsigned long i_iget5_block;
	unsigned long i_iget5_offset;
	unsigned int i_first_extent;
	unsigned char i_file_format;
	unsigned char i_format_parm[3];
	unsigned long i_next_section_block;
	unsigned long i_next_section_offset;
	off_t i_section_size;
	struct inode vfs_inode;
};

struct isofs_sb_info {
	unsigned long s_ninodes;
	unsigned long s_nzones;
	unsigned long s_firstdatazone;
	unsigned long s_log_zone_size;
	unsigned long s_max_size;
	
	int           s_rock_offset; 
	unsigned char s_joliet_level;
	unsigned char s_mapping;
	unsigned int  s_high_sierra:1;
	unsigned int  s_rock:2;
	unsigned int  s_utf8:1;
	unsigned int  s_cruft:1; 
	unsigned int  s_nocompress:1;
	unsigned int  s_hide:1;
	unsigned int  s_showassoc:1;
	unsigned int  s_overriderockperm:1;
	unsigned int  s_uid_set:1;
	unsigned int  s_gid_set:1;

	umode_t s_fmode;
	umode_t s_dmode;
	gid_t s_gid;
	uid_t s_uid;
	struct nls_table *s_nls_iocharset; 
};

#define ISOFS_INVALID_MODE ((umode_t) -1)

static inline struct isofs_sb_info *ISOFS_SB(struct super_block *sb)
{
	return sb->s_fs_info;
}

static inline struct iso_inode_info *ISOFS_I(struct inode *inode)
{
	return container_of(inode, struct iso_inode_info, vfs_inode);
}

static inline int isonum_711(char *p)
{
	return *(u8 *)p;
}
static inline int isonum_712(char *p)
{
	return *(s8 *)p;
}
static inline unsigned int isonum_721(char *p)
{
	return get_unaligned_le16(p);
}
static inline unsigned int isonum_722(char *p)
{
	return get_unaligned_be16(p);
}
static inline unsigned int isonum_723(char *p)
{
	
	return get_unaligned_le16(p);
}
static inline unsigned int isonum_731(char *p)
{
	return get_unaligned_le32(p);
}
static inline unsigned int isonum_732(char *p)
{
	return get_unaligned_be32(p);
}
static inline unsigned int isonum_733(char *p)
{
	
	return get_unaligned_le32(p);
}
extern int iso_date(char *, int);

struct inode;		

extern int parse_rock_ridge_inode(struct iso_directory_record *, struct inode *);
extern int get_rock_ridge_filename(struct iso_directory_record *, char *, struct inode *);
extern int isofs_name_translate(struct iso_directory_record *, char *, struct inode *);

int get_joliet_filename(struct iso_directory_record *, unsigned char *, struct inode *);
int get_acorn_filename(struct iso_directory_record *, char *, struct inode *);

extern struct dentry *isofs_lookup(struct inode *, struct dentry *, struct nameidata *);
extern struct buffer_head *isofs_bread(struct inode *, sector_t);
extern int isofs_get_blocks(struct inode *, sector_t, struct buffer_head **, unsigned long);

extern struct inode *isofs_iget(struct super_block *sb,
                                unsigned long block,
                                unsigned long offset);

static inline unsigned long isofs_get_ino(unsigned long block,
					  unsigned long offset,
					  unsigned long bufbits)
{
	return (block << (bufbits - 5)) | (offset >> 5);
}

static inline void
isofs_normalize_block_and_offset(struct iso_directory_record* de,
				 unsigned long *block,
				 unsigned long *offset)
{
	
	if (de->flags[0] & 2) {
		*offset = 0;
		*block = (unsigned long)isonum_733(de->extent)
			+ (unsigned long)isonum_711(de->ext_attr_length);
	}
}

extern const struct inode_operations isofs_dir_inode_operations;
extern const struct file_operations isofs_dir_operations;
extern const struct address_space_operations isofs_symlink_aops;
extern const struct export_operations isofs_export_ops;
