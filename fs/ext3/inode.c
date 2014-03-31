/*
 *  linux/fs/ext3/inode.c
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 *  from
 *
 *  linux/fs/minix/inode.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  Goal-directed block allocation by Stephen Tweedie
 *	(sct@redhat.com), 1993, 1998
 *  Big-endian to little-endian byte-swapping/bitmaps by
 *        David S. Miller (davem@caip.rutgers.edu), 1995
 *  64-bit file support on 64-bit platforms by Jakub Jelinek
 *	(jj@sunsite.ms.mff.cuni.cz)
 *
 *  Assorted race fixes, rewrite of ext3_get_block() by Al Viro, 2000
 */

#include <linux/highuid.h>
#include <linux/quotaops.h>
#include <linux/writeback.h>
#include <linux/mpage.h>
#include <linux/namei.h>
#include "ext3.h"
#include "xattr.h"
#include "acl.h"

static int ext3_writepage_trans_blocks(struct inode *inode);
static int ext3_block_truncate_page(struct inode *inode, loff_t from);

static int ext3_inode_is_fast_symlink(struct inode *inode)
{
	int ea_blocks = EXT3_I(inode)->i_file_acl ?
		(inode->i_sb->s_blocksize >> 9) : 0;

	return (S_ISLNK(inode->i_mode) && inode->i_blocks - ea_blocks == 0);
}

int ext3_forget(handle_t *handle, int is_metadata, struct inode *inode,
			struct buffer_head *bh, ext3_fsblk_t blocknr)
{
	int err;

	might_sleep();

	trace_ext3_forget(inode, is_metadata, blocknr);
	BUFFER_TRACE(bh, "enter");

	jbd_debug(4, "forgetting bh %p: is_metadata = %d, mode %o, "
		  "data mode %lx\n",
		  bh, is_metadata, inode->i_mode,
		  test_opt(inode->i_sb, DATA_FLAGS));


	if (test_opt(inode->i_sb, DATA_FLAGS) == EXT3_MOUNT_JOURNAL_DATA ||
	    (!is_metadata && !ext3_should_journal_data(inode))) {
		if (bh) {
			BUFFER_TRACE(bh, "call journal_forget");
			return ext3_journal_forget(handle, bh);
		}
		return 0;
	}

	BUFFER_TRACE(bh, "call ext3_journal_revoke");
	err = ext3_journal_revoke(handle, blocknr, bh);
	if (err)
		ext3_abort(inode->i_sb, __func__,
			   "error %d when attempting revoke", err);
	BUFFER_TRACE(bh, "exit");
	return err;
}

static unsigned long blocks_for_truncate(struct inode *inode)
{
	unsigned long needed;

	needed = inode->i_blocks >> (inode->i_sb->s_blocksize_bits - 9);

	if (needed < 2)
		needed = 2;

	if (needed > EXT3_MAX_TRANS_DATA)
		needed = EXT3_MAX_TRANS_DATA;

	return EXT3_DATA_TRANS_BLOCKS(inode->i_sb) + needed;
}

static handle_t *start_transaction(struct inode *inode)
{
	handle_t *result;

	result = ext3_journal_start(inode, blocks_for_truncate(inode));
	if (!IS_ERR(result))
		return result;

	ext3_std_error(inode->i_sb, PTR_ERR(result));
	return result;
}

static int try_to_extend_transaction(handle_t *handle, struct inode *inode)
{
	if (handle->h_buffer_credits > EXT3_RESERVE_TRANS_BLOCKS)
		return 0;
	if (!ext3_journal_extend(handle, blocks_for_truncate(inode)))
		return 0;
	return 1;
}

static int truncate_restart_transaction(handle_t *handle, struct inode *inode)
{
	int ret;

	jbd_debug(2, "restarting handle %p\n", handle);
	mutex_unlock(&EXT3_I(inode)->truncate_mutex);
	ret = ext3_journal_restart(handle, blocks_for_truncate(inode));
	mutex_lock(&EXT3_I(inode)->truncate_mutex);
	return ret;
}

void ext3_evict_inode (struct inode *inode)
{
	struct ext3_inode_info *ei = EXT3_I(inode);
	struct ext3_block_alloc_info *rsv;
	handle_t *handle;
	int want_delete = 0;

	trace_ext3_evict_inode(inode);
	if (!inode->i_nlink && !is_bad_inode(inode)) {
		dquot_initialize(inode);
		want_delete = 1;
	}

	if (inode->i_nlink && ext3_should_journal_data(inode) &&
	    EXT3_SB(inode->i_sb)->s_journal &&
	    (S_ISLNK(inode->i_mode) || S_ISREG(inode->i_mode))) {
		tid_t commit_tid = atomic_read(&ei->i_datasync_tid);
		journal_t *journal = EXT3_SB(inode->i_sb)->s_journal;

		log_start_commit(journal, commit_tid);
		log_wait_commit(journal, commit_tid);
		filemap_write_and_wait(&inode->i_data);
	}
	truncate_inode_pages(&inode->i_data, 0);

	ext3_discard_reservation(inode);
	rsv = ei->i_block_alloc_info;
	ei->i_block_alloc_info = NULL;
	if (unlikely(rsv))
		kfree(rsv);

	if (!want_delete)
		goto no_delete;

	handle = start_transaction(inode);
	if (IS_ERR(handle)) {
		ext3_orphan_del(NULL, inode);
		goto no_delete;
	}

	if (IS_SYNC(inode))
		handle->h_sync = 1;
	inode->i_size = 0;
	if (inode->i_blocks)
		ext3_truncate(inode);
	ext3_orphan_del(handle, inode);
	ei->i_dtime = get_seconds();

	if (ext3_mark_inode_dirty(handle, inode)) {
		
		dquot_drop(inode);
		end_writeback(inode);
	} else {
		ext3_xattr_delete_inode(handle, inode);
		dquot_free_inode(inode);
		dquot_drop(inode);
		end_writeback(inode);
		ext3_free_inode(handle, inode);
	}
	ext3_journal_stop(handle);
	return;
no_delete:
	end_writeback(inode);
	dquot_drop(inode);
}

typedef struct {
	__le32	*p;
	__le32	key;
	struct buffer_head *bh;
} Indirect;

static inline void add_chain(Indirect *p, struct buffer_head *bh, __le32 *v)
{
	p->key = *(p->p = v);
	p->bh = bh;
}

static int verify_chain(Indirect *from, Indirect *to)
{
	while (from <= to && from->key == *from->p)
		from++;
	return (from > to);
}



static int ext3_block_to_path(struct inode *inode,
			long i_block, int offsets[4], int *boundary)
{
	int ptrs = EXT3_ADDR_PER_BLOCK(inode->i_sb);
	int ptrs_bits = EXT3_ADDR_PER_BLOCK_BITS(inode->i_sb);
	const long direct_blocks = EXT3_NDIR_BLOCKS,
		indirect_blocks = ptrs,
		double_blocks = (1 << (ptrs_bits * 2));
	int n = 0;
	int final = 0;

	if (i_block < 0) {
		ext3_warning (inode->i_sb, "ext3_block_to_path", "block < 0");
	} else if (i_block < direct_blocks) {
		offsets[n++] = i_block;
		final = direct_blocks;
	} else if ( (i_block -= direct_blocks) < indirect_blocks) {
		offsets[n++] = EXT3_IND_BLOCK;
		offsets[n++] = i_block;
		final = ptrs;
	} else if ((i_block -= indirect_blocks) < double_blocks) {
		offsets[n++] = EXT3_DIND_BLOCK;
		offsets[n++] = i_block >> ptrs_bits;
		offsets[n++] = i_block & (ptrs - 1);
		final = ptrs;
	} else if (((i_block -= double_blocks) >> (ptrs_bits * 2)) < ptrs) {
		offsets[n++] = EXT3_TIND_BLOCK;
		offsets[n++] = i_block >> (ptrs_bits * 2);
		offsets[n++] = (i_block >> ptrs_bits) & (ptrs - 1);
		offsets[n++] = i_block & (ptrs - 1);
		final = ptrs;
	} else {
		ext3_warning(inode->i_sb, "ext3_block_to_path", "block > big");
	}
	if (boundary)
		*boundary = final - 1 - (i_block & (ptrs - 1));
	return n;
}

static Indirect *ext3_get_branch(struct inode *inode, int depth, int *offsets,
				 Indirect chain[4], int *err)
{
	struct super_block *sb = inode->i_sb;
	Indirect *p = chain;
	struct buffer_head *bh;

	*err = 0;
	
	add_chain (chain, NULL, EXT3_I(inode)->i_data + *offsets);
	if (!p->key)
		goto no_block;
	while (--depth) {
		bh = sb_bread(sb, le32_to_cpu(p->key));
		if (!bh)
			goto failure;
		
		if (!verify_chain(chain, p))
			goto changed;
		add_chain(++p, bh, (__le32*)bh->b_data + *++offsets);
		
		if (!p->key)
			goto no_block;
	}
	return NULL;

changed:
	brelse(bh);
	*err = -EAGAIN;
	goto no_block;
failure:
	*err = -EIO;
no_block:
	return p;
}

static ext3_fsblk_t ext3_find_near(struct inode *inode, Indirect *ind)
{
	struct ext3_inode_info *ei = EXT3_I(inode);
	__le32 *start = ind->bh ? (__le32*) ind->bh->b_data : ei->i_data;
	__le32 *p;
	ext3_fsblk_t bg_start;
	ext3_grpblk_t colour;

	
	for (p = ind->p - 1; p >= start; p--) {
		if (*p)
			return le32_to_cpu(*p);
	}

	
	if (ind->bh)
		return ind->bh->b_blocknr;

	bg_start = ext3_group_first_block_no(inode->i_sb, ei->i_block_group);
	colour = (current->pid % 16) *
			(EXT3_BLOCKS_PER_GROUP(inode->i_sb) / 16);
	return bg_start + colour;
}


static ext3_fsblk_t ext3_find_goal(struct inode *inode, long block,
				   Indirect *partial)
{
	struct ext3_block_alloc_info *block_i;

	block_i =  EXT3_I(inode)->i_block_alloc_info;

	if (block_i && (block == block_i->last_alloc_logical_block + 1)
		&& (block_i->last_alloc_physical_block != 0)) {
		return block_i->last_alloc_physical_block + 1;
	}

	return ext3_find_near(inode, partial);
}

