#ifndef _OMFS_FS_H
#define _OMFS_FS_H


#define OMFS_MAGIC 0xC2993D87
#define OMFS_IMAGIC 0xD2

#define OMFS_DIR 'D'
#define OMFS_FILE 'F'
#define OMFS_INODE_NORMAL 'e'
#define OMFS_INODE_CONTINUATION 'c'
#define OMFS_INODE_SYSTEM 's'
#define OMFS_NAMELEN 256
#define OMFS_DIR_START 0x1b8
#define OMFS_EXTENT_START 0x1d0
#define OMFS_EXTENT_CONT 0x40
#define OMFS_XOR_COUNT 19
#define OMFS_MAX_BLOCK_SIZE 8192
#define OMFS_MAX_CLUSTER_SIZE 8

struct omfs_super_block {
	char s_fill1[256];
	__be64 s_root_block;		
	__be64 s_num_blocks;		
	__be32 s_magic;			
	__be32 s_blocksize;		
	__be32 s_mirrors;		
	__be32 s_sys_blocksize;		
};

struct omfs_header {
	__be64 h_self;			
	__be32 h_body_size;		
	__be16 h_crc;			
	char h_fill1[2];
	u8 h_version;			
	char h_type;			
	u8 h_magic;			
	u8 h_check_xor;			
	__be32 h_fill2;
};

struct omfs_root_block {
	struct omfs_header r_head;	
	__be64 r_fill1;
	__be64 r_num_blocks;		
	__be64 r_root_dir;		
	__be64 r_bitmap;		
	__be32 r_blocksize;		
	__be32 r_clustersize;		
	__be64 r_mirrors;		
	char r_name[OMFS_NAMELEN];	
};

struct omfs_inode {
	struct omfs_header i_head;	
	__be64 i_parent;		
	__be64 i_sibling;		
	__be64 i_ctime;			
	char i_fill1[35];
	char i_type;			
	__be32 i_fill2;
	char i_fill3[64];
	char i_name[OMFS_NAMELEN];	
	__be64 i_size;			
};

struct omfs_extent_entry {
	__be64 e_cluster;		
	__be64 e_blocks;		
};

struct omfs_extent {
	__be64 e_next;			
	__be32 e_extent_count;		
	__be32 e_fill;
	struct omfs_extent_entry e_entry;	
};

#endif
