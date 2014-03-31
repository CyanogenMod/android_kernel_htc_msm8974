/*
 * linux/fs/jbd/transaction.c
 *
 * Written by Stephen C. Tweedie <sct@redhat.com>, 1998
 *
 * Copyright 1998 Red Hat corp --- All Rights Reserved
 *
 * This file is part of the Linux kernel and is made available under
 * the terms of the GNU General Public License, version 2, or at your
 * option, any later version, incorporated herein by reference.
 *
 * Generic filesystem transaction handling code; part of the ext2fs
 * journaling system.
 *
 * This file manages transactions (compound commits managed by the
 * journaling code) and handles (individual atomic operations by the
 * filesystem).
 */

#include <linux/time.h>
#include <linux/fs.h>
#include <linux/jbd.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/hrtimer.h>
#include <linux/backing-dev.h>

static void __journal_temp_unlink_buffer(struct journal_head *jh);


static transaction_t *
get_transaction(journal_t *journal, transaction_t *transaction)
{
	transaction->t_journal = journal;
	transaction->t_state = T_RUNNING;
	transaction->t_start_time = ktime_get();
	transaction->t_tid = journal->j_transaction_sequence++;
	transaction->t_expires = jiffies + journal->j_commit_interval;
	spin_lock_init(&transaction->t_handle_lock);

	
	journal->j_commit_timer.expires =
				round_jiffies_up(transaction->t_expires);
	add_timer(&journal->j_commit_timer);

	J_ASSERT(journal->j_running_transaction == NULL);
	journal->j_running_transaction = transaction;

	return transaction;
}



static int start_this_handle(journal_t *journal, handle_t *handle)
{
	transaction_t *transaction;
	int needed;
	int nblocks = handle->h_buffer_credits;
	transaction_t *new_transaction = NULL;
	int ret = 0;

	if (nblocks > journal->j_max_transaction_buffers) {
		printk(KERN_ERR "JBD: %s wants too many credits (%d > %d)\n",
		       current->comm, nblocks,
		       journal->j_max_transaction_buffers);
		ret = -ENOSPC;
		goto out;
	}

alloc_transaction:
	if (!journal->j_running_transaction) {
		new_transaction = kzalloc(sizeof(*new_transaction), GFP_NOFS);
		if (!new_transaction) {
			congestion_wait(BLK_RW_ASYNC, HZ/50);
			goto alloc_transaction;
		}
	}

	jbd_debug(3, "New handle %p going live.\n", handle);

repeat:

	spin_lock(&journal->j_state_lock);
repeat_locked:
	if (is_journal_aborted(journal) ||
	    (journal->j_errno != 0 && !(journal->j_flags & JFS_ACK_ERR))) {
		spin_unlock(&journal->j_state_lock);
		ret = -EROFS;
		goto out;
	}

	
	if (journal->j_barrier_count) {
		spin_unlock(&journal->j_state_lock);
		wait_event(journal->j_wait_transaction_locked,
				journal->j_barrier_count == 0);
		goto repeat;
	}

	if (!journal->j_running_transaction) {
		if (!new_transaction) {
			spin_unlock(&journal->j_state_lock);
			goto alloc_transaction;
		}
		get_transaction(journal, new_transaction);
		new_transaction = NULL;
	}

	transaction = journal->j_running_transaction;

	if (transaction->t_state == T_LOCKED) {
		DEFINE_WAIT(wait);

		prepare_to_wait(&journal->j_wait_transaction_locked,
					&wait, TASK_UNINTERRUPTIBLE);
		spin_unlock(&journal->j_state_lock);
		schedule();
		finish_wait(&journal->j_wait_transaction_locked, &wait);
		goto repeat;
	}

	spin_lock(&transaction->t_handle_lock);
	needed = transaction->t_outstanding_credits + nblocks;

	if (needed > journal->j_max_transaction_buffers) {
		DEFINE_WAIT(wait);

		jbd_debug(2, "Handle %p starting new commit...\n", handle);
		spin_unlock(&transaction->t_handle_lock);
		prepare_to_wait(&journal->j_wait_transaction_locked, &wait,
				TASK_UNINTERRUPTIBLE);
		__log_start_commit(journal, transaction->t_tid);
		spin_unlock(&journal->j_state_lock);
		schedule();
		finish_wait(&journal->j_wait_transaction_locked, &wait);
		goto repeat;
	}


	if (__log_space_left(journal) < jbd_space_needed(journal)) {
		jbd_debug(2, "Handle %p waiting for checkpoint...\n", handle);
		spin_unlock(&transaction->t_handle_lock);
		__log_wait_for_space(journal);
		goto repeat_locked;
	}


	handle->h_transaction = transaction;
	transaction->t_outstanding_credits += nblocks;
	transaction->t_updates++;
	transaction->t_handle_count++;
	jbd_debug(4, "Handle %p given %d credits (total %d, free %d)\n",
		  handle, nblocks, transaction->t_outstanding_credits,
		  __log_space_left(journal));
	spin_unlock(&transaction->t_handle_lock);
	spin_unlock(&journal->j_state_lock);

	lock_map_acquire(&handle->h_lockdep_map);
out:
	if (unlikely(new_transaction))		
		kfree(new_transaction);
	return ret;
}

static struct lock_class_key jbd_handle_key;

static handle_t *new_handle(int nblocks)
{
	handle_t *handle = jbd_alloc_handle(GFP_NOFS);
	if (!handle)
		return NULL;
	memset(handle, 0, sizeof(*handle));
	handle->h_buffer_credits = nblocks;
	handle->h_ref = 1;

	lockdep_init_map(&handle->h_lockdep_map, "jbd_handle", &jbd_handle_key, 0);

	return handle;
}

