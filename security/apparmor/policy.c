/*
 * AppArmor security module
 *
 * This file contains AppArmor policy manipulation functions
 *
 * Copyright (C) 1998-2008 Novell/SUSE
 * Copyright 2009-2010 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 *
 * AppArmor policy is based around profiles, which contain the rules a
 * task is confined by.  Every task in the system has a profile attached
 * to it determined either by matching "unconfined" tasks against the
 * visible set of profiles or by following a profiles attachment rules.
 *
 * Each profile exists in a profile namespace which is a container of
 * visible profiles.  Each namespace contains a special "unconfined" profile,
 * which doesn't enforce any confinement on a task beyond DAC.
 *
 * Namespace and profile names can be written together in either
 * of two syntaxes.
 *	:namespace:profile - used by kernel interfaces for easy detection
 *	namespace://profile - used by policy
 *
 * Profile names can not start with : or @ or ^ and may not contain \0
 *
 * Reserved profile names
 *	unconfined - special automatically generated unconfined profile
 *	inherit - special name to indicate profile inheritance
 *	null-XXXX-YYYY - special automatically generated learning profiles
 *
 * Namespace names may not start with / or @ and may not contain \0 or :
 * Reserved namespace names
 *	user-XXXX - user defined profiles
 *
 * a // in a profile or namespace name indicates a hierarchical name with the
 * name before the // being the parent and the name after the child.
 *
 * Profile and namespace hierarchies serve two different but similar purposes.
 * The namespace contains the set of visible profiles that are considered
 * for attachment.  The hierarchy of namespaces allows for virtualizing
 * the namespace so that for example a chroot can have its own set of profiles
 * which may define some local user namespaces.
 * The profile hierarchy severs two distinct purposes,
 * -  it allows for sub profiles or hats, which allows an application to run
 *    subprograms under its own profile with different restriction than it
 *    self, and not have it use the system profile.
 *    eg. if a mail program starts an editor, the policy might make the
 *        restrictions tighter on the editor tighter than the mail program,
 *        and definitely different than general editor restrictions
 * - it allows for binary hierarchy of profiles, so that execution history
 *   is preserved.  This feature isn't exploited by AppArmor reference policy
 *   but is allowed.  NOTE: this is currently suboptimal because profile
 *   aliasing is not currently implemented so that a profile for each
 *   level must be defined.
 *   eg. /bin/bash///bin/ls as a name would indicate /bin/ls was started
 *       from /bin/bash
 *
 *   A profile or namespace name that can contain one or more // separators
 *   is referred to as an hname (hierarchical).
 *   eg.  /bin/bash//bin/ls
 *
 *   An fqname is a name that may contain both namespace and profile hnames.
 *   eg. :ns:/bin/bash//bin/ls
 *
 * NOTES:
 *   - locking of profile lists is currently fairly coarse.  All profile
 *     lists within a namespace use the namespace lock.
 * FIXME: move profile lists to using rcu_lists
 */

#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>

#include "include/apparmor.h"
#include "include/capability.h"
#include "include/context.h"
#include "include/file.h"
#include "include/ipc.h"
#include "include/match.h"
#include "include/path.h"
#include "include/policy.h"
#include "include/policy_unpack.h"
#include "include/resource.h"
#include "include/sid.h"


struct aa_namespace *root_ns;

const char *const profile_mode_names[] = {
	"enforce",
	"complain",
	"kill",
};

static const char *hname_tail(const char *hname)
{
	char *split;
	hname = strim((char *)hname);
	for (split = strstr(hname, "//"); split; split = strstr(hname, "//"))
		hname = split + 2;

	return hname;
}

static bool policy_init(struct aa_policy *policy, const char *prefix,
			const char *name)
{
	
	if (prefix) {
		policy->hname = kmalloc(strlen(prefix) + strlen(name) + 3,
					GFP_KERNEL);
		if (policy->hname)
			sprintf(policy->hname, "%s//%s", prefix, name);
	} else
		policy->hname = kstrdup(name, GFP_KERNEL);
	if (!policy->hname)
		return 0;
	
	policy->name = (char *)hname_tail(policy->hname);
	INIT_LIST_HEAD(&policy->list);
	INIT_LIST_HEAD(&policy->profiles);
	kref_init(&policy->count);

	return 1;
}

