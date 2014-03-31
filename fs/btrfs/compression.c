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
#include <linux/bit_spinlock.h>
#include <linux/slab.h>
#include "compat.h"
#include "ctree.h"
#include "disk-io.h"
#include "transaction.h"
#include "btrfs_inode.h"
#include "volumes.h"
#include "ordered-data.h"
#include "compression.h"
#include "extent_io.h"
#include "extent_map.h"

struct compressed_bio {
	
	atomic_t pending_bios;

	
	struct page **compressed_pages;

	
	struct inode *inode;

	
	u64 start;

	
	unsigned long len;

	
	unsigned long compressed_len;

	
	int compress_type;

	
	unsigned long nr_pages;

	
	int errors;
	int mirror_num;

	
	struct bio *orig_bio;

	u32 sums;
};

static inline int compressed_bio_size(struct btrfs_root *root,
				      unsigned long disk_size)
{
	u16 csum_size = btrfs_super_csum_size(root->fs_info->super_copy);

	return sizeof(struct compressed_bio) +
		((disk_size + root->sectorsize - 1) / root->sectorsize) *
		csum_size;
}

static struct bio *compressed_bio_alloc(struct block_device *bdev,
					u64 first_byte, gfp_t gfp_flags)
{
	int nr_vecs;

	nr_vecs = bio_get_nr_vecs(bdev);
	return btrfs_bio_alloc(bdev, first_byte >> 9, nr_vecs, gfp_flags);
}

static int check_compressed_csum(struct inode *inode,
				 struct compressed_bio *cb,
				 u64 disk_start)
{
	int ret;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct page *page;
	unsigned long i;
	char *kaddr;
	u32 csum;
	u32 *cb_sum = &cb->sums;

	if (BTRFS_I(inode)->flags & BTRFS_INODE_NODATASUM)
		return 0;

	for (i = 0; i < cb->nr_pages; i++) {
		page = cb->compressed_pages[i];
		csum = ~(u32)0;

		kaddr = kmap_atomic(page);
		csum = btrfs_csum_data(root, kaddr, csum, PAGE_CACHE_SIZE);
		btrfs_csum_final(csum, (char *)&csum);
		kunmap_atomic(kaddr);

		if (csum != *cb_sum) {
			printk(KERN_INFO "btrfs csum failed ino %llu "
			       "extent %llu csum %u "
			       "wanted %u mirror %d\n",
			       (unsigned long long)btrfs_ino(inode),
			       (unsigned long long)disk_start,
			       csum, *cb_sum, cb->mirror_num);
			ret = -EIO;
			goto fail;
		}
		cb_sum++;

	}
	ret = 0;
fail:
	return ret;
}

static void end_compressed_bio_read(struct bio *bio, int err)
{
	struct compressed_bio *cb = bio->bi_private;
	struct inode *inode;
	struct page *page;
	unsigned long index;
	int ret;

	if (err)
		cb->errors = 1;

	if (!atomic_dec_and_test(&cb->pending_bios))
		goto out;

	inode = cb->inode;
	ret = check_compressed_csum(inode, cb, (u64)bio->bi_sector << 9);
	if (ret)
		goto csum_failed;

	ret = btrfs_decompress_biovec(cb->compress_type,
				      cb->compressed_pages,
				      cb->start,
				      cb->orig_bio->bi_io_vec,
				      cb->orig_bio->bi_vcnt,
				      cb->compressed_len);
csum_failed:
	if (ret)
		cb->errors = 1;

	
	index = 0;
	for (index = 0; index < cb->nr_pages; index++) {
		page = cb->compressed_pages[index];
		page->mapping = NULL;
		page_cache_release(page);
	}

	
	if (cb->errors) {
		bio_io_error(cb->orig_bio);
	} else {
		int bio_index = 0;
		struct bio_vec *bvec = cb->orig_bio->bi_io_vec;

		while (bio_index < cb->orig_bio->bi_vcnt) {
			SetPageChecked(bvec->bv_page);
			bvec++;
			bio_index++;
		}
		bio_endio(cb->orig_bio, 0);
	}

	
	kfree(cb->compressed_pages);
	kfree(cb);
out:
	bio_put(bio);
}

