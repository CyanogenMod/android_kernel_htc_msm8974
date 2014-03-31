/* -*- mode: c; c-basic-offset: 8; -*-
 * vim: noexpandtab sw=8 ts=8 sts=0:
 *
 * namei.c
 *
 * Create and rename file, directory, symlinks
 *
 * Copyright (C) 2002, 2004 Oracle.  All rights reserved.
 *
 *  Portions of this code from linux/fs/ext3/dir.c
 *
 *  Copyright (C) 1992, 1993, 1994, 1995
 *  Remy Card (card@masi.ibp.fr)
 *  Laboratoire MASI - Institut Blaise pascal
 *  Universite Pierre et Marie Curie (Paris VI)
 *
 *   from
 *
 *   linux/fs/minix/dir.c
 *
 *   Copyright (C) 1991, 1992 Linux Torvalds
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
 */

#include <linux/fs.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/highmem.h>
#include <linux/quotaops.h>

#include <cluster/masklog.h>

#include "ocfs2.h"

#include "alloc.h"
#include "dcache.h"
#include "dir.h"
#include "dlmglue.h"
#include "extent_map.h"
#include "file.h"
#include "inode.h"
#include "journal.h"
#include "namei.h"
#include "suballoc.h"
#include "super.h"
#include "symlink.h"
#include "sysfile.h"
#include "uptodate.h"
#include "xattr.h"
#include "acl.h"
#include "ocfs2_trace.h"

#include "buffer_head_io.h"

static int ocfs2_mknod_locked(struct ocfs2_super *osb,
			      struct inode *dir,
			      struct inode *inode,
			      dev_t dev,
			      struct buffer_head **new_fe_bh,
			      struct buffer_head *parent_fe_bh,
			      handle_t *handle,
			      struct ocfs2_alloc_context *inode_ac);

static int ocfs2_prepare_orphan_dir(struct ocfs2_super *osb,
				    struct inode **ret_orphan_dir,
				    u64 blkno,
				    char *name,
				    struct ocfs2_dir_lookup_result *lookup);

static int ocfs2_orphan_add(struct ocfs2_super *osb,
			    handle_t *handle,
			    struct inode *inode,
			    struct buffer_head *fe_bh,
			    char *name,
			    struct ocfs2_dir_lookup_result *lookup,
			    struct inode *orphan_dir_inode);

static int ocfs2_create_symlink_data(struct ocfs2_super *osb,
				     handle_t *handle,
				     struct inode *inode,
				     const char *symname);

#define OCFS2_ORPHAN_NAMELEN ((int)(2 * sizeof(u64)))

static struct dentry *ocfs2_lookup(struct inode *dir, struct dentry *dentry,
				   struct nameidata *nd)
{
	int status;
	u64 blkno;
	struct inode *inode = NULL;
	struct dentry *ret;
	struct ocfs2_inode_info *oi;

	trace_ocfs2_lookup(dir, dentry, dentry->d_name.len,
			   dentry->d_name.name,
			   (unsigned long long)OCFS2_I(dir)->ip_blkno, 0);

	if (dentry->d_name.len > OCFS2_MAX_FILENAME_LEN) {
		ret = ERR_PTR(-ENAMETOOLONG);
		goto bail;
	}

	status = ocfs2_inode_lock_nested(dir, NULL, 0, OI_LS_PARENT);
	if (status < 0) {
		if (status != -ENOENT)
			mlog_errno(status);
		ret = ERR_PTR(status);
		goto bail;
	}

	status = ocfs2_lookup_ino_from_name(dir, dentry->d_name.name,
					    dentry->d_name.len, &blkno);
	if (status < 0)
		goto bail_add;

	inode = ocfs2_iget(OCFS2_SB(dir->i_sb), blkno, 0, 0);
	if (IS_ERR(inode)) {
		ret = ERR_PTR(-EACCES);
		goto bail_unlock;
	}

	oi = OCFS2_I(inode);
	spin_lock(&oi->ip_lock);
	oi->ip_flags &= ~OCFS2_INODE_MAYBE_ORPHANED;
	spin_unlock(&oi->ip_lock);

bail_add:
	ret = d_splice_alias(inode, dentry);

	if (inode) {
		if (ret)
			dentry = ret;

		status = ocfs2_dentry_attach_lock(dentry, inode,
						  OCFS2_I(dir)->ip_blkno);
		if (status) {
			mlog_errno(status);
			ret = ERR_PTR(status);
			goto bail_unlock;
		}
	} else
		ocfs2_dentry_attach_gen(dentry);

bail_unlock:
	ocfs2_inode_unlock(dir, 0);

bail:

	trace_ocfs2_lookup_ret(ret);

	return ret;
}

static struct inode *ocfs2_get_init_inode(struct inode *dir, umode_t mode)
{
	struct inode *inode;

	inode = new_inode(dir->i_sb);
	if (!inode) {
		mlog(ML_ERROR, "new_inode failed!\n");
		return NULL;
	}

	if (S_ISDIR(mode))
		set_nlink(inode, 2);
	inode_init_owner(inode, dir, mode);
	dquot_initialize(inode);
	return inode;
}

static int ocfs2_mknod(struct inode *dir,
		       struct dentry *dentry,
		       umode_t mode,
		       dev_t dev)
{
	int status = 0;
	struct buffer_head *parent_fe_bh = NULL;
	handle_t *handle = NULL;
	struct ocfs2_super *osb;
	struct ocfs2_dinode *dirfe;
	struct buffer_head *new_fe_bh = NULL;
	struct inode *inode = NULL;
	struct ocfs2_alloc_context *inode_ac = NULL;
	struct ocfs2_alloc_context *data_ac = NULL;
	struct ocfs2_alloc_context *meta_ac = NULL;
	int want_clusters = 0;
	int want_meta = 0;
	int xattr_credits = 0;
	struct ocfs2_security_xattr_info si = {
		.enable = 1,
	};
	int did_quota_inode = 0;
	struct ocfs2_dir_lookup_result lookup = { NULL, };
	sigset_t oldset;
	int did_block_signals = 0;

	trace_ocfs2_mknod(dir, dentry, dentry->d_name.len, dentry->d_name.name,
			  (unsigned long long)OCFS2_I(dir)->ip_blkno,
			  (unsigned long)dev, mode);

	dquot_initialize(dir);

	
	osb = OCFS2_SB(dir->i_sb);

	status = ocfs2_inode_lock(dir, &parent_fe_bh, 1);
	if (status < 0) {
		if (status != -ENOENT)
			mlog_errno(status);
		return status;
	}

	if (S_ISDIR(mode) && (dir->i_nlink >= ocfs2_link_max(osb))) {
		status = -EMLINK;
		goto leave;
	}

	dirfe = (struct ocfs2_dinode *) parent_fe_bh->b_data;
	if (!ocfs2_read_links_count(dirfe)) {
		
		status = -ENOENT;
		goto leave;
	}

	status = ocfs2_check_dir_for_entry(dir, dentry->d_name.name,
					   dentry->d_name.len);
	if (status)
		goto leave;

	
	status = ocfs2_prepare_dir_for_insert(osb, dir, parent_fe_bh,
					      dentry->d_name.name,
					      dentry->d_name.len, &lookup);
	if (status < 0) {
		mlog_errno(status);
		goto leave;
	}

	
	status = ocfs2_reserve_new_inode(osb, &inode_ac);
	if (status < 0) {
		if (status != -ENOSPC)
			mlog_errno(status);
		goto leave;
	}

	inode = ocfs2_get_init_inode(dir, mode);
	if (!inode) {
		status = -ENOMEM;
		mlog_errno(status);
		goto leave;
	}

	
	status = ocfs2_init_security_get(inode, dir, &dentry->d_name, &si);
	if (status) {
		if (status == -EOPNOTSUPP)
			si.enable = 0;
		else {
			mlog_errno(status);
			goto leave;
		}
	}

	
	status = ocfs2_calc_xattr_init(dir, parent_fe_bh, mode,
				       &si, &want_clusters,
				       &xattr_credits, &want_meta);
	if (status < 0) {
		mlog_errno(status);
		goto leave;
	}

	
	if (S_ISDIR(mode) && !ocfs2_supports_inline_data(osb)) {
		want_clusters += 1;

		
		if (ocfs2_supports_indexed_dirs(osb))
			want_meta++;
	}

	status = ocfs2_reserve_new_metadata_blocks(osb, want_meta, &meta_ac);
	if (status < 0) {
		if (status != -ENOSPC)
			mlog_errno(status);
		goto leave;
	}

	status = ocfs2_reserve_clusters(osb, want_clusters, &data_ac);
	if (status < 0) {
		if (status != -ENOSPC)
			mlog_errno(status);
		goto leave;
	}

	handle = ocfs2_start_trans(osb, ocfs2_mknod_credits(osb->sb,
							    S_ISDIR(mode),
							    xattr_credits));
	if (IS_ERR(handle)) {
		status = PTR_ERR(handle);
		handle = NULL;
		mlog_errno(status);
		goto leave;
	}

	
	ocfs2_block_signals(&oldset);
	did_block_signals = 1;

	status = dquot_alloc_inode(inode);
	if (status)
		goto leave;
	did_quota_inode = 1;

	
	status = ocfs2_mknod_locked(osb, dir, inode, dev,
				    &new_fe_bh, parent_fe_bh, handle,
				    inode_ac);
	if (status < 0) {
		mlog_errno(status);
		goto leave;
	}

	if (S_ISDIR(mode)) {
		status = ocfs2_fill_new_dir(osb, handle, dir, inode,
					    new_fe_bh, data_ac, meta_ac);
		if (status < 0) {
			mlog_errno(status);
			goto leave;
		}

		status = ocfs2_journal_access_di(handle, INODE_CACHE(dir),
						 parent_fe_bh,
						 OCFS2_JOURNAL_ACCESS_WRITE);
		if (status < 0) {
			mlog_errno(status);
			goto leave;
		}
		ocfs2_add_links_count(dirfe, 1);
		ocfs2_journal_dirty(handle, parent_fe_bh);
		inc_nlink(dir);
	}

	status = ocfs2_init_acl(handle, inode, dir, new_fe_bh, parent_fe_bh,
				meta_ac, data_ac);
	if (status < 0) {
		mlog_errno(status);
		goto leave;
	}

	if (si.enable) {
		status = ocfs2_init_security_set(handle, inode, new_fe_bh, &si,
						 meta_ac, data_ac);
		if (status < 0) {
			mlog_errno(status);
			goto leave;
		}
	}

	status = ocfs2_dentry_attach_lock(dentry, inode,
					  OCFS2_I(dir)->ip_blkno);
	if (status) {
		mlog_errno(status);
		goto leave;
	}

	status = ocfs2_add_entry(handle, dentry, inode,
				 OCFS2_I(inode)->ip_blkno, parent_fe_bh,
				 &lookup);
	if (status < 0) {
		mlog_errno(status);
		goto leave;
	}

	insert_inode_hash(inode);
	d_instantiate(dentry, inode);
	status = 0;
leave:
	if (status < 0 && did_quota_inode)
		dquot_free_inode(inode);
	if (handle)
		ocfs2_commit_trans(osb, handle);

	ocfs2_inode_unlock(dir, 1);
	if (did_block_signals)
		ocfs2_unblock_signals(&oldset);

	brelse(new_fe_bh);
	brelse(parent_fe_bh);
	kfree(si.name);
	kfree(si.value);

	ocfs2_free_dir_lookup_result(&lookup);

	if (inode_ac)
		ocfs2_free_alloc_context(inode_ac);

	if (data_ac)
		ocfs2_free_alloc_context(data_ac);

	if (meta_ac)
		ocfs2_free_alloc_context(meta_ac);

	if ((status < 0) && inode) {
		OCFS2_I(inode)->ip_flags |= OCFS2_INODE_SKIP_ORPHAN_DIR;
		clear_nlink(inode);
		iput(inode);
	}

	if (status)
		mlog_errno(status);

	return status;
}

