/* * This file is part of UBIFS.
 *
 * Copyright (C) 2006-2008 Nokia Corporation.
 * Copyright (C) 2006, 2007 University of Szeged, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Authors: Artem Bityutskiy (Битюцкий Артём)
 *          Adrian Hunter
 *          Zoltan Sogor
 */


#include "ubifs.h"

static int inherit_flags(const struct inode *dir, umode_t mode)
{
	int flags;
	const struct ubifs_inode *ui = ubifs_inode(dir);

	if (!S_ISDIR(dir->i_mode))
		return 0;

	flags = ui->flags & (UBIFS_COMPR_FL | UBIFS_SYNC_FL | UBIFS_DIRSYNC_FL);
	if (!S_ISDIR(mode))
		
		flags &= ~UBIFS_DIRSYNC_FL;
	return flags;
}

struct inode *ubifs_new_inode(struct ubifs_info *c, const struct inode *dir,
			      umode_t mode)
{
	struct inode *inode;
	struct ubifs_inode *ui;

	inode = new_inode(c->vfs_sb);
	ui = ubifs_inode(inode);
	if (!inode)
		return ERR_PTR(-ENOMEM);

	inode->i_flags |= S_NOCMTIME;

	inode_init_owner(inode, dir, mode);
	inode->i_mtime = inode->i_atime = inode->i_ctime =
			 ubifs_current_time(inode);
	inode->i_mapping->nrpages = 0;
	
	inode->i_mapping->backing_dev_info = &c->bdi;

	switch (mode & S_IFMT) {
	case S_IFREG:
		inode->i_mapping->a_ops = &ubifs_file_address_operations;
		inode->i_op = &ubifs_file_inode_operations;
		inode->i_fop = &ubifs_file_operations;
		break;
	case S_IFDIR:
		inode->i_op  = &ubifs_dir_inode_operations;
		inode->i_fop = &ubifs_dir_operations;
		inode->i_size = ui->ui_size = UBIFS_INO_NODE_SZ;
		break;
	case S_IFLNK:
		inode->i_op = &ubifs_symlink_inode_operations;
		break;
	case S_IFSOCK:
	case S_IFIFO:
	case S_IFBLK:
	case S_IFCHR:
		inode->i_op  = &ubifs_file_inode_operations;
		break;
	default:
		BUG();
	}

	ui->flags = inherit_flags(dir, mode);
	ubifs_set_inode_flags(inode);
	if (S_ISREG(mode))
		ui->compr_type = c->default_compr;
	else
		ui->compr_type = UBIFS_COMPR_NONE;
	ui->synced_i_size = 0;

	spin_lock(&c->cnt_lock);
	
	if (c->highest_inum >= INUM_WARN_WATERMARK) {
		if (c->highest_inum >= INUM_WATERMARK) {
			spin_unlock(&c->cnt_lock);
			ubifs_err("out of inode numbers");
			make_bad_inode(inode);
			iput(inode);
			return ERR_PTR(-EINVAL);
		}
		ubifs_warn("running out of inode numbers (current %lu, max %d)",
			   (unsigned long)c->highest_inum, INUM_WATERMARK);
	}

	inode->i_ino = ++c->highest_inum;
	ui->creat_sqnum = ++c->max_sqnum;
	spin_unlock(&c->cnt_lock);
	return inode;
}

#ifdef CONFIG_UBIFS_FS_DEBUG

static int dbg_check_name(const struct ubifs_info *c,
			  const struct ubifs_dent_node *dent,
			  const struct qstr *nm)
{
	if (!dbg_is_chk_gen(c))
		return 0;
	if (le16_to_cpu(dent->nlen) != nm->len)
		return -EINVAL;
	if (memcmp(dent->name, nm->name, nm->len))
		return -EINVAL;
	return 0;
}

#else

#define dbg_check_name(c, dent, nm) 0

#endif

static struct dentry *ubifs_lookup(struct inode *dir, struct dentry *dentry,
				   struct nameidata *nd)
{
	int err;
	union ubifs_key key;
	struct inode *inode = NULL;
	struct ubifs_dent_node *dent;
	struct ubifs_info *c = dir->i_sb->s_fs_info;

