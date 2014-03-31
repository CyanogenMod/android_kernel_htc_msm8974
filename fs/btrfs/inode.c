/*
 * Copyright (C) 2007 Oracle.  All rights reserved.
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

#include <linux/kernel.h>
#include <linux/bio.h>
#include <linux/buffer_head.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/highmem.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/backing-dev.h>
#include <linux/mpage.h>
#include <linux/swap.h>
#include <linux/writeback.h>
#include <linux/statfs.h>
#include <linux/compat.h>
#include <linux/bit_spinlock.h>
#include <linux/xattr.h>
#include <linux/posix_acl.h>
#include <linux/falloc.h>
#include <linux/slab.h>
#include <linux/ratelimit.h>
#include <linux/mount.h>
#include "compat.h"
#include "ctree.h"
#include "disk-io.h"
#include "transaction.h"
#include "btrfs_inode.h"
#include "ioctl.h"
#include "print-tree.h"
#include "ordered-data.h"
#include "xattr.h"
#include "tree-log.h"
#include "volumes.h"
#include "compression.h"
#include "locking.h"
#include "free-space-cache.h"
#include "inode-map.h"

struct btrfs_iget_args {
	u64 ino;
	struct btrfs_root *root;
};

static const struct inode_operations btrfs_dir_inode_operations;
static const struct inode_operations btrfs_symlink_inode_operations;
static const struct inode_operations btrfs_dir_ro_inode_operations;
static const struct inode_operations btrfs_special_inode_operations;
static const struct inode_operations btrfs_file_inode_operations;
static const struct address_space_operations btrfs_aops;
static const struct address_space_operations btrfs_symlink_aops;
static const struct file_operations btrfs_dir_file_operations;
static struct extent_io_ops btrfs_extent_io_ops;

static struct kmem_cache *btrfs_inode_cachep;
struct kmem_cache *btrfs_trans_handle_cachep;
struct kmem_cache *btrfs_transaction_cachep;
struct kmem_cache *btrfs_path_cachep;
struct kmem_cache *btrfs_free_space_cachep;

#define S_SHIFT 12
static unsigned char btrfs_type_by_mode[S_IFMT >> S_SHIFT] = {
	[S_IFREG >> S_SHIFT]	= BTRFS_FT_REG_FILE,
	[S_IFDIR >> S_SHIFT]	= BTRFS_FT_DIR,
	[S_IFCHR >> S_SHIFT]	= BTRFS_FT_CHRDEV,
	[S_IFBLK >> S_SHIFT]	= BTRFS_FT_BLKDEV,
	[S_IFIFO >> S_SHIFT]	= BTRFS_FT_FIFO,
	[S_IFSOCK >> S_SHIFT]	= BTRFS_FT_SOCK,
	[S_IFLNK >> S_SHIFT]	= BTRFS_FT_SYMLINK,
};

static int btrfs_setsize(struct inode *inode, loff_t newsize);
static int btrfs_truncate(struct inode *inode);
static int btrfs_finish_ordered_io(struct inode *inode, u64 start, u64 end);
static noinline int cow_file_range(struct inode *inode,
				   struct page *locked_page,
				   u64 start, u64 end, int *page_started,
				   unsigned long *nr_written, int unlock);
static noinline int btrfs_update_inode_fallback(struct btrfs_trans_handle *trans,
				struct btrfs_root *root, struct inode *inode);

static int btrfs_init_inode_security(struct btrfs_trans_handle *trans,
				     struct inode *inode,  struct inode *dir,
				     const struct qstr *qstr)
{
	int err;

	err = btrfs_init_acl(trans, inode, dir);
	if (!err)
		err = btrfs_xattr_security_init(trans, inode, dir, qstr);
	return err;
}

static noinline int insert_inline_extent(struct btrfs_trans_handle *trans,
				struct btrfs_root *root, struct inode *inode,
				u64 start, size_t size, size_t compressed_size,
				int compress_type,
				struct page **compressed_pages)
{
	struct btrfs_key key;
	struct btrfs_path *path;
	struct extent_buffer *leaf;
	struct page *page = NULL;
	char *kaddr;
	unsigned long ptr;
	struct btrfs_file_extent_item *ei;
	int err = 0;
	int ret;
	size_t cur_size = size;
	size_t datasize;
	unsigned long offset;

	if (compressed_size && compressed_pages)
		cur_size = compressed_size;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	path->leave_spinning = 1;

	key.objectid = btrfs_ino(inode);
	key.offset = start;
	btrfs_set_key_type(&key, BTRFS_EXTENT_DATA_KEY);
	datasize = btrfs_file_extent_calc_inline_size(cur_size);

	inode_add_bytes(inode, size);
	ret = btrfs_insert_empty_item(trans, root, path, &key,
				      datasize);
	if (ret) {
		err = ret;
		goto fail;
	}
	leaf = path->nodes[0];
	ei = btrfs_item_ptr(leaf, path->slots[0],
			    struct btrfs_file_extent_item);
	btrfs_set_file_extent_generation(leaf, ei, trans->transid);
	btrfs_set_file_extent_type(leaf, ei, BTRFS_FILE_EXTENT_INLINE);
	btrfs_set_file_extent_encryption(leaf, ei, 0);
	btrfs_set_file_extent_other_encoding(leaf, ei, 0);
	btrfs_set_file_extent_ram_bytes(leaf, ei, size);
	ptr = btrfs_file_extent_inline_start(ei);

	if (compress_type != BTRFS_COMPRESS_NONE) {
		struct page *cpage;
		int i = 0;
		while (compressed_size > 0) {
			cpage = compressed_pages[i];
			cur_size = min_t(unsigned long, compressed_size,
				       PAGE_CACHE_SIZE);

			kaddr = kmap_atomic(cpage);
			write_extent_buffer(leaf, kaddr, ptr, cur_size);
			kunmap_atomic(kaddr);

			i++;
			ptr += cur_size;
			compressed_size -= cur_size;
		}
		btrfs_set_file_extent_compression(leaf, ei,
						  compress_type);
	} else {
		page = find_get_page(inode->i_mapping,
				     start >> PAGE_CACHE_SHIFT);
		btrfs_set_file_extent_compression(leaf, ei, 0);
		kaddr = kmap_atomic(page);
		offset = start & (PAGE_CACHE_SIZE - 1);
		write_extent_buffer(leaf, kaddr + offset, ptr, size);
		kunmap_atomic(kaddr);
		page_cache_release(page);
	}
	btrfs_mark_buffer_dirty(leaf);
	btrfs_free_path(path);

	BTRFS_I(inode)->disk_i_size = inode->i_size;
	ret = btrfs_update_inode(trans, root, inode);

	return ret;
fail:
	btrfs_free_path(path);
	return err;
}


static noinline int cow_file_range_inline(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root,
				 struct inode *inode, u64 start, u64 end,
				 size_t compressed_size, int compress_type,
				 struct page **compressed_pages)
{
	u64 isize = i_size_read(inode);
	u64 actual_end = min(end + 1, isize);
	u64 inline_len = actual_end - start;
	u64 aligned_end = (end + root->sectorsize - 1) &
			~((u64)root->sectorsize - 1);
	u64 hint_byte;
	u64 data_len = inline_len;
	int ret;

	if (compressed_size)
		data_len = compressed_size;

	if (start > 0 ||
	    actual_end >= PAGE_CACHE_SIZE ||
	    data_len >= BTRFS_MAX_INLINE_DATA_SIZE(root) ||
	    (!compressed_size &&
	    (actual_end & (root->sectorsize - 1)) == 0) ||
	    end + 1 < isize ||
	    data_len > root->fs_info->max_inline) {
		return 1;
	}

	ret = btrfs_drop_extents(trans, inode, start, aligned_end,
				 &hint_byte, 1);
	if (ret)
		return ret;

	if (isize > actual_end)
		inline_len = min_t(u64, isize, actual_end);
	ret = insert_inline_extent(trans, root, inode, start,
				   inline_len, compressed_size,
				   compress_type, compressed_pages);
	if (ret) {
		btrfs_abort_transaction(trans, root, ret);
		return ret;
	}
	btrfs_delalloc_release_metadata(inode, end + 1 - start);
	btrfs_drop_extent_cache(inode, start, aligned_end - 1, 0);
	return 0;
}

struct async_extent {
	u64 start;
	u64 ram_size;
	u64 compressed_size;
	struct page **pages;
	unsigned long nr_pages;
	int compress_type;
	struct list_head list;
};

struct async_cow {
	struct inode *inode;
	struct btrfs_root *root;
	struct page *locked_page;
	u64 start;
	u64 end;
	struct list_head extents;
	struct btrfs_work work;
};

static noinline int add_async_extent(struct async_cow *cow,
				     u64 start, u64 ram_size,
				     u64 compressed_size,
				     struct page **pages,
				     unsigned long nr_pages,
				     int compress_type)
{
	struct async_extent *async_extent;

	async_extent = kmalloc(sizeof(*async_extent), GFP_NOFS);
	BUG_ON(!async_extent); 
	async_extent->start = start;
	async_extent->ram_size = ram_size;
	async_extent->compressed_size = compressed_size;
	async_extent->pages = pages;
	async_extent->nr_pages = nr_pages;
	async_extent->compress_type = compress_type;
	list_add_tail(&async_extent->list, &cow->extents);
	return 0;
}

/*
 * we create compressed extents in two phases.  The first
 * phase compresses a range of pages that have already been
 * locked (both pages and state bits are locked).
 *
 * This is done inside an ordered work queue, and the compression
 * is spread across many cpus.  The actual IO submission is step
 * two, and the ordered work queue takes care of making sure that
 * happens in the same order things were put onto the queue by
 * writepages and friends.
 *
 * If this code finds it can't get good compression, it puts an
 * entry onto the work queue to write the uncompressed bytes.  This
 * makes sure that both compressed inodes and uncompressed inodes
 * are written in the same order that pdflush sent them down.
 */
static noinline int compress_file_range(struct inode *inode,
					struct page *locked_page,
					u64 start, u64 end,
					struct async_cow *async_cow,
					int *num_added)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_trans_handle *trans;
	u64 num_bytes;
	u64 blocksize = root->sectorsize;
	u64 actual_end;
	u64 isize = i_size_read(inode);
	int ret = 0;
	struct page **pages = NULL;
	unsigned long nr_pages;
	unsigned long nr_pages_ret = 0;
	unsigned long total_compressed = 0;
	unsigned long total_in = 0;
	unsigned long max_compressed = 128 * 1024;
	unsigned long max_uncompressed = 128 * 1024;
	int i;
	int will_compress;
	int compress_type = root->fs_info->compress_type;

	
	if ((end - start + 1) < 16 * 1024 &&
	    (start > 0 || end + 1 < BTRFS_I(inode)->disk_i_size))
		btrfs_add_inode_defrag(NULL, inode);

	actual_end = min_t(u64, isize, end + 1);
again:
	will_compress = 0;
	nr_pages = (end >> PAGE_CACHE_SHIFT) - (start >> PAGE_CACHE_SHIFT) + 1;
	nr_pages = min(nr_pages, (128 * 1024UL) / PAGE_CACHE_SIZE);

	if (actual_end <= start)
		goto cleanup_and_bail_uncompressed;

	total_compressed = actual_end - start;

	total_compressed = min(total_compressed, max_uncompressed);
	num_bytes = (end - start + blocksize) & ~(blocksize - 1);
	num_bytes = max(blocksize,  num_bytes);
	total_in = 0;
	ret = 0;

	if (!(BTRFS_I(inode)->flags & BTRFS_INODE_NOCOMPRESS) &&
	    (btrfs_test_opt(root, COMPRESS) ||
	     (BTRFS_I(inode)->force_compress) ||
	     (BTRFS_I(inode)->flags & BTRFS_INODE_COMPRESS))) {
		WARN_ON(pages);
		pages = kzalloc(sizeof(struct page *) * nr_pages, GFP_NOFS);
		if (!pages) {
			
			goto cont;
		}

		if (BTRFS_I(inode)->force_compress)
			compress_type = BTRFS_I(inode)->force_compress;

		ret = btrfs_compress_pages(compress_type,
					   inode->i_mapping, start,
					   total_compressed, pages,
					   nr_pages, &nr_pages_ret,
					   &total_in,
					   &total_compressed,
					   max_compressed);

		if (!ret) {
			unsigned long offset = total_compressed &
				(PAGE_CACHE_SIZE - 1);
			struct page *page = pages[nr_pages_ret - 1];
			char *kaddr;

			if (offset) {
				kaddr = kmap_atomic(page);
				memset(kaddr + offset, 0,
				       PAGE_CACHE_SIZE - offset);
				kunmap_atomic(kaddr);
			}
			will_compress = 1;
		}
	}
cont:
	if (start == 0) {
		trans = btrfs_join_transaction(root);
		if (IS_ERR(trans)) {
			ret = PTR_ERR(trans);
			trans = NULL;
			goto cleanup_and_out;
		}
		trans->block_rsv = &root->fs_info->delalloc_block_rsv;

		
		if (ret || total_in < (actual_end - start)) {
			ret = cow_file_range_inline(trans, root, inode,
						    start, end, 0, 0, NULL);
		} else {
			
			ret = cow_file_range_inline(trans, root, inode,
						    start, end,
						    total_compressed,
						    compress_type, pages);
		}
		if (ret <= 0) {
			extent_clear_unlock_delalloc(inode,
			     &BTRFS_I(inode)->io_tree,
			     start, end, NULL,
			     EXTENT_CLEAR_UNLOCK_PAGE | EXTENT_CLEAR_DIRTY |
			     EXTENT_CLEAR_DELALLOC |
			     EXTENT_SET_WRITEBACK | EXTENT_END_WRITEBACK);

			btrfs_end_transaction(trans, root);
			goto free_pages_out;
		}
		btrfs_end_transaction(trans, root);
	}

	if (will_compress) {
		total_compressed = (total_compressed + blocksize - 1) &
			~(blocksize - 1);

		total_in = (total_in + PAGE_CACHE_SIZE - 1) &
			~(PAGE_CACHE_SIZE - 1);
		if (total_compressed >= total_in) {
			will_compress = 0;
		} else {
			num_bytes = total_in;
		}
	}
	if (!will_compress && pages) {
		for (i = 0; i < nr_pages_ret; i++) {
			WARN_ON(pages[i]->mapping);
			page_cache_release(pages[i]);
		}
		kfree(pages);
		pages = NULL;
		total_compressed = 0;
		nr_pages_ret = 0;

		
		if (!btrfs_test_opt(root, FORCE_COMPRESS) &&
		    !(BTRFS_I(inode)->force_compress)) {
			BTRFS_I(inode)->flags |= BTRFS_INODE_NOCOMPRESS;
		}
	}
	if (will_compress) {
		*num_added += 1;

		add_async_extent(async_cow, start, num_bytes,
				 total_compressed, pages, nr_pages_ret,
				 compress_type);

		if (start + num_bytes < end) {
			start += num_bytes;
			pages = NULL;
			cond_resched();
			goto again;
		}
	} else {
cleanup_and_bail_uncompressed:
		if (page_offset(locked_page) >= start &&
		    page_offset(locked_page) <= end) {
			__set_page_dirty_nobuffers(locked_page);
			
		}
		add_async_extent(async_cow, start, end - start + 1,
				 0, NULL, 0, BTRFS_COMPRESS_NONE);
		*num_added += 1;
	}

out:
	return ret;

free_pages_out:
	for (i = 0; i < nr_pages_ret; i++) {
		WARN_ON(pages[i]->mapping);
		page_cache_release(pages[i]);
	}
	kfree(pages);

	goto out;

cleanup_and_out:
	extent_clear_unlock_delalloc(inode, &BTRFS_I(inode)->io_tree,
				     start, end, NULL,
				     EXTENT_CLEAR_UNLOCK_PAGE |
				     EXTENT_CLEAR_DIRTY |
				     EXTENT_CLEAR_DELALLOC |
				     EXTENT_SET_WRITEBACK |
				     EXTENT_END_WRITEBACK);
	if (!trans || IS_ERR(trans))
		btrfs_error(root->fs_info, ret, "Failed to join transaction");
	else
		btrfs_abort_transaction(trans, root, ret);
	goto free_pages_out;
}

static noinline int submit_compressed_extents(struct inode *inode,
					      struct async_cow *async_cow)
{
	struct async_extent *async_extent;
	u64 alloc_hint = 0;
	struct btrfs_trans_handle *trans;
	struct btrfs_key ins;
	struct extent_map *em;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct extent_map_tree *em_tree = &BTRFS_I(inode)->extent_tree;
	struct extent_io_tree *io_tree;
	int ret = 0;

	if (list_empty(&async_cow->extents))
		return 0;


	while (!list_empty(&async_cow->extents)) {
		async_extent = list_entry(async_cow->extents.next,
					  struct async_extent, list);
		list_del(&async_extent->list);

		io_tree = &BTRFS_I(inode)->io_tree;

retry:
		
		if (!async_extent->pages) {
			int page_started = 0;
			unsigned long nr_written = 0;

			lock_extent(io_tree, async_extent->start,
					 async_extent->start +
					 async_extent->ram_size - 1);

			
			ret = cow_file_range(inode, async_cow->locked_page,
					     async_extent->start,
					     async_extent->start +
					     async_extent->ram_size - 1,
					     &page_started, &nr_written, 0);

			

			if (!page_started && !ret)
				extent_write_locked_range(io_tree,
						  inode, async_extent->start,
						  async_extent->start +
						  async_extent->ram_size - 1,
						  btrfs_get_extent,
						  WB_SYNC_ALL);
			kfree(async_extent);
			cond_resched();
			continue;
		}

		lock_extent(io_tree, async_extent->start,
			    async_extent->start + async_extent->ram_size - 1);

		trans = btrfs_join_transaction(root);
		if (IS_ERR(trans)) {
			ret = PTR_ERR(trans);
		} else {
			trans->block_rsv = &root->fs_info->delalloc_block_rsv;
			ret = btrfs_reserve_extent(trans, root,
					   async_extent->compressed_size,
					   async_extent->compressed_size,
					   0, alloc_hint, &ins, 1);
			if (ret)
				btrfs_abort_transaction(trans, root, ret);
			btrfs_end_transaction(trans, root);
		}

		if (ret) {
			int i;
			for (i = 0; i < async_extent->nr_pages; i++) {
				WARN_ON(async_extent->pages[i]->mapping);
				page_cache_release(async_extent->pages[i]);
			}
			kfree(async_extent->pages);
			async_extent->nr_pages = 0;
			async_extent->pages = NULL;
			unlock_extent(io_tree, async_extent->start,
				      async_extent->start +
				      async_extent->ram_size - 1);
			if (ret == -ENOSPC)
				goto retry;
			goto out_free; 
		}

		btrfs_drop_extent_cache(inode, async_extent->start,
					async_extent->start +
					async_extent->ram_size - 1, 0);

		em = alloc_extent_map();
		BUG_ON(!em); 
		em->start = async_extent->start;
		em->len = async_extent->ram_size;
		em->orig_start = em->start;

		em->block_start = ins.objectid;
		em->block_len = ins.offset;
		em->bdev = root->fs_info->fs_devices->latest_bdev;
		em->compress_type = async_extent->compress_type;
		set_bit(EXTENT_FLAG_PINNED, &em->flags);
		set_bit(EXTENT_FLAG_COMPRESSED, &em->flags);

		while (1) {
			write_lock(&em_tree->lock);
			ret = add_extent_mapping(em_tree, em);
			write_unlock(&em_tree->lock);
			if (ret != -EEXIST) {
				free_extent_map(em);
				break;
			}
			btrfs_drop_extent_cache(inode, async_extent->start,
						async_extent->start +
						async_extent->ram_size - 1, 0);
		}

		ret = btrfs_add_ordered_extent_compress(inode,
						async_extent->start,
						ins.objectid,
						async_extent->ram_size,
						ins.offset,
						BTRFS_ORDERED_COMPRESSED,
						async_extent->compress_type);
		BUG_ON(ret); 

		extent_clear_unlock_delalloc(inode,
				&BTRFS_I(inode)->io_tree,
				async_extent->start,
				async_extent->start +
				async_extent->ram_size - 1,
				NULL, EXTENT_CLEAR_UNLOCK_PAGE |
				EXTENT_CLEAR_UNLOCK |
				EXTENT_CLEAR_DELALLOC |
				EXTENT_CLEAR_DIRTY | EXTENT_SET_WRITEBACK);

		ret = btrfs_submit_compressed_write(inode,
				    async_extent->start,
				    async_extent->ram_size,
				    ins.objectid,
				    ins.offset, async_extent->pages,
				    async_extent->nr_pages);

		BUG_ON(ret); 
		alloc_hint = ins.objectid + ins.offset;
		kfree(async_extent);
		cond_resched();
	}
	ret = 0;
out:
	return ret;
out_free:
	kfree(async_extent);
	goto out;
}

static u64 get_extent_allocation_hint(struct inode *inode, u64 start,
				      u64 num_bytes)
{
	struct extent_map_tree *em_tree = &BTRFS_I(inode)->extent_tree;
	struct extent_map *em;
	u64 alloc_hint = 0;

	read_lock(&em_tree->lock);
	em = search_extent_mapping(em_tree, start, num_bytes);
	if (em) {
		if (em->block_start >= EXTENT_MAP_LAST_BYTE) {
			free_extent_map(em);
			em = search_extent_mapping(em_tree, 0, 0);
			if (em && em->block_start < EXTENT_MAP_LAST_BYTE)
				alloc_hint = em->block_start;
			if (em)
				free_extent_map(em);
		} else {
			alloc_hint = em->block_start;
			free_extent_map(em);
		}
	}
	read_unlock(&em_tree->lock);

	return alloc_hint;
}

static noinline int cow_file_range(struct inode *inode,
				   struct page *locked_page,
				   u64 start, u64 end, int *page_started,
				   unsigned long *nr_written,
				   int unlock)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_trans_handle *trans;
	u64 alloc_hint = 0;
	u64 num_bytes;
	unsigned long ram_size;
	u64 disk_num_bytes;
	u64 cur_alloc_size;
	u64 blocksize = root->sectorsize;
	struct btrfs_key ins;
	struct extent_map *em;
	struct extent_map_tree *em_tree = &BTRFS_I(inode)->extent_tree;
	int ret = 0;

	BUG_ON(btrfs_is_free_space_inode(root, inode));
	trans = btrfs_join_transaction(root);
	if (IS_ERR(trans)) {
		extent_clear_unlock_delalloc(inode,
			     &BTRFS_I(inode)->io_tree,
			     start, end, NULL,
			     EXTENT_CLEAR_UNLOCK_PAGE |
			     EXTENT_CLEAR_UNLOCK |
			     EXTENT_CLEAR_DELALLOC |
			     EXTENT_CLEAR_DIRTY |
			     EXTENT_SET_WRITEBACK |
			     EXTENT_END_WRITEBACK);
		return PTR_ERR(trans);
	}
	trans->block_rsv = &root->fs_info->delalloc_block_rsv;

	num_bytes = (end - start + blocksize) & ~(blocksize - 1);
	num_bytes = max(blocksize,  num_bytes);
	disk_num_bytes = num_bytes;
	ret = 0;

	
	if (num_bytes < 64 * 1024 &&
	    (start > 0 || end + 1 < BTRFS_I(inode)->disk_i_size))
		btrfs_add_inode_defrag(trans, inode);

	if (start == 0) {
		
		ret = cow_file_range_inline(trans, root, inode,
					    start, end, 0, 0, NULL);
		if (ret == 0) {
			extent_clear_unlock_delalloc(inode,
				     &BTRFS_I(inode)->io_tree,
				     start, end, NULL,
				     EXTENT_CLEAR_UNLOCK_PAGE |
				     EXTENT_CLEAR_UNLOCK |
				     EXTENT_CLEAR_DELALLOC |
				     EXTENT_CLEAR_DIRTY |
				     EXTENT_SET_WRITEBACK |
				     EXTENT_END_WRITEBACK);

			*nr_written = *nr_written +
			     (end - start + PAGE_CACHE_SIZE) / PAGE_CACHE_SIZE;
			*page_started = 1;
			goto out;
		} else if (ret < 0) {
			btrfs_abort_transaction(trans, root, ret);
			goto out_unlock;
		}
	}

	BUG_ON(disk_num_bytes >
	       btrfs_super_total_bytes(root->fs_info->super_copy));

	alloc_hint = get_extent_allocation_hint(inode, start, num_bytes);
	btrfs_drop_extent_cache(inode, start, start + num_bytes - 1, 0);

	while (disk_num_bytes > 0) {
		unsigned long op;

		cur_alloc_size = disk_num_bytes;
		ret = btrfs_reserve_extent(trans, root, cur_alloc_size,
					   root->sectorsize, 0, alloc_hint,
					   &ins, 1);
		if (ret < 0) {
			btrfs_abort_transaction(trans, root, ret);
			goto out_unlock;
		}

		em = alloc_extent_map();
		BUG_ON(!em); 
		em->start = start;
		em->orig_start = em->start;
		ram_size = ins.offset;
		em->len = ins.offset;

		em->block_start = ins.objectid;
		em->block_len = ins.offset;
		em->bdev = root->fs_info->fs_devices->latest_bdev;
		set_bit(EXTENT_FLAG_PINNED, &em->flags);

		while (1) {
			write_lock(&em_tree->lock);
			ret = add_extent_mapping(em_tree, em);
			write_unlock(&em_tree->lock);
			if (ret != -EEXIST) {
				free_extent_map(em);
				break;
			}
			btrfs_drop_extent_cache(inode, start,
						start + ram_size - 1, 0);
		}

		cur_alloc_size = ins.offset;
		ret = btrfs_add_ordered_extent(inode, start, ins.objectid,
					       ram_size, cur_alloc_size, 0);
		BUG_ON(ret); 

		if (root->root_key.objectid ==
		    BTRFS_DATA_RELOC_TREE_OBJECTID) {
			ret = btrfs_reloc_clone_csums(inode, start,
						      cur_alloc_size);
			if (ret) {
				btrfs_abort_transaction(trans, root, ret);
				goto out_unlock;
			}
		}

		if (disk_num_bytes < cur_alloc_size)
			break;

		op = unlock ? EXTENT_CLEAR_UNLOCK_PAGE : 0;
		op |= EXTENT_CLEAR_UNLOCK | EXTENT_CLEAR_DELALLOC |
			EXTENT_SET_PRIVATE2;

		extent_clear_unlock_delalloc(inode, &BTRFS_I(inode)->io_tree,
					     start, start + ram_size - 1,
					     locked_page, op);
		disk_num_bytes -= cur_alloc_size;
		num_bytes -= cur_alloc_size;
		alloc_hint = ins.objectid + ins.offset;
		start += cur_alloc_size;
	}
	ret = 0;
out:
	btrfs_end_transaction(trans, root);

	return ret;
out_unlock:
	extent_clear_unlock_delalloc(inode,
		     &BTRFS_I(inode)->io_tree,
		     start, end, NULL,
		     EXTENT_CLEAR_UNLOCK_PAGE |
		     EXTENT_CLEAR_UNLOCK |
		     EXTENT_CLEAR_DELALLOC |
		     EXTENT_CLEAR_DIRTY |
		     EXTENT_SET_WRITEBACK |
		     EXTENT_END_WRITEBACK);

	goto out;
}

