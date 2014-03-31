/*
 * linux/fs/jbd/commit.c
 *
 * Written by Stephen C. Tweedie <sct@redhat.com>, 1998
 *
 * Copyright 1998 Red Hat corp --- All Rights Reserved
 *
 * This file is part of the Linux kernel and is made available under
 * the terms of the GNU General Public License, version 2, or at your
 * option, any later version, incorporated herein by reference.
 *
 * Journal commit routines for the generic filesystem journaling code;
 * part of the ext2fs journaling system.
 */

#include <linux/time.h>
#include <linux/fs.h>
#include <linux/jbd.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/bio.h>
#include <linux/blkdev.h>
#include <trace/events/jbd.h>

static void journal_end_buffer_io_sync(struct buffer_head *bh, int uptodate)
{
	BUFFER_TRACE(bh, "");
	if (uptodate)
		set_buffer_uptodate(bh);
	else
		clear_buffer_uptodate(bh);
	unlock_buffer(bh);
}

static void release_buffer_page(struct buffer_head *bh)
{
	struct page *page;

	if (buffer_dirty(bh))
		goto nope;
	if (atomic_read(&bh->b_count) != 1)
		goto nope;
	page = bh->b_page;
	if (!page)
		goto nope;
	if (page->mapping)
		goto nope;

	
	if (!trylock_page(page))
		goto nope;

	page_cache_get(page);
	__brelse(bh);
	try_to_free_buffers(page);
	unlock_page(page);
	page_cache_release(page);
	return;

nope:
	__brelse(bh);
}

static void release_data_buffer(struct buffer_head *bh)
{
	if (buffer_freed(bh)) {
		clear_buffer_freed(bh);
		release_buffer_page(bh);
	} else
		put_bh(bh);
}

static int inverted_lock(journal_t *journal, struct buffer_head *bh)
{
	if (!jbd_trylock_bh_state(bh)) {
		spin_unlock(&journal->j_list_lock);
		schedule();
		return 0;
	}
	return 1;
}

static int journal_write_commit_record(journal_t *journal,
					transaction_t *commit_transaction)
{
	struct journal_head *descriptor;
	struct buffer_head *bh;
	journal_header_t *header;
	int ret;

	if (is_journal_aborted(journal))
		return 0;

	descriptor = journal_get_descriptor_buffer(journal);
	if (!descriptor)
		return 1;

	bh = jh2bh(descriptor);

	header = (journal_header_t *)(bh->b_data);
	header->h_magic = cpu_to_be32(JFS_MAGIC_NUMBER);
	header->h_blocktype = cpu_to_be32(JFS_COMMIT_BLOCK);
	header->h_sequence = cpu_to_be32(commit_transaction->t_tid);

	JBUFFER_TRACE(descriptor, "write commit block");
	set_buffer_dirty(bh);

	if (journal->j_flags & JFS_BARRIER)
		ret = __sync_dirty_buffer(bh, WRITE_SYNC | WRITE_FLUSH_FUA);
	else
		ret = sync_dirty_buffer(bh);

	put_bh(bh);		
	journal_put_journal_head(descriptor);

	return (ret == -EIO);
}

static void journal_do_submit_data(struct buffer_head **wbuf, int bufs,
				   int write_op)
{
	int i;

	for (i = 0; i < bufs; i++) {
		wbuf[i]->b_end_io = end_buffer_write_sync;
		
		submit_bh(write_op, wbuf[i]);
	}
}