static int __ocfs2_mknod_locked(struct inode *dir,
				struct inode *inode,
				dev_t dev,
				struct buffer_head **new_fe_bh,
				struct buffer_head *parent_fe_bh,
				handle_t *handle,
				struct ocfs2_alloc_context *inode_ac,
				u64 fe_blkno, u64 suballoc_loc, u16 suballoc_bit)
{
	int status = 0;
	struct ocfs2_super *osb = OCFS2_SB(dir->i_sb);
	struct ocfs2_dinode *fe = NULL;
	struct ocfs2_extent_list *fel;
	u16 feat;

	*new_fe_bh = NULL;

	inode->i_ino = ino_from_blkno(osb->sb, fe_blkno);
	OCFS2_I(inode)->ip_blkno = fe_blkno;
	spin_lock(&osb->osb_lock);
	inode->i_generation = osb->s_next_generation++;
	spin_unlock(&osb->osb_lock);

	*new_fe_bh = sb_getblk(osb->sb, fe_blkno);
	if (!*new_fe_bh) {
		status = -EIO;
		mlog_errno(status);
		goto leave;
	}
	ocfs2_set_new_buffer_uptodate(INODE_CACHE(inode), *new_fe_bh);

	status = ocfs2_journal_access_di(handle, INODE_CACHE(inode),
					 *new_fe_bh,
					 OCFS2_JOURNAL_ACCESS_CREATE);
	if (status < 0) {
		mlog_errno(status);
		goto leave;
	}

	fe = (struct ocfs2_dinode *) (*new_fe_bh)->b_data;
	memset(fe, 0, osb->sb->s_blocksize);

	fe->i_generation = cpu_to_le32(inode->i_generation);
	fe->i_fs_generation = cpu_to_le32(osb->fs_generation);
	fe->i_blkno = cpu_to_le64(fe_blkno);
	fe->i_suballoc_loc = cpu_to_le64(suballoc_loc);
	fe->i_suballoc_bit = cpu_to_le16(suballoc_bit);
	fe->i_suballoc_slot = cpu_to_le16(inode_ac->ac_alloc_slot);
	fe->i_uid = cpu_to_le32(inode->i_uid);
	fe->i_gid = cpu_to_le32(inode->i_gid);
	fe->i_mode = cpu_to_le16(inode->i_mode);
	if (S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode))
		fe->id1.dev1.i_rdev = cpu_to_le64(huge_encode_dev(dev));

	ocfs2_set_links_count(fe, inode->i_nlink);

	fe->i_last_eb_blk = 0;
	strcpy(fe->i_signature, OCFS2_INODE_SIGNATURE);
	le32_add_cpu(&fe->i_flags, OCFS2_VALID_FL);
	fe->i_atime = fe->i_ctime = fe->i_mtime =
		cpu_to_le64(CURRENT_TIME.tv_sec);
	fe->i_mtime_nsec = fe->i_ctime_nsec = fe->i_atime_nsec =
		cpu_to_le32(CURRENT_TIME.tv_nsec);
	fe->i_dtime = 0;

	feat = le16_to_cpu(fe->i_dyn_features);
	if (S_ISDIR(inode->i_mode) && ocfs2_supports_inline_data(osb)) {
		fe->i_dyn_features = cpu_to_le16(feat | OCFS2_INLINE_DATA_FL);

		fe->id2.i_data.id_count = cpu_to_le16(
				ocfs2_max_inline_data_with_xattr(osb->sb, fe));
	} else {
		fel = &fe->id2.i_list;
		fel->l_tree_depth = 0;
		fel->l_next_free_rec = 0;
		fel->l_count = cpu_to_le16(ocfs2_extent_recs_per_inode(osb->sb));
	}

	ocfs2_journal_dirty(handle, *new_fe_bh);

	ocfs2_populate_inode(inode, fe, 1);
	ocfs2_ci_set_new(osb, INODE_CACHE(inode));
	if (!ocfs2_mount_local(osb)) {
		status = ocfs2_create_new_inode_locks(inode);
		if (status < 0)
			mlog_errno(status);
	}

	status = 0; 

leave:
	if (status < 0) {
		if (*new_fe_bh) {
			brelse(*new_fe_bh);
			*new_fe_bh = NULL;
		}
	}

	if (status)
		mlog_errno(status);
	return status;
}

static int ocfs2_mknod_locked(struct ocfs2_super *osb,
			      struct inode *dir,
			      struct inode *inode,
			      dev_t dev,
			      struct buffer_head **new_fe_bh,
			      struct buffer_head *parent_fe_bh,
			      handle_t *handle,
			      struct ocfs2_alloc_context *inode_ac)
{
	int status = 0;
	u64 suballoc_loc, fe_blkno = 0;
	u16 suballoc_bit;

	*new_fe_bh = NULL;

	status = ocfs2_claim_new_inode(handle, dir, parent_fe_bh,
				       inode_ac, &suballoc_loc,
				       &suballoc_bit, &fe_blkno);
	if (status < 0) {
		mlog_errno(status);
		return status;
	}

	return __ocfs2_mknod_locked(dir, inode, dev, new_fe_bh,
				    parent_fe_bh, handle, inode_ac,
				    fe_blkno, suballoc_loc, suballoc_bit);
}

static int ocfs2_mkdir(struct inode *dir,
		       struct dentry *dentry,
		       umode_t mode)
{
	int ret;

	trace_ocfs2_mkdir(dir, dentry, dentry->d_name.len, dentry->d_name.name,
			  OCFS2_I(dir)->ip_blkno, mode);
	ret = ocfs2_mknod(dir, dentry, mode | S_IFDIR, 0);
	if (ret)
		mlog_errno(ret);

	return ret;
}

static int ocfs2_create(struct inode *dir,
			struct dentry *dentry,
			umode_t mode,
			struct nameidata *nd)
{
	int ret;

	trace_ocfs2_create(dir, dentry, dentry->d_name.len, dentry->d_name.name,
			   (unsigned long long)OCFS2_I(dir)->ip_blkno, mode);
	ret = ocfs2_mknod(dir, dentry, mode | S_IFREG, 0);
	if (ret)
		mlog_errno(ret);

