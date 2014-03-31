/*
 *  linux/fs/ext3/ialloc.c
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 *  BSD ufs-inspired inode and directory allocation by
 *  Stephen Tweedie (sct@redhat.com), 1993
 *  Big-endian to little-endian byte-swapping/bitmaps by
 *        David S. Miller (davem@caip.rutgers.edu), 1995
 */

#include <linux/quotaops.h>
#include <linux/random.h>

#include "ext3.h"
#include "xattr.h"
#include "acl.h"




static struct buffer_head *
read_inode_bitmap(struct super_block * sb, unsigned long block_group)
{
	struct ext3_group_desc *desc;
	struct buffer_head *bh = NULL;

	desc = ext3_get_group_desc(sb, block_group, NULL);
	if (!desc)
		goto error_out;

	bh = sb_bread(sb, le32_to_cpu(desc->bg_inode_bitmap));
	if (!bh)
		ext3_error(sb, "read_inode_bitmap",
			    "Cannot read inode bitmap - "
			    "block_group = %lu, inode_bitmap = %u",
			    block_group, le32_to_cpu(desc->bg_inode_bitmap));
error_out:
	return bh;
}

void ext3_free_inode (handle_t *handle, struct inode * inode)
{
	struct super_block * sb = inode->i_sb;
	int is_directory;
	unsigned long ino;
	struct buffer_head *bitmap_bh = NULL;
	struct buffer_head *bh2;
	unsigned long block_group;
	unsigned long bit;
	struct ext3_group_desc * gdp;
	struct ext3_super_block * es;
	struct ext3_sb_info *sbi;
	int fatal = 0, err;

	if (atomic_read(&inode->i_count) > 1) {
		printk ("ext3_free_inode: inode has count=%d\n",
					atomic_read(&inode->i_count));
		return;
	}
	if (inode->i_nlink) {
		printk ("ext3_free_inode: inode has nlink=%d\n",
			inode->i_nlink);
		return;
	}
	if (!sb) {
		printk("ext3_free_inode: inode on nonexistent device\n");
		return;
	}
	sbi = EXT3_SB(sb);

	ino = inode->i_ino;
	ext3_debug ("freeing inode %lu\n", ino);
	trace_ext3_free_inode(inode);

	is_directory = S_ISDIR(inode->i_mode);

	es = EXT3_SB(sb)->s_es;
	if (ino < EXT3_FIRST_INO(sb) || ino > le32_to_cpu(es->s_inodes_count)) {
		ext3_error (sb, "ext3_free_inode",
			    "reserved or nonexistent inode %lu", ino);
		goto error_return;
	}
	block_group = (ino - 1) / EXT3_INODES_PER_GROUP(sb);
	bit = (ino - 1) % EXT3_INODES_PER_GROUP(sb);
	bitmap_bh = read_inode_bitmap(sb, block_group);
	if (!bitmap_bh)
		goto error_return;

	BUFFER_TRACE(bitmap_bh, "get_write_access");
	fatal = ext3_journal_get_write_access(handle, bitmap_bh);
	if (fatal)
		goto error_return;

	
	if (!ext3_clear_bit_atomic(sb_bgl_lock(sbi, block_group),
					bit, bitmap_bh->b_data))
		ext3_error (sb, "ext3_free_inode",
			      "bit already cleared for inode %lu", ino);
	else {
		gdp = ext3_get_group_desc (sb, block_group, &bh2);

		BUFFER_TRACE(bh2, "get_write_access");
		fatal = ext3_journal_get_write_access(handle, bh2);
		if (fatal) goto error_return;

		if (gdp) {
			spin_lock(sb_bgl_lock(sbi, block_group));
			le16_add_cpu(&gdp->bg_free_inodes_count, 1);
			if (is_directory)
				le16_add_cpu(&gdp->bg_used_dirs_count, -1);
			spin_unlock(sb_bgl_lock(sbi, block_group));
			percpu_counter_inc(&sbi->s_freeinodes_counter);
			if (is_directory)
				percpu_counter_dec(&sbi->s_dirs_counter);

		}
		BUFFER_TRACE(bh2, "call ext3_journal_dirty_metadata");
		err = ext3_journal_dirty_metadata(handle, bh2);
		if (!fatal) fatal = err;
	}
	BUFFER_TRACE(bitmap_bh, "call ext3_journal_dirty_metadata");
	err = ext3_journal_dirty_metadata(handle, bitmap_bh);
	if (!fatal)
		fatal = err;

error_return:
	brelse(bitmap_bh);
	ext3_std_error(sb, fatal);
}


