/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Definitions for the interrupt related bits in the I/O ASIC
 * interrupt status register (and the interrupt mask register, of course)
 *
 * Created with Information from:
 *
 * "DEC 3000 300/400/500/600/700/800/900 AXP Models System Programmer's Manual"
 *
 * and the Mach Sources
 *
 * Copyright (C) 199x  the Anonymous
 * Copyright (C) 2002  Maciej W. Rozycki
 */

#ifndef __ASM_DEC_IOASIC_INTS_H
#define __ASM_DEC_IOASIC_INTS_H

					
#define IO_INR_SCC0A_TXDMA	31	
#define IO_INR_SCC0A_TXERR	30	
#define IO_INR_SCC0A_RXDMA	29	
#define IO_INR_SCC0A_RXERR	28	
#define IO_INR_ASC_DMA		19	
#define IO_INR_ASC_ERR		18	
#define IO_INR_ASC_MERR		17	
#define IO_INR_LANCE_MERR	16	

					
#define IO_INR_SCC1A_TXDMA	27	
#define IO_INR_SCC1A_TXERR	26	
#define IO_INR_SCC1A_RXDMA	25	
#define IO_INR_SCC1A_RXERR	24	
#define IO_INR_RES_23		23	
#define IO_INR_RES_22		22	
#define IO_INR_RES_21		21	
#define IO_INR_RES_20		20	

					
#define IO_INR_AB_TXDMA		27	
#define IO_INR_AB_TXERR		26	
#define IO_INR_AB_RXDMA		25	
#define IO_INR_AB_RXERR		24	
#define IO_INR_FLOPPY_ERR	23	
#define IO_INR_ISDN_TXDMA	22	
#define IO_INR_ISDN_RXDMA	21	
#define IO_INR_ISDN_ERR		20	

#define IO_INR_DMA		16	



#define IO_IRQ_BASE		8	
#define IO_IRQ_LINES		32	

#define IO_IRQ_NR(n)		((n) + IO_IRQ_BASE)
#define IO_IRQ_MASK(n)		(1 << (n))
#define IO_IRQ_ALL		0x0000ffff
#define IO_IRQ_DMA		0xffff0000

#endif 
