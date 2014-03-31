/*
 *  linux/fs/9p/vfs_addr.c
 *
 * This file contians vfs address (mmap) ops for 9P2000.
 *
 *  Copyright (C) 2005 by Eric Van Hensbergen <ericvh@gmail.com>
 *  Copyright (C) 2002 by Ron Minnich <rminnich@lanl.gov>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to:
 *  Free Software Foundation
 *  51 Franklin Street, Fifth Floor
 *  Boston, MA  02111-1301  USA
 *
 */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/inet.h>
#include <linux/pagemap.h>
#include <linux/idr.h>
#include <linux/sched.h>
#include <net/9p/9p.h>
#include <net/9p/client.h>

#include "v9fs.h"
#include "v9fs_vfs.h"
#include "cache.h"
#include "fid.h"

static int v9fs_fid_readpage(struct p9_fid *fid, struct page *page)
{
	int retval;
	loff_t offset;
	char *buffer;
	struct inode *inode;

	inode = page->mapping->host;
	p9_debug(P9_DEBUG_VFS, "\n");

	BUG_ON(!PageLocked(page));

	retval = v9fs_readpage_from_fscache(inode, page);
	if (retval == 0)
		return retval;

	buffer = kmap(page);
	offset = page_offset(page);

	retval = v9fs_fid_readn(fid, buffer, NULL, PAGE_CACHE_SIZE, offset);
	if (retval < 0) {
		v9fs_uncache_page(inode, page);
		goto done;
	}

	memset(buffer + retval, 0, PAGE_CACHE_SIZE - retval);
	flush_dcache_page(page);
	SetPageUptodate(page);

	v9fs_readpage_to_fscache(inode, page);
	retval = 0;

done:
	kunmap(page);
	unlock_page(page);
	return retval;
}


static int v9fs_vfs_readpage(struct file *filp, struct page *page)
{
	return v9fs_fid_readpage(filp->private_data, page);
}


static int v9fs_vfs_readpages(struct file *filp, struct address_space *mapping,
			     struct list_head *pages, unsigned nr_pages)
{
	int ret = 0;
	struct inode *inode;

	inode = mapping->host;
	p9_debug(P9_DEBUG_VFS, "inode: %p file: %p\n", inode, filp);

	ret = v9fs_readpages_from_fscache(inode, mapping, pages, &nr_pages);
	if (ret == 0)
		return ret;

	ret = read_cache_pages(mapping, pages, (void *)v9fs_vfs_readpage, filp);
	p9_debug(P9_DEBUG_VFS, "  = %d\n", ret);
	return ret;
}


static int v9fs_release_page(struct page *page, gfp_t gfp)
{
	if (PagePrivate(page))
		return 0;
	return v9fs_fscache_release_page(page, gfp);
}


static void v9fs_invalidate_page(struct page *page, unsigned long offset)
{
	if (offset == 0)
		v9fs_fscache_invalidate_page(page);
}

static int v9fs_vfs_writepage_locked(struct page *page)
{
	char *buffer;
	int retval, len;
	loff_t offset, size;
	mm_segment_t old_fs;
	struct v9fs_inode *v9inode;
	struct inode *inode = page->mapping->host;

	v9inode = V9FS_I(inode);
	size = i_size_read(inode);
	if (page->index == size >> PAGE_CACHE_SHIFT)
		len = size & ~PAGE_CACHE_MASK;
	else
		len = PAGE_CACHE_SIZE;

	set_page_writeback(page);

	buffer = kmap(page);
	offset = page_offset(page);

	old_fs = get_fs();
	set_fs(get_ds());
	
	BUG_ON(!v9inode->writeback_fid);

	retval = v9fs_file_write_internal(inode,
					  v9inode->writeback_fid,
					  (__force const char __user *)buffer,
					  len, &offset, 0);
	if (retval > 0)
		retval = 0;

	set_fs(old_fs);
	kunmap(page);
	end_page_writeback(page);
	return retval;
}

static int v9fs_vfs_writepage(struct page *page, struct writeback_control *wbc)
{
	int retval;

	retval = v9fs_vfs_writepage_locked(page);
	if (retval < 0) {
		if (retval == -EAGAIN) {
			redirty_page_for_writepage(wbc, page);
			retval = 0;
		} else {
			SetPageError(page);
			mapping_set_error(page->mapping, retval);
		}
	} else
		retval = 0;

	unlock_page(page);
	return retval;
}


static int v9fs_launder_page(struct page *page)
{
	int retval;
	struct inode *inode = page->mapping->host;

	v9fs_fscache_wait_on_page_write(inode, page);
	if (clear_page_dirty_for_io(page)) {
		retval = v9fs_vfs_writepage_locked(page);
		if (retval)
			return retval;
	}
	return 0;
}

static ssize_t
v9fs_direct_IO(int rw, struct kiocb *iocb, const struct iovec *iov,
	       loff_t pos, unsigned long nr_segs)
{
	p9_debug(P9_DEBUG_VFS, "v9fs_direct_IO: v9fs_direct_IO (%s) off/no(%lld/%lu) EINVAL\n",
		 iocb->ki_filp->f_path.dentry->d_name.name,
		 (long long)pos, nr_segs);

	return -EINVAL;
}

static int v9fs_write_begin(struct file *filp, struct address_space *mapping,
			    loff_t pos, unsigned len, unsigned flags,
			    struct page **pagep, void **fsdata)
{
	int retval = 0;
	struct page *page;
	struct v9fs_inode *v9inode;
	pgoff_t index = pos >> PAGE_CACHE_SHIFT;
	struct inode *inode = mapping->host;

	v9inode = V9FS_I(inode);
start:
	page = grab_cache_page_write_begin(mapping, index, flags);
	if (!page) {
		retval = -ENOMEM;
		goto out;
	}
	BUG_ON(!v9inode->writeback_fid);
	if (PageUptodate(page))
		goto out;

	if (len == PAGE_CACHE_SIZE)
		goto out;

	retval = v9fs_fid_readpage(v9inode->writeback_fid, page);
	page_cache_release(page);
	if (!retval)
		goto start;
out:
	*pagep = page;
	return retval;
}

static int v9fs_write_end(struct file *filp, struct address_space *mapping,
			  loff_t pos, unsigned len, unsigned copied,
			  struct page *page, void *fsdata)
{
	loff_t last_pos = pos + copied;
	struct inode *inode = page->mapping->host;

	if (unlikely(copied < len)) {
		unsigned from = pos & (PAGE_CACHE_SIZE - 1);

		zero_user(page, from + copied, len - copied);
		flush_dcache_page(page);
	}

	if (!PageUptodate(page))
		SetPageUptodate(page);
	if (last_pos > inode->i_size) {
		inode_add_bytes(inode, last_pos - inode->i_size);
		i_size_write(inode, last_pos);
	}
	set_page_dirty(page);
	unlock_page(page);
	page_cache_release(page);

	return copied;
}


const struct address_space_operations v9fs_addr_operations = {
	.readpage = v9fs_vfs_readpage,
	.readpages = v9fs_vfs_readpages,
	.set_page_dirty = __set_page_dirty_nobuffers,
	.writepage = v9fs_vfs_writepage,
	.write_begin = v9fs_write_begin,
	.write_end = v9fs_write_end,
	.releasepage = v9fs_release_page,
	.invalidatepage = v9fs_invalidate_page,
	.launder_page = v9fs_launder_page,
	.direct_IO = v9fs_direct_IO,
};