handle_t *journal_start(journal_t *journal, int nblocks)
{
	handle_t *handle = journal_current_handle();
	int err;

	if (!journal)
		return ERR_PTR(-EROFS);

	if (handle) {
		J_ASSERT(handle->h_transaction->t_journal == journal);
		handle->h_ref++;
		return handle;
	}

	handle = new_handle(nblocks);
	if (!handle)
		return ERR_PTR(-ENOMEM);

	current->journal_info = handle;

	err = start_this_handle(journal, handle);
	if (err < 0) {
		jbd_free_handle(handle);
		current->journal_info = NULL;
		handle = ERR_PTR(err);
	}
	return handle;
}

int journal_extend(handle_t *handle, int nblocks)
{
	transaction_t *transaction = handle->h_transaction;
	journal_t *journal = transaction->t_journal;
	int result;
	int wanted;

	result = -EIO;
	if (is_handle_aborted(handle))
		goto out;

	result = 1;

	spin_lock(&journal->j_state_lock);

	
	if (handle->h_transaction->t_state != T_RUNNING) {
		jbd_debug(3, "denied handle %p %d blocks: "
			  "transaction not running\n", handle, nblocks);
		goto error_out;
	}

	spin_lock(&transaction->t_handle_lock);
	wanted = transaction->t_outstanding_credits + nblocks;

	if (wanted > journal->j_max_transaction_buffers) {
		jbd_debug(3, "denied handle %p %d blocks: "
			  "transaction too large\n", handle, nblocks);
		goto unlock;
	}

	if (wanted > __log_space_left(journal)) {
		jbd_debug(3, "denied handle %p %d blocks: "
			  "insufficient log space\n", handle, nblocks);
		goto unlock;
	}

	handle->h_buffer_credits += nblocks;
	transaction->t_outstanding_credits += nblocks;
	result = 0;

	jbd_debug(3, "extended handle %p by %d\n", handle, nblocks);
unlock:
	spin_unlock(&transaction->t_handle_lock);
error_out:
	spin_unlock(&journal->j_state_lock);
out:
	return result;
}



int journal_restart(handle_t *handle, int nblocks)
{
	transaction_t *transaction = handle->h_transaction;
	journal_t *journal = transaction->t_journal;
	int ret;

	if (is_handle_aborted(handle))
		return 0;

	J_ASSERT(transaction->t_updates > 0);
	J_ASSERT(journal_current_handle() == handle);

	spin_lock(&journal->j_state_lock);
	spin_lock(&transaction->t_handle_lock);
	transaction->t_outstanding_credits -= handle->h_buffer_credits;
	transaction->t_updates--;

	if (!transaction->t_updates)
		wake_up(&journal->j_wait_updates);
	spin_unlock(&transaction->t_handle_lock);

	jbd_debug(2, "restarting handle %p\n", handle);
	__log_start_commit(journal, transaction->t_tid);
	spin_unlock(&journal->j_state_lock);

	lock_map_release(&handle->h_lockdep_map);
	handle->h_buffer_credits = nblocks;
	ret = start_this_handle(journal, handle);
	return ret;
}


void journal_lock_updates(journal_t *journal)
{
	DEFINE_WAIT(wait);

wait:
	
	wait_event(journal->j_wait_transaction_locked,
		   journal->j_barrier_count == 0);

	spin_lock(&journal->j_state_lock);
	if (journal->j_barrier_count > 0) {
		spin_unlock(&journal->j_state_lock);
		goto wait;
	}
	++journal->j_barrier_count;

	
	while (1) {
		transaction_t *transaction = journal->j_running_transaction;

		if (!transaction)
			break;

		spin_lock(&transaction->t_handle_lock);
		if (!transaction->t_updates) {
			spin_unlock(&transaction->t_handle_lock);
			break;
		}
		prepare_to_wait(&journal->j_wait_updates, &wait,
				TASK_UNINTERRUPTIBLE);
		spin_unlock(&transaction->t_handle_lock);
		spin_unlock(&journal->j_state_lock);
		schedule();
		finish_wait(&journal->j_wait_updates, &wait);
		spin_lock(&journal->j_state_lock);
	}
	spin_unlock(&journal->j_state_lock);
}

void journal_unlock_updates (journal_t *journal)
{
	J_ASSERT(journal->j_barrier_count != 0);

	spin_lock(&journal->j_state_lock);
	--journal->j_barrier_count;
	spin_unlock(&journal->j_state_lock);
	wake_up(&journal->j_wait_transaction_locked);
}

static void warn_dirty_buffer(struct buffer_head *bh)
{
	char b[BDEVNAME_SIZE];

	printk(KERN_WARNING
	       "JBD: Spotted dirty metadata buffer (dev = %s, blocknr = %llu). "
	       "There's a risk of filesystem corruption in case of system "
	       "crash.\n",
	       bdevname(bh->b_bdev, b), (unsigned long long)bh->b_blocknr);
}