	return ret;
}

static int ocfs2_link(struct dentry *old_dentry,
		      struct inode *dir,
		      struct dentry *dentry)
{
	handle_t *handle;
	struct inode *inode = old_dentry->d_inode;
	int err;
	struct buffer_head *fe_bh = NULL;
	struct buffer_head *parent_fe_bh = NULL;
	struct ocfs2_dinode *fe = NULL;
	struct ocfs2_super *osb = OCFS2_SB(dir->i_sb);
	struct ocfs2_dir_lookup_result lookup = { NULL, };
	sigset_t oldset;

	trace_ocfs2_link((unsigned long long)OCFS2_I(inode)->ip_blkno,
			 old_dentry->d_name.len, old_dentry->d_name.name,
			 dentry->d_name.len, dentry->d_name.name);

	if (S_ISDIR(inode->i_mode))
		return -EPERM;

	dquot_initialize(dir);

	err = ocfs2_inode_lock_nested(dir, &parent_fe_bh, 1, OI_LS_PARENT);
	if (err < 0) {
		if (err != -ENOENT)
			mlog_errno(err);
		return err;
	}

	if (!dir->i_nlink) {
		err = -ENOENT;
		goto out;
	}

	err = ocfs2_check_dir_for_entry(dir, dentry->d_name.name,
					dentry->d_name.len);
	if (err)
		goto out;

	err = ocfs2_prepare_dir_for_insert(osb, dir, parent_fe_bh,
					   dentry->d_name.name,
					   dentry->d_name.len, &lookup);
	if (err < 0) {
		mlog_errno(err);
		goto out;
	}

	err = ocfs2_inode_lock(inode, &fe_bh, 1);
	if (err < 0) {
		if (err != -ENOENT)
			mlog_errno(err);
		goto out;
	}

	fe = (struct ocfs2_dinode *) fe_bh->b_data;
	if (ocfs2_read_links_count(fe) >= ocfs2_link_max(osb)) {
		err = -EMLINK;
		goto out_unlock_inode;
	}

	handle = ocfs2_start_trans(osb, ocfs2_link_credits(osb->sb));
	if (IS_ERR(handle)) {
		err = PTR_ERR(handle);
		handle = NULL;
		mlog_errno(err);
		goto out_unlock_inode;
	}

	
	ocfs2_block_signals(&oldset);

	err = ocfs2_journal_access_di(handle, INODE_CACHE(inode), fe_bh,
				      OCFS2_JOURNAL_ACCESS_WRITE);
	if (err < 0) {
		mlog_errno(err);
		goto out_commit;
	}

	inc_nlink(inode);
	inode->i_ctime = CURRENT_TIME;
	ocfs2_set_links_count(fe, inode->i_nlink);
	fe->i_ctime = cpu_to_le64(inode->i_ctime.tv_sec);
	fe->i_ctime_nsec = cpu_to_le32(inode->i_ctime.tv_nsec);
	ocfs2_journal_dirty(handle, fe_bh);

	err = ocfs2_add_entry(handle, dentry, inode,
			      OCFS2_I(inode)->ip_blkno,
			      parent_fe_bh, &lookup);
	if (err) {
		ocfs2_add_links_count(fe, -1);
		drop_nlink(inode);
		mlog_errno(err);
		goto out_commit;
	}

	err = ocfs2_dentry_attach_lock(dentry, inode, OCFS2_I(dir)->ip_blkno);
	if (err) {
		mlog_errno(err);
		goto out_commit;
	}

	ihold(inode);
	d_instantiate(dentry, inode);

out_commit:
	ocfs2_commit_trans(osb, handle);
	ocfs2_unblock_signals(&oldset);
out_unlock_inode:
	ocfs2_inode_unlock(inode, 1);

out:
	ocfs2_inode_unlock(dir, 1);

	brelse(fe_bh);
	brelse(parent_fe_bh);

	ocfs2_free_dir_lookup_result(&lookup);

	if (err)
		mlog_errno(err);

	return err;
}

static int ocfs2_remote_dentry_delete(struct dentry *dentry)
{
	int ret;

	ret = ocfs2_dentry_lock(dentry, 1);
	if (ret)
		mlog_errno(ret);
	else
		ocfs2_dentry_unlock(dentry, 1);

	return ret;
}

static inline int inode_is_unlinkable(struct inode *inode)
{
	if (S_ISDIR(inode->i_mode)) {
		if (inode->i_nlink == 2)
			return 1;
		return 0;
	}

	if (inode->i_nlink == 1)
		return 1;
	return 0;
}

static int ocfs2_unlink(struct inode *dir,
			struct dentry *dentry)
{
	int status;
	int child_locked = 0;
	struct inode *inode = dentry->d_inode;
	struct inode *orphan_dir = NULL;
	struct ocfs2_super *osb = OCFS2_SB(dir->i_sb);
	u64 blkno;
	struct ocfs2_dinode *fe = NULL;
	struct buffer_head *fe_bh = NULL;
	struct buffer_head *parent_node_bh = NULL;
	handle_t *handle = NULL;
	char orphan_name[OCFS2_ORPHAN_NAMELEN + 1];
	struct ocfs2_dir_lookup_result lookup = { NULL, };
	struct ocfs2_dir_lookup_result orphan_insert = { NULL, };

	trace_ocfs2_unlink(dir, dentry, dentry->d_name.len,
			   dentry->d_name.name,
			   (unsigned long long)OCFS2_I(dir)->ip_blkno,
			   (unsigned long long)OCFS2_I(inode)->ip_blkno);

	dquot_initialize(dir);

	BUG_ON(dentry->d_parent->d_inode != dir);

	if (inode == osb->root_inode)
		return -EPERM;

	status = ocfs2_inode_lock_nested(dir, &parent_node_bh, 1,
					 OI_LS_PARENT);
	if (status < 0) {
		if (status != -ENOENT)
			mlog_errno(status);
		return status;
	}

	status = ocfs2_find_files_on_disk(dentry->d_name.name,
					  dentry->d_name.len, &blkno, dir,
					  &lookup);
	if (status < 0) {
		if (status != -ENOENT)
			mlog_errno(status);
		goto leave;
	}

	if (OCFS2_I(inode)->ip_blkno != blkno) {
		status = -ENOENT;

		trace_ocfs2_unlink_noent(
				(unsigned long long)OCFS2_I(inode)->ip_blkno,
				(unsigned long long)blkno,
				OCFS2_I(inode)->ip_flags);
		goto leave;
	}

	status = ocfs2_inode_lock(inode, &fe_bh, 1);
	if (status < 0) {
		if (status != -ENOENT)
			mlog_errno(status);
		goto leave;
	}
	child_locked = 1;

	if (S_ISDIR(inode->i_mode)) {
		if (inode->i_nlink != 2 || !ocfs2_empty_dir(inode)) {
			status = -ENOTEMPTY;
			goto leave;
		}
	}

	status = ocfs2_remote_dentry_delete(dentry);
	if (status < 0) {
		mlog_errno(status);
		goto leave;
	}

	if (inode_is_unlinkable(inode)) {
		status = ocfs2_prepare_orphan_dir(osb, &orphan_dir,
						  OCFS2_I(inode)->ip_blkno,
						  orphan_name, &orphan_insert);
		if (status < 0) {
			mlog_errno(status);
			goto leave;
		}
	}

	handle = ocfs2_start_trans(osb, ocfs2_unlink_credits(osb->sb));
	if (IS_ERR(handle)) {
		status = PTR_ERR(handle);
		handle = NULL;
		mlog_errno(status);
		goto leave;
	}

	status = ocfs2_journal_access_di(handle, INODE_CACHE(inode), fe_bh,
					 OCFS2_JOURNAL_ACCESS_WRITE);
	if (status < 0) {
		mlog_errno(status);
		goto leave;
	}

	fe = (struct ocfs2_dinode *) fe_bh->b_data;

	if (inode_is_unlinkable(inode)) {
		status = ocfs2_orphan_add(osb, handle, inode, fe_bh, orphan_name,
					  &orphan_insert, orphan_dir);
		if (status < 0) {
			mlog_errno(status);
			goto leave;
		}
	}

	
	status = ocfs2_delete_entry(handle, dir, &lookup);
	if (status < 0) {
		mlog_errno(status);
		goto leave;
	}

	if (S_ISDIR(inode->i_mode))
		drop_nlink(inode);
	drop_nlink(inode);
	ocfs2_set_links_count(fe, inode->i_nlink);
	ocfs2_journal_dirty(handle, fe_bh);

	dir->i_ctime = dir->i_mtime = CURRENT_TIME;
	if (S_ISDIR(inode->i_mode))
		drop_nlink(dir);

	status = ocfs2_mark_inode_dirty(handle, dir, parent_node_bh);
	if (status < 0) {
		mlog_errno(status);
		if (S_ISDIR(inode->i_mode))
			inc_nlink(dir);
	}

leave:
	if (handle)
		ocfs2_commit_trans(osb, handle);

