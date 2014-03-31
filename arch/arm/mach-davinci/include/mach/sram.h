/*
 * mach/sram.h - DaVinci simple SRAM allocator
 *
 * Copyright (C) 2009 David Brownell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __MACH_SRAM_H
#define __MACH_SRAM_H

#define SRAM_GRANULARITY	512

extern void *sram_alloc(size_t len, dma_addr_t *dma);
extern void sram_free(void *addr, size_t len);

#endif 
