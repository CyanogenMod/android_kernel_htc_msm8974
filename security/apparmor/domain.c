/*
 * AppArmor security module
 *
 * This file contains AppArmor policy attachment and domain transitions
 *
 * Copyright (C) 2002-2008 Novell/SUSE
 * Copyright 2009-2010 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 */

#include <linux/errno.h>
#include <linux/fdtable.h>
#include <linux/file.h>
#include <linux/mount.h>
#include <linux/syscalls.h>
#include <linux/tracehook.h>
#include <linux/personality.h>

#include "include/audit.h"
#include "include/apparmorfs.h"
#include "include/context.h"
#include "include/domain.h"
#include "include/file.h"
#include "include/ipc.h"
#include "include/match.h"
#include "include/path.h"
#include "include/policy.h"

void aa_free_domain_entries(struct aa_domain *domain)
{
	int i;
	if (domain) {
		if (!domain->table)
			return;

		for (i = 0; i < domain->size; i++)
			kzfree(domain->table[i]);
		kzfree(domain->table);
		domain->table = NULL;
	}
}

static int may_change_ptraced_domain(struct task_struct *task,
				     struct aa_profile *to_profile)
{
	struct task_struct *tracer;
	const struct cred *cred = NULL;
	struct aa_profile *tracerp = NULL;
	int error = 0;

	rcu_read_lock();
	tracer = ptrace_parent(task);
	if (tracer) {
		
		cred = get_task_cred(tracer);
		tracerp = aa_cred_profile(cred);
	}

	
	if (!tracer || unconfined(tracerp))
		goto out;

	error = aa_may_ptrace(tracer, tracerp, to_profile, PTRACE_MODE_ATTACH);

out:
	rcu_read_unlock();
	if (cred)
		put_cred(cred);

	return error;
}

static struct file_perms change_profile_perms(struct aa_profile *profile,
					      struct aa_namespace *ns,
					      const char *name, u32 request,
					      unsigned int start)
{
	struct file_perms perms;
	struct path_cond cond = { };
	unsigned int state;

	if (unconfined(profile)) {
		perms.allow = AA_MAY_CHANGE_PROFILE | AA_MAY_ONEXEC;
		perms.audit = perms.quiet = perms.kill = 0;
		return perms;
	} else if (!profile->file.dfa) {
		return nullperms;
	} else if ((ns == profile->ns)) {
		
		aa_str_perms(profile->file.dfa, start, name, &cond, &perms);
		if (COMBINED_PERM_MASK(perms) & request)
			return perms;
	}

	
	state = aa_dfa_match(profile->file.dfa, start, ns->base.name);
	state = aa_dfa_match_len(profile->file.dfa, state, ":", 1);
	aa_str_perms(profile->file.dfa, state, name, &cond, &perms);

	return perms;
}

static struct aa_profile *__attach_match(const char *name,
					 struct list_head *head)
{
	int len = 0;
	struct aa_profile *profile, *candidate = NULL;

	list_for_each_entry(profile, head, base.list) {
		if (profile->flags & PFLAG_NULL)
			continue;
		if (profile->xmatch && profile->xmatch_len > len) {
			unsigned int state = aa_dfa_match(profile->xmatch,
							  DFA_START, name);
			u32 perm = dfa_user_allow(profile->xmatch, state);
			
			if (perm & MAY_EXEC) {
				candidate = profile;
				len = profile->xmatch_len;
			}
		} else if (!strcmp(profile->base.name, name))
			
			return profile;
	}

	return candidate;
}

static struct aa_profile *find_attach(struct aa_namespace *ns,
				      struct list_head *list, const char *name)
{
	struct aa_profile *profile;

	read_lock(&ns->lock);
	profile = aa_get_profile(__attach_match(name, list));
	read_unlock(&ns->lock);

	return profile;
}

static const char *separate_fqname(const char *fqname, const char **ns_name)
{
	const char *name;

	if (fqname[0] == ':') {
		*ns_name = fqname + 1;		
		name = *ns_name + strlen(*ns_name) + 1;
		if (!*name)
			name = NULL;
	} else {
		*ns_name = NULL;
		name = fqname;
	}

	return name;
}

