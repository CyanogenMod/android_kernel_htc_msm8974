/*
 * This file is part of UBIFS.
 *
 * Copyright (C) 2006-2008 Nokia Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Authors: Artem Bityutskiy (Битюцкий Артём)
 *          Adrian Hunter
 */


#include "ubifs.h"
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/slab.h>

static int read_block(struct inode *inode, void *addr, unsigned int block,
		      struct ubifs_data_node *dn)
{
	struct ubifs_info *c = inode->i_sb->s_fs_info;
	int err, len, out_len;
	union ubifs_key key;
	unsigned int dlen;

	data_key_init(c, &key, inode->i_ino, block);
	err = ubifs_tnc_lookup(c, &key, dn);
	if (err) {
		if (err == -ENOENT)
			
			memset(addr, 0, UBIFS_BLOCK_SIZE);
		return err;
	}

	ubifs_assert(le64_to_cpu(dn->ch.sqnum) >
		     ubifs_inode(inode)->creat_sqnum);
	len = le32_to_cpu(dn->size);
	if (len <= 0 || len > UBIFS_BLOCK_SIZE)
		goto dump;

	dlen = le32_to_cpu(dn->ch.len) - UBIFS_DATA_NODE_SZ;
	out_len = UBIFS_BLOCK_SIZE;
	err = ubifs_decompress(&dn->data, dlen, addr, &out_len,
			       le16_to_cpu(dn->compr_type));
	if (err || len != out_len)
		goto dump;

	if (len < UBIFS_BLOCK_SIZE)
		memset(addr + len, 0, UBIFS_BLOCK_SIZE - len);

	return 0;

dump:
	ubifs_err("bad data node (block %u, inode %lu)",
		  block, inode->i_ino);
	dbg_dump_node(c, dn);
	return -EINVAL;
}

static int do_readpage(struct page *page)
{
	void *addr;
	int err = 0, i;
	unsigned int block, beyond;
	struct ubifs_data_node *dn;
	struct inode *inode = page->mapping->host;
	loff_t i_size = i_size_read(inode);

	dbg_gen("ino %lu, pg %lu, i_size %lld, flags %#lx",
		inode->i_ino, page->index, i_size, page->flags);
	ubifs_assert(!PageChecked(page));
	ubifs_assert(!PagePrivate(page));

	addr = kmap(page);

	block = page->index << UBIFS_BLOCKS_PER_PAGE_SHIFT;
	beyond = (i_size + UBIFS_BLOCK_SIZE - 1) >> UBIFS_BLOCK_SHIFT;
	if (block >= beyond) {
		
		SetPageChecked(page);
		memset(addr, 0, PAGE_CACHE_SIZE);
		goto out;
	}

	dn = kmalloc(UBIFS_MAX_DATA_NODE_SZ, GFP_NOFS);
	if (!dn) {
		err = -ENOMEM;
		goto error;
	}

	i = 0;
	while (1) {
		int ret;

		if (block >= beyond) {
			
			err = -ENOENT;
			memset(addr, 0, UBIFS_BLOCK_SIZE);
		} else {
			ret = read_block(inode, addr, block, dn);
			if (ret) {
				err = ret;
				if (err != -ENOENT)
					break;
			} else if (block + 1 == beyond) {
				int dlen = le32_to_cpu(dn->size);
				int ilen = i_size & (UBIFS_BLOCK_SIZE - 1);

				if (ilen && ilen < dlen)
					memset(addr + ilen, 0, dlen - ilen);
			}
		}
		if (++i >= UBIFS_BLOCKS_PER_PAGE)
			break;
		block += 1;
		addr += UBIFS_BLOCK_SIZE;
	}
	if (err) {
		if (err == -ENOENT) {
			
			SetPageChecked(page);
			dbg_gen("hole");
			goto out_free;
		}
		ubifs_err("cannot read page %lu of inode %lu, error %d",
			  page->index, inode->i_ino, err);
		goto error;
	}

out_free:
	kfree(dn);
out:
	SetPageUptodate(page);
	ClearPageError(page);
	flush_dcache_page(page);
	kunmap(page);
	return 0;

error:
	kfree(dn);
	ClearPageUptodate(page);
	SetPageError(page);
	flush_dcache_page(page);
	kunmap(page);
	return err;
}

static void release_new_page_budget(struct ubifs_info *c)
{
	struct ubifs_budget_req req = { .recalculate = 1, .new_page = 1 };

	ubifs_release_budget(c, &req);
}

static void release_existing_page_budget(struct ubifs_info *c)
{
	struct ubifs_budget_req req = { .dd_growth = c->bi.page_budget};

	ubifs_release_budget(c, &req);
}

