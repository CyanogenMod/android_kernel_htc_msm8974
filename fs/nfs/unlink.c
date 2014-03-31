
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/dcache.h>
#include <linux/sunrpc/sched.h>
#include <linux/sunrpc/clnt.h>
#include <linux/nfs_fs.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/namei.h>

#include "internal.h"
#include "nfs4_fs.h"
#include "iostat.h"
#include "delegation.h"

static void
nfs_free_unlinkdata(struct nfs_unlinkdata *data)
{
	iput(data->dir);
	put_rpccred(data->cred);
	kfree(data->args.name.name);
	kfree(data);
}

#define NAME_ALLOC_LEN(len)	((len+16) & ~15)
static int nfs_copy_dname(struct dentry *dentry, struct nfs_unlinkdata *data)
{
	char		*str;
	int		len = dentry->d_name.len;

	str = kmemdup(dentry->d_name.name, NAME_ALLOC_LEN(len), GFP_KERNEL);
	if (!str)
		return -ENOMEM;
	data->args.name.len = len;
	data->args.name.name = str;
	return 0;
}

static void nfs_free_dname(struct nfs_unlinkdata *data)
{
	kfree(data->args.name.name);
	data->args.name.name = NULL;
	data->args.name.len = 0;
}

static void nfs_dec_sillycount(struct inode *dir)
{
	struct nfs_inode *nfsi = NFS_I(dir);
	if (atomic_dec_return(&nfsi->silly_count) == 1)
		wake_up(&nfsi->waitqueue);
}

static void nfs_async_unlink_done(struct rpc_task *task, void *calldata)
{
	struct nfs_unlinkdata *data = calldata;
	struct inode *dir = data->dir;

	if (!NFS_PROTO(dir)->unlink_done(task, dir))
		rpc_restart_call_prepare(task);
}

static void nfs_async_unlink_release(void *calldata)
{
	struct nfs_unlinkdata	*data = calldata;
	struct super_block *sb = data->dir->i_sb;

	nfs_dec_sillycount(data->dir);
	nfs_free_unlinkdata(data);
	nfs_sb_deactive(sb);
}

static void nfs_unlink_prepare(struct rpc_task *task, void *calldata)
{
	struct nfs_unlinkdata *data = calldata;
	NFS_PROTO(data->dir)->unlink_rpc_prepare(task, data);
}

static const struct rpc_call_ops nfs_unlink_ops = {
	.rpc_call_done = nfs_async_unlink_done,
	.rpc_release = nfs_async_unlink_release,
	.rpc_call_prepare = nfs_unlink_prepare,
};

static int nfs_do_call_unlink(struct dentry *parent, struct inode *dir, struct nfs_unlinkdata *data)
{
	struct rpc_message msg = {
		.rpc_argp = &data->args,
		.rpc_resp = &data->res,
		.rpc_cred = data->cred,
	};
	struct rpc_task_setup task_setup_data = {
		.rpc_message = &msg,
		.callback_ops = &nfs_unlink_ops,
		.callback_data = data,
		.workqueue = nfsiod_workqueue,
		.flags = RPC_TASK_ASYNC,
	};
	struct rpc_task *task;
	struct dentry *alias;

	alias = d_lookup(parent, &data->args.name);
	if (alias != NULL) {
		int ret;
		void *devname_garbage = NULL;

		nfs_free_dname(data);
		ret = nfs_copy_dname(alias, data);
		spin_lock(&alias->d_lock);
		if (ret == 0 && alias->d_inode != NULL &&
		    !(alias->d_flags & DCACHE_NFSFS_RENAMED)) {
			devname_garbage = alias->d_fsdata;
			alias->d_fsdata = data;
			alias->d_flags |= DCACHE_NFSFS_RENAMED;
			ret = 1;
		} else
			ret = 0;
		spin_unlock(&alias->d_lock);
		nfs_dec_sillycount(dir);
		dput(alias);
		kfree(devname_garbage);
		return ret;
	}
	data->dir = igrab(dir);
	if (!data->dir) {
		nfs_dec_sillycount(dir);
		return 0;
	}
	nfs_sb_active(dir->i_sb);
	data->args.fh = NFS_FH(dir);
	nfs_fattr_init(data->res.dir_attr);

	NFS_PROTO(dir)->unlink_setup(&msg, dir);

	task_setup_data.rpc_client = NFS_CLIENT(dir);
	task = rpc_run_task(&task_setup_data);
	if (!IS_ERR(task))
		rpc_put_task_async(task);
	return 1;
}

