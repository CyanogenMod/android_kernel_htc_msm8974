/*
 *  kernel/cpuset.c
 *
 *  Processor and Memory placement constraints for sets of tasks.
 *
 *  Copyright (C) 2003 BULL SA.
 *  Copyright (C) 2004-2007 Silicon Graphics, Inc.
 *  Copyright (C) 2006 Google, Inc
 *
 *  Portions derived from Patrick Mochel's sysfs code.
 *  sysfs is Copyright (c) 2001-3 Patrick Mochel
 *
 *  2003-10-10 Written by Simon Derr.
 *  2003-10-22 Updates by Stephen Hemminger.
 *  2004 May-July Rework by Paul Jackson.
 *  2006 Rework by Paul Menage to use generic cgroups
 *  2008 Rework of the scheduler domains and CPU hotplug handling
 *       by Max Krasnyansky
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License.  See the file COPYING in the main directory of the Linux
 *  distribution for more details.
 */

#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/cpuset.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kmod.h>
#include <linux/list.h>
#include <linux/mempolicy.h>
#include <linux/mm.h>
#include <linux/memory.h>
#include <linux/export.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/pagemap.h>
#include <linux/proc_fs.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/security.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/backing-dev.h>
#include <linux/sort.h>

#include <asm/uaccess.h>
#include <linux/atomic.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/cgroup.h>

static struct workqueue_struct *cpuset_wq;

int number_of_cpusets __read_mostly;

struct cgroup_subsys cpuset_subsys;
struct cpuset;


struct fmeter {
	int cnt;		
	int val;		
	time_t time;		
	spinlock_t lock;	
};

struct cpuset {
	struct cgroup_subsys_state css;

	unsigned long flags;		
	cpumask_var_t cpus_allowed;	
	nodemask_t mems_allowed;	

	struct cpuset *parent;		

	struct fmeter fmeter;		

	
	int pn;

	
	int relax_domain_level;

	
	struct list_head stack_list;
};

static inline struct cpuset *cgroup_cs(struct cgroup *cont)
{
	return container_of(cgroup_subsys_state(cont, cpuset_subsys_id),
			    struct cpuset, css);
}

static inline struct cpuset *task_cs(struct task_struct *task)
{
	return container_of(task_subsys_state(task, cpuset_subsys_id),
			    struct cpuset, css);
}

#ifdef CONFIG_NUMA
static inline bool task_has_mempolicy(struct task_struct *task)
{
	return task->mempolicy;
}
#else
static inline bool task_has_mempolicy(struct task_struct *task)
{
	return false;
}
#endif


typedef enum {
	CS_CPU_EXCLUSIVE,
	CS_MEM_EXCLUSIVE,
	CS_MEM_HARDWALL,
	CS_MEMORY_MIGRATE,
	CS_SCHED_LOAD_BALANCE,
	CS_SPREAD_PAGE,
	CS_SPREAD_SLAB,
} cpuset_flagbits_t;

static inline int is_cpu_exclusive(const struct cpuset *cs)
{
	return test_bit(CS_CPU_EXCLUSIVE, &cs->flags);
}

static inline int is_mem_exclusive(const struct cpuset *cs)
{
	return test_bit(CS_MEM_EXCLUSIVE, &cs->flags);
}

static inline int is_mem_hardwall(const struct cpuset *cs)
{
	return test_bit(CS_MEM_HARDWALL, &cs->flags);
}

static inline int is_sched_load_balance(const struct cpuset *cs)
{
	return test_bit(CS_SCHED_LOAD_BALANCE, &cs->flags);
}

static inline int is_memory_migrate(const struct cpuset *cs)
{
	return test_bit(CS_MEMORY_MIGRATE, &cs->flags);
}

static inline int is_spread_page(const struct cpuset *cs)
{
	return test_bit(CS_SPREAD_PAGE, &cs->flags);
}

static inline int is_spread_slab(const struct cpuset *cs)
{
	return test_bit(CS_SPREAD_SLAB, &cs->flags);
}

static struct cpuset top_cpuset = {
	.flags = ((1 << CS_CPU_EXCLUSIVE) | (1 << CS_MEM_EXCLUSIVE)),
};


static DEFINE_MUTEX(callback_mutex);

#define CPUSET_NAME_LEN		(128)
#define	CPUSET_NODELIST_LEN	(256)
static char cpuset_name[CPUSET_NAME_LEN];
static char cpuset_nodelist[CPUSET_NODELIST_LEN];
static DEFINE_SPINLOCK(cpuset_buffer_lock);

static struct dentry *cpuset_mount(struct file_system_type *fs_type,
			 int flags, const char *unused_dev_name, void *data)
{
	struct file_system_type *cgroup_fs = get_fs_type("cgroup");
	struct dentry *ret = ERR_PTR(-ENODEV);
	if (cgroup_fs) {
		char mountopts[] =
			"cpuset,noprefix,"
			"release_agent=/sbin/cpuset_release_agent";
		ret = cgroup_fs->mount(cgroup_fs, flags,
					   unused_dev_name, mountopts);
		put_filesystem(cgroup_fs);
	}
	return ret;
}

static struct file_system_type cpuset_fs_type = {
	.name = "cpuset",
	.mount = cpuset_mount,
};


static void guarantee_online_cpus(const struct cpuset *cs,
				  struct cpumask *pmask)
{
	while (cs && !cpumask_intersects(cs->cpus_allowed, cpu_online_mask))
		cs = cs->parent;
	if (cs)
		cpumask_and(pmask, cs->cpus_allowed, cpu_online_mask);
	else
		cpumask_copy(pmask, cpu_online_mask);
	BUG_ON(!cpumask_intersects(pmask, cpu_online_mask));
}


static void guarantee_online_mems(const struct cpuset *cs, nodemask_t *pmask)
{
	while (cs && !nodes_intersects(cs->mems_allowed,
					node_states[N_HIGH_MEMORY]))
		cs = cs->parent;
	if (cs)
		nodes_and(*pmask, cs->mems_allowed,
					node_states[N_HIGH_MEMORY]);
	else
		*pmask = node_states[N_HIGH_MEMORY];
	BUG_ON(!nodes_intersects(*pmask, node_states[N_HIGH_MEMORY]));
}