#define INODE_COST 64
#define BLOCK_COST 256

static int find_group_orlov(struct super_block *sb, struct inode *parent)
{
	int parent_group = EXT3_I(parent)->i_block_group;
	struct ext3_sb_info *sbi = EXT3_SB(sb);
	struct ext3_super_block *es = sbi->s_es;
	int ngroups = sbi->s_groups_count;
	int inodes_per_group = EXT3_INODES_PER_GROUP(sb);
	unsigned int freei, avefreei;
	ext3_fsblk_t freeb, avefreeb;
	ext3_fsblk_t blocks_per_dir;
	unsigned int ndirs;
	int max_debt, max_dirs, min_inodes;
	ext3_grpblk_t min_blocks;
	int group = -1, i;
	struct ext3_group_desc *desc;

	freei = percpu_counter_read_positive(&sbi->s_freeinodes_counter);
	avefreei = freei / ngroups;
	freeb = percpu_counter_read_positive(&sbi->s_freeblocks_counter);
	avefreeb = freeb / ngroups;
	ndirs = percpu_counter_read_positive(&sbi->s_dirs_counter);

	if ((parent == sb->s_root->d_inode) ||
	    (EXT3_I(parent)->i_flags & EXT3_TOPDIR_FL)) {
		int best_ndir = inodes_per_group;
		int best_group = -1;

		get_random_bytes(&group, sizeof(group));
		parent_group = (unsigned)group % ngroups;
		for (i = 0; i < ngroups; i++) {
			group = (parent_group + i) % ngroups;
			desc = ext3_get_group_desc (sb, group, NULL);
			if (!desc || !desc->bg_free_inodes_count)
				continue;
			if (le16_to_cpu(desc->bg_used_dirs_count) >= best_ndir)
				continue;
			if (le16_to_cpu(desc->bg_free_inodes_count) < avefreei)
				continue;
			if (le16_to_cpu(desc->bg_free_blocks_count) < avefreeb)
				continue;
			best_group = group;
			best_ndir = le16_to_cpu(desc->bg_used_dirs_count);
		}
		if (best_group >= 0)
			return best_group;
		goto fallback;
	}

	blocks_per_dir = (le32_to_cpu(es->s_blocks_count) - freeb) / ndirs;

	max_dirs = ndirs / ngroups + inodes_per_group / 16;
	min_inodes = avefreei - inodes_per_group / 4;
	min_blocks = avefreeb - EXT3_BLOCKS_PER_GROUP(sb) / 4;

	max_debt = EXT3_BLOCKS_PER_GROUP(sb) / max(blocks_per_dir, (ext3_fsblk_t)BLOCK_COST);
	if (max_debt * INODE_COST > inodes_per_group)
		max_debt = inodes_per_group / INODE_COST;
	if (max_debt > 255)
		max_debt = 255;
	if (max_debt == 0)
		max_debt = 1;

	for (i = 0; i < ngroups; i++) {
		group = (parent_group + i) % ngroups;
		desc = ext3_get_group_desc (sb, group, NULL);
		if (!desc || !desc->bg_free_inodes_count)
			continue;
		if (le16_to_cpu(desc->bg_used_dirs_count) >= max_dirs)
			continue;
		if (le16_to_cpu(desc->bg_free_inodes_count) < min_inodes)
			continue;
		if (le16_to_cpu(desc->bg_free_blocks_count) < min_blocks)
			continue;
		return group;
	}

fallback:
	for (i = 0; i < ngroups; i++) {
		group = (parent_group + i) % ngroups;
		desc = ext3_get_group_desc (sb, group, NULL);
		if (!desc || !desc->bg_free_inodes_count)
			continue;
		if (le16_to_cpu(desc->bg_free_inodes_count) >= avefreei)
			return group;
	}

	if (avefreei) {
		avefreei = 0;
		goto fallback;
	}

	return -1;
}

