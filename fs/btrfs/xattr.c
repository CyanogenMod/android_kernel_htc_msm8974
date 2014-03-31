/*
 * Copyright (C) 2007 Red Hat.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License v2 as published by the Free Software Foundation.
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
 */

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/rwsem.h>
#include <linux/xattr.h>
#include <linux/security.h>
#include "ctree.h"
#include "btrfs_inode.h"
#include "transaction.h"
#include "xattr.h"
#include "disk-io.h"


ssize_t __btrfs_getxattr(struct inode *inode, const char *name,
				void *buffer, size_t size)
{
	struct btrfs_dir_item *di;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_path *path;
	struct extent_buffer *leaf;
	int ret = 0;
	unsigned long data_ptr;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	
	di = btrfs_lookup_xattr(NULL, root, path, btrfs_ino(inode), name,
				strlen(name), 0);
	if (!di) {
		ret = -ENODATA;
		goto out;
	} else if (IS_ERR(di)) {
		ret = PTR_ERR(di);
		goto out;
	}

	leaf = path->nodes[0];
	
	if (!size) {
		ret = btrfs_dir_data_len(leaf, di);
		goto out;
	}

	
	if (btrfs_dir_data_len(leaf, di) > size) {
		ret = -ERANGE;
		goto out;
	}

	data_ptr = (unsigned long)((char *)(di + 1) +
				   btrfs_dir_name_len(leaf, di));
	read_extent_buffer(leaf, buffer, data_ptr,
			   btrfs_dir_data_len(leaf, di));
	ret = btrfs_dir_data_len(leaf, di);

out:
	btrfs_free_path(path);
	return ret;
}

static int do_setxattr(struct btrfs_trans_handle *trans,
		       struct inode *inode, const char *name,
		       const void *value, size_t size, int flags)
{
	struct btrfs_dir_item *di;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_path *path;
	size_t name_len = strlen(name);
	int ret = 0;

	if (name_len + size > BTRFS_MAX_XATTR_SIZE(root))
		return -ENOSPC;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	if (flags & XATTR_REPLACE) {
		di = btrfs_lookup_xattr(trans, root, path, btrfs_ino(inode), name,
					name_len, -1);
		if (IS_ERR(di)) {
			ret = PTR_ERR(di);
			goto out;
		} else if (!di) {
			ret = -ENODATA;
			goto out;
		}
		ret = btrfs_delete_one_dir_name(trans, root, path, di);
		if (ret)
			goto out;
		btrfs_release_path(path);

		if (!value)
			goto out;
	}

again:
	ret = btrfs_insert_xattr_item(trans, root, path, btrfs_ino(inode),
				      name, name_len, value, size);
	if (ret == -EOVERFLOW)
		ret = -EEXIST;

	if (ret == -EEXIST) {
		if (flags & XATTR_CREATE)
			goto out;
		btrfs_release_path(path);
		di = btrfs_lookup_xattr(trans, root, path, btrfs_ino(inode),
					name, name_len, -1);
		if (IS_ERR(di)) {
			ret = PTR_ERR(di);
			goto out;
		} else if (!di) {
			
			btrfs_release_path(path);
			goto again;
		}

		ret = btrfs_delete_one_dir_name(trans, root, path, di);
		if (ret)
			goto out;

		if (value) {
			btrfs_release_path(path);
			goto again;
		}
	}
out:
	btrfs_free_path(path);
	return ret;
}

int __btrfs_setxattr(struct btrfs_trans_handle *trans,
		     struct inode *inode, const char *name,
		     const void *value, size_t size, int flags)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	int ret;

	if (trans)
		return do_setxattr(trans, inode, name, value, size, flags);

	trans = btrfs_start_transaction(root, 2);
	if (IS_ERR(trans))
		return PTR_ERR(trans);

	ret = do_setxattr(trans, inode, name, value, size, flags);
	if (ret)
		goto out;

	inode->i_ctime = CURRENT_TIME;
	ret = btrfs_update_inode(trans, root, inode);
	BUG_ON(ret);
out:
	btrfs_end_transaction(trans, root);
	return ret;
}

