/*
 * Squashfs - a compressed read only filesystem for Linux
 *
 * Copyright (c) 2002, 2003, 2004, 2005, 2006, 2007, 2008
 * Phillip Lougher <phillip@squashfs.org.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * file.c
 */


#include <linux/fs.h>
#include <linux/vfs.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/pagemap.h>
#include <linux/mutex.h>

#include "squashfs_fs.h"
#include "squashfs_fs_sb.h"
#include "squashfs_fs_i.h"
#include "squashfs.h"

static struct meta_index *locate_meta_index(struct inode *inode, int offset,
				int index)
{
	struct meta_index *meta = NULL;
	struct squashfs_sb_info *msblk = inode->i_sb->s_fs_info;
	int i;

	mutex_lock(&msblk->meta_index_mutex);

	TRACE("locate_meta_index: index %d, offset %d\n", index, offset);

	if (msblk->meta_index == NULL)
		goto not_allocated;

	for (i = 0; i < SQUASHFS_META_SLOTS; i++) {
		if (msblk->meta_index[i].inode_number == inode->i_ino &&
				msblk->meta_index[i].offset >= offset &&
				msblk->meta_index[i].offset <= index &&
				msblk->meta_index[i].locked == 0) {
			TRACE("locate_meta_index: entry %d, offset %d\n", i,
					msblk->meta_index[i].offset);
			meta = &msblk->meta_index[i];
			offset = meta->offset;
		}
	}

	if (meta)
		meta->locked = 1;

not_allocated:
	mutex_unlock(&msblk->meta_index_mutex);

	return meta;
}


static struct meta_index *empty_meta_index(struct inode *inode, int offset,
				int skip)
{
	struct squashfs_sb_info *msblk = inode->i_sb->s_fs_info;
	struct meta_index *meta = NULL;
	int i;

	mutex_lock(&msblk->meta_index_mutex);

	TRACE("empty_meta_index: offset %d, skip %d\n", offset, skip);

	if (msblk->meta_index == NULL) {
		msblk->meta_index = kcalloc(SQUASHFS_META_SLOTS,
			sizeof(*(msblk->meta_index)), GFP_KERNEL);
		if (msblk->meta_index == NULL) {
			ERROR("Failed to allocate meta_index\n");
			goto failed;
		}
		for (i = 0; i < SQUASHFS_META_SLOTS; i++) {
			msblk->meta_index[i].inode_number = 0;
			msblk->meta_index[i].locked = 0;
		}
		msblk->next_meta_index = 0;
	}

	for (i = SQUASHFS_META_SLOTS; i &&
			msblk->meta_index[msblk->next_meta_index].locked; i--)
		msblk->next_meta_index = (msblk->next_meta_index + 1) %
			SQUASHFS_META_SLOTS;

	if (i == 0) {
		TRACE("empty_meta_index: failed!\n");
		goto failed;
	}

	TRACE("empty_meta_index: returned meta entry %d, %p\n",
			msblk->next_meta_index,
			&msblk->meta_index[msblk->next_meta_index]);

	meta = &msblk->meta_index[msblk->next_meta_index];
	msblk->next_meta_index = (msblk->next_meta_index + 1) %
			SQUASHFS_META_SLOTS;

	meta->inode_number = inode->i_ino;
	meta->offset = offset;
	meta->skip = skip;
	meta->entries = 0;
	meta->locked = 1;

failed:
	mutex_unlock(&msblk->meta_index_mutex);
	return meta;
}


static void release_meta_index(struct inode *inode, struct meta_index *meta)
{
	struct squashfs_sb_info *msblk = inode->i_sb->s_fs_info;
	mutex_lock(&msblk->meta_index_mutex);
	meta->locked = 0;
	mutex_unlock(&msblk->meta_index_mutex);
}


