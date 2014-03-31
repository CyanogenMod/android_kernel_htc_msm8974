/*
 * linux/fs/reiserfs/xattr.c
 *
 * Copyright (c) 2002 by Jeff Mahoney, <jeffm@suse.com>
 *
 */


#include "reiserfs.h"
#include <linux/capability.h>
#include <linux/dcache.h>
#include <linux/namei.h>
#include <linux/errno.h>
#include <linux/gfp.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/pagemap.h>
#include <linux/xattr.h>
#include "xattr.h"
#include "acl.h"
#include <asm/uaccess.h>
#include <net/checksum.h>
#include <linux/stat.h>
#include <linux/quotaops.h>
#include <linux/security.h>

#define PRIVROOT_NAME ".reiserfs_priv"
#define XAROOT_NAME   "xattrs"


#ifdef CONFIG_REISERFS_FS_XATTR
static int xattr_create(struct inode *dir, struct dentry *dentry, int mode)
{
	BUG_ON(!mutex_is_locked(&dir->i_mutex));
	return dir->i_op->create(dir, dentry, mode, NULL);
}
#endif

static int xattr_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
	BUG_ON(!mutex_is_locked(&dir->i_mutex));
	return dir->i_op->mkdir(dir, dentry, mode);
}

static int xattr_unlink(struct inode *dir, struct dentry *dentry)
{
	int error;
	BUG_ON(!mutex_is_locked(&dir->i_mutex));

	reiserfs_mutex_lock_nested_safe(&dentry->d_inode->i_mutex,
					I_MUTEX_CHILD, dir->i_sb);
	error = dir->i_op->unlink(dir, dentry);
	mutex_unlock(&dentry->d_inode->i_mutex);

	if (!error)
		d_delete(dentry);
	return error;
}

static int xattr_rmdir(struct inode *dir, struct dentry *dentry)
{
	int error;
	BUG_ON(!mutex_is_locked(&dir->i_mutex));

	reiserfs_mutex_lock_nested_safe(&dentry->d_inode->i_mutex,
					I_MUTEX_CHILD, dir->i_sb);
	error = dir->i_op->rmdir(dir, dentry);
	if (!error)
		dentry->d_inode->i_flags |= S_DEAD;
	mutex_unlock(&dentry->d_inode->i_mutex);
	if (!error)
		d_delete(dentry);

	return error;
}

#define xattr_may_create(flags)	(!flags || flags & XATTR_CREATE)

static struct dentry *open_xa_root(struct super_block *sb, int flags)
{
	struct dentry *privroot = REISERFS_SB(sb)->priv_root;
	struct dentry *xaroot;
	if (!privroot->d_inode)
		return ERR_PTR(-ENODATA);

	mutex_lock_nested(&privroot->d_inode->i_mutex, I_MUTEX_XATTR);

	xaroot = dget(REISERFS_SB(sb)->xattr_root);
	if (!xaroot)
		xaroot = ERR_PTR(-ENODATA);
	else if (!xaroot->d_inode) {
		int err = -ENODATA;
		if (xattr_may_create(flags))
			err = xattr_mkdir(privroot->d_inode, xaroot, 0700);
		if (err) {
			dput(xaroot);
			xaroot = ERR_PTR(err);
		}
	}

	mutex_unlock(&privroot->d_inode->i_mutex);
	return xaroot;
}

static struct dentry *open_xa_dir(const struct inode *inode, int flags)
{
	struct dentry *xaroot, *xadir;
	char namebuf[17];

	xaroot = open_xa_root(inode->i_sb, flags);
	if (IS_ERR(xaroot))
		return xaroot;

	snprintf(namebuf, sizeof(namebuf), "%X.%X",
		 le32_to_cpu(INODE_PKEY(inode)->k_objectid),
		 inode->i_generation);

	mutex_lock_nested(&xaroot->d_inode->i_mutex, I_MUTEX_XATTR);

	xadir = lookup_one_len(namebuf, xaroot, strlen(namebuf));
	if (!IS_ERR(xadir) && !xadir->d_inode) {
		int err = -ENODATA;
		if (xattr_may_create(flags))
			err = xattr_mkdir(xaroot->d_inode, xadir, 0700);
		if (err) {
			dput(xadir);
			xadir = ERR_PTR(err);
		}
	}

