/*
 * JFFS2 -- Journalling Flash File System, Version 2.
 *
 * Copyright © 2001-2007 Red Hat, Inc.
 *
 * Created by David Woodhouse <dwmw2@infradead.org>
 *
 * For licensing information, see the file 'LICENCE' in this directory.
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/namei.h>
#include "nodelist.h"

static void *jffs2_follow_link(struct dentry *dentry, struct nameidata *nd);

const struct inode_operations jffs2_symlink_inode_operations =
{
	.readlink =	generic_readlink,
	.follow_link =	jffs2_follow_link,
	.get_acl =	jffs2_get_acl,
	.setattr =	jffs2_setattr,
	.setxattr =	jffs2_setxattr,
	.getxattr =	jffs2_getxattr,
	.listxattr =	jffs2_listxattr,
	.removexattr =	jffs2_removexattr
};

static void *jffs2_follow_link(struct dentry *dentry, struct nameidata *nd)
{
	struct jffs2_inode_info *f = JFFS2_INODE_INFO(dentry->d_inode);
	char *p = (char *)f->target;


	if (!p) {
		pr_err("%s(): can't find symlink target\n", __func__);
		p = ERR_PTR(-EIO);
	}
	jffs2_dbg(1, "%s(): target path is '%s'\n",
		  __func__, (char *)f->target);

	nd_set_link(nd, p);

	return NULL;
}

