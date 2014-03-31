/* -*- mode: c; c-basic-offset: 8; -*-
 * vim: noexpandtab sw=8 ts=8 sts=0:
 *
 * alloc.h
 *
 * Function prototypes
 *
 * Copyright (C) 2002, 2004 Oracle.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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

#ifndef OCFS2_ALLOC_H
#define OCFS2_ALLOC_H


#define OCFS2_MAX_XATTR_TREE_LEAF_SIZE 65536

struct ocfs2_extent_tree_operations;
struct ocfs2_extent_tree {
	struct ocfs2_extent_tree_operations	*et_ops;
	struct buffer_head			*et_root_bh;
	struct ocfs2_extent_list		*et_root_el;
	struct ocfs2_caching_info		*et_ci;
	ocfs2_journal_access_func		et_root_journal_access;
	void					*et_object;
	unsigned int				et_max_leaf_clusters;
};

void ocfs2_init_dinode_extent_tree(struct ocfs2_extent_tree *et,
				   struct ocfs2_caching_info *ci,
				   struct buffer_head *bh);
void ocfs2_init_xattr_tree_extent_tree(struct ocfs2_extent_tree *et,
				       struct ocfs2_caching_info *ci,
				       struct buffer_head *bh);
struct ocfs2_xattr_value_buf;
void ocfs2_init_xattr_value_extent_tree(struct ocfs2_extent_tree *et,
					struct ocfs2_caching_info *ci,
					struct ocfs2_xattr_value_buf *vb);
void ocfs2_init_dx_root_extent_tree(struct ocfs2_extent_tree *et,
				    struct ocfs2_caching_info *ci,
				    struct buffer_head *bh);
void ocfs2_init_refcount_extent_tree(struct ocfs2_extent_tree *et,
				     struct ocfs2_caching_info *ci,
				     struct buffer_head *bh);

int ocfs2_read_extent_block(struct ocfs2_caching_info *ci, u64 eb_blkno,
			    struct buffer_head **bh);

struct ocfs2_alloc_context;
int ocfs2_insert_extent(handle_t *handle,
			struct ocfs2_extent_tree *et,
			u32 cpos,
			u64 start_blk,
			u32 new_clusters,
			u8 flags,
			struct ocfs2_alloc_context *meta_ac);

enum ocfs2_alloc_restarted {
	RESTART_NONE = 0,
	RESTART_TRANS,
	RESTART_META
};
int ocfs2_add_clusters_in_btree(handle_t *handle,
				struct ocfs2_extent_tree *et,
				u32 *logical_offset,
				u32 clusters_to_add,
				int mark_unwritten,
				struct ocfs2_alloc_context *data_ac,
				struct ocfs2_alloc_context *meta_ac,
				enum ocfs2_alloc_restarted *reason_ret);
struct ocfs2_cached_dealloc_ctxt;
struct ocfs2_path;
int ocfs2_split_extent(handle_t *handle,
		       struct ocfs2_extent_tree *et,
		       struct ocfs2_path *path,
		       int split_index,
		       struct ocfs2_extent_rec *split_rec,
		       struct ocfs2_alloc_context *meta_ac,
		       struct ocfs2_cached_dealloc_ctxt *dealloc);
int ocfs2_mark_extent_written(struct inode *inode,
			      struct ocfs2_extent_tree *et,
			      handle_t *handle, u32 cpos, u32 len, u32 phys,
			      struct ocfs2_alloc_context *meta_ac,
			      struct ocfs2_cached_dealloc_ctxt *dealloc);
int ocfs2_change_extent_flag(handle_t *handle,
			     struct ocfs2_extent_tree *et,
			     u32 cpos, u32 len, u32 phys,
			     struct ocfs2_alloc_context *meta_ac,
			     struct ocfs2_cached_dealloc_ctxt *dealloc,
			     int new_flags, int clear_flags);
int ocfs2_remove_extent(handle_t *handle, struct ocfs2_extent_tree *et,
			u32 cpos, u32 len,
			struct ocfs2_alloc_context *meta_ac,
			struct ocfs2_cached_dealloc_ctxt *dealloc);
int ocfs2_remove_btree_range(struct inode *inode,
			     struct ocfs2_extent_tree *et,
			     u32 cpos, u32 phys_cpos, u32 len, int flags,
			     struct ocfs2_cached_dealloc_ctxt *dealloc,
			     u64 refcount_loc);

int ocfs2_num_free_extents(struct ocfs2_super *osb,
			   struct ocfs2_extent_tree *et);

static inline int ocfs2_extend_meta_needed(struct ocfs2_extent_list *root_el)
{
	return le16_to_cpu(root_el->l_tree_depth) + 2;
}

void ocfs2_dinode_new_extent_list(struct inode *inode, struct ocfs2_dinode *di);
void ocfs2_set_inode_data_inline(struct inode *inode, struct ocfs2_dinode *di);
int ocfs2_convert_inline_data_to_extents(struct inode *inode,
					 struct buffer_head *di_bh);

int ocfs2_truncate_log_init(struct ocfs2_super *osb);
void ocfs2_truncate_log_shutdown(struct ocfs2_super *osb);
void ocfs2_schedule_truncate_log_flush(struct ocfs2_super *osb,
				       int cancel);
int ocfs2_flush_truncate_log(struct ocfs2_super *osb);
int ocfs2_begin_truncate_log_recovery(struct ocfs2_super *osb,
				      int slot_num,
				      struct ocfs2_dinode **tl_copy);
int ocfs2_complete_truncate_log_recovery(struct ocfs2_super *osb,
					 struct ocfs2_dinode *tl_copy);
int ocfs2_truncate_log_needs_flush(struct ocfs2_super *osb);
int ocfs2_truncate_log_append(struct ocfs2_super *osb,
			      handle_t *handle,
			      u64 start_blk,
			      unsigned int num_clusters);
int __ocfs2_flush_truncate_log(struct ocfs2_super *osb);

struct ocfs2_cached_dealloc_ctxt {
	struct ocfs2_per_slot_free_list		*c_first_suballocator;
	struct ocfs2_cached_block_free 		*c_global_allocator;
};
static inline void ocfs2_init_dealloc_ctxt(struct ocfs2_cached_dealloc_ctxt *c)
{
	c->c_first_suballocator = NULL;
	c->c_global_allocator = NULL;
}
int ocfs2_cache_cluster_dealloc(struct ocfs2_cached_dealloc_ctxt *ctxt,
				u64 blkno, unsigned int bit);
int ocfs2_cache_block_dealloc(struct ocfs2_cached_dealloc_ctxt *ctxt,
			      int type, int slot, u64 suballoc, u64 blkno,
			      unsigned int bit);
static inline int ocfs2_dealloc_has_cluster(struct ocfs2_cached_dealloc_ctxt *c)
{
	return c->c_global_allocator != NULL;
}
int ocfs2_run_deallocs(struct ocfs2_super *osb,
		       struct ocfs2_cached_dealloc_ctxt *ctxt);

struct ocfs2_truncate_context {
	struct ocfs2_cached_dealloc_ctxt tc_dealloc;
	int tc_ext_alloc_locked; 
	
	struct buffer_head *tc_last_eb_bh;
};

int ocfs2_zero_range_for_truncate(struct inode *inode, handle_t *handle,
				  u64 range_start, u64 range_end);
int ocfs2_commit_truncate(struct ocfs2_super *osb,
			  struct inode *inode,
			  struct buffer_head *di_bh);
int ocfs2_truncate_inline(struct inode *inode, struct buffer_head *di_bh,
			  unsigned int start, unsigned int end, int trunc);

int ocfs2_find_leaf(struct ocfs2_caching_info *ci,
		    struct ocfs2_extent_list *root_el, u32 cpos,
		    struct buffer_head **leaf_bh);
int ocfs2_search_extent_list(struct ocfs2_extent_list *el, u32 v_cluster);

int ocfs2_trim_fs(struct super_block *sb, struct fstrim_range *range);
static inline unsigned int ocfs2_rec_clusters(struct ocfs2_extent_list *el,
					      struct ocfs2_extent_rec *rec)
{
	/*
	 * Cluster count in extent records is slightly different
	 * between interior nodes and leaf nodes. This is to support
	 * unwritten extents which need a flags field in leaf node
	 * records, thus shrinking the available space for a clusters
	 * field.
	 */
	if (el->l_tree_depth)
		return le32_to_cpu(rec->e_int_clusters);
	else
		return le16_to_cpu(rec->e_leaf_clusters);
}

