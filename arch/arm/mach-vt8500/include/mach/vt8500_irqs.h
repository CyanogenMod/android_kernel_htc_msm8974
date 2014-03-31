/*
 *  arch/arm/mach-vt8500/include/mach/vt8500_irqs.h
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


#define IRQ_JPEGENC	0	
#define IRQ_JPEGDEC	1	
				
#define IRQ_PATA	3	
				
#define IRQ_DMA		5	
#define IRQ_EXT0	6	
#define IRQ_EXT1	7	
#define IRQ_GE		8	
#define IRQ_GOV		9	
#define IRQ_ETHER	10	
#define IRQ_MPEGTS	11	
#define IRQ_LCDC	12	
#define IRQ_EXT2	13	
#define IRQ_EXT3	14	
#define IRQ_EXT4	15	
#define IRQ_CIPHER	16	
#define IRQ_VPP		17	
#define IRQ_I2C1	18	
#define IRQ_I2C0	19	
#define IRQ_SDMMC	20	
#define IRQ_SDMMC_DMA	21	
#define IRQ_PMC_WU	22	
				
#define IRQ_SPI0	24	
#define IRQ_SPI1	25	
#define IRQ_SPI2	26	
#define IRQ_LCDDF	27	
#define IRQ_NAND	28	
#define IRQ_NAND_DMA	29	
#define IRQ_MS		30	
#define IRQ_MS_DMA	31	
#define IRQ_UART0	32	
#define IRQ_UART1	33	
#define IRQ_I2S		34	
#define IRQ_PCM		35	
#define IRQ_PMCOS0	36	
#define IRQ_PMCOS1	37	
#define IRQ_PMCOS2	38	
#define IRQ_PMCOS3	39	
#define IRQ_VPU		40	
#define IRQ_VID		41	
#define IRQ_AC97	42	
#define IRQ_EHCI	43	
#define IRQ_NOR		44	
#define IRQ_PS2MOUSE	45	
#define IRQ_PS2KBD	46	
#define IRQ_UART2	47	
#define IRQ_RTC		48	
#define IRQ_RTCSM	49	
#define IRQ_UART3	50	
#define IRQ_ADC		51	
#define IRQ_EXT5	52	
#define IRQ_EXT6	53	
#define IRQ_EXT7	54	
#define IRQ_CIR		55	
#define IRQ_DMA0	56	
#define IRQ_DMA1	57	
#define IRQ_DMA2	58	
#define IRQ_DMA3	59	
#define IRQ_DMA4	60	
#define IRQ_DMA5	61	
#define IRQ_DMA6	62	
#define IRQ_DMA7	63	

#define VT8500_NR_IRQS		64