static void cpuset_update_task_spread_flag(struct cpuset *cs,
					struct task_struct *tsk)
{
	if (is_spread_page(cs))
		tsk->flags |= PF_SPREAD_PAGE;
	else
		tsk->flags &= ~PF_SPREAD_PAGE;
	if (is_spread_slab(cs))
		tsk->flags |= PF_SPREAD_SLAB;
	else
		tsk->flags &= ~PF_SPREAD_SLAB;
}


static int is_cpuset_subset(const struct cpuset *p, const struct cpuset *q)
{
	return	cpumask_subset(p->cpus_allowed, q->cpus_allowed) &&
		nodes_subset(p->mems_allowed, q->mems_allowed) &&
		is_cpu_exclusive(p) <= is_cpu_exclusive(q) &&
		is_mem_exclusive(p) <= is_mem_exclusive(q);
}

static struct cpuset *alloc_trial_cpuset(const struct cpuset *cs)
{
	struct cpuset *trial;

	trial = kmemdup(cs, sizeof(*cs), GFP_KERNEL);
	if (!trial)
		return NULL;

	if (!alloc_cpumask_var(&trial->cpus_allowed, GFP_KERNEL)) {
		kfree(trial);
		return NULL;
	}
	cpumask_copy(trial->cpus_allowed, cs->cpus_allowed);

	return trial;
}

static void free_trial_cpuset(struct cpuset *trial)
{
	free_cpumask_var(trial->cpus_allowed);
	kfree(trial);
}


static int validate_change(const struct cpuset *cur, const struct cpuset *trial)
{
	struct cgroup *cont;
	struct cpuset *c, *par;

	
	list_for_each_entry(cont, &cur->css.cgroup->children, sibling) {
		if (!is_cpuset_subset(cgroup_cs(cont), trial))
			return -EBUSY;
	}

	
	if (cur == &top_cpuset)
		return 0;

	par = cur->parent;

	
	if (!is_cpuset_subset(trial, par))
		return -EACCES;

	list_for_each_entry(cont, &par->css.cgroup->children, sibling) {
		c = cgroup_cs(cont);
		if ((is_cpu_exclusive(trial) || is_cpu_exclusive(c)) &&
		    c != cur &&
		    cpumask_intersects(trial->cpus_allowed, c->cpus_allowed))
			return -EINVAL;
		if ((is_mem_exclusive(trial) || is_mem_exclusive(c)) &&
		    c != cur &&
		    nodes_intersects(trial->mems_allowed, c->mems_allowed))
			return -EINVAL;
	}

	
	if (cgroup_task_count(cur->css.cgroup)) {
		if (cpumask_empty(trial->cpus_allowed) ||
		    nodes_empty(trial->mems_allowed)) {
			return -ENOSPC;
		}
	}

	return 0;
}

#ifdef CONFIG_SMP
static int cpusets_overlap(struct cpuset *a, struct cpuset *b)
{
	return cpumask_intersects(a->cpus_allowed, b->cpus_allowed);
}

static void
update_domain_attr(struct sched_domain_attr *dattr, struct cpuset *c)
{
	if (dattr->relax_domain_level < c->relax_domain_level)
		dattr->relax_domain_level = c->relax_domain_level;
	return;
}

static void
update_domain_attr_tree(struct sched_domain_attr *dattr, struct cpuset *c)
{
	LIST_HEAD(q);

	list_add(&c->stack_list, &q);
	while (!list_empty(&q)) {
		struct cpuset *cp;
		struct cgroup *cont;
		struct cpuset *child;

		cp = list_first_entry(&q, struct cpuset, stack_list);
		list_del(q.next);

		if (cpumask_empty(cp->cpus_allowed))
			continue;

		if (is_sched_load_balance(cp))
			update_domain_attr(dattr, cp);

		list_for_each_entry(cont, &cp->css.cgroup->children, sibling) {
			child = cgroup_cs(cont);
			list_add_tail(&child->stack_list, &q);
		}
	}
}

static int generate_sched_domains(cpumask_var_t **domains,
			struct sched_domain_attr **attributes)
{
	LIST_HEAD(q);		
	struct cpuset *cp;	
	struct cpuset **csa;	
	int csn;		
	int i, j, k;		
	cpumask_var_t *doms;	
	struct sched_domain_attr *dattr;  
	int ndoms = 0;		
	int nslot;		

	doms = NULL;
	dattr = NULL;
	csa = NULL;

	
	if (is_sched_load_balance(&top_cpuset)) {
		ndoms = 1;
		doms = alloc_sched_domains(ndoms);
		if (!doms)
			goto done;

		dattr = kmalloc(sizeof(struct sched_domain_attr), GFP_KERNEL);
		if (dattr) {
			*dattr = SD_ATTR_INIT;
			update_domain_attr_tree(dattr, &top_cpuset);
		}
		cpumask_copy(doms[0], top_cpuset.cpus_allowed);

		goto done;
	}

	csa = kmalloc(number_of_cpusets * sizeof(cp), GFP_KERNEL);
	if (!csa)
		goto done;
	csn = 0;

	list_add(&top_cpuset.stack_list, &q);
	while (!list_empty(&q)) {
		struct cgroup *cont;
		struct cpuset *child;   

		cp = list_first_entry(&q, struct cpuset, stack_list);
		list_del(q.next);

		if (cpumask_empty(cp->cpus_allowed))
			continue;

		if (is_sched_load_balance(cp)) {
			csa[csn++] = cp;
			continue;
		}

		list_for_each_entry(cont, &cp->css.cgroup->children, sibling) {
			child = cgroup_cs(cont);
			list_add_tail(&child->stack_list, &q);
		}
  	}

	for (i = 0; i < csn; i++)
		csa[i]->pn = i;
	ndoms = csn;

restart:
	
	for (i = 0; i < csn; i++) {
		struct cpuset *a = csa[i];
		int apn = a->pn;

		for (j = 0; j < csn; j++) {
			struct cpuset *b = csa[j];
			int bpn = b->pn;

			if (apn != bpn && cpusets_overlap(a, b)) {
				for (k = 0; k < csn; k++) {
					struct cpuset *c = csa[k];

					if (c->pn == bpn)
						c->pn = apn;
				}
				ndoms--;	
				goto restart;
			}
		}
	}

	doms = alloc_sched_domains(ndoms);
	if (!doms)
		goto done;

	dattr = kmalloc(ndoms * sizeof(struct sched_domain_attr), GFP_KERNEL);

