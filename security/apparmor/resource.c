/*
 * AppArmor security module
 *
 * This file contains AppArmor resource mediation and attachment
 *
 * Copyright (C) 1998-2008 Novell/SUSE
 * Copyright 2009-2010 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 */

#include <linux/audit.h>

#include "include/audit.h"
#include "include/resource.h"
#include "include/policy.h"

#include "rlim_names.h"

struct aa_fs_entry aa_fs_entry_rlimit[] = {
	AA_FS_FILE_STRING("mask", AA_FS_RLIMIT_MASK),
	{ }
};

static void audit_cb(struct audit_buffer *ab, void *va)
{
	struct common_audit_data *sa = va;

	audit_log_format(ab, " rlimit=%s value=%lu",
			 rlim_names[sa->aad->rlim.rlim], sa->aad->rlim.max);
}

static int audit_resource(struct aa_profile *profile, unsigned int resource,
			  unsigned long value, int error)
{
	struct common_audit_data sa;
	struct apparmor_audit_data aad = {0,};

	COMMON_AUDIT_DATA_INIT(&sa, NONE);
	sa.aad = &aad;
	aad.op = OP_SETRLIMIT,
	aad.rlim.rlim = resource;
	aad.rlim.max = value;
	aad.error = error;
	return aa_audit(AUDIT_APPARMOR_AUTO, profile, GFP_KERNEL, &sa,
			audit_cb);
}

int aa_map_resource(int resource)
{
	return rlim_map[resource];
}

int aa_task_setrlimit(struct aa_profile *profile, struct task_struct *task,
		      unsigned int resource, struct rlimit *new_rlim)
{
	int error = 0;

	if ((task != current->group_leader) ||
	    (profile->rlimits.mask & (1 << resource) &&
	     new_rlim->rlim_max > profile->rlimits.limits[resource].rlim_max))
		error = -EACCES;

	return audit_resource(profile, resource, new_rlim->rlim_max, error);
}

void __aa_transition_rlimits(struct aa_profile *old, struct aa_profile *new)
{
	unsigned int mask = 0;
	struct rlimit *rlim, *initrlim;
	int i;

	if (old->rlimits.mask) {
		for (i = 0, mask = 1; i < RLIM_NLIMITS; i++, mask <<= 1) {
			if (old->rlimits.mask & mask) {
				rlim = current->signal->rlim + i;
				initrlim = init_task.signal->rlim + i;
				rlim->rlim_cur = min(rlim->rlim_max,
						     initrlim->rlim_cur);
			}
		}
	}

	
	if (!new->rlimits.mask)
		return;
	for (i = 0, mask = 1; i < RLIM_NLIMITS; i++, mask <<= 1) {
		if (!(new->rlimits.mask & mask))
			continue;

		rlim = current->signal->rlim + i;
		rlim->rlim_max = min(rlim->rlim_max,
				     new->rlimits.limits[i].rlim_max);
		
		rlim->rlim_cur = min(rlim->rlim_cur, rlim->rlim_max);
	}
}
