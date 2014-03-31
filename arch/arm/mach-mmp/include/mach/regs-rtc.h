#ifndef __ASM_MACH_REGS_RTC_H
#define __ASM_MACH_REGS_RTC_H

#include <mach/addr-map.h>

#define RTC_VIRT_BASE	(APB_VIRT_BASE + 0x10000)
#define RTC_REG(x)	(*((volatile u32 __iomem *)(RTC_VIRT_BASE + (x))))


#define RCNR		RTC_REG(0x00)	
#define RTAR		RTC_REG(0x04)	
#define RTSR		RTC_REG(0x08)	
#define RTTR		RTC_REG(0x0C)	

#define RTSR_HZE	(1 << 3)	
#define RTSR_ALE	(1 << 2)	
#define RTSR_HZ		(1 << 1)	
#define RTSR_AL		(1 << 0)	

#endif 
