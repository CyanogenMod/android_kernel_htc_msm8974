/*
 * AppArmor security module
 *
 * This file contains AppArmor mediation of files
 *
 * Copyright (C) 1998-2008 Novell/SUSE
 * Copyright 2009-2010 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 */

#include "include/apparmor.h"
#include "include/audit.h"
#include "include/file.h"
#include "include/match.h"
#include "include/path.h"
#include "include/policy.h"

struct file_perms nullperms;


static void audit_file_mask(struct audit_buffer *ab, u32 mask)
{
	char str[10];

	char *m = str;

	if (mask & AA_EXEC_MMAP)
		*m++ = 'm';
	if (mask & (MAY_READ | AA_MAY_META_READ))
		*m++ = 'r';
	if (mask & (MAY_WRITE | AA_MAY_META_WRITE | AA_MAY_CHMOD |
		    AA_MAY_CHOWN))
		*m++ = 'w';
	else if (mask & MAY_APPEND)
		*m++ = 'a';
	if (mask & AA_MAY_CREATE)
		*m++ = 'c';
	if (mask & AA_MAY_DELETE)
		*m++ = 'd';
	if (mask & AA_MAY_LINK)
		*m++ = 'l';
	if (mask & AA_MAY_LOCK)
		*m++ = 'k';
	if (mask & MAY_EXEC)
		*m++ = 'x';
	*m = '\0';

	audit_log_string(ab, str);
}

static void file_audit_cb(struct audit_buffer *ab, void *va)
{
	struct common_audit_data *sa = va;
	uid_t fsuid = current_fsuid();

	if (sa->aad->fs.request & AA_AUDIT_FILE_MASK) {
		audit_log_format(ab, " requested_mask=");
		audit_file_mask(ab, sa->aad->fs.request);
	}
	if (sa->aad->fs.denied & AA_AUDIT_FILE_MASK) {
		audit_log_format(ab, " denied_mask=");
		audit_file_mask(ab, sa->aad->fs.denied);
	}
	if (sa->aad->fs.request & AA_AUDIT_FILE_MASK) {
		audit_log_format(ab, " fsuid=%d", fsuid);
		audit_log_format(ab, " ouid=%d", sa->aad->fs.ouid);
	}

	if (sa->aad->fs.target) {
		audit_log_format(ab, " target=");
		audit_log_untrustedstring(ab, sa->aad->fs.target);
	}
}

int aa_audit_file(struct aa_profile *profile, struct file_perms *perms,
		  gfp_t gfp, int op, u32 request, const char *name,
		  const char *target, uid_t ouid, const char *info, int error)
{
	int type = AUDIT_APPARMOR_AUTO;
	struct common_audit_data sa;
	struct apparmor_audit_data aad = {0,};
	COMMON_AUDIT_DATA_INIT(&sa, NONE);
	sa.aad = &aad;
	aad.op = op,
	aad.fs.request = request;
	aad.name = name;
	aad.fs.target = target;
	aad.fs.ouid = ouid;
	aad.info = info;
	aad.error = error;

	if (likely(!sa.aad->error)) {
		u32 mask = perms->audit;

		if (unlikely(AUDIT_MODE(profile) == AUDIT_ALL))
			mask = 0xffff;

		
		sa.aad->fs.request &= mask;

		if (likely(!sa.aad->fs.request))
			return 0;
		type = AUDIT_APPARMOR_AUDIT;
	} else {
		
		sa.aad->fs.request = sa.aad->fs.request & ~perms->allow;

		if (sa.aad->fs.request & perms->kill)
			type = AUDIT_APPARMOR_KILL;

		
		if ((sa.aad->fs.request & perms->quiet) &&
		    AUDIT_MODE(profile) != AUDIT_NOQUIET &&
		    AUDIT_MODE(profile) != AUDIT_ALL)
			sa.aad->fs.request &= ~perms->quiet;

		if (!sa.aad->fs.request)
			return COMPLAIN_MODE(profile) ? 0 : sa.aad->error;
	}

	sa.aad->fs.denied = sa.aad->fs.request & ~perms->allow;
	return aa_audit(type, profile, gfp, &sa, file_audit_cb);
}

static u32 map_old_perms(u32 old)
{
	u32 new = old & 0xf;
	if (old & MAY_READ)
		new |= AA_MAY_META_READ;
	if (old & MAY_WRITE)
		new |= AA_MAY_META_WRITE | AA_MAY_CREATE | AA_MAY_DELETE |
			AA_MAY_CHMOD | AA_MAY_CHOWN;
	if (old & 0x10)
		new |= AA_MAY_LINK;
	if (old & 0x20)
		new |= AA_MAY_LOCK | AA_LINK_SUBSET;
	if (old & 0x40)	
		new |= AA_EXEC_MMAP;

	return new;
}

static struct file_perms compute_perms(struct aa_dfa *dfa, unsigned int state,
				       struct path_cond *cond)
{
	struct file_perms perms;

	perms.kill = 0;