static long long read_indexes(struct super_block *sb, int n,
				u64 *start_block, int *offset)
{
	int err, i;
	long long block = 0;
	__le32 *blist = kmalloc(PAGE_CACHE_SIZE, GFP_KERNEL);

	if (blist == NULL) {
		ERROR("read_indexes: Failed to allocate block_list\n");
		return -ENOMEM;
	}

	while (n) {
		int blocks = min_t(int, n, PAGE_CACHE_SIZE >> 2);

		err = squashfs_read_metadata(sb, blist, start_block,
				offset, blocks << 2);
		if (err < 0) {
			ERROR("read_indexes: reading block [%llx:%x]\n",
				*start_block, *offset);
			goto failure;
		}

		for (i = 0; i < blocks; i++) {
			int size = le32_to_cpu(blist[i]);
			block += SQUASHFS_COMPRESSED_SIZE_BLOCK(size);
		}
		n -= blocks;
	}

	kfree(blist);
	return block;

failure:
	kfree(blist);
	return err;
}


static inline int calculate_skip(int blocks)
{
	int skip = blocks / ((SQUASHFS_META_ENTRIES + 1)
		 * SQUASHFS_META_INDEXES);
	return min(SQUASHFS_CACHED_BLKS - 1, skip + 1);
}


static int fill_meta_index(struct inode *inode, int index,
		u64 *index_block, int *index_offset, u64 *data_block)
{
	struct squashfs_sb_info *msblk = inode->i_sb->s_fs_info;
	int skip = calculate_skip(i_size_read(inode) >> msblk->block_log);
	int offset = 0;
	struct meta_index *meta;
	struct meta_entry *meta_entry;
	u64 cur_index_block = squashfs_i(inode)->block_list_start;
	int cur_offset = squashfs_i(inode)->offset;
	u64 cur_data_block = squashfs_i(inode)->start;
	int err, i;

	index /= SQUASHFS_META_INDEXES * skip;

	while (offset < index) {
		meta = locate_meta_index(inode, offset + 1, index);

		if (meta == NULL) {
			meta = empty_meta_index(inode, offset + 1, skip);
			if (meta == NULL)
				goto all_done;
		} else {
			offset = index < meta->offset + meta->entries ? index :
				meta->offset + meta->entries - 1;
			meta_entry = &meta->meta_entry[offset - meta->offset];
			cur_index_block = meta_entry->index_block +
				msblk->inode_table;
			cur_offset = meta_entry->offset;
			cur_data_block = meta_entry->data_block;
			TRACE("get_meta_index: offset %d, meta->offset %d, "
				"meta->entries %d\n", offset, meta->offset,
				meta->entries);
			TRACE("get_meta_index: index_block 0x%llx, offset 0x%x"
				" data_block 0x%llx\n", cur_index_block,
				cur_offset, cur_data_block);
		}

		for (i = meta->offset + meta->entries; i <= index &&
				i < meta->offset + SQUASHFS_META_ENTRIES; i++) {
			int blocks = skip * SQUASHFS_META_INDEXES;
			long long res = read_indexes(inode->i_sb, blocks,
					&cur_index_block, &cur_offset);

			if (res < 0) {
				if (meta->entries == 0)
					meta->inode_number = 0;
				err = res;
				goto failed;
			}

			cur_data_block += res;
			meta_entry = &meta->meta_entry[i - meta->offset];
			meta_entry->index_block = cur_index_block -
				msblk->inode_table;
			meta_entry->offset = cur_offset;
			meta_entry->data_block = cur_data_block;
			meta->entries++;
			offset++;
		}

		TRACE("get_meta_index: meta->offset %d, meta->entries %d\n",
				meta->offset, meta->entries);

		release_meta_index(inode, meta);
	}

all_done:
	*index_block = cur_index_block;
	*index_offset = cur_offset;
	*data_block = cur_data_block;

	return offset * SQUASHFS_META_INDEXES * skip;

failed:
	release_meta_index(inode, meta);
	return err;
}


static int read_blocklist(struct inode *inode, int index, u64 *block)
{
	u64 start;
	long long blks;
	int offset;
	__le32 size;
	int res = fill_meta_index(inode, index, &start, &offset, block);

	TRACE("read_blocklist: res %d, index %d, start 0x%llx, offset"
		       " 0x%x, block 0x%llx\n", res, index, start, offset,
			*block);

	if (res < 0)
		return res;

	if (res < index) {
		blks = read_indexes(inode->i_sb, index - res, &start, &offset);
		if (blks < 0)
			return (int) blks;
		*block += blks;
	}

	res = squashfs_read_metadata(inode->i_sb, &size, &start, &offset,
			sizeof(size));
	if (res < 0)
		return res;
	return le32_to_cpu(size);
}


