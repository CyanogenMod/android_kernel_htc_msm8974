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

#include <linux/sched.h>
#include <linux/slab.h>
#include "ctree.h"
#include "transaction.h"
#include "disk-io.h"
#include "locking.h"
#include "print-tree.h"
#include "compat.h"
#include "tree-log.h"

#define LOG_INODE_ALL 0
#define LOG_INODE_EXISTS 1


#define LOG_WALK_PIN_ONLY 0
#define LOG_WALK_REPLAY_INODES 1
#define LOG_WALK_REPLAY_ALL 2

static int btrfs_log_inode(struct btrfs_trans_handle *trans,
			     struct btrfs_root *root, struct inode *inode,
			     int inode_only);
static int link_to_fixup_dir(struct btrfs_trans_handle *trans,
			     struct btrfs_root *root,
			     struct btrfs_path *path, u64 objectid);
static noinline int replay_dir_deletes(struct btrfs_trans_handle *trans,
				       struct btrfs_root *root,
				       struct btrfs_root *log,
				       struct btrfs_path *path,
				       u64 dirid, int del_all);

/*
 * tree logging is a special write ahead log used to make sure that
 * fsyncs and O_SYNCs can happen without doing full tree commits.
 *
 * Full tree commits are expensive because they require commonly
 * modified blocks to be recowed, creating many dirty pages in the
 * extent tree an 4x-6x higher write load than ext3.
 *
 * Instead of doing a tree commit on every fsync, we use the
 * key ranges and transaction ids to find items for a given file or directory
 * that have changed in this transaction.  Those items are copied into
 * a special tree (one per subvolume root), that tree is written to disk
 * and then the fsync is considered complete.
 *
 * After a crash, items are copied out of the log-tree back into the
 * subvolume tree.  Any file data extents found are recorded in the extent
 * allocation tree, and the log-tree freed.
 *
 * The log tree is read three times, once to pin down all the extents it is
 * using in ram and once, once to create all the inodes logged in the tree
 * and once to do all the other items.
 */

static int start_log_trans(struct btrfs_trans_handle *trans,
			   struct btrfs_root *root)
{
	int ret;
	int err = 0;

	mutex_lock(&root->log_mutex);
	if (root->log_root) {
		if (!root->log_start_pid) {
			root->log_start_pid = current->pid;
			root->log_multiple_pids = false;
		} else if (root->log_start_pid != current->pid) {
			root->log_multiple_pids = true;
		}

		root->log_batch++;
		atomic_inc(&root->log_writers);
		mutex_unlock(&root->log_mutex);
		return 0;
	}
	root->log_multiple_pids = false;
	root->log_start_pid = current->pid;
	mutex_lock(&root->fs_info->tree_log_mutex);
	if (!root->fs_info->log_root_tree) {
		ret = btrfs_init_log_root_tree(trans, root->fs_info);
		if (ret)
			err = ret;
	}
	if (err == 0 && !root->log_root) {
		ret = btrfs_add_log_tree(trans, root);
		if (ret)
			err = ret;
	}
	mutex_unlock(&root->fs_info->tree_log_mutex);
	root->log_batch++;
	atomic_inc(&root->log_writers);
	mutex_unlock(&root->log_mutex);
	return err;
}

static int join_running_log_trans(struct btrfs_root *root)
{
	int ret = -ENOENT;

	smp_mb();
	if (!root->log_root)
		return -ENOENT;

	mutex_lock(&root->log_mutex);
	if (root->log_root) {
		ret = 0;
		atomic_inc(&root->log_writers);
	}
	mutex_unlock(&root->log_mutex);
	return ret;
}

int btrfs_pin_log_trans(struct btrfs_root *root)
{
	int ret = -ENOENT;

	mutex_lock(&root->log_mutex);
	atomic_inc(&root->log_writers);
	mutex_unlock(&root->log_mutex);
	return ret;
}

void btrfs_end_log_trans(struct btrfs_root *root)
{
	if (atomic_dec_and_test(&root->log_writers)) {
		smp_mb();
		if (waitqueue_active(&root->log_writer_wait))
			wake_up(&root->log_writer_wait);
	}
}


struct walk_control {
	int free;

	int write;

	int wait;

	int pin;

	
	int stage;

	
	struct btrfs_root *replay_dest;

	
	struct btrfs_trans_handle *trans;

	int (*process_func)(struct btrfs_root *log, struct extent_buffer *eb,
			    struct walk_control *wc, u64 gen);
};

static int process_one_buffer(struct btrfs_root *log,
			      struct extent_buffer *eb,
			      struct walk_control *wc, u64 gen)
{
	if (wc->pin)
		btrfs_pin_extent_for_log_replay(wc->trans,
						log->fs_info->extent_root,
						eb->start, eb->len);

	if (btrfs_buffer_uptodate(eb, gen, 0)) {
		if (wc->write)
			btrfs_write_tree_block(eb);
		if (wc->wait)
			btrfs_wait_tree_block_writeback(eb);
	}
	return 0;
}

/*
 * Item overwrite used by replay and tree logging.  eb, slot and key all refer
 * to the src data we are copying out.
 *
 * root is the tree we are copying into, and path is a scratch
 * path for use in this function (it should be released on entry and
 * will be released on exit).
 *
 * If the key is already in the destination tree the existing item is
 * overwritten.  If the existing item isn't big enough, it is extended.
 * If it is too large, it is truncated.
 *
 * If the key isn't in the destination yet, a new item is inserted.
 */
static noinline int overwrite_item(struct btrfs_trans_handle *trans,
				   struct btrfs_root *root,
				   struct btrfs_path *path,
				   struct extent_buffer *eb, int slot,
				   struct btrfs_key *key)
{
	int ret;
	u32 item_size;
	u64 saved_i_size = 0;
	int save_old_i_size = 0;
	unsigned long src_ptr;
	unsigned long dst_ptr;
	int overwrite_root = 0;

	if (root->root_key.objectid != BTRFS_TREE_LOG_OBJECTID)
		overwrite_root = 1;

	item_size = btrfs_item_size_nr(eb, slot);
	src_ptr = btrfs_item_ptr_offset(eb, slot);

	
	ret = btrfs_search_slot(NULL, root, key, path, 0, 0);
	if (ret == 0) {
		char *src_copy;
		char *dst_copy;
		u32 dst_size = btrfs_item_size_nr(path->nodes[0],
						  path->slots[0]);
		if (dst_size != item_size)
			goto insert;

		if (item_size == 0) {
			btrfs_release_path(path);
			return 0;
		}
		dst_copy = kmalloc(item_size, GFP_NOFS);
		src_copy = kmalloc(item_size, GFP_NOFS);
		if (!dst_copy || !src_copy) {
			btrfs_release_path(path);
			kfree(dst_copy);
			kfree(src_copy);
			return -ENOMEM;
		}

		read_extent_buffer(eb, src_copy, src_ptr, item_size);

		dst_ptr = btrfs_item_ptr_offset(path->nodes[0], path->slots[0]);
		read_extent_buffer(path->nodes[0], dst_copy, dst_ptr,
				   item_size);
		ret = memcmp(dst_copy, src_copy, item_size);

		kfree(dst_copy);
		kfree(src_copy);
		if (ret == 0) {
			btrfs_release_path(path);
			return 0;
		}

	}
insert:
	btrfs_release_path(path);
	
	ret = btrfs_insert_empty_item(trans, root, path,
				      key, item_size);

	
	if (ret == -EEXIST) {
		u32 found_size;
		found_size = btrfs_item_size_nr(path->nodes[0],
						path->slots[0]);
		if (found_size > item_size)
			btrfs_truncate_item(trans, root, path, item_size, 1);
		else if (found_size < item_size)
			btrfs_extend_item(trans, root, path,
					  item_size - found_size);
	} else if (ret) {
		return ret;
	}
	dst_ptr = btrfs_item_ptr_offset(path->nodes[0],
					path->slots[0]);

	if (key->type == BTRFS_INODE_ITEM_KEY && ret == -EEXIST) {
		struct btrfs_inode_item *src_item;
		struct btrfs_inode_item *dst_item;

		src_item = (struct btrfs_inode_item *)src_ptr;
		dst_item = (struct btrfs_inode_item *)dst_ptr;

		if (btrfs_inode_generation(eb, src_item) == 0)
			goto no_copy;

		if (overwrite_root &&
		    S_ISDIR(btrfs_inode_mode(eb, src_item)) &&
		    S_ISDIR(btrfs_inode_mode(path->nodes[0], dst_item))) {
			save_old_i_size = 1;
			saved_i_size = btrfs_inode_size(path->nodes[0],
							dst_item);
		}
	}

	copy_extent_buffer(path->nodes[0], eb, dst_ptr,
			   src_ptr, item_size);

	if (save_old_i_size) {
		struct btrfs_inode_item *dst_item;
		dst_item = (struct btrfs_inode_item *)dst_ptr;
		btrfs_set_inode_size(path->nodes[0], dst_item, saved_i_size);
	}

	
	if (key->type == BTRFS_INODE_ITEM_KEY) {
		struct btrfs_inode_item *dst_item;
		dst_item = (struct btrfs_inode_item *)dst_ptr;
		if (btrfs_inode_generation(path->nodes[0], dst_item) == 0) {
			btrfs_set_inode_generation(path->nodes[0], dst_item,
						   trans->transid);
		}
	}
no_copy:
	btrfs_mark_buffer_dirty(path->nodes[0]);
	btrfs_release_path(path);
	return 0;
}

static noinline struct inode *read_one_inode(struct btrfs_root *root,
					     u64 objectid)
{
	struct btrfs_key key;
	struct inode *inode;

	key.objectid = objectid;
	key.type = BTRFS_INODE_ITEM_KEY;
	key.offset = 0;
	inode = btrfs_iget(root->fs_info->sb, &key, root, NULL);
	if (IS_ERR(inode)) {
		inode = NULL;
	} else if (is_bad_inode(inode)) {
		iput(inode);
		inode = NULL;
	}
	return inode;
}

static noinline int replay_one_extent(struct btrfs_trans_handle *trans,
				      struct btrfs_root *root,
				      struct btrfs_path *path,
				      struct extent_buffer *eb, int slot,
				      struct btrfs_key *key)
{
	int found_type;
	u64 mask = root->sectorsize - 1;
	u64 extent_end;
	u64 alloc_hint;
	u64 start = key->offset;
	u64 saved_nbytes;
	struct btrfs_file_extent_item *item;
	struct inode *inode = NULL;
	unsigned long size;
	int ret = 0;

	item = btrfs_item_ptr(eb, slot, struct btrfs_file_extent_item);
	found_type = btrfs_file_extent_type(eb, item);

