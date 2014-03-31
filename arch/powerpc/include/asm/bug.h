#ifndef _ASM_POWERPC_BUG_H
#define _ASM_POWERPC_BUG_H
#ifdef __KERNEL__

#include <asm/asm-compat.h>

#define BUG_OPCODE .long 0x00b00b00  
#define BUG_ILLEGAL_INSTR "0x00b00b00" 

#ifdef CONFIG_BUG

#ifdef __ASSEMBLY__
#include <asm/asm-offsets.h>
#ifdef CONFIG_DEBUG_BUGVERBOSE
.macro EMIT_BUG_ENTRY addr,file,line,flags
	 .section __bug_table,"a"
5001:	 PPC_LONG \addr, 5002f
	 .short \line, \flags
	 .org 5001b+BUG_ENTRY_SIZE
	 .previous
	 .section .rodata,"a"
5002:	 .asciz "\file"
	 .previous
.endm
#else
.macro EMIT_BUG_ENTRY addr,file,line,flags
	 .section __bug_table,"a"
5001:	 PPC_LONG \addr
	 .short \flags
	 .org 5001b+BUG_ENTRY_SIZE
	 .previous
.endm
#endif 

#else 
#ifdef CONFIG_DEBUG_BUGVERBOSE
#define _EMIT_BUG_ENTRY				\
	".section __bug_table,\"a\"\n"		\
	"2:\t" PPC_LONG "1b, %0\n"		\
	"\t.short %1, %2\n"			\
	".org 2b+%3\n"				\
	".previous\n"
#else
#define _EMIT_BUG_ENTRY				\
	".section __bug_table,\"a\"\n"		\
	"2:\t" PPC_LONG "1b\n"			\
	"\t.short %2\n"				\
	".org 2b+%3\n"				\
	".previous\n"
#endif


#define BUG() do {						\
	__asm__ __volatile__(					\
		"1:	twi 31,0,0\n"				\
		_EMIT_BUG_ENTRY					\
		: : "i" (__FILE__), "i" (__LINE__),		\
		    "i" (0), "i"  (sizeof(struct bug_entry)));	\
	unreachable();						\
} while (0)

#define BUG_ON(x) do {						\
	if (__builtin_constant_p(x)) {				\
		if (x)						\
			BUG();					\
	} else {						\
		__asm__ __volatile__(				\
		"1:	"PPC_TLNEI"	%4,0\n"			\
		_EMIT_BUG_ENTRY					\
		: : "i" (__FILE__), "i" (__LINE__), "i" (0),	\
		  "i" (sizeof(struct bug_entry)),		\
		  "r" ((__force long)(x)));			\
	}							\
} while (0)

#define __WARN_TAINT(taint) do {				\
	__asm__ __volatile__(					\
		"1:	twi 31,0,0\n"				\
		_EMIT_BUG_ENTRY					\
		: : "i" (__FILE__), "i" (__LINE__),		\
		  "i" (BUGFLAG_TAINT(taint)),			\
		  "i" (sizeof(struct bug_entry)));		\
} while (0)

#define WARN_ON(x) ({						\
	int __ret_warn_on = !!(x);				\
	if (__builtin_constant_p(__ret_warn_on)) {		\
		if (__ret_warn_on)				\
			__WARN();				\
	} else {						\
		__asm__ __volatile__(				\
		"1:	"PPC_TLNEI"	%4,0\n"			\
		_EMIT_BUG_ENTRY					\
		: : "i" (__FILE__), "i" (__LINE__),		\
		  "i" (BUGFLAG_TAINT(TAINT_WARN)),		\
		  "i" (sizeof(struct bug_entry)),		\
		  "r" (__ret_warn_on));				\
	}							\
	unlikely(__ret_warn_on);				\
})

#define HAVE_ARCH_BUG
#define HAVE_ARCH_BUG_ON
#define HAVE_ARCH_WARN_ON
#endif 
#else
#ifdef __ASSEMBLY__
.macro EMIT_BUG_ENTRY addr,file,line,flags
.endm
#else 
#define _EMIT_BUG_ENTRY
#endif
#endif 

#include <asm-generic/bug.h>

#ifndef __ASSEMBLY__

struct pt_regs;
extern int do_page_fault(struct pt_regs *, unsigned long, unsigned long);
extern void bad_page_fault(struct pt_regs *, unsigned long, int);
extern void _exception(int, struct pt_regs *, int, unsigned long);
extern void die(const char *, struct pt_regs *, long);
extern void print_backtrace(unsigned long *);

#endif 

#endif 
#endif 