static int write_begin_slow(struct address_space *mapping,
			    loff_t pos, unsigned len, struct page **pagep,
			    unsigned flags)
{
	struct inode *inode = mapping->host;
	struct ubifs_info *c = inode->i_sb->s_fs_info;
	pgoff_t index = pos >> PAGE_CACHE_SHIFT;
	struct ubifs_budget_req req = { .new_page = 1 };
	int uninitialized_var(err), appending = !!(pos + len > inode->i_size);
	struct page *page;

	dbg_gen("ino %lu, pos %llu, len %u, i_size %lld",
		inode->i_ino, pos, len, inode->i_size);

	/*
	 * At the slow path we have to budget before locking the page, because
	 * budgeting may force write-back, which would wait on locked pages and
	 * deadlock if we had the page locked. At this point we do not know
	 * anything about the page, so assume that this is a new page which is
	 * written to a hole. This corresponds to largest budget. Later the
	 * budget will be amended if this is not true.
	 */
	if (appending)
		
		req.dirtied_ino = 1;

	err = ubifs_budget_space(c, &req);
	if (unlikely(err))
		return err;

	page = grab_cache_page_write_begin(mapping, index, flags);
	if (unlikely(!page)) {
		ubifs_release_budget(c, &req);
		return -ENOMEM;
	}

	if (!PageUptodate(page)) {
		if (!(pos & ~PAGE_CACHE_MASK) && len == PAGE_CACHE_SIZE)
			SetPageChecked(page);
		else {
			err = do_readpage(page);
			if (err) {
				unlock_page(page);
				page_cache_release(page);
				return err;
			}
		}

		SetPageUptodate(page);
		ClearPageError(page);
	}

	if (PagePrivate(page))
		release_new_page_budget(c);
	else if (!PageChecked(page))
		ubifs_convert_page_budget(c);

	if (appending) {
		struct ubifs_inode *ui = ubifs_inode(inode);

		mutex_lock(&ui->ui_mutex);
		if (ui->dirty)
			ubifs_release_dirty_inode_budget(c, ui);
	}

	*pagep = page;
	return 0;
}

static int allocate_budget(struct ubifs_info *c, struct page *page,
			   struct ubifs_inode *ui, int appending)
{
	struct ubifs_budget_req req = { .fast = 1 };

	if (PagePrivate(page)) {
		if (!appending)
			return 0;

		mutex_lock(&ui->ui_mutex);
		if (ui->dirty)
			return 0;

		req.dirtied_ino = 1;
	} else {
		if (PageChecked(page))
			req.new_page = 1;
		else
			req.dirtied_page = 1;

		if (appending) {
			mutex_lock(&ui->ui_mutex);
			if (!ui->dirty)
				req.dirtied_ino = 1;
		}
	}

	return ubifs_budget_space(c, &req);
}

/*
 * This function is called when a page of data is going to be written. Since
 * the page of data will not necessarily go to the flash straight away, UBIFS
 * has to reserve space on the media for it, which is done by means of
 * budgeting.
 *
 * This is the hot-path of the file-system and we are trying to optimize it as
 * much as possible. For this reasons it is split on 2 parts - slow and fast.
 *
 * There many budgeting cases:
 *     o a new page is appended - we have to budget for a new page and for
 *       changing the inode; however, if the inode is already dirty, there is
 *       no need to budget for it;
 *     o an existing clean page is changed - we have budget for it; if the page
 *       does not exist on the media (a hole), we have to budget for a new
 *       page; otherwise, we may budget for changing an existing page; the
 *       difference between these cases is that changing an existing page does
 *       not introduce anything new to the FS indexing information, so it does
 *       not grow, and smaller budget is acquired in this case;
 *     o an existing dirty page is changed - no need to budget at all, because
 *       the page budget has been acquired by earlier, when the page has been
 *       marked dirty.
 *
 * UBIFS budgeting sub-system may force write-back if it thinks there is no
 * space to reserve. This imposes some locking restrictions and makes it
 * impossible to take into account the above cases, and makes it impossible to
 * optimize budgeting.
 *
 * The solution for this is that the fast path of 'ubifs_write_begin()' assumes
 * there is a plenty of flash space and the budget will be acquired quickly,
 * without forcing write-back. The slow path does not make this assumption.
 */
static int ubifs_write_begin(struct file *file, struct address_space *mapping,
			     loff_t pos, unsigned len, unsigned flags,
			     struct page **pagep, void **fsdata)
{
	struct inode *inode = mapping->host;
	struct ubifs_info *c = inode->i_sb->s_fs_info;
	struct ubifs_inode *ui = ubifs_inode(inode);
	pgoff_t index = pos >> PAGE_CACHE_SHIFT;
	int uninitialized_var(err), appending = !!(pos + len > inode->i_size);
	int skipped_read = 0;
	struct page *page;

	ubifs_assert(ubifs_inode(inode)->ui_size == inode->i_size);
	ubifs_assert(!c->ro_media && !c->ro_mount);

	if (unlikely(c->ro_error))
		return -EROFS;

	
	page = grab_cache_page_write_begin(mapping, index, flags);
	if (unlikely(!page))
		return -ENOMEM;

	if (!PageUptodate(page)) {
		
		if (!(pos & ~PAGE_CACHE_MASK) && len == PAGE_CACHE_SIZE) {
			SetPageChecked(page);
			skipped_read = 1;
		} else {
			err = do_readpage(page);
			if (err) {
				unlock_page(page);
				page_cache_release(page);
				return err;
			}
		}

		SetPageUptodate(page);
		ClearPageError(page);
	}

