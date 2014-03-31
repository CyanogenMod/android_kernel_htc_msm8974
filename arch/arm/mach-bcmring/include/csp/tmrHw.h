/*****************************************************************************
* Copyright 2004 - 2008 Broadcom Corporation.  All rights reserved.
*
* Unless you and Broadcom execute a separate written software license
* agreement governing use of this software, this software is licensed to you
* under the terms of the GNU General Public License version 2, available at
* http://www.broadcom.com/licenses/GPLv2.php (the "GPL").
*
* Notwithstanding the above, under no circumstances may you combine this
* software in any way with any other Broadcom software provided under a
* license other than the GPL, without Broadcom's express prior written
* consent.
*****************************************************************************/

#ifndef _TMRHW_H
#define _TMRHW_H

#include <csp/stdint.h>

typedef uint32_t tmrHw_ID_t;	
typedef uint32_t tmrHw_COUNT_t;	
typedef uint32_t tmrHw_INTERVAL_t;	
typedef uint32_t tmrHw_RATE_t;	

typedef enum {
	tmrHw_INTERRUPT_STATUS_SET,	
	tmrHw_INTERRUPT_STATUS_UNSET	
} tmrHw_INTERRUPT_STATUS_e;

typedef enum {
	tmrHw_CAPABILITY_CLOCK,	
	tmrHw_CAPABILITY_RESOLUTION	
} tmrHw_CAPABILITY_e;

uint32_t tmrHw_getTimerCapability(tmrHw_ID_t timerId,	
				  tmrHw_CAPABILITY_e capability	
);

tmrHw_RATE_t tmrHw_setPeriodicTimerRate(tmrHw_ID_t timerId,	
					tmrHw_RATE_t rate	
);

tmrHw_INTERVAL_t tmrHw_setPeriodicTimerInterval(tmrHw_ID_t timerId,	
						tmrHw_INTERVAL_t msec	
);

tmrHw_INTERVAL_t tmrHw_setOneshotTimerInterval(tmrHw_ID_t timerId,	
					       tmrHw_INTERVAL_t msec	
);

tmrHw_RATE_t tmrHw_setFreeRunningTimer(tmrHw_ID_t timerId,	
				       uint32_t divider	
) __attribute__ ((section(".aramtext")));

int tmrHw_startTimer(tmrHw_ID_t timerId	
) __attribute__ ((section(".aramtext")));

int tmrHw_stopTimer(tmrHw_ID_t timerId	
);

tmrHw_COUNT_t tmrHw_GetCurrentCount(tmrHw_ID_t timerId	
) __attribute__ ((section(".aramtext")));

tmrHw_RATE_t tmrHw_getCountRate(tmrHw_ID_t timerId	
) __attribute__ ((section(".aramtext")));

void tmrHw_enableInterrupt(tmrHw_ID_t timerId	
);

void tmrHw_disableInterrupt(tmrHw_ID_t timerId	
);

void tmrHw_clearInterrupt(tmrHw_ID_t timerId	
);

tmrHw_INTERRUPT_STATUS_e tmrHw_getInterruptStatus(tmrHw_ID_t timerId	
);

tmrHw_ID_t tmrHw_getInterruptSource(void);

void tmrHw_printDebugInfo(tmrHw_ID_t timerId,	
			  int (*fpPrint) (const char *, ...)	
);

void tmrHw_udelay(tmrHw_ID_t timerId,	
		  unsigned long usecs	
) __attribute__ ((section(".aramtext")));

#endif 