static int nfs_call_unlink(struct dentry *dentry, struct nfs_unlinkdata *data)
{
	struct dentry *parent;
	struct inode *dir;
	int ret = 0;


	parent = dget_parent(dentry);
	if (parent == NULL)
		goto out_free;
	dir = parent->d_inode;
	
	spin_lock(&dir->i_lock);
	if (atomic_inc_not_zero(&NFS_I(dir)->silly_count) == 0) {
		
		hlist_add_head(&data->list, &NFS_I(dir)->silly_list);
		spin_unlock(&dir->i_lock);
		ret = 1;
		goto out_dput;
	}
	spin_unlock(&dir->i_lock);
	ret = nfs_do_call_unlink(parent, dir, data);
out_dput:
	dput(parent);
out_free:
	return ret;
}

void nfs_block_sillyrename(struct dentry *dentry)
{
	struct nfs_inode *nfsi = NFS_I(dentry->d_inode);

	wait_event(nfsi->waitqueue, atomic_cmpxchg(&nfsi->silly_count, 1, 0) == 1);
}

void nfs_unblock_sillyrename(struct dentry *dentry)
{
	struct inode *dir = dentry->d_inode;
	struct nfs_inode *nfsi = NFS_I(dir);
	struct nfs_unlinkdata *data;

	atomic_inc(&nfsi->silly_count);
	spin_lock(&dir->i_lock);
	while (!hlist_empty(&nfsi->silly_list)) {
		if (!atomic_inc_not_zero(&nfsi->silly_count))
			break;
		data = hlist_entry(nfsi->silly_list.first, struct nfs_unlinkdata, list);
		hlist_del(&data->list);
		spin_unlock(&dir->i_lock);
		if (nfs_do_call_unlink(dentry, dir, data) == 0)
			nfs_free_unlinkdata(data);
		spin_lock(&dir->i_lock);
	}
	spin_unlock(&dir->i_lock);
}

static int
nfs_async_unlink(struct inode *dir, struct dentry *dentry)
{
	struct nfs_unlinkdata *data;
	int status = -ENOMEM;
	void *devname_garbage = NULL;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (data == NULL)
		goto out;

	data->cred = rpc_lookup_cred();
	if (IS_ERR(data->cred)) {
		status = PTR_ERR(data->cred);
		goto out_free;
	}
	data->res.dir_attr = &data->dir_attr;

	status = -EBUSY;
	spin_lock(&dentry->d_lock);
	if (dentry->d_flags & DCACHE_NFSFS_RENAMED)
		goto out_unlock;
	dentry->d_flags |= DCACHE_NFSFS_RENAMED;
	devname_garbage = dentry->d_fsdata;
	dentry->d_fsdata = data;
	spin_unlock(&dentry->d_lock);
	if (devname_garbage)
		kfree(devname_garbage);
	return 0;
out_unlock:
	spin_unlock(&dentry->d_lock);
	put_rpccred(data->cred);
out_free:
	kfree(data);
out:
	return status;
}

void
nfs_complete_unlink(struct dentry *dentry, struct inode *inode)
{
	struct nfs_unlinkdata	*data = NULL;

	spin_lock(&dentry->d_lock);
	if (dentry->d_flags & DCACHE_NFSFS_RENAMED) {
		dentry->d_flags &= ~DCACHE_NFSFS_RENAMED;
		data = dentry->d_fsdata;
		dentry->d_fsdata = NULL;
	}
	spin_unlock(&dentry->d_lock);

	if (data != NULL && (NFS_STALE(inode) || !nfs_call_unlink(dentry, data)))
		nfs_free_unlinkdata(data);
}

static void
nfs_cancel_async_unlink(struct dentry *dentry)
{
	spin_lock(&dentry->d_lock);
	if (dentry->d_flags & DCACHE_NFSFS_RENAMED) {
		struct nfs_unlinkdata *data = dentry->d_fsdata;

		dentry->d_flags &= ~DCACHE_NFSFS_RENAMED;
		dentry->d_fsdata = NULL;
		spin_unlock(&dentry->d_lock);
		nfs_free_unlinkdata(data);
		return;
	}
	spin_unlock(&dentry->d_lock);
}

static void nfs_async_rename_done(struct rpc_task *task, void *calldata)
{
	struct nfs_renamedata *data = calldata;
	struct inode *old_dir = data->old_dir;
	struct inode *new_dir = data->new_dir;
	struct dentry *old_dentry = data->old_dentry;
	struct dentry *new_dentry = data->new_dentry;

	if (!NFS_PROTO(old_dir)->rename_done(task, old_dir, new_dir)) {
		rpc_restart_call_prepare(task);
		return;
	}

	if (task->tk_status != 0) {
		nfs_cancel_async_unlink(old_dentry);
		return;
	}

	d_drop(old_dentry);
	d_drop(new_dentry);
}

static void nfs_async_rename_release(void *calldata)
{
	struct nfs_renamedata	*data = calldata;
	struct super_block *sb = data->old_dir->i_sb;

	if (data->old_dentry->d_inode)
		nfs_mark_for_revalidate(data->old_dentry->d_inode);

	dput(data->old_dentry);
	dput(data->new_dentry);
	iput(data->old_dir);
	iput(data->new_dir);
	nfs_sb_deactive(sb);
	put_rpccred(data->cred);
	kfree(data);
}

