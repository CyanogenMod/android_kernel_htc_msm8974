/*
 * security/tomoyo/tomoyo.c
 *
 * Copyright (C) 2005-2011  NTT DATA CORPORATION
 */

#include <linux/security.h>
#include "common.h"

static int tomoyo_cred_alloc_blank(struct cred *new, gfp_t gfp)
{
	new->security = NULL;
	return 0;
}

static int tomoyo_cred_prepare(struct cred *new, const struct cred *old,
			       gfp_t gfp)
{
	struct tomoyo_domain_info *domain = old->security;
	new->security = domain;
	if (domain)
		atomic_inc(&domain->users);
	return 0;
}

static void tomoyo_cred_transfer(struct cred *new, const struct cred *old)
{
	tomoyo_cred_prepare(new, old, 0);
}

static void tomoyo_cred_free(struct cred *cred)
{
	struct tomoyo_domain_info *domain = cred->security;
	if (domain)
		atomic_dec(&domain->users);
}

static int tomoyo_bprm_set_creds(struct linux_binprm *bprm)
{
	int rc;

	rc = cap_bprm_set_creds(bprm);
	if (rc)
		return rc;

	if (bprm->cred_prepared)
		return 0;
#ifndef CONFIG_SECURITY_TOMOYO_OMIT_USERSPACE_LOADER
	if (!tomoyo_policy_loaded)
		tomoyo_load_policy(bprm->filename);
#endif
	atomic_dec(&((struct tomoyo_domain_info *)
		     bprm->cred->security)->users);
	bprm->cred->security = NULL;
	return 0;
}

static int tomoyo_bprm_check_security(struct linux_binprm *bprm)
{
	struct tomoyo_domain_info *domain = bprm->cred->security;

	if (!domain) {
		const int idx = tomoyo_read_lock();
		const int err = tomoyo_find_next_domain(bprm);
		tomoyo_read_unlock(idx);
		return err;
	}
	return tomoyo_check_open_permission(domain, &bprm->file->f_path,
					    O_RDONLY);
}

static int tomoyo_inode_getattr(struct vfsmount *mnt, struct dentry *dentry)
{
	struct path path = { mnt, dentry };
	return tomoyo_path_perm(TOMOYO_TYPE_GETATTR, &path, NULL);
}

static int tomoyo_path_truncate(struct path *path)
{
	return tomoyo_path_perm(TOMOYO_TYPE_TRUNCATE, path, NULL);
}

static int tomoyo_path_unlink(struct path *parent, struct dentry *dentry)
{
	struct path path = { parent->mnt, dentry };
	return tomoyo_path_perm(TOMOYO_TYPE_UNLINK, &path, NULL);
}

static int tomoyo_path_mkdir(struct path *parent, struct dentry *dentry,
			     umode_t mode)
{
	struct path path = { parent->mnt, dentry };
	return tomoyo_path_number_perm(TOMOYO_TYPE_MKDIR, &path,
				       mode & S_IALLUGO);
}

static int tomoyo_path_rmdir(struct path *parent, struct dentry *dentry)
{
	struct path path = { parent->mnt, dentry };
	return tomoyo_path_perm(TOMOYO_TYPE_RMDIR, &path, NULL);
}

static int tomoyo_path_symlink(struct path *parent, struct dentry *dentry,
			       const char *old_name)
{
	struct path path = { parent->mnt, dentry };
	return tomoyo_path_perm(TOMOYO_TYPE_SYMLINK, &path, old_name);
}

static int tomoyo_path_mknod(struct path *parent, struct dentry *dentry,
			     umode_t mode, unsigned int dev)
{
	struct path path = { parent->mnt, dentry };
	int type = TOMOYO_TYPE_CREATE;
	const unsigned int perm = mode & S_IALLUGO;

	switch (mode & S_IFMT) {
	case S_IFCHR:
		type = TOMOYO_TYPE_MKCHAR;
		break;
	case S_IFBLK:
		type = TOMOYO_TYPE_MKBLOCK;
		break;
	default:
		goto no_dev;
	}
	return tomoyo_mkdev_perm(type, &path, perm, dev);
 no_dev:
	switch (mode & S_IFMT) {
	case S_IFIFO:
		type = TOMOYO_TYPE_MKFIFO;
		break;
	case S_IFSOCK:
		type = TOMOYO_TYPE_MKSOCK;
		break;
	}
	return tomoyo_path_number_perm(type, &path, perm);
}

static int tomoyo_path_link(struct dentry *old_dentry, struct path *new_dir,
			    struct dentry *new_dentry)
{
	struct path path1 = { new_dir->mnt, old_dentry };
	struct path path2 = { new_dir->mnt, new_dentry };
	return tomoyo_path2_perm(TOMOYO_TYPE_LINK, &path1, &path2);
}

static int tomoyo_path_rename(struct path *old_parent,
			      struct dentry *old_dentry,
			      struct path *new_parent,
			      struct dentry *new_dentry)
{
	struct path path1 = { old_parent->mnt, old_dentry };
	struct path path2 = { new_parent->mnt, new_dentry };
	return tomoyo_path2_perm(TOMOYO_TYPE_RENAME, &path1, &path2);
}