static int find_group_other(struct super_block *sb, struct inode *parent)
{
	int parent_group = EXT3_I(parent)->i_block_group;
	int ngroups = EXT3_SB(sb)->s_groups_count;
	struct ext3_group_desc *desc;
	int group, i;

	group = parent_group;
	desc = ext3_get_group_desc (sb, group, NULL);
	if (desc && le16_to_cpu(desc->bg_free_inodes_count) &&
			le16_to_cpu(desc->bg_free_blocks_count))
		return group;

	group = (group + parent->i_ino) % ngroups;

	for (i = 1; i < ngroups; i <<= 1) {
		group += i;
		if (group >= ngroups)
			group -= ngroups;
		desc = ext3_get_group_desc (sb, group, NULL);
		if (desc && le16_to_cpu(desc->bg_free_inodes_count) &&
				le16_to_cpu(desc->bg_free_blocks_count))
			return group;
	}

	group = parent_group;
	for (i = 0; i < ngroups; i++) {
		if (++group >= ngroups)
			group = 0;
		desc = ext3_get_group_desc (sb, group, NULL);
		if (desc && le16_to_cpu(desc->bg_free_inodes_count))
			return group;
	}

	return -1;
}

struct inode *ext3_new_inode(handle_t *handle, struct inode * dir,
			     const struct qstr *qstr, umode_t mode)
{
	struct super_block *sb;
	struct buffer_head *bitmap_bh = NULL;
	struct buffer_head *bh2;
	int group;
	unsigned long ino = 0;
	struct inode * inode;
	struct ext3_group_desc * gdp = NULL;
	struct ext3_super_block * es;
	struct ext3_inode_info *ei;
	struct ext3_sb_info *sbi;
	int err = 0;
	struct inode *ret;
	int i;

	
	if (!dir || !dir->i_nlink)
		return ERR_PTR(-EPERM);

	sb = dir->i_sb;
	trace_ext3_request_inode(dir, mode);
	inode = new_inode(sb);
	if (!inode)
		return ERR_PTR(-ENOMEM);
	ei = EXT3_I(inode);

	sbi = EXT3_SB(sb);
	es = sbi->s_es;
	if (S_ISDIR(mode))
		group = find_group_orlov(sb, dir);
	else
		group = find_group_other(sb, dir);

	err = -ENOSPC;
	if (group == -1)
		goto out;

	for (i = 0; i < sbi->s_groups_count; i++) {
		err = -EIO;

		gdp = ext3_get_group_desc(sb, group, &bh2);
		if (!gdp)
			goto fail;

		brelse(bitmap_bh);
		bitmap_bh = read_inode_bitmap(sb, group);
		if (!bitmap_bh)
			goto fail;

		ino = 0;

repeat_in_this_group:
		ino = ext3_find_next_zero_bit((unsigned long *)
				bitmap_bh->b_data, EXT3_INODES_PER_GROUP(sb), ino);
		if (ino < EXT3_INODES_PER_GROUP(sb)) {

			BUFFER_TRACE(bitmap_bh, "get_write_access");
			err = ext3_journal_get_write_access(handle, bitmap_bh);
			if (err)
				goto fail;

			if (!ext3_set_bit_atomic(sb_bgl_lock(sbi, group),
						ino, bitmap_bh->b_data)) {
				
				BUFFER_TRACE(bitmap_bh,
					"call ext3_journal_dirty_metadata");
				err = ext3_journal_dirty_metadata(handle,
								bitmap_bh);
				if (err)
					goto fail;
				goto got;
			}
			
			journal_release_buffer(handle, bitmap_bh);

			if (++ino < EXT3_INODES_PER_GROUP(sb))
				goto repeat_in_this_group;
		}

		if (++group == sbi->s_groups_count)
			group = 0;
	}
	err = -ENOSPC;
	goto out;

got:
	ino += group * EXT3_INODES_PER_GROUP(sb) + 1;
	if (ino < EXT3_FIRST_INO(sb) || ino > le32_to_cpu(es->s_inodes_count)) {
		ext3_error (sb, "ext3_new_inode",
			    "reserved inode or inode > inodes count - "
			    "block_group = %d, inode=%lu", group, ino);
		err = -EIO;
		goto fail;
	}

	BUFFER_TRACE(bh2, "get_write_access");
	err = ext3_journal_get_write_access(handle, bh2);
	if (err) goto fail;
	spin_lock(sb_bgl_lock(sbi, group));
	le16_add_cpu(&gdp->bg_free_inodes_count, -1);
	if (S_ISDIR(mode)) {
		le16_add_cpu(&gdp->bg_used_dirs_count, 1);
	}
	spin_unlock(sb_bgl_lock(sbi, group));
	BUFFER_TRACE(bh2, "call ext3_journal_dirty_metadata");
	err = ext3_journal_dirty_metadata(handle, bh2);
	if (err) goto fail;

	percpu_counter_dec(&sbi->s_freeinodes_counter);
	if (S_ISDIR(mode))
		percpu_counter_inc(&sbi->s_dirs_counter);


	if (test_opt(sb, GRPID)) {
		inode->i_mode = mode;
		inode->i_uid = current_fsuid();
		inode->i_gid = dir->i_gid;
	} else
		inode_init_owner(inode, dir, mode);

	inode->i_ino = ino;
	
	inode->i_blocks = 0;
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME_SEC;

	memset(ei->i_data, 0, sizeof(ei->i_data));
	ei->i_dir_start_lookup = 0;
	ei->i_disksize = 0;

	ei->i_flags =
		ext3_mask_flags(mode, EXT3_I(dir)->i_flags & EXT3_FL_INHERITED);
#ifdef EXT3_FRAGMENTS
	ei->i_faddr = 0;
	ei->i_frag_no = 0;
	ei->i_frag_size = 0;
#endif
	ei->i_file_acl = 0;
	ei->i_dir_acl = 0;
	ei->i_dtime = 0;
	ei->i_block_alloc_info = NULL;
	ei->i_block_group = group;

	ext3_set_inode_flags(inode);
	if (IS_DIRSYNC(inode))
		handle->h_sync = 1;
	if (insert_inode_locked(inode) < 0) {
		err = -EIO;
		goto fail;
	}
	spin_lock(&sbi->s_next_gen_lock);
	inode->i_generation = sbi->s_next_generation++;
	spin_unlock(&sbi->s_next_gen_lock);

	ei->i_state_flags = 0;
	ext3_set_inode_state(inode, EXT3_STATE_NEW);

	
	if (ino >= EXT3_FIRST_INO(sb) + 1 &&
	    EXT3_INODE_SIZE(sb) > EXT3_GOOD_OLD_INODE_SIZE) {
		ei->i_extra_isize =
			sizeof(struct ext3_inode) - EXT3_GOOD_OLD_INODE_SIZE;
	} else {
		ei->i_extra_isize = 0;
	}

	ret = inode;
	dquot_initialize(inode);
	err = dquot_alloc_inode(inode);
	if (err)
		goto fail_drop;

	err = ext3_init_acl(handle, inode, dir);
	if (err)
		goto fail_free_drop;

	err = ext3_init_security(handle, inode, dir, qstr);
	if (err)
		goto fail_free_drop;

	err = ext3_mark_inode_dirty(handle, inode);
	if (err) {
		ext3_std_error(sb, err);
		goto fail_free_drop;
	}

	ext3_debug("allocating inode %lu\n", inode->i_ino);
	trace_ext3_allocate_inode(inode, dir, mode);
	goto really_out;
fail:
	ext3_std_error(sb, err);
out:
	iput(inode);
	ret = ERR_PTR(err);
really_out:
	brelse(bitmap_bh);
	return ret;

fail_free_drop:
	dquot_free_inode(inode);

fail_drop:
	dquot_drop(inode);
	inode->i_flags |= S_NOQUOTA;
	clear_nlink(inode);
	unlock_new_inode(inode);
	iput(inode);
	brelse(bitmap_bh);
	return ERR_PTR(err);
}

