/*
 * fs/logfs/logfs_abi.h
 *
 * As should be obvious for Linux kernel code, license is GPLv2
 *
 * Copyright (c) 2005-2008 Joern Engel <joern@logfs.org>
 *
 * Public header for logfs.
 */
#ifndef FS_LOGFS_LOGFS_ABI_H
#define FS_LOGFS_LOGFS_ABI_H

#ifndef BUILD_BUG_ON
#define BUILD_BUG_ON(condition) 
#endif

#define SIZE_CHECK(type, size)					\
static inline void check_##type(void)				\
{								\
	BUILD_BUG_ON(sizeof(struct type) != (size));		\
}




#define LOGFS_MAGIC		0x7a3a8e5cb9d5bf67ull
#define LOGFS_MAGIC_U32		0xc97e8168u

#define LOGFS_BLOCKSIZE		(4096ull)
#define LOGFS_BLOCK_FACTOR	(LOGFS_BLOCKSIZE / sizeof(u64))
#define LOGFS_BLOCK_BITS	(9)

#define I0_BLOCKS		(16)
#define I1_BLOCKS		LOGFS_BLOCK_FACTOR
#define I2_BLOCKS		(LOGFS_BLOCK_FACTOR * I1_BLOCKS)
#define I3_BLOCKS		(LOGFS_BLOCK_FACTOR * I2_BLOCKS)
#define I4_BLOCKS		(LOGFS_BLOCK_FACTOR * I3_BLOCKS)
#define I5_BLOCKS		(LOGFS_BLOCK_FACTOR * I4_BLOCKS)

#define INDIRECT_INDEX		I0_BLOCKS
#define LOGFS_EMBEDDED_FIELDS	(I0_BLOCKS + 1)

#define LOGFS_EMBEDDED_SIZE	(LOGFS_EMBEDDED_FIELDS * sizeof(u64))
#define LOGFS_I0_SIZE		(I0_BLOCKS * LOGFS_BLOCKSIZE)
#define LOGFS_I1_SIZE		(I1_BLOCKS * LOGFS_BLOCKSIZE)
#define LOGFS_I2_SIZE		(I2_BLOCKS * LOGFS_BLOCKSIZE)
#define LOGFS_I3_SIZE		(I3_BLOCKS * LOGFS_BLOCKSIZE)
#define LOGFS_I4_SIZE		(I4_BLOCKS * LOGFS_BLOCKSIZE)
#define LOGFS_I5_SIZE		(I5_BLOCKS * LOGFS_BLOCKSIZE)

#define LOGFS_FULLY_POPULATED (1ULL << 63)
#define pure_ofs(ofs) (ofs & ~LOGFS_FULLY_POPULATED)

#define LOGFS_MAX_INDIRECT	(5)
#define LOGFS_MAX_LEVELS	(LOGFS_MAX_INDIRECT + 1)
#define LOGFS_NO_AREAS		(2 * LOGFS_MAX_LEVELS)

#define LOGFS_MAX_NAMELEN	(255)

#define LOGFS_JOURNAL_SEGS	(16)

#define MAX_CACHED_SEGS		(64)


#define LOGFS_OBJECT_HEADERSIZE	(0x1c)
#define LOGFS_SEGMENT_HEADERSIZE (0x18)
#define LOGFS_MAX_OBJECTSIZE	(LOGFS_OBJECT_HEADERSIZE + LOGFS_BLOCKSIZE)
#define LOGFS_SEGMENT_RESERVE	\
	(LOGFS_SEGMENT_HEADERSIZE + LOGFS_MAX_OBJECTSIZE - 1)

enum {
	SEG_SUPER	= 0x01,
	SEG_JOURNAL	= 0x02,
	SEG_OSTORE	= 0x03,
};

struct logfs_segment_header {
	__be32	crc;
	__be16	pad;
	__u8	type;
	__u8	level;
	__be32	segno;
	__be32	ec;
	__be64	gec;
};

SIZE_CHECK(logfs_segment_header, LOGFS_SEGMENT_HEADERSIZE);

#define LOGFS_FEATURES_INCOMPAT		(0ull)
#define LOGFS_FEATURES_RO_COMPAT	(0ull)
#define LOGFS_FEATURES_COMPAT		(0ull)

struct logfs_disk_super {
	struct logfs_segment_header ds_sh;
	__be64	ds_magic;

	__be32	ds_crc;
	__u8	ds_ifile_levels;
	__u8	ds_iblock_levels;
	__u8	ds_data_levels;
	__u8	ds_segment_shift;
	__u8	ds_block_shift;
	__u8	ds_write_shift;
	__u8	pad0[6];

	__be64	ds_filesystem_size;
	__be32	ds_segment_size;
	__be32  ds_bad_seg_reserve;

	__be64	ds_feature_incompat;
	__be64	ds_feature_ro_compat;

	__be64	ds_feature_compat;
	__be64	ds_feature_flags;

	__be64	ds_root_reserve;
	__be64  ds_speed_reserve;

	__be32	ds_journal_seg[LOGFS_JOURNAL_SEGS];

	__be64	ds_super_ofs[2];
	__be64	pad3[8];
};

SIZE_CHECK(logfs_disk_super, 256);

enum {
	OBJ_BLOCK	= 0x04,
	OBJ_INODE	= 0x05,
	OBJ_DENTRY	= 0x06,
};

struct logfs_object_header {
	__be32	crc;
	__be16	len;
	__u8	type;
	__u8	compr;
	__be64	ino;
	__be64	bix;
	__be32	data_crc;
} __attribute__((packed));

SIZE_CHECK(logfs_object_header, LOGFS_OBJECT_HEADERSIZE);

