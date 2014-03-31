/* ASB2305 Initialisation
 *
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */
#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <asm/io.h>
#include <asm/setup.h>
#include <asm/processor.h>
#include <asm/intctl-regs.h>
#include <asm/serial-regs.h>
#include <unit/serial.h>

asmlinkage void __init unit_init(void)
{
#ifndef CONFIG_GDBSTUB_ON_TTYSx
	
#ifdef CONFIG_EXT_SERIAL_IRQ_LEVEL
	set_intr_level(XIRQ0, NUM2GxICR_LEVEL(CONFIG_EXT_SERIAL_IRQ_LEVEL));
#endif
#endif 
}

void __init unit_setup(void)
{
#ifdef CONFIG_PCI
	unit_pci_init();
#endif
}

void __init unit_init_IRQ(void)
{
	unsigned int extnum;

	for (extnum = 0; extnum < NR_XIRQS; extnum++) {
		switch (GET_XIRQ_TRIGGER(extnum)) {
		case XIRQ_TRIGGER_HILEVEL:
		case XIRQ_TRIGGER_LOWLEVEL:
			mn10300_set_lateack_irq_type(XIRQ2IRQ(extnum));
			break;
		default:
			break;
		}
	}
}
