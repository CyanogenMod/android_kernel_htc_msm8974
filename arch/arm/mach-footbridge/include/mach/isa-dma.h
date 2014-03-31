/*
 *  arch/arm/mach-footbridge/include/mach/isa-dma.h
 *
 *  Architecture DMA routines
 *
 *  Copyright (C) 1998,1999 Russell King
 *  Copyright (C) 1998,1999 Philip Blundell
 */
#ifndef __ASM_ARCH_DMA_H
#define __ASM_ARCH_DMA_H

#define _ISA_DMA(x)		(0+(x))
#define _DC21285_DMA(x)		(8+(x))

#define MAX_DMA_CHANNELS	10

#define DMA_FLOPPY		_ISA_DMA(2)
#define DMA_ISA_CASCADE		_ISA_DMA(4)

#endif 