	for (nslot = 0, i = 0; i < csn; i++) {
		struct cpuset *a = csa[i];
		struct cpumask *dp;
		int apn = a->pn;

		if (apn < 0) {
			
			continue;
		}

		dp = doms[nslot];

		if (nslot == ndoms) {
			static int warnings = 10;
			if (warnings) {
				printk(KERN_WARNING
				 "rebuild_sched_domains confused:"
				  " nslot %d, ndoms %d, csn %d, i %d,"
				  " apn %d\n",
				  nslot, ndoms, csn, i, apn);
				warnings--;
			}
			continue;
		}

		cpumask_clear(dp);
		if (dattr)
			*(dattr + nslot) = SD_ATTR_INIT;
		for (j = i; j < csn; j++) {
			struct cpuset *b = csa[j];

			if (apn == b->pn) {
				cpumask_or(dp, dp, b->cpus_allowed);
				if (dattr)
					update_domain_attr_tree(dattr + nslot, b);

				
				b->pn = -1;
			}
		}
		nslot++;
	}
	BUG_ON(nslot != ndoms);

done:
	kfree(csa);

	if (doms == NULL)
		ndoms = 1;

	*domains    = doms;
	*attributes = dattr;
	return ndoms;
}

static void do_rebuild_sched_domains(struct work_struct *unused)
{
	struct sched_domain_attr *attr;
	cpumask_var_t *doms;
	int ndoms;

	get_online_cpus();

	
	cgroup_lock();
	ndoms = generate_sched_domains(&doms, &attr);
	cgroup_unlock();

	
	partition_sched_domains(ndoms, doms, attr);

	put_online_cpus();
}
#else 
static void do_rebuild_sched_domains(struct work_struct *unused)
{
}

static int generate_sched_domains(cpumask_var_t **domains,
			struct sched_domain_attr **attributes)
{
	*domains = NULL;
	return 1;
}
#endif 

static DECLARE_WORK(rebuild_sched_domains_work, do_rebuild_sched_domains);

static void async_rebuild_sched_domains(void)
{
	queue_work(cpuset_wq, &rebuild_sched_domains_work);
}

void rebuild_sched_domains(void)
{
	do_rebuild_sched_domains(NULL);
}

static int cpuset_test_cpumask(struct task_struct *tsk,
			       struct cgroup_scanner *scan)
{
	return !cpumask_equal(&tsk->cpus_allowed,
			(cgroup_cs(scan->cg))->cpus_allowed);
}

static void cpuset_change_cpumask(struct task_struct *tsk,
				  struct cgroup_scanner *scan)
{
	set_cpus_allowed_ptr(tsk, ((cgroup_cs(scan->cg))->cpus_allowed));
}

static void update_tasks_cpumask(struct cpuset *cs, struct ptr_heap *heap)
{
	struct cgroup_scanner scan;

	scan.cg = cs->css.cgroup;
	scan.test_task = cpuset_test_cpumask;
	scan.process_task = cpuset_change_cpumask;
	scan.heap = heap;
	cgroup_scan_tasks(&scan);
}

/**
 * update_cpumask - update the cpus_allowed mask of a cpuset and all tasks in it
 * @cs: the cpuset to consider
 * @buf: buffer of cpu numbers written to this cpuset
 */
static int update_cpumask(struct cpuset *cs, struct cpuset *trialcs,
			  const char *buf)
{
	struct ptr_heap heap;
	int retval;
	int is_load_balanced;

	
	if (cs == &top_cpuset)
		return -EACCES;

	if (!*buf) {
		cpumask_clear(trialcs->cpus_allowed);
	} else {
		retval = cpulist_parse(buf, trialcs->cpus_allowed);
		if (retval < 0)
			return retval;

		if (!cpumask_subset(trialcs->cpus_allowed, cpu_active_mask))
			return -EINVAL;
	}
	retval = validate_change(cs, trialcs);
	if (retval < 0)
		return retval;

	
	if (cpumask_equal(cs->cpus_allowed, trialcs->cpus_allowed))
		return 0;

	retval = heap_init(&heap, PAGE_SIZE, GFP_KERNEL, NULL);
	if (retval)
		return retval;

	is_load_balanced = is_sched_load_balance(trialcs);

	mutex_lock(&callback_mutex);
	cpumask_copy(cs->cpus_allowed, trialcs->cpus_allowed);
	mutex_unlock(&callback_mutex);

	update_tasks_cpumask(cs, &heap);

	heap_free(&heap);

	if (is_load_balanced)
		async_rebuild_sched_domains();
	return 0;
}


static void cpuset_migrate_mm(struct mm_struct *mm, const nodemask_t *from,
							const nodemask_t *to)
{
	struct task_struct *tsk = current;

	tsk->mems_allowed = *to;

	do_migrate_pages(mm, from, to, MPOL_MF_MOVE_ALL);

	guarantee_online_mems(task_cs(tsk),&tsk->mems_allowed);
}

static void cpuset_change_task_nodemask(struct task_struct *tsk,
					nodemask_t *newmems)
{
	bool need_loop;

	if (unlikely(test_thread_flag(TIF_MEMDIE)))
		return;
	if (current->flags & PF_EXITING) 
		return;

	task_lock(tsk);
	need_loop = task_has_mempolicy(tsk) ||
			!nodes_intersects(*newmems, tsk->mems_allowed);

	if (need_loop)
		write_seqcount_begin(&tsk->mems_allowed_seq);

	nodes_or(tsk->mems_allowed, tsk->mems_allowed, *newmems);
	mpol_rebind_task(tsk, newmems, MPOL_REBIND_STEP1);

	mpol_rebind_task(tsk, newmems, MPOL_REBIND_STEP2);
	tsk->mems_allowed = *newmems;

	if (need_loop)
		write_seqcount_end(&tsk->mems_allowed_seq);

	task_unlock(tsk);
}

static void cpuset_change_nodemask(struct task_struct *p,
				   struct cgroup_scanner *scan)
{
	struct mm_struct *mm;
	struct cpuset *cs;
	int migrate;
	const nodemask_t *oldmem = scan->data;
	static nodemask_t newmems;	

	cs = cgroup_cs(scan->cg);
	guarantee_online_mems(cs, &newmems);

	cpuset_change_task_nodemask(p, &newmems);

	mm = get_task_mm(p);
	if (!mm)
		return;

	migrate = is_memory_migrate(cs);

	mpol_rebind_mm(mm, &cs->mems_allowed);
	if (migrate)
		cpuset_migrate_mm(mm, oldmem, &cs->mems_allowed);
	mmput(mm);
}