	if (found_type == BTRFS_FILE_EXTENT_REG ||
	    found_type == BTRFS_FILE_EXTENT_PREALLOC)
		extent_end = start + btrfs_file_extent_num_bytes(eb, item);
	else if (found_type == BTRFS_FILE_EXTENT_INLINE) {
		size = btrfs_file_extent_inline_len(eb, item);
		extent_end = (start + size + mask) & ~mask;
	} else {
		ret = 0;
		goto out;
	}

	inode = read_one_inode(root, key->objectid);
	if (!inode) {
		ret = -EIO;
		goto out;
	}

	ret = btrfs_lookup_file_extent(trans, root, path, btrfs_ino(inode),
				       start, 0);

	if (ret == 0 &&
	    (found_type == BTRFS_FILE_EXTENT_REG ||
	     found_type == BTRFS_FILE_EXTENT_PREALLOC)) {
		struct btrfs_file_extent_item cmp1;
		struct btrfs_file_extent_item cmp2;
		struct btrfs_file_extent_item *existing;
		struct extent_buffer *leaf;

		leaf = path->nodes[0];
		existing = btrfs_item_ptr(leaf, path->slots[0],
					  struct btrfs_file_extent_item);

		read_extent_buffer(eb, &cmp1, (unsigned long)item,
				   sizeof(cmp1));
		read_extent_buffer(leaf, &cmp2, (unsigned long)existing,
				   sizeof(cmp2));

		if (memcmp(&cmp1, &cmp2, sizeof(cmp1)) == 0) {
			btrfs_release_path(path);
			goto out;
		}
	}
	btrfs_release_path(path);

	saved_nbytes = inode_get_bytes(inode);
	
	ret = btrfs_drop_extents(trans, inode, start, extent_end,
				 &alloc_hint, 1);
	BUG_ON(ret);

	if (found_type == BTRFS_FILE_EXTENT_REG ||
	    found_type == BTRFS_FILE_EXTENT_PREALLOC) {
		u64 offset;
		unsigned long dest_offset;
		struct btrfs_key ins;

		ret = btrfs_insert_empty_item(trans, root, path, key,
					      sizeof(*item));
		BUG_ON(ret);
		dest_offset = btrfs_item_ptr_offset(path->nodes[0],
						    path->slots[0]);
		copy_extent_buffer(path->nodes[0], eb, dest_offset,
				(unsigned long)item,  sizeof(*item));

		ins.objectid = btrfs_file_extent_disk_bytenr(eb, item);
		ins.offset = btrfs_file_extent_disk_num_bytes(eb, item);
		ins.type = BTRFS_EXTENT_ITEM_KEY;
		offset = key->offset - btrfs_file_extent_offset(eb, item);

		if (ins.objectid > 0) {
			u64 csum_start;
			u64 csum_end;
			LIST_HEAD(ordered_sums);
			ret = btrfs_lookup_extent(root, ins.objectid,
						ins.offset);
			if (ret == 0) {
				ret = btrfs_inc_extent_ref(trans, root,
						ins.objectid, ins.offset,
						0, root->root_key.objectid,
						key->objectid, offset, 0);
				BUG_ON(ret);
			} else {
				ret = btrfs_alloc_logged_file_extent(trans,
						root, root->root_key.objectid,
						key->objectid, offset, &ins);
				BUG_ON(ret);
			}
			btrfs_release_path(path);

			if (btrfs_file_extent_compression(eb, item)) {
				csum_start = ins.objectid;
				csum_end = csum_start + ins.offset;
			} else {
				csum_start = ins.objectid +
					btrfs_file_extent_offset(eb, item);
				csum_end = csum_start +
					btrfs_file_extent_num_bytes(eb, item);
			}

			ret = btrfs_lookup_csums_range(root->log_root,
						csum_start, csum_end - 1,
						&ordered_sums, 0);
			BUG_ON(ret);
			while (!list_empty(&ordered_sums)) {
				struct btrfs_ordered_sum *sums;
				sums = list_entry(ordered_sums.next,
						struct btrfs_ordered_sum,
						list);
				ret = btrfs_csum_file_blocks(trans,
						root->fs_info->csum_root,
						sums);
				BUG_ON(ret);
				list_del(&sums->list);
				kfree(sums);
			}
		} else {
			btrfs_release_path(path);
		}
	} else if (found_type == BTRFS_FILE_EXTENT_INLINE) {
		
		ret = overwrite_item(trans, root, path, eb, slot, key);
		BUG_ON(ret);
	}

	inode_set_bytes(inode, saved_nbytes);
	btrfs_update_inode(trans, root, inode);
out:
	if (inode)
		iput(inode);
	return ret;
}

static noinline int drop_one_dir_item(struct btrfs_trans_handle *trans,
				      struct btrfs_root *root,
				      struct btrfs_path *path,
				      struct inode *dir,
				      struct btrfs_dir_item *di)
{
	struct inode *inode;
	char *name;
	int name_len;
	struct extent_buffer *leaf;
	struct btrfs_key location;
	int ret;

	leaf = path->nodes[0];

	btrfs_dir_item_key_to_cpu(leaf, di, &location);
	name_len = btrfs_dir_name_len(leaf, di);
	name = kmalloc(name_len, GFP_NOFS);
	if (!name)
		return -ENOMEM;

	read_extent_buffer(leaf, name, (unsigned long)(di + 1), name_len);
	btrfs_release_path(path);

	inode = read_one_inode(root, location.objectid);
	if (!inode) {
		kfree(name);
		return -EIO;
	}

	ret = link_to_fixup_dir(trans, root, path, location.objectid);
	BUG_ON(ret);

	ret = btrfs_unlink_inode(trans, root, dir, inode, name, name_len);
	BUG_ON(ret);
	kfree(name);

	iput(inode);
	return ret;
}

static noinline int inode_in_dir(struct btrfs_root *root,
				 struct btrfs_path *path,
				 u64 dirid, u64 objectid, u64 index,
				 const char *name, int name_len)
{
	struct btrfs_dir_item *di;
	struct btrfs_key location;
	int match = 0;

	di = btrfs_lookup_dir_index_item(NULL, root, path, dirid,
					 index, name, name_len, 0);
	if (di && !IS_ERR(di)) {
		btrfs_dir_item_key_to_cpu(path->nodes[0], di, &location);
		if (location.objectid != objectid)
			goto out;
	} else
		goto out;
	btrfs_release_path(path);

	di = btrfs_lookup_dir_item(NULL, root, path, dirid, name, name_len, 0);
	if (di && !IS_ERR(di)) {
		btrfs_dir_item_key_to_cpu(path->nodes[0], di, &location);
		if (location.objectid != objectid)
			goto out;
	} else
		goto out;
	match = 1;
out:
	btrfs_release_path(path);
	return match;
}

static noinline int backref_in_log(struct btrfs_root *log,
				   struct btrfs_key *key,
				   char *name, int namelen)
{
	struct btrfs_path *path;
	struct btrfs_inode_ref *ref;
	unsigned long ptr;
	unsigned long ptr_end;
	unsigned long name_ptr;
	int found_name_len;
	int item_size;
	int ret;
	int match = 0;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	ret = btrfs_search_slot(NULL, log, key, path, 0, 0);
	if (ret != 0)
		goto out;

	item_size = btrfs_item_size_nr(path->nodes[0], path->slots[0]);
	ptr = btrfs_item_ptr_offset(path->nodes[0], path->slots[0]);
	ptr_end = ptr + item_size;
	while (ptr < ptr_end) {
		ref = (struct btrfs_inode_ref *)ptr;
		found_name_len = btrfs_inode_ref_name_len(path->nodes[0], ref);
		if (found_name_len == namelen) {
			name_ptr = (unsigned long)(ref + 1);
			ret = memcmp_extent_buffer(path->nodes[0], name,
						   name_ptr, namelen);
			if (ret == 0) {
				match = 1;
				goto out;
			}
		}
		ptr = (unsigned long)(ref + 1) + found_name_len;
	}
out:
	btrfs_free_path(path);
	return match;
}


static noinline int add_inode_ref(struct btrfs_trans_handle *trans,
				  struct btrfs_root *root,
				  struct btrfs_root *log,
				  struct btrfs_path *path,
				  struct extent_buffer *eb, int slot,
				  struct btrfs_key *key)
{
	struct btrfs_inode_ref *ref;
	struct btrfs_dir_item *di;
	struct inode *dir;
	struct inode *inode;
	unsigned long ref_ptr;
	unsigned long ref_end;
	char *name;
	int namelen;
	int ret;
	int search_done = 0;

	dir = read_one_inode(root, key->offset);
	if (!dir)
		return -ENOENT;

	inode = read_one_inode(root, key->objectid);
	if (!inode) {
		iput(dir);
		return -EIO;
	}

	ref_ptr = btrfs_item_ptr_offset(eb, slot);
	ref_end = ref_ptr + btrfs_item_size_nr(eb, slot);

again:
	ref = (struct btrfs_inode_ref *)ref_ptr;

	namelen = btrfs_inode_ref_name_len(eb, ref);
	name = kmalloc(namelen, GFP_NOFS);
	BUG_ON(!name);

	read_extent_buffer(eb, name, (unsigned long)(ref + 1), namelen);

	
	if (inode_in_dir(root, path, btrfs_ino(dir), btrfs_ino(inode),
			 btrfs_inode_ref_index(eb, ref),
			 name, namelen)) {
		goto out;
	}


	if (search_done)
		goto insert;

	ret = btrfs_search_slot(NULL, root, key, path, 0, 0);
	if (ret == 0) {
		char *victim_name;
		int victim_name_len;
		struct btrfs_inode_ref *victim_ref;
		unsigned long ptr;
		unsigned long ptr_end;
		struct extent_buffer *leaf = path->nodes[0];

		if (key->objectid == key->offset)
			goto out_nowrite;

		ptr = btrfs_item_ptr_offset(leaf, path->slots[0]);
		ptr_end = ptr + btrfs_item_size_nr(leaf, path->slots[0]);
		while (ptr < ptr_end) {
			victim_ref = (struct btrfs_inode_ref *)ptr;
			victim_name_len = btrfs_inode_ref_name_len(leaf,
								   victim_ref);
			victim_name = kmalloc(victim_name_len, GFP_NOFS);
			BUG_ON(!victim_name);

			read_extent_buffer(leaf, victim_name,
					   (unsigned long)(victim_ref + 1),
					   victim_name_len);

			if (!backref_in_log(log, key, victim_name,
					    victim_name_len)) {
				btrfs_inc_nlink(inode);
				btrfs_release_path(path);

				ret = btrfs_unlink_inode(trans, root, dir,
							 inode, victim_name,
							 victim_name_len);
			}
			kfree(victim_name);
			ptr = (unsigned long)(victim_ref + 1) + victim_name_len;
		}
		BUG_ON(ret);

		search_done = 1;
	}
	btrfs_release_path(path);

	
	di = btrfs_lookup_dir_index_item(trans, root, path, btrfs_ino(dir),
					 btrfs_inode_ref_index(eb, ref),
					 name, namelen, 0);
	if (di && !IS_ERR(di)) {
		ret = drop_one_dir_item(trans, root, path, dir, di);
		BUG_ON(ret);
	}
	btrfs_release_path(path);

	
	di = btrfs_lookup_dir_item(trans, root, path, btrfs_ino(dir),
				   name, namelen, 0);
	if (di && !IS_ERR(di)) {
		ret = drop_one_dir_item(trans, root, path, dir, di);
		BUG_ON(ret);
	}
	btrfs_release_path(path);

insert:
	
