#ifndef __ASM_MACH_REGS_RTC_H
#define __ASM_MACH_REGS_RTC_H

#include <mach/hardware.h>


#define RCNR		__REG(0x40900000)  
#define RTAR		__REG(0x40900004)  
#define RTSR		__REG(0x40900008)  
#define RTTR		__REG(0x4090000C)  
#define PIAR		__REG(0x40900038)  

#define RTSR_PICE	(1 << 15)	
#define RTSR_PIALE	(1 << 14)	
#define RTSR_HZE	(1 << 3)	
#define RTSR_ALE	(1 << 2)	
#define RTSR_HZ		(1 << 1)	
#define RTSR_AL		(1 << 0)	

#endif 
