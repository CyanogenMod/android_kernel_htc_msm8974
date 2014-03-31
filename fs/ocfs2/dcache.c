/* -*- mode: c; c-basic-offset: 8; -*-
 * vim: noexpandtab sw=8 ts=8 sts=0:
 *
 * dcache.c
 *
 * dentry cache handling code
 *
 * Copyright (C) 2002, 2004 Oracle.  All rights reserved.
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
#include <linux/namei.h>

#include <cluster/masklog.h>

#include "ocfs2.h"

#include "alloc.h"
#include "dcache.h"
#include "dlmglue.h"
#include "file.h"
#include "inode.h"
#include "super.h"
#include "ocfs2_trace.h"

void ocfs2_dentry_attach_gen(struct dentry *dentry)
{
	unsigned long gen =
		OCFS2_I(dentry->d_parent->d_inode)->ip_dir_lock_gen;
	BUG_ON(dentry->d_inode);
	dentry->d_fsdata = (void *)gen;
}


static int ocfs2_dentry_revalidate(struct dentry *dentry,
				   struct nameidata *nd)
{
	struct inode *inode;
	int ret = 0;    
	struct ocfs2_super *osb;

	if (nd && nd->flags & LOOKUP_RCU)
		return -ECHILD;

	inode = dentry->d_inode;
	osb = OCFS2_SB(dentry->d_sb);

	trace_ocfs2_dentry_revalidate(dentry, dentry->d_name.len,
				      dentry->d_name.name);

	if (inode == NULL) {
		unsigned long gen = (unsigned long) dentry->d_fsdata;
		unsigned long pgen =
			OCFS2_I(dentry->d_parent->d_inode)->ip_dir_lock_gen;

		trace_ocfs2_dentry_revalidate_negative(dentry->d_name.len,
						       dentry->d_name.name,
						       pgen, gen);
		if (gen != pgen)
			goto bail;
		goto valid;
	}

	BUG_ON(!osb);

	if (inode == osb->root_inode || is_bad_inode(inode))
		goto bail;

	spin_lock(&OCFS2_I(inode)->ip_lock);
	
	if (OCFS2_I(inode)->ip_flags & OCFS2_INODE_DELETED) {
		spin_unlock(&OCFS2_I(inode)->ip_lock);
		trace_ocfs2_dentry_revalidate_delete(
				(unsigned long long)OCFS2_I(inode)->ip_blkno);
		goto bail;
	}
	spin_unlock(&OCFS2_I(inode)->ip_lock);

	if (inode->i_nlink == 0) {
		trace_ocfs2_dentry_revalidate_orphaned(
			(unsigned long long)OCFS2_I(inode)->ip_blkno,
			S_ISDIR(inode->i_mode));
		goto bail;
	}

	if (!dentry->d_fsdata) {
		trace_ocfs2_dentry_revalidate_nofsdata(
				(unsigned long long)OCFS2_I(inode)->ip_blkno);
		goto bail;
	}

valid:
	ret = 1;

bail:
	trace_ocfs2_dentry_revalidate_ret(ret);
	return ret;
}

static int ocfs2_match_dentry(struct dentry *dentry,
			      u64 parent_blkno,
			      int skip_unhashed)
{
	struct inode *parent;

	if (!dentry->d_fsdata)
		return 0;

	if (!dentry->d_parent)
		return 0;

	if (skip_unhashed && d_unhashed(dentry))
		return 0;

	parent = dentry->d_parent->d_inode;
	
	if (!parent)
		return 0;

	
	if (OCFS2_I(parent)->ip_blkno != parent_blkno)
		return 0;

	return 1;
}

struct dentry *ocfs2_find_local_alias(struct inode *inode,
				      u64 parent_blkno,
				      int skip_unhashed)
{
	struct list_head *p;
	struct dentry *dentry = NULL;

	spin_lock(&inode->i_lock);
	list_for_each(p, &inode->i_dentry) {
		dentry = list_entry(p, struct dentry, d_alias);

		spin_lock(&dentry->d_lock);
		if (ocfs2_match_dentry(dentry, parent_blkno, skip_unhashed)) {
			trace_ocfs2_find_local_alias(dentry->d_name.len,
						     dentry->d_name.name);

			dget_dlock(dentry);
			spin_unlock(&dentry->d_lock);
			break;
		}
		spin_unlock(&dentry->d_lock);

		dentry = NULL;
	}

	spin_unlock(&inode->i_lock);

	return dentry;
}

DEFINE_SPINLOCK(dentry_attach_lock);

int ocfs2_dentry_attach_lock(struct dentry *dentry,
			     struct inode *inode,
			     u64 parent_blkno)
{
	int ret;
	struct dentry *alias;
	struct ocfs2_dentry_lock *dl = dentry->d_fsdata;

	trace_ocfs2_dentry_attach_lock(dentry->d_name.len, dentry->d_name.name,
				       (unsigned long long)parent_blkno, dl);

	if (!inode)
		return 0;

	if (!dentry->d_inode && dentry->d_fsdata) {
		dentry->d_fsdata = dl = NULL;
	}

	if (dl) {
		mlog_bug_on_msg(dl->dl_parent_blkno != parent_blkno,
				" \"%.*s\": old parent: %llu, new: %llu\n",
				dentry->d_name.len, dentry->d_name.name,
				(unsigned long long)parent_blkno,
				(unsigned long long)dl->dl_parent_blkno);
		return 0;
	}

	alias = ocfs2_find_local_alias(inode, parent_blkno, 0);
	if (alias) {
		dl = alias->d_fsdata;
		mlog_bug_on_msg(!dl, "parent %llu, ino %llu\n",
				(unsigned long long)parent_blkno,
				(unsigned long long)OCFS2_I(inode)->ip_blkno);

		mlog_bug_on_msg(dl->dl_parent_blkno != parent_blkno,
				" \"%.*s\": old parent: %llu, new: %llu\n",
				dentry->d_name.len, dentry->d_name.name,
				(unsigned long long)parent_blkno,
				(unsigned long long)dl->dl_parent_blkno);

		trace_ocfs2_dentry_attach_lock_found(dl->dl_lockres.l_name,
				(unsigned long long)parent_blkno,
				(unsigned long long)OCFS2_I(inode)->ip_blkno);

		goto out_attach;
	}

	dl = kmalloc(sizeof(*dl), GFP_NOFS);
	if (!dl) {
		ret = -ENOMEM;
		mlog_errno(ret);
		return ret;
	}

	dl->dl_count = 0;
	dl->dl_inode = igrab(inode);
	dl->dl_parent_blkno = parent_blkno;
	ocfs2_dentry_lock_res_init(dl, parent_blkno, inode);

out_attach:
	spin_lock(&dentry_attach_lock);
	dentry->d_fsdata = dl;
	dl->dl_count++;
	spin_unlock(&dentry_attach_lock);

	ret = ocfs2_dentry_lock(dentry, 0);
	if (!ret)
		ocfs2_dentry_unlock(dentry, 0);
	else
		mlog_errno(ret);

	if (ret < 0 && !alias) {
		ocfs2_lock_res_free(&dl->dl_lockres);
		BUG_ON(dl->dl_count != 1);
		spin_lock(&dentry_attach_lock);
		dentry->d_fsdata = NULL;
		spin_unlock(&dentry_attach_lock);
		kfree(dl);
		iput(inode);
	}

	dput(alias);

	return ret;
}

DEFINE_SPINLOCK(dentry_list_lock);

#define DL_INODE_DROP_COUNT 64

static void __ocfs2_drop_dl_inodes(struct ocfs2_super *osb, int drop_count)
{
	struct ocfs2_dentry_lock *dl;

	spin_lock(&dentry_list_lock);
	while (osb->dentry_lock_list && (drop_count < 0 || drop_count--)) {
		dl = osb->dentry_lock_list;
		osb->dentry_lock_list = dl->dl_next;
		spin_unlock(&dentry_list_lock);
		iput(dl->dl_inode);
		kfree(dl);
		spin_lock(&dentry_list_lock);
	}
	spin_unlock(&dentry_list_lock);
}

void ocfs2_drop_dl_inodes(struct work_struct *work)
{
	struct ocfs2_super *osb = container_of(work, struct ocfs2_super,
					       dentry_lock_work);

	__ocfs2_drop_dl_inodes(osb, DL_INODE_DROP_COUNT);
	spin_lock(&dentry_list_lock);
	if (osb->dentry_lock_list &&
	    !ocfs2_test_osb_flag(osb, OCFS2_OSB_DROP_DENTRY_LOCK_IMMED))
		queue_work(ocfs2_wq, &osb->dentry_lock_work);
	spin_unlock(&dentry_list_lock);
}

void ocfs2_drop_all_dl_inodes(struct ocfs2_super *osb)
{
	__ocfs2_drop_dl_inodes(osb, -1);
}

static void ocfs2_drop_dentry_lock(struct ocfs2_super *osb,
				   struct ocfs2_dentry_lock *dl)
{
	ocfs2_simple_drop_lockres(osb, &dl->dl_lockres);
	ocfs2_lock_res_free(&dl->dl_lockres);

	spin_lock(&dentry_list_lock);
	if (!osb->dentry_lock_list &&
	    !ocfs2_test_osb_flag(osb, OCFS2_OSB_DROP_DENTRY_LOCK_IMMED))
		queue_work(ocfs2_wq, &osb->dentry_lock_work);
	dl->dl_next = osb->dentry_lock_list;
	osb->dentry_lock_list = dl;
	spin_unlock(&dentry_list_lock);
}

void ocfs2_dentry_lock_put(struct ocfs2_super *osb,
			   struct ocfs2_dentry_lock *dl)
{
	int unlock;

	BUG_ON(dl->dl_count == 0);

	spin_lock(&dentry_attach_lock);
	dl->dl_count--;
	unlock = !dl->dl_count;
	spin_unlock(&dentry_attach_lock);

	if (unlock)
		ocfs2_drop_dentry_lock(osb, dl);
}

static void ocfs2_dentry_iput(struct dentry *dentry, struct inode *inode)
{
	struct ocfs2_dentry_lock *dl = dentry->d_fsdata;

	if (!dl) {
		if (!(dentry->d_flags & DCACHE_DISCONNECTED) &&
		    !d_unhashed(dentry)) {
			unsigned long long ino = 0ULL;
			if (inode)
				ino = (unsigned long long)OCFS2_I(inode)->ip_blkno;
			mlog(ML_ERROR, "Dentry is missing cluster lock. "
			     "inode: %llu, d_flags: 0x%x, d_name: %.*s\n",
			     ino, dentry->d_flags, dentry->d_name.len,
			     dentry->d_name.name);
		}

		goto out;
	}

	mlog_bug_on_msg(dl->dl_count == 0, "dentry: %.*s, count: %u\n",
			dentry->d_name.len, dentry->d_name.name,
			dl->dl_count);

	ocfs2_dentry_lock_put(OCFS2_SB(dentry->d_sb), dl);

out:
	iput(inode);
}

void ocfs2_dentry_move(struct dentry *dentry, struct dentry *target,
		       struct inode *old_dir, struct inode *new_dir)
{
	int ret;
	struct ocfs2_super *osb = OCFS2_SB(old_dir->i_sb);
	struct inode *inode = dentry->d_inode;

	if (old_dir == new_dir)
		goto out_move;

	ocfs2_dentry_lock_put(osb, dentry->d_fsdata);

	dentry->d_fsdata = NULL;
	ret = ocfs2_dentry_attach_lock(dentry, inode, OCFS2_I(new_dir)->ip_blkno);
	if (ret)
		mlog_errno(ret);

out_move:
	d_move(dentry, target);
}

const struct dentry_operations ocfs2_dentry_ops = {
	.d_revalidate		= ocfs2_dentry_revalidate,
	.d_iput			= ocfs2_dentry_iput,
};
