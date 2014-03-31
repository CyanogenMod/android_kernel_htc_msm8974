/*
 *  linux/fs/ext3/balloc.c
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 *  Enhanced block allocation by Stephen Tweedie (sct@redhat.com), 1993
 *  Big-endian to little-endian byte-swapping/bitmaps by
 *        David S. Miller (davem@caip.rutgers.edu), 1995
 */

#include <linux/quotaops.h>
#include <linux/blkdev.h>
#include "ext3.h"




#define in_range(b, first, len)	((b) >= (first) && (b) <= (first) + (len) - 1)

static void ext3_get_group_no_and_offset(struct super_block *sb,
	ext3_fsblk_t blocknr, unsigned long *blockgrpp, ext3_grpblk_t *offsetp)
{
	struct ext3_super_block *es = EXT3_SB(sb)->s_es;

	blocknr = blocknr - le32_to_cpu(es->s_first_data_block);
	if (offsetp)
		*offsetp = blocknr % EXT3_BLOCKS_PER_GROUP(sb);
	if (blockgrpp)
		*blockgrpp = blocknr / EXT3_BLOCKS_PER_GROUP(sb);
}

struct ext3_group_desc * ext3_get_group_desc(struct super_block * sb,
					     unsigned int block_group,
					     struct buffer_head ** bh)
{
	unsigned long group_desc;
	unsigned long offset;
	struct ext3_group_desc * desc;
	struct ext3_sb_info *sbi = EXT3_SB(sb);

	if (block_group >= sbi->s_groups_count) {
		ext3_error (sb, "ext3_get_group_desc",
			    "block_group >= groups_count - "
			    "block_group = %d, groups_count = %lu",
			    block_group, sbi->s_groups_count);

		return NULL;
	}
	smp_rmb();

	group_desc = block_group >> EXT3_DESC_PER_BLOCK_BITS(sb);
	offset = block_group & (EXT3_DESC_PER_BLOCK(sb) - 1);
	if (!sbi->s_group_desc[group_desc]) {
		ext3_error (sb, "ext3_get_group_desc",
			    "Group descriptor not loaded - "
			    "block_group = %d, group_desc = %lu, desc = %lu",
			     block_group, group_desc, offset);
		return NULL;
	}

	desc = (struct ext3_group_desc *) sbi->s_group_desc[group_desc]->b_data;
	if (bh)
		*bh = sbi->s_group_desc[group_desc];
	return desc + offset;
}

static int ext3_valid_block_bitmap(struct super_block *sb,
					struct ext3_group_desc *desc,
					unsigned int block_group,
					struct buffer_head *bh)
{
	ext3_grpblk_t offset;
	ext3_grpblk_t next_zero_bit;
	ext3_fsblk_t bitmap_blk;
	ext3_fsblk_t group_first_block;

	group_first_block = ext3_group_first_block_no(sb, block_group);

	
	bitmap_blk = le32_to_cpu(desc->bg_block_bitmap);
	offset = bitmap_blk - group_first_block;
	if (!ext3_test_bit(offset, bh->b_data))
		
		goto err_out;

	
	bitmap_blk = le32_to_cpu(desc->bg_inode_bitmap);
	offset = bitmap_blk - group_first_block;
	if (!ext3_test_bit(offset, bh->b_data))
		
		goto err_out;

	
	bitmap_blk = le32_to_cpu(desc->bg_inode_table);
	offset = bitmap_blk - group_first_block;
	next_zero_bit = ext3_find_next_zero_bit(bh->b_data,
				offset + EXT3_SB(sb)->s_itb_per_group,
				offset);
	if (next_zero_bit >= offset + EXT3_SB(sb)->s_itb_per_group)
		
		return 1;

err_out:
	ext3_error(sb, __func__,
			"Invalid block bitmap - "
			"block_group = %d, block = %lu",
			block_group, bitmap_blk);
	return 0;
}

static struct buffer_head *
read_block_bitmap(struct super_block *sb, unsigned int block_group)
{
	struct ext3_group_desc * desc;
	struct buffer_head * bh = NULL;
	ext3_fsblk_t bitmap_blk;

	desc = ext3_get_group_desc(sb, block_group, NULL);
	if (!desc)
		return NULL;
	trace_ext3_read_block_bitmap(sb, block_group);
	bitmap_blk = le32_to_cpu(desc->bg_block_bitmap);
	bh = sb_getblk(sb, bitmap_blk);
	if (unlikely(!bh)) {
		ext3_error(sb, __func__,
			    "Cannot read block bitmap - "
			    "block_group = %d, block_bitmap = %u",
			    block_group, le32_to_cpu(desc->bg_block_bitmap));
		return NULL;
	}
	if (likely(bh_uptodate_or_lock(bh)))
		return bh;

	if (bh_submit_read(bh) < 0) {
		brelse(bh);
		ext3_error(sb, __func__,
			    "Cannot read block bitmap - "
			    "block_group = %d, block_bitmap = %u",
			    block_group, le32_to_cpu(desc->bg_block_bitmap));
		return NULL;
	}
	ext3_valid_block_bitmap(sb, desc, block_group, bh);
	return bh;
}

#if 1
static void __rsv_window_dump(struct rb_root *root, int verbose,
			      const char *fn)
{
	struct rb_node *n;
	struct ext3_reserve_window_node *rsv, *prev;
	int bad;

restart:
	n = rb_first(root);
	bad = 0;
	prev = NULL;

	printk("Block Allocation Reservation Windows Map (%s):\n", fn);
	while (n) {
		rsv = rb_entry(n, struct ext3_reserve_window_node, rsv_node);
		if (verbose)
			printk("reservation window 0x%p "
			       "start:  %lu, end:  %lu\n",
			       rsv, rsv->rsv_start, rsv->rsv_end);
		if (rsv->rsv_start && rsv->rsv_start >= rsv->rsv_end) {
			printk("Bad reservation %p (start >= end)\n",
			       rsv);
			bad = 1;
		}
		if (prev && prev->rsv_end >= rsv->rsv_start) {
			printk("Bad reservation %p (prev->end >= start)\n",
			       rsv);
			bad = 1;
		}
		if (bad) {
			if (!verbose) {
				printk("Restarting reservation walk in verbose mode\n");
				verbose = 1;
				goto restart;
			}
		}
		n = rb_next(n);
		prev = rsv;
	}
	printk("Window map complete.\n");
	BUG_ON(bad);
}
#define rsv_window_dump(root, verbose) \
	__rsv_window_dump((root), (verbose), __func__)