static noinline void async_cow_start(struct btrfs_work *work)
{
	struct async_cow *async_cow;
	int num_added = 0;
	async_cow = container_of(work, struct async_cow, work);

	compress_file_range(async_cow->inode, async_cow->locked_page,
			    async_cow->start, async_cow->end, async_cow,
			    &num_added);
	if (num_added == 0)
		async_cow->inode = NULL;
}

static noinline void async_cow_submit(struct btrfs_work *work)
{
	struct async_cow *async_cow;
	struct btrfs_root *root;
	unsigned long nr_pages;

	async_cow = container_of(work, struct async_cow, work);

	root = async_cow->root;
	nr_pages = (async_cow->end - async_cow->start + PAGE_CACHE_SIZE) >>
		PAGE_CACHE_SHIFT;

	atomic_sub(nr_pages, &root->fs_info->async_delalloc_pages);

	if (atomic_read(&root->fs_info->async_delalloc_pages) <
	    5 * 1042 * 1024 &&
	    waitqueue_active(&root->fs_info->async_submit_wait))
		wake_up(&root->fs_info->async_submit_wait);

	if (async_cow->inode)
		submit_compressed_extents(async_cow->inode, async_cow);
}

static noinline void async_cow_free(struct btrfs_work *work)
{
	struct async_cow *async_cow;
	async_cow = container_of(work, struct async_cow, work);
	kfree(async_cow);
}

static int cow_file_range_async(struct inode *inode, struct page *locked_page,
				u64 start, u64 end, int *page_started,
				unsigned long *nr_written)
{
	struct async_cow *async_cow;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	unsigned long nr_pages;
	u64 cur_end;
	int limit = 10 * 1024 * 1042;

	clear_extent_bit(&BTRFS_I(inode)->io_tree, start, end, EXTENT_LOCKED,
			 1, 0, NULL, GFP_NOFS);
	while (start < end) {
		async_cow = kmalloc(sizeof(*async_cow), GFP_NOFS);
		BUG_ON(!async_cow); 
		async_cow->inode = inode;
		async_cow->root = root;
		async_cow->locked_page = locked_page;
		async_cow->start = start;

		if (BTRFS_I(inode)->flags & BTRFS_INODE_NOCOMPRESS)
			cur_end = end;
		else
			cur_end = min(end, start + 512 * 1024 - 1);

		async_cow->end = cur_end;
		INIT_LIST_HEAD(&async_cow->extents);

		async_cow->work.func = async_cow_start;
		async_cow->work.ordered_func = async_cow_submit;
		async_cow->work.ordered_free = async_cow_free;
		async_cow->work.flags = 0;

		nr_pages = (cur_end - start + PAGE_CACHE_SIZE) >>
			PAGE_CACHE_SHIFT;
		atomic_add(nr_pages, &root->fs_info->async_delalloc_pages);

		btrfs_queue_worker(&root->fs_info->delalloc_workers,
				   &async_cow->work);

		if (atomic_read(&root->fs_info->async_delalloc_pages) > limit) {
			wait_event(root->fs_info->async_submit_wait,
			   (atomic_read(&root->fs_info->async_delalloc_pages) <
			    limit));
		}

		while (atomic_read(&root->fs_info->async_submit_draining) &&
		      atomic_read(&root->fs_info->async_delalloc_pages)) {
			wait_event(root->fs_info->async_submit_wait,
			  (atomic_read(&root->fs_info->async_delalloc_pages) ==
			   0));
		}

		*nr_written += nr_pages;
		start = cur_end + 1;
	}
	*page_started = 1;
	return 0;
}

static noinline int csum_exist_in_range(struct btrfs_root *root,
					u64 bytenr, u64 num_bytes)
{
	int ret;
	struct btrfs_ordered_sum *sums;
	LIST_HEAD(list);

	ret = btrfs_lookup_csums_range(root->fs_info->csum_root, bytenr,
				       bytenr + num_bytes - 1, &list, 0);
	if (ret == 0 && list_empty(&list))
		return 0;

	while (!list_empty(&list)) {
		sums = list_entry(list.next, struct btrfs_ordered_sum, list);
		list_del(&sums->list);
		kfree(sums);
	}
	return 1;
}

static noinline int run_delalloc_nocow(struct inode *inode,
				       struct page *locked_page,
			      u64 start, u64 end, int *page_started, int force,
			      unsigned long *nr_written)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_trans_handle *trans;
	struct extent_buffer *leaf;
	struct btrfs_path *path;
	struct btrfs_file_extent_item *fi;
	struct btrfs_key found_key;
	u64 cow_start;
	u64 cur_offset;
	u64 extent_end;
	u64 extent_offset;
	u64 disk_bytenr;
	u64 num_bytes;
	int extent_type;
	int ret, err;
	int type;
	int nocow;
	int check_prev = 1;
	bool nolock;
	u64 ino = btrfs_ino(inode);

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	nolock = btrfs_is_free_space_inode(root, inode);

	if (nolock)
		trans = btrfs_join_transaction_nolock(root);
	else
		trans = btrfs_join_transaction(root);

	if (IS_ERR(trans)) {
		btrfs_free_path(path);
		return PTR_ERR(trans);
	}

	trans->block_rsv = &root->fs_info->delalloc_block_rsv;

	cow_start = (u64)-1;
	cur_offset = start;
	while (1) {
		ret = btrfs_lookup_file_extent(trans, root, path, ino,
					       cur_offset, 0);
		if (ret < 0) {
			btrfs_abort_transaction(trans, root, ret);
			goto error;
		}
		if (ret > 0 && path->slots[0] > 0 && check_prev) {
			leaf = path->nodes[0];
			btrfs_item_key_to_cpu(leaf, &found_key,
					      path->slots[0] - 1);
			if (found_key.objectid == ino &&
			    found_key.type == BTRFS_EXTENT_DATA_KEY)
				path->slots[0]--;
		}
		check_prev = 0;
next_slot:
		leaf = path->nodes[0];
		if (path->slots[0] >= btrfs_header_nritems(leaf)) {
			ret = btrfs_next_leaf(root, path);
			if (ret < 0) {
				btrfs_abort_transaction(trans, root, ret);
				goto error;
			}
			if (ret > 0)
				break;
			leaf = path->nodes[0];
		}

		nocow = 0;
		disk_bytenr = 0;
		num_bytes = 0;
		btrfs_item_key_to_cpu(leaf, &found_key, path->slots[0]);

		if (found_key.objectid > ino ||
		    found_key.type > BTRFS_EXTENT_DATA_KEY ||
		    found_key.offset > end)
			break;

		if (found_key.offset > cur_offset) {
			extent_end = found_key.offset;
			extent_type = 0;
			goto out_check;
		}

		fi = btrfs_item_ptr(leaf, path->slots[0],
				    struct btrfs_file_extent_item);
		extent_type = btrfs_file_extent_type(leaf, fi);

		if (extent_type == BTRFS_FILE_EXTENT_REG ||
		    extent_type == BTRFS_FILE_EXTENT_PREALLOC) {
			disk_bytenr = btrfs_file_extent_disk_bytenr(leaf, fi);
			extent_offset = btrfs_file_extent_offset(leaf, fi);
			extent_end = found_key.offset +
				btrfs_file_extent_num_bytes(leaf, fi);
			if (extent_end <= start) {
				path->slots[0]++;
				goto next_slot;
			}
			if (disk_bytenr == 0)
				goto out_check;
			if (btrfs_file_extent_compression(leaf, fi) ||
			    btrfs_file_extent_encryption(leaf, fi) ||
			    btrfs_file_extent_other_encoding(leaf, fi))
				goto out_check;
			if (extent_type == BTRFS_FILE_EXTENT_REG && !force)
				goto out_check;
			if (btrfs_extent_readonly(root, disk_bytenr))
				goto out_check;
			if (btrfs_cross_ref_exist(trans, root, ino,
						  found_key.offset -
						  extent_offset, disk_bytenr))
				goto out_check;
			disk_bytenr += extent_offset;
			disk_bytenr += cur_offset - found_key.offset;
			num_bytes = min(end + 1, extent_end) - cur_offset;
			if (csum_exist_in_range(root, disk_bytenr, num_bytes))
				goto out_check;
			nocow = 1;
		} else if (extent_type == BTRFS_FILE_EXTENT_INLINE) {
			extent_end = found_key.offset +
				btrfs_file_extent_inline_len(leaf, fi);
			extent_end = ALIGN(extent_end, root->sectorsize);
		} else {
			BUG_ON(1);
		}
out_check:
		if (extent_end <= start) {
			path->slots[0]++;
			goto next_slot;
		}
		if (!nocow) {
			if (cow_start == (u64)-1)
				cow_start = cur_offset;
			cur_offset = extent_end;
			if (cur_offset > end)
				break;
			path->slots[0]++;
			goto next_slot;
		}

		btrfs_release_path(path);
		if (cow_start != (u64)-1) {
			ret = cow_file_range(inode, locked_page, cow_start,
					found_key.offset - 1, page_started,
					nr_written, 1);
			if (ret) {
				btrfs_abort_transaction(trans, root, ret);
				goto error;
			}
			cow_start = (u64)-1;
		}

		if (extent_type == BTRFS_FILE_EXTENT_PREALLOC) {
			struct extent_map *em;
			struct extent_map_tree *em_tree;
			em_tree = &BTRFS_I(inode)->extent_tree;
			em = alloc_extent_map();
			BUG_ON(!em); 
			em->start = cur_offset;
			em->orig_start = em->start;
			em->len = num_bytes;
			em->block_len = num_bytes;
			em->block_start = disk_bytenr;
			em->bdev = root->fs_info->fs_devices->latest_bdev;
			set_bit(EXTENT_FLAG_PINNED, &em->flags);
			while (1) {
				write_lock(&em_tree->lock);
				ret = add_extent_mapping(em_tree, em);
				write_unlock(&em_tree->lock);
				if (ret != -EEXIST) {
					free_extent_map(em);
					break;
				}
				btrfs_drop_extent_cache(inode, em->start,
						em->start + em->len - 1, 0);
			}
			type = BTRFS_ORDERED_PREALLOC;
		} else {
			type = BTRFS_ORDERED_NOCOW;
		}

		ret = btrfs_add_ordered_extent(inode, cur_offset, disk_bytenr,
					       num_bytes, num_bytes, type);
		BUG_ON(ret); 

		if (root->root_key.objectid ==
		    BTRFS_DATA_RELOC_TREE_OBJECTID) {
			ret = btrfs_reloc_clone_csums(inode, cur_offset,
						      num_bytes);
			if (ret) {
				btrfs_abort_transaction(trans, root, ret);
				goto error;
			}
		}

		extent_clear_unlock_delalloc(inode, &BTRFS_I(inode)->io_tree,
				cur_offset, cur_offset + num_bytes - 1,
				locked_page, EXTENT_CLEAR_UNLOCK_PAGE |
				EXTENT_CLEAR_UNLOCK | EXTENT_CLEAR_DELALLOC |
				EXTENT_SET_PRIVATE2);
		cur_offset = extent_end;
		if (cur_offset > end)
			break;
	}
	btrfs_release_path(path);

	if (cur_offset <= end && cow_start == (u64)-1)
		cow_start = cur_offset;
	if (cow_start != (u64)-1) {
		ret = cow_file_range(inode, locked_page, cow_start, end,
				     page_started, nr_written, 1);
		if (ret) {
			btrfs_abort_transaction(trans, root, ret);
			goto error;
		}
	}

error:
	if (nolock) {
		err = btrfs_end_transaction_nolock(trans, root);
	} else {
		err = btrfs_end_transaction(trans, root);
	}
	if (!ret)
		ret = err;

	btrfs_free_path(path);
	return ret;
}

static int run_delalloc_range(struct inode *inode, struct page *locked_page,
			      u64 start, u64 end, int *page_started,
			      unsigned long *nr_written)
{
	int ret;
	struct btrfs_root *root = BTRFS_I(inode)->root;

	if (BTRFS_I(inode)->flags & BTRFS_INODE_NODATACOW)
		ret = run_delalloc_nocow(inode, locked_page, start, end,
					 page_started, 1, nr_written);
	else if (BTRFS_I(inode)->flags & BTRFS_INODE_PREALLOC)
		ret = run_delalloc_nocow(inode, locked_page, start, end,
					 page_started, 0, nr_written);
	else if (!btrfs_test_opt(root, COMPRESS) &&
		 !(BTRFS_I(inode)->force_compress) &&
		 !(BTRFS_I(inode)->flags & BTRFS_INODE_COMPRESS))
		ret = cow_file_range(inode, locked_page, start, end,
				      page_started, nr_written, 1);
	else
		ret = cow_file_range_async(inode, locked_page, start, end,
					   page_started, nr_written);
	return ret;
}

static void btrfs_split_extent_hook(struct inode *inode,
				    struct extent_state *orig, u64 split)
{
	
	if (!(orig->state & EXTENT_DELALLOC))
		return;

	spin_lock(&BTRFS_I(inode)->lock);
	BTRFS_I(inode)->outstanding_extents++;
	spin_unlock(&BTRFS_I(inode)->lock);
}

static void btrfs_merge_extent_hook(struct inode *inode,
				    struct extent_state *new,
				    struct extent_state *other)
{
	
	if (!(other->state & EXTENT_DELALLOC))
		return;

	spin_lock(&BTRFS_I(inode)->lock);
	BTRFS_I(inode)->outstanding_extents--;
	spin_unlock(&BTRFS_I(inode)->lock);
}

static void btrfs_set_bit_hook(struct inode *inode,
			       struct extent_state *state, int *bits)
{

	if (!(state->state & EXTENT_DELALLOC) && (*bits & EXTENT_DELALLOC)) {
		struct btrfs_root *root = BTRFS_I(inode)->root;
		u64 len = state->end + 1 - state->start;
		bool do_list = !btrfs_is_free_space_inode(root, inode);

		if (*bits & EXTENT_FIRST_DELALLOC) {
			*bits &= ~EXTENT_FIRST_DELALLOC;
		} else {
			spin_lock(&BTRFS_I(inode)->lock);
			BTRFS_I(inode)->outstanding_extents++;
			spin_unlock(&BTRFS_I(inode)->lock);
		}

		spin_lock(&root->fs_info->delalloc_lock);
		BTRFS_I(inode)->delalloc_bytes += len;
		root->fs_info->delalloc_bytes += len;
		if (do_list && list_empty(&BTRFS_I(inode)->delalloc_inodes)) {
			list_add_tail(&BTRFS_I(inode)->delalloc_inodes,
				      &root->fs_info->delalloc_inodes);
		}
		spin_unlock(&root->fs_info->delalloc_lock);
	}
}

static void btrfs_clear_bit_hook(struct inode *inode,
				 struct extent_state *state, int *bits)
{
	if ((state->state & EXTENT_DELALLOC) && (*bits & EXTENT_DELALLOC)) {
		struct btrfs_root *root = BTRFS_I(inode)->root;
		u64 len = state->end + 1 - state->start;
		bool do_list = !btrfs_is_free_space_inode(root, inode);

		if (*bits & EXTENT_FIRST_DELALLOC) {
			*bits &= ~EXTENT_FIRST_DELALLOC;
		} else if (!(*bits & EXTENT_DO_ACCOUNTING)) {
			spin_lock(&BTRFS_I(inode)->lock);
			BTRFS_I(inode)->outstanding_extents--;
			spin_unlock(&BTRFS_I(inode)->lock);
		}

		if (*bits & EXTENT_DO_ACCOUNTING)
			btrfs_delalloc_release_metadata(inode, len);

		if (root->root_key.objectid != BTRFS_DATA_RELOC_TREE_OBJECTID
		    && do_list)
			btrfs_free_reserved_data_space(inode, len);

		spin_lock(&root->fs_info->delalloc_lock);
		root->fs_info->delalloc_bytes -= len;
		BTRFS_I(inode)->delalloc_bytes -= len;

		if (do_list && BTRFS_I(inode)->delalloc_bytes == 0 &&
		    !list_empty(&BTRFS_I(inode)->delalloc_inodes)) {
			list_del_init(&BTRFS_I(inode)->delalloc_inodes);
		}
		spin_unlock(&root->fs_info->delalloc_lock);
	}
}

int btrfs_merge_bio_hook(struct page *page, unsigned long offset,
			 size_t size, struct bio *bio,
			 unsigned long bio_flags)
{
	struct btrfs_root *root = BTRFS_I(page->mapping->host)->root;
	struct btrfs_mapping_tree *map_tree;
	u64 logical = (u64)bio->bi_sector << 9;
	u64 length = 0;
	u64 map_length;
	int ret;

	if (bio_flags & EXTENT_BIO_COMPRESSED)
		return 0;

	length = bio->bi_size;
	map_tree = &root->fs_info->mapping_tree;
	map_length = length;
	ret = btrfs_map_block(map_tree, READ, logical,
			      &map_length, NULL, 0);
	
	BUG_ON(ret < 0);
	if (map_length < length + size)
		return 1;
	return 0;
}

static int __btrfs_submit_bio_start(struct inode *inode, int rw,
				    struct bio *bio, int mirror_num,
				    unsigned long bio_flags,
				    u64 bio_offset)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	int ret = 0;

	ret = btrfs_csum_one_bio(root, inode, bio, 0, 0);
	BUG_ON(ret); 
	return 0;
}

static int __btrfs_submit_bio_done(struct inode *inode, int rw, struct bio *bio,
			  int mirror_num, unsigned long bio_flags,
			  u64 bio_offset)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	return btrfs_map_bio(root, rw, bio, mirror_num, 1);
}

static int btrfs_submit_bio_hook(struct inode *inode, int rw, struct bio *bio,
			  int mirror_num, unsigned long bio_flags,
			  u64 bio_offset)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	int ret = 0;
	int skip_sum;
	int metadata = 0;

	skip_sum = BTRFS_I(inode)->flags & BTRFS_INODE_NODATASUM;

	if (btrfs_is_free_space_inode(root, inode))
		metadata = 2;

	ret = btrfs_bio_wq_end_io(root->fs_info, bio, metadata);
	if (ret)
		return ret;

	if (!(rw & REQ_WRITE)) {
		if (bio_flags & EXTENT_BIO_COMPRESSED) {
			return btrfs_submit_compressed_read(inode, bio,
						    mirror_num, bio_flags);
		} else if (!skip_sum) {
			ret = btrfs_lookup_bio_sums(root, inode, bio, NULL);
			if (ret)
				return ret;
		}
		goto mapit;
	} else if (!skip_sum) {
		
		if (root->root_key.objectid == BTRFS_DATA_RELOC_TREE_OBJECTID)
			goto mapit;
		
		return btrfs_wq_submit_bio(BTRFS_I(inode)->root->fs_info,
				   inode, rw, bio, mirror_num,
				   bio_flags, bio_offset,
				   __btrfs_submit_bio_start,
				   __btrfs_submit_bio_done);
	}

mapit:
	return btrfs_map_bio(root, rw, bio, mirror_num, 0);
}

static noinline int add_pending_csums(struct btrfs_trans_handle *trans,
			     struct inode *inode, u64 file_offset,
			     struct list_head *list)
{
	struct btrfs_ordered_sum *sum;

	list_for_each_entry(sum, list, list) {
		btrfs_csum_file_blocks(trans,
		       BTRFS_I(inode)->root->fs_info->csum_root, sum);
	}
	return 0;
}

int btrfs_set_extent_delalloc(struct inode *inode, u64 start, u64 end,
			      struct extent_state **cached_state)
{
	if ((end & (PAGE_CACHE_SIZE - 1)) == 0)
		WARN_ON(1);
	return set_extent_delalloc(&BTRFS_I(inode)->io_tree, start, end,
				   cached_state, GFP_NOFS);
}

struct btrfs_writepage_fixup {
	struct page *page;
	struct btrfs_work work;
};

static void btrfs_writepage_fixup_worker(struct btrfs_work *work)
{
	struct btrfs_writepage_fixup *fixup;
	struct btrfs_ordered_extent *ordered;
	struct extent_state *cached_state = NULL;
	struct page *page;
	struct inode *inode;
	u64 page_start;
	u64 page_end;
	int ret;

	fixup = container_of(work, struct btrfs_writepage_fixup, work);
	page = fixup->page;
again:
	lock_page(page);
	if (!page->mapping || !PageDirty(page) || !PageChecked(page)) {
		ClearPageChecked(page);
		goto out_page;
	}

	inode = page->mapping->host;
	page_start = page_offset(page);
	page_end = page_offset(page) + PAGE_CACHE_SIZE - 1;

	lock_extent_bits(&BTRFS_I(inode)->io_tree, page_start, page_end, 0,
			 &cached_state);

	
	if (PagePrivate2(page))
		goto out;

	ordered = btrfs_lookup_ordered_extent(inode, page_start);
	if (ordered) {
		unlock_extent_cached(&BTRFS_I(inode)->io_tree, page_start,
				     page_end, &cached_state, GFP_NOFS);
		unlock_page(page);
		btrfs_start_ordered_extent(inode, ordered, 1);
		btrfs_put_ordered_extent(ordered);
		goto again;
	}

	ret = btrfs_delalloc_reserve_space(inode, PAGE_CACHE_SIZE);
	if (ret) {
		mapping_set_error(page->mapping, ret);
		end_extent_writepage(page, ret, page_start, page_end);
		ClearPageChecked(page);
		goto out;
	 }

	btrfs_set_extent_delalloc(inode, page_start, page_end, &cached_state);
	ClearPageChecked(page);
	set_page_dirty(page);
out:
	unlock_extent_cached(&BTRFS_I(inode)->io_tree, page_start, page_end,
			     &cached_state, GFP_NOFS);
out_page:
	unlock_page(page);
	page_cache_release(page);
	kfree(fixup);
}

static int btrfs_writepage_start_hook(struct page *page, u64 start, u64 end)
{
	struct inode *inode = page->mapping->host;
	struct btrfs_writepage_fixup *fixup;
	struct btrfs_root *root = BTRFS_I(inode)->root;

	
	if (TestClearPagePrivate2(page))
		return 0;

	if (PageChecked(page))
		return -EAGAIN;

	fixup = kzalloc(sizeof(*fixup), GFP_NOFS);
	if (!fixup)
		return -EAGAIN;

	SetPageChecked(page);
	page_cache_get(page);
	fixup->work.func = btrfs_writepage_fixup_worker;
	fixup->page = page;
	btrfs_queue_worker(&root->fs_info->fixup_workers, &fixup->work);
	return -EBUSY;
}

static int insert_reserved_file_extent(struct btrfs_trans_handle *trans,
				       struct inode *inode, u64 file_pos,
				       u64 disk_bytenr, u64 disk_num_bytes,
				       u64 num_bytes, u64 ram_bytes,
				       u8 compression, u8 encryption,
				       u16 other_encoding, int extent_type)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_file_extent_item *fi;
	struct btrfs_path *path;
	struct extent_buffer *leaf;
	struct btrfs_key ins;
	u64 hint;
	int ret;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	path->leave_spinning = 1;

	ret = btrfs_drop_extents(trans, inode, file_pos, file_pos + num_bytes,
				 &hint, 0);
	if (ret)
		goto out;

	ins.objectid = btrfs_ino(inode);
	ins.offset = file_pos;
	ins.type = BTRFS_EXTENT_DATA_KEY;
	ret = btrfs_insert_empty_item(trans, root, path, &ins, sizeof(*fi));
	if (ret)
		goto out;
	leaf = path->nodes[0];
	fi = btrfs_item_ptr(leaf, path->slots[0],
			    struct btrfs_file_extent_item);
	btrfs_set_file_extent_generation(leaf, fi, trans->transid);
	btrfs_set_file_extent_type(leaf, fi, extent_type);
	btrfs_set_file_extent_disk_bytenr(leaf, fi, disk_bytenr);
	btrfs_set_file_extent_disk_num_bytes(leaf, fi, disk_num_bytes);
	btrfs_set_file_extent_offset(leaf, fi, 0);
	btrfs_set_file_extent_num_bytes(leaf, fi, num_bytes);
	btrfs_set_file_extent_ram_bytes(leaf, fi, ram_bytes);
	btrfs_set_file_extent_compression(leaf, fi, compression);
	btrfs_set_file_extent_encryption(leaf, fi, encryption);
	btrfs_set_file_extent_other_encoding(leaf, fi, other_encoding);

	btrfs_unlock_up_safe(path, 1);
	btrfs_set_lock_blocking(leaf);

	btrfs_mark_buffer_dirty(leaf);

	inode_add_bytes(inode, num_bytes);

	ins.objectid = disk_bytenr;
	ins.offset = disk_num_bytes;
	ins.type = BTRFS_EXTENT_ITEM_KEY;
	ret = btrfs_alloc_reserved_file_extent(trans, root,
					root->root_key.objectid,
					btrfs_ino(inode), file_pos, &ins);
out:
	btrfs_free_path(path);

	return ret;
}

/* as ordered data IO finishes, this gets called so we can finish
 * an ordered extent if the range of bytes in the file it covers are
 * fully written.
 */
static int btrfs_finish_ordered_io(struct inode *inode, u64 start, u64 end)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_trans_handle *trans = NULL;
	struct btrfs_ordered_extent *ordered_extent = NULL;
	struct extent_io_tree *io_tree = &BTRFS_I(inode)->io_tree;
	struct extent_state *cached_state = NULL;
	int compress_type = 0;
	int ret;
	bool nolock;

	ret = btrfs_dec_test_ordered_pending(inode, &ordered_extent, start,
					     end - start + 1);
	if (!ret)
		return 0;
	BUG_ON(!ordered_extent); 

	nolock = btrfs_is_free_space_inode(root, inode);

	if (test_bit(BTRFS_ORDERED_NOCOW, &ordered_extent->flags)) {
		BUG_ON(!list_empty(&ordered_extent->list)); 
		ret = btrfs_ordered_update_i_size(inode, 0, ordered_extent);
		if (!ret) {
			if (nolock)
				trans = btrfs_join_transaction_nolock(root);
			else
				trans = btrfs_join_transaction(root);
			if (IS_ERR(trans))
				return PTR_ERR(trans);
			trans->block_rsv = &root->fs_info->delalloc_block_rsv;
			ret = btrfs_update_inode_fallback(trans, root, inode);
			if (ret) 
				btrfs_abort_transaction(trans, root, ret);
		}
		goto out;
	}

	lock_extent_bits(io_tree, ordered_extent->file_offset,
			 ordered_extent->file_offset + ordered_extent->len - 1,
			 0, &cached_state);

	if (nolock)
		trans = btrfs_join_transaction_nolock(root);
	else
		trans = btrfs_join_transaction(root);
	if (IS_ERR(trans)) {
		ret = PTR_ERR(trans);
		trans = NULL;
		goto out_unlock;
	}
	trans->block_rsv = &root->fs_info->delalloc_block_rsv;

	if (test_bit(BTRFS_ORDERED_COMPRESSED, &ordered_extent->flags))
		compress_type = ordered_extent->compress_type;
	if (test_bit(BTRFS_ORDERED_PREALLOC, &ordered_extent->flags)) {
		BUG_ON(compress_type);
		ret = btrfs_mark_extent_written(trans, inode,
						ordered_extent->file_offset,
						ordered_extent->file_offset +
						ordered_extent->len);
	} else {
		BUG_ON(root == root->fs_info->tree_root);
		ret = insert_reserved_file_extent(trans, inode,
						ordered_extent->file_offset,
						ordered_extent->start,
						ordered_extent->disk_len,
						ordered_extent->len,
						ordered_extent->len,
						compress_type, 0, 0,
						BTRFS_FILE_EXTENT_REG);
		unpin_extent_cache(&BTRFS_I(inode)->extent_tree,
				   ordered_extent->file_offset,
				   ordered_extent->len);
	}
	unlock_extent_cached(io_tree, ordered_extent->file_offset,
			     ordered_extent->file_offset +
			     ordered_extent->len - 1, &cached_state, GFP_NOFS);
	if (ret < 0) {
		btrfs_abort_transaction(trans, root, ret);
		goto out;
	}

	add_pending_csums(trans, inode, ordered_extent->file_offset,
			  &ordered_extent->list);

	ret = btrfs_ordered_update_i_size(inode, 0, ordered_extent);
	if (!ret || !test_bit(BTRFS_ORDERED_PREALLOC, &ordered_extent->flags)) {
		ret = btrfs_update_inode_fallback(trans, root, inode);
		if (ret) { 
			btrfs_abort_transaction(trans, root, ret);
			goto out;
		}
	}
	ret = 0;