	err = allocate_budget(c, page, ui, appending);
	if (unlikely(err)) {
		ubifs_assert(err == -ENOSPC);
		if (skipped_read) {
			ClearPageChecked(page);
			ClearPageUptodate(page);
		}
		if (appending) {
			ubifs_assert(mutex_is_locked(&ui->ui_mutex));
			mutex_unlock(&ui->ui_mutex);
		}
		unlock_page(page);
		page_cache_release(page);

		return write_begin_slow(mapping, pos, len, pagep, flags);
	}

	*pagep = page;
	return 0;

}

static void cancel_budget(struct ubifs_info *c, struct page *page,
			  struct ubifs_inode *ui, int appending)
{
	if (appending) {
		if (!ui->dirty)
			ubifs_release_dirty_inode_budget(c, ui);
		mutex_unlock(&ui->ui_mutex);
	}
	if (!PagePrivate(page)) {
		if (PageChecked(page))
			release_new_page_budget(c);
		else
			release_existing_page_budget(c);
	}
}

static int ubifs_write_end(struct file *file, struct address_space *mapping,
			   loff_t pos, unsigned len, unsigned copied,
			   struct page *page, void *fsdata)
{
	struct inode *inode = mapping->host;
	struct ubifs_inode *ui = ubifs_inode(inode);
	struct ubifs_info *c = inode->i_sb->s_fs_info;
	loff_t end_pos = pos + len;
	int appending = !!(end_pos > inode->i_size);

	dbg_gen("ino %lu, pos %llu, pg %lu, len %u, copied %d, i_size %lld",
		inode->i_ino, pos, page->index, len, copied, inode->i_size);

	if (unlikely(copied < len && len == PAGE_CACHE_SIZE)) {
		dbg_gen("copied %d instead of %d, read page and repeat",
			copied, len);
		cancel_budget(c, page, ui, appending);
		ClearPageChecked(page);

		copied = do_readpage(page);
		goto out;
	}

	if (!PagePrivate(page)) {
		SetPagePrivate(page);
		atomic_long_inc(&c->dirty_pg_cnt);
		__set_page_dirty_nobuffers(page);
	}

	if (appending) {
		i_size_write(inode, end_pos);
		ui->ui_size = end_pos;
		__mark_inode_dirty(inode, I_DIRTY_DATASYNC);
		ubifs_assert(mutex_is_locked(&ui->ui_mutex));
		mutex_unlock(&ui->ui_mutex);
	}

out:
	unlock_page(page);
	page_cache_release(page);
	return copied;
}

static int populate_page(struct ubifs_info *c, struct page *page,
			 struct bu_info *bu, int *n)
{
	int i = 0, nn = *n, offs = bu->zbranch[0].offs, hole = 0, read = 0;
	struct inode *inode = page->mapping->host;
	loff_t i_size = i_size_read(inode);
	unsigned int page_block;
	void *addr, *zaddr;
	pgoff_t end_index;

	dbg_gen("ino %lu, pg %lu, i_size %lld, flags %#lx",
		inode->i_ino, page->index, i_size, page->flags);

	addr = zaddr = kmap(page);

	end_index = (i_size - 1) >> PAGE_CACHE_SHIFT;
	if (!i_size || page->index > end_index) {
		hole = 1;
		memset(addr, 0, PAGE_CACHE_SIZE);
		goto out_hole;
	}

	page_block = page->index << UBIFS_BLOCKS_PER_PAGE_SHIFT;
	while (1) {
		int err, len, out_len, dlen;

		if (nn >= bu->cnt) {
			hole = 1;
			memset(addr, 0, UBIFS_BLOCK_SIZE);
		} else if (key_block(c, &bu->zbranch[nn].key) == page_block) {
			struct ubifs_data_node *dn;

			dn = bu->buf + (bu->zbranch[nn].offs - offs);

			ubifs_assert(le64_to_cpu(dn->ch.sqnum) >
				     ubifs_inode(inode)->creat_sqnum);

			len = le32_to_cpu(dn->size);
			if (len <= 0 || len > UBIFS_BLOCK_SIZE)
				goto out_err;

			dlen = le32_to_cpu(dn->ch.len) - UBIFS_DATA_NODE_SZ;
			out_len = UBIFS_BLOCK_SIZE;
			err = ubifs_decompress(&dn->data, dlen, addr, &out_len,
					       le16_to_cpu(dn->compr_type));
			if (err || len != out_len)
				goto out_err;

			if (len < UBIFS_BLOCK_SIZE)
				memset(addr + len, 0, UBIFS_BLOCK_SIZE - len);

			nn += 1;
			read = (i << UBIFS_BLOCK_SHIFT) + len;
		} else if (key_block(c, &bu->zbranch[nn].key) < page_block) {
			nn += 1;
			continue;
		} else {
			hole = 1;
			memset(addr, 0, UBIFS_BLOCK_SIZE);
		}
		if (++i >= UBIFS_BLOCKS_PER_PAGE)
			break;
		addr += UBIFS_BLOCK_SIZE;
		page_block += 1;
	}

