/*
 * Copyright (C) 2006-2010 Red Hat, Inc.  All rights reserved.
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU General Public License v.2.
 */

#include <linux/miscdevice.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/signal.h>
#include <linux/spinlock.h>
#include <linux/dlm.h>
#include <linux/dlm_device.h>
#include <linux/slab.h>

#include "dlm_internal.h"
#include "lockspace.h"
#include "lock.h"
#include "lvb_table.h"
#include "user.h"
#include "ast.h"

static const char name_prefix[] = "dlm";
static const struct file_operations device_fops;
static atomic_t dlm_monitor_opened;
static int dlm_monitor_unused = 1;

#ifdef CONFIG_COMPAT

struct dlm_lock_params32 {
	__u8 mode;
	__u8 namelen;
	__u16 unused;
	__u32 flags;
	__u32 lkid;
	__u32 parent;
	__u64 xid;
	__u64 timeout;
	__u32 castparam;
	__u32 castaddr;
	__u32 bastparam;
	__u32 bastaddr;
	__u32 lksb;
	char lvb[DLM_USER_LVB_LEN];
	char name[0];
};

struct dlm_write_request32 {
	__u32 version[3];
	__u8 cmd;
	__u8 is64bit;
	__u8 unused[2];

	union  {
		struct dlm_lock_params32 lock;
		struct dlm_lspace_params lspace;
		struct dlm_purge_params purge;
	} i;
};

struct dlm_lksb32 {
	__u32 sb_status;
	__u32 sb_lkid;
	__u8 sb_flags;
	__u32 sb_lvbptr;
};

struct dlm_lock_result32 {
	__u32 version[3];
	__u32 length;
	__u32 user_astaddr;
	__u32 user_astparam;
	__u32 user_lksb;
	struct dlm_lksb32 lksb;
	__u8 bast_mode;
	__u8 unused[3];
	
	__u32 lvb_offset;
};

static void compat_input(struct dlm_write_request *kb,
			 struct dlm_write_request32 *kb32,
			 int namelen)
{
	kb->version[0] = kb32->version[0];
	kb->version[1] = kb32->version[1];
	kb->version[2] = kb32->version[2];

	kb->cmd = kb32->cmd;
	kb->is64bit = kb32->is64bit;
	if (kb->cmd == DLM_USER_CREATE_LOCKSPACE ||
	    kb->cmd == DLM_USER_REMOVE_LOCKSPACE) {
		kb->i.lspace.flags = kb32->i.lspace.flags;
		kb->i.lspace.minor = kb32->i.lspace.minor;
		memcpy(kb->i.lspace.name, kb32->i.lspace.name, namelen);
	} else if (kb->cmd == DLM_USER_PURGE) {
		kb->i.purge.nodeid = kb32->i.purge.nodeid;
		kb->i.purge.pid = kb32->i.purge.pid;
	} else {
		kb->i.lock.mode = kb32->i.lock.mode;
		kb->i.lock.namelen = kb32->i.lock.namelen;
		kb->i.lock.flags = kb32->i.lock.flags;
		kb->i.lock.lkid = kb32->i.lock.lkid;
		kb->i.lock.parent = kb32->i.lock.parent;
		kb->i.lock.xid = kb32->i.lock.xid;
		kb->i.lock.timeout = kb32->i.lock.timeout;
		kb->i.lock.castparam = (void *)(long)kb32->i.lock.castparam;
		kb->i.lock.castaddr = (void *)(long)kb32->i.lock.castaddr;
		kb->i.lock.bastparam = (void *)(long)kb32->i.lock.bastparam;
		kb->i.lock.bastaddr = (void *)(long)kb32->i.lock.bastaddr;
		kb->i.lock.lksb = (void *)(long)kb32->i.lock.lksb;
		memcpy(kb->i.lock.lvb, kb32->i.lock.lvb, DLM_USER_LVB_LEN);
		memcpy(kb->i.lock.name, kb32->i.lock.name, namelen);
	}
}

