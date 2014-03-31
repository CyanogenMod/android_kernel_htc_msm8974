#ifndef _ASM_POWERPC_MODULE_H
#define _ASM_POWERPC_MODULE_H
#ifdef __KERNEL__

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/list.h>
#include <asm/bug.h>


#ifndef __powerpc64__

struct ppc_plt_entry {
	
	unsigned int jump[4];
};
#endif	


struct mod_arch_specific {
#ifdef __powerpc64__
	unsigned int stubs_section;	
	unsigned int toc_section;	
#ifdef CONFIG_DYNAMIC_FTRACE
	unsigned long toc;
	unsigned long tramp;
#endif

#else 
	
	unsigned int core_plt_section;
	unsigned int init_plt_section;
#ifdef CONFIG_DYNAMIC_FTRACE
	unsigned long tramp;
#endif
#endif 

	
	struct list_head bug_list;
	struct bug_entry *bug_table;
	unsigned int num_bugs;
};


#ifdef __powerpc64__
#    define Elf_Shdr	Elf64_Shdr
#    define Elf_Sym	Elf64_Sym
#    define Elf_Ehdr	Elf64_Ehdr
#    ifdef MODULE
	asm(".section .stubs,\"ax\",@nobits; .align 3; .previous");
#    endif
#else
#    define Elf_Shdr	Elf32_Shdr
#    define Elf_Sym	Elf32_Sym
#    define Elf_Ehdr	Elf32_Ehdr
#    ifdef MODULE
	asm(".section .plt,\"ax\",@nobits; .align 3; .previous");
	asm(".section .init.plt,\"ax\",@nobits; .align 3; .previous");
#    endif	
#endif

#ifdef CONFIG_DYNAMIC_FTRACE
#    ifdef MODULE
	asm(".section .ftrace.tramp,\"ax\",@nobits; .align 3; .previous");
#    endif	
#endif


struct exception_table_entry;
void sort_ex_table(struct exception_table_entry *start,
		   struct exception_table_entry *finish);

#ifdef CONFIG_MODVERSIONS
#define ARCH_RELOCATES_KCRCTAB

extern const unsigned long reloc_start[];
#endif
#endif 
#endif	
