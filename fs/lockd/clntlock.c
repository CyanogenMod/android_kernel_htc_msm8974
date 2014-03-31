/*
 * linux/fs/lockd/clntlock.c
 *
 * Lock handling for the client side NLM implementation
 *
 * Copyright (C) 1996, Olaf Kirch <okir@monad.swb.de>
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/nfs_fs.h>
#include <linux/sunrpc/clnt.h>
#include <linux/sunrpc/svc.h>
#include <linux/lockd/lockd.h>
#include <linux/kthread.h>

#define NLMDBG_FACILITY		NLMDBG_CLIENT

static int			reclaimer(void *ptr);


struct nlm_wait {
	struct list_head	b_list;		
	wait_queue_head_t	b_wait;		
	struct nlm_host *	b_host;
	struct file_lock *	b_lock;		
	unsigned short		b_reclaim;	
	__be32			b_status;	
};

static LIST_HEAD(nlm_blocked);
static DEFINE_SPINLOCK(nlm_blocked_lock);

struct nlm_host *nlmclnt_init(const struct nlmclnt_initdata *nlm_init)
{
	struct nlm_host *host;
	u32 nlm_version = (nlm_init->nfs_version == 2) ? 1 : 4;
	int status;

	status = lockd_up();
	if (status < 0)
		return ERR_PTR(status);

	host = nlmclnt_lookup_host(nlm_init->address, nlm_init->addrlen,
				   nlm_init->protocol, nlm_version,
				   nlm_init->hostname, nlm_init->noresvport,
				   nlm_init->net);
	if (host == NULL) {
		lockd_down();
		return ERR_PTR(-ENOLCK);
	}

	return host;
}
EXPORT_SYMBOL_GPL(nlmclnt_init);

void nlmclnt_done(struct nlm_host *host)
{
	nlmclnt_release_host(host);
	lockd_down();
}
EXPORT_SYMBOL_GPL(nlmclnt_done);

struct nlm_wait *nlmclnt_prepare_block(struct nlm_host *host, struct file_lock *fl)
{
	struct nlm_wait *block;

	block = kmalloc(sizeof(*block), GFP_KERNEL);
	if (block != NULL) {
		block->b_host = host;
		block->b_lock = fl;
		init_waitqueue_head(&block->b_wait);
		block->b_status = nlm_lck_blocked;

		spin_lock(&nlm_blocked_lock);
		list_add(&block->b_list, &nlm_blocked);
		spin_unlock(&nlm_blocked_lock);
	}
	return block;
}

void nlmclnt_finish_block(struct nlm_wait *block)
{
	if (block == NULL)
		return;
	spin_lock(&nlm_blocked_lock);
	list_del(&block->b_list);
	spin_unlock(&nlm_blocked_lock);
	kfree(block);
}

int nlmclnt_block(struct nlm_wait *block, struct nlm_rqst *req, long timeout)
{
	long ret;

	if (block == NULL)
		return -EAGAIN;

	ret = wait_event_interruptible_timeout(block->b_wait,
			block->b_status != nlm_lck_blocked,
			timeout);
	if (ret < 0)
		return -ERESTARTSYS;
	req->a_res.status = block->b_status;
	return 0;
}

__be32 nlmclnt_grant(const struct sockaddr *addr, const struct nlm_lock *lock)
{
	const struct file_lock *fl = &lock->fl;
	const struct nfs_fh *fh = &lock->fh;
	struct nlm_wait	*block;
	__be32 res = nlm_lck_denied;

	spin_lock(&nlm_blocked_lock);
	list_for_each_entry(block, &nlm_blocked, b_list) {
		struct file_lock *fl_blocked = block->b_lock;

		if (fl_blocked->fl_start != fl->fl_start)
			continue;
		if (fl_blocked->fl_end != fl->fl_end)
			continue;
		if (fl_blocked->fl_u.nfs_fl.owner->pid != lock->svid)
			continue;
		if (!rpc_cmp_addr(nlm_addr(block->b_host), addr))
			continue;
		if (nfs_compare_fh(NFS_FH(fl_blocked->fl_file->f_path.dentry->d_inode) ,fh) != 0)
			continue;
		block->b_status = nlm_granted;
		wake_up(&block->b_wait);
		res = nlm_granted;
	}
	spin_unlock(&nlm_blocked_lock);
	return res;
}


void
nlmclnt_recovery(struct nlm_host *host)
{
	struct task_struct *task;

	if (!host->h_reclaiming++) {
		nlm_get_host(host);
		task = kthread_run(reclaimer, host, "%s-reclaim", host->h_name);
		if (IS_ERR(task))
			printk(KERN_ERR "lockd: unable to spawn reclaimer "
				"thread. Locks for %s won't be reclaimed! "
				"(%ld)\n", host->h_name, PTR_ERR(task));
	}
}

static int
reclaimer(void *ptr)
{
	struct nlm_host	  *host = (struct nlm_host *) ptr;
	struct nlm_wait	  *block;
	struct file_lock *fl, *next;
	u32 nsmstate;

	allow_signal(SIGKILL);

	down_write(&host->h_rwsem);
	lockd_up();	

	dprintk("lockd: reclaiming locks for host %s\n", host->h_name);

restart:
	nsmstate = host->h_nsmstate;

	host->h_nextrebind = jiffies;
	nlm_rebind_host(host);

	
	list_splice_init(&host->h_granted, &host->h_reclaim);
	list_for_each_entry_safe(fl, next, &host->h_reclaim, fl_u.nfs_fl.list) {
		list_del_init(&fl->fl_u.nfs_fl.list);

		if (signalled())
			continue;
		if (nlmclnt_reclaim(host, fl) != 0)
			continue;
		list_add_tail(&fl->fl_u.nfs_fl.list, &host->h_granted);
		if (host->h_nsmstate != nsmstate) {
			
			goto restart;
		}
	}

	host->h_reclaiming = 0;
	up_write(&host->h_rwsem);
	dprintk("NLM: done reclaiming locks for host %s\n", host->h_name);

	
	spin_lock(&nlm_blocked_lock);
	list_for_each_entry(block, &nlm_blocked, b_list) {
		if (block->b_host == host) {
			block->b_status = nlm_lck_denied_grace_period;
			wake_up(&block->b_wait);
		}
	}
	spin_unlock(&nlm_blocked_lock);

	
	nlmclnt_release_host(host);
	lockd_down();
	return 0;
}