static int squashfs_readpage(struct file *file, struct page *page)
{
	struct inode *inode = page->mapping->host;
	struct squashfs_sb_info *msblk = inode->i_sb->s_fs_info;
	int bytes, i, offset = 0, sparse = 0;
	struct squashfs_cache_entry *buffer = NULL;
	void *pageaddr;

	int mask = (1 << (msblk->block_log - PAGE_CACHE_SHIFT)) - 1;
	int index = page->index >> (msblk->block_log - PAGE_CACHE_SHIFT);
	int start_index = page->index & ~mask;
	int end_index = start_index | mask;
	int file_end = i_size_read(inode) >> msblk->block_log;

	TRACE("Entered squashfs_readpage, page index %lx, start block %llx\n",
				page->index, squashfs_i(inode)->start);

	if (page->index >= ((i_size_read(inode) + PAGE_CACHE_SIZE - 1) >>
					PAGE_CACHE_SHIFT))
		goto out;

	if (index < file_end || squashfs_i(inode)->fragment_block ==
					SQUASHFS_INVALID_BLK) {
		u64 block = 0;
		int bsize = read_blocklist(inode, index, &block);
		if (bsize < 0)
			goto error_out;

		if (bsize == 0) { 
			bytes = index == file_end ?
				(i_size_read(inode) & (msblk->block_size - 1)) :
				 msblk->block_size;
			sparse = 1;
		} else {
			buffer = squashfs_get_datablock(inode->i_sb,
								block, bsize);
			if (buffer->error) {
				ERROR("Unable to read page, block %llx, size %x"
					"\n", block, bsize);
				squashfs_cache_put(buffer);
				goto error_out;
			}
			bytes = buffer->length;
		}
	} else {
		buffer = squashfs_get_fragment(inode->i_sb,
				squashfs_i(inode)->fragment_block,
				squashfs_i(inode)->fragment_size);

		if (buffer->error) {
			ERROR("Unable to read page, block %llx, size %x\n",
				squashfs_i(inode)->fragment_block,
				squashfs_i(inode)->fragment_size);
			squashfs_cache_put(buffer);
			goto error_out;
		}
		bytes = i_size_read(inode) & (msblk->block_size - 1);
		offset = squashfs_i(inode)->fragment_offset;
	}

	for (i = start_index; i <= end_index && bytes > 0; i++,
			bytes -= PAGE_CACHE_SIZE, offset += PAGE_CACHE_SIZE) {
		struct page *push_page;
		int avail = sparse ? 0 : min_t(int, bytes, PAGE_CACHE_SIZE);

		TRACE("bytes %d, i %d, available_bytes %d\n", bytes, i, avail);

		push_page = (i == page->index) ? page :
			grab_cache_page_nowait(page->mapping, i);

		if (!push_page)
			continue;

		if (PageUptodate(push_page))
			goto skip_page;

		pageaddr = kmap_atomic(push_page);
		squashfs_copy_data(pageaddr, buffer, offset, avail);
		memset(pageaddr + avail, 0, PAGE_CACHE_SIZE - avail);
		kunmap_atomic(pageaddr);
		flush_dcache_page(push_page);
		SetPageUptodate(push_page);
skip_page:
		unlock_page(push_page);
		if (i != page->index)
			page_cache_release(push_page);
	}

	if (!sparse)
		squashfs_cache_put(buffer);

	return 0;

error_out:
	SetPageError(page);
out:
	pageaddr = kmap_atomic(page);
	memset(pageaddr, 0, PAGE_CACHE_SIZE);
	kunmap_atomic(pageaddr);
	flush_dcache_page(page);
	if (!PageError(page))
		SetPageUptodate(page);
	unlock_page(page);

	return 0;
}


const struct address_space_operations squashfs_aops = {
	.readpage = squashfs_readpage
};