static noinline void end_compressed_writeback(struct inode *inode, u64 start,
					      unsigned long ram_size)
{
	unsigned long index = start >> PAGE_CACHE_SHIFT;
	unsigned long end_index = (start + ram_size - 1) >> PAGE_CACHE_SHIFT;
	struct page *pages[16];
	unsigned long nr_pages = end_index - index + 1;
	int i;
	int ret;

	while (nr_pages > 0) {
		ret = find_get_pages_contig(inode->i_mapping, index,
				     min_t(unsigned long,
				     nr_pages, ARRAY_SIZE(pages)), pages);
		if (ret == 0) {
			nr_pages -= 1;
			index += 1;
			continue;
		}
		for (i = 0; i < ret; i++) {
			end_page_writeback(pages[i]);
			page_cache_release(pages[i]);
		}
		nr_pages -= ret;
		index += ret;
	}
	
}

static void end_compressed_bio_write(struct bio *bio, int err)
{
	struct extent_io_tree *tree;
	struct compressed_bio *cb = bio->bi_private;
	struct inode *inode;
	struct page *page;
	unsigned long index;

	if (err)
		cb->errors = 1;

	if (!atomic_dec_and_test(&cb->pending_bios))
		goto out;

	inode = cb->inode;
	tree = &BTRFS_I(inode)->io_tree;
	cb->compressed_pages[0]->mapping = cb->inode->i_mapping;
	tree->ops->writepage_end_io_hook(cb->compressed_pages[0],
					 cb->start,
					 cb->start + cb->len - 1,
					 NULL, 1);
	cb->compressed_pages[0]->mapping = NULL;

	end_compressed_writeback(inode, cb->start, cb->len);
	

	index = 0;
	for (index = 0; index < cb->nr_pages; index++) {
		page = cb->compressed_pages[index];
		page->mapping = NULL;
		page_cache_release(page);
	}

	
	kfree(cb->compressed_pages);
	kfree(cb);
out:
	bio_put(bio);
}

int btrfs_submit_compressed_write(struct inode *inode, u64 start,
				 unsigned long len, u64 disk_start,
				 unsigned long compressed_len,
				 struct page **compressed_pages,
				 unsigned long nr_pages)
{
	struct bio *bio = NULL;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct compressed_bio *cb;
	unsigned long bytes_left;
	struct extent_io_tree *io_tree = &BTRFS_I(inode)->io_tree;
	int pg_index = 0;
	struct page *page;
	u64 first_byte = disk_start;
	struct block_device *bdev;
	int ret;
	int skip_sum = BTRFS_I(inode)->flags & BTRFS_INODE_NODATASUM;

	WARN_ON(start & ((u64)PAGE_CACHE_SIZE - 1));
	cb = kmalloc(compressed_bio_size(root, compressed_len), GFP_NOFS);
	if (!cb)
		return -ENOMEM;
	atomic_set(&cb->pending_bios, 0);
	cb->errors = 0;
	cb->inode = inode;
	cb->start = start;
	cb->len = len;
	cb->mirror_num = 0;
	cb->compressed_pages = compressed_pages;
	cb->compressed_len = compressed_len;
	cb->orig_bio = NULL;
	cb->nr_pages = nr_pages;

	bdev = BTRFS_I(inode)->root->fs_info->fs_devices->latest_bdev;

	bio = compressed_bio_alloc(bdev, first_byte, GFP_NOFS);
	if(!bio) {
		kfree(cb);
		return -ENOMEM;
	}
	bio->bi_private = cb;
	bio->bi_end_io = end_compressed_bio_write;
	atomic_inc(&cb->pending_bios);

	
	bytes_left = compressed_len;
	for (pg_index = 0; pg_index < cb->nr_pages; pg_index++) {
		page = compressed_pages[pg_index];
		page->mapping = inode->i_mapping;
		if (bio->bi_size)
			ret = io_tree->ops->merge_bio_hook(page, 0,
							   PAGE_CACHE_SIZE,
							   bio, 0);
		else
			ret = 0;

		page->mapping = NULL;
		if (ret || bio_add_page(bio, page, PAGE_CACHE_SIZE, 0) <
		    PAGE_CACHE_SIZE) {
			bio_get(bio);

			atomic_inc(&cb->pending_bios);
			ret = btrfs_bio_wq_end_io(root->fs_info, bio, 0);
			BUG_ON(ret); 

			if (!skip_sum) {
				ret = btrfs_csum_one_bio(root, inode, bio,
							 start, 1);
				BUG_ON(ret); 
			}

			ret = btrfs_map_bio(root, WRITE, bio, 0, 1);
			BUG_ON(ret); 

			bio_put(bio);

			bio = compressed_bio_alloc(bdev, first_byte, GFP_NOFS);
			BUG_ON(!bio);
			bio->bi_private = cb;
			bio->bi_end_io = end_compressed_bio_write;
			bio_add_page(bio, page, PAGE_CACHE_SIZE, 0);
		}
		if (bytes_left < PAGE_CACHE_SIZE) {
			printk("bytes left %lu compress len %lu nr %lu\n",
			       bytes_left, cb->compressed_len, cb->nr_pages);
		}
		bytes_left -= PAGE_CACHE_SIZE;
		first_byte += PAGE_CACHE_SIZE;
		cond_resched();
	}
	bio_get(bio);

	ret = btrfs_bio_wq_end_io(root->fs_info, bio, 0);
	BUG_ON(ret); 

	if (!skip_sum) {
		ret = btrfs_csum_one_bio(root, inode, bio, start, 1);
		BUG_ON(ret); 
	}

	ret = btrfs_map_bio(root, WRITE, bio, 0, 1);
	BUG_ON(ret); 

	bio_put(bio);
	return 0;
}

