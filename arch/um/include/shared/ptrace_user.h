/* 
 * Copyright (C) 2000 - 2007 Jeff Dike (jdike@{addtoit,linux.intel}.com)
 * Licensed under the GPL
 */

#ifndef __PTRACE_USER_H__
#define __PTRACE_USER_H__

#include <sys/ptrace.h>
#include <sysdep/ptrace_user.h>

extern int ptrace_getregs(long pid, unsigned long *regs_out);
extern int ptrace_setregs(long pid, unsigned long *regs_in);


#ifndef PTRACE_SYSEMU
#define PTRACE_SYSEMU 31
#endif
#ifndef PTRACE_SYSEMU_SINGLESTEP
#define PTRACE_SYSEMU_SINGLESTEP 32
#endif

#ifndef PTRACE_OLDSETOPTIONS
#define PTRACE_OLDSETOPTIONS PTRACE_SETOPTIONS
#endif

void set_using_sysemu(int value);
int get_using_sysemu(void);
extern int sysemu_supported;

#define SELECT_PTRACE_OPERATION(sysemu_mode, singlestep_mode) \
	(((int[3][3] ) { \
		{ PTRACE_SYSCALL, PTRACE_SYSCALL, PTRACE_SINGLESTEP }, \
		{ PTRACE_SYSEMU, PTRACE_SYSEMU, PTRACE_SINGLESTEP }, \
		{ PTRACE_SYSEMU, PTRACE_SYSEMU_SINGLESTEP, \
		  PTRACE_SYSEMU_SINGLESTEP } }) \
		[sysemu_mode][singlestep_mode])

#endif
