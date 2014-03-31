/* elf.h: FR-V ELF definitions
 *
 * Copyright (C) 2003 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 * - Derived from include/asm-m68knommu/elf.h
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#ifndef __ASM_ELF_H
#define __ASM_ELF_H

#include <asm/ptrace.h>
#include <asm/user.h>

struct elf32_hdr;

#define EF_FRV_GPR_MASK         0x00000003 
#define EF_FRV_GPR32		0x00000001 
#define EF_FRV_GPR64		0x00000002 
#define EF_FRV_FPR_MASK         0x0000000c 
#define EF_FRV_FPR32		0x00000004 
#define EF_FRV_FPR64		0x00000008 
#define EF_FRV_FPR_NONE		0x0000000C 
#define EF_FRV_DWORD_MASK       0x00000030 
#define EF_FRV_DWORD_YES	0x00000010 
#define EF_FRV_DWORD_NO		0x00000020 
#define EF_FRV_DOUBLE		0x00000040 
#define EF_FRV_MEDIA		0x00000080 
#define EF_FRV_PIC		0x00000100 
#define EF_FRV_NON_PIC_RELOCS	0x00000200 
#define EF_FRV_MULADD           0x00000400 
#define EF_FRV_BIGPIC           0x00000800 
#define EF_FRV_LIBPIC           0x00001000 
#define EF_FRV_G0               0x00002000 
#define EF_FRV_NOPACK           0x00004000 
#define EF_FRV_FDPIC            0x00008000 
#define EF_FRV_CPU_MASK         0xff000000 
#define EF_FRV_CPU_GENERIC	0x00000000 
#define EF_FRV_CPU_FR500	0x01000000 
#define EF_FRV_CPU_FR300	0x02000000 
#define EF_FRV_CPU_SIMPLE       0x03000000 
#define EF_FRV_CPU_TOMCAT       0x04000000 
#define EF_FRV_CPU_FR400	0x05000000 
#define EF_FRV_CPU_FR550        0x06000000 
#define EF_FRV_CPU_FR405	0x07000000 
#define EF_FRV_CPU_FR450	0x08000000 



typedef unsigned long elf_greg_t;

#define ELF_NGREG (sizeof(struct pt_regs) / sizeof(elf_greg_t))
typedef elf_greg_t elf_gregset_t[ELF_NGREG];

typedef struct user_fpmedia_regs elf_fpregset_t;

extern int elf_check_arch(const struct elf32_hdr *hdr);

#define elf_check_fdpic(x) ((x)->e_flags & EF_FRV_FDPIC && !((x)->e_flags & EF_FRV_NON_PIC_RELOCS))
#define elf_check_const_displacement(x) ((x)->e_flags & EF_FRV_PIC)

#define ELF_CLASS	ELFCLASS32
#define ELF_DATA	ELFDATA2MSB
#define ELF_ARCH	EM_FRV

#define ELF_PLAT_INIT(_r)			\
do {						\
	__kernel_frame0_ptr->gr16	= 0;	\
	__kernel_frame0_ptr->gr17	= 0;	\
	__kernel_frame0_ptr->gr18	= 0;	\
	__kernel_frame0_ptr->gr19	= 0;	\
	__kernel_frame0_ptr->gr20	= 0;	\
	__kernel_frame0_ptr->gr21	= 0;	\
	__kernel_frame0_ptr->gr22	= 0;	\
	__kernel_frame0_ptr->gr23	= 0;	\
	__kernel_frame0_ptr->gr24	= 0;	\
	__kernel_frame0_ptr->gr25	= 0;	\
	__kernel_frame0_ptr->gr26	= 0;	\
	__kernel_frame0_ptr->gr27	= 0;	\
	__kernel_frame0_ptr->gr29	= 0;	\
} while(0)

#define ELF_FDPIC_PLAT_INIT(_regs, _exec_map_addr, _interp_map_addr, _dynamic_addr)	\
do {											\
	__kernel_frame0_ptr->gr16	= _exec_map_addr;				\
	__kernel_frame0_ptr->gr17	= _interp_map_addr;				\
	__kernel_frame0_ptr->gr18	= _dynamic_addr;				\
	__kernel_frame0_ptr->gr19	= 0;						\
	__kernel_frame0_ptr->gr20	= 0;						\
	__kernel_frame0_ptr->gr21	= 0;						\
	__kernel_frame0_ptr->gr22	= 0;						\
	__kernel_frame0_ptr->gr23	= 0;						\
	__kernel_frame0_ptr->gr24	= 0;						\
	__kernel_frame0_ptr->gr25	= 0;						\
	__kernel_frame0_ptr->gr26	= 0;						\
	__kernel_frame0_ptr->gr27	= 0;						\
	__kernel_frame0_ptr->gr29	= 0;						\
} while(0)

#define CORE_DUMP_USE_REGSET
#define ELF_FDPIC_CORE_EFLAGS	EF_FRV_FDPIC
#define ELF_EXEC_PAGESIZE	16384


#define ELF_ET_DYN_BASE         0x08000000UL


#define ELF_HWCAP	(0)


#define ELF_PLATFORM  (NULL)

#define SET_PERSONALITY(ex) set_personality(PER_LINUX)

#endif
