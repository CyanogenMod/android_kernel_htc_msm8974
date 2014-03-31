/*
 * Defines for the address space, registers and register configuration
 * (bit masks, access macros etc) for the PMC-Sierra line of MSP products.
 * This file contains addess maps for all the devices in the line of
 * products but only has register definitions and configuration masks for
 * registers which aren't definitely associated with any device.  Things
 * like clock settings, reset access, the ELB etc.  Individual device
 * drivers will reference the appropriate XXX_BASE value defined here
 * and have individual registers offset from that.
 *
 * Copyright (C) 2005-2007 PMC-Sierra, Inc.  All rights reserved.
 * Author: Andrew Hughes, Andrew_Hughes@pmc-sierra.com
 *
 * ########################################################################
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * ########################################################################
 */

#include <asm/addrspace.h>
#include <linux/types.h>

#ifndef _ASM_MSP_REGS_H
#define _ASM_MSP_REGS_H


#define MSP_SLP_BASE		0x1c000000
					
#define MSP_RST_BASE		(MSP_SLP_BASE + 0x10)
					
#define MSP_RST_SIZE		0x0C	

#define MSP_WTIMER_BASE		(MSP_SLP_BASE + 0x04C)
					
#define MSP_ITIMER_BASE		(MSP_SLP_BASE + 0x054)
					
#define MSP_UART0_BASE		(MSP_SLP_BASE + 0x100)
					
#define MSP_BCPY_CTRL_BASE	(MSP_SLP_BASE + 0x120)
					
#define MSP_BCPY_DESC_BASE	(MSP_SLP_BASE + 0x160)
					

#define MSP_PCI_BASE		0x19000000

#define MSP_MSB_BASE		0x18000000
					
#define MSP_PER_BASE		(MSP_MSB_BASE + 0x400000)
					
#define MSP_MAC0_BASE		(MSP_MSB_BASE + 0x600000)
					
#define MSP_MAC1_BASE		(MSP_MSB_BASE + 0x700000)
					
#define MSP_MAC_SIZE		0xE0	

#define MSP_SEC_BASE		(MSP_MSB_BASE + 0x800000)
					
#define MSP_MAC2_BASE		(MSP_MSB_BASE + 0x900000)
					
#define MSP_ADSL2_BASE		(MSP_MSB_BASE + 0xA80000)
					
#define MSP_USB0_BASE		(MSP_MSB_BASE + 0xB00000)
					
#define MSP_USB1_BASE		(MSP_MSB_BASE + 0x300000)
					
#define MSP_CPUIF_BASE		(MSP_MSB_BASE + 0xC00000)
					

#define MSP_UART1_BASE		(MSP_PER_BASE + 0x030)
					
#define MSP_SPI_BASE		(MSP_PER_BASE + 0x058)
					
#define MSP_TWI_BASE		(MSP_PER_BASE + 0x090)
					
#define MSP_PTIMER_BASE		(MSP_PER_BASE + 0x0F0)
					

#define MSP_MEM_CFG_BASE	0x17f00000

#define MSP_MEM_INDIRECT_CTL_10	0x10



#ifdef __ASSEMBLER__
	#define regptr(addr) (KSEG1ADDR(addr))
#else
	#define regptr(addr) ((volatile u32 *const)(KSEG1ADDR(addr)))
#endif


#define	DEV_ID_REG	regptr(MSP_SLP_BASE + 0x00)
					
#define	FWR_ID_REG	regptr(MSP_SLP_BASE + 0x04)
					
#define	SYS_ID_REG0	regptr(MSP_SLP_BASE + 0x08)
					
#define	SYS_ID_REG1	regptr(MSP_SLP_BASE + 0x0C)
					

#define	RST_STS_REG	regptr(MSP_SLP_BASE + 0x10)
					
#define	RST_SET_REG	regptr(MSP_SLP_BASE + 0x14)
					
#define	RST_CLR_REG	regptr(MSP_SLP_BASE + 0x18)
					

#define PCI_SLP_REG	regptr(MSP_SLP_BASE + 0x1C)
					
#define URT_SLP_REG	regptr(MSP_SLP_BASE + 0x20)
					
#define PLL1_SLP_REG	regptr(MSP_SLP_BASE + 0x2C)
					
#define PLL0_SLP_REG	regptr(MSP_SLP_BASE + 0x30)
					
#define MIPS_SLP_REG	regptr(MSP_SLP_BASE + 0x34)
					
#define	VE_SLP_REG	regptr(MSP_SLP_BASE + 0x38)
					