struct inode *ext3_orphan_get(struct super_block *sb, unsigned long ino)
{
	unsigned long max_ino = le32_to_cpu(EXT3_SB(sb)->s_es->s_inodes_count);
	unsigned long block_group;
	int bit;
	struct buffer_head *bitmap_bh;
	struct inode *inode = NULL;
	long err = -EIO;

	
	if (ino > max_ino) {
		ext3_warning(sb, __func__,
			     "bad orphan ino %lu!  e2fsck was run?", ino);
		goto error;
	}

	block_group = (ino - 1) / EXT3_INODES_PER_GROUP(sb);
	bit = (ino - 1) % EXT3_INODES_PER_GROUP(sb);
	bitmap_bh = read_inode_bitmap(sb, block_group);
	if (!bitmap_bh) {
		ext3_warning(sb, __func__,
			     "inode bitmap error for orphan %lu", ino);
		goto error;
	}

	if (!ext3_test_bit(bit, bitmap_bh->b_data))
		goto bad_orphan;

	inode = ext3_iget(sb, ino);
	if (IS_ERR(inode))
		goto iget_failed;

	if (inode->i_nlink && !ext3_can_truncate(inode))
		goto bad_orphan;

	if (NEXT_ORPHAN(inode) > max_ino)
		goto bad_orphan;
	brelse(bitmap_bh);
	return inode;

iget_failed:
	err = PTR_ERR(inode);
	inode = NULL;
bad_orphan:
	ext3_warning(sb, __func__,
		     "bad orphan inode %lu!  e2fsck was run?", ino);
	printk(KERN_NOTICE "ext3_test_bit(bit=%d, block=%llu) = %d\n",
	       bit, (unsigned long long)bitmap_bh->b_blocknr,
	       ext3_test_bit(bit, bitmap_bh->b_data));
	printk(KERN_NOTICE "inode=%p\n", inode);
	if (inode) {
		printk(KERN_NOTICE "is_bad_inode(inode)=%d\n",
		       is_bad_inode(inode));
		printk(KERN_NOTICE "NEXT_ORPHAN(inode)=%u\n",
		       NEXT_ORPHAN(inode));
		printk(KERN_NOTICE "max_ino=%lu\n", max_ino);
		printk(KERN_NOTICE "i_nlink=%u\n", inode->i_nlink);
		
		if (inode->i_nlink == 0)
			inode->i_blocks = 0;
		iput(inode);
	}
	brelse(bitmap_bh);
error:
	return ERR_PTR(err);
}