static void compat_output(struct dlm_lock_result *res,
			  struct dlm_lock_result32 *res32)
{
	res32->version[0] = res->version[0];
	res32->version[1] = res->version[1];
	res32->version[2] = res->version[2];

	res32->user_astaddr = (__u32)(long)res->user_astaddr;
	res32->user_astparam = (__u32)(long)res->user_astparam;
	res32->user_lksb = (__u32)(long)res->user_lksb;
	res32->bast_mode = res->bast_mode;

	res32->lvb_offset = res->lvb_offset;
	res32->length = res->length;

	res32->lksb.sb_status = res->lksb.sb_status;
	res32->lksb.sb_flags = res->lksb.sb_flags;
	res32->lksb.sb_lkid = res->lksb.sb_lkid;
	res32->lksb.sb_lvbptr = (__u32)(long)res->lksb.sb_lvbptr;
}
#endif


static int lkb_is_endoflife(int mode, int status)
{
	switch (status) {
	case -DLM_EUNLOCK:
		return 1;
	case -DLM_ECANCEL:
	case -ETIMEDOUT:
	case -EDEADLK:
	case -EAGAIN:
		if (mode == DLM_LOCK_IV)
			return 1;
		break;
	}
	return 0;
}


void dlm_user_add_ast(struct dlm_lkb *lkb, uint32_t flags, int mode,
		      int status, uint32_t sbflags, uint64_t seq)
{
	struct dlm_ls *ls;
	struct dlm_user_args *ua;
	struct dlm_user_proc *proc;
	int rv;

	if (lkb->lkb_flags & (DLM_IFL_ORPHAN | DLM_IFL_DEAD))
		return;

	ls = lkb->lkb_resource->res_ls;
	mutex_lock(&ls->ls_clear_proc_locks);


	if (lkb->lkb_flags & (DLM_IFL_ORPHAN | DLM_IFL_DEAD))
		goto out;

	DLM_ASSERT(lkb->lkb_ua, dlm_print_lkb(lkb););
	ua = lkb->lkb_ua;
	proc = ua->proc;

	if ((flags & DLM_CB_BAST) && ua->bastaddr == NULL)
		goto out;

	if ((flags & DLM_CB_CAST) && lkb_is_endoflife(mode, status))
		lkb->lkb_flags |= DLM_IFL_ENDOFLIFE;

	spin_lock(&proc->asts_spin);

	rv = dlm_add_lkb_callback(lkb, flags, mode, status, sbflags, seq);
	if (rv < 0) {
		spin_unlock(&proc->asts_spin);
		goto out;
	}

	if (list_empty(&lkb->lkb_cb_list)) {
		kref_get(&lkb->lkb_ref);
		list_add_tail(&lkb->lkb_cb_list, &proc->asts);
		wake_up_interruptible(&proc->wait);
	}
	spin_unlock(&proc->asts_spin);

	if (lkb->lkb_flags & DLM_IFL_ENDOFLIFE) {
		
		spin_lock(&proc->locks_spin);
		if (!list_empty(&lkb->lkb_ownqueue)) {
			list_del_init(&lkb->lkb_ownqueue);
			dlm_put_lkb(lkb);
		}
		spin_unlock(&proc->locks_spin);
	}
 out:
	mutex_unlock(&ls->ls_clear_proc_locks);
}

static int device_user_lock(struct dlm_user_proc *proc,
			    struct dlm_lock_params *params)
{
	struct dlm_ls *ls;
	struct dlm_user_args *ua;
	int error = -ENOMEM;

	ls = dlm_find_lockspace_local(proc->lockspace);
	if (!ls)
		return -ENOENT;

	if (!params->castaddr || !params->lksb) {
		error = -EINVAL;
		goto out;
	}

