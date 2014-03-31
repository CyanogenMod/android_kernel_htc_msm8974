/*
 * Read-Copy Update mechanism for mutual exclusion, the Bloatwatch edition
 * Internal non-public definitions that provide either classic
 * or preemptible semantics.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (c) 2010 Linaro
 *
 * Author: Paul E. McKenney <paulmck@linux.vnet.ibm.com>
 */

#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

struct rcu_ctrlblk {
	struct rcu_head *rcucblist;	
	struct rcu_head **donetail;	
	struct rcu_head **curtail;	
	RCU_TRACE(long qlen);		
	RCU_TRACE(char *name);		
};

static struct rcu_ctrlblk rcu_sched_ctrlblk = {
	.donetail	= &rcu_sched_ctrlblk.rcucblist,
	.curtail	= &rcu_sched_ctrlblk.rcucblist,
	RCU_TRACE(.name = "rcu_sched")
};

static struct rcu_ctrlblk rcu_bh_ctrlblk = {
	.donetail	= &rcu_bh_ctrlblk.rcucblist,
	.curtail	= &rcu_bh_ctrlblk.rcucblist,
	RCU_TRACE(.name = "rcu_bh")
};

#ifdef CONFIG_DEBUG_LOCK_ALLOC
int rcu_scheduler_active __read_mostly;
EXPORT_SYMBOL_GPL(rcu_scheduler_active);
#endif 

#ifdef CONFIG_TINY_PREEMPT_RCU

#include <linux/delay.h>

struct rcu_preempt_ctrlblk {
	struct rcu_ctrlblk rcb;	
	struct rcu_head **nexttail;
				
				
				
				
				
				
				
				
				
	struct list_head blkd_tasks;
				
				
				
	struct list_head *gp_tasks;
				
				
				
	struct list_head *exp_tasks;
				
				
				
				
				
#ifdef CONFIG_RCU_BOOST
	struct list_head *boost_tasks;
				
				
				
				
				
#endif 
	u8 gpnum;		
	u8 gpcpu;		
	u8 completed;		
				
#ifdef CONFIG_RCU_BOOST
	unsigned long boost_time; 
#endif 
#ifdef CONFIG_RCU_TRACE
	unsigned long n_grace_periods;
#ifdef CONFIG_RCU_BOOST
	unsigned long n_tasks_boosted;
				
	unsigned long n_exp_boosts;
				
	unsigned long n_normal_boosts;
				
	unsigned long n_balk_blkd_tasks;
				
	unsigned long n_balk_exp_gp_tasks;
				
	unsigned long n_balk_boost_tasks;
				
	unsigned long n_balk_notyet;
				
	unsigned long n_balk_nos;
				
				
#endif 
#endif 
};

static struct rcu_preempt_ctrlblk rcu_preempt_ctrlblk = {
	.rcb.donetail = &rcu_preempt_ctrlblk.rcb.rcucblist,
	.rcb.curtail = &rcu_preempt_ctrlblk.rcb.rcucblist,
	.nexttail = &rcu_preempt_ctrlblk.rcb.rcucblist,
	.blkd_tasks = LIST_HEAD_INIT(rcu_preempt_ctrlblk.blkd_tasks),
	RCU_TRACE(.rcb.name = "rcu_preempt")
};

static void rcu_read_unlock_special(struct task_struct *t);
static int rcu_preempted_readers_exp(void);
static void rcu_report_exp_done(void);

static int rcu_cpu_blocking_cur_gp(void)
{
	return rcu_preempt_ctrlblk.gpcpu != rcu_preempt_ctrlblk.gpnum;
}

static int rcu_preempt_running_reader(void)
{
	return current->rcu_read_lock_nesting;
}

static int rcu_preempt_blocked_readers_any(void)
{
	return !list_empty(&rcu_preempt_ctrlblk.blkd_tasks);
}

static int rcu_preempt_blocked_readers_cgp(void)
{
	return rcu_preempt_ctrlblk.gp_tasks != NULL;
}

static int rcu_preempt_needs_another_gp(void)
{
	return *rcu_preempt_ctrlblk.rcb.curtail != NULL;
}

static int rcu_preempt_gp_in_progress(void)
{
	return rcu_preempt_ctrlblk.completed != rcu_preempt_ctrlblk.gpnum;
}

