/*
 * intc.c  --  interrupt controller or ColdFire 5272 SoC
 *
 * (C) Copyright 2009, Greg Ungerer <gerg@snapgear.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <asm/coldfire.h>
#include <asm/mcfsim.h>
#include <asm/traps.h>


struct irqmap {
	unsigned char	icr;
	unsigned char	index;
	unsigned char	ack;
};

static struct irqmap intc_irqmap[MCFINT_VECMAX - MCFINT_VECBASE] = {
		{ .icr = 0,           .index = 0,  .ack = 0, },
		{ .icr = MCFSIM_ICR1, .index = 28, .ack = 1, },
		{ .icr = MCFSIM_ICR1, .index = 24, .ack = 1, },
		{ .icr = MCFSIM_ICR1, .index = 20, .ack = 1, },
		{ .icr = MCFSIM_ICR1, .index = 16, .ack = 1, },
		{ .icr = MCFSIM_ICR1, .index = 12, .ack = 0, },
		{ .icr = MCFSIM_ICR1, .index = 8,  .ack = 0, },
		{ .icr = MCFSIM_ICR1, .index = 4,  .ack = 0, },
		{ .icr = MCFSIM_ICR1, .index = 0,  .ack = 0, },
		{ .icr = MCFSIM_ICR2, .index = 28, .ack = 0, },
		{ .icr = MCFSIM_ICR2, .index = 24, .ack = 0, },
		{ .icr = MCFSIM_ICR2, .index = 20, .ack = 0, },
		{ .icr = MCFSIM_ICR2, .index = 16, .ack = 0, },
		{ .icr = MCFSIM_ICR2, .index = 12, .ack = 0, },
		{ .icr = MCFSIM_ICR2, .index = 8,  .ack = 0, },
		{ .icr = MCFSIM_ICR2, .index = 4,  .ack = 0, },
		{ .icr = MCFSIM_ICR2, .index = 0,  .ack = 0, },
		{ .icr = MCFSIM_ICR3, .index = 28, .ack = 0, },
		{ .icr = MCFSIM_ICR3, .index = 24, .ack = 0, },
		{ .icr = MCFSIM_ICR3, .index = 20, .ack = 0, },
		{ .icr = MCFSIM_ICR3, .index = 16, .ack = 0, },
			{ .icr = MCFSIM_ICR3, .index = 12, .ack = 0, },
			{ .icr = MCFSIM_ICR3, .index = 8,  .ack = 0, },
			{ .icr = MCFSIM_ICR3, .index = 4,  .ack = 0, },
		{ .icr = MCFSIM_ICR3, .index = 0,  .ack = 0, },
		{ .icr = MCFSIM_ICR4, .index = 28, .ack = 0, },
		{ .icr = MCFSIM_ICR4, .index = 24, .ack = 1, },
		{ .icr = MCFSIM_ICR4, .index = 20, .ack = 1, },
		{ .icr = MCFSIM_ICR4, .index = 16, .ack = 0, },
};

static void intc_irq_mask(struct irq_data *d)
{
	unsigned int irq = d->irq;

	if ((irq >= MCFINT_VECBASE) && (irq <= MCFINT_VECMAX)) {
		u32 v;
		irq -= MCFINT_VECBASE;
		v = 0x8 << intc_irqmap[irq].index;
		writel(v, MCF_MBAR + intc_irqmap[irq].icr);
	}
}

static void intc_irq_unmask(struct irq_data *d)
{
	unsigned int irq = d->irq;

	if ((irq >= MCFINT_VECBASE) && (irq <= MCFINT_VECMAX)) {
		u32 v;
		irq -= MCFINT_VECBASE;
		v = 0xd << intc_irqmap[irq].index;
		writel(v, MCF_MBAR + intc_irqmap[irq].icr);
	}
}

static void intc_irq_ack(struct irq_data *d)
{
	unsigned int irq = d->irq;

	
	if ((irq >= MCFINT_VECBASE) && (irq <= MCFINT_VECMAX)) {
		irq -= MCFINT_VECBASE;
		if (intc_irqmap[irq].ack) {
			u32 v;
			v = readl(MCF_MBAR + intc_irqmap[irq].icr);
			v &= (0x7 << intc_irqmap[irq].index);
			v |= (0x8 << intc_irqmap[irq].index);
			writel(v, MCF_MBAR + intc_irqmap[irq].icr);
		}
	}
}

static int intc_irq_set_type(struct irq_data *d, unsigned int type)
{
	unsigned int irq = d->irq;

	if ((irq >= MCFINT_VECBASE) && (irq <= MCFINT_VECMAX)) {
		irq -= MCFINT_VECBASE;
		if (intc_irqmap[irq].ack) {
			u32 v;
			v = readl(MCF_MBAR + MCFSIM_PITR);
			if (type == IRQ_TYPE_EDGE_FALLING)
				v &= ~(0x1 << (32 - irq));
			else
				v |= (0x1 << (32 - irq));
			writel(v, MCF_MBAR + MCFSIM_PITR);
		}
	}
	return 0;
}

static void intc_external_irq(unsigned int irq, struct irq_desc *desc)
{
	irq_desc_get_chip(desc)->irq_ack(&desc->irq_data);
	handle_simple_irq(irq, desc);
}

static struct irq_chip intc_irq_chip = {
	.name		= "CF-INTC",
	.irq_mask	= intc_irq_mask,
	.irq_unmask	= intc_irq_unmask,
	.irq_mask_ack	= intc_irq_mask,
	.irq_ack	= intc_irq_ack,
	.irq_set_type	= intc_irq_set_type,
};

void __init init_IRQ(void)
{
	int irq, edge;

	
	writel(0x88888888, MCF_MBAR + MCFSIM_ICR1);
	writel(0x88888888, MCF_MBAR + MCFSIM_ICR2);
	writel(0x88888888, MCF_MBAR + MCFSIM_ICR3);
	writel(0x88888888, MCF_MBAR + MCFSIM_ICR4);

	for (irq = 0; (irq < NR_IRQS); irq++) {
		irq_set_chip(irq, &intc_irq_chip);
		edge = 0;
		if ((irq >= MCFINT_VECBASE) && (irq <= MCFINT_VECMAX))
			edge = intc_irqmap[irq - MCFINT_VECBASE].ack;
		if (edge) {
			irq_set_irq_type(irq, IRQ_TYPE_EDGE_RISING);
			irq_set_handler(irq, intc_external_irq);
		} else {
			irq_set_irq_type(irq, IRQ_TYPE_LEVEL_HIGH);
			irq_set_handler(irq, handle_level_irq);
		}
	}
}