static int ext3_blks_to_allocate(Indirect *branch, int k, unsigned long blks,
		int blocks_to_boundary)
{
	unsigned long count = 0;

	if (k > 0) {
		
		if (blks < blocks_to_boundary + 1)
			count += blks;
		else
			count += blocks_to_boundary + 1;
		return count;
	}

	count++;
	while (count < blks && count <= blocks_to_boundary &&
		le32_to_cpu(*(branch[0].p + count)) == 0) {
		count++;
	}
	return count;
}

static int ext3_alloc_blocks(handle_t *handle, struct inode *inode,
			ext3_fsblk_t goal, int indirect_blks, int blks,
			ext3_fsblk_t new_blocks[4], int *err)
{
	int target, i;
	unsigned long count = 0;
	int index = 0;
	ext3_fsblk_t current_block = 0;
	int ret = 0;

	target = blks + indirect_blks;

	while (1) {
		count = target;
		
		current_block = ext3_new_blocks(handle,inode,goal,&count,err);
		if (*err)
			goto failed_out;

		target -= count;
		
		while (index < indirect_blks && count) {
			new_blocks[index++] = current_block++;
			count--;
		}

		if (count > 0)
			break;
	}

	
	new_blocks[index] = current_block;

	
	ret = count;
	*err = 0;
	return ret;
failed_out:
	for (i = 0; i <index; i++)
		ext3_free_blocks(handle, inode, new_blocks[i], 1);
	return ret;
}

static int ext3_alloc_branch(handle_t *handle, struct inode *inode,
			int indirect_blks, int *blks, ext3_fsblk_t goal,
			int *offsets, Indirect *branch)
{
	int blocksize = inode->i_sb->s_blocksize;
	int i, n = 0;
	int err = 0;
	struct buffer_head *bh;
	int num;
	ext3_fsblk_t new_blocks[4];
	ext3_fsblk_t current_block;

	num = ext3_alloc_blocks(handle, inode, goal, indirect_blks,
				*blks, new_blocks, &err);
	if (err)
		return err;

	branch[0].key = cpu_to_le32(new_blocks[0]);
	for (n = 1; n <= indirect_blks;  n++) {
		bh = sb_getblk(inode->i_sb, new_blocks[n-1]);
		branch[n].bh = bh;
		lock_buffer(bh);
		BUFFER_TRACE(bh, "call get_create_access");
		err = ext3_journal_get_create_access(handle, bh);
		if (err) {
			unlock_buffer(bh);
			brelse(bh);
			goto failed;
		}

		memset(bh->b_data, 0, blocksize);
		branch[n].p = (__le32 *) bh->b_data + offsets[n];
		branch[n].key = cpu_to_le32(new_blocks[n]);
		*branch[n].p = branch[n].key;
		if ( n == indirect_blks) {
			current_block = new_blocks[n];
			for (i=1; i < num; i++)
				*(branch[n].p + i) = cpu_to_le32(++current_block);
		}
		BUFFER_TRACE(bh, "marking uptodate");
		set_buffer_uptodate(bh);
		unlock_buffer(bh);

		BUFFER_TRACE(bh, "call ext3_journal_dirty_metadata");
		err = ext3_journal_dirty_metadata(handle, bh);
		if (err)
			goto failed;
	}
	*blks = num;
	return err;
failed:
	
	for (i = 1; i <= n ; i++) {
		BUFFER_TRACE(branch[i].bh, "call journal_forget");
		ext3_journal_forget(handle, branch[i].bh);
	}
	for (i = 0; i <indirect_blks; i++)
		ext3_free_blocks(handle, inode, new_blocks[i], 1);

	ext3_free_blocks(handle, inode, new_blocks[i], num);

	return err;
}

static int ext3_splice_branch(handle_t *handle, struct inode *inode,
			long block, Indirect *where, int num, int blks)
{
	int i;
	int err = 0;
	struct ext3_block_alloc_info *block_i;
	ext3_fsblk_t current_block;
	struct ext3_inode_info *ei = EXT3_I(inode);
	struct timespec now;

	block_i = ei->i_block_alloc_info;
	if (where->bh) {
		BUFFER_TRACE(where->bh, "get_write_access");
		err = ext3_journal_get_write_access(handle, where->bh);
		if (err)
			goto err_out;
	}
	

	*where->p = where->key;

	if (num == 0 && blks > 1) {
		current_block = le32_to_cpu(where->key) + 1;
		for (i = 1; i < blks; i++)
			*(where->p + i ) = cpu_to_le32(current_block++);
	}

	if (block_i) {
		block_i->last_alloc_logical_block = block + blks - 1;
		block_i->last_alloc_physical_block =
				le32_to_cpu(where[num].key) + blks - 1;
	}

	
	now = CURRENT_TIME_SEC;
	if (!timespec_equal(&inode->i_ctime, &now) || !where->bh) {
		inode->i_ctime = now;
		ext3_mark_inode_dirty(handle, inode);
	}
	
	atomic_set(&ei->i_datasync_tid, handle->h_transaction->t_tid);

	
	if (where->bh) {
		jbd_debug(5, "splicing indirect only\n");
		BUFFER_TRACE(where->bh, "call ext3_journal_dirty_metadata");
		err = ext3_journal_dirty_metadata(handle, where->bh);
		if (err)
			goto err_out;
	} else {
		jbd_debug(5, "splicing direct\n");
	}
	return err;

err_out:
	for (i = 1; i <= num; i++) {
		BUFFER_TRACE(where[i].bh, "call journal_forget");
		ext3_journal_forget(handle, where[i].bh);
		ext3_free_blocks(handle,inode,le32_to_cpu(where[i-1].key),1);
	}
	ext3_free_blocks(handle, inode, le32_to_cpu(where[num].key), blks);

	return err;
}

int ext3_get_blocks_handle(handle_t *handle, struct inode *inode,
		sector_t iblock, unsigned long maxblocks,
		struct buffer_head *bh_result,
		int create)
{
	int err = -EIO;
	int offsets[4];
	Indirect chain[4];
	Indirect *partial;
	ext3_fsblk_t goal;
	int indirect_blks;
	int blocks_to_boundary = 0;
	int depth;
	struct ext3_inode_info *ei = EXT3_I(inode);
	int count = 0;
	ext3_fsblk_t first_block = 0;


	trace_ext3_get_blocks_enter(inode, iblock, maxblocks, create);
	J_ASSERT(handle != NULL || create == 0);
	depth = ext3_block_to_path(inode,iblock,offsets,&blocks_to_boundary);

	if (depth == 0)
		goto out;

	partial = ext3_get_branch(inode, depth, offsets, chain, &err);

	
	if (!partial) {
		first_block = le32_to_cpu(chain[depth - 1].key);
		clear_buffer_new(bh_result);
		count++;
		
		while (count < maxblocks && count <= blocks_to_boundary) {
			ext3_fsblk_t blk;

			if (!verify_chain(chain, chain + depth - 1)) {
				err = -EAGAIN;
				count = 0;
				break;
			}
			blk = le32_to_cpu(*(chain[depth-1].p + count));

			if (blk == first_block + count)
				count++;
			else
				break;
		}
		if (err != -EAGAIN)
			goto got_it;
	}

	
	if (!create || err == -EIO)
		goto cleanup;

	mutex_lock(&ei->truncate_mutex);

	if (err == -EAGAIN || !verify_chain(chain, partial)) {
		while (partial > chain) {
			brelse(partial->bh);
			partial--;
		}
		partial = ext3_get_branch(inode, depth, offsets, chain, &err);
		if (!partial) {
			count++;
			mutex_unlock(&ei->truncate_mutex);
			if (err)
				goto cleanup;
			clear_buffer_new(bh_result);
			goto got_it;
		}
	}

	if (S_ISREG(inode->i_mode) && (!ei->i_block_alloc_info))
		ext3_init_block_alloc_info(inode);

	goal = ext3_find_goal(inode, iblock, partial);

	
	indirect_blks = (chain + depth) - partial - 1;

	count = ext3_blks_to_allocate(partial, indirect_blks,
					maxblocks, blocks_to_boundary);
	err = ext3_alloc_branch(handle, inode, indirect_blks, &count, goal,
				offsets + (partial - chain), partial);

	if (!err)
		err = ext3_splice_branch(handle, inode, iblock,
					partial, indirect_blks, count);
	mutex_unlock(&ei->truncate_mutex);
	if (err)
		goto cleanup;

	set_buffer_new(bh_result);
got_it:
	map_bh(bh_result, inode->i_sb, le32_to_cpu(chain[depth-1].key));
	if (count > blocks_to_boundary)
		set_buffer_boundary(bh_result);
	err = count;
	
	partial = chain + depth - 1;	
cleanup:
	while (partial > chain) {
		BUFFER_TRACE(partial->bh, "call brelse");
		brelse(partial->bh);
		partial--;
	}
	BUFFER_TRACE(bh_result, "returned");
out:
	trace_ext3_get_blocks_exit(inode, iblock,
				   depth ? le32_to_cpu(chain[depth-1].key) : 0,
				   count, err);
	return err;
}

#define DIO_MAX_BLOCKS 4096
#define DIO_CREDITS 25

static int ext3_get_block(struct inode *inode, sector_t iblock,
			struct buffer_head *bh_result, int create)
{
	handle_t *handle = ext3_journal_current_handle();
	int ret = 0, started = 0;
	unsigned max_blocks = bh_result->b_size >> inode->i_blkbits;

	if (create && !handle) {	
		if (max_blocks > DIO_MAX_BLOCKS)
			max_blocks = DIO_MAX_BLOCKS;
		handle = ext3_journal_start(inode, DIO_CREDITS +
				EXT3_MAXQUOTAS_TRANS_BLOCKS(inode->i_sb));
		if (IS_ERR(handle)) {
			ret = PTR_ERR(handle);
			goto out;
		}
		started = 1;
	}

	ret = ext3_get_blocks_handle(handle, inode, iblock,
					max_blocks, bh_result, create);
	if (ret > 0) {
		bh_result->b_size = (ret << inode->i_blkbits);
		ret = 0;
	}
	if (started)
		ext3_journal_stop(handle);
out:
	return ret;
}