static int
do_get_write_access(handle_t *handle, struct journal_head *jh,
			int force_copy)
{
	struct buffer_head *bh;
	transaction_t *transaction;
	journal_t *journal;
	int error;
	char *frozen_buffer = NULL;
	int need_copy = 0;

	if (is_handle_aborted(handle))
		return -EROFS;

	transaction = handle->h_transaction;
	journal = transaction->t_journal;

	jbd_debug(5, "journal_head %p, force_copy %d\n", jh, force_copy);

	JBUFFER_TRACE(jh, "entry");
repeat:
	bh = jh2bh(jh);

	

	lock_buffer(bh);
	jbd_lock_bh_state(bh);


	if (buffer_dirty(bh)) {
		if (jh->b_transaction) {
			J_ASSERT_JH(jh,
				jh->b_transaction == transaction ||
				jh->b_transaction ==
					journal->j_committing_transaction);
			if (jh->b_next_transaction)
				J_ASSERT_JH(jh, jh->b_next_transaction ==
							transaction);
			warn_dirty_buffer(bh);
		}
		JBUFFER_TRACE(jh, "Journalling dirty buffer");
		clear_buffer_dirty(bh);
		set_buffer_jbddirty(bh);
	}

	unlock_buffer(bh);

	error = -EROFS;
	if (is_handle_aborted(handle)) {
		jbd_unlock_bh_state(bh);
		goto out;
	}
	error = 0;

	if (jh->b_transaction == transaction ||
	    jh->b_next_transaction == transaction)
		goto done;

	jh->b_modified = 0;

	if (jh->b_frozen_data) {
		JBUFFER_TRACE(jh, "has frozen data");
		J_ASSERT_JH(jh, jh->b_next_transaction == NULL);
		jh->b_next_transaction = transaction;
		goto done;
	}

	

	if (jh->b_transaction && jh->b_transaction != transaction) {
		JBUFFER_TRACE(jh, "owned by older transaction");
		J_ASSERT_JH(jh, jh->b_next_transaction == NULL);
		J_ASSERT_JH(jh, jh->b_transaction ==
					journal->j_committing_transaction);


		if (jh->b_jlist == BJ_Shadow) {
			DEFINE_WAIT_BIT(wait, &bh->b_state, BH_Unshadow);
			wait_queue_head_t *wqh;

			wqh = bit_waitqueue(&bh->b_state, BH_Unshadow);

			JBUFFER_TRACE(jh, "on shadow: sleep");
			jbd_unlock_bh_state(bh);
			
			for ( ; ; ) {
				prepare_to_wait(wqh, &wait.wait,
						TASK_UNINTERRUPTIBLE);
				if (jh->b_jlist != BJ_Shadow)
					break;
				schedule();
			}
			finish_wait(wqh, &wait.wait);
			goto repeat;
		}


		if (jh->b_jlist != BJ_Forget || force_copy) {
			JBUFFER_TRACE(jh, "generate frozen data");
			if (!frozen_buffer) {
				JBUFFER_TRACE(jh, "allocate memory for buffer");
				jbd_unlock_bh_state(bh);
				frozen_buffer =
					jbd_alloc(jh2bh(jh)->b_size,
							 GFP_NOFS);
				if (!frozen_buffer) {
					printk(KERN_EMERG
					       "%s: OOM for frozen_buffer\n",
					       __func__);
					JBUFFER_TRACE(jh, "oom!");
					error = -ENOMEM;
					jbd_lock_bh_state(bh);
					goto done;
				}
				goto repeat;
			}
			jh->b_frozen_data = frozen_buffer;
			frozen_buffer = NULL;
			need_copy = 1;
		}
		jh->b_next_transaction = transaction;
	}


	/*
	 * Finally, if the buffer is not journaled right now, we need to make
	 * sure it doesn't get written to disk before the caller actually
	 * commits the new data
	 */
	if (!jh->b_transaction) {
		JBUFFER_TRACE(jh, "no transaction");
		J_ASSERT_JH(jh, !jh->b_next_transaction);
		JBUFFER_TRACE(jh, "file as BJ_Reserved");
		spin_lock(&journal->j_list_lock);
		__journal_file_buffer(jh, transaction, BJ_Reserved);
		spin_unlock(&journal->j_list_lock);
	}

done:
	if (need_copy) {
		struct page *page;
		int offset;
		char *source;

		J_EXPECT_JH(jh, buffer_uptodate(jh2bh(jh)),
			    "Possible IO failure.\n");
		page = jh2bh(jh)->b_page;
		offset = offset_in_page(jh2bh(jh)->b_data);
		source = kmap_atomic(page);
		memcpy(jh->b_frozen_data, source+offset, jh2bh(jh)->b_size);
		kunmap_atomic(source);
	}
	jbd_unlock_bh_state(bh);

	journal_cancel_revoke(handle, jh);

out:
	if (unlikely(frozen_buffer))	
		jbd_free(frozen_buffer, bh->b_size);

	JBUFFER_TRACE(jh, "exit");
	return error;
}


int journal_get_write_access(handle_t *handle, struct buffer_head *bh)
{
	struct journal_head *jh = journal_add_journal_head(bh);
	int rc;

	rc = do_get_write_access(handle, jh, 0);
	journal_put_journal_head(jh);
	return rc;
}



int journal_get_create_access(handle_t *handle, struct buffer_head *bh)
{
	transaction_t *transaction = handle->h_transaction;
	journal_t *journal = transaction->t_journal;
	struct journal_head *jh = journal_add_journal_head(bh);
	int err;

	jbd_debug(5, "journal_head %p\n", jh);
	err = -EROFS;
	if (is_handle_aborted(handle))
		goto out;
	err = 0;

	JBUFFER_TRACE(jh, "entry");
	jbd_lock_bh_state(bh);
	spin_lock(&journal->j_list_lock);
	J_ASSERT_JH(jh, (jh->b_transaction == transaction ||
		jh->b_transaction == NULL ||
		(jh->b_transaction == journal->j_committing_transaction &&
			  jh->b_jlist == BJ_Forget)));

	J_ASSERT_JH(jh, jh->b_next_transaction == NULL);
	J_ASSERT_JH(jh, buffer_locked(jh2bh(jh)));

	if (jh->b_transaction == NULL) {
		clear_buffer_dirty(jh2bh(jh));

		
		jh->b_modified = 0;

		JBUFFER_TRACE(jh, "file as BJ_Reserved");
		__journal_file_buffer(jh, transaction, BJ_Reserved);
	} else if (jh->b_transaction == journal->j_committing_transaction) {
		
		jh->b_modified = 0;

		JBUFFER_TRACE(jh, "set next transaction");
		jh->b_next_transaction = transaction;
	}
	spin_unlock(&journal->j_list_lock);
	jbd_unlock_bh_state(bh);

	JBUFFER_TRACE(jh, "cancelling revoke");
	journal_cancel_revoke(handle, jh);
out:
	journal_put_journal_head(jh);
	return err;
}