	ret = btrfs_add_link(trans, dir, inode, name, namelen, 0,
			     btrfs_inode_ref_index(eb, ref));
	BUG_ON(ret);

	btrfs_update_inode(trans, root, inode);

out:
	ref_ptr = (unsigned long)(ref + 1) + namelen;
	kfree(name);
	if (ref_ptr < ref_end)
		goto again;

	
	ret = overwrite_item(trans, root, path, eb, slot, key);
	BUG_ON(ret);

out_nowrite:
	btrfs_release_path(path);
	iput(dir);
	iput(inode);
	return 0;
}

static int insert_orphan_item(struct btrfs_trans_handle *trans,
			      struct btrfs_root *root, u64 offset)
{
	int ret;
	ret = btrfs_find_orphan_item(root, offset);
	if (ret > 0)
		ret = btrfs_insert_orphan_item(trans, root, offset);
	return ret;
}


static noinline int fixup_inode_link_count(struct btrfs_trans_handle *trans,
					   struct btrfs_root *root,
					   struct inode *inode)
{
	struct btrfs_path *path;
	int ret;
	struct btrfs_key key;
	u64 nlink = 0;
	unsigned long ptr;
	unsigned long ptr_end;
	int name_len;
	u64 ino = btrfs_ino(inode);

	key.objectid = ino;
	key.type = BTRFS_INODE_REF_KEY;
	key.offset = (u64)-1;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	while (1) {
		ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
		if (ret < 0)
			break;
		if (ret > 0) {
			if (path->slots[0] == 0)
				break;
			path->slots[0]--;
		}
		btrfs_item_key_to_cpu(path->nodes[0], &key,
				      path->slots[0]);
		if (key.objectid != ino ||
		    key.type != BTRFS_INODE_REF_KEY)
			break;
		ptr = btrfs_item_ptr_offset(path->nodes[0], path->slots[0]);
		ptr_end = ptr + btrfs_item_size_nr(path->nodes[0],
						   path->slots[0]);
		while (ptr < ptr_end) {
			struct btrfs_inode_ref *ref;

			ref = (struct btrfs_inode_ref *)ptr;
			name_len = btrfs_inode_ref_name_len(path->nodes[0],
							    ref);
			ptr = (unsigned long)(ref + 1) + name_len;
			nlink++;
		}

		if (key.offset == 0)
			break;
		key.offset--;
		btrfs_release_path(path);
	}
	btrfs_release_path(path);
	if (nlink != inode->i_nlink) {
		set_nlink(inode, nlink);
		btrfs_update_inode(trans, root, inode);
	}
	BTRFS_I(inode)->index_cnt = (u64)-1;

	if (inode->i_nlink == 0) {
		if (S_ISDIR(inode->i_mode)) {
			ret = replay_dir_deletes(trans, root, NULL, path,
						 ino, 1);
			BUG_ON(ret);
		}
		ret = insert_orphan_item(trans, root, ino);
		BUG_ON(ret);
	}
	btrfs_free_path(path);

	return 0;
}

static noinline int fixup_inode_link_counts(struct btrfs_trans_handle *trans,
					    struct btrfs_root *root,
					    struct btrfs_path *path)
{
	int ret;
	struct btrfs_key key;
	struct inode *inode;

	key.objectid = BTRFS_TREE_LOG_FIXUP_OBJECTID;
	key.type = BTRFS_ORPHAN_ITEM_KEY;
	key.offset = (u64)-1;
	while (1) {
		ret = btrfs_search_slot(trans, root, &key, path, -1, 1);
		if (ret < 0)
			break;

		if (ret == 1) {
			if (path->slots[0] == 0)
				break;
			path->slots[0]--;
		}

		btrfs_item_key_to_cpu(path->nodes[0], &key, path->slots[0]);
		if (key.objectid != BTRFS_TREE_LOG_FIXUP_OBJECTID ||
		    key.type != BTRFS_ORPHAN_ITEM_KEY)
			break;

		ret = btrfs_del_item(trans, root, path);
		if (ret)
			goto out;

		btrfs_release_path(path);
		inode = read_one_inode(root, key.offset);
		if (!inode)
			return -EIO;

		ret = fixup_inode_link_count(trans, root, inode);
		BUG_ON(ret);

		iput(inode);

		key.offset = (u64)-1;
	}
	ret = 0;
out:
	btrfs_release_path(path);
	return ret;
}


static noinline int link_to_fixup_dir(struct btrfs_trans_handle *trans,
				      struct btrfs_root *root,
				      struct btrfs_path *path,
				      u64 objectid)
{
	struct btrfs_key key;
	int ret = 0;
	struct inode *inode;

	inode = read_one_inode(root, objectid);
	if (!inode)
		return -EIO;

	key.objectid = BTRFS_TREE_LOG_FIXUP_OBJECTID;
	btrfs_set_key_type(&key, BTRFS_ORPHAN_ITEM_KEY);
	key.offset = objectid;

	ret = btrfs_insert_empty_item(trans, root, path, &key, 0);

	btrfs_release_path(path);
	if (ret == 0) {
		btrfs_inc_nlink(inode);
		btrfs_update_inode(trans, root, inode);
	} else if (ret == -EEXIST) {
		ret = 0;
	} else {
		BUG();
	}
	iput(inode);

	return ret;
}

static noinline int insert_one_name(struct btrfs_trans_handle *trans,
				    struct btrfs_root *root,
				    struct btrfs_path *path,
				    u64 dirid, u64 index,
				    char *name, int name_len, u8 type,
				    struct btrfs_key *location)
{
	struct inode *inode;
	struct inode *dir;
	int ret;

	inode = read_one_inode(root, location->objectid);
	if (!inode)
		return -ENOENT;

	dir = read_one_inode(root, dirid);
	if (!dir) {
		iput(inode);
		return -EIO;
	}
	ret = btrfs_add_link(trans, dir, inode, name, name_len, 1, index);

	

	iput(inode);
	iput(dir);
	return ret;
}

static noinline int replay_one_name(struct btrfs_trans_handle *trans,
				    struct btrfs_root *root,
				    struct btrfs_path *path,
				    struct extent_buffer *eb,
				    struct btrfs_dir_item *di,
				    struct btrfs_key *key)
{
	char *name;
	int name_len;
	struct btrfs_dir_item *dst_di;
	struct btrfs_key found_key;
	struct btrfs_key log_key;
	struct inode *dir;
	u8 log_type;
	int exists;
	int ret;

	dir = read_one_inode(root, key->objectid);
	if (!dir)
		return -EIO;

	name_len = btrfs_dir_name_len(eb, di);
	name = kmalloc(name_len, GFP_NOFS);
	if (!name)
		return -ENOMEM;

	log_type = btrfs_dir_type(eb, di);
	read_extent_buffer(eb, name, (unsigned long)(di + 1),
		   name_len);

	btrfs_dir_item_key_to_cpu(eb, di, &log_key);
	exists = btrfs_lookup_inode(trans, root, path, &log_key, 0);
	if (exists == 0)
		exists = 1;
	else
		exists = 0;
	btrfs_release_path(path);

	if (key->type == BTRFS_DIR_ITEM_KEY) {
		dst_di = btrfs_lookup_dir_item(trans, root, path, key->objectid,
				       name, name_len, 1);
	} else if (key->type == BTRFS_DIR_INDEX_KEY) {
		dst_di = btrfs_lookup_dir_index_item(trans, root, path,
						     key->objectid,
						     key->offset, name,
						     name_len, 1);
	} else {
		BUG();
	}
	if (IS_ERR_OR_NULL(dst_di)) {
		if (key->type != BTRFS_DIR_INDEX_KEY)
			goto out;
		goto insert;
	}

	btrfs_dir_item_key_to_cpu(path->nodes[0], dst_di, &found_key);
	
	if (found_key.objectid == log_key.objectid &&
	    found_key.type == log_key.type &&
	    found_key.offset == log_key.offset &&
	    btrfs_dir_type(path->nodes[0], dst_di) == log_type) {
		goto out;
	}

	if (!exists)
		goto out;

	ret = drop_one_dir_item(trans, root, path, dir, dst_di);
	BUG_ON(ret);

	if (key->type == BTRFS_DIR_INDEX_KEY)
		goto insert;
out:
	btrfs_release_path(path);
	kfree(name);
	iput(dir);
	return 0;

insert:
	btrfs_release_path(path);
	ret = insert_one_name(trans, root, path, key->objectid, key->offset,
			      name, name_len, log_type, &log_key);

	BUG_ON(ret && ret != -ENOENT);
	goto out;
}

static noinline int replay_one_dir_item(struct btrfs_trans_handle *trans,
					struct btrfs_root *root,
					struct btrfs_path *path,
					struct extent_buffer *eb, int slot,
					struct btrfs_key *key)
{
	int ret;
	u32 item_size = btrfs_item_size_nr(eb, slot);
	struct btrfs_dir_item *di;
	int name_len;
	unsigned long ptr;
	unsigned long ptr_end;

	ptr = btrfs_item_ptr_offset(eb, slot);
	ptr_end = ptr + item_size;
	while (ptr < ptr_end) {
		di = (struct btrfs_dir_item *)ptr;
		if (verify_dir_item(root, eb, di))
			return -EIO;
		name_len = btrfs_dir_name_len(eb, di);
		ret = replay_one_name(trans, root, path, eb, di, key);
		BUG_ON(ret);
		ptr = (unsigned long)(di + 1);
		ptr += name_len;
	}
	return 0;
}