#define MSB_SLP_REG	regptr(MSP_SLP_BASE + 0x40)
					
#define SMAC_SLP_REG	regptr(MSP_SLP_BASE + 0x44)
					
#define PERF_SLP_REG	regptr(MSP_SLP_BASE + 0x48)
					

#define SLP_INT_STS_REG regptr(MSP_SLP_BASE + 0x70)
					
#define SLP_INT_MSK_REG regptr(MSP_SLP_BASE + 0x74)
					
#define SE_MBOX_REG	regptr(MSP_SLP_BASE + 0x78)
					
#define VE_MBOX_REG	regptr(MSP_SLP_BASE + 0x7C)
					

#define CS0_CNFG_REG	regptr(MSP_SLP_BASE + 0x80)
					
#define CS0_ADDR_REG	regptr(MSP_SLP_BASE + 0x84)
					
#define CS0_MASK_REG	regptr(MSP_SLP_BASE + 0x88)
					
#define CS0_ACCESS_REG	regptr(MSP_SLP_BASE + 0x8C)
					

#define CS1_CNFG_REG	regptr(MSP_SLP_BASE + 0x90)
					
#define CS1_ADDR_REG	regptr(MSP_SLP_BASE + 0x94)
					
#define CS1_MASK_REG	regptr(MSP_SLP_BASE + 0x98)
					
#define CS1_ACCESS_REG	regptr(MSP_SLP_BASE + 0x9C)
					

#define CS2_CNFG_REG	regptr(MSP_SLP_BASE + 0xA0)
					
#define CS2_ADDR_REG	regptr(MSP_SLP_BASE + 0xA4)
					
#define CS2_MASK_REG	regptr(MSP_SLP_BASE + 0xA8)
					
#define CS2_ACCESS_REG	regptr(MSP_SLP_BASE + 0xAC)
					

#define CS3_CNFG_REG	regptr(MSP_SLP_BASE + 0xB0)
					
#define CS3_ADDR_REG	regptr(MSP_SLP_BASE + 0xB4)
					
#define CS3_MASK_REG	regptr(MSP_SLP_BASE + 0xB8)
					
#define CS3_ACCESS_REG	regptr(MSP_SLP_BASE + 0xBC)
					

#define CS4_CNFG_REG	regptr(MSP_SLP_BASE + 0xC0)
					
#define CS4_ADDR_REG	regptr(MSP_SLP_BASE + 0xC4)
					
#define CS4_MASK_REG	regptr(MSP_SLP_BASE + 0xC8)
					
#define CS4_ACCESS_REG	regptr(MSP_SLP_BASE + 0xCC)
					

#define CS5_CNFG_REG	regptr(MSP_SLP_BASE + 0xD0)
					
#define CS5_ADDR_REG	regptr(MSP_SLP_BASE + 0xD4)
					
#define CS5_MASK_REG	regptr(MSP_SLP_BASE + 0xD8)
					
#define CS5_ACCESS_REG	regptr(MSP_SLP_BASE + 0xDC)
					

#define ELB_1PC_EN_REG	regptr(MSP_SLP_BASE + 0xEC)
					

#define ELB_CLK_CFG_REG	regptr(MSP_SLP_BASE + 0xFC)
					

#define UART0_STATUS_REG	regptr(MSP_UART0_BASE + 0x0c0)
					
#define UART1_STATUS_REG	regptr(MSP_UART1_BASE + 0x170)
					

#define PERF_MON_CTRL_REG	regptr(MSP_SLP_BASE + 0x140)
					
#define PERF_MON_CLR_REG	regptr(MSP_SLP_BASE + 0x144)
					
#define PERF_MON_CNTH_REG	regptr(MSP_SLP_BASE + 0x148)
					
#define PERF_MON_CNTL_REG	regptr(MSP_SLP_BASE + 0x14C)
					

#define SYS_CTRL_REG		regptr(MSP_SLP_BASE + 0x150)
					
#define SYS_ERR1_REG		regptr(MSP_SLP_BASE + 0x154)
					
#define SYS_ERR2_REG		regptr(MSP_SLP_BASE + 0x158)
					
#define SYS_INT_CFG_REG		regptr(MSP_SLP_BASE + 0x15C)
					

#define VE_MEM_REG		regptr(MSP_SLP_BASE + 0x17C)
					

#define CPU_ERR1_REG		regptr(MSP_SLP_BASE + 0x180)
					
#define CPU_ERR2_REG		regptr(MSP_SLP_BASE + 0x184)
					