	mutex_unlock(&xaroot->d_inode->i_mutex);
	dput(xaroot);
	return xadir;
}

struct reiserfs_dentry_buf {
	struct dentry *xadir;
	int count;
	struct dentry *dentries[8];
};

static int
fill_with_dentries(void *buf, const char *name, int namelen, loff_t offset,
		    u64 ino, unsigned int d_type)
{
	struct reiserfs_dentry_buf *dbuf = buf;
	struct dentry *dentry;
	WARN_ON_ONCE(!mutex_is_locked(&dbuf->xadir->d_inode->i_mutex));

	if (dbuf->count == ARRAY_SIZE(dbuf->dentries))
		return -ENOSPC;

	if (name[0] == '.' && (name[1] == '\0' ||
			       (name[1] == '.' && name[2] == '\0')))
		return 0;

	dentry = lookup_one_len(name, dbuf->xadir, namelen);
	if (IS_ERR(dentry)) {
		return PTR_ERR(dentry);
	} else if (!dentry->d_inode) {
		
		reiserfs_error(dentry->d_sb, "xattr-20003",
			       "Corrupted directory: xattr %s listed but "
			       "not found for file %s.\n",
			       dentry->d_name.name, dbuf->xadir->d_name.name);
		dput(dentry);
		return -EIO;
	}

	dbuf->dentries[dbuf->count++] = dentry;
	return 0;
}

static void
cleanup_dentry_buf(struct reiserfs_dentry_buf *buf)
{
	int i;
	for (i = 0; i < buf->count; i++)
		if (buf->dentries[i])
			dput(buf->dentries[i]);
}

static int reiserfs_for_each_xattr(struct inode *inode,
				   int (*action)(struct dentry *, void *),
				   void *data)
{
	struct dentry *dir;
	int i, err = 0;
	loff_t pos = 0;
	struct reiserfs_dentry_buf buf = {
		.count = 0,
	};

	
	if (IS_PRIVATE(inode) || get_inode_sd_version(inode) == STAT_DATA_V1)
		return 0;

	reiserfs_write_unlock(inode->i_sb);
	dir = open_xa_dir(inode, XATTR_REPLACE);
	if (IS_ERR(dir)) {
		err = PTR_ERR(dir);
		reiserfs_write_lock(inode->i_sb);
		goto out;
	} else if (!dir->d_inode) {
		err = 0;
		reiserfs_write_lock(inode->i_sb);
		goto out_dir;
	}

	mutex_lock_nested(&dir->d_inode->i_mutex, I_MUTEX_XATTR);

	reiserfs_write_lock(inode->i_sb);

	buf.xadir = dir;
	err = reiserfs_readdir_dentry(dir, &buf, fill_with_dentries, &pos);
	while ((err == 0 || err == -ENOSPC) && buf.count) {
		err = 0;

		for (i = 0; i < buf.count && buf.dentries[i]; i++) {
			int lerr = 0;
			struct dentry *dentry = buf.dentries[i];

			if (err == 0 && !S_ISDIR(dentry->d_inode->i_mode))
				lerr = action(dentry, data);

			dput(dentry);
			buf.dentries[i] = NULL;
			err = lerr ?: err;
		}
		buf.count = 0;
		if (!err)
			err = reiserfs_readdir_dentry(dir, &buf,
						      fill_with_dentries, &pos);
	}
	mutex_unlock(&dir->d_inode->i_mutex);

	
	cleanup_dentry_buf(&buf);

	if (!err) {
		int blocks = JOURNAL_PER_BALANCE_CNT * 2 + 2 +
			     4 * REISERFS_QUOTA_TRANS_BLOCKS(inode->i_sb);
		struct reiserfs_transaction_handle th;
		err = journal_begin(&th, inode->i_sb, blocks);
		if (!err) {
			int jerror;
			reiserfs_mutex_lock_nested_safe(
					  &dir->d_parent->d_inode->i_mutex,
					  I_MUTEX_XATTR, inode->i_sb);
			err = action(dir, data);
			jerror = journal_end(&th, inode->i_sb, blocks);
			mutex_unlock(&dir->d_parent->d_inode->i_mutex);
			err = jerror ?: err;
		}
	}
out_dir:
	dput(dir);
out:
	
	if (err == -ENODATA)
		err = 0;
	return err;
}

