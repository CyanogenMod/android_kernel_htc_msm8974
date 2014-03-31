/*
 * YAFFS: Yet another Flash File System . A NAND-flash specific file system.
 *
 * Copyright (C) 2002-2010 Aleph One Ltd.
 *   for Toby Churchill Ltd and Brightstar Engineering
 *
 * Created by Charles Manning <charles@aleph1.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1 as
 * published by the Free Software Foundation.
 *
 * Note: Only YAFFS headers are LGPL, YAFFS C code is covered by GPL.
 */

#ifndef __YAFFS_GUTS_H__
#define __YAFFS_GUTS_H__

#include "yportenv.h"

#define YAFFS_OK	1
#define YAFFS_FAIL  0

#define YAFFS_MAGIC			0x5941FF53

#define YAFFS_NTNODES_LEVEL0	  	16
#define YAFFS_TNODES_LEVEL0_BITS	4
#define YAFFS_TNODES_LEVEL0_MASK	0xf

#define YAFFS_NTNODES_INTERNAL 		(YAFFS_NTNODES_LEVEL0 / 2)
#define YAFFS_TNODES_INTERNAL_BITS 	(YAFFS_TNODES_LEVEL0_BITS - 1)
#define YAFFS_TNODES_INTERNAL_MASK	0x7
#define YAFFS_TNODES_MAX_LEVEL		6

#ifndef CONFIG_YAFFS_NO_YAFFS1
#define YAFFS_BYTES_PER_SPARE		16
#define YAFFS_BYTES_PER_CHUNK		512
#define YAFFS_CHUNK_SIZE_SHIFT		9
#define YAFFS_CHUNKS_PER_BLOCK		32
#define YAFFS_BYTES_PER_BLOCK		(YAFFS_CHUNKS_PER_BLOCK*YAFFS_BYTES_PER_CHUNK)
#endif

#define YAFFS_MIN_YAFFS2_CHUNK_SIZE 	1024
#define YAFFS_MIN_YAFFS2_SPARE_SIZE	32

#define YAFFS_MAX_CHUNK_ID		0x000FFFFF

#define YAFFS_ALLOCATION_NOBJECTS	100
#define YAFFS_ALLOCATION_NTNODES	100
#define YAFFS_ALLOCATION_NLINKS		100

#define YAFFS_NOBJECT_BUCKETS		256

#define YAFFS_OBJECT_SPACE		0x40000
#define YAFFS_MAX_OBJECT_ID		(YAFFS_OBJECT_SPACE -1)

#define YAFFS_CHECKPOINT_VERSION 	4

#ifdef CONFIG_YAFFS_UNICODE
#define YAFFS_MAX_NAME_LENGTH		127
#define YAFFS_MAX_ALIAS_LENGTH		79
#else
#define YAFFS_MAX_NAME_LENGTH		255
#define YAFFS_MAX_ALIAS_LENGTH		159
#endif

#define YAFFS_SHORT_NAME_LENGTH		15

#define YAFFS_OBJECTID_ROOT		1
#define YAFFS_OBJECTID_LOSTNFOUND	2
#define YAFFS_OBJECTID_UNLINKED		3
#define YAFFS_OBJECTID_DELETED		4

#define YAFFS_OBJECTID_SB_HEADER	0x10
#define YAFFS_OBJECTID_CHECKPOINT_DATA	0x20
#define YAFFS_SEQUENCE_CHECKPOINT_DATA  0x21

#define YAFFS_MAX_SHORT_OP_CACHES	20

#define YAFFS_N_TEMP_BUFFERS		6

#define YAFFS_WR_ATTEMPTS		(5*64)

#define YAFFS_LOWEST_SEQUENCE_NUMBER	0x00001000
#define YAFFS_HIGHEST_SEQUENCE_NUMBER	0xEFFFFF00

#define YAFFS_SEQUENCE_BAD_BLOCK	0xFFFF0000

struct yaffs_cache {
	struct yaffs_obj *object;
	int chunk_id;
	int last_use;
	int dirty;
	int n_bytes;		
	int locked;		
	u8 *data;
};