#define EXTENDED_GPIO1_REG	regptr(MSP_SLP_BASE + 0x188)
#define EXTENDED_GPIO2_REG	regptr(MSP_SLP_BASE + 0x18c)
#define EXTENDED_GPIO_REG	EXTENDED_GPIO1_REG
					

#define SLP_ERR_STS_REG		regptr(MSP_SLP_BASE + 0x190)
					
#define SLP_ERR_MSK_REG		regptr(MSP_SLP_BASE + 0x194)
					
#define SLP_ELB_ERST_REG	regptr(MSP_SLP_BASE + 0x198)
					
#define SLP_BOOT_STS_REG	regptr(MSP_SLP_BASE + 0x19C)
					

#define CS0_EXT_ADDR_REG	regptr(MSP_SLP_BASE + 0x1A0)
					
#define CS1_EXT_ADDR_REG	regptr(MSP_SLP_BASE + 0x1A4)
					
#define CS2_EXT_ADDR_REG	regptr(MSP_SLP_BASE + 0x1A8)
					
#define CS3_EXT_ADDR_REG	regptr(MSP_SLP_BASE + 0x1AC)
					
#define CS5_EXT_ADDR_REG	regptr(MSP_SLP_BASE + 0x1B4)
					

#define PLL_LOCK_REG		regptr(MSP_SLP_BASE + 0x200)
					
#define PLL_ARST_REG		regptr(MSP_SLP_BASE + 0x204)
					
#define PLL0_ADJ_REG		regptr(MSP_SLP_BASE + 0x208)
					
#define PLL1_ADJ_REG		regptr(MSP_SLP_BASE + 0x20C)
					


#define PER_CTRL_REG		regptr(MSP_PER_BASE + 0x50)
					
#define PER_STS_REG		regptr(MSP_PER_BASE + 0x54)
					

#define SMPI_TX_SZ_REG		regptr(MSP_PER_BASE + 0x58)
					
#define SMPI_RX_SZ_REG		regptr(MSP_PER_BASE + 0x5C)
					
#define SMPI_CTL_REG		regptr(MSP_PER_BASE + 0x60)
					
#define SMPI_MS_REG		regptr(MSP_PER_BASE + 0x64)
					
#define SMPI_CORE_DATA_REG	regptr(MSP_PER_BASE + 0xC0)
					
#define SMPI_CORE_CTRL_REG	regptr(MSP_PER_BASE + 0xC4)
					
#define SMPI_CORE_STAT_REG	regptr(MSP_PER_BASE + 0xC8)
					
#define SMPI_CORE_SSEL_REG	regptr(MSP_PER_BASE + 0xCC)
					
#define SMPI_FIFO_REG		regptr(MSP_PER_BASE + 0xD0)
					

#define PER_ERR_STS_REG		regptr(MSP_PER_BASE + 0x70)
					
#define PER_ERR_MSK_REG		regptr(MSP_PER_BASE + 0x74)
					
#define PER_HDR1_REG		regptr(MSP_PER_BASE + 0x78)
					
#define PER_HDR2_REG		regptr(MSP_PER_BASE + 0x7C)
					

#define PER_INT_STS_REG		regptr(MSP_PER_BASE + 0x80)
					
#define PER_INT_MSK_REG		regptr(MSP_PER_BASE + 0x84)
					
#define GPIO_INT_STS_REG	regptr(MSP_PER_BASE + 0x88)
					
#define GPIO_INT_MSK_REG	regptr(MSP_PER_BASE + 0x8C)
					

#define POLO_GPIO_DAT1_REG	regptr(MSP_PER_BASE + 0x0E0)
					
#define POLO_GPIO_CFG1_REG	regptr(MSP_PER_BASE + 0x0E4)
					
#define POLO_GPIO_CFG2_REG	regptr(MSP_PER_BASE + 0x0E8)
					
#define POLO_GPIO_OD1_REG	regptr(MSP_PER_BASE + 0x0EC)
					
#define POLO_GPIO_CFG3_REG	regptr(MSP_PER_BASE + 0x170)
					
#define POLO_GPIO_DAT2_REG	regptr(MSP_PER_BASE + 0x174)
					
#define POLO_GPIO_DAT3_REG	regptr(MSP_PER_BASE + 0x178)
					
#define POLO_GPIO_DAT4_REG	regptr(MSP_PER_BASE + 0x17C)
					
#define POLO_GPIO_DAT5_REG	regptr(MSP_PER_BASE + 0x180)
					
#define POLO_GPIO_DAT6_REG	regptr(MSP_PER_BASE + 0x184)
					