#else
#define rsv_window_dump(root, verbose) do {} while (0)
#endif

static int
goal_in_my_reservation(struct ext3_reserve_window *rsv, ext3_grpblk_t grp_goal,
			unsigned int group, struct super_block * sb)
{
	ext3_fsblk_t group_first_block, group_last_block;

	group_first_block = ext3_group_first_block_no(sb, group);
	group_last_block = group_first_block + (EXT3_BLOCKS_PER_GROUP(sb) - 1);

	if ((rsv->_rsv_start > group_last_block) ||
	    (rsv->_rsv_end < group_first_block))
		return 0;
	if ((grp_goal >= 0) && ((grp_goal + group_first_block < rsv->_rsv_start)
		|| (grp_goal + group_first_block > rsv->_rsv_end)))
		return 0;
	return 1;
}

static struct ext3_reserve_window_node *
search_reserve_window(struct rb_root *root, ext3_fsblk_t goal)
{
	struct rb_node *n = root->rb_node;
	struct ext3_reserve_window_node *rsv;

	if (!n)
		return NULL;

	do {
		rsv = rb_entry(n, struct ext3_reserve_window_node, rsv_node);

		if (goal < rsv->rsv_start)
			n = n->rb_left;
		else if (goal > rsv->rsv_end)
			n = n->rb_right;
		else
			return rsv;
	} while (n);
	if (rsv->rsv_start > goal) {
		n = rb_prev(&rsv->rsv_node);
		rsv = rb_entry(n, struct ext3_reserve_window_node, rsv_node);
	}
	return rsv;
}

void ext3_rsv_window_add(struct super_block *sb,
		    struct ext3_reserve_window_node *rsv)
{
	struct rb_root *root = &EXT3_SB(sb)->s_rsv_window_root;
	struct rb_node *node = &rsv->rsv_node;
	ext3_fsblk_t start = rsv->rsv_start;

	struct rb_node ** p = &root->rb_node;
	struct rb_node * parent = NULL;
	struct ext3_reserve_window_node *this;

	trace_ext3_rsv_window_add(sb, rsv);
	while (*p)
	{
		parent = *p;
		this = rb_entry(parent, struct ext3_reserve_window_node, rsv_node);

		if (start < this->rsv_start)
			p = &(*p)->rb_left;
		else if (start > this->rsv_end)
			p = &(*p)->rb_right;
		else {
			rsv_window_dump(root, 1);
			BUG();
		}
	}

	rb_link_node(node, parent, p);
	rb_insert_color(node, root);
}

static void rsv_window_remove(struct super_block *sb,
			      struct ext3_reserve_window_node *rsv)
{
	rsv->rsv_start = EXT3_RESERVE_WINDOW_NOT_ALLOCATED;
	rsv->rsv_end = EXT3_RESERVE_WINDOW_NOT_ALLOCATED;
	rsv->rsv_alloc_hit = 0;
	rb_erase(&rsv->rsv_node, &EXT3_SB(sb)->s_rsv_window_root);
}

static inline int rsv_is_empty(struct ext3_reserve_window *rsv)
{
	
	return rsv->_rsv_end == EXT3_RESERVE_WINDOW_NOT_ALLOCATED;
}

void ext3_init_block_alloc_info(struct inode *inode)
{
	struct ext3_inode_info *ei = EXT3_I(inode);
	struct ext3_block_alloc_info *block_i;
	struct super_block *sb = inode->i_sb;

	block_i = kmalloc(sizeof(*block_i), GFP_NOFS);
	if (block_i) {
		struct ext3_reserve_window_node *rsv = &block_i->rsv_window_node;

		rsv->rsv_start = EXT3_RESERVE_WINDOW_NOT_ALLOCATED;
		rsv->rsv_end = EXT3_RESERVE_WINDOW_NOT_ALLOCATED;

		if (!test_opt(sb, RESERVATION))
			rsv->rsv_goal_size = 0;
		else
			rsv->rsv_goal_size = EXT3_DEFAULT_RESERVE_BLOCKS;
		rsv->rsv_alloc_hit = 0;
		block_i->last_alloc_logical_block = 0;
		block_i->last_alloc_physical_block = 0;
	}
	ei->i_block_alloc_info = block_i;
}

void ext3_discard_reservation(struct inode *inode)
{
	struct ext3_inode_info *ei = EXT3_I(inode);
	struct ext3_block_alloc_info *block_i = ei->i_block_alloc_info;
	struct ext3_reserve_window_node *rsv;
	spinlock_t *rsv_lock = &EXT3_SB(inode->i_sb)->s_rsv_window_lock;

	if (!block_i)
		return;

	rsv = &block_i->rsv_window_node;
	if (!rsv_is_empty(&rsv->rsv_window)) {
		spin_lock(rsv_lock);
		if (!rsv_is_empty(&rsv->rsv_window)) {
			trace_ext3_discard_reservation(inode, rsv);
			rsv_window_remove(inode->i_sb, rsv);
		}
		spin_unlock(rsv_lock);
	}
}

