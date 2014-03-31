/*
 * auxio.h:  Definitions and code for the Auxiliary I/O registers.
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 *
 * Refactoring for unified NCR/PCIO support 2002 Eric Brower (ebrower@usa.net)
 */
#ifndef _SPARC64_AUXIO_H
#define _SPARC64_AUXIO_H

#define AUXIO_AUX1_MASK		0xc0 
#define AUXIO_AUX1_FDENS	0x20 
#define AUXIO_AUX1_LTE 		0x08 
#define AUXIO_AUX1_MMUX		0x04 
#define AUXIO_AUX1_FTCNT	0x02 
#define AUXIO_AUX1_LED		0x01 

#define AUXIO_AUX2_MASK		0xdc 
#define AUXIO_AUX2_PFAILDET	0x20 
#define AUXIO_AUX2_PFAILCLR 	0x02 
#define AUXIO_AUX2_PWR_OFF	0x01 

#define AUXIO_PCIO_LED		0x01 

#define AUXIO_PCIO_CPWR_OFF	0x02 
#define AUXIO_PCIO_SPWR_OFF	0x01 

#ifndef __ASSEMBLY__

extern void __iomem *auxio_register;

#define AUXIO_LTE_ON	1
#define AUXIO_LTE_OFF	0

extern void auxio_set_lte(int on);

#define AUXIO_LED_ON	1
#define AUXIO_LED_OFF	0

extern void auxio_set_led(int on);

#endif 

#endif 
