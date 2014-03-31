/*
 * arch/arm/mach-at91/include/mach/at91_pit.h
 *
 * Copyright (C) 2007 Andrew Victor
 * Copyright (C) 2007 Atmel Corporation.
 *
 * Periodic Interval Timer (PIT) - System peripherals regsters.
 * Based on AT91SAM9261 datasheet revision D.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef AT91_PIT_H
#define AT91_PIT_H

#define AT91_PIT_MR		0x00			
#define		AT91_PIT_PITIEN		(1 << 25)		
#define		AT91_PIT_PITEN		(1 << 24)		
#define		AT91_PIT_PIV		(0xfffff)		

#define AT91_PIT_SR		0x04			
#define		AT91_PIT_PITS		(1 << 0)		

#define AT91_PIT_PIVR		0x08			
#define AT91_PIT_PIIR		0x0c			
#define		AT91_PIT_PICNT		(0xfff << 20)		
#define		AT91_PIT_CPIV		(0xfffff)		

#endif
