/*
 *  arch/arm/mach-vt8500/include/mach/vt8500_regs.h
 *
 *  Copyright (C) 2010 Alexey Charkov <alchark@gmail.com>
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef __ASM_ARM_ARCH_VT8500_REGS_H
#define __ASM_ARM_ARCH_VT8500_REGS_H


#define VT8500_REGS_START_PHYS	0xd8000000	
#define VT8500_REGS_START_VIRT	0xf8000000	

#define VT8500_DDR_BASE		0xd8000000	
#define VT8500_DMA_BASE		0xd8001000	
#define VT8500_SFLASH_BASE	0xd8002000	
#define VT8500_ETHER_BASE	0xd8004000	
#define VT8500_CIPHER_BASE	0xd8006000	
#define VT8500_USB_BASE		0xd8007800	
# define VT8500_EHCI_BASE	0xd8007900	
# define VT8500_UHCI_BASE	0xd8007b01	
#define VT8500_PATA_BASE	0xd8008000	
#define VT8500_PS2_BASE		0xd8008800	
#define VT8500_NAND_BASE	0xd8009000	
#define VT8500_NOR_BASE		0xd8009400	
#define VT8500_SDMMC_BASE	0xd800a000	
#define VT8500_MS_BASE		0xd800b000	
#define VT8500_LCDC_BASE	0xd800e400	
#define VT8500_VPU_BASE		0xd8050000	
#define VT8500_GOV_BASE		0xd8050300	
#define VT8500_GEGEA_BASE	0xd8050400	
#define VT8500_LCDF_BASE	0xd8050900	
#define VT8500_VID_BASE		0xd8050a00	
#define VT8500_VPP_BASE		0xd8050b00	
#define VT8500_TSBK_BASE	0xd80f4000	
#define VT8500_JPEGDEC_BASE	0xd80fe000	
#define VT8500_JPEGENC_BASE	0xd80ff000	
#define VT8500_RTC_BASE		0xd8100000	
#define VT8500_GPIO_BASE	0xd8110000	
#define VT8500_SCC_BASE		0xd8120000	
#define VT8500_PMC_BASE		0xd8130000	
#define VT8500_IC_BASE		0xd8140000	
#define VT8500_UART0_BASE	0xd8200000	
#define VT8500_UART2_BASE	0xd8210000	
#define VT8500_PWM_BASE		0xd8220000	
#define VT8500_SPI0_BASE	0xd8240000	
#define VT8500_SPI1_BASE	0xd8250000	
#define VT8500_CIR_BASE		0xd8270000	
#define VT8500_I2C0_BASE	0xd8280000	
#define VT8500_AC97_BASE	0xd8290000	
#define VT8500_SPI2_BASE	0xd82a0000	
#define VT8500_UART1_BASE	0xd82b0000	
#define VT8500_UART3_BASE	0xd82c0000	
#define VT8500_PCM_BASE		0xd82d0000	
#define VT8500_I2C1_BASE	0xd8320000	
#define VT8500_I2S_BASE		0xd8330000	
#define VT8500_ADC_BASE		0xd8340000	

#define VT8500_REGS_END_PHYS	0xd834ffff	
#define VT8500_REGS_LENGTH	(VT8500_REGS_END_PHYS \
				- VT8500_REGS_START_PHYS + 1)

#endif