static void policy_destroy(struct aa_policy *policy)
{
	
	if (!list_empty(&policy->profiles)) {
		AA_ERROR("%s: internal error, "
			 "policy '%s' still contains profiles\n",
			 __func__, policy->name);
		BUG();
	}
	if (!list_empty(&policy->list)) {
		AA_ERROR("%s: internal error, policy '%s' still on list\n",
			 __func__, policy->name);
		BUG();
	}

	
	kzfree(policy->hname);
}

static struct aa_policy *__policy_find(struct list_head *head, const char *name)
{
	struct aa_policy *policy;

	list_for_each_entry(policy, head, list) {
		if (!strcmp(policy->name, name))
			return policy;
	}
	return NULL;
}

static struct aa_policy *__policy_strn_find(struct list_head *head,
					    const char *str, int len)
{
	struct aa_policy *policy;

	list_for_each_entry(policy, head, list) {
		if (aa_strneq(policy->name, str, len))
			return policy;
	}

	return NULL;
}


static const char *hidden_ns_name = "---";
bool aa_ns_visible(struct aa_namespace *curr, struct aa_namespace *view)
{
	if (curr == view)
		return true;

	for ( ; view; view = view->parent) {
		if (view->parent == curr)
			return true;
	}
	return false;
}

const char *aa_ns_name(struct aa_namespace *curr, struct aa_namespace *view)
{
	
	if (curr == view)
		return "";

	if (aa_ns_visible(curr, view)) {
		return view->base.hname + strlen(curr->base.hname) + 2;
	} else
		return hidden_ns_name;
}

static struct aa_namespace *alloc_namespace(const char *prefix,
					    const char *name)
{
	struct aa_namespace *ns;

	ns = kzalloc(sizeof(*ns), GFP_KERNEL);
	AA_DEBUG("%s(%p)\n", __func__, ns);
	if (!ns)
		return NULL;
	if (!policy_init(&ns->base, prefix, name))
		goto fail_ns;

	INIT_LIST_HEAD(&ns->sub_ns);
	rwlock_init(&ns->lock);

	
	ns->unconfined = aa_alloc_profile("unconfined");
	if (!ns->unconfined)
		goto fail_unconfined;

	ns->unconfined->sid = aa_alloc_sid();
	ns->unconfined->flags = PFLAG_UNCONFINED | PFLAG_IX_ON_NAME_ERROR |
	    PFLAG_IMMUTABLE;

	ns->unconfined->ns = aa_get_namespace(ns);

	return ns;

fail_unconfined:
	kzfree(ns->base.hname);
fail_ns:
	kzfree(ns);
	return NULL;
}

static void free_namespace(struct aa_namespace *ns)
{
	if (!ns)
		return;

	policy_destroy(&ns->base);
	aa_put_namespace(ns->parent);

	if (ns->unconfined && ns->unconfined->ns == ns)
		ns->unconfined->ns = NULL;

	aa_put_profile(ns->unconfined);
	kzfree(ns);
}

void aa_free_namespace_kref(struct kref *kref)
{
	free_namespace(container_of(kref, struct aa_namespace, base.count));
}

static struct aa_namespace *__aa_find_namespace(struct list_head *head,
						const char *name)
{
	return (struct aa_namespace *)__policy_find(head, name);
}

struct aa_namespace *aa_find_namespace(struct aa_namespace *root,
				       const char *name)
{
	struct aa_namespace *ns = NULL;

	read_lock(&root->lock);
	ns = aa_get_namespace(__aa_find_namespace(&root->sub_ns, name));
	read_unlock(&root->lock);

	return ns;
}

static struct aa_namespace *aa_prepare_namespace(const char *name)
{
	struct aa_namespace *ns, *root;

	root = aa_current_profile()->ns;

	write_lock(&root->lock);

	
	if (!name) {
		
		ns = aa_get_namespace(root);
		goto out;
	}

	
	
	ns = aa_get_namespace(__aa_find_namespace(&root->sub_ns, name));
	if (!ns) {
		
		struct aa_namespace *new_ns;
		write_unlock(&root->lock);
		new_ns = alloc_namespace(root->base.hname, name);
		if (!new_ns)
			return NULL;
		write_lock(&root->lock);
		
		ns = __aa_find_namespace(&root->sub_ns, name);
		if (!ns) {
			
			new_ns->parent = aa_get_namespace(root);

			list_add(&new_ns->base.list, &root->sub_ns);
			
			ns = aa_get_namespace(new_ns);
		} else {
			
			free_namespace(new_ns);
			
			aa_get_namespace(ns);
		}
	}
out:
	write_unlock(&root->lock);

	
	return ns;
}

