#ifndef __ASM_SH_CPU_SH4_RTC_H
#define __ASM_SH_CPU_SH4_RTC_H

#if defined(CONFIG_CPU_SUBTYPE_SH7722) || defined(CONFIG_CPU_SUBTYPE_SH7723)
#define rtc_reg_size		sizeof(u16)
#else
#define rtc_reg_size		sizeof(u32)
#endif

#define RTC_BIT_INVERTED	0x40	
#define RTC_DEF_CAPABILITIES	RTC_CAP_4_DIGIT_YEAR

#endif 