int ext3_fiemap(struct inode *inode, struct fiemap_extent_info *fieinfo,
		u64 start, u64 len)
{
	return generic_block_fiemap(inode, fieinfo, start, len,
				    ext3_get_block);
}

struct buffer_head *ext3_getblk(handle_t *handle, struct inode *inode,
				long block, int create, int *errp)
{
	struct buffer_head dummy;
	int fatal = 0, err;

	J_ASSERT(handle != NULL || create == 0);

	dummy.b_state = 0;
	dummy.b_blocknr = -1000;
	buffer_trace_init(&dummy.b_history);
	err = ext3_get_blocks_handle(handle, inode, block, 1,
					&dummy, create);
	if (err > 0) {
		if (err > 1)
			WARN_ON(1);
		err = 0;
	}
	*errp = err;
	if (!err && buffer_mapped(&dummy)) {
		struct buffer_head *bh;
		bh = sb_getblk(inode->i_sb, dummy.b_blocknr);
		if (!bh) {
			*errp = -EIO;
			goto err;
		}
		if (buffer_new(&dummy)) {
			J_ASSERT(create != 0);
			J_ASSERT(handle != NULL);

			lock_buffer(bh);
			BUFFER_TRACE(bh, "call get_create_access");
			fatal = ext3_journal_get_create_access(handle, bh);
			if (!fatal && !buffer_uptodate(bh)) {
				memset(bh->b_data,0,inode->i_sb->s_blocksize);
				set_buffer_uptodate(bh);
			}
			unlock_buffer(bh);
			BUFFER_TRACE(bh, "call ext3_journal_dirty_metadata");
			err = ext3_journal_dirty_metadata(handle, bh);
			if (!fatal)
				fatal = err;
		} else {
			BUFFER_TRACE(bh, "not a new buffer");
		}
		if (fatal) {
			*errp = fatal;
			brelse(bh);
			bh = NULL;
		}
		return bh;
	}
err:
	return NULL;
}

struct buffer_head *ext3_bread(handle_t *handle, struct inode *inode,
			       int block, int create, int *err)
{
	struct buffer_head * bh;

	bh = ext3_getblk(handle, inode, block, create, err);
	if (!bh)
		return bh;
	if (bh_uptodate_or_lock(bh))
		return bh;
	get_bh(bh);
	bh->b_end_io = end_buffer_read_sync;
	submit_bh(READ | REQ_META | REQ_PRIO, bh);
	wait_on_buffer(bh);
	if (buffer_uptodate(bh))
		return bh;
	put_bh(bh);
	*err = -EIO;
	return NULL;
}

static int walk_page_buffers(	handle_t *handle,
				struct buffer_head *head,
				unsigned from,
				unsigned to,
				int *partial,
				int (*fn)(	handle_t *handle,
						struct buffer_head *bh))
{
	struct buffer_head *bh;
	unsigned block_start, block_end;
	unsigned blocksize = head->b_size;
	int err, ret = 0;
	struct buffer_head *next;

	for (	bh = head, block_start = 0;
		ret == 0 && (bh != head || !block_start);
		block_start = block_end, bh = next)
	{
		next = bh->b_this_page;
		block_end = block_start + blocksize;
		if (block_end <= from || block_start >= to) {
			if (partial && !buffer_uptodate(bh))
				*partial = 1;
			continue;
		}
		err = (*fn)(handle, bh);
		if (!ret)
			ret = err;
	}
	return ret;
}

static int do_journal_get_write_access(handle_t *handle,
					struct buffer_head *bh)
{
	int dirty = buffer_dirty(bh);
	int ret;

	if (!buffer_mapped(bh) || buffer_freed(bh))
		return 0;
	if (dirty)
		clear_buffer_dirty(bh);
	ret = ext3_journal_get_write_access(handle, bh);
	if (!ret && dirty)
		ret = ext3_journal_dirty_metadata(handle, bh);
	return ret;
}

static void ext3_truncate_failed_write(struct inode *inode)
{
	truncate_inode_pages(inode->i_mapping, inode->i_size);
	ext3_truncate(inode);
}

/*
 * Truncate blocks that were not used by direct IO write. We have to zero out
 * the last file block as well because direct IO might have written to it.
 */
static void ext3_truncate_failed_direct_write(struct inode *inode)
{
	ext3_block_truncate_page(inode, inode->i_size);
	ext3_truncate(inode);
}

static int ext3_write_begin(struct file *file, struct address_space *mapping,
				loff_t pos, unsigned len, unsigned flags,
				struct page **pagep, void **fsdata)
{
	struct inode *inode = mapping->host;
	int ret;
	handle_t *handle;
	int retries = 0;
	struct page *page;
	pgoff_t index;
	unsigned from, to;
	int needed_blocks = ext3_writepage_trans_blocks(inode) + 1;

	trace_ext3_write_begin(inode, pos, len, flags);

	index = pos >> PAGE_CACHE_SHIFT;
	from = pos & (PAGE_CACHE_SIZE - 1);
	to = from + len;

retry:
	page = grab_cache_page_write_begin(mapping, index, flags);
	if (!page)
		return -ENOMEM;
	*pagep = page;

	handle = ext3_journal_start(inode, needed_blocks);
	if (IS_ERR(handle)) {
		unlock_page(page);
		page_cache_release(page);
		ret = PTR_ERR(handle);
		goto out;
	}
	ret = __block_write_begin(page, pos, len, ext3_get_block);
	if (ret)
		goto write_begin_failed;

	if (ext3_should_journal_data(inode)) {
		ret = walk_page_buffers(handle, page_buffers(page),
				from, to, NULL, do_journal_get_write_access);
	}
write_begin_failed:
	if (ret) {
		if (pos + len > inode->i_size && ext3_can_truncate(inode))
			ext3_orphan_add(handle, inode);
		ext3_journal_stop(handle);
		unlock_page(page);
		page_cache_release(page);
		if (pos + len > inode->i_size)
			ext3_truncate_failed_write(inode);
	}
	if (ret == -ENOSPC && ext3_should_retry_alloc(inode->i_sb, &retries))
		goto retry;
out:
	return ret;
}


int ext3_journal_dirty_data(handle_t *handle, struct buffer_head *bh)
{
	int err = journal_dirty_data(handle, bh);
	if (err)
		ext3_journal_abort_handle(__func__, __func__,
						bh, handle, err);
	return err;
}

static int journal_dirty_data_fn(handle_t *handle, struct buffer_head *bh)
{
	if (buffer_mapped(bh) && buffer_uptodate(bh))
		return ext3_journal_dirty_data(handle, bh);
	return 0;
}

static int write_end_fn(handle_t *handle, struct buffer_head *bh)
{
	if (!buffer_mapped(bh) || buffer_freed(bh))
		return 0;
	set_buffer_uptodate(bh);
	return ext3_journal_dirty_metadata(handle, bh);
}

static void update_file_sizes(struct inode *inode, loff_t pos, unsigned copied)
{
	
	if (pos + copied > inode->i_size)
		i_size_write(inode, pos + copied);
	if (pos + copied > EXT3_I(inode)->i_disksize) {
		EXT3_I(inode)->i_disksize = pos + copied;
		mark_inode_dirty(inode);
	}
}

static int ext3_ordered_write_end(struct file *file,
				struct address_space *mapping,
				loff_t pos, unsigned len, unsigned copied,
				struct page *page, void *fsdata)
{
	handle_t *handle = ext3_journal_current_handle();
	struct inode *inode = file->f_mapping->host;
	unsigned from, to;
	int ret = 0, ret2;

	trace_ext3_ordered_write_end(inode, pos, len, copied);
	copied = block_write_end(file, mapping, pos, len, copied, page, fsdata);

	from = pos & (PAGE_CACHE_SIZE - 1);
	to = from + copied;
	ret = walk_page_buffers(handle, page_buffers(page),
		from, to, NULL, journal_dirty_data_fn);

	if (ret == 0)
		update_file_sizes(inode, pos, copied);
	if (pos + len > inode->i_size && ext3_can_truncate(inode))
		ext3_orphan_add(handle, inode);
	ret2 = ext3_journal_stop(handle);
	if (!ret)
		ret = ret2;
	unlock_page(page);
	page_cache_release(page);

	if (pos + len > inode->i_size)
		ext3_truncate_failed_write(inode);
	return ret ? ret : copied;
}

static int ext3_writeback_write_end(struct file *file,
				struct address_space *mapping,
				loff_t pos, unsigned len, unsigned copied,
				struct page *page, void *fsdata)
{
	handle_t *handle = ext3_journal_current_handle();
	struct inode *inode = file->f_mapping->host;
	int ret;

	trace_ext3_writeback_write_end(inode, pos, len, copied);
	copied = block_write_end(file, mapping, pos, len, copied, page, fsdata);
	update_file_sizes(inode, pos, copied);
	if (pos + len > inode->i_size && ext3_can_truncate(inode))
		ext3_orphan_add(handle, inode);
	ret = ext3_journal_stop(handle);
	unlock_page(page);
	page_cache_release(page);

	if (pos + len > inode->i_size)
		ext3_truncate_failed_write(inode);
	return ret ? ret : copied;
}

static int ext3_journalled_write_end(struct file *file,
				struct address_space *mapping,
				loff_t pos, unsigned len, unsigned copied,
				struct page *page, void *fsdata)
{
	handle_t *handle = ext3_journal_current_handle();
	struct inode *inode = mapping->host;
	struct ext3_inode_info *ei = EXT3_I(inode);
	int ret = 0, ret2;
	int partial = 0;
	unsigned from, to;

	trace_ext3_journalled_write_end(inode, pos, len, copied);
	from = pos & (PAGE_CACHE_SIZE - 1);
	to = from + len;

	if (copied < len) {
		if (!PageUptodate(page))
			copied = 0;
		page_zero_new_buffers(page, from + copied, to);
		to = from + copied;
	}

	ret = walk_page_buffers(handle, page_buffers(page), from,
				to, &partial, write_end_fn);
	if (!partial)
		SetPageUptodate(page);

