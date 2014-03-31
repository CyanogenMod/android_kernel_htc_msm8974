#include <linux/file.h>
#include <linux/fs.h>
#include <linux/export.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/slab.h>

#include <asm/uaccess.h>

#include "spufs.h"

static long do_spu_run(struct file *filp,
			__u32 __user *unpc,
			__u32 __user *ustatus)
{
	long ret;
	struct spufs_inode_info *i;
	u32 npc, status;

	ret = -EFAULT;
	if (get_user(npc, unpc))
		goto out;

	
	ret = -EINVAL;
	if (filp->f_op != &spufs_context_fops)
		goto out;

	i = SPUFS_I(filp->f_path.dentry->d_inode);
	ret = spufs_run_spu(i->i_ctx, &npc, &status);

	if (put_user(npc, unpc))
		ret = -EFAULT;

	if (ustatus && put_user(status, ustatus))
		ret = -EFAULT;
out:
	return ret;
}

static long do_spu_create(const char __user *pathname, unsigned int flags,
		umode_t mode, struct file *neighbor)
{
	struct path path;
	struct dentry *dentry;
	int ret;

	dentry = user_path_create(AT_FDCWD, pathname, &path, 1);
	ret = PTR_ERR(dentry);
	if (!IS_ERR(dentry)) {
		ret = spufs_create(&path, dentry, flags, mode, neighbor);
		path_put(&path);
	}

	return ret;
}

struct spufs_calls spufs_calls = {
	.create_thread = do_spu_create,
	.spu_run = do_spu_run,
	.coredump_extra_notes_size = spufs_coredump_extra_notes_size,
	.coredump_extra_notes_write = spufs_coredump_extra_notes_write,
	.notify_spus_active = do_notify_spus_active,
	.owner = THIS_MODULE,
};
