/*
 * linux/ipc/namespace.c
 * Copyright (C) 2006 Pavel Emelyanov <xemul@openvz.org> OpenVZ, SWsoft Inc.
 */

#include <linux/ipc.h>
#include <linux/msg.h>
#include <linux/ipc_namespace.h>
#include <linux/rcupdate.h>
#include <linux/nsproxy.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/mount.h>
#include <linux/user_namespace.h>
#include <linux/proc_fs.h>

#include "util.h"

static struct ipc_namespace *create_ipc_ns(struct task_struct *tsk,
					   struct ipc_namespace *old_ns)
{
	struct ipc_namespace *ns;
	int err;

	ns = kmalloc(sizeof(struct ipc_namespace), GFP_KERNEL);
	if (ns == NULL)
		return ERR_PTR(-ENOMEM);

	atomic_set(&ns->count, 1);
	err = mq_init_ns(ns);
	if (err) {
		kfree(ns);
		return ERR_PTR(err);
	}
	atomic_inc(&nr_ipc_ns);

	sem_init_ns(ns);
	msg_init_ns(ns);
	shm_init_ns(ns);

	ipcns_notify(IPCNS_CREATED);
	register_ipcns_notifier(ns);

	ns->user_ns = get_user_ns(task_cred_xxx(tsk, user)->user_ns);

	return ns;
}

struct ipc_namespace *copy_ipcs(unsigned long flags,
				struct task_struct *tsk)
{
	struct ipc_namespace *ns = tsk->nsproxy->ipc_ns;

	if (!(flags & CLONE_NEWIPC))
		return get_ipc_ns(ns);
	return create_ipc_ns(tsk, ns);
}

void free_ipcs(struct ipc_namespace *ns, struct ipc_ids *ids,
	       void (*free)(struct ipc_namespace *, struct kern_ipc_perm *))
{
	struct kern_ipc_perm *perm;
	int next_id;
	int total, in_use;

	down_write(&ids->rw_mutex);

	in_use = ids->in_use;

	for (total = 0, next_id = 0; total < in_use; next_id++) {
		perm = idr_find(&ids->ipcs_idr, next_id);
		if (perm == NULL)
			continue;
		ipc_lock_by_ptr(perm);
		free(ns, perm);
		total++;
	}
	up_write(&ids->rw_mutex);
}

static void free_ipc_ns(struct ipc_namespace *ns)
{
	unregister_ipcns_notifier(ns);
	sem_exit_ns(ns);
	msg_exit_ns(ns);
	shm_exit_ns(ns);
	atomic_dec(&nr_ipc_ns);

	ipcns_notify(IPCNS_REMOVED);
	put_user_ns(ns->user_ns);
	kfree(ns);
}

void put_ipc_ns(struct ipc_namespace *ns)
{
	if (atomic_dec_and_lock(&ns->count, &mq_lock)) {
		mq_clear_sbinfo(ns);
		spin_unlock(&mq_lock);
		mq_put_mnt(ns);
		free_ipc_ns(ns);
	}
}

static void *ipcns_get(struct task_struct *task)
{
	struct ipc_namespace *ns = NULL;
	struct nsproxy *nsproxy;

	rcu_read_lock();
	nsproxy = task_nsproxy(task);
	if (nsproxy)
		ns = get_ipc_ns(nsproxy->ipc_ns);
	rcu_read_unlock();

	return ns;
}

static void ipcns_put(void *ns)
{
	return put_ipc_ns(ns);
}

static int ipcns_install(struct nsproxy *nsproxy, void *ns)
{
	
	exit_sem(current);
	put_ipc_ns(nsproxy->ipc_ns);
	nsproxy->ipc_ns = get_ipc_ns(ns);
	return 0;
}

const struct proc_ns_operations ipcns_operations = {
	.name		= "ipc",
	.type		= CLONE_NEWIPC,
	.get		= ipcns_get,
	.put		= ipcns_put,
	.install	= ipcns_install,
};
