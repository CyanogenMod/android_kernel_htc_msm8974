/*
 * Copyright (C) 2007 Lemote, Inc. & Institute of Computing Technology
 * Author: Fuxin Zhang, zhangfx@lemote.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */
#include <linux/pci.h>

#include <pci.h>
#include <loongson.h>

static struct resource loongson_pci_mem_resource = {
	.name   = "pci memory space",
	.start  = LOONGSON_PCI_MEM_START,
	.end    = LOONGSON_PCI_MEM_END,
	.flags  = IORESOURCE_MEM,
};

static struct resource loongson_pci_io_resource = {
	.name   = "pci io space",
	.start  = LOONGSON_PCI_IO_START,
	.end    = IO_SPACE_LIMIT,
	.flags  = IORESOURCE_IO,
};

static struct pci_controller  loongson_pci_controller = {
	.pci_ops        = &loongson_pci_ops,
	.io_resource    = &loongson_pci_io_resource,
	.mem_resource   = &loongson_pci_mem_resource,
	.mem_offset     = 0x00000000UL,
	.io_offset      = 0x00000000UL,
};

static void __init setup_pcimap(void)
{
	LOONGSON_PCIMAP = LOONGSON_PCIMAP_PCIMAP_2 |
		LOONGSON_PCIMAP_WIN(2, LOONGSON_PCILO2_BASE) |
		LOONGSON_PCIMAP_WIN(1, LOONGSON_PCILO1_BASE) |
		LOONGSON_PCIMAP_WIN(0, 0);

	LOONGSON_PCIBASE0 = 0x80000000ul;   
	
	LOONGSON_PCI_HIT0_SEL_L = 0xc000000cul;
	LOONGSON_PCI_HIT0_SEL_H = 0xfffffffful;
	LOONGSON_PCI_HIT1_SEL_L = 0x00000006ul; 
	LOONGSON_PCI_HIT1_SEL_H = 0x00000000ul;
	LOONGSON_PCI_HIT2_SEL_L = 0x00000006ul; 
	LOONGSON_PCI_HIT2_SEL_H = 0x00000000ul;

	
	LOONGSON_PCI_ISR4C = 0xd2000001ul;

	LOONGSON_PXARB_CFG = 0x00fe0105ul;

#ifdef CONFIG_CPU_SUPPORTS_ADDRWINCFG
	LOONGSON_ADDRWIN_CPUTOPCI(ADDRWIN_WIN2, LOONGSON_CPU_MEM_SRC,
		LOONGSON_PCI_MEM_DST, MMAP_CPUTOPCI_SIZE);
#endif
}

static int __init pcibios_init(void)
{
	setup_pcimap();

	loongson_pci_controller.io_map_base = mips_io_port_base;

	register_pci_controller(&loongson_pci_controller);

	return 0;
}

arch_initcall(pcibios_init);
