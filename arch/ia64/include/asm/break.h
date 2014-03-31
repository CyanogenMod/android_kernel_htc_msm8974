#ifndef _ASM_IA64_BREAK_H
#define _ASM_IA64_BREAK_H

/*
 * IA-64 Linux break numbers.
 *
 * Copyright (C) 1999 Hewlett-Packard Co
 * Copyright (C) 1999 David Mosberger-Tang <davidm@hpl.hp.com>
 */

#define __IA64_BREAK_KDB		0x80100
#define __IA64_BREAK_KPROBE		0x81000 
#define __IA64_BREAK_JPROBE		0x82000

#define __IA64_BREAK_SYSCALL		0x100000

#define __IA64_XEN_HYPERCALL		0x1000
#define __IA64_XEN_HYPERPRIVOP_START	0x1
#define __IA64_XEN_HYPERPRIVOP_MAX	0x1a

#endif 
