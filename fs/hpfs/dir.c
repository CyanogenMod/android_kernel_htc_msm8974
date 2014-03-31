
#include <linux/slab.h>
#include "hpfs_fn.h"

static int hpfs_dir_release(struct inode *inode, struct file *filp)
{
	hpfs_lock(inode->i_sb);
	hpfs_del_pos(inode, &filp->f_pos);
	
	hpfs_unlock(inode->i_sb);
	return 0;
}


static loff_t hpfs_dir_lseek(struct file *filp, loff_t off, int whence)
{
	loff_t new_off = off + (whence == 1 ? filp->f_pos : 0);
	loff_t pos;
	struct quad_buffer_head qbh;
	struct inode *i = filp->f_path.dentry->d_inode;
	struct hpfs_inode_info *hpfs_inode = hpfs_i(i);
	struct super_block *s = i->i_sb;

	
	if (whence == SEEK_DATA || whence == SEEK_HOLE)
		return -EINVAL;

	hpfs_lock(s);

	
	if (new_off == 0 || new_off == 1 || new_off == 11 || new_off == 12 || new_off == 13) goto ok;
	mutex_lock(&i->i_mutex);
	pos = ((loff_t) hpfs_de_as_down_as_possible(s, hpfs_inode->i_dno) << 4) + 1;
	while (pos != new_off) {
		if (map_pos_dirent(i, &pos, &qbh)) hpfs_brelse4(&qbh);
		else goto fail;
		if (pos == 12) goto fail;
	}
	mutex_unlock(&i->i_mutex);
ok:
	hpfs_unlock(s);
	return filp->f_pos = new_off;
fail:
	mutex_unlock(&i->i_mutex);
	
	hpfs_unlock(s);
	return -ESPIPE;
}

static int hpfs_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct inode *inode = filp->f_path.dentry->d_inode;
	struct hpfs_inode_info *hpfs_inode = hpfs_i(inode);
	struct quad_buffer_head qbh;
	struct hpfs_dirent *de;
	int lc;
	long old_pos;
	unsigned char *tempname;
	int c1, c2 = 0;
	int ret = 0;

	hpfs_lock(inode->i_sb);

	if (hpfs_sb(inode->i_sb)->sb_chk) {
		if (hpfs_chk_sectors(inode->i_sb, inode->i_ino, 1, "dir_fnode")) {
			ret = -EFSERROR;
			goto out;
		}
		if (hpfs_chk_sectors(inode->i_sb, hpfs_inode->i_dno, 4, "dir_dnode")) {
			ret = -EFSERROR;
			goto out;
		}
	}
	if (hpfs_sb(inode->i_sb)->sb_chk >= 2) {
		struct buffer_head *bh;
		struct fnode *fno;
		int e = 0;
		if (!(fno = hpfs_map_fnode(inode->i_sb, inode->i_ino, &bh))) {
			ret = -EIOERROR;
			goto out;
		}
		if (!fno->dirflag) {
			e = 1;
			hpfs_error(inode->i_sb, "not a directory, fnode %08lx",
					(unsigned long)inode->i_ino);
		}
		if (hpfs_inode->i_dno != le32_to_cpu(fno->u.external[0].disk_secno)) {
			e = 1;
			hpfs_error(inode->i_sb, "corrupted inode: i_dno == %08x, fnode -> dnode == %08x", hpfs_inode->i_dno, le32_to_cpu(fno->u.external[0].disk_secno));
		}
		brelse(bh);
		if (e) {
			ret = -EFSERROR;
			goto out;
		}
	}
	lc = hpfs_sb(inode->i_sb)->sb_lowercase;
	if (filp->f_pos == 12) { 
		filp->f_pos = 13; 
		goto out;
	}
	if (filp->f_pos == 13) {
		ret = -ENOENT;
		goto out;
	}
	
	while (1) {
		again:
		if (hpfs_sb(inode->i_sb)->sb_chk)
			if (hpfs_stop_cycles(inode->i_sb, filp->f_pos, &c1, &c2, "hpfs_readdir")) {
				ret = -EFSERROR;
				goto out;
			}
		if (filp->f_pos == 12)
			goto out;
		if (filp->f_pos == 3 || filp->f_pos == 4 || filp->f_pos == 5) {
			printk("HPFS: warning: pos==%d\n",(int)filp->f_pos);
			goto out;
		}
		if (filp->f_pos == 0) {
			if (filldir(dirent, ".", 1, filp->f_pos, inode->i_ino, DT_DIR) < 0)
				goto out;
			filp->f_pos = 11;
		}
		if (filp->f_pos == 11) {
			if (filldir(dirent, "..", 2, filp->f_pos, hpfs_inode->i_parent_dir, DT_DIR) < 0)
				goto out;
			filp->f_pos = 1;
		}
		if (filp->f_pos == 1) {
			filp->f_pos = ((loff_t) hpfs_de_as_down_as_possible(inode->i_sb, hpfs_inode->i_dno) << 4) + 1;
			hpfs_add_pos(inode, &filp->f_pos);
			filp->f_version = inode->i_version;
		}
		old_pos = filp->f_pos;
		if (!(de = map_pos_dirent(inode, &filp->f_pos, &qbh))) {
			ret = -EIOERROR;
			goto out;
		}
		if (de->first || de->last) {
			if (hpfs_sb(inode->i_sb)->sb_chk) {
				if (de->first && !de->last && (de->namelen != 2
				    || de ->name[0] != 1 || de->name[1] != 1))
					hpfs_error(inode->i_sb, "hpfs_readdir: bad ^A^A entry; pos = %08lx", old_pos);
				if (de->last && (de->namelen != 1 || de ->name[0] != 255))
					hpfs_error(inode->i_sb, "hpfs_readdir: bad \\377 entry; pos = %08lx", old_pos);
			}
			hpfs_brelse4(&qbh);
			goto again;
		}
		tempname = hpfs_translate_name(inode->i_sb, de->name, de->namelen, lc, de->not_8x3);
		if (filldir(dirent, tempname, de->namelen, old_pos, le32_to_cpu(de->fnode), DT_UNKNOWN) < 0) {
			filp->f_pos = old_pos;
			if (tempname != de->name) kfree(tempname);
			hpfs_brelse4(&qbh);
			goto out;
		}
		if (tempname != de->name) kfree(tempname);
		hpfs_brelse4(&qbh);
	}
