/*
 * Copyright 2010 Tilera Corporation. All Rights Reserved.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *   NON INFRINGEMENT.  See the GNU General Public License for
 *   more details.
 */

#ifndef _ASM_TILE_PCI_H
#define _ASM_TILE_PCI_H

#include <linux/pci.h>
#include <asm-generic/pci_iomap.h>

struct pci_controller {
	int index;		
	struct pci_bus *root_bus;

	int first_busno;
	int last_busno;

	int hv_cfg_fd[2];	
	int hv_mem_fd;		

	struct pci_ops *ops;

	int irq_base;		
	int plx_gen1;		

	
	struct resource mem_resources[3];
};

#define PCI_DMA_BUS_IS_PHYS     1

int __init tile_pci_init(void);
int __init pcibios_init(void);

static inline void pci_iounmap(struct pci_dev *dev, void __iomem *addr) {}

void __devinit pcibios_fixup_bus(struct pci_bus *bus);

#define	TILE_NUM_PCIE	2

#define pci_domain_nr(bus) (((struct pci_controller *)(bus)->sysdata)->index)

static inline int pci_proc_domain(struct pci_bus *bus)
{
	return 1;
}

static inline int pcibios_assign_all_busses(void)
{
	return 1;
}

#define PCIBIOS_MIN_MEM		0
#define PCIBIOS_MIN_IO		0

extern int tile_plx_gen1;

#define cpumask_of_pcibus(bus) cpu_online_mask

#include <asm-generic/pci-dma-compat.h>

#include <asm-generic/pci.h>

#endif 