#define POLO_GPIO_DAT7_REG	regptr(MSP_PER_BASE + 0x188)
					
#define POLO_GPIO_CFG4_REG	regptr(MSP_PER_BASE + 0x18C)
					
#define POLO_GPIO_CFG5_REG	regptr(MSP_PER_BASE + 0x190)
					
#define POLO_GPIO_CFG6_REG	regptr(MSP_PER_BASE + 0x194)
					
#define POLO_GPIO_CFG7_REG	regptr(MSP_PER_BASE + 0x198)
					
#define POLO_GPIO_OD2_REG	regptr(MSP_PER_BASE + 0x19C)
					

#define GPIO_DATA1_REG		regptr(MSP_PER_BASE + 0x170)
					
#define GPIO_DATA2_REG		regptr(MSP_PER_BASE + 0x174)
					
#define GPIO_DATA3_REG		regptr(MSP_PER_BASE + 0x178)
					
#define GPIO_DATA4_REG		regptr(MSP_PER_BASE + 0x17C)
					
#define GPIO_CFG1_REG		regptr(MSP_PER_BASE + 0x180)
					
#define GPIO_CFG2_REG		regptr(MSP_PER_BASE + 0x184)
					
#define GPIO_CFG3_REG		regptr(MSP_PER_BASE + 0x188)
					
#define GPIO_CFG4_REG		regptr(MSP_PER_BASE + 0x18C)
					
#define GPIO_OD_REG		regptr(MSP_PER_BASE + 0x190)
					

#define PCI_FLUSH_REG		regptr(MSP_CPUIF_BASE + 0x00)
					
#define OCP_ERR1_REG		regptr(MSP_CPUIF_BASE + 0x04)
					
#define OCP_ERR2_REG		regptr(MSP_CPUIF_BASE + 0x08)
					
#define OCP_STS_REG		regptr(MSP_CPUIF_BASE + 0x0C)
					
#define CPUIF_PM_REG		regptr(MSP_CPUIF_BASE + 0x10)
					
#define CPUIF_CFG_REG		regptr(MSP_CPUIF_BASE + 0x10)
					

#define MSP_CIC_BASE		(MSP_CPUIF_BASE + 0x8000)
					
#define CIC_EXT_CFG_REG		regptr(MSP_CIC_BASE + 0x00)
					
#define CIC_STS_REG		regptr(MSP_CIC_BASE + 0x04)
					
#define CIC_VPE0_MSK_REG	regptr(MSP_CIC_BASE + 0x08)
					
#define CIC_VPE1_MSK_REG	regptr(MSP_CIC_BASE + 0x0C)
					
#define CIC_TC0_MSK_REG		regptr(MSP_CIC_BASE + 0x10)
					
#define CIC_TC1_MSK_REG		regptr(MSP_CIC_BASE + 0x14)
					
#define CIC_TC2_MSK_REG		regptr(MSP_CIC_BASE + 0x18)
					
#define CIC_TC3_MSK_REG		regptr(MSP_CIC_BASE + 0x18)
					
#define CIC_TC4_MSK_REG		regptr(MSP_CIC_BASE + 0x18)
					
#define CIC_PCIMSI_STS_REG	regptr(MSP_CIC_BASE + 0x18)
#define CIC_PCIMSI_MSK_REG	regptr(MSP_CIC_BASE + 0x18)
#define CIC_PCIFLSH_REG		regptr(MSP_CIC_BASE + 0x18)
#define CIC_VPE0_SWINT_REG	regptr(MSP_CIC_BASE + 0x08)


#define MEM_CFG1_REG		regptr(MSP_MEM_CFG_BASE + 0x00)
#define MEM_SS_ADDR		regptr(MSP_MEM_CFG_BASE + 0x00)
#define MEM_SS_DATA		regptr(MSP_MEM_CFG_BASE + 0x04)
#define MEM_SS_WRITE		regptr(MSP_MEM_CFG_BASE + 0x08)

#define PCI_BASE_REG		regptr(MSP_PCI_BASE + 0x00)
#define PCI_CONFIG_SPACE_REG	regptr(MSP_PCI_BASE + 0x800)
#define PCI_JTAG_DEVID_REG	regptr(MSP_SLP_BASE + 0x13c)


#define DEV_ID_PCI_DIS		(1 << 26)       
#define DEV_ID_PCI_HOST		(1 << 20)       
#define DEV_ID_SINGLE_PC	(1 << 19)       
#define DEV_ID_FAMILY		(0xff << 8)     
#define POLO_ZEUS_SUB_FAMILY	(0x7  << 16)    