static noinline int find_dir_range(struct btrfs_root *root,
				   struct btrfs_path *path,
				   u64 dirid, int key_type,
				   u64 *start_ret, u64 *end_ret)
{
	struct btrfs_key key;
	u64 found_end;
	struct btrfs_dir_log_item *item;
	int ret;
	int nritems;

	if (*start_ret == (u64)-1)
		return 1;

	key.objectid = dirid;
	key.type = key_type;
	key.offset = *start_ret;

	ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
	if (ret < 0)
		goto out;
	if (ret > 0) {
		if (path->slots[0] == 0)
			goto out;
		path->slots[0]--;
	}
	if (ret != 0)
		btrfs_item_key_to_cpu(path->nodes[0], &key, path->slots[0]);

	if (key.type != key_type || key.objectid != dirid) {
		ret = 1;
		goto next;
	}
	item = btrfs_item_ptr(path->nodes[0], path->slots[0],
			      struct btrfs_dir_log_item);
	found_end = btrfs_dir_log_end(path->nodes[0], item);

	if (*start_ret >= key.offset && *start_ret <= found_end) {
		ret = 0;
		*start_ret = key.offset;
		*end_ret = found_end;
		goto out;
	}
	ret = 1;
next:
	
	nritems = btrfs_header_nritems(path->nodes[0]);
	if (path->slots[0] >= nritems) {
		ret = btrfs_next_leaf(root, path);
		if (ret)
			goto out;
	} else {
		path->slots[0]++;
	}

	btrfs_item_key_to_cpu(path->nodes[0], &key, path->slots[0]);

	if (key.type != key_type || key.objectid != dirid) {
		ret = 1;
		goto out;
	}
	item = btrfs_item_ptr(path->nodes[0], path->slots[0],
			      struct btrfs_dir_log_item);
	found_end = btrfs_dir_log_end(path->nodes[0], item);
	*start_ret = key.offset;
	*end_ret = found_end;
	ret = 0;
out:
	btrfs_release_path(path);
	return ret;
}

static noinline int check_item_in_log(struct btrfs_trans_handle *trans,
				      struct btrfs_root *root,
				      struct btrfs_root *log,
				      struct btrfs_path *path,
				      struct btrfs_path *log_path,
				      struct inode *dir,
				      struct btrfs_key *dir_key)
{
	int ret;
	struct extent_buffer *eb;
	int slot;
	u32 item_size;
	struct btrfs_dir_item *di;
	struct btrfs_dir_item *log_di;
	int name_len;
	unsigned long ptr;
	unsigned long ptr_end;
	char *name;
	struct inode *inode;
	struct btrfs_key location;

again:
	eb = path->nodes[0];
	slot = path->slots[0];
	item_size = btrfs_item_size_nr(eb, slot);
	ptr = btrfs_item_ptr_offset(eb, slot);
	ptr_end = ptr + item_size;
	while (ptr < ptr_end) {
		di = (struct btrfs_dir_item *)ptr;
		if (verify_dir_item(root, eb, di)) {
			ret = -EIO;
			goto out;
		}

		name_len = btrfs_dir_name_len(eb, di);
		name = kmalloc(name_len, GFP_NOFS);
		if (!name) {
			ret = -ENOMEM;
			goto out;
		}
		read_extent_buffer(eb, name, (unsigned long)(di + 1),
				  name_len);
		log_di = NULL;
		if (log && dir_key->type == BTRFS_DIR_ITEM_KEY) {
			log_di = btrfs_lookup_dir_item(trans, log, log_path,
						       dir_key->objectid,
						       name, name_len, 0);
		} else if (log && dir_key->type == BTRFS_DIR_INDEX_KEY) {
			log_di = btrfs_lookup_dir_index_item(trans, log,
						     log_path,
						     dir_key->objectid,
						     dir_key->offset,
						     name, name_len, 0);
		}
		if (IS_ERR_OR_NULL(log_di)) {
			btrfs_dir_item_key_to_cpu(eb, di, &location);
			btrfs_release_path(path);
			btrfs_release_path(log_path);
			inode = read_one_inode(root, location.objectid);
			if (!inode) {
				kfree(name);
				return -EIO;
			}

			ret = link_to_fixup_dir(trans, root,
						path, location.objectid);
			BUG_ON(ret);
			btrfs_inc_nlink(inode);
			ret = btrfs_unlink_inode(trans, root, dir, inode,
						 name, name_len);
			BUG_ON(ret);
			kfree(name);
			iput(inode);

			ret = btrfs_search_slot(NULL, root, dir_key, path,
						0, 0);
			if (ret == 0)
				goto again;
			ret = 0;
			goto out;
		}
		btrfs_release_path(log_path);
		kfree(name);

		ptr = (unsigned long)(di + 1);
		ptr += name_len;
	}
	ret = 0;
out:
	btrfs_release_path(path);
	btrfs_release_path(log_path);
	return ret;
}

static noinline int replay_dir_deletes(struct btrfs_trans_handle *trans,
				       struct btrfs_root *root,
				       struct btrfs_root *log,
				       struct btrfs_path *path,
				       u64 dirid, int del_all)
{
	u64 range_start;
	u64 range_end;
	int key_type = BTRFS_DIR_LOG_ITEM_KEY;
	int ret = 0;
	struct btrfs_key dir_key;
	struct btrfs_key found_key;
	struct btrfs_path *log_path;
	struct inode *dir;

	dir_key.objectid = dirid;
	dir_key.type = BTRFS_DIR_ITEM_KEY;
	log_path = btrfs_alloc_path();
	if (!log_path)
		return -ENOMEM;

	dir = read_one_inode(root, dirid);
	if (!dir) {
		btrfs_free_path(log_path);
		return 0;
	}
again:
	range_start = 0;
	range_end = 0;
	while (1) {
		if (del_all)
			range_end = (u64)-1;
		else {
			ret = find_dir_range(log, path, dirid, key_type,
					     &range_start, &range_end);
			if (ret != 0)
				break;
		}

		dir_key.offset = range_start;
		while (1) {
			int nritems;
			ret = btrfs_search_slot(NULL, root, &dir_key, path,
						0, 0);
			if (ret < 0)
				goto out;

			nritems = btrfs_header_nritems(path->nodes[0]);
			if (path->slots[0] >= nritems) {
				ret = btrfs_next_leaf(root, path);
				if (ret)
					break;
			}
			btrfs_item_key_to_cpu(path->nodes[0], &found_key,
					      path->slots[0]);
			if (found_key.objectid != dirid ||
			    found_key.type != dir_key.type)
				goto next_type;

			if (found_key.offset > range_end)
				break;

			ret = check_item_in_log(trans, root, log, path,
						log_path, dir,
						&found_key);
			BUG_ON(ret);
			if (found_key.offset == (u64)-1)
				break;
			dir_key.offset = found_key.offset + 1;
		}
		btrfs_release_path(path);
		if (range_end == (u64)-1)
			break;
		range_start = range_end + 1;
	}

next_type:
	ret = 0;
	if (key_type == BTRFS_DIR_LOG_ITEM_KEY) {
		key_type = BTRFS_DIR_LOG_INDEX_KEY;
		dir_key.type = BTRFS_DIR_INDEX_KEY;
		btrfs_release_path(path);
		goto again;
	}
out:
	btrfs_release_path(path);
	btrfs_free_path(log_path);
	iput(dir);
	return ret;
}

static int replay_one_buffer(struct btrfs_root *log, struct extent_buffer *eb,
			     struct walk_control *wc, u64 gen)
{
	int nritems;
	struct btrfs_path *path;
	struct btrfs_root *root = wc->replay_dest;
	struct btrfs_key key;
	int level;
	int i;
	int ret;

	btrfs_read_buffer(eb, gen);

	level = btrfs_header_level(eb);

	if (level != 0)
		return 0;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	nritems = btrfs_header_nritems(eb);
	for (i = 0; i < nritems; i++) {
		btrfs_item_key_to_cpu(eb, &key, i);

		
		if (key.type == BTRFS_INODE_ITEM_KEY &&
		    wc->stage == LOG_WALK_REPLAY_INODES) {
			struct btrfs_inode_item *inode_item;
			u32 mode;

			inode_item = btrfs_item_ptr(eb, i,
					    struct btrfs_inode_item);
			mode = btrfs_inode_mode(eb, inode_item);
			if (S_ISDIR(mode)) {
				ret = replay_dir_deletes(wc->trans,
					 root, log, path, key.objectid, 0);
				BUG_ON(ret);
			}
			ret = overwrite_item(wc->trans, root, path,
					     eb, i, &key);
			BUG_ON(ret);

			if (S_ISREG(mode)) {
				ret = insert_orphan_item(wc->trans, root,
							 key.objectid);
				BUG_ON(ret);
			}

			ret = link_to_fixup_dir(wc->trans, root,
						path, key.objectid);
			BUG_ON(ret);
		}
		if (wc->stage < LOG_WALK_REPLAY_ALL)
			continue;

		
		if (key.type == BTRFS_XATTR_ITEM_KEY) {
			ret = overwrite_item(wc->trans, root, path,
					     eb, i, &key);
			BUG_ON(ret);
		} else if (key.type == BTRFS_INODE_REF_KEY) {
			ret = add_inode_ref(wc->trans, root, log, path,
					    eb, i, &key);
			BUG_ON(ret && ret != -ENOENT);
		} else if (key.type == BTRFS_EXTENT_DATA_KEY) {
			ret = replay_one_extent(wc->trans, root, path,
						eb, i, &key);
			BUG_ON(ret);
		} else if (key.type == BTRFS_DIR_ITEM_KEY ||
			   key.type == BTRFS_DIR_INDEX_KEY) {
			ret = replay_one_dir_item(wc->trans, root, path,
						  eb, i, &key);
			BUG_ON(ret);
		}
	}
	btrfs_free_path(path);
	return 0;
}

static noinline int walk_down_log_tree(struct btrfs_trans_handle *trans,
				   struct btrfs_root *root,
				   struct btrfs_path *path, int *level,
				   struct walk_control *wc)
{
	u64 root_owner;
	u64 bytenr;
	u64 ptr_gen;
	struct extent_buffer *next;
	struct extent_buffer *cur;
	struct extent_buffer *parent;
	u32 blocksize;
	int ret = 0;

	WARN_ON(*level < 0);
	WARN_ON(*level >= BTRFS_MAX_LEVEL);