out:
	if (root != root->fs_info->tree_root)
		btrfs_delalloc_release_metadata(inode, ordered_extent->len);
	if (trans) {
		if (nolock)
			btrfs_end_transaction_nolock(trans, root);
		else
			btrfs_end_transaction(trans, root);
	}

	
	btrfs_put_ordered_extent(ordered_extent);
	
	btrfs_put_ordered_extent(ordered_extent);

	return 0;
out_unlock:
	unlock_extent_cached(io_tree, ordered_extent->file_offset,
			     ordered_extent->file_offset +
			     ordered_extent->len - 1, &cached_state, GFP_NOFS);
	goto out;
}

static int btrfs_writepage_end_io_hook(struct page *page, u64 start, u64 end,
				struct extent_state *state, int uptodate)
{
	trace_btrfs_writepage_end_io_hook(page, start, end, uptodate);

	ClearPagePrivate2(page);
	return btrfs_finish_ordered_io(page->mapping->host, start, end);
}

static int btrfs_readpage_end_io_hook(struct page *page, u64 start, u64 end,
			       struct extent_state *state, int mirror)
{
	size_t offset = start - ((u64)page->index << PAGE_CACHE_SHIFT);
	struct inode *inode = page->mapping->host;
	struct extent_io_tree *io_tree = &BTRFS_I(inode)->io_tree;
	char *kaddr;
	u64 private = ~(u32)0;
	int ret;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	u32 csum = ~(u32)0;

	if (PageChecked(page)) {
		ClearPageChecked(page);
		goto good;
	}

	if (BTRFS_I(inode)->flags & BTRFS_INODE_NODATASUM)
		goto good;

	if (root->root_key.objectid == BTRFS_DATA_RELOC_TREE_OBJECTID &&
	    test_range_bit(io_tree, start, end, EXTENT_NODATASUM, 1, NULL)) {
		clear_extent_bits(io_tree, start, end, EXTENT_NODATASUM,
				  GFP_NOFS);
		return 0;
	}

	if (state && state->start == start) {
		private = state->private;
		ret = 0;
	} else {
		ret = get_state_private(io_tree, start, &private);
	}
	kaddr = kmap_atomic(page);
	if (ret)
		goto zeroit;

	csum = btrfs_csum_data(root, kaddr + offset, csum,  end - start + 1);
	btrfs_csum_final(csum, (char *)&csum);
	if (csum != private)
		goto zeroit;

	kunmap_atomic(kaddr);
good:
	return 0;

zeroit:
	printk_ratelimited(KERN_INFO "btrfs csum failed ino %llu off %llu csum %u "
		       "private %llu\n",
		       (unsigned long long)btrfs_ino(page->mapping->host),
		       (unsigned long long)start, csum,
		       (unsigned long long)private);
	memset(kaddr + offset, 1, end - start + 1);
	flush_dcache_page(page);
	kunmap_atomic(kaddr);
	if (private == 0)
		return 0;
	return -EIO;
}

struct delayed_iput {
	struct list_head list;
	struct inode *inode;
};

void btrfs_add_delayed_iput(struct inode *inode)
{
	struct btrfs_fs_info *fs_info = BTRFS_I(inode)->root->fs_info;
	struct delayed_iput *delayed;

	if (atomic_add_unless(&inode->i_count, -1, 1))
		return;

	delayed = kmalloc(sizeof(*delayed), GFP_NOFS | __GFP_NOFAIL);
	delayed->inode = inode;

	spin_lock(&fs_info->delayed_iput_lock);
	list_add_tail(&delayed->list, &fs_info->delayed_iputs);
	spin_unlock(&fs_info->delayed_iput_lock);
}

void btrfs_run_delayed_iputs(struct btrfs_root *root)
{
	LIST_HEAD(list);
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct delayed_iput *delayed;
	int empty;

	spin_lock(&fs_info->delayed_iput_lock);
	empty = list_empty(&fs_info->delayed_iputs);
	spin_unlock(&fs_info->delayed_iput_lock);
	if (empty)
		return;

	down_read(&root->fs_info->cleanup_work_sem);
	spin_lock(&fs_info->delayed_iput_lock);
	list_splice_init(&fs_info->delayed_iputs, &list);
	spin_unlock(&fs_info->delayed_iput_lock);

	while (!list_empty(&list)) {
		delayed = list_entry(list.next, struct delayed_iput, list);
		list_del(&delayed->list);
		iput(delayed->inode);
		kfree(delayed);
	}
	up_read(&root->fs_info->cleanup_work_sem);
}

enum btrfs_orphan_cleanup_state {
	ORPHAN_CLEANUP_STARTED	= 1,
	ORPHAN_CLEANUP_DONE	= 2,
};

void btrfs_orphan_commit_root(struct btrfs_trans_handle *trans,
			      struct btrfs_root *root)
{
	struct btrfs_block_rsv *block_rsv;
	int ret;

	if (!list_empty(&root->orphan_list) ||
	    root->orphan_cleanup_state != ORPHAN_CLEANUP_DONE)
		return;

	spin_lock(&root->orphan_lock);
	if (!list_empty(&root->orphan_list)) {
		spin_unlock(&root->orphan_lock);
		return;
	}

	if (root->orphan_cleanup_state != ORPHAN_CLEANUP_DONE) {
		spin_unlock(&root->orphan_lock);
		return;
	}

	block_rsv = root->orphan_block_rsv;
	root->orphan_block_rsv = NULL;
	spin_unlock(&root->orphan_lock);

	if (root->orphan_item_inserted &&
	    btrfs_root_refs(&root->root_item) > 0) {
		ret = btrfs_del_orphan_item(trans, root->fs_info->tree_root,
					    root->root_key.objectid);
		BUG_ON(ret);
		root->orphan_item_inserted = 0;
	}

	if (block_rsv) {
		WARN_ON(block_rsv->size > 0);
		btrfs_free_block_rsv(root, block_rsv);
	}
}

int btrfs_orphan_add(struct btrfs_trans_handle *trans, struct inode *inode)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_block_rsv *block_rsv = NULL;
	int reserve = 0;
	int insert = 0;
	int ret;

	if (!root->orphan_block_rsv) {
		block_rsv = btrfs_alloc_block_rsv(root);
		if (!block_rsv)
			return -ENOMEM;
	}

	spin_lock(&root->orphan_lock);
	if (!root->orphan_block_rsv) {
		root->orphan_block_rsv = block_rsv;
	} else if (block_rsv) {
		btrfs_free_block_rsv(root, block_rsv);
		block_rsv = NULL;
	}

	if (list_empty(&BTRFS_I(inode)->i_orphan)) {
		list_add(&BTRFS_I(inode)->i_orphan, &root->orphan_list);
#if 0
		if (!xchg(&root->orphan_item_inserted, 1))
			insert = 2;
		else
			insert = 1;
#endif
		insert = 1;
	}

	if (!BTRFS_I(inode)->orphan_meta_reserved) {
		BTRFS_I(inode)->orphan_meta_reserved = 1;
		reserve = 1;
	}
	spin_unlock(&root->orphan_lock);

	
	if (reserve) {
		ret = btrfs_orphan_reserve_metadata(trans, inode);
		BUG_ON(ret); 
	}

	
	if (insert >= 1) {
		ret = btrfs_insert_orphan_item(trans, root, btrfs_ino(inode));
		if (ret && ret != -EEXIST) {
			btrfs_abort_transaction(trans, root, ret);
			return ret;
		}
		ret = 0;
	}

	
	if (insert >= 2) {
		ret = btrfs_insert_orphan_item(trans, root->fs_info->tree_root,
					       root->root_key.objectid);
		if (ret && ret != -EEXIST) {
			btrfs_abort_transaction(trans, root, ret);
			return ret;
		}
	}
	return 0;
}

int btrfs_orphan_del(struct btrfs_trans_handle *trans, struct inode *inode)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	int delete_item = 0;
	int release_rsv = 0;
	int ret = 0;

	spin_lock(&root->orphan_lock);
	if (!list_empty(&BTRFS_I(inode)->i_orphan)) {
		list_del_init(&BTRFS_I(inode)->i_orphan);
		delete_item = 1;
	}

	if (BTRFS_I(inode)->orphan_meta_reserved) {
		BTRFS_I(inode)->orphan_meta_reserved = 0;
		release_rsv = 1;
	}
	spin_unlock(&root->orphan_lock);

	if (trans && delete_item) {
		ret = btrfs_del_orphan_item(trans, root, btrfs_ino(inode));
		BUG_ON(ret); 
	}

	if (release_rsv)
		btrfs_orphan_release_metadata(inode);

	return 0;
}

int btrfs_orphan_cleanup(struct btrfs_root *root)
{
	struct btrfs_path *path;
	struct extent_buffer *leaf;
	struct btrfs_key key, found_key;
	struct btrfs_trans_handle *trans;
	struct inode *inode;
	u64 last_objectid = 0;
	int ret = 0, nr_unlink = 0, nr_truncate = 0;

	if (cmpxchg(&root->orphan_cleanup_state, 0, ORPHAN_CLEANUP_STARTED))
		return 0;

	path = btrfs_alloc_path();
	if (!path) {
		ret = -ENOMEM;
		goto out;
	}
	path->reada = -1;

	key.objectid = BTRFS_ORPHAN_OBJECTID;
	btrfs_set_key_type(&key, BTRFS_ORPHAN_ITEM_KEY);
	key.offset = (u64)-1;

	while (1) {
		ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
		if (ret < 0)
			goto out;

		if (ret > 0) {
			ret = 0;
			if (path->slots[0] == 0)
				break;
			path->slots[0]--;
		}

		
		leaf = path->nodes[0];
		btrfs_item_key_to_cpu(leaf, &found_key, path->slots[0]);

		
		if (found_key.objectid != BTRFS_ORPHAN_OBJECTID)
			break;
		if (btrfs_key_type(&found_key) != BTRFS_ORPHAN_ITEM_KEY)
			break;

		
		btrfs_release_path(path);


		if (found_key.offset == last_objectid) {
			printk(KERN_ERR "btrfs: Error removing orphan entry, "
			       "stopping orphan cleanup\n");
			ret = -EINVAL;
			goto out;
		}

		last_objectid = found_key.offset;

		found_key.objectid = found_key.offset;
		found_key.type = BTRFS_INODE_ITEM_KEY;
		found_key.offset = 0;
		inode = btrfs_iget(root->fs_info->sb, &found_key, root, NULL);
		ret = PTR_RET(inode);
		if (ret && ret != -ESTALE)
			goto out;

		if (ret == -ESTALE && root == root->fs_info->tree_root) {
			struct btrfs_root *dead_root;
			struct btrfs_fs_info *fs_info = root->fs_info;
			int is_dead_root = 0;

			spin_lock(&fs_info->trans_lock);
			list_for_each_entry(dead_root, &fs_info->dead_roots,
					    root_list) {
				if (dead_root->root_key.objectid ==
				    found_key.objectid) {
					is_dead_root = 1;
					break;
				}
			}
			spin_unlock(&fs_info->trans_lock);
			if (is_dead_root) {
				
				key.offset = found_key.objectid - 1;
				continue;
			}
		}
		if (ret == -ESTALE) {
			trans = btrfs_start_transaction(root, 1);
			if (IS_ERR(trans)) {
				ret = PTR_ERR(trans);
				goto out;
			}
			ret = btrfs_del_orphan_item(trans, root,
						    found_key.objectid);
			BUG_ON(ret); 
			btrfs_end_transaction(trans, root);
			continue;
		}

		spin_lock(&root->orphan_lock);
		list_add(&BTRFS_I(inode)->i_orphan, &root->orphan_list);
		spin_unlock(&root->orphan_lock);

		
		if (inode->i_nlink) {
			if (!S_ISREG(inode->i_mode)) {
				WARN_ON(1);
				iput(inode);
				continue;
			}
			nr_truncate++;
			ret = btrfs_truncate(inode);
		} else {
			nr_unlink++;
		}

		
		iput(inode);
		if (ret)
			goto out;
	}
	
	btrfs_release_path(path);

	root->orphan_cleanup_state = ORPHAN_CLEANUP_DONE;

	if (root->orphan_block_rsv)
		btrfs_block_rsv_release(root, root->orphan_block_rsv,
					(u64)-1);

	if (root->orphan_block_rsv || root->orphan_item_inserted) {
		trans = btrfs_join_transaction(root);
		if (!IS_ERR(trans))
			btrfs_end_transaction(trans, root);
	}

	if (nr_unlink)
		printk(KERN_INFO "btrfs: unlinked %d orphans\n", nr_unlink);
	if (nr_truncate)
		printk(KERN_INFO "btrfs: truncated %d orphans\n", nr_truncate);

out:
	if (ret)
		printk(KERN_CRIT "btrfs: could not do orphan cleanup %d\n", ret);
	btrfs_free_path(path);
	return ret;
}

static noinline int acls_after_inode_item(struct extent_buffer *leaf,
					  int slot, u64 objectid)
{
	u32 nritems = btrfs_header_nritems(leaf);
	struct btrfs_key found_key;
	int scanned = 0;

	slot++;
	while (slot < nritems) {
		btrfs_item_key_to_cpu(leaf, &found_key, slot);

		
		if (found_key.objectid != objectid)
			return 0;

		
		if (found_key.type == BTRFS_XATTR_ITEM_KEY)
			return 1;

		if (found_key.type > BTRFS_XATTR_ITEM_KEY)
			return 0;

		slot++;
		scanned++;

		if (scanned >= 8)
			break;
	}
	return 1;
}

static void btrfs_read_locked_inode(struct inode *inode)
{
	struct btrfs_path *path;
	struct extent_buffer *leaf;
	struct btrfs_inode_item *inode_item;
	struct btrfs_timespec *tspec;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_key location;
	int maybe_acls;
	u32 rdev;
	int ret;
	bool filled = false;

	ret = btrfs_fill_inode(inode, &rdev);
	if (!ret)
		filled = true;

	path = btrfs_alloc_path();
	if (!path)
		goto make_bad;

	path->leave_spinning = 1;
	memcpy(&location, &BTRFS_I(inode)->location, sizeof(location));

	ret = btrfs_lookup_inode(NULL, root, path, &location, 0);
	if (ret)
		goto make_bad;

	leaf = path->nodes[0];

	if (filled)
		goto cache_acl;

	inode_item = btrfs_item_ptr(leaf, path->slots[0],
				    struct btrfs_inode_item);
	inode->i_mode = btrfs_inode_mode(leaf, inode_item);
	set_nlink(inode, btrfs_inode_nlink(leaf, inode_item));
	inode->i_uid = btrfs_inode_uid(leaf, inode_item);
	inode->i_gid = btrfs_inode_gid(leaf, inode_item);
	btrfs_i_size_write(inode, btrfs_inode_size(leaf, inode_item));

	tspec = btrfs_inode_atime(inode_item);
	inode->i_atime.tv_sec = btrfs_timespec_sec(leaf, tspec);
	inode->i_atime.tv_nsec = btrfs_timespec_nsec(leaf, tspec);

	tspec = btrfs_inode_mtime(inode_item);
	inode->i_mtime.tv_sec = btrfs_timespec_sec(leaf, tspec);
	inode->i_mtime.tv_nsec = btrfs_timespec_nsec(leaf, tspec);

	tspec = btrfs_inode_ctime(inode_item);
	inode->i_ctime.tv_sec = btrfs_timespec_sec(leaf, tspec);
	inode->i_ctime.tv_nsec = btrfs_timespec_nsec(leaf, tspec);

	inode_set_bytes(inode, btrfs_inode_nbytes(leaf, inode_item));
	BTRFS_I(inode)->generation = btrfs_inode_generation(leaf, inode_item);
	BTRFS_I(inode)->sequence = btrfs_inode_sequence(leaf, inode_item);
	inode->i_generation = BTRFS_I(inode)->generation;
	inode->i_rdev = 0;
	rdev = btrfs_inode_rdev(leaf, inode_item);

	BTRFS_I(inode)->index_cnt = (u64)-1;
	BTRFS_I(inode)->flags = btrfs_inode_flags(leaf, inode_item);
cache_acl:
	maybe_acls = acls_after_inode_item(leaf, path->slots[0],
					   btrfs_ino(inode));
	if (!maybe_acls)
		cache_no_acl(inode);

	btrfs_free_path(path);

	switch (inode->i_mode & S_IFMT) {
	case S_IFREG:
		inode->i_mapping->a_ops = &btrfs_aops;
		inode->i_mapping->backing_dev_info = &root->fs_info->bdi;
		BTRFS_I(inode)->io_tree.ops = &btrfs_extent_io_ops;
		inode->i_fop = &btrfs_file_operations;
		inode->i_op = &btrfs_file_inode_operations;
		break;
	case S_IFDIR:
		inode->i_fop = &btrfs_dir_file_operations;
		if (root == root->fs_info->tree_root)
			inode->i_op = &btrfs_dir_ro_inode_operations;
		else
			inode->i_op = &btrfs_dir_inode_operations;
		break;
	case S_IFLNK:
		inode->i_op = &btrfs_symlink_inode_operations;
		inode->i_mapping->a_ops = &btrfs_symlink_aops;
		inode->i_mapping->backing_dev_info = &root->fs_info->bdi;
		break;
	default:
		inode->i_op = &btrfs_special_inode_operations;
		init_special_inode(inode, inode->i_mode, rdev);
		break;
	}

	btrfs_update_iflags(inode);
	return;

make_bad:
	btrfs_free_path(path);
	make_bad_inode(inode);
}

static void fill_inode_item(struct btrfs_trans_handle *trans,
			    struct extent_buffer *leaf,
			    struct btrfs_inode_item *item,
			    struct inode *inode)
{
	btrfs_set_inode_uid(leaf, item, inode->i_uid);
	btrfs_set_inode_gid(leaf, item, inode->i_gid);
	btrfs_set_inode_size(leaf, item, BTRFS_I(inode)->disk_i_size);
	btrfs_set_inode_mode(leaf, item, inode->i_mode);
	btrfs_set_inode_nlink(leaf, item, inode->i_nlink);

	btrfs_set_timespec_sec(leaf, btrfs_inode_atime(item),
			       inode->i_atime.tv_sec);
	btrfs_set_timespec_nsec(leaf, btrfs_inode_atime(item),
				inode->i_atime.tv_nsec);

	btrfs_set_timespec_sec(leaf, btrfs_inode_mtime(item),
			       inode->i_mtime.tv_sec);
	btrfs_set_timespec_nsec(leaf, btrfs_inode_mtime(item),
				inode->i_mtime.tv_nsec);

	btrfs_set_timespec_sec(leaf, btrfs_inode_ctime(item),
			       inode->i_ctime.tv_sec);
	btrfs_set_timespec_nsec(leaf, btrfs_inode_ctime(item),
				inode->i_ctime.tv_nsec);

	btrfs_set_inode_nbytes(leaf, item, inode_get_bytes(inode));
	btrfs_set_inode_generation(leaf, item, BTRFS_I(inode)->generation);
	btrfs_set_inode_sequence(leaf, item, BTRFS_I(inode)->sequence);
	btrfs_set_inode_transid(leaf, item, trans->transid);
	btrfs_set_inode_rdev(leaf, item, inode->i_rdev);
	btrfs_set_inode_flags(leaf, item, BTRFS_I(inode)->flags);
	btrfs_set_inode_block_group(leaf, item, 0);
}

static noinline int btrfs_update_inode_item(struct btrfs_trans_handle *trans,
				struct btrfs_root *root, struct inode *inode)
{
	struct btrfs_inode_item *inode_item;
	struct btrfs_path *path;
	struct extent_buffer *leaf;
	int ret;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	path->leave_spinning = 1;
	ret = btrfs_lookup_inode(trans, root, path, &BTRFS_I(inode)->location,
				 1);
	if (ret) {
		if (ret > 0)
			ret = -ENOENT;
		goto failed;
	}

	btrfs_unlock_up_safe(path, 1);
	leaf = path->nodes[0];
	inode_item = btrfs_item_ptr(leaf, path->slots[0],
				    struct btrfs_inode_item);

	fill_inode_item(trans, leaf, inode_item, inode);
	btrfs_mark_buffer_dirty(leaf);
	btrfs_set_inode_last_trans(trans, inode);
	ret = 0;
failed:
	btrfs_free_path(path);
	return ret;
}

noinline int btrfs_update_inode(struct btrfs_trans_handle *trans,
				struct btrfs_root *root, struct inode *inode)
{
	int ret;

	if (!btrfs_is_free_space_inode(root, inode)
	    && root->root_key.objectid != BTRFS_DATA_RELOC_TREE_OBJECTID) {
		ret = btrfs_delayed_update_inode(trans, root, inode);
		if (!ret)
			btrfs_set_inode_last_trans(trans, inode);
		return ret;
	}

	return btrfs_update_inode_item(trans, root, inode);
}

static noinline int btrfs_update_inode_fallback(struct btrfs_trans_handle *trans,
				struct btrfs_root *root, struct inode *inode)
{
	int ret;

	ret = btrfs_update_inode(trans, root, inode);
	if (ret == -ENOSPC)
		return btrfs_update_inode_item(trans, root, inode);
	return ret;
}

static int __btrfs_unlink_inode(struct btrfs_trans_handle *trans,
				struct btrfs_root *root,
				struct inode *dir, struct inode *inode,
				const char *name, int name_len)
{
	struct btrfs_path *path;
	int ret = 0;
	struct extent_buffer *leaf;
	struct btrfs_dir_item *di;
	struct btrfs_key key;
	u64 index;
	u64 ino = btrfs_ino(inode);
	u64 dir_ino = btrfs_ino(dir);

	path = btrfs_alloc_path();
	if (!path) {
		ret = -ENOMEM;
		goto out;
	}

	path->leave_spinning = 1;
	di = btrfs_lookup_dir_item(trans, root, path, dir_ino,
				    name, name_len, -1);
	if (IS_ERR(di)) {
		ret = PTR_ERR(di);
		goto err;
	}
	if (!di) {
		ret = -ENOENT;
		goto err;
	}
	leaf = path->nodes[0];
	btrfs_dir_item_key_to_cpu(leaf, di, &key);
	ret = btrfs_delete_one_dir_name(trans, root, path, di);
	if (ret)
		goto err;
	btrfs_release_path(path);

	ret = btrfs_del_inode_ref(trans, root, name, name_len, ino,
				  dir_ino, &index);
	if (ret) {
		printk(KERN_INFO "btrfs failed to delete reference to %.*s, "
		       "inode %llu parent %llu\n", name_len, name,
		       (unsigned long long)ino, (unsigned long long)dir_ino);
		btrfs_abort_transaction(trans, root, ret);
		goto err;
	}

	ret = btrfs_delete_delayed_dir_index(trans, root, dir, index);
	if (ret) {
		btrfs_abort_transaction(trans, root, ret);
		goto err;
	}

	ret = btrfs_del_inode_ref_in_log(trans, root, name, name_len,
					 inode, dir_ino);
	if (ret != 0 && ret != -ENOENT) {
		btrfs_abort_transaction(trans, root, ret);
		goto err;
	}

	ret = btrfs_del_dir_entries_in_log(trans, root, name, name_len,
					   dir, index);
	if (ret == -ENOENT)
		ret = 0;
err:
	btrfs_free_path(path);
	if (ret)
		goto out;

	btrfs_i_size_write(dir, dir->i_size - name_len * 2);
	inode->i_ctime = dir->i_mtime = dir->i_ctime = CURRENT_TIME;
	btrfs_update_inode(trans, root, dir);
out:
	return ret;
}

int btrfs_unlink_inode(struct btrfs_trans_handle *trans,
		       struct btrfs_root *root,
		       struct inode *dir, struct inode *inode,
		       const char *name, int name_len)
{
	int ret;
	ret = __btrfs_unlink_inode(trans, root, dir, inode, name, name_len);
	if (!ret) {
		btrfs_drop_nlink(inode);
		ret = btrfs_update_inode(trans, root, inode);
	}
	return ret;
}
		

static int check_path_shared(struct btrfs_root *root,
			     struct btrfs_path *path)
{
	struct extent_buffer *eb;
	int level;
	u64 refs = 1;

	for (level = 0; level < BTRFS_MAX_LEVEL; level++) {
		int ret;

		if (!path->nodes[level])
			break;
		eb = path->nodes[level];
		if (!btrfs_block_can_be_shared(root, eb))
			continue;
		ret = btrfs_lookup_extent_info(NULL, root, eb->start, eb->len,
					       &refs, NULL);
		if (refs > 1)
			return 1;
	}
	return 0;
}

static struct btrfs_trans_handle *__unlink_start_trans(struct inode *dir,
						       struct dentry *dentry)
{
	struct btrfs_trans_handle *trans;
	struct btrfs_root *root = BTRFS_I(dir)->root;
	struct btrfs_path *path;
	struct btrfs_inode_ref *ref;
	struct btrfs_dir_item *di;
	struct inode *inode = dentry->d_inode;
	u64 index;
	int check_link = 1;
	int err = -ENOSPC;
	int ret;
	u64 ino = btrfs_ino(inode);
	u64 dir_ino = btrfs_ino(dir);

	trans = btrfs_start_transaction(root, 8);
	if (!IS_ERR(trans) || PTR_ERR(trans) != -ENOSPC)
		return trans;

	if (ino == BTRFS_EMPTY_SUBVOL_DIR_OBJECTID)
		return ERR_PTR(-ENOSPC);

	
	if (S_ISDIR(inode->i_mode) && atomic_read(&inode->i_count) > 1)
		return ERR_PTR(-ENOSPC);

	if (atomic_read(&inode->i_count) > 2)
		return ERR_PTR(-ENOSPC);

	if (xchg(&root->fs_info->enospc_unlink, 1))
		return ERR_PTR(-ENOSPC);