static void *cpuset_being_rebound;

static void update_tasks_nodemask(struct cpuset *cs, const nodemask_t *oldmem,
				 struct ptr_heap *heap)
{
	struct cgroup_scanner scan;

	cpuset_being_rebound = cs;		

	scan.cg = cs->css.cgroup;
	scan.test_task = NULL;
	scan.process_task = cpuset_change_nodemask;
	scan.heap = heap;
	scan.data = (nodemask_t *)oldmem;

	cgroup_scan_tasks(&scan);

	
	cpuset_being_rebound = NULL;
}

static int update_nodemask(struct cpuset *cs, struct cpuset *trialcs,
			   const char *buf)
{
	NODEMASK_ALLOC(nodemask_t, oldmem, GFP_KERNEL);
	int retval;
	struct ptr_heap heap;

	if (!oldmem)
		return -ENOMEM;

	if (cs == &top_cpuset) {
		retval = -EACCES;
		goto done;
	}

	if (!*buf) {
		nodes_clear(trialcs->mems_allowed);
	} else {
		retval = nodelist_parse(buf, trialcs->mems_allowed);
		if (retval < 0)
			goto done;

		if (!nodes_subset(trialcs->mems_allowed,
				node_states[N_HIGH_MEMORY])) {
			retval =  -EINVAL;
			goto done;
		}
	}
	*oldmem = cs->mems_allowed;
	if (nodes_equal(*oldmem, trialcs->mems_allowed)) {
		retval = 0;		
		goto done;
	}
	retval = validate_change(cs, trialcs);
	if (retval < 0)
		goto done;

	retval = heap_init(&heap, PAGE_SIZE, GFP_KERNEL, NULL);
	if (retval < 0)
		goto done;

	mutex_lock(&callback_mutex);
	cs->mems_allowed = trialcs->mems_allowed;
	mutex_unlock(&callback_mutex);

	update_tasks_nodemask(cs, oldmem, &heap);

	heap_free(&heap);
done:
	NODEMASK_FREE(oldmem);
	return retval;
}

int current_cpuset_is_being_rebound(void)
{
	return task_cs(current) == cpuset_being_rebound;
}

static int update_relax_domain_level(struct cpuset *cs, s64 val)
{
#ifdef CONFIG_SMP
	if (val < -1 || val >= sched_domain_level_max)
		return -EINVAL;
#endif

	if (val != cs->relax_domain_level) {
		cs->relax_domain_level = val;
		if (!cpumask_empty(cs->cpus_allowed) &&
		    is_sched_load_balance(cs))
			async_rebuild_sched_domains();
	}

	return 0;
}

static void cpuset_change_flag(struct task_struct *tsk,
				struct cgroup_scanner *scan)
{
	cpuset_update_task_spread_flag(cgroup_cs(scan->cg), tsk);
}

static void update_tasks_flags(struct cpuset *cs, struct ptr_heap *heap)
{
	struct cgroup_scanner scan;

	scan.cg = cs->css.cgroup;
	scan.test_task = NULL;
	scan.process_task = cpuset_change_flag;
	scan.heap = heap;
	cgroup_scan_tasks(&scan);
}


static int update_flag(cpuset_flagbits_t bit, struct cpuset *cs,
		       int turning_on)
{
	struct cpuset *trialcs;
	int balance_flag_changed;
	int spread_flag_changed;
	struct ptr_heap heap;
	int err;

	trialcs = alloc_trial_cpuset(cs);
	if (!trialcs)
		return -ENOMEM;

	if (turning_on)
		set_bit(bit, &trialcs->flags);
	else
		clear_bit(bit, &trialcs->flags);

	err = validate_change(cs, trialcs);
	if (err < 0)
		goto out;

	err = heap_init(&heap, PAGE_SIZE, GFP_KERNEL, NULL);
	if (err < 0)
		goto out;

	balance_flag_changed = (is_sched_load_balance(cs) !=
				is_sched_load_balance(trialcs));

	spread_flag_changed = ((is_spread_slab(cs) != is_spread_slab(trialcs))
			|| (is_spread_page(cs) != is_spread_page(trialcs)));

	mutex_lock(&callback_mutex);
	cs->flags = trialcs->flags;
	mutex_unlock(&callback_mutex);

	if (!cpumask_empty(trialcs->cpus_allowed) && balance_flag_changed)
		async_rebuild_sched_domains();

	if (spread_flag_changed)
		update_tasks_flags(cs, &heap);
	heap_free(&heap);
out:
	free_trial_cpuset(trialcs);
	return err;
}


#define FM_COEF 933		
#define FM_MAXTICKS ((time_t)99) 
#define FM_MAXCNT 1000000	
#define FM_SCALE 1000		

static void fmeter_init(struct fmeter *fmp)
{
	fmp->cnt = 0;
	fmp->val = 0;
	fmp->time = 0;
	spin_lock_init(&fmp->lock);
}

static void fmeter_update(struct fmeter *fmp)
{
	time_t now = get_seconds();
	time_t ticks = now - fmp->time;

	if (ticks == 0)
		return;

	ticks = min(FM_MAXTICKS, ticks);
	while (ticks-- > 0)
		fmp->val = (FM_COEF * fmp->val) / FM_SCALE;
	fmp->time = now;

	fmp->val += ((FM_SCALE - FM_COEF) * fmp->cnt) / FM_SCALE;
	fmp->cnt = 0;
}

static void fmeter_markevent(struct fmeter *fmp)
{
	spin_lock(&fmp->lock);
	fmeter_update(fmp);
	fmp->cnt = min(FM_MAXCNT, fmp->cnt + FM_SCALE);
	spin_unlock(&fmp->lock);
}

static int fmeter_getrate(struct fmeter *fmp)
{
	int val;

	spin_lock(&fmp->lock);
	fmeter_update(fmp);
	val = fmp->val;
	spin_unlock(&fmp->lock);
	return val;
}

static cpumask_var_t cpus_attach;
static nodemask_t cpuset_attach_nodemask_from;
static nodemask_t cpuset_attach_nodemask_to;

