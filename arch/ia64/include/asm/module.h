#ifndef _ASM_IA64_MODULE_H
#define _ASM_IA64_MODULE_H

/*
 * IA-64-specific support for kernel module loader.
 *
 * Copyright (C) 2003 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 */

struct elf64_shdr;			

struct mod_arch_specific {
	struct elf64_shdr *core_plt;	
	struct elf64_shdr *init_plt;	
	struct elf64_shdr *got;		
	struct elf64_shdr *opd;		
	struct elf64_shdr *unwind;	
#ifdef CONFIG_PARAVIRT
	struct elf64_shdr *paravirt_bundles;
					
	struct elf64_shdr *paravirt_insts;
					
#endif
	unsigned long gp;		

	void *core_unw_table;		
	void *init_unw_table;		
	unsigned int next_got_entry;	
};

#define Elf_Shdr	Elf64_Shdr
#define Elf_Sym		Elf64_Sym
#define Elf_Ehdr	Elf64_Ehdr

#define MODULE_PROC_FAMILY	"ia64"
#define MODULE_ARCH_VERMAGIC	MODULE_PROC_FAMILY \
	"gcc-" __stringify(__GNUC__) "." __stringify(__GNUC_MINOR__)

#define ARCH_SHF_SMALL	SHF_IA_64_SHORT

#endif 
