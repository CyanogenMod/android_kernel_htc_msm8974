/*
 * arch/arm/mach-sa1100/include/mach/nanoengine.h
 *
 * This file contains the hardware specific definitions for nanoEngine.
 * Only include this file from SA1100-specific files.
 *
 * Copyright (C) 2010 Marcelo Roberto Jimenez <mroberto@cpti.cetuc.puc-rio.br>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#ifndef __ASM_ARCH_NANOENGINE_H
#define __ASM_ARCH_NANOENGINE_H

#include <mach/irqs.h>

#define GPIO_PC_READY0	11 
#define GPIO_PC_READY1	12 
#define GPIO_PC_CD0	13 
#define GPIO_PC_CD1	14 
#define GPIO_PC_RESET0	15 
#define GPIO_PC_RESET1	16 

#define NANOENGINE_IRQ_GPIO_PCI		IRQ_GPIO0
#define NANOENGINE_IRQ_GPIO_PC_READY0	IRQ_GPIO11
#define NANOENGINE_IRQ_GPIO_PC_READY1	IRQ_GPIO12
#define NANOENGINE_IRQ_GPIO_PC_CD0	IRQ_GPIO13
#define NANOENGINE_IRQ_GPIO_PC_CD1	IRQ_GPIO14


#define NANO_PCI_MEM_RW_PHYS		0x18600000
#define NANO_PCI_MEM_RW_VIRT		0xf1000000
#define NANO_PCI_MEM_RW_SIZE		SZ_1M
#define NANO_PCI_CONFIG_SPACE_PHYS	0x18A10000
#define NANO_PCI_CONFIG_SPACE_VIRT	0xf2000000
#define NANO_PCI_CONFIG_SPACE_SIZE	SZ_64K

#endif