out:
	hpfs_unlock(inode->i_sb);
	return ret;
}


struct dentry *hpfs_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *nd)
{
	const unsigned char *name = dentry->d_name.name;
	unsigned len = dentry->d_name.len;
	struct quad_buffer_head qbh;
	struct hpfs_dirent *de;
	ino_t ino;
	int err;
	struct inode *result = NULL;
	struct hpfs_inode_info *hpfs_result;

	hpfs_lock(dir->i_sb);
	if ((err = hpfs_chk_name(name, &len))) {
		if (err == -ENAMETOOLONG) {
			hpfs_unlock(dir->i_sb);
			return ERR_PTR(-ENAMETOOLONG);
		}
		goto end_add;
	}


	de = map_dirent(dir, hpfs_i(dir)->i_dno, name, len, NULL, &qbh);


	if (!de) goto end;


	ino = le32_to_cpu(de->fnode);


	result = iget_locked(dir->i_sb, ino);
	if (!result) {
		hpfs_error(dir->i_sb, "hpfs_lookup: can't get inode");
		goto bail1;
	}
	if (result->i_state & I_NEW) {
		hpfs_init_inode(result);
		if (de->directory)
			hpfs_read_inode(result);
		else if (le32_to_cpu(de->ea_size) && hpfs_sb(dir->i_sb)->sb_eas)
			hpfs_read_inode(result);
		else {
			result->i_mode |= S_IFREG;
			result->i_mode &= ~0111;
			result->i_op = &hpfs_file_iops;
			result->i_fop = &hpfs_file_ops;
			set_nlink(result, 1);
		}
		unlock_new_inode(result);
	}
	hpfs_result = hpfs_i(result);
	if (!de->directory) hpfs_result->i_parent_dir = dir->i_ino;

	if (de->has_acl || de->has_xtd_perm) if (!(dir->i_sb->s_flags & MS_RDONLY)) {
		hpfs_error(result->i_sb, "ACLs or XPERM found. This is probably HPFS386. This driver doesn't support it now. Send me some info on these structures");
		goto bail1;
	}


	if (!result->i_ctime.tv_sec) {
		if (!(result->i_ctime.tv_sec = local_to_gmt(dir->i_sb, le32_to_cpu(de->creation_date))))
			result->i_ctime.tv_sec = 1;
		result->i_ctime.tv_nsec = 0;
		result->i_mtime.tv_sec = local_to_gmt(dir->i_sb, le32_to_cpu(de->write_date));
		result->i_mtime.tv_nsec = 0;
		result->i_atime.tv_sec = local_to_gmt(dir->i_sb, le32_to_cpu(de->read_date));
		result->i_atime.tv_nsec = 0;
		hpfs_result->i_ea_size = le32_to_cpu(de->ea_size);
		if (!hpfs_result->i_ea_mode && de->read_only)
			result->i_mode &= ~0222;
		if (!de->directory) {
			if (result->i_size == -1) {
				result->i_size = le32_to_cpu(de->file_size);
				result->i_data.a_ops = &hpfs_aops;
				hpfs_i(result)->mmu_private = result->i_size;
				result->i_blocks = 1 + ((result->i_size + 511) >> 9);
			}
		}
	}

	hpfs_brelse4(&qbh);


	end:
	end_add:
	hpfs_unlock(dir->i_sb);
	d_add(dentry, result);
	return NULL;

	bail1:
	
	hpfs_brelse4(&qbh);
	
	

	hpfs_unlock(dir->i_sb);
	return ERR_PTR(-ENOENT);
}

const struct file_operations hpfs_dir_ops =
{
	.llseek		= hpfs_dir_lseek,
	.read		= generic_read_dir,
	.readdir	= hpfs_readdir,
	.release	= hpfs_dir_release,
	.fsync		= hpfs_file_fsync,
};
