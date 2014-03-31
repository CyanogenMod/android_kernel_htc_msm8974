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
 * symlink.c
 */


#include <linux/fs.h>
#include <linux/vfs.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/pagemap.h>
#include <linux/xattr.h>

#include "squashfs_fs.h"
#include "squashfs_fs_sb.h"
#include "squashfs_fs_i.h"
#include "squashfs.h"
#include "xattr.h"

static int squashfs_symlink_readpage(struct file *file, struct page *page)
{
	struct inode *inode = page->mapping->host;
	struct super_block *sb = inode->i_sb;
	struct squashfs_sb_info *msblk = sb->s_fs_info;
	int index = page->index << PAGE_CACHE_SHIFT;
	u64 block = squashfs_i(inode)->start;
	int offset = squashfs_i(inode)->offset;
	int length = min_t(int, i_size_read(inode) - index, PAGE_CACHE_SIZE);
	int bytes, copied;
	void *pageaddr;
	struct squashfs_cache_entry *entry;

	TRACE("Entered squashfs_symlink_readpage, page index %ld, start block "
			"%llx, offset %x\n", page->index, block, offset);

	if (index) {
		bytes = squashfs_read_metadata(sb, NULL, &block, &offset,
								index);
		if (bytes < 0) {
			ERROR("Unable to read symlink [%llx:%x]\n",
				squashfs_i(inode)->start,
				squashfs_i(inode)->offset);
			goto error_out;
		}
	}

	for (bytes = 0; bytes < length; offset = 0, bytes += copied) {
		entry = squashfs_cache_get(sb, msblk->block_cache, block, 0);
		if (entry->error) {
			ERROR("Unable to read symlink [%llx:%x]\n",
				squashfs_i(inode)->start,
				squashfs_i(inode)->offset);
			squashfs_cache_put(entry);
			goto error_out;
		}

		pageaddr = kmap_atomic(page);
		copied = squashfs_copy_data(pageaddr + bytes, entry, offset,
								length - bytes);
		if (copied == length - bytes)
			memset(pageaddr + length, 0, PAGE_CACHE_SIZE - length);
		else
			block = entry->next_index;
		kunmap_atomic(pageaddr);
		squashfs_cache_put(entry);
	}

	flush_dcache_page(page);
	SetPageUptodate(page);
	unlock_page(page);
	return 0;

error_out:
	SetPageError(page);
	unlock_page(page);
	return 0;
}


const struct address_space_operations squashfs_symlink_aops = {
	.readpage = squashfs_symlink_readpage
};

const struct inode_operations squashfs_symlink_inode_ops = {
	.readlink = generic_readlink,
	.follow_link = page_follow_link_light,
	.put_link = page_put_link,
	.getxattr = generic_getxattr,
	.listxattr = squashfs_listxattr
};