int journal_get_undo_access(handle_t *handle, struct buffer_head *bh)
{
	int err;
	struct journal_head *jh = journal_add_journal_head(bh);
	char *committed_data = NULL;

	JBUFFER_TRACE(jh, "entry");

	err = do_get_write_access(handle, jh, 1);
	if (err)
		goto out;

repeat:
	if (!jh->b_committed_data) {
		committed_data = jbd_alloc(jh2bh(jh)->b_size, GFP_NOFS);
		if (!committed_data) {
			printk(KERN_EMERG "%s: No memory for committed data\n",
				__func__);
			err = -ENOMEM;
			goto out;
		}
	}

	jbd_lock_bh_state(bh);
	if (!jh->b_committed_data) {
		JBUFFER_TRACE(jh, "generate b_committed data");
		if (!committed_data) {
			jbd_unlock_bh_state(bh);
			goto repeat;
		}

		jh->b_committed_data = committed_data;
		committed_data = NULL;
		memcpy(jh->b_committed_data, bh->b_data, bh->b_size);
	}
	jbd_unlock_bh_state(bh);
out:
	journal_put_journal_head(jh);
	if (unlikely(committed_data))
		jbd_free(committed_data, bh->b_size);
	return err;
}

int journal_dirty_data(handle_t *handle, struct buffer_head *bh)
{
	journal_t *journal = handle->h_transaction->t_journal;
	int need_brelse = 0;
	struct journal_head *jh;
	int ret = 0;

	if (is_handle_aborted(handle))
		return ret;

	jh = journal_add_journal_head(bh);
	JBUFFER_TRACE(jh, "entry");

	jbd_debug(4, "jh: %p, tid:%d\n", jh, handle->h_transaction->t_tid);

	/*
	 * What if the buffer is already part of a running transaction?
	 *
	 * There are two cases:
	 * 1) It is part of the current running transaction.  Refile it,
	 *    just in case we have allocated it as metadata, deallocated
	 *    it, then reallocated it as data.
	 * 2) It is part of the previous, still-committing transaction.
	 *    If all we want to do is to guarantee that the buffer will be
	 *    written to disk before this new transaction commits, then
	 *    being sure that the *previous* transaction has this same
	 *    property is sufficient for us!  Just leave it on its old
	 *    transaction.
	 *
	 * In case (2), the buffer must not already exist as metadata
	 * --- that would violate write ordering (a transaction is free
	 * to write its data at any point, even before the previous
	 * committing transaction has committed).  The caller must
	 * never, ever allow this to happen: there's nothing we can do
	 * about it in this layer.
	 */
	jbd_lock_bh_state(bh);
	spin_lock(&journal->j_list_lock);

	
	if (!buffer_mapped(bh)) {
		JBUFFER_TRACE(jh, "unmapped buffer, bailing out");
		goto no_journal;
	}

	if (jh->b_transaction) {
		JBUFFER_TRACE(jh, "has transaction");
		if (jh->b_transaction != handle->h_transaction) {
			JBUFFER_TRACE(jh, "belongs to older transaction");
			J_ASSERT_JH(jh, jh->b_transaction ==
					journal->j_committing_transaction);

			
			

			/*
			 * If we're journalling data, and this buffer was
			 * subject to a write(), it could be metadata, forget
			 * or shadow against the committing transaction.  Now,
			 * someone has dirtied the same darn page via a mapping
			 * and it is being writepage()'d.
			 * We *could* just steal the page from commit, with some
			 * fancy locking there.  Instead, we just skip it -
			 * don't tie the page's buffers to the new transaction
			 * at all.
			 * Implication: if we crash before the writepage() data
			 * is written into the filesystem, recovery will replay
			 * the write() data.
			 */
			if (jh->b_jlist != BJ_None &&
					jh->b_jlist != BJ_SyncData &&
					jh->b_jlist != BJ_Locked) {
				JBUFFER_TRACE(jh, "Not stealing");
				goto no_journal;
			}

			if (buffer_dirty(bh)) {
				get_bh(bh);
				spin_unlock(&journal->j_list_lock);
				jbd_unlock_bh_state(bh);
				need_brelse = 1;
				sync_dirty_buffer(bh);
				jbd_lock_bh_state(bh);
				spin_lock(&journal->j_list_lock);
				
				if (!buffer_mapped(bh)) {
					JBUFFER_TRACE(jh, "buffer got unmapped");
					goto no_journal;
				}
			}

			if (unlikely(!buffer_uptodate(bh))) {
				ret = -EIO;
				goto no_journal;
			}
			
			if (jh->b_transaction != NULL &&
			    jh->b_transaction != handle->h_transaction) {
				JBUFFER_TRACE(jh, "unfile from commit");
				__journal_temp_unlink_buffer(jh);
				jh->b_transaction = handle->h_transaction;
			}
			

		}
		if (jh->b_jlist != BJ_SyncData && jh->b_jlist != BJ_Locked) {
			JBUFFER_TRACE(jh, "not on correct data list: unfile");
			J_ASSERT_JH(jh, jh->b_jlist != BJ_Shadow);
			JBUFFER_TRACE(jh, "file as data");
			__journal_file_buffer(jh, handle->h_transaction,
						BJ_SyncData);
		}
	} else {
		JBUFFER_TRACE(jh, "not on a transaction");
		__journal_file_buffer(jh, handle->h_transaction, BJ_SyncData);
	}
no_journal:
	spin_unlock(&journal->j_list_lock);
	jbd_unlock_bh_state(bh);
	if (need_brelse) {
		BUFFER_TRACE(bh, "brelse");
		__brelse(bh);
	}
	JBUFFER_TRACE(jh, "exit");
	journal_put_journal_head(jh);
	return ret;
}