	if (end_index == page->index) {
		int len = i_size & (PAGE_CACHE_SIZE - 1);

		if (len && len < read)
			memset(zaddr + len, 0, read - len);
	}

out_hole:
	if (hole) {
		SetPageChecked(page);
		dbg_gen("hole");
	}

	SetPageUptodate(page);
	ClearPageError(page);
	flush_dcache_page(page);
	kunmap(page);
	*n = nn;
	return 0;

out_err:
	ClearPageUptodate(page);
	SetPageError(page);
	flush_dcache_page(page);
	kunmap(page);
	ubifs_err("bad data node (block %u, inode %lu)",
		  page_block, inode->i_ino);
	return -EINVAL;
}

static int ubifs_do_bulk_read(struct ubifs_info *c, struct bu_info *bu,
			      struct page *page1)
{
	pgoff_t offset = page1->index, end_index;
	struct address_space *mapping = page1->mapping;
	struct inode *inode = mapping->host;
	struct ubifs_inode *ui = ubifs_inode(inode);
	int err, page_idx, page_cnt, ret = 0, n = 0;
	int allocate = bu->buf ? 0 : 1;
	loff_t isize;

	err = ubifs_tnc_get_bu_keys(c, bu);
	if (err)
		goto out_warn;

	if (bu->eof) {
		
		ui->read_in_a_row = 1;
		ui->bulk_read = 0;
	}

	page_cnt = bu->blk_cnt >> UBIFS_BLOCKS_PER_PAGE_SHIFT;
	if (!page_cnt) {
		goto out_bu_off;
	}

	if (bu->cnt) {
		if (allocate) {
			bu->buf_len = bu->zbranch[bu->cnt - 1].offs +
				      bu->zbranch[bu->cnt - 1].len -
				      bu->zbranch[0].offs;
			ubifs_assert(bu->buf_len > 0);
			ubifs_assert(bu->buf_len <= c->leb_size);
			bu->buf = kmalloc(bu->buf_len, GFP_NOFS | __GFP_NOWARN);
			if (!bu->buf)
				goto out_bu_off;
		}

		err = ubifs_tnc_bulk_read(c, bu);
		if (err)
			goto out_warn;
	}

	err = populate_page(c, page1, bu, &n);
	if (err)
		goto out_warn;

	unlock_page(page1);
	ret = 1;

	isize = i_size_read(inode);
	if (isize == 0)
		goto out_free;
	end_index = ((isize - 1) >> PAGE_CACHE_SHIFT);

	for (page_idx = 1; page_idx < page_cnt; page_idx++) {
		pgoff_t page_offset = offset + page_idx;
		struct page *page;

		if (page_offset > end_index)
			break;
		page = find_or_create_page(mapping, page_offset,
					   GFP_NOFS | __GFP_COLD);
		if (!page)
			break;
		if (!PageUptodate(page))
			err = populate_page(c, page, bu, &n);
		unlock_page(page);
		page_cache_release(page);
		if (err)
			break;
	}

	ui->last_page_read = offset + page_idx - 1;

out_free:
	if (allocate)
		kfree(bu->buf);
	return ret;

out_warn:
	ubifs_warn("ignoring error %d and skipping bulk-read", err);
	goto out_free;

out_bu_off:
	ui->read_in_a_row = ui->bulk_read = 0;
	goto out_free;
}

static int ubifs_bulk_read(struct page *page)
{
	struct inode *inode = page->mapping->host;
	struct ubifs_info *c = inode->i_sb->s_fs_info;
	struct ubifs_inode *ui = ubifs_inode(inode);
	pgoff_t index = page->index, last_page_read = ui->last_page_read;
	struct bu_info *bu;
	int err = 0, allocated = 0;

	ui->last_page_read = index;
	if (!c->bulk_read)
		return 0;

	if (!mutex_trylock(&ui->ui_mutex))
		return 0;

	if (index != last_page_read + 1) {
		
		ui->read_in_a_row = 1;
		if (ui->bulk_read)
			ui->bulk_read = 0;
		goto out_unlock;
	}

	if (!ui->bulk_read) {
		ui->read_in_a_row += 1;
		if (ui->read_in_a_row < 3)
			goto out_unlock;
		
		ui->bulk_read = 1;
	}

	if (mutex_trylock(&c->bu_mutex))
		bu = &c->bu;
	else {
		bu = kmalloc(sizeof(struct bu_info), GFP_NOFS | __GFP_NOWARN);
		if (!bu)
			goto out_unlock;

		bu->buf = NULL;
		allocated = 1;
	}

	bu->buf_len = c->max_bu_buf_len;
	data_key_init(c, &bu->key, inode->i_ino,
		      page->index << UBIFS_BLOCKS_PER_PAGE_SHIFT);
	err = ubifs_do_bulk_read(c, bu, page);

	if (!allocated)
		mutex_unlock(&c->bu_mutex);
	else
		kfree(bu);

out_unlock:
	mutex_unlock(&ui->ui_mutex);
	return err;
}