	dbg_gen("'%.*s' in dir ino %lu",
		dentry->d_name.len, dentry->d_name.name, dir->i_ino);

	if (dentry->d_name.len > UBIFS_MAX_NLEN)
		return ERR_PTR(-ENAMETOOLONG);

	dent = kmalloc(UBIFS_MAX_DENT_NODE_SZ, GFP_NOFS);
	if (!dent)
		return ERR_PTR(-ENOMEM);

	dent_key_init(c, &key, dir->i_ino, &dentry->d_name);

	err = ubifs_tnc_lookup_nm(c, &key, dent, &dentry->d_name);
	if (err) {
		if (err == -ENOENT) {
			dbg_gen("not found");
			goto done;
		}
		goto out;
	}

	if (dbg_check_name(c, dent, &dentry->d_name)) {
		err = -EINVAL;
		goto out;
	}

	inode = ubifs_iget(dir->i_sb, le64_to_cpu(dent->inum));
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		ubifs_err("dead directory entry '%.*s', error %d",
			  dentry->d_name.len, dentry->d_name.name, err);
		ubifs_ro_mode(c, err);
		goto out;
	}

done:
	kfree(dent);
	d_add(dentry, inode);
	return NULL;

out:
	kfree(dent);
	return ERR_PTR(err);
}

static int ubifs_create(struct inode *dir, struct dentry *dentry, umode_t mode,
			struct nameidata *nd)
{
	struct inode *inode;
	struct ubifs_info *c = dir->i_sb->s_fs_info;
	int err, sz_change = CALC_DENT_SIZE(dentry->d_name.len);
	struct ubifs_budget_req req = { .new_ino = 1, .new_dent = 1,
					.dirtied_ino = 1 };
	struct ubifs_inode *dir_ui = ubifs_inode(dir);


	dbg_gen("dent '%.*s', mode %#hx in dir ino %lu",
		dentry->d_name.len, dentry->d_name.name, mode, dir->i_ino);

	err = ubifs_budget_space(c, &req);
	if (err)
		return err;

	inode = ubifs_new_inode(c, dir, mode);
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		goto out_budg;
	}

	mutex_lock(&dir_ui->ui_mutex);
	dir->i_size += sz_change;
	dir_ui->ui_size = dir->i_size;
	dir->i_mtime = dir->i_ctime = inode->i_ctime;
	err = ubifs_jnl_update(c, dir, &dentry->d_name, inode, 0, 0);
	if (err)
		goto out_cancel;
	mutex_unlock(&dir_ui->ui_mutex);

	ubifs_release_budget(c, &req);
	insert_inode_hash(inode);
	d_instantiate(dentry, inode);
	return 0;

out_cancel:
	dir->i_size -= sz_change;
	dir_ui->ui_size = dir->i_size;
	mutex_unlock(&dir_ui->ui_mutex);
	make_bad_inode(inode);
	iput(inode);
out_budg:
	ubifs_release_budget(c, &req);
	ubifs_err("cannot create regular file, error %d", err);
	return err;
}

static unsigned int vfs_dent_type(uint8_t type)
{
	switch (type) {
	case UBIFS_ITYPE_REG:
		return DT_REG;
	case UBIFS_ITYPE_DIR:
		return DT_DIR;
	case UBIFS_ITYPE_LNK:
		return DT_LNK;
	case UBIFS_ITYPE_BLK:
		return DT_BLK;
	case UBIFS_ITYPE_CHR:
		return DT_CHR;
	case UBIFS_ITYPE_FIFO:
		return DT_FIFO;
	case UBIFS_ITYPE_SOCK:
		return DT_SOCK;
	default:
		BUG();
	}
	return 0;
}