void ext3_free_blocks_sb(handle_t *handle, struct super_block *sb,
			 ext3_fsblk_t block, unsigned long count,
			 unsigned long *pdquot_freed_blocks)
{
	struct buffer_head *bitmap_bh = NULL;
	struct buffer_head *gd_bh;
	unsigned long block_group;
	ext3_grpblk_t bit;
	unsigned long i;
	unsigned long overflow;
	struct ext3_group_desc * desc;
	struct ext3_super_block * es;
	struct ext3_sb_info *sbi;
	int err = 0, ret;
	ext3_grpblk_t group_freed;

	*pdquot_freed_blocks = 0;
	sbi = EXT3_SB(sb);
	es = sbi->s_es;
	if (block < le32_to_cpu(es->s_first_data_block) ||
	    block + count < block ||
	    block + count > le32_to_cpu(es->s_blocks_count)) {
		ext3_error (sb, "ext3_free_blocks",
			    "Freeing blocks not in datazone - "
			    "block = "E3FSBLK", count = %lu", block, count);
		goto error_return;
	}

	ext3_debug ("freeing block(s) %lu-%lu\n", block, block + count - 1);

do_more:
	overflow = 0;
	block_group = (block - le32_to_cpu(es->s_first_data_block)) /
		      EXT3_BLOCKS_PER_GROUP(sb);
	bit = (block - le32_to_cpu(es->s_first_data_block)) %
		      EXT3_BLOCKS_PER_GROUP(sb);
	if (bit + count > EXT3_BLOCKS_PER_GROUP(sb)) {
		overflow = bit + count - EXT3_BLOCKS_PER_GROUP(sb);
		count -= overflow;
	}
	brelse(bitmap_bh);
	bitmap_bh = read_block_bitmap(sb, block_group);
	if (!bitmap_bh)
		goto error_return;
	desc = ext3_get_group_desc (sb, block_group, &gd_bh);
	if (!desc)
		goto error_return;

	if (in_range (le32_to_cpu(desc->bg_block_bitmap), block, count) ||
	    in_range (le32_to_cpu(desc->bg_inode_bitmap), block, count) ||
	    in_range (block, le32_to_cpu(desc->bg_inode_table),
		      sbi->s_itb_per_group) ||
	    in_range (block + count - 1, le32_to_cpu(desc->bg_inode_table),
		      sbi->s_itb_per_group)) {
		ext3_error (sb, "ext3_free_blocks",
			    "Freeing blocks in system zones - "
			    "Block = "E3FSBLK", count = %lu",
			    block, count);
		goto error_return;
	}

	
	BUFFER_TRACE(bitmap_bh, "getting undo access");
	err = ext3_journal_get_undo_access(handle, bitmap_bh);
	if (err)
		goto error_return;

	BUFFER_TRACE(gd_bh, "get_write_access");
	err = ext3_journal_get_write_access(handle, gd_bh);
	if (err)
		goto error_return;

	jbd_lock_bh_state(bitmap_bh);

	for (i = 0, group_freed = 0; i < count; i++) {
#ifdef CONFIG_JBD_DEBUG
		jbd_unlock_bh_state(bitmap_bh);
		{
			struct buffer_head *debug_bh;
			debug_bh = sb_find_get_block(sb, block + i);
			if (debug_bh) {
				BUFFER_TRACE(debug_bh, "Deleted!");
				if (!bh2jh(bitmap_bh)->b_committed_data)
					BUFFER_TRACE(debug_bh,
						"No committed data in bitmap");
				BUFFER_TRACE2(debug_bh, bitmap_bh, "bitmap");
				__brelse(debug_bh);
			}
		}
		jbd_lock_bh_state(bitmap_bh);
#endif
		if (need_resched()) {
			jbd_unlock_bh_state(bitmap_bh);
			cond_resched();
			jbd_lock_bh_state(bitmap_bh);
		}
		BUFFER_TRACE(bitmap_bh, "set in b_committed_data");
		J_ASSERT_BH(bitmap_bh,
				bh2jh(bitmap_bh)->b_committed_data != NULL);
		ext3_set_bit_atomic(sb_bgl_lock(sbi, block_group), bit + i,
				bh2jh(bitmap_bh)->b_committed_data);

		BUFFER_TRACE(bitmap_bh, "clear bit");
		if (!ext3_clear_bit_atomic(sb_bgl_lock(sbi, block_group),
						bit + i, bitmap_bh->b_data)) {
			jbd_unlock_bh_state(bitmap_bh);
			ext3_error(sb, __func__,
				"bit already cleared for block "E3FSBLK,
				 block + i);
			jbd_lock_bh_state(bitmap_bh);
			BUFFER_TRACE(bitmap_bh, "bit already cleared");
		} else {
			group_freed++;
		}
	}
	jbd_unlock_bh_state(bitmap_bh);

	spin_lock(sb_bgl_lock(sbi, block_group));
	le16_add_cpu(&desc->bg_free_blocks_count, group_freed);
	spin_unlock(sb_bgl_lock(sbi, block_group));
	percpu_counter_add(&sbi->s_freeblocks_counter, count);

	
	BUFFER_TRACE(bitmap_bh, "dirtied bitmap block");
	err = ext3_journal_dirty_metadata(handle, bitmap_bh);

	
	BUFFER_TRACE(gd_bh, "dirtied group descriptor block");
	ret = ext3_journal_dirty_metadata(handle, gd_bh);
	if (!err) err = ret;
	*pdquot_freed_blocks += group_freed;

	if (overflow && !err) {
		block += count;
		count = overflow;
		goto do_more;
	}

error_return:
	brelse(bitmap_bh);
	ext3_std_error(sb, err);
	return;
}

void ext3_free_blocks(handle_t *handle, struct inode *inode,
			ext3_fsblk_t block, unsigned long count)
{
	struct super_block *sb = inode->i_sb;
	unsigned long dquot_freed_blocks;

	trace_ext3_free_blocks(inode, block, count);
	ext3_free_blocks_sb(handle, sb, block, count, &dquot_freed_blocks);
	if (dquot_freed_blocks)
		dquot_free_block(inode, dquot_freed_blocks);
	return;
}

/**
 * ext3_test_allocatable()
 * @nr:			given allocation block group
 * @bh:			bufferhead contains the bitmap of the given block group
 *
 * For ext3 allocations, we must not reuse any blocks which are
 * allocated in the bitmap buffer's "last committed data" copy.  This
 * prevents deletes from freeing up the page for reuse until we have
 * committed the delete transaction.
 *
 * If we didn't do this, then deleting something and reallocating it as
 * data would allow the old block to be overwritten before the
 * transaction committed (because we force data to disk before commit).
 * This would lead to corruption if we crashed between overwriting the
 * data and committing the delete.
 *
 * @@@ We may want to make this allocation behaviour conditional on
 * data-writes at some point, and disable it for metadata allocations or
 * sync-data inodes.
 */
static int ext3_test_allocatable(ext3_grpblk_t nr, struct buffer_head *bh)
{
	int ret;
	struct journal_head *jh = bh2jh(bh);

	if (ext3_test_bit(nr, bh->b_data))
		return 0;

	jbd_lock_bh_state(bh);
	if (!jh->b_committed_data)
		ret = 1;
	else
		ret = !ext3_test_bit(nr, jh->b_committed_data);
	jbd_unlock_bh_state(bh);
	return ret;
}