	path = btrfs_alloc_path();
	if (!path) {
		root->fs_info->enospc_unlink = 0;
		return ERR_PTR(-ENOMEM);
	}

	
	trans = btrfs_start_transaction(root, 1);
	if (IS_ERR(trans)) {
		btrfs_free_path(path);
		root->fs_info->enospc_unlink = 0;
		return trans;
	}

	path->skip_locking = 1;
	path->search_commit_root = 1;

	ret = btrfs_lookup_inode(trans, root, path,
				&BTRFS_I(dir)->location, 0);
	if (ret < 0) {
		err = ret;
		goto out;
	}
	if (ret == 0) {
		if (check_path_shared(root, path))
			goto out;
	} else {
		check_link = 0;
	}
	btrfs_release_path(path);

	ret = btrfs_lookup_inode(trans, root, path,
				&BTRFS_I(inode)->location, 0);
	if (ret < 0) {
		err = ret;
		goto out;
	}
	if (ret == 0) {
		if (check_path_shared(root, path))
			goto out;
	} else {
		check_link = 0;
	}
	btrfs_release_path(path);

	if (ret == 0 && S_ISREG(inode->i_mode)) {
		ret = btrfs_lookup_file_extent(trans, root, path,
					       ino, (u64)-1, 0);
		if (ret < 0) {
			err = ret;
			goto out;
		}
		BUG_ON(ret == 0); 
		if (check_path_shared(root, path))
			goto out;
		btrfs_release_path(path);
	}

	if (!check_link) {
		err = 0;
		goto out;
	}

	di = btrfs_lookup_dir_item(trans, root, path, dir_ino,
				dentry->d_name.name, dentry->d_name.len, 0);
	if (IS_ERR(di)) {
		err = PTR_ERR(di);
		goto out;
	}
	if (di) {
		if (check_path_shared(root, path))
			goto out;
	} else {
		err = 0;
		goto out;
	}
	btrfs_release_path(path);

	ref = btrfs_lookup_inode_ref(trans, root, path,
				dentry->d_name.name, dentry->d_name.len,
				ino, dir_ino, 0);
	if (IS_ERR(ref)) {
		err = PTR_ERR(ref);
		goto out;
	}
	BUG_ON(!ref); 
	if (check_path_shared(root, path))
		goto out;
	index = btrfs_inode_ref_index(path->nodes[0], ref);
	btrfs_release_path(path);

	di = btrfs_lookup_dir_index_item(trans, root, path, dir_ino, index,
				dentry->d_name.name, dentry->d_name.len, 0);
	if (IS_ERR(di)) {
		err = PTR_ERR(di);
		goto out;
	}
	BUG_ON(ret == -ENOENT);
	if (check_path_shared(root, path))
		goto out;

	err = 0;
out:
	btrfs_free_path(path);
	
	if (!err)
		err = btrfs_block_rsv_migrate(trans->block_rsv,
				&root->fs_info->global_block_rsv,
				trans->bytes_reserved);

	if (err) {
		btrfs_end_transaction(trans, root);
		root->fs_info->enospc_unlink = 0;
		return ERR_PTR(err);
	}

	trans->block_rsv = &root->fs_info->global_block_rsv;
	return trans;
}

static void __unlink_end_trans(struct btrfs_trans_handle *trans,
			       struct btrfs_root *root)
{
	if (trans->block_rsv == &root->fs_info->global_block_rsv) {
		btrfs_block_rsv_release(root, trans->block_rsv,
					trans->bytes_reserved);
		trans->block_rsv = &root->fs_info->trans_block_rsv;
		BUG_ON(!root->fs_info->enospc_unlink);
		root->fs_info->enospc_unlink = 0;
	}
	btrfs_end_transaction(trans, root);
}

static int btrfs_unlink(struct inode *dir, struct dentry *dentry)
{
	struct btrfs_root *root = BTRFS_I(dir)->root;
	struct btrfs_trans_handle *trans;
	struct inode *inode = dentry->d_inode;
	int ret;
	unsigned long nr = 0;

	trans = __unlink_start_trans(dir, dentry);
	if (IS_ERR(trans))
		return PTR_ERR(trans);

	btrfs_record_unlink_dir(trans, dir, dentry->d_inode, 0);

	ret = btrfs_unlink_inode(trans, root, dir, dentry->d_inode,
				 dentry->d_name.name, dentry->d_name.len);
	if (ret)
		goto out;

	if (inode->i_nlink == 0) {
		ret = btrfs_orphan_add(trans, inode);
		if (ret)
			goto out;
	}

out:
	nr = trans->blocks_used;
	__unlink_end_trans(trans, root);
	btrfs_btree_balance_dirty(root, nr);
	return ret;
}

int btrfs_unlink_subvol(struct btrfs_trans_handle *trans,
			struct btrfs_root *root,
			struct inode *dir, u64 objectid,
			const char *name, int name_len)
{
	struct btrfs_path *path;
	struct extent_buffer *leaf;
	struct btrfs_dir_item *di;
	struct btrfs_key key;
	u64 index;
	int ret;
	u64 dir_ino = btrfs_ino(dir);

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	di = btrfs_lookup_dir_item(trans, root, path, dir_ino,
				   name, name_len, -1);
	if (IS_ERR_OR_NULL(di)) {
		if (!di)
			ret = -ENOENT;
		else
			ret = PTR_ERR(di);
		goto out;
	}

	leaf = path->nodes[0];
	btrfs_dir_item_key_to_cpu(leaf, di, &key);
	WARN_ON(key.type != BTRFS_ROOT_ITEM_KEY || key.objectid != objectid);
	ret = btrfs_delete_one_dir_name(trans, root, path, di);
	if (ret) {
		btrfs_abort_transaction(trans, root, ret);
		goto out;
	}
	btrfs_release_path(path);

	ret = btrfs_del_root_ref(trans, root->fs_info->tree_root,
				 objectid, root->root_key.objectid,
				 dir_ino, &index, name, name_len);
	if (ret < 0) {
		if (ret != -ENOENT) {
			btrfs_abort_transaction(trans, root, ret);
			goto out;
		}
		di = btrfs_search_dir_index_item(root, path, dir_ino,
						 name, name_len);
		if (IS_ERR_OR_NULL(di)) {
			if (!di)
				ret = -ENOENT;
			else
				ret = PTR_ERR(di);
			btrfs_abort_transaction(trans, root, ret);
			goto out;
		}

		leaf = path->nodes[0];
		btrfs_item_key_to_cpu(leaf, &key, path->slots[0]);
		btrfs_release_path(path);
		index = key.offset;
	}
	btrfs_release_path(path);

	ret = btrfs_delete_delayed_dir_index(trans, root, dir, index);
	if (ret) {
		btrfs_abort_transaction(trans, root, ret);
		goto out;
	}

	btrfs_i_size_write(dir, dir->i_size - name_len * 2);
	dir->i_mtime = dir->i_ctime = CURRENT_TIME;
	ret = btrfs_update_inode(trans, root, dir);
	if (ret)
		btrfs_abort_transaction(trans, root, ret);
out:
	btrfs_free_path(path);
	return ret;
}

static int btrfs_rmdir(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = dentry->d_inode;
	int err = 0;
	struct btrfs_root *root = BTRFS_I(dir)->root;
	struct btrfs_trans_handle *trans;
	unsigned long nr = 0;

	if (inode->i_size > BTRFS_EMPTY_DIR_SIZE ||
	    btrfs_ino(inode) == BTRFS_FIRST_FREE_OBJECTID)
		return -ENOTEMPTY;

	trans = __unlink_start_trans(dir, dentry);
	if (IS_ERR(trans))
		return PTR_ERR(trans);

	if (unlikely(btrfs_ino(inode) == BTRFS_EMPTY_SUBVOL_DIR_OBJECTID)) {
		err = btrfs_unlink_subvol(trans, root, dir,
					  BTRFS_I(inode)->location.objectid,
					  dentry->d_name.name,
					  dentry->d_name.len);
		goto out;
	}

	err = btrfs_orphan_add(trans, inode);
	if (err)
		goto out;

	
	err = btrfs_unlink_inode(trans, root, dir, dentry->d_inode,
				 dentry->d_name.name, dentry->d_name.len);
	if (!err)
		btrfs_i_size_write(inode, 0);
out:
	nr = trans->blocks_used;
	__unlink_end_trans(trans, root);
	btrfs_btree_balance_dirty(root, nr);

	return err;
}

int btrfs_truncate_inode_items(struct btrfs_trans_handle *trans,
			       struct btrfs_root *root,
			       struct inode *inode,
			       u64 new_size, u32 min_type)
{
	struct btrfs_path *path;
	struct extent_buffer *leaf;
	struct btrfs_file_extent_item *fi;
	struct btrfs_key key;
	struct btrfs_key found_key;
	u64 extent_start = 0;
	u64 extent_num_bytes = 0;
	u64 extent_offset = 0;
	u64 item_end = 0;
	u64 mask = root->sectorsize - 1;
	u32 found_type = (u8)-1;
	int found_extent;
	int del_item;
	int pending_del_nr = 0;
	int pending_del_slot = 0;
	int extent_type = -1;
	int ret;
	int err = 0;
	u64 ino = btrfs_ino(inode);

	BUG_ON(new_size > 0 && min_type != BTRFS_EXTENT_DATA_KEY);

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;
	path->reada = -1;

	if (root->ref_cows || root == root->fs_info->tree_root)
		btrfs_drop_extent_cache(inode, new_size & (~mask), (u64)-1, 0);

	if (min_type == 0 && root == BTRFS_I(inode)->root)
		btrfs_kill_delayed_inode_items(inode);

	key.objectid = ino;
	key.offset = (u64)-1;
	key.type = (u8)-1;

search_again:
	path->leave_spinning = 1;
	ret = btrfs_search_slot(trans, root, &key, path, -1, 1);
	if (ret < 0) {
		err = ret;
		goto out;
	}

	if (ret > 0) {
		if (path->slots[0] == 0)
			goto out;
		path->slots[0]--;
	}

	while (1) {
		fi = NULL;
		leaf = path->nodes[0];
		btrfs_item_key_to_cpu(leaf, &found_key, path->slots[0]);
		found_type = btrfs_key_type(&found_key);

		if (found_key.objectid != ino)
			break;

		if (found_type < min_type)
			break;

		item_end = found_key.offset;
		if (found_type == BTRFS_EXTENT_DATA_KEY) {
			fi = btrfs_item_ptr(leaf, path->slots[0],
					    struct btrfs_file_extent_item);
			extent_type = btrfs_file_extent_type(leaf, fi);
			if (extent_type != BTRFS_FILE_EXTENT_INLINE) {
				item_end +=
				    btrfs_file_extent_num_bytes(leaf, fi);
			} else if (extent_type == BTRFS_FILE_EXTENT_INLINE) {
				item_end += btrfs_file_extent_inline_len(leaf,
									 fi);
			}
			item_end--;
		}
		if (found_type > min_type) {
			del_item = 1;
		} else {
			if (item_end < new_size)
				break;
			if (found_key.offset >= new_size)
				del_item = 1;
			else
				del_item = 0;
		}
		found_extent = 0;
		
		if (found_type != BTRFS_EXTENT_DATA_KEY)
			goto delete;

		if (extent_type != BTRFS_FILE_EXTENT_INLINE) {
			u64 num_dec;
			extent_start = btrfs_file_extent_disk_bytenr(leaf, fi);
			if (!del_item) {
				u64 orig_num_bytes =
					btrfs_file_extent_num_bytes(leaf, fi);
				extent_num_bytes = new_size -
					found_key.offset + root->sectorsize - 1;
				extent_num_bytes = extent_num_bytes &
					~((u64)root->sectorsize - 1);
				btrfs_set_file_extent_num_bytes(leaf, fi,
							 extent_num_bytes);
				num_dec = (orig_num_bytes -
					   extent_num_bytes);
				if (root->ref_cows && extent_start != 0)
					inode_sub_bytes(inode, num_dec);
				btrfs_mark_buffer_dirty(leaf);
			} else {
				extent_num_bytes =
					btrfs_file_extent_disk_num_bytes(leaf,
									 fi);
				extent_offset = found_key.offset -
					btrfs_file_extent_offset(leaf, fi);

				
				num_dec = btrfs_file_extent_num_bytes(leaf, fi);
				if (extent_start != 0) {
					found_extent = 1;
					if (root->ref_cows)
						inode_sub_bytes(inode, num_dec);
				}
			}
		} else if (extent_type == BTRFS_FILE_EXTENT_INLINE) {
			if (!del_item &&
			    btrfs_file_extent_compression(leaf, fi) == 0 &&
			    btrfs_file_extent_encryption(leaf, fi) == 0 &&
			    btrfs_file_extent_other_encoding(leaf, fi) == 0) {
				u32 size = new_size - found_key.offset;

				if (root->ref_cows) {
					inode_sub_bytes(inode, item_end + 1 -
							new_size);
				}
				size =
				    btrfs_file_extent_calc_inline_size(size);
				btrfs_truncate_item(trans, root, path,
						    size, 1);
			} else if (root->ref_cows) {
				inode_sub_bytes(inode, item_end + 1 -
						found_key.offset);
			}
		}
delete:
		if (del_item) {
			if (!pending_del_nr) {
				
				pending_del_slot = path->slots[0];
				pending_del_nr = 1;
			} else if (pending_del_nr &&
				   path->slots[0] + 1 == pending_del_slot) {
				
				pending_del_nr++;
				pending_del_slot = path->slots[0];
			} else {
				BUG();
			}
		} else {
			break;
		}
		if (found_extent && (root->ref_cows ||
				     root == root->fs_info->tree_root)) {
			btrfs_set_path_blocking(path);
			ret = btrfs_free_extent(trans, root, extent_start,
						extent_num_bytes, 0,
						btrfs_header_owner(leaf),
						ino, extent_offset, 0);
			BUG_ON(ret);
		}

		if (found_type == BTRFS_INODE_ITEM_KEY)
			break;

		if (path->slots[0] == 0 ||
		    path->slots[0] != pending_del_slot) {
			if (root->ref_cows &&
			    BTRFS_I(inode)->location.objectid !=
						BTRFS_FREE_INO_OBJECTID) {
				err = -EAGAIN;
				goto out;
			}
			if (pending_del_nr) {
				ret = btrfs_del_items(trans, root, path,
						pending_del_slot,
						pending_del_nr);
				if (ret) {
					btrfs_abort_transaction(trans,
								root, ret);
					goto error;
				}
				pending_del_nr = 0;
			}
			btrfs_release_path(path);
			goto search_again;
		} else {
			path->slots[0]--;
		}
	}
out:
	if (pending_del_nr) {
		ret = btrfs_del_items(trans, root, path, pending_del_slot,
				      pending_del_nr);
		if (ret)
			btrfs_abort_transaction(trans, root, ret);
	}
error:
	btrfs_free_path(path);
	return err;
}

static int btrfs_truncate_page(struct address_space *mapping, loff_t from)
{
	struct inode *inode = mapping->host;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct extent_io_tree *io_tree = &BTRFS_I(inode)->io_tree;
	struct btrfs_ordered_extent *ordered;
	struct extent_state *cached_state = NULL;
	char *kaddr;
	u32 blocksize = root->sectorsize;
	pgoff_t index = from >> PAGE_CACHE_SHIFT;
	unsigned offset = from & (PAGE_CACHE_SIZE-1);
	struct page *page;
	gfp_t mask = btrfs_alloc_write_mask(mapping);
	int ret = 0;
	u64 page_start;
	u64 page_end;

	if ((offset & (blocksize - 1)) == 0)
		goto out;
	ret = btrfs_delalloc_reserve_space(inode, PAGE_CACHE_SIZE);
	if (ret)
		goto out;

	ret = -ENOMEM;
again:
	page = find_or_create_page(mapping, index, mask);
	if (!page) {
		btrfs_delalloc_release_space(inode, PAGE_CACHE_SIZE);
		goto out;
	}

	page_start = page_offset(page);
	page_end = page_start + PAGE_CACHE_SIZE - 1;

	if (!PageUptodate(page)) {
		ret = btrfs_readpage(NULL, page);
		lock_page(page);
		if (page->mapping != mapping) {
			unlock_page(page);
			page_cache_release(page);
			goto again;
		}
		if (!PageUptodate(page)) {
			ret = -EIO;
			goto out_unlock;
		}
	}
	wait_on_page_writeback(page);

	lock_extent_bits(io_tree, page_start, page_end, 0, &cached_state);
	set_page_extent_mapped(page);

	ordered = btrfs_lookup_ordered_extent(inode, page_start);
	if (ordered) {
		unlock_extent_cached(io_tree, page_start, page_end,
				     &cached_state, GFP_NOFS);
		unlock_page(page);
		page_cache_release(page);
		btrfs_start_ordered_extent(inode, ordered, 1);
		btrfs_put_ordered_extent(ordered);
		goto again;
	}

	clear_extent_bit(&BTRFS_I(inode)->io_tree, page_start, page_end,
			  EXTENT_DIRTY | EXTENT_DELALLOC | EXTENT_DO_ACCOUNTING,
			  0, 0, &cached_state, GFP_NOFS);

	ret = btrfs_set_extent_delalloc(inode, page_start, page_end,
					&cached_state);
	if (ret) {
		unlock_extent_cached(io_tree, page_start, page_end,
				     &cached_state, GFP_NOFS);
		goto out_unlock;
	}

	ret = 0;
	if (offset != PAGE_CACHE_SIZE) {
		kaddr = kmap(page);
		memset(kaddr + offset, 0, PAGE_CACHE_SIZE - offset);
		flush_dcache_page(page);
		kunmap(page);
	}
	ClearPageChecked(page);
	set_page_dirty(page);
	unlock_extent_cached(io_tree, page_start, page_end, &cached_state,
			     GFP_NOFS);

out_unlock:
	if (ret)
		btrfs_delalloc_release_space(inode, PAGE_CACHE_SIZE);
	unlock_page(page);
	page_cache_release(page);
out:
	return ret;
}

int btrfs_cont_expand(struct inode *inode, loff_t oldsize, loff_t size)
{
	struct btrfs_trans_handle *trans;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct extent_io_tree *io_tree = &BTRFS_I(inode)->io_tree;
	struct extent_map *em = NULL;
	struct extent_state *cached_state = NULL;
	u64 mask = root->sectorsize - 1;
	u64 hole_start = (oldsize + mask) & ~mask;
	u64 block_end = (size + mask) & ~mask;
	u64 last_byte;
	u64 cur_offset;
	u64 hole_size;
	int err = 0;

	if (size <= hole_start)
		return 0;

	while (1) {
		struct btrfs_ordered_extent *ordered;
		btrfs_wait_ordered_range(inode, hole_start,
					 block_end - hole_start);
		lock_extent_bits(io_tree, hole_start, block_end - 1, 0,
				 &cached_state);
		ordered = btrfs_lookup_ordered_extent(inode, hole_start);
		if (!ordered)
			break;
		unlock_extent_cached(io_tree, hole_start, block_end - 1,
				     &cached_state, GFP_NOFS);
		btrfs_put_ordered_extent(ordered);
	}

	cur_offset = hole_start;
	while (1) {
		em = btrfs_get_extent(inode, NULL, 0, cur_offset,
				block_end - cur_offset, 0);
		if (IS_ERR(em)) {
			err = PTR_ERR(em);
			break;
		}
		last_byte = min(extent_map_end(em), block_end);
		last_byte = (last_byte + mask) & ~mask;
		if (!test_bit(EXTENT_FLAG_PREALLOC, &em->flags)) {
			u64 hint_byte = 0;
			hole_size = last_byte - cur_offset;

			trans = btrfs_start_transaction(root, 3);
			if (IS_ERR(trans)) {
				err = PTR_ERR(trans);
				break;
			}

			err = btrfs_drop_extents(trans, inode, cur_offset,
						 cur_offset + hole_size,
						 &hint_byte, 1);
			if (err) {
				btrfs_abort_transaction(trans, root, err);
				btrfs_end_transaction(trans, root);
				break;
			}

			err = btrfs_insert_file_extent(trans, root,
					btrfs_ino(inode), cur_offset, 0,
					0, hole_size, 0, hole_size,
					0, 0, 0);
			if (err) {
				btrfs_abort_transaction(trans, root, err);
				btrfs_end_transaction(trans, root);
				break;
			}

			btrfs_drop_extent_cache(inode, hole_start,
					last_byte - 1, 0);

			btrfs_update_inode(trans, root, inode);
			btrfs_end_transaction(trans, root);
		}
		free_extent_map(em);
		em = NULL;
		cur_offset = last_byte;
		if (cur_offset >= block_end)
			break;
	}

	free_extent_map(em);
	unlock_extent_cached(io_tree, hole_start, block_end - 1, &cached_state,
			     GFP_NOFS);
	return err;
}

static int btrfs_setsize(struct inode *inode, loff_t newsize)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_trans_handle *trans;
	loff_t oldsize = i_size_read(inode);
	int ret;

	if (newsize == oldsize)
		return 0;

	if (newsize > oldsize) {
		truncate_pagecache(inode, oldsize, newsize);
		ret = btrfs_cont_expand(inode, oldsize, newsize);
		if (ret)
			return ret;

		trans = btrfs_start_transaction(root, 1);
		if (IS_ERR(trans))
			return PTR_ERR(trans);

		i_size_write(inode, newsize);
		btrfs_ordered_update_i_size(inode, i_size_read(inode), NULL);
		ret = btrfs_update_inode(trans, root, inode);
		btrfs_end_transaction(trans, root);
	} else {

		if (newsize == 0)
			BTRFS_I(inode)->ordered_data_close = 1;

		
		truncate_setsize(inode, newsize);
		ret = btrfs_truncate(inode);
	}

	return ret;
}

static int btrfs_setattr(struct dentry *dentry, struct iattr *attr)
{
	struct inode *inode = dentry->d_inode;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	int err;

	if (btrfs_root_readonly(root))
		return -EROFS;

	err = inode_change_ok(inode, attr);
	if (err)
		return err;

	if (S_ISREG(inode->i_mode) && (attr->ia_valid & ATTR_SIZE)) {
		err = btrfs_setsize(inode, attr->ia_size);
		if (err)
			return err;
	}

	if (attr->ia_valid) {
		setattr_copy(inode, attr);
		err = btrfs_dirty_inode(inode);

		if (!err && attr->ia_valid & ATTR_MODE)
			err = btrfs_acl_chmod(inode);
	}

	return err;
}

void btrfs_evict_inode(struct inode *inode)
{
	struct btrfs_trans_handle *trans;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_block_rsv *rsv, *global_rsv;
	u64 min_size = btrfs_calc_trunc_metadata_size(root, 1);
	unsigned long nr;
	int ret;

	trace_btrfs_inode_evict(inode);

	truncate_inode_pages(&inode->i_data, 0);
	if (inode->i_nlink && (btrfs_root_refs(&root->root_item) != 0 ||
			       btrfs_is_free_space_inode(root, inode)))
		goto no_delete;

	if (is_bad_inode(inode)) {
		btrfs_orphan_del(NULL, inode);
		goto no_delete;
	}
	
	btrfs_wait_ordered_range(inode, 0, (u64)-1);

	if (root->fs_info->log_root_recovering) {
		BUG_ON(!list_empty(&BTRFS_I(inode)->i_orphan));
		goto no_delete;
	}

	if (inode->i_nlink > 0) {
		BUG_ON(btrfs_root_refs(&root->root_item) != 0);
		goto no_delete;
	}

	rsv = btrfs_alloc_block_rsv(root);
	if (!rsv) {
		btrfs_orphan_del(NULL, inode);
		goto no_delete;
	}
	rsv->size = min_size;
	global_rsv = &root->fs_info->global_block_rsv;

	btrfs_i_size_write(inode, 0);

	while (1) {
		ret = btrfs_block_rsv_refill_noflush(root, rsv, min_size);

		if (ret)
			ret = btrfs_block_rsv_migrate(global_rsv, rsv, min_size);

		if (ret) {
			printk(KERN_WARNING "Could not get space for a "
			       "delete, will truncate on mount %d\n", ret);
			btrfs_orphan_del(NULL, inode);
			btrfs_free_block_rsv(root, rsv);
			goto no_delete;
		}

		trans = btrfs_start_transaction(root, 0);
		if (IS_ERR(trans)) {
			btrfs_orphan_del(NULL, inode);
			btrfs_free_block_rsv(root, rsv);
			goto no_delete;
		}

		trans->block_rsv = rsv;

		ret = btrfs_truncate_inode_items(trans, root, inode, 0, 0);
		if (ret != -EAGAIN)
			break;

		nr = trans->blocks_used;
		btrfs_end_transaction(trans, root);
		trans = NULL;
		btrfs_btree_balance_dirty(root, nr);
	}

	btrfs_free_block_rsv(root, rsv);

	if (ret == 0) {
		trans->block_rsv = root->orphan_block_rsv;
		ret = btrfs_orphan_del(trans, inode);
		BUG_ON(ret);
	}

	trans->block_rsv = &root->fs_info->trans_block_rsv;
	if (!(root == root->fs_info->tree_root ||
	      root->root_key.objectid == BTRFS_TREE_RELOC_OBJECTID))
		btrfs_return_ino(root, btrfs_ino(inode));

	nr = trans->blocks_used;
	btrfs_end_transaction(trans, root);
	btrfs_btree_balance_dirty(root, nr);
no_delete:
	end_writeback(inode);
	return;
}

static int btrfs_inode_by_name(struct inode *dir, struct dentry *dentry,
			       struct btrfs_key *location)
{
	const char *name = dentry->d_name.name;
	int namelen = dentry->d_name.len;
	struct btrfs_dir_item *di;
	struct btrfs_path *path;
	struct btrfs_root *root = BTRFS_I(dir)->root;
	int ret = 0;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	di = btrfs_lookup_dir_item(NULL, root, path, btrfs_ino(dir), name,
				    namelen, 0);
	if (IS_ERR(di))
		ret = PTR_ERR(di);

	if (IS_ERR_OR_NULL(di))
		goto out_err;

	btrfs_dir_item_key_to_cpu(path->nodes[0], di, location);
out:
	btrfs_free_path(path);
	return ret;
out_err:
	location->objectid = 0;
	goto out;
}

static int fixup_tree_root_location(struct btrfs_root *root,
				    struct inode *dir,
				    struct dentry *dentry,
				    struct btrfs_key *location,
				    struct btrfs_root **sub_root)
{
	struct btrfs_path *path;
	struct btrfs_root *new_root;
	struct btrfs_root_ref *ref;
	struct extent_buffer *leaf;
	int ret;
	int err = 0;

	path = btrfs_alloc_path();
	if (!path) {
		err = -ENOMEM;
		goto out;
	}

	err = -ENOENT;
	ret = btrfs_find_root_ref(root->fs_info->tree_root, path,
				  BTRFS_I(dir)->root->root_key.objectid,
				  location->objectid);
	if (ret) {
		if (ret < 0)
			err = ret;
		goto out;
	}

