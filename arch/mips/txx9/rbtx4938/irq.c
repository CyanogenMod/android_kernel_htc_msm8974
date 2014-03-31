/*
 * Toshiba RBTX4938 specific interrupt handlers
 * Copyright (C) 2000-2001 Toshiba Corporation
 *
 * 2003-2005 (c) MontaVista Software, Inc. This file is licensed under the
 * terms of the GNU General Public License version 2. This program is
 * licensed "as is" without any warranty of any kind, whether express
 * or implied.
 *
 * Support for TX4938 in 2.6 - Manish Lachwani (mlachwani@mvista.com)
 */

#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <asm/mipsregs.h>
#include <asm/txx9/generic.h>
#include <asm/txx9/rbtx4938.h>

static int toshiba_rbtx4938_irq_nested(int sw_irq)
{
	u8 level3;

	level3 = readb(rbtx4938_imstat_addr);
	if (unlikely(!level3))
		return -1;
	
	return RBTX4938_IRQ_IOC + __fls8(level3);
}

static void toshiba_rbtx4938_irq_ioc_enable(struct irq_data *d)
{
	unsigned char v;

	v = readb(rbtx4938_imask_addr);
	v |= (1 << (d->irq - RBTX4938_IRQ_IOC));
	writeb(v, rbtx4938_imask_addr);
	mmiowb();
}

static void toshiba_rbtx4938_irq_ioc_disable(struct irq_data *d)
{
	unsigned char v;

	v = readb(rbtx4938_imask_addr);
	v &= ~(1 << (d->irq - RBTX4938_IRQ_IOC));
	writeb(v, rbtx4938_imask_addr);
	mmiowb();
}

#define TOSHIBA_RBTX4938_IOC_NAME "RBTX4938-IOC"
static struct irq_chip toshiba_rbtx4938_irq_ioc_type = {
	.name = TOSHIBA_RBTX4938_IOC_NAME,
	.irq_mask = toshiba_rbtx4938_irq_ioc_disable,
	.irq_unmask = toshiba_rbtx4938_irq_ioc_enable,
};

static int rbtx4938_irq_dispatch(int pending)
{
	int irq;

	if (pending & STATUSF_IP7)
		irq = MIPS_CPU_IRQ_BASE + 7;
	else if (pending & STATUSF_IP2) {
		irq = txx9_irq();
		if (irq == RBTX4938_IRQ_IOCINT)
			irq = toshiba_rbtx4938_irq_nested(irq);
	} else if (pending & STATUSF_IP1)
		irq = MIPS_CPU_IRQ_BASE + 0;
	else if (pending & STATUSF_IP0)
		irq = MIPS_CPU_IRQ_BASE + 1;
	else
		irq = -1;
	return irq;
}

static void __init toshiba_rbtx4938_irq_ioc_init(void)
{
	int i;

	for (i = RBTX4938_IRQ_IOC;
	     i < RBTX4938_IRQ_IOC + RBTX4938_NR_IRQ_IOC; i++)
		irq_set_chip_and_handler(i, &toshiba_rbtx4938_irq_ioc_type,
					 handle_level_irq);

	irq_set_chained_handler(RBTX4938_IRQ_IOCINT, handle_simple_irq);
}

void __init rbtx4938_irq_setup(void)
{
	txx9_irq_dispatch = rbtx4938_irq_dispatch;
	
	
	

	
	writeb(0, rbtx4938_imask_addr);

	
	writeb(0, rbtx4938_softint_addr);
	tx4938_irq_init();
	toshiba_rbtx4938_irq_ioc_init();
	
	irq_set_irq_type(RBTX4938_IRQ_ETHER, IRQF_TRIGGER_HIGH);
}