static const char *next_name(int xtype, const char *name)
{
	return NULL;
}

static struct aa_profile *x_table_lookup(struct aa_profile *profile, u32 xindex)
{
	struct aa_profile *new_profile = NULL;
	struct aa_namespace *ns = profile->ns;
	u32 xtype = xindex & AA_X_TYPE_MASK;
	int index = xindex & AA_X_INDEX_MASK;
	const char *name;

	
	for (name = profile->file.trans.table[index]; !new_profile && name;
	     name = next_name(xtype, name)) {
		struct aa_namespace *new_ns;
		const char *xname = NULL;

		new_ns = NULL;
		if (xindex & AA_X_CHILD) {
			
			new_profile = aa_find_child(profile, name);
			continue;
		} else if (*name == ':') {
			
			const char *ns_name;
			xname = name = separate_fqname(name, &ns_name);
			if (!xname)
				
				xname = profile->base.hname;
			if (*ns_name == '@') {
				
				;
			}
			
			new_ns = aa_find_namespace(ns, ns_name);
			if (!new_ns)
				continue;
		} else if (*name == '@') {
			
			continue;
		} else {
			
			xname = name;
		}

		
		new_profile = aa_lookup_profile(new_ns ? new_ns : ns, xname);
		aa_put_namespace(new_ns);
	}

	
	return new_profile;
}

static struct aa_profile *x_to_profile(struct aa_profile *profile,
				       const char *name, u32 xindex)
{
	struct aa_profile *new_profile = NULL;
	struct aa_namespace *ns = profile->ns;
	u32 xtype = xindex & AA_X_TYPE_MASK;

	switch (xtype) {
	case AA_X_NONE:
		
		return NULL;
	case AA_X_NAME:
		if (xindex & AA_X_CHILD)
			
			new_profile = find_attach(ns, &profile->base.profiles,
						  name);
		else
			
			new_profile = find_attach(ns, &ns->base.profiles,
						  name);
		break;
	case AA_X_TABLE:
		
		new_profile = x_table_lookup(profile, xindex);
		break;
	}

	
	return new_profile;
}