	ua = kzalloc(sizeof(struct dlm_user_args), GFP_NOFS);
	if (!ua)
		goto out;
	ua->proc = proc;
	ua->user_lksb = params->lksb;
	ua->castparam = params->castparam;
	ua->castaddr = params->castaddr;
	ua->bastparam = params->bastparam;
	ua->bastaddr = params->bastaddr;
	ua->xid = params->xid;

	if (params->flags & DLM_LKF_CONVERT)
		error = dlm_user_convert(ls, ua,
				         params->mode, params->flags,
				         params->lkid, params->lvb,
					 (unsigned long) params->timeout);
	else {
		error = dlm_user_request(ls, ua,
					 params->mode, params->flags,
					 params->name, params->namelen,
					 (unsigned long) params->timeout);
		if (!error)
			error = ua->lksb.sb_lkid;
	}
 out:
	dlm_put_lockspace(ls);
	return error;
}

static int device_user_unlock(struct dlm_user_proc *proc,
			      struct dlm_lock_params *params)
{
	struct dlm_ls *ls;
	struct dlm_user_args *ua;
	int error = -ENOMEM;

	ls = dlm_find_lockspace_local(proc->lockspace);
	if (!ls)
		return -ENOENT;

	ua = kzalloc(sizeof(struct dlm_user_args), GFP_NOFS);
	if (!ua)
		goto out;
	ua->proc = proc;
	ua->user_lksb = params->lksb;
	ua->castparam = params->castparam;
	ua->castaddr = params->castaddr;

	if (params->flags & DLM_LKF_CANCEL)
		error = dlm_user_cancel(ls, ua, params->flags, params->lkid);
	else
		error = dlm_user_unlock(ls, ua, params->flags, params->lkid,
					params->lvb);
 out:
	dlm_put_lockspace(ls);
	return error;
}

static int device_user_deadlock(struct dlm_user_proc *proc,
				struct dlm_lock_params *params)
{
	struct dlm_ls *ls;
	int error;

	ls = dlm_find_lockspace_local(proc->lockspace);
	if (!ls)
		return -ENOENT;

	error = dlm_user_deadlock(ls, params->flags, params->lkid);

	dlm_put_lockspace(ls);
	return error;
}

static int dlm_device_register(struct dlm_ls *ls, char *name)
{
	int error, len;

	if (ls->ls_device.name)
		return 0;

	error = -ENOMEM;
	len = strlen(name) + strlen(name_prefix) + 2;
	ls->ls_device.name = kzalloc(len, GFP_NOFS);
	if (!ls->ls_device.name)
		goto fail;

	snprintf((char *)ls->ls_device.name, len, "%s_%s", name_prefix,
		 name);
	ls->ls_device.fops = &device_fops;
	ls->ls_device.minor = MISC_DYNAMIC_MINOR;

	error = misc_register(&ls->ls_device);
	if (error) {
		kfree(ls->ls_device.name);
	}
fail:
	return error;
}

int dlm_device_deregister(struct dlm_ls *ls)
{
	int error;

	if (!ls->ls_device.name)
		return 0;

	error = misc_deregister(&ls->ls_device);
	if (!error)
		kfree(ls->ls_device.name);
	return error;
}

static int device_user_purge(struct dlm_user_proc *proc,
			     struct dlm_purge_params *params)
{
	struct dlm_ls *ls;
	int error;

	ls = dlm_find_lockspace_local(proc->lockspace);
	if (!ls)
		return -ENOENT;

	error = dlm_user_purge(ls, proc, params->nodeid, params->pid);

	dlm_put_lockspace(ls);
	return error;
}

static int device_create_lockspace(struct dlm_lspace_params *params)
{
	dlm_lockspace_t *lockspace;
	struct dlm_ls *ls;
	int error;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	error = dlm_new_lockspace(params->name, NULL, params->flags,
				  DLM_USER_LVB_LEN, NULL, NULL, NULL,
				  &lockspace);
	if (error)
		return error;

	ls = dlm_find_lockspace_local(lockspace);
	if (!ls)
		return -ENOENT;

	error = dlm_device_register(ls, params->name);
	dlm_put_lockspace(ls);

	if (error)
		dlm_release_lockspace(lockspace, 0);
	else
		error = ls->ls_device.minor;

	return error;
}

