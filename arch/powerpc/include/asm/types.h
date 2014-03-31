#ifndef _ASM_POWERPC_TYPES_H
#define _ASM_POWERPC_TYPES_H

#if !defined(__SANE_USERSPACE_TYPES__) && defined(__powerpc64__) && !defined(__KERNEL__)
# include <asm-generic/int-l64.h>
#else
# include <asm-generic/int-ll64.h>
#endif

#ifndef __ASSEMBLY__

/*
 * This file is never included by application software unless
 * explicitly requested (e.g., via linux/types.h) in which case the
 * application is Linux specific so (user-) name space pollution is
 * not a major issue.  However, for interoperability, libraries still
 * need to be careful to avoid a name clashes.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

typedef struct {
	__u32 u[4];
} __attribute__((aligned(16))) __vector128;

#endif 

#ifdef __KERNEL__
#ifndef __ASSEMBLY__

typedef __vector128 vector128;

typedef struct {
	unsigned long entry;
	unsigned long toc;
	unsigned long env;
} func_descr_t;

#endif 

#endif 

#endif 