static int ubifs_readdir(struct file *file, void *dirent, filldir_t filldir)
{
	int err, over = 0;
	struct qstr nm;
	union ubifs_key key;
	struct ubifs_dent_node *dent;
	struct inode *dir = file->f_path.dentry->d_inode;
	struct ubifs_info *c = dir->i_sb->s_fs_info;

	dbg_gen("dir ino %lu, f_pos %#llx", dir->i_ino, file->f_pos);

	if (file->f_pos > UBIFS_S_KEY_HASH_MASK || file->f_pos == 2)
		return 0;

	
	if (file->f_pos == 0) {
		ubifs_assert(!file->private_data);
		over = filldir(dirent, ".", 1, 0, dir->i_ino, DT_DIR);
		if (over)
			return 0;
		file->f_pos = 1;
	}

	if (file->f_pos == 1) {
		ubifs_assert(!file->private_data);
		over = filldir(dirent, "..", 2, 1,
			       parent_ino(file->f_path.dentry), DT_DIR);
		if (over)
			return 0;

		
		lowest_dent_key(c, &key, dir->i_ino);
		nm.name = NULL;
		dent = ubifs_tnc_next_ent(c, &key, &nm);
		if (IS_ERR(dent)) {
			err = PTR_ERR(dent);
			goto out;
		}

		file->f_pos = key_hash_flash(c, &dent->key);
		file->private_data = dent;
	}

	dent = file->private_data;
	if (!dent) {
		dent_key_init_hash(c, &key, dir->i_ino, file->f_pos);
		nm.name = NULL;
		dent = ubifs_tnc_next_ent(c, &key, &nm);
		if (IS_ERR(dent)) {
			err = PTR_ERR(dent);
			goto out;
		}
		file->f_pos = key_hash_flash(c, &dent->key);
		file->private_data = dent;
	}

	while (1) {
		dbg_gen("feed '%s', ino %llu, new f_pos %#x",
			dent->name, (unsigned long long)le64_to_cpu(dent->inum),
			key_hash_flash(c, &dent->key));
		ubifs_assert(le64_to_cpu(dent->ch.sqnum) >
			     ubifs_inode(dir)->creat_sqnum);

		nm.len = le16_to_cpu(dent->nlen);
		over = filldir(dirent, dent->name, nm.len, file->f_pos,
			       le64_to_cpu(dent->inum),
			       vfs_dent_type(dent->type));
		if (over)
			return 0;

		
		key_read(c, &dent->key, &key);
		nm.name = dent->name;
		dent = ubifs_tnc_next_ent(c, &key, &nm);
		if (IS_ERR(dent)) {
			err = PTR_ERR(dent);
			goto out;
		}

		kfree(file->private_data);
		file->f_pos = key_hash_flash(c, &dent->key);
		file->private_data = dent;
		cond_resched();
	}

out:
	if (err != -ENOENT) {
		ubifs_err("cannot find next direntry, error %d", err);
		return err;
	}

	kfree(file->private_data);
	file->private_data = NULL;
	file->f_pos = 2;
	return 0;
}

static loff_t ubifs_dir_llseek(struct file *file, loff_t offset, int origin)
{
	kfree(file->private_data);
	file->private_data = NULL;
	return generic_file_llseek(file, offset, origin);
}

static int ubifs_dir_release(struct inode *dir, struct file *file)
{
	kfree(file->private_data);
	file->private_data = NULL;
	return 0;
}

static void lock_2_inodes(struct inode *inode1, struct inode *inode2)
{
	mutex_lock_nested(&ubifs_inode(inode1)->ui_mutex, WB_MUTEX_1);
	mutex_lock_nested(&ubifs_inode(inode2)->ui_mutex, WB_MUTEX_2);
}

static void unlock_2_inodes(struct inode *inode1, struct inode *inode2)
{
	mutex_unlock(&ubifs_inode(inode2)->ui_mutex);
	mutex_unlock(&ubifs_inode(inode1)->ui_mutex);
}

static int ubifs_link(struct dentry *old_dentry, struct inode *dir,
		      struct dentry *dentry)
{
	struct ubifs_info *c = dir->i_sb->s_fs_info;
	struct inode *inode = old_dentry->d_inode;
	struct ubifs_inode *ui = ubifs_inode(inode);
	struct ubifs_inode *dir_ui = ubifs_inode(dir);
	int err, sz_change = CALC_DENT_SIZE(dentry->d_name.len);
	struct ubifs_budget_req req = { .new_dent = 1, .dirtied_ino = 2,
				.dirtied_ino_d = ALIGN(ui->data_len, 8) };


