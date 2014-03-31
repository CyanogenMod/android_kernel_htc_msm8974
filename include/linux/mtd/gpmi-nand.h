/*
 * Copyright (C) 2011 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __MACH_MXS_GPMI_NAND_H__
#define __MACH_MXS_GPMI_NAND_H__

#define GPMI_NAND_RES_SIZE	6

#define GPMI_NAND_GPMI_REGS_ADDR_RES_NAME  "GPMI NAND GPMI Registers"
#define GPMI_NAND_GPMI_INTERRUPT_RES_NAME  "GPMI NAND GPMI Interrupt"
#define GPMI_NAND_BCH_REGS_ADDR_RES_NAME   "GPMI NAND BCH Registers"
#define GPMI_NAND_BCH_INTERRUPT_RES_NAME   "GPMI NAND BCH Interrupt"
#define GPMI_NAND_DMA_CHANNELS_RES_NAME    "GPMI NAND DMA Channels"
#define GPMI_NAND_DMA_INTERRUPT_RES_NAME   "GPMI NAND DMA Interrupt"

struct gpmi_nand_platform_data {
	
	int		(*platform_init)(void);

	
	unsigned int	min_prop_delay_in_ns;
	unsigned int	max_prop_delay_in_ns;
	unsigned int	max_chip_count;

	
	struct		mtd_partition *partitions;
	unsigned	partition_count;
};
#endif
