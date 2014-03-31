/*
 * AppArmor security module
 *
 * This file contains basic common functions used in AppArmor
 *
 * Copyright (C) 1998-2008 Novell/SUSE
 * Copyright 2009-2010 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 */

#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/vmalloc.h>

#include "include/audit.h"
#include "include/apparmor.h"


char *aa_split_fqname(char *fqname, char **ns_name)
{
	char *name = strim(fqname);

	*ns_name = NULL;
	if (name[0] == ':') {
		char *split = strchr(&name[1], ':');
		*ns_name = skip_spaces(&name[1]);
		if (split) {
			
			*split = 0;
			name = skip_spaces(split + 1);
		} else
			
			name = NULL;
	}
	if (name && *name == 0)
		name = NULL;

	return name;
}

void aa_info_message(const char *str)
{
	if (audit_enabled) {
		struct common_audit_data sa;
		struct apparmor_audit_data aad = {0,};
		COMMON_AUDIT_DATA_INIT(&sa, NONE);
		sa.aad = &aad;
		aad.info = str;
		aa_audit_msg(AUDIT_APPARMOR_STATUS, &sa, NULL);
	}
	printk(KERN_INFO "AppArmor: %s\n", str);
}

void *kvmalloc(size_t size)
{
	void *buffer = NULL;

	if (size == 0)
		return NULL;

	
	if (size <= (16*PAGE_SIZE))
		buffer = kmalloc(size, GFP_NOIO | __GFP_NOWARN);
	if (!buffer) {
		if (size < sizeof(struct work_struct))
			size = sizeof(struct work_struct);
		buffer = vmalloc(size);
	}
	return buffer;
}

static void do_vfree(struct work_struct *work)
{
	vfree(work);
}

void kvfree(void *buffer)
{
	if (is_vmalloc_addr(buffer)) {
		struct work_struct *work = (struct work_struct *) buffer;
		INIT_WORK(work, do_vfree);
		schedule_work(work);
	} else
		kfree(buffer);
}
