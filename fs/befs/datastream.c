/*
 * linux/fs/befs/datastream.c
 *
 * Copyright (C) 2001 Will Dyson <will_dyson@pobox.com>
 *
 * Based on portions of file.c by Makoto Kato <m_kato@ga2.so-net.ne.jp>
 *
 * Many thanks to Dominic Giampaolo, author of "Practical File System
 * Design with the Be File System", for such a helpful book.
 *
 */

#include <linux/kernel.h>
#include <linux/buffer_head.h>
#include <linux/string.h>

#include "befs.h"
#include "datastream.h"
#include "io.h"

const befs_inode_addr BAD_IADDR = { 0, 0, 0 };

static int befs_find_brun_direct(struct super_block *sb,
				 befs_data_stream * data,
				 befs_blocknr_t blockno, befs_block_run * run);

static int befs_find_brun_indirect(struct super_block *sb,
				   befs_data_stream * data,
				   befs_blocknr_t blockno,
				   befs_block_run * run);

static int befs_find_brun_dblindirect(struct super_block *sb,
				      befs_data_stream * data,
				      befs_blocknr_t blockno,
				      befs_block_run * run);

struct buffer_head *
befs_read_datastream(struct super_block *sb, befs_data_stream * ds,
		     befs_off_t pos, uint * off)
{
	struct buffer_head *bh = NULL;
	befs_block_run run;
	befs_blocknr_t block;	

	befs_debug(sb, "---> befs_read_datastream() %Lu", pos);
	block = pos >> BEFS_SB(sb)->block_shift;
	if (off)
		*off = pos - (block << BEFS_SB(sb)->block_shift);

	if (befs_fblock2brun(sb, ds, block, &run) != BEFS_OK) {
		befs_error(sb, "BeFS: Error finding disk addr of block %lu",
			   block);
		befs_debug(sb, "<--- befs_read_datastream() ERROR");
		return NULL;
	}
	bh = befs_bread_iaddr(sb, run);
	if (!bh) {
		befs_error(sb, "BeFS: Error reading block %lu from datastream",
			   block);
		return NULL;
	}

	befs_debug(sb, "<--- befs_read_datastream() read data, starting at %Lu",
		   pos);

	return bh;
}

int
befs_fblock2brun(struct super_block *sb, befs_data_stream * data,
		 befs_blocknr_t fblock, befs_block_run * run)
{
	int err;
	befs_off_t pos = fblock << BEFS_SB(sb)->block_shift;

	if (pos < data->max_direct_range) {
		err = befs_find_brun_direct(sb, data, fblock, run);

	} else if (pos < data->max_indirect_range) {
		err = befs_find_brun_indirect(sb, data, fblock, run);

	} else if (pos < data->max_double_indirect_range) {
		err = befs_find_brun_dblindirect(sb, data, fblock, run);

	} else {
		befs_error(sb,
			   "befs_fblock2brun() was asked to find block %lu, "
			   "which is not mapped by the datastream\n", fblock);
		err = BEFS_ERR;
	}
	return err;
}

size_t
befs_read_lsymlink(struct super_block * sb, befs_data_stream * ds, void *buff,
		   befs_off_t len)
{
	befs_off_t bytes_read = 0;	
	u16 plen;
	struct buffer_head *bh = NULL;
	befs_debug(sb, "---> befs_read_lsymlink() length: %Lu", len);

	while (bytes_read < len) {
		bh = befs_read_datastream(sb, ds, bytes_read, NULL);
		if (!bh) {
			befs_error(sb, "BeFS: Error reading datastream block "
				   "starting from %Lu", bytes_read);
			befs_debug(sb, "<--- befs_read_lsymlink() ERROR");
			return bytes_read;

		}
		plen = ((bytes_read + BEFS_SB(sb)->block_size) < len) ?
		    BEFS_SB(sb)->block_size : len - bytes_read;
		memcpy(buff + bytes_read, bh->b_data, plen);
		brelse(bh);
		bytes_read += plen;
	}

	befs_debug(sb, "<--- befs_read_lsymlink() read %u bytes", bytes_read);
	return bytes_read;
}


