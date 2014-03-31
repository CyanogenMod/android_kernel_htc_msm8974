/*
 *  linux/fs/ext3/fsync.c
 *
 *  Copyright (C) 1993  Stephen Tweedie (sct@redhat.com)
 *  from
 *  Copyright (C) 1992  Remy Card (card@masi.ibp.fr)
 *                      Laboratoire MASI - Institut Blaise Pascal
 *                      Universite Pierre et Marie Curie (Paris VI)
 *  from
 *  linux/fs/minix/truncate.c   Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  ext3fs fsync primitive
 *
 *  Big-endian to little-endian byte-swapping/bitmaps by
 *        David S. Miller (davem@caip.rutgers.edu), 1995
 *
 *  Removed unnecessary code duplication for little endian machines
 *  and excessive __inline__s.
 *        Andi Kleen, 1997
 *
 * Major simplications and cleanup - we only need to do the metadata, because
 * we can depend on generic_block_fdatasync() to sync the data blocks.
 */

#include <linux/blkdev.h>
#include <linux/writeback.h>
#include "ext3.h"


int ext3_sync_file(struct file *file, loff_t start, loff_t end, int datasync)
{
	struct inode *inode = file->f_mapping->host;
	struct ext3_inode_info *ei = EXT3_I(inode);
	journal_t *journal = EXT3_SB(inode->i_sb)->s_journal;
	int ret, needs_barrier = 0;
	tid_t commit_tid;

	trace_ext3_sync_file_enter(file, datasync);

	if (inode->i_sb->s_flags & MS_RDONLY)
		return 0;

	ret = filemap_write_and_wait_range(inode->i_mapping, start, end);
	if (ret)
		goto out;

	J_ASSERT(ext3_journal_current_handle() == NULL);

	if (ext3_should_journal_data(inode)) {
		ret = ext3_force_commit(inode->i_sb);
		goto out;
	}

	if (datasync)
		commit_tid = atomic_read(&ei->i_datasync_tid);
	else
		commit_tid = atomic_read(&ei->i_sync_tid);

	if (test_opt(inode->i_sb, BARRIER) &&
	    !journal_trans_will_send_data_barrier(journal, commit_tid))
		needs_barrier = 1;
	log_start_commit(journal, commit_tid);
	ret = log_wait_commit(journal, commit_tid);

	if (needs_barrier)
		blkdev_issue_flush(inode->i_sb->s_bdev, GFP_KERNEL, NULL);
out:
	trace_ext3_sync_file_exit(inode, ret);
	return ret;
}