static int device_remove_lockspace(struct dlm_lspace_params *params)
{
	dlm_lockspace_t *lockspace;
	struct dlm_ls *ls;
	int error, force = 0;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	ls = dlm_find_lockspace_device(params->minor);
	if (!ls)
		return -ENOENT;

	if (params->flags & DLM_USER_LSFLG_FORCEFREE)
		force = 2;

	lockspace = ls->ls_local_handle;
	dlm_put_lockspace(ls);


	error = dlm_release_lockspace(lockspace, force);
	if (error > 0)
		error = 0;
	return error;
}

static int check_version(struct dlm_write_request *req)
{
	if (req->version[0] != DLM_DEVICE_VERSION_MAJOR ||
	    (req->version[0] == DLM_DEVICE_VERSION_MAJOR &&
	     req->version[1] > DLM_DEVICE_VERSION_MINOR)) {

		printk(KERN_DEBUG "dlm: process %s (%d) version mismatch "
		       "user (%d.%d.%d) kernel (%d.%d.%d)\n",
		       current->comm,
		       task_pid_nr(current),
		       req->version[0],
		       req->version[1],
		       req->version[2],
		       DLM_DEVICE_VERSION_MAJOR,
		       DLM_DEVICE_VERSION_MINOR,
		       DLM_DEVICE_VERSION_PATCH);
		return -EINVAL;
	}
	return 0;
}



static ssize_t device_write(struct file *file, const char __user *buf,
			    size_t count, loff_t *ppos)
{
	struct dlm_user_proc *proc = file->private_data;
	struct dlm_write_request *kbuf;
	sigset_t tmpsig, allsigs;
	int error;

#ifdef CONFIG_COMPAT
	if (count < sizeof(struct dlm_write_request32))
#else
	if (count < sizeof(struct dlm_write_request))
#endif
		return -EINVAL;

	kbuf = kzalloc(count + 1, GFP_NOFS);
	if (!kbuf)
		return -ENOMEM;

	if (copy_from_user(kbuf, buf, count)) {
		error = -EFAULT;
		goto out_free;
	}

	if (check_version(kbuf)) {
		error = -EBADE;
		goto out_free;
	}

#ifdef CONFIG_COMPAT
	if (!kbuf->is64bit) {
		struct dlm_write_request32 *k32buf;
		int namelen = 0;

		if (count > sizeof(struct dlm_write_request32))
			namelen = count - sizeof(struct dlm_write_request32);

		k32buf = (struct dlm_write_request32 *)kbuf;

		
		kbuf = kzalloc(sizeof(struct dlm_write_request) + namelen + 1,
			       GFP_NOFS);
		if (!kbuf) {
			kfree(k32buf);
			return -ENOMEM;
		}

		if (proc)
			set_bit(DLM_PROC_FLAGS_COMPAT, &proc->flags);

		compat_input(kbuf, k32buf, namelen);
		kfree(k32buf);
	}
#endif

	
	if ((kbuf->cmd == DLM_USER_LOCK || kbuf->cmd == DLM_USER_UNLOCK) &&
	    (proc && test_bit(DLM_PROC_FLAGS_CLOSING, &proc->flags))) {
		error = -EINVAL;
		goto out_free;
	}

	sigfillset(&allsigs);
	sigprocmask(SIG_BLOCK, &allsigs, &tmpsig);

	error = -EINVAL;

	switch (kbuf->cmd)
	{
	case DLM_USER_LOCK:
		if (!proc) {
			log_print("no locking on control device");
			goto out_sig;
		}
		error = device_user_lock(proc, &kbuf->i.lock);
		break;

	case DLM_USER_UNLOCK:
		if (!proc) {
			log_print("no locking on control device");
			goto out_sig;
		}
		error = device_user_unlock(proc, &kbuf->i.lock);
		break;

	case DLM_USER_DEADLOCK:
		if (!proc) {
			log_print("no locking on control device");
			goto out_sig;
		}
		error = device_user_deadlock(proc, &kbuf->i.lock);
		break;

	case DLM_USER_CREATE_LOCKSPACE:
		if (proc) {
			log_print("create/remove only on control device");
			goto out_sig;
		}
		error = device_create_lockspace(&kbuf->i.lspace);
		break;

	case DLM_USER_REMOVE_LOCKSPACE:
		if (proc) {
			log_print("create/remove only on control device");
			goto out_sig;
		}
		error = device_remove_lockspace(&kbuf->i.lspace);
		break;

	case DLM_USER_PURGE:
		if (!proc) {
			log_print("no locking on control device");
			goto out_sig;
		}
		error = device_user_purge(proc, &kbuf->i.purge);
		break;

	default:
		log_print("Unknown command passed to DLM device : %d\n",
			  kbuf->cmd);
	}

 out_sig:
	sigprocmask(SIG_SETMASK, &tmpsig, NULL);
 out_free:
	kfree(kbuf);
	return error;
}


