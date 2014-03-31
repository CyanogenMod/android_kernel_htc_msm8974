/* $Id: cache.h,v 1.6 2004/03/11 18:08:05 lethal Exp $
 *
 * include/asm-sh/cache.h
 *
 * Copyright 1999 (C) Niibe Yutaka
 * Copyright 2002, 2003 (C) Paul Mundt
 */
#ifndef __ASM_SH_CACHE_H
#define __ASM_SH_CACHE_H
#ifdef __KERNEL__

#include <linux/init.h>
#include <cpu/cache.h>

#define L1_CACHE_BYTES		(1 << L1_CACHE_SHIFT)

#define __read_mostly __attribute__((__section__(".data..read_mostly")))

#ifndef __ASSEMBLY__
struct cache_info {
	unsigned int ways;		
	unsigned int sets;		
	unsigned int linesz;		

	unsigned int way_size;		

	unsigned int way_incr;
	unsigned int entry_shift;
	unsigned int entry_mask;

	unsigned int alias_mask;
	unsigned int n_aliases;		

	unsigned long flags;
};
#endif 
#endif 
#endif 
