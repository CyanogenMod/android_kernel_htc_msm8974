/*
 * arch/arm/mach-at91/include/mach/at91x40.h
 *
 * (C) Copyright 2007, Greg Ungerer <gerg@snapgear.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef AT91X40_H
#define AT91X40_H

#define AT91X40_ID_USART0	2	
#define AT91X40_ID_USART1	3	
#define AT91X40_ID_TC0		4	
#define AT91X40_ID_TC1		5	
#define AT91X40_ID_TC2		6	
#define AT91X40_ID_WD		7	
#define AT91X40_ID_PIOA		8	

#define AT91X40_ID_IRQ0		16	
#define AT91X40_ID_IRQ1		17	
#define AT91X40_ID_IRQ2		18	

#define AT91_BASE_SYS	0xffc00000

#define AT91_EBI	0xffe00000	
#define AT91_SF		0xfff00000	
#define AT91_USART1	0xfffcc000	
#define AT91_USART0	0xfffd0000	
#define AT91_TC		0xfffe0000	
#define AT91_PIOA	0xffff0000	
#define AT91_PS		0xffff4000	
#define AT91_WD		0xffff8000	

#define AT91_DBGU_CIDR	(AT91_SF + 0)	
#define AT91_DBGU_EXID	(AT91_SF + 4)	

#define	AT91_PS_CR	(AT91_PS + 0)	
#define	AT91_PS_CR_CPU	(1 << 0)	

#endif 
