/*
 * arch/arm/mach-sa1100/include/mach/memory.h
 *
 * Copyright (C) 1999-2000 Nicolas Pitre <nico@fluxnic.net>
 */

#ifndef __ASM_ARCH_MEMORY_H
#define __ASM_ARCH_MEMORY_H

#include <asm/sizes.h>

#define PLAT_PHYS_OFFSET	UL(0xc0000000)

#define MAX_PHYSMEM_BITS	32
#define SECTION_SIZE_BITS	27

#define FLUSH_BASE_PHYS		0xe0000000
#define FLUSH_BASE		0xf5000000
#define FLUSH_BASE_MINICACHE	0xf5100000

#endif
