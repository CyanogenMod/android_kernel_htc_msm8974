/* MN10300 ELF constant and register definitions
 *
 * Copyright (C) 2007 Matsushita Electric Industrial Co., Ltd.
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */
#ifndef _ASM_ELF_H
#define _ASM_ELF_H

#include <linux/utsname.h>
#include <asm/ptrace.h>
#include <asm/user.h>

#define R_MN10300_NONE		0	
#define R_MN10300_32		1	
#define R_MN10300_16		2	
#define R_MN10300_8		3	
#define R_MN10300_PCREL32	4	
#define R_MN10300_PCREL16	5	
#define R_MN10300_PCREL8	6	
#define R_MN10300_24		9	
#define R_MN10300_RELATIVE	23	
#define R_MN10300_SYM_DIFF	33	
#define R_MN10300_ALIGN 	34	

#define HWCAP_MN10300_ATOMIC_OP_UNIT	1	


typedef unsigned long elf_greg_t;

#define ELF_NGREG ((sizeof(struct pt_regs) / sizeof(elf_greg_t)) - 1)
typedef elf_greg_t elf_gregset_t[ELF_NGREG];

#define ELF_NFPREG 32
typedef float elf_fpreg_t;

typedef struct {
	elf_fpreg_t	fpregs[ELF_NFPREG];
	u_int32_t	fpcr;
} elf_fpregset_t;

#define elf_check_arch(x) \
	(((x)->e_machine == EM_CYGNUS_MN10300) ||	\
	 ((x)->e_machine == EM_MN10300))

#define ELF_CLASS	ELFCLASS32
#define ELF_DATA	ELFDATA2LSB
#define ELF_ARCH	EM_MN10300

#define ELF_PLAT_INIT(_r, load_addr)					\
do {									\
	struct pt_regs *_ur = current->thread.uregs;			\
	_ur->a3   = 0;	_ur->a2   = 0;	_ur->d3   = 0;	_ur->d2   = 0;	\
	_ur->mcvf = 0;	_ur->mcrl = 0;	_ur->mcrh = 0;	_ur->mdrq = 0;	\
	_ur->e1   = 0;	_ur->e0   = 0;	_ur->e7   = 0;	_ur->e6   = 0;	\
	_ur->e5   = 0;	_ur->e4   = 0;	_ur->e3   = 0;	_ur->e2   = 0;	\
	_ur->lar  = 0;	_ur->lir  = 0;	_ur->mdr  = 0;			\
	_ur->a1   = 0;	_ur->a0   = 0;	_ur->d1   = 0;	_ur->d0   = 0;	\
} while (0)

#define CORE_DUMP_USE_REGSET
#define ELF_EXEC_PAGESIZE	4096

#define ELF_ET_DYN_BASE         0x04000000

#define ELF_CORE_COPY_REGS(pr_reg, regs)	\
do {						\
	pr_reg[0]	= regs->a3;		\
	pr_reg[1]	= regs->a2;		\
	pr_reg[2]	= regs->d3;		\
	pr_reg[3]	= regs->d2;		\
	pr_reg[4]	= regs->mcvf;		\
	pr_reg[5]	= regs->mcrl;		\
	pr_reg[6]	= regs->mcrh;		\
	pr_reg[7]	= regs->mdrq;		\
	pr_reg[8]	= regs->e1;		\
	pr_reg[9]	= regs->e0;		\
	pr_reg[10]	= regs->e7;		\
	pr_reg[11]	= regs->e6;		\
	pr_reg[12]	= regs->e5;		\
	pr_reg[13]	= regs->e4;		\
	pr_reg[14]	= regs->e3;		\
	pr_reg[15]	= regs->e2;		\
	pr_reg[16]	= regs->sp;		\
	pr_reg[17]	= regs->lar;		\
	pr_reg[18]	= regs->lir;		\
	pr_reg[19]	= regs->mdr;		\
	pr_reg[20]	= regs->a1;		\
	pr_reg[21]	= regs->a0;		\
	pr_reg[22]	= regs->d1;		\
	pr_reg[23]	= regs->d0;		\
	pr_reg[24]	= regs->orig_d0;	\
	pr_reg[25]	= regs->epsw;		\
	pr_reg[26]	= regs->pc;		\
} while (0);

#ifdef CONFIG_MN10300_HAS_ATOMIC_OPS_UNIT
#define ELF_HWCAP	(HWCAP_MN10300_ATOMIC_OP_UNIT)
#else
#define ELF_HWCAP	(0)
#endif

#define ELF_PLATFORM  (NULL)

#ifdef __KERNEL__
#define SET_PERSONALITY(ex) set_personality(PER_LINUX)
#endif

#endif 
