/*
 * Copyright 2000 by Hans Reiser, licensing governed by reiserfs/README
 */

#include <linux/time.h>
#include <linux/fs.h>
#include "reiserfs.h"
#include "acl.h"
#include "xattr.h"
#include <linux/exportfs.h>
#include <linux/pagemap.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <asm/unaligned.h>
#include <linux/buffer_head.h>
#include <linux/mpage.h>
#include <linux/writeback.h>
#include <linux/quotaops.h>
#include <linux/swap.h>

int reiserfs_commit_write(struct file *f, struct page *page,
			  unsigned from, unsigned to);

void reiserfs_evict_inode(struct inode *inode)
{
	
	int jbegin_count =
	    JOURNAL_PER_BALANCE_CNT * 2 +
	    2 * REISERFS_QUOTA_INIT_BLOCKS(inode->i_sb);
	struct reiserfs_transaction_handle th;
	int depth;
	int err;

	if (!inode->i_nlink && !is_bad_inode(inode))
		dquot_initialize(inode);

	truncate_inode_pages(&inode->i_data, 0);
	if (inode->i_nlink)
		goto no_delete;

	depth = reiserfs_write_lock_once(inode->i_sb);

	
	if (!(inode->i_state & I_NEW) && INODE_PKEY(inode)->k_objectid != 0) {	
		reiserfs_delete_xattrs(inode);

		if (journal_begin(&th, inode->i_sb, jbegin_count))
			goto out;
		reiserfs_update_inode_transaction(inode);

		reiserfs_discard_prealloc(&th, inode);

		err = reiserfs_delete_object(&th, inode);

		if (!err) 
			dquot_free_inode(inode);

		if (journal_end(&th, inode->i_sb, jbegin_count))
			goto out;

		if (err)
		    goto out;

		
		remove_save_link(inode, 0  );	
	} else {
		
		;
	}
      out:
	end_writeback(inode);	
	dquot_drop(inode);
	inode->i_blocks = 0;
	reiserfs_write_unlock_once(inode->i_sb, depth);
	return;

no_delete:
	end_writeback(inode);
	dquot_drop(inode);
}

static void _make_cpu_key(struct cpu_key *key, int version, __u32 dirid,
			  __u32 objectid, loff_t offset, int type, int length)
{
	key->version = version;

	key->on_disk_key.k_dir_id = dirid;
	key->on_disk_key.k_objectid = objectid;
	set_cpu_key_k_offset(key, offset);
	set_cpu_key_k_type(key, type);
	key->key_length = length;
}

void make_cpu_key(struct cpu_key *key, struct inode *inode, loff_t offset,
		  int type, int length)
{
	_make_cpu_key(key, get_inode_item_key_version(inode),
		      le32_to_cpu(INODE_PKEY(inode)->k_dir_id),
		      le32_to_cpu(INODE_PKEY(inode)->k_objectid), offset, type,
		      length);
}

inline void make_le_item_head(struct item_head *ih, const struct cpu_key *key,
			      int version,
			      loff_t offset, int type, int length,
			      int entry_count  )
{
	if (key) {
		ih->ih_key.k_dir_id = cpu_to_le32(key->on_disk_key.k_dir_id);
		ih->ih_key.k_objectid =
		    cpu_to_le32(key->on_disk_key.k_objectid);
	}
	put_ih_version(ih, version);
	set_le_ih_k_offset(ih, offset);
	set_le_ih_k_type(ih, type);
	put_ih_item_len(ih, length);
	
	
	
	put_ih_entry_count(ih, entry_count);
}




static inline void fix_tail_page_for_writing(struct page *page)
{
	struct buffer_head *head, *next, *bh;

	if (page && page_has_buffers(page)) {
		head = page_buffers(page);
		bh = head;
		do {
			next = bh->b_this_page;
			if (buffer_mapped(bh) && bh->b_blocknr == 0) {
				reiserfs_unmap_buffer(bh);
			}
			bh = next;
		} while (bh != head);
	}
}

static inline int allocation_needed(int retval, b_blocknr_t allocated,
				    struct item_head *ih,
				    __le32 * item, int pos_in_item)
{
	if (allocated)
		return 0;
	if (retval == POSITION_FOUND && is_indirect_le_ih(ih) &&
	    get_block_num(item, pos_in_item))
		return 0;
	return 1;
}

static inline int indirect_item_found(int retval, struct item_head *ih)
{
	return (retval == POSITION_FOUND) && is_indirect_le_ih(ih);
}

static inline void set_block_dev_mapped(struct buffer_head *bh,
					b_blocknr_t block, struct inode *inode)
{
	map_bh(bh, inode->i_sb, block);
}

static int file_capable(struct inode *inode, sector_t block)
{
	if (get_inode_item_key_version(inode) != KEY_FORMAT_3_5 ||	
	    block < (1 << (31 - inode->i_sb->s_blocksize_bits)))	
		return 1;

	return 0;
}

static int restart_transaction(struct reiserfs_transaction_handle *th,
			       struct inode *inode, struct treepath *path)
{
	struct super_block *s = th->t_super;
	int len = th->t_blocks_allocated;
	int err;

	BUG_ON(!th->t_trans_id);
	BUG_ON(!th->t_refcount);

	pathrelse(path);

	
	if (th->t_refcount > 1) {
		return 0;
	}
	reiserfs_update_sd(th, inode);
	err = journal_end(th, s, len);
	if (!err) {
		err = journal_begin(th, s, JOURNAL_PER_BALANCE_CNT * 6);
		if (!err)
			reiserfs_update_inode_transaction(inode);
	}
	return err;
}



static int _get_block_create_0(struct inode *inode, sector_t block,
			       struct buffer_head *bh_result, int args)
{
	INITIALIZE_PATH(path);
	struct cpu_key key;
	struct buffer_head *bh;
	struct item_head *ih, tmp_ih;
	b_blocknr_t blocknr;
	char *p = NULL;
	int chars;
	int ret;
	int result;
	int done = 0;
	unsigned long offset;

	
	make_cpu_key(&key, inode,
		     (loff_t) block * inode->i_sb->s_blocksize + 1, TYPE_ANY,
		     3);

	result = search_for_position_by_key(inode->i_sb, &key, &path);
	if (result != POSITION_FOUND) {
		pathrelse(&path);
		if (p)
			kunmap(bh_result->b_page);
		if (result == IO_ERROR)
			return -EIO;
		
		// That there is some MMAPED data associated with it that is yet to be written to disk.
		if ((args & GET_BLOCK_NO_HOLE)
		    && !PageUptodate(bh_result->b_page)) {
			return -ENOENT;
		}
		return 0;
	}
	
	bh = get_last_bh(&path);
	ih = get_ih(&path);
	if (is_indirect_le_ih(ih)) {
		__le32 *ind_item = (__le32 *) B_I_PITEM(bh, ih);

		blocknr = get_block_num(ind_item, path.pos_in_item);
		ret = 0;
		if (blocknr) {
			map_bh(bh_result, inode->i_sb, blocknr);
			if (path.pos_in_item ==
			    ((ih_item_len(ih) / UNFM_P_SIZE) - 1)) {
				set_buffer_boundary(bh_result);
			}
		} else
			
			// That there is some MMAPED data associated with it that is yet to  be written to disk.
		if ((args & GET_BLOCK_NO_HOLE)
			    && !PageUptodate(bh_result->b_page)) {
			ret = -ENOENT;
		}

		pathrelse(&path);
		if (p)
			kunmap(bh_result->b_page);
		return ret;
	}
	
	if (!(args & GET_BLOCK_READ_DIRECT)) {
		
		
		pathrelse(&path);
		if (p)
			kunmap(bh_result->b_page);
		return -ENOENT;
	}

	if (buffer_uptodate(bh_result)) {
		goto finished;
	} else
	if (!bh_result->b_page || PageUptodate(bh_result->b_page)) {
		set_buffer_uptodate(bh_result);
		goto finished;
	}
	
	offset = (cpu_key_k_offset(&key) - 1) & (PAGE_CACHE_SIZE - 1);
	copy_item_head(&tmp_ih, ih);

	if (!p)
		p = (char *)kmap(bh_result->b_page);

	p += offset;
	memset(p, 0, inode->i_sb->s_blocksize);
	do {
		if (!is_direct_le_ih(ih)) {
			BUG();
		}
		if ((le_ih_k_offset(ih) + path.pos_in_item) > inode->i_size)
			break;
		if ((le_ih_k_offset(ih) - 1 + ih_item_len(ih)) > inode->i_size) {
			chars =
			    inode->i_size - (le_ih_k_offset(ih) - 1) -
			    path.pos_in_item;
			done = 1;
		} else {
			chars = ih_item_len(ih) - path.pos_in_item;
		}
		memcpy(p, B_I_PITEM(bh, ih) + path.pos_in_item, chars);

		if (done)
			break;

		p += chars;

		if (PATH_LAST_POSITION(&path) != (B_NR_ITEMS(bh) - 1))
			
			
			
			
			break;

		
		set_cpu_key_k_offset(&key, cpu_key_k_offset(&key) + chars);
		result = search_for_position_by_key(inode->i_sb, &key, &path);
		if (result != POSITION_FOUND)
			
			break;
		bh = get_last_bh(&path);
		ih = get_ih(&path);
	} while (1);

	flush_dcache_page(bh_result->b_page);
	kunmap(bh_result->b_page);

      finished:
	pathrelse(&path);

	if (result == IO_ERROR)
		return -EIO;

	map_bh(bh_result, inode->i_sb, 0);
	set_buffer_uptodate(bh_result);
	return 0;
}

static int reiserfs_bmap(struct inode *inode, sector_t block,
			 struct buffer_head *bh_result, int create)
{
	if (!file_capable(inode, block))
		return -EFBIG;

	reiserfs_write_lock(inode->i_sb);
	
	_get_block_create_0(inode, block, bh_result, 0);
	reiserfs_write_unlock(inode->i_sb);
	return 0;
}

static int reiserfs_get_block_create_0(struct inode *inode, sector_t block,
				       struct buffer_head *bh_result,
				       int create)
{
	return reiserfs_get_block(inode, block, bh_result, GET_BLOCK_NO_HOLE);
}

