#ifndef _ASM_POWERPC_PTE_8xx_H
#define _ASM_POWERPC_PTE_8xx_H
#ifdef __KERNEL__


#define _PAGE_PRESENT	0x0001	
#define _PAGE_FILE	0x0002	
#define _PAGE_NO_CACHE	0x0002	
#define _PAGE_SHARED	0x0004	
#define _PAGE_SPECIAL	0x0008	
#define _PAGE_DIRTY	0x0100	

#define _PAGE_GUARDED	0x0010	
#define _PAGE_ACCESSED	0x0020	
#define _PAGE_WRITETHRU	0x0040	

#define _PAGE_RW	0x0400	
#define _PAGE_USER	0x0800	

#define _PMD_PRESENT	0x0001
#define _PMD_BAD	0x0ff0
#define _PMD_PAGE_MASK	0x000c
#define _PMD_PAGE_8M	0x000c

#define _PTE_NONE_MASK _PAGE_ACCESSED

#define PTE_ATOMIC_UPDATES	1

#define _PAGE_KERNEL_RO	(_PAGE_SHARED)
#define _PAGE_KERNEL_RW	(_PAGE_DIRTY | _PAGE_RW | _PAGE_HWWRITE)

#endif 
#endif 