	if (pos + copied > inode->i_size)
		i_size_write(inode, pos + copied);
	if (pos + len > inode->i_size && ext3_can_truncate(inode))
		ext3_orphan_add(handle, inode);
	ext3_set_inode_state(inode, EXT3_STATE_JDATA);
	atomic_set(&ei->i_datasync_tid, handle->h_transaction->t_tid);
	if (inode->i_size > ei->i_disksize) {
		ei->i_disksize = inode->i_size;
		ret2 = ext3_mark_inode_dirty(handle, inode);
		if (!ret)
			ret = ret2;
	}

	ret2 = ext3_journal_stop(handle);
	if (!ret)
		ret = ret2;
	unlock_page(page);
	page_cache_release(page);

	if (pos + len > inode->i_size)
		ext3_truncate_failed_write(inode);
	return ret ? ret : copied;
}

/*
 * bmap() is special.  It gets used by applications such as lilo and by
 * the swapper to find the on-disk block of a specific piece of data.
 *
 * Naturally, this is dangerous if the block concerned is still in the
 * journal.  If somebody makes a swapfile on an ext3 data-journaling
 * filesystem and enables swap, then they may get a nasty shock when the
 * data getting swapped to that swapfile suddenly gets overwritten by
 * the original zero's written out previously to the journal and
 * awaiting writeback in the kernel's buffer cache.
 *
 * So, if we see any bmap calls here on a modified, data-journaled file,
 * take extra steps to flush any blocks which might be in the cache.
 */
static sector_t ext3_bmap(struct address_space *mapping, sector_t block)
{
	struct inode *inode = mapping->host;
	journal_t *journal;
	int err;

	if (ext3_test_inode_state(inode, EXT3_STATE_JDATA)) {

		ext3_clear_inode_state(inode, EXT3_STATE_JDATA);
		journal = EXT3_JOURNAL(inode);
		journal_lock_updates(journal);
		err = journal_flush(journal);
		journal_unlock_updates(journal);

		if (err)
			return 0;
	}

	return generic_block_bmap(mapping,block,ext3_get_block);
}

static int bget_one(handle_t *handle, struct buffer_head *bh)
{
	get_bh(bh);
	return 0;
}

static int bput_one(handle_t *handle, struct buffer_head *bh)
{
	put_bh(bh);
	return 0;
}

static int buffer_unmapped(handle_t *handle, struct buffer_head *bh)
{
	return !buffer_mapped(bh);
}

static int ext3_ordered_writepage(struct page *page,
				struct writeback_control *wbc)
{
	struct inode *inode = page->mapping->host;
	struct buffer_head *page_bufs;
	handle_t *handle = NULL;
	int ret = 0;
	int err;

	J_ASSERT(PageLocked(page));
	WARN_ON_ONCE(IS_RDONLY(inode) &&
		     !(EXT3_SB(inode->i_sb)->s_mount_state & EXT3_ERROR_FS));

	if (ext3_journal_current_handle())
		goto out_fail;

	trace_ext3_ordered_writepage(page);
	if (!page_has_buffers(page)) {
		create_empty_buffers(page, inode->i_sb->s_blocksize,
				(1 << BH_Dirty)|(1 << BH_Uptodate));
		page_bufs = page_buffers(page);
	} else {
		page_bufs = page_buffers(page);
		if (!walk_page_buffers(NULL, page_bufs, 0, PAGE_CACHE_SIZE,
				       NULL, buffer_unmapped)) {
			return block_write_full_page(page, NULL, wbc);
		}
	}
	handle = ext3_journal_start(inode, ext3_writepage_trans_blocks(inode));

	if (IS_ERR(handle)) {
		ret = PTR_ERR(handle);
		goto out_fail;
	}

	walk_page_buffers(handle, page_bufs, 0,
			PAGE_CACHE_SIZE, NULL, bget_one);

	ret = block_write_full_page(page, ext3_get_block, wbc);


	if (ret == 0) {
		err = walk_page_buffers(handle, page_bufs, 0, PAGE_CACHE_SIZE,
					NULL, journal_dirty_data_fn);
		if (!ret)
			ret = err;
	}
	walk_page_buffers(handle, page_bufs, 0,
			PAGE_CACHE_SIZE, NULL, bput_one);
	err = ext3_journal_stop(handle);
	if (!ret)
		ret = err;
	return ret;

out_fail:
	redirty_page_for_writepage(wbc, page);
	unlock_page(page);
	return ret;
}

static int ext3_writeback_writepage(struct page *page,
				struct writeback_control *wbc)
{
	struct inode *inode = page->mapping->host;
	handle_t *handle = NULL;
	int ret = 0;
	int err;

	J_ASSERT(PageLocked(page));
	WARN_ON_ONCE(IS_RDONLY(inode) &&
		     !(EXT3_SB(inode->i_sb)->s_mount_state & EXT3_ERROR_FS));

	if (ext3_journal_current_handle())
		goto out_fail;

	trace_ext3_writeback_writepage(page);
	if (page_has_buffers(page)) {
		if (!walk_page_buffers(NULL, page_buffers(page), 0,
				      PAGE_CACHE_SIZE, NULL, buffer_unmapped)) {
			return block_write_full_page(page, NULL, wbc);
		}
	}

	handle = ext3_journal_start(inode, ext3_writepage_trans_blocks(inode));
	if (IS_ERR(handle)) {
		ret = PTR_ERR(handle);
		goto out_fail;
	}

	ret = block_write_full_page(page, ext3_get_block, wbc);

	err = ext3_journal_stop(handle);
	if (!ret)
		ret = err;
	return ret;

out_fail:
	redirty_page_for_writepage(wbc, page);
	unlock_page(page);
	return ret;
}

static int ext3_journalled_writepage(struct page *page,
				struct writeback_control *wbc)
{
	struct inode *inode = page->mapping->host;
	handle_t *handle = NULL;
	int ret = 0;
	int err;

	J_ASSERT(PageLocked(page));
	WARN_ON_ONCE(IS_RDONLY(inode) &&
		     !(EXT3_SB(inode->i_sb)->s_mount_state & EXT3_ERROR_FS));

	if (ext3_journal_current_handle())
		goto no_write;

	trace_ext3_journalled_writepage(page);
	handle = ext3_journal_start(inode, ext3_writepage_trans_blocks(inode));
	if (IS_ERR(handle)) {
		ret = PTR_ERR(handle);
		goto no_write;
	}

	if (!page_has_buffers(page) || PageChecked(page)) {
		ClearPageChecked(page);
		ret = __block_write_begin(page, 0, PAGE_CACHE_SIZE,
					  ext3_get_block);
		if (ret != 0) {
			ext3_journal_stop(handle);
			goto out_unlock;
		}
		ret = walk_page_buffers(handle, page_buffers(page), 0,
			PAGE_CACHE_SIZE, NULL, do_journal_get_write_access);

		err = walk_page_buffers(handle, page_buffers(page), 0,
				PAGE_CACHE_SIZE, NULL, write_end_fn);
		if (ret == 0)
			ret = err;
		ext3_set_inode_state(inode, EXT3_STATE_JDATA);
		atomic_set(&EXT3_I(inode)->i_datasync_tid,
			   handle->h_transaction->t_tid);
		unlock_page(page);
	} else {
		ret = block_write_full_page(page, ext3_get_block, wbc);
	}
	err = ext3_journal_stop(handle);
	if (!ret)
		ret = err;
out:
	return ret;

no_write:
	redirty_page_for_writepage(wbc, page);
out_unlock:
	unlock_page(page);
	goto out;
}

static int ext3_readpage(struct file *file, struct page *page)
{
	trace_ext3_readpage(page);
	return mpage_readpage(page, ext3_get_block);
}

static int
ext3_readpages(struct file *file, struct address_space *mapping,
		struct list_head *pages, unsigned nr_pages)
{
	return mpage_readpages(mapping, pages, nr_pages, ext3_get_block);
}

static void ext3_invalidatepage(struct page *page, unsigned long offset)
{
	journal_t *journal = EXT3_JOURNAL(page->mapping->host);

	trace_ext3_invalidatepage(page, offset);

	if (offset == 0)
		ClearPageChecked(page);

	journal_invalidatepage(journal, page, offset);
}

static int ext3_releasepage(struct page *page, gfp_t wait)
{
	journal_t *journal = EXT3_JOURNAL(page->mapping->host);

	trace_ext3_releasepage(page);
	WARN_ON(PageChecked(page));
	if (!page_has_buffers(page))
		return 0;
	return journal_try_to_free_buffers(journal, page, wait);
}

static ssize_t ext3_direct_IO(int rw, struct kiocb *iocb,
			const struct iovec *iov, loff_t offset,
			unsigned long nr_segs)
{
	struct file *file = iocb->ki_filp;
	struct inode *inode = file->f_mapping->host;
	struct ext3_inode_info *ei = EXT3_I(inode);
	handle_t *handle;
	ssize_t ret;
	int orphan = 0;
	size_t count = iov_length(iov, nr_segs);
	int retries = 0;

	trace_ext3_direct_IO_enter(inode, offset, iov_length(iov, nr_segs), rw);

	if (rw == WRITE) {
		loff_t final_size = offset + count;

		if (final_size > inode->i_size) {
			
			handle = ext3_journal_start(inode, 2);
			if (IS_ERR(handle)) {
				ret = PTR_ERR(handle);
				goto out;
			}
			ret = ext3_orphan_add(handle, inode);
			if (ret) {
				ext3_journal_stop(handle);
				goto out;
			}
			orphan = 1;
			ei->i_disksize = inode->i_size;
			ext3_journal_stop(handle);
		}
	}

retry:
	ret = blockdev_direct_IO(rw, iocb, inode, iov, offset, nr_segs,
				 ext3_get_block);
	if (unlikely((rw & WRITE) && ret < 0)) {
		loff_t isize = i_size_read(inode);
		loff_t end = offset + iov_length(iov, nr_segs);

		if (end > isize)
			ext3_truncate_failed_direct_write(inode);
	}
	if (ret == -ENOSPC && ext3_should_retry_alloc(inode->i_sb, &retries))
		goto retry;

	if (orphan) {
		int err;

		
		handle = ext3_journal_start(inode, 2);
		if (IS_ERR(handle)) {
			/* This is really bad luck. We've written the data
			 * but cannot extend i_size. Truncate allocated blocks
			 * and pretend the write failed... */
			ext3_truncate_failed_direct_write(inode);
			ret = PTR_ERR(handle);
			goto out;
		}
		if (inode->i_nlink)
			ext3_orphan_del(handle, inode);
		if (ret > 0) {
			loff_t end = offset + ret;
			if (end > inode->i_size) {
				ei->i_disksize = end;
				i_size_write(inode, end);
				ext3_mark_inode_dirty(handle, inode);
			}
		}
		err = ext3_journal_stop(handle);
		if (ret == 0)
			ret = err;
	}
out:
	trace_ext3_direct_IO_exit(inode, offset,
				iov_length(iov, nr_segs), rw, ret);
	return ret;
}