static int reiserfs_get_blocks_direct_io(struct inode *inode,
					 sector_t iblock,
					 struct buffer_head *bh_result,
					 int create)
{
	int ret;

	bh_result->b_page = NULL;

	bh_result->b_size = (1 << inode->i_blkbits);

	ret = reiserfs_get_block(inode, iblock, bh_result,
				 create | GET_BLOCK_NO_DANGLE);
	if (ret)
		goto out;

	
	if (buffer_mapped(bh_result) && bh_result->b_blocknr == 0) {
		clear_buffer_mapped(bh_result);
		ret = -EINVAL;
	}
	if (REISERFS_I(inode)->i_flags & i_pack_on_close_mask) {
		int err;

		reiserfs_write_lock(inode->i_sb);

		err = reiserfs_commit_for_inode(inode);
		REISERFS_I(inode)->i_flags &= ~i_pack_on_close_mask;

		reiserfs_write_unlock(inode->i_sb);

		if (err < 0)
			ret = err;
	}
      out:
	return ret;
}

static int convert_tail_for_hole(struct inode *inode,
				 struct buffer_head *bh_result,
				 loff_t tail_offset)
{
	unsigned long index;
	unsigned long tail_end;
	unsigned long tail_start;
	struct page *tail_page;
	struct page *hole_page = bh_result->b_page;
	int retval = 0;

	if ((tail_offset & (bh_result->b_size - 1)) != 1)
		return -EIO;

	
	tail_start = tail_offset & (PAGE_CACHE_SIZE - 1);
	tail_end = (tail_start | (bh_result->b_size - 1)) + 1;

	index = tail_offset >> PAGE_CACHE_SHIFT;
	if (!hole_page || index != hole_page->index) {
		tail_page = grab_cache_page(inode->i_mapping, index);
		retval = -ENOMEM;
		if (!tail_page) {
			goto out;
		}
	} else {
		tail_page = hole_page;
	}

	fix_tail_page_for_writing(tail_page);
	retval = __reiserfs_write_begin(tail_page, tail_start,
				      tail_end - tail_start);
	if (retval)
		goto unlock;

	
	flush_dcache_page(tail_page);

	retval = reiserfs_commit_write(NULL, tail_page, tail_start, tail_end);

      unlock:
	if (tail_page != hole_page) {
		unlock_page(tail_page);
		page_cache_release(tail_page);
	}
      out:
	return retval;
}

static inline int _allocate_block(struct reiserfs_transaction_handle *th,
				  sector_t block,
				  struct inode *inode,
				  b_blocknr_t * allocated_block_nr,
				  struct treepath *path, int flags)
{
	BUG_ON(!th->t_trans_id);

#ifdef REISERFS_PREALLOCATE
	if (!(flags & GET_BLOCK_NO_IMUX)) {
		return reiserfs_new_unf_blocknrs2(th, inode, allocated_block_nr,
						  path, block);
	}
#endif
	return reiserfs_new_unf_blocknrs(th, inode, allocated_block_nr, path,
					 block);
}

int reiserfs_get_block(struct inode *inode, sector_t block,
		       struct buffer_head *bh_result, int create)
{
	int repeat, retval = 0;
	b_blocknr_t allocated_block_nr = 0;	
	INITIALIZE_PATH(path);
	int pos_in_item;
	struct cpu_key key;
	struct buffer_head *bh, *unbh = NULL;
	struct item_head *ih, tmp_ih;
	__le32 *item;
	int done;
	int fs_gen;
	int lock_depth;
	struct reiserfs_transaction_handle *th = NULL;
	int jbegin_count =
	    JOURNAL_PER_BALANCE_CNT * 3 + 1 +
	    2 * REISERFS_QUOTA_TRANS_BLOCKS(inode->i_sb);
	int version;
	int dangle = 1;
	loff_t new_offset =
	    (((loff_t) block) << inode->i_sb->s_blocksize_bits) + 1;

	lock_depth = reiserfs_write_lock_once(inode->i_sb);
	version = get_inode_item_key_version(inode);

	if (!file_capable(inode, block)) {
		reiserfs_write_unlock_once(inode->i_sb, lock_depth);
		return -EFBIG;
	}

	if (!(create & GET_BLOCK_CREATE)) {
		int ret;
		
		ret = _get_block_create_0(inode, block, bh_result,
					  create | GET_BLOCK_READ_DIRECT);
		reiserfs_write_unlock_once(inode->i_sb, lock_depth);
		return ret;
	}
	if ((create & GET_BLOCK_NO_DANGLE) ||
	    reiserfs_transaction_running(inode->i_sb))
		dangle = 0;

	if ((have_large_tails(inode->i_sb)
	     && inode->i_size < i_block_size(inode) * 4)
	    || (have_small_tails(inode->i_sb)
		&& inode->i_size < i_block_size(inode)))
		REISERFS_I(inode)->i_flags |= i_pack_on_close_mask;

	
	make_cpu_key(&key, inode, new_offset, TYPE_ANY, 3  );
	if ((new_offset + inode->i_sb->s_blocksize - 1) > inode->i_size) {
	      start_trans:
		th = reiserfs_persistent_transaction(inode->i_sb, jbegin_count);
		if (!th) {
			retval = -ENOMEM;
			goto failure;
		}
		reiserfs_update_inode_transaction(inode);
	}
      research:

	retval = search_for_position_by_key(inode->i_sb, &key, &path);
	if (retval == IO_ERROR) {
		retval = -EIO;
		goto failure;
	}

	bh = get_last_bh(&path);
	ih = get_ih(&path);
	item = get_item(&path);
	pos_in_item = path.pos_in_item;

	fs_gen = get_generation(inode->i_sb);
	copy_item_head(&tmp_ih, ih);

	if (allocation_needed
	    (retval, allocated_block_nr, ih, item, pos_in_item)) {
		
		if (!th) {
			pathrelse(&path);
			goto start_trans;
		}

		repeat =
		    _allocate_block(th, block, inode, &allocated_block_nr,
				    &path, create);

		if (repeat == NO_DISK_SPACE || repeat == QUOTA_EXCEEDED) {
			SB_JOURNAL(inode->i_sb)->j_next_async_flush = 1;
			retval = restart_transaction(th, inode, &path);
			if (retval)
				goto failure;
			repeat =
			    _allocate_block(th, block, inode,
					    &allocated_block_nr, NULL, create);

			if (repeat != NO_DISK_SPACE && repeat != QUOTA_EXCEEDED) {
				goto research;
			}
			if (repeat == QUOTA_EXCEEDED)
				retval = -EDQUOT;
			else
				retval = -ENOSPC;
			goto failure;
		}

		if (fs_changed(fs_gen, inode->i_sb)
		    && item_moved(&tmp_ih, &path)) {
			goto research;
		}
	}

	if (indirect_item_found(retval, ih)) {
		b_blocknr_t unfm_ptr;
		unfm_ptr = get_block_num(item, pos_in_item);
		if (unfm_ptr == 0) {
			
			reiserfs_prepare_for_journal(inode->i_sb, bh, 1);
			if (fs_changed(fs_gen, inode->i_sb)
			    && item_moved(&tmp_ih, &path)) {
				reiserfs_restore_prepared_buffer(inode->i_sb,
								 bh);
				goto research;
			}
			set_buffer_new(bh_result);
			if (buffer_dirty(bh_result)
			    && reiserfs_data_ordered(inode->i_sb))
				reiserfs_add_ordered_list(inode, bh_result);
			put_block_num(item, pos_in_item, allocated_block_nr);
			unfm_ptr = allocated_block_nr;
			journal_mark_dirty(th, inode->i_sb, bh);
			reiserfs_update_sd(th, inode);
		}
		set_block_dev_mapped(bh_result, unfm_ptr, inode);
		pathrelse(&path);
		retval = 0;
		if (!dangle && th)
			retval = reiserfs_end_persistent_transaction(th);

		reiserfs_write_unlock_once(inode->i_sb, lock_depth);

		return retval;
	}

	if (!th) {
		pathrelse(&path);
		goto start_trans;
	}

	done = 0;
	do {
		if (is_statdata_le_ih(ih)) {
			__le32 unp = 0;
			struct cpu_key tmp_key;

			
			make_le_item_head(&tmp_ih, &key, version, 1,
					  TYPE_INDIRECT, UNFM_P_SIZE,
					  0  );

			if (cpu_key_k_offset(&key) == 1) {
				unp = cpu_to_le32(allocated_block_nr);
				set_block_dev_mapped(bh_result,
						     allocated_block_nr, inode);
				set_buffer_new(bh_result);
				done = 1;
			}
			tmp_key = key;	
			set_cpu_key_k_offset(&tmp_key, 1);
			PATH_LAST_POSITION(&path)++;

			retval =
			    reiserfs_insert_item(th, &path, &tmp_key, &tmp_ih,
						 inode, (char *)&unp);
			if (retval) {
				reiserfs_free_block(th, inode,
						    allocated_block_nr, 1);
				goto failure;	
			}
			
		} else if (is_direct_le_ih(ih)) {
			
			loff_t tail_offset;

			tail_offset =
			    ((le_ih_k_offset(ih) -
			      1) & ~(inode->i_sb->s_blocksize - 1)) + 1;
			if (tail_offset == cpu_key_k_offset(&key)) {
				set_block_dev_mapped(bh_result,
						     allocated_block_nr, inode);
				unbh = bh_result;
				done = 1;
			} else {

				pathrelse(&path);
				BUG_ON(!th->t_refcount);
				if (th->t_refcount == 1) {
					retval =
					    reiserfs_end_persistent_transaction
					    (th);
					th = NULL;
					if (retval)
						goto failure;
				}

				retval =
				    convert_tail_for_hole(inode, bh_result,
							  tail_offset);
				if (retval) {
					if (retval != -ENOSPC)
						reiserfs_error(inode->i_sb,
							"clm-6004",
							"convert tail failed "
							"inode %lu, error %d",
							inode->i_ino,
							retval);
					if (allocated_block_nr) {
						
						if (!th)
							th = reiserfs_persistent_transaction(inode->i_sb, 3);
						if (th)
							reiserfs_free_block(th,
									    inode,
									    allocated_block_nr,
									    1);
					}
					goto failure;
				}
				goto research;
			}
			retval =
			    direct2indirect(th, inode, &path, unbh,
					    tail_offset);
			if (retval) {
				reiserfs_unmap_buffer(unbh);
				reiserfs_free_block(th, inode,
						    allocated_block_nr, 1);
				goto failure;
			}
			set_buffer_uptodate(unbh);

			if (unbh->b_page) {
				reiserfs_add_tail_list(inode, unbh);

				mark_buffer_dirty(unbh);
			}
		} else {
			struct cpu_key tmp_key;
			unp_t unf_single = 0;	
			
			unp_t *un;
			__u64 max_to_insert =
			    MAX_ITEM_LEN(inode->i_sb->s_blocksize) /
			    UNFM_P_SIZE;
			__u64 blocks_needed;

			RFALSE(pos_in_item != ih_item_len(ih) / UNFM_P_SIZE,
			       "vs-804: invalid position for append");
			
			make_cpu_key(&tmp_key, inode,
				     le_key_k_offset(version,
						     &(ih->ih_key)) +
				     op_bytes_number(ih,
						     inode->i_sb->s_blocksize),
				     
				     TYPE_INDIRECT, 3);	

			RFALSE(cpu_key_k_offset(&tmp_key) > cpu_key_k_offset(&key),
			       "green-805: invalid offset");
			blocks_needed =
			    1 +
			    ((cpu_key_k_offset(&key) -
			      cpu_key_k_offset(&tmp_key)) >> inode->i_sb->
			     s_blocksize_bits);

			if (blocks_needed == 1) {
				un = &unf_single;
			} else {
				un = kzalloc(min(blocks_needed, max_to_insert) * UNFM_P_SIZE, GFP_NOFS);
				if (!un) {
					un = &unf_single;
					blocks_needed = 1;
					max_to_insert = 0;
				}
			}
			if (blocks_needed <= max_to_insert) {
				un[blocks_needed - 1] =
				    cpu_to_le32(allocated_block_nr);
				set_block_dev_mapped(bh_result,
						     allocated_block_nr, inode);
				set_buffer_new(bh_result);
				done = 1;
			} else {
				
				blocks_needed =
				    max_to_insert ? max_to_insert : 1;
			}
			retval =
			    reiserfs_paste_into_item(th, &path, &tmp_key, inode,
						     (char *)un,
						     UNFM_P_SIZE *
						     blocks_needed);

			if (blocks_needed != 1)
				kfree(un);

			if (retval) {
				reiserfs_free_block(th, inode,
						    allocated_block_nr, 1);
				goto failure;
			}
			if (!done) {
				inode->i_size +=
				    inode->i_sb->s_blocksize * blocks_needed;
			}
		}

		if (done == 1)
			break;

		if (journal_transaction_should_end(th, th->t_blocks_allocated)) {
			retval = restart_transaction(th, inode, &path);
			if (retval)
				goto failure;
		}
		if (need_resched()) {
			reiserfs_write_unlock_once(inode->i_sb, lock_depth);
			schedule();
			lock_depth = reiserfs_write_lock_once(inode->i_sb);
		}

		retval = search_for_position_by_key(inode->i_sb, &key, &path);
		if (retval == IO_ERROR) {
			retval = -EIO;
			goto failure;
		}
		if (retval == POSITION_FOUND) {
			reiserfs_warning(inode->i_sb, "vs-825",
					 "%K should not be found", &key);
			retval = -EEXIST;
			if (allocated_block_nr)
				reiserfs_free_block(th, inode,
						    allocated_block_nr, 1);
			pathrelse(&path);
			goto failure;
		}
		bh = get_last_bh(&path);
		ih = get_ih(&path);
		item = get_item(&path);
		pos_in_item = path.pos_in_item;
	} while (1);

