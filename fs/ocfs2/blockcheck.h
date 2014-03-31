/* -*- mode: c; c-basic-offset: 8; -*-
 * vim: noexpandtab sw=8 ts=8 sts=0:
 *
 * blockcheck.h
 *
 * Checksum and ECC codes for the OCFS2 userspace library.
 *
 * Copyright (C) 2004, 2008 Oracle.  All rights reserved.
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

#ifndef OCFS2_BLOCKCHECK_H
#define OCFS2_BLOCKCHECK_H


struct ocfs2_blockcheck_stats {
	spinlock_t b_lock;
	u64 b_check_count;	
	u64 b_failure_count;	
	u64 b_recover_count;	

	struct dentry *b_debug_dir;	
	struct dentry *b_debug_check;	
	struct dentry *b_debug_failure;	
	struct dentry *b_debug_recover;	
};


void ocfs2_compute_meta_ecc(struct super_block *sb, void *data,
			    struct ocfs2_block_check *bc);
int ocfs2_validate_meta_ecc(struct super_block *sb, void *data,
			    struct ocfs2_block_check *bc);
void ocfs2_compute_meta_ecc_bhs(struct super_block *sb,
				struct buffer_head **bhs, int nr,
				struct ocfs2_block_check *bc);
int ocfs2_validate_meta_ecc_bhs(struct super_block *sb,
				struct buffer_head **bhs, int nr,
				struct ocfs2_block_check *bc);

void ocfs2_block_check_compute(void *data, size_t blocksize,
			       struct ocfs2_block_check *bc);
int ocfs2_block_check_validate(void *data, size_t blocksize,
			       struct ocfs2_block_check *bc,
			       struct ocfs2_blockcheck_stats *stats);
void ocfs2_block_check_compute_bhs(struct buffer_head **bhs, int nr,
				   struct ocfs2_block_check *bc);
int ocfs2_block_check_validate_bhs(struct buffer_head **bhs, int nr,
				   struct ocfs2_block_check *bc,
				   struct ocfs2_blockcheck_stats *stats);

int ocfs2_blockcheck_stats_debugfs_install(struct ocfs2_blockcheck_stats *stats,
					   struct dentry *parent);
void ocfs2_blockcheck_stats_debugfs_remove(struct ocfs2_blockcheck_stats *stats);


u32 ocfs2_hamming_encode(u32 parity, void *data, unsigned int d,
			 unsigned int nr);
void ocfs2_hamming_fix(void *data, unsigned int d, unsigned int nr,
		       unsigned int fix);

extern u32 ocfs2_hamming_encode_block(void *data, unsigned int blocksize);
extern void ocfs2_hamming_fix_block(void *data, unsigned int blocksize,
				    unsigned int fix);
#endif
