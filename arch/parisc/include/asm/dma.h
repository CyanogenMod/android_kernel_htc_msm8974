/* asm/dma.h: Defines for using and allocating dma channels.
 * Written by Hennus Bergman, 1992.
 * High DMA channel support & info by Hannu Savolainen
 * and John Boyd, Nov. 1992.
 * (c) Copyright 2000, Grant Grundler
 */

#ifndef _ASM_DMA_H
#define _ASM_DMA_H

#include <asm/io.h>		

#define dma_outb	outb
#define dma_inb		inb

#define DMA_CHUNK_SIZE	(BITS_PER_LONG*PAGE_SIZE)

#define MAX_DMA_ADDRESS (~0UL)

#define MAX_DMA_CHANNELS 8
#define DMA_MODE_READ	0x44	
#define DMA_MODE_WRITE	0x48	
#define DMA_MODE_CASCADE 0xC0	

#define DMA_AUTOINIT	0x10

#define IO_DMA1_BASE	0x00	
#define IO_DMA2_BASE	0xC0	

#define DMA1_CMD_REG		0x08	
#define DMA1_STAT_REG		0x08	
#define DMA1_REQ_REG            0x09    
#define DMA1_MASK_REG		0x0A	
#define DMA1_MODE_REG		0x0B	
#define DMA1_CLEAR_FF_REG	0x0C	
#define DMA1_TEMP_REG           0x0D    
#define DMA1_RESET_REG		0x0D	
#define DMA1_CLR_MASK_REG       0x0E    
#define DMA1_MASK_ALL_REG       0x0F    
#define DMA1_EXT_MODE_REG	(0x400 | DMA1_MODE_REG)

#define DMA2_CMD_REG		0xD0	
#define DMA2_STAT_REG		0xD0	
#define DMA2_REQ_REG            0xD2    
#define DMA2_MASK_REG		0xD4	
#define DMA2_MODE_REG		0xD6	
#define DMA2_CLEAR_FF_REG	0xD8	
#define DMA2_TEMP_REG           0xDA    
#define DMA2_RESET_REG		0xDA	
#define DMA2_CLR_MASK_REG       0xDC    
#define DMA2_MASK_ALL_REG       0xDE    
#define DMA2_EXT_MODE_REG	(0x400 | DMA2_MODE_REG)

static __inline__ unsigned long claim_dma_lock(void)
{
	return 0;
}

static __inline__ void release_dma_lock(unsigned long flags)
{
}


static __inline__ int get_dma_residue(unsigned int dmanr)
{
	unsigned int io_port = (dmanr<=3)? ((dmanr&3)<<1) + 1 + IO_DMA1_BASE
					 : ((dmanr&3)<<2) + 2 + IO_DMA2_BASE;

	
	unsigned short count;

	count = 1 + dma_inb(io_port);
	count += dma_inb(io_port) << 8;
	
	return (dmanr<=3)? count : (count<<1);
}

static __inline__ void enable_dma(unsigned int dmanr)
{
#ifdef CONFIG_SUPERIO
	if (dmanr<=3)
		dma_outb(dmanr,  DMA1_MASK_REG);
	else
		dma_outb(dmanr & 3,  DMA2_MASK_REG);
#endif
}

static __inline__ void disable_dma(unsigned int dmanr)
{
#ifdef CONFIG_SUPERIO
	if (dmanr<=3)
		dma_outb(dmanr | 4,  DMA1_MASK_REG);
	else
		dma_outb((dmanr & 3) | 4,  DMA2_MASK_REG);
#endif
}

#define request_dma(dmanr, device_id)	(0)

static __inline__ void clear_dma_ff(unsigned int dmanr)
{
}

static __inline__ void set_dma_mode(unsigned int dmanr, char mode)
{
}

static __inline__ void set_dma_page(unsigned int dmanr, char pagenr)
{
}


static __inline__ void set_dma_addr(unsigned int dmanr, unsigned int a)
{
}


static __inline__ void set_dma_count(unsigned int dmanr, unsigned int count)
{
}


#define free_dma(dmanr)

#ifdef CONFIG_PCI
extern int isa_dma_bridge_buggy;
#else
#define isa_dma_bridge_buggy 	(0)
#endif

#endif 
