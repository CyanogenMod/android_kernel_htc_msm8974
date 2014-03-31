/*
 *  linux/fs/ext3/resize.c
 *
 * Support for resizing an ext3 filesystem while it is mounted.
 *
 * Copyright (C) 2001, 2002 Andreas Dilger <adilger@clusterfs.com>
 *
 * This could probably be made into a module, because it is not often in use.
 */


#define EXT3FS_DEBUG

#include "ext3.h"


#define outside(b, first, last)	((b) < (first) || (b) >= (last))
#define inside(b, first, last)	((b) >= (first) && (b) < (last))

static int verify_group_input(struct super_block *sb,
			      struct ext3_new_group_data *input)
{
	struct ext3_sb_info *sbi = EXT3_SB(sb);
	struct ext3_super_block *es = sbi->s_es;
	ext3_fsblk_t start = le32_to_cpu(es->s_blocks_count);
	ext3_fsblk_t end = start + input->blocks_count;
	unsigned group = input->group;
	ext3_fsblk_t itend = input->inode_table + sbi->s_itb_per_group;
	unsigned overhead = ext3_bg_has_super(sb, group) ?
		(1 + ext3_bg_num_gdb(sb, group) +
		 le16_to_cpu(es->s_reserved_gdt_blocks)) : 0;
	ext3_fsblk_t metaend = start + overhead;
	struct buffer_head *bh = NULL;
	ext3_grpblk_t free_blocks_count;
	int err = -EINVAL;

	input->free_blocks_count = free_blocks_count =
		input->blocks_count - 2 - overhead - sbi->s_itb_per_group;

	if (test_opt(sb, DEBUG))
		printk(KERN_DEBUG "EXT3-fs: adding %s group %u: %u blocks "
		       "(%d free, %u reserved)\n",
		       ext3_bg_has_super(sb, input->group) ? "normal" :
		       "no-super", input->group, input->blocks_count,
		       free_blocks_count, input->reserved_blocks);

	if (group != sbi->s_groups_count)
		ext3_warning(sb, __func__,
			     "Cannot add at group %u (only %lu groups)",
			     input->group, sbi->s_groups_count);
	else if ((start - le32_to_cpu(es->s_first_data_block)) %
		 EXT3_BLOCKS_PER_GROUP(sb))
		ext3_warning(sb, __func__, "Last group not full");
	else if (input->reserved_blocks > input->blocks_count / 5)
		ext3_warning(sb, __func__, "Reserved blocks too high (%u)",
			     input->reserved_blocks);
	else if (free_blocks_count < 0)
		ext3_warning(sb, __func__, "Bad blocks count %u",
			     input->blocks_count);
	else if (!(bh = sb_bread(sb, end - 1)))
		ext3_warning(sb, __func__,
			     "Cannot read last block ("E3FSBLK")",
			     end - 1);
	else if (outside(input->block_bitmap, start, end))
		ext3_warning(sb, __func__,
			     "Block bitmap not in group (block %u)",
			     input->block_bitmap);
	else if (outside(input->inode_bitmap, start, end))
		ext3_warning(sb, __func__,
			     "Inode bitmap not in group (block %u)",
			     input->inode_bitmap);
	else if (outside(input->inode_table, start, end) ||
	         outside(itend - 1, start, end))
		ext3_warning(sb, __func__,
			     "Inode table not in group (blocks %u-"E3FSBLK")",
			     input->inode_table, itend - 1);
	else if (input->inode_bitmap == input->block_bitmap)
		ext3_warning(sb, __func__,
			     "Block bitmap same as inode bitmap (%u)",
			     input->block_bitmap);
	else if (inside(input->block_bitmap, input->inode_table, itend))
		ext3_warning(sb, __func__,
			     "Block bitmap (%u) in inode table (%u-"E3FSBLK")",
			     input->block_bitmap, input->inode_table, itend-1);
	else if (inside(input->inode_bitmap, input->inode_table, itend))
		ext3_warning(sb, __func__,
			     "Inode bitmap (%u) in inode table (%u-"E3FSBLK")",
			     input->inode_bitmap, input->inode_table, itend-1);
	else if (inside(input->block_bitmap, start, metaend))
		ext3_warning(sb, __func__,
			     "Block bitmap (%u) in GDT table"
			     " ("E3FSBLK"-"E3FSBLK")",
			     input->block_bitmap, start, metaend - 1);
	else if (inside(input->inode_bitmap, start, metaend))
		ext3_warning(sb, __func__,
			     "Inode bitmap (%u) in GDT table"
			     " ("E3FSBLK"-"E3FSBLK")",
			     input->inode_bitmap, start, metaend - 1);
	else if (inside(input->inode_table, start, metaend) ||
	         inside(itend - 1, start, metaend))
		ext3_warning(sb, __func__,
			     "Inode table (%u-"E3FSBLK") overlaps"
			     "GDT table ("E3FSBLK"-"E3FSBLK")",
			     input->inode_table, itend - 1, start, metaend - 1);
	else
		err = 0;
	brelse(bh);

