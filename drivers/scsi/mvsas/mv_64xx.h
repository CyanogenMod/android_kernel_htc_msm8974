/*
 * Marvell 88SE64xx hardware specific head file
 *
 * Copyright 2007 Red Hat, Inc.
 * Copyright 2008 Marvell. <kewei@marvell.com>
 * Copyright 2009-2011 Marvell. <yuxiangl@marvell.com>
 *
 * This file is licensed under GPLv2.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
*/

#ifndef _MVS64XX_REG_H_
#define _MVS64XX_REG_H_

#include <linux/types.h>

#define MAX_LINK_RATE		SAS_LINK_RATE_3_0_GBPS

enum hw_registers {
	MVS_GBL_CTL		= 0x04,  
	MVS_GBL_INT_STAT	= 0x08,  
	MVS_GBL_PI		= 0x0C,  

	MVS_PHY_CTL		= 0x40,  
	MVS_PORTS_IMP		= 0x9C,  

	MVS_GBL_PORT_TYPE	= 0xa0,  

	MVS_CTL			= 0x100, 
	MVS_PCS			= 0x104, 
	MVS_CMD_LIST_LO		= 0x108, 
	MVS_CMD_LIST_HI		= 0x10C,
	MVS_RX_FIS_LO		= 0x110, 
	MVS_RX_FIS_HI		= 0x114,

	MVS_TX_CFG		= 0x120, 
	MVS_TX_LO		= 0x124, 
	MVS_TX_HI		= 0x128,

	MVS_TX_PROD_IDX		= 0x12C, 
	MVS_TX_CONS_IDX		= 0x130, 
	MVS_RX_CFG		= 0x134, 
	MVS_RX_LO		= 0x138, 
	MVS_RX_HI		= 0x13C,
	MVS_RX_CONS_IDX		= 0x140, 

	MVS_INT_COAL		= 0x148, 
	MVS_INT_COAL_TMOUT	= 0x14C, 
	MVS_INT_STAT		= 0x150, 
	MVS_INT_MASK		= 0x154, 
	MVS_INT_STAT_SRS_0	= 0x158, 
	MVS_INT_MASK_SRS_0	= 0x15C,

					 
	MVS_P0_INT_STAT		= 0x160, 
	MVS_P0_INT_MASK		= 0x164, 
					 
	MVS_P4_INT_STAT		= 0x200, 
	MVS_P4_INT_MASK		= 0x204, 

					 
	MVS_P0_SER_CTLSTAT	= 0x180, 
					 
	MVS_P4_SER_CTLSTAT	= 0x220, 

	MVS_CMD_ADDR		= 0x1B8, 
	MVS_CMD_DATA		= 0x1BC, 

					 
	MVS_P0_CFG_ADDR		= 0x1C0, 
	MVS_P0_CFG_DATA		= 0x1C4, 
					 
	MVS_P4_CFG_ADDR		= 0x230, 
	MVS_P4_CFG_DATA		= 0x234, 

					 
	MVS_P0_VSR_ADDR		= 0x1E0, 
	MVS_P0_VSR_DATA		= 0x1E4, 
					 
	MVS_P4_VSR_ADDR		= 0x250, 
	MVS_P4_VSR_DATA		= 0x254, 
};

enum pci_cfg_registers {
	PCR_PHY_CTL		= 0x40,
	PCR_PHY_CTL2		= 0x90,
	PCR_DEV_CTRL		= 0xE8,
	PCR_LINK_STAT		= 0xF2,
};

enum sas_sata_vsp_regs {
	VSR_PHY_STAT		= 0x00, 
	VSR_PHY_MODE1		= 0x01, 
	VSR_PHY_MODE2		= 0x02, 
	VSR_PHY_MODE3		= 0x03, 
	VSR_PHY_MODE4		= 0x04, 
	VSR_PHY_MODE5		= 0x05, 
	VSR_PHY_MODE6		= 0x06, 
	VSR_PHY_MODE7		= 0x07, 
	VSR_PHY_MODE8		= 0x08, 
	VSR_PHY_MODE9		= 0x09, 
	VSR_PHY_MODE10		= 0x0A, 
	VSR_PHY_MODE11		= 0x0B, 
	VSR_PHY_VS0		= 0x0C, 
	VSR_PHY_VS1		= 0x0D, 
};

enum chip_register_bits {
	PHY_MIN_SPP_PHYS_LINK_RATE_MASK = (0xF << 8),
	PHY_MAX_SPP_PHYS_LINK_RATE_MASK = (0xF << 12),
	PHY_NEG_SPP_PHYS_LINK_RATE_MASK_OFFSET = (16),
	PHY_NEG_SPP_PHYS_LINK_RATE_MASK =
			(0xF << PHY_NEG_SPP_PHYS_LINK_RATE_MASK_OFFSET),
};

#define MAX_SG_ENTRY		64

struct mvs_prd {
	__le64			addr;		
	__le32			reserved;
	__le32			len;		
};

#define SPI_CTRL_REG				0xc0
#define SPI_CTRL_VENDOR_ENABLE		(1U<<29)
#define SPI_CTRL_SPIRDY         		(1U<<22)
#define SPI_CTRL_SPISTART			(1U<<20)

#define SPI_CMD_REG		0xc4
#define SPI_DATA_REG		0xc8

#define SPI_CTRL_REG_64XX		0x10
#define SPI_CMD_REG_64XX		0x14
#define SPI_DATA_REG_64XX		0x18

#endif