int apparmor_bprm_set_creds(struct linux_binprm *bprm)
{
	struct aa_task_cxt *cxt;
	struct aa_profile *profile, *new_profile = NULL;
	struct aa_namespace *ns;
	char *buffer = NULL;
	unsigned int state;
	struct file_perms perms = {};
	struct path_cond cond = {
		bprm->file->f_path.dentry->d_inode->i_uid,
		bprm->file->f_path.dentry->d_inode->i_mode
	};
	const char *name = NULL, *target = NULL, *info = NULL;
	int error = cap_bprm_set_creds(bprm);
	if (error)
		return error;

	if (bprm->cred_prepared)
		return 0;

	cxt = bprm->cred->security;
	BUG_ON(!cxt);

	profile = aa_get_profile(aa_newest_version(cxt->profile));
	ns = profile->ns;
	state = profile->file.start;

	
	error = aa_path_name(&bprm->file->f_path, profile->path_flags, &buffer,
			     &name, &info);
	if (error) {
		if (profile->flags &
		    (PFLAG_IX_ON_NAME_ERROR | PFLAG_UNCONFINED))
			error = 0;
		name = bprm->filename;
		goto audit;
	}

	if (unconfined(profile)) {
		
		if (cxt->onexec)
			
			new_profile = aa_get_profile(cxt->onexec);
		else
			new_profile = find_attach(ns, &ns->base.profiles, name);
		if (!new_profile)
			goto cleanup;
		goto apply;
	}

	
	state = aa_str_perms(profile->file.dfa, state, name, &cond, &perms);
	if (cxt->onexec) {
		struct file_perms cp;
		info = "change_profile onexec";
		if (!(perms.allow & AA_MAY_ONEXEC))
			goto audit;

		state = aa_dfa_null_transition(profile->file.dfa, state);
		cp = change_profile_perms(profile, cxt->onexec->ns,
					  cxt->onexec->base.name,
					  AA_MAY_ONEXEC, state);

		if (!(cp.allow & AA_MAY_ONEXEC))
			goto audit;
		new_profile = aa_get_profile(aa_newest_version(cxt->onexec));
		goto apply;
	}

	if (perms.allow & MAY_EXEC) {
		
		new_profile = x_to_profile(profile, name, perms.xindex);
		if (!new_profile) {
			if (perms.xindex & AA_X_INHERIT) {
				info = "ix fallback";
				new_profile = aa_get_profile(profile);
				goto x_clear;
			} else if (perms.xindex & AA_X_UNCONFINED) {
				new_profile = aa_get_profile(ns->unconfined);
				info = "ux fallback";
			} else {
				error = -ENOENT;
				info = "profile not found";
			}
		}
	} else if (COMPLAIN_MODE(profile)) {
		
		new_profile = aa_new_null_profile(profile, 0);
		if (!new_profile) {
			error = -ENOMEM;
			info = "could not create null profile";
		} else {
			error = -EACCES;
			target = new_profile->base.hname;
		}
		perms.xindex |= AA_X_UNSAFE;
	} else
		
		error = -EACCES;

	if (!new_profile)
		goto audit;

	if (bprm->unsafe & LSM_UNSAFE_SHARE) {
		
		;
	}

	if (bprm->unsafe & (LSM_UNSAFE_PTRACE | LSM_UNSAFE_PTRACE_CAP)) {
		error = may_change_ptraced_domain(current, new_profile);
		if (error) {
			aa_put_profile(new_profile);
			goto audit;
		}
	}

	if (!(perms.xindex & AA_X_UNSAFE)) {
		AA_DEBUG("scrubbing environment variables for %s profile=%s\n",
			 name, new_profile->base.hname);
		bprm->unsafe |= AA_SECURE_X_NEEDED;
	}
apply:
	target = new_profile->base.hname;
	
	bprm->per_clear |= PER_CLEAR_ON_SETID;

x_clear:
	aa_put_profile(cxt->profile);
	
	cxt->profile = new_profile;

	
	aa_put_profile(cxt->previous);
	aa_put_profile(cxt->onexec);
	cxt->previous = NULL;
	cxt->onexec = NULL;
	cxt->token = 0;

audit:
	error = aa_audit_file(profile, &perms, GFP_KERNEL, OP_EXEC, MAY_EXEC,
			      name, target, cond.uid, info, error);

cleanup:
	aa_put_profile(profile);
	kfree(buffer);

	return error;
}

int apparmor_bprm_secureexec(struct linux_binprm *bprm)
{
	int ret = cap_bprm_secureexec(bprm);

	if (!ret && (bprm->unsafe & AA_SECURE_X_NEEDED))
		ret = 1;

	return ret;
}

void apparmor_bprm_committing_creds(struct linux_binprm *bprm)
{
	struct aa_profile *profile = __aa_current_profile();
	struct aa_task_cxt *new_cxt = bprm->cred->security;

	
	if ((new_cxt->profile == profile) ||
	    (unconfined(new_cxt->profile)))
		return;

	current->pdeath_signal = 0;

	
	__aa_transition_rlimits(profile, new_cxt->profile);
}

void apparmor_bprm_committed_creds(struct linux_binprm *bprm)
{
	
	return;
}


static char *new_compound_name(const char *n1, const char *n2)
{
	char *name = kmalloc(strlen(n1) + strlen(n2) + 3, GFP_KERNEL);
	if (name)
		sprintf(name, "%s//%s", n1, n2);
	return name;
}