static noinline int add_ra_bio_pages(struct inode *inode,
				     u64 compressed_end,
				     struct compressed_bio *cb)
{
	unsigned long end_index;
	unsigned long pg_index;
	u64 last_offset;
	u64 isize = i_size_read(inode);
	int ret;
	struct page *page;
	unsigned long nr_pages = 0;
	struct extent_map *em;
	struct address_space *mapping = inode->i_mapping;
	struct extent_map_tree *em_tree;
	struct extent_io_tree *tree;
	u64 end;
	int misses = 0;

	page = cb->orig_bio->bi_io_vec[cb->orig_bio->bi_vcnt - 1].bv_page;
	last_offset = (page_offset(page) + PAGE_CACHE_SIZE);
	em_tree = &BTRFS_I(inode)->extent_tree;
	tree = &BTRFS_I(inode)->io_tree;

	if (isize == 0)
		return 0;

	end_index = (i_size_read(inode) - 1) >> PAGE_CACHE_SHIFT;

	while (last_offset < compressed_end) {
		pg_index = last_offset >> PAGE_CACHE_SHIFT;

		if (pg_index > end_index)
			break;

		rcu_read_lock();
		page = radix_tree_lookup(&mapping->page_tree, pg_index);
		rcu_read_unlock();
		if (page) {
			misses++;
			if (misses > 4)
				break;
			goto next;
		}

		page = __page_cache_alloc(mapping_gfp_mask(mapping) &
								~__GFP_FS);
		if (!page)
			break;

		if (add_to_page_cache_lru(page, mapping, pg_index,
								GFP_NOFS)) {
			page_cache_release(page);
			goto next;
		}

		end = last_offset + PAGE_CACHE_SIZE - 1;
		set_page_extent_mapped(page);
		lock_extent(tree, last_offset, end);
		read_lock(&em_tree->lock);
		em = lookup_extent_mapping(em_tree, last_offset,
					   PAGE_CACHE_SIZE);
		read_unlock(&em_tree->lock);

		if (!em || last_offset < em->start ||
		    (last_offset + PAGE_CACHE_SIZE > extent_map_end(em)) ||
		    (em->block_start >> 9) != cb->orig_bio->bi_sector) {
			free_extent_map(em);
			unlock_extent(tree, last_offset, end);
			unlock_page(page);
			page_cache_release(page);
			break;
		}
		free_extent_map(em);

		if (page->index == end_index) {
			char *userpage;
			size_t zero_offset = isize & (PAGE_CACHE_SIZE - 1);

			if (zero_offset) {
				int zeros;
				zeros = PAGE_CACHE_SIZE - zero_offset;
				userpage = kmap_atomic(page);
				memset(userpage + zero_offset, 0, zeros);
				flush_dcache_page(page);
				kunmap_atomic(userpage);
			}
		}

		ret = bio_add_page(cb->orig_bio, page,
				   PAGE_CACHE_SIZE, 0);

		if (ret == PAGE_CACHE_SIZE) {
			nr_pages++;
			page_cache_release(page);
		} else {
			unlock_extent(tree, last_offset, end);
			unlock_page(page);
			page_cache_release(page);
			break;
		}
next:
		last_offset += PAGE_CACHE_SIZE;
	}
	return 0;
}

