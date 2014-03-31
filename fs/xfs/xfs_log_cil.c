/*
 * Copyright (c) 2010 Red Hat, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write the Free Software Foundation,
 * Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_types.h"
#include "xfs_bit.h"
#include "xfs_log.h"
#include "xfs_inum.h"
#include "xfs_trans.h"
#include "xfs_trans_priv.h"
#include "xfs_log_priv.h"
#include "xfs_sb.h"
#include "xfs_ag.h"
#include "xfs_mount.h"
#include "xfs_error.h"
#include "xfs_alloc.h"
#include "xfs_discard.h"

int
xlog_cil_init(
	struct log	*log)
{
	struct xfs_cil	*cil;
	struct xfs_cil_ctx *ctx;

	cil = kmem_zalloc(sizeof(*cil), KM_SLEEP|KM_MAYFAIL);
	if (!cil)
		return ENOMEM;

	ctx = kmem_zalloc(sizeof(*ctx), KM_SLEEP|KM_MAYFAIL);
	if (!ctx) {
		kmem_free(cil);
		return ENOMEM;
	}

	INIT_LIST_HEAD(&cil->xc_cil);
	INIT_LIST_HEAD(&cil->xc_committing);
	spin_lock_init(&cil->xc_cil_lock);
	init_rwsem(&cil->xc_ctx_lock);
	init_waitqueue_head(&cil->xc_commit_wait);

	INIT_LIST_HEAD(&ctx->committing);
	INIT_LIST_HEAD(&ctx->busy_extents);
	ctx->sequence = 1;
	ctx->cil = cil;
	cil->xc_ctx = ctx;
	cil->xc_current_sequence = ctx->sequence;

	cil->xc_log = log;
	log->l_cilp = cil;
	return 0;
}

void
xlog_cil_destroy(
	struct log	*log)
{
	if (log->l_cilp->xc_ctx) {
		if (log->l_cilp->xc_ctx->ticket)
			xfs_log_ticket_put(log->l_cilp->xc_ctx->ticket);
		kmem_free(log->l_cilp->xc_ctx);
	}

	ASSERT(list_empty(&log->l_cilp->xc_cil));
	kmem_free(log->l_cilp);
}

static struct xlog_ticket *
xlog_cil_ticket_alloc(
	struct log	*log)
{
	struct xlog_ticket *tic;

	tic = xlog_ticket_alloc(log, 0, 1, XFS_TRANSACTION, 0,
				KM_SLEEP|KM_NOFS);
	tic->t_trans_type = XFS_TRANS_CHECKPOINT;

	tic->t_curr_res = 0;
	return tic;
}

void
xlog_cil_init_post_recovery(
	struct log	*log)
{
	log->l_cilp->xc_ctx->ticket = xlog_cil_ticket_alloc(log);
	log->l_cilp->xc_ctx->sequence = 1;
	log->l_cilp->xc_ctx->commit_lsn = xlog_assign_lsn(log->l_curr_cycle,
								log->l_curr_block);
}

/*
 * Format log item into a flat buffers
 *
 * For delayed logging, we need to hold a formatted buffer containing all the
 * changes on the log item. This enables us to relog the item in memory and
 * write it out asynchronously without needing to relock the object that was
 * modified at the time it gets written into the iclog.
 *
 * This function builds a vector for the changes in each log item in the
 * transaction. It then works out the length of the buffer needed for each log
 * item, allocates them and formats the vector for the item into the buffer.
 * The buffer is then attached to the log item are then inserted into the
 * Committed Item List for tracking until the next checkpoint is written out.
 *
 * We don't set up region headers during this process; we simply copy the
 * regions into the flat buffer. We can do this because we still have to do a
 * formatting step to write the regions into the iclog buffer.  Writing the
 * ophdrs during the iclog write means that we can support splitting large
 * regions across iclog boundares without needing a change in the format of the
 * item/region encapsulation.
 *
 * Hence what we need to do now is change the rewrite the vector array to point
 * to the copied region inside the buffer we just allocated. This allows us to
 * format the regions into the iclog as though they are being formatted
 * directly out of the objects themselves.
 */
