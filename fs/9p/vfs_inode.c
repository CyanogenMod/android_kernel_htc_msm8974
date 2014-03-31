/*
 *  linux/fs/9p/vfs_inode.c
 *
 * This file contains vfs inode ops for the 9P2000 protocol.
 *
 *  Copyright (C) 2004 by Eric Van Hensbergen <ericvh@gmail.com>
 *  Copyright (C) 2002 by Ron Minnich <rminnich@lanl.gov>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to:
 *  Free Software Foundation
 *  51 Franklin Street, Fifth Floor
 *  Boston, MA  02111-1301  USA
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/pagemap.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/inet.h>
#include <linux/namei.h>
#include <linux/idr.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/xattr.h>
#include <linux/posix_acl.h>
#include <net/9p/9p.h>
#include <net/9p/client.h>

#include "v9fs.h"
#include "v9fs_vfs.h"
#include "fid.h"
#include "cache.h"
#include "xattr.h"
#include "acl.h"

static const struct inode_operations v9fs_dir_inode_operations;
static const struct inode_operations v9fs_dir_inode_operations_dotu;
static const struct inode_operations v9fs_file_inode_operations;
static const struct inode_operations v9fs_symlink_inode_operations;


static u32 unixmode2p9mode(struct v9fs_session_info *v9ses, umode_t mode)
{
	int res;
	res = mode & 0777;
	if (S_ISDIR(mode))
		res |= P9_DMDIR;
	if (v9fs_proto_dotu(v9ses)) {
		if (v9ses->nodev == 0) {
			if (S_ISSOCK(mode))
				res |= P9_DMSOCKET;
			if (S_ISFIFO(mode))
				res |= P9_DMNAMEDPIPE;
			if (S_ISBLK(mode))
				res |= P9_DMDEVICE;
			if (S_ISCHR(mode))
				res |= P9_DMDEVICE;
		}

		if ((mode & S_ISUID) == S_ISUID)
			res |= P9_DMSETUID;
		if ((mode & S_ISGID) == S_ISGID)
			res |= P9_DMSETGID;
		if ((mode & S_ISVTX) == S_ISVTX)
			res |= P9_DMSETVTX;
	}
	return res;
}

static int p9mode2perm(struct v9fs_session_info *v9ses,
		       struct p9_wstat *stat)
{
	int res;
	int mode = stat->mode;

	res = mode & S_IALLUGO;
	if (v9fs_proto_dotu(v9ses)) {
		if ((mode & P9_DMSETUID) == P9_DMSETUID)
			res |= S_ISUID;

		if ((mode & P9_DMSETGID) == P9_DMSETGID)
			res |= S_ISGID;

		if ((mode & P9_DMSETVTX) == P9_DMSETVTX)
			res |= S_ISVTX;
	}
	return res;
}

static umode_t p9mode2unixmode(struct v9fs_session_info *v9ses,
			       struct p9_wstat *stat, dev_t *rdev)
{
	int res;
	u32 mode = stat->mode;

	*rdev = 0;
	res = p9mode2perm(v9ses, stat);

	if ((mode & P9_DMDIR) == P9_DMDIR)
		res |= S_IFDIR;
	else if ((mode & P9_DMSYMLINK) && (v9fs_proto_dotu(v9ses)))
		res |= S_IFLNK;
	else if ((mode & P9_DMSOCKET) && (v9fs_proto_dotu(v9ses))
		 && (v9ses->nodev == 0))
		res |= S_IFSOCK;
	else if ((mode & P9_DMNAMEDPIPE) && (v9fs_proto_dotu(v9ses))
		 && (v9ses->nodev == 0))
		res |= S_IFIFO;
	else if ((mode & P9_DMDEVICE) && (v9fs_proto_dotu(v9ses))
		 && (v9ses->nodev == 0)) {
		char type = 0, ext[32];
		int major = -1, minor = -1;

		strncpy(ext, stat->extension, sizeof(ext));
		sscanf(ext, "%c %u %u", &type, &major, &minor);
		switch (type) {
		case 'c':
			res |= S_IFCHR;
			break;
		case 'b':
			res |= S_IFBLK;
			break;
		default:
			p9_debug(P9_DEBUG_ERROR, "Unknown special type %c %s\n",
				 type, stat->extension);
		};
		*rdev = MKDEV(major, minor);
	} else
		res |= S_IFREG;

	return res;
}


int v9fs_uflags2omode(int uflags, int extended)
{
	int ret;

	ret = 0;
	switch (uflags&3) {
	default:
	case O_RDONLY:
		ret = P9_OREAD;
		break;

	case O_WRONLY:
		ret = P9_OWRITE;
		break;

	case O_RDWR:
		ret = P9_ORDWR;
		break;
	}

	if (uflags & O_TRUNC)
		ret |= P9_OTRUNC;

	if (extended) {
		if (uflags & O_EXCL)
			ret |= P9_OEXCL;

		if (uflags & O_APPEND)
			ret |= P9_OAPPEND;
	}

	return ret;
}


void
v9fs_blank_wstat(struct p9_wstat *wstat)
{
	wstat->type = ~0;
	wstat->dev = ~0;
	wstat->qid.type = ~0;
	wstat->qid.version = ~0;
	*((long long *)&wstat->qid.path) = ~0;
	wstat->mode = ~0;
	wstat->atime = ~0;
	wstat->mtime = ~0;
	wstat->length = ~0;
	wstat->name = NULL;
	wstat->uid = NULL;
	wstat->gid = NULL;
	wstat->muid = NULL;
	wstat->n_uid = ~0;
	wstat->n_gid = ~0;
	wstat->n_muid = ~0;
	wstat->extension = NULL;
}

struct inode *v9fs_alloc_inode(struct super_block *sb)
{
	struct v9fs_inode *v9inode;
	v9inode = (struct v9fs_inode *)kmem_cache_alloc(v9fs_inode_cache,
							GFP_KERNEL);
	if (!v9inode)
		return NULL;
#ifdef CONFIG_9P_FSCACHE
	v9inode->fscache = NULL;
	spin_lock_init(&v9inode->fscache_lock);
#endif
	v9inode->writeback_fid = NULL;
	v9inode->cache_validity = 0;
	mutex_init(&v9inode->v_mutex);
	return &v9inode->vfs_inode;
}


static void v9fs_i_callback(struct rcu_head *head)
{
	struct inode *inode = container_of(head, struct inode, i_rcu);
	kmem_cache_free(v9fs_inode_cache, V9FS_I(inode));
}

void v9fs_destroy_inode(struct inode *inode)
{
	call_rcu(&inode->i_rcu, v9fs_i_callback);
}

int v9fs_init_inode(struct v9fs_session_info *v9ses,
		    struct inode *inode, umode_t mode, dev_t rdev)
{
	int err = 0;

	inode_init_owner(inode, NULL, mode);
	inode->i_blocks = 0;
	inode->i_rdev = rdev;
	inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
	inode->i_mapping->a_ops = &v9fs_addr_operations;

	switch (mode & S_IFMT) {
	case S_IFIFO:
	case S_IFBLK:
	case S_IFCHR:
	case S_IFSOCK:
		if (v9fs_proto_dotl(v9ses)) {
			inode->i_op = &v9fs_file_inode_operations_dotl;
		} else if (v9fs_proto_dotu(v9ses)) {
			inode->i_op = &v9fs_file_inode_operations;
		} else {
			p9_debug(P9_DEBUG_ERROR,
				 "special files without extended mode\n");
			err = -EINVAL;
			goto error;
		}
		init_special_inode(inode, inode->i_mode, inode->i_rdev);
		break;
	case S_IFREG:
		if (v9fs_proto_dotl(v9ses)) {
			inode->i_op = &v9fs_file_inode_operations_dotl;
			if (v9ses->cache)
				inode->i_fop =
					&v9fs_cached_file_operations_dotl;
			else
				inode->i_fop = &v9fs_file_operations_dotl;
		} else {
			inode->i_op = &v9fs_file_inode_operations;
			if (v9ses->cache)
				inode->i_fop = &v9fs_cached_file_operations;
			else
				inode->i_fop = &v9fs_file_operations;
		}

		break;
	case S_IFLNK:
		if (!v9fs_proto_dotu(v9ses) && !v9fs_proto_dotl(v9ses)) {
			p9_debug(P9_DEBUG_ERROR,
				 "extended modes used with legacy protocol\n");
			err = -EINVAL;
			goto error;
		}

		if (v9fs_proto_dotl(v9ses))
			inode->i_op = &v9fs_symlink_inode_operations_dotl;
		else
			inode->i_op = &v9fs_symlink_inode_operations;

		break;
	case S_IFDIR:
		inc_nlink(inode);
		if (v9fs_proto_dotl(v9ses))
			inode->i_op = &v9fs_dir_inode_operations_dotl;
		else if (v9fs_proto_dotu(v9ses))
			inode->i_op = &v9fs_dir_inode_operations_dotu;
		else
			inode->i_op = &v9fs_dir_inode_operations;

		if (v9fs_proto_dotl(v9ses))
			inode->i_fop = &v9fs_dir_operations_dotl;
		else
			inode->i_fop = &v9fs_dir_operations;

		break;
	default:
		p9_debug(P9_DEBUG_ERROR, "BAD mode 0x%hx S_IFMT 0x%x\n",
			 mode, mode & S_IFMT);
		err = -EINVAL;
		goto error;
	}
error:
	return err;

}


struct inode *v9fs_get_inode(struct super_block *sb, umode_t mode, dev_t rdev)
{
	int err;
	struct inode *inode;
	struct v9fs_session_info *v9ses = sb->s_fs_info;

	p9_debug(P9_DEBUG_VFS, "super block: %p mode: %ho\n", sb, mode);

	inode = new_inode(sb);
	if (!inode) {
		pr_warn("%s (%d): Problem allocating inode\n",
			__func__, task_pid_nr(current));
		return ERR_PTR(-ENOMEM);
	}
	err = v9fs_init_inode(v9ses, inode, mode, rdev);
	if (err) {
		iput(inode);
		return ERR_PTR(err);
	}
	return inode;
}



void v9fs_evict_inode(struct inode *inode)
{
	struct v9fs_inode *v9inode = V9FS_I(inode);

	truncate_inode_pages(inode->i_mapping, 0);
	end_writeback(inode);
	filemap_fdatawrite(inode->i_mapping);

#ifdef CONFIG_9P_FSCACHE
	v9fs_cache_inode_put_cookie(inode);
#endif
	
	if (v9inode->writeback_fid) {
		p9_client_clunk(v9inode->writeback_fid);
		v9inode->writeback_fid = NULL;
	}
}

static int v9fs_test_inode(struct inode *inode, void *data)
{
	int umode;
	dev_t rdev;
	struct v9fs_inode *v9inode = V9FS_I(inode);
	struct p9_wstat *st = (struct p9_wstat *)data;
	struct v9fs_session_info *v9ses = v9fs_inode2v9ses(inode);

	umode = p9mode2unixmode(v9ses, st, &rdev);
	
	if ((inode->i_mode & S_IFMT) != (umode & S_IFMT))
		return 0;

	
	if (memcmp(&v9inode->qid.version,
		   &st->qid.version, sizeof(v9inode->qid.version)))
		return 0;

	if (v9inode->qid.type != st->qid.type)
		return 0;
	return 1;
}

static int v9fs_test_new_inode(struct inode *inode, void *data)
{
	return 0;
}

static int v9fs_set_inode(struct inode *inode,  void *data)
{
	struct v9fs_inode *v9inode = V9FS_I(inode);
	struct p9_wstat *st = (struct p9_wstat *)data;

	memcpy(&v9inode->qid, &st->qid, sizeof(st->qid));
	return 0;
}

static struct inode *v9fs_qid_iget(struct super_block *sb,
				   struct p9_qid *qid,
				   struct p9_wstat *st,
				   int new)
{
	dev_t rdev;
	int retval;
	umode_t umode;
	unsigned long i_ino;
	struct inode *inode;
	struct v9fs_session_info *v9ses = sb->s_fs_info;
	int (*test)(struct inode *, void *);

	if (new)
		test = v9fs_test_new_inode;
	else
		test = v9fs_test_inode;

	i_ino = v9fs_qid2ino(qid);
	inode = iget5_locked(sb, i_ino, test, v9fs_set_inode, st);
	if (!inode)
		return ERR_PTR(-ENOMEM);
	if (!(inode->i_state & I_NEW))
		return inode;
	inode->i_ino = i_ino;
	umode = p9mode2unixmode(v9ses, st, &rdev);
	retval = v9fs_init_inode(v9ses, inode, umode, rdev);
	if (retval)
		goto error;

	v9fs_stat2inode(st, inode, sb);
#ifdef CONFIG_9P_FSCACHE
	v9fs_cache_inode_get_cookie(inode);
#endif
	unlock_new_inode(inode);
	return inode;
error:
	unlock_new_inode(inode);
	iput(inode);
	return ERR_PTR(retval);

}

struct inode *
v9fs_inode_from_fid(struct v9fs_session_info *v9ses, struct p9_fid *fid,
		    struct super_block *sb, int new)
{
	struct p9_wstat *st;
	struct inode *inode = NULL;

	st = p9_client_stat(fid);
	if (IS_ERR(st))
		return ERR_CAST(st);

	inode = v9fs_qid_iget(sb, &st->qid, st, new);
	p9stat_free(st);
	kfree(st);
	return inode;
}

static int v9fs_at_to_dotl_flags(int flags)
{
	int rflags = 0;
	if (flags & AT_REMOVEDIR)
		rflags |= P9_DOTL_AT_REMOVEDIR;
	return rflags;
}


static int v9fs_remove(struct inode *dir, struct dentry *dentry, int flags)
{
	struct inode *inode;
	int retval = -EOPNOTSUPP;
	struct p9_fid *v9fid, *dfid;
	struct v9fs_session_info *v9ses;

	p9_debug(P9_DEBUG_VFS, "inode: %p dentry: %p rmdir: %x\n",
		 dir, dentry, flags);

	v9ses = v9fs_inode2v9ses(dir);
	inode = dentry->d_inode;
	dfid = v9fs_fid_lookup(dentry->d_parent);
	if (IS_ERR(dfid)) {
		retval = PTR_ERR(dfid);
		p9_debug(P9_DEBUG_VFS, "fid lookup failed %d\n", retval);
		return retval;
	}
	if (v9fs_proto_dotl(v9ses))
		retval = p9_client_unlinkat(dfid, dentry->d_name.name,
					    v9fs_at_to_dotl_flags(flags));
	if (retval == -EOPNOTSUPP) {
		
		v9fid = v9fs_fid_clone(dentry);
		if (IS_ERR(v9fid))
			return PTR_ERR(v9fid);
		retval = p9_client_remove(v9fid);
	}
	if (!retval) {
		if (flags & AT_REMOVEDIR) {
			clear_nlink(inode);
			drop_nlink(dir);
		} else
			drop_nlink(inode);

		v9fs_invalidate_inode_attr(inode);
		v9fs_invalidate_inode_attr(dir);
	}
	return retval;
}

static struct p9_fid *
v9fs_create(struct v9fs_session_info *v9ses, struct inode *dir,
		struct dentry *dentry, char *extension, u32 perm, u8 mode)
{
	int err;
	char *name;
	struct p9_fid *dfid, *ofid, *fid;
	struct inode *inode;

	p9_debug(P9_DEBUG_VFS, "name %s\n", dentry->d_name.name);

	err = 0;
	ofid = NULL;
	fid = NULL;
	name = (char *) dentry->d_name.name;
	dfid = v9fs_fid_lookup(dentry->d_parent);
	if (IS_ERR(dfid)) {
		err = PTR_ERR(dfid);
		p9_debug(P9_DEBUG_VFS, "fid lookup failed %d\n", err);
		return ERR_PTR(err);
	}

	
	ofid = p9_client_walk(dfid, 0, NULL, 1);
	if (IS_ERR(ofid)) {
		err = PTR_ERR(ofid);
		p9_debug(P9_DEBUG_VFS, "p9_client_walk failed %d\n", err);
		return ERR_PTR(err);
	}

	err = p9_client_fcreate(ofid, name, perm, mode, extension);
	if (err < 0) {
		p9_debug(P9_DEBUG_VFS, "p9_client_fcreate failed %d\n", err);
		goto error;
	}

	if (!(perm & P9_DMLINK)) {
		
		fid = p9_client_walk(dfid, 1, &name, 1);
		if (IS_ERR(fid)) {
			err = PTR_ERR(fid);
			p9_debug(P9_DEBUG_VFS,
				   "p9_client_walk failed %d\n", err);
			fid = NULL;
			goto error;
		}
		inode = v9fs_get_new_inode_from_fid(v9ses, fid, dir->i_sb);
		if (IS_ERR(inode)) {
			err = PTR_ERR(inode);
			p9_debug(P9_DEBUG_VFS,
				   "inode creation failed %d\n", err);
			goto error;
		}
		err = v9fs_fid_add(dentry, fid);
		if (err < 0)
			goto error;
		d_instantiate(dentry, inode);
	}
	return ofid;
error:
	if (ofid)
		p9_client_clunk(ofid);

	if (fid)
		p9_client_clunk(fid);

	return ERR_PTR(err);
}


static int
v9fs_vfs_create(struct inode *dir, struct dentry *dentry, umode_t mode,
		struct nameidata *nd)
{
	int err;
	u32 perm;
	int flags;
	struct file *filp;
	struct v9fs_inode *v9inode;
	struct v9fs_session_info *v9ses;
	struct p9_fid *fid, *inode_fid;

	err = 0;
	fid = NULL;
	v9ses = v9fs_inode2v9ses(dir);
	perm = unixmode2p9mode(v9ses, mode);
	if (nd)
		flags = nd->intent.open.flags;
	else
		flags = O_RDWR;

	fid = v9fs_create(v9ses, dir, dentry, NULL, perm,
				v9fs_uflags2omode(flags,
						v9fs_proto_dotu(v9ses)));
	if (IS_ERR(fid)) {
		err = PTR_ERR(fid);
		fid = NULL;
		goto error;
	}

	v9fs_invalidate_inode_attr(dir);
	
	if (nd) {
		v9inode = V9FS_I(dentry->d_inode);
		mutex_lock(&v9inode->v_mutex);
		if (v9ses->cache && !v9inode->writeback_fid &&
		    ((flags & O_ACCMODE) != O_RDONLY)) {
			inode_fid = v9fs_writeback_fid(dentry);
			if (IS_ERR(inode_fid)) {
				err = PTR_ERR(inode_fid);
				mutex_unlock(&v9inode->v_mutex);
				goto error;
			}
			v9inode->writeback_fid = (void *) inode_fid;
		}
		mutex_unlock(&v9inode->v_mutex);
		filp = lookup_instantiate_filp(nd, dentry, generic_file_open);
		if (IS_ERR(filp)) {
			err = PTR_ERR(filp);
			goto error;
		}

		filp->private_data = fid;
#ifdef CONFIG_9P_FSCACHE
		if (v9ses->cache)
			v9fs_cache_inode_set_cookie(dentry->d_inode, filp);
#endif
	} else
		p9_client_clunk(fid);

	return 0;

error:
	if (fid)
		p9_client_clunk(fid);

	return err;
}


static int v9fs_vfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
	int err;
	u32 perm;
	struct p9_fid *fid;
	struct v9fs_session_info *v9ses;

	p9_debug(P9_DEBUG_VFS, "name %s\n", dentry->d_name.name);
	err = 0;
	v9ses = v9fs_inode2v9ses(dir);
	perm = unixmode2p9mode(v9ses, mode | S_IFDIR);
	fid = v9fs_create(v9ses, dir, dentry, NULL, perm, P9_OREAD);
	if (IS_ERR(fid)) {
		err = PTR_ERR(fid);
		fid = NULL;
	} else {
		inc_nlink(dir);
		v9fs_invalidate_inode_attr(dir);
	}

	if (fid)
		p9_client_clunk(fid);

	return err;
}


struct dentry *v9fs_vfs_lookup(struct inode *dir, struct dentry *dentry,
				      struct nameidata *nameidata)
{
	struct dentry *res;
	struct super_block *sb;
	struct v9fs_session_info *v9ses;
	struct p9_fid *dfid, *fid;
	struct inode *inode;
	char *name;
	int result = 0;

	p9_debug(P9_DEBUG_VFS, "dir: %p dentry: (%s) %p nameidata: %p\n",
		 dir, dentry->d_name.name, dentry, nameidata);

	if (dentry->d_name.len > NAME_MAX)
		return ERR_PTR(-ENAMETOOLONG);

	sb = dir->i_sb;
	v9ses = v9fs_inode2v9ses(dir);
	
	dfid = v9fs_fid_lookup(dentry->d_parent);
	if (IS_ERR(dfid))
		return ERR_CAST(dfid);

	name = (char *) dentry->d_name.name;
	fid = p9_client_walk(dfid, 1, &name, 1);
	if (IS_ERR(fid)) {
		result = PTR_ERR(fid);
		if (result == -ENOENT) {
			inode = NULL;
			goto inst_out;
		}

		return ERR_PTR(result);
	}
	if (v9ses->cache)
		inode = v9fs_get_inode_from_fid(v9ses, fid, dir->i_sb);
	else
		inode = v9fs_get_new_inode_from_fid(v9ses, fid, dir->i_sb);
	if (IS_ERR(inode)) {
		result = PTR_ERR(inode);
		inode = NULL;
		goto error;
	}
	result = v9fs_fid_add(dentry, fid);
	if (result < 0)
		goto error_iput;
inst_out:
	res = d_materialise_unique(dentry, inode);
	if (!IS_ERR(res))
		return res;
	result = PTR_ERR(res);
error_iput:
	iput(inode);
error:
	p9_client_clunk(fid);

	return ERR_PTR(result);
}


int v9fs_vfs_unlink(struct inode *i, struct dentry *d)
{
	return v9fs_remove(i, d, 0);
}


int v9fs_vfs_rmdir(struct inode *i, struct dentry *d)
{
	return v9fs_remove(i, d, AT_REMOVEDIR);
}


int
v9fs_vfs_rename(struct inode *old_dir, struct dentry *old_dentry,
		struct inode *new_dir, struct dentry *new_dentry)
{
	int retval;
	struct inode *old_inode;
	struct inode *new_inode;
	struct v9fs_session_info *v9ses;
	struct p9_fid *oldfid;
	struct p9_fid *olddirfid;
	struct p9_fid *newdirfid;
	struct p9_wstat wstat;

	p9_debug(P9_DEBUG_VFS, "\n");
	retval = 0;
	old_inode = old_dentry->d_inode;
	new_inode = new_dentry->d_inode;
	v9ses = v9fs_inode2v9ses(old_inode);
	oldfid = v9fs_fid_lookup(old_dentry);
	if (IS_ERR(oldfid))
		return PTR_ERR(oldfid);

	olddirfid = v9fs_fid_clone(old_dentry->d_parent);
	if (IS_ERR(olddirfid)) {
		retval = PTR_ERR(olddirfid);
		goto done;
	}

	newdirfid = v9fs_fid_clone(new_dentry->d_parent);
	if (IS_ERR(newdirfid)) {
		retval = PTR_ERR(newdirfid);
		goto clunk_olddir;
	}

	down_write(&v9ses->rename_sem);
	if (v9fs_proto_dotl(v9ses)) {
		retval = p9_client_renameat(olddirfid, old_dentry->d_name.name,
					    newdirfid, new_dentry->d_name.name);
		if (retval == -EOPNOTSUPP)
			retval = p9_client_rename(oldfid, newdirfid,
						  new_dentry->d_name.name);
		if (retval != -EOPNOTSUPP)
			goto clunk_newdir;
	}
	if (old_dentry->d_parent != new_dentry->d_parent) {

		p9_debug(P9_DEBUG_ERROR, "old dir and new dir are different\n");
		retval = -EXDEV;
		goto clunk_newdir;
	}
	v9fs_blank_wstat(&wstat);
	wstat.muid = v9ses->uname;
	wstat.name = (char *) new_dentry->d_name.name;
	retval = p9_client_wstat(oldfid, &wstat);

clunk_newdir:
	if (!retval) {
		if (new_inode) {
			if (S_ISDIR(new_inode->i_mode))
				clear_nlink(new_inode);
			else
				drop_nlink(new_inode);
		}
		if (S_ISDIR(old_inode->i_mode)) {
			if (!new_inode)
				inc_nlink(new_dir);
			drop_nlink(old_dir);
		}
		v9fs_invalidate_inode_attr(old_inode);
		v9fs_invalidate_inode_attr(old_dir);
		v9fs_invalidate_inode_attr(new_dir);

		
		d_move(old_dentry, new_dentry);
	}
	up_write(&v9ses->rename_sem);
	p9_client_clunk(newdirfid);

clunk_olddir:
	p9_client_clunk(olddirfid);

done:
	return retval;
}


static int
v9fs_vfs_getattr(struct vfsmount *mnt, struct dentry *dentry,
		 struct kstat *stat)
{
	int err;
	struct v9fs_session_info *v9ses;
	struct p9_fid *fid;
	struct p9_wstat *st;

	p9_debug(P9_DEBUG_VFS, "dentry: %p\n", dentry);
	err = -EPERM;
	v9ses = v9fs_dentry2v9ses(dentry);
	if (v9ses->cache == CACHE_LOOSE || v9ses->cache == CACHE_FSCACHE) {
		generic_fillattr(dentry->d_inode, stat);
		return 0;
	}
	fid = v9fs_fid_lookup(dentry);
	if (IS_ERR(fid))
		return PTR_ERR(fid);

	st = p9_client_stat(fid);
	if (IS_ERR(st))
		return PTR_ERR(st);

	v9fs_stat2inode(st, dentry->d_inode, dentry->d_inode->i_sb);
	generic_fillattr(dentry->d_inode, stat);

	p9stat_free(st);
	kfree(st);
	return 0;
}


static int v9fs_vfs_setattr(struct dentry *dentry, struct iattr *iattr)
{
	int retval;
	struct v9fs_session_info *v9ses;
	struct p9_fid *fid;
	struct p9_wstat wstat;

	p9_debug(P9_DEBUG_VFS, "\n");
	retval = inode_change_ok(dentry->d_inode, iattr);
	if (retval)
		return retval;

	retval = -EPERM;
	v9ses = v9fs_dentry2v9ses(dentry);
	fid = v9fs_fid_lookup(dentry);
	if(IS_ERR(fid))
		return PTR_ERR(fid);

	v9fs_blank_wstat(&wstat);
	if (iattr->ia_valid & ATTR_MODE)
		wstat.mode = unixmode2p9mode(v9ses, iattr->ia_mode);

	if (iattr->ia_valid & ATTR_MTIME)
		wstat.mtime = iattr->ia_mtime.tv_sec;

	if (iattr->ia_valid & ATTR_ATIME)
		wstat.atime = iattr->ia_atime.tv_sec;

	if (iattr->ia_valid & ATTR_SIZE)
		wstat.length = iattr->ia_size;

	if (v9fs_proto_dotu(v9ses)) {
		if (iattr->ia_valid & ATTR_UID)
			wstat.n_uid = iattr->ia_uid;

		if (iattr->ia_valid & ATTR_GID)
			wstat.n_gid = iattr->ia_gid;
	}

	
	if (S_ISREG(dentry->d_inode->i_mode))
		filemap_write_and_wait(dentry->d_inode->i_mapping);

	retval = p9_client_wstat(fid, &wstat);
	if (retval < 0)
		return retval;

	if ((iattr->ia_valid & ATTR_SIZE) &&
	    iattr->ia_size != i_size_read(dentry->d_inode))
		truncate_setsize(dentry->d_inode, iattr->ia_size);

	v9fs_invalidate_inode_attr(dentry->d_inode);

	setattr_copy(dentry->d_inode, iattr);
	mark_inode_dirty(dentry->d_inode);
	return 0;
}


void
v9fs_stat2inode(struct p9_wstat *stat, struct inode *inode,
	struct super_block *sb)
{
	umode_t mode;
	char ext[32];
	char tag_name[14];
	unsigned int i_nlink;
	struct v9fs_session_info *v9ses = sb->s_fs_info;
	struct v9fs_inode *v9inode = V9FS_I(inode);

	set_nlink(inode, 1);

	inode->i_atime.tv_sec = stat->atime;
	inode->i_mtime.tv_sec = stat->mtime;
	inode->i_ctime.tv_sec = stat->mtime;

	inode->i_uid = v9ses->dfltuid;
	inode->i_gid = v9ses->dfltgid;

	if (v9fs_proto_dotu(v9ses)) {
		inode->i_uid = stat->n_uid;
		inode->i_gid = stat->n_gid;
	}
	if ((S_ISREG(inode->i_mode)) || (S_ISDIR(inode->i_mode))) {
		if (v9fs_proto_dotu(v9ses) && (stat->extension[0] != '\0')) {
			strncpy(ext, stat->extension, sizeof(ext));
			
			sscanf(ext, "%13s %u", tag_name, &i_nlink);
			if (!strncmp(tag_name, "HARDLINKCOUNT", 13))
				set_nlink(inode, i_nlink);
		}
	}
	mode = p9mode2perm(v9ses, stat);
	mode |= inode->i_mode & ~S_IALLUGO;
	inode->i_mode = mode;
	i_size_write(inode, stat->length);

	
	inode->i_blocks = (i_size_read(inode) + 512 - 1) >> 9;
	v9inode->cache_validity &= ~V9FS_INO_INVALID_ATTR;
}


ino_t v9fs_qid2ino(struct p9_qid *qid)
{
	u64 path = qid->path + 2;
	ino_t i = 0;

	if (sizeof(ino_t) == sizeof(path))
		memcpy(&i, &path, sizeof(ino_t));
	else
		i = (ino_t) (path ^ (path >> 32));

	return i;
}


static int v9fs_readlink(struct dentry *dentry, char *buffer, int buflen)
{
	int retval;

	struct v9fs_session_info *v9ses;
	struct p9_fid *fid;
	struct p9_wstat *st;

	p9_debug(P9_DEBUG_VFS, " %s\n", dentry->d_name.name);
	retval = -EPERM;
	v9ses = v9fs_dentry2v9ses(dentry);
	fid = v9fs_fid_lookup(dentry);
	if (IS_ERR(fid))
		return PTR_ERR(fid);

	if (!v9fs_proto_dotu(v9ses))
		return -EBADF;

	st = p9_client_stat(fid);
	if (IS_ERR(st))
		return PTR_ERR(st);

	if (!(st->mode & P9_DMSYMLINK)) {
		retval = -EINVAL;
		goto done;
	}

	
	strncpy(buffer, st->extension, buflen);

	p9_debug(P9_DEBUG_VFS, "%s -> %s (%s)\n",
		 dentry->d_name.name, st->extension, buffer);

	retval = strnlen(buffer, buflen);
done:
	p9stat_free(st);
	kfree(st);
	return retval;
}


static void *v9fs_vfs_follow_link(struct dentry *dentry, struct nameidata *nd)
{
	int len = 0;
	char *link = __getname();

	p9_debug(P9_DEBUG_VFS, "%s\n", dentry->d_name.name);

	if (!link)
		link = ERR_PTR(-ENOMEM);
	else {
		len = v9fs_readlink(dentry, link, PATH_MAX);

		if (len < 0) {
			__putname(link);
			link = ERR_PTR(len);
		} else
			link[min(len, PATH_MAX-1)] = 0;
	}
	nd_set_link(nd, link);

	return NULL;
}


void
v9fs_vfs_put_link(struct dentry *dentry, struct nameidata *nd, void *p)
{
	char *s = nd_get_link(nd);

	p9_debug(P9_DEBUG_VFS, " %s %s\n",
		 dentry->d_name.name, IS_ERR(s) ? "<error>" : s);
	if (!IS_ERR(s))
		__putname(s);
}


static int v9fs_vfs_mkspecial(struct inode *dir, struct dentry *dentry,
	u32 perm, const char *extension)
{
	struct p9_fid *fid;
	struct v9fs_session_info *v9ses;

	v9ses = v9fs_inode2v9ses(dir);
	if (!v9fs_proto_dotu(v9ses)) {
		p9_debug(P9_DEBUG_ERROR, "not extended\n");
		return -EPERM;
	}

	fid = v9fs_create(v9ses, dir, dentry, (char *) extension, perm,
								P9_OREAD);
	if (IS_ERR(fid))
		return PTR_ERR(fid);

	v9fs_invalidate_inode_attr(dir);
	p9_client_clunk(fid);
	return 0;
}


static int
v9fs_vfs_symlink(struct inode *dir, struct dentry *dentry, const char *symname)
{
	p9_debug(P9_DEBUG_VFS, " %lu,%s,%s\n",
		 dir->i_ino, dentry->d_name.name, symname);

	return v9fs_vfs_mkspecial(dir, dentry, P9_DMSYMLINK, symname);
}


static int
v9fs_vfs_link(struct dentry *old_dentry, struct inode *dir,
	      struct dentry *dentry)
{
	int retval;
	char *name;
	struct p9_fid *oldfid;

	p9_debug(P9_DEBUG_VFS, " %lu,%s,%s\n",
		 dir->i_ino, dentry->d_name.name, old_dentry->d_name.name);

	oldfid = v9fs_fid_clone(old_dentry);
	if (IS_ERR(oldfid))
		return PTR_ERR(oldfid);

	name = __getname();
	if (unlikely(!name)) {
		retval = -ENOMEM;
		goto clunk_fid;
	}

	sprintf(name, "%d\n", oldfid->fid);
	retval = v9fs_vfs_mkspecial(dir, dentry, P9_DMLINK, name);
	__putname(name);
	if (!retval) {
		v9fs_refresh_inode(oldfid, old_dentry->d_inode);
		v9fs_invalidate_inode_attr(dir);
	}
clunk_fid:
	p9_client_clunk(oldfid);
	return retval;
}


static int
v9fs_vfs_mknod(struct inode *dir, struct dentry *dentry, umode_t mode, dev_t rdev)
{
	struct v9fs_session_info *v9ses = v9fs_inode2v9ses(dir);
	int retval;
	char *name;
	u32 perm;

	p9_debug(P9_DEBUG_VFS, " %lu,%s mode: %hx MAJOR: %u MINOR: %u\n",
		 dir->i_ino, dentry->d_name.name, mode,
		 MAJOR(rdev), MINOR(rdev));

	if (!new_valid_dev(rdev))
		return -EINVAL;

	name = __getname();
	if (!name)
		return -ENOMEM;
	
	if (S_ISBLK(mode))
		sprintf(name, "b %u %u", MAJOR(rdev), MINOR(rdev));
	else if (S_ISCHR(mode))
		sprintf(name, "c %u %u", MAJOR(rdev), MINOR(rdev));
	else if (S_ISFIFO(mode))
		*name = 0;
	else if (S_ISSOCK(mode))
		*name = 0;
	else {
		__putname(name);
		return -EINVAL;
	}

	perm = unixmode2p9mode(v9ses, mode);
	retval = v9fs_vfs_mkspecial(dir, dentry, perm, name);
	__putname(name);

	return retval;
}

int v9fs_refresh_inode(struct p9_fid *fid, struct inode *inode)
{
	int umode;
	dev_t rdev;
	loff_t i_size;
	struct p9_wstat *st;
	struct v9fs_session_info *v9ses;

	v9ses = v9fs_inode2v9ses(inode);
	st = p9_client_stat(fid);
	if (IS_ERR(st))
		return PTR_ERR(st);
	umode = p9mode2unixmode(v9ses, st, &rdev);
	if ((inode->i_mode & S_IFMT) != (umode & S_IFMT))
		goto out;

	spin_lock(&inode->i_lock);
	i_size = inode->i_size;
	v9fs_stat2inode(st, inode, inode->i_sb);
	if (v9ses->cache)
		inode->i_size = i_size;
	spin_unlock(&inode->i_lock);
out:
	p9stat_free(st);
	kfree(st);
	return 0;
}

static const struct inode_operations v9fs_dir_inode_operations_dotu = {
	.create = v9fs_vfs_create,
	.lookup = v9fs_vfs_lookup,
	.symlink = v9fs_vfs_symlink,
	.link = v9fs_vfs_link,
	.unlink = v9fs_vfs_unlink,
	.mkdir = v9fs_vfs_mkdir,
	.rmdir = v9fs_vfs_rmdir,
	.mknod = v9fs_vfs_mknod,
	.rename = v9fs_vfs_rename,
	.getattr = v9fs_vfs_getattr,
	.setattr = v9fs_vfs_setattr,
};

static const struct inode_operations v9fs_dir_inode_operations = {
	.create = v9fs_vfs_create,
	.lookup = v9fs_vfs_lookup,
	.unlink = v9fs_vfs_unlink,
	.mkdir = v9fs_vfs_mkdir,
	.rmdir = v9fs_vfs_rmdir,
	.mknod = v9fs_vfs_mknod,
	.rename = v9fs_vfs_rename,
	.getattr = v9fs_vfs_getattr,
	.setattr = v9fs_vfs_setattr,
};

static const struct inode_operations v9fs_file_inode_operations = {
	.getattr = v9fs_vfs_getattr,
	.setattr = v9fs_vfs_setattr,
};

static const struct inode_operations v9fs_symlink_inode_operations = {
	.readlink = generic_readlink,
	.follow_link = v9fs_vfs_follow_link,
	.put_link = v9fs_vfs_put_link,
	.getattr = v9fs_vfs_getattr,
	.setattr = v9fs_vfs_setattr,
};