static struct list_head *rcu_next_node_entry(struct task_struct *t)
{
	struct list_head *np;

	np = t->rcu_node_entry.next;
	if (np == &rcu_preempt_ctrlblk.blkd_tasks)
		np = NULL;
	return np;
}

#ifdef CONFIG_RCU_TRACE

#ifdef CONFIG_RCU_BOOST
static void rcu_initiate_boost_trace(void);
#endif 

static void show_tiny_preempt_stats(struct seq_file *m)
{
	seq_printf(m, "rcu_preempt: qlen=%ld gp=%lu g%u/p%u/c%u tasks=%c%c%c\n",
		   rcu_preempt_ctrlblk.rcb.qlen,
		   rcu_preempt_ctrlblk.n_grace_periods,
		   rcu_preempt_ctrlblk.gpnum,
		   rcu_preempt_ctrlblk.gpcpu,
		   rcu_preempt_ctrlblk.completed,
		   "T."[list_empty(&rcu_preempt_ctrlblk.blkd_tasks)],
		   "N."[!rcu_preempt_ctrlblk.gp_tasks],
		   "E."[!rcu_preempt_ctrlblk.exp_tasks]);
#ifdef CONFIG_RCU_BOOST
	seq_printf(m, "%sttb=%c ntb=%lu neb=%lu nnb=%lu j=%04x bt=%04x\n",
		   "             ",
		   "B."[!rcu_preempt_ctrlblk.boost_tasks],
		   rcu_preempt_ctrlblk.n_tasks_boosted,
		   rcu_preempt_ctrlblk.n_exp_boosts,
		   rcu_preempt_ctrlblk.n_normal_boosts,
		   (int)(jiffies & 0xffff),
		   (int)(rcu_preempt_ctrlblk.boost_time & 0xffff));
	seq_printf(m, "%s: nt=%lu egt=%lu bt=%lu ny=%lu nos=%lu\n",
		   "             balk",
		   rcu_preempt_ctrlblk.n_balk_blkd_tasks,
		   rcu_preempt_ctrlblk.n_balk_exp_gp_tasks,
		   rcu_preempt_ctrlblk.n_balk_boost_tasks,
		   rcu_preempt_ctrlblk.n_balk_notyet,
		   rcu_preempt_ctrlblk.n_balk_nos);
#endif 
}

#endif 

#ifdef CONFIG_RCU_BOOST

#include "rtmutex_common.h"

#define RCU_BOOST_PRIO CONFIG_RCU_BOOST_PRIO

static struct task_struct *rcu_kthread_task;
static DECLARE_WAIT_QUEUE_HEAD(rcu_kthread_wq);
static unsigned long have_rcu_kthread_work;

static int rcu_boost(void)
{
	unsigned long flags;
	struct rt_mutex mtx;
	struct task_struct *t;
	struct list_head *tb;

	if (rcu_preempt_ctrlblk.boost_tasks == NULL &&
	    rcu_preempt_ctrlblk.exp_tasks == NULL)
		return 0;  

	raw_local_irq_save(flags);

	if (rcu_preempt_ctrlblk.boost_tasks == NULL &&
	    rcu_preempt_ctrlblk.exp_tasks == NULL) {
		raw_local_irq_restore(flags);
		return 0;
	}

	if (rcu_preempt_ctrlblk.exp_tasks != NULL) {
		tb = rcu_preempt_ctrlblk.exp_tasks;
		RCU_TRACE(rcu_preempt_ctrlblk.n_exp_boosts++);
	} else {
		tb = rcu_preempt_ctrlblk.boost_tasks;
		RCU_TRACE(rcu_preempt_ctrlblk.n_normal_boosts++);
	}
	RCU_TRACE(rcu_preempt_ctrlblk.n_tasks_boosted++);

	t = container_of(tb, struct task_struct, rcu_node_entry);
	rt_mutex_init_proxy_locked(&mtx, t);
	t->rcu_boost_mutex = &mtx;
	raw_local_irq_restore(flags);
	rt_mutex_lock(&mtx);
	rt_mutex_unlock(&mtx);  

	return ACCESS_ONCE(rcu_preempt_ctrlblk.boost_tasks) != NULL ||
	       ACCESS_ONCE(rcu_preempt_ctrlblk.exp_tasks) != NULL;
}

