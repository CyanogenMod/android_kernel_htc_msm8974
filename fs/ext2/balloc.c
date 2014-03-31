/*
 *  linux/fs/ext2/balloc.c
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

#include "ext2.h"
#include <linux/quotaops.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/buffer_head.h>
#include <linux/capability.h>




#define in_range(b, first, len)	((b) >= (first) && (b) <= (first) + (len) - 1)

struct ext2_group_desc * ext2_get_group_desc(struct super_block * sb,
					     unsigned int block_group,
					     struct buffer_head ** bh)
{
	unsigned long group_desc;
	unsigned long offset;
	struct ext2_group_desc * desc;
	struct ext2_sb_info *sbi = EXT2_SB(sb);

	if (block_group >= sbi->s_groups_count) {
		ext2_error (sb, "ext2_get_group_desc",
			    "block_group >= groups_count - "
			    "block_group = %d, groups_count = %lu",
			    block_group, sbi->s_groups_count);

		return NULL;
	}

	group_desc = block_group >> EXT2_DESC_PER_BLOCK_BITS(sb);
	offset = block_group & (EXT2_DESC_PER_BLOCK(sb) - 1);
	if (!sbi->s_group_desc[group_desc]) {
		ext2_error (sb, "ext2_get_group_desc",
			    "Group descriptor not loaded - "
			    "block_group = %d, group_desc = %lu, desc = %lu",
			     block_group, group_desc, offset);
		return NULL;
	}

	desc = (struct ext2_group_desc *) sbi->s_group_desc[group_desc]->b_data;
	if (bh)
		*bh = sbi->s_group_desc[group_desc];
	return desc + offset;
}

static int ext2_valid_block_bitmap(struct super_block *sb,
					struct ext2_group_desc *desc,
					unsigned int block_group,
					struct buffer_head *bh)
{
	ext2_grpblk_t offset;
	ext2_grpblk_t next_zero_bit;
	ext2_fsblk_t bitmap_blk;
	ext2_fsblk_t group_first_block;

	group_first_block = ext2_group_first_block_no(sb, block_group);

	
	bitmap_blk = le32_to_cpu(desc->bg_block_bitmap);
	offset = bitmap_blk - group_first_block;
	if (!ext2_test_bit(offset, bh->b_data))
		
		goto err_out;

	
	bitmap_blk = le32_to_cpu(desc->bg_inode_bitmap);
	offset = bitmap_blk - group_first_block;
	if (!ext2_test_bit(offset, bh->b_data))
		
		goto err_out;

	
	bitmap_blk = le32_to_cpu(desc->bg_inode_table);
	offset = bitmap_blk - group_first_block;
	next_zero_bit = ext2_find_next_zero_bit(bh->b_data,
				offset + EXT2_SB(sb)->s_itb_per_group,
				offset);
	if (next_zero_bit >= offset + EXT2_SB(sb)->s_itb_per_group)
		
		return 1;

err_out:
	ext2_error(sb, __func__,
			"Invalid block bitmap - "
			"block_group = %d, block = %lu",
			block_group, bitmap_blk);
	return 0;
}

static struct buffer_head *
read_block_bitmap(struct super_block *sb, unsigned int block_group)
{
	struct ext2_group_desc * desc;
	struct buffer_head * bh = NULL;
	ext2_fsblk_t bitmap_blk;

	desc = ext2_get_group_desc(sb, block_group, NULL);
	if (!desc)
		return NULL;
	bitmap_blk = le32_to_cpu(desc->bg_block_bitmap);
	bh = sb_getblk(sb, bitmap_blk);
	if (unlikely(!bh)) {
		ext2_error(sb, __func__,
			    "Cannot read block bitmap - "
			    "block_group = %d, block_bitmap = %u",
			    block_group, le32_to_cpu(desc->bg_block_bitmap));
		return NULL;
	}
	if (likely(bh_uptodate_or_lock(bh)))
		return bh;

	if (bh_submit_read(bh) < 0) {
		brelse(bh);
		ext2_error(sb, __func__,
			    "Cannot read block bitmap - "
			    "block_group = %d, block_bitmap = %u",
			    block_group, le32_to_cpu(desc->bg_block_bitmap));
		return NULL;
	}

	ext2_valid_block_bitmap(sb, desc, block_group, bh);
	return bh;
}

static void release_blocks(struct super_block *sb, int count)
{
	if (count) {
		struct ext2_sb_info *sbi = EXT2_SB(sb);

		percpu_counter_add(&sbi->s_freeblocks_counter, count);
		sb->s_dirt = 1;
	}
}

static void group_adjust_blocks(struct super_block *sb, int group_no,
	struct ext2_group_desc *desc, struct buffer_head *bh, int count)
{
	if (count) {
		struct ext2_sb_info *sbi = EXT2_SB(sb);
		unsigned free_blocks;

		spin_lock(sb_bgl_lock(sbi, group_no));
		free_blocks = le16_to_cpu(desc->bg_free_blocks_count);
		desc->bg_free_blocks_count = cpu_to_le16(free_blocks + count);
		spin_unlock(sb_bgl_lock(sbi, group_no));
		sb->s_dirt = 1;
		mark_buffer_dirty(bh);
	}
}


#if 1
static void __rsv_window_dump(struct rb_root *root, int verbose,
			      const char *fn)
{
	struct rb_node *n;
	struct ext2_reserve_window_node *rsv, *prev;
	int bad;

restart:
	n = rb_first(root);
	bad = 0;
	prev = NULL;

	printk("Block Allocation Reservation Windows Map (%s):\n", fn);
	while (n) {
		rsv = rb_entry(n, struct ext2_reserve_window_node, rsv_node);
		if (verbose)
			printk("reservation window 0x%p "
				"start: %lu, end: %lu\n",
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
goal_in_my_reservation(struct ext2_reserve_window *rsv, ext2_grpblk_t grp_goal,
			unsigned int group, struct super_block * sb)
{
	ext2_fsblk_t group_first_block, group_last_block;

	group_first_block = ext2_group_first_block_no(sb, group);
	group_last_block = group_first_block + EXT2_BLOCKS_PER_GROUP(sb) - 1;

	if ((rsv->_rsv_start > group_last_block) ||
	    (rsv->_rsv_end < group_first_block))
		return 0;
	if ((grp_goal >= 0) && ((grp_goal + group_first_block < rsv->_rsv_start)
		|| (grp_goal + group_first_block > rsv->_rsv_end)))
		return 0;
	return 1;
}

static struct ext2_reserve_window_node *
search_reserve_window(struct rb_root *root, ext2_fsblk_t goal)
{
	struct rb_node *n = root->rb_node;
	struct ext2_reserve_window_node *rsv;

	if (!n)
		return NULL;

	do {
		rsv = rb_entry(n, struct ext2_reserve_window_node, rsv_node);

		if (goal < rsv->rsv_start)
			n = n->rb_left;
		else if (goal > rsv->rsv_end)
			n = n->rb_right;
		else
			return rsv;
	} while (n);
	if (rsv->rsv_start > goal) {
		n = rb_prev(&rsv->rsv_node);
		rsv = rb_entry(n, struct ext2_reserve_window_node, rsv_node);
	}
	return rsv;
}

void ext2_rsv_window_add(struct super_block *sb,
		    struct ext2_reserve_window_node *rsv)
{
	struct rb_root *root = &EXT2_SB(sb)->s_rsv_window_root;
	struct rb_node *node = &rsv->rsv_node;
	ext2_fsblk_t start = rsv->rsv_start;

	struct rb_node ** p = &root->rb_node;
	struct rb_node * parent = NULL;
	struct ext2_reserve_window_node *this;

	while (*p)
	{
		parent = *p;
		this = rb_entry(parent, struct ext2_reserve_window_node, rsv_node);

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
			      struct ext2_reserve_window_node *rsv)
{
	rsv->rsv_start = EXT2_RESERVE_WINDOW_NOT_ALLOCATED;
	rsv->rsv_end = EXT2_RESERVE_WINDOW_NOT_ALLOCATED;
	rsv->rsv_alloc_hit = 0;
	rb_erase(&rsv->rsv_node, &EXT2_SB(sb)->s_rsv_window_root);
}

static inline int rsv_is_empty(struct ext2_reserve_window *rsv)
{
	
	return (rsv->_rsv_end == EXT2_RESERVE_WINDOW_NOT_ALLOCATED);
}

void ext2_init_block_alloc_info(struct inode *inode)
{
	struct ext2_inode_info *ei = EXT2_I(inode);
	struct ext2_block_alloc_info *block_i;
	struct super_block *sb = inode->i_sb;

	block_i = kmalloc(sizeof(*block_i), GFP_NOFS);
	if (block_i) {
		struct ext2_reserve_window_node *rsv = &block_i->rsv_window_node;

		rsv->rsv_start = EXT2_RESERVE_WINDOW_NOT_ALLOCATED;
		rsv->rsv_end = EXT2_RESERVE_WINDOW_NOT_ALLOCATED;

		if (!test_opt(sb, RESERVATION))
			rsv->rsv_goal_size = 0;
		else
			rsv->rsv_goal_size = EXT2_DEFAULT_RESERVE_BLOCKS;
		rsv->rsv_alloc_hit = 0;
		block_i->last_alloc_logical_block = 0;
		block_i->last_alloc_physical_block = 0;
	}
	ei->i_block_alloc_info = block_i;
}

void ext2_discard_reservation(struct inode *inode)
{
	struct ext2_inode_info *ei = EXT2_I(inode);
	struct ext2_block_alloc_info *block_i = ei->i_block_alloc_info;
	struct ext2_reserve_window_node *rsv;
	spinlock_t *rsv_lock = &EXT2_SB(inode->i_sb)->s_rsv_window_lock;

	if (!block_i)
		return;

	rsv = &block_i->rsv_window_node;
	if (!rsv_is_empty(&rsv->rsv_window)) {
		spin_lock(rsv_lock);
		if (!rsv_is_empty(&rsv->rsv_window))
			rsv_window_remove(inode->i_sb, rsv);
		spin_unlock(rsv_lock);
	}
}

void ext2_free_blocks (struct inode * inode, unsigned long block,
		       unsigned long count)
{
	struct buffer_head *bitmap_bh = NULL;
	struct buffer_head * bh2;
	unsigned long block_group;
	unsigned long bit;
	unsigned long i;
	unsigned long overflow;
	struct super_block * sb = inode->i_sb;
	struct ext2_sb_info * sbi = EXT2_SB(sb);
	struct ext2_group_desc * desc;
	struct ext2_super_block * es = sbi->s_es;
	unsigned freed = 0, group_freed;

	if (block < le32_to_cpu(es->s_first_data_block) ||
	    block + count < block ||
	    block + count > le32_to_cpu(es->s_blocks_count)) {
		ext2_error (sb, "ext2_free_blocks",
			    "Freeing blocks not in datazone - "
			    "block = %lu, count = %lu", block, count);
		goto error_return;
	}

	ext2_debug ("freeing block(s) %lu-%lu\n", block, block + count - 1);

do_more:
	overflow = 0;
	block_group = (block - le32_to_cpu(es->s_first_data_block)) /
		      EXT2_BLOCKS_PER_GROUP(sb);
	bit = (block - le32_to_cpu(es->s_first_data_block)) %
		      EXT2_BLOCKS_PER_GROUP(sb);
	if (bit + count > EXT2_BLOCKS_PER_GROUP(sb)) {
		overflow = bit + count - EXT2_BLOCKS_PER_GROUP(sb);
		count -= overflow;
	}
	brelse(bitmap_bh);
	bitmap_bh = read_block_bitmap(sb, block_group);
	if (!bitmap_bh)
		goto error_return;

	desc = ext2_get_group_desc (sb, block_group, &bh2);
	if (!desc)
		goto error_return;

	if (in_range (le32_to_cpu(desc->bg_block_bitmap), block, count) ||
	    in_range (le32_to_cpu(desc->bg_inode_bitmap), block, count) ||
	    in_range (block, le32_to_cpu(desc->bg_inode_table),
		      sbi->s_itb_per_group) ||
	    in_range (block + count - 1, le32_to_cpu(desc->bg_inode_table),
		      sbi->s_itb_per_group)) {
		ext2_error (sb, "ext2_free_blocks",
			    "Freeing blocks in system zones - "
			    "Block = %lu, count = %lu",
			    block, count);
		goto error_return;
	}

	for (i = 0, group_freed = 0; i < count; i++) {
		if (!ext2_clear_bit_atomic(sb_bgl_lock(sbi, block_group),
						bit + i, bitmap_bh->b_data)) {
			ext2_error(sb, __func__,
				"bit already cleared for block %lu", block + i);
		} else {
			group_freed++;
		}
	}

	mark_buffer_dirty(bitmap_bh);
	if (sb->s_flags & MS_SYNCHRONOUS)
		sync_dirty_buffer(bitmap_bh);

	group_adjust_blocks(sb, block_group, desc, bh2, group_freed);
	freed += group_freed;

	if (overflow) {
		block += count;
		count = overflow;
		goto do_more;
	}
error_return:
	brelse(bitmap_bh);
	release_blocks(sb, freed);
	dquot_free_block_nodirty(inode, freed);
}

static ext2_grpblk_t
bitmap_search_next_usable_block(ext2_grpblk_t start, struct buffer_head *bh,
					ext2_grpblk_t maxblocks)
{
	ext2_grpblk_t next;

	next = ext2_find_next_zero_bit(bh->b_data, maxblocks, start);
	if (next >= maxblocks)
		return -1;
	return next;
}

static ext2_grpblk_t
find_next_usable_block(int start, struct buffer_head *bh, int maxblocks)
{
	ext2_grpblk_t here, next;
	char *p, *r;

	if (start > 0) {
		ext2_grpblk_t end_goal = (start + 63) & ~63;
		if (end_goal > maxblocks)
			end_goal = maxblocks;
		here = ext2_find_next_zero_bit(bh->b_data, end_goal, start);
		if (here < end_goal)
			return here;
		ext2_debug("Bit not found near goal\n");
	}

	here = start;
	if (here < 0)
		here = 0;

	p = ((char *)bh->b_data) + (here >> 3);
	r = memscan(p, 0, ((maxblocks + 7) >> 3) - (here >> 3));
	next = (r - ((char *)bh->b_data)) << 3;

	if (next < maxblocks && next >= here)
		return next;

	here = bitmap_search_next_usable_block(here, bh, maxblocks);
	return here;
}

static int
ext2_try_to_allocate(struct super_block *sb, int group,
			struct buffer_head *bitmap_bh, ext2_grpblk_t grp_goal,
			unsigned long *count,
			struct ext2_reserve_window *my_rsv)
{
	ext2_fsblk_t group_first_block;
       	ext2_grpblk_t start, end;
	unsigned long num = 0;

	
	if (my_rsv) {
		group_first_block = ext2_group_first_block_no(sb, group);
		if (my_rsv->_rsv_start >= group_first_block)
			start = my_rsv->_rsv_start - group_first_block;
		else
			
			start = 0;
		end = my_rsv->_rsv_end - group_first_block + 1;
		if (end > EXT2_BLOCKS_PER_GROUP(sb))
			
			end = EXT2_BLOCKS_PER_GROUP(sb);
		if ((start <= grp_goal) && (grp_goal < end))
			start = grp_goal;
		else
			grp_goal = -1;
	} else {
		if (grp_goal > 0)
			start = grp_goal;
		else
			start = 0;
		end = EXT2_BLOCKS_PER_GROUP(sb);
	}

	BUG_ON(start > EXT2_BLOCKS_PER_GROUP(sb));

repeat:
	if (grp_goal < 0) {
		grp_goal = find_next_usable_block(start, bitmap_bh, end);
		if (grp_goal < 0)
			goto fail_access;
		if (!my_rsv) {
			int i;

			for (i = 0; i < 7 && grp_goal > start &&
					!ext2_test_bit(grp_goal - 1,
					     		bitmap_bh->b_data);
			     		i++, grp_goal--)
				;
		}
	}
	start = grp_goal;

	if (ext2_set_bit_atomic(sb_bgl_lock(EXT2_SB(sb), group), grp_goal,
			       				bitmap_bh->b_data)) {
		start++;
		grp_goal++;
		if (start >= end)
			goto fail_access;
		goto repeat;
	}
	num++;
	grp_goal++;
	while (num < *count && grp_goal < end
		&& !ext2_set_bit_atomic(sb_bgl_lock(EXT2_SB(sb), group),
					grp_goal, bitmap_bh->b_data)) {
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
				struct ext2_reserve_window_node *search_head,
				struct ext2_reserve_window_node *my_rsv,
				struct super_block * sb,
				ext2_fsblk_t start_block,
				ext2_fsblk_t last_block)
{
	struct rb_node *next;
	struct ext2_reserve_window_node *rsv, *prev;
	ext2_fsblk_t cur;
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
		rsv = rb_entry(next,struct ext2_reserve_window_node,rsv_node);

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
		ext2_rsv_window_add(sb, my_rsv);

	return 0;
}

static int alloc_new_reservation(struct ext2_reserve_window_node *my_rsv,
		ext2_grpblk_t grp_goal, struct super_block *sb,
		unsigned int group, struct buffer_head *bitmap_bh)
{
	struct ext2_reserve_window_node *search_head;
	ext2_fsblk_t group_first_block, group_end_block, start_block;
	ext2_grpblk_t first_free_block;
	struct rb_root *fs_rsv_root = &EXT2_SB(sb)->s_rsv_window_root;
	unsigned long size;
	int ret;
	spinlock_t *rsv_lock = &EXT2_SB(sb)->s_rsv_window_lock;

	group_first_block = ext2_group_first_block_no(sb, group);
	group_end_block = group_first_block + (EXT2_BLOCKS_PER_GROUP(sb) - 1);

	if (grp_goal < 0)
		start_block = group_first_block;
	else
		start_block = grp_goal + group_first_block;

	size = my_rsv->rsv_goal_size;

	if (!rsv_is_empty(&my_rsv->rsv_window)) {

		if ((my_rsv->rsv_start <= group_end_block) &&
				(my_rsv->rsv_end > group_end_block) &&
				(start_block >= my_rsv->rsv_start))
			return -1;

		if ((my_rsv->rsv_alloc_hit >
		     (my_rsv->rsv_end - my_rsv->rsv_start + 1) / 2)) {
			size = size * 2;
			if (size > EXT2_MAX_RESERVE_BLOCKS)
				size = EXT2_MAX_RESERVE_BLOCKS;
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
	if (start_block >= my_rsv->rsv_start && start_block <= my_rsv->rsv_end)
		return 0;		
	search_head = my_rsv;
	spin_lock(rsv_lock);
	goto retry;
}

static void try_to_extend_reservation(struct ext2_reserve_window_node *my_rsv,
			struct super_block *sb, int size)
{
	struct ext2_reserve_window_node *next_rsv;
	struct rb_node *next;
	spinlock_t *rsv_lock = &EXT2_SB(sb)->s_rsv_window_lock;

	if (!spin_trylock(rsv_lock))
		return;

	next = rb_next(&my_rsv->rsv_node);

	if (!next)
		my_rsv->rsv_end += size;
	else {
		next_rsv = rb_entry(next, struct ext2_reserve_window_node, rsv_node);

		if ((next_rsv->rsv_start - my_rsv->rsv_end - 1) >= size)
			my_rsv->rsv_end += size;
		else
			my_rsv->rsv_end = next_rsv->rsv_start - 1;
	}
	spin_unlock(rsv_lock);
}

static ext2_grpblk_t
ext2_try_to_allocate_with_rsv(struct super_block *sb, unsigned int group,
			struct buffer_head *bitmap_bh, ext2_grpblk_t grp_goal,
			struct ext2_reserve_window_node * my_rsv,
			unsigned long *count)
{
	ext2_fsblk_t group_first_block, group_last_block;
	ext2_grpblk_t ret = 0;
	unsigned long num = *count;

	if (my_rsv == NULL) {
		return ext2_try_to_allocate(sb, group, bitmap_bh,
						grp_goal, count, NULL);
	}
	group_first_block = ext2_group_first_block_no(sb, group);
	group_last_block = group_first_block + (EXT2_BLOCKS_PER_GROUP(sb) - 1);

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
			rsv_window_dump(&EXT2_SB(sb)->s_rsv_window_root, 1);
			BUG();
		}
		ret = ext2_try_to_allocate(sb, group, bitmap_bh, grp_goal,
					   &num, &my_rsv->rsv_window);
		if (ret >= 0) {
			my_rsv->rsv_alloc_hit += num;
			*count = num;
			break;				
		}
		num = *count;
	}
	return ret;
}

static int ext2_has_free_blocks(struct ext2_sb_info *sbi)
{
	ext2_fsblk_t free_blocks, root_blocks;

	free_blocks = percpu_counter_read_positive(&sbi->s_freeblocks_counter);
	root_blocks = le32_to_cpu(sbi->s_es->s_r_blocks_count);
	if (free_blocks < root_blocks + 1 && !capable(CAP_SYS_RESOURCE) &&
		sbi->s_resuid != current_fsuid() &&
		(sbi->s_resgid == 0 || !in_group_p (sbi->s_resgid))) {
		return 0;
	}
	return 1;
}

ext2_fsblk_t ext2_new_blocks(struct inode *inode, ext2_fsblk_t goal,
		    unsigned long *count, int *errp)
{
	struct buffer_head *bitmap_bh = NULL;
	struct buffer_head *gdp_bh;
	int group_no;
	int goal_group;
	ext2_grpblk_t grp_target_blk;	
	ext2_grpblk_t grp_alloc_blk;	
	ext2_fsblk_t ret_block;		
	int bgi;			
	int performed_allocation = 0;
	ext2_grpblk_t free_blocks;	
	struct super_block *sb;
	struct ext2_group_desc *gdp;
	struct ext2_super_block *es;
	struct ext2_sb_info *sbi;
	struct ext2_reserve_window_node *my_rsv = NULL;
	struct ext2_block_alloc_info *block_i;
	unsigned short windowsz = 0;
	unsigned long ngroups;
	unsigned long num = *count;
	int ret;

	*errp = -ENOSPC;
	sb = inode->i_sb;
	if (!sb) {
		printk("ext2_new_blocks: nonexistent device");
		return 0;
	}

	ret = dquot_alloc_block(inode, num);
	if (ret) {
		*errp = ret;
		return 0;
	}

	sbi = EXT2_SB(sb);
	es = EXT2_SB(sb)->s_es;
	ext2_debug("goal=%lu.\n", goal);
	block_i = EXT2_I(inode)->i_block_alloc_info;
	if (block_i) {
		windowsz = block_i->rsv_window_node.rsv_goal_size;
		if (windowsz > 0)
			my_rsv = &block_i->rsv_window_node;
	}

	if (!ext2_has_free_blocks(sbi)) {
		*errp = -ENOSPC;
		goto out;
	}

	if (goal < le32_to_cpu(es->s_first_data_block) ||
	    goal >= le32_to_cpu(es->s_blocks_count))
		goal = le32_to_cpu(es->s_first_data_block);
	group_no = (goal - le32_to_cpu(es->s_first_data_block)) /
			EXT2_BLOCKS_PER_GROUP(sb);
	goal_group = group_no;
retry_alloc:
	gdp = ext2_get_group_desc(sb, group_no, &gdp_bh);
	if (!gdp)
		goto io_error;

	free_blocks = le16_to_cpu(gdp->bg_free_blocks_count);
	if (my_rsv && (free_blocks < windowsz)
		&& (free_blocks > 0)
		&& (rsv_is_empty(&my_rsv->rsv_window)))
		my_rsv = NULL;

	if (free_blocks > 0) {
		grp_target_blk = ((goal - le32_to_cpu(es->s_first_data_block)) %
				EXT2_BLOCKS_PER_GROUP(sb));
		bitmap_bh = read_block_bitmap(sb, group_no);
		if (!bitmap_bh)
			goto io_error;
		grp_alloc_blk = ext2_try_to_allocate_with_rsv(sb, group_no,
					bitmap_bh, grp_target_blk,
					my_rsv, &num);
		if (grp_alloc_blk >= 0)
			goto allocated;
	}

	ngroups = EXT2_SB(sb)->s_groups_count;
	smp_rmb();

	for (bgi = 0; bgi < ngroups; bgi++) {
		group_no++;
		if (group_no >= ngroups)
			group_no = 0;
		gdp = ext2_get_group_desc(sb, group_no, &gdp_bh);
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
		grp_alloc_blk = ext2_try_to_allocate_with_rsv(sb, group_no,
					bitmap_bh, -1, my_rsv, &num);
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

	ext2_debug("using block group %d(%d)\n",
			group_no, gdp->bg_free_blocks_count);

	ret_block = grp_alloc_blk + ext2_group_first_block_no(sb, group_no);

	if (in_range(le32_to_cpu(gdp->bg_block_bitmap), ret_block, num) ||
	    in_range(le32_to_cpu(gdp->bg_inode_bitmap), ret_block, num) ||
	    in_range(ret_block, le32_to_cpu(gdp->bg_inode_table),
		      EXT2_SB(sb)->s_itb_per_group) ||
	    in_range(ret_block + num - 1, le32_to_cpu(gdp->bg_inode_table),
		      EXT2_SB(sb)->s_itb_per_group)) {
		ext2_error(sb, "ext2_new_blocks",
			    "Allocating block in system zone - "
			    "blocks from "E2FSBLK", length %lu",
			    ret_block, num);
		goto retry_alloc;
	}

	performed_allocation = 1;

	if (ret_block + num - 1 >= le32_to_cpu(es->s_blocks_count)) {
		ext2_error(sb, "ext2_new_blocks",
			    "block("E2FSBLK") >= blocks count(%d) - "
			    "block_group = %d, es == %p ", ret_block,
			le32_to_cpu(es->s_blocks_count), group_no, es);
		goto out;
	}

	group_adjust_blocks(sb, group_no, gdp, gdp_bh, -num);
	percpu_counter_sub(&sbi->s_freeblocks_counter, num);

	mark_buffer_dirty(bitmap_bh);
	if (sb->s_flags & MS_SYNCHRONOUS)
		sync_dirty_buffer(bitmap_bh);

	*errp = 0;
	brelse(bitmap_bh);
	dquot_free_block_nodirty(inode, *count-num);
	mark_inode_dirty(inode);
	*count = num;
	return ret_block;

io_error:
	*errp = -EIO;
out:
	if (!performed_allocation) {
		dquot_free_block_nodirty(inode, *count);
		mark_inode_dirty(inode);
	}
	brelse(bitmap_bh);
	return 0;
}

ext2_fsblk_t ext2_new_block(struct inode *inode, unsigned long goal, int *errp)
{
	unsigned long count = 1;

	return ext2_new_blocks(inode, goal, &count, errp);
}

#ifdef EXT2FS_DEBUG

static const int nibblemap[] = {4, 3, 3, 2, 3, 2, 2, 1, 3, 2, 2, 1, 2, 1, 1, 0};

unsigned long ext2_count_free (struct buffer_head * map, unsigned int numchars)
{
	unsigned int i;
	unsigned long sum = 0;

	if (!map)
		return (0);
	for (i = 0; i < numchars; i++)
		sum += nibblemap[map->b_data[i] & 0xf] +
			nibblemap[(map->b_data[i] >> 4) & 0xf];
	return (sum);
}

#endif  

unsigned long ext2_count_free_blocks (struct super_block * sb)
{
	struct ext2_group_desc * desc;
	unsigned long desc_count = 0;
	int i;
#ifdef EXT2FS_DEBUG
	unsigned long bitmap_count, x;
	struct ext2_super_block *es;

	es = EXT2_SB(sb)->s_es;
	desc_count = 0;
	bitmap_count = 0;
	desc = NULL;
	for (i = 0; i < EXT2_SB(sb)->s_groups_count; i++) {
		struct buffer_head *bitmap_bh;
		desc = ext2_get_group_desc (sb, i, NULL);
		if (!desc)
			continue;
		desc_count += le16_to_cpu(desc->bg_free_blocks_count);
		bitmap_bh = read_block_bitmap(sb, i);
		if (!bitmap_bh)
			continue;
		
		x = ext2_count_free(bitmap_bh, sb->s_blocksize);
		printk ("group %d: stored = %d, counted = %lu\n",
			i, le16_to_cpu(desc->bg_free_blocks_count), x);
		bitmap_count += x;
		brelse(bitmap_bh);
	}
	printk("ext2_count_free_blocks: stored = %lu, computed = %lu, %lu\n",
		(long)le32_to_cpu(es->s_free_blocks_count),
		desc_count, bitmap_count);
	return bitmap_count;
#else
        for (i = 0; i < EXT2_SB(sb)->s_groups_count; i++) {
                desc = ext2_get_group_desc (sb, i, NULL);
                if (!desc)
                        continue;
                desc_count += le16_to_cpu(desc->bg_free_blocks_count);
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

static int ext2_group_sparse(int group)
{
	if (group <= 1)
		return 1;
	return (test_root(group, 3) || test_root(group, 5) ||
		test_root(group, 7));
}

int ext2_bg_has_super(struct super_block *sb, int group)
{
	if (EXT2_HAS_RO_COMPAT_FEATURE(sb,EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER)&&
	    !ext2_group_sparse(group))
		return 0;
	return 1;
}

unsigned long ext2_bg_num_gdb(struct super_block *sb, int group)
{
	return ext2_bg_has_super(sb, group) ? EXT2_SB(sb)->s_gdb_count : 0;
}