	dbg_gen("dent '%.*s' to ino %lu (nlink %d) in dir ino %lu",
		dentry->d_name.len, dentry->d_name.name, inode->i_ino,
		inode->i_nlink, dir->i_ino);
	ubifs_assert(mutex_is_locked(&dir->i_mutex));
	ubifs_assert(mutex_is_locked(&inode->i_mutex));

	err = dbg_check_synced_i_size(c, inode);
	if (err)
		return err;

	err = ubifs_budget_space(c, &req);
	if (err)
		return err;

	lock_2_inodes(dir, inode);
	inc_nlink(inode);
	ihold(inode);
	inode->i_ctime = ubifs_current_time(inode);
	dir->i_size += sz_change;
	dir_ui->ui_size = dir->i_size;
	dir->i_mtime = dir->i_ctime = inode->i_ctime;
	err = ubifs_jnl_update(c, dir, &dentry->d_name, inode, 0, 0);
	if (err)
		goto out_cancel;
	unlock_2_inodes(dir, inode);

	ubifs_release_budget(c, &req);
	d_instantiate(dentry, inode);
	return 0;

out_cancel:
	dir->i_size -= sz_change;
	dir_ui->ui_size = dir->i_size;
	drop_nlink(inode);
	unlock_2_inodes(dir, inode);
	ubifs_release_budget(c, &req);
	iput(inode);
	return err;
}

static int ubifs_unlink(struct inode *dir, struct dentry *dentry)
{
	struct ubifs_info *c = dir->i_sb->s_fs_info;
	struct inode *inode = dentry->d_inode;
	struct ubifs_inode *dir_ui = ubifs_inode(dir);
	int sz_change = CALC_DENT_SIZE(dentry->d_name.len);
	int err, budgeted = 1;
	struct ubifs_budget_req req = { .mod_dent = 1, .dirtied_ino = 2 };
	unsigned int saved_nlink = inode->i_nlink;


	dbg_gen("dent '%.*s' from ino %lu (nlink %d) in dir ino %lu",
		dentry->d_name.len, dentry->d_name.name, inode->i_ino,
		inode->i_nlink, dir->i_ino);
	ubifs_assert(mutex_is_locked(&dir->i_mutex));
	ubifs_assert(mutex_is_locked(&inode->i_mutex));
	err = dbg_check_synced_i_size(c, inode);
	if (err)
		return err;

	err = ubifs_budget_space(c, &req);
	if (err) {
		if (err != -ENOSPC)
			return err;
		budgeted = 0;
	}

	lock_2_inodes(dir, inode);
	inode->i_ctime = ubifs_current_time(dir);
	drop_nlink(inode);
	dir->i_size -= sz_change;
	dir_ui->ui_size = dir->i_size;
	dir->i_mtime = dir->i_ctime = inode->i_ctime;
	err = ubifs_jnl_update(c, dir, &dentry->d_name, inode, 1, 0);
	if (err)
		goto out_cancel;
	unlock_2_inodes(dir, inode);

	if (budgeted)
		ubifs_release_budget(c, &req);
	else {
		
		c->bi.nospace = c->bi.nospace_rp = 0;
		smp_wmb();
	}
	return 0;

out_cancel:
	dir->i_size += sz_change;
	dir_ui->ui_size = dir->i_size;
	set_nlink(inode, saved_nlink);
	unlock_2_inodes(dir, inode);
	if (budgeted)
		ubifs_release_budget(c, &req);
	return err;
}

static int check_dir_empty(struct ubifs_info *c, struct inode *dir)
{
	struct qstr nm = { .name = NULL };
	struct ubifs_dent_node *dent;
	union ubifs_key key;
	int err;

	lowest_dent_key(c, &key, dir->i_ino);
	dent = ubifs_tnc_next_ent(c, &key, &nm);
	if (IS_ERR(dent)) {
		err = PTR_ERR(dent);
		if (err == -ENOENT)
			err = 0;
	} else {
		kfree(dent);
		err = -ENOTEMPTY;
	}
	return err;
}