	retval = 0;

      failure:
	if (th && (!dangle || (retval && !th->t_trans_id))) {
		int err;
		if (th->t_trans_id)
			reiserfs_update_sd(th, inode);
		err = reiserfs_end_persistent_transaction(th);
		if (err)
			retval = err;
	}

	reiserfs_write_unlock_once(inode->i_sb, lock_depth);
	reiserfs_check_path(&path);
	return retval;
}

static int
reiserfs_readpages(struct file *file, struct address_space *mapping,
		   struct list_head *pages, unsigned nr_pages)
{
	return mpage_readpages(mapping, pages, nr_pages, reiserfs_get_block);
}

static int real_space_diff(struct inode *inode, int sd_size)
{
	int bytes;
	loff_t blocksize = inode->i_sb->s_blocksize;

	if (S_ISLNK(inode->i_mode) || S_ISDIR(inode->i_mode))
		return sd_size;

	bytes =
	    ((inode->i_size +
	      (blocksize - 1)) >> inode->i_sb->s_blocksize_bits) * UNFM_P_SIZE +
	    sd_size;
	return bytes;
}

static inline loff_t to_real_used_space(struct inode *inode, ulong blocks,
					int sd_size)
{
	if (S_ISLNK(inode->i_mode) || S_ISDIR(inode->i_mode)) {
		return inode->i_size +
		    (loff_t) (real_space_diff(inode, sd_size));
	}
	return ((loff_t) real_space_diff(inode, sd_size)) +
	    (((loff_t) blocks) << 9);
}

static inline ulong to_fake_used_blocks(struct inode *inode, int sd_size)
{
	loff_t bytes = inode_get_bytes(inode);
	loff_t real_space = real_space_diff(inode, sd_size);

	
	if (S_ISLNK(inode->i_mode) || S_ISDIR(inode->i_mode)) {
		bytes += (loff_t) 511;
	}

	if (bytes < real_space)
		return 0;
	return (bytes - real_space) >> 9;
}


static void init_inode(struct inode *inode, struct treepath *path)
{
	struct buffer_head *bh;
	struct item_head *ih;
	__u32 rdev;
	

	bh = PATH_PLAST_BUFFER(path);
	ih = PATH_PITEM_HEAD(path);

	copy_key(INODE_PKEY(inode), &(ih->ih_key));

	INIT_LIST_HEAD(&(REISERFS_I(inode)->i_prealloc_list));
	REISERFS_I(inode)->i_flags = 0;
	REISERFS_I(inode)->i_prealloc_block = 0;
	REISERFS_I(inode)->i_prealloc_count = 0;
	REISERFS_I(inode)->i_trans_id = 0;
	REISERFS_I(inode)->i_jl = NULL;
	reiserfs_init_xattr_rwsem(inode);

	if (stat_data_v1(ih)) {
		struct stat_data_v1 *sd =
		    (struct stat_data_v1 *)B_I_PITEM(bh, ih);
		unsigned long blocks;

		set_inode_item_key_version(inode, KEY_FORMAT_3_5);
		set_inode_sd_version(inode, STAT_DATA_V1);
		inode->i_mode = sd_v1_mode(sd);
		set_nlink(inode, sd_v1_nlink(sd));
		inode->i_uid = sd_v1_uid(sd);
		inode->i_gid = sd_v1_gid(sd);
		inode->i_size = sd_v1_size(sd);
		inode->i_atime.tv_sec = sd_v1_atime(sd);
		inode->i_mtime.tv_sec = sd_v1_mtime(sd);
		inode->i_ctime.tv_sec = sd_v1_ctime(sd);
		inode->i_atime.tv_nsec = 0;
		inode->i_ctime.tv_nsec = 0;
		inode->i_mtime.tv_nsec = 0;

		inode->i_blocks = sd_v1_blocks(sd);
		inode->i_generation = le32_to_cpu(INODE_PKEY(inode)->k_dir_id);
		blocks = (inode->i_size + 511) >> 9;
		blocks = _ROUND_UP(blocks, inode->i_sb->s_blocksize >> 9);
		if (inode->i_blocks > blocks) {
			
			
			
			
			
			inode->i_blocks = blocks;
		}

		rdev = sd_v1_rdev(sd);
		REISERFS_I(inode)->i_first_direct_byte =
		    sd_v1_first_direct_byte(sd);
		if (inode->i_blocks & 1) {
			inode->i_blocks++;
		}
		inode_set_bytes(inode,
				to_real_used_space(inode, inode->i_blocks,
						   SD_V1_SIZE));
		REISERFS_I(inode)->i_flags &= ~i_nopack_mask;
	} else {
		
		
		struct stat_data *sd = (struct stat_data *)B_I_PITEM(bh, ih);

		inode->i_mode = sd_v2_mode(sd);
		set_nlink(inode, sd_v2_nlink(sd));
		inode->i_uid = sd_v2_uid(sd);
		inode->i_size = sd_v2_size(sd);
		inode->i_gid = sd_v2_gid(sd);
		inode->i_mtime.tv_sec = sd_v2_mtime(sd);
		inode->i_atime.tv_sec = sd_v2_atime(sd);
		inode->i_ctime.tv_sec = sd_v2_ctime(sd);
		inode->i_ctime.tv_nsec = 0;
		inode->i_mtime.tv_nsec = 0;
		inode->i_atime.tv_nsec = 0;
		inode->i_blocks = sd_v2_blocks(sd);
		rdev = sd_v2_rdev(sd);
		if (S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode))
			inode->i_generation =
			    le32_to_cpu(INODE_PKEY(inode)->k_dir_id);
		else
			inode->i_generation = sd_v2_generation(sd);

		if (S_ISDIR(inode->i_mode) || S_ISLNK(inode->i_mode))
			set_inode_item_key_version(inode, KEY_FORMAT_3_5);
		else
			set_inode_item_key_version(inode, KEY_FORMAT_3_6);
		REISERFS_I(inode)->i_first_direct_byte = 0;
		set_inode_sd_version(inode, STAT_DATA_V2);
		inode_set_bytes(inode,
				to_real_used_space(inode, inode->i_blocks,
						   SD_V2_SIZE));
		REISERFS_I(inode)->i_attrs = sd_v2_attrs(sd);
		sd_attrs_to_i_attrs(sd_v2_attrs(sd), inode);
	}

	pathrelse(path);
	if (S_ISREG(inode->i_mode)) {
		inode->i_op = &reiserfs_file_inode_operations;
		inode->i_fop = &reiserfs_file_operations;
		inode->i_mapping->a_ops = &reiserfs_address_space_operations;
	} else if (S_ISDIR(inode->i_mode)) {
		inode->i_op = &reiserfs_dir_inode_operations;
		inode->i_fop = &reiserfs_dir_operations;
	} else if (S_ISLNK(inode->i_mode)) {
		inode->i_op = &reiserfs_symlink_inode_operations;
		inode->i_mapping->a_ops = &reiserfs_address_space_operations;
	} else {
		inode->i_blocks = 0;
		inode->i_op = &reiserfs_special_inode_operations;
		init_special_inode(inode, inode->i_mode, new_decode_dev(rdev));
	}
}