static int ext3_journalled_set_page_dirty(struct page *page)
{
	SetPageChecked(page);
	return __set_page_dirty_nobuffers(page);
}

static const struct address_space_operations ext3_ordered_aops = {
	.readpage		= ext3_readpage,
	.readpages		= ext3_readpages,
	.writepage		= ext3_ordered_writepage,
	.write_begin		= ext3_write_begin,
	.write_end		= ext3_ordered_write_end,
	.bmap			= ext3_bmap,
	.invalidatepage		= ext3_invalidatepage,
	.releasepage		= ext3_releasepage,
	.direct_IO		= ext3_direct_IO,
	.migratepage		= buffer_migrate_page,
	.is_partially_uptodate  = block_is_partially_uptodate,
	.error_remove_page	= generic_error_remove_page,
};

static const struct address_space_operations ext3_writeback_aops = {
	.readpage		= ext3_readpage,
	.readpages		= ext3_readpages,
	.writepage		= ext3_writeback_writepage,
	.write_begin		= ext3_write_begin,
	.write_end		= ext3_writeback_write_end,
	.bmap			= ext3_bmap,
	.invalidatepage		= ext3_invalidatepage,
	.releasepage		= ext3_releasepage,
	.direct_IO		= ext3_direct_IO,
	.migratepage		= buffer_migrate_page,
	.is_partially_uptodate  = block_is_partially_uptodate,
	.error_remove_page	= generic_error_remove_page,
};

static const struct address_space_operations ext3_journalled_aops = {
	.readpage		= ext3_readpage,
	.readpages		= ext3_readpages,
	.writepage		= ext3_journalled_writepage,
	.write_begin		= ext3_write_begin,
	.write_end		= ext3_journalled_write_end,
	.set_page_dirty		= ext3_journalled_set_page_dirty,
	.bmap			= ext3_bmap,
	.invalidatepage		= ext3_invalidatepage,
	.releasepage		= ext3_releasepage,
	.is_partially_uptodate  = block_is_partially_uptodate,
	.error_remove_page	= generic_error_remove_page,
};

void ext3_set_aops(struct inode *inode)
{
	if (ext3_should_order_data(inode))
		inode->i_mapping->a_ops = &ext3_ordered_aops;
	else if (ext3_should_writeback_data(inode))
		inode->i_mapping->a_ops = &ext3_writeback_aops;
	else
		inode->i_mapping->a_ops = &ext3_journalled_aops;
}

static int ext3_block_truncate_page(struct inode *inode, loff_t from)
{
	ext3_fsblk_t index = from >> PAGE_CACHE_SHIFT;
	unsigned offset = from & (PAGE_CACHE_SIZE - 1);
	unsigned blocksize, iblock, length, pos;
	struct page *page;
	handle_t *handle = NULL;
	struct buffer_head *bh;
	int err = 0;

	
	blocksize = inode->i_sb->s_blocksize;
	if ((from & (blocksize - 1)) == 0)
		return 0;

	page = grab_cache_page(inode->i_mapping, index);
	if (!page)
		return -ENOMEM;
	length = blocksize - (offset & (blocksize - 1));
	iblock = index << (PAGE_CACHE_SHIFT - inode->i_sb->s_blocksize_bits);

	if (!page_has_buffers(page))
		create_empty_buffers(page, blocksize, 0);

	
	bh = page_buffers(page);
	pos = blocksize;
	while (offset >= pos) {
		bh = bh->b_this_page;
		iblock++;
		pos += blocksize;
	}

	err = 0;
	if (buffer_freed(bh)) {
		BUFFER_TRACE(bh, "freed: skip");
		goto unlock;
	}

	if (!buffer_mapped(bh)) {
		BUFFER_TRACE(bh, "unmapped");
		ext3_get_block(inode, iblock, bh, 0);
		
		if (!buffer_mapped(bh)) {
			BUFFER_TRACE(bh, "still unmapped");
			goto unlock;
		}
	}

	
	if (PageUptodate(page))
		set_buffer_uptodate(bh);

	if (!bh_uptodate_or_lock(bh)) {
		err = bh_submit_read(bh);
		
		if (err)
			goto unlock;
	}

	
	if (!ext3_should_writeback_data(inode)) {
		
		handle = ext3_journal_start(inode, 1);
		if (IS_ERR(handle)) {
			clear_highpage(page);
			flush_dcache_page(page);
			err = PTR_ERR(handle);
			goto unlock;
		}
	}

	if (ext3_should_journal_data(inode)) {
		BUFFER_TRACE(bh, "get write access");
		err = ext3_journal_get_write_access(handle, bh);
		if (err)
			goto stop;
	}

	zero_user(page, offset, length);
	BUFFER_TRACE(bh, "zeroed end of block");

	err = 0;
	if (ext3_should_journal_data(inode)) {
		err = ext3_journal_dirty_metadata(handle, bh);
	} else {
		if (ext3_should_order_data(inode))
			err = ext3_journal_dirty_data(handle, bh);
		mark_buffer_dirty(bh);
	}
stop:
	if (handle)
		ext3_journal_stop(handle);

unlock:
	unlock_page(page);
	page_cache_release(page);
	return err;
}

static inline int all_zeroes(__le32 *p, __le32 *q)
{
	while (p < q)
		if (*p++)
			return 0;
	return 1;
}


static Indirect *ext3_find_shared(struct inode *inode, int depth,
			int offsets[4], Indirect chain[4], __le32 *top)
{
	Indirect *partial, *p;
	int k, err;

	*top = 0;
	
	for (k = depth; k > 1 && !offsets[k-1]; k--)
		;
	partial = ext3_get_branch(inode, k, offsets, chain, &err);
	
	if (!partial)
		partial = chain + k-1;
	if (!partial->key && *partial->p)
		
		goto no_top;
	for (p=partial; p>chain && all_zeroes((__le32*)p->bh->b_data,p->p); p--)
		;
	if (p == chain + k - 1 && p > chain) {
		p->p--;
	} else {
		*top = *p->p;
		
#if 0
		*p->p = 0;
#endif
	}
	

	while(partial > p) {
		brelse(partial->bh);
		partial--;
	}
no_top:
	return partial;
}

static void ext3_clear_blocks(handle_t *handle, struct inode *inode,
		struct buffer_head *bh, ext3_fsblk_t block_to_free,
		unsigned long count, __le32 *first, __le32 *last)
{
	__le32 *p;
	if (try_to_extend_transaction(handle, inode)) {
		if (bh) {
			BUFFER_TRACE(bh, "call ext3_journal_dirty_metadata");
			if (ext3_journal_dirty_metadata(handle, bh))
				return;
		}
		ext3_mark_inode_dirty(handle, inode);
		truncate_restart_transaction(handle, inode);
		if (bh) {
			BUFFER_TRACE(bh, "retaking write access");
			if (ext3_journal_get_write_access(handle, bh))
				return;
		}
	}

	for (p = first; p < last; p++) {
		u32 nr = le32_to_cpu(*p);
		if (nr) {
			struct buffer_head *bh;

			*p = 0;
			bh = sb_find_get_block(inode->i_sb, nr);
			ext3_forget(handle, 0, inode, bh, nr);
		}
	}

	ext3_free_blocks(handle, inode, block_to_free, count);
}

static void ext3_free_data(handle_t *handle, struct inode *inode,
			   struct buffer_head *this_bh,
			   __le32 *first, __le32 *last)
{
	ext3_fsblk_t block_to_free = 0;    
	unsigned long count = 0;	    
	__le32 *block_to_free_p = NULL;	    
	ext3_fsblk_t nr;		    
	__le32 *p;			    
	int err;

	if (this_bh) {				
		BUFFER_TRACE(this_bh, "get_write_access");
		err = ext3_journal_get_write_access(handle, this_bh);
		if (err)
			return;
	}

	for (p = first; p < last; p++) {
		nr = le32_to_cpu(*p);
		if (nr) {
			
			if (count == 0) {
				block_to_free = nr;
				block_to_free_p = p;
				count = 1;
			} else if (nr == block_to_free + count) {
				count++;
			} else {
				ext3_clear_blocks(handle, inode, this_bh,
						  block_to_free,
						  count, block_to_free_p, p);
				block_to_free = nr;
				block_to_free_p = p;
				count = 1;
			}
		}
	}

	if (count > 0)
		ext3_clear_blocks(handle, inode, this_bh, block_to_free,
				  count, block_to_free_p, p);

	if (this_bh) {
		BUFFER_TRACE(this_bh, "call ext3_journal_dirty_metadata");

		if (bh2jh(this_bh))
			ext3_journal_dirty_metadata(handle, this_bh);
		else
			ext3_error(inode->i_sb, "ext3_free_data",
				   "circular indirect block detected, "
				   "inode=%lu, block=%llu",
				   inode->i_ino,
				   (unsigned long long)this_bh->b_blocknr);
	}
}

