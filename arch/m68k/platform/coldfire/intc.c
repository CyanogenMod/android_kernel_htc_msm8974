/*
 * intc.c  -- support for the old ColdFire interrupt controller
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
#include <linux/irq.h>
#include <linux/io.h>
#include <asm/traps.h>
#include <asm/coldfire.h>
#include <asm/mcfsim.h>

unsigned char mcf_irq2imr[NR_IRQS];

#define	EIRQ1	25
#define	EIRQ7	31

#ifdef MCFSIM_IMR_IS_16BITS

void mcf_setimr(int index)
{
	u16 imr;
	imr = __raw_readw(MCF_MBAR + MCFSIM_IMR);
	__raw_writew(imr | (0x1 << index), MCF_MBAR + MCFSIM_IMR);
}

void mcf_clrimr(int index)
{
	u16 imr;
	imr = __raw_readw(MCF_MBAR + MCFSIM_IMR);
	__raw_writew(imr & ~(0x1 << index), MCF_MBAR + MCFSIM_IMR);
}

void mcf_maskimr(unsigned int mask)
{
	u16 imr;
	imr = __raw_readw(MCF_MBAR + MCFSIM_IMR);
	imr |= mask;
	__raw_writew(imr, MCF_MBAR + MCFSIM_IMR);
}

#else

void mcf_setimr(int index)
{
	u32 imr;
	imr = __raw_readl(MCF_MBAR + MCFSIM_IMR);
	__raw_writel(imr | (0x1 << index), MCF_MBAR + MCFSIM_IMR);
}

void mcf_clrimr(int index)
{
	u32 imr;
	imr = __raw_readl(MCF_MBAR + MCFSIM_IMR);
	__raw_writel(imr & ~(0x1 << index), MCF_MBAR + MCFSIM_IMR);
}

void mcf_maskimr(unsigned int mask)
{
	u32 imr;
	imr = __raw_readl(MCF_MBAR + MCFSIM_IMR);
	imr |= mask;
	__raw_writel(imr, MCF_MBAR + MCFSIM_IMR);
}

#endif

void mcf_autovector(int irq)
{
#ifdef MCFSIM_AVR
	if ((irq >= EIRQ1) && (irq <= EIRQ7)) {
		u8 avec;
		avec = __raw_readb(MCF_MBAR + MCFSIM_AVR);
		avec |= (0x1 << (irq - EIRQ1 + 1));
		__raw_writeb(avec, MCF_MBAR + MCFSIM_AVR);
	}
#endif
}

static void intc_irq_mask(struct irq_data *d)
{
	if (mcf_irq2imr[d->irq])
		mcf_setimr(mcf_irq2imr[d->irq]);
}

static void intc_irq_unmask(struct irq_data *d)
{
	if (mcf_irq2imr[d->irq])
		mcf_clrimr(mcf_irq2imr[d->irq]);
}

static int intc_irq_set_type(struct irq_data *d, unsigned int type)
{
	return 0;
}

static struct irq_chip intc_irq_chip = {
	.name		= "CF-INTC",
	.irq_mask	= intc_irq_mask,
	.irq_unmask	= intc_irq_unmask,
	.irq_set_type	= intc_irq_set_type,
};

void __init init_IRQ(void)
{
	int irq;

	mcf_maskimr(0xffffffff);

	for (irq = 0; (irq < NR_IRQS); irq++) {
		irq_set_chip(irq, &intc_irq_chip);
		irq_set_irq_type(irq, IRQ_TYPE_LEVEL_HIGH);
		irq_set_handler(irq, handle_level_irq);
	}
}

