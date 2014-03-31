/*
 *  arch/arm/mach-rpc/include/mach/memory.h
 *
 *  Copyright (C) 1996,1997,1998 Russell King.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Changelog:
 *   20-Oct-1996 RMK	Created
 *   31-Dec-1997 RMK	Fixed definitions to reduce warnings
 *   11-Jan-1998 RMK	Uninlined to reduce hits on cache
 *   08-Feb-1998 RMK	Added __virt_to_bus and __bus_to_virt
 *   21-Mar-1999 RMK	Renamed to memory.h
 *		 RMK	Added TASK_SIZE and PAGE_OFFSET
 */
#ifndef __ASM_ARCH_MEMORY_H
#define __ASM_ARCH_MEMORY_H

#define PLAT_PHYS_OFFSET	UL(0x10000000)

#define FLUSH_BASE_PHYS		0x00000000
#define FLUSH_BASE		0xdf000000

#define MAX_PHYSMEM_BITS	29
#define SECTION_SIZE_BITS	26

#endif