static int tomoyo_file_fcntl(struct file *file, unsigned int cmd,
			     unsigned long arg)
{
	if (!(cmd == F_SETFL && ((arg ^ file->f_flags) & O_APPEND)))
		return 0;
	return tomoyo_check_open_permission(tomoyo_domain(), &file->f_path,
					    O_WRONLY | (arg & O_APPEND));
}

static int tomoyo_dentry_open(struct file *f, const struct cred *cred)
{
	int flags = f->f_flags;
	
	if (current->in_execve)
		return 0;
	return tomoyo_check_open_permission(tomoyo_domain(), &f->f_path, flags);
}

static int tomoyo_file_ioctl(struct file *file, unsigned int cmd,
			     unsigned long arg)
{
	return tomoyo_path_number_perm(TOMOYO_TYPE_IOCTL, &file->f_path, cmd);
}

static int tomoyo_path_chmod(struct path *path, umode_t mode)
{
	return tomoyo_path_number_perm(TOMOYO_TYPE_CHMOD, path,
				       mode & S_IALLUGO);
}

static int tomoyo_path_chown(struct path *path, uid_t uid, gid_t gid)
{
	int error = 0;
	if (uid != (uid_t) -1)
		error = tomoyo_path_number_perm(TOMOYO_TYPE_CHOWN, path, uid);
	if (!error && gid != (gid_t) -1)
		error = tomoyo_path_number_perm(TOMOYO_TYPE_CHGRP, path, gid);
	return error;
}

static int tomoyo_path_chroot(struct path *path)
{
	return tomoyo_path_perm(TOMOYO_TYPE_CHROOT, path, NULL);
}

static int tomoyo_sb_mount(char *dev_name, struct path *path,
			   char *type, unsigned long flags, void *data)
{
	return tomoyo_mount_permission(dev_name, path, type, flags, data);
}

static int tomoyo_sb_umount(struct vfsmount *mnt, int flags)
{
	struct path path = { mnt, mnt->mnt_root };
	return tomoyo_path_perm(TOMOYO_TYPE_UMOUNT, &path, NULL);
}

static int tomoyo_sb_pivotroot(struct path *old_path, struct path *new_path)
{
	return tomoyo_path2_perm(TOMOYO_TYPE_PIVOT_ROOT, new_path, old_path);
}

static int tomoyo_socket_listen(struct socket *sock, int backlog)
{
	return tomoyo_socket_listen_permission(sock);
}

static int tomoyo_socket_connect(struct socket *sock, struct sockaddr *addr,
				 int addr_len)
{
	return tomoyo_socket_connect_permission(sock, addr, addr_len);
}

static int tomoyo_socket_bind(struct socket *sock, struct sockaddr *addr,
			      int addr_len)
{
	return tomoyo_socket_bind_permission(sock, addr, addr_len);
}

static int tomoyo_socket_sendmsg(struct socket *sock, struct msghdr *msg,
				 int size)
{
	return tomoyo_socket_sendmsg_permission(sock, msg, size);
}

static struct security_operations tomoyo_security_ops = {
	.name                = "tomoyo",
	.cred_alloc_blank    = tomoyo_cred_alloc_blank,
	.cred_prepare        = tomoyo_cred_prepare,
	.cred_transfer	     = tomoyo_cred_transfer,
	.cred_free           = tomoyo_cred_free,
	.bprm_set_creds      = tomoyo_bprm_set_creds,
	.bprm_check_security = tomoyo_bprm_check_security,
	.file_fcntl          = tomoyo_file_fcntl,
	.dentry_open         = tomoyo_dentry_open,
	.path_truncate       = tomoyo_path_truncate,
	.path_unlink         = tomoyo_path_unlink,
	.path_mkdir          = tomoyo_path_mkdir,
	.path_rmdir          = tomoyo_path_rmdir,
	.path_symlink        = tomoyo_path_symlink,
	.path_mknod          = tomoyo_path_mknod,
	.path_link           = tomoyo_path_link,
	.path_rename         = tomoyo_path_rename,
	.inode_getattr       = tomoyo_inode_getattr,
	.file_ioctl          = tomoyo_file_ioctl,
	.path_chmod          = tomoyo_path_chmod,
	.path_chown          = tomoyo_path_chown,
	.path_chroot         = tomoyo_path_chroot,
	.sb_mount            = tomoyo_sb_mount,
	.sb_umount           = tomoyo_sb_umount,
	.sb_pivotroot        = tomoyo_sb_pivotroot,
	.socket_bind         = tomoyo_socket_bind,
	.socket_connect      = tomoyo_socket_connect,
	.socket_listen       = tomoyo_socket_listen,
	.socket_sendmsg      = tomoyo_socket_sendmsg,
};

struct srcu_struct tomoyo_ss;

static int __init tomoyo_init(void)
{
	struct cred *cred = (struct cred *) current_cred();

	if (!security_module_enable(&tomoyo_security_ops))
		return 0;
	
	if (register_security(&tomoyo_security_ops) ||
	    init_srcu_struct(&tomoyo_ss))
		panic("Failure registering TOMOYO Linux");
	printk(KERN_INFO "TOMOYO Linux initialized\n");
	cred->security = &tomoyo_kernel_domain;
	tomoyo_mm_init();
	return 0;
}

security_initcall(tomoyo_init);