	if (child_locked)
		ocfs2_inode_unlock(inode, 1);

	ocfs2_inode_unlock(dir, 1);

	if (orphan_dir) {
		
		ocfs2_inode_unlock(orphan_dir, 1);
		mutex_unlock(&orphan_dir->i_mutex);
		iput(orphan_dir);
	}

	brelse(fe_bh);
	brelse(parent_node_bh);

	ocfs2_free_dir_lookup_result(&orphan_insert);
	ocfs2_free_dir_lookup_result(&lookup);

	if (status)
		mlog_errno(status);

	return status;
}

static int ocfs2_double_lock(struct ocfs2_super *osb,
			     struct buffer_head **bh1,
			     struct inode *inode1,
			     struct buffer_head **bh2,
			     struct inode *inode2)
{
	int status;
	struct ocfs2_inode_info *oi1 = OCFS2_I(inode1);
	struct ocfs2_inode_info *oi2 = OCFS2_I(inode2);
	struct buffer_head **tmpbh;
	struct inode *tmpinode;

	trace_ocfs2_double_lock((unsigned long long)oi1->ip_blkno,
				(unsigned long long)oi2->ip_blkno);

	if (*bh1)
		*bh1 = NULL;
	if (*bh2)
		*bh2 = NULL;

	
	if (oi1->ip_blkno != oi2->ip_blkno) {
		if (oi1->ip_blkno < oi2->ip_blkno) {
			
			tmpbh = bh2;
			bh2 = bh1;
			bh1 = tmpbh;

			tmpinode = inode2;
			inode2 = inode1;
			inode1 = tmpinode;
		}
		
		status = ocfs2_inode_lock_nested(inode2, bh2, 1,
						 OI_LS_RENAME1);
		if (status < 0) {
			if (status != -ENOENT)
				mlog_errno(status);
			goto bail;
		}
	}

	
	status = ocfs2_inode_lock_nested(inode1, bh1, 1, OI_LS_RENAME2);
	if (status < 0) {
		if (oi1->ip_blkno != oi2->ip_blkno) {
			ocfs2_inode_unlock(inode2, 1);
			brelse(*bh2);
			*bh2 = NULL;
		}

		if (status != -ENOENT)
			mlog_errno(status);
	}

	trace_ocfs2_double_lock_end(
			(unsigned long long)OCFS2_I(inode1)->ip_blkno,
			(unsigned long long)OCFS2_I(inode2)->ip_blkno);

bail:
	if (status)
		mlog_errno(status);
	return status;
}

static void ocfs2_double_unlock(struct inode *inode1, struct inode *inode2)
{
	ocfs2_inode_unlock(inode1, 1);

	if (inode1 != inode2)
		ocfs2_inode_unlock(inode2, 1);
}

static int ocfs2_rename(struct inode *old_dir,
			struct dentry *old_dentry,
			struct inode *new_dir,
			struct dentry *new_dentry)
{
	int status = 0, rename_lock = 0, parents_locked = 0, target_exists = 0;
	int old_child_locked = 0, new_child_locked = 0, update_dot_dot = 0;
	struct inode *old_inode = old_dentry->d_inode;
	struct inode *new_inode = new_dentry->d_inode;
	struct inode *orphan_dir = NULL;
	struct ocfs2_dinode *newfe = NULL;
	char orphan_name[OCFS2_ORPHAN_NAMELEN + 1];
	struct buffer_head *newfe_bh = NULL;
	struct buffer_head *old_inode_bh = NULL;
	struct ocfs2_super *osb = NULL;
	u64 newfe_blkno, old_de_ino;
	handle_t *handle = NULL;
	struct buffer_head *old_dir_bh = NULL;
	struct buffer_head *new_dir_bh = NULL;
	u32 old_dir_nlink = old_dir->i_nlink;
	struct ocfs2_dinode *old_di;
	struct ocfs2_dir_lookup_result old_inode_dot_dot_res = { NULL, };
	struct ocfs2_dir_lookup_result target_lookup_res = { NULL, };
	struct ocfs2_dir_lookup_result old_entry_lookup = { NULL, };
	struct ocfs2_dir_lookup_result orphan_insert = { NULL, };
	struct ocfs2_dir_lookup_result target_insert = { NULL, };


	trace_ocfs2_rename(old_dir, old_dentry, new_dir, new_dentry,
			   old_dentry->d_name.len, old_dentry->d_name.name,
			   new_dentry->d_name.len, new_dentry->d_name.name);

	dquot_initialize(old_dir);
	dquot_initialize(new_dir);

	osb = OCFS2_SB(old_dir->i_sb);

	if (new_inode) {
		if (!igrab(new_inode))
			BUG();
	}

	if (old_dir != new_dir && S_ISDIR(old_inode->i_mode)) {
		status = ocfs2_rename_lock(osb);
		if (status < 0) {
			mlog_errno(status);
			goto bail;
		}
		rename_lock = 1;
	}

	
	status = ocfs2_double_lock(osb, &old_dir_bh, old_dir,
				   &new_dir_bh, new_dir);
	if (status < 0) {
		mlog_errno(status);
		goto bail;
	}
	parents_locked = 1;

	if (!new_dir_bh) {
		if (old_dir_bh) {
			new_dir_bh = old_dir_bh;
			get_bh(new_dir_bh);
		} else {
			mlog(ML_ERROR, "no old_dir_bh!\n");
			status = -EIO;
			goto bail;
		}
	}

	status = ocfs2_inode_lock_nested(old_inode, &old_inode_bh, 1,
					 OI_LS_PARENT);
	if (status < 0) {
		if (status != -ENOENT)
			mlog_errno(status);
		goto bail;
	}
	old_child_locked = 1;

	status = ocfs2_remote_dentry_delete(old_dentry);
	if (status < 0) {
		mlog_errno(status);
		goto bail;
	}

	if (S_ISDIR(old_inode->i_mode)) {
		u64 old_inode_parent;

		update_dot_dot = 1;
		status = ocfs2_find_files_on_disk("..", 2, &old_inode_parent,
						  old_inode,
						  &old_inode_dot_dot_res);
		if (status) {
			status = -EIO;
			goto bail;
		}

		if (old_inode_parent != OCFS2_I(old_dir)->ip_blkno) {
			status = -EIO;
			goto bail;
		}

		if (!new_inode && new_dir != old_dir &&
		    new_dir->i_nlink >= ocfs2_link_max(osb)) {
			status = -EMLINK;
			goto bail;
		}
	}

	status = ocfs2_lookup_ino_from_name(old_dir, old_dentry->d_name.name,
					    old_dentry->d_name.len,
					    &old_de_ino);
	if (status) {
		status = -ENOENT;
		goto bail;
	}

	if (old_de_ino != OCFS2_I(old_inode)->ip_blkno) {
		status = -ENOENT;
		goto bail;
	}

	status = ocfs2_find_files_on_disk(new_dentry->d_name.name,
					  new_dentry->d_name.len,
					  &newfe_blkno, new_dir,
					  &target_lookup_res);
	if ((status < 0) && (status != -ENOENT)) {
		
		
		mlog_errno(status);
		goto bail;
	}
	if (status == 0)
		target_exists = 1;

	if (!target_exists && new_inode) {
		status = -ENOENT;
		goto bail;
	}

	if (target_exists) {
		if (!new_inode) {
			status = -EACCES;

			trace_ocfs2_rename_target_exists(new_dentry->d_name.len,
						new_dentry->d_name.name);
			goto bail;
		}

		if (OCFS2_I(new_inode)->ip_blkno != newfe_blkno) {
			status = -EACCES;

			trace_ocfs2_rename_disagree(
			     (unsigned long long)OCFS2_I(new_inode)->ip_blkno,
			     (unsigned long long)newfe_blkno,
			     OCFS2_I(new_inode)->ip_flags);
			goto bail;
		}

		status = ocfs2_inode_lock(new_inode, &newfe_bh, 1);
		if (status < 0) {
			if (status != -ENOENT)
				mlog_errno(status);
			goto bail;
		}
		new_child_locked = 1;

		status = ocfs2_remote_dentry_delete(new_dentry);
		if (status < 0) {
			mlog_errno(status);
			goto bail;
		}

		newfe = (struct ocfs2_dinode *) newfe_bh->b_data;

		trace_ocfs2_rename_over_existing(
		     (unsigned long long)newfe_blkno, newfe_bh, newfe_bh ?
		     (unsigned long long)newfe_bh->b_blocknr : 0ULL);

		if (S_ISDIR(new_inode->i_mode) || (new_inode->i_nlink == 1)) {
			status = ocfs2_prepare_orphan_dir(osb, &orphan_dir,
						OCFS2_I(new_inode)->ip_blkno,
						orphan_name, &orphan_insert);
			if (status < 0) {
				mlog_errno(status);
				goto bail;
			}
		}
	} else {
		BUG_ON(new_dentry->d_parent->d_inode != new_dir);

		status = ocfs2_check_dir_for_entry(new_dir,
						   new_dentry->d_name.name,
						   new_dentry->d_name.len);
		if (status)
			goto bail;

		status = ocfs2_prepare_dir_for_insert(osb, new_dir, new_dir_bh,
						      new_dentry->d_name.name,
						      new_dentry->d_name.len,
						      &target_insert);
		if (status < 0) {
			mlog_errno(status);
			goto bail;
		}
	}

