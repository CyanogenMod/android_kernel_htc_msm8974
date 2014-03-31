/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1994, 1995, 1996 by Ralf Baechle
 */
#ifndef	_ASM_CACHECTL
#define	_ASM_CACHECTL

#define	ICACHE	(1<<0)		
#define	DCACHE	(1<<1)		
#define	BCACHE	(ICACHE|DCACHE)	

#define CACHEABLE	0	
#define UNCACHEABLE	1	

#endif	
