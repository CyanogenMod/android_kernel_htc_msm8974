/*
 * include/asm-sh/watchdog.h
 *
 * Copyright (C) 2002, 2003 Paul Mundt
 * Copyright (C) 2009 Siemens AG
 * Copyright (C) 2009 Valentin Sitdikov
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */
#ifndef __ASM_SH_WATCHDOG_H
#define __ASM_SH_WATCHDOG_H
#ifdef __KERNEL__

#include <linux/types.h>
#include <linux/io.h>

#define WTCNT_HIGH	0x5a
#define WTCSR_HIGH	0xa5

#define WTCSR_CKS2	0x04
#define WTCSR_CKS1	0x02
#define WTCSR_CKS0	0x01

#include <cpu/watchdog.h>

#ifndef WTCNT_R
#  define WTCNT_R	WTCNT
#endif

#ifndef WTCSR_R
#  define WTCSR_R	WTCSR
#endif

#define WTCSR_CKS_32	0x00
#define WTCSR_CKS_64	0x01
#define WTCSR_CKS_128	0x02
#define WTCSR_CKS_256	0x03
#define WTCSR_CKS_512	0x04
#define WTCSR_CKS_1024	0x05
#define WTCSR_CKS_2048	0x06
#define WTCSR_CKS_4096	0x07

#if defined(CONFIG_CPU_SUBTYPE_SH7785) || defined(CONFIG_CPU_SUBTYPE_SH7780)
static inline __u32 sh_wdt_read_cnt(void)
{
	return __raw_readl(WTCNT_R);
}

static inline void sh_wdt_write_cnt(__u32 val)
{
	__raw_writel((WTCNT_HIGH << 24) | (__u32)val, WTCNT);
}

static inline void sh_wdt_write_bst(__u32 val)
{
	__raw_writel((WTBST_HIGH << 24) | (__u32)val, WTBST);
}
static inline __u32 sh_wdt_read_csr(void)
{
	return __raw_readl(WTCSR_R);
}

static inline void sh_wdt_write_csr(__u32 val)
{
	__raw_writel((WTCSR_HIGH << 24) | (__u32)val, WTCSR);
}
#else
static inline __u8 sh_wdt_read_cnt(void)
{
	return __raw_readb(WTCNT_R);
}

static inline void sh_wdt_write_cnt(__u8 val)
{
	__raw_writew((WTCNT_HIGH << 8) | (__u16)val, WTCNT);
}

static inline __u8 sh_wdt_read_csr(void)
{
	return __raw_readb(WTCSR_R);
}

static inline void sh_wdt_write_csr(__u8 val)
{
	__raw_writew((WTCSR_HIGH << 8) | (__u16)val, WTCSR);
}
#endif 
#endif 
#endif 