static void ext3_free_branches(handle_t *handle, struct inode *inode,
			       struct buffer_head *parent_bh,
			       __le32 *first, __le32 *last, int depth)
{
	ext3_fsblk_t nr;
	__le32 *p;

	if (is_handle_aborted(handle))
		return;

	if (depth--) {
		struct buffer_head *bh;
		int addr_per_block = EXT3_ADDR_PER_BLOCK(inode->i_sb);
		p = last;
		while (--p >= first) {
			nr = le32_to_cpu(*p);
			if (!nr)
				continue;		

			
			bh = sb_bread(inode->i_sb, nr);

			if (!bh) {
				ext3_error(inode->i_sb, "ext3_free_branches",
					   "Read failure, inode=%lu, block="E3FSBLK,
					   inode->i_ino, nr);
				continue;
			}

			
			BUFFER_TRACE(bh, "free child branches");
			ext3_free_branches(handle, inode, bh,
					   (__le32*)bh->b_data,
					   (__le32*)bh->b_data + addr_per_block,
					   depth);

			if (is_handle_aborted(handle))
				return;
			if (try_to_extend_transaction(handle, inode)) {
				ext3_mark_inode_dirty(handle, inode);
				truncate_restart_transaction(handle, inode);
			}

			ext3_forget(handle, 1, inode, bh, bh->b_blocknr);

			ext3_free_blocks(handle, inode, nr, 1);

			if (parent_bh) {
				BUFFER_TRACE(parent_bh, "get_write_access");
				if (!ext3_journal_get_write_access(handle,
								   parent_bh)){
					*p = 0;
					BUFFER_TRACE(parent_bh,
					"call ext3_journal_dirty_metadata");
					ext3_journal_dirty_metadata(handle,
								    parent_bh);
				}
			}
		}
	} else {
		
		BUFFER_TRACE(parent_bh, "free data blocks");
		ext3_free_data(handle, inode, parent_bh, first, last);
	}
}

int ext3_can_truncate(struct inode *inode)
{
	if (S_ISREG(inode->i_mode))
		return 1;
	if (S_ISDIR(inode->i_mode))
		return 1;
	if (S_ISLNK(inode->i_mode))
		return !ext3_inode_is_fast_symlink(inode);
	return 0;
}

void ext3_truncate(struct inode *inode)
{
	handle_t *handle;
	struct ext3_inode_info *ei = EXT3_I(inode);
	__le32 *i_data = ei->i_data;
	int addr_per_block = EXT3_ADDR_PER_BLOCK(inode->i_sb);
	int offsets[4];
	Indirect chain[4];
	Indirect *partial;
	__le32 nr = 0;
	int n;
	long last_block;
	unsigned blocksize = inode->i_sb->s_blocksize;

	trace_ext3_truncate_enter(inode);

	if (!ext3_can_truncate(inode))
		goto out_notrans;

	if (inode->i_size == 0 && ext3_should_writeback_data(inode))
		ext3_set_inode_state(inode, EXT3_STATE_FLUSH_ON_CLOSE);

	handle = start_transaction(inode);
	if (IS_ERR(handle))
		goto out_notrans;

	last_block = (inode->i_size + blocksize-1)
					>> EXT3_BLOCK_SIZE_BITS(inode->i_sb);
	n = ext3_block_to_path(inode, last_block, offsets, NULL);
	if (n == 0)
		goto out_stop;	

	if (ext3_orphan_add(handle, inode))
		goto out_stop;

	ei->i_disksize = inode->i_size;

	mutex_lock(&ei->truncate_mutex);

	if (n == 1) {		
		ext3_free_data(handle, inode, NULL, i_data+offsets[0],
			       i_data + EXT3_NDIR_BLOCKS);
		goto do_indirects;
	}

	partial = ext3_find_shared(inode, n, offsets, chain, &nr);
	
	if (nr) {
		if (partial == chain) {
			
			ext3_free_branches(handle, inode, NULL,
					   &nr, &nr+1, (chain+n-1) - partial);
			*partial->p = 0;
		} else {
			
			ext3_free_branches(handle, inode, partial->bh,
					partial->p,
					partial->p+1, (chain+n-1) - partial);
		}
	}
	
	while (partial > chain) {
		ext3_free_branches(handle, inode, partial->bh, partial->p + 1,
				   (__le32*)partial->bh->b_data+addr_per_block,
				   (chain+n-1) - partial);
		BUFFER_TRACE(partial->bh, "call brelse");
		brelse (partial->bh);
		partial--;
	}
do_indirects:
	
	switch (offsets[0]) {
	default:
		nr = i_data[EXT3_IND_BLOCK];
		if (nr) {
			ext3_free_branches(handle, inode, NULL, &nr, &nr+1, 1);
			i_data[EXT3_IND_BLOCK] = 0;
		}
	case EXT3_IND_BLOCK:
		nr = i_data[EXT3_DIND_BLOCK];
		if (nr) {
			ext3_free_branches(handle, inode, NULL, &nr, &nr+1, 2);
			i_data[EXT3_DIND_BLOCK] = 0;
		}
	case EXT3_DIND_BLOCK:
		nr = i_data[EXT3_TIND_BLOCK];
		if (nr) {
			ext3_free_branches(handle, inode, NULL, &nr, &nr+1, 3);
			i_data[EXT3_TIND_BLOCK] = 0;
		}
	case EXT3_TIND_BLOCK:
		;
	}

	ext3_discard_reservation(inode);

	mutex_unlock(&ei->truncate_mutex);
	inode->i_mtime = inode->i_ctime = CURRENT_TIME_SEC;
	ext3_mark_inode_dirty(handle, inode);

	if (IS_SYNC(inode))
		handle->h_sync = 1;
out_stop:
	if (inode->i_nlink)
		ext3_orphan_del(handle, inode);

	ext3_journal_stop(handle);
	trace_ext3_truncate_exit(inode);
	return;
out_notrans:
	if (inode->i_nlink)
		ext3_orphan_del(NULL, inode);
	trace_ext3_truncate_exit(inode);
}

static ext3_fsblk_t ext3_get_inode_block(struct super_block *sb,
		unsigned long ino, struct ext3_iloc *iloc)
{
	unsigned long block_group;
	unsigned long offset;
	ext3_fsblk_t block;
	struct ext3_group_desc *gdp;

	if (!ext3_valid_inum(sb, ino)) {
		return 0;
	}

	block_group = (ino - 1) / EXT3_INODES_PER_GROUP(sb);
	gdp = ext3_get_group_desc(sb, block_group, NULL);
	if (!gdp)
		return 0;
	offset = ((ino - 1) % EXT3_INODES_PER_GROUP(sb)) *
		EXT3_INODE_SIZE(sb);
	block = le32_to_cpu(gdp->bg_inode_table) +
		(offset >> EXT3_BLOCK_SIZE_BITS(sb));

	iloc->block_group = block_group;
	iloc->offset = offset & (EXT3_BLOCK_SIZE(sb) - 1);
	return block;
}

static int __ext3_get_inode_loc(struct inode *inode,
				struct ext3_iloc *iloc, int in_mem)
{
	ext3_fsblk_t block;
	struct buffer_head *bh;

	block = ext3_get_inode_block(inode->i_sb, inode->i_ino, iloc);
	if (!block)
		return -EIO;

	bh = sb_getblk(inode->i_sb, block);
	if (!bh) {
		ext3_error (inode->i_sb, "ext3_get_inode_loc",
				"unable to read inode block - "
				"inode=%lu, block="E3FSBLK,
				 inode->i_ino, block);
		return -EIO;
	}
	if (!buffer_uptodate(bh)) {
		lock_buffer(bh);

		if (buffer_write_io_error(bh) && !buffer_uptodate(bh))
			set_buffer_uptodate(bh);

		if (buffer_uptodate(bh)) {
			
			unlock_buffer(bh);
			goto has_buffer;
		}

		if (in_mem) {
			struct buffer_head *bitmap_bh;
			struct ext3_group_desc *desc;
			int inodes_per_buffer;
			int inode_offset, i;
			int block_group;
			int start;

			block_group = (inode->i_ino - 1) /
					EXT3_INODES_PER_GROUP(inode->i_sb);
			inodes_per_buffer = bh->b_size /
				EXT3_INODE_SIZE(inode->i_sb);
			inode_offset = ((inode->i_ino - 1) %
					EXT3_INODES_PER_GROUP(inode->i_sb));
			start = inode_offset & ~(inodes_per_buffer - 1);

			
			desc = ext3_get_group_desc(inode->i_sb,
						block_group, NULL);
			if (!desc)
				goto make_io;

			bitmap_bh = sb_getblk(inode->i_sb,
					le32_to_cpu(desc->bg_inode_bitmap));
			if (!bitmap_bh)
				goto make_io;

			if (!buffer_uptodate(bitmap_bh)) {
				brelse(bitmap_bh);
				goto make_io;
			}
			for (i = start; i < start + inodes_per_buffer; i++) {
				if (i == inode_offset)
					continue;
				if (ext3_test_bit(i, bitmap_bh->b_data))
					break;
			}
			brelse(bitmap_bh);
			if (i == start + inodes_per_buffer) {
				
				memset(bh->b_data, 0, bh->b_size);
				set_buffer_uptodate(bh);
				unlock_buffer(bh);
				goto has_buffer;
			}
		}

make_io:
		trace_ext3_load_inode(inode);
		get_bh(bh);
		bh->b_end_io = end_buffer_read_sync;
		submit_bh(READ | REQ_META | REQ_PRIO, bh);
		wait_on_buffer(bh);
		if (!buffer_uptodate(bh)) {
			ext3_error(inode->i_sb, "ext3_get_inode_loc",
					"unable to read inode block - "
					"inode=%lu, block="E3FSBLK,
					inode->i_ino, block);
			brelse(bh);
			return -EIO;
		}
	}
has_buffer:
	iloc->bh = bh;
	return 0;
}

int ext3_get_inode_loc(struct inode *inode, struct ext3_iloc *iloc)
{
	
	return __ext3_get_inode_loc(inode, iloc,
		!ext3_test_inode_state(inode, EXT3_STATE_XATTR));
}

void ext3_set_inode_flags(struct inode *inode)
{
	unsigned int flags = EXT3_I(inode)->i_flags;

	inode->i_flags &= ~(S_SYNC|S_APPEND|S_IMMUTABLE|S_NOATIME|S_DIRSYNC);
	if (flags & EXT3_SYNC_FL)
		inode->i_flags |= S_SYNC;
	if (flags & EXT3_APPEND_FL)
		inode->i_flags |= S_APPEND;
	if (flags & EXT3_IMMUTABLE_FL)
		inode->i_flags |= S_IMMUTABLE;
	if (flags & EXT3_NOATIME_FL)
		inode->i_flags |= S_NOATIME;
	if (flags & EXT3_DIRSYNC_FL)
		inode->i_flags |= S_DIRSYNC;
}