#ifndef CONFIG_YAFFS_NO_YAFFS1
struct yaffs_tags {
	unsigned chunk_id:20;
	unsigned serial_number:2;
	unsigned n_bytes_lsb:10;
	unsigned obj_id:18;
	unsigned ecc:12;
	unsigned n_bytes_msb:2;
};

union yaffs_tags_union {
	struct yaffs_tags as_tags;
	u8 as_bytes[8];
};

#endif


enum yaffs_ecc_result {
	YAFFS_ECC_RESULT_UNKNOWN,
	YAFFS_ECC_RESULT_NO_ERROR,
	YAFFS_ECC_RESULT_FIXED,
	YAFFS_ECC_RESULT_UNFIXED
};

enum yaffs_obj_type {
	YAFFS_OBJECT_TYPE_UNKNOWN,
	YAFFS_OBJECT_TYPE_FILE,
	YAFFS_OBJECT_TYPE_SYMLINK,
	YAFFS_OBJECT_TYPE_DIRECTORY,
	YAFFS_OBJECT_TYPE_HARDLINK,
	YAFFS_OBJECT_TYPE_SPECIAL
};

#define YAFFS_OBJECT_TYPE_MAX YAFFS_OBJECT_TYPE_SPECIAL

struct yaffs_ext_tags {

	unsigned validity0;
	unsigned chunk_used;	
	unsigned obj_id;	
	unsigned chunk_id;	
	unsigned n_bytes;	

	
	enum yaffs_ecc_result ecc_result;
	unsigned block_bad;

	
	unsigned is_deleted;	
	unsigned serial_number;	

	
	unsigned seq_number;	

	

	unsigned extra_available;	
	unsigned extra_parent_id;	
	unsigned extra_is_shrink;	
	unsigned extra_shadows;	

	enum yaffs_obj_type extra_obj_type;	

	unsigned extra_length;	
	unsigned extra_equiv_id;	

	unsigned validity1;

};

struct yaffs_spare {
	u8 tb0;
	u8 tb1;
	u8 tb2;
	u8 tb3;
	u8 page_status;		
	u8 block_status;
	u8 tb4;
	u8 tb5;
	u8 ecc1[3];
	u8 tb6;
	u8 tb7;
	u8 ecc2[3];
};

struct yaffs_nand_spare {
	struct yaffs_spare spare;
	int eccres1;
	int eccres2;
};


enum yaffs_block_state {
	YAFFS_BLOCK_STATE_UNKNOWN = 0,

	YAFFS_BLOCK_STATE_SCANNING,
	

	YAFFS_BLOCK_STATE_NEEDS_SCANNING,

	YAFFS_BLOCK_STATE_EMPTY,
	

	YAFFS_BLOCK_STATE_ALLOCATING,

	YAFFS_BLOCK_STATE_FULL,

	YAFFS_BLOCK_STATE_DIRTY,

	YAFFS_BLOCK_STATE_CHECKPOINT,
	

	YAFFS_BLOCK_STATE_COLLECTING,
	

	YAFFS_BLOCK_STATE_DEAD
	    
};

#define	YAFFS_NUMBER_OF_BLOCK_STATES (YAFFS_BLOCK_STATE_DEAD + 1)

struct yaffs_block_info {

	int soft_del_pages:10;	
	int pages_in_use:10;	
	unsigned block_state:4;	
	u32 needs_retiring:1;	
	
	u32 skip_erased_check:1;	
	u32 gc_prioritise:1;	
	u32 chunk_error_strikes:3;	

#ifdef CONFIG_YAFFS_YAFFS2
	u32 has_shrink_hdr:1;	
	u32 seq_number;		
#endif

};


struct yaffs_obj_hdr {
	enum yaffs_obj_type type;

	
	int parent_obj_id;
	u16 sum_no_longer_used;	
	YCHAR name[YAFFS_MAX_NAME_LENGTH + 1];

	
	u32 yst_mode;		

	u32 yst_uid;
	u32 yst_gid;
	u32 yst_atime;
	u32 yst_mtime;
	u32 yst_ctime;

	
	int file_size;

	
	int equiv_id;

	
	YCHAR alias[YAFFS_MAX_ALIAS_LENGTH + 1];

	u32 yst_rdev;		