static int delete_one_xattr(struct dentry *dentry, void *data)
{
	struct inode *dir = dentry->d_parent->d_inode;

	
	if (S_ISDIR(dentry->d_inode->i_mode))
		return xattr_rmdir(dir, dentry);

	return xattr_unlink(dir, dentry);
}

static int chown_one_xattr(struct dentry *dentry, void *data)
{
	struct iattr *attrs = data;
	return reiserfs_setattr(dentry, attrs);
}

int reiserfs_delete_xattrs(struct inode *inode)
{
	int err = reiserfs_for_each_xattr(inode, delete_one_xattr, NULL);
	if (err)
		reiserfs_warning(inode->i_sb, "jdm-20004",
				 "Couldn't delete all xattrs (%d)\n", err);
	return err;
}

int reiserfs_chown_xattrs(struct inode *inode, struct iattr *attrs)
{
	int err = reiserfs_for_each_xattr(inode, chown_one_xattr, attrs);
	if (err)
		reiserfs_warning(inode->i_sb, "jdm-20007",
				 "Couldn't chown all xattrs (%d)\n", err);
	return err;
}

#ifdef CONFIG_REISERFS_FS_XATTR
static struct dentry *xattr_lookup(struct inode *inode, const char *name,
				    int flags)
{
	struct dentry *xadir, *xafile;
	int err = 0;

	xadir = open_xa_dir(inode, flags);
	if (IS_ERR(xadir))
		return ERR_CAST(xadir);

	mutex_lock_nested(&xadir->d_inode->i_mutex, I_MUTEX_XATTR);
	xafile = lookup_one_len(name, xadir, strlen(name));
	if (IS_ERR(xafile)) {
		err = PTR_ERR(xafile);
		goto out;
	}

	if (xafile->d_inode && (flags & XATTR_CREATE))
		err = -EEXIST;

	if (!xafile->d_inode) {
		err = -ENODATA;
		if (xattr_may_create(flags))
			err = xattr_create(xadir->d_inode, xafile,
					      0700|S_IFREG);
	}

	if (err)
		dput(xafile);
out:
	mutex_unlock(&xadir->d_inode->i_mutex);
	dput(xadir);
	if (err)
		return ERR_PTR(err);
	return xafile;
}

static inline void reiserfs_put_page(struct page *page)
{
	kunmap(page);
	page_cache_release(page);
}

static struct page *reiserfs_get_page(struct inode *dir, size_t n)
{
	struct address_space *mapping = dir->i_mapping;
	struct page *page;
	mapping_set_gfp_mask(mapping, GFP_NOFS);
	page = read_mapping_page(mapping, n >> PAGE_CACHE_SHIFT, NULL);
	if (!IS_ERR(page)) {
		kmap(page);
		if (PageError(page))
			goto fail;
	}
	return page;

      fail:
	reiserfs_put_page(page);
	return ERR_PTR(-EIO);
}

static inline __u32 xattr_hash(const char *msg, int len)
{
	return csum_partial(msg, len, 0);
}

int reiserfs_commit_write(struct file *f, struct page *page,
			  unsigned from, unsigned to);

static void update_ctime(struct inode *inode)
{
	struct timespec now = current_fs_time(inode->i_sb);
	if (inode_unhashed(inode) || !inode->i_nlink ||
	    timespec_equal(&inode->i_ctime, &now))
		return;

	inode->i_ctime = CURRENT_TIME_SEC;
	mark_inode_dirty(inode);
}

static int lookup_and_delete_xattr(struct inode *inode, const char *name)
{
	int err = 0;
	struct dentry *dentry, *xadir;

	xadir = open_xa_dir(inode, XATTR_REPLACE);
	if (IS_ERR(xadir))
		return PTR_ERR(xadir);

	mutex_lock_nested(&xadir->d_inode->i_mutex, I_MUTEX_XATTR);
	dentry = lookup_one_len(name, xadir, strlen(name));
	if (IS_ERR(dentry)) {
		err = PTR_ERR(dentry);
		goto out_dput;
	}

	if (dentry->d_inode) {
		reiserfs_write_lock(inode->i_sb);
		err = xattr_unlink(xadir->d_inode, dentry);
		reiserfs_write_unlock(inode->i_sb);
		update_ctime(inode);
	}

	dput(dentry);
out_dput:
	mutex_unlock(&xadir->d_inode->i_mutex);
	dput(xadir);
	return err;
}