void ext3_get_inode_flags(struct ext3_inode_info *ei)
{
	unsigned int flags = ei->vfs_inode.i_flags;

	ei->i_flags &= ~(EXT3_SYNC_FL|EXT3_APPEND_FL|
			EXT3_IMMUTABLE_FL|EXT3_NOATIME_FL|EXT3_DIRSYNC_FL);
	if (flags & S_SYNC)
		ei->i_flags |= EXT3_SYNC_FL;
	if (flags & S_APPEND)
		ei->i_flags |= EXT3_APPEND_FL;
	if (flags & S_IMMUTABLE)
		ei->i_flags |= EXT3_IMMUTABLE_FL;
	if (flags & S_NOATIME)
		ei->i_flags |= EXT3_NOATIME_FL;
	if (flags & S_DIRSYNC)
		ei->i_flags |= EXT3_DIRSYNC_FL;
}

struct inode *ext3_iget(struct super_block *sb, unsigned long ino)
{
	struct ext3_iloc iloc;
	struct ext3_inode *raw_inode;
	struct ext3_inode_info *ei;
	struct buffer_head *bh;
	struct inode *inode;
	journal_t *journal = EXT3_SB(sb)->s_journal;
	transaction_t *transaction;
	long ret;
	int block;

	inode = iget_locked(sb, ino);
	if (!inode)
		return ERR_PTR(-ENOMEM);
	if (!(inode->i_state & I_NEW))
		return inode;

	ei = EXT3_I(inode);
	ei->i_block_alloc_info = NULL;

	ret = __ext3_get_inode_loc(inode, &iloc, 0);
	if (ret < 0)
		goto bad_inode;
	bh = iloc.bh;
	raw_inode = ext3_raw_inode(&iloc);
	inode->i_mode = le16_to_cpu(raw_inode->i_mode);
	inode->i_uid = (uid_t)le16_to_cpu(raw_inode->i_uid_low);
	inode->i_gid = (gid_t)le16_to_cpu(raw_inode->i_gid_low);
	if(!(test_opt (inode->i_sb, NO_UID32))) {
		inode->i_uid |= le16_to_cpu(raw_inode->i_uid_high) << 16;
		inode->i_gid |= le16_to_cpu(raw_inode->i_gid_high) << 16;
	}
	set_nlink(inode, le16_to_cpu(raw_inode->i_links_count));
	inode->i_size = le32_to_cpu(raw_inode->i_size);
	inode->i_atime.tv_sec = (signed)le32_to_cpu(raw_inode->i_atime);
	inode->i_ctime.tv_sec = (signed)le32_to_cpu(raw_inode->i_ctime);
	inode->i_mtime.tv_sec = (signed)le32_to_cpu(raw_inode->i_mtime);
	inode->i_atime.tv_nsec = inode->i_ctime.tv_nsec = inode->i_mtime.tv_nsec = 0;

	ei->i_state_flags = 0;
	ei->i_dir_start_lookup = 0;
	ei->i_dtime = le32_to_cpu(raw_inode->i_dtime);
	if (inode->i_nlink == 0) {
		if (inode->i_mode == 0 ||
		    !(EXT3_SB(inode->i_sb)->s_mount_state & EXT3_ORPHAN_FS)) {
			
			brelse (bh);
			ret = -ESTALE;
			goto bad_inode;
		}
	}
	inode->i_blocks = le32_to_cpu(raw_inode->i_blocks);
	ei->i_flags = le32_to_cpu(raw_inode->i_flags);
#ifdef EXT3_FRAGMENTS
	ei->i_faddr = le32_to_cpu(raw_inode->i_faddr);
	ei->i_frag_no = raw_inode->i_frag;
	ei->i_frag_size = raw_inode->i_fsize;
#endif
	ei->i_file_acl = le32_to_cpu(raw_inode->i_file_acl);
	if (!S_ISREG(inode->i_mode)) {
		ei->i_dir_acl = le32_to_cpu(raw_inode->i_dir_acl);
	} else {
		inode->i_size |=
			((__u64)le32_to_cpu(raw_inode->i_size_high)) << 32;
	}
	ei->i_disksize = inode->i_size;
	inode->i_generation = le32_to_cpu(raw_inode->i_generation);
	ei->i_block_group = iloc.block_group;
	for (block = 0; block < EXT3_N_BLOCKS; block++)
		ei->i_data[block] = raw_inode->i_block[block];
	INIT_LIST_HEAD(&ei->i_orphan);

	if (journal) {
		tid_t tid;

		spin_lock(&journal->j_state_lock);
		if (journal->j_running_transaction)
			transaction = journal->j_running_transaction;
		else
			transaction = journal->j_committing_transaction;
		if (transaction)
			tid = transaction->t_tid;
		else
			tid = journal->j_commit_sequence;
		spin_unlock(&journal->j_state_lock);
		atomic_set(&ei->i_sync_tid, tid);
		atomic_set(&ei->i_datasync_tid, tid);
	}

	if (inode->i_ino >= EXT3_FIRST_INO(inode->i_sb) + 1 &&
	    EXT3_INODE_SIZE(inode->i_sb) > EXT3_GOOD_OLD_INODE_SIZE) {
		ei->i_extra_isize = le16_to_cpu(raw_inode->i_extra_isize);
		if (EXT3_GOOD_OLD_INODE_SIZE + ei->i_extra_isize >
		    EXT3_INODE_SIZE(inode->i_sb)) {
			brelse (bh);
			ret = -EIO;
			goto bad_inode;
		}
		if (ei->i_extra_isize == 0) {
			
			ei->i_extra_isize = sizeof(struct ext3_inode) -
					    EXT3_GOOD_OLD_INODE_SIZE;
		} else {
			__le32 *magic = (void *)raw_inode +
					EXT3_GOOD_OLD_INODE_SIZE +
					ei->i_extra_isize;
			if (*magic == cpu_to_le32(EXT3_XATTR_MAGIC))
				 ext3_set_inode_state(inode, EXT3_STATE_XATTR);
		}
	} else
		ei->i_extra_isize = 0;

	if (S_ISREG(inode->i_mode)) {
		inode->i_op = &ext3_file_inode_operations;
		inode->i_fop = &ext3_file_operations;
		ext3_set_aops(inode);
	} else if (S_ISDIR(inode->i_mode)) {
		inode->i_op = &ext3_dir_inode_operations;
		inode->i_fop = &ext3_dir_operations;
	} else if (S_ISLNK(inode->i_mode)) {
		if (ext3_inode_is_fast_symlink(inode)) {
			inode->i_op = &ext3_fast_symlink_inode_operations;
			nd_terminate_link(ei->i_data, inode->i_size,
				sizeof(ei->i_data) - 1);
		} else {
			inode->i_op = &ext3_symlink_inode_operations;
			ext3_set_aops(inode);
		}
	} else {
		inode->i_op = &ext3_special_inode_operations;
		if (raw_inode->i_block[0])
			init_special_inode(inode, inode->i_mode,
			   old_decode_dev(le32_to_cpu(raw_inode->i_block[0])));
		else
			init_special_inode(inode, inode->i_mode,
			   new_decode_dev(le32_to_cpu(raw_inode->i_block[1])));
	}
	brelse (iloc.bh);
	ext3_set_inode_flags(inode);
	unlock_new_inode(inode);
	return inode;

bad_inode:
	iget_failed(inode);
	return ERR_PTR(ret);
}

static int ext3_do_update_inode(handle_t *handle,
				struct inode *inode,
				struct ext3_iloc *iloc)
{
	struct ext3_inode *raw_inode = ext3_raw_inode(iloc);
	struct ext3_inode_info *ei = EXT3_I(inode);
	struct buffer_head *bh = iloc->bh;
	int err = 0, rc, block;

again:
	
	lock_buffer(bh);

	if (ext3_test_inode_state(inode, EXT3_STATE_NEW))
		memset(raw_inode, 0, EXT3_SB(inode->i_sb)->s_inode_size);

	ext3_get_inode_flags(ei);
	raw_inode->i_mode = cpu_to_le16(inode->i_mode);
	if(!(test_opt(inode->i_sb, NO_UID32))) {
		raw_inode->i_uid_low = cpu_to_le16(low_16_bits(inode->i_uid));
		raw_inode->i_gid_low = cpu_to_le16(low_16_bits(inode->i_gid));
		if(!ei->i_dtime) {
			raw_inode->i_uid_high =
				cpu_to_le16(high_16_bits(inode->i_uid));
			raw_inode->i_gid_high =
				cpu_to_le16(high_16_bits(inode->i_gid));
		} else {
			raw_inode->i_uid_high = 0;
			raw_inode->i_gid_high = 0;
		}
	} else {
		raw_inode->i_uid_low =
			cpu_to_le16(fs_high2lowuid(inode->i_uid));
		raw_inode->i_gid_low =
			cpu_to_le16(fs_high2lowgid(inode->i_gid));
		raw_inode->i_uid_high = 0;
		raw_inode->i_gid_high = 0;
	}
	raw_inode->i_links_count = cpu_to_le16(inode->i_nlink);
	raw_inode->i_size = cpu_to_le32(ei->i_disksize);
	raw_inode->i_atime = cpu_to_le32(inode->i_atime.tv_sec);
	raw_inode->i_ctime = cpu_to_le32(inode->i_ctime.tv_sec);
	raw_inode->i_mtime = cpu_to_le32(inode->i_mtime.tv_sec);
	raw_inode->i_blocks = cpu_to_le32(inode->i_blocks);
	raw_inode->i_dtime = cpu_to_le32(ei->i_dtime);
	raw_inode->i_flags = cpu_to_le32(ei->i_flags);
#ifdef EXT3_FRAGMENTS
	raw_inode->i_faddr = cpu_to_le32(ei->i_faddr);
	raw_inode->i_frag = ei->i_frag_no;
	raw_inode->i_fsize = ei->i_frag_size;
#endif
	raw_inode->i_file_acl = cpu_to_le32(ei->i_file_acl);
	if (!S_ISREG(inode->i_mode)) {
		raw_inode->i_dir_acl = cpu_to_le32(ei->i_dir_acl);
	} else {
		raw_inode->i_size_high =
			cpu_to_le32(ei->i_disksize >> 32);
		if (ei->i_disksize > 0x7fffffffULL) {
			struct super_block *sb = inode->i_sb;
			if (!EXT3_HAS_RO_COMPAT_FEATURE(sb,
					EXT3_FEATURE_RO_COMPAT_LARGE_FILE) ||
			    EXT3_SB(sb)->s_es->s_rev_level ==
					cpu_to_le32(EXT3_GOOD_OLD_REV)) {
				unlock_buffer(bh);
				err = ext3_journal_get_write_access(handle,
						EXT3_SB(sb)->s_sbh);
				if (err)
					goto out_brelse;

				ext3_update_dynamic_rev(sb);
				EXT3_SET_RO_COMPAT_FEATURE(sb,
					EXT3_FEATURE_RO_COMPAT_LARGE_FILE);
				handle->h_sync = 1;
				err = ext3_journal_dirty_metadata(handle,
						EXT3_SB(sb)->s_sbh);
				
				goto again;
			}
		}
	}
	raw_inode->i_generation = cpu_to_le32(inode->i_generation);
	if (S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode)) {
		if (old_valid_dev(inode->i_rdev)) {
			raw_inode->i_block[0] =
				cpu_to_le32(old_encode_dev(inode->i_rdev));
			raw_inode->i_block[1] = 0;
		} else {
			raw_inode->i_block[0] = 0;
			raw_inode->i_block[1] =
				cpu_to_le32(new_encode_dev(inode->i_rdev));
			raw_inode->i_block[2] = 0;
		}
	} else for (block = 0; block < EXT3_N_BLOCKS; block++)
		raw_inode->i_block[block] = ei->i_data[block];

	if (ei->i_extra_isize)
		raw_inode->i_extra_isize = cpu_to_le16(ei->i_extra_isize);

	BUFFER_TRACE(bh, "call ext3_journal_dirty_metadata");
	unlock_buffer(bh);
	rc = ext3_journal_dirty_metadata(handle, bh);
	if (!err)
		err = rc;
	ext3_clear_inode_state(inode, EXT3_STATE_NEW);

	atomic_set(&ei->i_sync_tid, handle->h_transaction->t_tid);