static ext3_grpblk_t
bitmap_search_next_usable_block(ext3_grpblk_t start, struct buffer_head *bh,
					ext3_grpblk_t maxblocks)
{
	ext3_grpblk_t next;
	struct journal_head *jh = bh2jh(bh);

	while (start < maxblocks) {
		next = ext3_find_next_zero_bit(bh->b_data, maxblocks, start);
		if (next >= maxblocks)
			return -1;
		if (ext3_test_allocatable(next, bh))
			return next;
		jbd_lock_bh_state(bh);
		if (jh->b_committed_data)
			start = ext3_find_next_zero_bit(jh->b_committed_data,
							maxblocks, next);
		jbd_unlock_bh_state(bh);
	}
	return -1;
}

static ext3_grpblk_t
find_next_usable_block(ext3_grpblk_t start, struct buffer_head *bh,
			ext3_grpblk_t maxblocks)
{
	ext3_grpblk_t here, next;
	char *p, *r;

	if (start > 0) {
		ext3_grpblk_t end_goal = (start + 63) & ~63;
		if (end_goal > maxblocks)
			end_goal = maxblocks;
		here = ext3_find_next_zero_bit(bh->b_data, end_goal, start);
		if (here < end_goal && ext3_test_allocatable(here, bh))
			return here;
		ext3_debug("Bit not found near goal\n");
	}

	here = start;
	if (here < 0)
		here = 0;

	p = bh->b_data + (here >> 3);
	r = memscan(p, 0, ((maxblocks + 7) >> 3) - (here >> 3));
	next = (r - bh->b_data) << 3;

	if (next < maxblocks && next >= start && ext3_test_allocatable(next, bh))
		return next;

	here = bitmap_search_next_usable_block(here, bh, maxblocks);
	return here;
}

static inline int
claim_block(spinlock_t *lock, ext3_grpblk_t block, struct buffer_head *bh)
{
	struct journal_head *jh = bh2jh(bh);
	int ret;

	if (ext3_set_bit_atomic(lock, block, bh->b_data))
		return 0;
	jbd_lock_bh_state(bh);
	if (jh->b_committed_data && ext3_test_bit(block,jh->b_committed_data)) {
		ext3_clear_bit_atomic(lock, block, bh->b_data);
		ret = 0;
	} else {
		ret = 1;
	}
	jbd_unlock_bh_state(bh);
	return ret;
}

static ext3_grpblk_t
ext3_try_to_allocate(struct super_block *sb, handle_t *handle, int group,
			struct buffer_head *bitmap_bh, ext3_grpblk_t grp_goal,
			unsigned long *count, struct ext3_reserve_window *my_rsv)
{
	ext3_fsblk_t group_first_block;
	ext3_grpblk_t start, end;
	unsigned long num = 0;

	
	if (my_rsv) {
		group_first_block = ext3_group_first_block_no(sb, group);
		if (my_rsv->_rsv_start >= group_first_block)
			start = my_rsv->_rsv_start - group_first_block;
		else
			
			start = 0;
		end = my_rsv->_rsv_end - group_first_block + 1;
		if (end > EXT3_BLOCKS_PER_GROUP(sb))
			
			end = EXT3_BLOCKS_PER_GROUP(sb);
		if ((start <= grp_goal) && (grp_goal < end))
			start = grp_goal;
		else
			grp_goal = -1;
	} else {
		if (grp_goal > 0)
			start = grp_goal;
		else
			start = 0;
		end = EXT3_BLOCKS_PER_GROUP(sb);
	}

	BUG_ON(start > EXT3_BLOCKS_PER_GROUP(sb));

repeat:
	if (grp_goal < 0 || !ext3_test_allocatable(grp_goal, bitmap_bh)) {
		grp_goal = find_next_usable_block(start, bitmap_bh, end);
		if (grp_goal < 0)
			goto fail_access;
		if (!my_rsv) {
			int i;

			for (i = 0; i < 7 && grp_goal > start &&
					ext3_test_allocatable(grp_goal - 1,
								bitmap_bh);
					i++, grp_goal--)
				;
		}
	}
	start = grp_goal;

	if (!claim_block(sb_bgl_lock(EXT3_SB(sb), group),
		grp_goal, bitmap_bh)) {
		start++;
		grp_goal++;
		if (start >= end)
			goto fail_access;
		goto repeat;
	}
	num++;
	grp_goal++;
	while (num < *count && grp_goal < end
		&& ext3_test_allocatable(grp_goal, bitmap_bh)
		&& claim_block(sb_bgl_lock(EXT3_SB(sb), group),
				grp_goal, bitmap_bh)) {
		num++;
		grp_goal++;
	}
	*count = num;
	return grp_goal - num;
fail_access:
	*count = num;
	return -1;
}

static int find_next_reservable_window(
				struct ext3_reserve_window_node *search_head,
				struct ext3_reserve_window_node *my_rsv,
				struct super_block * sb,
				ext3_fsblk_t start_block,
				ext3_fsblk_t last_block)
{
	struct rb_node *next;
	struct ext3_reserve_window_node *rsv, *prev;
	ext3_fsblk_t cur;
	int size = my_rsv->rsv_goal_size;

	
	
	cur = start_block;
	rsv = search_head;
	if (!rsv)
		return -1;

	while (1) {
		if (cur <= rsv->rsv_end)
			cur = rsv->rsv_end + 1;

		if (cur > last_block)
			return -1;		

		prev = rsv;
		next = rb_next(&rsv->rsv_node);
		rsv = rb_entry(next,struct ext3_reserve_window_node,rsv_node);

		if (!next)
			break;

		if (cur + size <= rsv->rsv_start) {
			break;
		}
	}

	if ((prev != my_rsv) && (!rsv_is_empty(&my_rsv->rsv_window)))
		rsv_window_remove(sb, my_rsv);

	my_rsv->rsv_start = cur;
	my_rsv->rsv_end = cur + size - 1;
	my_rsv->rsv_alloc_hit = 0;

	if (prev != my_rsv)
		ext3_rsv_window_add(sb, my_rsv);

	return 0;
}