	return err;
}

static struct buffer_head *bclean(handle_t *handle, struct super_block *sb,
				  ext3_fsblk_t blk)
{
	struct buffer_head *bh;
	int err;

	bh = sb_getblk(sb, blk);
	if (!bh)
		return ERR_PTR(-EIO);
	if ((err = ext3_journal_get_write_access(handle, bh))) {
		brelse(bh);
		bh = ERR_PTR(err);
	} else {
		lock_buffer(bh);
		memset(bh->b_data, 0, sb->s_blocksize);
		set_buffer_uptodate(bh);
		unlock_buffer(bh);
	}

	return bh;
}

static void mark_bitmap_end(int start_bit, int end_bit, char *bitmap)
{
	int i;

	if (start_bit >= end_bit)
		return;

	ext3_debug("mark end bits +%d through +%d used\n", start_bit, end_bit);
	for (i = start_bit; i < ((start_bit + 7) & ~7UL); i++)
		ext3_set_bit(i, bitmap);
	if (i < end_bit)
		memset(bitmap + (i >> 3), 0xff, (end_bit - i) >> 3);
}

static int extend_or_restart_transaction(handle_t *handle, int thresh,
					 struct buffer_head *bh)
{
	int err;

	if (handle->h_buffer_credits >= thresh)
		return 0;

	err = ext3_journal_extend(handle, EXT3_MAX_TRANS_DATA);
	if (err < 0)
		return err;
	if (err) {
		err = ext3_journal_restart(handle, EXT3_MAX_TRANS_DATA);
		if (err)
			return err;
		err = ext3_journal_get_write_access(handle, bh);
		if (err)
			return err;
	}

	return 0;
}

static int setup_new_group_blocks(struct super_block *sb,
				  struct ext3_new_group_data *input)
{
	struct ext3_sb_info *sbi = EXT3_SB(sb);
	ext3_fsblk_t start = ext3_group_first_block_no(sb, input->group);
	int reserved_gdb = ext3_bg_has_super(sb, input->group) ?
		le16_to_cpu(sbi->s_es->s_reserved_gdt_blocks) : 0;
	unsigned long gdblocks = ext3_bg_num_gdb(sb, input->group);
	struct buffer_head *bh;
	handle_t *handle;
	ext3_fsblk_t block;
	ext3_grpblk_t bit;
	int i;
	int err = 0, err2;

	
	handle = ext3_journal_start_sb(sb, EXT3_MAX_TRANS_DATA);

	if (IS_ERR(handle))
		return PTR_ERR(handle);

	mutex_lock(&sbi->s_resize_lock);
	if (input->group != sbi->s_groups_count) {
		err = -EBUSY;
		goto exit_journal;
	}

	if (IS_ERR(bh = bclean(handle, sb, input->block_bitmap))) {
		err = PTR_ERR(bh);
		goto exit_journal;
	}