int btrfs_submit_compressed_read(struct inode *inode, struct bio *bio,
				 int mirror_num, unsigned long bio_flags)
{
	struct extent_io_tree *tree;
	struct extent_map_tree *em_tree;
	struct compressed_bio *cb;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	unsigned long uncompressed_len = bio->bi_vcnt * PAGE_CACHE_SIZE;
	unsigned long compressed_len;
	unsigned long nr_pages;
	unsigned long pg_index;
	struct page *page;
	struct block_device *bdev;
	struct bio *comp_bio;
	u64 cur_disk_byte = (u64)bio->bi_sector << 9;
	u64 em_len;
	u64 em_start;
	struct extent_map *em;
	int ret = -ENOMEM;
	u32 *sums;

	tree = &BTRFS_I(inode)->io_tree;
	em_tree = &BTRFS_I(inode)->extent_tree;

	
	read_lock(&em_tree->lock);
	em = lookup_extent_mapping(em_tree,
				   page_offset(bio->bi_io_vec->bv_page),
				   PAGE_CACHE_SIZE);
	read_unlock(&em_tree->lock);
	if (!em)
		return -EIO;

	compressed_len = em->block_len;
	cb = kmalloc(compressed_bio_size(root, compressed_len), GFP_NOFS);
	if (!cb)
		goto out;

	atomic_set(&cb->pending_bios, 0);
	cb->errors = 0;
	cb->inode = inode;
	cb->mirror_num = mirror_num;
	sums = &cb->sums;

	cb->start = em->orig_start;
	em_len = em->len;
	em_start = em->start;

	free_extent_map(em);
	em = NULL;

	cb->len = uncompressed_len;
	cb->compressed_len = compressed_len;
	cb->compress_type = extent_compress_type(bio_flags);
	cb->orig_bio = bio;

	nr_pages = (compressed_len + PAGE_CACHE_SIZE - 1) /
				 PAGE_CACHE_SIZE;
	cb->compressed_pages = kzalloc(sizeof(struct page *) * nr_pages,
				       GFP_NOFS);
	if (!cb->compressed_pages)
		goto fail1;

	bdev = BTRFS_I(inode)->root->fs_info->fs_devices->latest_bdev;

	for (pg_index = 0; pg_index < nr_pages; pg_index++) {
		cb->compressed_pages[pg_index] = alloc_page(GFP_NOFS |
							      __GFP_HIGHMEM);
		if (!cb->compressed_pages[pg_index])
			goto fail2;
	}
	cb->nr_pages = nr_pages;

	add_ra_bio_pages(inode, em_start + em_len, cb);

	
	uncompressed_len = bio->bi_vcnt * PAGE_CACHE_SIZE;
	cb->len = uncompressed_len;

	comp_bio = compressed_bio_alloc(bdev, cur_disk_byte, GFP_NOFS);
	if (!comp_bio)
		goto fail2;
	comp_bio->bi_private = cb;
	comp_bio->bi_end_io = end_compressed_bio_read;
	atomic_inc(&cb->pending_bios);

	for (pg_index = 0; pg_index < nr_pages; pg_index++) {
		page = cb->compressed_pages[pg_index];
		page->mapping = inode->i_mapping;
		page->index = em_start >> PAGE_CACHE_SHIFT;

		if (comp_bio->bi_size)
			ret = tree->ops->merge_bio_hook(page, 0,
							PAGE_CACHE_SIZE,
							comp_bio, 0);
		else
			ret = 0;

		page->mapping = NULL;
		if (ret || bio_add_page(comp_bio, page, PAGE_CACHE_SIZE, 0) <
		    PAGE_CACHE_SIZE) {
			bio_get(comp_bio);

			ret = btrfs_bio_wq_end_io(root->fs_info, comp_bio, 0);
			BUG_ON(ret); 

			atomic_inc(&cb->pending_bios);

			if (!(BTRFS_I(inode)->flags & BTRFS_INODE_NODATASUM)) {
				ret = btrfs_lookup_bio_sums(root, inode,
							comp_bio, sums);
				BUG_ON(ret); 
			}
			sums += (comp_bio->bi_size + root->sectorsize - 1) /
				root->sectorsize;

			ret = btrfs_map_bio(root, READ, comp_bio,
					    mirror_num, 0);
			BUG_ON(ret); 

			bio_put(comp_bio);

			comp_bio = compressed_bio_alloc(bdev, cur_disk_byte,
							GFP_NOFS);
			BUG_ON(!comp_bio);
			comp_bio->bi_private = cb;
			comp_bio->bi_end_io = end_compressed_bio_read;

			bio_add_page(comp_bio, page, PAGE_CACHE_SIZE, 0);
		}
		cur_disk_byte += PAGE_CACHE_SIZE;
	}
	bio_get(comp_bio);

	ret = btrfs_bio_wq_end_io(root->fs_info, comp_bio, 0);
	BUG_ON(ret); 

	if (!(BTRFS_I(inode)->flags & BTRFS_INODE_NODATASUM)) {
		ret = btrfs_lookup_bio_sums(root, inode, comp_bio, sums);
		BUG_ON(ret); 
	}

	ret = btrfs_map_bio(root, READ, comp_bio, mirror_num, 0);
	BUG_ON(ret); 

	bio_put(comp_bio);
	return 0;

fail2:
	for (pg_index = 0; pg_index < nr_pages; pg_index++)
		free_page((unsigned long)cb->compressed_pages[pg_index]);

	kfree(cb->compressed_pages);
fail1:
	kfree(cb);
out:
	free_extent_map(em);
	return ret;
}

