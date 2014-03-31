/* -*- mode: c; c-basic-offset: 8; -*-
 * vim: noexpandtab sw=8 ts=8 sts=0:
 *
 * blockcheck.c
 *
 * Checksum and ECC codes for the OCFS2 userspace library.
 *
 * Copyright (C) 2006, 2008 Oracle.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/crc32.h>
#include <linux/buffer_head.h>
#include <linux/bitops.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/byteorder.h>

#include <cluster/masklog.h>

#include "ocfs2.h"

#include "blockcheck.h"




static unsigned int calc_code_bit(unsigned int i, unsigned int *p_cache)
{
	unsigned int b, p = 0;

	b = i + 1;

	
	if (p_cache)
		p = *p_cache;
        b += p;

	for (; (1 << p) < (b + 1); p++)
		b++;

	if (p_cache)
		*p_cache = p;

	return b;
}

u32 ocfs2_hamming_encode(u32 parity, void *data, unsigned int d, unsigned int nr)
{
	unsigned int i, b, p = 0;

	BUG_ON(!d);

	for (i = 0; (i = ocfs2_find_next_bit(data, d, i)) < d; i++)
	{
		b = calc_code_bit(nr + i, &p);

		parity ^= b;
	}

	return parity;
}

u32 ocfs2_hamming_encode_block(void *data, unsigned int blocksize)
{
	return ocfs2_hamming_encode(0, data, blocksize * 8, 0);
}

void ocfs2_hamming_fix(void *data, unsigned int d, unsigned int nr,
		       unsigned int fix)
{
	unsigned int i, b;

	BUG_ON(!d);

	if (hweight32(fix) == 1)
		return;

	if (fix >= calc_code_bit(nr + d, NULL))
		return;

	b = calc_code_bit(nr, NULL);
	
	if (fix < b)
		return;

	for (i = 0; i < d; i++, b++)
	{
		
		while (hweight32(b) == 1)
			b++;

		if (b == fix)
		{
			if (ocfs2_test_bit(i, data))
				ocfs2_clear_bit(i, data);
			else
				ocfs2_set_bit(i, data);
			break;
		}
	}
}

void ocfs2_hamming_fix_block(void *data, unsigned int blocksize,
			     unsigned int fix)
{
	ocfs2_hamming_fix(data, blocksize * 8, 0, fix);
}



#ifdef CONFIG_DEBUG_FS

static int blockcheck_u64_get(void *data, u64 *val)
{
	*val = *(u64 *)data;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(blockcheck_fops, blockcheck_u64_get, NULL, "%llu\n");

static struct dentry *blockcheck_debugfs_create(const char *name,
						struct dentry *parent,
						u64 *value)
{
	return debugfs_create_file(name, S_IFREG | S_IRUSR, parent, value,
				   &blockcheck_fops);
}

static void ocfs2_blockcheck_debug_remove(struct ocfs2_blockcheck_stats *stats)
{
	if (stats) {
		debugfs_remove(stats->b_debug_check);
		stats->b_debug_check = NULL;
		debugfs_remove(stats->b_debug_failure);
		stats->b_debug_failure = NULL;
		debugfs_remove(stats->b_debug_recover);
		stats->b_debug_recover = NULL;
		debugfs_remove(stats->b_debug_dir);
		stats->b_debug_dir = NULL;
	}
}

static int ocfs2_blockcheck_debug_install(struct ocfs2_blockcheck_stats *stats,
					  struct dentry *parent)
{
	int rc = -EINVAL;

	if (!stats)
		goto out;

	stats->b_debug_dir = debugfs_create_dir("blockcheck", parent);
	if (!stats->b_debug_dir)
		goto out;

	stats->b_debug_check =
		blockcheck_debugfs_create("blocks_checked",
					  stats->b_debug_dir,
					  &stats->b_check_count);

	stats->b_debug_failure =
		blockcheck_debugfs_create("checksums_failed",
					  stats->b_debug_dir,
					  &stats->b_failure_count);

	stats->b_debug_recover =
		blockcheck_debugfs_create("ecc_recoveries",
					  stats->b_debug_dir,
					  &stats->b_recover_count);
	if (stats->b_debug_check && stats->b_debug_failure &&
	    stats->b_debug_recover)
		rc = 0;

out:
	if (rc)
		ocfs2_blockcheck_debug_remove(stats);
	return rc;
}
#else
static inline int ocfs2_blockcheck_debug_install(struct ocfs2_blockcheck_stats *stats,
						 struct dentry *parent)
{
	return 0;
}

static inline void ocfs2_blockcheck_debug_remove(struct ocfs2_blockcheck_stats *stats)
{
}
#endif  

int ocfs2_blockcheck_stats_debugfs_install(struct ocfs2_blockcheck_stats *stats,
					   struct dentry *parent)
{
	return ocfs2_blockcheck_debug_install(stats, parent);
}

void ocfs2_blockcheck_stats_debugfs_remove(struct ocfs2_blockcheck_stats *stats)
{
	ocfs2_blockcheck_debug_remove(stats);
}

static void ocfs2_blockcheck_inc_check(struct ocfs2_blockcheck_stats *stats)
{
	u64 new_count;

	if (!stats)
		return;

	spin_lock(&stats->b_lock);
	stats->b_check_count++;
	new_count = stats->b_check_count;
	spin_unlock(&stats->b_lock);

	if (!new_count)
		mlog(ML_NOTICE, "Block check count has wrapped\n");
}

static void ocfs2_blockcheck_inc_failure(struct ocfs2_blockcheck_stats *stats)
{
	u64 new_count;

	if (!stats)
		return;

	spin_lock(&stats->b_lock);
	stats->b_failure_count++;
	new_count = stats->b_failure_count;
	spin_unlock(&stats->b_lock);

	if (!new_count)
		mlog(ML_NOTICE, "Checksum failure count has wrapped\n");
}

static void ocfs2_blockcheck_inc_recover(struct ocfs2_blockcheck_stats *stats)
{
	u64 new_count;

	if (!stats)
		return;

	spin_lock(&stats->b_lock);
	stats->b_recover_count++;
	new_count = stats->b_recover_count;
	spin_unlock(&stats->b_lock);

	if (!new_count)
		mlog(ML_NOTICE, "ECC recovery count has wrapped\n");
}




void ocfs2_block_check_compute(void *data, size_t blocksize,
			       struct ocfs2_block_check *bc)
{
	u32 crc;
	u32 ecc;

	memset(bc, 0, sizeof(struct ocfs2_block_check));

	crc = crc32_le(~0, data, blocksize);
	ecc = ocfs2_hamming_encode_block(data, blocksize);

	BUG_ON(ecc > USHRT_MAX);

	bc->bc_crc32e = cpu_to_le32(crc);
	bc->bc_ecc = cpu_to_le16((u16)ecc);
}

int ocfs2_block_check_validate(void *data, size_t blocksize,
			       struct ocfs2_block_check *bc,
			       struct ocfs2_blockcheck_stats *stats)
{
	int rc = 0;
	struct ocfs2_block_check check;
	u32 crc, ecc;

	ocfs2_blockcheck_inc_check(stats);

	check.bc_crc32e = le32_to_cpu(bc->bc_crc32e);
	check.bc_ecc = le16_to_cpu(bc->bc_ecc);

	memset(bc, 0, sizeof(struct ocfs2_block_check));

	
	crc = crc32_le(~0, data, blocksize);
	if (crc == check.bc_crc32e)
		goto out;

	ocfs2_blockcheck_inc_failure(stats);
	mlog(ML_ERROR,
	     "CRC32 failed: stored: 0x%x, computed 0x%x. Applying ECC.\n",
	     (unsigned int)check.bc_crc32e, (unsigned int)crc);

	
	ecc = ocfs2_hamming_encode_block(data, blocksize);
	ocfs2_hamming_fix_block(data, blocksize, ecc ^ check.bc_ecc);

	
	crc = crc32_le(~0, data, blocksize);
	if (crc == check.bc_crc32e) {
		ocfs2_blockcheck_inc_recover(stats);
		goto out;
	}

	mlog(ML_ERROR, "Fixed CRC32 failed: stored: 0x%x, computed 0x%x\n",
	     (unsigned int)check.bc_crc32e, (unsigned int)crc);

	rc = -EIO;

out:
	bc->bc_crc32e = cpu_to_le32(check.bc_crc32e);
	bc->bc_ecc = cpu_to_le16(check.bc_ecc);

	return rc;
}

void ocfs2_block_check_compute_bhs(struct buffer_head **bhs, int nr,
				   struct ocfs2_block_check *bc)
{
	int i;
	u32 crc, ecc;

	BUG_ON(nr < 0);

	if (!nr)
		return;

	memset(bc, 0, sizeof(struct ocfs2_block_check));

	for (i = 0, crc = ~0, ecc = 0; i < nr; i++) {
		crc = crc32_le(crc, bhs[i]->b_data, bhs[i]->b_size);
		ecc = (u16)ocfs2_hamming_encode(ecc, bhs[i]->b_data,
						bhs[i]->b_size * 8,
						bhs[i]->b_size * 8 * i);
	}

	BUG_ON(ecc > USHRT_MAX);

	bc->bc_crc32e = cpu_to_le32(crc);
	bc->bc_ecc = cpu_to_le16((u16)ecc);
}

int ocfs2_block_check_validate_bhs(struct buffer_head **bhs, int nr,
				   struct ocfs2_block_check *bc,
				   struct ocfs2_blockcheck_stats *stats)
{
	int i, rc = 0;
	struct ocfs2_block_check check;
	u32 crc, ecc, fix;

	BUG_ON(nr < 0);

	if (!nr)
		return 0;

	ocfs2_blockcheck_inc_check(stats);

	check.bc_crc32e = le32_to_cpu(bc->bc_crc32e);
	check.bc_ecc = le16_to_cpu(bc->bc_ecc);

	memset(bc, 0, sizeof(struct ocfs2_block_check));

	
	for (i = 0, crc = ~0; i < nr; i++)
		crc = crc32_le(crc, bhs[i]->b_data, bhs[i]->b_size);
	if (crc == check.bc_crc32e)
		goto out;

	ocfs2_blockcheck_inc_failure(stats);
	mlog(ML_ERROR,
	     "CRC32 failed: stored: %u, computed %u.  Applying ECC.\n",
	     (unsigned int)check.bc_crc32e, (unsigned int)crc);

	
	for (i = 0, ecc = 0; i < nr; i++) {
		ecc = (u16)ocfs2_hamming_encode(ecc, bhs[i]->b_data,
						bhs[i]->b_size * 8,
						bhs[i]->b_size * 8 * i);
	}
	fix = ecc ^ check.bc_ecc;
	for (i = 0; i < nr; i++) {
		ocfs2_hamming_fix(bhs[i]->b_data, bhs[i]->b_size * 8,
				  bhs[i]->b_size * 8 * i, fix);
	}

	
	for (i = 0, crc = ~0; i < nr; i++)
		crc = crc32_le(crc, bhs[i]->b_data, bhs[i]->b_size);
	if (crc == check.bc_crc32e) {
		ocfs2_blockcheck_inc_recover(stats);
		goto out;
	}

	mlog(ML_ERROR, "Fixed CRC32 failed: stored: %u, computed %u\n",
	     (unsigned int)check.bc_crc32e, (unsigned int)crc);

	rc = -EIO;

out:
	bc->bc_crc32e = cpu_to_le32(check.bc_crc32e);
	bc->bc_ecc = cpu_to_le16(check.bc_ecc);

	return rc;
}

void ocfs2_compute_meta_ecc(struct super_block *sb, void *data,
			    struct ocfs2_block_check *bc)
{
	if (ocfs2_meta_ecc(OCFS2_SB(sb)))
		ocfs2_block_check_compute(data, sb->s_blocksize, bc);
}

int ocfs2_validate_meta_ecc(struct super_block *sb, void *data,
			    struct ocfs2_block_check *bc)
{
	int rc = 0;
	struct ocfs2_super *osb = OCFS2_SB(sb);

	if (ocfs2_meta_ecc(osb))
		rc = ocfs2_block_check_validate(data, sb->s_blocksize, bc,
						&osb->osb_ecc_stats);

	return rc;
}

void ocfs2_compute_meta_ecc_bhs(struct super_block *sb,
				struct buffer_head **bhs, int nr,
				struct ocfs2_block_check *bc)
{
	if (ocfs2_meta_ecc(OCFS2_SB(sb)))
		ocfs2_block_check_compute_bhs(bhs, nr, bc);
}

int ocfs2_validate_meta_ecc_bhs(struct super_block *sb,
				struct buffer_head **bhs, int nr,
				struct ocfs2_block_check *bc)
{
	int rc = 0;
	struct ocfs2_super *osb = OCFS2_SB(sb);

	if (ocfs2_meta_ecc(osb))
		rc = ocfs2_block_check_validate_bhs(bhs, nr, bc,
						    &osb->osb_ecc_stats);

	return rc;
}