	handle = ocfs2_start_trans(osb, ocfs2_rename_credits(osb->sb));
	if (IS_ERR(handle)) {
		status = PTR_ERR(handle);
		handle = NULL;
		mlog_errno(status);
		goto bail;
	}

	if (target_exists) {
		if (S_ISDIR(new_inode->i_mode)) {
			if (new_inode->i_nlink != 2 ||
			    !ocfs2_empty_dir(new_inode)) {
				status = -ENOTEMPTY;
				goto bail;
			}
		}
		status = ocfs2_journal_access_di(handle, INODE_CACHE(new_inode),
						 newfe_bh,
						 OCFS2_JOURNAL_ACCESS_WRITE);
		if (status < 0) {
			mlog_errno(status);
			goto bail;
		}

		if (S_ISDIR(new_inode->i_mode) ||
		    (ocfs2_read_links_count(newfe) == 1)) {
			status = ocfs2_orphan_add(osb, handle, new_inode,
						  newfe_bh, orphan_name,
						  &orphan_insert, orphan_dir);
			if (status < 0) {
				mlog_errno(status);
				goto bail;
			}
		}

		
		status = ocfs2_update_entry(new_dir, handle, &target_lookup_res,
					    old_inode);
		if (status < 0) {
			mlog_errno(status);
			goto bail;
		}
		new_dir->i_version++;

		if (S_ISDIR(new_inode->i_mode))
			ocfs2_set_links_count(newfe, 0);
		else
			ocfs2_add_links_count(newfe, -1);
		ocfs2_journal_dirty(handle, newfe_bh);
	} else {
		
		status = ocfs2_add_entry(handle, new_dentry, old_inode,
					 OCFS2_I(old_inode)->ip_blkno,
					 new_dir_bh, &target_insert);
	}

	old_inode->i_ctime = CURRENT_TIME;
	mark_inode_dirty(old_inode);

	status = ocfs2_journal_access_di(handle, INODE_CACHE(old_inode),
					 old_inode_bh,
					 OCFS2_JOURNAL_ACCESS_WRITE);
	if (status >= 0) {
		old_di = (struct ocfs2_dinode *) old_inode_bh->b_data;

		old_di->i_ctime = cpu_to_le64(old_inode->i_ctime.tv_sec);
		old_di->i_ctime_nsec = cpu_to_le32(old_inode->i_ctime.tv_nsec);
		ocfs2_journal_dirty(handle, old_inode_bh);
	} else
		mlog_errno(status);

	status = ocfs2_find_entry(old_dentry->d_name.name,
				  old_dentry->d_name.len, old_dir,
				  &old_entry_lookup);
	if (status)
		goto bail;

	status = ocfs2_delete_entry(handle, old_dir, &old_entry_lookup);
	if (status < 0) {
		mlog_errno(status);
		goto bail;
	}

	if (new_inode) {
		drop_nlink(new_inode);
		new_inode->i_ctime = CURRENT_TIME;
	}
	old_dir->i_ctime = old_dir->i_mtime = CURRENT_TIME;

	if (update_dot_dot) {
		status = ocfs2_update_entry(old_inode, handle,
					    &old_inode_dot_dot_res, new_dir);
		drop_nlink(old_dir);
		if (new_inode) {
			drop_nlink(new_inode);
		} else {
			inc_nlink(new_dir);
			mark_inode_dirty(new_dir);
		}
	}
	mark_inode_dirty(old_dir);
	ocfs2_mark_inode_dirty(handle, old_dir, old_dir_bh);
	if (new_inode) {
		mark_inode_dirty(new_inode);
		ocfs2_mark_inode_dirty(handle, new_inode, newfe_bh);
	}

	if (old_dir != new_dir) {
		
		new_dir->i_ctime = new_dir->i_mtime = old_dir->i_ctime;

		ocfs2_mark_inode_dirty(handle, new_dir, new_dir_bh);
	}

	if (old_dir_nlink != old_dir->i_nlink) {
		if (!old_dir_bh) {
			mlog(ML_ERROR, "need to change nlink for old dir "
			     "%llu from %d to %d but bh is NULL!\n",
			     (unsigned long long)OCFS2_I(old_dir)->ip_blkno,
			     (int)old_dir_nlink, old_dir->i_nlink);
		} else {
			struct ocfs2_dinode *fe;
			status = ocfs2_journal_access_di(handle,
							 INODE_CACHE(old_dir),
							 old_dir_bh,
							 OCFS2_JOURNAL_ACCESS_WRITE);
			fe = (struct ocfs2_dinode *) old_dir_bh->b_data;
			ocfs2_set_links_count(fe, old_dir->i_nlink);
			ocfs2_journal_dirty(handle, old_dir_bh);
		}
	}
	ocfs2_dentry_move(old_dentry, new_dentry, old_dir, new_dir);
	status = 0;
bail:
	if (rename_lock)
		ocfs2_rename_unlock(osb);

	if (handle)
		ocfs2_commit_trans(osb, handle);

	if (parents_locked)
		ocfs2_double_unlock(old_dir, new_dir);

	if (old_child_locked)
		ocfs2_inode_unlock(old_inode, 1);

	if (new_child_locked)
		ocfs2_inode_unlock(new_inode, 1);

	if (orphan_dir) {
		
		ocfs2_inode_unlock(orphan_dir, 1);
		mutex_unlock(&orphan_dir->i_mutex);
		iput(orphan_dir);
	}

	if (new_inode)
		sync_mapping_buffers(old_inode->i_mapping);

	if (new_inode)
		iput(new_inode);

	ocfs2_free_dir_lookup_result(&target_lookup_res);
	ocfs2_free_dir_lookup_result(&old_entry_lookup);
	ocfs2_free_dir_lookup_result(&old_inode_dot_dot_res);
	ocfs2_free_dir_lookup_result(&orphan_insert);
	ocfs2_free_dir_lookup_result(&target_insert);

	brelse(newfe_bh);
	brelse(old_inode_bh);
	brelse(old_dir_bh);
	brelse(new_dir_bh);

	if (status)
		mlog_errno(status);

	return status;
}

static int ocfs2_create_symlink_data(struct ocfs2_super *osb,
				     handle_t *handle,
				     struct inode *inode,
				     const char *symname)
{
	struct buffer_head **bhs = NULL;
	const char *c;
	struct super_block *sb = osb->sb;
	u64 p_blkno, p_blocks;
	int virtual, blocks, status, i, bytes_left;

	bytes_left = i_size_read(inode) + 1;
	blocks = (bytes_left + sb->s_blocksize - 1) >> sb->s_blocksize_bits;

	trace_ocfs2_create_symlink_data((unsigned long long)inode->i_blocks,
					i_size_read(inode), blocks);

	
	if (bytes_left >
	    ocfs2_clusters_to_bytes(sb, OCFS2_I(inode)->ip_clusters)) {
		status = -EIO;
		mlog_errno(status);
		goto bail;
	}

	bhs = kcalloc(blocks, sizeof(struct buffer_head *), GFP_KERNEL);
	if (!bhs) {
		status = -ENOMEM;
		mlog_errno(status);
		goto bail;
	}

	status = ocfs2_extent_map_get_blocks(inode, 0, &p_blkno, &p_blocks,
					     NULL);
	if (status < 0) {
		mlog_errno(status);
		goto bail;
	}

	if ((p_blocks << sb->s_blocksize_bits) < bytes_left) {
		status = -EIO;
		mlog_errno(status);
		goto bail;
	}

	virtual = 0;
	while(bytes_left > 0) {
		c = &symname[virtual * sb->s_blocksize];

		bhs[virtual] = sb_getblk(sb, p_blkno);
		if (!bhs[virtual]) {
			status = -ENOMEM;
			mlog_errno(status);
			goto bail;
		}
		ocfs2_set_new_buffer_uptodate(INODE_CACHE(inode),
					      bhs[virtual]);

		status = ocfs2_journal_access(handle, INODE_CACHE(inode),
					      bhs[virtual],
					      OCFS2_JOURNAL_ACCESS_CREATE);
		if (status < 0) {
			mlog_errno(status);
			goto bail;
		}

		memset(bhs[virtual]->b_data, 0, sb->s_blocksize);

		memcpy(bhs[virtual]->b_data, c,
		       (bytes_left > sb->s_blocksize) ? sb->s_blocksize :
		       bytes_left);

		ocfs2_journal_dirty(handle, bhs[virtual]);

		virtual++;
		p_blkno++;
		bytes_left -= sb->s_blocksize;
	}

	status = 0;
bail:

	if (bhs) {
		for(i = 0; i < blocks; i++)
			brelse(bhs[i]);
		kfree(bhs);
	}

	if (status)
		mlog_errno(status);
	return status;
}

