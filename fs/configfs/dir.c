/* -*- mode: c; c-basic-offset: 8; -*-
 * vim: noexpandtab sw=8 ts=8 sts=0:
 *
 * dir.c - Operations for configfs directories.
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

#undef DEBUG

#include <linux/fs.h>
#include <linux/mount.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/err.h>

#include <linux/configfs.h>
#include "configfs_internal.h"

DECLARE_RWSEM(configfs_rename_sem);
DEFINE_SPINLOCK(configfs_dirent_lock);

static void configfs_d_iput(struct dentry * dentry,
			    struct inode * inode)
{
	struct configfs_dirent *sd = dentry->d_fsdata;

	if (sd) {
		BUG_ON(sd->s_dentry != dentry);
		
		spin_lock(&configfs_dirent_lock);
		sd->s_dentry = NULL;
		spin_unlock(&configfs_dirent_lock);
		configfs_put(sd);
	}
	iput(inode);
}

static int configfs_d_delete(const struct dentry *dentry)
{
	return 1;
}

const struct dentry_operations configfs_dentry_ops = {
	.d_iput		= configfs_d_iput,
	
	.d_delete	= configfs_d_delete,
};

#ifdef CONFIG_LOCKDEP


static void configfs_init_dirent_depth(struct configfs_dirent *sd)
{
	sd->s_depth = -1;
}

static void configfs_set_dir_dirent_depth(struct configfs_dirent *parent_sd,
					  struct configfs_dirent *sd)
{
	int parent_depth = parent_sd->s_depth;

	if (parent_depth >= 0)
		sd->s_depth = parent_depth + 1;
}

static void
configfs_adjust_dir_dirent_depth_before_populate(struct configfs_dirent *sd)
{
	if (sd->s_depth == -1)
		sd->s_depth = 0;
}

static void
configfs_adjust_dir_dirent_depth_after_populate(struct configfs_dirent *sd)
{
	
	sd->s_depth = -1;
}

#else 

static void configfs_init_dirent_depth(struct configfs_dirent *sd)
{
}

static void configfs_set_dir_dirent_depth(struct configfs_dirent *parent_sd,
					  struct configfs_dirent *sd)
{
}

static void
configfs_adjust_dir_dirent_depth_before_populate(struct configfs_dirent *sd)
{
}

static void
configfs_adjust_dir_dirent_depth_after_populate(struct configfs_dirent *sd)
{
}

#endif 

static struct configfs_dirent *configfs_new_dirent(struct configfs_dirent *parent_sd,
						   void *element, int type)
{
	struct configfs_dirent * sd;

	sd = kmem_cache_zalloc(configfs_dir_cachep, GFP_KERNEL);
	if (!sd)
		return ERR_PTR(-ENOMEM);

	atomic_set(&sd->s_count, 1);
	INIT_LIST_HEAD(&sd->s_links);
	INIT_LIST_HEAD(&sd->s_children);
	sd->s_element = element;
	sd->s_type = type;
	configfs_init_dirent_depth(sd);
	spin_lock(&configfs_dirent_lock);
	if (parent_sd->s_type & CONFIGFS_USET_DROPPING) {
		spin_unlock(&configfs_dirent_lock);
		kmem_cache_free(configfs_dir_cachep, sd);
		return ERR_PTR(-ENOENT);
	}
	list_add(&sd->s_sibling, &parent_sd->s_children);
	spin_unlock(&configfs_dirent_lock);

	return sd;
}

static int configfs_dirent_exists(struct configfs_dirent *parent_sd,
				  const unsigned char *new)
{
	struct configfs_dirent * sd;

	list_for_each_entry(sd, &parent_sd->s_children, s_sibling) {
		if (sd->s_element) {
			const unsigned char *existing = configfs_get_name(sd);
			if (strcmp(existing, new))
				continue;
			else
				return -EEXIST;
		}
	}

	return 0;
}


int configfs_make_dirent(struct configfs_dirent * parent_sd,
			 struct dentry * dentry, void * element,
			 umode_t mode, int type)
{
	struct configfs_dirent * sd;

	sd = configfs_new_dirent(parent_sd, element, type);
	if (IS_ERR(sd))
		return PTR_ERR(sd);

	sd->s_mode = mode;
	sd->s_dentry = dentry;
	if (dentry)
		dentry->d_fsdata = configfs_get(sd);

	return 0;
}

static int init_dir(struct inode * inode)
{
	inode->i_op = &configfs_dir_inode_operations;
	inode->i_fop = &configfs_dir_operations;

	
	inc_nlink(inode);
	return 0;
}

static int configfs_init_file(struct inode * inode)
{
	inode->i_size = PAGE_SIZE;
	inode->i_fop = &configfs_file_operations;
	return 0;
}

static int init_symlink(struct inode * inode)
{
	inode->i_op = &configfs_symlink_inode_operations;
	return 0;
}

static int create_dir(struct config_item *k, struct dentry *d)
{
	int error;
	umode_t mode = S_IFDIR| S_IRWXU | S_IRUGO | S_IXUGO;
	struct dentry *p = d->d_parent;

	BUG_ON(!k);

	error = configfs_dirent_exists(p->d_fsdata, d->d_name.name);
	if (!error)
		error = configfs_make_dirent(p->d_fsdata, d, k, mode,
					     CONFIGFS_DIR | CONFIGFS_USET_CREATING);
	if (!error) {
		configfs_set_dir_dirent_depth(p->d_fsdata, d->d_fsdata);
		error = configfs_create(d, mode, init_dir);
		if (!error) {
			inc_nlink(p->d_inode);
		} else {
			struct configfs_dirent *sd = d->d_fsdata;
			if (sd) {
				spin_lock(&configfs_dirent_lock);
				list_del_init(&sd->s_sibling);
				spin_unlock(&configfs_dirent_lock);
				configfs_put(sd);
			}
		}
	}
	return error;
}



static int configfs_create_dir(struct config_item * item, struct dentry *dentry)
{
	int error = create_dir(item, dentry);
	if (!error)
		item->ci_dentry = dentry;
	return error;
}

static void configfs_dir_set_ready(struct configfs_dirent *sd)
{
	struct configfs_dirent *child_sd;

	sd->s_type &= ~CONFIGFS_USET_CREATING;
	list_for_each_entry(child_sd, &sd->s_children, s_sibling)
		if (child_sd->s_type & CONFIGFS_USET_CREATING)
			configfs_dir_set_ready(child_sd);
}

int configfs_dirent_is_ready(struct configfs_dirent *sd)
{
	int ret;

	spin_lock(&configfs_dirent_lock);
	ret = !(sd->s_type & CONFIGFS_USET_CREATING);
	spin_unlock(&configfs_dirent_lock);

	return ret;
}

int configfs_create_link(struct configfs_symlink *sl,
			 struct dentry *parent,
			 struct dentry *dentry)
{
	int err = 0;
	umode_t mode = S_IFLNK | S_IRWXUGO;

	err = configfs_make_dirent(parent->d_fsdata, dentry, sl, mode,
				   CONFIGFS_ITEM_LINK);
	if (!err) {
		err = configfs_create(dentry, mode, init_symlink);
		if (err) {
			struct configfs_dirent *sd = dentry->d_fsdata;
			if (sd) {
				spin_lock(&configfs_dirent_lock);
				list_del_init(&sd->s_sibling);
				spin_unlock(&configfs_dirent_lock);
				configfs_put(sd);
			}
		}
	}
	return err;
}

static void remove_dir(struct dentry * d)
{
	struct dentry * parent = dget(d->d_parent);
	struct configfs_dirent * sd;

	sd = d->d_fsdata;
	spin_lock(&configfs_dirent_lock);
	list_del_init(&sd->s_sibling);
	spin_unlock(&configfs_dirent_lock);
	configfs_put(sd);
	if (d->d_inode)
		simple_rmdir(parent->d_inode,d);

	pr_debug(" o %s removing done (%d)\n",d->d_name.name, d->d_count);

	dput(parent);
}


static void configfs_remove_dir(struct config_item * item)
{
	struct dentry * dentry = dget(item->ci_dentry);

	if (!dentry)
		return;

	remove_dir(dentry);
	dput(dentry);
}


static int configfs_attach_attr(struct configfs_dirent * sd, struct dentry * dentry)
{
	struct configfs_attribute * attr = sd->s_element;
	int error;

	dentry->d_fsdata = configfs_get(sd);
	sd->s_dentry = dentry;
	error = configfs_create(dentry, (attr->ca_mode & S_IALLUGO) | S_IFREG,
				configfs_init_file);
	if (error) {
		configfs_put(sd);
		return error;
	}

	d_rehash(dentry);

	return 0;
}

static struct dentry * configfs_lookup(struct inode *dir,
				       struct dentry *dentry,
				       struct nameidata *nd)
{
	struct configfs_dirent * parent_sd = dentry->d_parent->d_fsdata;
	struct configfs_dirent * sd;
	int found = 0;
	int err;

	err = -ENOENT;
	if (!configfs_dirent_is_ready(parent_sd))
		goto out;

	list_for_each_entry(sd, &parent_sd->s_children, s_sibling) {
		if (sd->s_type & CONFIGFS_NOT_PINNED) {
			const unsigned char * name = configfs_get_name(sd);

			if (strcmp(name, dentry->d_name.name))
				continue;

			found = 1;
			err = configfs_attach_attr(sd, dentry);
			break;
		}
	}

	if (!found) {
		if (dentry->d_name.len > NAME_MAX)
			return ERR_PTR(-ENAMETOOLONG);
		d_add(dentry, NULL);
		return NULL;
	}

out:
	return ERR_PTR(err);
}

static int configfs_detach_prep(struct dentry *dentry, struct mutex **wait_mutex)
{
	struct configfs_dirent *parent_sd = dentry->d_fsdata;
	struct configfs_dirent *sd;
	int ret;

	
	parent_sd->s_type |= CONFIGFS_USET_DROPPING;

	ret = -EBUSY;
	if (!list_empty(&parent_sd->s_links))
		goto out;

	ret = 0;
	list_for_each_entry(sd, &parent_sd->s_children, s_sibling) {
		if (!sd->s_element ||
		    (sd->s_type & CONFIGFS_NOT_PINNED))
			continue;
		if (sd->s_type & CONFIGFS_USET_DEFAULT) {
			
			if (sd->s_type & CONFIGFS_USET_IN_MKDIR) {
				if (wait_mutex)
					*wait_mutex = &sd->s_dentry->d_inode->i_mutex;
				return -EAGAIN;
			}

			ret = configfs_detach_prep(sd->s_dentry, wait_mutex);
			if (!ret)
				continue;
		} else
			ret = -ENOTEMPTY;

		break;
	}

out:
	return ret;
}

static void configfs_detach_rollback(struct dentry *dentry)
{
	struct configfs_dirent *parent_sd = dentry->d_fsdata;
	struct configfs_dirent *sd;

	parent_sd->s_type &= ~CONFIGFS_USET_DROPPING;

	list_for_each_entry(sd, &parent_sd->s_children, s_sibling)
		if (sd->s_type & CONFIGFS_USET_DEFAULT)
			configfs_detach_rollback(sd->s_dentry);
}

static void detach_attrs(struct config_item * item)
{
	struct dentry * dentry = dget(item->ci_dentry);
	struct configfs_dirent * parent_sd;
	struct configfs_dirent * sd, * tmp;

	if (!dentry)
		return;

	pr_debug("configfs %s: dropping attrs for  dir\n",
		 dentry->d_name.name);

	parent_sd = dentry->d_fsdata;
	list_for_each_entry_safe(sd, tmp, &parent_sd->s_children, s_sibling) {
		if (!sd->s_element || !(sd->s_type & CONFIGFS_NOT_PINNED))
			continue;
		spin_lock(&configfs_dirent_lock);
		list_del_init(&sd->s_sibling);
		spin_unlock(&configfs_dirent_lock);
		configfs_drop_dentry(sd, dentry);
		configfs_put(sd);
	}

	dput(dentry);
}

static int populate_attrs(struct config_item *item)
{
	struct config_item_type *t = item->ci_type;
	struct configfs_attribute *attr;
	int error = 0;
	int i;

	if (!t)
		return -EINVAL;
	if (t->ct_attrs) {
		for (i = 0; (attr = t->ct_attrs[i]) != NULL; i++) {
			if ((error = configfs_create_file(item, attr)))
				break;
		}
	}

	if (error)
		detach_attrs(item);

	return error;
}

static int configfs_attach_group(struct config_item *parent_item,
				 struct config_item *item,
				 struct dentry *dentry);
static void configfs_detach_group(struct config_item *item);

static void detach_groups(struct config_group *group)
{
	struct dentry * dentry = dget(group->cg_item.ci_dentry);
	struct dentry *child;
	struct configfs_dirent *parent_sd;
	struct configfs_dirent *sd, *tmp;

	if (!dentry)
		return;

	parent_sd = dentry->d_fsdata;
	list_for_each_entry_safe(sd, tmp, &parent_sd->s_children, s_sibling) {
		if (!sd->s_element ||
		    !(sd->s_type & CONFIGFS_USET_DEFAULT))
			continue;

		child = sd->s_dentry;

		mutex_lock(&child->d_inode->i_mutex);

		configfs_detach_group(sd->s_element);
		child->d_inode->i_flags |= S_DEAD;
		dont_mount(child);

		mutex_unlock(&child->d_inode->i_mutex);

		d_delete(child);
		dput(child);
	}

	dput(dentry);
}

static int create_default_group(struct config_group *parent_group,
				struct config_group *group)
{
	int ret;
	struct qstr name;
	struct configfs_dirent *sd;
	
	struct dentry *child, *parent = parent_group->cg_item.ci_dentry;

	if (!group->cg_item.ci_name)
		group->cg_item.ci_name = group->cg_item.ci_namebuf;
	name.name = group->cg_item.ci_name;
	name.len = strlen(name.name);
	name.hash = full_name_hash(name.name, name.len);

	ret = -ENOMEM;
	child = d_alloc(parent, &name);
	if (child) {
		d_add(child, NULL);

		ret = configfs_attach_group(&parent_group->cg_item,
					    &group->cg_item, child);
		if (!ret) {
			sd = child->d_fsdata;
			sd->s_type |= CONFIGFS_USET_DEFAULT;
		} else {
			BUG_ON(child->d_inode);
			d_drop(child);
			dput(child);
		}
	}

	return ret;
}

static int populate_groups(struct config_group *group)
{
	struct config_group *new_group;
	int ret = 0;
	int i;

	if (group->default_groups) {
		for (i = 0; group->default_groups[i]; i++) {
			new_group = group->default_groups[i];

			ret = create_default_group(group, new_group);
			if (ret) {
				detach_groups(group);
				break;
			}
		}
	}

	return ret;
}


static void unlink_obj(struct config_item *item)
{
	struct config_group *group;

	group = item->ci_group;
	if (group) {
		list_del_init(&item->ci_entry);

		item->ci_group = NULL;
		item->ci_parent = NULL;

		
		config_item_put(item);

		
		config_group_put(group);
	}
}

static void link_obj(struct config_item *parent_item, struct config_item *item)
{
	item->ci_parent = parent_item;

	item->ci_group = config_group_get(to_config_group(parent_item));
	list_add_tail(&item->ci_entry, &item->ci_group->cg_children);

	config_item_get(item);
}

static void unlink_group(struct config_group *group)
{
	int i;
	struct config_group *new_group;

	if (group->default_groups) {
		for (i = 0; group->default_groups[i]; i++) {
			new_group = group->default_groups[i];
			unlink_group(new_group);
		}
	}

	group->cg_subsys = NULL;
	unlink_obj(&group->cg_item);
}

static void link_group(struct config_group *parent_group, struct config_group *group)
{
	int i;
	struct config_group *new_group;
	struct configfs_subsystem *subsys = NULL; 

	link_obj(&parent_group->cg_item, &group->cg_item);

	if (parent_group->cg_subsys)
		subsys = parent_group->cg_subsys;
	else if (configfs_is_root(&parent_group->cg_item))
		subsys = to_configfs_subsystem(group);
	else
		BUG();
	group->cg_subsys = subsys;

	if (group->default_groups) {
		for (i = 0; group->default_groups[i]; i++) {
			new_group = group->default_groups[i];
			link_group(group, new_group);
		}
	}
}

static int configfs_attach_item(struct config_item *parent_item,
				struct config_item *item,
				struct dentry *dentry)
{
	int ret;

	ret = configfs_create_dir(item, dentry);
	if (!ret) {
		ret = populate_attrs(item);
		if (ret) {
			mutex_lock(&dentry->d_inode->i_mutex);
			configfs_remove_dir(item);
			dentry->d_inode->i_flags |= S_DEAD;
			dont_mount(dentry);
			mutex_unlock(&dentry->d_inode->i_mutex);
			d_delete(dentry);
		}
	}

	return ret;
}

static void configfs_detach_item(struct config_item *item)
{
	detach_attrs(item);
	configfs_remove_dir(item);
}

static int configfs_attach_group(struct config_item *parent_item,
				 struct config_item *item,
				 struct dentry *dentry)
{
	int ret;
	struct configfs_dirent *sd;

	ret = configfs_attach_item(parent_item, item, dentry);
	if (!ret) {
		sd = dentry->d_fsdata;
		sd->s_type |= CONFIGFS_USET_DIR;

		mutex_lock_nested(&dentry->d_inode->i_mutex, I_MUTEX_CHILD);
		configfs_adjust_dir_dirent_depth_before_populate(sd);
		ret = populate_groups(to_config_group(item));
		if (ret) {
			configfs_detach_item(item);
			dentry->d_inode->i_flags |= S_DEAD;
			dont_mount(dentry);
		}
		configfs_adjust_dir_dirent_depth_after_populate(sd);
		mutex_unlock(&dentry->d_inode->i_mutex);
		if (ret)
			d_delete(dentry);
	}

	return ret;
}

static void configfs_detach_group(struct config_item *item)
{
	detach_groups(to_config_group(item));
	configfs_detach_item(item);
}

static void client_disconnect_notify(struct config_item *parent_item,
				     struct config_item *item)
{
	struct config_item_type *type;

	type = parent_item->ci_type;
	BUG_ON(!type);

	if (type->ct_group_ops && type->ct_group_ops->disconnect_notify)
		type->ct_group_ops->disconnect_notify(to_config_group(parent_item),
						      item);
}

static void client_drop_item(struct config_item *parent_item,
			     struct config_item *item)
{
	struct config_item_type *type;

	type = parent_item->ci_type;
	BUG_ON(!type);

	if (type->ct_group_ops && type->ct_group_ops->drop_item)
		type->ct_group_ops->drop_item(to_config_group(parent_item),
					      item);
	else
		config_item_put(item);
}

#ifdef DEBUG
static void configfs_dump_one(struct configfs_dirent *sd, int level)
{
	printk(KERN_INFO "%*s\"%s\":\n", level, " ", configfs_get_name(sd));

#define type_print(_type) if (sd->s_type & _type) printk(KERN_INFO "%*s %s\n", level, " ", #_type);
	type_print(CONFIGFS_ROOT);
	type_print(CONFIGFS_DIR);
	type_print(CONFIGFS_ITEM_ATTR);
	type_print(CONFIGFS_ITEM_LINK);
	type_print(CONFIGFS_USET_DIR);
	type_print(CONFIGFS_USET_DEFAULT);
	type_print(CONFIGFS_USET_DROPPING);
#undef type_print
}

static int configfs_dump(struct configfs_dirent *sd, int level)
{
	struct configfs_dirent *child_sd;
	int ret = 0;

	configfs_dump_one(sd, level);

	if (!(sd->s_type & (CONFIGFS_DIR|CONFIGFS_ROOT)))
		return 0;

	list_for_each_entry(child_sd, &sd->s_children, s_sibling) {
		ret = configfs_dump(child_sd, level + 2);
		if (ret)
			break;
	}

	return ret;
}
#endif



static int configfs_depend_prep(struct dentry *origin,
				struct config_item *target)
{
	struct configfs_dirent *child_sd, *sd = origin->d_fsdata;
	int ret = 0;

	BUG_ON(!origin || !sd);

	if (sd->s_element == target)  
		goto out;

	list_for_each_entry(child_sd, &sd->s_children, s_sibling) {
		if ((child_sd->s_type & CONFIGFS_DIR) &&
		    !(child_sd->s_type & CONFIGFS_USET_DROPPING) &&
		    !(child_sd->s_type & CONFIGFS_USET_CREATING)) {
			ret = configfs_depend_prep(child_sd->s_dentry,
						   target);
			if (!ret)
				goto out;  
		}
	}

	
	ret = -ENOENT;

out:
	return ret;
}

int configfs_depend_item(struct configfs_subsystem *subsys,
			 struct config_item *target)
{
	int ret;
	struct configfs_dirent *p, *root_sd, *subsys_sd = NULL;
	struct config_item *s_item = &subsys->su_group.cg_item;
	struct dentry *root;

	root = configfs_pin_fs();
	if (IS_ERR(root))
		return PTR_ERR(root);

	mutex_lock(&root->d_inode->i_mutex);

	root_sd = root->d_fsdata;

	list_for_each_entry(p, &root_sd->s_children, s_sibling) {
		if (p->s_type & CONFIGFS_DIR) {
			if (p->s_element == s_item) {
				subsys_sd = p;
				break;
			}
		}
	}

	if (!subsys_sd) {
		ret = -ENOENT;
		goto out_unlock_fs;
	}

	

	spin_lock(&configfs_dirent_lock);
	
	ret = configfs_depend_prep(subsys_sd->s_dentry, target);
	if (ret)
		goto out_unlock_dirent_lock;

	p = target->ci_dentry->d_fsdata;
	p->s_dependent_count += 1;

out_unlock_dirent_lock:
	spin_unlock(&configfs_dirent_lock);
out_unlock_fs:
	mutex_unlock(&root->d_inode->i_mutex);

	configfs_release_fs();

	return ret;
}
EXPORT_SYMBOL(configfs_depend_item);

void configfs_undepend_item(struct configfs_subsystem *subsys,
			    struct config_item *target)
{
	struct configfs_dirent *sd;

	spin_lock(&configfs_dirent_lock);

	sd = target->ci_dentry->d_fsdata;
	BUG_ON(sd->s_dependent_count < 1);

	sd->s_dependent_count -= 1;

	spin_unlock(&configfs_dirent_lock);
}
EXPORT_SYMBOL(configfs_undepend_item);

static int configfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
	int ret = 0;
	int module_got = 0;
	struct config_group *group = NULL;
	struct config_item *item = NULL;
	struct config_item *parent_item;
	struct configfs_subsystem *subsys;
	struct configfs_dirent *sd;
	struct config_item_type *type;
	struct module *subsys_owner = NULL, *new_item_owner = NULL;
	char *name;

	sd = dentry->d_parent->d_fsdata;

	if (!configfs_dirent_is_ready(sd)) {
		ret = -ENOENT;
		goto out;
	}

	if (!(sd->s_type & CONFIGFS_USET_DIR)) {
		ret = -EPERM;
		goto out;
	}

	
	parent_item = configfs_get_config_item(dentry->d_parent);
	type = parent_item->ci_type;
	subsys = to_config_group(parent_item)->cg_subsys;
	BUG_ON(!subsys);

	if (!type || !type->ct_group_ops ||
	    (!type->ct_group_ops->make_group &&
	     !type->ct_group_ops->make_item)) {
		ret = -EPERM;  
		goto out_put;
	}

	if (!subsys->su_group.cg_item.ci_type) {
		ret = -EINVAL;
		goto out_put;
	}
	subsys_owner = subsys->su_group.cg_item.ci_type->ct_owner;
	if (!try_module_get(subsys_owner)) {
		ret = -EINVAL;
		goto out_put;
	}

	name = kmalloc(dentry->d_name.len + 1, GFP_KERNEL);
	if (!name) {
		ret = -ENOMEM;
		goto out_subsys_put;
	}

	snprintf(name, dentry->d_name.len + 1, "%s", dentry->d_name.name);

	mutex_lock(&subsys->su_mutex);
	if (type->ct_group_ops->make_group) {
		group = type->ct_group_ops->make_group(to_config_group(parent_item), name);
		if (!group)
			group = ERR_PTR(-ENOMEM);
		if (!IS_ERR(group)) {
			link_group(to_config_group(parent_item), group);
			item = &group->cg_item;
		} else
			ret = PTR_ERR(group);
	} else {
		item = type->ct_group_ops->make_item(to_config_group(parent_item), name);
		if (!item)
			item = ERR_PTR(-ENOMEM);
		if (!IS_ERR(item))
			link_obj(parent_item, item);
		else
			ret = PTR_ERR(item);
	}
	mutex_unlock(&subsys->su_mutex);

	kfree(name);
	if (ret) {
		goto out_subsys_put;
	}


	type = item->ci_type;
	if (!type) {
		ret = -EINVAL;
		goto out_unlink;
	}

	new_item_owner = type->ct_owner;
	if (!try_module_get(new_item_owner)) {
		ret = -EINVAL;
		goto out_unlink;
	}

	module_got = 1;

	spin_lock(&configfs_dirent_lock);
	
	sd->s_type |= CONFIGFS_USET_IN_MKDIR;
	spin_unlock(&configfs_dirent_lock);

	if (group)
		ret = configfs_attach_group(parent_item, item, dentry);
	else
		ret = configfs_attach_item(parent_item, item, dentry);

	spin_lock(&configfs_dirent_lock);
	sd->s_type &= ~CONFIGFS_USET_IN_MKDIR;
	if (!ret)
		configfs_dir_set_ready(dentry->d_fsdata);
	spin_unlock(&configfs_dirent_lock);

out_unlink:
	if (ret) {
		
		mutex_lock(&subsys->su_mutex);

		client_disconnect_notify(parent_item, item);
		if (group)
			unlink_group(group);
		else
			unlink_obj(item);
		client_drop_item(parent_item, item);

		mutex_unlock(&subsys->su_mutex);

		if (module_got)
			module_put(new_item_owner);
	}

out_subsys_put:
	if (ret)
		module_put(subsys_owner);

out_put:
	config_item_put(parent_item);

out:
	return ret;
}

static int configfs_rmdir(struct inode *dir, struct dentry *dentry)
{
	struct config_item *parent_item;
	struct config_item *item;
	struct configfs_subsystem *subsys;
	struct configfs_dirent *sd;
	struct module *subsys_owner = NULL, *dead_item_owner = NULL;
	int ret;

	sd = dentry->d_fsdata;
	if (sd->s_type & CONFIGFS_USET_DEFAULT)
		return -EPERM;

	
	parent_item = configfs_get_config_item(dentry->d_parent);
	subsys = to_config_group(parent_item)->cg_subsys;
	BUG_ON(!subsys);

	if (!parent_item->ci_type) {
		config_item_put(parent_item);
		return -EINVAL;
	}

	
	BUG_ON(!subsys->su_group.cg_item.ci_type);
	subsys_owner = subsys->su_group.cg_item.ci_type->ct_owner;

	do {
		struct mutex *wait_mutex;

		mutex_lock(&configfs_symlink_mutex);
		spin_lock(&configfs_dirent_lock);
		ret = sd->s_dependent_count ? -EBUSY : 0;
		if (!ret) {
			ret = configfs_detach_prep(dentry, &wait_mutex);
			if (ret)
				configfs_detach_rollback(dentry);
		}
		spin_unlock(&configfs_dirent_lock);
		mutex_unlock(&configfs_symlink_mutex);

		if (ret) {
			if (ret != -EAGAIN) {
				config_item_put(parent_item);
				return ret;
			}

			
			mutex_lock(wait_mutex);
			mutex_unlock(wait_mutex);
		}
	} while (ret == -EAGAIN);

	
	item = configfs_get_config_item(dentry);

	
	config_item_put(parent_item);

	if (item->ci_type)
		dead_item_owner = item->ci_type->ct_owner;

	if (sd->s_type & CONFIGFS_USET_DIR) {
		configfs_detach_group(item);

		mutex_lock(&subsys->su_mutex);
		client_disconnect_notify(parent_item, item);
		unlink_group(to_config_group(item));
	} else {
		configfs_detach_item(item);

		mutex_lock(&subsys->su_mutex);
		client_disconnect_notify(parent_item, item);
		unlink_obj(item);
	}

	client_drop_item(parent_item, item);
	mutex_unlock(&subsys->su_mutex);

	
	config_item_put(item);

	module_put(dead_item_owner);
	module_put(subsys_owner);

	return 0;
}

const struct inode_operations configfs_dir_inode_operations = {
	.mkdir		= configfs_mkdir,
	.rmdir		= configfs_rmdir,
	.symlink	= configfs_symlink,
	.unlink		= configfs_unlink,
	.lookup		= configfs_lookup,
	.setattr	= configfs_setattr,
};

const struct inode_operations configfs_root_inode_operations = {
	.lookup		= configfs_lookup,
	.setattr	= configfs_setattr,
};

#if 0
int configfs_rename_dir(struct config_item * item, const char *new_name)
{
	int error = 0;
	struct dentry * new_dentry, * parent;

	if (!strcmp(config_item_name(item), new_name))
		return -EINVAL;

	if (!item->parent)
		return -EINVAL;

	down_write(&configfs_rename_sem);
	parent = item->parent->dentry;

	mutex_lock(&parent->d_inode->i_mutex);

	new_dentry = lookup_one_len(new_name, parent, strlen(new_name));
	if (!IS_ERR(new_dentry)) {
		if (!new_dentry->d_inode) {
			error = config_item_set_name(item, "%s", new_name);
			if (!error) {
				d_add(new_dentry, NULL);
				d_move(item->dentry, new_dentry);
			}
			else
				d_delete(new_dentry);
		} else
			error = -EEXIST;
		dput(new_dentry);
	}
	mutex_unlock(&parent->d_inode->i_mutex);
	up_write(&configfs_rename_sem);

	return error;
}
#endif

static int configfs_dir_open(struct inode *inode, struct file *file)
{
	struct dentry * dentry = file->f_path.dentry;
	struct configfs_dirent * parent_sd = dentry->d_fsdata;
	int err;

	mutex_lock(&dentry->d_inode->i_mutex);
	err = -ENOENT;
	if (configfs_dirent_is_ready(parent_sd)) {
		file->private_data = configfs_new_dirent(parent_sd, NULL, 0);
		if (IS_ERR(file->private_data))
			err = PTR_ERR(file->private_data);
		else
			err = 0;
	}
	mutex_unlock(&dentry->d_inode->i_mutex);

	return err;
}

static int configfs_dir_close(struct inode *inode, struct file *file)
{
	struct dentry * dentry = file->f_path.dentry;
	struct configfs_dirent * cursor = file->private_data;

	mutex_lock(&dentry->d_inode->i_mutex);
	spin_lock(&configfs_dirent_lock);
	list_del_init(&cursor->s_sibling);
	spin_unlock(&configfs_dirent_lock);
	mutex_unlock(&dentry->d_inode->i_mutex);

	release_configfs_dirent(cursor);

	return 0;
}

static inline unsigned char dt_type(struct configfs_dirent *sd)
{
	return (sd->s_mode >> 12) & 15;
}

static int configfs_readdir(struct file * filp, void * dirent, filldir_t filldir)
{
	struct dentry *dentry = filp->f_path.dentry;
	struct super_block *sb = dentry->d_sb;
	struct configfs_dirent * parent_sd = dentry->d_fsdata;
	struct configfs_dirent *cursor = filp->private_data;
	struct list_head *p, *q = &cursor->s_sibling;
	ino_t ino = 0;
	int i = filp->f_pos;

	switch (i) {
		case 0:
			ino = dentry->d_inode->i_ino;
			if (filldir(dirent, ".", 1, i, ino, DT_DIR) < 0)
				break;
			filp->f_pos++;
			i++;
			
		case 1:
			ino = parent_ino(dentry);
			if (filldir(dirent, "..", 2, i, ino, DT_DIR) < 0)
				break;
			filp->f_pos++;
			i++;
			
		default:
			if (filp->f_pos == 2) {
				spin_lock(&configfs_dirent_lock);
				list_move(q, &parent_sd->s_children);
				spin_unlock(&configfs_dirent_lock);
			}
			for (p=q->next; p!= &parent_sd->s_children; p=p->next) {
				struct configfs_dirent *next;
				const char * name;
				int len;
				struct inode *inode = NULL;

				next = list_entry(p, struct configfs_dirent,
						   s_sibling);
				if (!next->s_element)
					continue;

				name = configfs_get_name(next);
				len = strlen(name);

				spin_lock(&configfs_dirent_lock);
				dentry = next->s_dentry;
				if (dentry)
					inode = dentry->d_inode;
				if (inode)
					ino = inode->i_ino;
				spin_unlock(&configfs_dirent_lock);
				if (!inode)
					ino = iunique(sb, 2);

				if (filldir(dirent, name, len, filp->f_pos, ino,
						 dt_type(next)) < 0)
					return 0;

				spin_lock(&configfs_dirent_lock);
				list_move(q, p);
				spin_unlock(&configfs_dirent_lock);
				p = q;
				filp->f_pos++;
			}
	}
	return 0;
}

static loff_t configfs_dir_lseek(struct file * file, loff_t offset, int origin)
{
	struct dentry * dentry = file->f_path.dentry;

	mutex_lock(&dentry->d_inode->i_mutex);
	switch (origin) {
		case 1:
			offset += file->f_pos;
		case 0:
			if (offset >= 0)
				break;
		default:
			mutex_unlock(&file->f_path.dentry->d_inode->i_mutex);
			return -EINVAL;
	}
	if (offset != file->f_pos) {
		file->f_pos = offset;
		if (file->f_pos >= 2) {
			struct configfs_dirent *sd = dentry->d_fsdata;
			struct configfs_dirent *cursor = file->private_data;
			struct list_head *p;
			loff_t n = file->f_pos - 2;

			spin_lock(&configfs_dirent_lock);
			list_del(&cursor->s_sibling);
			p = sd->s_children.next;
			while (n && p != &sd->s_children) {
				struct configfs_dirent *next;
				next = list_entry(p, struct configfs_dirent,
						   s_sibling);
				if (next->s_element)
					n--;
				p = p->next;
			}
			list_add_tail(&cursor->s_sibling, p);
			spin_unlock(&configfs_dirent_lock);
		}
	}
	mutex_unlock(&dentry->d_inode->i_mutex);
	return offset;
}

const struct file_operations configfs_dir_operations = {
	.open		= configfs_dir_open,
	.release	= configfs_dir_close,
	.llseek		= configfs_dir_lseek,
	.read		= generic_read_dir,
	.readdir	= configfs_readdir,
};

int configfs_register_subsystem(struct configfs_subsystem *subsys)
{
	int err;
	struct config_group *group = &subsys->su_group;
	struct qstr name;
	struct dentry *dentry;
	struct dentry *root;
	struct configfs_dirent *sd;

	root = configfs_pin_fs();
	if (IS_ERR(root))
		return PTR_ERR(root);

	if (!group->cg_item.ci_name)
		group->cg_item.ci_name = group->cg_item.ci_namebuf;

	sd = root->d_fsdata;
	link_group(to_config_group(sd->s_element), group);

	mutex_lock_nested(&root->d_inode->i_mutex, I_MUTEX_PARENT);

	name.name = group->cg_item.ci_name;
	name.len = strlen(name.name);
	name.hash = full_name_hash(name.name, name.len);

	err = -ENOMEM;
	dentry = d_alloc(root, &name);
	if (dentry) {
		d_add(dentry, NULL);

		err = configfs_attach_group(sd->s_element, &group->cg_item,
					    dentry);
		if (err) {
			BUG_ON(dentry->d_inode);
			d_drop(dentry);
			dput(dentry);
		} else {
			spin_lock(&configfs_dirent_lock);
			configfs_dir_set_ready(dentry->d_fsdata);
			spin_unlock(&configfs_dirent_lock);
		}
	}

	mutex_unlock(&root->d_inode->i_mutex);

	if (err) {
		unlink_group(group);
		configfs_release_fs();
	}

	return err;
}

void configfs_unregister_subsystem(struct configfs_subsystem *subsys)
{
	struct config_group *group = &subsys->su_group;
	struct dentry *dentry = group->cg_item.ci_dentry;
	struct dentry *root = dentry->d_sb->s_root;

	if (dentry->d_parent != root) {
		printk(KERN_ERR "configfs: Tried to unregister non-subsystem!\n");
		return;
	}

	mutex_lock_nested(&root->d_inode->i_mutex,
			  I_MUTEX_PARENT);
	mutex_lock_nested(&dentry->d_inode->i_mutex, I_MUTEX_CHILD);
	mutex_lock(&configfs_symlink_mutex);
	spin_lock(&configfs_dirent_lock);
	if (configfs_detach_prep(dentry, NULL)) {
		printk(KERN_ERR "configfs: Tried to unregister non-empty subsystem!\n");
	}
	spin_unlock(&configfs_dirent_lock);
	mutex_unlock(&configfs_symlink_mutex);
	configfs_detach_group(&group->cg_item);
	dentry->d_inode->i_flags |= S_DEAD;
	dont_mount(dentry);
	mutex_unlock(&dentry->d_inode->i_mutex);

	d_delete(dentry);

	mutex_unlock(&root->d_inode->i_mutex);

	dput(dentry);

	unlink_group(group);
	configfs_release_fs();
}

EXPORT_SYMBOL(configfs_register_subsystem);
EXPORT_SYMBOL(configfs_unregister_subsystem);
