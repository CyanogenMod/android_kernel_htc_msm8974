#ifndef _ASM_IA64_TYPES_H
#define _ASM_IA64_TYPES_H


#ifdef __KERNEL__
#include <asm-generic/int-ll64.h>
#else
#include <asm-generic/int-l64.h>
#endif

#ifdef __ASSEMBLY__
# define __IA64_UL(x)		(x)
# define __IA64_UL_CONST(x)	x

#else
# define __IA64_UL(x)		((unsigned long)(x))
# define __IA64_UL_CONST(x)	x##UL

# ifdef __KERNEL__

struct fnptr {
	unsigned long ip;
	unsigned long gp;
};

# endif 
#endif 

#endif 