	u32 win_ctime[2];
	u32 win_atime[2];
	u32 win_mtime[2];

	u32 inband_shadowed_obj_id;
	u32 inband_is_shrink;

	u32 reserved[2];
	int shadows_obj;	

	/* is_shrink applies to object headers written when we shrink the file (ie resize) */
	u32 is_shrink;

};


struct yaffs_tnode {
	struct yaffs_tnode *internal[YAFFS_NTNODES_INTERNAL];
};


struct yaffs_file_var {
	u32 file_size;
	u32 scanned_size;
	u32 shrink_size;
	int top_level;
	struct yaffs_tnode *top;
};

struct yaffs_dir_var {
	struct list_head children;	
	struct list_head dirty;	
};

struct yaffs_symlink_var {
	YCHAR *alias;
};

struct yaffs_hardlink_var {
	struct yaffs_obj *equiv_obj;
	u32 equiv_id;
};

union yaffs_obj_var {
	struct yaffs_file_var file_variant;
	struct yaffs_dir_var dir_variant;
	struct yaffs_symlink_var symlink_variant;
	struct yaffs_hardlink_var hardlink_variant;
};

struct yaffs_obj {
	u8 deleted:1;		
	u8 soft_del:1;		
	u8 unlinked:1;		
	u8 fake:1;		
	u8 rename_allowed:1;	
	u8 unlink_allowed:1;
	u8 dirty:1;		/* the object needs to be written to flash */
	u8 valid:1;		
	u8 lazy_loaded:1;	

	u8 defered_free:1;	
	u8 being_created:1;	
	u8 is_shadowed:1;	

	u8 xattr_known:1;	
	u8 has_xattr:1;		

	u8 serial;		
	u16 sum;		

	struct yaffs_dev *my_dev;	

	struct list_head hash_link;	

	struct list_head hard_links;	

	
	
	struct yaffs_obj *parent;
	struct list_head siblings;

	
	int hdr_chunk;

	int n_data_chunks;	

	u32 obj_id;		

	u32 yst_mode;

#ifndef CONFIG_YAFFS_NO_SHORT_NAMES
	YCHAR short_name[YAFFS_SHORT_NAME_LENGTH + 1];
#endif

#ifdef CONFIG_YAFFS_WINCE
	u32 win_ctime[2];
	u32 win_mtime[2];
	u32 win_atime[2];
#else
	u32 yst_uid;
	u32 yst_gid;
	u32 yst_atime;
	u32 yst_mtime;
	u32 yst_ctime;
#endif

	u32 yst_rdev;

	void *my_inode;

	enum yaffs_obj_type variant_type;

	union yaffs_obj_var variant;

};

struct yaffs_obj_bucket {
	struct list_head list;
	int count;
};


struct yaffs_checkpt_obj {
	int struct_type;
	u32 obj_id;
	u32 parent_id;
	int hdr_chunk;
	enum yaffs_obj_type variant_type:3;
	u8 deleted:1;
	u8 soft_del:1;
	u8 unlinked:1;
	u8 fake:1;
	u8 rename_allowed:1;
	u8 unlink_allowed:1;
	u8 serial;
	int n_data_chunks;
	u32 size_or_equiv_obj;
};


struct yaffs_buffer {
	u8 *buffer;
	int line;		
	int max_line;
};


struct yaffs_param {
	const YCHAR *name;


	int inband_tags;	
	u32 total_bytes_per_chunk;	
	int chunks_per_block;	
	int spare_bytes_per_chunk;	
	int start_block;	
	int end_block;		
	int n_reserved_blocks;	
	

	int n_caches;		
	int use_nand_ecc;	
	int no_tags_ecc;	

	int is_yaffs2;		

	int empty_lost_n_found;	

	int refresh_period;	

	
	u8 skip_checkpt_rd;
	u8 skip_checkpt_wr;

	int enable_xattr;	

	