int journal_dirty_metadata(handle_t *handle, struct buffer_head *bh)
{
	transaction_t *transaction = handle->h_transaction;
	journal_t *journal = transaction->t_journal;
	struct journal_head *jh = bh2jh(bh);

	jbd_debug(5, "journal_head %p\n", jh);
	JBUFFER_TRACE(jh, "entry");
	if (is_handle_aborted(handle))
		goto out;

	jbd_lock_bh_state(bh);

	if (jh->b_modified == 0) {
		jh->b_modified = 1;
		J_ASSERT_JH(jh, handle->h_buffer_credits > 0);
		handle->h_buffer_credits--;
	}

	if (jh->b_transaction == transaction && jh->b_jlist == BJ_Metadata) {
		JBUFFER_TRACE(jh, "fastpath");
		J_ASSERT_JH(jh, jh->b_transaction ==
					journal->j_running_transaction);
		goto out_unlock_bh;
	}

	set_buffer_jbddirty(bh);

	if (jh->b_transaction != transaction) {
		JBUFFER_TRACE(jh, "already on other transaction");
		J_ASSERT_JH(jh, jh->b_transaction ==
					journal->j_committing_transaction);
		J_ASSERT_JH(jh, jh->b_next_transaction == transaction);
		goto out_unlock_bh;
	}

	
	J_ASSERT_JH(jh, jh->b_frozen_data == NULL);

	JBUFFER_TRACE(jh, "file as BJ_Metadata");
	spin_lock(&journal->j_list_lock);
	__journal_file_buffer(jh, handle->h_transaction, BJ_Metadata);
	spin_unlock(&journal->j_list_lock);
out_unlock_bh:
	jbd_unlock_bh_state(bh);
out:
	JBUFFER_TRACE(jh, "exit");
	return 0;
}

void
journal_release_buffer(handle_t *handle, struct buffer_head *bh)
{
	BUFFER_TRACE(bh, "entry");
}

int journal_forget (handle_t *handle, struct buffer_head *bh)
{
	transaction_t *transaction = handle->h_transaction;
	journal_t *journal = transaction->t_journal;
	struct journal_head *jh;
	int drop_reserve = 0;
	int err = 0;
	int was_modified = 0;

	BUFFER_TRACE(bh, "entry");

	jbd_lock_bh_state(bh);
	spin_lock(&journal->j_list_lock);

	if (!buffer_jbd(bh))
		goto not_jbd;
	jh = bh2jh(bh);

	if (!J_EXPECT_JH(jh, !jh->b_committed_data,
			 "inconsistent data on disk")) {
		err = -EIO;
		goto not_jbd;
	}

	
	was_modified = jh->b_modified;

	jh->b_modified = 0;

	if (jh->b_transaction == handle->h_transaction) {
		J_ASSERT_JH(jh, !jh->b_frozen_data);

		clear_buffer_dirty(bh);
		clear_buffer_jbddirty(bh);

		JBUFFER_TRACE(jh, "belongs to current transaction: unfile");

		if (was_modified)
			drop_reserve = 1;


		if (jh->b_cp_transaction) {
			__journal_temp_unlink_buffer(jh);
			__journal_file_buffer(jh, transaction, BJ_Forget);
		} else {
			__journal_unfile_buffer(jh);
			if (!buffer_jbd(bh)) {
				spin_unlock(&journal->j_list_lock);
				jbd_unlock_bh_state(bh);
				__bforget(bh);
				goto drop;
			}
		}
	} else if (jh->b_transaction) {
		J_ASSERT_JH(jh, (jh->b_transaction ==
				 journal->j_committing_transaction));
		JBUFFER_TRACE(jh, "belongs to older transaction");

		if (jh->b_next_transaction) {
			J_ASSERT(jh->b_next_transaction == transaction);
			jh->b_next_transaction = NULL;

			if (was_modified)
				drop_reserve = 1;
		}
	}

not_jbd:
	spin_unlock(&journal->j_list_lock);
	jbd_unlock_bh_state(bh);
	__brelse(bh);
drop:
	if (drop_reserve) {
		
		handle->h_buffer_credits++;
	}
	return err;
}

