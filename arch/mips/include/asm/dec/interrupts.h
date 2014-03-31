/*
 * Miscellaneous definitions used to initialise the interrupt vector table
 * with the machine-specific interrupt routines.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1997 by Paul M. Antoine.
 * reworked 1998 by Harald Koerfgen.
 * Copyright (C) 2001, 2002, 2003  Maciej W. Rozycki
 */

#ifndef __ASM_DEC_INTERRUPTS_H
#define __ASM_DEC_INTERRUPTS_H

#include <irq.h>
#include <asm/mipsregs.h>


#define DEC_IRQ_CASCADE		0	

#define DEC_IRQ_AB_RECV		1	
#define DEC_IRQ_AB_XMIT		2	
#define DEC_IRQ_DZ11		3	
#define DEC_IRQ_ASC		4	
#define DEC_IRQ_FLOPPY		5	
#define DEC_IRQ_FPU		6	
#define DEC_IRQ_HALT		7	
#define DEC_IRQ_ISDN		8	
#define DEC_IRQ_LANCE		9	
#define DEC_IRQ_BUS		10	
#define DEC_IRQ_PSU		11	
#define DEC_IRQ_RTC		12	
#define DEC_IRQ_SCC0		13	
#define DEC_IRQ_SCC1		14	
#define DEC_IRQ_SII		15	
#define DEC_IRQ_TC0		16	
#define DEC_IRQ_TC1		17	
#define DEC_IRQ_TC2		18	
#define DEC_IRQ_TIMER		19	
#define DEC_IRQ_VIDEO		20	

#define DEC_IRQ_ASC_MERR	21	
#define DEC_IRQ_ASC_ERR		22	
#define DEC_IRQ_ASC_DMA		23	
#define DEC_IRQ_FLOPPY_ERR	24	
#define DEC_IRQ_ISDN_ERR	25	
#define DEC_IRQ_ISDN_RXDMA	26	
#define DEC_IRQ_ISDN_TXDMA	27	
#define DEC_IRQ_LANCE_MERR	28	
#define DEC_IRQ_SCC0A_RXERR	29	
#define DEC_IRQ_SCC0A_RXDMA	30	
#define DEC_IRQ_SCC0A_TXERR	31	
#define DEC_IRQ_SCC0A_TXDMA	32	
#define DEC_IRQ_AB_RXERR	33	
#define DEC_IRQ_AB_RXDMA	34	
#define DEC_IRQ_AB_TXERR	35	
#define DEC_IRQ_AB_TXDMA	36	
#define DEC_IRQ_SCC1A_RXERR	37	
#define DEC_IRQ_SCC1A_RXDMA	38	
#define DEC_IRQ_SCC1A_TXERR	39	
#define DEC_IRQ_SCC1A_TXDMA	40	

#define DEC_IRQ_TC5		DEC_IRQ_ASC	
#define DEC_IRQ_TC6		DEC_IRQ_LANCE	

#define DEC_NR_INTS		41


#define DEC_MAX_CPU_INTS	6
#define DEC_MAX_ASIC_INTS	9


#define DEC_CPU_INR_FPU		7	
#define DEC_CPU_INR_SW1		1	
#define DEC_CPU_INR_SW0		0	

#define DEC_CPU_IRQ_BASE	MIPS_CPU_IRQ_BASE	

#define DEC_CPU_IRQ_NR(n)	((n) + DEC_CPU_IRQ_BASE)
#define DEC_CPU_IRQ_MASK(n)	(1 << ((n) + CAUSEB_IP))
#define DEC_CPU_IRQ_ALL		(0xff << CAUSEB_IP)


#ifndef __ASSEMBLY__

typedef union { int i; void *p; } int_ptr;
extern int dec_interrupt[DEC_NR_INTS];
extern int_ptr cpu_mask_nr_tbl[DEC_MAX_CPU_INTS][2];
extern int_ptr asic_mask_nr_tbl[DEC_MAX_ASIC_INTS][2];
extern int cpu_fpu_mask;


extern void kn02_io_int(void);
extern void kn02xa_io_int(void);
extern void kn03_io_int(void);
extern void asic_dma_int(void);
extern void asic_all_int(void);
extern void kn02_all_int(void);
extern void cpu_all_int(void);

extern void dec_intr_unimplemented(void);
extern void asic_intr_unimplemented(void);

#endif 

#endif
