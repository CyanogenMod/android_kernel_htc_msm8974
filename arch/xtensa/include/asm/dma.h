/*
 * include/asm-xtensa/dma.h
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2003 - 2005 Tensilica Inc.
 */

#ifndef _XTENSA_DMA_H
#define _XTENSA_DMA_H

#include <asm/io.h>		

#define MAX_DMA_CHANNELS	8


#ifndef MAX_DMA_ADDRESS
#define MAX_DMA_ADDRESS		(PAGE_OFFSET + XCHAL_KIO_SIZE - 1)
#endif

extern int request_dma(unsigned int dmanr, const char * device_id);
extern void free_dma(unsigned int dmanr);

#ifdef CONFIG_PCI
extern int isa_dma_bridge_buggy;
#else
#define isa_dma_bridge_buggy 	(0)
#endif


#endif