	leaf = path->nodes[0];
	ref = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_root_ref);
	if (btrfs_root_ref_dirid(leaf, ref) != btrfs_ino(dir) ||
	    btrfs_root_ref_name_len(leaf, ref) != dentry->d_name.len)
		goto out;

	ret = memcmp_extent_buffer(leaf, dentry->d_name.name,
				   (unsigned long)(ref + 1),
				   dentry->d_name.len);
	if (ret)
		goto out;

	btrfs_release_path(path);

	new_root = btrfs_read_fs_root_no_name(root->fs_info, location);
	if (IS_ERR(new_root)) {
		err = PTR_ERR(new_root);
		goto out;
	}

	if (btrfs_root_refs(&new_root->root_item) == 0) {
		err = -ENOENT;
		goto out;
	}

	*sub_root = new_root;
	location->objectid = btrfs_root_dirid(&new_root->root_item);
	location->type = BTRFS_INODE_ITEM_KEY;
	location->offset = 0;
	err = 0;
out:
	btrfs_free_path(path);
	return err;
}

static void inode_tree_add(struct inode *inode)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_inode *entry;
	struct rb_node **p;
	struct rb_node *parent;
	u64 ino = btrfs_ino(inode);
again:
	p = &root->inode_tree.rb_node;
	parent = NULL;

	if (inode_unhashed(inode))
		return;

	spin_lock(&root->inode_lock);
	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct btrfs_inode, rb_node);

		if (ino < btrfs_ino(&entry->vfs_inode))
			p = &parent->rb_left;
		else if (ino > btrfs_ino(&entry->vfs_inode))
			p = &parent->rb_right;
		else {
			WARN_ON(!(entry->vfs_inode.i_state &
				  (I_WILL_FREE | I_FREEING)));
			rb_erase(parent, &root->inode_tree);
			RB_CLEAR_NODE(parent);
			spin_unlock(&root->inode_lock);
			goto again;
		}
	}
	rb_link_node(&BTRFS_I(inode)->rb_node, parent, p);
	rb_insert_color(&BTRFS_I(inode)->rb_node, &root->inode_tree);
	spin_unlock(&root->inode_lock);
}

static void inode_tree_del(struct inode *inode)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	int empty = 0;

	spin_lock(&root->inode_lock);
	if (!RB_EMPTY_NODE(&BTRFS_I(inode)->rb_node)) {
		rb_erase(&BTRFS_I(inode)->rb_node, &root->inode_tree);
		RB_CLEAR_NODE(&BTRFS_I(inode)->rb_node);
		empty = RB_EMPTY_ROOT(&root->inode_tree);
	}
	spin_unlock(&root->inode_lock);

	if (empty && btrfs_root_refs(&root->root_item) == 0 &&
	    root != root->fs_info->tree_root) {
		synchronize_srcu(&root->fs_info->subvol_srcu);
		spin_lock(&root->inode_lock);
		empty = RB_EMPTY_ROOT(&root->inode_tree);
		spin_unlock(&root->inode_lock);
		if (empty)
			btrfs_add_dead_root(root);
	}
}

void btrfs_invalidate_inodes(struct btrfs_root *root)
{
	struct rb_node *node;
	struct rb_node *prev;
	struct btrfs_inode *entry;
	struct inode *inode;
	u64 objectid = 0;

	WARN_ON(btrfs_root_refs(&root->root_item) != 0);

	spin_lock(&root->inode_lock);
again:
	node = root->inode_tree.rb_node;
	prev = NULL;
	while (node) {
		prev = node;
		entry = rb_entry(node, struct btrfs_inode, rb_node);

		if (objectid < btrfs_ino(&entry->vfs_inode))
			node = node->rb_left;
		else if (objectid > btrfs_ino(&entry->vfs_inode))
			node = node->rb_right;
		else
			break;
	}
	if (!node) {
		while (prev) {
			entry = rb_entry(prev, struct btrfs_inode, rb_node);
			if (objectid <= btrfs_ino(&entry->vfs_inode)) {
				node = prev;
				break;
			}
			prev = rb_next(prev);
		}
	}
	while (node) {
		entry = rb_entry(node, struct btrfs_inode, rb_node);
		objectid = btrfs_ino(&entry->vfs_inode) + 1;
		inode = igrab(&entry->vfs_inode);
		if (inode) {
			spin_unlock(&root->inode_lock);
			if (atomic_read(&inode->i_count) > 1)
				d_prune_aliases(inode);
			iput(inode);
			cond_resched();
			spin_lock(&root->inode_lock);
			goto again;
		}

		if (cond_resched_lock(&root->inode_lock))
			goto again;

		node = rb_next(node);
	}
	spin_unlock(&root->inode_lock);
}

static int btrfs_init_locked_inode(struct inode *inode, void *p)
{
	struct btrfs_iget_args *args = p;
	inode->i_ino = args->ino;
	BTRFS_I(inode)->root = args->root;
	btrfs_set_inode_space_info(args->root, inode);
	return 0;
}

static int btrfs_find_actor(struct inode *inode, void *opaque)
{
	struct btrfs_iget_args *args = opaque;
	return args->ino == btrfs_ino(inode) &&
		args->root == BTRFS_I(inode)->root;
}

static struct inode *btrfs_iget_locked(struct super_block *s,
				       u64 objectid,
				       struct btrfs_root *root)
{
	struct inode *inode;
	struct btrfs_iget_args args;
	args.ino = objectid;
	args.root = root;

	inode = iget5_locked(s, objectid, btrfs_find_actor,
			     btrfs_init_locked_inode,
			     (void *)&args);
	return inode;
}

struct inode *btrfs_iget(struct super_block *s, struct btrfs_key *location,
			 struct btrfs_root *root, int *new)
{
	struct inode *inode;

	inode = btrfs_iget_locked(s, location->objectid, root);
	if (!inode)
		return ERR_PTR(-ENOMEM);

	if (inode->i_state & I_NEW) {
		BTRFS_I(inode)->root = root;
		memcpy(&BTRFS_I(inode)->location, location, sizeof(*location));
		btrfs_read_locked_inode(inode);
		if (!is_bad_inode(inode)) {
			inode_tree_add(inode);
			unlock_new_inode(inode);
			if (new)
				*new = 1;
		} else {
			unlock_new_inode(inode);
			iput(inode);
			inode = ERR_PTR(-ESTALE);
		}
	}

	return inode;
}

static struct inode *new_simple_dir(struct super_block *s,
				    struct btrfs_key *key,
				    struct btrfs_root *root)
{
	struct inode *inode = new_inode(s);

	if (!inode)
		return ERR_PTR(-ENOMEM);

	BTRFS_I(inode)->root = root;
	memcpy(&BTRFS_I(inode)->location, key, sizeof(*key));
	BTRFS_I(inode)->dummy_inode = 1;

	inode->i_ino = BTRFS_EMPTY_SUBVOL_DIR_OBJECTID;
	inode->i_op = &btrfs_dir_ro_inode_operations;
	inode->i_fop = &simple_dir_operations;
	inode->i_mode = S_IFDIR | S_IRUGO | S_IWUSR | S_IXUGO;
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;

	return inode;
}

struct inode *btrfs_lookup_dentry(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode;
	struct btrfs_root *root = BTRFS_I(dir)->root;
	struct btrfs_root *sub_root = root;
	struct btrfs_key location;
	int index;
	int ret = 0;

	if (dentry->d_name.len > BTRFS_NAME_LEN)
		return ERR_PTR(-ENAMETOOLONG);

	if (unlikely(d_need_lookup(dentry))) {
		memcpy(&location, dentry->d_fsdata, sizeof(struct btrfs_key));
		kfree(dentry->d_fsdata);
		dentry->d_fsdata = NULL;
		
		d_drop(dentry);
	} else {
		ret = btrfs_inode_by_name(dir, dentry, &location);
	}

	if (ret < 0)
		return ERR_PTR(ret);

	if (location.objectid == 0)
		return NULL;

	if (location.type == BTRFS_INODE_ITEM_KEY) {
		inode = btrfs_iget(dir->i_sb, &location, root, NULL);
		return inode;
	}

	BUG_ON(location.type != BTRFS_ROOT_ITEM_KEY);

	index = srcu_read_lock(&root->fs_info->subvol_srcu);
	ret = fixup_tree_root_location(root, dir, dentry,
				       &location, &sub_root);
	if (ret < 0) {
		if (ret != -ENOENT)
			inode = ERR_PTR(ret);
		else
			inode = new_simple_dir(dir->i_sb, &location, sub_root);
	} else {
		inode = btrfs_iget(dir->i_sb, &location, sub_root, NULL);
	}
	srcu_read_unlock(&root->fs_info->subvol_srcu, index);

	if (!IS_ERR(inode) && root != sub_root) {
		down_read(&root->fs_info->cleanup_work_sem);
		if (!(inode->i_sb->s_flags & MS_RDONLY))
			ret = btrfs_orphan_cleanup(sub_root);
		up_read(&root->fs_info->cleanup_work_sem);
		if (ret)
			inode = ERR_PTR(ret);
	}

	return inode;
}

static int btrfs_dentry_delete(const struct dentry *dentry)
{
	struct btrfs_root *root;
	struct inode *inode = dentry->d_inode;

	if (!inode && !IS_ROOT(dentry))
		inode = dentry->d_parent->d_inode;

	if (inode) {
		root = BTRFS_I(inode)->root;
		if (btrfs_root_refs(&root->root_item) == 0)
			return 1;

		if (btrfs_ino(inode) == BTRFS_EMPTY_SUBVOL_DIR_OBJECTID)
			return 1;
	}
	return 0;
}

static void btrfs_dentry_release(struct dentry *dentry)
{
	if (dentry->d_fsdata)
		kfree(dentry->d_fsdata);
}

static struct dentry *btrfs_lookup(struct inode *dir, struct dentry *dentry,
				   struct nameidata *nd)
{
	struct dentry *ret;

	ret = d_splice_alias(btrfs_lookup_dentry(dir, dentry), dentry);
	if (unlikely(d_need_lookup(dentry))) {
		spin_lock(&dentry->d_lock);
		dentry->d_flags &= ~DCACHE_NEED_LOOKUP;
		spin_unlock(&dentry->d_lock);
	}
	return ret;
}

unsigned char btrfs_filetype_table[] = {
	DT_UNKNOWN, DT_REG, DT_DIR, DT_CHR, DT_BLK, DT_FIFO, DT_SOCK, DT_LNK
};

static int btrfs_real_readdir(struct file *filp, void *dirent,
			      filldir_t filldir)
{
	struct inode *inode = filp->f_dentry->d_inode;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_item *item;
	struct btrfs_dir_item *di;
	struct btrfs_key key;
	struct btrfs_key found_key;
	struct btrfs_path *path;
	struct list_head ins_list;
	struct list_head del_list;
	int ret;
	struct extent_buffer *leaf;
	int slot;
	unsigned char d_type;
	int over = 0;
	u32 di_cur;
	u32 di_total;
	u32 di_len;
	int key_type = BTRFS_DIR_INDEX_KEY;
	char tmp_name[32];
	char *name_ptr;
	int name_len;
	int is_curr = 0;	

	
	if (root->fs_info->tree_root == root)
		key_type = BTRFS_DIR_ITEM_KEY;

	
	if (filp->f_pos == 0) {
		over = filldir(dirent, ".", 1,
			       filp->f_pos, btrfs_ino(inode), DT_DIR);
		if (over)
			return 0;
		filp->f_pos = 1;
	}
	
	if (filp->f_pos == 1) {
		u64 pino = parent_ino(filp->f_path.dentry);
		over = filldir(dirent, "..", 2,
			       filp->f_pos, pino, DT_DIR);
		if (over)
			return 0;
		filp->f_pos = 2;
	}
	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	path->reada = 1;

	if (key_type == BTRFS_DIR_INDEX_KEY) {
		INIT_LIST_HEAD(&ins_list);
		INIT_LIST_HEAD(&del_list);
		btrfs_get_delayed_items(inode, &ins_list, &del_list);
	}

	btrfs_set_key_type(&key, key_type);
	key.offset = filp->f_pos;
	key.objectid = btrfs_ino(inode);

	ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
	if (ret < 0)
		goto err;

	while (1) {
		leaf = path->nodes[0];
		slot = path->slots[0];
		if (slot >= btrfs_header_nritems(leaf)) {
			ret = btrfs_next_leaf(root, path);
			if (ret < 0)
				goto err;
			else if (ret > 0)
				break;
			continue;
		}

		item = btrfs_item_nr(leaf, slot);
		btrfs_item_key_to_cpu(leaf, &found_key, slot);

		if (found_key.objectid != key.objectid)
			break;
		if (btrfs_key_type(&found_key) != key_type)
			break;
		if (found_key.offset < filp->f_pos)
			goto next;
		if (key_type == BTRFS_DIR_INDEX_KEY &&
		    btrfs_should_delete_dir_index(&del_list,
						  found_key.offset))
			goto next;

		filp->f_pos = found_key.offset;
		is_curr = 1;

		di = btrfs_item_ptr(leaf, slot, struct btrfs_dir_item);
		di_cur = 0;
		di_total = btrfs_item_size(leaf, item);

		while (di_cur < di_total) {
			struct btrfs_key location;

			if (verify_dir_item(root, leaf, di))
				break;

			name_len = btrfs_dir_name_len(leaf, di);
			if (name_len <= sizeof(tmp_name)) {
				name_ptr = tmp_name;
			} else {
				name_ptr = kmalloc(name_len, GFP_NOFS);
				if (!name_ptr) {
					ret = -ENOMEM;
					goto err;
				}
			}
			read_extent_buffer(leaf, name_ptr,
					   (unsigned long)(di + 1), name_len);

			d_type = btrfs_filetype_table[btrfs_dir_type(leaf, di)];
			btrfs_dir_item_key_to_cpu(leaf, di, &location);


			if (location.type == BTRFS_ROOT_ITEM_KEY &&
			    location.objectid == root->root_key.objectid) {
				over = 0;
				goto skip;
			}
			over = filldir(dirent, name_ptr, name_len,
				       found_key.offset, location.objectid,
				       d_type);

skip:
			if (name_ptr != tmp_name)
				kfree(name_ptr);

			if (over)
				goto nopos;
			di_len = btrfs_dir_name_len(leaf, di) +
				 btrfs_dir_data_len(leaf, di) + sizeof(*di);
			di_cur += di_len;
			di = (struct btrfs_dir_item *)((char *)di + di_len);
		}
next:
		path->slots[0]++;
	}

	if (key_type == BTRFS_DIR_INDEX_KEY) {
		if (is_curr)
			filp->f_pos++;
		ret = btrfs_readdir_delayed_dir_index(filp, dirent, filldir,
						      &ins_list);
		if (ret)
			goto nopos;
	}

	
	if (key_type == BTRFS_DIR_INDEX_KEY)
		filp->f_pos = 0x7fffffff;
	else
		filp->f_pos++;
nopos:
	ret = 0;
err:
	if (key_type == BTRFS_DIR_INDEX_KEY)
		btrfs_put_delayed_items(&ins_list, &del_list);
	btrfs_free_path(path);
	return ret;
}

int btrfs_write_inode(struct inode *inode, struct writeback_control *wbc)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_trans_handle *trans;
	int ret = 0;
	bool nolock = false;

	if (BTRFS_I(inode)->dummy_inode)
		return 0;

	if (btrfs_fs_closing(root->fs_info) && btrfs_is_free_space_inode(root, inode))
		nolock = true;

	if (wbc->sync_mode == WB_SYNC_ALL) {
		if (nolock)
			trans = btrfs_join_transaction_nolock(root);
		else
			trans = btrfs_join_transaction(root);
		if (IS_ERR(trans))
			return PTR_ERR(trans);
		if (nolock)
			ret = btrfs_end_transaction_nolock(trans, root);
		else
			ret = btrfs_commit_transaction(trans, root);
	}
	return ret;
}

int btrfs_dirty_inode(struct inode *inode)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_trans_handle *trans;
	int ret;

	if (BTRFS_I(inode)->dummy_inode)
		return 0;

	trans = btrfs_join_transaction(root);
	if (IS_ERR(trans))
		return PTR_ERR(trans);

	ret = btrfs_update_inode(trans, root, inode);
	if (ret && ret == -ENOSPC) {
		
		btrfs_end_transaction(trans, root);
		trans = btrfs_start_transaction(root, 1);
		if (IS_ERR(trans))
			return PTR_ERR(trans);

		ret = btrfs_update_inode(trans, root, inode);
	}
	btrfs_end_transaction(trans, root);
	if (BTRFS_I(inode)->delayed_node)
		btrfs_balance_delayed_items(root);

	return ret;
}

int btrfs_update_time(struct file *file)
{
	struct inode *inode = file->f_path.dentry->d_inode;
	struct timespec now;
	int ret;
	enum { S_MTIME = 1, S_CTIME = 2, S_VERSION = 4 } sync_it = 0;

	
	if (IS_NOCMTIME(inode))
		return 0;

	now = current_fs_time(inode->i_sb);
	if (!timespec_equal(&inode->i_mtime, &now))
		sync_it = S_MTIME;

	if (!timespec_equal(&inode->i_ctime, &now))
		sync_it |= S_CTIME;

	if (IS_I_VERSION(inode))
		sync_it |= S_VERSION;

	if (!sync_it)
		return 0;

	
	if (mnt_want_write_file(file))
		return 0;

	
	if (sync_it & S_VERSION)
		inode_inc_iversion(inode);
	if (sync_it & S_CTIME)
		inode->i_ctime = now;
	if (sync_it & S_MTIME)
		inode->i_mtime = now;
	ret = btrfs_dirty_inode(inode);
	if (!ret)
		mark_inode_dirty_sync(inode);
	mnt_drop_write(file->f_path.mnt);
	return ret;
}

static int btrfs_set_inode_index_count(struct inode *inode)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_key key, found_key;
	struct btrfs_path *path;
	struct extent_buffer *leaf;
	int ret;

	key.objectid = btrfs_ino(inode);
	btrfs_set_key_type(&key, BTRFS_DIR_INDEX_KEY);
	key.offset = (u64)-1;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
	if (ret < 0)
		goto out;
	
	if (ret == 0)
		goto out;
	ret = 0;

	if (path->slots[0] == 0) {
		BTRFS_I(inode)->index_cnt = 2;
		goto out;
	}

	path->slots[0]--;

	leaf = path->nodes[0];
	btrfs_item_key_to_cpu(leaf, &found_key, path->slots[0]);

	if (found_key.objectid != btrfs_ino(inode) ||
	    btrfs_key_type(&found_key) != BTRFS_DIR_INDEX_KEY) {
		BTRFS_I(inode)->index_cnt = 2;
		goto out;
	}

	BTRFS_I(inode)->index_cnt = found_key.offset + 1;
out:
	btrfs_free_path(path);
	return ret;
}

int btrfs_set_inode_index(struct inode *dir, u64 *index)
{
	int ret = 0;

	if (BTRFS_I(dir)->index_cnt == (u64)-1) {
		ret = btrfs_inode_delayed_dir_index_count(dir);
		if (ret) {
			ret = btrfs_set_inode_index_count(dir);
			if (ret)
				return ret;
		}
	}

	*index = BTRFS_I(dir)->index_cnt;
	BTRFS_I(dir)->index_cnt++;

	return ret;
}

static struct inode *btrfs_new_inode(struct btrfs_trans_handle *trans,
				     struct btrfs_root *root,
				     struct inode *dir,
				     const char *name, int name_len,
				     u64 ref_objectid, u64 objectid,
				     umode_t mode, u64 *index)
{
	struct inode *inode;
	struct btrfs_inode_item *inode_item;
	struct btrfs_key *location;
	struct btrfs_path *path;
	struct btrfs_inode_ref *ref;
	struct btrfs_key key[2];
	u32 sizes[2];
	unsigned long ptr;
	int ret;
	int owner;

	path = btrfs_alloc_path();
	if (!path)
		return ERR_PTR(-ENOMEM);

	inode = new_inode(root->fs_info->sb);
	if (!inode) {
		btrfs_free_path(path);
		return ERR_PTR(-ENOMEM);
	}

	inode->i_ino = objectid;

	if (dir) {
		trace_btrfs_inode_request(dir);

		ret = btrfs_set_inode_index(dir, index);
		if (ret) {
			btrfs_free_path(path);
			iput(inode);
			return ERR_PTR(ret);
		}
	}
	BTRFS_I(inode)->index_cnt = 2;
	BTRFS_I(inode)->root = root;
	BTRFS_I(inode)->generation = trans->transid;
	inode->i_generation = BTRFS_I(inode)->generation;
	btrfs_set_inode_space_info(root, inode);

	if (S_ISDIR(mode))
		owner = 0;
	else
		owner = 1;

	key[0].objectid = objectid;
	btrfs_set_key_type(&key[0], BTRFS_INODE_ITEM_KEY);
	key[0].offset = 0;

	key[1].objectid = objectid;
	btrfs_set_key_type(&key[1], BTRFS_INODE_REF_KEY);
	key[1].offset = ref_objectid;

	sizes[0] = sizeof(struct btrfs_inode_item);
	sizes[1] = name_len + sizeof(*ref);

	path->leave_spinning = 1;
	ret = btrfs_insert_empty_items(trans, root, path, key, sizes, 2);
	if (ret != 0)
		goto fail;

	inode_init_owner(inode, dir, mode);
	inode_set_bytes(inode, 0);
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
	inode_item = btrfs_item_ptr(path->nodes[0], path->slots[0],
				  struct btrfs_inode_item);
	fill_inode_item(trans, path->nodes[0], inode_item, inode);

	ref = btrfs_item_ptr(path->nodes[0], path->slots[0] + 1,
			     struct btrfs_inode_ref);
	btrfs_set_inode_ref_name_len(path->nodes[0], ref, name_len);
	btrfs_set_inode_ref_index(path->nodes[0], ref, *index);
	ptr = (unsigned long)(ref + 1);
	write_extent_buffer(path->nodes[0], name, ptr, name_len);

	btrfs_mark_buffer_dirty(path->nodes[0]);
	btrfs_free_path(path);

	location = &BTRFS_I(inode)->location;
	location->objectid = objectid;
	location->offset = 0;
	btrfs_set_key_type(location, BTRFS_INODE_ITEM_KEY);

	btrfs_inherit_iflags(inode, dir);

	if (S_ISREG(mode)) {
		if (btrfs_test_opt(root, NODATASUM))
			BTRFS_I(inode)->flags |= BTRFS_INODE_NODATASUM;
		if (btrfs_test_opt(root, NODATACOW) ||
		    (BTRFS_I(dir)->flags & BTRFS_INODE_NODATACOW))
			BTRFS_I(inode)->flags |= BTRFS_INODE_NODATACOW;
	}

	insert_inode_hash(inode);
	inode_tree_add(inode);

	trace_btrfs_inode_new(inode);
	btrfs_set_inode_last_trans(trans, inode);

	return inode;
fail:
	if (dir)
		BTRFS_I(dir)->index_cnt--;
	btrfs_free_path(path);
	iput(inode);
	return ERR_PTR(ret);
}

static inline u8 btrfs_inode_type(struct inode *inode)
{
	return btrfs_type_by_mode[(inode->i_mode & S_IFMT) >> S_SHIFT];
}

int btrfs_add_link(struct btrfs_trans_handle *trans,
		   struct inode *parent_inode, struct inode *inode,
		   const char *name, int name_len, int add_backref, u64 index)
{
	int ret = 0;
	struct btrfs_key key;
	struct btrfs_root *root = BTRFS_I(parent_inode)->root;
	u64 ino = btrfs_ino(inode);
	u64 parent_ino = btrfs_ino(parent_inode);

	if (unlikely(ino == BTRFS_FIRST_FREE_OBJECTID)) {
		memcpy(&key, &BTRFS_I(inode)->root->root_key, sizeof(key));
	} else {
		key.objectid = ino;
		btrfs_set_key_type(&key, BTRFS_INODE_ITEM_KEY);
		key.offset = 0;
	}

	if (unlikely(ino == BTRFS_FIRST_FREE_OBJECTID)) {
		ret = btrfs_add_root_ref(trans, root->fs_info->tree_root,
					 key.objectid, root->root_key.objectid,
					 parent_ino, index, name, name_len);
	} else if (add_backref) {
		ret = btrfs_insert_inode_ref(trans, root, name, name_len, ino,
					     parent_ino, index);
	}

	
	if (ret)
		return ret;

	ret = btrfs_insert_dir_item(trans, root, name, name_len,
				    parent_inode, &key,
				    btrfs_inode_type(inode), index);
	if (ret == -EEXIST)
		goto fail_dir_item;
	else if (ret) {
		btrfs_abort_transaction(trans, root, ret);
		return ret;
	}

	btrfs_i_size_write(parent_inode, parent_inode->i_size +
			   name_len * 2);
	parent_inode->i_mtime = parent_inode->i_ctime = CURRENT_TIME;
	ret = btrfs_update_inode(trans, root, parent_inode);
	if (ret)
		btrfs_abort_transaction(trans, root, ret);
	return ret;

fail_dir_item:
	if (unlikely(ino == BTRFS_FIRST_FREE_OBJECTID)) {
		u64 local_index;
		int err;
		err = btrfs_del_root_ref(trans, root->fs_info->tree_root,
				 key.objectid, root->root_key.objectid,
				 parent_ino, &local_index, name, name_len);

	} else if (add_backref) {
		u64 local_index;
		int err;

		err = btrfs_del_inode_ref(trans, root, name, name_len,
					  ino, parent_ino, &local_index);
	}
	return ret;
}

static int btrfs_add_nondir(struct btrfs_trans_handle *trans,
			    struct inode *dir, struct dentry *dentry,
			    struct inode *inode, int backref, u64 index)
{
	int err = btrfs_add_link(trans, dir, inode,
				 dentry->d_name.name, dentry->d_name.len,
				 backref, index);
	if (err > 0)
		err = -EEXIST;
	return err;
}

static int btrfs_mknod(struct inode *dir, struct dentry *dentry,
			umode_t mode, dev_t rdev)
{
	struct btrfs_trans_handle *trans;
	struct btrfs_root *root = BTRFS_I(dir)->root;
	struct inode *inode = NULL;
	int err;
	int drop_inode = 0;
	u64 objectid;
	unsigned long nr = 0;
	u64 index = 0;

	if (!new_valid_dev(rdev))
		return -EINVAL;

	trans = btrfs_start_transaction(root, 5);
	if (IS_ERR(trans))
		return PTR_ERR(trans);

	err = btrfs_find_free_ino(root, &objectid);
	if (err)
		goto out_unlock;

	inode = btrfs_new_inode(trans, root, dir, dentry->d_name.name,
				dentry->d_name.len, btrfs_ino(dir), objectid,
				mode, &index);
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		goto out_unlock;
	}

	err = btrfs_init_inode_security(trans, inode, dir, &dentry->d_name);
	if (err) {
		drop_inode = 1;
		goto out_unlock;
	}


	inode->i_op = &btrfs_special_inode_operations;
	err = btrfs_add_nondir(trans, dir, dentry, inode, 0, index);
	if (err)
		drop_inode = 1;
	else {
		init_special_inode(inode, inode->i_mode, rdev);
		btrfs_update_inode(trans, root, inode);
		d_instantiate(dentry, inode);
	}
out_unlock:
	nr = trans->blocks_used;
	btrfs_end_transaction(trans, root);
	btrfs_btree_balance_dirty(root, nr);
	if (drop_inode) {
		inode_dec_link_count(inode);
		iput(inode);
	}
	return err;
}