static void inode2sd(void *sd, struct inode *inode, loff_t size)
{
	struct stat_data *sd_v2 = (struct stat_data *)sd;
	__u16 flags;

	set_sd_v2_mode(sd_v2, inode->i_mode);
	set_sd_v2_nlink(sd_v2, inode->i_nlink);
	set_sd_v2_uid(sd_v2, inode->i_uid);
	set_sd_v2_size(sd_v2, size);
	set_sd_v2_gid(sd_v2, inode->i_gid);
	set_sd_v2_mtime(sd_v2, inode->i_mtime.tv_sec);
	set_sd_v2_atime(sd_v2, inode->i_atime.tv_sec);
	set_sd_v2_ctime(sd_v2, inode->i_ctime.tv_sec);
	set_sd_v2_blocks(sd_v2, to_fake_used_blocks(inode, SD_V2_SIZE));
	if (S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode))
		set_sd_v2_rdev(sd_v2, new_encode_dev(inode->i_rdev));
	else
		set_sd_v2_generation(sd_v2, inode->i_generation);
	flags = REISERFS_I(inode)->i_attrs;
	i_attrs_to_sd_attrs(inode, &flags);
	set_sd_v2_attrs(sd_v2, flags);
}

static void inode2sd_v1(void *sd, struct inode *inode, loff_t size)
{
	struct stat_data_v1 *sd_v1 = (struct stat_data_v1 *)sd;

	set_sd_v1_mode(sd_v1, inode->i_mode);
	set_sd_v1_uid(sd_v1, inode->i_uid);
	set_sd_v1_gid(sd_v1, inode->i_gid);
	set_sd_v1_nlink(sd_v1, inode->i_nlink);
	set_sd_v1_size(sd_v1, size);
	set_sd_v1_atime(sd_v1, inode->i_atime.tv_sec);
	set_sd_v1_ctime(sd_v1, inode->i_ctime.tv_sec);
	set_sd_v1_mtime(sd_v1, inode->i_mtime.tv_sec);

	if (S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode))
		set_sd_v1_rdev(sd_v1, new_encode_dev(inode->i_rdev));
	else
		set_sd_v1_blocks(sd_v1, to_fake_used_blocks(inode, SD_V1_SIZE));

	
	set_sd_v1_first_direct_byte(sd_v1,
				    REISERFS_I(inode)->i_first_direct_byte);
}

static void update_stat_data(struct treepath *path, struct inode *inode,
			     loff_t size)
{
	struct buffer_head *bh;
	struct item_head *ih;

	bh = PATH_PLAST_BUFFER(path);
	ih = PATH_PITEM_HEAD(path);

	if (!is_statdata_le_ih(ih))
		reiserfs_panic(inode->i_sb, "vs-13065", "key %k, found item %h",
			       INODE_PKEY(inode), ih);

	if (stat_data_v1(ih)) {
		
		inode2sd_v1(B_I_PITEM(bh, ih), inode, size);
	} else {
		inode2sd(B_I_PITEM(bh, ih), inode, size);
	}

	return;
}

void reiserfs_update_sd_size(struct reiserfs_transaction_handle *th,
			     struct inode *inode, loff_t size)
{
	struct cpu_key key;
	INITIALIZE_PATH(path);
	struct buffer_head *bh;
	int fs_gen;
	struct item_head *ih, tmp_ih;
	int retval;

	BUG_ON(!th->t_trans_id);

	make_cpu_key(&key, inode, SD_OFFSET, TYPE_STAT_DATA, 3);	

	for (;;) {
		int pos;
		
		retval = search_item(inode->i_sb, &key, &path);
		if (retval == IO_ERROR) {
			reiserfs_error(inode->i_sb, "vs-13050",
				       "i/o failure occurred trying to "
				       "update %K stat data", &key);
			return;
		}
		if (retval == ITEM_NOT_FOUND) {
			pos = PATH_LAST_POSITION(&path);
			pathrelse(&path);
			if (inode->i_nlink == 0) {
				
				return;
			}
			reiserfs_warning(inode->i_sb, "vs-13060",
					 "stat data of object %k (nlink == %d) "
					 "not found (pos %d)",
					 INODE_PKEY(inode), inode->i_nlink,
					 pos);
			reiserfs_check_path(&path);
			return;
		}

		bh = get_last_bh(&path);
		ih = get_ih(&path);
		copy_item_head(&tmp_ih, ih);
		fs_gen = get_generation(inode->i_sb);
		reiserfs_prepare_for_journal(inode->i_sb, bh, 1);
		if (fs_changed(fs_gen, inode->i_sb)
		    && item_moved(&tmp_ih, &path)) {
			reiserfs_restore_prepared_buffer(inode->i_sb, bh);
			continue;	
		}
		break;
	}
	update_stat_data(&path, inode, size);
	journal_mark_dirty(th, th->t_super, bh);
	pathrelse(&path);
	return;
}

static void reiserfs_make_bad_inode(struct inode *inode)
{
	memset(INODE_PKEY(inode), 0, KEY_SIZE);
	make_bad_inode(inode);
}


int reiserfs_init_locked_inode(struct inode *inode, void *p)
{
	struct reiserfs_iget_args *args = (struct reiserfs_iget_args *)p;
	inode->i_ino = args->objectid;
	INODE_PKEY(inode)->k_dir_id = cpu_to_le32(args->dirid);
	return 0;
}

void reiserfs_read_locked_inode(struct inode *inode,
				struct reiserfs_iget_args *args)
{
	INITIALIZE_PATH(path_to_sd);
	struct cpu_key key;
	unsigned long dirino;
	int retval;

	dirino = args->dirid;

	key.version = KEY_FORMAT_3_5;
	key.on_disk_key.k_dir_id = dirino;
	key.on_disk_key.k_objectid = inode->i_ino;
	key.on_disk_key.k_offset = 0;
	key.on_disk_key.k_type = 0;

	
	retval = search_item(inode->i_sb, &key, &path_to_sd);
	if (retval == IO_ERROR) {
		reiserfs_error(inode->i_sb, "vs-13070",
			       "i/o failure occurred trying to find "
			       "stat data of %K", &key);
		reiserfs_make_bad_inode(inode);
		return;
	}
	if (retval != ITEM_FOUND) {
		
		pathrelse(&path_to_sd);
		reiserfs_make_bad_inode(inode);
		clear_nlink(inode);
		return;
	}

	init_inode(inode, &path_to_sd);

	if ((inode->i_nlink == 0) &&
	    !REISERFS_SB(inode->i_sb)->s_is_unlinked_ok) {
		reiserfs_warning(inode->i_sb, "vs-13075",
				 "dead inode read from disk %K. "
				 "This is likely to be race with knfsd. Ignore",
				 &key);
		reiserfs_make_bad_inode(inode);
	}

	reiserfs_check_path(&path_to_sd);	

	if (get_inode_sd_version(inode) == STAT_DATA_V1)
		cache_no_acl(inode);
}

int reiserfs_find_actor(struct inode *inode, void *opaque)
{
	struct reiserfs_iget_args *args;

	args = opaque;
	
	return (inode->i_ino == args->objectid) &&
	    (le32_to_cpu(INODE_PKEY(inode)->k_dir_id) == args->dirid);
}

struct inode *reiserfs_iget(struct super_block *s, const struct cpu_key *key)
{
	struct inode *inode;
	struct reiserfs_iget_args args;

	args.objectid = key->on_disk_key.k_objectid;
	args.dirid = key->on_disk_key.k_dir_id;
	reiserfs_write_unlock(s);
	inode = iget5_locked(s, key->on_disk_key.k_objectid,
			     reiserfs_find_actor, reiserfs_init_locked_inode,
			     (void *)(&args));
	reiserfs_write_lock(s);
	if (!inode)
		return ERR_PTR(-ENOMEM);

	if (inode->i_state & I_NEW) {
		reiserfs_read_locked_inode(inode, &args);
		unlock_new_inode(inode);
	}

	if (comp_short_keys(INODE_PKEY(inode), key) || is_bad_inode(inode)) {
		
		iput(inode);
		inode = NULL;
	}
	return inode;
}

static struct dentry *reiserfs_get_dentry(struct super_block *sb,
	u32 objectid, u32 dir_id, u32 generation)

{
	struct cpu_key key;
	struct inode *inode;

	key.on_disk_key.k_objectid = objectid;
	key.on_disk_key.k_dir_id = dir_id;
	reiserfs_write_lock(sb);
	inode = reiserfs_iget(sb, &key);
	if (inode && !IS_ERR(inode) && generation != 0 &&
	    generation != inode->i_generation) {
		iput(inode);
		inode = NULL;
	}
	reiserfs_write_unlock(sb);

	return d_obtain_alias(inode);
}

struct dentry *reiserfs_fh_to_dentry(struct super_block *sb, struct fid *fid,
		int fh_len, int fh_type)
{
	if (fh_type > fh_len) {
		if (fh_type != 6 || fh_len != 5)
			reiserfs_warning(sb, "reiserfs-13077",
				"nfsd/reiserfs, fhtype=%d, len=%d - odd",
				fh_type, fh_len);
		fh_type = 5;
	}

	return reiserfs_get_dentry(sb, fid->raw[0], fid->raw[1],
		(fh_type == 3 || fh_type >= 5) ? fid->raw[2] : 0);
}

struct dentry *reiserfs_fh_to_parent(struct super_block *sb, struct fid *fid,
		int fh_len, int fh_type)
{
	if (fh_type < 4)
		return NULL;

	return reiserfs_get_dentry(sb,
		(fh_type >= 5) ? fid->raw[3] : fid->raw[2],
		(fh_type >= 5) ? fid->raw[4] : fid->raw[3],
		(fh_type == 6) ? fid->raw[5] : 0);
}

int reiserfs_encode_fh(struct dentry *dentry, __u32 * data, int *lenp,
		       int need_parent)
{
	struct inode *inode = dentry->d_inode;
	int maxlen = *lenp;

	if (need_parent && (maxlen < 5)) {
		*lenp = 5;
		return 255;
	} else if (maxlen < 3) {
		*lenp = 3;
		return 255;
	}

	data[0] = inode->i_ino;
	data[1] = le32_to_cpu(INODE_PKEY(inode)->k_dir_id);
	data[2] = inode->i_generation;
	*lenp = 3;
	
	if (maxlen < 5 || !need_parent)
		return 3;

	spin_lock(&dentry->d_lock);
	inode = dentry->d_parent->d_inode;
	data[3] = inode->i_ino;
	data[4] = le32_to_cpu(INODE_PKEY(inode)->k_dir_id);
	*lenp = 5;
	if (maxlen >= 6) {
		data[5] = inode->i_generation;
		*lenp = 6;
	}
	spin_unlock(&dentry->d_lock);
	return *lenp;
}

int reiserfs_write_inode(struct inode *inode, struct writeback_control *wbc)
{
	struct reiserfs_transaction_handle th;
	int jbegin_count = 1;

	if (inode->i_sb->s_flags & MS_RDONLY)
		return -EROFS;
	if (wbc->sync_mode == WB_SYNC_ALL && !(current->flags & PF_MEMALLOC)) {
		reiserfs_write_lock(inode->i_sb);
		if (!journal_begin(&th, inode->i_sb, jbegin_count)) {
			reiserfs_update_sd(&th, inode);
			journal_end_sync(&th, inode->i_sb, jbegin_count);
		}
		reiserfs_write_unlock(inode->i_sb);
	}
	return 0;
}

