/*
 * Copyright 2001 MontaVista Software Inc.
 * Author: Jun Sun, jsun@mvista.com or jsun@junsun.net
 *
 * Copyright (C) 2001 Ralf Baechle
 * Copyright (C) 2005  MIPS Technologies, Inc.  All rights reserved.
 *      Author: Maciej W. Rozycki <macro@mips.com>
 *
 * This file define the irq handler for MIPS CPU interrupts.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/irq.h>

#include <asm/irq_cpu.h>
#include <asm/mipsregs.h>
#include <asm/mipsmtregs.h>

static inline void unmask_mips_irq(struct irq_data *d)
{
	set_c0_status(0x100 << (d->irq - MIPS_CPU_IRQ_BASE));
	irq_enable_hazard();
}

static inline void mask_mips_irq(struct irq_data *d)
{
	clear_c0_status(0x100 << (d->irq - MIPS_CPU_IRQ_BASE));
	irq_disable_hazard();
}

static struct irq_chip mips_cpu_irq_controller = {
	.name		= "MIPS",
	.irq_ack	= mask_mips_irq,
	.irq_mask	= mask_mips_irq,
	.irq_mask_ack	= mask_mips_irq,
	.irq_unmask	= unmask_mips_irq,
	.irq_eoi	= unmask_mips_irq,
};


static unsigned int mips_mt_cpu_irq_startup(struct irq_data *d)
{
	unsigned int vpflags = dvpe();

	clear_c0_cause(0x100 << (d->irq - MIPS_CPU_IRQ_BASE));
	evpe(vpflags);
	unmask_mips_irq(d);
	return 0;
}

static void mips_mt_cpu_irq_ack(struct irq_data *d)
{
	unsigned int vpflags = dvpe();
	clear_c0_cause(0x100 << (d->irq - MIPS_CPU_IRQ_BASE));
	evpe(vpflags);
	mask_mips_irq(d);
}

static struct irq_chip mips_mt_cpu_irq_controller = {
	.name		= "MIPS",
	.irq_startup	= mips_mt_cpu_irq_startup,
	.irq_ack	= mips_mt_cpu_irq_ack,
	.irq_mask	= mask_mips_irq,
	.irq_mask_ack	= mips_mt_cpu_irq_ack,
	.irq_unmask	= unmask_mips_irq,
	.irq_eoi	= unmask_mips_irq,
};

void __init mips_cpu_irq_init(void)
{
	int irq_base = MIPS_CPU_IRQ_BASE;
	int i;

	
	clear_c0_status(ST0_IM);
	clear_c0_cause(CAUSEF_IP);

	
	for (i = irq_base; i < irq_base + 2; i++)
		irq_set_chip_and_handler(i, cpu_has_mipsmt ?
					 &mips_mt_cpu_irq_controller :
					 &mips_cpu_irq_controller,
					 handle_percpu_irq);

	for (i = irq_base + 2; i < irq_base + 8; i++)
		irq_set_chip_and_handler(i, &mips_cpu_irq_controller,
					 handle_percpu_irq);
}
