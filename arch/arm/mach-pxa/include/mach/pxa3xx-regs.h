/*
 * arch/arm/mach-pxa/include/mach/pxa3xx-regs.h
 *
 * PXA3xx specific register definitions
 *
 * Copyright (C) 2007 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARCH_PXA3XX_REGS_H
#define __ASM_ARCH_PXA3XX_REGS_H

#include <mach/hardware.h>

#define OSCC           __REG(0x41350000)  

#define OSCC_PEN       (1 << 11)       


#define PMCR		__REG(0x40F50000)	
#define PSR		__REG(0x40F50004)	
#define PSPR		__REG(0x40F50008)	
#define PCFR		__REG(0x40F5000C)	
#define PWER		__REG(0x40F50010)	
#define PWSR		__REG(0x40F50014)	
#define PECR		__REG(0x40F50018)	
#define DCDCSR		__REG(0x40F50080)	
#define PVCR		__REG(0x40F50100)	
#define PCMD(x)		__REG(0x40F50110 + ((x) << 2))

#define ASCR		__REG(0x40f40000)	
#define ARSR		__REG(0x40f40004)	
#define AD3ER		__REG(0x40f40008)	
#define AD3SR		__REG(0x40f4000c)	
#define AD2D0ER		__REG(0x40f40010)	
#define AD2D0SR		__REG(0x40f40014)	
#define AD2D1ER		__REG(0x40f40018)	
#define AD2D1SR		__REG(0x40f4001c)	
#define AD1D0ER		__REG(0x40f40020)	
#define AD1D0SR		__REG(0x40f40024)	
#define AGENP		__REG(0x40f4002c)	
#define AD3R		__REG(0x40f40030)	
#define AD2R		__REG(0x40f40034)	
#define AD1R		__REG(0x40f40038)	

#define ASCR_RDH		(1 << 31)
#define ASCR_D1S		(1 << 2)
#define ASCR_D2S		(1 << 1)
#define ASCR_D3S		(1 << 0)

#define ARSR_GPR		(1 << 3)
#define ARSR_LPMR		(1 << 2)
#define ARSR_WDT		(1 << 1)
#define ARSR_HWR		(1 << 0)

#define ADXER_WRTC		(1 << 31)	
#define ADXER_WOST		(1 << 30)	
#define ADXER_WTSI		(1 << 29)	
#define ADXER_WUSBH		(1 << 28)	
#define ADXER_WUSB2		(1 << 26)	
#define ADXER_WMSL0		(1 << 24)	
#define ADXER_WDMUX3		(1 << 23)	
#define ADXER_WDMUX2		(1 << 22)	
#define ADXER_WKP		(1 << 21)	
#define ADXER_WUSIM1		(1 << 20)	
#define ADXER_WUSIM0		(1 << 19)	
#define ADXER_WOTG		(1 << 16)	
#define ADXER_MFP_WFLASH	(1 << 15)	
#define ADXER_MFP_GEN12		(1 << 14)	
#define ADXER_MFP_WMMC2		(1 << 13)	
#define ADXER_MFP_WMMC1		(1 << 12)	
#define ADXER_MFP_WI2C		(1 << 11)	
#define ADXER_MFP_WSSP4		(1 << 10)	
#define ADXER_MFP_WSSP3		(1 << 9)	
#define ADXER_MFP_WMAXTRIX	(1 << 8)	
#define ADXER_MFP_WUART3	(1 << 7)	
#define ADXER_MFP_WUART2	(1 << 6)	
#define ADXER_MFP_WUART1	(1 << 5)	
#define ADXER_MFP_WSSP2		(1 << 4)	
#define ADXER_MFP_WSSP1		(1 << 3)	
#define ADXER_MFP_WAC97		(1 << 2)	
#define ADXER_WEXTWAKE1		(1 << 1)	
#define ADXER_WEXTWAKE0		(1 << 0)	

#define ADXR_L2			(1 << 8)
#define ADXR_R5			(1 << 5)
#define ADXR_R4			(1 << 4)
#define ADXR_R3			(1 << 3)
#define ADXR_R2			(1 << 2)
#define ADXR_R1			(1 << 1)
#define ADXR_R0			(1 << 0)

#define PXA3xx_PM_S3D4C4	0x07	
#define PXA3xx_PM_S2D3C4	0x06	
#define PXA3xx_PM_S0D2C2	0x03	
#define PXA3xx_PM_S0D1C2	0x02	
#define PXA3xx_PM_S0D0C1	0x01

#define ACCR		__REG(0x41340000)	
#define ACSR		__REG(0x41340004)	
#define AICSR		__REG(0x41340008)	
#define CKENA		__REG(0x4134000C)	
#define CKENB		__REG(0x41340010)	
#define AC97_DIV	__REG(0x41340014)	

#define ACCR_XPDIS		(1 << 31)	
#define ACCR_SPDIS		(1 << 30)	
#define ACCR_D0CS		(1 << 26)	
#define ACCR_PCCE		(1 << 11)	
#define ACCR_DDR_D0CS		(1 << 7)	

#define ACCR_SMCFS_MASK		(0x7 << 23)	
#define ACCR_SFLFS_MASK		(0x3 << 18)	
#define ACCR_XSPCLK_MASK	(0x3 << 16)	
#define ACCR_HSS_MASK		(0x3 << 14)	
#define ACCR_DMCFS_MASK		(0x3 << 12)	
#define ACCR_XN_MASK		(0x7 << 8)	
#define ACCR_XL_MASK		(0x1f)		

#define ACCR_SMCFS(x)		(((x) & 0x7) << 23)
#define ACCR_SFLFS(x)		(((x) & 0x3) << 18)
#define ACCR_XSPCLK(x)		(((x) & 0x3) << 16)
#define ACCR_HSS(x)		(((x) & 0x3) << 14)
#define ACCR_DMCFS(x)		(((x) & 0x3) << 12)
#define ACCR_XN(x)		(((x) & 0x7) << 8)
#define ACCR_XL(x)		((x) & 0x1f)

#define CKEN_LCD	1	
#define CKEN_USBH	2	
#define CKEN_CAMERA	3	
#define CKEN_NAND	4	
#define CKEN_USB2	6	
#define CKEN_DMC	8	
#define CKEN_SMC	9	
#define CKEN_ISC	10	
#define CKEN_BOOT	11	
#define CKEN_MMC1	12	
#define CKEN_MMC2	13	
#define CKEN_KEYPAD	14	
#define CKEN_CIR	15	
#define CKEN_USIM0	17	
#define CKEN_USIM1	18	
#define CKEN_TPM	19	
#define CKEN_UDC	20	
#define CKEN_BTUART	21	
#define CKEN_FFUART	22	
#define CKEN_STUART	23	
#define CKEN_AC97	24	
#define CKEN_TOUCH	25	
#define CKEN_SSP1	26	
#define CKEN_SSP2	27	
#define CKEN_SSP3	28	
#define CKEN_SSP4	29	
#define CKEN_MSL0	30	
#define CKEN_PWM0	32	
#define CKEN_PWM1	33	
#define CKEN_I2C	36	
#define CKEN_INTC	38	
#define CKEN_GPIO	39	
#define CKEN_1WIRE	40	
#define CKEN_HSIO2	41	
#define CKEN_MINI_IM	48	
#define CKEN_MINI_LCD	49	

#define CKEN_MMC3	5	
#define CKEN_MVED	43	

#define CKEN_PXA300_GCU		42	
#define CKEN_PXA320_GCU		7	

#endif 
