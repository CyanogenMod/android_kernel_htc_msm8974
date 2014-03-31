#ifndef _ASM_POWERPC_MMAN_H
#define _ASM_POWERPC_MMAN_H

#include <asm-generic/mman-common.h>

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#define PROT_SAO	0x10		

#define MAP_RENAME      MAP_ANONYMOUS   
#define MAP_NORESERVE   0x40            
#define MAP_LOCKED	0x80

#define MAP_GROWSDOWN	0x0100		
#define MAP_DENYWRITE	0x0800		
#define MAP_EXECUTABLE	0x1000		

#define MCL_CURRENT     0x2000          
#define MCL_FUTURE      0x4000          

#define MAP_POPULATE	0x8000		
#define MAP_NONBLOCK	0x10000		
#define MAP_STACK	0x20000		
#define MAP_HUGETLB	0x40000		

#ifdef __KERNEL__
#ifdef CONFIG_PPC64

#include <asm/cputable.h>
#include <linux/mm.h>

static inline unsigned long arch_calc_vm_prot_bits(unsigned long prot)
{
	return (prot & PROT_SAO) ? VM_SAO : 0;
}
#define arch_calc_vm_prot_bits(prot) arch_calc_vm_prot_bits(prot)

static inline pgprot_t arch_vm_get_page_prot(unsigned long vm_flags)
{
	return (vm_flags & VM_SAO) ? __pgprot(_PAGE_SAO) : __pgprot(0);
}
#define arch_vm_get_page_prot(vm_flags) arch_vm_get_page_prot(vm_flags)

static inline int arch_validate_prot(unsigned long prot)
{
	if (prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC | PROT_SEM | PROT_SAO))
		return 0;
	if ((prot & PROT_SAO) && !cpu_has_feature(CPU_FTR_SAO))
		return 0;
	return 1;
}
#define arch_validate_prot(prot) arch_validate_prot(prot)

#endif 
#endif 
#endif	
