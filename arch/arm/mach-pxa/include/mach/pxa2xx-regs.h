/*
 *  arch/arm/mach-pxa/include/mach/pxa2xx-regs.h
 *
 *  Taken from pxa-regs.h by Russell King
 *
 *  Author:	Nicolas Pitre
 *  Copyright:	MontaVista Software Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PXA2XX_REGS_H
#define __PXA2XX_REGS_H

#include <mach/hardware.h>


#define PMCR		__REG(0x40F00000)  
#define PSSR		__REG(0x40F00004)  
#define PSPR		__REG(0x40F00008)  
#define PWER		__REG(0x40F0000C)  
#define PRER		__REG(0x40F00010)  
#define PFER		__REG(0x40F00014)  
#define PEDR		__REG(0x40F00018)  
#define PCFR		__REG(0x40F0001C)  
#define PGSR0		__REG(0x40F00020)  
#define PGSR1		__REG(0x40F00024)  
#define PGSR2		__REG(0x40F00028)  
#define PGSR3		__REG(0x40F0002C)  
#define RCSR		__REG(0x40F00030)  

#define PSLR		__REG(0x40F00034)	
#define PSTR		__REG(0x40F00038)	
#define PSNR		__REG(0x40F0003C)	
#define PVCR		__REG(0x40F00040)	
#define PKWR		__REG(0x40F00050)	
#define PKSR		__REG(0x40F00054)	
#define PCMD(x)	__REG2(0x40F00080, (x)<<2)
#define PCMD0	__REG(0x40F00080 + 0 * 4)
#define PCMD1	__REG(0x40F00080 + 1 * 4)
#define PCMD2	__REG(0x40F00080 + 2 * 4)
#define PCMD3	__REG(0x40F00080 + 3 * 4)
#define PCMD4	__REG(0x40F00080 + 4 * 4)
#define PCMD5	__REG(0x40F00080 + 5 * 4)
#define PCMD6	__REG(0x40F00080 + 6 * 4)
#define PCMD7	__REG(0x40F00080 + 7 * 4)
#define PCMD8	__REG(0x40F00080 + 8 * 4)
#define PCMD9	__REG(0x40F00080 + 9 * 4)
#define PCMD10	__REG(0x40F00080 + 10 * 4)
#define PCMD11	__REG(0x40F00080 + 11 * 4)
#define PCMD12	__REG(0x40F00080 + 12 * 4)
#define PCMD13	__REG(0x40F00080 + 13 * 4)
#define PCMD14	__REG(0x40F00080 + 14 * 4)
#define PCMD15	__REG(0x40F00080 + 15 * 4)
#define PCMD16	__REG(0x40F00080 + 16 * 4)
#define PCMD17	__REG(0x40F00080 + 17 * 4)
#define PCMD18	__REG(0x40F00080 + 18 * 4)
#define PCMD19	__REG(0x40F00080 + 19 * 4)
#define PCMD20	__REG(0x40F00080 + 20 * 4)
#define PCMD21	__REG(0x40F00080 + 21 * 4)
#define PCMD22	__REG(0x40F00080 + 22 * 4)
#define PCMD23	__REG(0x40F00080 + 23 * 4)
#define PCMD24	__REG(0x40F00080 + 24 * 4)
#define PCMD25	__REG(0x40F00080 + 25 * 4)
#define PCMD26	__REG(0x40F00080 + 26 * 4)
#define PCMD27	__REG(0x40F00080 + 27 * 4)
#define PCMD28	__REG(0x40F00080 + 28 * 4)
#define PCMD29	__REG(0x40F00080 + 29 * 4)
#define PCMD30	__REG(0x40F00080 + 30 * 4)
#define PCMD31	__REG(0x40F00080 + 31 * 4)

#define PCMD_MBC	(1<<12)
#define PCMD_DCE	(1<<11)
#define PCMD_LC	(1<<10)
#define PCMD_SQC	(3<<8)	
#define PVCR_VCSA	(0x1<<14)
#define PVCR_CommandDelay (0xf80)
#define PCFR_PI2C_EN	(0x1 << 6)

#define PSSR_OTGPH	(1 << 6)	
#define PSSR_RDH	(1 << 5)	
#define PSSR_PH		(1 << 4)	
#define PSSR_STS	(1 << 3)	
#define PSSR_VFS	(1 << 2)	
#define PSSR_BFS	(1 << 1)	
#define PSSR_SSS	(1 << 0)	

#define PSLR_SL_ROD	(1 << 20)	

#define PCFR_RO		(1 << 15)	
#define PCFR_PO		(1 << 14)	
#define PCFR_GPROD	(1 << 12)	
#define PCFR_L1_EN	(1 << 11)	
#define PCFR_FVC	(1 << 10)	
#define PCFR_DC_EN	(1 << 7)	
#define PCFR_PI2CEN	(1 << 6)	
#define PCFR_GPR_EN	(1 << 4)	
#define PCFR_DS		(1 << 3)	
#define PCFR_FS		(1 << 2)	
#define PCFR_FP		(1 << 1)	
#define PCFR_OPDE	(1 << 0)	

#define RCSR_GPR	(1 << 3)	
#define RCSR_SMR	(1 << 2)	
#define RCSR_WDR	(1 << 1)	
#define RCSR_HWR	(1 << 0)	

#define PWER_GPIO(Nb)	(1 << Nb)	
#define PWER_GPIO0	PWER_GPIO (0)	
#define PWER_GPIO1	PWER_GPIO (1)	
#define PWER_GPIO2	PWER_GPIO (2)	
#define PWER_GPIO3	PWER_GPIO (3)	
#define PWER_GPIO4	PWER_GPIO (4)	
#define PWER_GPIO5	PWER_GPIO (5)	
#define PWER_GPIO6	PWER_GPIO (6)	
#define PWER_GPIO7	PWER_GPIO (7)	
#define PWER_GPIO8	PWER_GPIO (8)	
#define PWER_GPIO9	PWER_GPIO (9)	
#define PWER_GPIO10	PWER_GPIO (10)	
#define PWER_GPIO11	PWER_GPIO (11)	
#define PWER_GPIO12	PWER_GPIO (12)	
#define PWER_GPIO13	PWER_GPIO (13)	
#define PWER_GPIO14	PWER_GPIO (14)	
#define PWER_GPIO15	PWER_GPIO (15)	
#define PWER_RTC	0x80000000	

#define CCCR		__REG(0x41300000)  
#define CCSR		__REG(0x4130000C)  
#define CKEN		__REG(0x41300004)  
#define OSCC		__REG(0x41300008)  

#define CCCR_N_MASK	0x0380	
#define CCCR_M_MASK	0x0060	
#define CCCR_L_MASK	0x001f	

#define CKEN_AC97CONF   (31)    
#define CKEN_CAMERA	(24)	
#define CKEN_SSP1	(23)	
#define CKEN_MEMC	(22)	
#define CKEN_MEMSTK	(21)	
#define CKEN_IM		(20)	
#define CKEN_KEYPAD	(19)	
#define CKEN_USIM	(18)	
#define CKEN_MSL	(17)	
#define CKEN_LCD	(16)	
#define CKEN_PWRI2C	(15)	
#define CKEN_I2C	(14)	
#define CKEN_FICP	(13)	
#define CKEN_MMC	(12)	
#define CKEN_USB	(11)	
#define CKEN_ASSP	(10)	
#define CKEN_USBHOST	(10)	
#define CKEN_OSTIMER	(9)	
#define CKEN_NSSP	(9)	
#define CKEN_I2S	(8)	
#define CKEN_BTUART	(7)	
#define CKEN_FFUART	(6)	
#define CKEN_STUART	(5)	
#define CKEN_HWUART	(4)	
#define CKEN_SSP3	(4)	
#define CKEN_SSP	(3)	
#define CKEN_SSP2	(3)	
#define CKEN_AC97	(2)	
#define CKEN_PWM1	(1)	
#define CKEN_PWM0	(0)	

#define OSCC_OON	(1 << 1)	
#define OSCC_OOK	(1 << 0)	


#define PWRMODE_IDLE		0x1
#define PWRMODE_STANDBY		0x2
#define PWRMODE_SLEEP		0x3
#define PWRMODE_DEEPSLEEP	0x7

#endif