static int device_open(struct inode *inode, struct file *file)
{
	struct dlm_user_proc *proc;
	struct dlm_ls *ls;

	ls = dlm_find_lockspace_device(iminor(inode));
	if (!ls)
		return -ENOENT;

	proc = kzalloc(sizeof(struct dlm_user_proc), GFP_NOFS);
	if (!proc) {
		dlm_put_lockspace(ls);
		return -ENOMEM;
	}

	proc->lockspace = ls->ls_local_handle;
	INIT_LIST_HEAD(&proc->asts);
	INIT_LIST_HEAD(&proc->locks);
	INIT_LIST_HEAD(&proc->unlocking);
	spin_lock_init(&proc->asts_spin);
	spin_lock_init(&proc->locks_spin);
	init_waitqueue_head(&proc->wait);
	file->private_data = proc;

	return 0;
}

static int device_close(struct inode *inode, struct file *file)
{
	struct dlm_user_proc *proc = file->private_data;
	struct dlm_ls *ls;
	sigset_t tmpsig, allsigs;

	ls = dlm_find_lockspace_local(proc->lockspace);
	if (!ls)
		return -ENOENT;

	sigfillset(&allsigs);
	sigprocmask(SIG_BLOCK, &allsigs, &tmpsig);

	set_bit(DLM_PROC_FLAGS_CLOSING, &proc->flags);

	dlm_clear_proc_locks(ls, proc);


	kfree(proc);
	file->private_data = NULL;

	dlm_put_lockspace(ls);
	dlm_put_lockspace(ls);  


	sigprocmask(SIG_SETMASK, &tmpsig, NULL);
	recalc_sigpending();

	return 0;
}

