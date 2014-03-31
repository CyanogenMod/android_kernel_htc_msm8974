#ifndef __ASM_ARCH_RESET_H
#define __ASM_ARCH_RESET_H

#include "hardware.h"

#define RESET_STATUS_HARDWARE	(1 << 0)	
#define RESET_STATUS_WATCHDOG	(1 << 1)	
#define RESET_STATUS_LOWPOWER	(1 << 2)	
#define RESET_STATUS_GPIO	(1 << 3)	
#define RESET_STATUS_ALL	(0xf)

extern unsigned int reset_status;
static inline void clear_reset_status(unsigned int mask)
{
	RCSR = mask;
}

#endif 