int
reiserfs_xattr_set_handle(struct reiserfs_transaction_handle *th,
			  struct inode *inode, const char *name,
			  const void *buffer, size_t buffer_size, int flags)
{
	int err = 0;
	struct dentry *dentry;
	struct page *page;
	char *data;
	size_t file_pos = 0;
	size_t buffer_pos = 0;
	size_t new_size;
	__u32 xahash = 0;

	if (get_inode_sd_version(inode) == STAT_DATA_V1)
		return -EOPNOTSUPP;

	reiserfs_write_unlock(inode->i_sb);

	if (!buffer) {
		err = lookup_and_delete_xattr(inode, name);
		reiserfs_write_lock(inode->i_sb);
		return err;
	}

	dentry = xattr_lookup(inode, name, flags);
	if (IS_ERR(dentry)) {
		reiserfs_write_lock(inode->i_sb);
		return PTR_ERR(dentry);
	}

	down_write(&REISERFS_I(inode)->i_xattr_sem);

	reiserfs_write_lock(inode->i_sb);

	xahash = xattr_hash(buffer, buffer_size);
	while (buffer_pos < buffer_size || buffer_pos == 0) {
		size_t chunk;
		size_t skip = 0;
		size_t page_offset = (file_pos & (PAGE_CACHE_SIZE - 1));
		if (buffer_size - buffer_pos > PAGE_CACHE_SIZE)
			chunk = PAGE_CACHE_SIZE;
		else
			chunk = buffer_size - buffer_pos;

		page = reiserfs_get_page(dentry->d_inode, file_pos);
		if (IS_ERR(page)) {
			err = PTR_ERR(page);
			goto out_unlock;
		}

		lock_page(page);
		data = page_address(page);

		if (file_pos == 0) {
			struct reiserfs_xattr_header *rxh;
			skip = file_pos = sizeof(struct reiserfs_xattr_header);
			if (chunk + skip > PAGE_CACHE_SIZE)
				chunk = PAGE_CACHE_SIZE - skip;
			rxh = (struct reiserfs_xattr_header *)data;
			rxh->h_magic = cpu_to_le32(REISERFS_XATTR_MAGIC);
			rxh->h_hash = cpu_to_le32(xahash);
		}

		err = __reiserfs_write_begin(page, page_offset, chunk + skip);
		if (!err) {
			if (buffer)
				memcpy(data + skip, buffer + buffer_pos, chunk);
			err = reiserfs_commit_write(NULL, page, page_offset,
						    page_offset + chunk +
						    skip);
		}
		unlock_page(page);
		reiserfs_put_page(page);
		buffer_pos += chunk;
		file_pos += chunk;
		skip = 0;
		if (err || buffer_size == 0 || !buffer)
			break;
	}

	new_size = buffer_size + sizeof(struct reiserfs_xattr_header);
	if (!err && new_size < i_size_read(dentry->d_inode)) {
		struct iattr newattrs = {
			.ia_ctime = current_fs_time(inode->i_sb),
			.ia_size = new_size,
			.ia_valid = ATTR_SIZE | ATTR_CTIME,
		};

		reiserfs_write_unlock(inode->i_sb);
		mutex_lock_nested(&dentry->d_inode->i_mutex, I_MUTEX_XATTR);
		inode_dio_wait(dentry->d_inode);
		reiserfs_write_lock(inode->i_sb);

		err = reiserfs_setattr(dentry, &newattrs);
		mutex_unlock(&dentry->d_inode->i_mutex);
	} else
		update_ctime(inode);
out_unlock:
	up_write(&REISERFS_I(inode)->i_xattr_sem);
	dput(dentry);
	return err;
}

