/* MN10300 ISA DMA handlers and definitions
 *
 * Copyright (C) 2007 Matsushita Electric Industrial Co., Ltd.
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */
#ifndef _ASM_DMA_H
#define _ASM_DMA_H

#include <linux/spinlock.h>
#include <asm/io.h>
#include <linux/delay.h>

#undef MAX_DMA_CHANNELS		
#define MAX_DMA_ADDRESS		0xbfffffff

extern spinlock_t dma_spin_lock;

static inline unsigned long claim_dma_lock(void)
{
	unsigned long flags;
	spin_lock_irqsave(&dma_spin_lock, flags);
	return flags;
}

static inline void release_dma_lock(unsigned long flags)
{
	spin_unlock_irqrestore(&dma_spin_lock, flags);
}

static inline void enable_dma(unsigned int dmanr)
{
}

static inline void disable_dma(unsigned int dmanr)
{
}

static inline void clear_dma_ff(unsigned int dmanr)
{
}

static inline void set_dma_mode(unsigned int dmanr, char mode)
{
}

static inline void set_dma_page(unsigned int dmanr, char pagenr)
{
}


static inline void set_dma_addr(unsigned int dmanr, unsigned int a)
{
}


static inline void set_dma_count(unsigned int dmanr, unsigned int count)
{
}


static inline int get_dma_residue(unsigned int dmanr)
{
	return 0;
}


extern int request_dma(unsigned int dmanr, const char *device_id);
extern void free_dma(unsigned int dmanr);


#ifdef CONFIG_PCI
extern int isa_dma_bridge_buggy;
#else
#define isa_dma_bridge_buggy 	(0)
#endif

#endif 
