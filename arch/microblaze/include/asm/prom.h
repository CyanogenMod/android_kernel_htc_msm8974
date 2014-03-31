/*
 * Definitions for talking to the Open Firmware PROM on
 * Power Macintosh computers.
 *
 * Copyright (C) 1996-2005 Paul Mackerras.
 *
 * Updates for PPC64 by Peter Bergner & David Engebretsen, IBM Corp.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/of.h>	

#ifndef _ASM_MICROBLAZE_PROM_H
#define _ASM_MICROBLAZE_PROM_H
#ifdef __KERNEL__
#ifndef __ASSEMBLY__

#include <linux/types.h>
#include <asm/irq.h>
#include <linux/atomic.h>

#define HAVE_ARCH_DEVTREE_FIXUPS

enum early_consoles {
	UARTLITE = 1,
	UART16550 = 2,
};

extern int of_early_console(void *version);


#ifdef CONFIG_PCI
extern unsigned long pci_address_to_pio(phys_addr_t address);
#define pci_address_to_pio pci_address_to_pio
#endif	

void of_parse_dma_window(struct device_node *dn, const void *dma_window_prop,
		unsigned long *busno, unsigned long *phys, unsigned long *size);

extern void kdump_move_device_tree(void);

struct device_node *of_get_cpu_node(int cpu, unsigned int *thread);

#endif 
#endif 

#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>

#endif 