static int alloc_new_reservation(struct ext3_reserve_window_node *my_rsv,
		ext3_grpblk_t grp_goal, struct super_block *sb,
		unsigned int group, struct buffer_head *bitmap_bh)
{
	struct ext3_reserve_window_node *search_head;
	ext3_fsblk_t group_first_block, group_end_block, start_block;
	ext3_grpblk_t first_free_block;
	struct rb_root *fs_rsv_root = &EXT3_SB(sb)->s_rsv_window_root;
	unsigned long size;
	int ret;
	spinlock_t *rsv_lock = &EXT3_SB(sb)->s_rsv_window_lock;

	group_first_block = ext3_group_first_block_no(sb, group);
	group_end_block = group_first_block + (EXT3_BLOCKS_PER_GROUP(sb) - 1);

	if (grp_goal < 0)
		start_block = group_first_block;
	else
		start_block = grp_goal + group_first_block;

	trace_ext3_alloc_new_reservation(sb, start_block);
	size = my_rsv->rsv_goal_size;

	if (!rsv_is_empty(&my_rsv->rsv_window)) {

		if ((my_rsv->rsv_start <= group_end_block) &&
				(my_rsv->rsv_end > group_end_block) &&
				(start_block >= my_rsv->rsv_start))
			return -1;

		if ((my_rsv->rsv_alloc_hit >
		     (my_rsv->rsv_end - my_rsv->rsv_start + 1) / 2)) {
			size = size * 2;
			if (size > EXT3_MAX_RESERVE_BLOCKS)
				size = EXT3_MAX_RESERVE_BLOCKS;
			my_rsv->rsv_goal_size= size;
		}
	}

	spin_lock(rsv_lock);
	search_head = search_reserve_window(fs_rsv_root, start_block);

retry:
	ret = find_next_reservable_window(search_head, my_rsv, sb,
						start_block, group_end_block);

	if (ret == -1) {
		if (!rsv_is_empty(&my_rsv->rsv_window))
			rsv_window_remove(sb, my_rsv);
		spin_unlock(rsv_lock);
		return -1;
	}

	spin_unlock(rsv_lock);
	first_free_block = bitmap_search_next_usable_block(
			my_rsv->rsv_start - group_first_block,
			bitmap_bh, group_end_block - group_first_block + 1);

	if (first_free_block < 0) {
		spin_lock(rsv_lock);
		if (!rsv_is_empty(&my_rsv->rsv_window))
			rsv_window_remove(sb, my_rsv);
		spin_unlock(rsv_lock);
		return -1;		
	}

	start_block = first_free_block + group_first_block;
	if (start_block >= my_rsv->rsv_start &&
	    start_block <= my_rsv->rsv_end) {
		trace_ext3_reserved(sb, start_block, my_rsv);
		return 0;		
	}
	search_head = my_rsv;
	spin_lock(rsv_lock);
	goto retry;
}

static void try_to_extend_reservation(struct ext3_reserve_window_node *my_rsv,
			struct super_block *sb, int size)
{
	struct ext3_reserve_window_node *next_rsv;
	struct rb_node *next;
	spinlock_t *rsv_lock = &EXT3_SB(sb)->s_rsv_window_lock;

	if (!spin_trylock(rsv_lock))
		return;

	next = rb_next(&my_rsv->rsv_node);

	if (!next)
		my_rsv->rsv_end += size;
	else {
		next_rsv = rb_entry(next, struct ext3_reserve_window_node, rsv_node);

		if ((next_rsv->rsv_start - my_rsv->rsv_end - 1) >= size)
			my_rsv->rsv_end += size;
		else
			my_rsv->rsv_end = next_rsv->rsv_start - 1;
	}
	spin_unlock(rsv_lock);
}

static ext3_grpblk_t
ext3_try_to_allocate_with_rsv(struct super_block *sb, handle_t *handle,
			unsigned int group, struct buffer_head *bitmap_bh,
			ext3_grpblk_t grp_goal,
			struct ext3_reserve_window_node * my_rsv,
			unsigned long *count, int *errp)
{
	ext3_fsblk_t group_first_block, group_last_block;
	ext3_grpblk_t ret = 0;
	int fatal;
	unsigned long num = *count;

	*errp = 0;

	BUFFER_TRACE(bitmap_bh, "get undo access for new block");
	fatal = ext3_journal_get_undo_access(handle, bitmap_bh);
	if (fatal) {
		*errp = fatal;
		return -1;
	}

	if (my_rsv == NULL ) {
		ret = ext3_try_to_allocate(sb, handle, group, bitmap_bh,
						grp_goal, count, NULL);
		goto out;
	}
	group_first_block = ext3_group_first_block_no(sb, group);
	group_last_block = group_first_block + (EXT3_BLOCKS_PER_GROUP(sb) - 1);

	while (1) {
		if (rsv_is_empty(&my_rsv->rsv_window) || (ret < 0) ||
			!goal_in_my_reservation(&my_rsv->rsv_window,
						grp_goal, group, sb)) {
			if (my_rsv->rsv_goal_size < *count)
				my_rsv->rsv_goal_size = *count;
			ret = alloc_new_reservation(my_rsv, grp_goal, sb,
							group, bitmap_bh);
			if (ret < 0)
				break;			

			if (!goal_in_my_reservation(&my_rsv->rsv_window,
							grp_goal, group, sb))
				grp_goal = -1;
		} else if (grp_goal >= 0) {
			int curr = my_rsv->rsv_end -
					(grp_goal + group_first_block) + 1;

			if (curr < *count)
				try_to_extend_reservation(my_rsv, sb,
							*count - curr);
		}

		if ((my_rsv->rsv_start > group_last_block) ||
				(my_rsv->rsv_end < group_first_block)) {
			rsv_window_dump(&EXT3_SB(sb)->s_rsv_window_root, 1);
			BUG();
		}
		ret = ext3_try_to_allocate(sb, handle, group, bitmap_bh,
					   grp_goal, &num, &my_rsv->rsv_window);
		if (ret >= 0) {
			my_rsv->rsv_alloc_hit += num;
			*count = num;
			break;				
		}
		num = *count;
	}
out:
	if (ret >= 0) {
		BUFFER_TRACE(bitmap_bh, "journal_dirty_metadata for "
					"bitmap block");
		fatal = ext3_journal_dirty_metadata(handle, bitmap_bh);
		if (fatal) {
			*errp = fatal;
			return -1;
		}
		return ret;
	}

	BUFFER_TRACE(bitmap_bh, "journal_release_buffer");
	ext3_journal_release_buffer(handle, bitmap_bh);
	return ret;
}