static void nfs_rename_prepare(struct rpc_task *task, void *calldata)
{
	struct nfs_renamedata *data = calldata;
	NFS_PROTO(data->old_dir)->rename_rpc_prepare(task, data);
}

static const struct rpc_call_ops nfs_rename_ops = {
	.rpc_call_done = nfs_async_rename_done,
	.rpc_release = nfs_async_rename_release,
	.rpc_call_prepare = nfs_rename_prepare,
};

static struct rpc_task *
nfs_async_rename(struct inode *old_dir, struct inode *new_dir,
		 struct dentry *old_dentry, struct dentry *new_dentry)
{
	struct nfs_renamedata *data;
	struct rpc_message msg = { };
	struct rpc_task_setup task_setup_data = {
		.rpc_message = &msg,
		.callback_ops = &nfs_rename_ops,
		.workqueue = nfsiod_workqueue,
		.rpc_client = NFS_CLIENT(old_dir),
		.flags = RPC_TASK_ASYNC,
	};

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (data == NULL)
		return ERR_PTR(-ENOMEM);
	task_setup_data.callback_data = data;

	data->cred = rpc_lookup_cred();
	if (IS_ERR(data->cred)) {
		struct rpc_task *task = ERR_CAST(data->cred);
		kfree(data);
		return task;
	}

	msg.rpc_argp = &data->args;
	msg.rpc_resp = &data->res;
	msg.rpc_cred = data->cred;

	
	data->old_dir = old_dir;
	ihold(old_dir);
	data->new_dir = new_dir;
	ihold(new_dir);
	data->old_dentry = dget(old_dentry);
	data->new_dentry = dget(new_dentry);
	nfs_fattr_init(&data->old_fattr);
	nfs_fattr_init(&data->new_fattr);

	
	data->args.old_dir = NFS_FH(old_dir);
	data->args.old_name = &old_dentry->d_name;
	data->args.new_dir = NFS_FH(new_dir);
	data->args.new_name = &new_dentry->d_name;

	
	data->res.old_fattr = &data->old_fattr;
	data->res.new_fattr = &data->new_fattr;

	nfs_sb_active(old_dir->i_sb);

	NFS_PROTO(data->old_dir)->rename_setup(&msg, old_dir);

	return rpc_run_task(&task_setup_data);
}

int
nfs_sillyrename(struct inode *dir, struct dentry *dentry)
{
	static unsigned int sillycounter;
	const int      fileidsize  = sizeof(NFS_FILEID(dentry->d_inode))*2;
	const int      countersize = sizeof(sillycounter)*2;
	const int      slen        = sizeof(".nfs")+fileidsize+countersize-1;
	char           silly[slen+1];
	struct dentry *sdentry;
	struct rpc_task *task;
	int            error = -EIO;

	dfprintk(VFS, "NFS: silly-rename(%s/%s, ct=%d)\n",
		dentry->d_parent->d_name.name, dentry->d_name.name,
		dentry->d_count);
	nfs_inc_stats(dir, NFSIOS_SILLYRENAME);

	error = -EBUSY;
	if (dentry->d_flags & DCACHE_NFSFS_RENAMED)
		goto out;

	sprintf(silly, ".nfs%*.*Lx",
		fileidsize, fileidsize,
		(unsigned long long)NFS_FILEID(dentry->d_inode));

	
	nfs_inode_return_delegation(dentry->d_inode);

	sdentry = NULL;
	do {
		char *suffix = silly + slen - countersize;

		dput(sdentry);
		sillycounter++;
		sprintf(suffix, "%*.*x", countersize, countersize, sillycounter);

		dfprintk(VFS, "NFS: trying to rename %s to %s\n",
				dentry->d_name.name, silly);

		sdentry = lookup_one_len(silly, dentry->d_parent, slen);
		if (IS_ERR(sdentry))
			goto out;
	} while (sdentry->d_inode != NULL); 

	error = nfs_async_unlink(dir, dentry);
	if (error)
		goto out_dput;

	
	error = nfs_copy_dname(sdentry,
				(struct nfs_unlinkdata *)dentry->d_fsdata);
	if (error) {
		nfs_cancel_async_unlink(dentry);
		goto out_dput;
	}

	
	task = nfs_async_rename(dir, dir, dentry, sdentry);
	if (IS_ERR(task)) {
		error = -EBUSY;
		nfs_cancel_async_unlink(dentry);
		goto out_dput;
	}

	
	error = rpc_wait_for_completion_task(task);
	if (error == 0)
		error = task->tk_status;
	rpc_put_task(task);
out_dput:
	dput(sdentry);
out:
	return error;
}