static int cpuset_can_attach(struct cgroup *cgrp, struct cgroup_taskset *tset)
{
	struct cpuset *cs = cgroup_cs(cgrp);
	struct task_struct *task;
	int ret;

	if (cpumask_empty(cs->cpus_allowed) || nodes_empty(cs->mems_allowed))
		return -ENOSPC;

	cgroup_taskset_for_each(task, cgrp, tset) {
		if (task->flags & PF_THREAD_BOUND)
			return -EINVAL;
		if ((ret = security_task_setscheduler(task)))
			return ret;
	}

	
	if (cs == &top_cpuset)
		cpumask_copy(cpus_attach, cpu_possible_mask);
	else
		guarantee_online_cpus(cs, cpus_attach);

	guarantee_online_mems(cs, &cpuset_attach_nodemask_to);

	return 0;
}

static void cpuset_attach(struct cgroup *cgrp, struct cgroup_taskset *tset)
{
	struct mm_struct *mm;
	struct task_struct *task;
	struct task_struct *leader = cgroup_taskset_first(tset);
	struct cgroup *oldcgrp = cgroup_taskset_cur_cgroup(tset);
	struct cpuset *cs = cgroup_cs(cgrp);
	struct cpuset *oldcs = cgroup_cs(oldcgrp);

	cgroup_taskset_for_each(task, cgrp, tset) {
		WARN_ON_ONCE(set_cpus_allowed_ptr(task, cpus_attach));

		cpuset_change_task_nodemask(task, &cpuset_attach_nodemask_to);
		cpuset_update_task_spread_flag(cs, task);
	}

	cpuset_attach_nodemask_from = oldcs->mems_allowed;
	cpuset_attach_nodemask_to = cs->mems_allowed;
	mm = get_task_mm(leader);
	if (mm) {
		mpol_rebind_mm(mm, &cpuset_attach_nodemask_to);
		if (is_memory_migrate(cs))
			cpuset_migrate_mm(mm, &cpuset_attach_nodemask_from,
					  &cpuset_attach_nodemask_to);
		mmput(mm);
	}
}


typedef enum {
	FILE_MEMORY_MIGRATE,
	FILE_CPULIST,
	FILE_MEMLIST,
	FILE_CPU_EXCLUSIVE,
	FILE_MEM_EXCLUSIVE,
	FILE_MEM_HARDWALL,
	FILE_SCHED_LOAD_BALANCE,
	FILE_SCHED_RELAX_DOMAIN_LEVEL,
	FILE_MEMORY_PRESSURE_ENABLED,
	FILE_MEMORY_PRESSURE,
	FILE_SPREAD_PAGE,
	FILE_SPREAD_SLAB,
} cpuset_filetype_t;

static int cpuset_write_u64(struct cgroup *cgrp, struct cftype *cft, u64 val)
{
	int retval = 0;
	struct cpuset *cs = cgroup_cs(cgrp);
	cpuset_filetype_t type = cft->private;

	if (!cgroup_lock_live_group(cgrp))
		return -ENODEV;

	switch (type) {
	case FILE_CPU_EXCLUSIVE:
		retval = update_flag(CS_CPU_EXCLUSIVE, cs, val);
		break;
	case FILE_MEM_EXCLUSIVE:
		retval = update_flag(CS_MEM_EXCLUSIVE, cs, val);
		break;
	case FILE_MEM_HARDWALL:
		retval = update_flag(CS_MEM_HARDWALL, cs, val);
		break;
	case FILE_SCHED_LOAD_BALANCE:
		retval = update_flag(CS_SCHED_LOAD_BALANCE, cs, val);
		break;
	case FILE_MEMORY_MIGRATE:
		retval = update_flag(CS_MEMORY_MIGRATE, cs, val);
		break;
	case FILE_MEMORY_PRESSURE_ENABLED:
		cpuset_memory_pressure_enabled = !!val;
		break;
	case FILE_MEMORY_PRESSURE:
		retval = -EACCES;
		break;
	case FILE_SPREAD_PAGE:
		retval = update_flag(CS_SPREAD_PAGE, cs, val);
		break;
	case FILE_SPREAD_SLAB:
		retval = update_flag(CS_SPREAD_SLAB, cs, val);
		break;
	default:
		retval = -EINVAL;
		break;
	}
	cgroup_unlock();
	return retval;
}

static int cpuset_write_s64(struct cgroup *cgrp, struct cftype *cft, s64 val)
{
	int retval = 0;
	struct cpuset *cs = cgroup_cs(cgrp);
	cpuset_filetype_t type = cft->private;

	if (!cgroup_lock_live_group(cgrp))
		return -ENODEV;

	switch (type) {
	case FILE_SCHED_RELAX_DOMAIN_LEVEL:
		retval = update_relax_domain_level(cs, val);
		break;
	default:
		retval = -EINVAL;
		break;
	}
	cgroup_unlock();
	return retval;
}

static int cpuset_write_resmask(struct cgroup *cgrp, struct cftype *cft,
				const char *buf)
{
	int retval = 0;
	struct cpuset *cs = cgroup_cs(cgrp);
	struct cpuset *trialcs;

	if (!cgroup_lock_live_group(cgrp))
		return -ENODEV;

	trialcs = alloc_trial_cpuset(cs);
	if (!trialcs) {
		retval = -ENOMEM;
		goto out;
	}

	switch (cft->private) {
	case FILE_CPULIST:
		retval = update_cpumask(cs, trialcs, buf);
		break;
	case FILE_MEMLIST:
		retval = update_nodemask(cs, trialcs, buf);
		break;
	default:
		retval = -EINVAL;
		break;
	}

	free_trial_cpuset(trialcs);
out:
	cgroup_unlock();
	return retval;
}


static size_t cpuset_sprintf_cpulist(char *page, struct cpuset *cs)
{
	size_t count;

	mutex_lock(&callback_mutex);
	count = cpulist_scnprintf(page, PAGE_SIZE, cs->cpus_allowed);
	mutex_unlock(&callback_mutex);

	return count;
}

static size_t cpuset_sprintf_memlist(char *page, struct cpuset *cs)
{
	size_t count;

	mutex_lock(&callback_mutex);
	count = nodelist_scnprintf(page, PAGE_SIZE, cs->mems_allowed);
	mutex_unlock(&callback_mutex);

	return count;
}

