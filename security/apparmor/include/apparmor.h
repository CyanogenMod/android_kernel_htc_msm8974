/*
 * AppArmor security module
 *
 * This file contains AppArmor basic global and lib definitions
 *
 * Copyright (C) 1998-2008 Novell/SUSE
 * Copyright 2009-2010 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 */

#ifndef __APPARMOR_H
#define __APPARMOR_H

#include <linux/fs.h>

#include "match.h"

#define AA_CLASS_ENTRY		0
#define AA_CLASS_UNKNOWN	1
#define AA_CLASS_FILE		2
#define AA_CLASS_CAP		3
#define AA_CLASS_NET		4
#define AA_CLASS_RLIMITS	5
#define AA_CLASS_DOMAIN		6

#define AA_CLASS_LAST		AA_CLASS_DOMAIN

extern enum audit_mode aa_g_audit;
extern bool aa_g_audit_header;
extern bool aa_g_debug;
extern bool aa_g_lock_policy;
extern bool aa_g_logsyscall;
extern bool aa_g_paranoid_load;
extern unsigned int aa_g_path_max;


#define AA_DEBUG(fmt, args...)						\
	do {								\
		if (aa_g_debug && printk_ratelimit())			\
			printk(KERN_DEBUG "AppArmor: " fmt, ##args);	\
	} while (0)

#define AA_ERROR(fmt, args...)						\
	do {								\
		if (printk_ratelimit())					\
			printk(KERN_ERR "AppArmor: " fmt, ##args);	\
	} while (0)

extern int apparmor_initialized __initdata;

char *aa_split_fqname(char *args, char **ns_name);
void aa_info_message(const char *str);
void *kvmalloc(size_t size);
void kvfree(void *buffer);


static inline bool aa_strneq(const char *str, const char *sub, int len)
{
	return !strncmp(str, sub, len) && !str[len];
}

static inline unsigned int aa_dfa_null_transition(struct aa_dfa *dfa,
						  unsigned int start)
{
	
	return aa_dfa_next(dfa, start, 0);
}

static inline bool mediated_filesystem(struct inode *inode)
{
	return !(inode->i_sb->s_flags & MS_NOUSER);
}

#endif 