static void __list_add_profile(struct list_head *list,
			       struct aa_profile *profile)
{
	list_add(&profile->base.list, list);
	
	aa_get_profile(profile);
}

static void __list_remove_profile(struct aa_profile *profile)
{
	list_del_init(&profile->base.list);
	if (!(profile->flags & PFLAG_NO_LIST_REF))
		
		aa_put_profile(profile);
}

static void __replace_profile(struct aa_profile *old, struct aa_profile *new)
{
	struct aa_policy *policy;
	struct aa_profile *child, *tmp;

	if (old->parent)
		policy = &old->parent->base;
	else
		policy = &old->ns->base;

	
	new->parent = aa_get_profile(old->parent);
	new->ns = aa_get_namespace(old->ns);
	new->sid = old->sid;
	__list_add_profile(&policy->profiles, new);
	
	list_for_each_entry_safe(child, tmp, &old->base.profiles, base.list) {
		aa_put_profile(child->parent);
		child->parent = aa_get_profile(new);
		
		list_move(&child->base.list, &new->base.profiles);
	}

	
	old->replacedby = aa_get_profile(new);
	__list_remove_profile(old);
}

static void __profile_list_release(struct list_head *head);

static void __remove_profile(struct aa_profile *profile)
{
	
	__profile_list_release(&profile->base.profiles);
	
	profile->replacedby = aa_get_profile(profile->ns->unconfined);
	__list_remove_profile(profile);
}

static void __profile_list_release(struct list_head *head)
{
	struct aa_profile *profile, *tmp;
	list_for_each_entry_safe(profile, tmp, head, base.list)
		__remove_profile(profile);
}

static void __ns_list_release(struct list_head *head);

static void destroy_namespace(struct aa_namespace *ns)
{
	if (!ns)
		return;

	write_lock(&ns->lock);
	
	__profile_list_release(&ns->base.profiles);

	
	__ns_list_release(&ns->sub_ns);

	write_unlock(&ns->lock);
}

static void __remove_namespace(struct aa_namespace *ns)
{
	struct aa_profile *unconfined = ns->unconfined;

	
	list_del_init(&ns->base.list);

	if (ns->parent)
		ns->unconfined = aa_get_profile(ns->parent->unconfined);

	destroy_namespace(ns);

	
	aa_put_profile(unconfined);
	
	aa_put_namespace(ns);
}

static void __ns_list_release(struct list_head *head)
{
	struct aa_namespace *ns, *tmp;
	list_for_each_entry_safe(ns, tmp, head, base.list)
		__remove_namespace(ns);

}

int __init aa_alloc_root_ns(void)
{
	
	root_ns = alloc_namespace(NULL, "root");
	if (!root_ns)
		return -ENOMEM;

	return 0;
}

void __init aa_free_root_ns(void)
 {
	 struct aa_namespace *ns = root_ns;
	 root_ns = NULL;

	 destroy_namespace(ns);
	 aa_put_namespace(ns);
}

struct aa_profile *aa_alloc_profile(const char *hname)
{
	struct aa_profile *profile;

	
	profile = kzalloc(sizeof(*profile), GFP_KERNEL);
	if (!profile)
		return NULL;

	if (!policy_init(&profile->base, NULL, hname)) {
		kzfree(profile);
		return NULL;
	}

	
	return profile;
}

struct aa_profile *aa_new_null_profile(struct aa_profile *parent, int hat)
{
	struct aa_profile *profile = NULL;
	char *name;
	u32 sid = aa_alloc_sid();

	
	name = kmalloc(strlen(parent->base.hname) + 2 + 7 + 8, GFP_KERNEL);
	if (!name)
		goto fail;
	sprintf(name, "%s//null-%x", parent->base.hname, sid);

	profile = aa_alloc_profile(name);
	kfree(name);
	if (!profile)
		goto fail;

	profile->sid = sid;
	profile->mode = APPARMOR_COMPLAIN;
	profile->flags = PFLAG_NULL;
	if (hat)
		profile->flags |= PFLAG_HAT;

	
	profile->parent = aa_get_profile(parent);
	profile->ns = aa_get_namespace(parent->ns);

	write_lock(&profile->ns->lock);
	__list_add_profile(&parent->base.profiles, profile);
	write_unlock(&profile->ns->lock);

	
	return profile;

fail:
	aa_free_sid(sid);
	return NULL;
}