static int rcu_initiate_boost(void)
{
	if (!rcu_preempt_blocked_readers_cgp() &&
	    rcu_preempt_ctrlblk.exp_tasks == NULL) {
		RCU_TRACE(rcu_preempt_ctrlblk.n_balk_exp_gp_tasks++);
		return 0;
	}
	if (rcu_preempt_ctrlblk.exp_tasks != NULL ||
	    (rcu_preempt_ctrlblk.gp_tasks != NULL &&
	     rcu_preempt_ctrlblk.boost_tasks == NULL &&
	     ULONG_CMP_GE(jiffies, rcu_preempt_ctrlblk.boost_time))) {
		if (rcu_preempt_ctrlblk.exp_tasks == NULL)
			rcu_preempt_ctrlblk.boost_tasks =
				rcu_preempt_ctrlblk.gp_tasks;
		invoke_rcu_callbacks();
	} else
		RCU_TRACE(rcu_initiate_boost_trace());
	return 1;
}

#define RCU_BOOST_DELAY_JIFFIES DIV_ROUND_UP(CONFIG_RCU_BOOST_DELAY * HZ, 1000)

static void rcu_preempt_boost_start_gp(void)
{
	rcu_preempt_ctrlblk.boost_time = jiffies + RCU_BOOST_DELAY_JIFFIES;
}

#else 

static int rcu_initiate_boost(void)
{
	return rcu_preempt_blocked_readers_cgp();
}

static void rcu_preempt_boost_start_gp(void)
{
}

#endif 

static void rcu_preempt_cpu_qs(void)
{
	
	rcu_preempt_ctrlblk.gpcpu = rcu_preempt_ctrlblk.gpnum;
	current->rcu_read_unlock_special &= ~RCU_READ_UNLOCK_NEED_QS;

	
	if (!rcu_preempt_gp_in_progress())
		return;
	if (rcu_initiate_boost())
		return;

	
	rcu_preempt_ctrlblk.completed = rcu_preempt_ctrlblk.gpnum;
	rcu_preempt_ctrlblk.rcb.donetail = rcu_preempt_ctrlblk.rcb.curtail;
	rcu_preempt_ctrlblk.rcb.curtail = rcu_preempt_ctrlblk.nexttail;

	
	if (!rcu_preempt_blocked_readers_any())
		rcu_preempt_ctrlblk.rcb.donetail = rcu_preempt_ctrlblk.nexttail;

	
	if (*rcu_preempt_ctrlblk.rcb.donetail != NULL)
		invoke_rcu_callbacks();
}

static void rcu_preempt_start_gp(void)
{
	if (!rcu_preempt_gp_in_progress() && rcu_preempt_needs_another_gp()) {

		
		rcu_preempt_ctrlblk.gpnum++;
		RCU_TRACE(rcu_preempt_ctrlblk.n_grace_periods++);

		
		if (rcu_preempt_blocked_readers_any())
			rcu_preempt_ctrlblk.gp_tasks =
				rcu_preempt_ctrlblk.blkd_tasks.next;

		
		rcu_preempt_boost_start_gp();

		
		if (!rcu_preempt_running_reader())
			rcu_preempt_cpu_qs();
	}
}

void rcu_preempt_note_context_switch(void)
{
	struct task_struct *t = current;
	unsigned long flags;

	local_irq_save(flags); 
	if (rcu_preempt_running_reader() > 0 &&
	    (t->rcu_read_unlock_special & RCU_READ_UNLOCK_BLOCKED) == 0) {

		
		t->rcu_read_unlock_special |= RCU_READ_UNLOCK_BLOCKED;

		list_add(&t->rcu_node_entry, &rcu_preempt_ctrlblk.blkd_tasks);
		if (rcu_cpu_blocking_cur_gp())
			rcu_preempt_ctrlblk.gp_tasks = &t->rcu_node_entry;
	} else if (rcu_preempt_running_reader() < 0 &&
		   t->rcu_read_unlock_special) {
		rcu_read_unlock_special(t);
	}

	rcu_preempt_cpu_qs();
	local_irq_restore(flags);
}

void __rcu_read_lock(void)
{
	current->rcu_read_lock_nesting++;
	barrier();  
}
EXPORT_SYMBOL_GPL(__rcu_read_lock);