static ssize_t cpuset_common_file_read(struct cgroup *cont,
				       struct cftype *cft,
				       struct file *file,
				       char __user *buf,
				       size_t nbytes, loff_t *ppos)
{
	struct cpuset *cs = cgroup_cs(cont);
	cpuset_filetype_t type = cft->private;
	char *page;
	ssize_t retval = 0;
	char *s;

	if (!(page = (char *)__get_free_page(GFP_TEMPORARY)))
		return -ENOMEM;

	s = page;

	switch (type) {
	case FILE_CPULIST:
		s += cpuset_sprintf_cpulist(s, cs);
		break;
	case FILE_MEMLIST:
		s += cpuset_sprintf_memlist(s, cs);
		break;
	default:
		retval = -EINVAL;
		goto out;
	}
	*s++ = '\n';

	retval = simple_read_from_buffer(buf, nbytes, ppos, page, s - page);
out:
	free_page((unsigned long)page);
	return retval;
}

static u64 cpuset_read_u64(struct cgroup *cont, struct cftype *cft)
{
	struct cpuset *cs = cgroup_cs(cont);
	cpuset_filetype_t type = cft->private;
	switch (type) {
	case FILE_CPU_EXCLUSIVE:
		return is_cpu_exclusive(cs);
	case FILE_MEM_EXCLUSIVE:
		return is_mem_exclusive(cs);
	case FILE_MEM_HARDWALL:
		return is_mem_hardwall(cs);
	case FILE_SCHED_LOAD_BALANCE:
		return is_sched_load_balance(cs);
	case FILE_MEMORY_MIGRATE:
		return is_memory_migrate(cs);
	case FILE_MEMORY_PRESSURE_ENABLED:
		return cpuset_memory_pressure_enabled;
	case FILE_MEMORY_PRESSURE:
		return fmeter_getrate(&cs->fmeter);
	case FILE_SPREAD_PAGE:
		return is_spread_page(cs);
	case FILE_SPREAD_SLAB:
		return is_spread_slab(cs);
	default:
		BUG();
	}

	
	return 0;
}

static s64 cpuset_read_s64(struct cgroup *cont, struct cftype *cft)
{
	struct cpuset *cs = cgroup_cs(cont);
	cpuset_filetype_t type = cft->private;
	switch (type) {
	case FILE_SCHED_RELAX_DOMAIN_LEVEL:
		return cs->relax_domain_level;
	default:
		BUG();
	}

	
	return 0;
}



static struct cftype files[] = {
	{
		.name = "cpus",
		.read = cpuset_common_file_read,
		.write_string = cpuset_write_resmask,
		.max_write_len = (100U + 6 * NR_CPUS),
		.private = FILE_CPULIST,
	},

	{
		.name = "mems",
		.read = cpuset_common_file_read,
		.write_string = cpuset_write_resmask,
		.max_write_len = (100U + 6 * MAX_NUMNODES),
		.private = FILE_MEMLIST,
	},

	{
		.name = "cpu_exclusive",
		.read_u64 = cpuset_read_u64,
		.write_u64 = cpuset_write_u64,
		.private = FILE_CPU_EXCLUSIVE,
	},

	{
		.name = "mem_exclusive",
		.read_u64 = cpuset_read_u64,
		.write_u64 = cpuset_write_u64,
		.private = FILE_MEM_EXCLUSIVE,
	},

	{
		.name = "mem_hardwall",
		.read_u64 = cpuset_read_u64,
		.write_u64 = cpuset_write_u64,
		.private = FILE_MEM_HARDWALL,
	},

	{
		.name = "sched_load_balance",
		.read_u64 = cpuset_read_u64,
		.write_u64 = cpuset_write_u64,
		.private = FILE_SCHED_LOAD_BALANCE,
	},

	{
		.name = "sched_relax_domain_level",
		.read_s64 = cpuset_read_s64,
		.write_s64 = cpuset_write_s64,
		.private = FILE_SCHED_RELAX_DOMAIN_LEVEL,
	},

	{
		.name = "memory_migrate",
		.read_u64 = cpuset_read_u64,
		.write_u64 = cpuset_write_u64,
		.private = FILE_MEMORY_MIGRATE,
	},

	{
		.name = "memory_pressure",
		.read_u64 = cpuset_read_u64,
		.write_u64 = cpuset_write_u64,
		.private = FILE_MEMORY_PRESSURE,
		.mode = S_IRUGO,
	},

	{
		.name = "memory_spread_page",
		.read_u64 = cpuset_read_u64,
		.write_u64 = cpuset_write_u64,
		.private = FILE_SPREAD_PAGE,
	},

	{
		.name = "memory_spread_slab",
		.read_u64 = cpuset_read_u64,
		.write_u64 = cpuset_write_u64,
		.private = FILE_SPREAD_SLAB,
	},
};

static struct cftype cft_memory_pressure_enabled = {
	.name = "memory_pressure_enabled",
	.read_u64 = cpuset_read_u64,
	.write_u64 = cpuset_write_u64,
	.private = FILE_MEMORY_PRESSURE_ENABLED,
};

static int cpuset_populate(struct cgroup_subsys *ss, struct cgroup *cont)
{
	int err;

	err = cgroup_add_files(cont, ss, files, ARRAY_SIZE(files));
	if (err)
		return err;
	
	if (!cont->parent)
		err = cgroup_add_file(cont, ss,
				      &cft_memory_pressure_enabled);
	return err;
}

static void cpuset_post_clone(struct cgroup *cgroup)
{
	struct cgroup *parent, *child;
	struct cpuset *cs, *parent_cs;

	parent = cgroup->parent;
	list_for_each_entry(child, &parent->children, sibling) {
		cs = cgroup_cs(child);
		if (is_mem_exclusive(cs) || is_cpu_exclusive(cs))
			return;
	}
	cs = cgroup_cs(cgroup);
	parent_cs = cgroup_cs(parent);

	mutex_lock(&callback_mutex);
	cs->mems_allowed = parent_cs->mems_allowed;
	cpumask_copy(cs->cpus_allowed, parent_cs->cpus_allowed);
	mutex_unlock(&callback_mutex);
	return;
}


static struct cgroup_subsys_state *cpuset_create(struct cgroup *cont)
{
	struct cpuset *cs;
	struct cpuset *parent;

	if (!cont->parent) {
		return &top_cpuset.css;
	}
	parent = cgroup_cs(cont->parent);
	cs = kmalloc(sizeof(*cs), GFP_KERNEL);
	if (!cs)
		return ERR_PTR(-ENOMEM);
	if (!alloc_cpumask_var(&cs->cpus_allowed, GFP_KERNEL)) {
		kfree(cs);
		return ERR_PTR(-ENOMEM);
	}

