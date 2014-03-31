/*
 * OpenRISC Linux
 *
 * Linux architectural port borrowing liberally from similar works of
 * others.  All original copyrights apply as per the original source
 * declaration.
 *
 * OpenRISC implementation:
 * Copyright (C) 2003 Matjaz Breskvar <phoenix@bsemi.com>
 * Copyright (C) 2010-2011 Jonas Bonn <jonas@southpole.se>
 * et al.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __ASM_OPENRISC_ELF_H
#define __ASM_OPENRISC_ELF_H

#include <linux/types.h>
#include <linux/ptrace.h>


#define R_OR32_NONE	0
#define R_OR32_32	1
#define R_OR32_16	2
#define R_OR32_8	3
#define R_OR32_CONST	4
#define R_OR32_CONSTH	5
#define R_OR32_JUMPTARG	6
#define R_OR32_VTINHERIT 7
#define R_OR32_VTENTRY	8

typedef unsigned long elf_greg_t;

#define ELF_NGREG (sizeof(struct user_regs_struct) / sizeof(elf_greg_t))
typedef elf_greg_t elf_gregset_t[ELF_NGREG];

typedef unsigned long elf_fpregset_t;

#define EM_OR32         0x8472
#define EM_OPENRISC     92     

#define ELF_ARCH	EM_OR32
#define ELF_CLASS	ELFCLASS32
#define ELF_DATA	ELFDATA2MSB

#ifdef __KERNEL__


#define elf_check_arch(x) \
	(((x)->e_machine == EM_OR32) || ((x)->e_machine == EM_OPENRISC))


#define ELF_ET_DYN_BASE         (0x08000000)

#define CORE_DUMP_USE_REGSET

#define ELF_EXEC_PAGESIZE	8192

extern void dump_elf_thread(elf_greg_t *dest, struct pt_regs *pt);
#define ELF_CORE_COPY_REGS(dest, regs) dump_elf_thread(dest, regs);


#define ELF_HWCAP	(0)


#define ELF_PLATFORM	(NULL)

#define SET_PERSONALITY(ex) set_personality(PER_LINUX)

#endif 
#endif