static int ubifs_readpage(struct file *file, struct page *page)
{
	if (ubifs_bulk_read(page))
		return 0;
	do_readpage(page);
	unlock_page(page);
	return 0;
}

static int do_writepage(struct page *page, int len)
{
	int err = 0, i, blen;
	unsigned int block;
	void *addr;
	union ubifs_key key;
	struct inode *inode = page->mapping->host;
	struct ubifs_info *c = inode->i_sb->s_fs_info;

#ifdef UBIFS_DEBUG
	spin_lock(&ui->ui_lock);
	ubifs_assert(page->index <= ui->synced_i_size << PAGE_CACHE_SIZE);
	spin_unlock(&ui->ui_lock);
#endif

	
	set_page_writeback(page);

	addr = kmap(page);
	block = page->index << UBIFS_BLOCKS_PER_PAGE_SHIFT;
	i = 0;
	while (len) {
		blen = min_t(int, len, UBIFS_BLOCK_SIZE);
		data_key_init(c, &key, inode->i_ino, block);
		err = ubifs_jnl_write_data(c, inode, &key, addr, blen);
		if (err)
			break;
		if (++i >= UBIFS_BLOCKS_PER_PAGE)
			break;
		block += 1;
		addr += blen;
		len -= blen;
	}
	if (err) {
		SetPageError(page);
		ubifs_err("cannot write page %lu of inode %lu, error %d",
			  page->index, inode->i_ino, err);
		ubifs_ro_mode(c, err);
	}

	ubifs_assert(PagePrivate(page));
	if (PageChecked(page))
		release_new_page_budget(c);
	else
		release_existing_page_budget(c);

	atomic_long_dec(&c->dirty_pg_cnt);
	ClearPagePrivate(page);
	ClearPageChecked(page);

	kunmap(page);
	unlock_page(page);
	end_page_writeback(page);
	return err;
}

/*
 * When writing-back dirty inodes, VFS first writes-back pages belonging to the
 * inode, then the inode itself. For UBIFS this may cause a problem. Consider a
 * situation when a we have an inode with size 0, then a megabyte of data is
 * appended to the inode, then write-back starts and flushes some amount of the
 * dirty pages, the journal becomes full, commit happens and finishes, and then
 * an unclean reboot happens. When the file system is mounted next time, the
 * inode size would still be 0, but there would be many pages which are beyond
 * the inode size, they would be indexed and consume flash space. Because the
 * journal has been committed, the replay would not be able to detect this
 * situation and correct the inode size. This means UBIFS would have to scan
 * whole index and correct all inode sizes, which is long an unacceptable.
 *
 * To prevent situations like this, UBIFS writes pages back only if they are
 * within the last synchronized inode size, i.e. the size which has been
 * written to the flash media last time. Otherwise, UBIFS forces inode
 * write-back, thus making sure the on-flash inode contains current inode size,
 * and then keeps writing pages back.
 *
 * Some locking issues explanation. 'ubifs_writepage()' first is called with
 * the page locked, and it locks @ui_mutex. However, write-back does take inode
 * @i_mutex, which means other VFS operations may be run on this inode at the
 * same time. And the problematic one is truncation to smaller size, from where
 * we have to call 'truncate_setsize()', which first changes @inode->i_size,
 * then drops the truncated pages. And while dropping the pages, it takes the
 * page lock. This means that 'do_truncation()' cannot call 'truncate_setsize()'
 * with @ui_mutex locked, because it would deadlock with 'ubifs_writepage()'.
 * This means that @inode->i_size is changed while @ui_mutex is unlocked.
 *
 * XXX(truncate): with the new truncate sequence this is not true anymore,
 * and the calls to truncate_setsize can be move around freely.  They should
 * be moved to the very end of the truncate sequence.
 *
 * But in 'ubifs_writepage()' we have to guarantee that we do not write beyond
 * inode size. How do we do this if @inode->i_size may became smaller while we
 * are in the middle of 'ubifs_writepage()'? The UBIFS solution is the
 * @ui->ui_isize "shadow" field which UBIFS uses instead of @inode->i_size
 * internally and updates it under @ui_mutex.
 *
 * Q: why we do not worry that if we race with truncation, we may end up with a
 * situation when the inode is truncated while we are in the middle of
 * 'do_writepage()', so we do write beyond inode size?
 * A: If we are in the middle of 'do_writepage()', truncation would be locked
 * on the page lock and it would not write the truncated inode node to the
 * journal before we have finished.
 */