static noinline void rcu_read_unlock_special(struct task_struct *t)
{
	int empty;
	int empty_exp;
	unsigned long flags;
	struct list_head *np;
#ifdef CONFIG_RCU_BOOST
	struct rt_mutex *rbmp = NULL;
#endif 
	int special;

	if (in_nmi())
		return;

	local_irq_save(flags);

	special = t->rcu_read_unlock_special;
	if (special & RCU_READ_UNLOCK_NEED_QS)
		rcu_preempt_cpu_qs();

	
	if (in_irq() || in_serving_softirq()) {
		local_irq_restore(flags);
		return;
	}

	
	if (special & RCU_READ_UNLOCK_BLOCKED) {
		t->rcu_read_unlock_special &= ~RCU_READ_UNLOCK_BLOCKED;

		empty = !rcu_preempt_blocked_readers_cgp();
		empty_exp = rcu_preempt_ctrlblk.exp_tasks == NULL;
		np = rcu_next_node_entry(t);
		list_del_init(&t->rcu_node_entry);
		if (&t->rcu_node_entry == rcu_preempt_ctrlblk.gp_tasks)
			rcu_preempt_ctrlblk.gp_tasks = np;
		if (&t->rcu_node_entry == rcu_preempt_ctrlblk.exp_tasks)
			rcu_preempt_ctrlblk.exp_tasks = np;
#ifdef CONFIG_RCU_BOOST
		if (&t->rcu_node_entry == rcu_preempt_ctrlblk.boost_tasks)
			rcu_preempt_ctrlblk.boost_tasks = np;
#endif 

		if (!empty && !rcu_preempt_blocked_readers_cgp()) {
			rcu_preempt_cpu_qs();
			rcu_preempt_start_gp();
		}

		if (!empty_exp && rcu_preempt_ctrlblk.exp_tasks == NULL)
			rcu_report_exp_done();
	}
#ifdef CONFIG_RCU_BOOST
	
	if (t->rcu_boost_mutex != NULL) {
		rbmp = t->rcu_boost_mutex;
		t->rcu_boost_mutex = NULL;
		rt_mutex_unlock(rbmp);
	}
#endif 
	local_irq_restore(flags);
}

void __rcu_read_unlock(void)
{
	struct task_struct *t = current;

	barrier();  
	if (t->rcu_read_lock_nesting != 1)
		--t->rcu_read_lock_nesting;
	else {
		t->rcu_read_lock_nesting = INT_MIN;
		barrier();  
		if (unlikely(ACCESS_ONCE(t->rcu_read_unlock_special)))
			rcu_read_unlock_special(t);
		barrier();  
		t->rcu_read_lock_nesting = 0;
	}
#ifdef CONFIG_PROVE_LOCKING
	{
		int rrln = ACCESS_ONCE(t->rcu_read_lock_nesting);

		WARN_ON_ONCE(rrln < 0 && rrln > INT_MIN / 2);
	}
#endif 
}
EXPORT_SYMBOL_GPL(__rcu_read_unlock);

static void rcu_preempt_check_callbacks(void)
{
	struct task_struct *t = current;

	if (rcu_preempt_gp_in_progress() &&
	    (!rcu_preempt_running_reader() ||
	     !rcu_cpu_blocking_cur_gp()))
		rcu_preempt_cpu_qs();
	if (&rcu_preempt_ctrlblk.rcb.rcucblist !=
	    rcu_preempt_ctrlblk.rcb.donetail)
		invoke_rcu_callbacks();
	if (rcu_preempt_gp_in_progress() &&
	    rcu_cpu_blocking_cur_gp() &&
	    rcu_preempt_running_reader() > 0)
		t->rcu_read_unlock_special |= RCU_READ_UNLOCK_NEED_QS;
}

static void rcu_preempt_remove_callbacks(struct rcu_ctrlblk *rcp)
{
	if (rcu_preempt_ctrlblk.nexttail == rcp->donetail)
		rcu_preempt_ctrlblk.nexttail = &rcp->rcucblist;
}

static void rcu_preempt_process_callbacks(void)
{
	__rcu_process_callbacks(&rcu_preempt_ctrlblk.rcb);
}