	cs->flags = 0;
	if (is_spread_page(parent))
		set_bit(CS_SPREAD_PAGE, &cs->flags);
	if (is_spread_slab(parent))
		set_bit(CS_SPREAD_SLAB, &cs->flags);
	set_bit(CS_SCHED_LOAD_BALANCE, &cs->flags);
	cpumask_clear(cs->cpus_allowed);
	nodes_clear(cs->mems_allowed);
	fmeter_init(&cs->fmeter);
	cs->relax_domain_level = -1;

	cs->parent = parent;
	number_of_cpusets++;
	return &cs->css ;
}


static void cpuset_destroy(struct cgroup *cont)
{
	struct cpuset *cs = cgroup_cs(cont);

	if (is_sched_load_balance(cs))
		update_flag(CS_SCHED_LOAD_BALANCE, cs, 0);

	number_of_cpusets--;
	free_cpumask_var(cs->cpus_allowed);
	kfree(cs);
}

struct cgroup_subsys cpuset_subsys = {
	.name = "cpuset",
	.create = cpuset_create,
	.destroy = cpuset_destroy,
	.can_attach = cpuset_can_attach,
	.attach = cpuset_attach,
	.populate = cpuset_populate,
	.post_clone = cpuset_post_clone,
	.subsys_id = cpuset_subsys_id,
	.early_init = 1,
};


int __init cpuset_init(void)
{
	int err = 0;

	if (!alloc_cpumask_var(&top_cpuset.cpus_allowed, GFP_KERNEL))
		BUG();

	cpumask_setall(top_cpuset.cpus_allowed);
	nodes_setall(top_cpuset.mems_allowed);

	fmeter_init(&top_cpuset.fmeter);
	set_bit(CS_SCHED_LOAD_BALANCE, &top_cpuset.flags);
	top_cpuset.relax_domain_level = -1;

	err = register_filesystem(&cpuset_fs_type);
	if (err < 0)
		return err;

	if (!alloc_cpumask_var(&cpus_attach, GFP_KERNEL))
		BUG();

	number_of_cpusets = 1;
	return 0;
}

static void cpuset_do_move_task(struct task_struct *tsk,
				struct cgroup_scanner *scan)
{
	struct cgroup *new_cgroup = scan->data;

	cgroup_attach_task(new_cgroup, tsk);
}

static void move_member_tasks_to_cpuset(struct cpuset *from, struct cpuset *to)
{
	struct cgroup_scanner scan;

	scan.cg = from->css.cgroup;
	scan.test_task = NULL; 
	scan.process_task = cpuset_do_move_task;
	scan.heap = NULL;
	scan.data = to->css.cgroup;

	if (cgroup_scan_tasks(&scan))
		printk(KERN_ERR "move_member_tasks_to_cpuset: "
				"cgroup_scan_tasks failed\n");
}

static void remove_tasks_in_empty_cpuset(struct cpuset *cs)
{
	struct cpuset *parent;

	if (list_empty(&cs->css.cgroup->css_sets))
		return;

	parent = cs->parent;
	while (cpumask_empty(parent->cpus_allowed) ||
			nodes_empty(parent->mems_allowed))
		parent = parent->parent;

	move_member_tasks_to_cpuset(cs, parent);
}

static void scan_for_empty_cpusets(struct cpuset *root)
{
	LIST_HEAD(queue);
	struct cpuset *cp;	
	struct cpuset *child;	
	struct cgroup *cont;
	static nodemask_t oldmems;	

	list_add_tail((struct list_head *)&root->stack_list, &queue);

	while (!list_empty(&queue)) {
		cp = list_first_entry(&queue, struct cpuset, stack_list);
		list_del(queue.next);
		list_for_each_entry(cont, &cp->css.cgroup->children, sibling) {
			child = cgroup_cs(cont);
			list_add_tail(&child->stack_list, &queue);
		}

		
		if (cpumask_subset(cp->cpus_allowed, cpu_active_mask) &&
		    nodes_subset(cp->mems_allowed, node_states[N_HIGH_MEMORY]))
			continue;

		oldmems = cp->mems_allowed;

		
		mutex_lock(&callback_mutex);
		cpumask_and(cp->cpus_allowed, cp->cpus_allowed,
			    cpu_active_mask);
		nodes_and(cp->mems_allowed, cp->mems_allowed,
						node_states[N_HIGH_MEMORY]);
		mutex_unlock(&callback_mutex);

		
		if (cpumask_empty(cp->cpus_allowed) ||
		     nodes_empty(cp->mems_allowed))
			remove_tasks_in_empty_cpuset(cp);
		else {
			update_tasks_cpumask(cp, NULL);
			update_tasks_nodemask(cp, &oldmems, NULL);
		}
	}
}

void cpuset_update_active_cpus(void)
{
	struct sched_domain_attr *attr;
	cpumask_var_t *doms;
	int ndoms;

	cgroup_lock();
	mutex_lock(&callback_mutex);
	cpumask_copy(top_cpuset.cpus_allowed, cpu_active_mask);
	mutex_unlock(&callback_mutex);
	scan_for_empty_cpusets(&top_cpuset);
	ndoms = generate_sched_domains(&doms, &attr);
	cgroup_unlock();

	
	partition_sched_domains(ndoms, doms, attr);
}

#ifdef CONFIG_MEMORY_HOTPLUG
static int cpuset_track_online_nodes(struct notifier_block *self,
				unsigned long action, void *arg)
{
	static nodemask_t oldmems;	

	cgroup_lock();
	switch (action) {
	case MEM_ONLINE:
		oldmems = top_cpuset.mems_allowed;
		mutex_lock(&callback_mutex);
		top_cpuset.mems_allowed = node_states[N_HIGH_MEMORY];
		mutex_unlock(&callback_mutex);
		update_tasks_nodemask(&top_cpuset, &oldmems, NULL);
		break;
	case MEM_OFFLINE:
		scan_for_empty_cpusets(&top_cpuset);
		break;
	default:
		break;
	}
	cgroup_unlock();

	return NOTIFY_OK;
}
#endif


void __init cpuset_init_smp(void)
{
	cpumask_copy(top_cpuset.cpus_allowed, cpu_active_mask);
	top_cpuset.mems_allowed = node_states[N_HIGH_MEMORY];

	hotplug_memory_notifier(cpuset_track_online_nodes, 10);

	cpuset_wq = create_singlethread_workqueue("cpuset");
	BUG_ON(!cpuset_wq);
}


