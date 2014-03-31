/* file-nommu.c: no-MMU version of ramfs
 *
 * Copyright (C) 2005 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/highmem.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/backing-dev.h>
#include <linux/ramfs.h>
#include <linux/pagevec.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include <asm/uaccess.h>
#include "internal.h"

static int ramfs_nommu_setattr(struct dentry *, struct iattr *);

const struct address_space_operations ramfs_aops = {
	.readpage		= simple_readpage,
	.write_begin		= simple_write_begin,
	.write_end		= simple_write_end,
	.set_page_dirty		= __set_page_dirty_no_writeback,
};

const struct file_operations ramfs_file_operations = {
	.mmap			= ramfs_nommu_mmap,
	.get_unmapped_area	= ramfs_nommu_get_unmapped_area,
	.read			= do_sync_read,
	.aio_read		= generic_file_aio_read,
	.write			= do_sync_write,
	.aio_write		= generic_file_aio_write,
	.fsync			= noop_fsync,
	.splice_read		= generic_file_splice_read,
	.splice_write		= generic_file_splice_write,
	.llseek			= generic_file_llseek,
};

const struct inode_operations ramfs_file_inode_operations = {
	.setattr		= ramfs_nommu_setattr,
	.getattr		= simple_getattr,
};

int ramfs_nommu_expand_for_mapping(struct inode *inode, size_t newsize)
{
	unsigned long npages, xpages, loop;
	struct page *pages;
	unsigned order;
	void *data;
	int ret;

	
	order = get_order(newsize);
	if (unlikely(order >= MAX_ORDER))
		return -EFBIG;

	ret = inode_newsize_ok(inode, newsize);
	if (ret)
		return ret;

	i_size_write(inode, newsize);

	pages = alloc_pages(mapping_gfp_mask(inode->i_mapping), order);
	if (!pages)
		return -ENOMEM;

	
	xpages = 1UL << order;
	npages = (newsize + PAGE_SIZE - 1) >> PAGE_SHIFT;

	split_page(pages, order);

	
	for (loop = npages; loop < xpages; loop++)
		__free_page(pages + loop);

	
	newsize = PAGE_SIZE * npages;
	data = page_address(pages);
	memset(data, 0, newsize);

	
	for (loop = 0; loop < npages; loop++) {
		struct page *page = pages + loop;

		ret = add_to_page_cache_lru(page, inode->i_mapping, loop,
					GFP_KERNEL);
		if (ret < 0)
			goto add_error;

		
		SetPageDirty(page);

		unlock_page(page);
		put_page(page);
	}

	return 0;

add_error:
	while (loop < npages)
		__free_page(pages + loop++);
	return ret;
}

static int ramfs_nommu_resize(struct inode *inode, loff_t newsize, loff_t size)
{
	int ret;

	if (size == 0) {
		if (unlikely(newsize >> 32))
			return -EFBIG;

		return ramfs_nommu_expand_for_mapping(inode, newsize);
	}

	
	if (newsize < size) {
		ret = nommu_shrink_inode_mappings(inode, size, newsize);
		if (ret < 0)
			return ret;
	}

	truncate_setsize(inode, newsize);
	return 0;
}

static int ramfs_nommu_setattr(struct dentry *dentry, struct iattr *ia)
{
	struct inode *inode = dentry->d_inode;
	unsigned int old_ia_valid = ia->ia_valid;
	int ret = 0;

	
	ret = inode_change_ok(inode, ia);
	if (ret)
		return ret;

	
	if (ia->ia_valid & ATTR_SIZE) {
		loff_t size = inode->i_size;

		if (ia->ia_size != size) {
			ret = ramfs_nommu_resize(inode, ia->ia_size, size);
			if (ret < 0 || ia->ia_valid == ATTR_SIZE)
				goto out;
		} else {
			ia->ia_valid |= ATTR_MTIME|ATTR_CTIME;
		}
	}

	setattr_copy(inode, ia);
 out:
	ia->ia_valid = old_ia_valid;
	return ret;
}

unsigned long ramfs_nommu_get_unmapped_area(struct file *file,
					    unsigned long addr, unsigned long len,
					    unsigned long pgoff, unsigned long flags)
{
	unsigned long maxpages, lpages, nr, loop, ret;
	struct inode *inode = file->f_path.dentry->d_inode;
	struct page **pages = NULL, **ptr, *page;
	loff_t isize;

	if (!(flags & MAP_SHARED))
		return addr;

	
	lpages = (len + PAGE_SIZE - 1) >> PAGE_SHIFT;
	isize = i_size_read(inode);

	ret = -EINVAL;
	maxpages = (isize + PAGE_SIZE - 1) >> PAGE_SHIFT;
	if (pgoff >= maxpages)
		goto out;

	if (maxpages - pgoff < lpages)
		goto out;

	
	ret = -ENOMEM;
	pages = kzalloc(lpages * sizeof(struct page *), GFP_KERNEL);
	if (!pages)
		goto out_free;

	nr = find_get_pages(inode->i_mapping, pgoff, lpages, pages);
	if (nr != lpages)
		goto out_free_pages; 

	
	ptr = pages;
	page = *ptr++;
	page++;
	for (loop = lpages; loop > 1; loop--)
		if (*ptr++ != page++)
			goto out_free_pages;

	
	ret = (unsigned long) page_address(pages[0]);

out_free_pages:
	ptr = pages;
	for (loop = nr; loop > 0; loop--)
		put_page(*ptr++);
out_free:
	kfree(pages);
out:
	return ret;
}

int ramfs_nommu_mmap(struct file *file, struct vm_area_struct *vma)
{
	if (!(vma->vm_flags & VM_SHARED))
		return -ENOSYS;

	file_accessed(file);
	vma->vm_ops = &generic_file_vm_ops;
	return 0;
}