static int ubifs_writepage(struct page *page, struct writeback_control *wbc)
{
	struct inode *inode = page->mapping->host;
	struct ubifs_inode *ui = ubifs_inode(inode);
	loff_t i_size =  i_size_read(inode), synced_i_size;
	pgoff_t end_index = i_size >> PAGE_CACHE_SHIFT;
	int err, len = i_size & (PAGE_CACHE_SIZE - 1);
	void *kaddr;

	dbg_gen("ino %lu, pg %lu, pg flags %#lx",
		inode->i_ino, page->index, page->flags);
	ubifs_assert(PagePrivate(page));

	
	if (page->index > end_index || (page->index == end_index && !len)) {
		err = 0;
		goto out_unlock;
	}

	spin_lock(&ui->ui_lock);
	synced_i_size = ui->synced_i_size;
	spin_unlock(&ui->ui_lock);

	
	if (page->index < end_index) {
		if (page->index >= synced_i_size >> PAGE_CACHE_SHIFT) {
			err = inode->i_sb->s_op->write_inode(inode, NULL);
			if (err)
				goto out_unlock;
			/*
			 * The inode has been written, but the write-buffer has
			 * not been synchronized, so in case of an unclean
			 * reboot we may end up with some pages beyond inode
			 * size, but they would be in the journal (because
			 * commit flushes write buffers) and recovery would deal
			 * with this.
			 */
		}
		return do_writepage(page, PAGE_CACHE_SIZE);
	}

	/*
	 * The page straddles @i_size. It must be zeroed out on each and every
	 * writepage invocation because it may be mmapped. "A file is mapped
	 * in multiples of the page size. For a file that is not a multiple of
	 * the page size, the remaining memory is zeroed when mapped, and
	 * writes to that region are not written out to the file."
	 */
	kaddr = kmap_atomic(page);
	memset(kaddr + len, 0, PAGE_CACHE_SIZE - len);
	flush_dcache_page(page);
	kunmap_atomic(kaddr);

	if (i_size > synced_i_size) {
		err = inode->i_sb->s_op->write_inode(inode, NULL);
		if (err)
			goto out_unlock;
	}

	return do_writepage(page, len);

out_unlock:
	unlock_page(page);
	return err;
}

static void do_attr_changes(struct inode *inode, const struct iattr *attr)
{
	if (attr->ia_valid & ATTR_UID)
		inode->i_uid = attr->ia_uid;
	if (attr->ia_valid & ATTR_GID)
		inode->i_gid = attr->ia_gid;
	if (attr->ia_valid & ATTR_ATIME)
		inode->i_atime = timespec_trunc(attr->ia_atime,
						inode->i_sb->s_time_gran);
	if (attr->ia_valid & ATTR_MTIME)
		inode->i_mtime = timespec_trunc(attr->ia_mtime,
						inode->i_sb->s_time_gran);
	if (attr->ia_valid & ATTR_CTIME)
		inode->i_ctime = timespec_trunc(attr->ia_ctime,
						inode->i_sb->s_time_gran);
	if (attr->ia_valid & ATTR_MODE) {
		umode_t mode = attr->ia_mode;

		if (!in_group_p(inode->i_gid) && !capable(CAP_FSETID))
			mode &= ~S_ISGID;
		inode->i_mode = mode;
	}
}

static int do_truncation(struct ubifs_info *c, struct inode *inode,
			 const struct iattr *attr)
{
	int err;
	struct ubifs_budget_req req;
	loff_t old_size = inode->i_size, new_size = attr->ia_size;
	int offset = new_size & (UBIFS_BLOCK_SIZE - 1), budgeted = 1;
	struct ubifs_inode *ui = ubifs_inode(inode);

	dbg_gen("ino %lu, size %lld -> %lld", inode->i_ino, old_size, new_size);
	memset(&req, 0, sizeof(struct ubifs_budget_req));

	/*
	 * If this is truncation to a smaller size, and we do not truncate on a
	 * block boundary, budget for changing one data block, because the last
	 * block will be re-written.
	 */
	if (new_size & (UBIFS_BLOCK_SIZE - 1))
		req.dirtied_page = 1;

	req.dirtied_ino = 1;
	
	req.dirtied_ino_d = UBIFS_TRUN_NODE_SZ;
	err = ubifs_budget_space(c, &req);
	if (err) {
		if (new_size || err != -ENOSPC)
			return err;
		budgeted = 0;
	}

	truncate_setsize(inode, new_size);

	if (offset) {
		pgoff_t index = new_size >> PAGE_CACHE_SHIFT;
		struct page *page;

		page = find_lock_page(inode->i_mapping, index);
		if (page) {
			if (PageDirty(page)) {
				ubifs_assert(PagePrivate(page));

				clear_page_dirty_for_io(page);
				if (UBIFS_BLOCKS_PER_PAGE_SHIFT)
					offset = new_size &
						 (PAGE_CACHE_SIZE - 1);
				err = do_writepage(page, offset);
				page_cache_release(page);
				if (err)
					goto out_budg;
			} else {
				unlock_page(page);
				page_cache_release(page);
			}
		}
	}

	mutex_lock(&ui->ui_mutex);
	ui->ui_size = inode->i_size;
	
	inode->i_mtime = inode->i_ctime = ubifs_current_time(inode);
	
	do_attr_changes(inode, attr);
	err = ubifs_jnl_truncate(c, inode, old_size, new_size);
	mutex_unlock(&ui->ui_mutex);

out_budg:
	if (budgeted)
		ubifs_release_budget(c, &req);
	else {
		c->bi.nospace = c->bi.nospace_rp = 0;
		smp_wmb();
	}
	return err;
}