static inline int ocfs2_is_empty_extent(struct ocfs2_extent_rec *rec)
{
	return !rec->e_leaf_clusters;
}

int ocfs2_grab_pages(struct inode *inode, loff_t start, loff_t end,
		     struct page **pages, int *num);
void ocfs2_map_and_dirty_page(struct inode *inode, handle_t *handle,
			      unsigned int from, unsigned int to,
			      struct page *page, int zero, u64 *phys);
struct ocfs2_path_item {
	struct buffer_head		*bh;
	struct ocfs2_extent_list	*el;
};

#define OCFS2_MAX_PATH_DEPTH	5

struct ocfs2_path {
	int				p_tree_depth;
	ocfs2_journal_access_func	p_root_access;
	struct ocfs2_path_item		p_node[OCFS2_MAX_PATH_DEPTH];
};

#define path_root_bh(_path) ((_path)->p_node[0].bh)
#define path_root_el(_path) ((_path)->p_node[0].el)
#define path_root_access(_path)((_path)->p_root_access)
#define path_leaf_bh(_path) ((_path)->p_node[(_path)->p_tree_depth].bh)
#define path_leaf_el(_path) ((_path)->p_node[(_path)->p_tree_depth].el)
#define path_num_items(_path) ((_path)->p_tree_depth + 1)

void ocfs2_reinit_path(struct ocfs2_path *path, int keep_root);
void ocfs2_free_path(struct ocfs2_path *path);
int ocfs2_find_path(struct ocfs2_caching_info *ci,
		    struct ocfs2_path *path,
		    u32 cpos);
struct ocfs2_path *ocfs2_new_path_from_path(struct ocfs2_path *path);
struct ocfs2_path *ocfs2_new_path_from_et(struct ocfs2_extent_tree *et);
int ocfs2_path_bh_journal_access(handle_t *handle,
				 struct ocfs2_caching_info *ci,
				 struct ocfs2_path *path,
				 int idx);
int ocfs2_journal_access_path(struct ocfs2_caching_info *ci,
			      handle_t *handle,
			      struct ocfs2_path *path);
int ocfs2_find_cpos_for_right_leaf(struct super_block *sb,
				   struct ocfs2_path *path, u32 *cpos);
int ocfs2_find_cpos_for_left_leaf(struct super_block *sb,
				  struct ocfs2_path *path, u32 *cpos);
int ocfs2_find_subtree_root(struct ocfs2_extent_tree *et,
			    struct ocfs2_path *left,
			    struct ocfs2_path *right);
#endif 