static int ubifs_rmdir(struct inode *dir, struct dentry *dentry)
{
	struct ubifs_info *c = dir->i_sb->s_fs_info;
	struct inode *inode = dentry->d_inode;
	int sz_change = CALC_DENT_SIZE(dentry->d_name.len);
	int err, budgeted = 1;
	struct ubifs_inode *dir_ui = ubifs_inode(dir);
	struct ubifs_budget_req req = { .mod_dent = 1, .dirtied_ino = 2 };


	dbg_gen("directory '%.*s', ino %lu in dir ino %lu", dentry->d_name.len,
		dentry->d_name.name, inode->i_ino, dir->i_ino);
	ubifs_assert(mutex_is_locked(&dir->i_mutex));
	ubifs_assert(mutex_is_locked(&inode->i_mutex));
	err = check_dir_empty(c, dentry->d_inode);
	if (err)
		return err;

	err = ubifs_budget_space(c, &req);
	if (err) {
		if (err != -ENOSPC)
			return err;
		budgeted = 0;
	}

	lock_2_inodes(dir, inode);
	inode->i_ctime = ubifs_current_time(dir);
	clear_nlink(inode);
	drop_nlink(dir);
	dir->i_size -= sz_change;
	dir_ui->ui_size = dir->i_size;
	dir->i_mtime = dir->i_ctime = inode->i_ctime;
	err = ubifs_jnl_update(c, dir, &dentry->d_name, inode, 1, 0);
	if (err)
		goto out_cancel;
	unlock_2_inodes(dir, inode);

	if (budgeted)
		ubifs_release_budget(c, &req);
	else {
		
		c->bi.nospace = c->bi.nospace_rp = 0;
		smp_wmb();
	}
	return 0;

out_cancel:
	dir->i_size += sz_change;
	dir_ui->ui_size = dir->i_size;
	inc_nlink(dir);
	set_nlink(inode, 2);
	unlock_2_inodes(dir, inode);
	if (budgeted)
		ubifs_release_budget(c, &req);
	return err;
}

static int ubifs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
	struct inode *inode;
	struct ubifs_inode *dir_ui = ubifs_inode(dir);
	struct ubifs_info *c = dir->i_sb->s_fs_info;
	int err, sz_change = CALC_DENT_SIZE(dentry->d_name.len);
	struct ubifs_budget_req req = { .new_ino = 1, .new_dent = 1 };


	dbg_gen("dent '%.*s', mode %#hx in dir ino %lu",
		dentry->d_name.len, dentry->d_name.name, mode, dir->i_ino);

	err = ubifs_budget_space(c, &req);
	if (err)
		return err;

	inode = ubifs_new_inode(c, dir, S_IFDIR | mode);
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		goto out_budg;
	}

	mutex_lock(&dir_ui->ui_mutex);
	insert_inode_hash(inode);
	inc_nlink(inode);
	inc_nlink(dir);
	dir->i_size += sz_change;
	dir_ui->ui_size = dir->i_size;
	dir->i_mtime = dir->i_ctime = inode->i_ctime;
	err = ubifs_jnl_update(c, dir, &dentry->d_name, inode, 0, 0);
	if (err) {
		ubifs_err("cannot create directory, error %d", err);
		goto out_cancel;
	}
	mutex_unlock(&dir_ui->ui_mutex);

	ubifs_release_budget(c, &req);
	d_instantiate(dentry, inode);
	return 0;

out_cancel:
	dir->i_size -= sz_change;
	dir_ui->ui_size = dir->i_size;
	drop_nlink(dir);
	mutex_unlock(&dir_ui->ui_mutex);
	make_bad_inode(inode);
	iput(inode);
out_budg:
	ubifs_release_budget(c, &req);
	return err;
}