static int reiserfs_new_directory(struct reiserfs_transaction_handle *th,
				  struct inode *inode,
				  struct item_head *ih, struct treepath *path,
				  struct inode *dir)
{
	struct super_block *sb = th->t_super;
	char empty_dir[EMPTY_DIR_SIZE];
	char *body = empty_dir;
	struct cpu_key key;
	int retval;

	BUG_ON(!th->t_trans_id);

	_make_cpu_key(&key, KEY_FORMAT_3_5, le32_to_cpu(ih->ih_key.k_dir_id),
		      le32_to_cpu(ih->ih_key.k_objectid), DOT_OFFSET,
		      TYPE_DIRENTRY, 3  );

	if (old_format_only(sb)) {
		make_le_item_head(ih, NULL, KEY_FORMAT_3_5, DOT_OFFSET,
				  TYPE_DIRENTRY, EMPTY_DIR_SIZE_V1, 2);

		make_empty_dir_item_v1(body, ih->ih_key.k_dir_id,
				       ih->ih_key.k_objectid,
				       INODE_PKEY(dir)->k_dir_id,
				       INODE_PKEY(dir)->k_objectid);
	} else {
		make_le_item_head(ih, NULL, KEY_FORMAT_3_5, DOT_OFFSET,
				  TYPE_DIRENTRY, EMPTY_DIR_SIZE, 2);

		make_empty_dir_item(body, ih->ih_key.k_dir_id,
				    ih->ih_key.k_objectid,
				    INODE_PKEY(dir)->k_dir_id,
				    INODE_PKEY(dir)->k_objectid);
	}

	
	retval = search_item(sb, &key, path);
	if (retval == IO_ERROR) {
		reiserfs_error(sb, "vs-13080",
			       "i/o failure occurred creating new directory");
		return -EIO;
	}
	if (retval == ITEM_FOUND) {
		pathrelse(path);
		reiserfs_warning(sb, "vs-13070",
				 "object with this key exists (%k)",
				 &(ih->ih_key));
		return -EEXIST;
	}

	
	return reiserfs_insert_item(th, path, &key, ih, inode, body);
}

static int reiserfs_new_symlink(struct reiserfs_transaction_handle *th, struct inode *inode,	
				struct item_head *ih,
				struct treepath *path, const char *symname,
				int item_len)
{
	struct super_block *sb = th->t_super;
	struct cpu_key key;
	int retval;

	BUG_ON(!th->t_trans_id);

	_make_cpu_key(&key, KEY_FORMAT_3_5,
		      le32_to_cpu(ih->ih_key.k_dir_id),
		      le32_to_cpu(ih->ih_key.k_objectid),
		      1, TYPE_DIRECT, 3  );

	make_le_item_head(ih, NULL, KEY_FORMAT_3_5, 1, TYPE_DIRECT, item_len,
			  0  );

	
	retval = search_item(sb, &key, path);
	if (retval == IO_ERROR) {
		reiserfs_error(sb, "vs-13080",
			       "i/o failure occurred creating new symlink");
		return -EIO;
	}
	if (retval == ITEM_FOUND) {
		pathrelse(path);
		reiserfs_warning(sb, "vs-13080",
				 "object with this key exists (%k)",
				 &(ih->ih_key));
		return -EEXIST;
	}

	
	return reiserfs_insert_item(th, path, &key, ih, inode, symname);
}

int reiserfs_new_inode(struct reiserfs_transaction_handle *th,
		       struct inode *dir, umode_t mode, const char *symname,
		       loff_t i_size, struct dentry *dentry,
		       struct inode *inode,
		       struct reiserfs_security_handle *security)
{
	struct super_block *sb;
	struct reiserfs_iget_args args;
	INITIALIZE_PATH(path_to_key);
	struct cpu_key key;
	struct item_head ih;
	struct stat_data sd;
	int retval;
	int err;

	BUG_ON(!th->t_trans_id);

	dquot_initialize(inode);
	err = dquot_alloc_inode(inode);
	if (err)
		goto out_end_trans;
	if (!dir->i_nlink) {
		err = -EPERM;
		goto out_bad_inode;
	}

	sb = dir->i_sb;

	
	ih.ih_key.k_dir_id = reiserfs_choose_packing(dir);
	ih.ih_key.k_objectid = cpu_to_le32(reiserfs_get_unused_objectid(th));
	if (!ih.ih_key.k_objectid) {
		err = -ENOMEM;
		goto out_bad_inode;
	}
	args.objectid = inode->i_ino = le32_to_cpu(ih.ih_key.k_objectid);
	if (old_format_only(sb))
		make_le_item_head(&ih, NULL, KEY_FORMAT_3_5, SD_OFFSET,
				  TYPE_STAT_DATA, SD_V1_SIZE, MAX_US_INT);
	else
		make_le_item_head(&ih, NULL, KEY_FORMAT_3_6, SD_OFFSET,
				  TYPE_STAT_DATA, SD_SIZE, MAX_US_INT);
	memcpy(INODE_PKEY(inode), &(ih.ih_key), KEY_SIZE);
	args.dirid = le32_to_cpu(ih.ih_key.k_dir_id);
	if (insert_inode_locked4(inode, args.objectid,
			     reiserfs_find_actor, &args) < 0) {
		err = -EINVAL;
		goto out_bad_inode;
	}
	if (old_format_only(sb))
		inode->i_generation = le32_to_cpu(INODE_PKEY(dir)->k_objectid);
	else
#if defined( USE_INODE_GENERATION_COUNTER )
		inode->i_generation =
		    le32_to_cpu(REISERFS_SB(sb)->s_rs->s_inode_generation);
#else
		inode->i_generation = ++event;
#endif

	
	set_nlink(inode, (S_ISDIR(mode) ? 2 : 1));

	

	
	if (S_ISLNK(inode->i_mode))
		inode->i_flags &= ~(S_IMMUTABLE | S_APPEND);

	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME_SEC;
	inode->i_size = i_size;
	inode->i_blocks = 0;
	inode->i_bytes = 0;
	REISERFS_I(inode)->i_first_direct_byte = S_ISLNK(mode) ? 1 :
	    U32_MAX  ;

	INIT_LIST_HEAD(&(REISERFS_I(inode)->i_prealloc_list));
	REISERFS_I(inode)->i_flags = 0;
	REISERFS_I(inode)->i_prealloc_block = 0;
	REISERFS_I(inode)->i_prealloc_count = 0;
	REISERFS_I(inode)->i_trans_id = 0;
	REISERFS_I(inode)->i_jl = NULL;
	REISERFS_I(inode)->i_attrs =
	    REISERFS_I(dir)->i_attrs & REISERFS_INHERIT_MASK;
	sd_attrs_to_i_attrs(REISERFS_I(inode)->i_attrs, inode);
	reiserfs_init_xattr_rwsem(inode);

	
	_make_cpu_key(&key, KEY_FORMAT_3_6, le32_to_cpu(ih.ih_key.k_dir_id),
		      le32_to_cpu(ih.ih_key.k_objectid), SD_OFFSET,
		      TYPE_STAT_DATA, 3  );

	
	retval = search_item(sb, &key, &path_to_key);
	if (retval == IO_ERROR) {
		err = -EIO;
		goto out_bad_inode;
	}
	if (retval == ITEM_FOUND) {
		pathrelse(&path_to_key);
		err = -EEXIST;
		goto out_bad_inode;
	}
	if (old_format_only(sb)) {
		if (inode->i_uid & ~0xffff || inode->i_gid & ~0xffff) {
			pathrelse(&path_to_key);
			
			err = -EINVAL;
			goto out_bad_inode;
		}
		inode2sd_v1(&sd, inode, inode->i_size);
	} else {
		inode2sd(&sd, inode, inode->i_size);
	}
	
	
	
	if (old_format_only(sb) || S_ISDIR(mode) || S_ISLNK(mode))
		set_inode_item_key_version(inode, KEY_FORMAT_3_5);
	else
		set_inode_item_key_version(inode, KEY_FORMAT_3_6);
	if (old_format_only(sb))
		set_inode_sd_version(inode, STAT_DATA_V1);
	else
		set_inode_sd_version(inode, STAT_DATA_V2);

	
#ifdef DISPLACE_NEW_PACKING_LOCALITIES
	if (REISERFS_I(dir)->new_packing_locality)
		th->displace_new_blocks = 1;
#endif
	retval =
	    reiserfs_insert_item(th, &path_to_key, &key, &ih, inode,
				 (char *)(&sd));
	if (retval) {
		err = retval;
		reiserfs_check_path(&path_to_key);
		goto out_bad_inode;
	}
#ifdef DISPLACE_NEW_PACKING_LOCALITIES
	if (!th->displace_new_blocks)
		REISERFS_I(dir)->new_packing_locality = 0;
#endif
	if (S_ISDIR(mode)) {
		
		retval =
		    reiserfs_new_directory(th, inode, &ih, &path_to_key, dir);
	}

	if (S_ISLNK(mode)) {
		
		if (!old_format_only(sb))
			i_size = ROUND_UP(i_size);
		retval =
		    reiserfs_new_symlink(th, inode, &ih, &path_to_key, symname,
					 i_size);
	}
	if (retval) {
		err = retval;
		reiserfs_check_path(&path_to_key);
		journal_end(th, th->t_super, th->t_blocks_allocated);
		goto out_inserted_sd;
	}

	if (reiserfs_posixacl(inode->i_sb)) {
		retval = reiserfs_inherit_default_acl(th, dir, dentry, inode);
		if (retval) {
			err = retval;
			reiserfs_check_path(&path_to_key);
			journal_end(th, th->t_super, th->t_blocks_allocated);
			goto out_inserted_sd;
		}
	} else if (inode->i_sb->s_flags & MS_POSIXACL) {
		reiserfs_warning(inode->i_sb, "jdm-13090",
				 "ACLs aren't enabled in the fs, "
				 "but vfs thinks they are!");
	} else if (IS_PRIVATE(dir))
		inode->i_flags |= S_PRIVATE;

	if (security->name) {
		retval = reiserfs_security_write(th, inode, security);
		if (retval) {
			err = retval;
			reiserfs_check_path(&path_to_key);
			retval = journal_end(th, th->t_super,
					     th->t_blocks_allocated);
			if (retval)
				err = retval;
			goto out_inserted_sd;
		}
	}