int reiserfs_xattr_set(struct inode *inode, const char *name,
		       const void *buffer, size_t buffer_size, int flags)
{

	struct reiserfs_transaction_handle th;
	int error, error2;
	size_t jbegin_count = reiserfs_xattr_nblocks(inode, buffer_size);

	if (!(flags & XATTR_REPLACE))
		jbegin_count += reiserfs_xattr_jcreate_nblocks(inode);

	reiserfs_write_lock(inode->i_sb);
	error = journal_begin(&th, inode->i_sb, jbegin_count);
	if (error) {
		reiserfs_write_unlock(inode->i_sb);
		return error;
	}

	error = reiserfs_xattr_set_handle(&th, inode, name,
					  buffer, buffer_size, flags);

	error2 = journal_end(&th, inode->i_sb, jbegin_count);
	if (error == 0)
		error = error2;
	reiserfs_write_unlock(inode->i_sb);

	return error;
}

int
reiserfs_xattr_get(struct inode *inode, const char *name, void *buffer,
		   size_t buffer_size)
{
	ssize_t err = 0;
	struct dentry *dentry;
	size_t isize;
	size_t file_pos = 0;
	size_t buffer_pos = 0;
	struct page *page;
	__u32 hash = 0;

	if (name == NULL)
		return -EINVAL;

	if (get_inode_sd_version(inode) == STAT_DATA_V1)
		return -EOPNOTSUPP;

	dentry = xattr_lookup(inode, name, XATTR_REPLACE);
	if (IS_ERR(dentry)) {
		err = PTR_ERR(dentry);
		goto out;
	}

	down_read(&REISERFS_I(inode)->i_xattr_sem);

	isize = i_size_read(dentry->d_inode);

	
	if (buffer == NULL) {
		err = isize - sizeof(struct reiserfs_xattr_header);
		goto out_unlock;
	}

	if (buffer_size < isize - sizeof(struct reiserfs_xattr_header)) {
		err = -ERANGE;
		goto out_unlock;
	}

	while (file_pos < isize) {
		size_t chunk;
		char *data;
		size_t skip = 0;
		if (isize - file_pos > PAGE_CACHE_SIZE)
			chunk = PAGE_CACHE_SIZE;
		else
			chunk = isize - file_pos;

		page = reiserfs_get_page(dentry->d_inode, file_pos);
		if (IS_ERR(page)) {
			err = PTR_ERR(page);
			goto out_unlock;
		}

		lock_page(page);
		data = page_address(page);
		if (file_pos == 0) {
			struct reiserfs_xattr_header *rxh =
			    (struct reiserfs_xattr_header *)data;
			skip = file_pos = sizeof(struct reiserfs_xattr_header);
			chunk -= skip;
			
			if (rxh->h_magic != cpu_to_le32(REISERFS_XATTR_MAGIC)) {
				unlock_page(page);
				reiserfs_put_page(page);
				reiserfs_warning(inode->i_sb, "jdm-20001",
						 "Invalid magic for xattr (%s) "
						 "associated with %k", name,
						 INODE_PKEY(inode));
				err = -EIO;
				goto out_unlock;
			}
			hash = le32_to_cpu(rxh->h_hash);
		}
		memcpy(buffer + buffer_pos, data + skip, chunk);
		unlock_page(page);
		reiserfs_put_page(page);
		file_pos += chunk;
		buffer_pos += chunk;
		skip = 0;
	}
	err = isize - sizeof(struct reiserfs_xattr_header);

	if (xattr_hash(buffer, isize - sizeof(struct reiserfs_xattr_header)) !=
	    hash) {
		reiserfs_warning(inode->i_sb, "jdm-20002",
				 "Invalid hash for xattr (%s) associated "
				 "with %k", name, INODE_PKEY(inode));
		err = -EIO;
	}

out_unlock:
	up_read(&REISERFS_I(inode)->i_xattr_sem);
	dput(dentry);

out:
	return err;
}

#define for_each_xattr_handler(handlers, handler)		\
		for ((handler) = *(handlers)++;			\
			(handler) != NULL;			\
			(handler) = *(handlers)++)

static inline const struct xattr_handler *
find_xattr_handler_prefix(const struct xattr_handler **handlers,
			   const char *name)
{
	const struct xattr_handler *xah;

	if (!handlers)
		return NULL;

	for_each_xattr_handler(handlers, xah) {
		if (strncmp(xah->prefix, name, strlen(xah->prefix)) == 0)
			break;
	}

	return xah;
}