static int ubifs_mknod(struct inode *dir, struct dentry *dentry,
		       umode_t mode, dev_t rdev)
{
	struct inode *inode;
	struct ubifs_inode *ui;
	struct ubifs_inode *dir_ui = ubifs_inode(dir);
	struct ubifs_info *c = dir->i_sb->s_fs_info;
	union ubifs_dev_desc *dev = NULL;
	int sz_change = CALC_DENT_SIZE(dentry->d_name.len);
	int err, devlen = 0;
	struct ubifs_budget_req req = { .new_ino = 1, .new_dent = 1,
					.new_ino_d = ALIGN(devlen, 8),
					.dirtied_ino = 1 };


	dbg_gen("dent '%.*s' in dir ino %lu",
		dentry->d_name.len, dentry->d_name.name, dir->i_ino);

	if (!new_valid_dev(rdev))
		return -EINVAL;

	if (S_ISBLK(mode) || S_ISCHR(mode)) {
		dev = kmalloc(sizeof(union ubifs_dev_desc), GFP_NOFS);
		if (!dev)
			return -ENOMEM;
		devlen = ubifs_encode_dev(dev, rdev);
	}

	err = ubifs_budget_space(c, &req);
	if (err) {
		kfree(dev);
		return err;
	}

	inode = ubifs_new_inode(c, dir, mode);
	if (IS_ERR(inode)) {
		kfree(dev);
		err = PTR_ERR(inode);
		goto out_budg;
	}

	init_special_inode(inode, inode->i_mode, rdev);
	inode->i_size = ubifs_inode(inode)->ui_size = devlen;
	ui = ubifs_inode(inode);
	ui->data = dev;
	ui->data_len = devlen;

	mutex_lock(&dir_ui->ui_mutex);
	dir->i_size += sz_change;
	dir_ui->ui_size = dir->i_size;
	dir->i_mtime = dir->i_ctime = inode->i_ctime;
	err = ubifs_jnl_update(c, dir, &dentry->d_name, inode, 0, 0);
	if (err)
		goto out_cancel;
	mutex_unlock(&dir_ui->ui_mutex);

	ubifs_release_budget(c, &req);
	insert_inode_hash(inode);
	d_instantiate(dentry, inode);
	return 0;

out_cancel:
	dir->i_size -= sz_change;
	dir_ui->ui_size = dir->i_size;
	mutex_unlock(&dir_ui->ui_mutex);
	make_bad_inode(inode);
	iput(inode);
out_budg:
	ubifs_release_budget(c, &req);
	return err;
}

static int ubifs_symlink(struct inode *dir, struct dentry *dentry,
			 const char *symname)
{
	struct inode *inode;
	struct ubifs_inode *ui;
	struct ubifs_inode *dir_ui = ubifs_inode(dir);
	struct ubifs_info *c = dir->i_sb->s_fs_info;
	int err, len = strlen(symname);
	int sz_change = CALC_DENT_SIZE(dentry->d_name.len);
	struct ubifs_budget_req req = { .new_ino = 1, .new_dent = 1,
					.new_ino_d = ALIGN(len, 8),
					.dirtied_ino = 1 };


	dbg_gen("dent '%.*s', target '%s' in dir ino %lu", dentry->d_name.len,
		dentry->d_name.name, symname, dir->i_ino);

	if (len > UBIFS_MAX_INO_DATA)
		return -ENAMETOOLONG;

	err = ubifs_budget_space(c, &req);
	if (err)
		return err;

	inode = ubifs_new_inode(c, dir, S_IFLNK | S_IRWXUGO);
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		goto out_budg;
	}

	ui = ubifs_inode(inode);
	ui->data = kmalloc(len + 1, GFP_NOFS);
	if (!ui->data) {
		err = -ENOMEM;
		goto out_inode;
	}

	memcpy(ui->data, symname, len);
	((char *)ui->data)[len] = '\0';
	/*
	 * The terminating zero byte is not written to the flash media and it
	 * is put just to make later in-memory string processing simpler. Thus,
	 * data length is @len, not @len + %1.
	 */
	ui->data_len = len;
	inode->i_size = ubifs_inode(inode)->ui_size = len;

	mutex_lock(&dir_ui->ui_mutex);
	dir->i_size += sz_change;
	dir_ui->ui_size = dir->i_size;
	dir->i_mtime = dir->i_ctime = inode->i_ctime;
	err = ubifs_jnl_update(c, dir, &dentry->d_name, inode, 0, 0);
	if (err)
		goto out_cancel;
	mutex_unlock(&dir_ui->ui_mutex);

	ubifs_release_budget(c, &req);
	insert_inode_hash(inode);
	d_instantiate(dentry, inode);
	return 0;