	reiserfs_update_sd(th, inode);
	reiserfs_check_path(&path_to_key);

	return 0;

      out_bad_inode:
	
	INODE_PKEY(inode)->k_objectid = 0;

	
	dquot_free_inode(inode);

      out_end_trans:
	journal_end(th, th->t_super, th->t_blocks_allocated);
	
	dquot_drop(inode);
	inode->i_flags |= S_NOQUOTA;
	make_bad_inode(inode);

      out_inserted_sd:
	clear_nlink(inode);
	th->t_trans_id = 0;	
	unlock_new_inode(inode); 
	iput(inode);
	return err;
}

static int grab_tail_page(struct inode *inode,
			  struct page **page_result,
			  struct buffer_head **bh_result)
{

	unsigned long index = (inode->i_size - 1) >> PAGE_CACHE_SHIFT;
	unsigned long pos = 0;
	unsigned long start = 0;
	unsigned long blocksize = inode->i_sb->s_blocksize;
	unsigned long offset = (inode->i_size) & (PAGE_CACHE_SIZE - 1);
	struct buffer_head *bh;
	struct buffer_head *head;
	struct page *page;
	int error;

	if ((offset & (blocksize - 1)) == 0) {
		return -ENOENT;
	}
	page = grab_cache_page(inode->i_mapping, index);
	error = -ENOMEM;
	if (!page) {
		goto out;
	}
	
	start = (offset / blocksize) * blocksize;

	error = __block_write_begin(page, start, offset - start,
				    reiserfs_get_block_create_0);
	if (error)
		goto unlock;

	head = page_buffers(page);
	bh = head;
	do {
		if (pos >= start) {
			break;
		}
		bh = bh->b_this_page;
		pos += blocksize;
	} while (bh != head);

	if (!buffer_uptodate(bh)) {
		reiserfs_error(inode->i_sb, "clm-6000",
			       "error reading block %lu", bh->b_blocknr);
		error = -EIO;
		goto unlock;
	}
	*bh_result = bh;
	*page_result = page;

      out:
	return error;

      unlock:
	unlock_page(page);
	page_cache_release(page);
	return error;
}

int reiserfs_truncate_file(struct inode *inode, int update_timestamps)
{
	struct reiserfs_transaction_handle th;
	
	unsigned long offset = inode->i_size & (PAGE_CACHE_SIZE - 1);
	unsigned blocksize = inode->i_sb->s_blocksize;
	unsigned length;
	struct page *page = NULL;
	int error;
	struct buffer_head *bh = NULL;
	int err2;
	int lock_depth;

	lock_depth = reiserfs_write_lock_once(inode->i_sb);

	if (inode->i_size > 0) {
		error = grab_tail_page(inode, &page, &bh);
		if (error) {
			
			
			
			if (error != -ENOENT)
				reiserfs_error(inode->i_sb, "clm-6001",
					       "grab_tail_page failed %d",
					       error);
			page = NULL;
			bh = NULL;
		}
	}

	error = journal_begin(&th, inode->i_sb,
			      JOURNAL_PER_BALANCE_CNT * 2 + 1);
	if (error)
		goto out;
	reiserfs_update_inode_transaction(inode);
	if (update_timestamps)
		add_save_link(&th, inode, 1);
	err2 = reiserfs_do_truncate(&th, inode, page, update_timestamps);
	error =
	    journal_end(&th, inode->i_sb, JOURNAL_PER_BALANCE_CNT * 2 + 1);
	if (error)
		goto out;

	
	if (err2) {
		error = err2;
  		goto out;
	}
	
	if (update_timestamps) {
		error = remove_save_link(inode, 1 );
		if (error)
			goto out;
	}

	if (page) {
		length = offset & (blocksize - 1);
		
		if (length) {
			length = blocksize - length;
			zero_user(page, offset, length);
			if (buffer_mapped(bh) && bh->b_blocknr != 0) {
				mark_buffer_dirty(bh);
			}
		}
		unlock_page(page);
		page_cache_release(page);
	}

	reiserfs_write_unlock_once(inode->i_sb, lock_depth);

	return 0;
      out:
	if (page) {
		unlock_page(page);
		page_cache_release(page);
	}

	reiserfs_write_unlock_once(inode->i_sb, lock_depth);

	return error;
}

static int map_block_for_writepage(struct inode *inode,
				   struct buffer_head *bh_result,
				   unsigned long block)
{
	struct reiserfs_transaction_handle th;
	int fs_gen;
	struct item_head tmp_ih;
	struct item_head *ih;
	struct buffer_head *bh;
	__le32 *item;
	struct cpu_key key;
	INITIALIZE_PATH(path);
	int pos_in_item;
	int jbegin_count = JOURNAL_PER_BALANCE_CNT;
	loff_t byte_offset = ((loff_t)block << inode->i_sb->s_blocksize_bits)+1;
	int retval;
	int use_get_block = 0;
	int bytes_copied = 0;
	int copy_size;
	int trans_running = 0;

	
	th.t_trans_id = 0;

	if (!buffer_uptodate(bh_result)) {
		return -EIO;
	}

	kmap(bh_result->b_page);
      start_over:
	reiserfs_write_lock(inode->i_sb);
	make_cpu_key(&key, inode, byte_offset, TYPE_ANY, 3);

      research:
	retval = search_for_position_by_key(inode->i_sb, &key, &path);
	if (retval != POSITION_FOUND) {
		use_get_block = 1;
		goto out;
	}

	bh = get_last_bh(&path);
	ih = get_ih(&path);
	item = get_item(&path);
	pos_in_item = path.pos_in_item;

	
	if (indirect_item_found(retval, ih)) {
		if (bytes_copied > 0) {
			reiserfs_warning(inode->i_sb, "clm-6002",
					 "bytes_copied %d", bytes_copied);
		}
		if (!get_block_num(item, pos_in_item)) {
			
			use_get_block = 1;
			goto out;
		}
		set_block_dev_mapped(bh_result,
				     get_block_num(item, pos_in_item), inode);
	} else if (is_direct_le_ih(ih)) {
		char *p;
		p = page_address(bh_result->b_page);
		p += (byte_offset - 1) & (PAGE_CACHE_SIZE - 1);
		copy_size = ih_item_len(ih) - pos_in_item;

		fs_gen = get_generation(inode->i_sb);
		copy_item_head(&tmp_ih, ih);

		if (!trans_running) {
			
			retval = journal_begin(&th, inode->i_sb, jbegin_count);
			if (retval)
				goto out;
			reiserfs_update_inode_transaction(inode);
			trans_running = 1;
			if (fs_changed(fs_gen, inode->i_sb)
			    && item_moved(&tmp_ih, &path)) {
				reiserfs_restore_prepared_buffer(inode->i_sb,
								 bh);
				goto research;
			}
		}

		reiserfs_prepare_for_journal(inode->i_sb, bh, 1);

		if (fs_changed(fs_gen, inode->i_sb)
		    && item_moved(&tmp_ih, &path)) {
			reiserfs_restore_prepared_buffer(inode->i_sb, bh);
			goto research;
		}

		memcpy(B_I_PITEM(bh, ih) + pos_in_item, p + bytes_copied,
		       copy_size);

		journal_mark_dirty(&th, inode->i_sb, bh);
		bytes_copied += copy_size;
		set_block_dev_mapped(bh_result, 0, inode);

		
		if (bytes_copied < bh_result->b_size &&
		    (byte_offset + bytes_copied) < inode->i_size) {
			set_cpu_key_k_offset(&key,
					     cpu_key_k_offset(&key) +
					     copy_size);
			goto research;
		}
	} else {
		reiserfs_warning(inode->i_sb, "clm-6003",
				 "bad item inode %lu", inode->i_ino);
		retval = -EIO;
		goto out;
	}
	retval = 0;

      out:
	pathrelse(&path);
	if (trans_running) {
		int err = journal_end(&th, inode->i_sb, jbegin_count);
		if (err)
			retval = err;
		trans_running = 0;
	}
	reiserfs_write_unlock(inode->i_sb);

	
	if (use_get_block) {
		retval = reiserfs_get_block(inode, block, bh_result,
					    GET_BLOCK_CREATE | GET_BLOCK_NO_IMUX
					    | GET_BLOCK_NO_DANGLE);
		if (!retval) {
			if (!buffer_mapped(bh_result)
			    || bh_result->b_blocknr == 0) {
				
				use_get_block = 0;
				goto start_over;
			}
		}
	}
	kunmap(bh_result->b_page);

	if (!retval && buffer_mapped(bh_result) && bh_result->b_blocknr == 0) {
		lock_buffer(bh_result);
		clear_buffer_dirty(bh_result);
		unlock_buffer(bh_result);
	}
	return retval;
}

static int reiserfs_write_full_page(struct page *page,
				    struct writeback_control *wbc)
{
	struct inode *inode = page->mapping->host;
	unsigned long end_index = inode->i_size >> PAGE_CACHE_SHIFT;
	int error = 0;
	unsigned long block;
	sector_t last_block;
	struct buffer_head *head, *bh;
	int partial = 0;
	int nr = 0;
	int checked = PageChecked(page);
	struct reiserfs_transaction_handle th;
	struct super_block *s = inode->i_sb;
	int bh_per_page = PAGE_CACHE_SIZE / s->s_blocksize;
	th.t_trans_id = 0;

	
	if (checked && (current->flags & PF_MEMALLOC)) {
		redirty_page_for_writepage(wbc, page);
		unlock_page(page);
		return 0;
	}

	if (!page_has_buffers(page)) {
		create_empty_buffers(page, s->s_blocksize,
				     (1 << BH_Dirty) | (1 << BH_Uptodate));
	}
	head = page_buffers(page);

	if (page->index >= end_index) {
		unsigned last_offset;

		last_offset = inode->i_size & (PAGE_CACHE_SIZE - 1);
		
		if (page->index >= end_index + 1 || !last_offset) {
			unlock_page(page);
			return 0;
		}
		zero_user_segment(page, last_offset, PAGE_CACHE_SIZE);
	}
	bh = head;
	block = page->index << (PAGE_CACHE_SHIFT - s->s_blocksize_bits);
	last_block = (i_size_read(inode) - 1) >> inode->i_blkbits;
	
	do {
		if (block > last_block) {
			clear_buffer_dirty(bh);
			set_buffer_uptodate(bh);
		} else if ((checked || buffer_dirty(bh)) &&
		           (!buffer_mapped(bh) || (buffer_mapped(bh)
						       && bh->b_blocknr ==
						       0))) {
			if ((error = map_block_for_writepage(inode, bh, block))) {
				goto fail;
			}
		}
		bh = bh->b_this_page;
		block++;
	} while (bh != head);