	int (*write_chunk_fn) (struct yaffs_dev * dev,
			       int nand_chunk, const u8 * data,
			       const struct yaffs_spare * spare);
	int (*read_chunk_fn) (struct yaffs_dev * dev,
			      int nand_chunk, u8 * data,
			      struct yaffs_spare * spare);
	int (*erase_fn) (struct yaffs_dev * dev, int flash_block);
	int (*initialise_flash_fn) (struct yaffs_dev * dev);
	int (*deinitialise_flash_fn) (struct yaffs_dev * dev);

#ifdef CONFIG_YAFFS_YAFFS2
	int (*write_chunk_tags_fn) (struct yaffs_dev * dev,
				    int nand_chunk, const u8 * data,
				    const struct yaffs_ext_tags * tags);
	int (*read_chunk_tags_fn) (struct yaffs_dev * dev,
				   int nand_chunk, u8 * data,
				   struct yaffs_ext_tags * tags);
	int (*bad_block_fn) (struct yaffs_dev * dev, int block_no);
	int (*query_block_fn) (struct yaffs_dev * dev, int block_no,
			       enum yaffs_block_state * state,
			       u32 * seq_number);
#endif

	void (*remove_obj_fn) (struct yaffs_obj * obj);

	
	void (*sb_dirty_fn) (struct yaffs_dev * dev);

	
	unsigned (*gc_control) (struct yaffs_dev * dev);

	
	int use_header_file_size;	
	int disable_lazy_load;	
	int wide_tnodes_disabled;	
	int disable_soft_del;	

	int defered_dir_update;	

#ifdef CONFIG_YAFFS_AUTO_UNICODE
	int auto_unicode;
#endif
	int always_check_erased;	
};

struct yaffs_dev {
	struct yaffs_param param;

	

	void *os_context;
	void *driver_context;

	struct list_head dev_list;

	
	int data_bytes_per_chunk;

	
	u16 chunk_grp_bits;	
	u16 chunk_grp_size;	

	
	u32 tnode_width;
	u32 tnode_mask;
	u32 tnode_size;

	
	u32 chunk_shift;	
	u32 chunk_div;		
	u32 chunk_mask;		

	int is_mounted;
	int read_only;
	int is_checkpointed;

	
	int internal_start_block;
	int internal_end_block;
	int block_offset;
	int chunk_offset;

	
	int checkpt_page_seq;	
	int checkpt_byte_count;
	int checkpt_byte_offs;
	u8 *checkpt_buffer;
	int checkpt_open_write;
	int blocks_in_checkpt;
	int checkpt_cur_chunk;
	int checkpt_cur_block;
	int checkpt_next_block;
	int *checkpt_block_list;
	int checkpt_max_blocks;
	u32 checkpt_sum;
	u32 checkpt_xor;

	int checkpoint_blocks_required;	

	
	struct yaffs_block_info *block_info;
	u8 *chunk_bits;		
	unsigned block_info_alt:1;	
	unsigned chunk_bits_alt:1;	
	int chunk_bit_stride;	

	int n_erased_blocks;
	int alloc_block;	
	u32 alloc_page;
	int alloc_block_finder;	

	
	void *allocator;
	int n_obj;
	int n_tnodes;

	int n_hardlinks;

	struct yaffs_obj_bucket obj_bucket[YAFFS_NOBJECT_BUCKETS];
	u32 bucket_finder;

	int n_free_chunks;

	
	u32 *gc_cleanup_list;	
	u32 n_clean_ups;

	unsigned has_pending_prioritised_gc;	
	unsigned gc_disable;
	unsigned gc_block_finder;
	unsigned gc_dirtiest;
	unsigned gc_pages_in_use;
	unsigned gc_not_done;
	unsigned gc_block;
	unsigned gc_chunk;
	unsigned gc_skip;

	
	struct yaffs_obj *root_dir;
	struct yaffs_obj *lost_n_found;


	int buffered_block;	
	int doing_buffered_block_rewrite;

