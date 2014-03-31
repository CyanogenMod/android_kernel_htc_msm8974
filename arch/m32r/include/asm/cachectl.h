/*
 * cachectl.h -- defines for M32R cache control system calls
 *
 * Copyright (C) 2003 by Kazuhiro Inaoka
 */
#ifndef	__ASM_M32R_CACHECTL
#define	__ASM_M32R_CACHECTL

#define	ICACHE	(1<<0)		
#define	DCACHE	(1<<1)		
#define	BCACHE	(ICACHE|DCACHE)	

#define CACHEABLE	0	
#define UNCACHEABLE	1	

#endif	
