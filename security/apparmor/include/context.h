/*
 * AppArmor security module
 *
 * This file contains AppArmor contexts used to associate "labels" to objects.
 *
 * Copyright (C) 1998-2008 Novell/SUSE
 * Copyright 2009-2010 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 */

#ifndef __AA_CONTEXT_H
#define __AA_CONTEXT_H

#include <linux/cred.h>
#include <linux/slab.h>
#include <linux/sched.h>

#include "policy.h"

struct aa_file_cxt {
	u16 allow;
};

static inline struct aa_file_cxt *aa_alloc_file_context(gfp_t gfp)
{
	return kzalloc(sizeof(struct aa_file_cxt), gfp);
}

static inline void aa_free_file_context(struct aa_file_cxt *cxt)
{
	if (cxt)
		kzfree(cxt);
}

struct aa_task_cxt {
	struct aa_profile *profile;
	struct aa_profile *onexec;
	struct aa_profile *previous;
	u64 token;
};

struct aa_task_cxt *aa_alloc_task_context(gfp_t flags);
void aa_free_task_context(struct aa_task_cxt *cxt);
void aa_dup_task_context(struct aa_task_cxt *new,
			 const struct aa_task_cxt *old);
int aa_replace_current_profile(struct aa_profile *profile);
int aa_set_current_onexec(struct aa_profile *profile);
int aa_set_current_hat(struct aa_profile *profile, u64 token);
int aa_restore_previous_profile(u64 cookie);

static inline bool __aa_task_is_confined(struct task_struct *task)
{
	struct aa_task_cxt *cxt = __task_cred(task)->security;

	BUG_ON(!cxt || !cxt->profile);
	if (unconfined(aa_newest_version(cxt->profile)))
		return 0;

	return 1;
}

static inline struct aa_profile *aa_cred_profile(const struct cred *cred)
{
	struct aa_task_cxt *cxt = cred->security;
	BUG_ON(!cxt || !cxt->profile);
	return aa_newest_version(cxt->profile);
}

static inline struct aa_profile *__aa_current_profile(void)
{
	return aa_cred_profile(current_cred());
}

static inline struct aa_profile *aa_current_profile(void)
{
	const struct aa_task_cxt *cxt = current_cred()->security;
	struct aa_profile *profile;
	BUG_ON(!cxt || !cxt->profile);

	profile = aa_newest_version(cxt->profile);
	if (unlikely((cxt->profile != profile)))
		aa_replace_current_profile(profile);

	return profile;
}

#endif 
