#ifndef __ASM_MACH_REGS_OST_H
#define __ASM_MACH_REGS_OST_H

#include <mach/hardware.h>


#define OSMR0		__REG(0x40A00000)  
#define OSMR1		__REG(0x40A00004)  
#define OSMR2		__REG(0x40A00008)  
#define OSMR3		__REG(0x40A0000C)  
#define OSMR4		__REG(0x40A00080)  
#define OSCR		__REG(0x40A00010)  
#define OSCR4		__REG(0x40A00040)  
#define OMCR4		__REG(0x40A000C0)  
#define OSSR		__REG(0x40A00014)  
#define OWER		__REG(0x40A00018)  
#define OIER		__REG(0x40A0001C)  

#define OSSR_M3		(1 << 3)	
#define OSSR_M2		(1 << 2)	
#define OSSR_M1		(1 << 1)	
#define OSSR_M0		(1 << 0)	

#define OWER_WME	(1 << 0)	

#define OIER_E3		(1 << 3)	
#define OIER_E2		(1 << 2)	
#define OIER_E1		(1 << 1)	
#define OIER_E0		(1 << 0)	

#endif 
