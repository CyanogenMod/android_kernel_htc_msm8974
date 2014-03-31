#ifndef _ASM_RTC_H
#define _ASM_RTC_H

void time_init(void);
extern void (*board_time_init)(void);
extern void (*rtc_sh_get_time)(struct timespec *);
extern int (*rtc_sh_set_time)(const time_t);

#define RTC_BATT_BAD 0x100	
#define RTC_SQWE 0x08		
#define RTC_DM_BINARY 0x04	
#define RTC_24H 0x02		
#define RTC_DST_EN 0x01	        

struct rtc_time;
unsigned int get_rtc_time(struct rtc_time *);
int set_rtc_time(struct rtc_time *);

#define RTC_CAP_4_DIGIT_YEAR	(1 << 0)

struct sh_rtc_platform_info {
	unsigned long capabilities;
};

#include <cpu/rtc.h>

#endif 
