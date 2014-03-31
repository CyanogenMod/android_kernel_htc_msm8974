/*
 * arch/arm/mach-at91/include/mach/at91_rtt.h
 *
 * Copyright (C) 2007 Andrew Victor
 * Copyright (C) 2007 Atmel Corporation.
 *
 * Real-time Timer (RTT) - System peripherals regsters.
 * Based on AT91SAM9261 datasheet revision D.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef AT91_RTT_H
#define AT91_RTT_H

#define AT91_RTT_MR		0x00			
#define		AT91_RTT_RTPRES		(0xffff << 0)		
#define		AT91_RTT_ALMIEN		(1 << 16)		
#define		AT91_RTT_RTTINCIEN	(1 << 17)		
#define		AT91_RTT_RTTRST		(1 << 18)		

#define AT91_RTT_AR		0x04			
#define		AT91_RTT_ALMV		(0xffffffff)		

#define AT91_RTT_VR		0x08			
#define		AT91_RTT_CRTV		(0xffffffff)		

#define AT91_RTT_SR		0x0c			
#define		AT91_RTT_ALMS		(1 << 0)		
#define		AT91_RTT_RTTINC		(1 << 1)		

#endif