	while (*level > 0) {
		WARN_ON(*level < 0);
		WARN_ON(*level >= BTRFS_MAX_LEVEL);
		cur = path->nodes[*level];

		if (btrfs_header_level(cur) != *level)
			WARN_ON(1);

		if (path->slots[*level] >=
		    btrfs_header_nritems(cur))
			break;

		bytenr = btrfs_node_blockptr(cur, path->slots[*level]);
		ptr_gen = btrfs_node_ptr_generation(cur, path->slots[*level]);
		blocksize = btrfs_level_size(root, *level - 1);

		parent = path->nodes[*level];
		root_owner = btrfs_header_owner(parent);

		next = btrfs_find_create_tree_block(root, bytenr, blocksize);
		if (!next)
			return -ENOMEM;

		if (*level == 1) {
			ret = wc->process_func(root, next, wc, ptr_gen);
			if (ret)
				return ret;

			path->slots[*level]++;
			if (wc->free) {
				btrfs_read_buffer(next, ptr_gen);

				btrfs_tree_lock(next);
				btrfs_set_lock_blocking(next);
				clean_tree_block(trans, root, next);
				btrfs_wait_tree_block_writeback(next);
				btrfs_tree_unlock(next);

				WARN_ON(root_owner !=
					BTRFS_TREE_LOG_OBJECTID);
				ret = btrfs_free_and_pin_reserved_extent(root,
							 bytenr, blocksize);
				BUG_ON(ret); 
			}
			free_extent_buffer(next);
			continue;
		}
		btrfs_read_buffer(next, ptr_gen);

		WARN_ON(*level <= 0);
		if (path->nodes[*level-1])
			free_extent_buffer(path->nodes[*level-1]);
		path->nodes[*level-1] = next;
		*level = btrfs_header_level(next);
		path->slots[*level] = 0;
		cond_resched();
	}
	WARN_ON(*level < 0);
	WARN_ON(*level >= BTRFS_MAX_LEVEL);

	path->slots[*level] = btrfs_header_nritems(path->nodes[*level]);

	cond_resched();
	return 0;
}

static noinline int walk_up_log_tree(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root,
				 struct btrfs_path *path, int *level,
				 struct walk_control *wc)
{
	u64 root_owner;
	int i;
	int slot;
	int ret;

	for (i = *level; i < BTRFS_MAX_LEVEL - 1 && path->nodes[i]; i++) {
		slot = path->slots[i];
		if (slot + 1 < btrfs_header_nritems(path->nodes[i])) {
			path->slots[i]++;
			*level = i;
			WARN_ON(*level == 0);
			return 0;
		} else {
			struct extent_buffer *parent;
			if (path->nodes[*level] == root->node)
				parent = path->nodes[*level];
			else
				parent = path->nodes[*level + 1];

			root_owner = btrfs_header_owner(parent);
			ret = wc->process_func(root, path->nodes[*level], wc,
				 btrfs_header_generation(path->nodes[*level]));
			if (ret)
				return ret;

			if (wc->free) {
				struct extent_buffer *next;

				next = path->nodes[*level];

				btrfs_tree_lock(next);
				btrfs_set_lock_blocking(next);
				clean_tree_block(trans, root, next);
				btrfs_wait_tree_block_writeback(next);
				btrfs_tree_unlock(next);

				WARN_ON(root_owner != BTRFS_TREE_LOG_OBJECTID);
				ret = btrfs_free_and_pin_reserved_extent(root,
						path->nodes[*level]->start,
						path->nodes[*level]->len);
				BUG_ON(ret);
			}
			free_extent_buffer(path->nodes[*level]);
			path->nodes[*level] = NULL;
			*level = i + 1;
		}
	}
	return 1;
}

static int walk_log_tree(struct btrfs_trans_handle *trans,
			 struct btrfs_root *log, struct walk_control *wc)
{
	int ret = 0;
	int wret;
	int level;
	struct btrfs_path *path;
	int i;
	int orig_level;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	level = btrfs_header_level(log->node);
	orig_level = level;
	path->nodes[level] = log->node;
	extent_buffer_get(log->node);
	path->slots[level] = 0;

	while (1) {
		wret = walk_down_log_tree(trans, log, path, &level, wc);
		if (wret > 0)
			break;
		if (wret < 0) {
			ret = wret;
			goto out;
		}

		wret = walk_up_log_tree(trans, log, path, &level, wc);
		if (wret > 0)
			break;
		if (wret < 0) {
			ret = wret;
			goto out;
		}
	}

	
	if (path->nodes[orig_level]) {
		ret = wc->process_func(log, path->nodes[orig_level], wc,
			 btrfs_header_generation(path->nodes[orig_level]));
		if (ret)
			goto out;
		if (wc->free) {
			struct extent_buffer *next;

			next = path->nodes[orig_level];

			btrfs_tree_lock(next);
			btrfs_set_lock_blocking(next);
			clean_tree_block(trans, log, next);
			btrfs_wait_tree_block_writeback(next);
			btrfs_tree_unlock(next);

			WARN_ON(log->root_key.objectid !=
				BTRFS_TREE_LOG_OBJECTID);
			ret = btrfs_free_and_pin_reserved_extent(log, next->start,
							 next->len);
			BUG_ON(ret); 
		}
	}

out:
	for (i = 0; i <= orig_level; i++) {
		if (path->nodes[i]) {
			free_extent_buffer(path->nodes[i]);
			path->nodes[i] = NULL;
		}
	}
	btrfs_free_path(path);
	return ret;
}

static int update_log_root(struct btrfs_trans_handle *trans,
			   struct btrfs_root *log)
{
	int ret;

	if (log->log_transid == 1) {
		
		ret = btrfs_insert_root(trans, log->fs_info->log_root_tree,
				&log->root_key, &log->root_item);
	} else {
		ret = btrfs_update_root(trans, log->fs_info->log_root_tree,
				&log->root_key, &log->root_item);
	}
	return ret;
}

static int wait_log_commit(struct btrfs_trans_handle *trans,
			   struct btrfs_root *root, unsigned long transid)
{
	DEFINE_WAIT(wait);
	int index = transid % 2;

	do {
		prepare_to_wait(&root->log_commit_wait[index],
				&wait, TASK_UNINTERRUPTIBLE);
		mutex_unlock(&root->log_mutex);

		if (root->fs_info->last_trans_log_full_commit !=
		    trans->transid && root->log_transid < transid + 2 &&
		    atomic_read(&root->log_commit[index]))
			schedule();

		finish_wait(&root->log_commit_wait[index], &wait);
		mutex_lock(&root->log_mutex);
	} while (root->fs_info->last_trans_log_full_commit !=
		 trans->transid && root->log_transid < transid + 2 &&
		 atomic_read(&root->log_commit[index]));
	return 0;
}

static void wait_for_writer(struct btrfs_trans_handle *trans,
			    struct btrfs_root *root)
{
	DEFINE_WAIT(wait);
	while (root->fs_info->last_trans_log_full_commit !=
	       trans->transid && atomic_read(&root->log_writers)) {
		prepare_to_wait(&root->log_writer_wait,
				&wait, TASK_UNINTERRUPTIBLE);
		mutex_unlock(&root->log_mutex);
		if (root->fs_info->last_trans_log_full_commit !=
		    trans->transid && atomic_read(&root->log_writers))
			schedule();
		mutex_lock(&root->log_mutex);
		finish_wait(&root->log_writer_wait, &wait);
	}
}

int btrfs_sync_log(struct btrfs_trans_handle *trans,
		   struct btrfs_root *root)
{
	int index1;
	int index2;
	int mark;
	int ret;
	struct btrfs_root *log = root->log_root;
	struct btrfs_root *log_root_tree = root->fs_info->log_root_tree;
	unsigned long log_transid = 0;

	mutex_lock(&root->log_mutex);
	index1 = root->log_transid % 2;
	if (atomic_read(&root->log_commit[index1])) {
		wait_log_commit(trans, root, root->log_transid);
		mutex_unlock(&root->log_mutex);
		return 0;
	}
	atomic_set(&root->log_commit[index1], 1);

	
	if (atomic_read(&root->log_commit[(index1 + 1) % 2]))
		wait_log_commit(trans, root, root->log_transid - 1);
	while (1) {
		unsigned long batch = root->log_batch;
		
		if (!btrfs_test_opt(root, SSD) && root->log_multiple_pids) {
			mutex_unlock(&root->log_mutex);
			schedule_timeout_uninterruptible(1);
			mutex_lock(&root->log_mutex);
		}
		wait_for_writer(trans, root);
		if (batch == root->log_batch)
			break;
	}

	
	if (root->fs_info->last_trans_log_full_commit == trans->transid) {
		ret = -EAGAIN;
		mutex_unlock(&root->log_mutex);
		goto out;
	}

	log_transid = root->log_transid;
	if (log_transid % 2 == 0)
		mark = EXTENT_DIRTY;
	else
		mark = EXTENT_NEW;

	ret = btrfs_write_marked_extents(log, &log->dirty_log_pages, mark);
	if (ret) {
		btrfs_abort_transaction(trans, root, ret);
		mutex_unlock(&root->log_mutex);
		goto out;
	}

	btrfs_set_root_node(&log->root_item, log->node);

	root->log_batch = 0;
	root->log_transid++;
	log->log_transid = root->log_transid;
	root->log_start_pid = 0;
	smp_mb();
	/*
	 * IO has been started, blocks of the log tree have WRITTEN flag set
	 * in their headers. new modifications of the log will be written to
	 * new positions. so it's safe to allow log writers to go in.
	 */
	mutex_unlock(&root->log_mutex);

	mutex_lock(&log_root_tree->log_mutex);
	log_root_tree->log_batch++;
	atomic_inc(&log_root_tree->log_writers);
	mutex_unlock(&log_root_tree->log_mutex);

	ret = update_log_root(trans, log);

	mutex_lock(&log_root_tree->log_mutex);
	if (atomic_dec_and_test(&log_root_tree->log_writers)) {
		smp_mb();
		if (waitqueue_active(&log_root_tree->log_writer_wait))
			wake_up(&log_root_tree->log_writer_wait);
	}

	if (ret) {
		if (ret != -ENOSPC) {
			btrfs_abort_transaction(trans, root, ret);
			mutex_unlock(&log_root_tree->log_mutex);
			goto out;
		}
		root->fs_info->last_trans_log_full_commit = trans->transid;
		btrfs_wait_marked_extents(log, &log->dirty_log_pages, mark);
		mutex_unlock(&log_root_tree->log_mutex);
		ret = -EAGAIN;
		goto out;
	}

	index2 = log_root_tree->log_transid % 2;
	if (atomic_read(&log_root_tree->log_commit[index2])) {
		btrfs_wait_marked_extents(log, &log->dirty_log_pages, mark);
		wait_log_commit(trans, log_root_tree,
				log_root_tree->log_transid);
		mutex_unlock(&log_root_tree->log_mutex);
		ret = 0;
		goto out;
	}
	atomic_set(&log_root_tree->log_commit[index2], 1);

