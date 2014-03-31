/*
 * security/tomoyo/realpath.c
 *
 * Copyright (C) 2005-2011  NTT DATA CORPORATION
 */

#include "common.h"
#include <linux/magic.h>

char *tomoyo_encode2(const char *str, int str_len)
{
	int i;
	int len = 0;
	const char *p = str;
	char *cp;
	char *cp0;

	if (!p)
		return NULL;
	for (i = 0; i < str_len; i++) {
		const unsigned char c = p[i];

		if (c == '\\')
			len += 2;
		else if (c > ' ' && c < 127)
			len++;
		else
			len += 4;
	}
	len++;
	
	cp = kzalloc(len + 10, GFP_NOFS);
	if (!cp)
		return NULL;
	cp0 = cp;
	p = str;
	for (i = 0; i < str_len; i++) {
		const unsigned char c = p[i];

		if (c == '\\') {
			*cp++ = '\\';
			*cp++ = '\\';
		} else if (c > ' ' && c < 127) {
			*cp++ = c;
		} else {
			*cp++ = '\\';
			*cp++ = (c >> 6) + '0';
			*cp++ = ((c >> 3) & 7) + '0';
			*cp++ = (c & 7) + '0';
		}
	}
	return cp0;
}

char *tomoyo_encode(const char *str)
{
	return str ? tomoyo_encode2(str, strlen(str)) : NULL;
}

static char *tomoyo_get_absolute_path(struct path *path, char * const buffer,
				      const int buflen)
{
	char *pos = ERR_PTR(-ENOMEM);
	if (buflen >= 256) {
		
		pos = d_absolute_path(path, buffer, buflen - 1);
		if (!IS_ERR(pos) && *pos == '/' && pos[1]) {
			struct inode *inode = path->dentry->d_inode;
			if (inode && S_ISDIR(inode->i_mode)) {
				buffer[buflen - 2] = '/';
				buffer[buflen - 1] = '\0';
			}
		}
	}
	return pos;
}

static char *tomoyo_get_dentry_path(struct dentry *dentry, char * const buffer,
				    const int buflen)
{
	char *pos = ERR_PTR(-ENOMEM);
	if (buflen >= 256) {
		pos = dentry_path_raw(dentry, buffer, buflen - 1);
		if (!IS_ERR(pos) && *pos == '/' && pos[1]) {
			struct inode *inode = dentry->d_inode;
			if (inode && S_ISDIR(inode->i_mode)) {
				buffer[buflen - 2] = '/';
				buffer[buflen - 1] = '\0';
			}
		}
	}
	return pos;
}

static char *tomoyo_get_local_path(struct dentry *dentry, char * const buffer,
				   const int buflen)
{
	struct super_block *sb = dentry->d_sb;
	char *pos = tomoyo_get_dentry_path(dentry, buffer, buflen);
	if (IS_ERR(pos))
		return pos;
	
	if (sb->s_magic == PROC_SUPER_MAGIC && *pos == '/') {
		char *ep;
		const pid_t pid = (pid_t) simple_strtoul(pos + 1, &ep, 10);
		if (*ep == '/' && pid && pid ==
		    task_tgid_nr_ns(current, sb->s_fs_info)) {
			pos = ep - 5;
			if (pos < buffer)
				goto out;
			memmove(pos, "/self", 5);
		}
		goto prepend_filesystem_name;
	}
	
	if (!MAJOR(sb->s_dev))
		goto prepend_filesystem_name;
	{
		struct inode *inode = sb->s_root->d_inode;
		if (inode->i_op && !inode->i_op->rename)
			goto prepend_filesystem_name;
	}
	
	{
		char name[64];
		int name_len;
		const dev_t dev = sb->s_dev;
		name[sizeof(name) - 1] = '\0';
		snprintf(name, sizeof(name) - 1, "dev(%u,%u):", MAJOR(dev),
			 MINOR(dev));
		name_len = strlen(name);
		pos -= name_len;
		if (pos < buffer)
			goto out;
		memmove(pos, name, name_len);
		return pos;
	}
	
prepend_filesystem_name:
	{
		const char *name = sb->s_type->name;
		const int name_len = strlen(name);
		pos -= name_len + 1;
		if (pos < buffer)
			goto out;
		memmove(pos, name, name_len);
		pos[name_len] = ':';
	}
	return pos;
out:
	return ERR_PTR(-ENOMEM);
}

static char *tomoyo_get_socket_name(struct path *path, char * const buffer,
				    const int buflen)
{
	struct inode *inode = path->dentry->d_inode;
	struct socket *sock = inode ? SOCKET_I(inode) : NULL;
	struct sock *sk = sock ? sock->sk : NULL;
	if (sk) {
		snprintf(buffer, buflen, "socket:[family=%u:type=%u:"
			 "protocol=%u]", sk->sk_family, sk->sk_type,
			 sk->sk_protocol);
	} else {
		snprintf(buffer, buflen, "socket:[unknown]");
	}
	return buffer;
}

char *tomoyo_realpath_from_path(struct path *path)
{
	char *buf = NULL;
	char *name = NULL;
	unsigned int buf_len = PAGE_SIZE / 2;
	struct dentry *dentry = path->dentry;
	struct super_block *sb;
	if (!dentry)
		return NULL;
	sb = dentry->d_sb;
	while (1) {
		char *pos;
		struct inode *inode;
		buf_len <<= 1;
		kfree(buf);
		buf = kmalloc(buf_len, GFP_NOFS);
		if (!buf)
			break;
		
		buf[buf_len - 1] = '\0';
		
		if (sb->s_magic == SOCKFS_MAGIC) {
			pos = tomoyo_get_socket_name(path, buf, buf_len - 1);
			goto encode;
		}
		
		if (dentry->d_op && dentry->d_op->d_dname) {
			pos = dentry->d_op->d_dname(dentry, buf, buf_len - 1);
			goto encode;
		}
		inode = sb->s_root->d_inode;
		if (!path->mnt || (inode->i_op && !inode->i_op->rename))
			pos = tomoyo_get_local_path(path->dentry, buf,
						    buf_len - 1);
		
		else {
			pos = tomoyo_get_absolute_path(path, buf, buf_len - 1);
			if (pos == ERR_PTR(-EINVAL))
				pos = tomoyo_get_local_path(path->dentry, buf,
							    buf_len - 1);
		}
encode:
		if (IS_ERR(pos))
			continue;
		name = tomoyo_encode(pos);
		break;
	}
	kfree(buf);
	if (!name)
		tomoyo_warn_oom(__func__);
	return name;
}

char *tomoyo_realpath_nofollow(const char *pathname)
{
	struct path path;

	if (pathname && kern_path(pathname, 0, &path) == 0) {
		char *buf = tomoyo_realpath_from_path(&path);
		path_put(&path);
		return buf;
	}
	return NULL;
}
