#ifndef _CRIS_PTRACE_H
#define _CRIS_PTRACE_H

#include <arch/ptrace.h>

#ifdef __KERNEL__

#define PTRACE_GETREGS            12
#define PTRACE_SETREGS            13

#define profile_pc(regs) instruction_pointer(regs)

#endif 

#endif 
