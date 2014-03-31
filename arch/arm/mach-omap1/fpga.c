/*
 * linux/arch/arm/mach-omap1/fpga.c
 *
 * Interrupt handler for OMAP-1510 Innovator FPGA
 *
 * Copyright (C) 2001 RidgeRun, Inc.
 * Author: Greg Lonnon <glonnon@ridgerun.com>
 *
 * Copyright (C) 2002 MontaVista Software, Inc.
 *
 * Separated FPGA interrupts from innovator1510.c and cleaned up for 2.6
 * Copyright (C) 2004 Nokia Corporation by Tony Lindrgen <tony@atomide.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/types.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/io.h>

#include <asm/irq.h>
#include <asm/mach/irq.h>

#include <plat/fpga.h>

#include <mach/hardware.h>

#include "iomap.h"

static void fpga_mask_irq(struct irq_data *d)
{
	unsigned int irq = d->irq - OMAP_FPGA_IRQ_BASE;

	if (irq < 8)
		__raw_writeb((__raw_readb(OMAP1510_FPGA_IMR_LO)
			      & ~(1 << irq)), OMAP1510_FPGA_IMR_LO);
	else if (irq < 16)
		__raw_writeb((__raw_readb(OMAP1510_FPGA_IMR_HI)
			      & ~(1 << (irq - 8))), OMAP1510_FPGA_IMR_HI);
	else
		__raw_writeb((__raw_readb(INNOVATOR_FPGA_IMR2)
			      & ~(1 << (irq - 16))), INNOVATOR_FPGA_IMR2);
}


static inline u32 get_fpga_unmasked_irqs(void)
{
	return
		((__raw_readb(OMAP1510_FPGA_ISR_LO) &
		  __raw_readb(OMAP1510_FPGA_IMR_LO))) |
		((__raw_readb(OMAP1510_FPGA_ISR_HI) &
		  __raw_readb(OMAP1510_FPGA_IMR_HI)) << 8) |
		((__raw_readb(INNOVATOR_FPGA_ISR2) &
		  __raw_readb(INNOVATOR_FPGA_IMR2)) << 16);
}


static void fpga_ack_irq(struct irq_data *d)
{
	
}

static void fpga_unmask_irq(struct irq_data *d)
{
	unsigned int irq = d->irq - OMAP_FPGA_IRQ_BASE;

	if (irq < 8)
		__raw_writeb((__raw_readb(OMAP1510_FPGA_IMR_LO) | (1 << irq)),
		     OMAP1510_FPGA_IMR_LO);
	else if (irq < 16)
		__raw_writeb((__raw_readb(OMAP1510_FPGA_IMR_HI)
			      | (1 << (irq - 8))), OMAP1510_FPGA_IMR_HI);
	else
		__raw_writeb((__raw_readb(INNOVATOR_FPGA_IMR2)
			      | (1 << (irq - 16))), INNOVATOR_FPGA_IMR2);
}

static void fpga_mask_ack_irq(struct irq_data *d)
{
	fpga_mask_irq(d);
	fpga_ack_irq(d);
}

void innovator_fpga_IRQ_demux(unsigned int irq, struct irq_desc *desc)
{
	u32 stat;
	int fpga_irq;

	stat = get_fpga_unmasked_irqs();

	if (!stat)
		return;

	for (fpga_irq = OMAP_FPGA_IRQ_BASE;
	     (fpga_irq < OMAP_FPGA_IRQ_END) && stat;
	     fpga_irq++, stat >>= 1) {
		if (stat & 1) {
			generic_handle_irq(fpga_irq);
		}
	}
}

static struct irq_chip omap_fpga_irq_ack = {
	.name		= "FPGA-ack",
	.irq_ack	= fpga_mask_ack_irq,
	.irq_mask	= fpga_mask_irq,
	.irq_unmask	= fpga_unmask_irq,
};


static struct irq_chip omap_fpga_irq = {
	.name		= "FPGA",
	.irq_ack	= fpga_ack_irq,
	.irq_mask	= fpga_mask_irq,
	.irq_unmask	= fpga_unmask_irq,
};

void omap1510_fpga_init_irq(void)
{
	int i, res;

	__raw_writeb(0, OMAP1510_FPGA_IMR_LO);
	__raw_writeb(0, OMAP1510_FPGA_IMR_HI);
	__raw_writeb(0, INNOVATOR_FPGA_IMR2);

	for (i = OMAP_FPGA_IRQ_BASE; i < OMAP_FPGA_IRQ_END; i++) {

		if (i == OMAP1510_INT_FPGA_TS) {
			irq_set_chip(i, &omap_fpga_irq_ack);
		}
		else {
			irq_set_chip(i, &omap_fpga_irq);
		}

		irq_set_handler(i, handle_edge_irq);
		set_irq_flags(i, IRQF_VALID);
	}

	res = gpio_request(13, "FPGA irq");
	if (res) {
		pr_err("%s failed to get gpio\n", __func__);
		return;
	}
	gpio_direction_input(13);
	irq_set_irq_type(gpio_to_irq(13), IRQ_TYPE_EDGE_RISING);
	irq_set_chained_handler(OMAP1510_INT_FPGA, innovator_fpga_IRQ_demux);
}