static int ocfs2_symlink(struct inode *dir,
			 struct dentry *dentry,
			 const char *symname)
{
	int status, l, credits;
	u64 newsize;
	struct ocfs2_super *osb = NULL;
	struct inode *inode = NULL;
	struct super_block *sb;
	struct buffer_head *new_fe_bh = NULL;
	struct buffer_head *parent_fe_bh = NULL;
	struct ocfs2_dinode *fe = NULL;
	struct ocfs2_dinode *dirfe;
	handle_t *handle = NULL;
	struct ocfs2_alloc_context *inode_ac = NULL;
	struct ocfs2_alloc_context *data_ac = NULL;
	struct ocfs2_alloc_context *xattr_ac = NULL;
	int want_clusters = 0;
	int xattr_credits = 0;
	struct ocfs2_security_xattr_info si = {
		.enable = 1,
	};
	int did_quota = 0, did_quota_inode = 0;
	struct ocfs2_dir_lookup_result lookup = { NULL, };
	sigset_t oldset;
	int did_block_signals = 0;

	trace_ocfs2_symlink_begin(dir, dentry, symname,
				  dentry->d_name.len, dentry->d_name.name);

	dquot_initialize(dir);

	sb = dir->i_sb;
	osb = OCFS2_SB(sb);

	l = strlen(symname) + 1;

	credits = ocfs2_calc_symlink_credits(sb);

	
	status = ocfs2_inode_lock(dir, &parent_fe_bh, 1);
	if (status < 0) {
		if (status != -ENOENT)
			mlog_errno(status);
		return status;
	}

	dirfe = (struct ocfs2_dinode *) parent_fe_bh->b_data;
	if (!ocfs2_read_links_count(dirfe)) {
		
		status = -ENOENT;
		goto bail;
	}

	status = ocfs2_check_dir_for_entry(dir, dentry->d_name.name,
					   dentry->d_name.len);
	if (status)
		goto bail;

	status = ocfs2_prepare_dir_for_insert(osb, dir, parent_fe_bh,
					      dentry->d_name.name,
					      dentry->d_name.len, &lookup);
	if (status < 0) {
		mlog_errno(status);
		goto bail;
	}

	status = ocfs2_reserve_new_inode(osb, &inode_ac);
	if (status < 0) {
		if (status != -ENOSPC)
			mlog_errno(status);
		goto bail;
	}

	inode = ocfs2_get_init_inode(dir, S_IFLNK | S_IRWXUGO);
	if (!inode) {
		status = -ENOMEM;
		mlog_errno(status);
		goto bail;
	}

	
	status = ocfs2_init_security_get(inode, dir, &dentry->d_name, &si);
	if (status) {
		if (status == -EOPNOTSUPP)
			si.enable = 0;
		else {
			mlog_errno(status);
			goto bail;
		}
	}

	
	if (si.enable) {
		status = ocfs2_calc_security_init(dir, &si, &want_clusters,
						  &xattr_credits, &xattr_ac);
		if (status < 0) {
			mlog_errno(status);
			goto bail;
		}
	}

	
	if (l > ocfs2_fast_symlink_chars(sb))
		want_clusters += 1;

	status = ocfs2_reserve_clusters(osb, want_clusters, &data_ac);
	if (status < 0) {
		if (status != -ENOSPC)
			mlog_errno(status);
		goto bail;
	}

	handle = ocfs2_start_trans(osb, credits + xattr_credits);
	if (IS_ERR(handle)) {
		status = PTR_ERR(handle);
		handle = NULL;
		mlog_errno(status);
		goto bail;
	}

	
	ocfs2_block_signals(&oldset);
	did_block_signals = 1;

	status = dquot_alloc_inode(inode);
	if (status)
		goto bail;
	did_quota_inode = 1;

	trace_ocfs2_symlink_create(dir, dentry, dentry->d_name.len,
				   dentry->d_name.name,
				   (unsigned long long)OCFS2_I(dir)->ip_blkno,
				   inode->i_mode);

	status = ocfs2_mknod_locked(osb, dir, inode,
				    0, &new_fe_bh, parent_fe_bh, handle,
				    inode_ac);
	if (status < 0) {
		mlog_errno(status);
		goto bail;
	}

	fe = (struct ocfs2_dinode *) new_fe_bh->b_data;
	inode->i_rdev = 0;
	newsize = l - 1;
	if (l > ocfs2_fast_symlink_chars(sb)) {
		u32 offset = 0;

		inode->i_op = &ocfs2_symlink_inode_operations;
		status = dquot_alloc_space_nodirty(inode,
		    ocfs2_clusters_to_bytes(osb->sb, 1));
		if (status)
			goto bail;
		did_quota = 1;
		status = ocfs2_add_inode_data(osb, inode, &offset, 1, 0,
					      new_fe_bh,
					      handle, data_ac, NULL,
					      NULL);
		if (status < 0) {
			if (status != -ENOSPC && status != -EINTR) {
				mlog(ML_ERROR,
				     "Failed to extend file to %llu\n",
				     (unsigned long long)newsize);
				mlog_errno(status);
				status = -ENOSPC;
			}
			goto bail;
		}
		i_size_write(inode, newsize);
		inode->i_blocks = ocfs2_inode_sector_count(inode);
	} else {
		inode->i_op = &ocfs2_fast_symlink_inode_operations;
		memcpy((char *) fe->id2.i_symlink, symname, l);
		i_size_write(inode, newsize);
		inode->i_blocks = 0;
	}

	status = ocfs2_mark_inode_dirty(handle, inode, new_fe_bh);
	if (status < 0) {
		mlog_errno(status);
		goto bail;
	}

	if (!ocfs2_inode_is_fast_symlink(inode)) {
		status = ocfs2_create_symlink_data(osb, handle, inode,
						   symname);
		if (status < 0) {
			mlog_errno(status);
			goto bail;
		}
	}

	if (si.enable) {
		status = ocfs2_init_security_set(handle, inode, new_fe_bh, &si,
						 xattr_ac, data_ac);
		if (status < 0) {
			mlog_errno(status);
			goto bail;
		}
	}

	status = ocfs2_dentry_attach_lock(dentry, inode, OCFS2_I(dir)->ip_blkno);
	if (status) {
		mlog_errno(status);
		goto bail;
	}

	status = ocfs2_add_entry(handle, dentry, inode,
				 le64_to_cpu(fe->i_blkno), parent_fe_bh,
				 &lookup);
	if (status < 0) {
		mlog_errno(status);
		goto bail;
	}

	insert_inode_hash(inode);
	d_instantiate(dentry, inode);
bail:
	if (status < 0 && did_quota)
		dquot_free_space_nodirty(inode,
					ocfs2_clusters_to_bytes(osb->sb, 1));
	if (status < 0 && did_quota_inode)
		dquot_free_inode(inode);
	if (handle)
		ocfs2_commit_trans(osb, handle);

	ocfs2_inode_unlock(dir, 1);
	if (did_block_signals)
		ocfs2_unblock_signals(&oldset);

	brelse(new_fe_bh);
	brelse(parent_fe_bh);
	kfree(si.name);
	kfree(si.value);
	ocfs2_free_dir_lookup_result(&lookup);
	if (inode_ac)
		ocfs2_free_alloc_context(inode_ac);
	if (data_ac)
		ocfs2_free_alloc_context(data_ac);
	if (xattr_ac)
		ocfs2_free_alloc_context(xattr_ac);
	if ((status < 0) && inode) {
		OCFS2_I(inode)->ip_flags |= OCFS2_INODE_SKIP_ORPHAN_DIR;
		clear_nlink(inode);
		iput(inode);
	}

	if (status)
		mlog_errno(status);

	return status;
}

static int ocfs2_blkno_stringify(u64 blkno, char *name)
{
	int status, namelen;

	namelen = snprintf(name, OCFS2_ORPHAN_NAMELEN + 1, "%016llx",
			   (long long)blkno);
	if (namelen <= 0) {
		if (namelen)
			status = namelen;
		else
			status = -EINVAL;
		mlog_errno(status);
		goto bail;
	}
	if (namelen != OCFS2_ORPHAN_NAMELEN) {
		status = -EINVAL;
		mlog_errno(status);
		goto bail;
	}

	trace_ocfs2_blkno_stringify(blkno, name, namelen);

	status = 0;
bail:
	if (status < 0)
		mlog_errno(status);
	return status;
}

static int ocfs2_lookup_lock_orphan_dir(struct ocfs2_super *osb,
					struct inode **ret_orphan_dir,
					struct buffer_head **ret_orphan_dir_bh)
{
	struct inode *orphan_dir_inode;
	struct buffer_head *orphan_dir_bh = NULL;
	int ret = 0;

	orphan_dir_inode = ocfs2_get_system_file_inode(osb,
						       ORPHAN_DIR_SYSTEM_INODE,
						       osb->slot_num);
	if (!orphan_dir_inode) {
		ret = -ENOENT;
		mlog_errno(ret);
		return ret;
	}

