#ifndef _ASM_X86_SPARSEMEM_H
#define _ASM_X86_SPARSEMEM_H

#ifdef CONFIG_SPARSEMEM

#ifdef CONFIG_X86_32
# ifdef CONFIG_X86_PAE
#  define SECTION_SIZE_BITS	29
#  define MAX_PHYSADDR_BITS	36
#  define MAX_PHYSMEM_BITS	36
# else
#  define SECTION_SIZE_BITS	26
#  define MAX_PHYSADDR_BITS	32
#  define MAX_PHYSMEM_BITS	32
# endif
#else 
# define SECTION_SIZE_BITS	27 
# define MAX_PHYSADDR_BITS	44
# define MAX_PHYSMEM_BITS	46
#endif

#endif 
#endif 
