/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1999 by Kaz Kojima
 *
 * Defitions for the address spaces of the SH CPUs.
 */
#ifndef __ASM_SH_ADDRSPACE_H
#define __ASM_SH_ADDRSPACE_H

#ifdef __KERNEL__

#include <cpu/addrspace.h>

#ifdef P1SEG


#define PXSEG(a)	(((unsigned long)(a)) & 0xe0000000)

#ifdef CONFIG_29BIT
#define P1SEGADDR(a)	\
	((__typeof__(a))(((unsigned long)(a) & 0x1fffffff) | P1SEG))
#define P2SEGADDR(a)	\
	((__typeof__(a))(((unsigned long)(a) & 0x1fffffff) | P2SEG))
#define P3SEGADDR(a)	\
	((__typeof__(a))(((unsigned long)(a) & 0x1fffffff) | P3SEG))
#define P4SEGADDR(a)	\
	((__typeof__(a))(((unsigned long)(a) & 0x1fffffff) | P4SEG))
#else
#define P1SEGADDR(a)	({ (void)(a); BUG(); NULL; })
#define P2SEGADDR(a)	({ (void)(a); BUG(); NULL; })
#define P3SEGADDR(a)	({ (void)(a); BUG(); NULL; })
#define P4SEGADDR(a)	({ (void)(a); BUG(); NULL; })
#endif
#endif 

#define IS_29BIT(a)	(((unsigned long)(a)) < 0x20000000)

#ifdef CONFIG_SH_STORE_QUEUES
#define P3_ADDR_MAX		(P4SEG_STORE_QUE + 0x04000000)
#else
#define P3_ADDR_MAX		P4SEG
#endif

#endif 
#endif 
