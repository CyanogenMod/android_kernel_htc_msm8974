/*
 * This file contains the hardware definitions of the Nomadik.
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
 * YOU should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */
#ifndef __ASM_ARCH_HARDWARE_H
#define __ASM_ARCH_HARDWARE_H

#define NOMADIK_IO_VIRTUAL	0xF0000000	
#define NOMADIK_IO_PHYSICAL	0x10000000	
#define NOMADIK_IO_SIZE		0x00300000	

#define io_p2v(x) ((void __iomem *)(x) \
			- NOMADIK_IO_PHYSICAL + NOMADIK_IO_VIRTUAL)
#define io_v2p(x) ((unsigned long)(x) \
			- NOMADIK_IO_VIRTUAL + NOMADIK_IO_PHYSICAL)

#define IO_ADDRESS(x) ((x) - NOMADIK_IO_PHYSICAL + NOMADIK_IO_VIRTUAL)

#define NOMADIK_FSMC_BASE	0x10100000	
#define NOMADIK_SDRAMC_BASE	0x10110000	
#define NOMADIK_CLCDC_BASE	0x10120000	
#define NOMADIK_MDIF_BASE	0x10120000	
#define NOMADIK_DMA0_BASE	0x10130000	
#define NOMADIK_IC_BASE		0x10140000	
#define NOMADIK_DMA1_BASE	0x10150000	
#define NOMADIK_USB_BASE	0x10170000	
#define NOMADIK_CRYP_BASE	0x10180000	
#define NOMADIK_SHA1_BASE	0x10190000	
#define NOMADIK_XTI_BASE	0x101A0000	
#define NOMADIK_RNG_BASE	0x101B0000	
#define NOMADIK_SRC_BASE	0x101E0000	
#define NOMADIK_WDOG_BASE	0x101E1000	
#define NOMADIK_MTU0_BASE	0x101E2000	
#define NOMADIK_MTU1_BASE	0x101E3000	
#define NOMADIK_GPIO0_BASE	0x101E4000	
#define NOMADIK_GPIO1_BASE	0x101E5000	
#define NOMADIK_GPIO2_BASE	0x101E6000	
#define NOMADIK_GPIO3_BASE	0x101E7000	
#define NOMADIK_RTC_BASE	0x101E8000	
#define NOMADIK_PMU_BASE	0x101E9000	
#define NOMADIK_OWM_BASE	0x101EA000	
#define NOMADIK_SCR_BASE	0x101EF000	
#define NOMADIK_MSP2_BASE	0x101F0000	
#define NOMADIK_MSP1_BASE	0x101F1000	
#define NOMADIK_UART2_BASE	0x101F2000	
#define NOMADIK_SSIRx_BASE	0x101F3000	
#define NOMADIK_SSITx_BASE	0x101F4000	
#define NOMADIK_MSHC_BASE	0x101F5000	
#define NOMADIK_SDI_BASE	0x101F6000	
#define NOMADIK_I2C1_BASE	0x101F7000	
#define NOMADIK_I2C0_BASE	0x101F8000	
#define NOMADIK_MSP0_BASE	0x101F9000	
#define NOMADIK_FIRDA_BASE	0x101FA000	
#define NOMADIK_UART1_BASE	0x101FB000	
#define NOMADIK_SSP_BASE	0x101FC000	
#define NOMADIK_UART0_BASE	0x101FD000	
#define NOMADIK_SGA_BASE	0x101FE000	
#define NOMADIK_L2CC_BASE	0x10210000	

#define NOMADIK_BACKUP_RAM	0x80010000
#define NOMADIK_EBROM		0x80000000	
#define NOMADIK_HAMACV_DMEM_BASE 0xA0100000	
#define NOMADIK_HAMACV_DMEM_END	0xA01FFFFF	
#define NOMADIK_HAMACA_DMEM	0xA0200000	

#define NOMADIK_FSMC_VA		IO_ADDRESS(NOMADIK_FSMC_BASE)
#define NOMADIK_MTU0_VA		IO_ADDRESS(NOMADIK_MTU0_BASE)
#define NOMADIK_MTU1_VA		IO_ADDRESS(NOMADIK_MTU1_BASE)

#endif 