static struct xfs_log_vec *
xlog_cil_prepare_log_vecs(
	struct xfs_trans	*tp)
{
	struct xfs_log_item_desc *lidp;
	struct xfs_log_vec	*lv = NULL;
	struct xfs_log_vec	*ret_lv = NULL;


	
	if (list_empty(&tp->t_items)) {
		ASSERT(0);
		return NULL;
	}

	list_for_each_entry(lidp, &tp->t_items, lid_trans) {
		struct xfs_log_vec *new_lv;
		void	*ptr;
		int	index;
		int	len = 0;
		uint	niovecs;

		
		if (!(lidp->lid_flags & XFS_LID_DIRTY))
			continue;

		
		niovecs = IOP_SIZE(lidp->lid_item);
		if (!niovecs)
			continue;

		new_lv = kmem_zalloc(sizeof(*new_lv) +
				niovecs * sizeof(struct xfs_log_iovec),
				KM_SLEEP);

		
		new_lv->lv_iovecp = (struct xfs_log_iovec *)&new_lv[1];
		new_lv->lv_niovecs = niovecs;
		new_lv->lv_item = lidp->lid_item;

		
		IOP_FORMAT(new_lv->lv_item, new_lv->lv_iovecp);
		for (index = 0; index < new_lv->lv_niovecs; index++)
			len += new_lv->lv_iovecp[index].i_len;

		new_lv->lv_buf_len = len;
		new_lv->lv_buf = kmem_alloc(new_lv->lv_buf_len,
				KM_SLEEP|KM_NOFS);
		ptr = new_lv->lv_buf;

		for (index = 0; index < new_lv->lv_niovecs; index++) {
			struct xfs_log_iovec *vec = &new_lv->lv_iovecp[index];

			memcpy(ptr, vec->i_addr, vec->i_len);
			vec->i_addr = ptr;
			ptr += vec->i_len;
		}
		ASSERT(ptr == new_lv->lv_buf + new_lv->lv_buf_len);

		if (!ret_lv)
			ret_lv = new_lv;
		else
			lv->lv_next = new_lv;
		lv = new_lv;
	}

	return ret_lv;
}

STATIC void
xfs_cil_prepare_item(
	struct log		*log,
	struct xfs_log_vec	*lv,
	int			*len,
	int			*diff_iovecs)
{
	struct xfs_log_vec	*old = lv->lv_item->li_lv;

	if (old) {
		
		ASSERT(!list_empty(&lv->lv_item->li_cil));
		ASSERT(old->lv_buf && old->lv_buf_len && old->lv_niovecs);

		*len += lv->lv_buf_len - old->lv_buf_len;
		*diff_iovecs += lv->lv_niovecs - old->lv_niovecs;
		kmem_free(old->lv_buf);
		kmem_free(old);
	} else {
		
		ASSERT(!lv->lv_item->li_lv);
		ASSERT(list_empty(&lv->lv_item->li_cil));

		*len += lv->lv_buf_len;
		*diff_iovecs += lv->lv_niovecs;
		IOP_PIN(lv->lv_item);

	}

	
	lv->lv_item->li_lv = lv;

	if (!lv->lv_item->li_seq)
		lv->lv_item->li_seq = log->l_cilp->xc_ctx->sequence;
}

static void
xlog_cil_insert_items(
	struct log		*log,
	struct xfs_log_vec	*log_vector,
	struct xlog_ticket	*ticket)
{
	struct xfs_cil		*cil = log->l_cilp;
	struct xfs_cil_ctx	*ctx = cil->xc_ctx;
	struct xfs_log_vec	*lv;
	int			len = 0;
	int			diff_iovecs = 0;
	int			iclog_space;

	ASSERT(log_vector);