	if (ext3_bg_has_super(sb, input->group)) {
		ext3_debug("mark backup superblock %#04lx (+0)\n", start);
		ext3_set_bit(0, bh->b_data);
	}

	
	for (i = 0, bit = 1, block = start + 1;
	     i < gdblocks; i++, block++, bit++) {
		struct buffer_head *gdb;

		ext3_debug("update backup group %#04lx (+%d)\n", block, bit);

		err = extend_or_restart_transaction(handle, 1, bh);
		if (err)
			goto exit_bh;

		gdb = sb_getblk(sb, block);
		if (!gdb) {
			err = -EIO;
			goto exit_bh;
		}
		if ((err = ext3_journal_get_write_access(handle, gdb))) {
			brelse(gdb);
			goto exit_bh;
		}
		lock_buffer(gdb);
		memcpy(gdb->b_data, sbi->s_group_desc[i]->b_data, gdb->b_size);
		set_buffer_uptodate(gdb);
		unlock_buffer(gdb);
		err = ext3_journal_dirty_metadata(handle, gdb);
		if (err) {
			brelse(gdb);
			goto exit_bh;
		}
		ext3_set_bit(bit, bh->b_data);
		brelse(gdb);
	}

	
	for (i = 0, bit = gdblocks + 1, block = start + bit;
	     i < reserved_gdb; i++, block++, bit++) {
		struct buffer_head *gdb;

		ext3_debug("clear reserved block %#04lx (+%d)\n", block, bit);

		err = extend_or_restart_transaction(handle, 1, bh);
		if (err)
			goto exit_bh;

		if (IS_ERR(gdb = bclean(handle, sb, block))) {
			err = PTR_ERR(gdb);
			goto exit_bh;
		}
		err = ext3_journal_dirty_metadata(handle, gdb);
		if (err) {
			brelse(gdb);
			goto exit_bh;
		}
		ext3_set_bit(bit, bh->b_data);
		brelse(gdb);
	}
	ext3_debug("mark block bitmap %#04x (+%ld)\n", input->block_bitmap,
		   input->block_bitmap - start);
	ext3_set_bit(input->block_bitmap - start, bh->b_data);
	ext3_debug("mark inode bitmap %#04x (+%ld)\n", input->inode_bitmap,
		   input->inode_bitmap - start);
	ext3_set_bit(input->inode_bitmap - start, bh->b_data);

	
	for (i = 0, block = input->inode_table, bit = block - start;
	     i < sbi->s_itb_per_group; i++, bit++, block++) {
		struct buffer_head *it;

		ext3_debug("clear inode block %#04lx (+%d)\n", block, bit);

		err = extend_or_restart_transaction(handle, 1, bh);
		if (err)
			goto exit_bh;

		if (IS_ERR(it = bclean(handle, sb, block))) {
			err = PTR_ERR(it);
			goto exit_bh;
		}
		err = ext3_journal_dirty_metadata(handle, it);
		if (err) {
			brelse(it);
			goto exit_bh;
		}
		brelse(it);
		ext3_set_bit(bit, bh->b_data);
	}

	err = extend_or_restart_transaction(handle, 2, bh);
	if (err)
		goto exit_bh;

	mark_bitmap_end(input->blocks_count, EXT3_BLOCKS_PER_GROUP(sb),
			bh->b_data);
	err = ext3_journal_dirty_metadata(handle, bh);
	if (err)
		goto exit_bh;
	brelse(bh);

	
	ext3_debug("clear inode bitmap %#04x (+%ld)\n",
		   input->inode_bitmap, input->inode_bitmap - start);
	if (IS_ERR(bh = bclean(handle, sb, input->inode_bitmap))) {
		err = PTR_ERR(bh);
		goto exit_journal;
	}

	mark_bitmap_end(EXT3_INODES_PER_GROUP(sb), EXT3_BLOCKS_PER_GROUP(sb),
			bh->b_data);
	err = ext3_journal_dirty_metadata(handle, bh);
exit_bh:
	brelse(bh);

exit_journal:
	mutex_unlock(&sbi->s_resize_lock);
	if ((err2 = ext3_journal_stop(handle)) && !err)
		err = err2;

	return err;
}

static unsigned ext3_list_backups(struct super_block *sb, unsigned *three,
				  unsigned *five, unsigned *seven)
{
	unsigned *min = three;
	int mult = 3;
	unsigned ret;

	if (!EXT3_HAS_RO_COMPAT_FEATURE(sb,
					EXT3_FEATURE_RO_COMPAT_SPARSE_SUPER)) {
		ret = *min;
		*min += 1;
		return ret;
	}

	if (*five < *min) {
		min = five;
		mult = 5;
	}
	if (*seven < *min) {
		min = seven;
		mult = 7;
	}

	ret = *min;
	*min *= mult;

	return ret;
}

