
#ifndef _PAGE_HASHPTE
#define _PAGE_HASHPTE	0
#endif
#ifndef _PAGE_SHARED
#define _PAGE_SHARED	0
#endif
#ifndef _PAGE_HWWRITE
#define _PAGE_HWWRITE	0
#endif
#ifndef _PAGE_EXEC
#define _PAGE_EXEC	0
#endif
#ifndef _PAGE_ENDIAN
#define _PAGE_ENDIAN	0
#endif
#ifndef _PAGE_COHERENT
#define _PAGE_COHERENT	0
#endif
#ifndef _PAGE_WRITETHRU
#define _PAGE_WRITETHRU	0
#endif
#ifndef _PAGE_4K_PFN
#define _PAGE_4K_PFN		0
#endif
#ifndef _PAGE_SAO
#define _PAGE_SAO	0
#endif
#ifndef _PAGE_PSIZE
#define _PAGE_PSIZE		0
#endif
#ifndef _PMD_PRESENT_MASK
#define _PMD_PRESENT_MASK	_PMD_PRESENT
#endif
#ifndef _PMD_SIZE
#define _PMD_SIZE	0
#define PMD_PAGE_SIZE(pmd)	bad_call_to_PMD_PAGE_SIZE()
#endif
#ifndef _PAGE_KERNEL_RO
#define _PAGE_KERNEL_RO		0
#endif
#ifndef _PAGE_KERNEL_ROX
#define _PAGE_KERNEL_ROX	(_PAGE_EXEC)
#endif
#ifndef _PAGE_KERNEL_RW
#define _PAGE_KERNEL_RW		(_PAGE_DIRTY | _PAGE_RW | _PAGE_HWWRITE)
#endif
#ifndef _PAGE_KERNEL_RWX
#define _PAGE_KERNEL_RWX	(_PAGE_DIRTY | _PAGE_RW | _PAGE_HWWRITE | _PAGE_EXEC)
#endif
#ifndef _PAGE_HPTEFLAGS
#define _PAGE_HPTEFLAGS _PAGE_HASHPTE
#endif
#ifndef _PTE_NONE_MASK
#define _PTE_NONE_MASK	_PAGE_HPTEFLAGS
#endif

#ifndef __ASSEMBLY__
extern unsigned long bad_call_to_PMD_PAGE_SIZE(void);
#endif 

#ifndef PTE_RPN_SHIFT
#define PTE_RPN_SHIFT	(PAGE_SHIFT)
#endif

#if defined(CONFIG_PPC32) && defined(CONFIG_PTE_64BIT)
#define PTE_RPN_MAX	(1ULL << (64 - PTE_RPN_SHIFT))
#define PTE_RPN_MASK	(~((1ULL<<PTE_RPN_SHIFT)-1))
#else
#define PTE_RPN_MAX	(1UL << (32 - PTE_RPN_SHIFT))
#define PTE_RPN_MASK	(~((1UL<<PTE_RPN_SHIFT)-1))
#endif

#define _PAGE_CHG_MASK	(PTE_RPN_MASK | _PAGE_HPTEFLAGS | _PAGE_DIRTY | \
                         _PAGE_ACCESSED | _PAGE_SPECIAL)

#define PAGE_PROT_BITS	(_PAGE_GUARDED | _PAGE_COHERENT | _PAGE_NO_CACHE | \
			 _PAGE_WRITETHRU | _PAGE_ENDIAN | _PAGE_4K_PFN | \
			 _PAGE_USER | _PAGE_ACCESSED | \
			 _PAGE_RW | _PAGE_HWWRITE | _PAGE_DIRTY | _PAGE_EXEC)

#define _PAGE_BASE_NC	(_PAGE_PRESENT | _PAGE_ACCESSED | _PAGE_PSIZE)
#if defined(CONFIG_SMP) || defined(CONFIG_PPC_STD_MMU)
#define _PAGE_BASE	(_PAGE_BASE_NC | _PAGE_COHERENT)
#else
#define _PAGE_BASE	(_PAGE_BASE_NC)
#endif

#define PAGE_NONE	__pgprot(_PAGE_BASE)
#define PAGE_SHARED	__pgprot(_PAGE_BASE | _PAGE_USER | _PAGE_RW)
#define PAGE_SHARED_X	__pgprot(_PAGE_BASE | _PAGE_USER | _PAGE_RW | _PAGE_EXEC)
#define PAGE_COPY	__pgprot(_PAGE_BASE | _PAGE_USER)
#define PAGE_COPY_X	__pgprot(_PAGE_BASE | _PAGE_USER | _PAGE_EXEC)
#define PAGE_READONLY	__pgprot(_PAGE_BASE | _PAGE_USER)
#define PAGE_READONLY_X	__pgprot(_PAGE_BASE | _PAGE_USER | _PAGE_EXEC)

#define __P000	PAGE_NONE
#define __P001	PAGE_READONLY
#define __P010	PAGE_COPY
#define __P011	PAGE_COPY
#define __P100	PAGE_READONLY_X
#define __P101	PAGE_READONLY_X
#define __P110	PAGE_COPY_X
#define __P111	PAGE_COPY_X

#define __S000	PAGE_NONE
#define __S001	PAGE_READONLY
#define __S010	PAGE_SHARED
#define __S011	PAGE_SHARED
#define __S100	PAGE_READONLY_X
#define __S101	PAGE_READONLY_X
#define __S110	PAGE_SHARED_X
#define __S111	PAGE_SHARED_X

#define PAGE_KERNEL	__pgprot(_PAGE_BASE | _PAGE_KERNEL_RW)
#define PAGE_KERNEL_NC	__pgprot(_PAGE_BASE_NC | _PAGE_KERNEL_RW | \
				 _PAGE_NO_CACHE)
#define PAGE_KERNEL_NCG	__pgprot(_PAGE_BASE_NC | _PAGE_KERNEL_RW | \
				 _PAGE_NO_CACHE | _PAGE_GUARDED)
#define PAGE_KERNEL_X	__pgprot(_PAGE_BASE | _PAGE_KERNEL_RWX)
#define PAGE_KERNEL_RO	__pgprot(_PAGE_BASE | _PAGE_KERNEL_RO)
#define PAGE_KERNEL_ROX	__pgprot(_PAGE_BASE | _PAGE_KERNEL_ROX)

#if defined(CONFIG_KGDB) || defined(CONFIG_XMON) || defined(CONFIG_BDI_SWITCH) ||\
	defined(CONFIG_KPROBES) || defined(CONFIG_DYNAMIC_FTRACE)
#define PAGE_KERNEL_TEXT	PAGE_KERNEL_X
#else
#define PAGE_KERNEL_TEXT	PAGE_KERNEL_ROX
#endif

#define PAGE_KERNEL_EXEC	PAGE_KERNEL_X

#define pte_user(val)		((val & _PAGE_USER) == _PAGE_USER)

#define PAGE_AGP		(PAGE_KERNEL_NC)
#define HAVE_PAGE_AGP

#define __HAVE_ARCH_PTE_SPECIAL