befs_blocknr_t
befs_count_blocks(struct super_block * sb, befs_data_stream * ds)
{
	befs_blocknr_t blocks;
	befs_blocknr_t datablocks;	
	befs_blocknr_t metablocks;	
	befs_sb_info *befs_sb = BEFS_SB(sb);

	befs_debug(sb, "---> befs_count_blocks()");

	datablocks = ds->size >> befs_sb->block_shift;
	if (ds->size & (befs_sb->block_size - 1))
		datablocks += 1;

	metablocks = 1;		

	
	if (ds->size > ds->max_direct_range)
		metablocks += ds->indirect.len;

	if (ds->size > ds->max_indirect_range && ds->max_indirect_range != 0) {
		uint dbl_bytes;
		uint dbl_bruns;
		uint indirblocks;

		dbl_bytes =
		    ds->max_double_indirect_range - ds->max_indirect_range;
		dbl_bruns =
		    dbl_bytes / (befs_sb->block_size * BEFS_DBLINDIR_BRUN_LEN);
		indirblocks = dbl_bruns / befs_iaddrs_per_block(sb);

		metablocks += ds->double_indirect.len;
		metablocks += indirblocks;
	}

	blocks = datablocks + metablocks;
	befs_debug(sb, "<--- befs_count_blocks() %u blocks", blocks);

	return blocks;
}

static int
befs_find_brun_direct(struct super_block *sb, befs_data_stream * data,
		      befs_blocknr_t blockno, befs_block_run * run)
{
	int i;
	befs_block_run *array = data->direct;
	befs_blocknr_t sum;
	befs_blocknr_t max_block =
	    data->max_direct_range >> BEFS_SB(sb)->block_shift;

	befs_debug(sb, "---> befs_find_brun_direct(), find %lu", blockno);

	if (blockno > max_block) {
		befs_error(sb, "befs_find_brun_direct() passed block outside of"
			   "direct region");
		return BEFS_ERR;
	}

	for (i = 0, sum = 0; i < BEFS_NUM_DIRECT_BLOCKS;
	     sum += array[i].len, i++) {
		if (blockno >= sum && blockno < sum + (array[i].len)) {
			int offset = blockno - sum;
			run->allocation_group = array[i].allocation_group;
			run->start = array[i].start + offset;
			run->len = array[i].len - offset;

			befs_debug(sb, "---> befs_find_brun_direct(), "
				   "found %lu at direct[%d]", blockno, i);
			return BEFS_OK;
		}
	}

	befs_debug(sb, "---> befs_find_brun_direct() ERROR");
	return BEFS_ERR;
}

static int
befs_find_brun_indirect(struct super_block *sb,
			befs_data_stream * data, befs_blocknr_t blockno,
			befs_block_run * run)
{
	int i, j;
	befs_blocknr_t sum = 0;
	befs_blocknr_t indir_start_blk;
	befs_blocknr_t search_blk;
	struct buffer_head *indirblock;
	befs_disk_block_run *array;

	befs_block_run indirect = data->indirect;
	befs_blocknr_t indirblockno = iaddr2blockno(sb, &indirect);
	int arraylen = befs_iaddrs_per_block(sb);

	befs_debug(sb, "---> befs_find_brun_indirect(), find %lu", blockno);

	indir_start_blk = data->max_direct_range >> BEFS_SB(sb)->block_shift;
	search_blk = blockno - indir_start_blk;

	
	for (i = 0; i < indirect.len; i++) {
		indirblock = befs_bread(sb, indirblockno + i);
		if (indirblock == NULL) {
			befs_debug(sb,
				   "---> befs_find_brun_indirect() failed to "
				   "read disk block %lu from the indirect brun",
				   indirblockno + i);
			return BEFS_ERR;
		}

		array = (befs_disk_block_run *) indirblock->b_data;

		for (j = 0; j < arraylen; ++j) {
			int len = fs16_to_cpu(sb, array[j].len);

			if (search_blk >= sum && search_blk < sum + len) {
				int offset = search_blk - sum;
				run->allocation_group =
				    fs32_to_cpu(sb, array[j].allocation_group);
				run->start =
				    fs16_to_cpu(sb, array[j].start) + offset;
				run->len =
				    fs16_to_cpu(sb, array[j].len) - offset;

				brelse(indirblock);
				befs_debug(sb,
					   "<--- befs_find_brun_indirect() found "
					   "file block %lu at indirect[%d]",
					   blockno, j + (i * arraylen));
				return BEFS_OK;
			}
			sum += len;
		}

		brelse(indirblock);
	}

	
	befs_error(sb, "BeFS: befs_find_brun_indirect() failed to find "
		   "file block %lu", blockno);

	befs_debug(sb, "<--- befs_find_brun_indirect() ERROR");
	return BEFS_ERR;
}