int journal_stop(handle_t *handle)
{
	transaction_t *transaction = handle->h_transaction;
	journal_t *journal = transaction->t_journal;
	int err;
	pid_t pid;

	J_ASSERT(journal_current_handle() == handle);

	if (is_handle_aborted(handle))
		err = -EIO;
	else {
		J_ASSERT(transaction->t_updates > 0);
		err = 0;
	}

	if (--handle->h_ref > 0) {
		jbd_debug(4, "h_ref %d -> %d\n", handle->h_ref + 1,
			  handle->h_ref);
		return err;
	}

	jbd_debug(4, "Handle %p going down\n", handle);

	pid = current->pid;
	if (handle->h_sync && journal->j_last_sync_writer != pid) {
		u64 commit_time, trans_time;

		journal->j_last_sync_writer = pid;

		spin_lock(&journal->j_state_lock);
		commit_time = journal->j_average_commit_time;
		spin_unlock(&journal->j_state_lock);

		trans_time = ktime_to_ns(ktime_sub(ktime_get(),
						   transaction->t_start_time));

		commit_time = min_t(u64, commit_time,
				    1000*jiffies_to_usecs(1));

		if (trans_time < commit_time) {
			ktime_t expires = ktime_add_ns(ktime_get(),
						       commit_time);
			set_current_state(TASK_UNINTERRUPTIBLE);
			schedule_hrtimeout(&expires, HRTIMER_MODE_ABS);
		}
	}

	if (handle->h_sync)
		transaction->t_synchronous_commit = 1;
	current->journal_info = NULL;
	spin_lock(&journal->j_state_lock);
	spin_lock(&transaction->t_handle_lock);
	transaction->t_outstanding_credits -= handle->h_buffer_credits;
	transaction->t_updates--;
	if (!transaction->t_updates) {
		wake_up(&journal->j_wait_updates);
		if (journal->j_barrier_count)
			wake_up(&journal->j_wait_transaction_locked);
	}

	if (handle->h_sync ||
			transaction->t_outstanding_credits >
				journal->j_max_transaction_buffers ||
			time_after_eq(jiffies, transaction->t_expires)) {
		tid_t tid = transaction->t_tid;

		spin_unlock(&transaction->t_handle_lock);
		jbd_debug(2, "transaction too old, requesting commit for "
					"handle %p\n", handle);
		
		__log_start_commit(journal, transaction->t_tid);
		spin_unlock(&journal->j_state_lock);

		if (handle->h_sync && !(current->flags & PF_MEMALLOC))
			err = log_wait_commit(journal, tid);
	} else {
		spin_unlock(&transaction->t_handle_lock);
		spin_unlock(&journal->j_state_lock);
	}

	lock_map_release(&handle->h_lockdep_map);

	jbd_free_handle(handle);
	return err;
}

int journal_force_commit(journal_t *journal)
{
	handle_t *handle;
	int ret;

	handle = journal_start(journal, 1);
	if (IS_ERR(handle)) {
		ret = PTR_ERR(handle);
	} else {
		handle->h_sync = 1;
		ret = journal_stop(handle);
	}
	return ret;
}



static inline void
__blist_add_buffer(struct journal_head **list, struct journal_head *jh)
{
	if (!*list) {
		jh->b_tnext = jh->b_tprev = jh;
		*list = jh;
	} else {
		
		struct journal_head *first = *list, *last = first->b_tprev;
		jh->b_tprev = last;
		jh->b_tnext = first;
		last->b_tnext = first->b_tprev = jh;
	}
}


static inline void
__blist_del_buffer(struct journal_head **list, struct journal_head *jh)
{
	if (*list == jh) {
		*list = jh->b_tnext;
		if (*list == jh)
			*list = NULL;
	}
	jh->b_tprev->b_tnext = jh->b_tnext;
	jh->b_tnext->b_tprev = jh->b_tprev;
}

static void __journal_temp_unlink_buffer(struct journal_head *jh)
{
	struct journal_head **list = NULL;
	transaction_t *transaction;
	struct buffer_head *bh = jh2bh(jh);

	J_ASSERT_JH(jh, jbd_is_locked_bh_state(bh));
	transaction = jh->b_transaction;
	if (transaction)
		assert_spin_locked(&transaction->t_journal->j_list_lock);

	J_ASSERT_JH(jh, jh->b_jlist < BJ_Types);
	if (jh->b_jlist != BJ_None)
		J_ASSERT_JH(jh, transaction != NULL);

	switch (jh->b_jlist) {
	case BJ_None:
		return;
	case BJ_SyncData:
		list = &transaction->t_sync_datalist;
		break;
	case BJ_Metadata:
		transaction->t_nr_buffers--;
		J_ASSERT_JH(jh, transaction->t_nr_buffers >= 0);
		list = &transaction->t_buffers;
		break;
	case BJ_Forget:
		list = &transaction->t_forget;
		break;
	case BJ_IO:
		list = &transaction->t_iobuf_list;
		break;
	case BJ_Shadow:
		list = &transaction->t_shadow_list;
		break;
	case BJ_LogCtl:
		list = &transaction->t_log_list;
		break;
	case BJ_Reserved:
		list = &transaction->t_reserved_list;
		break;
	case BJ_Locked:
		list = &transaction->t_locked_list;
		break;
	}

	__blist_del_buffer(list, jh);
	jh->b_jlist = BJ_None;
	if (test_clear_buffer_jbddirty(bh))
		mark_buffer_dirty(bh);	
}

void __journal_unfile_buffer(struct journal_head *jh)
{
	__journal_temp_unlink_buffer(jh);
	jh->b_transaction = NULL;
	journal_put_journal_head(jh);
}

void journal_unfile_buffer(journal_t *journal, struct journal_head *jh)
{
	struct buffer_head *bh = jh2bh(jh);

	
	get_bh(bh);
	jbd_lock_bh_state(bh);
	spin_lock(&journal->j_list_lock);
	__journal_unfile_buffer(jh);
	spin_unlock(&journal->j_list_lock);
	jbd_unlock_bh_state(bh);
	__brelse(bh);
}

static void
__journal_try_to_free_buffer(journal_t *journal, struct buffer_head *bh)
{
	struct journal_head *jh;

	jh = bh2jh(bh);

	if (buffer_locked(bh) || buffer_dirty(bh))
		goto out;

	if (jh->b_next_transaction != NULL)
		goto out;

	spin_lock(&journal->j_list_lock);
	if (jh->b_transaction != NULL && jh->b_cp_transaction == NULL) {
		if (jh->b_jlist == BJ_SyncData || jh->b_jlist == BJ_Locked) {
			/* A written-back ordered data buffer */
			JBUFFER_TRACE(jh, "release data");
			__journal_unfile_buffer(jh);
		}
	} else if (jh->b_cp_transaction != NULL && jh->b_transaction == NULL) {
		/* written-back checkpointed metadata buffer */
		if (jh->b_jlist == BJ_None) {
			JBUFFER_TRACE(jh, "remove from checkpoint list");
			__journal_remove_checkpoint(jh);
		}
	}
	spin_unlock(&journal->j_list_lock);
out:
	return;
}