out_cancel:
	dir->i_size -= sz_change;
	dir_ui->ui_size = dir->i_size;
	mutex_unlock(&dir_ui->ui_mutex);
out_inode:
	make_bad_inode(inode);
	iput(inode);
out_budg:
	ubifs_release_budget(c, &req);
	return err;
}

static void lock_3_inodes(struct inode *inode1, struct inode *inode2,
			  struct inode *inode3)
{
	mutex_lock_nested(&ubifs_inode(inode1)->ui_mutex, WB_MUTEX_1);
	if (inode2 != inode1)
		mutex_lock_nested(&ubifs_inode(inode2)->ui_mutex, WB_MUTEX_2);
	if (inode3)
		mutex_lock_nested(&ubifs_inode(inode3)->ui_mutex, WB_MUTEX_3);
}

static void unlock_3_inodes(struct inode *inode1, struct inode *inode2,
			    struct inode *inode3)
{
	if (inode3)
		mutex_unlock(&ubifs_inode(inode3)->ui_mutex);
	if (inode1 != inode2)
		mutex_unlock(&ubifs_inode(inode2)->ui_mutex);
	mutex_unlock(&ubifs_inode(inode1)->ui_mutex);
}

static int ubifs_rename(struct inode *old_dir, struct dentry *old_dentry,
			struct inode *new_dir, struct dentry *new_dentry)
{
	struct ubifs_info *c = old_dir->i_sb->s_fs_info;
	struct inode *old_inode = old_dentry->d_inode;
	struct inode *new_inode = new_dentry->d_inode;
	struct ubifs_inode *old_inode_ui = ubifs_inode(old_inode);
	int err, release, sync = 0, move = (new_dir != old_dir);
	int is_dir = S_ISDIR(old_inode->i_mode);
	int unlink = !!new_inode;
	int new_sz = CALC_DENT_SIZE(new_dentry->d_name.len);
	int old_sz = CALC_DENT_SIZE(old_dentry->d_name.len);
	struct ubifs_budget_req req = { .new_dent = 1, .mod_dent = 1,
					.dirtied_ino = 3 };
	struct ubifs_budget_req ino_req = { .dirtied_ino = 1,
			.dirtied_ino_d = ALIGN(old_inode_ui->data_len, 8) };
	struct timespec time;
	unsigned int saved_nlink = 0;


	dbg_gen("dent '%.*s' ino %lu in dir ino %lu to dent '%.*s' in "
		"dir ino %lu", old_dentry->d_name.len, old_dentry->d_name.name,
		old_inode->i_ino, old_dir->i_ino, new_dentry->d_name.len,
		new_dentry->d_name.name, new_dir->i_ino);
	ubifs_assert(mutex_is_locked(&old_dir->i_mutex));
	ubifs_assert(mutex_is_locked(&new_dir->i_mutex));
	if (unlink)
		ubifs_assert(mutex_is_locked(&new_inode->i_mutex));


	if (unlink && is_dir) {
		err = check_dir_empty(c, new_inode);
		if (err)
			return err;
	}

	err = ubifs_budget_space(c, &req);
	if (err)
		return err;
	err = ubifs_budget_space(c, &ino_req);
	if (err) {
		ubifs_release_budget(c, &req);
		return err;
	}

	lock_3_inodes(old_dir, new_dir, new_inode);

	time = ubifs_current_time(old_dir);
	old_inode->i_ctime = time;

	
	if (is_dir) {
		if (move) {
			drop_nlink(old_dir);
			if (!unlink)
				inc_nlink(new_dir);
		} else {
			if (unlink)
				drop_nlink(old_dir);
		}
	}

	old_dir->i_size -= old_sz;
	ubifs_inode(old_dir)->ui_size = old_dir->i_size;
	old_dir->i_mtime = old_dir->i_ctime = time;
	new_dir->i_mtime = new_dir->i_ctime = time;

	if (unlink) {
		saved_nlink = new_inode->i_nlink;
		if (is_dir)
			clear_nlink(new_inode);
		else
			drop_nlink(new_inode);
		new_inode->i_ctime = time;
	} else {
		new_dir->i_size += new_sz;
		ubifs_inode(new_dir)->ui_size = new_dir->i_size;
	}

