/*
 * linux/arch/$(ARCH)/platform/$(PLATFORM)/ints.c
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 2000  Michael Leslie <mleslie@lineo.com>
 * Copyright (c) 1996 Roman Zippel
 * Copyright (c) 1999 D. Jeff Dionne <jeff@uclinux.org>
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <asm/traps.h>
#include <asm/machdep.h>
#include <asm/m68360.h>

extern QUICC *pquicc;
extern void cpm_interrupt_init(void);

#define INTERNAL_IRQS (96)

asmlinkage void system_call(void);
asmlinkage void buserr(void);
asmlinkage void trap(void);
asmlinkage void bad_interrupt(void);
asmlinkage void inthandler(void);

static void intc_irq_unmask(struct irq_data *d)
{
	pquicc->intr_cimr |= (1 << d->irq);
}

static void intc_irq_mask(struct irq_data *d)
{
	pquicc->intr_cimr &= ~(1 << d->irq);
}

static void intc_irq_ack(struct irq_data *d)
{
	pquicc->intr_cisr = (1 << d->irq);
}

static struct irq_chip intc_irq_chip = {
	.name		= "M68K-INTC",
	.irq_mask	= intc_irq_mask,
	.irq_unmask	= intc_irq_unmask,
	.irq_ack	= intc_irq_ack,
};

void __init trap_init(void)
{
	int vba = (CPM_VECTOR_BASE<<4);

	
	_ramvec[2] = buserr;
	_ramvec[3] = trap;
	_ramvec[4] = trap;
	_ramvec[5] = trap;
	_ramvec[6] = trap;
	_ramvec[7] = trap;
	_ramvec[8] = trap;
	_ramvec[9] = trap;
	_ramvec[10] = trap;
	_ramvec[11] = trap;
	_ramvec[12] = trap;
	_ramvec[13] = trap;
	_ramvec[14] = trap;
	_ramvec[15] = trap;

	_ramvec[32] = system_call;
	_ramvec[33] = trap;

	cpm_interrupt_init();

	
	
	pquicc->intr_cicr = 0x00e49f00 | vba;

	
	_ramvec[vba+CPMVEC_ERROR]       = bad_interrupt; 
	_ramvec[vba+CPMVEC_PIO_PC11]    = inthandler;   
	_ramvec[vba+CPMVEC_PIO_PC10]    = inthandler;   
	_ramvec[vba+CPMVEC_SMC2]        = inthandler;   
	_ramvec[vba+CPMVEC_SMC1]        = inthandler;   
	_ramvec[vba+CPMVEC_SPI]         = inthandler;   
	_ramvec[vba+CPMVEC_PIO_PC9]     = inthandler;   
	_ramvec[vba+CPMVEC_TIMER4]      = inthandler;   
	_ramvec[vba+CPMVEC_RESERVED1]   = inthandler;   
	_ramvec[vba+CPMVEC_PIO_PC8]     = inthandler;   
	_ramvec[vba+CPMVEC_PIO_PC7]     = inthandler;  
	_ramvec[vba+CPMVEC_PIO_PC6]     = inthandler;  
	_ramvec[vba+CPMVEC_TIMER3]      = inthandler;  
	_ramvec[vba+CPMVEC_PIO_PC5]     = inthandler;  
	_ramvec[vba+CPMVEC_PIO_PC4]     = inthandler;  
	_ramvec[vba+CPMVEC_RESERVED2]   = inthandler;  
	_ramvec[vba+CPMVEC_RISCTIMER]   = inthandler;  
	_ramvec[vba+CPMVEC_TIMER2]      = inthandler;  
	_ramvec[vba+CPMVEC_RESERVED3]   = inthandler;  
	_ramvec[vba+CPMVEC_IDMA2]       = inthandler;  
	_ramvec[vba+CPMVEC_IDMA1]       = inthandler;  
	_ramvec[vba+CPMVEC_SDMA_CB_ERR] = inthandler;  
	_ramvec[vba+CPMVEC_PIO_PC3]     = inthandler;  
	_ramvec[vba+CPMVEC_PIO_PC2]     = inthandler;  
	  
	_ramvec[vba+CPMVEC_TIMER1]      = inthandler;  
	_ramvec[vba+CPMVEC_PIO_PC1]     = inthandler;  
	_ramvec[vba+CPMVEC_SCC4]        = inthandler;  
	_ramvec[vba+CPMVEC_SCC3]        = inthandler;  
	_ramvec[vba+CPMVEC_SCC2]        = inthandler;  
	_ramvec[vba+CPMVEC_SCC1]        = inthandler;  
	_ramvec[vba+CPMVEC_PIO_PC0]     = inthandler;  


	
	pquicc->intr_cimr = 0x00000000;
}

void init_IRQ(void)
{
	int i;

	for (i = 0; (i < NR_IRQS); i++) {
		irq_set_chip(i, &intc_irq_chip);
		irq_set_handler(i, handle_level_irq);
	}
}