static int journal_submit_data_buffers(journal_t *journal,
				       transaction_t *commit_transaction,
				       int write_op)
{
	struct journal_head *jh;
	struct buffer_head *bh;
	int locked;
	int bufs = 0;
	struct buffer_head **wbuf = journal->j_wbuf;
	int err = 0;

write_out_data:
	cond_resched();
	spin_lock(&journal->j_list_lock);

	while (commit_transaction->t_sync_datalist) {
		jh = commit_transaction->t_sync_datalist;
		bh = jh2bh(jh);
		locked = 0;

		get_bh(bh);
		if (buffer_dirty(bh)) {
			if (!trylock_buffer(bh)) {
				BUFFER_TRACE(bh, "needs blocking lock");
				spin_unlock(&journal->j_list_lock);
				trace_jbd_do_submit_data(journal,
						     commit_transaction);
				
				journal_do_submit_data(wbuf, bufs, write_op);
				bufs = 0;
				lock_buffer(bh);
				spin_lock(&journal->j_list_lock);
			}
			locked = 1;
		}
		
		if (!inverted_lock(journal, bh)) {
			jbd_lock_bh_state(bh);
			spin_lock(&journal->j_list_lock);
		}
		
		if (!buffer_jbd(bh) || bh2jh(bh) != jh
			|| jh->b_transaction != commit_transaction
			|| jh->b_jlist != BJ_SyncData) {
			jbd_unlock_bh_state(bh);
			if (locked)
				unlock_buffer(bh);
			BUFFER_TRACE(bh, "already cleaned up");
			release_data_buffer(bh);
			continue;
		}
		if (locked && test_clear_buffer_dirty(bh)) {
			BUFFER_TRACE(bh, "needs writeout, adding to array");
			wbuf[bufs++] = bh;
			__journal_file_buffer(jh, commit_transaction,
						BJ_Locked);
			jbd_unlock_bh_state(bh);
			if (bufs == journal->j_wbufsize) {
				spin_unlock(&journal->j_list_lock);
				trace_jbd_do_submit_data(journal,
						     commit_transaction);
				journal_do_submit_data(wbuf, bufs, write_op);
				bufs = 0;
				goto write_out_data;
			}
		} else if (!locked && buffer_locked(bh)) {
			__journal_file_buffer(jh, commit_transaction,
						BJ_Locked);
			jbd_unlock_bh_state(bh);
			put_bh(bh);
		} else {
			BUFFER_TRACE(bh, "writeout complete: unfile");
			if (unlikely(!buffer_uptodate(bh)))
				err = -EIO;
			__journal_unfile_buffer(jh);
			jbd_unlock_bh_state(bh);
			if (locked)
				unlock_buffer(bh);
			release_data_buffer(bh);
		}

		if (need_resched() || spin_needbreak(&journal->j_list_lock)) {
			spin_unlock(&journal->j_list_lock);
			goto write_out_data;
		}
	}
	spin_unlock(&journal->j_list_lock);
	trace_jbd_do_submit_data(journal, commit_transaction);
	journal_do_submit_data(wbuf, bufs, write_op);

	return err;
}