out_brelse:
	brelse (bh);
	ext3_std_error(inode->i_sb, err);
	return err;
}

int ext3_write_inode(struct inode *inode, struct writeback_control *wbc)
{
	if (current->flags & PF_MEMALLOC)
		return 0;

	if (ext3_journal_current_handle()) {
		jbd_debug(1, "called recursively, non-PF_MEMALLOC!\n");
		dump_stack();
		return -EIO;
	}

	if (wbc->sync_mode != WB_SYNC_ALL)
		return 0;

	return ext3_force_commit(inode->i_sb);
}

int ext3_setattr(struct dentry *dentry, struct iattr *attr)
{
	struct inode *inode = dentry->d_inode;
	int error, rc = 0;
	const unsigned int ia_valid = attr->ia_valid;

	error = inode_change_ok(inode, attr);
	if (error)
		return error;

	if (is_quota_modification(inode, attr))
		dquot_initialize(inode);
	if ((ia_valid & ATTR_UID && attr->ia_uid != inode->i_uid) ||
		(ia_valid & ATTR_GID && attr->ia_gid != inode->i_gid)) {
		handle_t *handle;

		handle = ext3_journal_start(inode, EXT3_MAXQUOTAS_INIT_BLOCKS(inode->i_sb)+
					EXT3_MAXQUOTAS_DEL_BLOCKS(inode->i_sb)+3);
		if (IS_ERR(handle)) {
			error = PTR_ERR(handle);
			goto err_out;
		}
		error = dquot_transfer(inode, attr);
		if (error) {
			ext3_journal_stop(handle);
			return error;
		}
		if (attr->ia_valid & ATTR_UID)
			inode->i_uid = attr->ia_uid;
		if (attr->ia_valid & ATTR_GID)
			inode->i_gid = attr->ia_gid;
		error = ext3_mark_inode_dirty(handle, inode);
		ext3_journal_stop(handle);
	}

	if (attr->ia_valid & ATTR_SIZE)
		inode_dio_wait(inode);

	if (S_ISREG(inode->i_mode) &&
	    attr->ia_valid & ATTR_SIZE && attr->ia_size < inode->i_size) {
		handle_t *handle;

		handle = ext3_journal_start(inode, 3);
		if (IS_ERR(handle)) {
			error = PTR_ERR(handle);
			goto err_out;
		}

		error = ext3_orphan_add(handle, inode);
		if (error) {
			ext3_journal_stop(handle);
			goto err_out;
		}
		EXT3_I(inode)->i_disksize = attr->ia_size;
		error = ext3_mark_inode_dirty(handle, inode);
		ext3_journal_stop(handle);
		if (error) {
			
			ext3_orphan_del(NULL, inode);
			goto err_out;
		}
		rc = ext3_block_truncate_page(inode, attr->ia_size);
		if (rc) {
			
			handle = ext3_journal_start(inode, 3);
			if (IS_ERR(handle)) {
				ext3_orphan_del(NULL, inode);
				goto err_out;
			}
			ext3_orphan_del(handle, inode);
			ext3_journal_stop(handle);
			goto err_out;
		}
	}

	if ((attr->ia_valid & ATTR_SIZE) &&
	    attr->ia_size != i_size_read(inode)) {
		truncate_setsize(inode, attr->ia_size);
		ext3_truncate(inode);
	}

	setattr_copy(inode, attr);
	mark_inode_dirty(inode);

	if (ia_valid & ATTR_MODE)
		rc = ext3_acl_chmod(inode);

err_out:
	ext3_std_error(inode->i_sb, error);
	if (!error)
		error = rc;
	return error;
}



static int ext3_writepage_trans_blocks(struct inode *inode)
{
	int bpp = ext3_journal_blocks_per_page(inode);
	int indirects = (EXT3_NDIR_BLOCKS % bpp) ? 5 : 3;
	int ret;

	if (ext3_should_journal_data(inode))
		ret = 3 * (bpp + indirects) + 2;
	else
		ret = 2 * (bpp + indirects) + indirects + 2;

#ifdef CONFIG_QUOTA
	ret += EXT3_MAXQUOTAS_TRANS_BLOCKS(inode->i_sb);
#endif

	return ret;
}

int ext3_mark_iloc_dirty(handle_t *handle,
		struct inode *inode, struct ext3_iloc *iloc)
{
	int err = 0;

	
	get_bh(iloc->bh);

	
	err = ext3_do_update_inode(handle, inode, iloc);
	put_bh(iloc->bh);
	return err;
}


int
ext3_reserve_inode_write(handle_t *handle, struct inode *inode,
			 struct ext3_iloc *iloc)
{
	int err = 0;
	if (handle) {
		err = ext3_get_inode_loc(inode, iloc);
		if (!err) {
			BUFFER_TRACE(iloc->bh, "get_write_access");
			err = ext3_journal_get_write_access(handle, iloc->bh);
			if (err) {
				brelse(iloc->bh);
				iloc->bh = NULL;
			}
		}
	}
	ext3_std_error(inode->i_sb, err);
	return err;
}

/*
 * What we do here is to mark the in-core inode as clean with respect to inode
 * dirtiness (it may still be data-dirty).
 * This means that the in-core inode may be reaped by prune_icache
 * without having to perform any I/O.  This is a very good thing,
 * because *any* task may call prune_icache - even ones which
 * have a transaction open against a different journal.
 *
 * Is this cheating?  Not really.  Sure, we haven't written the
 * inode out, but prune_icache isn't a user-visible syncing function.
 * Whenever the user wants stuff synced (sys_sync, sys_msync, sys_fsync)
 * we start and wait on commits.
 *
 * Is this efficient/effective?  Well, we're being nice to the system
 * by cleaning up our inodes proactively so they can be reaped
 * without I/O.  But we are potentially leaving up to five seconds'
 * worth of inodes floating about which prune_icache wants us to
 * write out.  One way to fix that would be to get prune_icache()
 * to do a write_super() to free up some memory.  It has the desired
 * effect.
 */
int ext3_mark_inode_dirty(handle_t *handle, struct inode *inode)
{
	struct ext3_iloc iloc;
	int err;

	might_sleep();
	trace_ext3_mark_inode_dirty(inode, _RET_IP_);
	err = ext3_reserve_inode_write(handle, inode, &iloc);
	if (!err)
		err = ext3_mark_iloc_dirty(handle, inode, &iloc);
	return err;
}

void ext3_dirty_inode(struct inode *inode, int flags)
{
	handle_t *current_handle = ext3_journal_current_handle();
	handle_t *handle;

	handle = ext3_journal_start(inode, 2);
	if (IS_ERR(handle))
		goto out;
	if (current_handle &&
		current_handle->h_transaction != handle->h_transaction) {
		
		printk(KERN_EMERG "%s: transactions do not match!\n",
		       __func__);
	} else {
		jbd_debug(5, "marking dirty.  outer handle=%p\n",
				current_handle);
		ext3_mark_inode_dirty(handle, inode);
	}
	ext3_journal_stop(handle);
out:
	return;
}

#if 0
static int ext3_pin_inode(handle_t *handle, struct inode *inode)
{
	struct ext3_iloc iloc;

	int err = 0;
	if (handle) {
		err = ext3_get_inode_loc(inode, &iloc);
		if (!err) {
			BUFFER_TRACE(iloc.bh, "get_write_access");
			err = journal_get_write_access(handle, iloc.bh);
			if (!err)
				err = ext3_journal_dirty_metadata(handle,
								  iloc.bh);
			brelse(iloc.bh);
		}
	}
	ext3_std_error(inode->i_sb, err);
	return err;
}
#endif

int ext3_change_inode_journal_flag(struct inode *inode, int val)
{
	journal_t *journal;
	handle_t *handle;
	int err;


	journal = EXT3_JOURNAL(inode);
	if (is_journal_aborted(journal))
		return -EROFS;

	journal_lock_updates(journal);
	journal_flush(journal);


	if (val)
		EXT3_I(inode)->i_flags |= EXT3_JOURNAL_DATA_FL;
	else
		EXT3_I(inode)->i_flags &= ~EXT3_JOURNAL_DATA_FL;
	ext3_set_aops(inode);

	journal_unlock_updates(journal);

	

	handle = ext3_journal_start(inode, 1);
	if (IS_ERR(handle))
		return PTR_ERR(handle);

	err = ext3_mark_inode_dirty(handle, inode);
	handle->h_sync = 1;
	ext3_journal_stop(handle);
	ext3_std_error(inode->i_sb, err);

	return err;
}