static int verify_reserved_gdb(struct super_block *sb,
			       struct buffer_head *primary)
{
	const ext3_fsblk_t blk = primary->b_blocknr;
	const unsigned long end = EXT3_SB(sb)->s_groups_count;
	unsigned three = 1;
	unsigned five = 5;
	unsigned seven = 7;
	unsigned grp;
	__le32 *p = (__le32 *)primary->b_data;
	int gdbackups = 0;

	while ((grp = ext3_list_backups(sb, &three, &five, &seven)) < end) {
		if (le32_to_cpu(*p++) != grp * EXT3_BLOCKS_PER_GROUP(sb) + blk){
			ext3_warning(sb, __func__,
				     "reserved GDT "E3FSBLK
				     " missing grp %d ("E3FSBLK")",
				     blk, grp,
				     grp * EXT3_BLOCKS_PER_GROUP(sb) + blk);
			return -EINVAL;
		}
		if (++gdbackups > EXT3_ADDR_PER_BLOCK(sb))
			return -EFBIG;
	}

	return gdbackups;
}

static int add_new_gdb(handle_t *handle, struct inode *inode,
		       struct ext3_new_group_data *input,
		       struct buffer_head **primary)
{
	struct super_block *sb = inode->i_sb;
	struct ext3_super_block *es = EXT3_SB(sb)->s_es;
	unsigned long gdb_num = input->group / EXT3_DESC_PER_BLOCK(sb);
	ext3_fsblk_t gdblock = EXT3_SB(sb)->s_sbh->b_blocknr + 1 + gdb_num;
	struct buffer_head **o_group_desc, **n_group_desc;
	struct buffer_head *dind;
	int gdbackups;
	struct ext3_iloc iloc;
	__le32 *data;
	int err;

	if (test_opt(sb, DEBUG))
		printk(KERN_DEBUG
		       "EXT3-fs: ext3_add_new_gdb: adding group block %lu\n",
		       gdb_num);

	if (EXT3_SB(sb)->s_sbh->b_blocknr !=
	    le32_to_cpu(EXT3_SB(sb)->s_es->s_first_data_block)) {
		ext3_warning(sb, __func__,
			"won't resize using backup superblock at %llu",
			(unsigned long long)EXT3_SB(sb)->s_sbh->b_blocknr);
		return -EPERM;
	}

	*primary = sb_bread(sb, gdblock);
	if (!*primary)
		return -EIO;

	if ((gdbackups = verify_reserved_gdb(sb, *primary)) < 0) {
		err = gdbackups;
		goto exit_bh;
	}

	data = EXT3_I(inode)->i_data + EXT3_DIND_BLOCK;
	dind = sb_bread(sb, le32_to_cpu(*data));
	if (!dind) {
		err = -EIO;
		goto exit_bh;
	}

	data = (__le32 *)dind->b_data;
	if (le32_to_cpu(data[gdb_num % EXT3_ADDR_PER_BLOCK(sb)]) != gdblock) {
		ext3_warning(sb, __func__,
			     "new group %u GDT block "E3FSBLK" not reserved",
			     input->group, gdblock);
		err = -EINVAL;
		goto exit_dind;
	}

	if ((err = ext3_journal_get_write_access(handle, EXT3_SB(sb)->s_sbh)))
		goto exit_dind;

	if ((err = ext3_journal_get_write_access(handle, *primary)))
		goto exit_sbh;

	if ((err = ext3_journal_get_write_access(handle, dind)))
		goto exit_primary;

	
	if ((err = ext3_reserve_inode_write(handle, inode, &iloc)))
		goto exit_dindj;

	n_group_desc = kmalloc((gdb_num + 1) * sizeof(struct buffer_head *),
			GFP_NOFS);
	if (!n_group_desc) {
		err = -ENOMEM;
		ext3_warning (sb, __func__,
			      "not enough memory for %lu groups", gdb_num + 1);
		goto exit_inode;
	}

	data[gdb_num % EXT3_ADDR_PER_BLOCK(sb)] = 0;
	err = ext3_journal_dirty_metadata(handle, dind);
	if (err)
		goto exit_group_desc;
	brelse(dind);
	dind = NULL;
	inode->i_blocks -= (gdbackups + 1) * sb->s_blocksize >> 9;
	err = ext3_mark_iloc_dirty(handle, inode, &iloc);
	if (err)
		goto exit_group_desc;
	memset((*primary)->b_data, 0, sb->s_blocksize);
	err = ext3_journal_dirty_metadata(handle, *primary);
	if (err)
		goto exit_group_desc;