ssize_t
reiserfs_getxattr(struct dentry * dentry, const char *name, void *buffer,
		  size_t size)
{
	const struct xattr_handler *handler;

	handler = find_xattr_handler_prefix(dentry->d_sb->s_xattr, name);

	if (!handler || get_inode_sd_version(dentry->d_inode) == STAT_DATA_V1)
		return -EOPNOTSUPP;

	return handler->get(dentry, name, buffer, size, handler->flags);
}

int
reiserfs_setxattr(struct dentry *dentry, const char *name, const void *value,
		  size_t size, int flags)
{
	const struct xattr_handler *handler;

	handler = find_xattr_handler_prefix(dentry->d_sb->s_xattr, name);

	if (!handler || get_inode_sd_version(dentry->d_inode) == STAT_DATA_V1)
		return -EOPNOTSUPP;

	return handler->set(dentry, name, value, size, flags, handler->flags);
}

int reiserfs_removexattr(struct dentry *dentry, const char *name)
{
	const struct xattr_handler *handler;
	handler = find_xattr_handler_prefix(dentry->d_sb->s_xattr, name);

	if (!handler || get_inode_sd_version(dentry->d_inode) == STAT_DATA_V1)
		return -EOPNOTSUPP;

	return handler->set(dentry, name, NULL, 0, XATTR_REPLACE, handler->flags);
}

struct listxattr_buf {
	size_t size;
	size_t pos;
	char *buf;
	struct dentry *dentry;
};

static int listxattr_filler(void *buf, const char *name, int namelen,
			    loff_t offset, u64 ino, unsigned int d_type)
{
	struct listxattr_buf *b = (struct listxattr_buf *)buf;
	size_t size;
	if (name[0] != '.' ||
	    (namelen != 1 && (name[1] != '.' || namelen != 2))) {
		const struct xattr_handler *handler;
		handler = find_xattr_handler_prefix(b->dentry->d_sb->s_xattr,
						    name);
		if (!handler)	
			return 0;
		if (b->buf) {
			size = handler->list(b->dentry, b->buf + b->pos,
					 b->size, name, namelen,
					 handler->flags);
			if (size > b->size)
				return -ERANGE;
		} else {
			size = handler->list(b->dentry, NULL, 0, name,
					     namelen, handler->flags);
		}

		b->pos += size;
	}
	return 0;
}

ssize_t reiserfs_listxattr(struct dentry * dentry, char *buffer, size_t size)
{
	struct dentry *dir;
	int err = 0;
	loff_t pos = 0;
	struct listxattr_buf buf = {
		.dentry = dentry,
		.buf = buffer,
		.size = buffer ? size : 0,
	};

	if (!dentry->d_inode)
		return -EINVAL;

	if (!dentry->d_sb->s_xattr ||
	    get_inode_sd_version(dentry->d_inode) == STAT_DATA_V1)
		return -EOPNOTSUPP;

	dir = open_xa_dir(dentry->d_inode, XATTR_REPLACE);
	if (IS_ERR(dir)) {
		err = PTR_ERR(dir);
		if (err == -ENODATA)
			err = 0;  
		goto out;
	}

	mutex_lock_nested(&dir->d_inode->i_mutex, I_MUTEX_XATTR);
	err = reiserfs_readdir_dentry(dir, &buf, listxattr_filler, &pos);
	mutex_unlock(&dir->d_inode->i_mutex);

	if (!err)
		err = buf.pos;

	dput(dir);
out:
	return err;
}

static int create_privroot(struct dentry *dentry)
{
	int err;
	struct inode *inode = dentry->d_parent->d_inode;
	WARN_ON_ONCE(!mutex_is_locked(&inode->i_mutex));

	err = xattr_mkdir(inode, dentry, 0700);
	if (err || !dentry->d_inode) {
		reiserfs_warning(dentry->d_sb, "jdm-20006",
				 "xattrs/ACLs enabled and couldn't "
				 "find/create .reiserfs_priv. "
				 "Failing mount.");
		return -EOPNOTSUPP;
	}

	dentry->d_inode->i_flags |= S_PRIVATE;
	reiserfs_info(dentry->d_sb, "Created %s - reserved for xattr "
		      "storage.\n", PRIVROOT_NAME);

	return 0;
}

