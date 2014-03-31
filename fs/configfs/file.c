/* -*- mode: c; c-basic-offset: 8; -*-
 * vim: noexpandtab sw=8 ts=8 sts=0:
 *
 * file.c - operations for regular (text) files.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 *
 * Based on sysfs:
 * 	sysfs is Copyright (C) 2001, 2002, 2003 Patrick Mochel
 *
 * configfs Copyright (C) 2005 Oracle.  All rights reserved.
 */

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <asm/uaccess.h>

#include <linux/configfs.h>
#include "configfs_internal.h"

#define SIMPLE_ATTR_SIZE 4096

struct configfs_buffer {
	size_t			count;
	loff_t			pos;
	char			* page;
	struct configfs_item_operations	* ops;
	struct mutex		mutex;
	int			needs_read_fill;
};


static int fill_read_buffer(struct dentry * dentry, struct configfs_buffer * buffer)
{
	struct configfs_attribute * attr = to_attr(dentry);
	struct config_item * item = to_item(dentry->d_parent);
	struct configfs_item_operations * ops = buffer->ops;
	int ret = 0;
	ssize_t count;

	if (!buffer->page)
		buffer->page = (char *) get_zeroed_page(GFP_KERNEL);
	if (!buffer->page)
		return -ENOMEM;

	count = ops->show_attribute(item,attr,buffer->page);
	buffer->needs_read_fill = 0;
	BUG_ON(count > (ssize_t)SIMPLE_ATTR_SIZE);
	if (count >= 0)
		buffer->count = count;
	else
		ret = count;
	return ret;
}


static ssize_t
configfs_read_file(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	struct configfs_buffer * buffer = file->private_data;
	ssize_t retval = 0;

	mutex_lock(&buffer->mutex);
	if (buffer->needs_read_fill) {
		if ((retval = fill_read_buffer(file->f_path.dentry,buffer)))
			goto out;
	}
	pr_debug("%s: count = %zd, ppos = %lld, buf = %s\n",
		 __func__, count, *ppos, buffer->page);
	retval = simple_read_from_buffer(buf, count, ppos, buffer->page,
					 buffer->count);
out:
	mutex_unlock(&buffer->mutex);
	return retval;
}



static int
fill_write_buffer(struct configfs_buffer * buffer, const char __user * buf, size_t count)
{
	int error;

	if (!buffer->page)
		buffer->page = (char *)__get_free_pages(GFP_KERNEL, 0);
	if (!buffer->page)
		return -ENOMEM;

	if (count >= SIMPLE_ATTR_SIZE)
		count = SIMPLE_ATTR_SIZE - 1;
	error = copy_from_user(buffer->page,buf,count);
	buffer->needs_read_fill = 1;
	buffer->page[count] = 0;
	return error ? -EFAULT : count;
}



static int
flush_write_buffer(struct dentry * dentry, struct configfs_buffer * buffer, size_t count)
{
	struct configfs_attribute * attr = to_attr(dentry);
	struct config_item * item = to_item(dentry->d_parent);
	struct configfs_item_operations * ops = buffer->ops;

	return ops->store_attribute(item,attr,buffer->page,count);
}



static ssize_t
configfs_write_file(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	struct configfs_buffer * buffer = file->private_data;
	ssize_t len;

	mutex_lock(&buffer->mutex);
	len = fill_write_buffer(buffer, buf, count);
	if (len > 0)
		len = flush_write_buffer(file->f_path.dentry, buffer, count);
	if (len > 0)
		*ppos += len;
	mutex_unlock(&buffer->mutex);
	return len;
}

static int check_perm(struct inode * inode, struct file * file)
{
	struct config_item *item = configfs_get_config_item(file->f_path.dentry->d_parent);
	struct configfs_attribute * attr = to_attr(file->f_path.dentry);
	struct configfs_buffer * buffer;
	struct configfs_item_operations * ops = NULL;
	int error = 0;

	if (!item || !attr)
		goto Einval;

	
	if (!try_module_get(attr->ca_owner)) {
		error = -ENODEV;
		goto Done;
	}

	if (item->ci_type)
		ops = item->ci_type->ct_item_ops;
	else
		goto Eaccess;

	if (file->f_mode & FMODE_WRITE) {

		if (!(inode->i_mode & S_IWUGO) || !ops->store_attribute)
			goto Eaccess;

	}

	if (file->f_mode & FMODE_READ) {
		if (!(inode->i_mode & S_IRUGO) || !ops->show_attribute)
			goto Eaccess;
	}

	buffer = kzalloc(sizeof(struct configfs_buffer),GFP_KERNEL);
	if (!buffer) {
		error = -ENOMEM;
		goto Enomem;
	}
	mutex_init(&buffer->mutex);
	buffer->needs_read_fill = 1;
	buffer->ops = ops;
	file->private_data = buffer;
	goto Done;

 Einval:
	error = -EINVAL;
	goto Done;
 Eaccess:
	error = -EACCES;
 Enomem:
	module_put(attr->ca_owner);
 Done:
	if (error && item)
		config_item_put(item);
	return error;
}

static int configfs_open_file(struct inode * inode, struct file * filp)
{
	return check_perm(inode,filp);
}

static int configfs_release(struct inode * inode, struct file * filp)
{
	struct config_item * item = to_item(filp->f_path.dentry->d_parent);
	struct configfs_attribute * attr = to_attr(filp->f_path.dentry);
	struct module * owner = attr->ca_owner;
	struct configfs_buffer * buffer = filp->private_data;

	if (item)
		config_item_put(item);
	
	module_put(owner);

	if (buffer) {
		if (buffer->page)
			free_page((unsigned long)buffer->page);
		mutex_destroy(&buffer->mutex);
		kfree(buffer);
	}
	return 0;
}

const struct file_operations configfs_file_operations = {
	.read		= configfs_read_file,
	.write		= configfs_write_file,
	.llseek		= generic_file_llseek,
	.open		= configfs_open_file,
	.release	= configfs_release,
};


int configfs_add_file(struct dentry * dir, const struct configfs_attribute * attr, int type)
{
	struct configfs_dirent * parent_sd = dir->d_fsdata;
	umode_t mode = (attr->ca_mode & S_IALLUGO) | S_IFREG;
	int error = 0;

	mutex_lock_nested(&dir->d_inode->i_mutex, I_MUTEX_NORMAL);
	error = configfs_make_dirent(parent_sd, NULL, (void *) attr, mode, type);
	mutex_unlock(&dir->d_inode->i_mutex);

	return error;
}



int configfs_create_file(struct config_item * item, const struct configfs_attribute * attr)
{
	BUG_ON(!item || !item->ci_dentry || !attr);

	return configfs_add_file(item->ci_dentry, attr,
				 CONFIGFS_ITEM_ATTR);
}