int aa_change_hat(const char *hats[], int count, u64 token, bool permtest)
{
	const struct cred *cred;
	struct aa_task_cxt *cxt;
	struct aa_profile *profile, *previous_profile, *hat = NULL;
	char *name = NULL;
	int i;
	struct file_perms perms = {};
	const char *target = NULL, *info = NULL;
	int error = 0;

	
	cred = get_current_cred();
	cxt = cred->security;
	profile = aa_cred_profile(cred);
	previous_profile = cxt->previous;

	if (unconfined(profile)) {
		info = "unconfined";
		error = -EPERM;
		goto audit;
	}

	if (count) {
		
		struct aa_profile *root;
		root = PROFILE_IS_HAT(profile) ? profile->parent : profile;

		
		for (i = 0; i < count && !hat; i++)
			
			hat = aa_find_child(root, hats[i]);
		if (!hat) {
			if (!COMPLAIN_MODE(root) || permtest) {
				if (list_empty(&root->base.profiles))
					error = -ECHILD;
				else
					error = -ENOENT;
				goto out;
			}


			
			name = new_compound_name(root->base.hname, hats[0]);
			target = name;
			
			hat = aa_new_null_profile(profile, 1);
			if (!hat) {
				info = "failed null profile create";
				error = -ENOMEM;
				goto audit;
			}
		} else {
			target = hat->base.hname;
			if (!PROFILE_IS_HAT(hat)) {
				info = "target not hat";
				error = -EPERM;
				goto audit;
			}
		}

		error = may_change_ptraced_domain(current, hat);
		if (error) {
			info = "ptraced";
			error = -EPERM;
			goto audit;
		}

		if (!permtest) {
			error = aa_set_current_hat(hat, token);
			if (error == -EACCES)
				
				perms.kill = AA_MAY_CHANGEHAT;
			else if (name && !error)
				
				error = -ENOENT;
		}
	} else if (previous_profile) {
		target = previous_profile->base.hname;
		error = aa_restore_previous_profile(token);
		perms.kill = AA_MAY_CHANGEHAT;
	} else
		
		goto out;

audit:
	if (!permtest)
		error = aa_audit_file(profile, &perms, GFP_KERNEL,
				      OP_CHANGE_HAT, AA_MAY_CHANGEHAT, NULL,
				      target, 0, info, error);

out:
	aa_put_profile(hat);
	kfree(name);
	put_cred(cred);

	return error;
}

int aa_change_profile(const char *ns_name, const char *hname, bool onexec,
		      bool permtest)
{
	const struct cred *cred;
	struct aa_task_cxt *cxt;
	struct aa_profile *profile, *target = NULL;
	struct aa_namespace *ns = NULL;
	struct file_perms perms = {};
	const char *name = NULL, *info = NULL;
	int op, error = 0;
	u32 request;

	if (!hname && !ns_name)
		return -EINVAL;

	if (onexec) {
		request = AA_MAY_ONEXEC;
		op = OP_CHANGE_ONEXEC;
	} else {
		request = AA_MAY_CHANGE_PROFILE;
		op = OP_CHANGE_PROFILE;
	}

	cred = get_current_cred();
	cxt = cred->security;
	profile = aa_cred_profile(cred);

	if (ns_name) {
		
		ns = aa_find_namespace(profile->ns, ns_name);
		if (!ns) {
			
			name = ns_name;
			info = "namespace not found";
			error = -ENOENT;
			goto audit;
		}
	} else
		
		ns = aa_get_namespace(profile->ns);

	
	if (!hname) {
		if (unconfined(profile))
			hname = ns->unconfined->base.hname;
		else
			hname = profile->base.hname;
	}

	perms = change_profile_perms(profile, ns, hname, request,
				     profile->file.start);
	if (!(perms.allow & request)) {
		error = -EACCES;
		goto audit;
	}

	
	target = aa_lookup_profile(ns, hname);
	if (!target) {
		info = "profile not found";
		error = -ENOENT;
		if (permtest || !COMPLAIN_MODE(profile))
			goto audit;
		
		target = aa_new_null_profile(profile, 0);
		if (!target) {
			info = "failed null profile create";
			error = -ENOMEM;
			goto audit;
		}
	}

	
	error = may_change_ptraced_domain(current, target);
	if (error) {
		info = "ptrace prevents transition";
		goto audit;
	}

	if (permtest)
		goto audit;

	if (onexec)
		error = aa_set_current_onexec(target);
	else
		error = aa_replace_current_profile(target);

audit:
	if (!permtest)
		error = aa_audit_file(profile, &perms, GFP_KERNEL, op, request,
				      name, hname, 0, info, error);

	aa_put_namespace(ns);
	aa_put_profile(target);
	put_cred(cred);

	return error;
}