	/*
	 * Do all the accounting aggregation and switching of log vectors
	 * around in a separate loop to the insertion of items into the CIL.
	 * Then we can do a separate loop to update the CIL within a single
	 * lock/unlock pair. This reduces the number of round trips on the CIL
	 * lock from O(nr_logvectors) to O(1) and greatly reduces the overall
	 * hold time for the transaction commit.
	 *
	 * If this is the first time the item is being placed into the CIL in
	 * this context, pin it so it can't be written to disk until the CIL is
	 * flushed to the iclog and the iclog written to disk.
	 *
	 * We can do this safely because the context can't checkpoint until we
	 * are done so it doesn't matter exactly how we update the CIL.
	 */
	for (lv = log_vector; lv; lv = lv->lv_next)
		xfs_cil_prepare_item(log, lv, &len, &diff_iovecs);

	
	len += diff_iovecs * sizeof(xlog_op_header_t);

	spin_lock(&cil->xc_cil_lock);

	
	for (lv = log_vector; lv; lv = lv->lv_next)
		list_move_tail(&lv->lv_item->li_cil, &cil->xc_cil);

	ctx->nvecs += diff_iovecs;

	if (ctx->ticket->t_curr_res == 0) {
		
		ASSERT(ticket->t_curr_res >= ctx->ticket->t_unit_res + len);
		ctx->ticket->t_curr_res = ctx->ticket->t_unit_res;
		ticket->t_curr_res -= ctx->ticket->t_unit_res;
	}

	
	iclog_space = log->l_iclog_size - log->l_iclog_hsize;
	if (len > 0 && (ctx->space_used / iclog_space !=
				(ctx->space_used + len) / iclog_space)) {
		int hdrs;

		hdrs = (len + iclog_space - 1) / iclog_space;
		
		hdrs *= log->l_iclog_hsize + sizeof(struct xlog_op_header);
		ctx->ticket->t_unit_res += hdrs;
		ctx->ticket->t_curr_res += hdrs;
		ticket->t_curr_res -= hdrs;
		ASSERT(ticket->t_curr_res >= len);
	}
	ticket->t_curr_res -= len;
	ctx->space_used += len;

	spin_unlock(&cil->xc_cil_lock);
}

static void
xlog_cil_free_logvec(
	struct xfs_log_vec	*log_vector)
{
	struct xfs_log_vec	*lv;

	for (lv = log_vector; lv; ) {
		struct xfs_log_vec *next = lv->lv_next;
		kmem_free(lv->lv_buf);
		kmem_free(lv);
		lv = next;
	}
}

static void
xlog_cil_committed(
	void	*args,
	int	abort)
{
	struct xfs_cil_ctx	*ctx = args;
	struct xfs_mount	*mp = ctx->cil->xc_log->l_mp;

	xfs_trans_committed_bulk(ctx->cil->xc_log->l_ailp, ctx->lv_chain,
					ctx->start_lsn, abort);

	xfs_alloc_busy_sort(&ctx->busy_extents);
	xfs_alloc_busy_clear(mp, &ctx->busy_extents,
			     (mp->m_flags & XFS_MOUNT_DISCARD) && !abort);

	spin_lock(&ctx->cil->xc_cil_lock);
	list_del(&ctx->committing);
	spin_unlock(&ctx->cil->xc_cil_lock);

	xlog_cil_free_logvec(ctx->lv_chain);

	if (!list_empty(&ctx->busy_extents)) {
		ASSERT(mp->m_flags & XFS_MOUNT_DISCARD);

		xfs_discard_extents(mp, &ctx->busy_extents);
		xfs_alloc_busy_clear(mp, &ctx->busy_extents, false);
	}

	kmem_free(ctx);
}