	if (current_fsuid() == cond->uid) {
		perms.allow = map_old_perms(dfa_user_allow(dfa, state));
		perms.audit = map_old_perms(dfa_user_audit(dfa, state));
		perms.quiet = map_old_perms(dfa_user_quiet(dfa, state));
		perms.xindex = dfa_user_xindex(dfa, state);
	} else {
		perms.allow = map_old_perms(dfa_other_allow(dfa, state));
		perms.audit = map_old_perms(dfa_other_audit(dfa, state));
		perms.quiet = map_old_perms(dfa_other_quiet(dfa, state));
		perms.xindex = dfa_other_xindex(dfa, state);
	}
	perms.allow |= AA_MAY_META_READ;

	
	if (ACCEPT_TABLE(dfa)[state] & 0x80000000)
		perms.allow |= AA_MAY_CHANGE_PROFILE;
	if (ACCEPT_TABLE(dfa)[state] & 0x40000000)
		perms.allow |= AA_MAY_ONEXEC;

	return perms;
}

unsigned int aa_str_perms(struct aa_dfa *dfa, unsigned int start,
			  const char *name, struct path_cond *cond,
			  struct file_perms *perms)
{
	unsigned int state;
	if (!dfa) {
		*perms = nullperms;
		return DFA_NOMATCH;
	}

	state = aa_dfa_match(dfa, start, name);
	*perms = compute_perms(dfa, state, cond);

	return state;
}

static inline bool is_deleted(struct dentry *dentry)
{
	if (d_unlinked(dentry) && dentry->d_inode->i_nlink == 0)
		return 1;
	return 0;
}

int aa_path_perm(int op, struct aa_profile *profile, struct path *path,
		 int flags, u32 request, struct path_cond *cond)
{
	char *buffer = NULL;
	struct file_perms perms = {};
	const char *name, *info = NULL;
	int error;

	flags |= profile->path_flags | (S_ISDIR(cond->mode) ? PATH_IS_DIR : 0);
	error = aa_path_name(path, flags, &buffer, &name, &info);
	if (error) {
		if (error == -ENOENT && is_deleted(path->dentry)) {
			error = 0;
			info = NULL;
			perms.allow = request;
		}
	} else {
		aa_str_perms(profile->file.dfa, profile->file.start, name, cond,
			     &perms);
		if (request & ~perms.allow)
			error = -EACCES;
	}
	error = aa_audit_file(profile, &perms, GFP_KERNEL, op, request, name,
			      NULL, cond->uid, info, error);
	kfree(buffer);

	return error;
}

static inline bool xindex_is_subset(u32 link, u32 target)
{
	if (((link & ~AA_X_UNSAFE) != (target & ~AA_X_UNSAFE)) ||
	    ((link & AA_X_UNSAFE) && !(target & AA_X_UNSAFE)))
		return 0;

	return 1;
}

int aa_path_link(struct aa_profile *profile, struct dentry *old_dentry,
		 struct path *new_dir, struct dentry *new_dentry)
{
	struct path link = { new_dir->mnt, new_dentry };
	struct path target = { new_dir->mnt, old_dentry };
	struct path_cond cond = {
		old_dentry->d_inode->i_uid,
		old_dentry->d_inode->i_mode
	};
	char *buffer = NULL, *buffer2 = NULL;
	const char *lname, *tname = NULL, *info = NULL;
	struct file_perms lperms, perms;
	u32 request = AA_MAY_LINK;
	unsigned int state;
	int error;

	lperms = nullperms;

	
	error = aa_path_name(&link, profile->path_flags, &buffer, &lname,
			     &info);
	if (error)
		goto audit;

	
	error = aa_path_name(&target, profile->path_flags, &buffer2, &tname,
			     &info);
	if (error)
		goto audit;

	error = -EACCES;
	
	state = aa_str_perms(profile->file.dfa, profile->file.start, lname,
			     &cond, &lperms);

	if (!(lperms.allow & AA_MAY_LINK))
		goto audit;

	
	state = aa_dfa_null_transition(profile->file.dfa, state);
	aa_str_perms(profile->file.dfa, state, tname, &cond, &perms);

	lperms.audit = perms.audit;
	lperms.quiet = perms.quiet;
	lperms.kill = perms.kill;

	if (!(perms.allow & AA_MAY_LINK)) {
		info = "target restricted";
		goto audit;
	}

	
	if (!(perms.allow & AA_LINK_SUBSET))
		goto done_tests;

	aa_str_perms(profile->file.dfa, profile->file.start, tname, &cond,
		     &perms);

	
	request = lperms.allow & ~AA_MAY_LINK;
	lperms.allow &= perms.allow | AA_MAY_LINK;

	request |= AA_AUDIT_FILE_MASK & (lperms.allow & ~perms.allow);
	if (request & ~lperms.allow) {
		goto audit;
	} else if ((lperms.allow & MAY_EXEC) &&
		   !xindex_is_subset(lperms.xindex, perms.xindex)) {
		lperms.allow &= ~MAY_EXEC;
		request |= MAY_EXEC;
		info = "link not subset of target";
		goto audit;
	}

done_tests:
	error = 0;

audit:
	error = aa_audit_file(profile, &lperms, GFP_KERNEL, OP_LINK, request,
			      lname, tname, cond.uid, info, error);
	kfree(buffer);
	kfree(buffer2);

	return error;
}

int aa_file_perm(int op, struct aa_profile *profile, struct file *file,
		 u32 request)
{
	struct path_cond cond = {
		.uid = file->f_path.dentry->d_inode->i_uid,
		.mode = file->f_path.dentry->d_inode->i_mode
	};

	return aa_path_perm(op, profile, &file->f_path, PATH_DELEGATE_DELETED,
			    request, &cond);
}