static int copy_result_to_user(struct dlm_user_args *ua, int compat,
			       uint32_t flags, int mode, int copy_lvb,
			       char __user *buf, size_t count)
{
#ifdef CONFIG_COMPAT
	struct dlm_lock_result32 result32;
#endif
	struct dlm_lock_result result;
	void *resultptr;
	int error=0;
	int len;
	int struct_len;

	memset(&result, 0, sizeof(struct dlm_lock_result));
	result.version[0] = DLM_DEVICE_VERSION_MAJOR;
	result.version[1] = DLM_DEVICE_VERSION_MINOR;
	result.version[2] = DLM_DEVICE_VERSION_PATCH;
	memcpy(&result.lksb, &ua->lksb, sizeof(struct dlm_lksb));
	result.user_lksb = ua->user_lksb;


	if (flags & DLM_CB_BAST) {
		result.user_astaddr = ua->bastaddr;
		result.user_astparam = ua->bastparam;
		result.bast_mode = mode;
	} else {
		result.user_astaddr = ua->castaddr;
		result.user_astparam = ua->castparam;
	}

#ifdef CONFIG_COMPAT
	if (compat)
		len = sizeof(struct dlm_lock_result32);
	else
#endif
		len = sizeof(struct dlm_lock_result);
	struct_len = len;


	if (copy_lvb && ua->lksb.sb_lvbptr && count >= len + DLM_USER_LVB_LEN) {
		if (copy_to_user(buf+len, ua->lksb.sb_lvbptr,
				 DLM_USER_LVB_LEN)) {
			error = -EFAULT;
			goto out;
		}

		result.lvb_offset = len;
		len += DLM_USER_LVB_LEN;
	}

	result.length = len;
	resultptr = &result;
#ifdef CONFIG_COMPAT
	if (compat) {
		compat_output(&result, &result32);
		resultptr = &result32;
	}
#endif

	if (copy_to_user(buf, resultptr, struct_len))
		error = -EFAULT;
	else
		error = len;
 out:
	return error;
}

static int copy_version_to_user(char __user *buf, size_t count)
{
	struct dlm_device_version ver;

	memset(&ver, 0, sizeof(struct dlm_device_version));
	ver.version[0] = DLM_DEVICE_VERSION_MAJOR;
	ver.version[1] = DLM_DEVICE_VERSION_MINOR;
	ver.version[2] = DLM_DEVICE_VERSION_PATCH;

	if (copy_to_user(buf, &ver, sizeof(struct dlm_device_version)))
		return -EFAULT;
	return sizeof(struct dlm_device_version);
}


static ssize_t device_read(struct file *file, char __user *buf, size_t count,
			   loff_t *ppos)
{
	struct dlm_user_proc *proc = file->private_data;
	struct dlm_lkb *lkb;
	DECLARE_WAITQUEUE(wait, current);
	struct dlm_callback cb;
	int rv, resid, copy_lvb = 0;

	if (count == sizeof(struct dlm_device_version)) {
		rv = copy_version_to_user(buf, count);
		return rv;
	}

	if (!proc) {
		log_print("non-version read from control device %zu", count);
		return -EINVAL;
	}

#ifdef CONFIG_COMPAT
	if (count < sizeof(struct dlm_lock_result32))
#else
	if (count < sizeof(struct dlm_lock_result))
#endif
		return -EINVAL;

 try_another:

	
	if (test_bit(DLM_PROC_FLAGS_CLOSING, &proc->flags))
		return -EINVAL;

	spin_lock(&proc->asts_spin);
	if (list_empty(&proc->asts)) {
		if (file->f_flags & O_NONBLOCK) {
			spin_unlock(&proc->asts_spin);
			return -EAGAIN;
		}

		add_wait_queue(&proc->wait, &wait);

	repeat:
		set_current_state(TASK_INTERRUPTIBLE);
		if (list_empty(&proc->asts) && !signal_pending(current)) {
			spin_unlock(&proc->asts_spin);
			schedule();
			spin_lock(&proc->asts_spin);
			goto repeat;
		}
		set_current_state(TASK_RUNNING);
		remove_wait_queue(&proc->wait, &wait);

		if (signal_pending(current)) {
			spin_unlock(&proc->asts_spin);
			return -ERESTARTSYS;
		}
	}


	lkb = list_entry(proc->asts.next, struct dlm_lkb, lkb_cb_list);

	rv = dlm_rem_lkb_callback(lkb->lkb_resource->res_ls, lkb, &cb, &resid);
	if (rv < 0) {
		log_print("dlm_rem_lkb_callback empty %x", lkb->lkb_id);
		list_del_init(&lkb->lkb_cb_list);
		spin_unlock(&proc->asts_spin);
		
		dlm_put_lkb(lkb);
		goto try_another;
	}
	if (!resid)
		list_del_init(&lkb->lkb_cb_list);
	spin_unlock(&proc->asts_spin);

	if (cb.flags & DLM_CB_SKIP) {
		
		if (!resid)
			dlm_put_lkb(lkb);
		goto try_another;
	}

	if (cb.flags & DLM_CB_CAST) {
		int old_mode, new_mode;

		old_mode = lkb->lkb_last_cast.mode;
		new_mode = cb.mode;

		if (!cb.sb_status && lkb->lkb_lksb->sb_lvbptr &&
		    dlm_lvb_operations[old_mode + 1][new_mode + 1])
			copy_lvb = 1;

		lkb->lkb_lksb->sb_status = cb.sb_status;
		lkb->lkb_lksb->sb_flags = cb.sb_flags;
	}

	rv = copy_result_to_user(lkb->lkb_ua,
				 test_bit(DLM_PROC_FLAGS_COMPAT, &proc->flags),
				 cb.flags, cb.mode, copy_lvb, buf, count);

	
	if (!resid)
		dlm_put_lkb(lkb);

	return rv;
}