static void free_profile(struct aa_profile *profile)
{
	AA_DEBUG("%s(%p)\n", __func__, profile);

	if (!profile)
		return;

	if (!list_empty(&profile->base.list)) {
		AA_ERROR("%s: internal error, "
			 "profile '%s' still on ns list\n",
			 __func__, profile->base.name);
		BUG();
	}

	
	policy_destroy(&profile->base);
	aa_put_profile(profile->parent);

	aa_put_namespace(profile->ns);
	kzfree(profile->rename);

	aa_free_file_rules(&profile->file);
	aa_free_cap_rules(&profile->caps);
	aa_free_rlimit_rules(&profile->rlimits);

	aa_free_sid(profile->sid);
	aa_put_dfa(profile->xmatch);
	aa_put_dfa(profile->policy.dfa);

	aa_put_profile(profile->replacedby);

	kzfree(profile);
}

void aa_free_profile_kref(struct kref *kref)
{
	struct aa_profile *p = container_of(kref, struct aa_profile,
					    base.count);

	free_profile(p);
}


static struct aa_profile *__find_child(struct list_head *head, const char *name)
{
	return (struct aa_profile *)__policy_find(head, name);
}

static struct aa_profile *__strn_find_child(struct list_head *head,
					    const char *name, int len)
{
	return (struct aa_profile *)__policy_strn_find(head, name, len);
}

struct aa_profile *aa_find_child(struct aa_profile *parent, const char *name)
{
	struct aa_profile *profile;

	read_lock(&parent->ns->lock);
	profile = aa_get_profile(__find_child(&parent->base.profiles, name));
	read_unlock(&parent->ns->lock);

	
	return profile;
}

static struct aa_policy *__lookup_parent(struct aa_namespace *ns,
					 const char *hname)
{
	struct aa_policy *policy;
	struct aa_profile *profile = NULL;
	char *split;

	policy = &ns->base;

	for (split = strstr(hname, "//"); split;) {
		profile = __strn_find_child(&policy->profiles, hname,
					    split - hname);
		if (!profile)
			return NULL;
		policy = &profile->base;
		hname = split + 2;
		split = strstr(hname, "//");
	}
	if (!profile)
		return &ns->base;
	return &profile->base;
}

static struct aa_profile *__lookup_profile(struct aa_policy *base,
					   const char *hname)
{
	struct aa_profile *profile = NULL;
	char *split;

	for (split = strstr(hname, "//"); split;) {
		profile = __strn_find_child(&base->profiles, hname,
					    split - hname);
		if (!profile)
			return NULL;

		base = &profile->base;
		hname = split + 2;
		split = strstr(hname, "//");
	}

	profile = __find_child(&base->profiles, hname);

	return profile;
}

struct aa_profile *aa_lookup_profile(struct aa_namespace *ns, const char *hname)
{
	struct aa_profile *profile;

	read_lock(&ns->lock);
	profile = aa_get_profile(__lookup_profile(&ns->base, hname));
	read_unlock(&ns->lock);

	
	return profile;
}

static int replacement_allowed(struct aa_profile *profile, int noreplace,
			       const char **info)
{
	if (profile) {
		if (profile->flags & PFLAG_IMMUTABLE) {
			*info = "cannot replace immutible profile";
			return -EPERM;
		} else if (noreplace) {
			*info = "profile already exists";
			return -EEXIST;
		}
	}
	return 0;
}

static void __add_new_profile(struct aa_namespace *ns, struct aa_policy *policy,
			      struct aa_profile *profile)
{
	if (policy != &ns->base)
		
		profile->parent = aa_get_profile((struct aa_profile *) policy);
	__list_add_profile(&policy->profiles, profile);
	
	profile->sid = aa_alloc_sid();
	profile->ns = aa_get_namespace(ns);
}

static int audit_policy(int op, gfp_t gfp, const char *name, const char *info,
			int error)
{
	struct common_audit_data sa;
	struct apparmor_audit_data aad = {0,};
	COMMON_AUDIT_DATA_INIT(&sa, NONE);
	sa.aad = &aad;
	aad.op = op;
	aad.name = name;
	aad.info = info;
	aad.error = error;

	return aa_audit(AUDIT_APPARMOR_STATUS, __aa_current_profile(), gfp,
			&sa, NULL);
}