	if (atomic_read(&log_root_tree->log_commit[(index2 + 1) % 2])) {
		wait_log_commit(trans, log_root_tree,
				log_root_tree->log_transid - 1);
	}

	wait_for_writer(trans, log_root_tree);

	if (root->fs_info->last_trans_log_full_commit == trans->transid) {
		btrfs_wait_marked_extents(log, &log->dirty_log_pages, mark);
		mutex_unlock(&log_root_tree->log_mutex);
		ret = -EAGAIN;
		goto out_wake_log_root;
	}

	ret = btrfs_write_and_wait_marked_extents(log_root_tree,
				&log_root_tree->dirty_log_pages,
				EXTENT_DIRTY | EXTENT_NEW);
	if (ret) {
		btrfs_abort_transaction(trans, root, ret);
		mutex_unlock(&log_root_tree->log_mutex);
		goto out_wake_log_root;
	}
	btrfs_wait_marked_extents(log, &log->dirty_log_pages, mark);

	btrfs_set_super_log_root(root->fs_info->super_for_commit,
				log_root_tree->node->start);
	btrfs_set_super_log_root_level(root->fs_info->super_for_commit,
				btrfs_header_level(log_root_tree->node));

	log_root_tree->log_batch = 0;
	log_root_tree->log_transid++;
	smp_mb();

	mutex_unlock(&log_root_tree->log_mutex);

	btrfs_scrub_pause_super(root);
	write_ctree_super(trans, root->fs_info->tree_root, 1);
	btrfs_scrub_continue_super(root);
	ret = 0;

	mutex_lock(&root->log_mutex);
	if (root->last_log_commit < log_transid)
		root->last_log_commit = log_transid;
	mutex_unlock(&root->log_mutex);

out_wake_log_root:
	atomic_set(&log_root_tree->log_commit[index2], 0);
	smp_mb();
	if (waitqueue_active(&log_root_tree->log_commit_wait[index2]))
		wake_up(&log_root_tree->log_commit_wait[index2]);
out:
	atomic_set(&root->log_commit[index1], 0);
	smp_mb();
	if (waitqueue_active(&root->log_commit_wait[index1]))
		wake_up(&root->log_commit_wait[index1]);
	return ret;
}

static void free_log_tree(struct btrfs_trans_handle *trans,
			  struct btrfs_root *log)
{
	int ret;
	u64 start;
	u64 end;
	struct walk_control wc = {
		.free = 1,
		.process_func = process_one_buffer
	};

	ret = walk_log_tree(trans, log, &wc);
	BUG_ON(ret);

	while (1) {
		ret = find_first_extent_bit(&log->dirty_log_pages,
				0, &start, &end, EXTENT_DIRTY | EXTENT_NEW);
		if (ret)
			break;

		clear_extent_bits(&log->dirty_log_pages, start, end,
				  EXTENT_DIRTY | EXTENT_NEW, GFP_NOFS);
	}

	free_extent_buffer(log->node);
	kfree(log);
}

int btrfs_free_log(struct btrfs_trans_handle *trans, struct btrfs_root *root)
{
	if (root->log_root) {
		free_log_tree(trans, root->log_root);
		root->log_root = NULL;
	}
	return 0;
}

int btrfs_free_log_root_tree(struct btrfs_trans_handle *trans,
			     struct btrfs_fs_info *fs_info)
{
	if (fs_info->log_root_tree) {
		free_log_tree(trans, fs_info->log_root_tree);
		fs_info->log_root_tree = NULL;
	}
	return 0;
}

int btrfs_del_dir_entries_in_log(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root,
				 const char *name, int name_len,
				 struct inode *dir, u64 index)
{
	struct btrfs_root *log;
	struct btrfs_dir_item *di;
	struct btrfs_path *path;
	int ret;
	int err = 0;
	int bytes_del = 0;
	u64 dir_ino = btrfs_ino(dir);

	if (BTRFS_I(dir)->logged_trans < trans->transid)
		return 0;

	ret = join_running_log_trans(root);
	if (ret)
		return 0;

	mutex_lock(&BTRFS_I(dir)->log_mutex);

	log = root->log_root;
	path = btrfs_alloc_path();
	if (!path) {
		err = -ENOMEM;
		goto out_unlock;
	}

	di = btrfs_lookup_dir_item(trans, log, path, dir_ino,
				   name, name_len, -1);
	if (IS_ERR(di)) {
		err = PTR_ERR(di);
		goto fail;
	}
	if (di) {
		ret = btrfs_delete_one_dir_name(trans, log, path, di);
		bytes_del += name_len;
		BUG_ON(ret);
	}
	btrfs_release_path(path);
	di = btrfs_lookup_dir_index_item(trans, log, path, dir_ino,
					 index, name, name_len, -1);
	if (IS_ERR(di)) {
		err = PTR_ERR(di);
		goto fail;
	}
	if (di) {
		ret = btrfs_delete_one_dir_name(trans, log, path, di);
		bytes_del += name_len;
		BUG_ON(ret);
	}

	if (bytes_del) {
		struct btrfs_key key;

		key.objectid = dir_ino;
		key.offset = 0;
		key.type = BTRFS_INODE_ITEM_KEY;
		btrfs_release_path(path);

		ret = btrfs_search_slot(trans, log, &key, path, 0, 1);
		if (ret < 0) {
			err = ret;
			goto fail;
		}
		if (ret == 0) {
			struct btrfs_inode_item *item;
			u64 i_size;

			item = btrfs_item_ptr(path->nodes[0], path->slots[0],
					      struct btrfs_inode_item);
			i_size = btrfs_inode_size(path->nodes[0], item);
			if (i_size > bytes_del)
				i_size -= bytes_del;
			else
				i_size = 0;
			btrfs_set_inode_size(path->nodes[0], item, i_size);
			btrfs_mark_buffer_dirty(path->nodes[0]);
		} else
			ret = 0;
		btrfs_release_path(path);
	}
fail:
	btrfs_free_path(path);
out_unlock:
	mutex_unlock(&BTRFS_I(dir)->log_mutex);
	if (ret == -ENOSPC) {
		root->fs_info->last_trans_log_full_commit = trans->transid;
		ret = 0;
	} else if (ret < 0)
		btrfs_abort_transaction(trans, root, ret);

	btrfs_end_log_trans(root);

	return err;
}

int btrfs_del_inode_ref_in_log(struct btrfs_trans_handle *trans,
			       struct btrfs_root *root,
			       const char *name, int name_len,
			       struct inode *inode, u64 dirid)
{
	struct btrfs_root *log;
	u64 index;
	int ret;

	if (BTRFS_I(inode)->logged_trans < trans->transid)
		return 0;

	ret = join_running_log_trans(root);
	if (ret)
		return 0;
	log = root->log_root;
	mutex_lock(&BTRFS_I(inode)->log_mutex);

	ret = btrfs_del_inode_ref(trans, log, name, name_len, btrfs_ino(inode),
				  dirid, &index);
	mutex_unlock(&BTRFS_I(inode)->log_mutex);
	if (ret == -ENOSPC) {
		root->fs_info->last_trans_log_full_commit = trans->transid;
		ret = 0;
	} else if (ret < 0 && ret != -ENOENT)
		btrfs_abort_transaction(trans, root, ret);
	btrfs_end_log_trans(root);

	return ret;
}

static noinline int insert_dir_log_key(struct btrfs_trans_handle *trans,
				       struct btrfs_root *log,
				       struct btrfs_path *path,
				       int key_type, u64 dirid,
				       u64 first_offset, u64 last_offset)
{
	int ret;
	struct btrfs_key key;
	struct btrfs_dir_log_item *item;

	key.objectid = dirid;
	key.offset = first_offset;
	if (key_type == BTRFS_DIR_ITEM_KEY)
		key.type = BTRFS_DIR_LOG_ITEM_KEY;
	else
		key.type = BTRFS_DIR_LOG_INDEX_KEY;
	ret = btrfs_insert_empty_item(trans, log, path, &key, sizeof(*item));
	if (ret)
		return ret;

	item = btrfs_item_ptr(path->nodes[0], path->slots[0],
			      struct btrfs_dir_log_item);
	btrfs_set_dir_log_end(path->nodes[0], item, last_offset);
	btrfs_mark_buffer_dirty(path->nodes[0]);
	btrfs_release_path(path);
	return 0;
}

static noinline int log_dir_items(struct btrfs_trans_handle *trans,
			  struct btrfs_root *root, struct inode *inode,
			  struct btrfs_path *path,
			  struct btrfs_path *dst_path, int key_type,
			  u64 min_offset, u64 *last_offset_ret)
{
	struct btrfs_key min_key;
	struct btrfs_key max_key;
	struct btrfs_root *log = root->log_root;
	struct extent_buffer *src;
	int err = 0;
	int ret;
	int i;
	int nritems;
	u64 first_offset = min_offset;
	u64 last_offset = (u64)-1;
	u64 ino = btrfs_ino(inode);

	log = root->log_root;
	max_key.objectid = ino;
	max_key.offset = (u64)-1;
	max_key.type = key_type;

	min_key.objectid = ino;
	min_key.type = key_type;
	min_key.offset = min_offset;

	path->keep_locks = 1;

	ret = btrfs_search_forward(root, &min_key, &max_key,
				   path, 0, trans->transid);

	if (ret != 0 || min_key.objectid != ino || min_key.type != key_type) {
		min_key.objectid = ino;
		min_key.type = key_type;
		min_key.offset = (u64)-1;
		btrfs_release_path(path);
		ret = btrfs_search_slot(NULL, root, &min_key, path, 0, 0);
		if (ret < 0) {
			btrfs_release_path(path);
			return ret;
		}
		ret = btrfs_previous_item(root, path, ino, key_type);

		if (ret == 0) {
			struct btrfs_key tmp;
			btrfs_item_key_to_cpu(path->nodes[0], &tmp,
					      path->slots[0]);
			if (key_type == tmp.type)
				first_offset = max(min_offset, tmp.offset) + 1;
		}
		goto done;
	}

	
	ret = btrfs_previous_item(root, path, ino, key_type);
	if (ret == 0) {
		struct btrfs_key tmp;
		btrfs_item_key_to_cpu(path->nodes[0], &tmp, path->slots[0]);
		if (key_type == tmp.type) {
			first_offset = tmp.offset;
			ret = overwrite_item(trans, log, dst_path,
					     path->nodes[0], path->slots[0],
					     &tmp);
			if (ret) {
				err = ret;
				goto done;
			}
		}
	}
	btrfs_release_path(path);

	
	ret = btrfs_search_slot(NULL, root, &min_key, path, 0, 0);
	if (ret != 0) {
		WARN_ON(1);
		goto done;
	}

	while (1) {
		struct btrfs_key tmp;
		src = path->nodes[0];
		nritems = btrfs_header_nritems(src);
		for (i = path->slots[0]; i < nritems; i++) {
			btrfs_item_key_to_cpu(src, &min_key, i);

			if (min_key.objectid != ino || min_key.type != key_type)
				goto done;
			ret = overwrite_item(trans, log, dst_path, src, i,
					     &min_key);
			if (ret) {
				err = ret;
				goto done;
			}
		}
		path->slots[0] = nritems;

		ret = btrfs_next_leaf(root, path);
		if (ret == 1) {
			last_offset = (u64)-1;
			goto done;
		}
		btrfs_item_key_to_cpu(path->nodes[0], &tmp, path->slots[0]);
		if (tmp.objectid != ino || tmp.type != key_type) {
			last_offset = (u64)-1;
			goto done;
		}
		if (btrfs_header_generation(path->nodes[0]) != trans->transid) {
			ret = overwrite_item(trans, log, dst_path,
					     path->nodes[0], path->slots[0],
					     &tmp);
			if (ret)
				err = ret;
			else
				last_offset = tmp.offset;
			goto done;
		}
	}
done:
	btrfs_release_path(path);
	btrfs_release_path(dst_path);

	if (err == 0) {
		*last_offset_ret = last_offset;
		ret = insert_dir_log_key(trans, log, path, key_type,
					 ino, first_offset, last_offset);
		if (ret)
			err = ret;
	}
	return err;
}

