#ifndef _ASM_POWERPC_PTE_HASH32_H
#define _ASM_POWERPC_PTE_HASH32_H
#ifdef __KERNEL__


#define _PAGE_PRESENT	0x001	
#define _PAGE_HASHPTE	0x002	
#define _PAGE_FILE	0x004	
#define _PAGE_USER	0x004	
#define _PAGE_GUARDED	0x008	
#define _PAGE_COHERENT	0x010	
#define _PAGE_NO_CACHE	0x020	
#define _PAGE_WRITETHRU	0x040	
#define _PAGE_DIRTY	0x080	
#define _PAGE_ACCESSED	0x100	
#define _PAGE_RW	0x400	
#define _PAGE_SPECIAL	0x800	

#ifdef CONFIG_PTE_64BIT
#define _PTE_NONE_MASK	(0xffffffff00000000ULL | _PAGE_HASHPTE)
#else
#define _PTE_NONE_MASK	_PAGE_HASHPTE
#endif

#define _PMD_PRESENT	0
#define _PMD_PRESENT_MASK (PAGE_MASK)
#define _PMD_BAD	(~PAGE_MASK)

#define PTE_ATOMIC_UPDATES	1

#endif 
#endif 