/**
 * int journal_try_to_free_buffers() - try to free page buffers.
 * @journal: journal for operation
 * @page: to try and free
 * @gfp_mask: we use the mask to detect how hard should we try to release
 * buffers. If __GFP_WAIT and __GFP_FS is set, we wait for commit code to
 * release the buffers.
 *
 *
 * For all the buffers on this page,
 * if they are fully written out ordered data, move them onto BUF_CLEAN
 * so try_to_free_buffers() can reap them.
 *
 * This function returns non-zero if we wish try_to_free_buffers()
 * to be called. We do this if the page is releasable by try_to_free_buffers().
 * We also do it if the page has locked or dirty buffers and the caller wants
 * us to perform sync or async writeout.
 *
 * This complicates JBD locking somewhat.  We aren't protected by the
 * BKL here.  We wish to remove the buffer from its committing or
 * running transaction's ->t_datalist via __journal_unfile_buffer.
 *
 * This may *change* the value of transaction_t->t_datalist, so anyone
 * who looks at t_datalist needs to lock against this function.
 *
 * Even worse, someone may be doing a journal_dirty_data on this
 * buffer.  So we need to lock against that.  journal_dirty_data()
 * will come out of the lock with the buffer dirty, which makes it
 * ineligible for release here.
 *
 * Who else is affected by this?  hmm...  Really the only contender
 * is do_get_write_access() - it could be looking at the buffer while
 * journal_try_to_free_buffer() is changing its state.  But that
 * cannot happen because we never reallocate freed data as metadata
 * while the data is part of a transaction.  Yes?
 *
 * Return 0 on failure, 1 on success
 */
int journal_try_to_free_buffers(journal_t *journal,
				struct page *page, gfp_t gfp_mask)
{
	struct buffer_head *head;
	struct buffer_head *bh;
	int ret = 0;

	J_ASSERT(PageLocked(page));

	head = page_buffers(page);
	bh = head;
	do {
		struct journal_head *jh;

		jh = journal_grab_journal_head(bh);
		if (!jh)
			continue;

		jbd_lock_bh_state(bh);
		__journal_try_to_free_buffer(journal, bh);
		journal_put_journal_head(jh);
		jbd_unlock_bh_state(bh);
		if (buffer_jbd(bh))
			goto busy;
	} while ((bh = bh->b_this_page) != head);

	ret = try_to_free_buffers(page);

busy:
	return ret;
}

static int __dispose_buffer(struct journal_head *jh, transaction_t *transaction)
{
	int may_free = 1;
	struct buffer_head *bh = jh2bh(jh);

	if (jh->b_cp_transaction) {
		JBUFFER_TRACE(jh, "on running+cp transaction");
		__journal_temp_unlink_buffer(jh);
		clear_buffer_dirty(bh);
		__journal_file_buffer(jh, transaction, BJ_Forget);
		may_free = 0;
	} else {
		JBUFFER_TRACE(jh, "on running transaction");
		__journal_unfile_buffer(jh);
	}
	return may_free;
}


static int journal_unmap_buffer(journal_t *journal, struct buffer_head *bh)
{
	transaction_t *transaction;
	struct journal_head *jh;
	int may_free = 1;
	int ret;

	BUFFER_TRACE(bh, "entry");


	if (!buffer_jbd(bh))
		goto zap_buffer_unlocked;

	spin_lock(&journal->j_state_lock);
	jbd_lock_bh_state(bh);
	spin_lock(&journal->j_list_lock);

	jh = journal_grab_journal_head(bh);
	if (!jh)
		goto zap_buffer_no_jh;

	transaction = jh->b_transaction;
	if (transaction == NULL) {
		if (!jh->b_cp_transaction) {
			JBUFFER_TRACE(jh, "not on any transaction: zap");
			goto zap_buffer;
		}

		if (!buffer_dirty(bh)) {
			/* bdflush has written it.  We can drop it now */
			goto zap_buffer;
		}

		/* OK, it must be in the journal but still not
		 * written fully to disk: it's metadata or
		 * journaled data... */

		if (journal->j_running_transaction) {
			JBUFFER_TRACE(jh, "checkpointed: add to BJ_Forget");
			ret = __dispose_buffer(jh,
					journal->j_running_transaction);
			journal_put_journal_head(jh);
			spin_unlock(&journal->j_list_lock);
			jbd_unlock_bh_state(bh);
			spin_unlock(&journal->j_state_lock);
			return ret;
		} else {
			if (journal->j_committing_transaction) {
				JBUFFER_TRACE(jh, "give to committing trans");
				ret = __dispose_buffer(jh,
					journal->j_committing_transaction);
				journal_put_journal_head(jh);
				spin_unlock(&journal->j_list_lock);
				jbd_unlock_bh_state(bh);
				spin_unlock(&journal->j_state_lock);
				return ret;
			} else {
				clear_buffer_jbddirty(bh);
				goto zap_buffer;
			}
		}
	} else if (transaction == journal->j_committing_transaction) {
		JBUFFER_TRACE(jh, "on committing transaction");
		if (jh->b_jlist == BJ_Locked) {
			may_free = __dispose_buffer(jh, transaction);
			goto zap_buffer;
		}
		set_buffer_freed(bh);
		if (journal->j_running_transaction && buffer_jbddirty(bh))
			jh->b_next_transaction = journal->j_running_transaction;
		journal_put_journal_head(jh);
		spin_unlock(&journal->j_list_lock);
		jbd_unlock_bh_state(bh);
		spin_unlock(&journal->j_state_lock);
		return 0;
	} else {
		J_ASSERT_JH(jh, transaction == journal->j_running_transaction);
		JBUFFER_TRACE(jh, "on running transaction");
		may_free = __dispose_buffer(jh, transaction);
	}

zap_buffer:
	journal_put_journal_head(jh);
zap_buffer_no_jh:
	spin_unlock(&journal->j_list_lock);
	jbd_unlock_bh_state(bh);
	spin_unlock(&journal->j_state_lock);
zap_buffer_unlocked:
	clear_buffer_dirty(bh);
	J_ASSERT_BH(bh, !buffer_jbddirty(bh));
	clear_buffer_mapped(bh);
	clear_buffer_req(bh);
	clear_buffer_new(bh);
	bh->b_bdev = NULL;
	return may_free;
}

