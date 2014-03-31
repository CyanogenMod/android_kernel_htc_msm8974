/* MN10300 on-chip Real-Time Clock registers
 *
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */
#ifndef _ASM_RTC_REGS_H
#define _ASM_RTC_REGS_H

#include <asm/intctl-regs.h>

#ifdef __KERNEL__

#define RTSCR			__SYSREG(0xd8600000, u8) 
#define RTSAR			__SYSREG(0xd8600001, u8) 
#define RTMCR			__SYSREG(0xd8600002, u8) 
#define RTMAR			__SYSREG(0xd8600003, u8) 
#define RTHCR			__SYSREG(0xd8600004, u8) 
#define RTHAR			__SYSREG(0xd8600005, u8) 
#define RTDWCR			__SYSREG(0xd8600006, u8) 
#define RTDMCR			__SYSREG(0xd8600007, u8) 
#define RTMTCR			__SYSREG(0xd8600008, u8) 
#define RTYCR			__SYSREG(0xd8600009, u8) 

#define RTCRA			__SYSREG(0xd860000a, u8)
#define RTCRA_RS		0x0f	
#define RTCRA_RS_NONE		0x00	
#define RTCRA_RS_3_90625ms	0x01	
#define RTCRA_RS_7_8125ms	0x02	
#define RTCRA_RS_122_070us	0x03	
#define RTCRA_RS_244_141us	0x04	
#define RTCRA_RS_488_281us	0x05	
#define RTCRA_RS_976_5625us	0x06	
#define RTCRA_RS_1_953125ms	0x07	
#define RTCRA_RS_3_90624ms	0x08	
#define RTCRA_RS_7_8125ms_b	0x09	
#define RTCRA_RS_15_625ms	0x0a	
#define RTCRA_RS_31_25ms	0x0b	
#define RTCRA_RS_62_5ms		0x0c	
#define RTCRA_RS_125ms		0x0d	
#define RTCRA_RS_250ms		0x0e	
#define RTCRA_RS_500ms		0x0f	
#define RTCRA_DVR		0x40	
#define RTCRA_UIP		0x80	

#define RTCRB			__SYSREG(0xd860000b, u8) 
#define RTCRB_DSE		0x01	
#define RTCRB_TM		0x02	
#define RTCRB_TM_12HR		0x00	
#define RTCRB_TM_24HR		0x02	
#define RTCRB_DM		0x04	
#define RTCRB_DM_BCD		0x00	
#define RTCRB_DM_BINARY		0x04	
#define RTCRB_UIE		0x10	
#define RTCRB_AIE		0x20	
#define RTCRB_PIE		0x40	
#define RTCRB_SET		0x80	

#define RTSRC			__SYSREG(0xd860000c, u8) 
#define RTSRC_UF		0x10	
#define RTSRC_AF		0x20	
#define RTSRC_PF		0x40	
#define RTSRC_IRQF		0x80	

#define RTIRQ			32
#define RTICR			GxICR(RTIRQ)

#define RTC_PORT(x)		0xd8600000
#define RTC_ALWAYS_BCD		1	

#define CMOS_READ(addr)		__SYSREG(0xd8600000 + (addr), u8)
#define CMOS_WRITE(val, addr)	\
	do { __SYSREG(0xd8600000 + (addr), u8) = val; } while (0)

#define RTC_IRQ			RTIRQ

#endif 

#endif 