enum {
	LOGFS_INO_MAPPING	= 0x00,
	LOGFS_INO_MASTER	= 0x01,
	LOGFS_INO_ROOT		= 0x02,
	LOGFS_INO_SEGFILE	= 0x03,
	LOGFS_RESERVED_INOS	= 0x10,
};

/*
 * Inode flags.  High bits should never be written to the medium.  They are
 * reserved for in-memory usage.
 * Low bits should either remain in sync with the corresponding FS_*_FL or
 * reuse slots that obviously don't make sense for logfs.
 *
 * LOGFS_IF_DIRTY	Inode must be written back
 * LOGFS_IF_ZOMBIE	Inode has been deleted
 * LOGFS_IF_STILLBORN	-ENOSPC happened when creating inode
 */
#define LOGFS_IF_COMPRESSED	0x00000004 
#define LOGFS_IF_DIRTY		0x20000000
#define LOGFS_IF_ZOMBIE		0x40000000
#define LOGFS_IF_STILLBORN	0x80000000

#define LOGFS_FL_USER_VISIBLE	(LOGFS_IF_COMPRESSED)
#define LOGFS_FL_USER_MODIFIABLE (LOGFS_IF_COMPRESSED)
#define LOGFS_FL_INHERITED	(LOGFS_IF_COMPRESSED)

struct logfs_disk_inode {
	__be16	di_mode;
	__u8	di_height;
	__u8	di_pad;
	__be32	di_flags;
	__be32	di_uid;
	__be32	di_gid;

	__be64	di_ctime;
	__be64	di_mtime;

	__be64	di_atime;
	__be32	di_refcount;
	__be32	di_generation;

	__be64	di_used_bytes;
	__be64	di_size;

	__be64	di_data[LOGFS_EMBEDDED_FIELDS];
};

SIZE_CHECK(logfs_disk_inode, 200);

#define INODE_POINTER_OFS \
	(offsetof(struct logfs_disk_inode, di_data) / sizeof(__be64))
#define INODE_USED_OFS \
	(offsetof(struct logfs_disk_inode, di_used_bytes) / sizeof(__be64))
#define INODE_SIZE_OFS \
	(offsetof(struct logfs_disk_inode, di_size) / sizeof(__be64))
#define INODE_HEIGHT_OFS	(0)

struct logfs_disk_dentry {
	__be64	ino;
	__be16	namelen;
	__u8	type;
	__u8	name[LOGFS_MAX_NAMELEN];
} __attribute__((packed));

SIZE_CHECK(logfs_disk_dentry, 266);

#define RESERVED		0xffffffff
#define BADSEG			0xffffffff
struct logfs_segment_entry {
	__be32	ec_level;
	__be32	valid;
};

SIZE_CHECK(logfs_segment_entry, 8);

struct logfs_journal_header {
	__be32	h_crc;
	__be16	h_len;
	__be16	h_datalen;
	__be16	h_type;
	__u8	h_compr;
	__u8	h_pad[5];
};

SIZE_CHECK(logfs_journal_header, 16);

enum logfs_vim {
	VIM_DEFAULT	= 0,
	VIM_SEGFILE	= 1,
};

struct logfs_je_area {
	__be32	segno;
	__be32	used_bytes;
	__u8	gc_level;
	__u8	vim;
} __attribute__((packed));

SIZE_CHECK(logfs_je_area, 10);

#define MAX_JOURNAL_HEADER \
	(sizeof(struct logfs_journal_header) + sizeof(struct logfs_je_area))

struct logfs_je_dynsb {
	__be64	ds_gec;
	__be64	ds_sweeper;

	__be64	ds_rename_dir;
	__be64	ds_rename_pos;

	__be64	ds_victim_ino;
	__be64	ds_victim_parent; 

	__be64	ds_used_bytes;
	__be32	ds_generation;
	__be32	pad;
};

SIZE_CHECK(logfs_je_dynsb, 64);

struct logfs_je_anchor {
	__be64	da_size;
	__be64	da_last_ino;

	__be64	da_used_bytes;
	u8	da_height;
	u8	pad[7];

	__be64	da_data[LOGFS_EMBEDDED_FIELDS];
};

SIZE_CHECK(logfs_je_anchor, 168);

struct logfs_je_spillout {
	__be64	so_segment[0];
};

SIZE_CHECK(logfs_je_spillout, 0);

struct logfs_je_journal_ec {
	__be32	ec[0];
};

SIZE_CHECK(logfs_je_journal_ec, 0);

struct logfs_je_free_segments {
	__be32	segno;
	__be32	ec;
};

SIZE_CHECK(logfs_je_free_segments, 8);

struct logfs_seg_alias {
	__be32	old_segno;
	__be32	new_segno;
};

SIZE_CHECK(logfs_seg_alias, 8);

struct logfs_obj_alias {
	__be64	ino;
	__be64	bix;
	__be64	val;
	u8	level;
	u8	pad[5];
	__be16	child_no;
};

SIZE_CHECK(logfs_obj_alias, 32);

enum {
	COMPR_NONE	= 0,
	COMPR_ZLIB	= 1,
};

enum {
	JE_FIRST	= 0x01,

	JEG_BASE	= 0x00,
	JE_COMMIT	= 0x02,
	JE_DYNSB	= 0x03,
	JE_ANCHOR	= 0x04,
	JE_ERASECOUNT	= 0x05,
	JE_SPILLOUT	= 0x06,
	JE_OBJ_ALIAS	= 0x0d,
	JE_AREA		= 0x0e,

	JE_LAST		= 0x0e,
};

#endif
