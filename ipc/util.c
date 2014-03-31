/*
 * linux/ipc/util.c
 * Copyright (C) 1992 Krishna Balasubramanian
 *
 * Sep 1997 - Call suser() last after "normal" permission checks so we
 *            get BSD style process accounting right.
 *            Occurs in several places in the IPC code.
 *            Chris Evans, <chris@ferret.lmh.ox.ac.uk>
 * Nov 1999 - ipc helper functions, unified SMP locking
 *	      Manfred Spraul <manfred@colorfullife.com>
 * Oct 2002 - One lock per IPC id. RCU ipc_free for lock-free grow_ary().
 *            Mingming Cao <cmm@us.ibm.com>
 * Mar 2006 - support for audit of ipc object properties
 *            Dustin Kirkland <dustin.kirkland@us.ibm.com>
 * Jun 2006 - namespaces ssupport
 *            OpenVZ, SWsoft Inc.
 *            Pavel Emelianov <xemul@openvz.org>
 */

#include <linux/mm.h>
#include <linux/shm.h>
#include <linux/init.h>
#include <linux/msg.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/capability.h>
#include <linux/highuid.h>
#include <linux/security.h>
#include <linux/rcupdate.h>
#include <linux/workqueue.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/audit.h>
#include <linux/nsproxy.h>
#include <linux/rwsem.h>
#include <linux/memory.h>
#include <linux/ipc_namespace.h>

#include <asm/unistd.h>

#include "util.h"

struct ipc_proc_iface {
	const char *path;
	const char *header;
	int ids;
	int (*show)(struct seq_file *, void *);
};

#ifdef CONFIG_MEMORY_HOTPLUG

static void ipc_memory_notifier(struct work_struct *work)
{
	ipcns_notify(IPCNS_MEMCHANGED);
}

static DECLARE_WORK(ipc_memory_wq, ipc_memory_notifier);


static int ipc_memory_callback(struct notifier_block *self,
				unsigned long action, void *arg)
{
	switch (action) {
	case MEM_ONLINE:    
	case MEM_OFFLINE:   
		if (!work_pending(&ipc_memory_wq))
			schedule_work(&ipc_memory_wq);
		break;
	case MEM_GOING_ONLINE:
	case MEM_GOING_OFFLINE:
	case MEM_CANCEL_ONLINE:
	case MEM_CANCEL_OFFLINE:
	default:
		break;
	}

	return NOTIFY_OK;
}

#endif 

 
static int __init ipc_init(void)
{
	sem_init();
	msg_init();
	shm_init();
	hotplug_memory_notifier(ipc_memory_callback, IPC_CALLBACK_PRI);
	register_ipcns_notifier(&init_ipc_ns);
	return 0;
}
__initcall(ipc_init);

 
void ipc_init_ids(struct ipc_ids *ids)
{
	init_rwsem(&ids->rw_mutex);

	ids->in_use = 0;
	ids->seq = 0;
	{
		int seq_limit = INT_MAX/SEQ_MULTIPLIER;
		if (seq_limit > USHRT_MAX)
			ids->seq_max = USHRT_MAX;
		 else
		 	ids->seq_max = seq_limit;
	}

	idr_init(&ids->ipcs_idr);
}

#ifdef CONFIG_PROC_FS
static const struct file_operations sysvipc_proc_fops;
void __init ipc_init_proc_interface(const char *path, const char *header,
		int ids, int (*show)(struct seq_file *, void *))
{
	struct proc_dir_entry *pde;
	struct ipc_proc_iface *iface;

	iface = kmalloc(sizeof(*iface), GFP_KERNEL);
	if (!iface)
		return;
	iface->path	= path;
	iface->header	= header;
	iface->ids	= ids;
	iface->show	= show;

	pde = proc_create_data(path,
			       S_IRUGO,        
			       NULL,           
			       &sysvipc_proc_fops,
			       iface);
	if (!pde) {
		kfree(iface);
	}
}
#endif

 
static struct kern_ipc_perm *ipc_findkey(struct ipc_ids *ids, key_t key)
{
	struct kern_ipc_perm *ipc;
	int next_id;
	int total;