unsigned long ext3_count_free_inodes (struct super_block * sb)
{
	unsigned long desc_count;
	struct ext3_group_desc *gdp;
	int i;
#ifdef EXT3FS_DEBUG
	struct ext3_super_block *es;
	unsigned long bitmap_count, x;
	struct buffer_head *bitmap_bh = NULL;

	es = EXT3_SB(sb)->s_es;
	desc_count = 0;
	bitmap_count = 0;
	gdp = NULL;
	for (i = 0; i < EXT3_SB(sb)->s_groups_count; i++) {
		gdp = ext3_get_group_desc (sb, i, NULL);
		if (!gdp)
			continue;
		desc_count += le16_to_cpu(gdp->bg_free_inodes_count);
		brelse(bitmap_bh);
		bitmap_bh = read_inode_bitmap(sb, i);
		if (!bitmap_bh)
			continue;

		x = ext3_count_free(bitmap_bh, EXT3_INODES_PER_GROUP(sb) / 8);
		printk("group %d: stored = %d, counted = %lu\n",
			i, le16_to_cpu(gdp->bg_free_inodes_count), x);
		bitmap_count += x;
	}
	brelse(bitmap_bh);
	printk("ext3_count_free_inodes: stored = %u, computed = %lu, %lu\n",
		le32_to_cpu(es->s_free_inodes_count), desc_count, bitmap_count);
	return desc_count;
#else
	desc_count = 0;
	for (i = 0; i < EXT3_SB(sb)->s_groups_count; i++) {
		gdp = ext3_get_group_desc (sb, i, NULL);
		if (!gdp)
			continue;
		desc_count += le16_to_cpu(gdp->bg_free_inodes_count);
		cond_resched();
	}
	return desc_count;
#endif
}

unsigned long ext3_count_dirs (struct super_block * sb)
{
	unsigned long count = 0;
	int i;

	for (i = 0; i < EXT3_SB(sb)->s_groups_count; i++) {
		struct ext3_group_desc *gdp = ext3_get_group_desc (sb, i, NULL);
		if (!gdp)
			continue;
		count += le16_to_cpu(gdp->bg_used_dirs_count);
	}
	return count;
}