static struct list_head comp_idle_workspace[BTRFS_COMPRESS_TYPES];
static spinlock_t comp_workspace_lock[BTRFS_COMPRESS_TYPES];
static int comp_num_workspace[BTRFS_COMPRESS_TYPES];
static atomic_t comp_alloc_workspace[BTRFS_COMPRESS_TYPES];
static wait_queue_head_t comp_workspace_wait[BTRFS_COMPRESS_TYPES];

struct btrfs_compress_op *btrfs_compress_op[] = {
	&btrfs_zlib_compress,
	&btrfs_lzo_compress,
};

void __init btrfs_init_compress(void)
{
	int i;

	for (i = 0; i < BTRFS_COMPRESS_TYPES; i++) {
		INIT_LIST_HEAD(&comp_idle_workspace[i]);
		spin_lock_init(&comp_workspace_lock[i]);
		atomic_set(&comp_alloc_workspace[i], 0);
		init_waitqueue_head(&comp_workspace_wait[i]);
	}
}

static struct list_head *find_workspace(int type)
{
	struct list_head *workspace;
	int cpus = num_online_cpus();
	int idx = type - 1;

	struct list_head *idle_workspace	= &comp_idle_workspace[idx];
	spinlock_t *workspace_lock		= &comp_workspace_lock[idx];
	atomic_t *alloc_workspace		= &comp_alloc_workspace[idx];
	wait_queue_head_t *workspace_wait	= &comp_workspace_wait[idx];
	int *num_workspace			= &comp_num_workspace[idx];
again:
	spin_lock(workspace_lock);
	if (!list_empty(idle_workspace)) {
		workspace = idle_workspace->next;
		list_del(workspace);
		(*num_workspace)--;
		spin_unlock(workspace_lock);
		return workspace;

	}
	if (atomic_read(alloc_workspace) > cpus) {
		DEFINE_WAIT(wait);

		spin_unlock(workspace_lock);
		prepare_to_wait(workspace_wait, &wait, TASK_UNINTERRUPTIBLE);
		if (atomic_read(alloc_workspace) > cpus && !*num_workspace)
			schedule();
		finish_wait(workspace_wait, &wait);
		goto again;
	}
	atomic_inc(alloc_workspace);
	spin_unlock(workspace_lock);

	workspace = btrfs_compress_op[idx]->alloc_workspace();
	if (IS_ERR(workspace)) {
		atomic_dec(alloc_workspace);
		wake_up(workspace_wait);
	}
	return workspace;
}

static void free_workspace(int type, struct list_head *workspace)
{
	int idx = type - 1;
	struct list_head *idle_workspace	= &comp_idle_workspace[idx];
	spinlock_t *workspace_lock		= &comp_workspace_lock[idx];
	atomic_t *alloc_workspace		= &comp_alloc_workspace[idx];
	wait_queue_head_t *workspace_wait	= &comp_workspace_wait[idx];
	int *num_workspace			= &comp_num_workspace[idx];

	spin_lock(workspace_lock);
	if (*num_workspace < num_online_cpus()) {
		list_add_tail(workspace, idle_workspace);
		(*num_workspace)++;
		spin_unlock(workspace_lock);
		goto wake;
	}
	spin_unlock(workspace_lock);

	btrfs_compress_op[idx]->free_workspace(workspace);
	atomic_dec(alloc_workspace);
wake:
	if (waitqueue_active(workspace_wait))
		wake_up(workspace_wait);
}