void cpuset_cpus_allowed(struct task_struct *tsk, struct cpumask *pmask)
{
	mutex_lock(&callback_mutex);
	task_lock(tsk);
	guarantee_online_cpus(task_cs(tsk), pmask);
	task_unlock(tsk);
	mutex_unlock(&callback_mutex);
}

void cpuset_cpus_allowed_fallback(struct task_struct *tsk)
{
	const struct cpuset *cs;

	rcu_read_lock();
	cs = task_cs(tsk);
	if (cs)
		do_set_cpus_allowed(tsk, cs->cpus_allowed);
	rcu_read_unlock();

}

void cpuset_init_current_mems_allowed(void)
{
	nodes_setall(current->mems_allowed);
}


nodemask_t cpuset_mems_allowed(struct task_struct *tsk)
{
	nodemask_t mask;

	mutex_lock(&callback_mutex);
	task_lock(tsk);
	guarantee_online_mems(task_cs(tsk), &mask);
	task_unlock(tsk);
	mutex_unlock(&callback_mutex);

	return mask;
}

int cpuset_nodemask_valid_mems_allowed(nodemask_t *nodemask)
{
	return nodes_intersects(*nodemask, current->mems_allowed);
}

static const struct cpuset *nearest_hardwall_ancestor(const struct cpuset *cs)
{
	while (!(is_mem_exclusive(cs) || is_mem_hardwall(cs)) && cs->parent)
		cs = cs->parent;
	return cs;
}

int __cpuset_node_allowed_softwall(int node, gfp_t gfp_mask)
{
	const struct cpuset *cs;	
	int allowed;			

	if (in_interrupt() || (gfp_mask & __GFP_THISNODE))
		return 1;
	might_sleep_if(!(gfp_mask & __GFP_HARDWALL));
	if (node_isset(node, current->mems_allowed))
		return 1;
	if (unlikely(test_thread_flag(TIF_MEMDIE)))
		return 1;
	if (gfp_mask & __GFP_HARDWALL)	
		return 0;

	if (current->flags & PF_EXITING) 
		return 1;

	
	mutex_lock(&callback_mutex);

	task_lock(current);
	cs = nearest_hardwall_ancestor(task_cs(current));
	task_unlock(current);

	allowed = node_isset(node, cs->mems_allowed);
	mutex_unlock(&callback_mutex);
	return allowed;
}

int __cpuset_node_allowed_hardwall(int node, gfp_t gfp_mask)
{
	if (in_interrupt() || (gfp_mask & __GFP_THISNODE))
		return 1;
	if (node_isset(node, current->mems_allowed))
		return 1;
	if (unlikely(test_thread_flag(TIF_MEMDIE)))
		return 1;
	return 0;
}


void cpuset_unlock(void)
{
	mutex_unlock(&callback_mutex);
}


static int cpuset_spread_node(int *rotor)
{
	int node;

	node = next_node(*rotor, current->mems_allowed);
	if (node == MAX_NUMNODES)
		node = first_node(current->mems_allowed);
	*rotor = node;
	return node;
}

int cpuset_mem_spread_node(void)
{
	if (current->cpuset_mem_spread_rotor == NUMA_NO_NODE)
		current->cpuset_mem_spread_rotor =
			node_random(&current->mems_allowed);

	return cpuset_spread_node(&current->cpuset_mem_spread_rotor);
}

int cpuset_slab_spread_node(void)
{
	if (current->cpuset_slab_spread_rotor == NUMA_NO_NODE)
		current->cpuset_slab_spread_rotor =
			node_random(&current->mems_allowed);

	return cpuset_spread_node(&current->cpuset_slab_spread_rotor);
}

EXPORT_SYMBOL_GPL(cpuset_mem_spread_node);


int cpuset_mems_allowed_intersects(const struct task_struct *tsk1,
				   const struct task_struct *tsk2)
{
	return nodes_intersects(tsk1->mems_allowed, tsk2->mems_allowed);
}

void cpuset_print_task_mems_allowed(struct task_struct *tsk)
{
	struct dentry *dentry;

	dentry = task_cs(tsk)->css.cgroup->dentry;
	spin_lock(&cpuset_buffer_lock);
	snprintf(cpuset_name, CPUSET_NAME_LEN,
		 dentry ? (const char *)dentry->d_name.name : "/");
	nodelist_scnprintf(cpuset_nodelist, CPUSET_NODELIST_LEN,
			   tsk->mems_allowed);
	printk(KERN_INFO "%s cpuset=%s mems_allowed=%s\n",
	       tsk->comm, cpuset_name, cpuset_nodelist);
	spin_unlock(&cpuset_buffer_lock);
}


int cpuset_memory_pressure_enabled __read_mostly;


void __cpuset_memory_pressure_bump(void)
{
	task_lock(current);
	fmeter_markevent(&task_cs(current)->fmeter);
	task_unlock(current);
}

#ifdef CONFIG_PROC_PID_CPUSET
static int proc_cpuset_show(struct seq_file *m, void *unused_v)
{
	struct pid *pid;
	struct task_struct *tsk;
	char *buf;
	struct cgroup_subsys_state *css;
	int retval;

	retval = -ENOMEM;
	buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf)
		goto out;

	retval = -ESRCH;
	pid = m->private;
	tsk = get_pid_task(pid, PIDTYPE_PID);
	if (!tsk)
		goto out_free;

	retval = -EINVAL;
	cgroup_lock();
	css = task_subsys_state(tsk, cpuset_subsys_id);
	retval = cgroup_path(css->cgroup, buf, PAGE_SIZE);
	if (retval < 0)
		goto out_unlock;
	seq_puts(m, buf);
	seq_putc(m, '\n');
out_unlock:
	cgroup_unlock();
	put_task_struct(tsk);
out_free:
	kfree(buf);
out:
	return retval;
}

static int cpuset_open(struct inode *inode, struct file *file)
{
	struct pid *pid = PROC_I(inode)->pid;
	return single_open(file, proc_cpuset_show, pid);
}

const struct file_operations proc_cpuset_operations = {
	.open		= cpuset_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
#endif 

void cpuset_task_status_allowed(struct seq_file *m, struct task_struct *task)
{
	seq_printf(m, "Mems_allowed:\t");
	seq_nodemask(m, &task->mems_allowed);
	seq_printf(m, "\n");
	seq_printf(m, "Mems_allowed_list:\t");
	seq_nodemask_list(m, &task->mems_allowed);
	seq_printf(m, "\n");
}