	for (total = 0, next_id = 0; total < ids->in_use; next_id++) {
		ipc = idr_find(&ids->ipcs_idr, next_id);

		if (ipc == NULL)
			continue;

		if (ipc->key != key) {
			total++;
			continue;
		}

		ipc_lock_by_ptr(ipc);
		return ipc;
	}

	return NULL;
}


int ipc_get_maxid(struct ipc_ids *ids)
{
	struct kern_ipc_perm *ipc;
	int max_id = -1;
	int total, id;

	if (ids->in_use == 0)
		return -1;

	if (ids->in_use == IPCMNI)
		return IPCMNI - 1;

	
	total = 0;
	for (id = 0; id < IPCMNI && total < ids->in_use; id++) {
		ipc = idr_find(&ids->ipcs_idr, id);
		if (ipc != NULL) {
			max_id = id;
			total++;
		}
	}
	return max_id;
}

 
int ipc_addid(struct ipc_ids* ids, struct kern_ipc_perm* new, int size)
{
	uid_t euid;
	gid_t egid;
	int id, err;

	if (size > IPCMNI)
		size = IPCMNI;

	if (ids->in_use >= size)
		return -ENOSPC;

	spin_lock_init(&new->lock);
	new->deleted = 0;
	rcu_read_lock();
	spin_lock(&new->lock);

	err = idr_get_new(&ids->ipcs_idr, new, &id);
	if (err) {
		spin_unlock(&new->lock);
		rcu_read_unlock();
		return err;
	}

	ids->in_use++;

	current_euid_egid(&euid, &egid);
	new->cuid = new->uid = euid;
	new->gid = new->cgid = egid;

	new->seq = ids->seq++;
	if(ids->seq > ids->seq_max)
		ids->seq = 0;

	new->id = ipc_buildid(id, new->seq);
	return id;
}

static int ipcget_new(struct ipc_namespace *ns, struct ipc_ids *ids,
		struct ipc_ops *ops, struct ipc_params *params)
{
	int err;
retry:
	err = idr_pre_get(&ids->ipcs_idr, GFP_KERNEL);

	if (!err)
		return -ENOMEM;

	down_write(&ids->rw_mutex);
	err = ops->getnew(ns, params);
	up_write(&ids->rw_mutex);

	if (err == -EAGAIN)
		goto retry;

	return err;
}

static int ipc_check_perms(struct ipc_namespace *ns,
			   struct kern_ipc_perm *ipcp,
			   struct ipc_ops *ops,
			   struct ipc_params *params)
{
	int err;

	if (ipcperms(ns, ipcp, params->flg))
		err = -EACCES;
	else {
		err = ops->associate(ipcp, params->flg);
		if (!err)
			err = ipcp->id;
	}

	return err;
}

static int ipcget_public(struct ipc_namespace *ns, struct ipc_ids *ids,
		struct ipc_ops *ops, struct ipc_params *params)
{
	struct kern_ipc_perm *ipcp;
	int flg = params->flg;
	int err;
retry:
	err = idr_pre_get(&ids->ipcs_idr, GFP_KERNEL);

	down_write(&ids->rw_mutex);
	ipcp = ipc_findkey(ids, params->key);
	if (ipcp == NULL) {
		
		if (!(flg & IPC_CREAT))
			err = -ENOENT;
		else if (!err)
			err = -ENOMEM;
		else
			err = ops->getnew(ns, params);
	} else {
		

		if (flg & IPC_CREAT && flg & IPC_EXCL)
			err = -EEXIST;
		else {
			err = 0;
			if (ops->more_checks)
				err = ops->more_checks(ipcp, params);
			if (!err)
				err = ipc_check_perms(ns, ipcp, ops, params);
		}
		ipc_unlock(ipcp);
	}
	up_write(&ids->rw_mutex);

	if (err == -EAGAIN)
		goto retry;

