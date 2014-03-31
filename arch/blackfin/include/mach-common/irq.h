/*
 * Common Blackfin IRQ definitions (i.e. the CEC)
 *
 * Copyright 2005-2011 Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later
 */

#ifndef _MACH_COMMON_IRQ_H_
#define _MACH_COMMON_IRQ_H_


#define IRQ_EMU			0	
#define IRQ_RST			1	
#define IRQ_NMI			2	
#define IRQ_EVX			3	
#define IRQ_UNUSED		4	
#define IRQ_HWERR		5	
#define IRQ_CORETMR		6	

#define BFIN_IRQ(x)		((x) + 7)

#define IVG7			7
#define IVG8			8
#define IVG9			9
#define IVG10			10
#define IVG11			11
#define IVG12			12
#define IVG13			13
#define IVG14			14
#define IVG15			15

#define NR_IRQS			(NR_MACH_IRQS + NR_SPARE_IRQS)

#endif
