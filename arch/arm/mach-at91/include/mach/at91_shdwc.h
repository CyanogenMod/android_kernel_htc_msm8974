/*
 * arch/arm/mach-at91/include/mach/at91_shdwc.h
 *
 * Copyright (C) 2007 Andrew Victor
 * Copyright (C) 2007 Atmel Corporation.
 *
 * Shutdown Controller (SHDWC) - System peripherals regsters.
 * Based on AT91SAM9261 datasheet revision D.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef AT91_SHDWC_H
#define AT91_SHDWC_H

#ifndef __ASSEMBLY__
extern void __iomem *at91_shdwc_base;

#define at91_shdwc_read(field) \
	__raw_readl(at91_shdwc_base + field)

#define at91_shdwc_write(field, value) \
	__raw_writel(value, at91_shdwc_base + field);
#endif

#define AT91_SHDW_CR		0x00			
#define		AT91_SHDW_SHDW		(1    << 0)		
#define		AT91_SHDW_KEY		(0xa5 << 24)		

#define AT91_SHDW_MR		0x04			
#define		AT91_SHDW_WKMODE0	(3 << 0)		
#define			AT91_SHDW_WKMODE0_NONE		0
#define			AT91_SHDW_WKMODE0_HIGH		1
#define			AT91_SHDW_WKMODE0_LOW		2
#define			AT91_SHDW_WKMODE0_ANYLEVEL	3
#define		AT91_SHDW_CPTWK0_MAX	0xf			
#define		AT91_SHDW_CPTWK0	(AT91_SHDW_CPTWK0_MAX << 4) 
#define			AT91_SHDW_CPTWK0_(x)	((x) << 4)
#define		AT91_SHDW_RTTWKEN	(1   << 16)		
#define		AT91_SHDW_RTCWKEN	(1   << 17)		

#define AT91_SHDW_SR		0x08			
#define		AT91_SHDW_WAKEUP0	(1 <<  0)		
#define		AT91_SHDW_RTTWK		(1 << 16)		
#define		AT91_SHDW_RTCWK		(1 << 17)		

#endif