static int ext3_has_free_blocks(struct ext3_sb_info *sbi, int use_reservation)
{
	ext3_fsblk_t free_blocks, root_blocks;

	free_blocks = percpu_counter_read_positive(&sbi->s_freeblocks_counter);
	root_blocks = le32_to_cpu(sbi->s_es->s_r_blocks_count);
	if (free_blocks < root_blocks + 1 && !capable(CAP_SYS_RESOURCE) &&
		!use_reservation && sbi->s_resuid != current_fsuid() &&
		(sbi->s_resgid == 0 || !in_group_p (sbi->s_resgid))) {
		return 0;
	}
	return 1;
}

int ext3_should_retry_alloc(struct super_block *sb, int *retries)
{
	if (!ext3_has_free_blocks(EXT3_SB(sb), 0) || (*retries)++ > 3)
		return 0;

	jbd_debug(1, "%s: retrying operation after ENOSPC\n", sb->s_id);

	return journal_force_commit_nested(EXT3_SB(sb)->s_journal);
}

ext3_fsblk_t ext3_new_blocks(handle_t *handle, struct inode *inode,
			ext3_fsblk_t goal, unsigned long *count, int *errp)
{
	struct buffer_head *bitmap_bh = NULL;
	struct buffer_head *gdp_bh;
	int group_no;
	int goal_group;
	ext3_grpblk_t grp_target_blk;	
	ext3_grpblk_t grp_alloc_blk;	
	ext3_fsblk_t ret_block;		
	int bgi;			
	int fatal = 0, err;
	int performed_allocation = 0;
	ext3_grpblk_t free_blocks;	
	struct super_block *sb;
	struct ext3_group_desc *gdp;
	struct ext3_super_block *es;
	struct ext3_sb_info *sbi;
	struct ext3_reserve_window_node *my_rsv = NULL;
	struct ext3_block_alloc_info *block_i;
	unsigned short windowsz = 0;
#ifdef EXT3FS_DEBUG
	static int goal_hits, goal_attempts;
#endif
	unsigned long ngroups;
	unsigned long num = *count;

	*errp = -ENOSPC;
	sb = inode->i_sb;

	err = dquot_alloc_block(inode, num);
	if (err) {
		*errp = err;
		return 0;
	}

	trace_ext3_request_blocks(inode, goal, num);

	sbi = EXT3_SB(sb);
	es = sbi->s_es;
	ext3_debug("goal=%lu.\n", goal);
	block_i = EXT3_I(inode)->i_block_alloc_info;
	if (block_i && ((windowsz = block_i->rsv_window_node.rsv_goal_size) > 0))
		my_rsv = &block_i->rsv_window_node;

	if (!ext3_has_free_blocks(sbi, IS_NOQUOTA(inode))) {
		*errp = -ENOSPC;
		goto out;
	}

	if (goal < le32_to_cpu(es->s_first_data_block) ||
	    goal >= le32_to_cpu(es->s_blocks_count))
		goal = le32_to_cpu(es->s_first_data_block);
	group_no = (goal - le32_to_cpu(es->s_first_data_block)) /
			EXT3_BLOCKS_PER_GROUP(sb);
	goal_group = group_no;
retry_alloc:
	gdp = ext3_get_group_desc(sb, group_no, &gdp_bh);
	if (!gdp)
		goto io_error;

	free_blocks = le16_to_cpu(gdp->bg_free_blocks_count);
	if (my_rsv && (free_blocks < windowsz)
		&& (free_blocks > 0)
		&& (rsv_is_empty(&my_rsv->rsv_window)))
		my_rsv = NULL;

	if (free_blocks > 0) {
		grp_target_blk = ((goal - le32_to_cpu(es->s_first_data_block)) %
				EXT3_BLOCKS_PER_GROUP(sb));
		bitmap_bh = read_block_bitmap(sb, group_no);
		if (!bitmap_bh)
			goto io_error;
		grp_alloc_blk = ext3_try_to_allocate_with_rsv(sb, handle,
					group_no, bitmap_bh, grp_target_blk,
					my_rsv,	&num, &fatal);
		if (fatal)
			goto out;
		if (grp_alloc_blk >= 0)
			goto allocated;
	}

	ngroups = EXT3_SB(sb)->s_groups_count;
	smp_rmb();

	for (bgi = 0; bgi < ngroups; bgi++) {
		group_no++;
		if (group_no >= ngroups)
			group_no = 0;
		gdp = ext3_get_group_desc(sb, group_no, &gdp_bh);
		if (!gdp)
			goto io_error;
		free_blocks = le16_to_cpu(gdp->bg_free_blocks_count);
		if (!free_blocks)
			continue;
		if (my_rsv && (free_blocks <= (windowsz/2)))
			continue;

		brelse(bitmap_bh);
		bitmap_bh = read_block_bitmap(sb, group_no);
		if (!bitmap_bh)
			goto io_error;
		grp_alloc_blk = ext3_try_to_allocate_with_rsv(sb, handle,
					group_no, bitmap_bh, -1, my_rsv,
					&num, &fatal);
		if (fatal)
			goto out;
		if (grp_alloc_blk >= 0)
			goto allocated;
	}
	if (my_rsv) {
		my_rsv = NULL;
		windowsz = 0;
		group_no = goal_group;
		goto retry_alloc;
	}
	
	*errp = -ENOSPC;
	goto out;

allocated:

	ext3_debug("using block group %d(%d)\n",
			group_no, gdp->bg_free_blocks_count);

	BUFFER_TRACE(gdp_bh, "get_write_access");
	fatal = ext3_journal_get_write_access(handle, gdp_bh);
	if (fatal)
		goto out;

	ret_block = grp_alloc_blk + ext3_group_first_block_no(sb, group_no);

	if (in_range(le32_to_cpu(gdp->bg_block_bitmap), ret_block, num) ||
	    in_range(le32_to_cpu(gdp->bg_inode_bitmap), ret_block, num) ||
	    in_range(ret_block, le32_to_cpu(gdp->bg_inode_table),
		      EXT3_SB(sb)->s_itb_per_group) ||
	    in_range(ret_block + num - 1, le32_to_cpu(gdp->bg_inode_table),
		      EXT3_SB(sb)->s_itb_per_group)) {
		ext3_error(sb, "ext3_new_block",
			    "Allocating block in system zone - "
			    "blocks from "E3FSBLK", length %lu",
			     ret_block, num);
		goto retry_alloc;
	}

	performed_allocation = 1;

#ifdef CONFIG_JBD_DEBUG
	{
		struct buffer_head *debug_bh;

		
		debug_bh = sb_find_get_block(sb, ret_block);
		if (debug_bh) {
			BUFFER_TRACE(debug_bh, "state when allocated");
			BUFFER_TRACE2(debug_bh, bitmap_bh, "bitmap state");
			brelse(debug_bh);
		}
	}
	jbd_lock_bh_state(bitmap_bh);
	spin_lock(sb_bgl_lock(sbi, group_no));
	if (buffer_jbd(bitmap_bh) && bh2jh(bitmap_bh)->b_committed_data) {
		int i;

		for (i = 0; i < num; i++) {
			if (ext3_test_bit(grp_alloc_blk+i,
					bh2jh(bitmap_bh)->b_committed_data)) {
				printk("%s: block was unexpectedly set in "
					"b_committed_data\n", __func__);
			}
		}
	}
	ext3_debug("found bit %d\n", grp_alloc_blk);
	spin_unlock(sb_bgl_lock(sbi, group_no));
	jbd_unlock_bh_state(bitmap_bh);
#endif

	if (ret_block + num - 1 >= le32_to_cpu(es->s_blocks_count)) {
		ext3_error(sb, "ext3_new_block",
			    "block("E3FSBLK") >= blocks count(%d) - "
			    "block_group = %d, es == %p ", ret_block,
			le32_to_cpu(es->s_blocks_count), group_no, es);
		goto out;
	}

	ext3_debug("allocating block %lu. Goal hits %d of %d.\n",
			ret_block, goal_hits, goal_attempts);

	spin_lock(sb_bgl_lock(sbi, group_no));
	le16_add_cpu(&gdp->bg_free_blocks_count, -num);
	spin_unlock(sb_bgl_lock(sbi, group_no));
	percpu_counter_sub(&sbi->s_freeblocks_counter, num);

	BUFFER_TRACE(gdp_bh, "journal_dirty_metadata for group descriptor");
	err = ext3_journal_dirty_metadata(handle, gdp_bh);
	if (!fatal)
		fatal = err;

	if (fatal)
		goto out;

	*errp = 0;
	brelse(bitmap_bh);

	if (num < *count) {
		dquot_free_block(inode, *count-num);
		*count = num;
	}

	trace_ext3_allocate_blocks(inode, goal, num,
				   (unsigned long long)ret_block);

	return ret_block;

io_error:
	*errp = -EIO;
out:
	if (fatal) {
		*errp = fatal;
		ext3_std_error(sb, fatal);
	}
	if (!performed_allocation)
		dquot_free_block(inode, *count);
	brelse(bitmap_bh);
	return 0;
}

