#ifndef _ASM_POWERPC_SIGCONTEXT_H
#define _ASM_POWERPC_SIGCONTEXT_H

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#include <linux/compiler.h>
#include <asm/ptrace.h>
#ifdef __powerpc64__
#include <asm/elf.h>
#endif

struct sigcontext {
	unsigned long	_unused[4];
	int		signal;
#ifdef __powerpc64__
	int		_pad0;
#endif
	unsigned long	handler;
	unsigned long	oldmask;
	struct pt_regs	__user *regs;
#ifdef __powerpc64__
	elf_gregset_t	gp_regs;
	elf_fpregset_t	fp_regs;
	elf_vrreg_t	__user *v_regs;
	long		vmx_reserve[ELF_NVRREG+ELF_NVRREG+32+1];
#endif
};

#endif 
