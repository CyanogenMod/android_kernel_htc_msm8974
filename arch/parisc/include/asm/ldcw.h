#ifndef __PARISC_LDCW_H
#define __PARISC_LDCW_H

#ifndef CONFIG_PA20

#define __PA_LDCW_ALIGNMENT	16
#define __ldcw_align(a) ({					\
	unsigned long __ret = (unsigned long) &(a)->lock[0];	\
	__ret = (__ret + __PA_LDCW_ALIGNMENT - 1)		\
		& ~(__PA_LDCW_ALIGNMENT - 1);			\
	(volatile unsigned int *) __ret;			\
})
#define __LDCW	"ldcw"

#else 

#define __PA_LDCW_ALIGNMENT	4
#define __ldcw_align(a) (&(a)->slock)
#define __LDCW	"ldcw,co"

#endif 

#define __ldcw(a) ({						\
	unsigned __ret;						\
	__asm__ __volatile__(__LDCW " 0(%2),%0"			\
		: "=r" (__ret), "+m" (*(a)) : "r" (a));		\
	__ret;							\
})

#ifdef CONFIG_SMP
# define __lock_aligned __attribute__((__section__(".data..lock_aligned")))
#endif

#endif 
