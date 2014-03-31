#ifndef _ASM_POWERPC_PTE_HASH64_H
#define _ASM_POWERPC_PTE_HASH64_H
#ifdef __KERNEL__

#define _PAGE_PRESENT		0x0001 
#define _PAGE_USER		0x0002 
#define _PAGE_FILE		0x0002 
#define _PAGE_EXEC		0x0004 
#define _PAGE_GUARDED		0x0008
#define _PAGE_COHERENT		0x0010 
#define _PAGE_NO_CACHE		0x0020 
#define _PAGE_WRITETHRU		0x0040 
#define _PAGE_DIRTY		0x0080 
#define _PAGE_ACCESSED		0x0100 
#define _PAGE_RW		0x0200 
#define _PAGE_BUSY		0x0800 

#define _PAGE_KERNEL_RW		(_PAGE_RW | _PAGE_DIRTY) 
#define _PAGE_KERNEL_RO		 _PAGE_KERNEL_RW

#define _PAGE_SAO		(_PAGE_WRITETHRU | _PAGE_NO_CACHE | _PAGE_COHERENT)

#define _PAGE_PSIZE		0

#define _PTEIDX_SECONDARY	0x8
#define _PTEIDX_GROUP_IX	0x7

#define PTE_ATOMIC_UPDATES	1

#ifdef CONFIG_PPC_64K_PAGES
#include <asm/pte-hash64-64k.h>
#else
#include <asm/pte-hash64-4k.h>
#endif

#endif 
#endif 