static int btrfs_create(struct inode *dir, struct dentry *dentry,
			umode_t mode, struct nameidata *nd)
{
	struct btrfs_trans_handle *trans;
	struct btrfs_root *root = BTRFS_I(dir)->root;
	struct inode *inode = NULL;
	int drop_inode = 0;
	int err;
	unsigned long nr = 0;
	u64 objectid;
	u64 index = 0;

	trans = btrfs_start_transaction(root, 5);
	if (IS_ERR(trans))
		return PTR_ERR(trans);

	err = btrfs_find_free_ino(root, &objectid);
	if (err)
		goto out_unlock;

	inode = btrfs_new_inode(trans, root, dir, dentry->d_name.name,
				dentry->d_name.len, btrfs_ino(dir), objectid,
				mode, &index);
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		goto out_unlock;
	}

	err = btrfs_init_inode_security(trans, inode, dir, &dentry->d_name);
	if (err) {
		drop_inode = 1;
		goto out_unlock;
	}

	inode->i_fop = &btrfs_file_operations;
	inode->i_op = &btrfs_file_inode_operations;

	err = btrfs_add_nondir(trans, dir, dentry, inode, 0, index);
	if (err)
		drop_inode = 1;
	else {
		inode->i_mapping->a_ops = &btrfs_aops;
		inode->i_mapping->backing_dev_info = &root->fs_info->bdi;
		BTRFS_I(inode)->io_tree.ops = &btrfs_extent_io_ops;
		d_instantiate(dentry, inode);
	}
out_unlock:
	nr = trans->blocks_used;
	btrfs_end_transaction(trans, root);
	if (drop_inode) {
		inode_dec_link_count(inode);
		iput(inode);
	}
	btrfs_btree_balance_dirty(root, nr);
	return err;
}

static int btrfs_link(struct dentry *old_dentry, struct inode *dir,
		      struct dentry *dentry)
{
	struct btrfs_trans_handle *trans;
	struct btrfs_root *root = BTRFS_I(dir)->root;
	struct inode *inode = old_dentry->d_inode;
	u64 index;
	unsigned long nr = 0;
	int err;
	int drop_inode = 0;

	
	if (root->objectid != BTRFS_I(inode)->root->objectid)
		return -EXDEV;

	if (inode->i_nlink == ~0U)
		return -EMLINK;

	err = btrfs_set_inode_index(dir, &index);
	if (err)
		goto fail;

	trans = btrfs_start_transaction(root, 5);
	if (IS_ERR(trans)) {
		err = PTR_ERR(trans);
		goto fail;
	}

	btrfs_inc_nlink(inode);
	inode->i_ctime = CURRENT_TIME;
	ihold(inode);

	err = btrfs_add_nondir(trans, dir, dentry, inode, 1, index);

	if (err) {
		drop_inode = 1;
	} else {
		struct dentry *parent = dentry->d_parent;
		err = btrfs_update_inode(trans, root, inode);
		if (err)
			goto fail;
		d_instantiate(dentry, inode);
		btrfs_log_new_name(trans, inode, NULL, parent);
	}

	nr = trans->blocks_used;
	btrfs_end_transaction(trans, root);
fail:
	if (drop_inode) {
		inode_dec_link_count(inode);
		iput(inode);
	}
	btrfs_btree_balance_dirty(root, nr);
	return err;
}

static int btrfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
	struct inode *inode = NULL;
	struct btrfs_trans_handle *trans;
	struct btrfs_root *root = BTRFS_I(dir)->root;
	int err = 0;
	int drop_on_err = 0;
	u64 objectid = 0;
	u64 index = 0;
	unsigned long nr = 1;

	trans = btrfs_start_transaction(root, 5);
	if (IS_ERR(trans))
		return PTR_ERR(trans);

	err = btrfs_find_free_ino(root, &objectid);
	if (err)
		goto out_fail;

	inode = btrfs_new_inode(trans, root, dir, dentry->d_name.name,
				dentry->d_name.len, btrfs_ino(dir), objectid,
				S_IFDIR | mode, &index);
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		goto out_fail;
	}

	drop_on_err = 1;

	err = btrfs_init_inode_security(trans, inode, dir, &dentry->d_name);
	if (err)
		goto out_fail;

	inode->i_op = &btrfs_dir_inode_operations;
	inode->i_fop = &btrfs_dir_file_operations;

	btrfs_i_size_write(inode, 0);
	err = btrfs_update_inode(trans, root, inode);
	if (err)
		goto out_fail;

	err = btrfs_add_link(trans, dir, inode, dentry->d_name.name,
			     dentry->d_name.len, 0, index);
	if (err)
		goto out_fail;

	d_instantiate(dentry, inode);
	drop_on_err = 0;

out_fail:
	nr = trans->blocks_used;
	btrfs_end_transaction(trans, root);
	if (drop_on_err)
		iput(inode);
	btrfs_btree_balance_dirty(root, nr);
	return err;
}

static int merge_extent_mapping(struct extent_map_tree *em_tree,
				struct extent_map *existing,
				struct extent_map *em,
				u64 map_start, u64 map_len)
{
	u64 start_diff;

	BUG_ON(map_start < em->start || map_start >= extent_map_end(em));
	start_diff = map_start - em->start;
	em->start = map_start;
	em->len = map_len;
	if (em->block_start < EXTENT_MAP_LAST_BYTE &&
	    !test_bit(EXTENT_FLAG_COMPRESSED, &em->flags)) {
		em->block_start += start_diff;
		em->block_len -= start_diff;
	}
	return add_extent_mapping(em_tree, em);
}

static noinline int uncompress_inline(struct btrfs_path *path,
				      struct inode *inode, struct page *page,
				      size_t pg_offset, u64 extent_offset,
				      struct btrfs_file_extent_item *item)
{
	int ret;
	struct extent_buffer *leaf = path->nodes[0];
	char *tmp;
	size_t max_size;
	unsigned long inline_size;
	unsigned long ptr;
	int compress_type;

	WARN_ON(pg_offset != 0);
	compress_type = btrfs_file_extent_compression(leaf, item);
	max_size = btrfs_file_extent_ram_bytes(leaf, item);
	inline_size = btrfs_file_extent_inline_item_len(leaf,
					btrfs_item_nr(leaf, path->slots[0]));
	tmp = kmalloc(inline_size, GFP_NOFS);
	if (!tmp)
		return -ENOMEM;
	ptr = btrfs_file_extent_inline_start(item);

	read_extent_buffer(leaf, tmp, ptr, inline_size);

	max_size = min_t(unsigned long, PAGE_CACHE_SIZE, max_size);
	ret = btrfs_decompress(compress_type, tmp, page,
			       extent_offset, inline_size, max_size);
	if (ret) {
		char *kaddr = kmap_atomic(page);
		unsigned long copy_size = min_t(u64,
				  PAGE_CACHE_SIZE - pg_offset,
				  max_size - extent_offset);
		memset(kaddr + pg_offset, 0, copy_size);
		kunmap_atomic(kaddr);
	}
	kfree(tmp);
	return 0;
}


struct extent_map *btrfs_get_extent(struct inode *inode, struct page *page,
				    size_t pg_offset, u64 start, u64 len,
				    int create)
{
	int ret;
	int err = 0;
	u64 bytenr;
	u64 extent_start = 0;
	u64 extent_end = 0;
	u64 objectid = btrfs_ino(inode);
	u32 found_type;
	struct btrfs_path *path = NULL;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_file_extent_item *item;
	struct extent_buffer *leaf;
	struct btrfs_key found_key;
	struct extent_map *em = NULL;
	struct extent_map_tree *em_tree = &BTRFS_I(inode)->extent_tree;
	struct extent_io_tree *io_tree = &BTRFS_I(inode)->io_tree;
	struct btrfs_trans_handle *trans = NULL;
	int compress_type;

again:
	read_lock(&em_tree->lock);
	em = lookup_extent_mapping(em_tree, start, len);
	if (em)
		em->bdev = root->fs_info->fs_devices->latest_bdev;
	read_unlock(&em_tree->lock);

	if (em) {
		if (em->start > start || em->start + em->len <= start)
			free_extent_map(em);
		else if (em->block_start == EXTENT_MAP_INLINE && page)
			free_extent_map(em);
		else
			goto out;
	}
	em = alloc_extent_map();
	if (!em) {
		err = -ENOMEM;
		goto out;
	}
	em->bdev = root->fs_info->fs_devices->latest_bdev;
	em->start = EXTENT_MAP_HOLE;
	em->orig_start = EXTENT_MAP_HOLE;
	em->len = (u64)-1;
	em->block_len = (u64)-1;

	if (!path) {
		path = btrfs_alloc_path();
		if (!path) {
			err = -ENOMEM;
			goto out;
		}
		path->reada = 1;
	}

	ret = btrfs_lookup_file_extent(trans, root, path,
				       objectid, start, trans != NULL);
	if (ret < 0) {
		err = ret;
		goto out;
	}

	if (ret != 0) {
		if (path->slots[0] == 0)
			goto not_found;
		path->slots[0]--;
	}

	leaf = path->nodes[0];
	item = btrfs_item_ptr(leaf, path->slots[0],
			      struct btrfs_file_extent_item);
	
	btrfs_item_key_to_cpu(leaf, &found_key, path->slots[0]);
	found_type = btrfs_key_type(&found_key);
	if (found_key.objectid != objectid ||
	    found_type != BTRFS_EXTENT_DATA_KEY) {
		goto not_found;
	}

	found_type = btrfs_file_extent_type(leaf, item);
	extent_start = found_key.offset;
	compress_type = btrfs_file_extent_compression(leaf, item);
	if (found_type == BTRFS_FILE_EXTENT_REG ||
	    found_type == BTRFS_FILE_EXTENT_PREALLOC) {
		extent_end = extent_start +
		       btrfs_file_extent_num_bytes(leaf, item);
	} else if (found_type == BTRFS_FILE_EXTENT_INLINE) {
		size_t size;
		size = btrfs_file_extent_inline_len(leaf, item);
		extent_end = (extent_start + size + root->sectorsize - 1) &
			~((u64)root->sectorsize - 1);
	}

	if (start >= extent_end) {
		path->slots[0]++;
		if (path->slots[0] >= btrfs_header_nritems(leaf)) {
			ret = btrfs_next_leaf(root, path);
			if (ret < 0) {
				err = ret;
				goto out;
			}
			if (ret > 0)
				goto not_found;
			leaf = path->nodes[0];
		}
		btrfs_item_key_to_cpu(leaf, &found_key, path->slots[0]);
		if (found_key.objectid != objectid ||
		    found_key.type != BTRFS_EXTENT_DATA_KEY)
			goto not_found;
		if (start + len <= found_key.offset)
			goto not_found;
		em->start = start;
		em->len = found_key.offset - start;
		goto not_found_em;
	}

	if (found_type == BTRFS_FILE_EXTENT_REG ||
	    found_type == BTRFS_FILE_EXTENT_PREALLOC) {
		em->start = extent_start;
		em->len = extent_end - extent_start;
		em->orig_start = extent_start -
				 btrfs_file_extent_offset(leaf, item);
		bytenr = btrfs_file_extent_disk_bytenr(leaf, item);
		if (bytenr == 0) {
			em->block_start = EXTENT_MAP_HOLE;
			goto insert;
		}
		if (compress_type != BTRFS_COMPRESS_NONE) {
			set_bit(EXTENT_FLAG_COMPRESSED, &em->flags);
			em->compress_type = compress_type;
			em->block_start = bytenr;
			em->block_len = btrfs_file_extent_disk_num_bytes(leaf,
									 item);
		} else {
			bytenr += btrfs_file_extent_offset(leaf, item);
			em->block_start = bytenr;
			em->block_len = em->len;
			if (found_type == BTRFS_FILE_EXTENT_PREALLOC)
				set_bit(EXTENT_FLAG_PREALLOC, &em->flags);
		}
		goto insert;
	} else if (found_type == BTRFS_FILE_EXTENT_INLINE) {
		unsigned long ptr;
		char *map;
		size_t size;
		size_t extent_offset;
		size_t copy_size;

		em->block_start = EXTENT_MAP_INLINE;
		if (!page || create) {
			em->start = extent_start;
			em->len = extent_end - extent_start;
			goto out;
		}

		size = btrfs_file_extent_inline_len(leaf, item);
		extent_offset = page_offset(page) + pg_offset - extent_start;
		copy_size = min_t(u64, PAGE_CACHE_SIZE - pg_offset,
				size - extent_offset);
		em->start = extent_start + extent_offset;
		em->len = (copy_size + root->sectorsize - 1) &
			~((u64)root->sectorsize - 1);
		em->orig_start = EXTENT_MAP_INLINE;
		if (compress_type) {
			set_bit(EXTENT_FLAG_COMPRESSED, &em->flags);
			em->compress_type = compress_type;
		}
		ptr = btrfs_file_extent_inline_start(item) + extent_offset;
		if (create == 0 && !PageUptodate(page)) {
			if (btrfs_file_extent_compression(leaf, item) !=
			    BTRFS_COMPRESS_NONE) {
				ret = uncompress_inline(path, inode, page,
							pg_offset,
							extent_offset, item);
				BUG_ON(ret); 
			} else {
				map = kmap(page);
				read_extent_buffer(leaf, map + pg_offset, ptr,
						   copy_size);
				if (pg_offset + copy_size < PAGE_CACHE_SIZE) {
					memset(map + pg_offset + copy_size, 0,
					       PAGE_CACHE_SIZE - pg_offset -
					       copy_size);
				}
				kunmap(page);
			}
			flush_dcache_page(page);
		} else if (create && PageUptodate(page)) {
			BUG();
			if (!trans) {
				kunmap(page);
				free_extent_map(em);
				em = NULL;

				btrfs_release_path(path);
				trans = btrfs_join_transaction(root);

				if (IS_ERR(trans))
					return ERR_CAST(trans);
				goto again;
			}
			map = kmap(page);
			write_extent_buffer(leaf, map + pg_offset, ptr,
					    copy_size);
			kunmap(page);
			btrfs_mark_buffer_dirty(leaf);
		}
		set_extent_uptodate(io_tree, em->start,
				    extent_map_end(em) - 1, NULL, GFP_NOFS);
		goto insert;
	} else {
		printk(KERN_ERR "btrfs unknown found_type %d\n", found_type);
		WARN_ON(1);
	}
not_found:
	em->start = start;
	em->len = len;
not_found_em:
	em->block_start = EXTENT_MAP_HOLE;
	set_bit(EXTENT_FLAG_VACANCY, &em->flags);
insert:
	btrfs_release_path(path);
	if (em->start > start || extent_map_end(em) <= start) {
		printk(KERN_ERR "Btrfs: bad extent! em: [%llu %llu] passed "
		       "[%llu %llu]\n", (unsigned long long)em->start,
		       (unsigned long long)em->len,
		       (unsigned long long)start,
		       (unsigned long long)len);
		err = -EIO;
		goto out;
	}

	err = 0;
	write_lock(&em_tree->lock);
	ret = add_extent_mapping(em_tree, em);
	if (ret == -EEXIST) {
		struct extent_map *existing;

		ret = 0;

		existing = lookup_extent_mapping(em_tree, start, len);
		if (existing && (existing->start > start ||
		    existing->start + existing->len <= start)) {
			free_extent_map(existing);
			existing = NULL;
		}
		if (!existing) {
			existing = lookup_extent_mapping(em_tree, em->start,
							 em->len);
			if (existing) {
				err = merge_extent_mapping(em_tree, existing,
							   em, start,
							   root->sectorsize);
				free_extent_map(existing);
				if (err) {
					free_extent_map(em);
					em = NULL;
				}
			} else {
				err = -EIO;
				free_extent_map(em);
				em = NULL;
			}
		} else {
			free_extent_map(em);
			em = existing;
			err = 0;
		}
	}
	write_unlock(&em_tree->lock);
out:

	trace_btrfs_get_extent(root, em);

	if (path)
		btrfs_free_path(path);
	if (trans) {
		ret = btrfs_end_transaction(trans, root);
		if (!err)
			err = ret;
	}
	if (err) {
		free_extent_map(em);
		return ERR_PTR(err);
	}
	BUG_ON(!em); 
	return em;
}

struct extent_map *btrfs_get_extent_fiemap(struct inode *inode, struct page *page,
					   size_t pg_offset, u64 start, u64 len,
					   int create)
{
	struct extent_map *em;
	struct extent_map *hole_em = NULL;
	u64 range_start = start;
	u64 end;
	u64 found;
	u64 found_end;
	int err = 0;

	em = btrfs_get_extent(inode, page, pg_offset, start, len, create);
	if (IS_ERR(em))
		return em;
	if (em) {
		if (em->block_start != EXTENT_MAP_HOLE)
			return em;
		else
			hole_em = em;
	}

	
	end = start + len;
	if (end < start)
		end = (u64)-1;
	else
		end -= 1;

	em = NULL;

	
	found = count_range_bits(&BTRFS_I(inode)->io_tree, &range_start,
				 end, len, EXTENT_DELALLOC, 1);
	found_end = range_start + found;
	if (found_end < range_start)
		found_end = (u64)-1;

	if (range_start > end || found_end <= start) {
		em = hole_em;
		hole_em = NULL;
		goto out;
	}

	range_start = max(start,range_start);
	found = found_end - range_start;

	if (found > 0) {
		u64 hole_start = start;
		u64 hole_len = len;

		em = alloc_extent_map();
		if (!em) {
			err = -ENOMEM;
			goto out;
		}
		if (hole_em) {
			u64 calc_end = extent_map_end(hole_em);

			if (calc_end <= start || (hole_em->start > end)) {
				free_extent_map(hole_em);
				hole_em = NULL;
			} else {
				hole_start = max(hole_em->start, start);
				hole_len = calc_end - hole_start;
			}
		}
		em->bdev = NULL;
		if (hole_em && range_start > hole_start) {
			em->len = min(hole_len,
				      range_start - hole_start);
			em->start = hole_start;
			em->orig_start = hole_start;
			em->block_start = hole_em->block_start;
			em->block_len = hole_len;
		} else {
			em->start = range_start;
			em->len = found;
			em->orig_start = range_start;
			em->block_start = EXTENT_MAP_DELALLOC;
			em->block_len = found;
		}
	} else if (hole_em) {
		return hole_em;
	}
out:

	free_extent_map(hole_em);
	if (err) {
		free_extent_map(em);
		return ERR_PTR(err);
	}
	return em;
}

static struct extent_map *btrfs_new_extent_direct(struct inode *inode,
						  struct extent_map *em,
						  u64 start, u64 len)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_trans_handle *trans;
	struct extent_map_tree *em_tree = &BTRFS_I(inode)->extent_tree;
	struct btrfs_key ins;
	u64 alloc_hint;
	int ret;
	bool insert = false;

	if (em->block_start != EXTENT_MAP_HOLE || em->start != start ||
	    em->len != len) {
		free_extent_map(em);
		em = NULL;
		insert = true;
		btrfs_drop_extent_cache(inode, start, start + len - 1, 0);
	}

	trans = btrfs_join_transaction(root);
	if (IS_ERR(trans))
		return ERR_CAST(trans);

	if (start <= BTRFS_I(inode)->disk_i_size && len < 64 * 1024)
		btrfs_add_inode_defrag(trans, inode);

	trans->block_rsv = &root->fs_info->delalloc_block_rsv;

	alloc_hint = get_extent_allocation_hint(inode, start, len);
	ret = btrfs_reserve_extent(trans, root, len, root->sectorsize, 0,
				   alloc_hint, &ins, 1);
	if (ret) {
		em = ERR_PTR(ret);
		goto out;
	}

	if (!em) {
		em = alloc_extent_map();
		if (!em) {
			em = ERR_PTR(-ENOMEM);
			goto out;
		}
	}

	em->start = start;
	em->orig_start = em->start;
	em->len = ins.offset;

	em->block_start = ins.objectid;
	em->block_len = ins.offset;
	em->bdev = root->fs_info->fs_devices->latest_bdev;

	em->flags = 0;
	set_bit(EXTENT_FLAG_PINNED, &em->flags);

	while (insert) {
		write_lock(&em_tree->lock);
		ret = add_extent_mapping(em_tree, em);
		write_unlock(&em_tree->lock);
		if (ret != -EEXIST)
			break;
		btrfs_drop_extent_cache(inode, start, start + em->len - 1, 0);
	}

	ret = btrfs_add_ordered_extent_dio(inode, start, ins.objectid,
					   ins.offset, ins.offset, 0);
	if (ret) {
		btrfs_free_reserved_extent(root, ins.objectid, ins.offset);
		em = ERR_PTR(ret);
	}
out:
	btrfs_end_transaction(trans, root);
	return em;
}

static noinline int can_nocow_odirect(struct btrfs_trans_handle *trans,
				      struct inode *inode, u64 offset, u64 len)
{
	struct btrfs_path *path;
	int ret;
	struct extent_buffer *leaf;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_file_extent_item *fi;
	struct btrfs_key key;
	u64 disk_bytenr;
	u64 backref_offset;
	u64 extent_end;
	u64 num_bytes;
	int slot;
	int found_type;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	ret = btrfs_lookup_file_extent(trans, root, path, btrfs_ino(inode),
				       offset, 0);
	if (ret < 0)
		goto out;

	slot = path->slots[0];
	if (ret == 1) {
		if (slot == 0) {
			
			ret = 0;
			goto out;
		}
		slot--;
	}
	ret = 0;
	leaf = path->nodes[0];
	btrfs_item_key_to_cpu(leaf, &key, slot);
	if (key.objectid != btrfs_ino(inode) ||
	    key.type != BTRFS_EXTENT_DATA_KEY) {
		
		goto out;
	}

	if (key.offset > offset) {
		
		goto out;
	}

	fi = btrfs_item_ptr(leaf, slot, struct btrfs_file_extent_item);
	found_type = btrfs_file_extent_type(leaf, fi);
	if (found_type != BTRFS_FILE_EXTENT_REG &&
	    found_type != BTRFS_FILE_EXTENT_PREALLOC) {
		
		goto out;
	}
	disk_bytenr = btrfs_file_extent_disk_bytenr(leaf, fi);
	backref_offset = btrfs_file_extent_offset(leaf, fi);

	extent_end = key.offset + btrfs_file_extent_num_bytes(leaf, fi);
	if (extent_end < offset + len) {
		
		goto out;
	}

	if (btrfs_extent_readonly(root, disk_bytenr))
		goto out;

	if (btrfs_cross_ref_exist(trans, root, btrfs_ino(inode),
				  key.offset - backref_offset, disk_bytenr))
		goto out;

	disk_bytenr += backref_offset;
	disk_bytenr += offset - key.offset;
	num_bytes = min(offset + len, extent_end) - offset;
	if (csum_exist_in_range(root, disk_bytenr, num_bytes))
				goto out;
	ret = 1;
out:
	btrfs_free_path(path);
	return ret;
}

static int btrfs_get_blocks_direct(struct inode *inode, sector_t iblock,
				   struct buffer_head *bh_result, int create)
{
	struct extent_map *em;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	u64 start = iblock << inode->i_blkbits;
	u64 len = bh_result->b_size;
	struct btrfs_trans_handle *trans;

	em = btrfs_get_extent(inode, NULL, 0, start, len, 0);
	if (IS_ERR(em))
		return PTR_ERR(em);

	if (test_bit(EXTENT_FLAG_COMPRESSED, &em->flags) ||
	    em->block_start == EXTENT_MAP_INLINE) {
		free_extent_map(em);
		return -ENOTBLK;
	}

	
	if (!create && (em->block_start == EXTENT_MAP_HOLE ||
			test_bit(EXTENT_FLAG_PREALLOC, &em->flags))) {
		free_extent_map(em);
		
		unlock_extent(&BTRFS_I(inode)->io_tree, start,
			      start + root->sectorsize - 1);
		return 0;
	}

	if (!create) {
		len = em->len - (start - em->start);
		goto map;
	}

	if (test_bit(EXTENT_FLAG_PREALLOC, &em->flags) ||
	    ((BTRFS_I(inode)->flags & BTRFS_INODE_NODATACOW) &&
	     em->block_start != EXTENT_MAP_HOLE)) {
		int type;
		int ret;
		u64 block_start;

		if (test_bit(EXTENT_FLAG_PREALLOC, &em->flags))
			type = BTRFS_ORDERED_PREALLOC;
		else
			type = BTRFS_ORDERED_NOCOW;
		len = min(len, em->len - (start - em->start));
		block_start = em->block_start + (start - em->start);

		trans = btrfs_join_transaction(root);
		if (IS_ERR(trans))
			goto must_cow;

		if (can_nocow_odirect(trans, inode, start, len) == 1) {
			ret = btrfs_add_ordered_extent_dio(inode, start,
					   block_start, len, len, type);
			btrfs_end_transaction(trans, root);
			if (ret) {
				free_extent_map(em);
				return ret;
			}
			goto unlock;
		}
		btrfs_end_transaction(trans, root);
	}
must_cow:
	len = bh_result->b_size;
	em = btrfs_new_extent_direct(inode, em, start, len);
	if (IS_ERR(em))
		return PTR_ERR(em);
	len = min(len, em->len - (start - em->start));
unlock:
	clear_extent_bit(&BTRFS_I(inode)->io_tree, start, start + len - 1,
			  EXTENT_LOCKED | EXTENT_DELALLOC | EXTENT_DIRTY, 1,
			  0, NULL, GFP_NOFS);
map:
	bh_result->b_blocknr = (em->block_start + (start - em->start)) >>
		inode->i_blkbits;
	bh_result->b_size = len;
	bh_result->b_bdev = em->bdev;
	set_buffer_mapped(bh_result);
	if (create && !test_bit(EXTENT_FLAG_PREALLOC, &em->flags))
		set_buffer_new(bh_result);

	free_extent_map(em);

	return 0;
}

struct btrfs_dio_private {
	struct inode *inode;
	u64 logical_offset;
	u64 disk_bytenr;
	u64 bytes;
	u32 *csums;
	void *private;

	
	atomic_t pending_bios;

	
	int errors;

	struct bio *orig_bio;
};

static void btrfs_endio_direct_read(struct bio *bio, int err)
{
	struct btrfs_dio_private *dip = bio->bi_private;
	struct bio_vec *bvec_end = bio->bi_io_vec + bio->bi_vcnt - 1;
	struct bio_vec *bvec = bio->bi_io_vec;
	struct inode *inode = dip->inode;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	u64 start;
	u32 *private = dip->csums;

	start = dip->logical_offset;
	do {
		if (!(BTRFS_I(inode)->flags & BTRFS_INODE_NODATASUM)) {
			struct page *page = bvec->bv_page;
			char *kaddr;
			u32 csum = ~(u32)0;
			unsigned long flags;

			local_irq_save(flags);
			kaddr = kmap_atomic(page);
			csum = btrfs_csum_data(root, kaddr + bvec->bv_offset,
					       csum, bvec->bv_len);
			btrfs_csum_final(csum, (char *)&csum);
			kunmap_atomic(kaddr);
			local_irq_restore(flags);

			flush_dcache_page(bvec->bv_page);
			if (csum != *private) {
				printk(KERN_ERR "btrfs csum failed ino %llu off"
				      " %llu csum %u private %u\n",
				      (unsigned long long)btrfs_ino(inode),
				      (unsigned long long)start,
				      csum, *private);
				err = -EIO;
			}
		}

		start += bvec->bv_len;
		private++;
		bvec++;
	} while (bvec <= bvec_end);

	unlock_extent(&BTRFS_I(inode)->io_tree, dip->logical_offset,
		      dip->logical_offset + dip->bytes - 1);
	bio->bi_private = dip->private;

	kfree(dip->csums);
	kfree(dip);

	
	if (err)
		clear_bit(BIO_UPTODATE, &bio->bi_flags);
	dio_end_io(bio, err);
}