	o_group_desc = EXT3_SB(sb)->s_group_desc;
	memcpy(n_group_desc, o_group_desc,
	       EXT3_SB(sb)->s_gdb_count * sizeof(struct buffer_head *));
	n_group_desc[gdb_num] = *primary;
	EXT3_SB(sb)->s_group_desc = n_group_desc;
	EXT3_SB(sb)->s_gdb_count++;
	kfree(o_group_desc);

	le16_add_cpu(&es->s_reserved_gdt_blocks, -1);
	err = ext3_journal_dirty_metadata(handle, EXT3_SB(sb)->s_sbh);
	if (err)
		goto exit_inode;

	return 0;

exit_group_desc:
	kfree(n_group_desc);
exit_inode:
	
	brelse(iloc.bh);
exit_dindj:
	
exit_primary:
	
exit_sbh:
	
exit_dind:
	brelse(dind);
exit_bh:
	brelse(*primary);

	ext3_debug("leaving with error %d\n", err);
	return err;
}

static int reserve_backup_gdb(handle_t *handle, struct inode *inode,
			      struct ext3_new_group_data *input)
{
	struct super_block *sb = inode->i_sb;
	int reserved_gdb =le16_to_cpu(EXT3_SB(sb)->s_es->s_reserved_gdt_blocks);
	struct buffer_head **primary;
	struct buffer_head *dind;
	struct ext3_iloc iloc;
	ext3_fsblk_t blk;
	__le32 *data, *end;
	int gdbackups = 0;
	int res, i;
	int err;

	primary = kmalloc(reserved_gdb * sizeof(*primary), GFP_NOFS);
	if (!primary)
		return -ENOMEM;

	data = EXT3_I(inode)->i_data + EXT3_DIND_BLOCK;
	dind = sb_bread(sb, le32_to_cpu(*data));
	if (!dind) {
		err = -EIO;
		goto exit_free;
	}

	blk = EXT3_SB(sb)->s_sbh->b_blocknr + 1 + EXT3_SB(sb)->s_gdb_count;
	data = (__le32 *)dind->b_data + (EXT3_SB(sb)->s_gdb_count %
					 EXT3_ADDR_PER_BLOCK(sb));
	end = (__le32 *)dind->b_data + EXT3_ADDR_PER_BLOCK(sb);

	
	for (res = 0; res < reserved_gdb; res++, blk++) {
		if (le32_to_cpu(*data) != blk) {
			ext3_warning(sb, __func__,
				     "reserved block "E3FSBLK
				     " not at offset %ld",
				     blk,
				     (long)(data - (__le32 *)dind->b_data));
			err = -EINVAL;
			goto exit_bh;
		}
		primary[res] = sb_bread(sb, blk);
		if (!primary[res]) {
			err = -EIO;
			goto exit_bh;
		}
		if ((gdbackups = verify_reserved_gdb(sb, primary[res])) < 0) {
			brelse(primary[res]);
			err = gdbackups;
			goto exit_bh;
		}
		if (++data >= end)
			data = (__le32 *)dind->b_data;
	}

	for (i = 0; i < reserved_gdb; i++) {
		if ((err = ext3_journal_get_write_access(handle, primary[i]))) {
			goto exit_bh;
		}
	}

	if ((err = ext3_reserve_inode_write(handle, inode, &iloc)))
		goto exit_bh;

	blk = input->group * EXT3_BLOCKS_PER_GROUP(sb);
	for (i = 0; i < reserved_gdb; i++) {
		int err2;
		data = (__le32 *)primary[i]->b_data;
		data[gdbackups] = cpu_to_le32(blk + primary[i]->b_blocknr);
		err2 = ext3_journal_dirty_metadata(handle, primary[i]);
		if (!err)
			err = err2;
	}
	inode->i_blocks += reserved_gdb * sb->s_blocksize >> 9;
	ext3_mark_iloc_dirty(handle, inode, &iloc);

exit_bh:
	while (--res >= 0)
		brelse(primary[res]);
	brelse(dind);

exit_free:
	kfree(primary);

