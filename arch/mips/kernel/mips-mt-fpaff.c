/*
 * General MIPS MT support routines, usable in AP/SP, SMVP, or SMTC kernels
 * Copyright (C) 2005 Mips Technologies, Inc
 */
#include <linux/cpu.h>
#include <linux/cpuset.h>
#include <linux/cpumask.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/security.h>
#include <linux/types.h>
#include <asm/uaccess.h>

cpumask_t mt_fpu_cpumask;

static int fpaff_threshold = -1;
unsigned long mt_fpemul_threshold;


static inline struct task_struct *find_process_by_pid(pid_t pid)
{
	return pid ? find_task_by_vpid(pid) : current;
}

static bool check_same_owner(struct task_struct *p)
{
	const struct cred *cred = current_cred(), *pcred;
	bool match;

	rcu_read_lock();
	pcred = __task_cred(p);
	match = (cred->euid == pcred->euid ||
		 cred->euid == pcred->uid);
	rcu_read_unlock();
	return match;
}

asmlinkage long mipsmt_sys_sched_setaffinity(pid_t pid, unsigned int len,
				      unsigned long __user *user_mask_ptr)
{
	cpumask_var_t cpus_allowed, new_mask, effective_mask;
	struct thread_info *ti;
	struct task_struct *p;
	int retval;

	if (len < sizeof(new_mask))
		return -EINVAL;

	if (copy_from_user(&new_mask, user_mask_ptr, sizeof(new_mask)))
		return -EFAULT;

	get_online_cpus();
	rcu_read_lock();

	p = find_process_by_pid(pid);
	if (!p) {
		rcu_read_unlock();
		put_online_cpus();
		return -ESRCH;
	}

	
	get_task_struct(p);
	rcu_read_unlock();

	if (!alloc_cpumask_var(&cpus_allowed, GFP_KERNEL)) {
		retval = -ENOMEM;
		goto out_put_task;
	}
	if (!alloc_cpumask_var(&new_mask, GFP_KERNEL)) {
		retval = -ENOMEM;
		goto out_free_cpus_allowed;
	}
	if (!alloc_cpumask_var(&effective_mask, GFP_KERNEL)) {
		retval = -ENOMEM;
		goto out_free_new_mask;
	}
	retval = -EPERM;
	if (!check_same_owner(p) && !capable(CAP_SYS_NICE))
		goto out_unlock;

	retval = security_task_setscheduler(p);
	if (retval)
		goto out_unlock;

	
	cpumask_copy(&p->thread.user_cpus_allowed, new_mask);

 again:
	
	ti = task_thread_info(p);
	if (test_ti_thread_flag(ti, TIF_FPUBOUND) &&
	    cpus_intersects(*new_mask, mt_fpu_cpumask)) {
		cpus_and(*effective_mask, *new_mask, mt_fpu_cpumask);
		retval = set_cpus_allowed_ptr(p, effective_mask);
	} else {
		cpumask_copy(effective_mask, new_mask);
		clear_ti_thread_flag(ti, TIF_FPUBOUND);
		retval = set_cpus_allowed_ptr(p, new_mask);
	}

	if (!retval) {
		cpuset_cpus_allowed(p, cpus_allowed);
		if (!cpumask_subset(effective_mask, cpus_allowed)) {
			cpumask_copy(new_mask, cpus_allowed);
			goto again;
		}
	}
out_unlock:
	free_cpumask_var(effective_mask);
out_free_new_mask:
	free_cpumask_var(new_mask);
out_free_cpus_allowed:
	free_cpumask_var(cpus_allowed);
out_put_task:
	put_task_struct(p);
	put_online_cpus();
	return retval;
}

asmlinkage long mipsmt_sys_sched_getaffinity(pid_t pid, unsigned int len,
				      unsigned long __user *user_mask_ptr)
{
	unsigned int real_len;
	cpumask_t mask;
	int retval;
	struct task_struct *p;

	real_len = sizeof(mask);
	if (len < real_len)
		return -EINVAL;

	get_online_cpus();
	read_lock(&tasklist_lock);

	retval = -ESRCH;
	p = find_process_by_pid(pid);
	if (!p)
		goto out_unlock;
	retval = security_task_getscheduler(p);
	if (retval)
		goto out_unlock;

	cpumask_and(&mask, &p->thread.user_cpus_allowed, cpu_possible_mask);

out_unlock:
	read_unlock(&tasklist_lock);
	put_online_cpus();
	if (retval)
		return retval;
	if (copy_to_user(user_mask_ptr, &mask, real_len))
		return -EFAULT;
	return real_len;
}


static int __init fpaff_thresh(char *str)
{
	get_option(&str, &fpaff_threshold);
	return 1;
}
__setup("fpaff=", fpaff_thresh);

#define FPUSEFACTOR 2000

static __init int mt_fp_affinity_init(void)
{
	if (fpaff_threshold >= 0) {
		mt_fpemul_threshold = fpaff_threshold;
	} else {
		mt_fpemul_threshold =
			(FPUSEFACTOR * (loops_per_jiffy/(500000/HZ))) / HZ;
	}
	printk(KERN_DEBUG "FPU Affinity set after %ld emulations\n",
	       mt_fpemul_threshold);

	return 0;
}
arch_initcall(mt_fp_affinity_init);