	if (IS_SYNC(old_inode)) {
		sync = IS_DIRSYNC(old_dir) || IS_DIRSYNC(new_dir);
		if (unlink && IS_SYNC(new_inode))
			sync = 1;
	}
	err = ubifs_jnl_rename(c, old_dir, old_dentry, new_dir, new_dentry,
			       sync);
	if (err)
		goto out_cancel;

	unlock_3_inodes(old_dir, new_dir, new_inode);
	ubifs_release_budget(c, &req);

	mutex_lock(&old_inode_ui->ui_mutex);
	release = old_inode_ui->dirty;
	mark_inode_dirty_sync(old_inode);
	mutex_unlock(&old_inode_ui->ui_mutex);

	if (release)
		ubifs_release_budget(c, &ino_req);
	if (IS_SYNC(old_inode))
		err = old_inode->i_sb->s_op->write_inode(old_inode, NULL);
	return err;

out_cancel:
	if (unlink) {
		set_nlink(new_inode, saved_nlink);
	} else {
		new_dir->i_size -= new_sz;
		ubifs_inode(new_dir)->ui_size = new_dir->i_size;
	}
	old_dir->i_size += old_sz;
	ubifs_inode(old_dir)->ui_size = old_dir->i_size;
	if (is_dir) {
		if (move) {
			inc_nlink(old_dir);
			if (!unlink)
				drop_nlink(new_dir);
		} else {
			if (unlink)
				inc_nlink(old_dir);
		}
	}
	unlock_3_inodes(old_dir, new_dir, new_inode);
	ubifs_release_budget(c, &ino_req);
	ubifs_release_budget(c, &req);
	return err;
}

int ubifs_getattr(struct vfsmount *mnt, struct dentry *dentry,
		  struct kstat *stat)
{
	loff_t size;
	struct inode *inode = dentry->d_inode;
	struct ubifs_inode *ui = ubifs_inode(inode);

	mutex_lock(&ui->ui_mutex);
	stat->dev = inode->i_sb->s_dev;
	stat->ino = inode->i_ino;
	stat->mode = inode->i_mode;
	stat->nlink = inode->i_nlink;
	stat->uid = inode->i_uid;
	stat->gid = inode->i_gid;
	stat->rdev = inode->i_rdev;
	stat->atime = inode->i_atime;
	stat->mtime = inode->i_mtime;
	stat->ctime = inode->i_ctime;
	stat->blksize = UBIFS_BLOCK_SIZE;
	stat->size = ui->ui_size;

	if (S_ISREG(inode->i_mode)) {
		size = ui->xattr_size;
		size += stat->size;
		size = ALIGN(size, UBIFS_BLOCK_SIZE);
		stat->blocks = size >> 9;
	} else
		stat->blocks = 0;
	mutex_unlock(&ui->ui_mutex);
	return 0;
}

const struct inode_operations ubifs_dir_inode_operations = {
	.lookup      = ubifs_lookup,
	.create      = ubifs_create,
	.link        = ubifs_link,
	.symlink     = ubifs_symlink,
	.unlink      = ubifs_unlink,
	.mkdir       = ubifs_mkdir,
	.rmdir       = ubifs_rmdir,
	.mknod       = ubifs_mknod,
	.rename      = ubifs_rename,
	.setattr     = ubifs_setattr,
	.getattr     = ubifs_getattr,
#ifdef CONFIG_UBIFS_FS_XATTR
	.setxattr    = ubifs_setxattr,
	.getxattr    = ubifs_getxattr,
	.listxattr   = ubifs_listxattr,
	.removexattr = ubifs_removexattr,
#endif
};

const struct file_operations ubifs_dir_operations = {
	.llseek         = ubifs_dir_llseek,
	.release        = ubifs_dir_release,
	.read           = generic_read_dir,
	.readdir        = ubifs_readdir,
	.fsync          = ubifs_fsync,
	.unlocked_ioctl = ubifs_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = ubifs_compat_ioctl,
#endif
};