void call_rcu(struct rcu_head *head, void (*func)(struct rcu_head *rcu))
{
	unsigned long flags;

	debug_rcu_head_queue(head);
	head->func = func;
	head->next = NULL;

	local_irq_save(flags);
	*rcu_preempt_ctrlblk.nexttail = head;
	rcu_preempt_ctrlblk.nexttail = &head->next;
	RCU_TRACE(rcu_preempt_ctrlblk.rcb.qlen++);
	rcu_preempt_start_gp();  
	local_irq_restore(flags);
}
EXPORT_SYMBOL_GPL(call_rcu);

void synchronize_rcu(void)
{
	rcu_lockdep_assert(!lock_is_held(&rcu_bh_lock_map) &&
			   !lock_is_held(&rcu_lock_map) &&
			   !lock_is_held(&rcu_sched_lock_map),
			   "Illegal synchronize_rcu() in RCU read-side critical section");

#ifdef CONFIG_DEBUG_LOCK_ALLOC
	if (!rcu_scheduler_active)
		return;
#endif 

	WARN_ON_ONCE(rcu_preempt_running_reader());
	if (!rcu_preempt_blocked_readers_any())
		return;

	
	rcu_barrier();
}
EXPORT_SYMBOL_GPL(synchronize_rcu);

static DECLARE_WAIT_QUEUE_HEAD(sync_rcu_preempt_exp_wq);
static unsigned long sync_rcu_preempt_exp_count;
static DEFINE_MUTEX(sync_rcu_preempt_exp_mutex);

static int rcu_preempted_readers_exp(void)
{
	return rcu_preempt_ctrlblk.exp_tasks != NULL;
}

static void rcu_report_exp_done(void)
{
	wake_up(&sync_rcu_preempt_exp_wq);
}

void synchronize_rcu_expedited(void)
{
	unsigned long flags;
	struct rcu_preempt_ctrlblk *rpcp = &rcu_preempt_ctrlblk;
	unsigned long snap;

	barrier(); 

	WARN_ON_ONCE(rcu_preempt_running_reader());

	snap = sync_rcu_preempt_exp_count + 1;
	mutex_lock(&sync_rcu_preempt_exp_mutex);
	if (ULONG_CMP_LT(snap, sync_rcu_preempt_exp_count))
		goto unlock_mb_ret; 

	local_irq_save(flags);


	
	rpcp->exp_tasks = rpcp->blkd_tasks.next;
	if (rpcp->exp_tasks == &rpcp->blkd_tasks)
		rpcp->exp_tasks = NULL;

	
	if (!rcu_preempted_readers_exp())
		local_irq_restore(flags);
	else {
		rcu_initiate_boost();
		local_irq_restore(flags);
		wait_event(sync_rcu_preempt_exp_wq,
			   !rcu_preempted_readers_exp());
	}

	
	barrier(); 
	sync_rcu_preempt_exp_count++;
unlock_mb_ret:
	mutex_unlock(&sync_rcu_preempt_exp_mutex);
	barrier(); 
}
EXPORT_SYMBOL_GPL(synchronize_rcu_expedited);

int rcu_preempt_needs_cpu(void)
{
	if (!rcu_preempt_running_reader())
		rcu_preempt_cpu_qs();
	return rcu_preempt_ctrlblk.rcb.rcucblist != NULL;
}

void exit_rcu(void)
{
	struct task_struct *t = current;

	if (t->rcu_read_lock_nesting == 0)
		return;
	t->rcu_read_lock_nesting = 1;
	__rcu_read_unlock();
}

#else 

#ifdef CONFIG_RCU_TRACE

static void show_tiny_preempt_stats(struct seq_file *m)
{
}

#endif 

static void rcu_preempt_check_callbacks(void)
{
}

static void rcu_preempt_remove_callbacks(struct rcu_ctrlblk *rcp)
{
}

static void rcu_preempt_process_callbacks(void)
{
}

#endif 

#ifdef CONFIG_RCU_BOOST

static void invoke_rcu_callbacks(void)
{
	have_rcu_kthread_work = 1;
	if (rcu_kthread_task != NULL)
		wake_up(&rcu_kthread_wq);
}

#ifdef CONFIG_RCU_TRACE

static bool rcu_is_callbacks_kthread(void)
{
	return rcu_kthread_task == current;
}

#endif 

