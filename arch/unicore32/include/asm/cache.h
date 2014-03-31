/*
 * linux/arch/unicore32/include/asm/cache.h
 *
 * Code specific to PKUnity SoC and UniCore ISA
 *
 * Copyright (C) 2001-2010 GUAN Xue-tao
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __UNICORE_CACHE_H__
#define __UNICORE_CACHE_H__

#define L1_CACHE_SHIFT		(5)
#define L1_CACHE_BYTES		(1 << L1_CACHE_SHIFT)

#define ARCH_DMA_MINALIGN	L1_CACHE_BYTES

#endif
