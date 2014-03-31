/*
 * Miscellaneous IOCTL commands for Dynamic Power Management Controller Driver
 *
 * Copyright (C) 2004-2009 Analog Device Inc.
 *
 * Licensed under the GPL-2
 */

#ifndef _BLACKFIN_DPMC_H_
#define _BLACKFIN_DPMC_H_

#include <mach/pll.h>

#define DF			0x0001	
#define PLL_OFF			0x0002	
#define STOPCK			0x0008	
#define PDWN			0x0020	
#ifdef __ADSPBF539__
# define IN_DELAY		0x0014	
# define OUT_DELAY		0x00C0	
#else
# define IN_DELAY		0x0040	
# define OUT_DELAY		0x0080	
#endif
#define BYPASS			0x0100	
#define MSEL			0x7E00	
#define SPORT_HYST		0x8000	
#define SET_MSEL(x)		(((x)&0x3F) << 0x9)	

#define SSEL			0x000F	
#define CSEL			0x0030	
#define CSEL_DIV1		0x0000	
#define CSEL_DIV2		0x0010	
#define CSEL_DIV4		0x0020	
#define CSEL_DIV8		0x0030	

#define CCLK_DIV1 CSEL_DIV1
#define CCLK_DIV2 CSEL_DIV2
#define CCLK_DIV4 CSEL_DIV4
#define CCLK_DIV8 CSEL_DIV8

#define SET_SSEL(x)	((x) & 0xF)	
#define SCLK_DIV(x)	(x)		

#define ACTIVE_PLLENABLED	0x0001	
#define FULL_ON			0x0002	
#define ACTIVE_PLLDISABLED	0x0004	
#define PLL_LOCKED		0x0020	

#define RTCWS			0x0400	
#define CANWS			0x0800	
#define USBWS			0x2000	
#define KPADWS			0x4000	
#define ROTWS			0x8000	
#define GPWS			0x1000	

#if defined(__ADSPBF52x__) || defined(__ADSPBF51x__)
#define FREQ			0x3000	
#define FREQ_1000		0x3000	
#else
#define FREQ			0x0003	
#define FREQ_333		0x0001	
#define FREQ_667		0x0002	
#define FREQ_1000		0x0003	
#endif
#define HIBERNATE		0x0000	

#define GAIN			0x000C	
#define GAIN_5			0x0000	
#define GAIN_10			0x0004	
#define GAIN_20			0x0008	
#define GAIN_50			0x000C	

#define VLEV			0x00F0	
#ifdef __ADSPBF52x__
#define VLEV_085		0x0040	
#define VLEV_090		0x0050	
#define VLEV_095		0x0060	
#define VLEV_100		0x0070	
#define VLEV_105		0x0080	
#define VLEV_110		0x0090	
#define VLEV_115		0x00A0	
#define VLEV_120		0x00B0	
#else
#define VLEV_085		0x0060	
#define VLEV_090		0x0070	
#define VLEV_095		0x0080	
#define VLEV_100		0x0090	
#define VLEV_105		0x00A0	
#define VLEV_110		0x00B0	
#define VLEV_115		0x00C0	
#define VLEV_120		0x00D0	
#define VLEV_125		0x00E0	
#define VLEV_130		0x00F0	
#endif

#define WAKE			0x0100	
#define CANWE			0x0200	
#define PHYWE			0x0400	
#define GPWE			0x0400	
#define MXVRWE			0x0400	
#define KPADWE			0x1000	
#define ROTWE			0x2000	
#define CLKBUFOE		0x4000	
#define SCKELOW			0x8000	

#if defined(__ADSPBF52x__) || defined(__ADSPBF51x__)
#define USBWE			0x0200	
#else
#define USBWE			0x0800	
#endif

#ifndef __ASSEMBLY__

void sleep_mode(u32 sic_iwr0, u32 sic_iwr1, u32 sic_iwr2);
void sleep_deeper(u32 sic_iwr0, u32 sic_iwr1, u32 sic_iwr2);
void do_hibernate(int wakeup);
void set_dram_srfs(void);
void unset_dram_srfs(void);

#define VRPAIR(vlev, freq) (((vlev) << 16) | ((freq) >> 16))

#ifdef CONFIG_CPU_FREQ
#define CPUFREQ_CPU 0
#endif
struct bfin_dpmc_platform_data {
	const unsigned int *tuple_tab;
	unsigned short tabsize;
	unsigned short vr_settling_time; 
};

#endif

#endif	