ext3_fsblk_t ext3_new_block(handle_t *handle, struct inode *inode,
			ext3_fsblk_t goal, int *errp)
{
	unsigned long count = 1;

	return ext3_new_blocks(handle, inode, goal, &count, errp);
}

ext3_fsblk_t ext3_count_free_blocks(struct super_block *sb)
{
	ext3_fsblk_t desc_count;
	struct ext3_group_desc *gdp;
	int i;
	unsigned long ngroups = EXT3_SB(sb)->s_groups_count;
#ifdef EXT3FS_DEBUG
	struct ext3_super_block *es;
	ext3_fsblk_t bitmap_count;
	unsigned long x;
	struct buffer_head *bitmap_bh = NULL;

	es = EXT3_SB(sb)->s_es;
	desc_count = 0;
	bitmap_count = 0;
	gdp = NULL;

	smp_rmb();
	for (i = 0; i < ngroups; i++) {
		gdp = ext3_get_group_desc(sb, i, NULL);
		if (!gdp)
			continue;
		desc_count += le16_to_cpu(gdp->bg_free_blocks_count);
		brelse(bitmap_bh);
		bitmap_bh = read_block_bitmap(sb, i);
		if (bitmap_bh == NULL)
			continue;

		x = ext3_count_free(bitmap_bh, sb->s_blocksize);
		printk("group %d: stored = %d, counted = %lu\n",
			i, le16_to_cpu(gdp->bg_free_blocks_count), x);
		bitmap_count += x;
	}
	brelse(bitmap_bh);
	printk("ext3_count_free_blocks: stored = "E3FSBLK
		", computed = "E3FSBLK", "E3FSBLK"\n",
	       le32_to_cpu(es->s_free_blocks_count),
		desc_count, bitmap_count);
	return bitmap_count;
#else
	desc_count = 0;
	smp_rmb();
	for (i = 0; i < ngroups; i++) {
		gdp = ext3_get_group_desc(sb, i, NULL);
		if (!gdp)
			continue;
		desc_count += le16_to_cpu(gdp->bg_free_blocks_count);
	}

	return desc_count;
#endif
}

static inline int test_root(int a, int b)
{
	int num = b;

	while (a > num)
		num *= b;
	return num == a;
}

static int ext3_group_sparse(int group)
{
	if (group <= 1)
		return 1;
	if (!(group & 1))
		return 0;
	return (test_root(group, 7) || test_root(group, 5) ||
		test_root(group, 3));
}

int ext3_bg_has_super(struct super_block *sb, int group)
{
	if (EXT3_HAS_RO_COMPAT_FEATURE(sb,
				EXT3_FEATURE_RO_COMPAT_SPARSE_SUPER) &&
			!ext3_group_sparse(group))
		return 0;
	return 1;
}

static unsigned long ext3_bg_num_gdb_meta(struct super_block *sb, int group)
{
	unsigned long metagroup = group / EXT3_DESC_PER_BLOCK(sb);
	unsigned long first = metagroup * EXT3_DESC_PER_BLOCK(sb);
	unsigned long last = first + EXT3_DESC_PER_BLOCK(sb) - 1;

	if (group == first || group == first + 1 || group == last)
		return 1;
	return 0;
}

static unsigned long ext3_bg_num_gdb_nometa(struct super_block *sb, int group)
{
	return ext3_bg_has_super(sb, group) ? EXT3_SB(sb)->s_gdb_count : 0;
}

unsigned long ext3_bg_num_gdb(struct super_block *sb, int group)
{
	unsigned long first_meta_bg =
			le32_to_cpu(EXT3_SB(sb)->s_es->s_first_meta_bg);
	unsigned long metagroup = group / EXT3_DESC_PER_BLOCK(sb);

	if (!EXT3_HAS_INCOMPAT_FEATURE(sb,EXT3_FEATURE_INCOMPAT_META_BG) ||
			metagroup < first_meta_bg)
		return ext3_bg_num_gdb_nometa(sb,group);

	return ext3_bg_num_gdb_meta(sb,group);

}