static int rcu_kthread(void *arg)
{
	unsigned long work;
	unsigned long morework;
	unsigned long flags;

	for (;;) {
		wait_event_interruptible(rcu_kthread_wq,
					 have_rcu_kthread_work != 0);
		morework = rcu_boost();
		local_irq_save(flags);
		work = have_rcu_kthread_work;
		have_rcu_kthread_work = morework;
		local_irq_restore(flags);
		if (work)
			rcu_process_callbacks(NULL);
		schedule_timeout_interruptible(1); 
	}

	return 0;  
}

static int __init rcu_spawn_kthreads(void)
{
	struct sched_param sp;

	rcu_kthread_task = kthread_run(rcu_kthread, NULL, "rcu_kthread");
	sp.sched_priority = RCU_BOOST_PRIO;
	sched_setscheduler_nocheck(rcu_kthread_task, SCHED_FIFO, &sp);
	return 0;
}
early_initcall(rcu_spawn_kthreads);

#else 

static int rcu_scheduler_fully_active __read_mostly;

void invoke_rcu_callbacks(void)
{
	if (rcu_scheduler_fully_active)
		raise_softirq(RCU_SOFTIRQ);
}

#ifdef CONFIG_RCU_TRACE

static bool rcu_is_callbacks_kthread(void)
{
	return false;
}

#endif 

static int __init rcu_scheduler_really_started(void)
{
	rcu_scheduler_fully_active = 1;
	open_softirq(RCU_SOFTIRQ, rcu_process_callbacks);
	raise_softirq(RCU_SOFTIRQ);  
	return 0;
}
early_initcall(rcu_scheduler_really_started);

#endif 

#ifdef CONFIG_DEBUG_LOCK_ALLOC
#include <linux/kernel_stat.h>

void __init rcu_scheduler_starting(void)
{
	WARN_ON(nr_context_switches() > 0);
	rcu_scheduler_active = 1;
}

#endif 

#ifdef CONFIG_RCU_TRACE

#ifdef CONFIG_RCU_BOOST

static void rcu_initiate_boost_trace(void)
{
	if (list_empty(&rcu_preempt_ctrlblk.blkd_tasks))
		rcu_preempt_ctrlblk.n_balk_blkd_tasks++;
	else if (rcu_preempt_ctrlblk.gp_tasks == NULL &&
		 rcu_preempt_ctrlblk.exp_tasks == NULL)
		rcu_preempt_ctrlblk.n_balk_exp_gp_tasks++;
	else if (rcu_preempt_ctrlblk.boost_tasks != NULL)
		rcu_preempt_ctrlblk.n_balk_boost_tasks++;
	else if (!ULONG_CMP_GE(jiffies, rcu_preempt_ctrlblk.boost_time))
		rcu_preempt_ctrlblk.n_balk_notyet++;
	else
		rcu_preempt_ctrlblk.n_balk_nos++;
}

#endif 

static void rcu_trace_sub_qlen(struct rcu_ctrlblk *rcp, int n)
{
	unsigned long flags;

	raw_local_irq_save(flags);
	rcp->qlen -= n;
	raw_local_irq_restore(flags);
}

static int show_tiny_stats(struct seq_file *m, void *unused)
{
	show_tiny_preempt_stats(m);
	seq_printf(m, "rcu_sched: qlen: %ld\n", rcu_sched_ctrlblk.qlen);
	seq_printf(m, "rcu_bh: qlen: %ld\n", rcu_bh_ctrlblk.qlen);
	return 0;
}

static int show_tiny_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_tiny_stats, NULL);
}

static const struct file_operations show_tiny_stats_fops = {
	.owner = THIS_MODULE,
	.open = show_tiny_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static struct dentry *rcudir;

static int __init rcutiny_trace_init(void)
{
	struct dentry *retval;

	rcudir = debugfs_create_dir("rcu", NULL);
	if (!rcudir)
		goto free_out;
	retval = debugfs_create_file("rcudata", 0444, rcudir,
				     NULL, &show_tiny_stats_fops);
	if (!retval)
		goto free_out;
	return 0;
free_out:
	debugfs_remove_recursive(rcudir);
	return 1;
}

static void __exit rcutiny_trace_cleanup(void)
{
	debugfs_remove_recursive(rcudir);
}

module_init(rcutiny_trace_init);
module_exit(rcutiny_trace_cleanup);

MODULE_AUTHOR("Paul E. McKenney");
MODULE_DESCRIPTION("Read-Copy Update tracing for tiny implementation");
MODULE_LICENSE("GPL");

#endif 