void journal_invalidatepage(journal_t *journal,
		      struct page *page,
		      unsigned long offset)
{
	struct buffer_head *head, *bh, *next;
	unsigned int curr_off = 0;
	int may_free = 1;

	if (!PageLocked(page))
		BUG();
	if (!page_has_buffers(page))
		return;


	head = bh = page_buffers(page);
	do {
		unsigned int next_off = curr_off + bh->b_size;
		next = bh->b_this_page;

		if (offset <= curr_off) {
			
			lock_buffer(bh);
			may_free &= journal_unmap_buffer(journal, bh);
			unlock_buffer(bh);
		}
		curr_off = next_off;
		bh = next;

	} while (bh != head);

	if (!offset) {
		if (may_free && try_to_free_buffers(page))
			J_ASSERT(!page_has_buffers(page));
	}
}

void __journal_file_buffer(struct journal_head *jh,
			transaction_t *transaction, int jlist)
{
	struct journal_head **list = NULL;
	int was_dirty = 0;
	struct buffer_head *bh = jh2bh(jh);

	J_ASSERT_JH(jh, jbd_is_locked_bh_state(bh));
	assert_spin_locked(&transaction->t_journal->j_list_lock);

	J_ASSERT_JH(jh, jh->b_jlist < BJ_Types);
	J_ASSERT_JH(jh, jh->b_transaction == transaction ||
				jh->b_transaction == NULL);

	if (jh->b_transaction && jh->b_jlist == jlist)
		return;

	if (jlist == BJ_Metadata || jlist == BJ_Reserved ||
	    jlist == BJ_Shadow || jlist == BJ_Forget) {
		if (buffer_dirty(bh))
			warn_dirty_buffer(bh);
		if (test_clear_buffer_dirty(bh) ||
		    test_clear_buffer_jbddirty(bh))
			was_dirty = 1;
	}

	if (jh->b_transaction)
		__journal_temp_unlink_buffer(jh);
	else
		journal_grab_journal_head(bh);
	jh->b_transaction = transaction;

	switch (jlist) {
	case BJ_None:
		J_ASSERT_JH(jh, !jh->b_committed_data);
		J_ASSERT_JH(jh, !jh->b_frozen_data);
		return;
	case BJ_SyncData:
		list = &transaction->t_sync_datalist;
		break;
	case BJ_Metadata:
		transaction->t_nr_buffers++;
		list = &transaction->t_buffers;
		break;
	case BJ_Forget:
		list = &transaction->t_forget;
		break;
	case BJ_IO:
		list = &transaction->t_iobuf_list;
		break;
	case BJ_Shadow:
		list = &transaction->t_shadow_list;
		break;
	case BJ_LogCtl:
		list = &transaction->t_log_list;
		break;
	case BJ_Reserved:
		list = &transaction->t_reserved_list;
		break;
	case BJ_Locked:
		list =  &transaction->t_locked_list;
		break;
	}

	__blist_add_buffer(list, jh);
	jh->b_jlist = jlist;

	if (was_dirty)
		set_buffer_jbddirty(bh);
}

void journal_file_buffer(struct journal_head *jh,
				transaction_t *transaction, int jlist)
{
	jbd_lock_bh_state(jh2bh(jh));
	spin_lock(&transaction->t_journal->j_list_lock);
	__journal_file_buffer(jh, transaction, jlist);
	spin_unlock(&transaction->t_journal->j_list_lock);
	jbd_unlock_bh_state(jh2bh(jh));
}

void __journal_refile_buffer(struct journal_head *jh)
{
	int was_dirty, jlist;
	struct buffer_head *bh = jh2bh(jh);

	J_ASSERT_JH(jh, jbd_is_locked_bh_state(bh));
	if (jh->b_transaction)
		assert_spin_locked(&jh->b_transaction->t_journal->j_list_lock);

	
	if (jh->b_next_transaction == NULL) {
		__journal_unfile_buffer(jh);
		return;
	}


	was_dirty = test_clear_buffer_jbddirty(bh);
	__journal_temp_unlink_buffer(jh);
	jh->b_transaction = jh->b_next_transaction;
	jh->b_next_transaction = NULL;
	if (buffer_freed(bh))
		jlist = BJ_Forget;
	else if (jh->b_modified)
		jlist = BJ_Metadata;
	else
		jlist = BJ_Reserved;
	__journal_file_buffer(jh, jh->b_transaction, jlist);
	J_ASSERT_JH(jh, jh->b_transaction->t_state == T_RUNNING);

	if (was_dirty)
		set_buffer_jbddirty(bh);
}

void journal_refile_buffer(journal_t *journal, struct journal_head *jh)
{
	struct buffer_head *bh = jh2bh(jh);

	
	get_bh(bh);
	jbd_lock_bh_state(bh);
	spin_lock(&journal->j_list_lock);
	__journal_refile_buffer(jh);
	jbd_unlock_bh_state(bh);
	spin_unlock(&journal->j_list_lock);
	__brelse(bh);
}
