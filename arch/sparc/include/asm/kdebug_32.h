/*
 * kdebug.h:  Defines and definitions for debugging the Linux kernel
 *            under various kernel debuggers.
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 */
#ifndef _SPARC_KDEBUG_H
#define _SPARC_KDEBUG_H

#include <asm/openprom.h>
#include <asm/vaddrs.h>


#define DEBUG_BP_TRAP     126

#ifndef __ASSEMBLY__

typedef unsigned int (*debugger_funct)(void);

struct kernel_debug {
	unsigned long kdebug_entry;
	unsigned long kdebug_trapme;   
	unsigned long *kdebug_stolen_pages;
	debugger_funct teach_debugger;
}; 

extern struct kernel_debug *linux_dbvec;

static inline void sp_enter_debugger(void)
{
	__asm__ __volatile__("jmpl %0, %%o7\n\t"
			     "nop\n\t" : :
			     "r" (linux_dbvec) : "o7", "memory");
}

#define SP_ENTER_DEBUGGER do { \
	     if((linux_dbvec!=0) && ((*(short *)linux_dbvec)!=-1)) \
	       sp_enter_debugger(); \
		       } while(0)

enum die_val {
	DIE_UNUSED,
	DIE_OOPS,
};

#endif 

#define KDEBUG_ENTRY_OFF    0x0
#define KDEBUG_DUNNO_OFF    0x4
#define KDEBUG_DUNNO2_OFF   0x8
#define KDEBUG_TEACH_OFF    0xc

#endif 
