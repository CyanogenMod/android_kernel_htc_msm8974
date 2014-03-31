/*
 * YAFFS: Yet another FFS. A NAND-flash specific file system.
 *
 * Copyright (C) 2002-2010 Aleph One Ltd.
 *   for Toby Churchill Ltd and Brightstar Engineering
 *
 * Created by Charles Manning <charles@aleph1.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include "yportenv.h"
#include "yaffs_trace.h"
#include "yaffs_guts.h"
#include "yaffs_packedtags1.h"
#include "yaffs_tagscompat.h"	
#include "yaffs_linux.h"

#include "linux/kernel.h"
#include "linux/version.h"
#include "linux/types.h"
#include "linux/mtd/mtd.h"

#ifndef CONFIG_YAFFS_9BYTE_TAGS
# define YTAG1_SIZE 8
#else
# define YTAG1_SIZE 9
#endif

/* Write a chunk (page) of data to NAND.
 *
 * Caller always provides ExtendedTags data which are converted to a more
 * compact (packed) form for storage in NAND.  A mini-ECC runs over the
 * contents of the tags meta-data; used to valid the tags when read.
 *
 *  - Pack ExtendedTags to packed_tags1 form
 *  - Compute mini-ECC for packed_tags1
 *  - Write data and packed tags to NAND.
 *
 * Note: Due to the use of the packed_tags1 meta-data which does not include
 * a full sequence number (as found in the larger packed_tags2 form) it is
 * necessary for Yaffs to re-write a chunk/page (just once) to mark it as
 * discarded and dirty.  This is not ideal: newer NAND parts are supposed
 * to be written just once.  When Yaffs performs this operation, this
 * function is called with a NULL data pointer -- calling MTD write_oob
 * without data is valid usage (2.6.17).
 *
 * Any underlying MTD error results in YAFFS_FAIL.
 * Returns YAFFS_OK or YAFFS_FAIL.
 */
int nandmtd1_write_chunk_tags(struct yaffs_dev *dev,
			      int nand_chunk, const u8 * data,
			      const struct yaffs_ext_tags *etags)
{
	struct mtd_info *mtd = yaffs_dev_to_mtd(dev);
	int chunk_bytes = dev->data_bytes_per_chunk;
	loff_t addr = ((loff_t) nand_chunk) * chunk_bytes;
	struct mtd_oob_ops ops;
	struct yaffs_packed_tags1 pt1;
	int retval;

	
	compile_time_assertion(sizeof(struct yaffs_packed_tags1) == 12);
	compile_time_assertion(sizeof(struct yaffs_tags) == 8);

	yaffs_pack_tags1(&pt1, etags);
	yaffs_calc_tags_ecc((struct yaffs_tags *)&pt1);

#ifndef CONFIG_YAFFS_9BYTE_TAGS
	if (etags->is_deleted) {
		memset(&pt1, 0xff, 8);
		
		pt1.deleted = 0;
	}
#else
	((u8 *) & pt1)[8] = 0xff;
	if (etags->is_deleted) {
		memset(&pt1, 0xff, 8);
		
		((u8 *) & pt1)[8] = 0;
	}
#endif

	memset(&ops, 0, sizeof(ops));
	ops.mode = MTD_OPS_AUTO_OOB;
	ops.len = (data) ? chunk_bytes : 0;
	ops.ooblen = YTAG1_SIZE;
	ops.datbuf = (u8 *) data;
	ops.oobbuf = (u8 *) & pt1;

	retval = mtd_write_oob(mtd, addr, &ops);
	if (retval) {
		yaffs_trace(YAFFS_TRACE_MTD,
			"write_oob failed, chunk %d, mtd error %d",
			nand_chunk, retval);
	}
	return retval ? YAFFS_FAIL : YAFFS_OK;
}

static int rettags(struct yaffs_ext_tags *etags, int ecc_result, int retval)
{
	if (etags) {
		memset(etags, 0, sizeof(*etags));
		etags->ecc_result = ecc_result;
	}
	return retval;
}

int nandmtd1_read_chunk_tags(struct yaffs_dev *dev,
			     int nand_chunk, u8 * data,
			     struct yaffs_ext_tags *etags)
{
	struct mtd_info *mtd = yaffs_dev_to_mtd(dev);
	int chunk_bytes = dev->data_bytes_per_chunk;
	loff_t addr = ((loff_t) nand_chunk) * chunk_bytes;
	int eccres = YAFFS_ECC_RESULT_NO_ERROR;
	struct mtd_oob_ops ops;
	struct yaffs_packed_tags1 pt1;
	int retval;
	int deleted;