STATIC int
xlog_cil_push(
	struct log		*log,
	xfs_lsn_t		push_seq)
{
	struct xfs_cil		*cil = log->l_cilp;
	struct xfs_log_vec	*lv;
	struct xfs_cil_ctx	*ctx;
	struct xfs_cil_ctx	*new_ctx;
	struct xlog_in_core	*commit_iclog;
	struct xlog_ticket	*tic;
	int			num_lv;
	int			num_iovecs;
	int			len;
	int			error = 0;
	struct xfs_trans_header thdr;
	struct xfs_log_iovec	lhdr;
	struct xfs_log_vec	lvhdr = { NULL };
	xfs_lsn_t		commit_lsn;

	if (!cil)
		return 0;

	ASSERT(!push_seq || push_seq <= cil->xc_ctx->sequence);

	new_ctx = kmem_zalloc(sizeof(*new_ctx), KM_SLEEP|KM_NOFS);
	new_ctx->ticket = xlog_cil_ticket_alloc(log);

	if (!down_write_trylock(&cil->xc_ctx_lock)) {
		if (!push_seq &&
		    cil->xc_ctx->space_used < XLOG_CIL_HARD_SPACE_LIMIT(log))
			goto out_free_ticket;
		down_write(&cil->xc_ctx_lock);
	}
	ctx = cil->xc_ctx;

	
	if (list_empty(&cil->xc_cil))
		goto out_skip;

	
	if (!push_seq && cil->xc_ctx->space_used < XLOG_CIL_SPACE_LIMIT(log))
		goto out_skip;

	
	if (push_seq && push_seq < cil->xc_ctx->sequence)
		goto out_skip;

	lv = NULL;
	num_lv = 0;
	num_iovecs = 0;
	len = 0;
	while (!list_empty(&cil->xc_cil)) {
		struct xfs_log_item	*item;
		int			i;

		item = list_first_entry(&cil->xc_cil,
					struct xfs_log_item, li_cil);
		list_del_init(&item->li_cil);
		if (!ctx->lv_chain)
			ctx->lv_chain = item->li_lv;
		else
			lv->lv_next = item->li_lv;
		lv = item->li_lv;
		item->li_lv = NULL;

		num_lv++;
		num_iovecs += lv->lv_niovecs;
		for (i = 0; i < lv->lv_niovecs; i++)
			len += lv->lv_iovecp[i].i_len;
	}

	INIT_LIST_HEAD(&new_ctx->committing);
	INIT_LIST_HEAD(&new_ctx->busy_extents);
	new_ctx->sequence = ctx->sequence + 1;
	new_ctx->cil = cil;
	cil->xc_ctx = new_ctx;

	cil->xc_current_sequence = new_ctx->sequence;

	spin_lock(&cil->xc_cil_lock);
	list_add(&ctx->committing, &cil->xc_committing);
	spin_unlock(&cil->xc_cil_lock);
	up_write(&cil->xc_ctx_lock);

	tic = ctx->ticket;
	thdr.th_magic = XFS_TRANS_HEADER_MAGIC;
	thdr.th_type = XFS_TRANS_CHECKPOINT;
	thdr.th_tid = tic->t_tid;
	thdr.th_num_items = num_iovecs;
	lhdr.i_addr = &thdr;
	lhdr.i_len = sizeof(xfs_trans_header_t);
	lhdr.i_type = XLOG_REG_TYPE_TRANSHDR;
	tic->t_curr_res -= lhdr.i_len + sizeof(xlog_op_header_t);

	lvhdr.lv_niovecs = 1;
	lvhdr.lv_iovecp = &lhdr;
	lvhdr.lv_next = ctx->lv_chain;

	error = xlog_write(log, &lvhdr, tic, &ctx->start_lsn, NULL, 0);
	if (error)
		goto out_abort_free_ticket;

	/*
	 * now that we've written the checkpoint into the log, strictly
	 * order the commit records so replay will get them in the right order.
	 */
restart:
	spin_lock(&cil->xc_cil_lock);
	list_for_each_entry(new_ctx, &cil->xc_committing, committing) {
		if (new_ctx->sequence >= ctx->sequence)
			continue;
		if (!new_ctx->commit_lsn) {
			xlog_wait(&cil->xc_commit_wait, &cil->xc_cil_lock);
			goto restart;
		}
	}
	spin_unlock(&cil->xc_cil_lock);

	
	commit_lsn = xfs_log_done(log->l_mp, tic, &commit_iclog, 0);
	if (commit_lsn == -1)
		goto out_abort;

	
	ctx->log_cb.cb_func = xlog_cil_committed;
	ctx->log_cb.cb_arg = ctx;
	error = xfs_log_notify(log->l_mp, commit_iclog, &ctx->log_cb);
	if (error)
		goto out_abort;

	spin_lock(&cil->xc_cil_lock);
	ctx->commit_lsn = commit_lsn;
	wake_up_all(&cil->xc_commit_wait);
	spin_unlock(&cil->xc_cil_lock);

	
	return xfs_log_release_iclog(log->l_mp, commit_iclog);

out_skip:
	up_write(&cil->xc_ctx_lock);
out_free_ticket:
	xfs_log_ticket_put(new_ctx->ticket);
	kmem_free(new_ctx);
	return 0;

out_abort_free_ticket:
	xfs_log_ticket_put(tic);
out_abort:
	xlog_cil_committed(ctx, XFS_LI_ABORTED);
	return XFS_ERROR(EIO);
}

