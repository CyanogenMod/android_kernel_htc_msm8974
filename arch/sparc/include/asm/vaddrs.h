#ifndef _SPARC_VADDRS_H
#define _SPARC_VADDRS_H

#include <asm/head.h>

/*
 * asm/vaddrs.h:  Here we define the virtual addresses at
 *                      which important things will be mapped.
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 * Copyright (C) 2000 Anton Blanchard (anton@samba.org)
 */

#define SRMMU_MAXMEM		0x0c000000

#define SRMMU_NOCACHE_VADDR	(KERNBASE + SRMMU_MAXMEM)
				
#define SRMMU_MIN_NOCACHE_PAGES (550)
#define SRMMU_MAX_NOCACHE_PAGES	(1280)

#define SRMMU_NOCACHE_ALCRATIO	64	

#define SUN4M_IOBASE_VADDR	0xfd000000 
#define IOBASE_VADDR		0xfe000000
#define IOBASE_END		0xfe600000

#define SUN4C_LOCK_VADDR	0xff000000
#define SUN4C_LOCK_END		0xffc00000

#define KADB_DEBUGGER_BEGVM	0xffc00000 
#define KADB_DEBUGGER_ENDVM	0xffd00000
#define DEBUG_FIRSTVADDR	KADB_DEBUGGER_BEGVM
#define DEBUG_LASTVADDR		KADB_DEBUGGER_ENDVM

#define LINUX_OPPROM_BEGVM	0xffd00000
#define LINUX_OPPROM_ENDVM	0xfff00000

#define DVMA_VADDR		0xfff00000 
#define DVMA_END		0xfffc0000

#endif 