	mutex_lock(&orphan_dir_inode->i_mutex);

	ret = ocfs2_inode_lock(orphan_dir_inode, &orphan_dir_bh, 1);
	if (ret < 0) {
		mutex_unlock(&orphan_dir_inode->i_mutex);
		iput(orphan_dir_inode);

		mlog_errno(ret);
		return ret;
	}

	*ret_orphan_dir = orphan_dir_inode;
	*ret_orphan_dir_bh = orphan_dir_bh;

	return 0;
}

static int __ocfs2_prepare_orphan_dir(struct inode *orphan_dir_inode,
				      struct buffer_head *orphan_dir_bh,
				      u64 blkno,
				      char *name,
				      struct ocfs2_dir_lookup_result *lookup)
{
	int ret;
	struct ocfs2_super *osb = OCFS2_SB(orphan_dir_inode->i_sb);

	ret = ocfs2_blkno_stringify(blkno, name);
	if (ret < 0) {
		mlog_errno(ret);
		return ret;
	}

	ret = ocfs2_prepare_dir_for_insert(osb, orphan_dir_inode,
					   orphan_dir_bh, name,
					   OCFS2_ORPHAN_NAMELEN, lookup);
	if (ret < 0) {
		mlog_errno(ret);
		return ret;
	}

	return 0;
}

static int ocfs2_prepare_orphan_dir(struct ocfs2_super *osb,
				    struct inode **ret_orphan_dir,
				    u64 blkno,
				    char *name,
				    struct ocfs2_dir_lookup_result *lookup)
{
	struct inode *orphan_dir_inode = NULL;
	struct buffer_head *orphan_dir_bh = NULL;
	int ret = 0;

	ret = ocfs2_lookup_lock_orphan_dir(osb, &orphan_dir_inode,
					   &orphan_dir_bh);
	if (ret < 0) {
		mlog_errno(ret);
		return ret;
	}

	ret = __ocfs2_prepare_orphan_dir(orphan_dir_inode, orphan_dir_bh,
					 blkno, name, lookup);
	if (ret < 0) {
		mlog_errno(ret);
		goto out;
	}

	*ret_orphan_dir = orphan_dir_inode;

out:
	brelse(orphan_dir_bh);

	if (ret) {
		ocfs2_inode_unlock(orphan_dir_inode, 1);
		mutex_unlock(&orphan_dir_inode->i_mutex);
		iput(orphan_dir_inode);
	}

	if (ret)
		mlog_errno(ret);
	return ret;
}

static int ocfs2_orphan_add(struct ocfs2_super *osb,
			    handle_t *handle,
			    struct inode *inode,
			    struct buffer_head *fe_bh,
			    char *name,
			    struct ocfs2_dir_lookup_result *lookup,
			    struct inode *orphan_dir_inode)
{
	struct buffer_head *orphan_dir_bh = NULL;
	int status = 0;
	struct ocfs2_dinode *orphan_fe;
	struct ocfs2_dinode *fe = (struct ocfs2_dinode *) fe_bh->b_data;

	trace_ocfs2_orphan_add_begin(
				(unsigned long long)OCFS2_I(inode)->ip_blkno);

	status = ocfs2_read_inode_block(orphan_dir_inode, &orphan_dir_bh);
	if (status < 0) {
		mlog_errno(status);
		goto leave;
	}

	status = ocfs2_journal_access_di(handle,
					 INODE_CACHE(orphan_dir_inode),
					 orphan_dir_bh,
					 OCFS2_JOURNAL_ACCESS_WRITE);
	if (status < 0) {
		mlog_errno(status);
		goto leave;
	}

	orphan_fe = (struct ocfs2_dinode *) orphan_dir_bh->b_data;
	if (S_ISDIR(inode->i_mode))
		ocfs2_add_links_count(orphan_fe, 1);
	set_nlink(orphan_dir_inode, ocfs2_read_links_count(orphan_fe));
	ocfs2_journal_dirty(handle, orphan_dir_bh);

	status = __ocfs2_add_entry(handle, orphan_dir_inode, name,
				   OCFS2_ORPHAN_NAMELEN, inode,
				   OCFS2_I(inode)->ip_blkno,
				   orphan_dir_bh, lookup);
	if (status < 0) {
		mlog_errno(status);
		goto leave;
	}

	status = ocfs2_journal_access_di(handle,
					 INODE_CACHE(inode),
					 fe_bh,
					 OCFS2_JOURNAL_ACCESS_WRITE);
	if (status < 0) {
		mlog_errno(status);
		goto leave;
	}

	le32_add_cpu(&fe->i_flags, OCFS2_ORPHANED_FL);
	OCFS2_I(inode)->ip_flags &= ~OCFS2_INODE_SKIP_ORPHAN_DIR;

	fe->i_orphaned_slot = cpu_to_le16(osb->slot_num);

	ocfs2_journal_dirty(handle, fe_bh);

	trace_ocfs2_orphan_add_end((unsigned long long)OCFS2_I(inode)->ip_blkno,
				   osb->slot_num);

leave:
	brelse(orphan_dir_bh);

	if (status)
		mlog_errno(status);
	return status;
}

int ocfs2_orphan_del(struct ocfs2_super *osb,
		     handle_t *handle,
		     struct inode *orphan_dir_inode,
		     struct inode *inode,
		     struct buffer_head *orphan_dir_bh)
{
	char name[OCFS2_ORPHAN_NAMELEN + 1];
	struct ocfs2_dinode *orphan_fe;
	int status = 0;
	struct ocfs2_dir_lookup_result lookup = { NULL, };

	status = ocfs2_blkno_stringify(OCFS2_I(inode)->ip_blkno, name);
	if (status < 0) {
		mlog_errno(status);
		goto leave;
	}

	trace_ocfs2_orphan_del(
	     (unsigned long long)OCFS2_I(orphan_dir_inode)->ip_blkno,
	     name, OCFS2_ORPHAN_NAMELEN);

	
	status = ocfs2_find_entry(name, OCFS2_ORPHAN_NAMELEN, orphan_dir_inode,
				  &lookup);
	if (status) {
		mlog_errno(status);
		goto leave;
	}

	
	status = ocfs2_delete_entry(handle, orphan_dir_inode, &lookup);
	if (status < 0) {
		mlog_errno(status);
		goto leave;
	}

	status = ocfs2_journal_access_di(handle,
					 INODE_CACHE(orphan_dir_inode),
					 orphan_dir_bh,
					 OCFS2_JOURNAL_ACCESS_WRITE);
	if (status < 0) {
		mlog_errno(status);
		goto leave;
	}

	
	orphan_fe = (struct ocfs2_dinode *) orphan_dir_bh->b_data;
	if (S_ISDIR(inode->i_mode))
		ocfs2_add_links_count(orphan_fe, -1);
	set_nlink(orphan_dir_inode, ocfs2_read_links_count(orphan_fe));
	ocfs2_journal_dirty(handle, orphan_dir_bh);

leave:
	ocfs2_free_dir_lookup_result(&lookup);

	if (status)
		mlog_errno(status);
	return status;
}

static int ocfs2_prep_new_orphaned_file(struct inode *dir,
					struct buffer_head *dir_bh,
					char *orphan_name,
					struct inode **ret_orphan_dir,
					u64 *ret_di_blkno,
					struct ocfs2_dir_lookup_result *orphan_insert,
					struct ocfs2_alloc_context **ret_inode_ac)
{
	int ret;
	u64 di_blkno;
	struct ocfs2_super *osb = OCFS2_SB(dir->i_sb);
	struct inode *orphan_dir = NULL;
	struct buffer_head *orphan_dir_bh = NULL;
	struct ocfs2_alloc_context *inode_ac = NULL;

	ret = ocfs2_lookup_lock_orphan_dir(osb, &orphan_dir, &orphan_dir_bh);
	if (ret < 0) {
		mlog_errno(ret);
		return ret;
	}

	
	ret = ocfs2_reserve_new_inode(osb, &inode_ac);
	if (ret < 0) {
		if (ret != -ENOSPC)
			mlog_errno(ret);
		goto out;
	}

	ret = ocfs2_find_new_inode_loc(dir, dir_bh, inode_ac,
				       &di_blkno);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	ret = __ocfs2_prepare_orphan_dir(orphan_dir, orphan_dir_bh,
					 di_blkno, orphan_name, orphan_insert);
	if (ret < 0) {
		mlog_errno(ret);
		goto out;
	}

out:
	if (ret == 0) {
		*ret_orphan_dir = orphan_dir;
		*ret_di_blkno = di_blkno;
		*ret_inode_ac = inode_ac;
	} else {
		
		if (inode_ac)
			ocfs2_free_alloc_context(inode_ac);

		
		mutex_unlock(&orphan_dir->i_mutex);
		ocfs2_inode_unlock(orphan_dir, 1);
		iput(orphan_dir);
	}

