#ifndef _ASM_IA64_CACHE_H
#define _ASM_IA64_CACHE_H


/*
 * Copyright (C) 1998-2000 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 */

#define L1_CACHE_SHIFT		CONFIG_IA64_L1_CACHE_SHIFT
#define L1_CACHE_BYTES		(1 << L1_CACHE_SHIFT)

#ifdef CONFIG_SMP
# define SMP_CACHE_SHIFT	L1_CACHE_SHIFT
# define SMP_CACHE_BYTES	L1_CACHE_BYTES
#else
# define SMP_CACHE_SHIFT	3
# define SMP_CACHE_BYTES	(1 << 3)
#endif

#define __read_mostly __attribute__((__section__(".data..read_mostly")))

#endif 