ssize_t btrfs_listxattr(struct dentry *dentry, char *buffer, size_t size)
{
	struct btrfs_key key, found_key;
	struct inode *inode = dentry->d_inode;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_path *path;
	struct extent_buffer *leaf;
	struct btrfs_dir_item *di;
	int ret = 0, slot;
	size_t total_size = 0, size_left = size;
	unsigned long name_ptr;
	size_t name_len;

	key.objectid = btrfs_ino(inode);
	btrfs_set_key_type(&key, BTRFS_XATTR_ITEM_KEY);
	key.offset = 0;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;
	path->reada = 2;

	
	ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
	if (ret < 0)
		goto err;

	while (1) {
		leaf = path->nodes[0];
		slot = path->slots[0];

		
		if (slot >= btrfs_header_nritems(leaf)) {
			ret = btrfs_next_leaf(root, path);
			if (ret < 0)
				goto err;
			else if (ret > 0)
				break;
			continue;
		}

		btrfs_item_key_to_cpu(leaf, &found_key, slot);

		
		if (found_key.objectid != key.objectid)
			break;
		if (btrfs_key_type(&found_key) != BTRFS_XATTR_ITEM_KEY)
			break;

		di = btrfs_item_ptr(leaf, slot, struct btrfs_dir_item);
		if (verify_dir_item(root, leaf, di))
			continue;

		name_len = btrfs_dir_name_len(leaf, di);
		total_size += name_len + 1;

		
		if (!size)
			goto next;

		if (!buffer || (name_len + 1) > size_left) {
			ret = -ERANGE;
			goto err;
		}

		name_ptr = (unsigned long)(di + 1);
		read_extent_buffer(leaf, buffer, name_ptr, name_len);
		buffer[name_len] = '\0';

		size_left -= name_len + 1;
		buffer += name_len + 1;
next:
		path->slots[0]++;
	}
	ret = total_size;

err:
	btrfs_free_path(path);

	return ret;
}

const struct xattr_handler *btrfs_xattr_handlers[] = {
#ifdef CONFIG_BTRFS_FS_POSIX_ACL
	&btrfs_xattr_acl_access_handler,
	&btrfs_xattr_acl_default_handler,
#endif
	NULL,
};

static bool btrfs_is_valid_xattr(const char *name)
{
	return !strncmp(name, XATTR_SECURITY_PREFIX,
			XATTR_SECURITY_PREFIX_LEN) ||
	       !strncmp(name, XATTR_SYSTEM_PREFIX, XATTR_SYSTEM_PREFIX_LEN) ||
	       !strncmp(name, XATTR_TRUSTED_PREFIX, XATTR_TRUSTED_PREFIX_LEN) ||
	       !strncmp(name, XATTR_USER_PREFIX, XATTR_USER_PREFIX_LEN);
}

ssize_t btrfs_getxattr(struct dentry *dentry, const char *name,
		       void *buffer, size_t size)
{
	if (!strncmp(name, XATTR_SYSTEM_PREFIX, XATTR_SYSTEM_PREFIX_LEN))
		return generic_getxattr(dentry, name, buffer, size);

	if (!btrfs_is_valid_xattr(name))
		return -EOPNOTSUPP;
	return __btrfs_getxattr(dentry->d_inode, name, buffer, size);
}

int btrfs_setxattr(struct dentry *dentry, const char *name, const void *value,
		   size_t size, int flags)
{
	struct btrfs_root *root = BTRFS_I(dentry->d_inode)->root;

	if (btrfs_root_readonly(root))
		return -EROFS;

	if (!strncmp(name, XATTR_SYSTEM_PREFIX, XATTR_SYSTEM_PREFIX_LEN))
		return generic_setxattr(dentry, name, value, size, flags);

	if (!btrfs_is_valid_xattr(name))
		return -EOPNOTSUPP;

	if (size == 0)
		value = "";  

	return __btrfs_setxattr(NULL, dentry->d_inode, name, value, size,
				flags);
}

int btrfs_removexattr(struct dentry *dentry, const char *name)
{
	struct btrfs_root *root = BTRFS_I(dentry->d_inode)->root;

	if (btrfs_root_readonly(root))
		return -EROFS;

	if (!strncmp(name, XATTR_SYSTEM_PREFIX, XATTR_SYSTEM_PREFIX_LEN))
		return generic_removexattr(dentry, name);

	if (!btrfs_is_valid_xattr(name))
		return -EOPNOTSUPP;

	return __btrfs_setxattr(NULL, dentry->d_inode, name, NULL, 0,
				XATTR_REPLACE);
}

int btrfs_initxattrs(struct inode *inode, const struct xattr *xattr_array,
		     void *fs_info)
{
	const struct xattr *xattr;
	struct btrfs_trans_handle *trans = fs_info;
	char *name;
	int err = 0;

	for (xattr = xattr_array; xattr->name != NULL; xattr++) {
		name = kmalloc(XATTR_SECURITY_PREFIX_LEN +
			       strlen(xattr->name) + 1, GFP_NOFS);
		if (!name) {
			err = -ENOMEM;
			break;
		}
		strcpy(name, XATTR_SECURITY_PREFIX);
		strcpy(name + XATTR_SECURITY_PREFIX_LEN, xattr->name);
		err = __btrfs_setxattr(trans, inode, name,
				       xattr->value, xattr->value_len, 0);
		kfree(name);
		if (err < 0)
			break;
	}
	return err;
}

int btrfs_xattr_security_init(struct btrfs_trans_handle *trans,
			      struct inode *inode, struct inode *dir,
			      const struct qstr *qstr)
{
	return security_inode_init_security(inode, dir, qstr,
					    &btrfs_initxattrs, trans);
}