	return err;
}


 
void ipc_rmid(struct ipc_ids *ids, struct kern_ipc_perm *ipcp)
{
	int lid = ipcid_to_idx(ipcp->id);

	idr_remove(&ids->ipcs_idr, lid);

	ids->in_use--;

	ipcp->deleted = 1;

	return;
}

 
void* ipc_alloc(int size)
{
	void* out;
	if(size > PAGE_SIZE)
		out = vmalloc(size);
	else
		out = kmalloc(size, GFP_KERNEL);
	return out;
}


void ipc_free(void* ptr, int size)
{
	if(size > PAGE_SIZE)
		vfree(ptr);
	else
		kfree(ptr);
}

struct ipc_rcu_hdr
{
	int refcount;
	int is_vmalloc;
	void *data[0];
};


struct ipc_rcu_grace
{
	struct rcu_head rcu;
	
	void *data[0];
};

struct ipc_rcu_sched
{
	struct work_struct work;
	
	void *data[0];
};

#define HDRLEN_KMALLOC		(sizeof(struct ipc_rcu_grace) > sizeof(struct ipc_rcu_hdr) ? \
					sizeof(struct ipc_rcu_grace) : sizeof(struct ipc_rcu_hdr))
#define HDRLEN_VMALLOC		(sizeof(struct ipc_rcu_sched) > HDRLEN_KMALLOC ? \
					sizeof(struct ipc_rcu_sched) : HDRLEN_KMALLOC)

static inline int rcu_use_vmalloc(int size)
{
	
	if (HDRLEN_KMALLOC + size > PAGE_SIZE)
		return 1;
	return 0;
}

 
void* ipc_rcu_alloc(int size)
{
	void* out;
	if (rcu_use_vmalloc(size)) {
		out = vmalloc(HDRLEN_VMALLOC + size);
		if (out) {
			out += HDRLEN_VMALLOC;
			container_of(out, struct ipc_rcu_hdr, data)->is_vmalloc = 1;
			container_of(out, struct ipc_rcu_hdr, data)->refcount = 1;
		}
	} else {
		out = kmalloc(HDRLEN_KMALLOC + size, GFP_KERNEL);
		if (out) {
			out += HDRLEN_KMALLOC;
			container_of(out, struct ipc_rcu_hdr, data)->is_vmalloc = 0;
			container_of(out, struct ipc_rcu_hdr, data)->refcount = 1;
		}
	}

	return out;
}

void ipc_rcu_getref(void *ptr)
{
	container_of(ptr, struct ipc_rcu_hdr, data)->refcount++;
}

static void ipc_do_vfree(struct work_struct *work)
{
	vfree(container_of(work, struct ipc_rcu_sched, work));
}

static void ipc_schedule_free(struct rcu_head *head)
{
	struct ipc_rcu_grace *grace;
	struct ipc_rcu_sched *sched;

	grace = container_of(head, struct ipc_rcu_grace, rcu);
	sched = container_of(&(grace->data[0]), struct ipc_rcu_sched,
				data[0]);

	INIT_WORK(&sched->work, ipc_do_vfree);
	schedule_work(&sched->work);
}

void ipc_rcu_putref(void *ptr)
{
	if (--container_of(ptr, struct ipc_rcu_hdr, data)->refcount > 0)
		return;

	if (container_of(ptr, struct ipc_rcu_hdr, data)->is_vmalloc) {
		call_rcu(&container_of(ptr, struct ipc_rcu_grace, data)->rcu,
				ipc_schedule_free);
	} else {
		kfree_rcu(container_of(ptr, struct ipc_rcu_grace, data), rcu);
	}
}

 
int ipcperms(struct ipc_namespace *ns, struct kern_ipc_perm *ipcp, short flag)
{
	uid_t euid = current_euid();
	int requested_mode, granted_mode;

	audit_ipc_obj(ipcp);
	requested_mode = (flag >> 6) | (flag >> 3) | flag;
	granted_mode = ipcp->mode;
	if (euid == ipcp->cuid ||
	    euid == ipcp->uid)
		granted_mode >>= 6;
	else if (in_group_p(ipcp->cgid) || in_group_p(ipcp->gid))
		granted_mode >>= 3;
	
	if ((requested_mode & ~granted_mode & 0007) && 
	    !ns_capable(ns->user_ns, CAP_IPC_OWNER))
		return -1;

	return security_ipc_permission(ipcp, flag);
}


 