static void btrfs_endio_direct_write(struct bio *bio, int err)
{
	struct btrfs_dio_private *dip = bio->bi_private;
	struct inode *inode = dip->inode;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_trans_handle *trans;
	struct btrfs_ordered_extent *ordered = NULL;
	struct extent_state *cached_state = NULL;
	u64 ordered_offset = dip->logical_offset;
	u64 ordered_bytes = dip->bytes;
	int ret;

	if (err)
		goto out_done;
again:
	ret = btrfs_dec_test_first_ordered_pending(inode, &ordered,
						   &ordered_offset,
						   ordered_bytes);
	if (!ret)
		goto out_test;

	BUG_ON(!ordered);

	trans = btrfs_join_transaction(root);
	if (IS_ERR(trans)) {
		err = -ENOMEM;
		goto out;
	}
	trans->block_rsv = &root->fs_info->delalloc_block_rsv;

	if (test_bit(BTRFS_ORDERED_NOCOW, &ordered->flags)) {
		ret = btrfs_ordered_update_i_size(inode, 0, ordered);
		if (!ret)
			err = btrfs_update_inode_fallback(trans, root, inode);
		goto out;
	}

	lock_extent_bits(&BTRFS_I(inode)->io_tree, ordered->file_offset,
			 ordered->file_offset + ordered->len - 1, 0,
			 &cached_state);

	if (test_bit(BTRFS_ORDERED_PREALLOC, &ordered->flags)) {
		ret = btrfs_mark_extent_written(trans, inode,
						ordered->file_offset,
						ordered->file_offset +
						ordered->len);
		if (ret) {
			err = ret;
			goto out_unlock;
		}
	} else {
		ret = insert_reserved_file_extent(trans, inode,
						  ordered->file_offset,
						  ordered->start,
						  ordered->disk_len,
						  ordered->len,
						  ordered->len,
						  0, 0, 0,
						  BTRFS_FILE_EXTENT_REG);
		unpin_extent_cache(&BTRFS_I(inode)->extent_tree,
				   ordered->file_offset, ordered->len);
		if (ret) {
			err = ret;
			WARN_ON(1);
			goto out_unlock;
		}
	}

	add_pending_csums(trans, inode, ordered->file_offset, &ordered->list);
	ret = btrfs_ordered_update_i_size(inode, 0, ordered);
	if (!ret || !test_bit(BTRFS_ORDERED_PREALLOC, &ordered->flags))
		btrfs_update_inode_fallback(trans, root, inode);
	ret = 0;
out_unlock:
	unlock_extent_cached(&BTRFS_I(inode)->io_tree, ordered->file_offset,
			     ordered->file_offset + ordered->len - 1,
			     &cached_state, GFP_NOFS);
out:
	btrfs_delalloc_release_metadata(inode, ordered->len);
	btrfs_end_transaction(trans, root);
	ordered_offset = ordered->file_offset + ordered->len;
	btrfs_put_ordered_extent(ordered);
	btrfs_put_ordered_extent(ordered);

out_test:
	if (ordered_offset < dip->logical_offset + dip->bytes) {
		ordered_bytes = dip->logical_offset + dip->bytes -
			ordered_offset;
		goto again;
	}
out_done:
	bio->bi_private = dip->private;

	kfree(dip->csums);
	kfree(dip);

	
	if (err)
		clear_bit(BIO_UPTODATE, &bio->bi_flags);
	dio_end_io(bio, err);
}

static int __btrfs_submit_bio_start_direct_io(struct inode *inode, int rw,
				    struct bio *bio, int mirror_num,
				    unsigned long bio_flags, u64 offset)
{
	int ret;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	ret = btrfs_csum_one_bio(root, inode, bio, offset, 1);
	BUG_ON(ret); 
	return 0;
}

static void btrfs_end_dio_bio(struct bio *bio, int err)
{
	struct btrfs_dio_private *dip = bio->bi_private;

	if (err) {
		printk(KERN_ERR "btrfs direct IO failed ino %llu rw %lu "
		      "sector %#Lx len %u err no %d\n",
		      (unsigned long long)btrfs_ino(dip->inode), bio->bi_rw,
		      (unsigned long long)bio->bi_sector, bio->bi_size, err);
		dip->errors = 1;

		smp_mb__before_atomic_dec();
	}

	
	if (!atomic_dec_and_test(&dip->pending_bios))
		goto out;

	if (dip->errors)
		bio_io_error(dip->orig_bio);
	else {
		set_bit(BIO_UPTODATE, &dip->orig_bio->bi_flags);
		bio_endio(dip->orig_bio, 0);
	}
out:
	bio_put(bio);
}

static struct bio *btrfs_dio_bio_alloc(struct block_device *bdev,
				       u64 first_sector, gfp_t gfp_flags)
{
	int nr_vecs = bio_get_nr_vecs(bdev);
	return btrfs_bio_alloc(bdev, first_sector, nr_vecs, gfp_flags);
}

static inline int __btrfs_submit_dio_bio(struct bio *bio, struct inode *inode,
					 int rw, u64 file_offset, int skip_sum,
					 u32 *csums, int async_submit)
{
	int write = rw & REQ_WRITE;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	int ret;

	bio_get(bio);
	ret = btrfs_bio_wq_end_io(root->fs_info, bio, 0);
	if (ret)
		goto err;

	if (skip_sum)
		goto map;

	if (write && async_submit) {
		ret = btrfs_wq_submit_bio(root->fs_info,
				   inode, rw, bio, 0, 0,
				   file_offset,
				   __btrfs_submit_bio_start_direct_io,
				   __btrfs_submit_bio_done);
		goto err;
	} else if (write) {
		ret = btrfs_csum_one_bio(root, inode, bio, file_offset, 1);
		if (ret)
			goto err;
	} else if (!skip_sum) {
		ret = btrfs_lookup_bio_sums_dio(root, inode, bio,
					  file_offset, csums);
		if (ret)
			goto err;
	}

map:
	ret = btrfs_map_bio(root, rw, bio, 0, async_submit);
err:
	bio_put(bio);
	return ret;
}

static int btrfs_submit_direct_hook(int rw, struct btrfs_dio_private *dip,
				    int skip_sum)
{
	struct inode *inode = dip->inode;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_mapping_tree *map_tree = &root->fs_info->mapping_tree;
	struct bio *bio;
	struct bio *orig_bio = dip->orig_bio;
	struct bio_vec *bvec = orig_bio->bi_io_vec;
	u64 start_sector = orig_bio->bi_sector;
	u64 file_offset = dip->logical_offset;
	u64 submit_len = 0;
	u64 map_length;
	int nr_pages = 0;
	u32 *csums = dip->csums;
	int ret = 0;
	int async_submit = 0;
	int write = rw & REQ_WRITE;

	map_length = orig_bio->bi_size;
	ret = btrfs_map_block(map_tree, READ, start_sector << 9,
			      &map_length, NULL, 0);
	if (ret) {
		bio_put(orig_bio);
		return -EIO;
	}

	if (map_length >= orig_bio->bi_size) {
		bio = orig_bio;
		goto submit;
	}

	async_submit = 1;
	bio = btrfs_dio_bio_alloc(orig_bio->bi_bdev, start_sector, GFP_NOFS);
	if (!bio)
		return -ENOMEM;
	bio->bi_private = dip;
	bio->bi_end_io = btrfs_end_dio_bio;
	atomic_inc(&dip->pending_bios);

	while (bvec <= (orig_bio->bi_io_vec + orig_bio->bi_vcnt - 1)) {
		if (unlikely(map_length < submit_len + bvec->bv_len ||
		    bio_add_page(bio, bvec->bv_page, bvec->bv_len,
				 bvec->bv_offset) < bvec->bv_len)) {
			atomic_inc(&dip->pending_bios);
			ret = __btrfs_submit_dio_bio(bio, inode, rw,
						     file_offset, skip_sum,
						     csums, async_submit);
			if (ret) {
				bio_put(bio);
				atomic_dec(&dip->pending_bios);
				goto out_err;
			}

			
			if (!write && !skip_sum)
				csums = csums + nr_pages;
			start_sector += submit_len >> 9;
			file_offset += submit_len;

			submit_len = 0;
			nr_pages = 0;

			bio = btrfs_dio_bio_alloc(orig_bio->bi_bdev,
						  start_sector, GFP_NOFS);
			if (!bio)
				goto out_err;
			bio->bi_private = dip;
			bio->bi_end_io = btrfs_end_dio_bio;

			map_length = orig_bio->bi_size;
			ret = btrfs_map_block(map_tree, READ, start_sector << 9,
					      &map_length, NULL, 0);
			if (ret) {
				bio_put(bio);
				goto out_err;
			}
		} else {
			submit_len += bvec->bv_len;
			nr_pages ++;
			bvec++;
		}
	}

submit:
	ret = __btrfs_submit_dio_bio(bio, inode, rw, file_offset, skip_sum,
				     csums, async_submit);
	if (!ret)
		return 0;

	bio_put(bio);
out_err:
	dip->errors = 1;
	smp_mb__before_atomic_dec();
	if (atomic_dec_and_test(&dip->pending_bios))
		bio_io_error(dip->orig_bio);

	
	return 0;
}

static void btrfs_submit_direct(int rw, struct bio *bio, struct inode *inode,
				loff_t file_offset)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_dio_private *dip;
	struct bio_vec *bvec = bio->bi_io_vec;
	int skip_sum;
	int write = rw & REQ_WRITE;
	int ret = 0;

	skip_sum = BTRFS_I(inode)->flags & BTRFS_INODE_NODATASUM;

	dip = kmalloc(sizeof(*dip), GFP_NOFS);
	if (!dip) {
		ret = -ENOMEM;
		goto free_ordered;
	}
	dip->csums = NULL;

	
	if (!write && !skip_sum) {
		dip->csums = kmalloc(sizeof(u32) * bio->bi_vcnt, GFP_NOFS);
		if (!dip->csums) {
			kfree(dip);
			ret = -ENOMEM;
			goto free_ordered;
		}
	}

	dip->private = bio->bi_private;
	dip->inode = inode;
	dip->logical_offset = file_offset;

	dip->bytes = 0;
	do {
		dip->bytes += bvec->bv_len;
		bvec++;
	} while (bvec <= (bio->bi_io_vec + bio->bi_vcnt - 1));

	dip->disk_bytenr = (u64)bio->bi_sector << 9;
	bio->bi_private = dip;
	dip->errors = 0;
	dip->orig_bio = bio;
	atomic_set(&dip->pending_bios, 0);

	if (write)
		bio->bi_end_io = btrfs_endio_direct_write;
	else
		bio->bi_end_io = btrfs_endio_direct_read;

	ret = btrfs_submit_direct_hook(rw, dip, skip_sum);
	if (!ret)
		return;
free_ordered:
	if (write) {
		struct btrfs_ordered_extent *ordered;
		ordered = btrfs_lookup_ordered_extent(inode, file_offset);
		if (!test_bit(BTRFS_ORDERED_PREALLOC, &ordered->flags) &&
		    !test_bit(BTRFS_ORDERED_NOCOW, &ordered->flags))
			btrfs_free_reserved_extent(root, ordered->start,
						   ordered->disk_len);
		btrfs_put_ordered_extent(ordered);
		btrfs_put_ordered_extent(ordered);
	}
	bio_endio(bio, ret);
}

static ssize_t check_direct_IO(struct btrfs_root *root, int rw, struct kiocb *iocb,
			const struct iovec *iov, loff_t offset,
			unsigned long nr_segs)
{
	int seg;
	int i;
	size_t size;
	unsigned long addr;
	unsigned blocksize_mask = root->sectorsize - 1;
	ssize_t retval = -EINVAL;
	loff_t end = offset;

	if (offset & blocksize_mask)
		goto out;

	
	for (seg = 0; seg < nr_segs; seg++) {
		addr = (unsigned long)iov[seg].iov_base;
		size = iov[seg].iov_len;
		end += size;
		if ((addr & blocksize_mask) || (size & blocksize_mask))
			goto out;

		
		if (rw & WRITE)
			continue;

		for (i = seg + 1; i < nr_segs; i++) {
			if (iov[seg].iov_base == iov[i].iov_base)
				goto out;
		}
	}
	retval = 0;
out:
	return retval;
}
static ssize_t btrfs_direct_IO(int rw, struct kiocb *iocb,
			const struct iovec *iov, loff_t offset,
			unsigned long nr_segs)
{
	struct file *file = iocb->ki_filp;
	struct inode *inode = file->f_mapping->host;
	struct btrfs_ordered_extent *ordered;
	struct extent_state *cached_state = NULL;
	u64 lockstart, lockend;
	ssize_t ret;
	int writing = rw & WRITE;
	int write_bits = 0;
	size_t count = iov_length(iov, nr_segs);

	if (check_direct_IO(BTRFS_I(inode)->root, rw, iocb, iov,
			    offset, nr_segs)) {
		return 0;
	}

	lockstart = offset;
	lockend = offset + count - 1;

	if (writing) {
		ret = btrfs_delalloc_reserve_space(inode, count);
		if (ret)
			goto out;
	}

	while (1) {
		lock_extent_bits(&BTRFS_I(inode)->io_tree, lockstart, lockend,
				 0, &cached_state);
		ordered = btrfs_lookup_ordered_range(inode, lockstart,
						     lockend - lockstart + 1);
		if (!ordered)
			break;
		unlock_extent_cached(&BTRFS_I(inode)->io_tree, lockstart, lockend,
				     &cached_state, GFP_NOFS);
		btrfs_start_ordered_extent(inode, ordered, 1);
		btrfs_put_ordered_extent(ordered);
		cond_resched();
	}

	if (writing) {
		write_bits = EXTENT_DELALLOC | EXTENT_DO_ACCOUNTING;
		ret = set_extent_bit(&BTRFS_I(inode)->io_tree, lockstart, lockend,
				     EXTENT_DELALLOC, NULL, &cached_state,
				     GFP_NOFS);
		if (ret) {
			clear_extent_bit(&BTRFS_I(inode)->io_tree, lockstart,
					 lockend, EXTENT_LOCKED | write_bits,
					 1, 0, &cached_state, GFP_NOFS);
			goto out;
		}
	}

	free_extent_state(cached_state);
	cached_state = NULL;

	ret = __blockdev_direct_IO(rw, iocb, inode,
		   BTRFS_I(inode)->root->fs_info->fs_devices->latest_bdev,
		   iov, offset, nr_segs, btrfs_get_blocks_direct, NULL,
		   btrfs_submit_direct, 0);

	if (ret < 0 && ret != -EIOCBQUEUED) {
		clear_extent_bit(&BTRFS_I(inode)->io_tree, offset,
			      offset + iov_length(iov, nr_segs) - 1,
			      EXTENT_LOCKED | write_bits, 1, 0,
			      &cached_state, GFP_NOFS);
	} else if (ret >= 0 && ret < iov_length(iov, nr_segs)) {
		clear_extent_bit(&BTRFS_I(inode)->io_tree, offset + ret,
			      offset + iov_length(iov, nr_segs) - 1,
			      EXTENT_LOCKED | write_bits, 1, 0,
			      &cached_state, GFP_NOFS);
	}
out:
	free_extent_state(cached_state);
	return ret;
}

static int btrfs_fiemap(struct inode *inode, struct fiemap_extent_info *fieinfo,
		__u64 start, __u64 len)
{
	return extent_fiemap(inode, fieinfo, start, len, btrfs_get_extent_fiemap);
}

int btrfs_readpage(struct file *file, struct page *page)
{
	struct extent_io_tree *tree;
	tree = &BTRFS_I(page->mapping->host)->io_tree;
	return extent_read_full_page(tree, page, btrfs_get_extent, 0);
}

static int btrfs_writepage(struct page *page, struct writeback_control *wbc)
{
	struct extent_io_tree *tree;


	if (current->flags & PF_MEMALLOC) {
		redirty_page_for_writepage(wbc, page);
		unlock_page(page);
		return 0;
	}
	tree = &BTRFS_I(page->mapping->host)->io_tree;
	return extent_write_full_page(tree, page, btrfs_get_extent, wbc);
}

int btrfs_writepages(struct address_space *mapping,
		     struct writeback_control *wbc)
{
	struct extent_io_tree *tree;

	tree = &BTRFS_I(mapping->host)->io_tree;
	return extent_writepages(tree, mapping, btrfs_get_extent, wbc);
}

static int
btrfs_readpages(struct file *file, struct address_space *mapping,
		struct list_head *pages, unsigned nr_pages)
{
	struct extent_io_tree *tree;
	tree = &BTRFS_I(mapping->host)->io_tree;
	return extent_readpages(tree, mapping, pages, nr_pages,
				btrfs_get_extent);
}
static int __btrfs_releasepage(struct page *page, gfp_t gfp_flags)
{
	struct extent_io_tree *tree;
	struct extent_map_tree *map;
	int ret;

	tree = &BTRFS_I(page->mapping->host)->io_tree;
	map = &BTRFS_I(page->mapping->host)->extent_tree;
	ret = try_release_extent_mapping(map, tree, page, gfp_flags);
	if (ret == 1) {
		ClearPagePrivate(page);
		set_page_private(page, 0);
		page_cache_release(page);
	}
	return ret;
}

static int btrfs_releasepage(struct page *page, gfp_t gfp_flags)
{
	if (PageWriteback(page) || PageDirty(page))
		return 0;
	return __btrfs_releasepage(page, gfp_flags & GFP_NOFS);
}

static void btrfs_invalidatepage(struct page *page, unsigned long offset)
{
	struct extent_io_tree *tree;
	struct btrfs_ordered_extent *ordered;
	struct extent_state *cached_state = NULL;
	u64 page_start = page_offset(page);
	u64 page_end = page_start + PAGE_CACHE_SIZE - 1;


	wait_on_page_writeback(page);

	tree = &BTRFS_I(page->mapping->host)->io_tree;
	if (offset) {
		btrfs_releasepage(page, GFP_NOFS);
		return;
	}
	lock_extent_bits(tree, page_start, page_end, 0, &cached_state);
	ordered = btrfs_lookup_ordered_extent(page->mapping->host,
					   page_offset(page));
	if (ordered) {
		clear_extent_bit(tree, page_start, page_end,
				 EXTENT_DIRTY | EXTENT_DELALLOC |
				 EXTENT_LOCKED | EXTENT_DO_ACCOUNTING, 1, 0,
				 &cached_state, GFP_NOFS);
		if (TestClearPagePrivate2(page)) {
			btrfs_finish_ordered_io(page->mapping->host,
						page_start, page_end);
		}
		btrfs_put_ordered_extent(ordered);
		cached_state = NULL;
		lock_extent_bits(tree, page_start, page_end, 0, &cached_state);
	}
	clear_extent_bit(tree, page_start, page_end,
		 EXTENT_LOCKED | EXTENT_DIRTY | EXTENT_DELALLOC |
		 EXTENT_DO_ACCOUNTING, 1, 1, &cached_state, GFP_NOFS);
	__btrfs_releasepage(page, GFP_NOFS);

	ClearPageChecked(page);
	if (PagePrivate(page)) {
		ClearPagePrivate(page);
		set_page_private(page, 0);
		page_cache_release(page);
	}
}

/*
 * btrfs_page_mkwrite() is not allowed to change the file size as it gets
 * called from a page fault handler when a page is first dirtied. Hence we must
 * be careful to check for EOF conditions here. We set the page up correctly
 * for a written page which means we get ENOSPC checking when writing into
 * holes and correct delalloc and unwritten extent mapping on filesystems that
 * support these features.
 *
 * We are not allowed to take the i_mutex here so we have to play games to
 * protect against truncate races as the page could now be beyond EOF.  Because
 * vmtruncate() writes the inode size before removing pages, once we have the
 * page lock we can determine safely if the page is beyond EOF. If it is not
 * beyond EOF, then the page is guaranteed safe against truncation until we
 * unlock the page.
 */
int btrfs_page_mkwrite(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct page *page = vmf->page;
	struct inode *inode = fdentry(vma->vm_file)->d_inode;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct extent_io_tree *io_tree = &BTRFS_I(inode)->io_tree;
	struct btrfs_ordered_extent *ordered;
	struct extent_state *cached_state = NULL;
	char *kaddr;
	unsigned long zero_start;
	loff_t size;
	int ret;
	int reserved = 0;
	u64 page_start;
	u64 page_end;

	ret  = btrfs_delalloc_reserve_space(inode, PAGE_CACHE_SIZE);
	if (!ret) {
		ret = btrfs_update_time(vma->vm_file);
		reserved = 1;
	}
	if (ret) {
		if (ret == -ENOMEM)
			ret = VM_FAULT_OOM;
		else 
			ret = VM_FAULT_SIGBUS;
		if (reserved)
			goto out;
		goto out_noreserve;
	}

	ret = VM_FAULT_NOPAGE; 
again:
	lock_page(page);
	size = i_size_read(inode);
	page_start = page_offset(page);
	page_end = page_start + PAGE_CACHE_SIZE - 1;

	if ((page->mapping != inode->i_mapping) ||
	    (page_start >= size)) {
		
		goto out_unlock;
	}
	wait_on_page_writeback(page);

	lock_extent_bits(io_tree, page_start, page_end, 0, &cached_state);
	set_page_extent_mapped(page);

	ordered = btrfs_lookup_ordered_extent(inode, page_start);
	if (ordered) {
		unlock_extent_cached(io_tree, page_start, page_end,
				     &cached_state, GFP_NOFS);
		unlock_page(page);
		btrfs_start_ordered_extent(inode, ordered, 1);
		btrfs_put_ordered_extent(ordered);
		goto again;
	}

	clear_extent_bit(&BTRFS_I(inode)->io_tree, page_start, page_end,
			  EXTENT_DIRTY | EXTENT_DELALLOC | EXTENT_DO_ACCOUNTING,
			  0, 0, &cached_state, GFP_NOFS);

	ret = btrfs_set_extent_delalloc(inode, page_start, page_end,
					&cached_state);
	if (ret) {
		unlock_extent_cached(io_tree, page_start, page_end,
				     &cached_state, GFP_NOFS);
		ret = VM_FAULT_SIGBUS;
		goto out_unlock;
	}
	ret = 0;

	
	if (page_start + PAGE_CACHE_SIZE > size)
		zero_start = size & ~PAGE_CACHE_MASK;
	else
		zero_start = PAGE_CACHE_SIZE;

	if (zero_start != PAGE_CACHE_SIZE) {
		kaddr = kmap(page);
		memset(kaddr + zero_start, 0, PAGE_CACHE_SIZE - zero_start);
		flush_dcache_page(page);
		kunmap(page);
	}
	ClearPageChecked(page);
	set_page_dirty(page);
	SetPageUptodate(page);

	BTRFS_I(inode)->last_trans = root->fs_info->generation;
	BTRFS_I(inode)->last_sub_trans = BTRFS_I(inode)->root->log_transid;

	unlock_extent_cached(io_tree, page_start, page_end, &cached_state, GFP_NOFS);

out_unlock:
	if (!ret)
		return VM_FAULT_LOCKED;
	unlock_page(page);
out:
	btrfs_delalloc_release_space(inode, PAGE_CACHE_SIZE);
out_noreserve:
	return ret;
}

static int btrfs_truncate(struct inode *inode)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_block_rsv *rsv;
	int ret;
	int err = 0;
	struct btrfs_trans_handle *trans;
	unsigned long nr;
	u64 mask = root->sectorsize - 1;
	u64 min_size = btrfs_calc_trunc_metadata_size(root, 1);

	ret = btrfs_truncate_page(inode->i_mapping, inode->i_size);
	if (ret)
		return ret;

	btrfs_wait_ordered_range(inode, inode->i_size & (~mask), (u64)-1);
	btrfs_ordered_update_i_size(inode, inode->i_size, NULL);

	rsv = btrfs_alloc_block_rsv(root);
	if (!rsv)
		return -ENOMEM;
	rsv->size = min_size;

	trans = btrfs_start_transaction(root, 4);
	if (IS_ERR(trans)) {
		err = PTR_ERR(trans);
		goto out;
	}

	
	ret = btrfs_block_rsv_migrate(&root->fs_info->trans_block_rsv, rsv,
				      min_size);
	BUG_ON(ret);

	ret = btrfs_orphan_add(trans, inode);
	if (ret) {
		btrfs_end_transaction(trans, root);
		goto out;
	}

	if (inode->i_size == 0 && BTRFS_I(inode)->ordered_data_close)
		btrfs_add_ordered_operation(trans, root, inode);

	while (1) {
		ret = btrfs_block_rsv_refill(root, rsv, min_size);
		if (ret) {
			if (ret == -EAGAIN)
				goto end_trans;
			err = ret;
			break;
		}

		if (!trans) {
			
			trans = btrfs_start_transaction(root, 1);
			if (IS_ERR(trans)) {
				ret = err = PTR_ERR(trans);
				trans = NULL;
				break;
			}
		}

		trans->block_rsv = rsv;

		ret = btrfs_truncate_inode_items(trans, root, inode,
						 inode->i_size,
						 BTRFS_EXTENT_DATA_KEY);
		if (ret != -EAGAIN) {
			err = ret;
			break;
		}

		trans->block_rsv = &root->fs_info->trans_block_rsv;
		ret = btrfs_update_inode(trans, root, inode);
		if (ret) {
			err = ret;
			break;
		}
end_trans:
		nr = trans->blocks_used;
		btrfs_end_transaction(trans, root);
		trans = NULL;
		btrfs_btree_balance_dirty(root, nr);
	}

	if (ret == 0 && inode->i_nlink > 0) {
		trans->block_rsv = root->orphan_block_rsv;
		ret = btrfs_orphan_del(trans, inode);
		if (ret)
			err = ret;
	} else if (ret && inode->i_nlink > 0) {
		ret = btrfs_orphan_del(NULL, inode);
	}

	if (trans) {
		trans->block_rsv = &root->fs_info->trans_block_rsv;
		ret = btrfs_update_inode(trans, root, inode);
		if (ret && !err)
			err = ret;

		nr = trans->blocks_used;
		ret = btrfs_end_transaction(trans, root);
		btrfs_btree_balance_dirty(root, nr);
	}

out:
	btrfs_free_block_rsv(root, rsv);

	if (ret && !err)
		err = ret;

	return err;
}

int btrfs_create_subvol_root(struct btrfs_trans_handle *trans,
			     struct btrfs_root *new_root, u64 new_dirid)
{
	struct inode *inode;
	int err;
	u64 index = 0;