	return err;
}

static void update_backups(struct super_block *sb,
			   int blk_off, char *data, int size)
{
	struct ext3_sb_info *sbi = EXT3_SB(sb);
	const unsigned long last = sbi->s_groups_count;
	const int bpg = EXT3_BLOCKS_PER_GROUP(sb);
	unsigned three = 1;
	unsigned five = 5;
	unsigned seven = 7;
	unsigned group;
	int rest = sb->s_blocksize - size;
	handle_t *handle;
	int err = 0, err2;

	handle = ext3_journal_start_sb(sb, EXT3_MAX_TRANS_DATA);
	if (IS_ERR(handle)) {
		group = 1;
		err = PTR_ERR(handle);
		goto exit_err;
	}

	while ((group = ext3_list_backups(sb, &three, &five, &seven)) < last) {
		struct buffer_head *bh;

		
		if (handle->h_buffer_credits == 0 &&
		    ext3_journal_extend(handle, EXT3_MAX_TRANS_DATA) &&
		    (err = ext3_journal_restart(handle, EXT3_MAX_TRANS_DATA)))
			break;

		bh = sb_getblk(sb, group * bpg + blk_off);
		if (!bh) {
			err = -EIO;
			break;
		}
		ext3_debug("update metadata backup %#04lx\n",
			  (unsigned long)bh->b_blocknr);
		if ((err = ext3_journal_get_write_access(handle, bh))) {
			brelse(bh);
			break;
		}
		lock_buffer(bh);
		memcpy(bh->b_data, data, size);
		if (rest)
			memset(bh->b_data + size, 0, rest);
		set_buffer_uptodate(bh);
		unlock_buffer(bh);
		err = ext3_journal_dirty_metadata(handle, bh);
		brelse(bh);
		if (err)
			break;
	}
	if ((err2 = ext3_journal_stop(handle)) && !err)
		err = err2;

	/*
	 * Ugh! Need to have e2fsck write the backup copies.  It is too
	 * late to revert the resize, we shouldn't fail just because of
	 * the backup copies (they are only needed in case of corruption).
	 *
	 * However, if we got here we have a journal problem too, so we
	 * can't really start a transaction to mark the superblock.
	 * Chicken out and just set the flag on the hope it will be written
	 * to disk, and if not - we will simply wait until next fsck.
	 */
exit_err:
	if (err) {
		ext3_warning(sb, __func__,
			     "can't update backup for group %d (err %d), "
			     "forcing fsck on next reboot", group, err);
		sbi->s_mount_state &= ~EXT3_VALID_FS;
		sbi->s_es->s_state &= cpu_to_le16(~EXT3_VALID_FS);
		mark_buffer_dirty(sbi->s_sbh);
	}
}