static noinline int log_directory_changes(struct btrfs_trans_handle *trans,
			  struct btrfs_root *root, struct inode *inode,
			  struct btrfs_path *path,
			  struct btrfs_path *dst_path)
{
	u64 min_key;
	u64 max_key;
	int ret;
	int key_type = BTRFS_DIR_ITEM_KEY;

again:
	min_key = 0;
	max_key = 0;
	while (1) {
		ret = log_dir_items(trans, root, inode, path,
				    dst_path, key_type, min_key,
				    &max_key);
		if (ret)
			return ret;
		if (max_key == (u64)-1)
			break;
		min_key = max_key + 1;
	}

	if (key_type == BTRFS_DIR_ITEM_KEY) {
		key_type = BTRFS_DIR_INDEX_KEY;
		goto again;
	}
	return 0;
}

static int drop_objectid_items(struct btrfs_trans_handle *trans,
				  struct btrfs_root *log,
				  struct btrfs_path *path,
				  u64 objectid, int max_key_type)
{
	int ret;
	struct btrfs_key key;
	struct btrfs_key found_key;

	key.objectid = objectid;
	key.type = max_key_type;
	key.offset = (u64)-1;

	while (1) {
		ret = btrfs_search_slot(trans, log, &key, path, -1, 1);
		BUG_ON(ret == 0);
		if (ret < 0)
			break;

		if (path->slots[0] == 0)
			break;

		path->slots[0]--;
		btrfs_item_key_to_cpu(path->nodes[0], &found_key,
				      path->slots[0]);

		if (found_key.objectid != objectid)
			break;

		ret = btrfs_del_item(trans, log, path);
		if (ret)
			break;
		btrfs_release_path(path);
	}
	btrfs_release_path(path);
	return ret;
}

static noinline int copy_items(struct btrfs_trans_handle *trans,
			       struct btrfs_root *log,
			       struct btrfs_path *dst_path,
			       struct extent_buffer *src,
			       int start_slot, int nr, int inode_only)
{
	unsigned long src_offset;
	unsigned long dst_offset;
	struct btrfs_file_extent_item *extent;
	struct btrfs_inode_item *inode_item;
	int ret;
	struct btrfs_key *ins_keys;
	u32 *ins_sizes;
	char *ins_data;
	int i;
	struct list_head ordered_sums;

	INIT_LIST_HEAD(&ordered_sums);

	ins_data = kmalloc(nr * sizeof(struct btrfs_key) +
			   nr * sizeof(u32), GFP_NOFS);
	if (!ins_data)
		return -ENOMEM;

	ins_sizes = (u32 *)ins_data;
	ins_keys = (struct btrfs_key *)(ins_data + nr * sizeof(u32));

	for (i = 0; i < nr; i++) {
		ins_sizes[i] = btrfs_item_size_nr(src, i + start_slot);
		btrfs_item_key_to_cpu(src, ins_keys + i, i + start_slot);
	}
	ret = btrfs_insert_empty_items(trans, log, dst_path,
				       ins_keys, ins_sizes, nr);
	if (ret) {
		kfree(ins_data);
		return ret;
	}

	for (i = 0; i < nr; i++, dst_path->slots[0]++) {
		dst_offset = btrfs_item_ptr_offset(dst_path->nodes[0],
						   dst_path->slots[0]);

		src_offset = btrfs_item_ptr_offset(src, start_slot + i);

		copy_extent_buffer(dst_path->nodes[0], src, dst_offset,
				   src_offset, ins_sizes[i]);

		if (inode_only == LOG_INODE_EXISTS &&
		    ins_keys[i].type == BTRFS_INODE_ITEM_KEY) {
			inode_item = btrfs_item_ptr(dst_path->nodes[0],
						    dst_path->slots[0],
						    struct btrfs_inode_item);
			btrfs_set_inode_size(dst_path->nodes[0], inode_item, 0);

			btrfs_set_inode_generation(dst_path->nodes[0],
						   inode_item, 0);
		}
		if (btrfs_key_type(ins_keys + i) == BTRFS_EXTENT_DATA_KEY) {
			int found_type;
			extent = btrfs_item_ptr(src, start_slot + i,
						struct btrfs_file_extent_item);

			if (btrfs_file_extent_generation(src, extent) < trans->transid)
				continue;

			found_type = btrfs_file_extent_type(src, extent);
			if (found_type == BTRFS_FILE_EXTENT_REG ||
			    found_type == BTRFS_FILE_EXTENT_PREALLOC) {
				u64 ds, dl, cs, cl;
				ds = btrfs_file_extent_disk_bytenr(src,
								extent);
				
				if (ds == 0)
					continue;

				dl = btrfs_file_extent_disk_num_bytes(src,
								extent);
				cs = btrfs_file_extent_offset(src, extent);
				cl = btrfs_file_extent_num_bytes(src,
								extent);
				if (btrfs_file_extent_compression(src,
								  extent)) {
					cs = 0;
					cl = dl;
				}

				ret = btrfs_lookup_csums_range(
						log->fs_info->csum_root,
						ds + cs, ds + cs + cl - 1,
						&ordered_sums, 0);
				BUG_ON(ret);
			}
		}
	}

	btrfs_mark_buffer_dirty(dst_path->nodes[0]);
	btrfs_release_path(dst_path);
	kfree(ins_data);

	ret = 0;
	while (!list_empty(&ordered_sums)) {
		struct btrfs_ordered_sum *sums = list_entry(ordered_sums.next,
						   struct btrfs_ordered_sum,
						   list);
		if (!ret)
			ret = btrfs_csum_file_blocks(trans, log, sums);
		list_del(&sums->list);
		kfree(sums);
	}
	return ret;
}

static int btrfs_log_inode(struct btrfs_trans_handle *trans,
			     struct btrfs_root *root, struct inode *inode,
			     int inode_only)
{
	struct btrfs_path *path;
	struct btrfs_path *dst_path;
	struct btrfs_key min_key;
	struct btrfs_key max_key;
	struct btrfs_root *log = root->log_root;
	struct extent_buffer *src = NULL;
	int err = 0;
	int ret;
	int nritems;
	int ins_start_slot = 0;
	int ins_nr;
	u64 ino = btrfs_ino(inode);

	log = root->log_root;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;
	dst_path = btrfs_alloc_path();
	if (!dst_path) {
		btrfs_free_path(path);
		return -ENOMEM;
	}

	min_key.objectid = ino;
	min_key.type = BTRFS_INODE_ITEM_KEY;
	min_key.offset = 0;

	max_key.objectid = ino;

	
	if (!S_ISDIR(inode->i_mode))
	    inode_only = LOG_INODE_ALL;

	if (inode_only == LOG_INODE_EXISTS || S_ISDIR(inode->i_mode))
		max_key.type = BTRFS_XATTR_ITEM_KEY;
	else
		max_key.type = (u8)-1;
	max_key.offset = (u64)-1;

	ret = btrfs_commit_inode_delayed_items(trans, inode);
	if (ret) {
		btrfs_free_path(path);
		btrfs_free_path(dst_path);
		return ret;
	}

	mutex_lock(&BTRFS_I(inode)->log_mutex);

	if (S_ISDIR(inode->i_mode)) {
		int max_key_type = BTRFS_DIR_LOG_INDEX_KEY;

		if (inode_only == LOG_INODE_EXISTS)
			max_key_type = BTRFS_XATTR_ITEM_KEY;
		ret = drop_objectid_items(trans, log, path, ino, max_key_type);
	} else {
		ret = btrfs_truncate_inode_items(trans, log, inode, 0, 0);
	}
	if (ret) {
		err = ret;
		goto out_unlock;
	}
	path->keep_locks = 1;

	while (1) {
		ins_nr = 0;
		ret = btrfs_search_forward(root, &min_key, &max_key,
					   path, 0, trans->transid);
		if (ret != 0)
			break;
again:
		
		if (min_key.objectid != ino)
			break;
		if (min_key.type > max_key.type)
			break;

		src = path->nodes[0];
		if (ins_nr && ins_start_slot + ins_nr == path->slots[0]) {
			ins_nr++;
			goto next_slot;
		} else if (!ins_nr) {
			ins_start_slot = path->slots[0];
			ins_nr = 1;
			goto next_slot;
		}

		ret = copy_items(trans, log, dst_path, src, ins_start_slot,
				 ins_nr, inode_only);
		if (ret) {
			err = ret;
			goto out_unlock;
		}
		ins_nr = 1;
		ins_start_slot = path->slots[0];
next_slot:

		nritems = btrfs_header_nritems(path->nodes[0]);
		path->slots[0]++;
		if (path->slots[0] < nritems) {
			btrfs_item_key_to_cpu(path->nodes[0], &min_key,
					      path->slots[0]);
			goto again;
		}
		if (ins_nr) {
			ret = copy_items(trans, log, dst_path, src,
					 ins_start_slot,
					 ins_nr, inode_only);
			if (ret) {
				err = ret;
				goto out_unlock;
			}
			ins_nr = 0;
		}
		btrfs_release_path(path);

		if (min_key.offset < (u64)-1)
			min_key.offset++;
		else if (min_key.type < (u8)-1)
			min_key.type++;
		else if (min_key.objectid < (u64)-1)
			min_key.objectid++;
		else
			break;
	}
	if (ins_nr) {
		ret = copy_items(trans, log, dst_path, src,
				 ins_start_slot,
				 ins_nr, inode_only);
		if (ret) {
			err = ret;
			goto out_unlock;
		}
		ins_nr = 0;
	}
	WARN_ON(ins_nr);
	if (inode_only == LOG_INODE_ALL && S_ISDIR(inode->i_mode)) {
		btrfs_release_path(path);
		btrfs_release_path(dst_path);
		ret = log_directory_changes(trans, root, inode, path, dst_path);
		if (ret) {
			err = ret;
			goto out_unlock;
		}
	}
	BTRFS_I(inode)->logged_trans = trans->transid;
out_unlock:
	mutex_unlock(&BTRFS_I(inode)->log_mutex);

	btrfs_free_path(path);
	btrfs_free_path(dst_path);
	return err;
}