	brelse(orphan_dir_bh);

	return 0;
}

int ocfs2_create_inode_in_orphan(struct inode *dir,
				 int mode,
				 struct inode **new_inode)
{
	int status, did_quota_inode = 0;
	struct inode *inode = NULL;
	struct inode *orphan_dir = NULL;
	struct ocfs2_super *osb = OCFS2_SB(dir->i_sb);
	struct ocfs2_dinode *di = NULL;
	handle_t *handle = NULL;
	char orphan_name[OCFS2_ORPHAN_NAMELEN + 1];
	struct buffer_head *parent_di_bh = NULL;
	struct buffer_head *new_di_bh = NULL;
	struct ocfs2_alloc_context *inode_ac = NULL;
	struct ocfs2_dir_lookup_result orphan_insert = { NULL, };
	u64 uninitialized_var(di_blkno), suballoc_loc;
	u16 suballoc_bit;

	status = ocfs2_inode_lock(dir, &parent_di_bh, 1);
	if (status < 0) {
		if (status != -ENOENT)
			mlog_errno(status);
		return status;
	}

	status = ocfs2_prep_new_orphaned_file(dir, parent_di_bh,
					      orphan_name, &orphan_dir,
					      &di_blkno, &orphan_insert, &inode_ac);
	if (status < 0) {
		if (status != -ENOSPC)
			mlog_errno(status);
		goto leave;
	}

	inode = ocfs2_get_init_inode(dir, mode);
	if (!inode) {
		status = -ENOMEM;
		mlog_errno(status);
		goto leave;
	}

	handle = ocfs2_start_trans(osb, ocfs2_mknod_credits(osb->sb, 0, 0));
	if (IS_ERR(handle)) {
		status = PTR_ERR(handle);
		handle = NULL;
		mlog_errno(status);
		goto leave;
	}

	status = dquot_alloc_inode(inode);
	if (status)
		goto leave;
	did_quota_inode = 1;

	status = ocfs2_claim_new_inode_at_loc(handle, dir, inode_ac,
					      &suballoc_loc,
					      &suballoc_bit, di_blkno);
	if (status < 0) {
		mlog_errno(status);
		goto leave;
	}

	clear_nlink(inode);
	
	status = __ocfs2_mknod_locked(dir, inode,
				      0, &new_di_bh, parent_di_bh, handle,
				      inode_ac, di_blkno, suballoc_loc,
				      suballoc_bit);
	if (status < 0) {
		mlog_errno(status);
		goto leave;
	}

	di = (struct ocfs2_dinode *)new_di_bh->b_data;
	status = ocfs2_orphan_add(osb, handle, inode, new_di_bh, orphan_name,
				  &orphan_insert, orphan_dir);
	if (status < 0) {
		mlog_errno(status);
		goto leave;
	}

	
	status = ocfs2_open_lock(inode);
	if (status < 0)
		mlog_errno(status);

	insert_inode_hash(inode);
leave:
	if (status < 0 && did_quota_inode)
		dquot_free_inode(inode);
	if (handle)
		ocfs2_commit_trans(osb, handle);

	if (orphan_dir) {
		
		ocfs2_inode_unlock(orphan_dir, 1);
		mutex_unlock(&orphan_dir->i_mutex);
		iput(orphan_dir);
	}

	if ((status < 0) && inode) {
		clear_nlink(inode);
		iput(inode);
	}

	if (inode_ac)
		ocfs2_free_alloc_context(inode_ac);

	brelse(new_di_bh);

	if (!status)
		*new_inode = inode;

	ocfs2_free_dir_lookup_result(&orphan_insert);

	ocfs2_inode_unlock(dir, 1);
	brelse(parent_di_bh);
	return status;
}

int ocfs2_mv_orphaned_inode_to_new(struct inode *dir,
				   struct inode *inode,
				   struct dentry *dentry)
{
	int status = 0;
	struct buffer_head *parent_di_bh = NULL;
	handle_t *handle = NULL;
	struct ocfs2_super *osb = OCFS2_SB(dir->i_sb);
	struct ocfs2_dinode *dir_di, *di;
	struct inode *orphan_dir_inode = NULL;
	struct buffer_head *orphan_dir_bh = NULL;
	struct buffer_head *di_bh = NULL;
	struct ocfs2_dir_lookup_result lookup = { NULL, };

	trace_ocfs2_mv_orphaned_inode_to_new(dir, dentry,
				dentry->d_name.len, dentry->d_name.name,
				(unsigned long long)OCFS2_I(dir)->ip_blkno,
				(unsigned long long)OCFS2_I(inode)->ip_blkno);

	status = ocfs2_inode_lock(dir, &parent_di_bh, 1);
	if (status < 0) {
		if (status != -ENOENT)
			mlog_errno(status);
		return status;
	}

	dir_di = (struct ocfs2_dinode *) parent_di_bh->b_data;
	if (!dir_di->i_links_count) {
		
		status = -ENOENT;
		goto leave;
	}

	status = ocfs2_check_dir_for_entry(dir, dentry->d_name.name,
					   dentry->d_name.len);
	if (status)
		goto leave;

	
	status = ocfs2_prepare_dir_for_insert(osb, dir, parent_di_bh,
					      dentry->d_name.name,
					      dentry->d_name.len, &lookup);
	if (status < 0) {
		mlog_errno(status);
		goto leave;
	}

	orphan_dir_inode = ocfs2_get_system_file_inode(osb,
						       ORPHAN_DIR_SYSTEM_INODE,
						       osb->slot_num);
	if (!orphan_dir_inode) {
		status = -EEXIST;
		mlog_errno(status);
		goto leave;
	}

	mutex_lock(&orphan_dir_inode->i_mutex);

	status = ocfs2_inode_lock(orphan_dir_inode, &orphan_dir_bh, 1);
	if (status < 0) {
		mlog_errno(status);
		mutex_unlock(&orphan_dir_inode->i_mutex);
		iput(orphan_dir_inode);
		goto leave;
	}

	status = ocfs2_read_inode_block(inode, &di_bh);
	if (status < 0) {
		mlog_errno(status);
		goto orphan_unlock;
	}

	handle = ocfs2_start_trans(osb, ocfs2_rename_credits(osb->sb));
	if (IS_ERR(handle)) {
		status = PTR_ERR(handle);
		handle = NULL;
		mlog_errno(status);
		goto orphan_unlock;
	}

	status = ocfs2_journal_access_di(handle, INODE_CACHE(inode),
					 di_bh, OCFS2_JOURNAL_ACCESS_WRITE);
	if (status < 0) {
		mlog_errno(status);
		goto out_commit;
	}

	status = ocfs2_orphan_del(osb, handle, orphan_dir_inode, inode,
				  orphan_dir_bh);
	if (status < 0) {
		mlog_errno(status);
		goto out_commit;
	}

	di = (struct ocfs2_dinode *)di_bh->b_data;
	le32_add_cpu(&di->i_flags, -OCFS2_ORPHANED_FL);
	di->i_orphaned_slot = 0;
	set_nlink(inode, 1);
	ocfs2_set_links_count(di, inode->i_nlink);
	ocfs2_journal_dirty(handle, di_bh);

	status = ocfs2_add_entry(handle, dentry, inode,
				 OCFS2_I(inode)->ip_blkno, parent_di_bh,
				 &lookup);
	if (status < 0) {
		mlog_errno(status);
		goto out_commit;
	}

	status = ocfs2_dentry_attach_lock(dentry, inode,
					  OCFS2_I(dir)->ip_blkno);
	if (status) {
		mlog_errno(status);
		goto out_commit;
	}

	d_instantiate(dentry, inode);
	status = 0;
out_commit:
	ocfs2_commit_trans(osb, handle);
orphan_unlock:
	ocfs2_inode_unlock(orphan_dir_inode, 1);
	mutex_unlock(&orphan_dir_inode->i_mutex);
	iput(orphan_dir_inode);
leave:

	ocfs2_inode_unlock(dir, 1);

	brelse(di_bh);
	brelse(parent_di_bh);
	brelse(orphan_dir_bh);

	ocfs2_free_dir_lookup_result(&lookup);

	if (status)
		mlog_errno(status);

	return status;
}

const struct inode_operations ocfs2_dir_iops = {
	.create		= ocfs2_create,
	.lookup		= ocfs2_lookup,
	.link		= ocfs2_link,
	.unlink		= ocfs2_unlink,
	.rmdir		= ocfs2_unlink,
	.symlink	= ocfs2_symlink,
	.mkdir		= ocfs2_mkdir,
	.mknod		= ocfs2_mknod,
	.rename		= ocfs2_rename,
	.setattr	= ocfs2_setattr,
	.getattr	= ocfs2_getattr,
	.permission	= ocfs2_permission,
	.setxattr	= generic_setxattr,
	.getxattr	= generic_getxattr,
	.listxattr	= ocfs2_listxattr,
	.removexattr	= generic_removexattr,
	.fiemap         = ocfs2_fiemap,
	.get_acl	= ocfs2_iop_get_acl,
};