int ext3_group_add(struct super_block *sb, struct ext3_new_group_data *input)
{
	struct ext3_sb_info *sbi = EXT3_SB(sb);
	struct ext3_super_block *es = sbi->s_es;
	int reserved_gdb = ext3_bg_has_super(sb, input->group) ?
		le16_to_cpu(es->s_reserved_gdt_blocks) : 0;
	struct buffer_head *primary = NULL;
	struct ext3_group_desc *gdp;
	struct inode *inode = NULL;
	handle_t *handle;
	int gdb_off, gdb_num;
	int err, err2;

	gdb_num = input->group / EXT3_DESC_PER_BLOCK(sb);
	gdb_off = input->group % EXT3_DESC_PER_BLOCK(sb);

	if (gdb_off == 0 && !EXT3_HAS_RO_COMPAT_FEATURE(sb,
					EXT3_FEATURE_RO_COMPAT_SPARSE_SUPER)) {
		ext3_warning(sb, __func__,
			     "Can't resize non-sparse filesystem further");
		return -EPERM;
	}

	if (le32_to_cpu(es->s_blocks_count) + input->blocks_count <
	    le32_to_cpu(es->s_blocks_count)) {
		ext3_warning(sb, __func__, "blocks_count overflow\n");
		return -EINVAL;
	}

	if (le32_to_cpu(es->s_inodes_count) + EXT3_INODES_PER_GROUP(sb) <
	    le32_to_cpu(es->s_inodes_count)) {
		ext3_warning(sb, __func__, "inodes_count overflow\n");
		return -EINVAL;
	}

	if (reserved_gdb || gdb_off == 0) {
		if (!EXT3_HAS_COMPAT_FEATURE(sb,
					     EXT3_FEATURE_COMPAT_RESIZE_INODE)
		    || !le16_to_cpu(es->s_reserved_gdt_blocks)) {
			ext3_warning(sb, __func__,
				     "No reserved GDT blocks, can't resize");
			return -EPERM;
		}
		inode = ext3_iget(sb, EXT3_RESIZE_INO);
		if (IS_ERR(inode)) {
			ext3_warning(sb, __func__,
				     "Error opening resize inode");
			return PTR_ERR(inode);
		}
	}

	if ((err = verify_group_input(sb, input)))
		goto exit_put;

	if ((err = setup_new_group_blocks(sb, input)))
		goto exit_put;

	handle = ext3_journal_start_sb(sb,
				       ext3_bg_has_super(sb, input->group) ?
				       3 + reserved_gdb : 4);
	if (IS_ERR(handle)) {
		err = PTR_ERR(handle);
		goto exit_put;
	}

	mutex_lock(&sbi->s_resize_lock);
	if (input->group != sbi->s_groups_count) {
		ext3_warning(sb, __func__,
			     "multiple resizers run on filesystem!");
		err = -EBUSY;
		goto exit_journal;
	}

	if ((err = ext3_journal_get_write_access(handle, sbi->s_sbh)))
		goto exit_journal;

	if (gdb_off) {
		primary = sbi->s_group_desc[gdb_num];
		if ((err = ext3_journal_get_write_access(handle, primary)))
			goto exit_journal;

		if (reserved_gdb && ext3_bg_num_gdb(sb, input->group) &&
		    (err = reserve_backup_gdb(handle, inode, input)))
			goto exit_journal;
	} else if ((err = add_new_gdb(handle, inode, input, &primary)))
		goto exit_journal;


	
	gdp = (struct ext3_group_desc *)primary->b_data + gdb_off;

	gdp->bg_block_bitmap = cpu_to_le32(input->block_bitmap);
	gdp->bg_inode_bitmap = cpu_to_le32(input->inode_bitmap);
	gdp->bg_inode_table = cpu_to_le32(input->inode_table);
	gdp->bg_free_blocks_count = cpu_to_le16(input->free_blocks_count);
	gdp->bg_free_inodes_count = cpu_to_le16(EXT3_INODES_PER_GROUP(sb));

	le32_add_cpu(&es->s_blocks_count, input->blocks_count);
	le32_add_cpu(&es->s_inodes_count, EXT3_INODES_PER_GROUP(sb));

	smp_wmb();

	
	sbi->s_groups_count++;

	err = ext3_journal_dirty_metadata(handle, primary);
	if (err)
		goto exit_journal;

	le32_add_cpu(&es->s_r_blocks_count, input->reserved_blocks);

	
	percpu_counter_add(&sbi->s_freeblocks_counter,
			   input->free_blocks_count);
	percpu_counter_add(&sbi->s_freeinodes_counter,
			   EXT3_INODES_PER_GROUP(sb));

	err = ext3_journal_dirty_metadata(handle, sbi->s_sbh);

exit_journal:
	mutex_unlock(&sbi->s_resize_lock);
	if ((err2 = ext3_journal_stop(handle)) && !err)
		err = err2;
	if (!err) {
		update_backups(sb, sbi->s_sbh->b_blocknr, (char *)es,
			       sizeof(struct ext3_super_block));
		update_backups(sb, primary->b_blocknr, primary->b_data,
			       primary->b_size);
	}
exit_put:
	iput(inode);
	return err;
} 

