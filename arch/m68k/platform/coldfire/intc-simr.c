/*
 * intc-simr.c
 *
 * Interrupt controller code for the ColdFire 5208, 5207 & 532x parts.
 *
 * (C) Copyright 2009-2011, Greg Ungerer <gerg@snapgear.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <asm/coldfire.h>
#include <asm/mcfsim.h>
#include <asm/traps.h>

#ifdef CONFIG_M520x
#define	EINT0	64	
#define	EINT1	65	
#define	EINT4	66	
#define	EINT7	67	

static unsigned int irqebitmap[] = { 0, 1, 4, 7 };
static unsigned int inline irq2ebit(unsigned int irq)
{
	return irqebitmap[irq - EINT0];
}

#else

#define	EINT0	64	
#define	EINT1	65	
#define	EINT7	71	

static unsigned int inline irq2ebit(unsigned int irq)
{
	return irq - EINT0;
}

#endif


static void intc_irq_mask(struct irq_data *d)
{
	unsigned int irq = d->irq - MCFINT_VECBASE;

	if (MCFINTC1_SIMR && (irq > 64))
		__raw_writeb(irq - 64, MCFINTC1_SIMR);
	else
		__raw_writeb(irq, MCFINTC0_SIMR);
}

static void intc_irq_unmask(struct irq_data *d)
{
	unsigned int irq = d->irq - MCFINT_VECBASE;

	if (MCFINTC1_CIMR && (irq > 64))
		__raw_writeb(irq - 64, MCFINTC1_CIMR);
	else
		__raw_writeb(irq, MCFINTC0_CIMR);
}

static void intc_irq_ack(struct irq_data *d)
{
	unsigned int ebit = irq2ebit(d->irq);

	__raw_writeb(0x1 << ebit, MCFEPORT_EPFR);
}

static unsigned int intc_irq_startup(struct irq_data *d)
{
	unsigned int irq = d->irq;

	if ((irq >= EINT1) && (irq <= EINT7)) {
		unsigned int ebit = irq2ebit(irq);
		u8 v;

		
		v = __raw_readb(MCFEPORT_EPDDR);
		__raw_writeb(v & ~(0x1 << ebit), MCFEPORT_EPDDR);

		
		v = __raw_readb(MCFEPORT_EPIER);
		__raw_writeb(v | (0x1 << ebit), MCFEPORT_EPIER);
	}

	irq -= MCFINT_VECBASE;
	if (MCFINTC1_ICR0 && (irq > 64))
		__raw_writeb(5, MCFINTC1_ICR0 + irq - 64);
	else
		__raw_writeb(5, MCFINTC0_ICR0 + irq);


	intc_irq_unmask(d);
	return 0;
}

static int intc_irq_set_type(struct irq_data *d, unsigned int type)
{
	unsigned int ebit, irq = d->irq;
	u16 pa, tb;

	switch (type) {
	case IRQ_TYPE_EDGE_RISING:
		tb = 0x1;
		break;
	case IRQ_TYPE_EDGE_FALLING:
		tb = 0x2;
		break;
	case IRQ_TYPE_EDGE_BOTH:
		tb = 0x3;
		break;
	default:
		
		tb = 0;
		break;
	}

	if (tb)
		irq_set_handler(irq, handle_edge_irq);

	ebit = irq2ebit(irq) * 2;
	pa = __raw_readw(MCFEPORT_EPPAR);
	pa = (pa & ~(0x3 << ebit)) | (tb << ebit);
	__raw_writew(pa, MCFEPORT_EPPAR);
	
	return 0;
}

static struct irq_chip intc_irq_chip = {
	.name		= "CF-INTC",
	.irq_startup	= intc_irq_startup,
	.irq_mask	= intc_irq_mask,
	.irq_unmask	= intc_irq_unmask,
};

static struct irq_chip intc_irq_chip_edge_port = {
	.name		= "CF-INTC-EP",
	.irq_startup	= intc_irq_startup,
	.irq_mask	= intc_irq_mask,
	.irq_unmask	= intc_irq_unmask,
	.irq_ack	= intc_irq_ack,
	.irq_set_type	= intc_irq_set_type,
};

void __init init_IRQ(void)
{
	int irq, eirq;

	
	__raw_writeb(0xff, MCFINTC0_SIMR);
	if (MCFINTC1_SIMR)
		__raw_writeb(0xff, MCFINTC1_SIMR);

	eirq = MCFINT_VECBASE + 64 + (MCFINTC1_ICR0 ? 64 : 0);
	for (irq = MCFINT_VECBASE; (irq < eirq); irq++) {
		if ((irq >= EINT1) && (irq <= EINT7))
			irq_set_chip(irq, &intc_irq_chip_edge_port);
		else
			irq_set_chip(irq, &intc_irq_chip);
		irq_set_irq_type(irq, IRQ_TYPE_LEVEL_HIGH);
		irq_set_handler(irq, handle_level_irq);
	}
}