	memset(&ops, 0, sizeof(ops));
	ops.mode = MTD_OPS_AUTO_OOB;
	ops.len = (data) ? chunk_bytes : 0;
	ops.ooblen = YTAG1_SIZE;
	ops.datbuf = data;
	ops.oobbuf = (u8 *) & pt1;

	retval = mtd_read_oob(mtd, addr, &ops);
	if (retval) {
		yaffs_trace(YAFFS_TRACE_MTD,
			"read_oob failed, chunk %d, mtd error %d",
			nand_chunk, retval);
	}

	switch (retval) {
	case 0:
		
		break;

	case -EUCLEAN:
		
		eccres = YAFFS_ECC_RESULT_FIXED;
		dev->n_ecc_fixed++;
		break;

	case -EBADMSG:
		
		dev->n_ecc_unfixed++;
		
	default:
		rettags(etags, YAFFS_ECC_RESULT_UNFIXED, 0);
		etags->block_bad = mtd_block_isbad(mtd, addr);
		return YAFFS_FAIL;
	}

	if (yaffs_check_ff((u8 *) & pt1, 8)) {
		
		return rettags(etags, YAFFS_ECC_RESULT_NO_ERROR, YAFFS_OK);
	}
#ifndef CONFIG_YAFFS_9BYTE_TAGS
	deleted = !pt1.deleted;
	pt1.deleted = 1;
#else
	deleted = (yaffs_count_bits(((u8 *) & pt1)[8]) < 7);
#endif

	retval = yaffs_check_tags_ecc((struct yaffs_tags *)&pt1);
	switch (retval) {
	case 0:
		
		break;
	case 1:
		
		dev->n_tags_ecc_fixed++;
		if (eccres == YAFFS_ECC_RESULT_NO_ERROR)
			eccres = YAFFS_ECC_RESULT_FIXED;
		break;
	default:
		
		dev->n_tags_ecc_unfixed++;
		return rettags(etags, YAFFS_ECC_RESULT_UNFIXED, YAFFS_FAIL);
	}

	pt1.should_be_ff = 0xFFFFFFFF;
	yaffs_unpack_tags1(etags, &pt1);
	etags->ecc_result = eccres;

	
	etags->is_deleted = deleted;
	return YAFFS_OK;
}

int nandmtd1_mark_block_bad(struct yaffs_dev *dev, int block_no)
{
	struct mtd_info *mtd = yaffs_dev_to_mtd(dev);
	int blocksize = dev->param.chunks_per_block * dev->data_bytes_per_chunk;
	int retval;

	yaffs_trace(YAFFS_TRACE_BAD_BLOCKS,
		"marking block %d bad", block_no);

	retval = mtd_block_markbad(mtd, (loff_t) blocksize * block_no);
	return (retval) ? YAFFS_FAIL : YAFFS_OK;
}

static int nandmtd1_test_prerequists(struct mtd_info *mtd)
{
	
	
	int oobavail = mtd->ecclayout->oobavail;

	if (oobavail < YTAG1_SIZE) {
		yaffs_trace(YAFFS_TRACE_ERROR,
			"mtd device has only %d bytes for tags, need %d",
			oobavail, YTAG1_SIZE);
		return YAFFS_FAIL;
	}
	return YAFFS_OK;
}

int nandmtd1_query_block(struct yaffs_dev *dev, int block_no,
			 enum yaffs_block_state *state_ptr, u32 * seq_ptr)
{
	struct mtd_info *mtd = yaffs_dev_to_mtd(dev);
	int chunk_num = block_no * dev->param.chunks_per_block;
	loff_t addr = (loff_t) chunk_num * dev->data_bytes_per_chunk;
	struct yaffs_ext_tags etags;
	int state = YAFFS_BLOCK_STATE_DEAD;
	int seqnum = 0;
	int retval;

	if (nandmtd1_test_prerequists(mtd) != YAFFS_OK)
		return YAFFS_FAIL;

	retval = nandmtd1_read_chunk_tags(dev, chunk_num, NULL, &etags);
	etags.block_bad = mtd_block_isbad(mtd, addr);
	if (etags.block_bad) {
		yaffs_trace(YAFFS_TRACE_BAD_BLOCKS,
			"block %d is marked bad", block_no);
		state = YAFFS_BLOCK_STATE_DEAD;
	} else if (etags.ecc_result != YAFFS_ECC_RESULT_NO_ERROR) {
		
		state = YAFFS_BLOCK_STATE_NEEDS_SCANNING;
	} else if (etags.chunk_used) {
		state = YAFFS_BLOCK_STATE_NEEDS_SCANNING;
		seqnum = etags.seq_number;
	} else {
		state = YAFFS_BLOCK_STATE_EMPTY;
	}

	*state_ptr = state;
	*seq_ptr = seqnum;

	
	return YAFFS_OK;
}