void journal_commit_transaction(journal_t *journal)
{
	transaction_t *commit_transaction;
	struct journal_head *jh, *new_jh, *descriptor;
	struct buffer_head **wbuf = journal->j_wbuf;
	int bufs;
	int flags;
	int err;
	unsigned int blocknr;
	ktime_t start_time;
	u64 commit_time;
	char *tagp = NULL;
	journal_header_t *header;
	journal_block_tag_t *tag = NULL;
	int space_left = 0;
	int first_tag = 0;
	int tag_flag;
	int i;
	struct blk_plug plug;


	
	if (journal->j_flags & JFS_FLUSHED) {
		jbd_debug(3, "super block updated\n");
		journal_update_superblock(journal, 1);
	} else {
		jbd_debug(3, "superblock not updated\n");
	}

	J_ASSERT(journal->j_running_transaction != NULL);
	J_ASSERT(journal->j_committing_transaction == NULL);

	commit_transaction = journal->j_running_transaction;
	J_ASSERT(commit_transaction->t_state == T_RUNNING);

	trace_jbd_start_commit(journal, commit_transaction);
	jbd_debug(1, "JBD: starting commit of transaction %d\n",
			commit_transaction->t_tid);

	spin_lock(&journal->j_state_lock);
	commit_transaction->t_state = T_LOCKED;

	trace_jbd_commit_locking(journal, commit_transaction);
	spin_lock(&commit_transaction->t_handle_lock);
	while (commit_transaction->t_updates) {
		DEFINE_WAIT(wait);

		prepare_to_wait(&journal->j_wait_updates, &wait,
					TASK_UNINTERRUPTIBLE);
		if (commit_transaction->t_updates) {
			spin_unlock(&commit_transaction->t_handle_lock);
			spin_unlock(&journal->j_state_lock);
			schedule();
			spin_lock(&journal->j_state_lock);
			spin_lock(&commit_transaction->t_handle_lock);
		}
		finish_wait(&journal->j_wait_updates, &wait);
	}
	spin_unlock(&commit_transaction->t_handle_lock);

	J_ASSERT (commit_transaction->t_outstanding_credits <=
			journal->j_max_transaction_buffers);

	while (commit_transaction->t_reserved_list) {
		jh = commit_transaction->t_reserved_list;
		JBUFFER_TRACE(jh, "reserved, unused: refile");
		if (jh->b_committed_data) {
			struct buffer_head *bh = jh2bh(jh);

			jbd_lock_bh_state(bh);
			jbd_free(jh->b_committed_data, bh->b_size);
			jh->b_committed_data = NULL;
			jbd_unlock_bh_state(bh);
		}
		journal_refile_buffer(journal, jh);
	}

	/*
	 * Now try to drop any written-back buffers from the journal's
	 * checkpoint lists.  We do this *before* commit because it potentially
	 * frees some memory
	 */
	spin_lock(&journal->j_list_lock);
	__journal_clean_checkpoint_list(journal);
	spin_unlock(&journal->j_list_lock);

	jbd_debug (3, "JBD: commit phase 1\n");

	journal_clear_buffer_revoked_flags(journal);

	journal_switch_revoke_table(journal);

	trace_jbd_commit_flushing(journal, commit_transaction);
	commit_transaction->t_state = T_FLUSH;
	journal->j_committing_transaction = commit_transaction;
	journal->j_running_transaction = NULL;
	start_time = ktime_get();
	commit_transaction->t_log_start = journal->j_head;
	wake_up(&journal->j_wait_transaction_locked);
	spin_unlock(&journal->j_state_lock);

	jbd_debug (3, "JBD: commit phase 2\n");

	blk_start_plug(&plug);
	err = journal_submit_data_buffers(journal, commit_transaction,
					  WRITE_SYNC);
	blk_finish_plug(&plug);

	spin_lock(&journal->j_list_lock);
	while (commit_transaction->t_locked_list) {
		struct buffer_head *bh;

		jh = commit_transaction->t_locked_list->b_tprev;
		bh = jh2bh(jh);
		get_bh(bh);
		if (buffer_locked(bh)) {
			spin_unlock(&journal->j_list_lock);
			wait_on_buffer(bh);
			spin_lock(&journal->j_list_lock);
		}
		if (unlikely(!buffer_uptodate(bh))) {
			if (!trylock_page(bh->b_page)) {
				spin_unlock(&journal->j_list_lock);
				lock_page(bh->b_page);
				spin_lock(&journal->j_list_lock);
			}
			if (bh->b_page->mapping)
				set_bit(AS_EIO, &bh->b_page->mapping->flags);

			unlock_page(bh->b_page);
			SetPageError(bh->b_page);
			err = -EIO;
		}
		if (!inverted_lock(journal, bh)) {
			put_bh(bh);
			spin_lock(&journal->j_list_lock);
			continue;
		}
		if (buffer_jbd(bh) && bh2jh(bh) == jh &&
		    jh->b_transaction == commit_transaction &&
		    jh->b_jlist == BJ_Locked)
			__journal_unfile_buffer(jh);
		jbd_unlock_bh_state(bh);
		release_data_buffer(bh);
		cond_resched_lock(&journal->j_list_lock);
	}
	spin_unlock(&journal->j_list_lock);

	if (err) {
		char b[BDEVNAME_SIZE];

		printk(KERN_WARNING
			"JBD: Detected IO errors while flushing file data "
			"on %s\n", bdevname(journal->j_fs_dev, b));
		if (journal->j_flags & JFS_ABORT_ON_SYNCDATA_ERR)
			journal_abort(journal, err);
		err = 0;
	}

	blk_start_plug(&plug);

	journal_write_revoke_records(journal, commit_transaction, WRITE_SYNC);

	J_ASSERT (commit_transaction->t_sync_datalist == NULL);

	jbd_debug (3, "JBD: commit phase 3\n");

	/*
	 * Way to go: we have now written out all of the data for a
	 * transaction!  Now comes the tricky part: we need to write out
	 * metadata.  Loop over the transaction's entire buffer list:
	 */
	spin_lock(&journal->j_state_lock);
	commit_transaction->t_state = T_COMMIT;
	spin_unlock(&journal->j_state_lock);

	trace_jbd_commit_logging(journal, commit_transaction);
	J_ASSERT(commit_transaction->t_nr_buffers <=
		 commit_transaction->t_outstanding_credits);

	descriptor = NULL;
	bufs = 0;
	while (commit_transaction->t_buffers) {

		

		jh = commit_transaction->t_buffers;


		if (is_journal_aborted(journal)) {
			clear_buffer_jbddirty(jh2bh(jh));
			JBUFFER_TRACE(jh, "journal is aborting: refile");
			journal_refile_buffer(journal, jh);
			if (!commit_transaction->t_buffers)
				goto start_journal_io;
			continue;
		}


		if (!descriptor) {
			struct buffer_head *bh;

			J_ASSERT (bufs == 0);

			jbd_debug(4, "JBD: get descriptor\n");

			descriptor = journal_get_descriptor_buffer(journal);
			if (!descriptor) {
				journal_abort(journal, -EIO);
				continue;
			}

			bh = jh2bh(descriptor);
			jbd_debug(4, "JBD: got buffer %llu (%p)\n",
				(unsigned long long)bh->b_blocknr, bh->b_data);
			header = (journal_header_t *)&bh->b_data[0];
			header->h_magic     = cpu_to_be32(JFS_MAGIC_NUMBER);
			header->h_blocktype = cpu_to_be32(JFS_DESCRIPTOR_BLOCK);
			header->h_sequence  = cpu_to_be32(commit_transaction->t_tid);

			tagp = &bh->b_data[sizeof(journal_header_t)];
			space_left = bh->b_size - sizeof(journal_header_t);
			first_tag = 1;
			set_buffer_jwrite(bh);
			set_buffer_dirty(bh);
			wbuf[bufs++] = bh;

			BUFFER_TRACE(bh, "ph3: file as descriptor");
			journal_file_buffer(descriptor, commit_transaction,
					BJ_LogCtl);
		}

		/* Where is the buffer to be written? */

		err = journal_next_log_block(journal, &blocknr);
		if (err) {
			journal_abort(journal, err);
			continue;
		}

		commit_transaction->t_outstanding_credits--;

		get_bh(jh2bh(jh));


		set_buffer_jwrite(jh2bh(jh));
		JBUFFER_TRACE(jh, "ph3: write metadata");
		flags = journal_write_metadata_buffer(commit_transaction,
						      jh, &new_jh, blocknr);
		set_buffer_jwrite(jh2bh(new_jh));
		wbuf[bufs++] = jh2bh(new_jh);


		tag_flag = 0;
		if (flags & 1)
			tag_flag |= JFS_FLAG_ESCAPE;
		if (!first_tag)
			tag_flag |= JFS_FLAG_SAME_UUID;

		tag = (journal_block_tag_t *) tagp;
		tag->t_blocknr = cpu_to_be32(jh2bh(jh)->b_blocknr);
		tag->t_flags = cpu_to_be32(tag_flag);
		tagp += sizeof(journal_block_tag_t);
		space_left -= sizeof(journal_block_tag_t);

		if (first_tag) {
			memcpy (tagp, journal->j_uuid, 16);
			tagp += 16;
			space_left -= 16;
			first_tag = 0;
		}


		if (bufs == journal->j_wbufsize ||
		    commit_transaction->t_buffers == NULL ||
		    space_left < sizeof(journal_block_tag_t) + 16) {

			jbd_debug(4, "JBD: Submit %d IOs\n", bufs);


			tag->t_flags |= cpu_to_be32(JFS_FLAG_LAST_TAG);

start_journal_io:
			for (i = 0; i < bufs; i++) {
				struct buffer_head *bh = wbuf[i];
				lock_buffer(bh);
				clear_buffer_dirty(bh);
				set_buffer_uptodate(bh);
				bh->b_end_io = journal_end_buffer_io_sync;
				submit_bh(WRITE_SYNC, bh);
			}
			cond_resched();

			descriptor = NULL;
			bufs = 0;
		}
	}

	blk_finish_plug(&plug);

	/* Lo and behold: we have just managed to send a transaction to
           the log.  Before we can commit it, wait for the IO so far to
           complete.  Control buffers being written are on the
           transaction's t_log_list queue, and metadata buffers are on
           the t_iobuf_list queue.

	   Wait for the buffers in reverse order.  That way we are
	   less likely to be woken up until all IOs have completed, and
	   so we incur less scheduling load.
	*/

	jbd_debug(3, "JBD: commit phase 4\n");

wait_for_iobuf:
	while (commit_transaction->t_iobuf_list != NULL) {
		struct buffer_head *bh;

		jh = commit_transaction->t_iobuf_list->b_tprev;
		bh = jh2bh(jh);
		if (buffer_locked(bh)) {
			wait_on_buffer(bh);
			goto wait_for_iobuf;
		}
		if (cond_resched())
			goto wait_for_iobuf;

		if (unlikely(!buffer_uptodate(bh)))
			err = -EIO;

		clear_buffer_jwrite(bh);

		JBUFFER_TRACE(jh, "ph4: unfile after journal write");
		journal_unfile_buffer(journal, jh);

		BUFFER_TRACE(bh, "dumping temporary bh");
		journal_put_journal_head(jh);
		__brelse(bh);
		J_ASSERT_BH(bh, atomic_read(&bh->b_count) == 0);
		free_buffer_head(bh);

		jh = commit_transaction->t_shadow_list->b_tprev;
		bh = jh2bh(jh);
		clear_buffer_jwrite(bh);
		J_ASSERT_BH(bh, buffer_jbddirty(bh));

		JBUFFER_TRACE(jh, "file as BJ_Forget");
		journal_file_buffer(jh, commit_transaction, BJ_Forget);
		smp_mb();
		wake_up_bit(&bh->b_state, BH_Unshadow);
		JBUFFER_TRACE(jh, "brelse shadowed buffer");
		__brelse(bh);
	}

	J_ASSERT (commit_transaction->t_shadow_list == NULL);

	jbd_debug(3, "JBD: commit phase 5\n");

	
 wait_for_ctlbuf:
	while (commit_transaction->t_log_list != NULL) {
		struct buffer_head *bh;

		jh = commit_transaction->t_log_list->b_tprev;
		bh = jh2bh(jh);
		if (buffer_locked(bh)) {
			wait_on_buffer(bh);
			goto wait_for_ctlbuf;
		}
		if (cond_resched())
			goto wait_for_ctlbuf;

		if (unlikely(!buffer_uptodate(bh)))
			err = -EIO;

		BUFFER_TRACE(bh, "ph5: control buffer writeout done: unfile");
		clear_buffer_jwrite(bh);
		journal_unfile_buffer(journal, jh);
		journal_put_journal_head(jh);
		__brelse(bh);		
		
	}

	if (err)
		journal_abort(journal, err);

	jbd_debug(3, "JBD: commit phase 6\n");

	/* All metadata is written, now write commit record and do cleanup */
	spin_lock(&journal->j_state_lock);
	J_ASSERT(commit_transaction->t_state == T_COMMIT);
	commit_transaction->t_state = T_COMMIT_RECORD;
	spin_unlock(&journal->j_state_lock);

	if (journal_write_commit_record(journal, commit_transaction))
		err = -EIO;

	if (err)
		journal_abort(journal, err);


	jbd_debug(3, "JBD: commit phase 7\n");

	J_ASSERT(commit_transaction->t_sync_datalist == NULL);
	J_ASSERT(commit_transaction->t_buffers == NULL);
	J_ASSERT(commit_transaction->t_checkpoint_list == NULL);
	J_ASSERT(commit_transaction->t_iobuf_list == NULL);
	J_ASSERT(commit_transaction->t_shadow_list == NULL);
	J_ASSERT(commit_transaction->t_log_list == NULL);

restart_loop:
	spin_lock(&journal->j_list_lock);
	while (commit_transaction->t_forget) {
		transaction_t *cp_transaction;
		struct buffer_head *bh;
		int try_to_free = 0;

		jh = commit_transaction->t_forget;
		spin_unlock(&journal->j_list_lock);
		bh = jh2bh(jh);
		get_bh(bh);
		jbd_lock_bh_state(bh);
		J_ASSERT_JH(jh,	jh->b_transaction == commit_transaction ||
			jh->b_transaction == journal->j_running_transaction);

		if (jh->b_committed_data) {
			jbd_free(jh->b_committed_data, bh->b_size);
			jh->b_committed_data = NULL;
			if (jh->b_frozen_data) {
				jh->b_committed_data = jh->b_frozen_data;
				jh->b_frozen_data = NULL;
			}
		} else if (jh->b_frozen_data) {
			jbd_free(jh->b_frozen_data, bh->b_size);
			jh->b_frozen_data = NULL;
		}

		spin_lock(&journal->j_list_lock);
		cp_transaction = jh->b_cp_transaction;
		if (cp_transaction) {
			JBUFFER_TRACE(jh, "remove from old cp transaction");
			__journal_remove_checkpoint(jh);
		}


		if (buffer_freed(bh) && !jh->b_next_transaction) {
			clear_buffer_freed(bh);
			clear_buffer_jbddirty(bh);
		}

		if (buffer_jbddirty(bh)) {
			JBUFFER_TRACE(jh, "add to new checkpointing trans");
			__journal_insert_checkpoint(jh, commit_transaction);
			if (is_journal_aborted(journal))
				clear_buffer_jbddirty(bh);
		} else {
			J_ASSERT_BH(bh, !buffer_dirty(bh));
			/*
			 * The buffer on BJ_Forget list and not jbddirty means
			 * it has been freed by this transaction and hence it
			 * could not have been reallocated until this
			 * transaction has committed. *BUT* it could be
			 * reallocated once we have written all the data to
			 * disk and before we process the buffer on BJ_Forget
			 * list.
			 */
			if (!jh->b_next_transaction)
				try_to_free = 1;
		}
		JBUFFER_TRACE(jh, "refile or unfile freed buffer");
		__journal_refile_buffer(jh);
		jbd_unlock_bh_state(bh);
		if (try_to_free)
			release_buffer_page(bh);
		else
			__brelse(bh);
		cond_resched_lock(&journal->j_list_lock);
	}
	spin_unlock(&journal->j_list_lock);
	spin_lock(&journal->j_state_lock);
	spin_lock(&journal->j_list_lock);
	if (commit_transaction->t_forget) {
		spin_unlock(&journal->j_list_lock);
		spin_unlock(&journal->j_state_lock);
		goto restart_loop;
	}

	

	jbd_debug(3, "JBD: commit phase 8\n");

	J_ASSERT(commit_transaction->t_state == T_COMMIT_RECORD);

	commit_transaction->t_state = T_FINISHED;
	J_ASSERT(commit_transaction == journal->j_committing_transaction);
	journal->j_commit_sequence = commit_transaction->t_tid;
	journal->j_committing_transaction = NULL;
	commit_time = ktime_to_ns(ktime_sub(ktime_get(), start_time));

	if (likely(journal->j_average_commit_time))
		journal->j_average_commit_time = (commit_time*3 +
				journal->j_average_commit_time) / 4;
	else
		journal->j_average_commit_time = commit_time;

	spin_unlock(&journal->j_state_lock);

	if (commit_transaction->t_checkpoint_list == NULL &&
	    commit_transaction->t_checkpoint_io_list == NULL) {
		__journal_drop_transaction(journal, commit_transaction);
	} else {
		if (journal->j_checkpoint_transactions == NULL) {
			journal->j_checkpoint_transactions = commit_transaction;
			commit_transaction->t_cpnext = commit_transaction;
			commit_transaction->t_cpprev = commit_transaction;
		} else {
			commit_transaction->t_cpnext =
				journal->j_checkpoint_transactions;
			commit_transaction->t_cpprev =
				commit_transaction->t_cpnext->t_cpprev;
			commit_transaction->t_cpnext->t_cpprev =
				commit_transaction;
			commit_transaction->t_cpprev->t_cpnext =
				commit_transaction;
		}
	}
	spin_unlock(&journal->j_list_lock);

	trace_jbd_end_commit(journal, commit_transaction);
	jbd_debug(1, "JBD: commit %d complete, head %d\n",
		  journal->j_commit_sequence, journal->j_tail_sequence);

	wake_up(&journal->j_wait_done_commit);
}