#define MSPFPGA_ID		(0x00  << 8)    
#define MSP5000_ID		(0x50  << 8)
#define MSP4F00_ID		(0x4f  << 8)    
#define MSP4E00_ID		(0x4f  << 8)    
#define MSP4200_ID		(0x42  << 8)
#define MSP4000_ID		(0x40  << 8)
#define MSP2XXX_ID		(0x20  << 8)
#define MSPZEUS_ID		(0x10  << 8)

#define MSP2004_SUB_ID		(0x0   << 16)
#define MSP2005_SUB_ID		(0x1   << 16)
#define MSP2006_SUB_ID		(0x1   << 16)
#define MSP2007_SUB_ID		(0x2   << 16)
#define MSP2010_SUB_ID		(0x3   << 16)
#define MSP2015_SUB_ID		(0x4   << 16)
#define MSP2020_SUB_ID		(0x5   << 16)
#define MSP2100_SUB_ID		(0x6   << 16)

#define MSP_GR_RST		(0x01 << 0)     
#define MSP_MR_RST		(0x01 << 1)     
#define MSP_PD_RST		(0x01 << 2)     
#define MSP_PP_RST		(0x01 << 3)     
#define MSP_EA_RST		(0x01 << 6)     
#define MSP_EB_RST		(0x01 << 7)     
#define MSP_SE_RST		(0x01 << 8)     
#define MSP_PB_RST		(0x01 << 9)     
#define MSP_EC_RST		(0x01 << 10)    
#define MSP_TW_RST		(0x01 << 11)    
#define MSP_SPI_RST		(0x01 << 12)    
#define MSP_U1_RST		(0x01 << 13)    
#define MSP_U0_RST		(0x01 << 14)    

#define MSP_BASE_BAUD		25000000
#define MSP_UART_REG_LEN	0x20

#define PCCARD_32		0x02    
#define SINGLE_PCCARD		0x01    


#define EXT_INT_POL(eirq)			(1 << (eirq + 8))
#define EXT_INT_EDGE(eirq)			(1 << eirq)

#define CIC_EXT_SET_TRIGGER_LEVEL(reg, eirq)	(reg &= ~EXT_INT_EDGE(eirq))
#define CIC_EXT_SET_TRIGGER_EDGE(reg, eirq)	(reg |= EXT_INT_EDGE(eirq))
#define CIC_EXT_SET_ACTIVE_HI(reg, eirq)	(reg |= EXT_INT_POL(eirq))
#define CIC_EXT_SET_ACTIVE_LO(reg, eirq)	(reg &= ~EXT_INT_POL(eirq))
#define CIC_EXT_SET_ACTIVE_RISING		CIC_EXT_SET_ACTIVE_HI
#define CIC_EXT_SET_ACTIVE_FALLING		CIC_EXT_SET_ACTIVE_LO

#define CIC_EXT_IS_TRIGGER_LEVEL(reg, eirq) \
				((reg & EXT_INT_EDGE(eirq)) == 0)
#define CIC_EXT_IS_TRIGGER_EDGE(reg, eirq)	(reg & EXT_INT_EDGE(eirq))
#define CIC_EXT_IS_ACTIVE_HI(reg, eirq)		(reg & EXT_INT_POL(eirq))
#define CIC_EXT_IS_ACTIVE_LO(reg, eirq) \
				((reg & EXT_INT_POL(eirq)) == 0)
#define CIC_EXT_IS_ACTIVE_RISING		CIC_EXT_IS_ACTIVE_HI
#define CIC_EXT_IS_ACTIVE_FALLING		CIC_EXT_IS_ACTIVE_LO


#define DDRC_CFG(n)		(n)
#define DDRC_DEBUG(n)		(0x04 + n)
#define DDRC_CTL(n)		(0x40 + n)

#define DDRC_INDIRECT_WRITE(reg, mask, value) \
({ \
	*MEM_SS_ADDR = (((mask) & 0xf) << 8) | ((reg) & 0xff); \
	*MEM_SS_DATA = (value); \
	*MEM_SS_WRITE = 1; \
})

#define SPI_MPI_RX_BUSY		0x00008000	
#define SPI_MPI_FIFO_EMPTY	0x00004000	
#define SPI_MPI_TX_BUSY		0x00002000	
#define SPI_MPI_FIFO_FULL	0x00001000	

#define SPI_MPI_RX_START	0x00000004	
#define SPI_MPI_FLUSH_Q		0x00000002	
#define SPI_MPI_TX_START	0x00000001	

#endif 