int
xfs_log_commit_cil(
	struct xfs_mount	*mp,
	struct xfs_trans	*tp,
	xfs_lsn_t		*commit_lsn,
	int			flags)
{
	struct log		*log = mp->m_log;
	int			log_flags = 0;
	int			push = 0;
	struct xfs_log_vec	*log_vector;

	if (flags & XFS_TRANS_RELEASE_LOG_RES)
		log_flags = XFS_LOG_REL_PERM_RESERV;

	log_vector = xlog_cil_prepare_log_vecs(tp);
	if (!log_vector)
		return ENOMEM;

	
	down_read(&log->l_cilp->xc_ctx_lock);
	if (commit_lsn)
		*commit_lsn = log->l_cilp->xc_ctx->sequence;

	xlog_cil_insert_items(log, log_vector, tp->t_ticket);

	
	if (tp->t_ticket->t_curr_res < 0)
		xlog_print_tic_res(log->l_mp, tp->t_ticket);

	
	if (!list_empty(&tp->t_busy)) {
		spin_lock(&log->l_cilp->xc_cil_lock);
		list_splice_init(&tp->t_busy,
					&log->l_cilp->xc_ctx->busy_extents);
		spin_unlock(&log->l_cilp->xc_cil_lock);
	}

	tp->t_commit_lsn = *commit_lsn;
	xfs_log_done(mp, tp->t_ticket, NULL, log_flags);
	xfs_trans_unreserve_and_mod_sb(tp);

	xfs_trans_free_items(tp, *commit_lsn, 0);

	
	if (log->l_cilp->xc_ctx->space_used > XLOG_CIL_SPACE_LIMIT(log))
		push = 1;

	up_read(&log->l_cilp->xc_ctx_lock);

	if (push)
		xlog_cil_push(log, 0);
	return 0;
}

xfs_lsn_t
xlog_cil_force_lsn(
	struct log	*log,
	xfs_lsn_t	sequence)
{
	struct xfs_cil		*cil = log->l_cilp;
	struct xfs_cil_ctx	*ctx;
	xfs_lsn_t		commit_lsn = NULLCOMMITLSN;

	ASSERT(sequence <= cil->xc_current_sequence);

	if (sequence == cil->xc_current_sequence)
		xlog_cil_push(log, sequence);

restart:
	spin_lock(&cil->xc_cil_lock);
	list_for_each_entry(ctx, &cil->xc_committing, committing) {
		if (ctx->sequence > sequence)
			continue;
		if (!ctx->commit_lsn) {
			xlog_wait(&cil->xc_commit_wait, &cil->xc_cil_lock);
			goto restart;
		}
		if (ctx->sequence != sequence)
			continue;
		
		commit_lsn = ctx->commit_lsn;
	}
	spin_unlock(&cil->xc_cil_lock);
	return commit_lsn;
}

bool
xfs_log_item_in_current_chkpt(
	struct xfs_log_item *lip)
{
	struct xfs_cil_ctx *ctx;

	if (list_empty(&lip->li_cil))
		return false;

	ctx = lip->li_mountp->m_log->l_cilp->xc_ctx;

	/*
	 * li_seq is written on the first commit of a log item to record the
	 * first checkpoint it is written to. Hence if it is different to the
	 * current sequence, we're in a new checkpoint.
	 */
	if (XFS_LSN_CMP(lip->li_seq, ctx->sequence) != 0)
		return false;
	return true;
}
