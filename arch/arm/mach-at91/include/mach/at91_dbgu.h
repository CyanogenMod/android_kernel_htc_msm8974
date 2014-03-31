/*
 * arch/arm/mach-at91/include/mach/at91_dbgu.h
 *
 * Copyright (C) 2005 Ivan Kokshaysky
 * Copyright (C) SAN People
 *
 * Debug Unit (DBGU) - System peripherals registers.
 * Based on AT91RM9200 datasheet revision E.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef AT91_DBGU_H
#define AT91_DBGU_H

#define dbgu_readl(dbgu, field) \
	__raw_readl(AT91_VA_BASE_SYS + dbgu + AT91_DBGU_ ## field)

#if !defined(CONFIG_ARCH_AT91X40)
#define AT91_DBGU_CR		(0x00)	
#define AT91_DBGU_MR		(0x04)	
#define AT91_DBGU_IER		(0x08)	
#define		AT91_DBGU_TXRDY		(1 << 1)		
#define		AT91_DBGU_TXEMPTY	(1 << 9)		
#define AT91_DBGU_IDR		(0x0c)	
#define AT91_DBGU_IMR		(0x10)	
#define AT91_DBGU_SR		(0x14)	
#define AT91_DBGU_RHR		(0x18)	
#define AT91_DBGU_THR		(0x1c)	
#define AT91_DBGU_BRGR		(0x20)	

#define AT91_DBGU_CIDR		(0x40)	
#define AT91_DBGU_EXID		(0x44)	
#define AT91_DBGU_FNR		(0x48)	
#define		AT91_DBGU_FNTRST	(1 << 0)		

#endif 

#define		AT91_CIDR_VERSION	(0x1f << 0)		
#define		AT91_CIDR_EPROC		(7    << 5)		
#define		AT91_CIDR_NVPSIZ	(0xf  << 8)		
#define		AT91_CIDR_NVPSIZ2	(0xf  << 12)		
#define		AT91_CIDR_SRAMSIZ	(0xf  << 16)		
#define			AT91_CIDR_SRAMSIZ_1K	(1 << 16)
#define			AT91_CIDR_SRAMSIZ_2K	(2 << 16)
#define			AT91_CIDR_SRAMSIZ_112K	(4 << 16)
#define			AT91_CIDR_SRAMSIZ_4K	(5 << 16)
#define			AT91_CIDR_SRAMSIZ_80K	(6 << 16)
#define			AT91_CIDR_SRAMSIZ_160K	(7 << 16)
#define			AT91_CIDR_SRAMSIZ_8K	(8 << 16)
#define			AT91_CIDR_SRAMSIZ_16K	(9 << 16)
#define			AT91_CIDR_SRAMSIZ_32K	(10 << 16)
#define			AT91_CIDR_SRAMSIZ_64K	(11 << 16)
#define			AT91_CIDR_SRAMSIZ_128K	(12 << 16)
#define			AT91_CIDR_SRAMSIZ_256K	(13 << 16)
#define			AT91_CIDR_SRAMSIZ_96K	(14 << 16)
#define			AT91_CIDR_SRAMSIZ_512K	(15 << 16)
#define		AT91_CIDR_ARCH		(0xff << 20)		
#define		AT91_CIDR_NVPTYP	(7    << 28)		
#define		AT91_CIDR_EXT		(1    << 31)		

#endif
