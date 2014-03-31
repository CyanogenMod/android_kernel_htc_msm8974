/*
 * arch/arm/mach-at91/include/mach/at91rm9200_mc.h
 *
 * Copyright (C) 2005 Ivan Kokshaysky
 * Copyright (C) SAN People
 *
 * Memory Controllers (MC, EBI, SMC, SDRAMC, BFC) - System peripherals registers.
 * Based on AT91RM9200 datasheet revision E.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef AT91RM9200_MC_H
#define AT91RM9200_MC_H

#define AT91_MC_RCR		0x00			
#define		AT91_MC_RCB		(1 <<  0)		

#define AT91_MC_ASR		0x04			
#define		AT91_MC_UNADD		(1 <<  0)		
#define		AT91_MC_MISADD		(1 <<  1)		
#define		AT91_MC_ABTSZ		(3 <<  8)		
#define			AT91_MC_ABTSZ_BYTE		(0 << 8)
#define			AT91_MC_ABTSZ_HALFWORD		(1 << 8)
#define			AT91_MC_ABTSZ_WORD		(2 << 8)
#define		AT91_MC_ABTTYP		(3 << 10)		
#define			AT91_MC_ABTTYP_DATAREAD		(0 << 10)
#define			AT91_MC_ABTTYP_DATAWRITE	(1 << 10)
#define			AT91_MC_ABTTYP_FETCH		(2 << 10)
#define		AT91_MC_MST0		(1 << 16)		
#define		AT91_MC_MST1		(1 << 17)		
#define		AT91_MC_MST2		(1 << 18)		
#define		AT91_MC_MST3		(1 << 19)		
#define		AT91_MC_SVMST0		(1 << 24)		
#define		AT91_MC_SVMST1		(1 << 25)		
#define		AT91_MC_SVMST2		(1 << 26)		
#define		AT91_MC_SVMST3		(1 << 27)		

#define AT91_MC_AASR		0x08			

#define AT91_MC_MPR		0x0c			
#define		AT91_MPR_MSTP0		(7 <<  0)		
#define		AT91_MPR_MSTP1		(7 <<  4)		
#define		AT91_MPR_MSTP2		(7 <<  8)		
#define		AT91_MPR_MSTP3		(7 << 12)		

#define AT91_EBI_CSA		0x60			
#define		AT91_EBI_CS0A		(1 << 0)		
#define			AT91_EBI_CS0A_SMC		(0 << 0)
#define			AT91_EBI_CS0A_BFC		(1 << 0)
#define		AT91_EBI_CS1A		(1 << 1)		
#define			AT91_EBI_CS1A_SMC		(0 << 1)
#define			AT91_EBI_CS1A_SDRAMC		(1 << 1)
#define		AT91_EBI_CS3A		(1 << 3)		
#define			AT91_EBI_CS3A_SMC		(0 << 3)
#define			AT91_EBI_CS3A_SMC_SMARTMEDIA	(1 << 3)
#define		AT91_EBI_CS4A		(1 << 4)		
#define			AT91_EBI_CS4A_SMC		(0 << 4)
#define			AT91_EBI_CS4A_SMC_COMPACTFLASH	(1 << 4)
#define AT91_EBI_CFGR		(AT91_MC + 0x64)	
#define		AT91_EBI_DBPUC		(1 << 0)		

#define	AT91_SMC_CSR(n)		(0x70 + ((n) * 4))	
#define		AT91_SMC_NWS		(0x7f <<  0)		
#define			AT91_SMC_NWS_(x)	((x) << 0)
#define		AT91_SMC_WSEN		(1    <<  7)		
#define		AT91_SMC_TDF		(0xf  <<  8)		
#define			AT91_SMC_TDF_(x)	((x) << 8)
#define		AT91_SMC_BAT		(1    << 12)		
#define		AT91_SMC_DBW		(3    << 13)		
#define			AT91_SMC_DBW_16		(1 << 13)
#define			AT91_SMC_DBW_8		(2 << 13)
#define		AT91_SMC_DPR		(1 << 15)		
#define		AT91_SMC_ACSS		(3 << 16)		
#define			AT91_SMC_ACSS_STD	(0 << 16)
#define			AT91_SMC_ACSS_1		(1 << 16)
#define			AT91_SMC_ACSS_2		(2 << 16)
#define			AT91_SMC_ACSS_3		(3 << 16)
#define		AT91_SMC_RWSETUP	(7 << 24)		
#define			AT91_SMC_RWSETUP_(x)	((x) << 24)
#define		AT91_SMC_RWHOLD		(7 << 28)		
#define			AT91_SMC_RWHOLD_(x)	((x) << 28)

#define AT91_BFC_MR		0xc0			
#define		AT91_BFC_BFCOM		(3   <<  0)		
#define			AT91_BFC_BFCOM_DISABLED	(0 << 0)
#define			AT91_BFC_BFCOM_ASYNC	(1 << 0)
#define			AT91_BFC_BFCOM_BURST	(2 << 0)
#define		AT91_BFC_BFCC		(3   <<  2)		
#define			AT91_BFC_BFCC_MCK	(1 << 2)
#define			AT91_BFC_BFCC_DIV2	(2 << 2)
#define			AT91_BFC_BFCC_DIV4	(3 << 2)
#define		AT91_BFC_AVL		(0xf <<  4)		
#define		AT91_BFC_PAGES		(7   <<  8)		
#define			AT91_BFC_PAGES_NO_PAGE	(0 << 8)
#define			AT91_BFC_PAGES_16	(1 << 8)
#define			AT91_BFC_PAGES_32	(2 << 8)
#define			AT91_BFC_PAGES_64	(3 << 8)
#define			AT91_BFC_PAGES_128	(4 << 8)
#define			AT91_BFC_PAGES_256	(5 << 8)
#define			AT91_BFC_PAGES_512	(6 << 8)
#define			AT91_BFC_PAGES_1024	(7 << 8)
#define		AT91_BFC_OEL		(3   << 12)		
#define		AT91_BFC_BAAEN		(1   << 16)		
#define		AT91_BFC_BFOEH		(1   << 17)		
#define		AT91_BFC_MUXEN		(1   << 18)		
#define		AT91_BFC_RDYEN		(1   << 19)		

#endif
