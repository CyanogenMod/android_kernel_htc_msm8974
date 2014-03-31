/*
 * AppArmor security module
 *
 * This file contains AppArmor function for pathnames
 *
 * Copyright (C) 1998-2008 Novell/SUSE
 * Copyright 2009-2010 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 */

#include <linux/magic.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/nsproxy.h>
#include <linux/path.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/fs_struct.h>

#include "include/apparmor.h"
#include "include/path.h"
#include "include/policy.h"


static int prepend(char **buffer, int buflen, const char *str, int namelen)
{
	buflen -= namelen;
	if (buflen < 0)
		return -ENAMETOOLONG;
	*buffer -= namelen;
	memcpy(*buffer, str, namelen);
	return 0;
}

#define CHROOT_NSCONNECT (PATH_CHROOT_REL | PATH_CHROOT_NSCONNECT)

static int d_namespace_path(struct path *path, char *buf, int buflen,
			    char **name, int flags)
{
	char *res;
	int error = 0;
	int connected = 1;

	if (path->mnt->mnt_flags & MNT_INTERNAL) {
		
		res = dentry_path(path->dentry, buf, buflen);
		*name = res;
		if (IS_ERR(res)) {
			*name = buf;
			return PTR_ERR(res);
		}
		if (path->dentry->d_sb->s_magic == PROC_SUPER_MAGIC &&
		    strncmp(*name, "/sys/", 5) == 0) {
			return prepend(name, *name - buf, "/proc", 5);
		}
		return 0;
	}

	
	if (flags & PATH_CHROOT_REL) {
		struct path root;
		get_fs_root(current->fs, &root);
		res = __d_path(path, &root, buf, buflen);
		path_put(&root);
	} else {
		res = d_absolute_path(path, buf, buflen);
		if (!our_mnt(path->mnt))
			connected = 0;
	}

	if (!res || IS_ERR(res)) {
		connected = 0;
		res = dentry_path_raw(path->dentry, buf, buflen);
		if (IS_ERR(res)) {
			error = PTR_ERR(res);
			*name = buf;
			goto out;
		};
	} else if (!our_mnt(path->mnt))
		connected = 0;

	*name = res;

	if (d_unlinked(path->dentry) && path->dentry->d_inode &&
	    !(flags & PATH_MEDIATE_DELETED)) {
			error = -ENOENT;
			goto out;
	}

	if (!connected) {
		if (!(flags & PATH_CONNECT_PATH) &&
			   !(((flags & CHROOT_NSCONNECT) == CHROOT_NSCONNECT) &&
			     our_mnt(path->mnt))) {
			error = -EACCES;
			if (*res == '/')
				*name = res + 1;
		}
	}

out:
	return error;
}

static int get_name_to_buffer(struct path *path, int flags, char *buffer,
			      int size, char **name, const char **info)
{
	int adjust = (flags & PATH_IS_DIR) ? 1 : 0;
	int error = d_namespace_path(path, buffer, size - adjust, name, flags);

	if (!error && (flags & PATH_IS_DIR) && (*name)[1] != '\0')
		strcpy(&buffer[size - 2], "/");

	if (info && error) {
		if (error == -ENOENT)
			*info = "Failed name lookup - deleted entry";
		else if (error == -ESTALE)
			*info = "Failed name lookup - disconnected path";
		else if (error == -ENAMETOOLONG)
			*info = "Failed name lookup - name too long";
		else
			*info = "Failed name lookup";
	}

	return error;
}

int aa_path_name(struct path *path, int flags, char **buffer, const char **name,
		 const char **info)
{
	char *buf, *str = NULL;
	int size = 256;
	int error;

	*name = NULL;
	*buffer = NULL;
	for (;;) {
		
		buf = kmalloc(size, GFP_KERNEL);
		if (!buf)
			return -ENOMEM;

		error = get_name_to_buffer(path, flags, buf, size, &str, info);
		if (error != -ENAMETOOLONG)
			break;

		kfree(buf);
		size <<= 1;
		if (size > aa_g_path_max)
			return -ENAMETOOLONG;
		*info = NULL;
	}
	*buffer = buf;
	*name = str;

	return error;
}