static int do_setattr(struct ubifs_info *c, struct inode *inode,
		      const struct iattr *attr)
{
	int err, release;
	loff_t new_size = attr->ia_size;
	struct ubifs_inode *ui = ubifs_inode(inode);
	struct ubifs_budget_req req = { .dirtied_ino = 1,
				.dirtied_ino_d = ALIGN(ui->data_len, 8) };

	err = ubifs_budget_space(c, &req);
	if (err)
		return err;

	if (attr->ia_valid & ATTR_SIZE) {
		dbg_gen("size %lld -> %lld", inode->i_size, new_size);
		truncate_setsize(inode, new_size);
	}

	mutex_lock(&ui->ui_mutex);
	if (attr->ia_valid & ATTR_SIZE) {
		
		inode->i_mtime = inode->i_ctime = ubifs_current_time(inode);
		
		ui->ui_size = inode->i_size;
	}

	do_attr_changes(inode, attr);

	release = ui->dirty;
	if (attr->ia_valid & ATTR_SIZE)
		 __mark_inode_dirty(inode, I_DIRTY_SYNC | I_DIRTY_DATASYNC);
	else
		mark_inode_dirty_sync(inode);
	mutex_unlock(&ui->ui_mutex);

	if (release)
		ubifs_release_budget(c, &req);
	if (IS_SYNC(inode))
		err = inode->i_sb->s_op->write_inode(inode, NULL);
	return err;
}

int ubifs_setattr(struct dentry *dentry, struct iattr *attr)
{
	int err;
	struct inode *inode = dentry->d_inode;
	struct ubifs_info *c = inode->i_sb->s_fs_info;

	dbg_gen("ino %lu, mode %#x, ia_valid %#x",
		inode->i_ino, inode->i_mode, attr->ia_valid);
	err = inode_change_ok(inode, attr);
	if (err)
		return err;

	err = dbg_check_synced_i_size(c, inode);
	if (err)
		return err;

	if ((attr->ia_valid & ATTR_SIZE) && attr->ia_size < inode->i_size)
		
		err = do_truncation(c, inode, attr);
	else
		err = do_setattr(c, inode, attr);

	return err;
}

static void ubifs_invalidatepage(struct page *page, unsigned long offset)
{
	struct inode *inode = page->mapping->host;
	struct ubifs_info *c = inode->i_sb->s_fs_info;

	ubifs_assert(PagePrivate(page));
	if (offset)
		
		return;

	if (PageChecked(page))
		release_new_page_budget(c);
	else
		release_existing_page_budget(c);

	atomic_long_dec(&c->dirty_pg_cnt);
	ClearPagePrivate(page);
	ClearPageChecked(page);
}

static void *ubifs_follow_link(struct dentry *dentry, struct nameidata *nd)
{
	struct ubifs_inode *ui = ubifs_inode(dentry->d_inode);

	nd_set_link(nd, ui->data);
	return NULL;
}

int ubifs_fsync(struct file *file, loff_t start, loff_t end, int datasync)
{
	struct inode *inode = file->f_mapping->host;
	struct ubifs_info *c = inode->i_sb->s_fs_info;
	int err;

	dbg_gen("syncing inode %lu", inode->i_ino);

	if (c->ro_mount)
		return 0;

	err = filemap_write_and_wait_range(inode->i_mapping, start, end);
	if (err)
		return err;
	mutex_lock(&inode->i_mutex);

	
	if (!datasync || (inode->i_state & I_DIRTY_DATASYNC)) {
		err = inode->i_sb->s_op->write_inode(inode, NULL);
		if (err)
			goto out;
	}

	err = ubifs_sync_wbufs_by_inode(c, inode);
out:
	mutex_unlock(&inode->i_mutex);
	return err;
}

static inline int mctime_update_needed(const struct inode *inode,
				       const struct timespec *now)
{
	if (!timespec_equal(&inode->i_mtime, now) ||
	    !timespec_equal(&inode->i_ctime, now))
		return 1;
	return 0;
}

static int update_mctime(struct ubifs_info *c, struct inode *inode)
{
	struct timespec now = ubifs_current_time(inode);
	struct ubifs_inode *ui = ubifs_inode(inode);

	if (mctime_update_needed(inode, &now)) {
		int err, release;
		struct ubifs_budget_req req = { .dirtied_ino = 1,
				.dirtied_ino_d = ALIGN(ui->data_len, 8) };

		err = ubifs_budget_space(c, &req);
		if (err)
			return err;

		mutex_lock(&ui->ui_mutex);
		inode->i_mtime = inode->i_ctime = ubifs_current_time(inode);
		release = ui->dirty;
		mark_inode_dirty_sync(inode);
		mutex_unlock(&ui->ui_mutex);
		if (release)
			ubifs_release_budget(c, &req);
	}

	return 0;
}

