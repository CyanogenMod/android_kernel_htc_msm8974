/*
 * Copyright (C) 2008-2009 Michal Simek <monstr@monstr.eu>
 * Copyright (C) 2008-2009 PetaLogix
 * Copyright (C) 2006 Atmark Techno, Inc.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#ifndef _ASM_MICROBLAZE_MMU_H
#define _ASM_MICROBLAZE_MMU_H

# ifndef CONFIG_MMU
#  include <asm-generic/mmu.h>
# else 
#  ifdef __KERNEL__
#   ifndef __ASSEMBLY__

typedef unsigned long mm_context_t;

typedef struct _PTE {
	unsigned long    v:1;	
	unsigned long vsid:24;	
	unsigned long    h:1;	
	unsigned long  api:6;	
	unsigned long  rpn:20;	
	unsigned long     :3;	
	unsigned long    r:1;	
	unsigned long    c:1;	
	unsigned long    w:1;	
	unsigned long    i:1;	
	unsigned long    m:1;	
	unsigned long    g:1;	
	unsigned long     :1;	
	unsigned long   pp:2;	
} PTE;

#  define PP_RWXX	0 
#  define PP_RWRX	1 
#  define PP_RWRW	2 
#  define PP_RXRX	3 

typedef struct _SEGREG {
	unsigned long    t:1;	
	unsigned long   ks:1;	
	unsigned long   kp:1;	
	unsigned long    n:1;	
	unsigned long     :4;	
	unsigned long vsid:24;	
} SEGREG;

extern void _tlbie(unsigned long va);	
extern void _tlbia(void);		

extern u32 tlb_skip;
#   endif 


#  define MICROBLAZE_TLB_SIZE 64

#  define MICROBLAZE_TLB_SKIP 0

#  define MICROBLAZE_LMB_TLB_ID 63


#  define TLB_LO		1
#  define TLB_HI		0
#  define TLB_DATA		TLB_LO
#  define TLB_TAG		TLB_HI

#  define TLB_EPN_MASK		0xFFFFFC00 
#  define TLB_PAGESZ_MASK	0x00000380
#  define TLB_PAGESZ(x)		(((x) & 0x7) << 7)
#  define PAGESZ_1K		0
#  define PAGESZ_4K		1
#  define PAGESZ_16K		2
#  define PAGESZ_64K		3
#  define PAGESZ_256K		4
#  define PAGESZ_1M		5
#  define PAGESZ_4M		6
#  define PAGESZ_16M		7
#  define TLB_VALID		0x00000040 

#  define TLB_RPN_MASK		0xFFFFFC00 
#  define TLB_PERM_MASK		0x00000300
#  define TLB_EX		0x00000200 
#  define TLB_WR		0x00000100 
#  define TLB_ZSEL_MASK		0x000000F0
#  define TLB_ZSEL(x)		(((x) & 0xF) << 4)
#  define TLB_ATTR_MASK		0x0000000F
#  define TLB_W			0x00000008 
#  define TLB_I			0x00000004 
#  define TLB_M			0x00000002 
#  define TLB_G			0x00000001 

#  endif 
# endif 
#endif 