static void free_workspaces(void)
{
	struct list_head *workspace;
	int i;

	for (i = 0; i < BTRFS_COMPRESS_TYPES; i++) {
		while (!list_empty(&comp_idle_workspace[i])) {
			workspace = comp_idle_workspace[i].next;
			list_del(workspace);
			btrfs_compress_op[i]->free_workspace(workspace);
			atomic_dec(&comp_alloc_workspace[i]);
		}
	}
}

int btrfs_compress_pages(int type, struct address_space *mapping,
			 u64 start, unsigned long len,
			 struct page **pages,
			 unsigned long nr_dest_pages,
			 unsigned long *out_pages,
			 unsigned long *total_in,
			 unsigned long *total_out,
			 unsigned long max_out)
{
	struct list_head *workspace;
	int ret;

	workspace = find_workspace(type);
	if (IS_ERR(workspace))
		return -1;

	ret = btrfs_compress_op[type-1]->compress_pages(workspace, mapping,
						      start, len, pages,
						      nr_dest_pages, out_pages,
						      total_in, total_out,
						      max_out);
	free_workspace(type, workspace);
	return ret;
}

int btrfs_decompress_biovec(int type, struct page **pages_in, u64 disk_start,
			    struct bio_vec *bvec, int vcnt, size_t srclen)
{
	struct list_head *workspace;
	int ret;

	workspace = find_workspace(type);
	if (IS_ERR(workspace))
		return -ENOMEM;

	ret = btrfs_compress_op[type-1]->decompress_biovec(workspace, pages_in,
							 disk_start,
							 bvec, vcnt, srclen);
	free_workspace(type, workspace);
	return ret;
}

int btrfs_decompress(int type, unsigned char *data_in, struct page *dest_page,
		     unsigned long start_byte, size_t srclen, size_t destlen)
{
	struct list_head *workspace;
	int ret;

	workspace = find_workspace(type);
	if (IS_ERR(workspace))
		return -ENOMEM;

	ret = btrfs_compress_op[type-1]->decompress(workspace, data_in,
						  dest_page, start_byte,
						  srclen, destlen);

	free_workspace(type, workspace);
	return ret;
}

void btrfs_exit_compress(void)
{
	free_workspaces();
}

int btrfs_decompress_buf2page(char *buf, unsigned long buf_start,
			      unsigned long total_out, u64 disk_start,
			      struct bio_vec *bvec, int vcnt,
			      unsigned long *pg_index,
			      unsigned long *pg_offset)
{
	unsigned long buf_offset;
	unsigned long current_buf_start;
	unsigned long start_byte;
	unsigned long working_bytes = total_out - buf_start;
	unsigned long bytes;
	char *kaddr;
	struct page *page_out = bvec[*pg_index].bv_page;

	start_byte = page_offset(page_out) - disk_start;

	
	if (total_out <= start_byte)
		return 1;

	if (total_out > start_byte && buf_start < start_byte) {
		buf_offset = start_byte - buf_start;
		working_bytes -= buf_offset;
	} else {
		buf_offset = 0;
	}
	current_buf_start = buf_start;

	
	while (working_bytes > 0) {
		bytes = min(PAGE_CACHE_SIZE - *pg_offset,
			    PAGE_CACHE_SIZE - buf_offset);
		bytes = min(bytes, working_bytes);
		kaddr = kmap_atomic(page_out);
		memcpy(kaddr + *pg_offset, buf + buf_offset, bytes);
		kunmap_atomic(kaddr);
		flush_dcache_page(page_out);

		*pg_offset += bytes;
		buf_offset += bytes;
		working_bytes -= bytes;
		current_buf_start += bytes;

		
		if (*pg_offset == PAGE_CACHE_SIZE) {
			(*pg_index)++;
			if (*pg_index >= vcnt)
				return 0;

			page_out = bvec[*pg_index].bv_page;
			*pg_offset = 0;
			start_byte = page_offset(page_out) - disk_start;

			if (total_out <= start_byte)
				return 1;

			if (total_out > start_byte &&
			    current_buf_start < start_byte) {
				buf_offset = start_byte - buf_start;
				working_bytes = total_out - start_byte;
				current_buf_start = buf_start + buf_offset;
			}
		}
	}

	return 1;
}