	if (checked) {
		ClearPageChecked(page);
		reiserfs_write_lock(s);
		error = journal_begin(&th, s, bh_per_page + 1);
		if (error) {
			reiserfs_write_unlock(s);
			goto fail;
		}
		reiserfs_update_inode_transaction(inode);
	}
	
	do {
		get_bh(bh);
		if (!buffer_mapped(bh))
			continue;
		if (buffer_mapped(bh) && bh->b_blocknr == 0)
			continue;

		if (checked) {
			reiserfs_prepare_for_journal(s, bh, 1);
			journal_mark_dirty(&th, s, bh);
			continue;
		}
		if (wbc->sync_mode != WB_SYNC_NONE) {
			lock_buffer(bh);
		} else {
			if (!trylock_buffer(bh)) {
				redirty_page_for_writepage(wbc, page);
				continue;
			}
		}
		if (test_clear_buffer_dirty(bh)) {
			mark_buffer_async_write(bh);
		} else {
			unlock_buffer(bh);
		}
	} while ((bh = bh->b_this_page) != head);

	if (checked) {
		error = journal_end(&th, s, bh_per_page + 1);
		reiserfs_write_unlock(s);
		if (error)
			goto fail;
	}
	BUG_ON(PageWriteback(page));
	set_page_writeback(page);
	unlock_page(page);

	do {
		struct buffer_head *next = bh->b_this_page;
		if (buffer_async_write(bh)) {
			submit_bh(WRITE, bh);
			nr++;
		}
		put_bh(bh);
		bh = next;
	} while (bh != head);

	error = 0;
      done:
	if (nr == 0) {
		bh = head;
		do {
			if (!buffer_uptodate(bh)) {
				partial = 1;
				break;
			}
			bh = bh->b_this_page;
		} while (bh != head);
		if (!partial)
			SetPageUptodate(page);
		end_page_writeback(page);
	}
	return error;

      fail:
	ClearPageUptodate(page);
	bh = head;
	do {
		get_bh(bh);
		if (buffer_mapped(bh) && buffer_dirty(bh) && bh->b_blocknr) {
			lock_buffer(bh);
			mark_buffer_async_write(bh);
		} else {
			clear_buffer_dirty(bh);
		}
		bh = bh->b_this_page;
	} while (bh != head);
	SetPageError(page);
	BUG_ON(PageWriteback(page));
	set_page_writeback(page);
	unlock_page(page);
	do {
		struct buffer_head *next = bh->b_this_page;
		if (buffer_async_write(bh)) {
			clear_buffer_dirty(bh);
			submit_bh(WRITE, bh);
			nr++;
		}
		put_bh(bh);
		bh = next;
	} while (bh != head);
	goto done;
}

static int reiserfs_readpage(struct file *f, struct page *page)
{
	return block_read_full_page(page, reiserfs_get_block);
}

static int reiserfs_writepage(struct page *page, struct writeback_control *wbc)
{
	struct inode *inode = page->mapping->host;
	reiserfs_wait_on_write_block(inode->i_sb);
	return reiserfs_write_full_page(page, wbc);
}

static void reiserfs_truncate_failed_write(struct inode *inode)
{
	truncate_inode_pages(inode->i_mapping, inode->i_size);
	reiserfs_truncate_file(inode, 0);
}

static int reiserfs_write_begin(struct file *file,
				struct address_space *mapping,
				loff_t pos, unsigned len, unsigned flags,
				struct page **pagep, void **fsdata)
{
	struct inode *inode;
	struct page *page;
	pgoff_t index;
	int ret;
	int old_ref = 0;

 	inode = mapping->host;
	*fsdata = 0;
 	if (flags & AOP_FLAG_CONT_EXPAND &&
 	    (pos & (inode->i_sb->s_blocksize - 1)) == 0) {
 		pos ++;
		*fsdata = (void *)(unsigned long)flags;
	}

	index = pos >> PAGE_CACHE_SHIFT;
	page = grab_cache_page_write_begin(mapping, index, flags);
	if (!page)
		return -ENOMEM;
	*pagep = page;

	reiserfs_wait_on_write_block(inode->i_sb);
	fix_tail_page_for_writing(page);
	if (reiserfs_transaction_running(inode->i_sb)) {
		struct reiserfs_transaction_handle *th;
		th = (struct reiserfs_transaction_handle *)current->
		    journal_info;
		BUG_ON(!th->t_refcount);
		BUG_ON(!th->t_trans_id);
		old_ref = th->t_refcount;
		th->t_refcount++;
	}
	ret = __block_write_begin(page, pos, len, reiserfs_get_block);
	if (ret && reiserfs_transaction_running(inode->i_sb)) {
		struct reiserfs_transaction_handle *th = current->journal_info;
		if (th->t_refcount > old_ref) {
			if (old_ref)
				th->t_refcount--;
			else {
				int err;
				reiserfs_write_lock(inode->i_sb);
				err = reiserfs_end_persistent_transaction(th);
				reiserfs_write_unlock(inode->i_sb);
				if (err)
					ret = err;
			}
		}
	}
	if (ret) {
		unlock_page(page);
		page_cache_release(page);
		
		reiserfs_truncate_failed_write(inode);
	}
	return ret;
}

int __reiserfs_write_begin(struct page *page, unsigned from, unsigned len)
{
	struct inode *inode = page->mapping->host;
	int ret;
	int old_ref = 0;

	reiserfs_write_unlock(inode->i_sb);
	reiserfs_wait_on_write_block(inode->i_sb);
	reiserfs_write_lock(inode->i_sb);

	fix_tail_page_for_writing(page);
	if (reiserfs_transaction_running(inode->i_sb)) {
		struct reiserfs_transaction_handle *th;
		th = (struct reiserfs_transaction_handle *)current->
		    journal_info;
		BUG_ON(!th->t_refcount);
		BUG_ON(!th->t_trans_id);
		old_ref = th->t_refcount;
		th->t_refcount++;
	}

	ret = __block_write_begin(page, from, len, reiserfs_get_block);
	if (ret && reiserfs_transaction_running(inode->i_sb)) {
		struct reiserfs_transaction_handle *th = current->journal_info;
		if (th->t_refcount > old_ref) {
			if (old_ref)
				th->t_refcount--;
			else {
				int err;
				reiserfs_write_lock(inode->i_sb);
				err = reiserfs_end_persistent_transaction(th);
				reiserfs_write_unlock(inode->i_sb);
				if (err)
					ret = err;
			}
		}
	}
	return ret;

}

static sector_t reiserfs_aop_bmap(struct address_space *as, sector_t block)
{
	return generic_block_bmap(as, block, reiserfs_bmap);
}

static int reiserfs_write_end(struct file *file, struct address_space *mapping,
			      loff_t pos, unsigned len, unsigned copied,
			      struct page *page, void *fsdata)
{
	struct inode *inode = page->mapping->host;
	int ret = 0;
	int update_sd = 0;
	struct reiserfs_transaction_handle *th;
	unsigned start;
	int lock_depth = 0;
	bool locked = false;

	if ((unsigned long)fsdata & AOP_FLAG_CONT_EXPAND)
		pos ++;

	reiserfs_wait_on_write_block(inode->i_sb);
	if (reiserfs_transaction_running(inode->i_sb))
		th = current->journal_info;
	else
		th = NULL;

	start = pos & (PAGE_CACHE_SIZE - 1);
	if (unlikely(copied < len)) {
		if (!PageUptodate(page))
			copied = 0;

		page_zero_new_buffers(page, start + copied, start + len);
	}
	flush_dcache_page(page);

	reiserfs_commit_page(inode, page, start, start + copied);

	if (pos + copied > inode->i_size) {
		struct reiserfs_transaction_handle myth;
		lock_depth = reiserfs_write_lock_once(inode->i_sb);
		locked = true;
		if ((have_large_tails(inode->i_sb)
		     && inode->i_size > i_block_size(inode) * 4)
		    || (have_small_tails(inode->i_sb)
			&& inode->i_size > i_block_size(inode)))
			REISERFS_I(inode)->i_flags &= ~i_pack_on_close_mask;

		ret = journal_begin(&myth, inode->i_sb, 1);
		if (ret)
			goto journal_error;

		reiserfs_update_inode_transaction(inode);
		inode->i_size = pos + copied;
		mark_inode_dirty(inode);
		reiserfs_update_sd(&myth, inode);
		update_sd = 1;
		ret = journal_end(&myth, inode->i_sb, 1);
		if (ret)
			goto journal_error;
	}
	if (th) {
		if (!locked) {
			lock_depth = reiserfs_write_lock_once(inode->i_sb);
			locked = true;
		}
		if (!update_sd)
			mark_inode_dirty(inode);
		ret = reiserfs_end_persistent_transaction(th);
		if (ret)
			goto out;
	}

      out:
	if (locked)
		reiserfs_write_unlock_once(inode->i_sb, lock_depth);
	unlock_page(page);
	page_cache_release(page);

	if (pos + len > inode->i_size)
		reiserfs_truncate_failed_write(inode);

	return ret == 0 ? copied : ret;

      journal_error:
	reiserfs_write_unlock_once(inode->i_sb, lock_depth);
	locked = false;
	if (th) {
		if (!update_sd)
			reiserfs_update_sd(th, inode);
		ret = reiserfs_end_persistent_transaction(th);
	}
	goto out;
}

int reiserfs_commit_write(struct file *f, struct page *page,
			  unsigned from, unsigned to)
{
	struct inode *inode = page->mapping->host;
	loff_t pos = ((loff_t) page->index << PAGE_CACHE_SHIFT) + to;
	int ret = 0;
	int update_sd = 0;
	struct reiserfs_transaction_handle *th = NULL;

	reiserfs_write_unlock(inode->i_sb);
	reiserfs_wait_on_write_block(inode->i_sb);
	reiserfs_write_lock(inode->i_sb);

	if (reiserfs_transaction_running(inode->i_sb)) {
		th = current->journal_info;
	}
	reiserfs_commit_page(inode, page, from, to);

	if (pos > inode->i_size) {
		struct reiserfs_transaction_handle myth;
		if ((have_large_tails(inode->i_sb)
		     && inode->i_size > i_block_size(inode) * 4)
		    || (have_small_tails(inode->i_sb)
			&& inode->i_size > i_block_size(inode)))
			REISERFS_I(inode)->i_flags &= ~i_pack_on_close_mask;

		ret = journal_begin(&myth, inode->i_sb, 1);
		if (ret)
			goto journal_error;

		reiserfs_update_inode_transaction(inode);
		inode->i_size = pos;
		mark_inode_dirty(inode);
		reiserfs_update_sd(&myth, inode);
		update_sd = 1;
		ret = journal_end(&myth, inode->i_sb, 1);
		if (ret)
			goto journal_error;
	}
	if (th) {
		if (!update_sd)
			mark_inode_dirty(inode);
		ret = reiserfs_end_persistent_transaction(th);
		if (ret)
			goto out;
	}

      out:
	return ret;

      journal_error:
	if (th) {
		if (!update_sd)
			reiserfs_update_sd(th, inode);
		ret = reiserfs_end_persistent_transaction(th);
	}

	return ret;
}

