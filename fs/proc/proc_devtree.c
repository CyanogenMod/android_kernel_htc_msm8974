/*
 * proc_devtree.c - handles /proc/device-tree
 *
 * Copyright 1997 Paul Mackerras
 */
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/time.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <asm/prom.h>
#include <asm/uaccess.h>
#include "internal.h"

static inline void set_node_proc_entry(struct device_node *np,
				       struct proc_dir_entry *de)
{
#ifdef HAVE_ARCH_DEVTREE_FIXUPS
	np->pde = de;
#endif
}

static struct proc_dir_entry *proc_device_tree;

static int property_proc_show(struct seq_file *m, void *v)
{
	struct property *pp = m->private;

	seq_write(m, pp->value, pp->length);
	return 0;
}

static int property_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, property_proc_show, PDE(inode)->data);
}

static const struct file_operations property_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= property_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};


static struct proc_dir_entry *
__proc_device_tree_add_prop(struct proc_dir_entry *de, struct property *pp,
		const char *name)
{
	struct proc_dir_entry *ent;

	ent = proc_create_data(name,
			       strncmp(name, "security-", 9) ? S_IRUGO : S_IRUSR,
			       de, &property_proc_fops, pp);
	if (ent == NULL)
		return NULL;

	if (!strncmp(name, "security-", 9))
		ent->size = 0; 
	else
		ent->size = pp->length;

	return ent;
}


void proc_device_tree_add_prop(struct proc_dir_entry *pde, struct property *prop)
{
	__proc_device_tree_add_prop(pde, prop, prop->name);
}

void proc_device_tree_remove_prop(struct proc_dir_entry *pde,
				  struct property *prop)
{
	remove_proc_entry(prop->name, pde);
}

void proc_device_tree_update_prop(struct proc_dir_entry *pde,
				  struct property *newprop,
				  struct property *oldprop)
{
	struct proc_dir_entry *ent;

	for (ent = pde->subdir; ent != NULL; ent = ent->next)
		if (ent->data == oldprop)
			break;
	if (ent == NULL) {
		printk(KERN_WARNING "device-tree: property \"%s\" "
		       " does not exist\n", oldprop->name);
	} else {
		ent->data = newprop;
		ent->size = newprop->length;
	}
}


static int duplicate_name(struct proc_dir_entry *de, const char *name)
{
	struct proc_dir_entry *ent;
	int found = 0;

	spin_lock(&proc_subdir_lock);

	for (ent = de->subdir; ent != NULL; ent = ent->next) {
		if (strcmp(ent->name, name) == 0) {
			found = 1;
			break;
		}
	}

	spin_unlock(&proc_subdir_lock);

	return found;
}

static const char *fixup_name(struct device_node *np, struct proc_dir_entry *de,
		const char *name)
{
	char *fixed_name;
	int fixup_len = strlen(name) + 2 + 1; 
	int i = 1, size;

realloc:
	fixed_name = kmalloc(fixup_len, GFP_KERNEL);
	if (fixed_name == NULL) {
		printk(KERN_ERR "device-tree: Out of memory trying to fixup "
				"name \"%s\"\n", name);
		return name;
	}

retry:
	size = snprintf(fixed_name, fixup_len, "%s#%d", name, i);
	size++; 

	if (size > fixup_len) {
		
		kfree(fixed_name);
		fixup_len = size;
		goto realloc;
	}

	if (duplicate_name(de, fixed_name)) {
		
		i++;
		goto retry;
	}

	printk(KERN_WARNING "device-tree: Duplicate name in %s, "
			"renamed to \"%s\"\n", np->full_name, fixed_name);

	return fixed_name;
}

void proc_device_tree_add_node(struct device_node *np,
			       struct proc_dir_entry *de)
{
	struct property *pp;
	struct proc_dir_entry *ent;
	struct device_node *child;
	const char *p;

	set_node_proc_entry(np, de);
	for (child = NULL; (child = of_get_next_child(np, child));) {
		
		p = strrchr(child->full_name, '/');
		if (!p)
			p = child->full_name;
		else
			++p;

		if (duplicate_name(de, p))
			p = fixup_name(np, de, p);

		ent = proc_mkdir(p, de);
		if (ent == NULL)
			break;
		proc_device_tree_add_node(child, ent);
	}
	of_node_put(child);

	for (pp = np->properties; pp != NULL; pp = pp->next) {
		p = pp->name;

		if (strchr(p, '/'))
			continue;

		if (duplicate_name(de, p))
			p = fixup_name(np, de, p);

		ent = __proc_device_tree_add_prop(de, pp, p);
		if (ent == NULL)
			break;
	}
}

void __init proc_device_tree_init(void)
{
	struct device_node *root;

	proc_device_tree = proc_mkdir("device-tree", NULL);
	if (proc_device_tree == NULL)
		return;
	root = of_find_node_by_path("/");
	if (root == NULL) {
		pr_debug("/proc/device-tree: can't find root\n");
		return;
	}
	proc_device_tree_add_node(root, proc_device_tree);
	of_node_put(root);
}
