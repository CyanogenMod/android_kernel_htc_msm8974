/******************************************************************************
*******************************************************************************
**
**  Copyright (C) Sistina Software, Inc.  1997-2003  All rights reserved.
**  Copyright (C) 2004-2011 Red Hat, Inc.  All rights reserved.
**
**  This copyrighted material is made available to anyone wishing to use,
**  modify, copy, or redistribute it subject to the terms and conditions
**  of the GNU General Public License v.2.
**
*******************************************************************************
******************************************************************************/

#include "dlm_internal.h"
#include "lockspace.h"
#include "member.h"
#include "dir.h"
#include "ast.h"
#include "recover.h"
#include "lowcomms.h"
#include "lock.h"
#include "requestqueue.h"
#include "recoverd.h"



static int enable_locking(struct dlm_ls *ls, uint64_t seq)
{
	int error = -EINTR;

	down_write(&ls->ls_recv_active);

	spin_lock(&ls->ls_recover_lock);
	if (ls->ls_recover_seq == seq) {
		set_bit(LSFL_RUNNING, &ls->ls_flags);
		
		up_write(&ls->ls_in_recovery);
		error = 0;
	}
	spin_unlock(&ls->ls_recover_lock);

	up_write(&ls->ls_recv_active);
	return error;
}

static int ls_recover(struct dlm_ls *ls, struct dlm_recover *rv)
{
	unsigned long start;
	int error, neg = 0;

	log_debug(ls, "dlm_recover %llx", (unsigned long long)rv->seq);

	mutex_lock(&ls->ls_recoverd_active);

	dlm_callback_suspend(ls);


	dlm_clear_toss_list(ls);


	dlm_create_root_list(ls);


	error = dlm_recover_members(ls, rv, &neg);
	if (error) {
		log_debug(ls, "dlm_recover_members error %d", error);
		goto fail;
	}

	dlm_set_recover_status(ls, DLM_RS_NODES);

	error = dlm_recover_members_wait(ls);
	if (error) {
		log_debug(ls, "dlm_recover_members_wait error %d", error);
		goto fail;
	}

	start = jiffies;


	error = dlm_recover_directory(ls);
	if (error) {
		log_debug(ls, "dlm_recover_directory error %d", error);
		goto fail;
	}

	dlm_set_recover_status(ls, DLM_RS_DIR);

	error = dlm_recover_directory_wait(ls);
	if (error) {
		log_debug(ls, "dlm_recover_directory_wait error %d", error);
		goto fail;
	}


	dlm_recover_waiters_pre(ls);

	error = dlm_recovery_stopped(ls);
	if (error)
		goto fail;

	if (neg || dlm_no_directory(ls)) {

		dlm_purge_locks(ls);


		error = dlm_recover_masters(ls);
		if (error) {
			log_debug(ls, "dlm_recover_masters error %d", error);
			goto fail;
		}


		error = dlm_recover_locks(ls);
		if (error) {
			log_debug(ls, "dlm_recover_locks error %d", error);
			goto fail;
		}

		dlm_set_recover_status(ls, DLM_RS_LOCKS);

		error = dlm_recover_locks_wait(ls);
		if (error) {
			log_debug(ls, "dlm_recover_locks_wait error %d", error);
			goto fail;
		}


		dlm_recover_rsbs(ls);
	} else {
		dlm_set_recover_status(ls, DLM_RS_LOCKS);

		error = dlm_recover_locks_wait(ls);
		if (error) {
			log_debug(ls, "dlm_recover_locks_wait error %d", error);
			goto fail;
		}
	}

	dlm_release_root_list(ls);


	dlm_purge_requestqueue(ls);

	dlm_set_recover_status(ls, DLM_RS_DONE);

	error = dlm_recover_done_wait(ls);
	if (error) {
		log_debug(ls, "dlm_recover_done_wait error %d", error);
		goto fail;
	}

	dlm_clear_members_gone(ls);

	dlm_adjust_timeouts(ls);

	dlm_callback_resume(ls);

	error = enable_locking(ls, rv->seq);
	if (error) {
		log_debug(ls, "enable_locking error %d", error);
		goto fail;
	}

	error = dlm_process_requestqueue(ls);
	if (error) {
		log_debug(ls, "dlm_process_requestqueue error %d", error);
		goto fail;
	}

	error = dlm_recover_waiters_post(ls);
	if (error) {
		log_debug(ls, "dlm_recover_waiters_post error %d", error);
		goto fail;
	}

	dlm_grant_after_purge(ls);

	log_debug(ls, "dlm_recover %llx generation %u done: %u ms",
		  (unsigned long long)rv->seq, ls->ls_generation,
		  jiffies_to_msecs(jiffies - start));
	mutex_unlock(&ls->ls_recoverd_active);

	dlm_lsop_recover_done(ls);
	return 0;

 fail:
	dlm_release_root_list(ls);
	log_debug(ls, "dlm_recover %llx error %d",
		  (unsigned long long)rv->seq, error);
	mutex_unlock(&ls->ls_recoverd_active);
	return error;
}


static void do_ls_recovery(struct dlm_ls *ls)
{
	struct dlm_recover *rv = NULL;

	spin_lock(&ls->ls_recover_lock);
	rv = ls->ls_recover_args;
	ls->ls_recover_args = NULL;
	if (rv && ls->ls_recover_seq == rv->seq)
		clear_bit(LSFL_RECOVERY_STOP, &ls->ls_flags);
	spin_unlock(&ls->ls_recover_lock);

	if (rv) {
		ls_recover(ls, rv);
		kfree(rv->nodes);
		kfree(rv);
	}
}

static int dlm_recoverd(void *arg)
{
	struct dlm_ls *ls;

	ls = dlm_find_lockspace_local(arg);
	if (!ls) {
		log_print("dlm_recoverd: no lockspace %p", arg);
		return -1;
	}

	while (!kthread_should_stop()) {
		set_current_state(TASK_INTERRUPTIBLE);
		if (!test_bit(LSFL_WORK, &ls->ls_flags))
			schedule();
		set_current_state(TASK_RUNNING);

		if (test_and_clear_bit(LSFL_WORK, &ls->ls_flags))
			do_ls_recovery(ls);
	}

	dlm_put_lockspace(ls);
	return 0;
}

void dlm_recoverd_kick(struct dlm_ls *ls)
{
	set_bit(LSFL_WORK, &ls->ls_flags);
	wake_up_process(ls->ls_recoverd_task);
}

int dlm_recoverd_start(struct dlm_ls *ls)
{
	struct task_struct *p;
	int error = 0;

	p = kthread_run(dlm_recoverd, ls, "dlm_recoverd");
	if (IS_ERR(p))
		error = PTR_ERR(p);
	else
                ls->ls_recoverd_task = p;
	return error;
}

void dlm_recoverd_stop(struct dlm_ls *ls)
{
	kthread_stop(ls->ls_recoverd_task);
}

void dlm_recoverd_suspend(struct dlm_ls *ls)
{
	wake_up(&ls->ls_wait_general);
	mutex_lock(&ls->ls_recoverd_active);
}

void dlm_recoverd_resume(struct dlm_ls *ls)
{
	mutex_unlock(&ls->ls_recoverd_active);
}

