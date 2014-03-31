/* MN10300 Reset controller and watchdog timer definitions
 *
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */

#ifndef _ASM_RESET_REGS_H
#define _ASM_RESET_REGS_H

#include <asm/cpu-regs.h>
#include <asm/exceptions.h>

#ifdef __KERNEL__

#define WDBC			__SYSREGC(0xc0001000, u8) 

#define WDCTR			__SYSREG(0xc0001002, u8)  
#define WDCTR_WDCK		0x07	
#define WDCTR_WDCK_256th	0x00	
#define WDCTR_WDCK_1024th	0x01	
#define WDCTR_WDCK_2048th	0x02	
#define WDCTR_WDCK_16384th	0x03	
#define WDCTR_WDCK_65536th	0x04	
#define WDCTR_WDRST		0x40	
#define WDCTR_WDCNE		0x80	

#define RSTCTR			__SYSREG(0xc0001004, u8) 
#define RSTCTR_CHIPRST		0x01	
#define RSTCTR_DBFRST		0x02	
#define RSTCTR_WDTRST		0x04	
#define RSTCTR_WDREN		0x08	

#ifndef __ASSEMBLY__

static inline void mn10300_proc_hard_reset(void)
{
	RSTCTR &= ~RSTCTR_CHIPRST;
	RSTCTR |= RSTCTR_CHIPRST;
}

extern unsigned int watchdog_alert_counter[];

extern void watchdog_go(void);
extern asmlinkage void watchdog_handler(void);
extern asmlinkage
void watchdog_interrupt(struct pt_regs *, enum exception_code);

#endif

#endif 

#endif 
