/*
 * Yama Linux Security Module
 *
 * Author: Kees Cook <keescook@chromium.org>
 *
 * Copyright (C) 2010 Canonical, Ltd.
 * Copyright (C) 2011 The Chromium OS Authors.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 */

#include <linux/security.h>
#include <linux/sysctl.h>
#include <linux/ptrace.h>
#include <linux/prctl.h>
#include <linux/ratelimit.h>

static int ptrace_scope = 1;

struct ptrace_relation {
	struct task_struct *tracer;
	struct task_struct *tracee;
	struct list_head node;
};

static LIST_HEAD(ptracer_relations);
static DEFINE_SPINLOCK(ptracer_relations_lock);

static int yama_ptracer_add(struct task_struct *tracer,
			    struct task_struct *tracee)
{
	int rc = 0;
	struct ptrace_relation *added;
	struct ptrace_relation *entry, *relation = NULL;

	added = kmalloc(sizeof(*added), GFP_KERNEL);
	if (!added)
		return -ENOMEM;

	spin_lock_bh(&ptracer_relations_lock);
	list_for_each_entry(entry, &ptracer_relations, node)
		if (entry->tracee == tracee) {
			relation = entry;
			break;
		}
	if (!relation) {
		relation = added;
		relation->tracee = tracee;
		list_add(&relation->node, &ptracer_relations);
	}
	relation->tracer = tracer;

	spin_unlock_bh(&ptracer_relations_lock);
	if (added != relation)
		kfree(added);

	return rc;
}

static void yama_ptracer_del(struct task_struct *tracer,
			     struct task_struct *tracee)
{
	struct ptrace_relation *relation, *safe;

	spin_lock_bh(&ptracer_relations_lock);
	list_for_each_entry_safe(relation, safe, &ptracer_relations, node)
		if (relation->tracee == tracee ||
		    (tracer && relation->tracer == tracer)) {
			list_del(&relation->node);
			kfree(relation);
		}
	spin_unlock_bh(&ptracer_relations_lock);
}

static void yama_task_free(struct task_struct *task)
{
	yama_ptracer_del(task, task);
}

static int yama_task_prctl(int option, unsigned long arg2, unsigned long arg3,
			   unsigned long arg4, unsigned long arg5)
{
	int rc;
	struct task_struct *myself = current;

	rc = cap_task_prctl(option, arg2, arg3, arg4, arg5);
	if (rc != -ENOSYS)
		return rc;

	switch (option) {
	case PR_SET_PTRACER:
		rcu_read_lock();
		if (!thread_group_leader(myself))
			myself = rcu_dereference(myself->group_leader);
		get_task_struct(myself);
		rcu_read_unlock();

		if (arg2 == 0) {
			yama_ptracer_del(NULL, myself);
			rc = 0;
		} else if (arg2 == PR_SET_PTRACER_ANY) {
			rc = yama_ptracer_add(NULL, myself);
		} else {
			struct task_struct *tracer;

			rcu_read_lock();
			tracer = find_task_by_vpid(arg2);
			if (tracer)
				get_task_struct(tracer);
			else
				rc = -EINVAL;
			rcu_read_unlock();

			if (tracer) {
				rc = yama_ptracer_add(tracer, myself);
				put_task_struct(tracer);
			}
		}

		put_task_struct(myself);
		break;
	}

	return rc;
}

static int task_is_descendant(struct task_struct *parent,
			      struct task_struct *child)
{
	int rc = 0;
	struct task_struct *walker = child;

	if (!parent || !child)
		return 0;

	rcu_read_lock();
	if (!thread_group_leader(parent))
		parent = rcu_dereference(parent->group_leader);
	while (walker->pid > 0) {
		if (!thread_group_leader(walker))
			walker = rcu_dereference(walker->group_leader);
		if (walker == parent) {
			rc = 1;
			break;
		}
		walker = rcu_dereference(walker->real_parent);
	}
	rcu_read_unlock();

	return rc;
}

static int ptracer_exception_found(struct task_struct *tracer,
				   struct task_struct *tracee)
{
	int rc = 0;
	struct ptrace_relation *relation;
	struct task_struct *parent = NULL;
	bool found = false;

	spin_lock_bh(&ptracer_relations_lock);
	rcu_read_lock();
	if (!thread_group_leader(tracee))
		tracee = rcu_dereference(tracee->group_leader);
	list_for_each_entry(relation, &ptracer_relations, node)
		if (relation->tracee == tracee) {
			parent = relation->tracer;
			found = true;
			break;
		}

	if (found && (parent == NULL || task_is_descendant(parent, tracer)))
		rc = 1;
	rcu_read_unlock();
	spin_unlock_bh(&ptracer_relations_lock);

	return rc;
}

static int yama_ptrace_access_check(struct task_struct *child,
				    unsigned int mode)
{
	int rc;

	rc = cap_ptrace_access_check(child, mode);
	if (rc)
		return rc;

	
	if (mode == PTRACE_MODE_ATTACH &&
	    ptrace_scope &&
	    !task_is_descendant(current, child) &&
	    !ptracer_exception_found(current, child) &&
	    !capable(CAP_SYS_PTRACE))
		rc = -EPERM;

	if (rc) {
		char name[sizeof(current->comm)];
		printk_ratelimited(KERN_NOTICE "ptrace of non-child"
			" pid %d was attempted by: %s (pid %d)\n",
			child->pid,
			get_task_comm(name, current),
			current->pid);
	}

	return rc;
}

static struct security_operations yama_ops = {
	.name =			"yama",

	.ptrace_access_check =	yama_ptrace_access_check,
	.task_prctl =		yama_task_prctl,
	.task_free =		yama_task_free,
};

#ifdef CONFIG_SYSCTL
static int zero;
static int one = 1;

struct ctl_path yama_sysctl_path[] = {
	{ .procname = "kernel", },
	{ .procname = "yama", },
	{ }
};

static struct ctl_table yama_sysctl_table[] = {
	{
		.procname       = "ptrace_scope",
		.data           = &ptrace_scope,
		.maxlen         = sizeof(int),
		.mode           = 0644,
		.proc_handler   = proc_dointvec_minmax,
		.extra1         = &zero,
		.extra2         = &one,
	},
	{ }
};
#endif 

static __init int yama_init(void)
{
	if (!security_module_enable(&yama_ops))
		return 0;

	printk(KERN_INFO "Yama: becoming mindful.\n");

	if (register_security(&yama_ops))
		panic("Yama: kernel registration failed.\n");

#ifdef CONFIG_SYSCTL
	if (!register_sysctl_paths(yama_sysctl_path, yama_sysctl_table))
		panic("Yama: sysctl registration failed.\n");
#endif

	return 0;
}

security_initcall(yama_init);
