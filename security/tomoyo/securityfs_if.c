/*
 * security/tomoyo/securityfs_if.c
 *
 * Copyright (C) 2005-2011  NTT DATA CORPORATION
 */

#include <linux/security.h>
#include "common.h"

static bool tomoyo_check_task_acl(struct tomoyo_request_info *r,
				  const struct tomoyo_acl_info *ptr)
{
	const struct tomoyo_task_acl *acl = container_of(ptr, typeof(*acl),
							 head);
	return !tomoyo_pathcmp(r->param.task.domainname, acl->domainname);
}

static ssize_t tomoyo_write_self(struct file *file, const char __user *buf,
			      size_t count, loff_t *ppos)
{
	char *data;
	int error;
	if (!count || count >= TOMOYO_EXEC_TMPSIZE - 10)
		return -ENOMEM;
	data = kzalloc(count + 1, GFP_NOFS);
	if (!data)
		return -ENOMEM;
	if (copy_from_user(data, buf, count)) {
		error = -EFAULT;
		goto out;
	}
	tomoyo_normalize_line(data);
	if (tomoyo_correct_domain(data)) {
		const int idx = tomoyo_read_lock();
		struct tomoyo_path_info name;
		struct tomoyo_request_info r;
		name.name = data;
		tomoyo_fill_path_info(&name);
		
		tomoyo_init_request_info(&r, NULL, TOMOYO_MAC_FILE_EXECUTE);
		r.param_type = TOMOYO_TYPE_MANUAL_TASK_ACL;
		r.param.task.domainname = &name;
		tomoyo_check_acl(&r, tomoyo_check_task_acl);
		if (!r.granted)
			error = -EPERM;
		else {
			struct tomoyo_domain_info *new_domain =
				tomoyo_assign_domain(data, true);
			if (!new_domain) {
				error = -ENOENT;
			} else {
				struct cred *cred = prepare_creds();
				if (!cred) {
					error = -ENOMEM;
				} else {
					struct tomoyo_domain_info *old_domain =
						cred->security;
					cred->security = new_domain;
					atomic_inc(&new_domain->users);
					atomic_dec(&old_domain->users);
					commit_creds(cred);
					error = 0;
				}
			}
		}
		tomoyo_read_unlock(idx);
	} else
		error = -EINVAL;
out:
	kfree(data);
	return error ? error : count;
}

static ssize_t tomoyo_read_self(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	const char *domain = tomoyo_domain()->domainname->name;
	loff_t len = strlen(domain);
	loff_t pos = *ppos;
	if (pos >= len || !count)
		return 0;
	len -= pos;
	if (count < len)
		len = count;
	if (copy_to_user(buf, domain + pos, len))
		return -EFAULT;
	*ppos += len;
	return len;
}

static const struct file_operations tomoyo_self_operations = {
	.write = tomoyo_write_self,
	.read  = tomoyo_read_self,
};

static int tomoyo_open(struct inode *inode, struct file *file)
{
	const int key = ((u8 *) file->f_path.dentry->d_inode->i_private)
		- ((u8 *) NULL);
	return tomoyo_open_control(key, file);
}

static int tomoyo_release(struct inode *inode, struct file *file)
{
	return tomoyo_close_control(file->private_data);
}

static unsigned int tomoyo_poll(struct file *file, poll_table *wait)
{
	return tomoyo_poll_control(file, wait);
}

static ssize_t tomoyo_read(struct file *file, char __user *buf, size_t count,
			   loff_t *ppos)
{
	return tomoyo_read_control(file->private_data, buf, count);
}

static ssize_t tomoyo_write(struct file *file, const char __user *buf,
			    size_t count, loff_t *ppos)
{
	return tomoyo_write_control(file->private_data, buf, count);
}

static const struct file_operations tomoyo_operations = {
	.open    = tomoyo_open,
	.release = tomoyo_release,
	.poll    = tomoyo_poll,
	.read    = tomoyo_read,
	.write   = tomoyo_write,
	.llseek  = noop_llseek,
};

static void __init tomoyo_create_entry(const char *name, const umode_t mode,
				       struct dentry *parent, const u8 key)
{
	securityfs_create_file(name, mode, parent, ((u8 *) NULL) + key,
			       &tomoyo_operations);
}

static int __init tomoyo_initerface_init(void)
{
	struct dentry *tomoyo_dir;

	
	if (current_cred()->security != &tomoyo_kernel_domain)
		return 0;

	tomoyo_dir = securityfs_create_dir("tomoyo", NULL);
	tomoyo_create_entry("query",            0600, tomoyo_dir,
			    TOMOYO_QUERY);
	tomoyo_create_entry("domain_policy",    0600, tomoyo_dir,
			    TOMOYO_DOMAINPOLICY);
	tomoyo_create_entry("exception_policy", 0600, tomoyo_dir,
			    TOMOYO_EXCEPTIONPOLICY);
	tomoyo_create_entry("audit",            0400, tomoyo_dir,
			    TOMOYO_AUDIT);
	tomoyo_create_entry(".process_status",  0600, tomoyo_dir,
			    TOMOYO_PROCESS_STATUS);
	tomoyo_create_entry("stat",             0644, tomoyo_dir,
			    TOMOYO_STAT);
	tomoyo_create_entry("profile",          0600, tomoyo_dir,
			    TOMOYO_PROFILE);
	tomoyo_create_entry("manager",          0600, tomoyo_dir,
			    TOMOYO_MANAGER);
	tomoyo_create_entry("version",          0400, tomoyo_dir,
			    TOMOYO_VERSION);
	securityfs_create_file("self_domain", 0666, tomoyo_dir, NULL,
			       &tomoyo_self_operations);
	tomoyo_load_builtin_policy();
	return 0;
}

fs_initcall(tomoyo_initerface_init);