	struct yaffs_cache *cache;
	int cache_last_use;

	
	struct yaffs_obj *unlinked_dir;	
	struct yaffs_obj *del_dir;	
	struct yaffs_obj *unlinked_deletion;	
	int n_deleted_files;	
	int n_unlinked_files;	
	int n_bg_deletions;	

	
	struct yaffs_buffer temp_buffer[YAFFS_N_TEMP_BUFFERS];
	int max_temp;
	int temp_in_use;
	int unmanaged_buffer_allocs;
	int unmanaged_buffer_deallocs;

	
	unsigned seq_number;	
	unsigned oldest_dirty_seq;
	unsigned oldest_dirty_block;

	
	int refresh_skip;	

	
	struct list_head dirty_dirs;	

	
	u32 n_page_writes;
	u32 n_page_reads;
	u32 n_erasures;
	u32 n_erase_failures;
	u32 n_gc_copies;
	u32 all_gcs;
	u32 passive_gc_count;
	u32 oldest_dirty_gc_count;
	u32 n_gc_blocks;
	u32 bg_gcs;
	u32 n_retired_writes;
	u32 n_retired_blocks;
	u32 n_ecc_fixed;
	u32 n_ecc_unfixed;
	u32 n_tags_ecc_fixed;
	u32 n_tags_ecc_unfixed;
	u32 n_deletions;
	u32 n_unmarked_deletions;
	u32 refresh_count;
	u32 cache_hits;

};

struct yaffs_checkpt_dev {
	int struct_type;
	int n_erased_blocks;
	int alloc_block;	
	u32 alloc_page;
	int n_free_chunks;

	int n_deleted_files;	
	int n_unlinked_files;	
	int n_bg_deletions;	

	
	unsigned seq_number;	

};

struct yaffs_checkpt_validity {
	int struct_type;
	u32 magic;
	u32 version;
	u32 head;
};

struct yaffs_shadow_fixer {
	int obj_id;
	int shadowed_id;
	struct yaffs_shadow_fixer *next;
};

struct yaffs_xattr_mod {
	int set;		
	const YCHAR *name;
	const void *data;
	int size;
	int flags;
	int result;
};


int yaffs_guts_initialise(struct yaffs_dev *dev);
void yaffs_deinitialise(struct yaffs_dev *dev);

int yaffs_get_n_free_chunks(struct yaffs_dev *dev);

int yaffs_rename_obj(struct yaffs_obj *old_dir, const YCHAR * old_name,
		     struct yaffs_obj *new_dir, const YCHAR * new_name);

int yaffs_unlinker(struct yaffs_obj *dir, const YCHAR * name);
int yaffs_del_obj(struct yaffs_obj *obj);

int yaffs_get_obj_name(struct yaffs_obj *obj, YCHAR * name, int buffer_size);
int yaffs_get_obj_length(struct yaffs_obj *obj);
int yaffs_get_obj_inode(struct yaffs_obj *obj);
unsigned yaffs_get_obj_type(struct yaffs_obj *obj);
int yaffs_get_obj_link_count(struct yaffs_obj *obj);

int yaffs_file_rd(struct yaffs_obj *obj, u8 * buffer, loff_t offset,
		  int n_bytes);
int yaffs_wr_file(struct yaffs_obj *obj, const u8 * buffer, loff_t offset,
		  int n_bytes, int write_trhrough);
int yaffs_resize_file(struct yaffs_obj *obj, loff_t new_size);

struct yaffs_obj *yaffs_create_file(struct yaffs_obj *parent,
				    const YCHAR * name, u32 mode, u32 uid,
				    u32 gid);

int yaffs_flush_file(struct yaffs_obj *obj, int update_time, int data_sync);

void yaffs_flush_whole_cache(struct yaffs_dev *dev);

int yaffs_checkpoint_save(struct yaffs_dev *dev);
int yaffs_checkpoint_restore(struct yaffs_dev *dev);

struct yaffs_obj *yaffs_create_dir(struct yaffs_obj *parent, const YCHAR * name,
				   u32 mode, u32 uid, u32 gid);
struct yaffs_obj *yaffs_find_by_name(struct yaffs_obj *the_dir,
				     const YCHAR * name);
struct yaffs_obj *yaffs_find_by_number(struct yaffs_dev *dev, u32 number);

struct yaffs_obj *yaffs_link_obj(struct yaffs_obj *parent, const YCHAR * name,
				 struct yaffs_obj *equiv_obj);

struct yaffs_obj *yaffs_get_equivalent_obj(struct yaffs_obj *obj);

struct yaffs_obj *yaffs_create_symlink(struct yaffs_obj *parent,
				       const YCHAR * name, u32 mode, u32 uid,
				       u32 gid, const YCHAR * alias);
