/*
 *  inode.c - securityfs
 *
 *  Copyright (C) 2005 Greg Kroah-Hartman <gregkh@suse.de>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License version
 *	2 as published by the Free Software Foundation.
 *
 *  Based on fs/debugfs/inode.c which had the following copyright notice:
 *    Copyright (C) 2004 Greg Kroah-Hartman <greg@kroah.com>
 *    Copyright (C) 2004 IBM Inc.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/mount.h>
#include <linux/pagemap.h>
#include <linux/init.h>
#include <linux/namei.h>
#include <linux/security.h>
#include <linux/magic.h>

static struct vfsmount *mount;
static int mount_count;

static inline int positive(struct dentry *dentry)
{
	return dentry->d_inode && !d_unhashed(dentry);
}

static int fill_super(struct super_block *sb, void *data, int silent)
{
	static struct tree_descr files[] = {{""}};

	return simple_fill_super(sb, SECURITYFS_MAGIC, files);
}

static struct dentry *get_sb(struct file_system_type *fs_type,
		  int flags, const char *dev_name,
		  void *data)
{
	return mount_single(fs_type, flags, data, fill_super);
}

static struct file_system_type fs_type = {
	.owner =	THIS_MODULE,
	.name =		"securityfs",
	.mount =	get_sb,
	.kill_sb =	kill_litter_super,
};

struct dentry *securityfs_create_file(const char *name, umode_t mode,
				   struct dentry *parent, void *data,
				   const struct file_operations *fops)
{
	struct dentry *dentry;
	int is_dir = S_ISDIR(mode);
	struct inode *dir, *inode;
	int error;

	if (!is_dir) {
		BUG_ON(!fops);
		mode = (mode & S_IALLUGO) | S_IFREG;
	}

	pr_debug("securityfs: creating file '%s'\n",name);

	error = simple_pin_fs(&fs_type, &mount, &mount_count);
	if (error)
		return ERR_PTR(error);

	if (!parent)
		parent = mount->mnt_root;

	dir = parent->d_inode;

	mutex_lock(&dir->i_mutex);
	dentry = lookup_one_len(name, parent, strlen(name));
	if (IS_ERR(dentry))
		goto out;

	if (dentry->d_inode) {
		error = -EEXIST;
		goto out1;
	}

	inode = new_inode(dir->i_sb);
	if (!inode) {
		error = -ENOMEM;
		goto out1;
	}

	inode->i_ino = get_next_ino();
	inode->i_mode = mode;
	inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
	inode->i_private = data;
	if (is_dir) {
		inode->i_op = &simple_dir_inode_operations;
		inode->i_fop = &simple_dir_operations;
		inc_nlink(inode);
		inc_nlink(dir);
	} else {
		inode->i_fop = fops;
	}
	d_instantiate(dentry, inode);
	dget(dentry);
	mutex_unlock(&dir->i_mutex);
	return dentry;

out1:
	dput(dentry);
	dentry = ERR_PTR(error);
out:
	mutex_unlock(&dir->i_mutex);
	simple_release_fs(&mount, &mount_count);
	return dentry;
}
EXPORT_SYMBOL_GPL(securityfs_create_file);

struct dentry *securityfs_create_dir(const char *name, struct dentry *parent)
{
	return securityfs_create_file(name,
				      S_IFDIR | S_IRWXU | S_IRUGO | S_IXUGO,
				      parent, NULL, NULL);
}
EXPORT_SYMBOL_GPL(securityfs_create_dir);

void securityfs_remove(struct dentry *dentry)
{
	struct dentry *parent;

	if (!dentry || IS_ERR(dentry))
		return;

	parent = dentry->d_parent;
	if (!parent || !parent->d_inode)
		return;

	mutex_lock(&parent->d_inode->i_mutex);
	if (positive(dentry)) {
		if (dentry->d_inode) {
			if (S_ISDIR(dentry->d_inode->i_mode))
				simple_rmdir(parent->d_inode, dentry);
			else
				simple_unlink(parent->d_inode, dentry);
			dput(dentry);
		}
	}
	mutex_unlock(&parent->d_inode->i_mutex);
	simple_release_fs(&mount, &mount_count);
}
EXPORT_SYMBOL_GPL(securityfs_remove);

static struct kobject *security_kobj;

static int __init securityfs_init(void)
{
	int retval;

	security_kobj = kobject_create_and_add("security", kernel_kobj);
	if (!security_kobj)
		return -EINVAL;

	retval = register_filesystem(&fs_type);
	if (retval)
		kobject_put(security_kobj);
	return retval;
}

core_initcall(securityfs_init);
MODULE_LICENSE("GPL");

