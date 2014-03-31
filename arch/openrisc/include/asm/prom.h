/*
 * OpenRISC Linux
 *
 * Linux architectural port borrowing liberally from similar works of
 * others.  All original copyrights apply as per the original source
 * declaration.
 *
 * OpenRISC implementation:
 * Copyright (C) 2010-2011 Jonas Bonn <jonas@southpole.se>
 * et al.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/of.h>	

#ifndef _ASM_OPENRISC_PROM_H
#define _ASM_OPENRISC_PROM_H
#ifdef __KERNEL__
#ifndef __ASSEMBLY__

#include <linux/types.h>
#include <asm/irq.h>
#include <linux/irqdomain.h>
#include <linux/atomic.h>
#include <linux/of_irq.h>
#include <linux/of_fdt.h>
#include <linux/of_address.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#define HAVE_ARCH_DEVTREE_FIXUPS

extern int early_uartlite_console(void);

void of_parse_dma_window(struct device_node *dn, const void *dma_window_prop,
		unsigned long *busno, unsigned long *phys, unsigned long *size);

extern void kdump_move_device_tree(void);

struct device_node *of_get_cpu_node(int cpu, unsigned int *thread);

extern const void *of_get_mac_address(struct device_node *np);

struct pci_dev;
extern int of_irq_map_pci(struct pci_dev *pdev, struct of_irq *out_irq);

#endif 
#endif 
#endif 
