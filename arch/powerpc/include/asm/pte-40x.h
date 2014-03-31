#ifndef _ASM_POWERPC_PTE_40x_H
#define _ASM_POWERPC_PTE_40x_H
#ifdef __KERNEL__


#define	_PAGE_GUARDED	0x001	
#define _PAGE_FILE	0x001	
#define _PAGE_PRESENT	0x002	
#define	_PAGE_NO_CACHE	0x004	
#define	_PAGE_WRITETHRU	0x008	
#define	_PAGE_USER	0x010	
#define	_PAGE_SPECIAL	0x020	
#define	_PAGE_RW	0x040	
#define	_PAGE_DIRTY	0x080	
#define _PAGE_HWWRITE	0x100	
#define _PAGE_EXEC	0x200	
#define _PAGE_ACCESSED	0x400	

#define _PMD_PRESENT	0x400	
#define _PMD_BAD	0x802
#define _PMD_SIZE	0x0e0	
#define _PMD_SIZE_4M	0x0c0
#define _PMD_SIZE_16M	0x0e0

#define PMD_PAGE_SIZE(pmdval)	(1024 << (((pmdval) & _PMD_SIZE) >> 4))

#define PTE_ATOMIC_UPDATES	1

#endif 
#endif 
