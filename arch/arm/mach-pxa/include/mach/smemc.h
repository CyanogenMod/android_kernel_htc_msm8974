/*
 * Static memory controller register definitions for PXA CPUs
 *
 * Copyright (C) 2010 Marek Vasut <marek.vasut@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SMEMC_REGS_H
#define __SMEMC_REGS_H

#define PXA2XX_SMEMC_BASE	0x48000000
#define PXA3XX_SMEMC_BASE	0x4a000000
#define SMEMC_VIRT		IOMEM(0xf6000000)

#define MDCNFG		(SMEMC_VIRT + 0x00)  
#define MDREFR		(SMEMC_VIRT + 0x04)  
#define MSC0		(SMEMC_VIRT + 0x08)  
#define MSC1		(SMEMC_VIRT + 0x0C)  
#define MSC2		(SMEMC_VIRT + 0x10)  
#define MECR		(SMEMC_VIRT + 0x14)  
#define SXLCR		(SMEMC_VIRT + 0x18)  /* LCR value to be written to SDRAM-Timing Synchronous Flash */
#define SXCNFG		(SMEMC_VIRT + 0x1C)  
#define SXMRS		(SMEMC_VIRT + 0x24)  /* MRS value to be written to Synchronous Flash or SMROM */
#define MCMEM0		(SMEMC_VIRT + 0x28)  
#define MCMEM1		(SMEMC_VIRT + 0x2C)  
#define MCATT0		(SMEMC_VIRT + 0x30)  
#define MCATT1		(SMEMC_VIRT + 0x34)  
#define MCIO0		(SMEMC_VIRT + 0x38)  
#define MCIO1		(SMEMC_VIRT + 0x3C)  
#define MDMRS		(SMEMC_VIRT + 0x40)  /* MRS value to be written to SDRAM */
#define BOOT_DEF	(SMEMC_VIRT + 0x44)  
#define MEMCLKCFG	(SMEMC_VIRT + 0x68)  
#define CSADRCFG0	(SMEMC_VIRT + 0x80)  
#define CSADRCFG1	(SMEMC_VIRT + 0x84)  
#define CSADRCFG2	(SMEMC_VIRT + 0x88)  
#define CSADRCFG3	(SMEMC_VIRT + 0x8C)  

#define MCMEM(s)	(SMEMC_VIRT + 0x28 + ((s)<<2))  
#define MCATT(s)	(SMEMC_VIRT + 0x30 + ((s)<<2))  
#define MCIO(s)		(SMEMC_VIRT + 0x38 + ((s)<<2))  

#define MECR_NOS	(1 << 0)	
#define MECR_CIT	(1 << 1)	

#define MDCNFG_DE0	(1 << 0)	
#define MDCNFG_DE1	(1 << 1)	
#define MDCNFG_DE2	(1 << 16)	
#define MDCNFG_DE3	(1 << 17)	

#define MDREFR_K0DB4	(1 << 29)	
#define MDREFR_K2FREE	(1 << 25)	
#define MDREFR_K1FREE	(1 << 24)	
#define MDREFR_K0FREE	(1 << 23)	
#define MDREFR_SLFRSH	(1 << 22)	
#define MDREFR_APD	(1 << 20)	
#define MDREFR_K2DB2	(1 << 19)	
#define MDREFR_K2RUN	(1 << 18)	
#define MDREFR_K1DB2	(1 << 17)	
#define MDREFR_K1RUN	(1 << 16)	
#define MDREFR_E1PIN	(1 << 15)	
#define MDREFR_K0DB2	(1 << 14)	
#define MDREFR_K0RUN	(1 << 13)	
#define MDREFR_E0PIN	(1 << 12)	

#endif