bool aa_may_manage_policy(int op)
{
	
	if (aa_g_lock_policy) {
		audit_policy(op, GFP_KERNEL, NULL, "policy_locked", -EACCES);
		return 0;
	}

	if (!capable(CAP_MAC_ADMIN)) {
		audit_policy(op, GFP_KERNEL, NULL, "not policy admin", -EACCES);
		return 0;
	}

	return 1;
}

ssize_t aa_replace_profiles(void *udata, size_t size, bool noreplace)
{
	struct aa_policy *policy;
	struct aa_profile *old_profile = NULL, *new_profile = NULL;
	struct aa_profile *rename_profile = NULL;
	struct aa_namespace *ns = NULL;
	const char *ns_name, *name = NULL, *info = NULL;
	int op = OP_PROF_REPL;
	ssize_t error;

	
	new_profile = aa_unpack(udata, size, &ns_name);
	if (IS_ERR(new_profile)) {
		error = PTR_ERR(new_profile);
		new_profile = NULL;
		goto fail;
	}

	
	ns = aa_prepare_namespace(ns_name);
	if (!ns) {
		info = "failed to prepare namespace";
		error = -ENOMEM;
		name = ns_name;
		goto fail;
	}

	name = new_profile->base.hname;

	write_lock(&ns->lock);
	
	policy = __lookup_parent(ns, new_profile->base.hname);

	if (!policy) {
		info = "parent does not exist";
		error = -ENOENT;
		goto audit;
	}

	old_profile = __find_child(&policy->profiles, new_profile->base.name);
	
	aa_get_profile(old_profile);

	if (new_profile->rename) {
		rename_profile = __lookup_profile(&ns->base,
						  new_profile->rename);
		
		aa_get_profile(rename_profile);

		if (!rename_profile) {
			info = "profile to rename does not exist";
			name = new_profile->rename;
			error = -ENOENT;
			goto audit;
		}
	}

	error = replacement_allowed(old_profile, noreplace, &info);
	if (error)
		goto audit;

	error = replacement_allowed(rename_profile, noreplace, &info);
	if (error)
		goto audit;

audit:
	if (!old_profile && !rename_profile)
		op = OP_PROF_LOAD;

	error = audit_policy(op, GFP_ATOMIC, name, info, error);

	if (!error) {
		if (rename_profile)
			__replace_profile(rename_profile, new_profile);
		if (old_profile) {
			if (rename_profile)
				aa_free_sid(new_profile->sid);
			__replace_profile(old_profile, new_profile);
		}
		if (!(old_profile || rename_profile))
			__add_new_profile(ns, policy, new_profile);
	}
	write_unlock(&ns->lock);

out:
	aa_put_namespace(ns);
	aa_put_profile(rename_profile);
	aa_put_profile(old_profile);
	aa_put_profile(new_profile);
	if (error)
		return error;
	return size;

fail:
	error = audit_policy(op, GFP_KERNEL, name, info, error);
	goto out;
}

ssize_t aa_remove_profiles(char *fqname, size_t size)
{
	struct aa_namespace *root, *ns = NULL;
	struct aa_profile *profile = NULL;
	const char *name = fqname, *info = NULL;
	ssize_t error = 0;

	if (*fqname == 0) {
		info = "no profile specified";
		error = -ENOENT;
		goto fail;
	}

	root = aa_current_profile()->ns;

	if (fqname[0] == ':') {
		char *ns_name;
		name = aa_split_fqname(fqname, &ns_name);
		if (ns_name) {
			
			ns = aa_find_namespace(root, ns_name);
			if (!ns) {
				info = "namespace does not exist";
				error = -ENOENT;
				goto fail;
			}
		}
	} else
		
		ns = aa_get_namespace(root);

	if (!name) {
		
		write_lock(&ns->parent->lock);
		__remove_namespace(ns);
		write_unlock(&ns->parent->lock);
	} else {
		
		write_lock(&ns->lock);
		profile = aa_get_profile(__lookup_profile(&ns->base, name));
		if (!profile) {
			error = -ENOENT;
			info = "profile does not exist";
			goto fail_ns_lock;
		}
		name = profile->base.hname;
		__remove_profile(profile);
		write_unlock(&ns->lock);
	}

	
	(void) audit_policy(OP_PROF_RM, GFP_KERNEL, name, info, error);
	aa_put_namespace(ns);
	aa_put_profile(profile);
	return size;

fail_ns_lock:
	write_unlock(&ns->lock);
	aa_put_namespace(ns);

fail:
	(void) audit_policy(OP_PROF_RM, GFP_KERNEL, name, info, error);
	return error;
}