void kernel_to_ipc64_perm (struct kern_ipc_perm *in, struct ipc64_perm *out)
{
	out->key	= in->key;
	out->uid	= in->uid;
	out->gid	= in->gid;
	out->cuid	= in->cuid;
	out->cgid	= in->cgid;
	out->mode	= in->mode;
	out->seq	= in->seq;
}

 
void ipc64_perm_to_ipc_perm (struct ipc64_perm *in, struct ipc_perm *out)
{
	out->key	= in->key;
	SET_UID(out->uid, in->uid);
	SET_GID(out->gid, in->gid);
	SET_UID(out->cuid, in->cuid);
	SET_GID(out->cgid, in->cgid);
	out->mode	= in->mode;
	out->seq	= in->seq;
}


struct kern_ipc_perm *ipc_lock(struct ipc_ids *ids, int id)
{
	struct kern_ipc_perm *out;
	int lid = ipcid_to_idx(id);

	rcu_read_lock();
	out = idr_find(&ids->ipcs_idr, lid);
	if (out == NULL) {
		rcu_read_unlock();
		return ERR_PTR(-EINVAL);
	}

	spin_lock(&out->lock);
	
	if (out->deleted) {
		spin_unlock(&out->lock);
		rcu_read_unlock();
		return ERR_PTR(-EINVAL);
	}

	return out;
}

struct kern_ipc_perm *ipc_lock_check(struct ipc_ids *ids, int id)
{
	struct kern_ipc_perm *out;

	out = ipc_lock(ids, id);
	if (IS_ERR(out))
		return out;

	if (ipc_checkid(out, id)) {
		ipc_unlock(out);
		return ERR_PTR(-EIDRM);
	}

	return out;
}

int ipcget(struct ipc_namespace *ns, struct ipc_ids *ids,
			struct ipc_ops *ops, struct ipc_params *params)
{
	if (params->key == IPC_PRIVATE)
		return ipcget_new(ns, ids, ops, params);
	else
		return ipcget_public(ns, ids, ops, params);
}

void ipc_update_perm(struct ipc64_perm *in, struct kern_ipc_perm *out)
{
	out->uid = in->uid;
	out->gid = in->gid;
	out->mode = (out->mode & ~S_IRWXUGO)
		| (in->mode & S_IRWXUGO);
}

struct kern_ipc_perm *ipcctl_pre_down(struct ipc_namespace *ns,
				      struct ipc_ids *ids, int id, int cmd,
				      struct ipc64_perm *perm, int extra_perm)
{
	struct kern_ipc_perm *ipcp;
	uid_t euid;
	int err;

	down_write(&ids->rw_mutex);
	ipcp = ipc_lock_check(ids, id);
	if (IS_ERR(ipcp)) {
		err = PTR_ERR(ipcp);
		goto out_up;
	}

	audit_ipc_obj(ipcp);
	if (cmd == IPC_SET)
		audit_ipc_set_perm(extra_perm, perm->uid,
					 perm->gid, perm->mode);

	euid = current_euid();
	if (euid == ipcp->cuid || euid == ipcp->uid  ||
	    ns_capable(ns->user_ns, CAP_SYS_ADMIN))
		return ipcp;

	err = -EPERM;
	ipc_unlock(ipcp);
out_up:
	up_write(&ids->rw_mutex);
	return ERR_PTR(err);
}

#ifdef __ARCH_WANT_IPC_PARSE_VERSION


 
int ipc_parse_version (int *cmd)
{
	if (*cmd & IPC_64) {
		*cmd ^= IPC_64;
		return IPC_64;
	} else {
		return IPC_OLD;
	}
}

#endif 