#else
int __init reiserfs_xattr_register_handlers(void) { return 0; }
void reiserfs_xattr_unregister_handlers(void) {}
static int create_privroot(struct dentry *dentry) { return 0; }
#endif

const struct xattr_handler *reiserfs_xattr_handlers[] = {
#ifdef CONFIG_REISERFS_FS_XATTR
	&reiserfs_xattr_user_handler,
	&reiserfs_xattr_trusted_handler,
#endif
#ifdef CONFIG_REISERFS_FS_SECURITY
	&reiserfs_xattr_security_handler,
#endif
#ifdef CONFIG_REISERFS_FS_POSIX_ACL
	&reiserfs_posix_acl_access_handler,
	&reiserfs_posix_acl_default_handler,
#endif
	NULL
};

static int xattr_mount_check(struct super_block *s)
{
	if (old_format_only(s)) {
		if (reiserfs_xattrs_optional(s)) {
			reiserfs_warning(s, "jdm-2005",
					 "xattrs/ACLs not supported "
					 "on pre-v3.6 format filesystems. "
					 "Failing mount.");
			return -EOPNOTSUPP;
		}
	}

	return 0;
}

int reiserfs_permission(struct inode *inode, int mask)
{
	if (IS_PRIVATE(inode))
		return 0;

	return generic_permission(inode, mask);
}

static int xattr_hide_revalidate(struct dentry *dentry, struct nameidata *nd)
{
	return -EPERM;
}

static const struct dentry_operations xattr_lookup_poison_ops = {
	.d_revalidate = xattr_hide_revalidate,
};

int reiserfs_lookup_privroot(struct super_block *s)
{
	struct dentry *dentry;
	int err = 0;

	
	reiserfs_mutex_lock_safe(&s->s_root->d_inode->i_mutex, s);
	dentry = lookup_one_len(PRIVROOT_NAME, s->s_root,
				strlen(PRIVROOT_NAME));
	if (!IS_ERR(dentry)) {
		REISERFS_SB(s)->priv_root = dentry;
		d_set_d_op(dentry, &xattr_lookup_poison_ops);
		if (dentry->d_inode)
			dentry->d_inode->i_flags |= S_PRIVATE;
	} else
		err = PTR_ERR(dentry);
	mutex_unlock(&s->s_root->d_inode->i_mutex);

	return err;
}

int reiserfs_xattr_init(struct super_block *s, int mount_flags)
{
	int err = 0;
	struct dentry *privroot = REISERFS_SB(s)->priv_root;

	err = xattr_mount_check(s);
	if (err)
		goto error;

	if (!privroot->d_inode && !(mount_flags & MS_RDONLY)) {
		reiserfs_mutex_lock_safe(&s->s_root->d_inode->i_mutex, s);
		err = create_privroot(REISERFS_SB(s)->priv_root);
		mutex_unlock(&s->s_root->d_inode->i_mutex);
	}

	if (privroot->d_inode) {
		s->s_xattr = reiserfs_xattr_handlers;
		reiserfs_mutex_lock_safe(&privroot->d_inode->i_mutex, s);
		if (!REISERFS_SB(s)->xattr_root) {
			struct dentry *dentry;
			dentry = lookup_one_len(XAROOT_NAME, privroot,
						strlen(XAROOT_NAME));
			if (!IS_ERR(dentry))
				REISERFS_SB(s)->xattr_root = dentry;
			else
				err = PTR_ERR(dentry);
		}
		mutex_unlock(&privroot->d_inode->i_mutex);
	}

error:
	if (err) {
		clear_bit(REISERFS_XATTRS_USER, &(REISERFS_SB(s)->s_mount_opt));
		clear_bit(REISERFS_POSIXACL, &(REISERFS_SB(s)->s_mount_opt));
	}

	
	if (reiserfs_posixacl(s))
		s->s_flags |= MS_POSIXACL;
	else
		s->s_flags &= ~MS_POSIXACL;

	return err;
}
