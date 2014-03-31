/*
 * include/asm-sh/cpu-sh2/watchdog.h
 *
 * Copyright (C) 2002, 2003 Paul Mundt
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#ifndef __ASM_CPU_SH2_WATCHDOG_H
#define __ASM_CPU_SH2_WATCHDOG_H

#define WTCNT		0xfffffe80
#define WTCSR		0xfffffe80
#define RSTCSR		0xfffffe82

#define WTCNT_R		(WTCNT + 1)
#define RSTCSR_R	(RSTCSR + 1)

#define WTCSR_IOVF	0x80
#define WTCSR_WT	0x40
#define WTCSR_TME	0x20
#define WTCSR_RSTS	0x00

#define RSTCSR_RSTS	0x20

static inline __u8 sh_wdt_read_rstcsr(void)
{
	return __raw_readb(RSTCSR_R);
}

static inline void sh_wdt_write_rstcsr(__u8 val)
{
	__raw_writeb((WTCNT_HIGH << 8) | (__u16)val, RSTCSR);
}

#endif 