void sd_attrs_to_i_attrs(__u16 sd_attrs, struct inode *inode)
{
	if (reiserfs_attrs(inode->i_sb)) {
		if (sd_attrs & REISERFS_SYNC_FL)
			inode->i_flags |= S_SYNC;
		else
			inode->i_flags &= ~S_SYNC;
		if (sd_attrs & REISERFS_IMMUTABLE_FL)
			inode->i_flags |= S_IMMUTABLE;
		else
			inode->i_flags &= ~S_IMMUTABLE;
		if (sd_attrs & REISERFS_APPEND_FL)
			inode->i_flags |= S_APPEND;
		else
			inode->i_flags &= ~S_APPEND;
		if (sd_attrs & REISERFS_NOATIME_FL)
			inode->i_flags |= S_NOATIME;
		else
			inode->i_flags &= ~S_NOATIME;
		if (sd_attrs & REISERFS_NOTAIL_FL)
			REISERFS_I(inode)->i_flags |= i_nopack_mask;
		else
			REISERFS_I(inode)->i_flags &= ~i_nopack_mask;
	}
}

void i_attrs_to_sd_attrs(struct inode *inode, __u16 * sd_attrs)
{
	if (reiserfs_attrs(inode->i_sb)) {
		if (inode->i_flags & S_IMMUTABLE)
			*sd_attrs |= REISERFS_IMMUTABLE_FL;
		else
			*sd_attrs &= ~REISERFS_IMMUTABLE_FL;
		if (inode->i_flags & S_SYNC)
			*sd_attrs |= REISERFS_SYNC_FL;
		else
			*sd_attrs &= ~REISERFS_SYNC_FL;
		if (inode->i_flags & S_NOATIME)
			*sd_attrs |= REISERFS_NOATIME_FL;
		else
			*sd_attrs &= ~REISERFS_NOATIME_FL;
		if (REISERFS_I(inode)->i_flags & i_nopack_mask)
			*sd_attrs |= REISERFS_NOTAIL_FL;
		else
			*sd_attrs &= ~REISERFS_NOTAIL_FL;
	}
}

static int invalidatepage_can_drop(struct inode *inode, struct buffer_head *bh)
{
	int ret = 1;
	struct reiserfs_journal *j = SB_JOURNAL(inode->i_sb);

	lock_buffer(bh);
	spin_lock(&j->j_dirty_buffers_lock);
	if (!buffer_mapped(bh)) {
		goto free_jh;
	}
	if (reiserfs_file_data_log(inode)) {
		if (buffer_journaled(bh) || buffer_journal_dirty(bh)) {
			ret = 0;
		}
	} else  if (buffer_dirty(bh)) {
		struct reiserfs_journal_list *jl;
		struct reiserfs_jh *jh = bh->b_private;

		if (jh && (jl = jh->jl)
		    && jl != SB_JOURNAL(inode->i_sb)->j_current_jl)
			ret = 0;
	}
      free_jh:
	if (ret && bh->b_private) {
		reiserfs_free_jh(bh);
	}
	spin_unlock(&j->j_dirty_buffers_lock);
	unlock_buffer(bh);
	return ret;
}

static void reiserfs_invalidatepage(struct page *page, unsigned long offset)
{
	struct buffer_head *head, *bh, *next;
	struct inode *inode = page->mapping->host;
	unsigned int curr_off = 0;
	int ret = 1;

	BUG_ON(!PageLocked(page));

	if (offset == 0)
		ClearPageChecked(page);

	if (!page_has_buffers(page))
		goto out;

	head = page_buffers(page);
	bh = head;
	do {
		unsigned int next_off = curr_off + bh->b_size;
		next = bh->b_this_page;

		if (offset <= curr_off) {
			if (invalidatepage_can_drop(inode, bh))
				reiserfs_unmap_buffer(bh);
			else
				ret = 0;
		}
		curr_off = next_off;
		bh = next;
	} while (bh != head);

	if (!offset && ret) {
		ret = try_to_release_page(page, 0);
		
	}
      out:
	return;
}

static int reiserfs_set_page_dirty(struct page *page)
{
	struct inode *inode = page->mapping->host;
	if (reiserfs_file_data_log(inode)) {
		SetPageChecked(page);
		return __set_page_dirty_nobuffers(page);
	}
	return __set_page_dirty_buffers(page);
}

static int reiserfs_releasepage(struct page *page, gfp_t unused_gfp_flags)
{
	struct inode *inode = page->mapping->host;
	struct reiserfs_journal *j = SB_JOURNAL(inode->i_sb);
	struct buffer_head *head;
	struct buffer_head *bh;
	int ret = 1;

	WARN_ON(PageChecked(page));
	spin_lock(&j->j_dirty_buffers_lock);
	head = page_buffers(page);
	bh = head;
	do {
		if (bh->b_private) {
			if (!buffer_dirty(bh) && !buffer_locked(bh)) {
				reiserfs_free_jh(bh);
			} else {
				ret = 0;
				break;
			}
		}
		bh = bh->b_this_page;
	} while (bh != head);
	if (ret)
		ret = try_to_free_buffers(page);
	spin_unlock(&j->j_dirty_buffers_lock);
	return ret;
}

static ssize_t reiserfs_direct_IO(int rw, struct kiocb *iocb,
				  const struct iovec *iov, loff_t offset,
				  unsigned long nr_segs)
{
	struct file *file = iocb->ki_filp;
	struct inode *inode = file->f_mapping->host;
	ssize_t ret;

	ret = blockdev_direct_IO(rw, iocb, inode, iov, offset, nr_segs,
				  reiserfs_get_blocks_direct_io);

	if (unlikely((rw & WRITE) && ret < 0)) {
		loff_t isize = i_size_read(inode);
		loff_t end = offset + iov_length(iov, nr_segs);

		if (end > isize)
			vmtruncate(inode, isize);
	}

	return ret;
}

int reiserfs_setattr(struct dentry *dentry, struct iattr *attr)
{
	struct inode *inode = dentry->d_inode;
	unsigned int ia_valid;
	int depth;
	int error;

	error = inode_change_ok(inode, attr);
	if (error)
		return error;

	
	ia_valid = attr->ia_valid &= ~(ATTR_KILL_SUID|ATTR_KILL_SGID);

	depth = reiserfs_write_lock_once(inode->i_sb);
	if (is_quota_modification(inode, attr))
		dquot_initialize(inode);

	if (attr->ia_valid & ATTR_SIZE) {
		if (get_inode_item_key_version(inode) == KEY_FORMAT_3_5 &&
		    attr->ia_size > MAX_NON_LFS) {
			error = -EFBIG;
			goto out;
		}

		inode_dio_wait(inode);

		
		if (attr->ia_size > inode->i_size) {
			error = generic_cont_expand_simple(inode, attr->ia_size);
			if (REISERFS_I(inode)->i_prealloc_count > 0) {
				int err;
				struct reiserfs_transaction_handle th;
				
				err = journal_begin(&th, inode->i_sb, 4);
				if (!err) {
					reiserfs_discard_prealloc(&th, inode);
					err = journal_end(&th, inode->i_sb, 4);
				}
				if (err)
					error = err;
			}
			if (error)
				goto out;
			attr->ia_valid |= (ATTR_MTIME | ATTR_CTIME);
		}
	}

	if ((((attr->ia_valid & ATTR_UID) && (attr->ia_uid & ~0xffff)) ||
	     ((attr->ia_valid & ATTR_GID) && (attr->ia_gid & ~0xffff))) &&
	    (get_inode_sd_version(inode) == STAT_DATA_V1)) {
		
		error = -EINVAL;
		goto out;
	}

	if ((ia_valid & ATTR_UID && attr->ia_uid != inode->i_uid) ||
	    (ia_valid & ATTR_GID && attr->ia_gid != inode->i_gid)) {
		struct reiserfs_transaction_handle th;
		int jbegin_count =
		    2 *
		    (REISERFS_QUOTA_INIT_BLOCKS(inode->i_sb) +
		     REISERFS_QUOTA_DEL_BLOCKS(inode->i_sb)) +
		    2;

		error = reiserfs_chown_xattrs(inode, attr);

		if (error)
			return error;

		
		error = journal_begin(&th, inode->i_sb, jbegin_count);
		if (error)
			goto out;
		error = dquot_transfer(inode, attr);
		if (error) {
			journal_end(&th, inode->i_sb, jbegin_count);
			goto out;
		}

		if (attr->ia_valid & ATTR_UID)
			inode->i_uid = attr->ia_uid;
		if (attr->ia_valid & ATTR_GID)
			inode->i_gid = attr->ia_gid;
		mark_inode_dirty(inode);
		error = journal_end(&th, inode->i_sb, jbegin_count);
		if (error)
			goto out;
	}

	reiserfs_write_unlock_once(inode->i_sb, depth);
	if ((attr->ia_valid & ATTR_SIZE) &&
	    attr->ia_size != i_size_read(inode))
		error = vmtruncate(inode, attr->ia_size);

	if (!error) {
		setattr_copy(inode, attr);
		mark_inode_dirty(inode);
	}
	depth = reiserfs_write_lock_once(inode->i_sb);

	if (!error && reiserfs_posixacl(inode->i_sb)) {
		if (attr->ia_valid & ATTR_MODE)
			error = reiserfs_acl_chmod(inode);
	}

      out:
	reiserfs_write_unlock_once(inode->i_sb, depth);

	return error;
}

const struct address_space_operations reiserfs_address_space_operations = {
	.writepage = reiserfs_writepage,
	.readpage = reiserfs_readpage,
	.readpages = reiserfs_readpages,
	.releasepage = reiserfs_releasepage,
	.invalidatepage = reiserfs_invalidatepage,
	.write_begin = reiserfs_write_begin,
	.write_end = reiserfs_write_end,
	.bmap = reiserfs_aop_bmap,
	.direct_IO = reiserfs_direct_IO,
	.set_page_dirty = reiserfs_set_page_dirty,
};
