/*
 * Copyright (C) 2002 - 2007 Jeff Dike (jdike@{addtoit,linux.intel}.com)
 * Licensed under the GPL
 */

#include "linux/kernel.h"
#include "linux/ptrace.h"
#include "kern_util.h"
#include "sysdep/ptrace.h"
#include "sysdep/syscalls.h"

extern int syscall_table_size;
#define NR_SYSCALLS (syscall_table_size / sizeof(void *))

void handle_syscall(struct uml_pt_regs *r)
{
	struct pt_regs *regs = container_of(r, struct pt_regs, regs);
	long result;
	int syscall;

	syscall_trace(r, 0);

	syscall = UPT_SYSCALL_NR(r);
	if ((syscall >= NR_SYSCALLS) || (syscall < 0))
		result = -ENOSYS;
	else result = EXECUTE_SYSCALL(syscall, regs);

	REGS_SET_SYSCALL_RETURN(r->gp, result);

	syscall_trace(r, 1);
}