int ext3_group_extend(struct super_block *sb, struct ext3_super_block *es,
		      ext3_fsblk_t n_blocks_count)
{
	ext3_fsblk_t o_blocks_count;
	ext3_grpblk_t last;
	ext3_grpblk_t add;
	struct buffer_head * bh;
	handle_t *handle;
	int err;
	unsigned long freed_blocks;

	o_blocks_count = le32_to_cpu(es->s_blocks_count);

	if (test_opt(sb, DEBUG))
		printk(KERN_DEBUG "EXT3-fs: extending last group from "E3FSBLK
		       " up to "E3FSBLK" blocks\n",
		       o_blocks_count, n_blocks_count);

	if (n_blocks_count == 0 || n_blocks_count == o_blocks_count)
		return 0;

	if (n_blocks_count > (sector_t)(~0ULL) >> (sb->s_blocksize_bits - 9)) {
		printk(KERN_ERR "EXT3-fs: filesystem on %s:"
			" too large to resize to "E3FSBLK" blocks safely\n",
			sb->s_id, n_blocks_count);
		if (sizeof(sector_t) < 8)
			ext3_warning(sb, __func__,
			"CONFIG_LBDAF not enabled\n");
		return -EINVAL;
	}

	if (n_blocks_count < o_blocks_count) {
		ext3_warning(sb, __func__,
			     "can't shrink FS - resize aborted");
		return -EBUSY;
	}

	
	last = (o_blocks_count - le32_to_cpu(es->s_first_data_block)) %
		EXT3_BLOCKS_PER_GROUP(sb);

	if (last == 0) {
		ext3_warning(sb, __func__,
			     "need to use ext2online to resize further");
		return -EPERM;
	}

	add = EXT3_BLOCKS_PER_GROUP(sb) - last;

	if (o_blocks_count + add < o_blocks_count) {
		ext3_warning(sb, __func__, "blocks_count overflow");
		return -EINVAL;
	}

	if (o_blocks_count + add > n_blocks_count)
		add = n_blocks_count - o_blocks_count;

	if (o_blocks_count + add < n_blocks_count)
		ext3_warning(sb, __func__,
			     "will only finish group ("E3FSBLK
			     " blocks, %u new)",
			     o_blocks_count + add, add);

	
	bh = sb_bread(sb, o_blocks_count + add -1);
	if (!bh) {
		ext3_warning(sb, __func__,
			     "can't read last block, resize aborted");
		return -ENOSPC;
	}
	brelse(bh);

	handle = ext3_journal_start_sb(sb, 3);
	if (IS_ERR(handle)) {
		err = PTR_ERR(handle);
		ext3_warning(sb, __func__, "error %d on journal start",err);
		goto exit_put;
	}

	mutex_lock(&EXT3_SB(sb)->s_resize_lock);
	if (o_blocks_count != le32_to_cpu(es->s_blocks_count)) {
		ext3_warning(sb, __func__,
			     "multiple resizers run on filesystem!");
		mutex_unlock(&EXT3_SB(sb)->s_resize_lock);
		ext3_journal_stop(handle);
		err = -EBUSY;
		goto exit_put;
	}

	if ((err = ext3_journal_get_write_access(handle,
						 EXT3_SB(sb)->s_sbh))) {
		ext3_warning(sb, __func__,
			     "error %d on journal write access", err);
		mutex_unlock(&EXT3_SB(sb)->s_resize_lock);
		ext3_journal_stop(handle);
		goto exit_put;
	}
	es->s_blocks_count = cpu_to_le32(o_blocks_count + add);
	err = ext3_journal_dirty_metadata(handle, EXT3_SB(sb)->s_sbh);
	mutex_unlock(&EXT3_SB(sb)->s_resize_lock);
	if (err) {
		ext3_warning(sb, __func__,
			     "error %d on journal dirty metadata", err);
		ext3_journal_stop(handle);
		goto exit_put;
	}
	ext3_debug("freeing blocks "E3FSBLK" through "E3FSBLK"\n",
		   o_blocks_count, o_blocks_count + add);
	ext3_free_blocks_sb(handle, sb, o_blocks_count, add, &freed_blocks);
	ext3_debug("freed blocks "E3FSBLK" through "E3FSBLK"\n",
		   o_blocks_count, o_blocks_count + add);
	if ((err = ext3_journal_stop(handle)))
		goto exit_put;
	if (test_opt(sb, DEBUG))
		printk(KERN_DEBUG "EXT3-fs: extended group to %u blocks\n",
		       le32_to_cpu(es->s_blocks_count));
	update_backups(sb, EXT3_SB(sb)->s_sbh->b_blocknr, (char *)es,
		       sizeof(struct ext3_super_block));
exit_put:
	return err;
} 