YCHAR *yaffs_get_symlink_alias(struct yaffs_obj *obj);

struct yaffs_obj *yaffs_create_special(struct yaffs_obj *parent,
				       const YCHAR * name, u32 mode, u32 uid,
				       u32 gid, u32 rdev);

int yaffs_set_xattrib(struct yaffs_obj *obj, const YCHAR * name,
		      const void *value, int size, int flags);
int yaffs_get_xattrib(struct yaffs_obj *obj, const YCHAR * name, void *value,
		      int size);
int yaffs_list_xattrib(struct yaffs_obj *obj, char *buffer, int size);
int yaffs_remove_xattrib(struct yaffs_obj *obj, const YCHAR * name);

struct yaffs_obj *yaffs_root(struct yaffs_dev *dev);
struct yaffs_obj *yaffs_lost_n_found(struct yaffs_dev *dev);

void yaffs_handle_defered_free(struct yaffs_obj *obj);

void yaffs_update_dirty_dirs(struct yaffs_dev *dev);

int yaffs_bg_gc(struct yaffs_dev *dev, unsigned urgency);

int yaffs_dump_obj(struct yaffs_obj *obj);

void yaffs_guts_test(struct yaffs_dev *dev);

void yaffs_chunk_del(struct yaffs_dev *dev, int chunk_id, int mark_flash,
		     int lyn);
int yaffs_check_ff(u8 * buffer, int n_bytes);
void yaffs_handle_chunk_error(struct yaffs_dev *dev,
			      struct yaffs_block_info *bi);

u8 *yaffs_get_temp_buffer(struct yaffs_dev *dev, int line_no);
void yaffs_release_temp_buffer(struct yaffs_dev *dev, u8 * buffer, int line_no);

struct yaffs_obj *yaffs_find_or_create_by_number(struct yaffs_dev *dev,
						 int number,
						 enum yaffs_obj_type type);
int yaffs_put_chunk_in_file(struct yaffs_obj *in, int inode_chunk,
			    int nand_chunk, int in_scan);
void yaffs_set_obj_name(struct yaffs_obj *obj, const YCHAR * name);
void yaffs_set_obj_name_from_oh(struct yaffs_obj *obj,
				const struct yaffs_obj_hdr *oh);
void yaffs_add_obj_to_dir(struct yaffs_obj *directory, struct yaffs_obj *obj);
YCHAR *yaffs_clone_str(const YCHAR * str);
void yaffs_link_fixup(struct yaffs_dev *dev, struct yaffs_obj *hard_list);
void yaffs_block_became_dirty(struct yaffs_dev *dev, int block_no);
int yaffs_update_oh(struct yaffs_obj *in, const YCHAR * name,
		    int force, int is_shrink, int shadows,
		    struct yaffs_xattr_mod *xop);
void yaffs_handle_shadowed_obj(struct yaffs_dev *dev, int obj_id,
			       int backward_scanning);
int yaffs_check_alloc_available(struct yaffs_dev *dev, int n_chunks);
struct yaffs_tnode *yaffs_get_tnode(struct yaffs_dev *dev);
struct yaffs_tnode *yaffs_add_find_tnode_0(struct yaffs_dev *dev,
					   struct yaffs_file_var *file_struct,
					   u32 chunk_id,
					   struct yaffs_tnode *passed_tn);

int yaffs_do_file_wr(struct yaffs_obj *in, const u8 * buffer, loff_t offset,
		     int n_bytes, int write_trhrough);
void yaffs_resize_file_down(struct yaffs_obj *obj, loff_t new_size);
void yaffs_skip_rest_of_block(struct yaffs_dev *dev);

int yaffs_count_free_chunks(struct yaffs_dev *dev);

struct yaffs_tnode *yaffs_find_tnode_0(struct yaffs_dev *dev,
				       struct yaffs_file_var *file_struct,
				       u32 chunk_id);

u32 yaffs_get_group_base(struct yaffs_dev *dev, struct yaffs_tnode *tn,
			 unsigned pos);

int yaffs_is_non_empty_dir(struct yaffs_obj *obj);
#endif
