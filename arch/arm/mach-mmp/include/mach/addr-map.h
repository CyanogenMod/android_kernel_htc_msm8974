/*
 * linux/arch/arm/mach-mmp/include/mach/addr-map.h
 *
 *   Common address map definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_MACH_ADDR_MAP_H
#define __ASM_MACH_ADDR_MAP_H

#define APB_PHYS_BASE		0xd4000000
#define APB_VIRT_BASE		IOMEM(0xfe000000)
#define APB_PHYS_SIZE		0x00200000

#define AXI_PHYS_BASE		0xd4200000
#define AXI_VIRT_BASE		IOMEM(0xfe200000)
#define AXI_PHYS_SIZE		0x00200000

#define SMC_CS0_PHYS_BASE	0x80000000
#define SMC_CS0_PHYS_SIZE	0x10000000
#define SMC_CS1_PHYS_BASE	0x90000000
#define SMC_CS1_PHYS_SIZE	0x10000000

#endif 