static noinline int check_parent_dirs_for_sync(struct btrfs_trans_handle *trans,
					       struct inode *inode,
					       struct dentry *parent,
					       struct super_block *sb,
					       u64 last_committed)
{
	int ret = 0;
	struct btrfs_root *root;
	struct dentry *old_parent = NULL;

	if (S_ISREG(inode->i_mode) &&
	    BTRFS_I(inode)->generation <= last_committed &&
	    BTRFS_I(inode)->last_unlink_trans <= last_committed)
			goto out;

	if (!S_ISDIR(inode->i_mode)) {
		if (!parent || !parent->d_inode || sb != parent->d_inode->i_sb)
			goto out;
		inode = parent->d_inode;
	}

	while (1) {
		BTRFS_I(inode)->logged_trans = trans->transid;
		smp_mb();

		if (BTRFS_I(inode)->last_unlink_trans > last_committed) {
			root = BTRFS_I(inode)->root;

			root->fs_info->last_trans_log_full_commit =
				trans->transid;
			ret = 1;
			break;
		}

		if (!parent || !parent->d_inode || sb != parent->d_inode->i_sb)
			break;

		if (IS_ROOT(parent))
			break;

		parent = dget_parent(parent);
		dput(old_parent);
		old_parent = parent;
		inode = parent->d_inode;

	}
	dput(old_parent);
out:
	return ret;
}

static int inode_in_log(struct btrfs_trans_handle *trans,
		 struct inode *inode)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	int ret = 0;

	mutex_lock(&root->log_mutex);
	if (BTRFS_I(inode)->logged_trans == trans->transid &&
	    BTRFS_I(inode)->last_sub_trans <= root->last_log_commit)
		ret = 1;
	mutex_unlock(&root->log_mutex);
	return ret;
}


int btrfs_log_inode_parent(struct btrfs_trans_handle *trans,
		    struct btrfs_root *root, struct inode *inode,
		    struct dentry *parent, int exists_only)
{
	int inode_only = exists_only ? LOG_INODE_EXISTS : LOG_INODE_ALL;
	struct super_block *sb;
	struct dentry *old_parent = NULL;
	int ret = 0;
	u64 last_committed = root->fs_info->last_trans_committed;

	sb = inode->i_sb;

	if (btrfs_test_opt(root, NOTREELOG)) {
		ret = 1;
		goto end_no_trans;
	}

	if (root->fs_info->last_trans_log_full_commit >
	    root->fs_info->last_trans_committed) {
		ret = 1;
		goto end_no_trans;
	}

	if (root != BTRFS_I(inode)->root ||
	    btrfs_root_refs(&root->root_item) == 0) {
		ret = 1;
		goto end_no_trans;
	}

	ret = check_parent_dirs_for_sync(trans, inode, parent,
					 sb, last_committed);
	if (ret)
		goto end_no_trans;

	if (inode_in_log(trans, inode)) {
		ret = BTRFS_NO_LOG_SYNC;
		goto end_no_trans;
	}

	ret = start_log_trans(trans, root);
	if (ret)
		goto end_trans;

	ret = btrfs_log_inode(trans, root, inode, inode_only);
	if (ret)
		goto end_trans;

	if (S_ISREG(inode->i_mode) &&
	    BTRFS_I(inode)->generation <= last_committed &&
	    BTRFS_I(inode)->last_unlink_trans <= last_committed) {
		ret = 0;
		goto end_trans;
	}

	inode_only = LOG_INODE_EXISTS;
	while (1) {
		if (!parent || !parent->d_inode || sb != parent->d_inode->i_sb)
			break;

		inode = parent->d_inode;
		if (root != BTRFS_I(inode)->root)
			break;

		if (BTRFS_I(inode)->generation >
		    root->fs_info->last_trans_committed) {
			ret = btrfs_log_inode(trans, root, inode, inode_only);
			if (ret)
				goto end_trans;
		}
		if (IS_ROOT(parent))
			break;

		parent = dget_parent(parent);
		dput(old_parent);
		old_parent = parent;
	}
	ret = 0;
end_trans:
	dput(old_parent);
	if (ret < 0) {
		BUG_ON(ret != -ENOSPC);
		root->fs_info->last_trans_log_full_commit = trans->transid;
		ret = 1;
	}
	btrfs_end_log_trans(root);
end_no_trans:
	return ret;
}

int btrfs_log_dentry_safe(struct btrfs_trans_handle *trans,
			  struct btrfs_root *root, struct dentry *dentry)
{
	struct dentry *parent = dget_parent(dentry);
	int ret;

	ret = btrfs_log_inode_parent(trans, root, dentry->d_inode, parent, 0);
	dput(parent);

	return ret;
}

int btrfs_recover_log_trees(struct btrfs_root *log_root_tree)
{
	int ret;
	struct btrfs_path *path;
	struct btrfs_trans_handle *trans;
	struct btrfs_key key;
	struct btrfs_key found_key;
	struct btrfs_key tmp_key;
	struct btrfs_root *log;
	struct btrfs_fs_info *fs_info = log_root_tree->fs_info;
	struct walk_control wc = {
		.process_func = process_one_buffer,
		.stage = 0,
	};

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	fs_info->log_root_recovering = 1;

	trans = btrfs_start_transaction(fs_info->tree_root, 0);
	if (IS_ERR(trans)) {
		ret = PTR_ERR(trans);
		goto error;
	}

	wc.trans = trans;
	wc.pin = 1;

	ret = walk_log_tree(trans, log_root_tree, &wc);
	if (ret) {
		btrfs_error(fs_info, ret, "Failed to pin buffers while "
			    "recovering log root tree.");
		goto error;
	}

again:
	key.objectid = BTRFS_TREE_LOG_OBJECTID;
	key.offset = (u64)-1;
	btrfs_set_key_type(&key, BTRFS_ROOT_ITEM_KEY);

	while (1) {
		ret = btrfs_search_slot(NULL, log_root_tree, &key, path, 0, 0);

		if (ret < 0) {
			btrfs_error(fs_info, ret,
				    "Couldn't find tree log root.");
			goto error;
		}
		if (ret > 0) {
			if (path->slots[0] == 0)
				break;
			path->slots[0]--;
		}
		btrfs_item_key_to_cpu(path->nodes[0], &found_key,
				      path->slots[0]);
		btrfs_release_path(path);
		if (found_key.objectid != BTRFS_TREE_LOG_OBJECTID)
			break;

		log = btrfs_read_fs_root_no_radix(log_root_tree,
						  &found_key);
		if (IS_ERR(log)) {
			ret = PTR_ERR(log);
			btrfs_error(fs_info, ret,
				    "Couldn't read tree log root.");
			goto error;
		}

		tmp_key.objectid = found_key.offset;
		tmp_key.type = BTRFS_ROOT_ITEM_KEY;
		tmp_key.offset = (u64)-1;

		wc.replay_dest = btrfs_read_fs_root_no_name(fs_info, &tmp_key);
		if (IS_ERR(wc.replay_dest)) {
			ret = PTR_ERR(wc.replay_dest);
			btrfs_error(fs_info, ret, "Couldn't read target root "
				    "for tree log recovery.");
			goto error;
		}

		wc.replay_dest->log_root = log;
		btrfs_record_root_in_trans(trans, wc.replay_dest);
		ret = walk_log_tree(trans, log, &wc);
		BUG_ON(ret);

		if (wc.stage == LOG_WALK_REPLAY_ALL) {
			ret = fixup_inode_link_counts(trans, wc.replay_dest,
						      path);
			BUG_ON(ret);
		}

		key.offset = found_key.offset - 1;
		wc.replay_dest->log_root = NULL;
		free_extent_buffer(log->node);
		free_extent_buffer(log->commit_root);
		kfree(log);

		if (found_key.offset == 0)
			break;
	}
	btrfs_release_path(path);

	
	if (wc.pin) {
		wc.pin = 0;
		wc.process_func = replay_one_buffer;
		wc.stage = LOG_WALK_REPLAY_INODES;
		goto again;
	}
	
	if (wc.stage < LOG_WALK_REPLAY_ALL) {
		wc.stage++;
		goto again;
	}

	btrfs_free_path(path);

	free_extent_buffer(log_root_tree->node);
	log_root_tree->log_root = NULL;
	fs_info->log_root_recovering = 0;

	
	btrfs_commit_transaction(trans, fs_info->tree_root);

	kfree(log_root_tree);
	return 0;

error:
	btrfs_free_path(path);
	return ret;
}

void btrfs_record_unlink_dir(struct btrfs_trans_handle *trans,
			     struct inode *dir, struct inode *inode,
			     int for_rename)
{
	if (S_ISREG(inode->i_mode))
		BTRFS_I(inode)->last_unlink_trans = trans->transid;

	smp_mb();
	if (BTRFS_I(dir)->logged_trans == trans->transid)
		return;

	if (BTRFS_I(inode)->logged_trans == trans->transid)
		return;

	if (for_rename)
		goto record;

	
	return;

record:
	BTRFS_I(dir)->last_unlink_trans = trans->transid;
}

int btrfs_log_new_name(struct btrfs_trans_handle *trans,
			struct inode *inode, struct inode *old_dir,
			struct dentry *parent)
{
	struct btrfs_root * root = BTRFS_I(inode)->root;

	if (S_ISREG(inode->i_mode))
		BTRFS_I(inode)->last_unlink_trans = trans->transid;

	if (BTRFS_I(inode)->logged_trans <=
	    root->fs_info->last_trans_committed &&
	    (!old_dir || BTRFS_I(old_dir)->logged_trans <=
		    root->fs_info->last_trans_committed))
		return 0;

	return btrfs_log_inode_parent(trans, root, inode, parent, 1);
}

