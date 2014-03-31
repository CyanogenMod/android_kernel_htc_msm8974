/*
 * Copyright (C) 2008 Oracle.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License v2 as published by the Free Software Foundation.
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
#ifndef __DELAYED_REF__
#define __DELAYED_REF__

#define BTRFS_ADD_DELAYED_REF    1 
#define BTRFS_DROP_DELAYED_REF   2 
#define BTRFS_ADD_DELAYED_EXTENT 3 
#define BTRFS_UPDATE_DELAYED_HEAD 4 

struct btrfs_delayed_ref_node {
	struct rb_node rb_node;

	
	u64 bytenr;

	
	u64 num_bytes;

	
	u64 seq;

	
	atomic_t refs;

	int ref_mod;

	unsigned int action:8;
	unsigned int type:8;
	
	unsigned int is_head:1;
	unsigned int in_tree:1;
};

struct btrfs_delayed_extent_op {
	struct btrfs_disk_key key;
	u64 flags_to_set;
	unsigned int update_key:1;
	unsigned int update_flags:1;
	unsigned int is_data:1;
};

struct btrfs_delayed_ref_head {
	struct btrfs_delayed_ref_node node;

	struct mutex mutex;

	struct list_head cluster;

	struct btrfs_delayed_extent_op *extent_op;
	unsigned int must_insert_reserved:1;
	unsigned int is_data:1;
};

struct btrfs_delayed_tree_ref {
	struct btrfs_delayed_ref_node node;
	u64 root;
	u64 parent;
	int level;
};

struct btrfs_delayed_data_ref {
	struct btrfs_delayed_ref_node node;
	u64 root;
	u64 parent;
	u64 objectid;
	u64 offset;
};

struct btrfs_delayed_ref_root {
	struct rb_root root;

	
	spinlock_t lock;

	unsigned long num_entries;

	
	unsigned long num_heads;

	
	unsigned long num_heads_ready;

	int flushing;

	u64 run_delayed_start;

	u64 seq;

	struct list_head seq_head;

	wait_queue_head_t seq_wait;
};

static inline void btrfs_put_delayed_ref(struct btrfs_delayed_ref_node *ref)
{
	WARN_ON(atomic_read(&ref->refs) == 0);
	if (atomic_dec_and_test(&ref->refs)) {
		WARN_ON(ref->in_tree);
		kfree(ref);
	}
}

int btrfs_add_delayed_tree_ref(struct btrfs_fs_info *fs_info,
			       struct btrfs_trans_handle *trans,
			       u64 bytenr, u64 num_bytes, u64 parent,
			       u64 ref_root, int level, int action,
			       struct btrfs_delayed_extent_op *extent_op,
			       int for_cow);
int btrfs_add_delayed_data_ref(struct btrfs_fs_info *fs_info,
			       struct btrfs_trans_handle *trans,
			       u64 bytenr, u64 num_bytes,
			       u64 parent, u64 ref_root,
			       u64 owner, u64 offset, int action,
			       struct btrfs_delayed_extent_op *extent_op,
			       int for_cow);
int btrfs_add_delayed_extent_op(struct btrfs_fs_info *fs_info,
				struct btrfs_trans_handle *trans,
				u64 bytenr, u64 num_bytes,
				struct btrfs_delayed_extent_op *extent_op);

struct btrfs_delayed_ref_head *
btrfs_find_delayed_ref_head(struct btrfs_trans_handle *trans, u64 bytenr);
int btrfs_delayed_ref_lock(struct btrfs_trans_handle *trans,
			   struct btrfs_delayed_ref_head *head);
int btrfs_find_ref_cluster(struct btrfs_trans_handle *trans,
			   struct list_head *cluster, u64 search_start);

struct seq_list {
	struct list_head list;
	u64 seq;
};

static inline u64 inc_delayed_seq(struct btrfs_delayed_ref_root *delayed_refs)
{
	assert_spin_locked(&delayed_refs->lock);
	++delayed_refs->seq;
	return delayed_refs->seq;
}

static inline void
btrfs_get_delayed_seq(struct btrfs_delayed_ref_root *delayed_refs,
		      struct seq_list *elem)
{
	assert_spin_locked(&delayed_refs->lock);
	elem->seq = delayed_refs->seq;
	list_add_tail(&elem->list, &delayed_refs->seq_head);
}

static inline void
btrfs_put_delayed_seq(struct btrfs_delayed_ref_root *delayed_refs,
		      struct seq_list *elem)
{
	spin_lock(&delayed_refs->lock);
	list_del(&elem->list);
	wake_up(&delayed_refs->seq_wait);
	spin_unlock(&delayed_refs->lock);
}

int btrfs_check_delayed_seq(struct btrfs_delayed_ref_root *delayed_refs,
			    u64 seq);

static inline int need_ref_seq(int for_cow, u64 rootid)
{
	if (for_cow)
		return 0;

	if (rootid == BTRFS_FS_TREE_OBJECTID)
		return 1;

	if ((s64)rootid >= (s64)BTRFS_FIRST_FREE_OBJECTID)
		return 1;

	return 0;
}

static int btrfs_delayed_ref_is_head(struct btrfs_delayed_ref_node *node)
{
	return node->is_head;
}

static inline struct btrfs_delayed_tree_ref *
btrfs_delayed_node_to_tree_ref(struct btrfs_delayed_ref_node *node)
{
	WARN_ON(btrfs_delayed_ref_is_head(node));
	return container_of(node, struct btrfs_delayed_tree_ref, node);
}

static inline struct btrfs_delayed_data_ref *
btrfs_delayed_node_to_data_ref(struct btrfs_delayed_ref_node *node)
{
	WARN_ON(btrfs_delayed_ref_is_head(node));
	return container_of(node, struct btrfs_delayed_data_ref, node);
}

static inline struct btrfs_delayed_ref_head *
btrfs_delayed_node_to_head(struct btrfs_delayed_ref_node *node)
{
	WARN_ON(!btrfs_delayed_ref_is_head(node));
	return container_of(node, struct btrfs_delayed_ref_head, node);
}
#endif