#ifdef CONFIG_PROC_FS
struct ipc_proc_iter {
	struct ipc_namespace *ns;
	struct ipc_proc_iface *iface;
};

static struct kern_ipc_perm *sysvipc_find_ipc(struct ipc_ids *ids, loff_t pos,
					      loff_t *new_pos)
{
	struct kern_ipc_perm *ipc;
	int total, id;

	total = 0;
	for (id = 0; id < pos && total < ids->in_use; id++) {
		ipc = idr_find(&ids->ipcs_idr, id);
		if (ipc != NULL)
			total++;
	}

	if (total >= ids->in_use)
		return NULL;

	for ( ; pos < IPCMNI; pos++) {
		ipc = idr_find(&ids->ipcs_idr, pos);
		if (ipc != NULL) {
			*new_pos = pos + 1;
			ipc_lock_by_ptr(ipc);
			return ipc;
		}
	}

	
	return NULL;
}

static void *sysvipc_proc_next(struct seq_file *s, void *it, loff_t *pos)
{
	struct ipc_proc_iter *iter = s->private;
	struct ipc_proc_iface *iface = iter->iface;
	struct kern_ipc_perm *ipc = it;

	
	if (ipc && ipc != SEQ_START_TOKEN)
		ipc_unlock(ipc);

	return sysvipc_find_ipc(&iter->ns->ids[iface->ids], *pos, pos);
}

static void *sysvipc_proc_start(struct seq_file *s, loff_t *pos)
{
	struct ipc_proc_iter *iter = s->private;
	struct ipc_proc_iface *iface = iter->iface;
	struct ipc_ids *ids;

	ids = &iter->ns->ids[iface->ids];

	down_read(&ids->rw_mutex);

	
	if (*pos < 0)
		return NULL;

	
	if (*pos == 0)
		return SEQ_START_TOKEN;

	
	return sysvipc_find_ipc(ids, *pos - 1, pos);
}

static void sysvipc_proc_stop(struct seq_file *s, void *it)
{
	struct kern_ipc_perm *ipc = it;
	struct ipc_proc_iter *iter = s->private;
	struct ipc_proc_iface *iface = iter->iface;
	struct ipc_ids *ids;

	
	if (ipc && ipc != SEQ_START_TOKEN)
		ipc_unlock(ipc);

	ids = &iter->ns->ids[iface->ids];
	
	up_read(&ids->rw_mutex);
}

static int sysvipc_proc_show(struct seq_file *s, void *it)
{
	struct ipc_proc_iter *iter = s->private;
	struct ipc_proc_iface *iface = iter->iface;

	if (it == SEQ_START_TOKEN)
		return seq_puts(s, iface->header);

	return iface->show(s, it);
}

static const struct seq_operations sysvipc_proc_seqops = {
	.start = sysvipc_proc_start,
	.stop  = sysvipc_proc_stop,
	.next  = sysvipc_proc_next,
	.show  = sysvipc_proc_show,
};

static int sysvipc_proc_open(struct inode *inode, struct file *file)
{
	int ret;
	struct seq_file *seq;
	struct ipc_proc_iter *iter;

	ret = -ENOMEM;
	iter = kmalloc(sizeof(*iter), GFP_KERNEL);
	if (!iter)
		goto out;

	ret = seq_open(file, &sysvipc_proc_seqops);
	if (ret)
		goto out_kfree;

	seq = file->private_data;
	seq->private = iter;

	iter->iface = PDE(inode)->data;
	iter->ns    = get_ipc_ns(current->nsproxy->ipc_ns);
out:
	return ret;
out_kfree:
	kfree(iter);
	goto out;
}

static int sysvipc_proc_release(struct inode *inode, struct file *file)
{
	struct seq_file *seq = file->private_data;
	struct ipc_proc_iter *iter = seq->private;
	put_ipc_ns(iter->ns);
	return seq_release_private(inode, file);
}

static const struct file_operations sysvipc_proc_fops = {
	.open    = sysvipc_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = sysvipc_proc_release,
};
#endif 
