#ifndef __ASM_CRISv10_ARCH_BUG_H
#define __ASM_CRISv10_ARCH_BUG_H

#include <linux/stringify.h>

#ifdef CONFIG_BUG
#ifdef CONFIG_DEBUG_BUGVERBOSE

#define BUG_PREFIX 0x0D7F
#define BUG_MAGIC  0x00001234

struct bug_frame {
	unsigned short prefix;
	unsigned int magic;
	unsigned short clear;
	unsigned short movu;
	unsigned short line;
	unsigned short jump;
	unsigned char *filename;
};

#if 0
#define BUG()								\
	__asm__ __volatile__ ("clear.d [" __stringify(BUG_MAGIC) "]\n\t"\
				"movu.w %0,$r0\n\t"			\
				"jump %1\n\t"				\
				: : "i" (__LINE__), "i" (__FILE__))
#else
#define BUG()								\
	__asm__ __volatile__ ("clear.d [" __stringify(BUG_MAGIC) "]\n\t"\
			      "movu.w " __stringify(__LINE__) ",$r0\n\t"\
			      "jump 0f\n\t"				\
			      ".section .rodata\n"			\
			      "0:\t.string \"" __FILE__ "\"\n\t"	\
			      ".previous")
#endif

#else

#define BUG() (*(int *)0 = 0)

#endif

#define HAVE_ARCH_BUG
#endif

#include <asm-generic/bug.h>

#endif
