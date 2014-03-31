/*
 * Copyright (C) 2007 Jeff Dike (jdike@{addtoit,linux.intel}.com)
 * Licensed under the GPL
 */

#include <signal.h>
#include <sched.h>
#include <asm/unistd.h>
#include <sys/time.h>
#include "as-layout.h"
#include "ptrace_user.h"
#include "stub-data.h"
#include "sysdep/stub.h"


void __attribute__ ((__section__ (".__syscall_stub")))
stub_clone_handler(void)
{
	struct stub_data *data = (struct stub_data *) STUB_DATA;
	long err;

	err = stub_syscall2(__NR_clone, CLONE_PARENT | CLONE_FILES | SIGCHLD,
			    STUB_DATA + UM_KERN_PAGE_SIZE / 2 - sizeof(void *));
	if (err != 0)
		goto out;

	err = stub_syscall4(__NR_ptrace, PTRACE_TRACEME, 0, 0, 0);
	if (err)
		goto out;

	err = stub_syscall3(__NR_setitimer, ITIMER_VIRTUAL,
			    (long) &data->timer, 0);
	if (err)
		goto out;

	remap_stack(data->fd, data->offset);
	goto done;

 out:
	data->err = err;
 done:
	trap_myself();
}