static ext3_grpblk_t ext3_trim_all_free(struct super_block *sb,
					unsigned int group,
					ext3_grpblk_t start, ext3_grpblk_t max,
					ext3_grpblk_t minblocks)
{
	handle_t *handle;
	ext3_grpblk_t next, free_blocks, bit, freed, count = 0;
	ext3_fsblk_t discard_block;
	struct ext3_sb_info *sbi;
	struct buffer_head *gdp_bh, *bitmap_bh = NULL;
	struct ext3_group_desc *gdp;
	int err = 0, ret = 0;

	handle = ext3_journal_start_sb(sb, 2);
	if (IS_ERR(handle))
		return PTR_ERR(handle);

	bitmap_bh = read_block_bitmap(sb, group);
	if (!bitmap_bh) {
		err = -EIO;
		goto err_out;
	}

	BUFFER_TRACE(bitmap_bh, "getting undo access");
	err = ext3_journal_get_undo_access(handle, bitmap_bh);
	if (err)
		goto err_out;

	gdp = ext3_get_group_desc(sb, group, &gdp_bh);
	if (!gdp) {
		err = -EIO;
		goto err_out;
	}

	BUFFER_TRACE(gdp_bh, "get_write_access");
	err = ext3_journal_get_write_access(handle, gdp_bh);
	if (err)
		goto err_out;

	free_blocks = le16_to_cpu(gdp->bg_free_blocks_count);
	sbi = EXT3_SB(sb);

	 
	while (start <= max) {
		start = bitmap_search_next_usable_block(start, bitmap_bh, max);
		if (start < 0)
			break;
		next = start;

		while (next <= max
			&& claim_block(sb_bgl_lock(sbi, group),
					next, bitmap_bh)) {
			next++;
		}

		 
		if (next == start)
			continue;

		discard_block = (ext3_fsblk_t)start +
				ext3_group_first_block_no(sb, group);

		
		spin_lock(sb_bgl_lock(sbi, group));
		le16_add_cpu(&gdp->bg_free_blocks_count, start - next);
		spin_unlock(sb_bgl_lock(sbi, group));
		percpu_counter_sub(&sbi->s_freeblocks_counter, next - start);

		free_blocks -= next - start;
		
		if ((next - start) < minblocks)
			goto free_extent;

		trace_ext3_discard_blocks(sb, discard_block, next - start);
		 
		err = sb_issue_discard(sb, discard_block, next - start,
				       GFP_NOFS, 0);
		count += (next - start);
free_extent:
		freed = 0;

		for (bit = start; bit < next; bit++) {
			BUFFER_TRACE(bitmap_bh, "clear bit");
			if (!ext3_clear_bit_atomic(sb_bgl_lock(sbi, group),
						bit, bitmap_bh->b_data)) {
				ext3_error(sb, __func__,
					"bit already cleared for block "E3FSBLK,
					 (unsigned long)bit);
				BUFFER_TRACE(bitmap_bh, "bit already cleared");
			} else {
				freed++;
			}
		}

		
		spin_lock(sb_bgl_lock(sbi, group));
		le16_add_cpu(&gdp->bg_free_blocks_count, freed);
		spin_unlock(sb_bgl_lock(sbi, group));
		percpu_counter_add(&sbi->s_freeblocks_counter, freed);

		start = next;
		if (err < 0) {
			if (err != -EOPNOTSUPP)
				ext3_warning(sb, __func__, "Discard command "
					     "returned error %d\n", err);
			break;
		}

		if (fatal_signal_pending(current)) {
			err = -ERESTARTSYS;
			break;
		}

		cond_resched();

		
		if (free_blocks < minblocks)
			break;
	}

	
	BUFFER_TRACE(bitmap_bh, "dirtied bitmap block");
	ret = ext3_journal_dirty_metadata(handle, bitmap_bh);
	if (!err)
		err = ret;

	
	BUFFER_TRACE(gdp_bh, "dirtied group descriptor block");
	ret = ext3_journal_dirty_metadata(handle, gdp_bh);
	if (!err)
		err = ret;

	ext3_debug("trimmed %d blocks in the group %d\n",
		count, group);

err_out:
	if (err)
		count = err;
	ext3_journal_stop(handle);
	brelse(bitmap_bh);

	return count;
}

int ext3_trim_fs(struct super_block *sb, struct fstrim_range *range)
{
	ext3_grpblk_t last_block, first_block;
	unsigned long group, first_group, last_group;
	struct ext3_group_desc *gdp;
	struct ext3_super_block *es = EXT3_SB(sb)->s_es;
	uint64_t start, minlen, end, trimmed = 0;
	ext3_fsblk_t first_data_blk =
			le32_to_cpu(EXT3_SB(sb)->s_es->s_first_data_block);
	ext3_fsblk_t max_blks = le32_to_cpu(es->s_blocks_count);
	int ret = 0;

	start = range->start >> sb->s_blocksize_bits;
	end = start + (range->len >> sb->s_blocksize_bits) - 1;
	minlen = range->minlen >> sb->s_blocksize_bits;

	if (unlikely(minlen > EXT3_BLOCKS_PER_GROUP(sb)) ||
	    unlikely(start >= max_blks))
		return -EINVAL;
	if (end >= max_blks)
		end = max_blks - 1;
	if (end <= first_data_blk)
		goto out;
	if (start < first_data_blk)
		start = first_data_blk;

	smp_rmb();

	
	ext3_get_group_no_and_offset(sb, (ext3_fsblk_t) start,
				     &first_group, &first_block);
	ext3_get_group_no_and_offset(sb, (ext3_fsblk_t) end,
				     &last_group, &last_block);

	
	end = EXT3_BLOCKS_PER_GROUP(sb) - 1;

	for (group = first_group; group <= last_group; group++) {
		gdp = ext3_get_group_desc(sb, group, NULL);
		if (!gdp)
			break;

		if (group == last_group)
			end = last_block;

		if (le16_to_cpu(gdp->bg_free_blocks_count) >= minlen) {
			ret = ext3_trim_all_free(sb, group, first_block,
						 end, minlen);
			if (ret < 0)
				break;
			trimmed += ret;
		}

		first_block = 0;
	}

	if (ret > 0)
		ret = 0;

out:
	range->len = trimmed * sb->s_blocksize;
	return ret;
}