static ssize_t ubifs_aio_write(struct kiocb *iocb, const struct iovec *iov,
			       unsigned long nr_segs, loff_t pos)
{
	int err;
	struct inode *inode = iocb->ki_filp->f_mapping->host;
	struct ubifs_info *c = inode->i_sb->s_fs_info;

	err = update_mctime(c, inode);
	if (err)
		return err;

	return generic_file_aio_write(iocb, iov, nr_segs, pos);
}

static int ubifs_set_page_dirty(struct page *page)
{
	int ret;

	ret = __set_page_dirty_nobuffers(page);
	ubifs_assert(ret == 0);
	return ret;
}

static int ubifs_releasepage(struct page *page, gfp_t unused_gfp_flags)
{
	if (PageWriteback(page))
		return 0;
	ubifs_assert(PagePrivate(page));
	ubifs_assert(0);
	ClearPagePrivate(page);
	ClearPageChecked(page);
	return 1;
}

static int ubifs_vm_page_mkwrite(struct vm_area_struct *vma,
				 struct vm_fault *vmf)
{
	struct page *page = vmf->page;
	struct inode *inode = vma->vm_file->f_path.dentry->d_inode;
	struct ubifs_info *c = inode->i_sb->s_fs_info;
	struct timespec now = ubifs_current_time(inode);
	struct ubifs_budget_req req = { .new_page = 1 };
	int err, update_time;

	dbg_gen("ino %lu, pg %lu, i_size %lld",	inode->i_ino, page->index,
		i_size_read(inode));
	ubifs_assert(!c->ro_media && !c->ro_mount);

	if (unlikely(c->ro_error))
		return VM_FAULT_SIGBUS; 

	update_time = mctime_update_needed(inode, &now);
	if (update_time)
		req.dirtied_ino = 1;

	err = ubifs_budget_space(c, &req);
	if (unlikely(err)) {
		if (err == -ENOSPC)
			ubifs_warn("out of space for mmapped file "
				   "(inode number %lu)", inode->i_ino);
		return VM_FAULT_SIGBUS;
	}

	lock_page(page);
	if (unlikely(page->mapping != inode->i_mapping ||
		     page_offset(page) > i_size_read(inode))) {
		
		err = -EINVAL;
		goto out_unlock;
	}

	if (PagePrivate(page))
		release_new_page_budget(c);
	else {
		if (!PageChecked(page))
			ubifs_convert_page_budget(c);
		SetPagePrivate(page);
		atomic_long_inc(&c->dirty_pg_cnt);
		__set_page_dirty_nobuffers(page);
	}

	if (update_time) {
		int release;
		struct ubifs_inode *ui = ubifs_inode(inode);

		mutex_lock(&ui->ui_mutex);
		inode->i_mtime = inode->i_ctime = ubifs_current_time(inode);
		release = ui->dirty;
		mark_inode_dirty_sync(inode);
		mutex_unlock(&ui->ui_mutex);
		if (release)
			ubifs_release_dirty_inode_budget(c, ui);
	}

	unlock_page(page);
	return 0;

out_unlock:
	unlock_page(page);
	ubifs_release_budget(c, &req);
	if (err)
		err = VM_FAULT_SIGBUS;
	return err;
}

static const struct vm_operations_struct ubifs_file_vm_ops = {
	.fault        = filemap_fault,
	.page_mkwrite = ubifs_vm_page_mkwrite,
};

static int ubifs_file_mmap(struct file *file, struct vm_area_struct *vma)
{
	int err;

	err = generic_file_mmap(file, vma);
	if (err)
		return err;
	vma->vm_ops = &ubifs_file_vm_ops;
	return 0;
}

const struct address_space_operations ubifs_file_address_operations = {
	.readpage       = ubifs_readpage,
	.writepage      = ubifs_writepage,
	.write_begin    = ubifs_write_begin,
	.write_end      = ubifs_write_end,
	.invalidatepage = ubifs_invalidatepage,
	.set_page_dirty = ubifs_set_page_dirty,
	.releasepage    = ubifs_releasepage,
};

const struct inode_operations ubifs_file_inode_operations = {
	.setattr     = ubifs_setattr,
	.getattr     = ubifs_getattr,
#ifdef CONFIG_UBIFS_FS_XATTR
	.setxattr    = ubifs_setxattr,
	.getxattr    = ubifs_getxattr,
	.listxattr   = ubifs_listxattr,
	.removexattr = ubifs_removexattr,
#endif
};

const struct inode_operations ubifs_symlink_inode_operations = {
	.readlink    = generic_readlink,
	.follow_link = ubifs_follow_link,
	.setattr     = ubifs_setattr,
	.getattr     = ubifs_getattr,
};

const struct file_operations ubifs_file_operations = {
	.llseek         = generic_file_llseek,
	.read           = do_sync_read,
	.write          = do_sync_write,
	.aio_read       = generic_file_aio_read,
	.aio_write      = ubifs_aio_write,
	.mmap           = ubifs_file_mmap,
	.fsync          = ubifs_fsync,
	.unlocked_ioctl = ubifs_ioctl,
	.splice_read	= generic_file_splice_read,
	.splice_write	= generic_file_splice_write,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = ubifs_compat_ioctl,
#endif
};
