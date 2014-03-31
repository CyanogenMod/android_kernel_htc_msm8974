/*
 * Header file for the Atmel DDR/SDR SDRAM Controller
 *
 * Copyright (C) 2010 Atmel Corporation
 *	Nicolas Ferre <nicolas.ferre@atmel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef AT91SAM9_DDRSDR_H
#define AT91SAM9_DDRSDR_H

#define AT91_DDRSDRC_MR		0x00	
#define		AT91_DDRSDRC_MODE	(0x7 << 0)		
#define			AT91_DDRSDRC_MODE_NORMAL	0
#define			AT91_DDRSDRC_MODE_NOP		1
#define			AT91_DDRSDRC_MODE_PRECHARGE	2
#define			AT91_DDRSDRC_MODE_LMR		3
#define			AT91_DDRSDRC_MODE_REFRESH	4
#define			AT91_DDRSDRC_MODE_EXT_LMR	5
#define			AT91_DDRSDRC_MODE_DEEP		6

#define AT91_DDRSDRC_RTR	0x04	
#define		AT91_DDRSDRC_COUNT	(0xfff << 0)		

#define AT91_DDRSDRC_CR		0x08	
#define		AT91_DDRSDRC_NC		(3 << 0)		
#define			AT91_DDRSDRC_NC_SDR8	(0 << 0)
#define			AT91_DDRSDRC_NC_SDR9	(1 << 0)
#define			AT91_DDRSDRC_NC_SDR10	(2 << 0)
#define			AT91_DDRSDRC_NC_SDR11	(3 << 0)
#define			AT91_DDRSDRC_NC_DDR9	(0 << 0)
#define			AT91_DDRSDRC_NC_DDR10	(1 << 0)
#define			AT91_DDRSDRC_NC_DDR11	(2 << 0)
#define			AT91_DDRSDRC_NC_DDR12	(3 << 0)
#define		AT91_DDRSDRC_NR		(3 << 2)		
#define			AT91_DDRSDRC_NR_11	(0 << 2)
#define			AT91_DDRSDRC_NR_12	(1 << 2)
#define			AT91_DDRSDRC_NR_13	(2 << 2)
#define			AT91_DDRSDRC_NR_14	(3 << 2)
#define		AT91_DDRSDRC_CAS	(7 << 4)		
#define			AT91_DDRSDRC_CAS_2	(2 << 4)
#define			AT91_DDRSDRC_CAS_3	(3 << 4)
#define			AT91_DDRSDRC_CAS_25	(6 << 4)
#define		AT91_DDRSDRC_RST_DLL	(1 << 7)		
#define		AT91_DDRSDRC_DICDS	(1 << 8)		
#define		AT91_DDRSDRC_DIS_DLL	(1 << 9)		
#define		AT91_DDRSDRC_OCD	(1 << 12)		
#define		AT91_DDRSDRC_DQMS	(1 << 16)		
#define		AT91_DDRSDRC_ACTBST	(1 << 18)		

#define AT91_DDRSDRC_T0PR	0x0C	
#define		AT91_DDRSDRC_TRAS	(0xf <<  0)		
#define		AT91_DDRSDRC_TRCD	(0xf <<  4)		
#define		AT91_DDRSDRC_TWR	(0xf <<  8)		
#define		AT91_DDRSDRC_TRC	(0xf << 12)		
#define		AT91_DDRSDRC_TRP	(0xf << 16)		
#define		AT91_DDRSDRC_TRRD	(0xf << 20)		
#define		AT91_DDRSDRC_TWTR	(0x7 << 24)		
#define		AT91_DDRSDRC_RED_WRRD	(0x1 << 27)		
#define		AT91_DDRSDRC_TMRD	(0xf << 28)		

#define AT91_DDRSDRC_T1PR	0x10	
#define		AT91_DDRSDRC_TRFC	(0x1f << 0)		
#define		AT91_DDRSDRC_TXSNR	(0xff << 8)		
#define		AT91_DDRSDRC_TXSRD	(0xff << 16)		
#define		AT91_DDRSDRC_TXP	(0xf  << 24)		

#define AT91_DDRSDRC_T2PR	0x14	
#define		AT91_DDRSDRC_TXARD	(0xf  << 0)		
#define		AT91_DDRSDRC_TXARDS	(0xf  << 4)		
#define		AT91_DDRSDRC_TRPA	(0xf  << 8)		
#define		AT91_DDRSDRC_TRTP	(0x7  << 12)		

#define AT91_DDRSDRC_LPR	0x1C	
#define		AT91_DDRSDRC_LPCB	(3 << 0)		
#define			AT91_DDRSDRC_LPCB_DISABLE		0
#define			AT91_DDRSDRC_LPCB_SELF_REFRESH		1
#define			AT91_DDRSDRC_LPCB_POWER_DOWN		2
#define			AT91_DDRSDRC_LPCB_DEEP_POWER_DOWN	3
#define		AT91_DDRSDRC_CLKFR	(1 << 2)	
#define		AT91_DDRSDRC_PASR	(7 << 4)	
#define		AT91_DDRSDRC_TCSR	(3 << 8)	
#define		AT91_DDRSDRC_DS		(3 << 10)	
#define		AT91_DDRSDRC_TIMEOUT	(3 << 12)	
#define			AT91_DDRSDRC_TIMEOUT_0_CLK_CYCLES	(0 << 12)
#define			AT91_DDRSDRC_TIMEOUT_64_CLK_CYCLES	(1 << 12)
#define			AT91_DDRSDRC_TIMEOUT_128_CLK_CYCLES	(2 << 12)
#define		AT91_DDRSDRC_APDE	(1 << 16)	 
#define		AT91_DDRSDRC_UPD_MR	(3 << 20)	 

#define AT91_DDRSDRC_MDR	0x20	
#define		AT91_DDRSDRC_MD		(3 << 0)		
#define			AT91_DDRSDRC_MD_SDR		0
#define			AT91_DDRSDRC_MD_LOW_POWER_SDR	1
#define			AT91_DDRSDRC_MD_LOW_POWER_DDR	3
#define			AT91_DDRSDRC_MD_DDR2		6	
#define		AT91_DDRSDRC_DBW	(1 << 4)		
#define			AT91_DDRSDRC_DBW_32BITS		(0 <<  4)
#define			AT91_DDRSDRC_DBW_16BITS		(1 <<  4)

#define AT91_DDRSDRC_DLL	0x24	
#define		AT91_DDRSDRC_MDINC	(1 << 0)		
#define		AT91_DDRSDRC_MDDEC	(1 << 1)		
#define		AT91_DDRSDRC_MDOVF	(1 << 2)		
#define		AT91_DDRSDRC_MDVAL	(0xff <<  8)		

#define AT91_DDRSDRC_HS		0x2C	
#define		AT91_DDRSDRC_DIS_ATCP_RD	(1 << 2)	

#define AT91_DDRSDRC_DELAY(n)	(0x30 + (0x4 * (n)))	

#define AT91_DDRSDRC_WPMR	0xE4	
#define		AT91_DDRSDRC_WP		(1 << 0)		
#define		AT91_DDRSDRC_WPKEY	(0xffffff << 8)		
#define		AT91_DDRSDRC_KEY	(0x444452 << 8)		

#define AT91_DDRSDRC_WPSR	0xE8	
#define		AT91_DDRSDRC_WPVS	(1 << 0)		
#define		AT91_DDRSDRC_WPVSRC	(0xffff << 8)		

#endif
