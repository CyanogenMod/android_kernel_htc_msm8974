/*
 * include/asm-sh/cpu-sh4/cache.h
 *
 * Copyright (C) 1999 Niibe Yutaka
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#ifndef __ASM_CPU_SH4_CACHE_H
#define __ASM_CPU_SH4_CACHE_H

#define L1_CACHE_SHIFT	5

#define SH_CACHE_VALID		1
#define SH_CACHE_UPDATED	2
#define SH_CACHE_COMBINED	4
#define SH_CACHE_ASSOC		8

#define CCR		0xff00001c	
#define CCR_CACHE_OCE	0x0001	
#define CCR_CACHE_WT	0x0002	
#define CCR_CACHE_CB	0x0004	
#define CCR_CACHE_OCI	0x0008	
#define CCR_CACHE_ORA	0x0020	
#define CCR_CACHE_OIX	0x0080	
#define CCR_CACHE_ICE	0x0100	
#define CCR_CACHE_ICI	0x0800	
#define CCR_CACHE_IIX	0x8000	
#ifndef CONFIG_CPU_SH4A
#define CCR_CACHE_EMODE	0x80000000	
#endif

#define CCR_CACHE_ENABLE	(CCR_CACHE_OCE|CCR_CACHE_ICE)
#define CCR_CACHE_INVALIDATE	(CCR_CACHE_OCI|CCR_CACHE_ICI)

#define CACHE_IC_ADDRESS_ARRAY	0xf0000000
#define CACHE_OC_ADDRESS_ARRAY	0xf4000000

#define RAMCR			0xFF000074

#endif 

