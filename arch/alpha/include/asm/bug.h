#ifndef _ALPHA_BUG_H
#define _ALPHA_BUG_H

#include <linux/linkage.h>

#ifdef CONFIG_BUG
#include <asm/pal.h>

#define BUG()	do {							\
	__asm__ __volatile__(						\
		"call_pal %0  # bugchk\n\t"				\
		".long %1\n\t.8byte %2"					\
		: : "i"(PAL_bugchk), "i"(__LINE__), "i"(__FILE__));	\
	unreachable();							\
  } while (0)

#define HAVE_ARCH_BUG
#endif

#include <asm-generic/bug.h>

#endif
