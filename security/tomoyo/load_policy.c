/*
 * security/tomoyo/load_policy.c
 *
 * Copyright (C) 2005-2011  NTT DATA CORPORATION
 */

#include "common.h"

#ifndef CONFIG_SECURITY_TOMOYO_OMIT_USERSPACE_LOADER

static const char *tomoyo_loader;

static int __init tomoyo_loader_setup(char *str)
{
	tomoyo_loader = str;
	return 0;
}

__setup("TOMOYO_loader=", tomoyo_loader_setup);

static bool tomoyo_policy_loader_exists(void)
{
	struct path path;
	if (!tomoyo_loader)
		tomoyo_loader = CONFIG_SECURITY_TOMOYO_POLICY_LOADER;
	if (kern_path(tomoyo_loader, LOOKUP_FOLLOW, &path)) {
		printk(KERN_INFO "Not activating Mandatory Access Control "
		       "as %s does not exist.\n", tomoyo_loader);
		return false;
	}
	path_put(&path);
	return true;
}

static const char *tomoyo_trigger;

static int __init tomoyo_trigger_setup(char *str)
{
	tomoyo_trigger = str;
	return 0;
}

__setup("TOMOYO_trigger=", tomoyo_trigger_setup);

void tomoyo_load_policy(const char *filename)
{
	static bool done;
	char *argv[2];
	char *envp[3];

	if (tomoyo_policy_loaded || done)
		return;
	if (!tomoyo_trigger)
		tomoyo_trigger = CONFIG_SECURITY_TOMOYO_ACTIVATION_TRIGGER;
	if (strcmp(filename, tomoyo_trigger))
		return;
	if (!tomoyo_policy_loader_exists())
		return;
	done = true;
	printk(KERN_INFO "Calling %s to load policy. Please wait.\n",
	       tomoyo_loader);
	argv[0] = (char *) tomoyo_loader;
	argv[1] = NULL;
	envp[0] = "HOME=/";
	envp[1] = "PATH=/sbin:/bin:/usr/sbin:/usr/bin";
	envp[2] = NULL;
	call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC);
	tomoyo_check_profile();
}

#endif