static unsigned int device_poll(struct file *file, poll_table *wait)
{
	struct dlm_user_proc *proc = file->private_data;

	poll_wait(file, &proc->wait, wait);

	spin_lock(&proc->asts_spin);
	if (!list_empty(&proc->asts)) {
		spin_unlock(&proc->asts_spin);
		return POLLIN | POLLRDNORM;
	}
	spin_unlock(&proc->asts_spin);
	return 0;
}

int dlm_user_daemon_available(void)
{

	if (!dlm_our_nodeid())
		return 0;


	if (dlm_monitor_unused)
		return 1;

	return atomic_read(&dlm_monitor_opened) ? 1 : 0;
}

static int ctl_device_open(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static int ctl_device_close(struct inode *inode, struct file *file)
{
	return 0;
}

static int monitor_device_open(struct inode *inode, struct file *file)
{
	atomic_inc(&dlm_monitor_opened);
	dlm_monitor_unused = 0;
	return 0;
}

static int monitor_device_close(struct inode *inode, struct file *file)
{
	if (atomic_dec_and_test(&dlm_monitor_opened))
		dlm_stop_lockspaces();
	return 0;
}

static const struct file_operations device_fops = {
	.open    = device_open,
	.release = device_close,
	.read    = device_read,
	.write   = device_write,
	.poll    = device_poll,
	.owner   = THIS_MODULE,
	.llseek  = noop_llseek,
};

static const struct file_operations ctl_device_fops = {
	.open    = ctl_device_open,
	.release = ctl_device_close,
	.read    = device_read,
	.write   = device_write,
	.owner   = THIS_MODULE,
	.llseek  = noop_llseek,
};

static struct miscdevice ctl_device = {
	.name  = "dlm-control",
	.fops  = &ctl_device_fops,
	.minor = MISC_DYNAMIC_MINOR,
};

static const struct file_operations monitor_device_fops = {
	.open    = monitor_device_open,
	.release = monitor_device_close,
	.owner   = THIS_MODULE,
	.llseek  = noop_llseek,
};

static struct miscdevice monitor_device = {
	.name  = "dlm-monitor",
	.fops  = &monitor_device_fops,
	.minor = MISC_DYNAMIC_MINOR,
};

int __init dlm_user_init(void)
{
	int error;

	atomic_set(&dlm_monitor_opened, 0);

	error = misc_register(&ctl_device);
	if (error) {
		log_print("misc_register failed for control device");
		goto out;
	}

	error = misc_register(&monitor_device);
	if (error) {
		log_print("misc_register failed for monitor device");
		misc_deregister(&ctl_device);
	}
 out:
	return error;
}

void dlm_user_exit(void)
{
	misc_deregister(&ctl_device);
	misc_deregister(&monitor_device);
}