static int
befs_find_brun_dblindirect(struct super_block *sb,
			   befs_data_stream * data, befs_blocknr_t blockno,
			   befs_block_run * run)
{
	int dblindir_indx;
	int indir_indx;
	int offset;
	int dbl_which_block;
	int which_block;
	int dbl_block_indx;
	int block_indx;
	off_t dblindir_leftover;
	befs_blocknr_t blockno_at_run_start;
	struct buffer_head *dbl_indir_block;
	struct buffer_head *indir_block;
	befs_block_run indir_run;
	befs_disk_inode_addr *iaddr_array = NULL;
	befs_sb_info *befs_sb = BEFS_SB(sb);

	befs_blocknr_t indir_start_blk =
	    data->max_indirect_range >> befs_sb->block_shift;

	off_t dbl_indir_off = blockno - indir_start_blk;

	size_t iblklen = BEFS_DBLINDIR_BRUN_LEN;

	size_t diblklen = iblklen * befs_iaddrs_per_block(sb)
	    * BEFS_DBLINDIR_BRUN_LEN;

	befs_debug(sb, "---> befs_find_brun_dblindirect() find %lu", blockno);


	dblindir_indx = dbl_indir_off / diblklen;
	dblindir_leftover = dbl_indir_off % diblklen;
	indir_indx = dblindir_leftover / diblklen;

	
	dbl_which_block = dblindir_indx / befs_iaddrs_per_block(sb);
	if (dbl_which_block > data->double_indirect.len) {
		befs_error(sb, "The double-indirect index calculated by "
			   "befs_read_brun_dblindirect(), %d, is outside the range "
			   "of the double-indirect block", dblindir_indx);
		return BEFS_ERR;
	}

	dbl_indir_block =
	    befs_bread(sb, iaddr2blockno(sb, &data->double_indirect) +
					dbl_which_block);
	if (dbl_indir_block == NULL) {
		befs_error(sb, "befs_read_brun_dblindirect() couldn't read the "
			   "double-indirect block at blockno %lu",
			   iaddr2blockno(sb,
					 &data->double_indirect) +
			   dbl_which_block);
		brelse(dbl_indir_block);
		return BEFS_ERR;
	}

	dbl_block_indx =
	    dblindir_indx - (dbl_which_block * befs_iaddrs_per_block(sb));
	iaddr_array = (befs_disk_inode_addr *) dbl_indir_block->b_data;
	indir_run = fsrun_to_cpu(sb, iaddr_array[dbl_block_indx]);
	brelse(dbl_indir_block);
	iaddr_array = NULL;

	
	which_block = indir_indx / befs_iaddrs_per_block(sb);
	if (which_block > indir_run.len) {
		befs_error(sb, "The indirect index calculated by "
			   "befs_read_brun_dblindirect(), %d, is outside the range "
			   "of the indirect block", indir_indx);
		return BEFS_ERR;
	}

	indir_block =
	    befs_bread(sb, iaddr2blockno(sb, &indir_run) + which_block);
	if (indir_block == NULL) {
		befs_error(sb, "befs_read_brun_dblindirect() couldn't read the "
			   "indirect block at blockno %lu",
			   iaddr2blockno(sb, &indir_run) + which_block);
		brelse(indir_block);
		return BEFS_ERR;
	}

	block_indx = indir_indx - (which_block * befs_iaddrs_per_block(sb));
	iaddr_array = (befs_disk_inode_addr *) indir_block->b_data;
	*run = fsrun_to_cpu(sb, iaddr_array[block_indx]);
	brelse(indir_block);
	iaddr_array = NULL;

	blockno_at_run_start = indir_start_blk;
	blockno_at_run_start += diblklen * dblindir_indx;
	blockno_at_run_start += iblklen * indir_indx;
	offset = blockno - blockno_at_run_start;

	run->start += offset;
	run->len -= offset;

	befs_debug(sb, "Found file block %lu in double_indirect[%d][%d],"
		   " double_indirect_leftover = %lu",
		   blockno, dblindir_indx, indir_indx, dblindir_leftover);

	return BEFS_OK;
}
