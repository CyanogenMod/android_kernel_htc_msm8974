/* include/asm-m68k/rtc.h
 *
 * Copyright Richard Zidlicky
 * implementation details for genrtc/q40rtc driver
 */
/* permission is hereby granted to copy, modify and redistribute this code
 * in terms of the GNU Library General Public License, Version 2 or later,
 * at your option.
 */

#ifndef _ASM_RTC_H
#define _ASM_RTC_H

#ifdef __KERNEL__

#include <linux/rtc.h>
#include <asm/errno.h>
#include <asm/machdep.h>

#define RTC_PIE 0x40		
#define RTC_AIE 0x20		
#define RTC_UIE 0x10		

#define RTC_BATT_BAD 0x100	
#define RTC_SQWE 0x08		
#define RTC_DM_BINARY 0x04	
#define RTC_24H 0x02		
#define RTC_DST_EN 0x01	        

static inline unsigned int get_rtc_time(struct rtc_time *time)
{
	if (mach_hwclk)
		mach_hwclk(0, time);
	return RTC_24H;
}

static inline int set_rtc_time(struct rtc_time *time)
{
	if (mach_hwclk)
		return mach_hwclk(1, time);
	return -EINVAL;
}

static inline unsigned int get_rtc_ss(void)
{
	if (mach_get_ss)
		return mach_get_ss();
	else{
		struct rtc_time h;

		get_rtc_time(&h);
		return h.tm_sec;
	}
}

static inline int get_rtc_pll(struct rtc_pll_info *pll)
{
	if (mach_get_rtc_pll)
		return mach_get_rtc_pll(pll);
	else
		return -EINVAL;
}
static inline int set_rtc_pll(struct rtc_pll_info *pll)
{
	if (mach_set_rtc_pll)
		return mach_set_rtc_pll(pll);
	else
		return -EINVAL;
}
#endif 

#endif 