	inode = btrfs_new_inode(trans, new_root, NULL, "..", 2,
				new_dirid, new_dirid,
				S_IFDIR | (~current_umask() & S_IRWXUGO),
				&index);
	if (IS_ERR(inode))
		return PTR_ERR(inode);
	inode->i_op = &btrfs_dir_inode_operations;
	inode->i_fop = &btrfs_dir_file_operations;

	set_nlink(inode, 1);
	btrfs_i_size_write(inode, 0);

	err = btrfs_update_inode(trans, new_root, inode);

	iput(inode);
	return err;
}

struct inode *btrfs_alloc_inode(struct super_block *sb)
{
	struct btrfs_inode *ei;
	struct inode *inode;

	ei = kmem_cache_alloc(btrfs_inode_cachep, GFP_NOFS);
	if (!ei)
		return NULL;

	ei->root = NULL;
	ei->space_info = NULL;
	ei->generation = 0;
	ei->sequence = 0;
	ei->last_trans = 0;
	ei->last_sub_trans = 0;
	ei->logged_trans = 0;
	ei->delalloc_bytes = 0;
	ei->disk_i_size = 0;
	ei->flags = 0;
	ei->csum_bytes = 0;
	ei->index_cnt = (u64)-1;
	ei->last_unlink_trans = 0;

	spin_lock_init(&ei->lock);
	ei->outstanding_extents = 0;
	ei->reserved_extents = 0;

	ei->ordered_data_close = 0;
	ei->orphan_meta_reserved = 0;
	ei->dummy_inode = 0;
	ei->in_defrag = 0;
	ei->delalloc_meta_reserved = 0;
	ei->force_compress = BTRFS_COMPRESS_NONE;

	ei->delayed_node = NULL;

	inode = &ei->vfs_inode;
	extent_map_tree_init(&ei->extent_tree);
	extent_io_tree_init(&ei->io_tree, &inode->i_data);
	extent_io_tree_init(&ei->io_failure_tree, &inode->i_data);
	ei->io_tree.track_uptodate = 1;
	ei->io_failure_tree.track_uptodate = 1;
	mutex_init(&ei->log_mutex);
	mutex_init(&ei->delalloc_mutex);
	btrfs_ordered_inode_tree_init(&ei->ordered_tree);
	INIT_LIST_HEAD(&ei->i_orphan);
	INIT_LIST_HEAD(&ei->delalloc_inodes);
	INIT_LIST_HEAD(&ei->ordered_operations);
	RB_CLEAR_NODE(&ei->rb_node);

	return inode;
}

static void btrfs_i_callback(struct rcu_head *head)
{
	struct inode *inode = container_of(head, struct inode, i_rcu);
	kmem_cache_free(btrfs_inode_cachep, BTRFS_I(inode));
}

void btrfs_destroy_inode(struct inode *inode)
{
	struct btrfs_ordered_extent *ordered;
	struct btrfs_root *root = BTRFS_I(inode)->root;

	WARN_ON(!list_empty(&inode->i_dentry));
	WARN_ON(inode->i_data.nrpages);
	WARN_ON(BTRFS_I(inode)->outstanding_extents);
	WARN_ON(BTRFS_I(inode)->reserved_extents);
	WARN_ON(BTRFS_I(inode)->delalloc_bytes);
	WARN_ON(BTRFS_I(inode)->csum_bytes);

	if (!root)
		goto free;

	smp_mb();
	if (!list_empty(&BTRFS_I(inode)->ordered_operations)) {
		spin_lock(&root->fs_info->ordered_extent_lock);
		list_del_init(&BTRFS_I(inode)->ordered_operations);
		spin_unlock(&root->fs_info->ordered_extent_lock);
	}

	spin_lock(&root->orphan_lock);
	if (!list_empty(&BTRFS_I(inode)->i_orphan)) {
		printk(KERN_INFO "BTRFS: inode %llu still on the orphan list\n",
		       (unsigned long long)btrfs_ino(inode));
		list_del_init(&BTRFS_I(inode)->i_orphan);
	}
	spin_unlock(&root->orphan_lock);

	while (1) {
		ordered = btrfs_lookup_first_ordered_extent(inode, (u64)-1);
		if (!ordered)
			break;
		else {
			printk(KERN_ERR "btrfs found ordered "
			       "extent %llu %llu on inode cleanup\n",
			       (unsigned long long)ordered->file_offset,
			       (unsigned long long)ordered->len);
			btrfs_remove_ordered_extent(inode, ordered);
			btrfs_put_ordered_extent(ordered);
			btrfs_put_ordered_extent(ordered);
		}
	}
	inode_tree_del(inode);
	btrfs_drop_extent_cache(inode, 0, (u64)-1, 0);
free:
	btrfs_remove_delayed_node(inode);
	call_rcu(&inode->i_rcu, btrfs_i_callback);
}

int btrfs_drop_inode(struct inode *inode)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;

	if (btrfs_root_refs(&root->root_item) == 0 &&
	    !btrfs_is_free_space_inode(root, inode))
		return 1;
	else
		return generic_drop_inode(inode);
}

static void init_once(void *foo)
{
	struct btrfs_inode *ei = (struct btrfs_inode *) foo;

	inode_init_once(&ei->vfs_inode);
}

void btrfs_destroy_cachep(void)
{
	if (btrfs_inode_cachep)
		kmem_cache_destroy(btrfs_inode_cachep);
	if (btrfs_trans_handle_cachep)
		kmem_cache_destroy(btrfs_trans_handle_cachep);
	if (btrfs_transaction_cachep)
		kmem_cache_destroy(btrfs_transaction_cachep);
	if (btrfs_path_cachep)
		kmem_cache_destroy(btrfs_path_cachep);
	if (btrfs_free_space_cachep)
		kmem_cache_destroy(btrfs_free_space_cachep);
}

int btrfs_init_cachep(void)
{
	btrfs_inode_cachep = kmem_cache_create("btrfs_inode_cache",
			sizeof(struct btrfs_inode), 0,
			SLAB_RECLAIM_ACCOUNT | SLAB_MEM_SPREAD, init_once);
	if (!btrfs_inode_cachep)
		goto fail;

	btrfs_trans_handle_cachep = kmem_cache_create("btrfs_trans_handle_cache",
			sizeof(struct btrfs_trans_handle), 0,
			SLAB_RECLAIM_ACCOUNT | SLAB_MEM_SPREAD, NULL);
	if (!btrfs_trans_handle_cachep)
		goto fail;

	btrfs_transaction_cachep = kmem_cache_create("btrfs_transaction_cache",
			sizeof(struct btrfs_transaction), 0,
			SLAB_RECLAIM_ACCOUNT | SLAB_MEM_SPREAD, NULL);
	if (!btrfs_transaction_cachep)
		goto fail;

	btrfs_path_cachep = kmem_cache_create("btrfs_path_cache",
			sizeof(struct btrfs_path), 0,
			SLAB_RECLAIM_ACCOUNT | SLAB_MEM_SPREAD, NULL);
	if (!btrfs_path_cachep)
		goto fail;

	btrfs_free_space_cachep = kmem_cache_create("btrfs_free_space_cache",
			sizeof(struct btrfs_free_space), 0,
			SLAB_RECLAIM_ACCOUNT | SLAB_MEM_SPREAD, NULL);
	if (!btrfs_free_space_cachep)
		goto fail;

	return 0;
fail:
	btrfs_destroy_cachep();
	return -ENOMEM;
}

static int btrfs_getattr(struct vfsmount *mnt,
			 struct dentry *dentry, struct kstat *stat)
{
	struct inode *inode = dentry->d_inode;
	u32 blocksize = inode->i_sb->s_blocksize;

	generic_fillattr(inode, stat);
	stat->dev = BTRFS_I(inode)->root->anon_dev;
	stat->blksize = PAGE_CACHE_SIZE;
	stat->blocks = (ALIGN(inode_get_bytes(inode), blocksize) +
		ALIGN(BTRFS_I(inode)->delalloc_bytes, blocksize)) >> 9;
	return 0;
}

static void fixup_inode_flags(struct inode *dir, struct inode *inode)
{
	struct btrfs_inode *b_dir = BTRFS_I(dir);
	struct btrfs_inode *b_inode = BTRFS_I(inode);

	if (b_dir->flags & BTRFS_INODE_NODATACOW)
		b_inode->flags |= BTRFS_INODE_NODATACOW;
	else
		b_inode->flags &= ~BTRFS_INODE_NODATACOW;

	if (b_dir->flags & BTRFS_INODE_COMPRESS)
		b_inode->flags |= BTRFS_INODE_COMPRESS;
	else
		b_inode->flags &= ~BTRFS_INODE_COMPRESS;
}

static int btrfs_rename(struct inode *old_dir, struct dentry *old_dentry,
			   struct inode *new_dir, struct dentry *new_dentry)
{
	struct btrfs_trans_handle *trans;
	struct btrfs_root *root = BTRFS_I(old_dir)->root;
	struct btrfs_root *dest = BTRFS_I(new_dir)->root;
	struct inode *new_inode = new_dentry->d_inode;
	struct inode *old_inode = old_dentry->d_inode;
	struct timespec ctime = CURRENT_TIME;
	u64 index = 0;
	u64 root_objectid;
	int ret;
	u64 old_ino = btrfs_ino(old_inode);

	if (btrfs_ino(new_dir) == BTRFS_EMPTY_SUBVOL_DIR_OBJECTID)
		return -EPERM;

	
	if (old_ino != BTRFS_FIRST_FREE_OBJECTID && root != dest)
		return -EXDEV;

	if (old_ino == BTRFS_EMPTY_SUBVOL_DIR_OBJECTID ||
	    (new_inode && btrfs_ino(new_inode) == BTRFS_FIRST_FREE_OBJECTID))
		return -ENOTEMPTY;

	if (S_ISDIR(old_inode->i_mode) && new_inode &&
	    new_inode->i_size > BTRFS_EMPTY_DIR_SIZE)
		return -ENOTEMPTY;
	if (new_inode && S_ISREG(old_inode->i_mode) && new_inode->i_size &&
	    old_inode->i_size > BTRFS_ORDERED_OPERATIONS_FLUSH_LIMIT)
		filemap_flush(old_inode->i_mapping);

	
	if (old_ino == BTRFS_FIRST_FREE_OBJECTID)
		down_read(&root->fs_info->subvol_sem);
	trans = btrfs_start_transaction(root, 20);
	if (IS_ERR(trans)) {
                ret = PTR_ERR(trans);
                goto out_notrans;
        }

	if (dest != root)
		btrfs_record_root_in_trans(trans, dest);

	ret = btrfs_set_inode_index(new_dir, &index);
	if (ret)
		goto out_fail;

	if (unlikely(old_ino == BTRFS_FIRST_FREE_OBJECTID)) {
		
		root->fs_info->last_trans_log_full_commit = trans->transid;
	} else {
		ret = btrfs_insert_inode_ref(trans, dest,
					     new_dentry->d_name.name,
					     new_dentry->d_name.len,
					     old_ino,
					     btrfs_ino(new_dir), index);
		if (ret)
			goto out_fail;
		btrfs_pin_log_trans(root);
	}
	if (new_inode && new_inode->i_size && S_ISREG(old_inode->i_mode))
		btrfs_add_ordered_operation(trans, root, old_inode);

	old_dir->i_ctime = old_dir->i_mtime = ctime;
	new_dir->i_ctime = new_dir->i_mtime = ctime;
	old_inode->i_ctime = ctime;

	if (old_dentry->d_parent != new_dentry->d_parent)
		btrfs_record_unlink_dir(trans, old_dir, old_inode, 1);

	if (unlikely(old_ino == BTRFS_FIRST_FREE_OBJECTID)) {
		root_objectid = BTRFS_I(old_inode)->root->root_key.objectid;
		ret = btrfs_unlink_subvol(trans, root, old_dir, root_objectid,
					old_dentry->d_name.name,
					old_dentry->d_name.len);
	} else {
		ret = __btrfs_unlink_inode(trans, root, old_dir,
					old_dentry->d_inode,
					old_dentry->d_name.name,
					old_dentry->d_name.len);
		if (!ret)
			ret = btrfs_update_inode(trans, root, old_inode);
	}
	if (ret) {
		btrfs_abort_transaction(trans, root, ret);
		goto out_fail;
	}

	if (new_inode) {
		new_inode->i_ctime = CURRENT_TIME;
		if (unlikely(btrfs_ino(new_inode) ==
			     BTRFS_EMPTY_SUBVOL_DIR_OBJECTID)) {
			root_objectid = BTRFS_I(new_inode)->location.objectid;
			ret = btrfs_unlink_subvol(trans, dest, new_dir,
						root_objectid,
						new_dentry->d_name.name,
						new_dentry->d_name.len);
			BUG_ON(new_inode->i_nlink == 0);
		} else {
			ret = btrfs_unlink_inode(trans, dest, new_dir,
						 new_dentry->d_inode,
						 new_dentry->d_name.name,
						 new_dentry->d_name.len);
		}
		if (!ret && new_inode->i_nlink == 0) {
			ret = btrfs_orphan_add(trans, new_dentry->d_inode);
			BUG_ON(ret);
		}
		if (ret) {
			btrfs_abort_transaction(trans, root, ret);
			goto out_fail;
		}
	}

	fixup_inode_flags(new_dir, old_inode);

	ret = btrfs_add_link(trans, new_dir, old_inode,
			     new_dentry->d_name.name,
			     new_dentry->d_name.len, 0, index);
	if (ret) {
		btrfs_abort_transaction(trans, root, ret);
		goto out_fail;
	}

	if (old_ino != BTRFS_FIRST_FREE_OBJECTID) {
		struct dentry *parent = new_dentry->d_parent;
		btrfs_log_new_name(trans, old_inode, old_dir, parent);
		btrfs_end_log_trans(root);
	}
out_fail:
	btrfs_end_transaction(trans, root);
out_notrans:
	if (old_ino == BTRFS_FIRST_FREE_OBJECTID)
		up_read(&root->fs_info->subvol_sem);

	return ret;
}

int btrfs_start_delalloc_inodes(struct btrfs_root *root, int delay_iput)
{
	struct list_head *head = &root->fs_info->delalloc_inodes;
	struct btrfs_inode *binode;
	struct inode *inode;

	if (root->fs_info->sb->s_flags & MS_RDONLY)
		return -EROFS;

	spin_lock(&root->fs_info->delalloc_lock);
	while (!list_empty(head)) {
		binode = list_entry(head->next, struct btrfs_inode,
				    delalloc_inodes);
		inode = igrab(&binode->vfs_inode);
		if (!inode)
			list_del_init(&binode->delalloc_inodes);
		spin_unlock(&root->fs_info->delalloc_lock);
		if (inode) {
			filemap_flush(inode->i_mapping);
			if (delay_iput)
				btrfs_add_delayed_iput(inode);
			else
				iput(inode);
		}
		cond_resched();
		spin_lock(&root->fs_info->delalloc_lock);
	}
	spin_unlock(&root->fs_info->delalloc_lock);

	atomic_inc(&root->fs_info->async_submit_draining);
	while (atomic_read(&root->fs_info->nr_async_submits) ||
	      atomic_read(&root->fs_info->async_delalloc_pages)) {
		wait_event(root->fs_info->async_submit_wait,
		   (atomic_read(&root->fs_info->nr_async_submits) == 0 &&
		    atomic_read(&root->fs_info->async_delalloc_pages) == 0));
	}
	atomic_dec(&root->fs_info->async_submit_draining);
	return 0;
}

static int btrfs_symlink(struct inode *dir, struct dentry *dentry,
			 const char *symname)
{
	struct btrfs_trans_handle *trans;
	struct btrfs_root *root = BTRFS_I(dir)->root;
	struct btrfs_path *path;
	struct btrfs_key key;
	struct inode *inode = NULL;
	int err;
	int drop_inode = 0;
	u64 objectid;
	u64 index = 0 ;
	int name_len;
	int datasize;
	unsigned long ptr;
	struct btrfs_file_extent_item *ei;
	struct extent_buffer *leaf;
	unsigned long nr = 0;

	name_len = strlen(symname) + 1;
	if (name_len > BTRFS_MAX_INLINE_DATA_SIZE(root))
		return -ENAMETOOLONG;

	trans = btrfs_start_transaction(root, 5);
	if (IS_ERR(trans))
		return PTR_ERR(trans);

	err = btrfs_find_free_ino(root, &objectid);
	if (err)
		goto out_unlock;

	inode = btrfs_new_inode(trans, root, dir, dentry->d_name.name,
				dentry->d_name.len, btrfs_ino(dir), objectid,
				S_IFLNK|S_IRWXUGO, &index);
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		goto out_unlock;
	}

	err = btrfs_init_inode_security(trans, inode, dir, &dentry->d_name);
	if (err) {
		drop_inode = 1;
		goto out_unlock;
	}

	inode->i_fop = &btrfs_file_operations;
	inode->i_op = &btrfs_file_inode_operations;

	err = btrfs_add_nondir(trans, dir, dentry, inode, 0, index);
	if (err)
		drop_inode = 1;
	else {
		inode->i_mapping->a_ops = &btrfs_aops;
		inode->i_mapping->backing_dev_info = &root->fs_info->bdi;
		BTRFS_I(inode)->io_tree.ops = &btrfs_extent_io_ops;
	}
	if (drop_inode)
		goto out_unlock;

	path = btrfs_alloc_path();
	if (!path) {
		err = -ENOMEM;
		drop_inode = 1;
		goto out_unlock;
	}
	key.objectid = btrfs_ino(inode);
	key.offset = 0;
	btrfs_set_key_type(&key, BTRFS_EXTENT_DATA_KEY);
	datasize = btrfs_file_extent_calc_inline_size(name_len);
	err = btrfs_insert_empty_item(trans, root, path, &key,
				      datasize);
	if (err) {
		drop_inode = 1;
		btrfs_free_path(path);
		goto out_unlock;
	}
	leaf = path->nodes[0];
	ei = btrfs_item_ptr(leaf, path->slots[0],
			    struct btrfs_file_extent_item);
	btrfs_set_file_extent_generation(leaf, ei, trans->transid);
	btrfs_set_file_extent_type(leaf, ei,
				   BTRFS_FILE_EXTENT_INLINE);
	btrfs_set_file_extent_encryption(leaf, ei, 0);
	btrfs_set_file_extent_compression(leaf, ei, 0);
	btrfs_set_file_extent_other_encoding(leaf, ei, 0);
	btrfs_set_file_extent_ram_bytes(leaf, ei, name_len);

	ptr = btrfs_file_extent_inline_start(ei);
	write_extent_buffer(leaf, symname, ptr, name_len);
	btrfs_mark_buffer_dirty(leaf);
	btrfs_free_path(path);

	inode->i_op = &btrfs_symlink_inode_operations;
	inode->i_mapping->a_ops = &btrfs_symlink_aops;
	inode->i_mapping->backing_dev_info = &root->fs_info->bdi;
	inode_set_bytes(inode, name_len);
	btrfs_i_size_write(inode, name_len - 1);
	err = btrfs_update_inode(trans, root, inode);
	if (err)
		drop_inode = 1;

out_unlock:
	if (!err)
		d_instantiate(dentry, inode);
	nr = trans->blocks_used;
	btrfs_end_transaction(trans, root);
	if (drop_inode) {
		inode_dec_link_count(inode);
		iput(inode);
	}
	btrfs_btree_balance_dirty(root, nr);
	return err;
}

static int __btrfs_prealloc_file_range(struct inode *inode, int mode,
				       u64 start, u64 num_bytes, u64 min_size,
				       loff_t actual_len, u64 *alloc_hint,
				       struct btrfs_trans_handle *trans)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_key ins;
	u64 cur_offset = start;
	u64 i_size;
	int ret = 0;
	bool own_trans = true;

	if (trans)
		own_trans = false;
	while (num_bytes > 0) {
		if (own_trans) {
			trans = btrfs_start_transaction(root, 3);
			if (IS_ERR(trans)) {
				ret = PTR_ERR(trans);
				break;
			}
		}

		ret = btrfs_reserve_extent(trans, root, num_bytes, min_size,
					   0, *alloc_hint, &ins, 1);
		if (ret) {
			if (own_trans)
				btrfs_end_transaction(trans, root);
			break;
		}

		ret = insert_reserved_file_extent(trans, inode,
						  cur_offset, ins.objectid,
						  ins.offset, ins.offset,
						  ins.offset, 0, 0, 0,
						  BTRFS_FILE_EXTENT_PREALLOC);
		if (ret) {
			btrfs_abort_transaction(trans, root, ret);
			if (own_trans)
				btrfs_end_transaction(trans, root);
			break;
		}
		btrfs_drop_extent_cache(inode, cur_offset,
					cur_offset + ins.offset -1, 0);

		num_bytes -= ins.offset;
		cur_offset += ins.offset;
		*alloc_hint = ins.objectid + ins.offset;

		inode->i_ctime = CURRENT_TIME;
		BTRFS_I(inode)->flags |= BTRFS_INODE_PREALLOC;
		if (!(mode & FALLOC_FL_KEEP_SIZE) &&
		    (actual_len > inode->i_size) &&
		    (cur_offset > inode->i_size)) {
			if (cur_offset > actual_len)
				i_size = actual_len;
			else
				i_size = cur_offset;
			i_size_write(inode, i_size);
			btrfs_ordered_update_i_size(inode, i_size, NULL);
		}

		ret = btrfs_update_inode(trans, root, inode);

		if (ret) {
			btrfs_abort_transaction(trans, root, ret);
			if (own_trans)
				btrfs_end_transaction(trans, root);
			break;
		}

		if (own_trans)
			btrfs_end_transaction(trans, root);
	}
	return ret;
}

int btrfs_prealloc_file_range(struct inode *inode, int mode,
			      u64 start, u64 num_bytes, u64 min_size,
			      loff_t actual_len, u64 *alloc_hint)
{
	return __btrfs_prealloc_file_range(inode, mode, start, num_bytes,
					   min_size, actual_len, alloc_hint,
					   NULL);
}

int btrfs_prealloc_file_range_trans(struct inode *inode,
				    struct btrfs_trans_handle *trans, int mode,
				    u64 start, u64 num_bytes, u64 min_size,
				    loff_t actual_len, u64 *alloc_hint)
{
	return __btrfs_prealloc_file_range(inode, mode, start, num_bytes,
					   min_size, actual_len, alloc_hint, trans);
}

static int btrfs_set_page_dirty(struct page *page)
{
	return __set_page_dirty_nobuffers(page);
}

static int btrfs_permission(struct inode *inode, int mask)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	umode_t mode = inode->i_mode;

	if (mask & MAY_WRITE &&
	    (S_ISREG(mode) || S_ISDIR(mode) || S_ISLNK(mode))) {
		if (btrfs_root_readonly(root))
			return -EROFS;
		if (BTRFS_I(inode)->flags & BTRFS_INODE_READONLY)
			return -EACCES;
	}
	return generic_permission(inode, mask);
}

static const struct inode_operations btrfs_dir_inode_operations = {
	.getattr	= btrfs_getattr,
	.lookup		= btrfs_lookup,
	.create		= btrfs_create,
	.unlink		= btrfs_unlink,
	.link		= btrfs_link,
	.mkdir		= btrfs_mkdir,
	.rmdir		= btrfs_rmdir,
	.rename		= btrfs_rename,
	.symlink	= btrfs_symlink,
	.setattr	= btrfs_setattr,
	.mknod		= btrfs_mknod,
	.setxattr	= btrfs_setxattr,
	.getxattr	= btrfs_getxattr,
	.listxattr	= btrfs_listxattr,
	.removexattr	= btrfs_removexattr,
	.permission	= btrfs_permission,
	.get_acl	= btrfs_get_acl,
};
static const struct inode_operations btrfs_dir_ro_inode_operations = {
	.lookup		= btrfs_lookup,
	.permission	= btrfs_permission,
	.get_acl	= btrfs_get_acl,
};

static const struct file_operations btrfs_dir_file_operations = {
	.llseek		= generic_file_llseek,
	.read		= generic_read_dir,
	.readdir	= btrfs_real_readdir,
	.unlocked_ioctl	= btrfs_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= btrfs_ioctl,
#endif
	.release        = btrfs_release_file,
	.fsync		= btrfs_sync_file,
};

static struct extent_io_ops btrfs_extent_io_ops = {
	.fill_delalloc = run_delalloc_range,
	.submit_bio_hook = btrfs_submit_bio_hook,
	.merge_bio_hook = btrfs_merge_bio_hook,
	.readpage_end_io_hook = btrfs_readpage_end_io_hook,
	.writepage_end_io_hook = btrfs_writepage_end_io_hook,
	.writepage_start_hook = btrfs_writepage_start_hook,
	.set_bit_hook = btrfs_set_bit_hook,
	.clear_bit_hook = btrfs_clear_bit_hook,
	.merge_extent_hook = btrfs_merge_extent_hook,
	.split_extent_hook = btrfs_split_extent_hook,
};

static const struct address_space_operations btrfs_aops = {
	.readpage	= btrfs_readpage,
	.writepage	= btrfs_writepage,
	.writepages	= btrfs_writepages,
	.readpages	= btrfs_readpages,
	.direct_IO	= btrfs_direct_IO,
	.invalidatepage = btrfs_invalidatepage,
	.releasepage	= btrfs_releasepage,
	.set_page_dirty	= btrfs_set_page_dirty,
	.error_remove_page = generic_error_remove_page,
};

static const struct address_space_operations btrfs_symlink_aops = {
	.readpage	= btrfs_readpage,
	.writepage	= btrfs_writepage,
	.invalidatepage = btrfs_invalidatepage,
	.releasepage	= btrfs_releasepage,
};

static const struct inode_operations btrfs_file_inode_operations = {
	.getattr	= btrfs_getattr,
	.setattr	= btrfs_setattr,
	.setxattr	= btrfs_setxattr,
	.getxattr	= btrfs_getxattr,
	.listxattr      = btrfs_listxattr,
	.removexattr	= btrfs_removexattr,
	.permission	= btrfs_permission,
	.fiemap		= btrfs_fiemap,
	.get_acl	= btrfs_get_acl,
};
static const struct inode_operations btrfs_special_inode_operations = {
	.getattr	= btrfs_getattr,
	.setattr	= btrfs_setattr,
	.permission	= btrfs_permission,
	.setxattr	= btrfs_setxattr,
	.getxattr	= btrfs_getxattr,
	.listxattr	= btrfs_listxattr,
	.removexattr	= btrfs_removexattr,
	.get_acl	= btrfs_get_acl,
};
static const struct inode_operations btrfs_symlink_inode_operations = {
	.readlink	= generic_readlink,
	.follow_link	= page_follow_link_light,
	.put_link	= page_put_link,
	.getattr	= btrfs_getattr,
	.setattr	= btrfs_setattr,
	.permission	= btrfs_permission,
	.setxattr	= btrfs_setxattr,
	.getxattr	= btrfs_getxattr,
	.listxattr	= btrfs_listxattr,
	.removexattr	= btrfs_removexattr,
	.get_acl	= btrfs_get_acl,
};

const struct dentry_operations btrfs_dentry_operations = {
	.d_delete	= btrfs_dentry_delete,
	.d_release	= btrfs_dentry_release,
};
