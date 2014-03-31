/* -*- mode: c; c-basic-offset: 8; -*-
 * vim: noexpandtab sw=8 ts=8 sts=0:
 *
 * stackglue.c
 *
 * Code which implements an OCFS2 specific interface to underlying
 * cluster stacks.
 *
 * Copyright (C) 2007, 2009 Oracle.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kmod.h>
#include <linux/fs.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/sysctl.h>

#include "ocfs2_fs.h"

#include "stackglue.h"

#define OCFS2_STACK_PLUGIN_O2CB		"o2cb"
#define OCFS2_STACK_PLUGIN_USER		"user"
#define OCFS2_MAX_HB_CTL_PATH		256

static struct ocfs2_protocol_version locking_max_version;
static DEFINE_SPINLOCK(ocfs2_stack_lock);
static LIST_HEAD(ocfs2_stack_list);
static char cluster_stack_name[OCFS2_STACK_LABEL_LEN + 1];
static char ocfs2_hb_ctl_path[OCFS2_MAX_HB_CTL_PATH] = "/sbin/ocfs2_hb_ctl";

static struct ocfs2_stack_plugin *active_stack;

static struct ocfs2_stack_plugin *ocfs2_stack_lookup(const char *name)
{
	struct ocfs2_stack_plugin *p;

	assert_spin_locked(&ocfs2_stack_lock);

	list_for_each_entry(p, &ocfs2_stack_list, sp_list) {
		if (!strcmp(p->sp_name, name))
			return p;
	}

	return NULL;
}

static int ocfs2_stack_driver_request(const char *stack_name,
				      const char *plugin_name)
{
	int rc;
	struct ocfs2_stack_plugin *p;

	spin_lock(&ocfs2_stack_lock);

	if (strcmp(stack_name, cluster_stack_name)) {
		rc = -EBUSY;
		goto out;
	}

	if (active_stack) {
		if (!strcmp(active_stack->sp_name, plugin_name))
			rc = 0;
		else
			rc = -EBUSY;
		goto out;
	}

	p = ocfs2_stack_lookup(plugin_name);
	if (!p || !try_module_get(p->sp_owner)) {
		rc = -ENOENT;
		goto out;
	}

	active_stack = p;
	rc = 0;

out:
	
	if (!rc)
		active_stack->sp_count++;

	spin_unlock(&ocfs2_stack_lock);
	return rc;
}

static int ocfs2_stack_driver_get(const char *stack_name)
{
	int rc;
	char *plugin_name = OCFS2_STACK_PLUGIN_O2CB;

	if (!stack_name || !*stack_name)
		stack_name = OCFS2_STACK_PLUGIN_O2CB;

	if (strlen(stack_name) != OCFS2_STACK_LABEL_LEN) {
		printk(KERN_ERR
		       "ocfs2 passed an invalid cluster stack label: \"%s\"\n",
		       stack_name);
		return -EINVAL;
	}

	
	if (strcmp(stack_name, OCFS2_STACK_PLUGIN_O2CB))
		plugin_name = OCFS2_STACK_PLUGIN_USER;

	rc = ocfs2_stack_driver_request(stack_name, plugin_name);
	if (rc == -ENOENT) {
		request_module("ocfs2_stack_%s", plugin_name);
		rc = ocfs2_stack_driver_request(stack_name, plugin_name);
	}

	if (rc == -ENOENT) {
		printk(KERN_ERR
		       "ocfs2: Cluster stack driver \"%s\" cannot be found\n",
		       plugin_name);
	} else if (rc == -EBUSY) {
		printk(KERN_ERR
		       "ocfs2: A different cluster stack is in use\n");
	}

	return rc;
}

static void ocfs2_stack_driver_put(void)
{
	spin_lock(&ocfs2_stack_lock);
	BUG_ON(active_stack == NULL);
	BUG_ON(active_stack->sp_count == 0);

	active_stack->sp_count--;
	if (!active_stack->sp_count) {
		module_put(active_stack->sp_owner);
		active_stack = NULL;
	}
	spin_unlock(&ocfs2_stack_lock);
}

int ocfs2_stack_glue_register(struct ocfs2_stack_plugin *plugin)
{
	int rc;

	spin_lock(&ocfs2_stack_lock);
	if (!ocfs2_stack_lookup(plugin->sp_name)) {
		plugin->sp_count = 0;
		plugin->sp_max_proto = locking_max_version;
		list_add(&plugin->sp_list, &ocfs2_stack_list);
		printk(KERN_INFO "ocfs2: Registered cluster interface %s\n",
		       plugin->sp_name);
		rc = 0;
	} else {
		printk(KERN_ERR "ocfs2: Stack \"%s\" already registered\n",
		       plugin->sp_name);
		rc = -EEXIST;
	}
	spin_unlock(&ocfs2_stack_lock);

	return rc;
}
EXPORT_SYMBOL_GPL(ocfs2_stack_glue_register);

void ocfs2_stack_glue_unregister(struct ocfs2_stack_plugin *plugin)
{
	struct ocfs2_stack_plugin *p;

	spin_lock(&ocfs2_stack_lock);
	p = ocfs2_stack_lookup(plugin->sp_name);
	if (p) {
		BUG_ON(p != plugin);
		BUG_ON(plugin == active_stack);
		BUG_ON(plugin->sp_count != 0);
		list_del_init(&plugin->sp_list);
		printk(KERN_INFO "ocfs2: Unregistered cluster interface %s\n",
		       plugin->sp_name);
	} else {
		printk(KERN_ERR "Stack \"%s\" is not registered\n",
		       plugin->sp_name);
	}
	spin_unlock(&ocfs2_stack_lock);
}
EXPORT_SYMBOL_GPL(ocfs2_stack_glue_unregister);

void ocfs2_stack_glue_set_max_proto_version(struct ocfs2_protocol_version *max_proto)
{
	struct ocfs2_stack_plugin *p;

	spin_lock(&ocfs2_stack_lock);
	if (memcmp(max_proto, &locking_max_version,
		   sizeof(struct ocfs2_protocol_version))) {
		BUG_ON(locking_max_version.pv_major != 0);

		locking_max_version = *max_proto;
		list_for_each_entry(p, &ocfs2_stack_list, sp_list) {
			p->sp_max_proto = locking_max_version;
		}
	}
	spin_unlock(&ocfs2_stack_lock);
}
EXPORT_SYMBOL_GPL(ocfs2_stack_glue_set_max_proto_version);


int ocfs2_dlm_lock(struct ocfs2_cluster_connection *conn,
		   int mode,
		   struct ocfs2_dlm_lksb *lksb,
		   u32 flags,
		   void *name,
		   unsigned int namelen)
{
	if (!lksb->lksb_conn)
		lksb->lksb_conn = conn;
	else
		BUG_ON(lksb->lksb_conn != conn);
	return active_stack->sp_ops->dlm_lock(conn, mode, lksb, flags,
					      name, namelen);
}
EXPORT_SYMBOL_GPL(ocfs2_dlm_lock);

int ocfs2_dlm_unlock(struct ocfs2_cluster_connection *conn,
		     struct ocfs2_dlm_lksb *lksb,
		     u32 flags)
{
	BUG_ON(lksb->lksb_conn == NULL);

	return active_stack->sp_ops->dlm_unlock(conn, lksb, flags);
}
EXPORT_SYMBOL_GPL(ocfs2_dlm_unlock);

int ocfs2_dlm_lock_status(struct ocfs2_dlm_lksb *lksb)
{
	return active_stack->sp_ops->lock_status(lksb);
}
EXPORT_SYMBOL_GPL(ocfs2_dlm_lock_status);

int ocfs2_dlm_lvb_valid(struct ocfs2_dlm_lksb *lksb)
{
	return active_stack->sp_ops->lvb_valid(lksb);
}
EXPORT_SYMBOL_GPL(ocfs2_dlm_lvb_valid);

void *ocfs2_dlm_lvb(struct ocfs2_dlm_lksb *lksb)
{
	return active_stack->sp_ops->lock_lvb(lksb);
}
EXPORT_SYMBOL_GPL(ocfs2_dlm_lvb);

void ocfs2_dlm_dump_lksb(struct ocfs2_dlm_lksb *lksb)
{
	active_stack->sp_ops->dump_lksb(lksb);
}
EXPORT_SYMBOL_GPL(ocfs2_dlm_dump_lksb);

int ocfs2_stack_supports_plocks(void)
{
	return active_stack && active_stack->sp_ops->plock;
}
EXPORT_SYMBOL_GPL(ocfs2_stack_supports_plocks);

int ocfs2_plock(struct ocfs2_cluster_connection *conn, u64 ino,
		struct file *file, int cmd, struct file_lock *fl)
{
	WARN_ON_ONCE(active_stack->sp_ops->plock == NULL);
	if (active_stack->sp_ops->plock)
		return active_stack->sp_ops->plock(conn, ino, file, cmd, fl);
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL_GPL(ocfs2_plock);

int ocfs2_cluster_connect(const char *stack_name,
			  const char *group,
			  int grouplen,
			  struct ocfs2_locking_protocol *lproto,
			  void (*recovery_handler)(int node_num,
						   void *recovery_data),
			  void *recovery_data,
			  struct ocfs2_cluster_connection **conn)
{
	int rc = 0;
	struct ocfs2_cluster_connection *new_conn;

	BUG_ON(group == NULL);
	BUG_ON(conn == NULL);
	BUG_ON(recovery_handler == NULL);

	if (grouplen > GROUP_NAME_MAX) {
		rc = -EINVAL;
		goto out;
	}

	if (memcmp(&lproto->lp_max_version, &locking_max_version,
		   sizeof(struct ocfs2_protocol_version))) {
		rc = -EINVAL;
		goto out;
	}

	new_conn = kzalloc(sizeof(struct ocfs2_cluster_connection),
			   GFP_KERNEL);
	if (!new_conn) {
		rc = -ENOMEM;
		goto out;
	}

	memcpy(new_conn->cc_name, group, grouplen);
	new_conn->cc_namelen = grouplen;
	new_conn->cc_recovery_handler = recovery_handler;
	new_conn->cc_recovery_data = recovery_data;

	new_conn->cc_proto = lproto;
	
	new_conn->cc_version = lproto->lp_max_version;

	
	rc = ocfs2_stack_driver_get(stack_name);
	if (rc)
		goto out_free;

	rc = active_stack->sp_ops->connect(new_conn);
	if (rc) {
		ocfs2_stack_driver_put();
		goto out_free;
	}

	*conn = new_conn;

out_free:
	if (rc)
		kfree(new_conn);

out:
	return rc;
}
EXPORT_SYMBOL_GPL(ocfs2_cluster_connect);

int ocfs2_cluster_connect_agnostic(const char *group,
				   int grouplen,
				   struct ocfs2_locking_protocol *lproto,
				   void (*recovery_handler)(int node_num,
							    void *recovery_data),
				   void *recovery_data,
				   struct ocfs2_cluster_connection **conn)
{
	char *stack_name = NULL;

	if (cluster_stack_name[0])
		stack_name = cluster_stack_name;
	return ocfs2_cluster_connect(stack_name, group, grouplen, lproto,
				     recovery_handler, recovery_data, conn);
}
EXPORT_SYMBOL_GPL(ocfs2_cluster_connect_agnostic);

int ocfs2_cluster_disconnect(struct ocfs2_cluster_connection *conn,
			     int hangup_pending)
{
	int ret;

	BUG_ON(conn == NULL);

	ret = active_stack->sp_ops->disconnect(conn);

	
	if (!ret) {
		kfree(conn);
		if (!hangup_pending)
			ocfs2_stack_driver_put();
	}

	return ret;
}
EXPORT_SYMBOL_GPL(ocfs2_cluster_disconnect);

static void ocfs2_leave_group(const char *group)
{
	int ret;
	char *argv[5], *envp[3];

	argv[0] = ocfs2_hb_ctl_path;
	argv[1] = "-K";
	argv[2] = "-u";
	argv[3] = (char *)group;
	argv[4] = NULL;

	
	envp[0] = "HOME=/";
	envp[1] = "PATH=/sbin:/bin:/usr/sbin:/usr/bin";
	envp[2] = NULL;

	ret = call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC);
	if (ret < 0) {
		printk(KERN_ERR
		       "ocfs2: Error %d running user helper "
		       "\"%s %s %s %s\"\n",
		       ret, argv[0], argv[1], argv[2], argv[3]);
	}
}

void ocfs2_cluster_hangup(const char *group, int grouplen)
{
	BUG_ON(group == NULL);
	BUG_ON(group[grouplen] != '\0');

	ocfs2_leave_group(group);

	
	ocfs2_stack_driver_put();
}
EXPORT_SYMBOL_GPL(ocfs2_cluster_hangup);

int ocfs2_cluster_this_node(unsigned int *node)
{
	return active_stack->sp_ops->this_node(node);
}
EXPORT_SYMBOL_GPL(ocfs2_cluster_this_node);



static ssize_t ocfs2_max_locking_protocol_show(struct kobject *kobj,
					       struct kobj_attribute *attr,
					       char *buf)
{
	ssize_t ret = 0;

	spin_lock(&ocfs2_stack_lock);
	if (locking_max_version.pv_major)
		ret = snprintf(buf, PAGE_SIZE, "%u.%u\n",
			       locking_max_version.pv_major,
			       locking_max_version.pv_minor);
	spin_unlock(&ocfs2_stack_lock);

	return ret;
}

static struct kobj_attribute ocfs2_attr_max_locking_protocol =
	__ATTR(max_locking_protocol, S_IFREG | S_IRUGO,
	       ocfs2_max_locking_protocol_show, NULL);

static ssize_t ocfs2_loaded_cluster_plugins_show(struct kobject *kobj,
						 struct kobj_attribute *attr,
						 char *buf)
{
	ssize_t ret = 0, total = 0, remain = PAGE_SIZE;
	struct ocfs2_stack_plugin *p;

	spin_lock(&ocfs2_stack_lock);
	list_for_each_entry(p, &ocfs2_stack_list, sp_list) {
		ret = snprintf(buf, remain, "%s\n",
			       p->sp_name);
		if (ret < 0) {
			total = ret;
			break;
		}
		if (ret == remain) {
			
			total = -E2BIG;
			break;
		}
		total += ret;
		remain -= ret;
	}
	spin_unlock(&ocfs2_stack_lock);

	return total;
}

static struct kobj_attribute ocfs2_attr_loaded_cluster_plugins =
	__ATTR(loaded_cluster_plugins, S_IFREG | S_IRUGO,
	       ocfs2_loaded_cluster_plugins_show, NULL);

static ssize_t ocfs2_active_cluster_plugin_show(struct kobject *kobj,
						struct kobj_attribute *attr,
						char *buf)
{
	ssize_t ret = 0;

	spin_lock(&ocfs2_stack_lock);
	if (active_stack) {
		ret = snprintf(buf, PAGE_SIZE, "%s\n",
			       active_stack->sp_name);
		if (ret == PAGE_SIZE)
			ret = -E2BIG;
	}
	spin_unlock(&ocfs2_stack_lock);

	return ret;
}

static struct kobj_attribute ocfs2_attr_active_cluster_plugin =
	__ATTR(active_cluster_plugin, S_IFREG | S_IRUGO,
	       ocfs2_active_cluster_plugin_show, NULL);

static ssize_t ocfs2_cluster_stack_show(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	ssize_t ret;
	spin_lock(&ocfs2_stack_lock);
	ret = snprintf(buf, PAGE_SIZE, "%s\n", cluster_stack_name);
	spin_unlock(&ocfs2_stack_lock);

	return ret;
}

static ssize_t ocfs2_cluster_stack_store(struct kobject *kobj,
					 struct kobj_attribute *attr,
					 const char *buf, size_t count)
{
	size_t len = count;
	ssize_t ret;

	if (len == 0)
		return len;

	if (buf[len - 1] == '\n')
		len--;

	if ((len != OCFS2_STACK_LABEL_LEN) ||
	    (strnlen(buf, len) != len))
		return -EINVAL;

	spin_lock(&ocfs2_stack_lock);
	if (active_stack) {
		if (!strncmp(buf, cluster_stack_name, len))
			ret = count;
		else
			ret = -EBUSY;
	} else {
		memcpy(cluster_stack_name, buf, len);
		ret = count;
	}
	spin_unlock(&ocfs2_stack_lock);

	return ret;
}


static struct kobj_attribute ocfs2_attr_cluster_stack =
	__ATTR(cluster_stack, S_IFREG | S_IRUGO | S_IWUSR,
	       ocfs2_cluster_stack_show,
	       ocfs2_cluster_stack_store);

static struct attribute *ocfs2_attrs[] = {
	&ocfs2_attr_max_locking_protocol.attr,
	&ocfs2_attr_loaded_cluster_plugins.attr,
	&ocfs2_attr_active_cluster_plugin.attr,
	&ocfs2_attr_cluster_stack.attr,
	NULL,
};

static struct attribute_group ocfs2_attr_group = {
	.attrs = ocfs2_attrs,
};

static struct kset *ocfs2_kset;

static void ocfs2_sysfs_exit(void)
{
	kset_unregister(ocfs2_kset);
}

static int ocfs2_sysfs_init(void)
{
	int ret;

	ocfs2_kset = kset_create_and_add("ocfs2", NULL, fs_kobj);
	if (!ocfs2_kset)
		return -ENOMEM;

	ret = sysfs_create_group(&ocfs2_kset->kobj, &ocfs2_attr_group);
	if (ret)
		goto error;

	return 0;

error:
	kset_unregister(ocfs2_kset);
	return ret;
}


#define FS_OCFS2_NM		1

static ctl_table ocfs2_nm_table[] = {
	{
		.procname	= "hb_ctl_path",
		.data		= ocfs2_hb_ctl_path,
		.maxlen		= OCFS2_MAX_HB_CTL_PATH,
		.mode		= 0644,
		.proc_handler	= proc_dostring,
	},
	{ }
};

static ctl_table ocfs2_mod_table[] = {
	{
		.procname	= "nm",
		.data		= NULL,
		.maxlen		= 0,
		.mode		= 0555,
		.child		= ocfs2_nm_table
	},
	{ }
};

static ctl_table ocfs2_kern_table[] = {
	{
		.procname	= "ocfs2",
		.data		= NULL,
		.maxlen		= 0,
		.mode		= 0555,
		.child		= ocfs2_mod_table
	},
	{ }
};

static ctl_table ocfs2_root_table[] = {
	{
		.procname	= "fs",
		.data		= NULL,
		.maxlen		= 0,
		.mode		= 0555,
		.child		= ocfs2_kern_table
	},
	{ }
};

static struct ctl_table_header *ocfs2_table_header = NULL;



static int __init ocfs2_stack_glue_init(void)
{
	strcpy(cluster_stack_name, OCFS2_STACK_PLUGIN_O2CB);

	ocfs2_table_header = register_sysctl_table(ocfs2_root_table);
	if (!ocfs2_table_header) {
		printk(KERN_ERR
		       "ocfs2 stack glue: unable to register sysctl\n");
		return -ENOMEM; 
	}

	return ocfs2_sysfs_init();
}

static void __exit ocfs2_stack_glue_exit(void)
{
	memset(&locking_max_version, 0,
	       sizeof(struct ocfs2_protocol_version));
	locking_max_version.pv_major = 0;
	locking_max_version.pv_minor = 0;
	ocfs2_sysfs_exit();
	if (ocfs2_table_header)
		unregister_sysctl_table(ocfs2_table_header);
}

MODULE_AUTHOR("Oracle");
MODULE_DESCRIPTION("ocfs2 cluter stack glue layer");
MODULE_LICENSE("GPL");
module_init(ocfs2_stack_glue_init);
module_exit(ocfs2_stack_glue_exit);
