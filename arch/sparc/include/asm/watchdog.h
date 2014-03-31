/*
 *
 * watchdog - Driver interface for the hardware watchdog timers
 * present on Sun Microsystems boardsets
 *
 * Copyright (c) 2000 Eric Brower <ebrower@usa.net>
 *
 */

#ifndef _SPARC64_WATCHDOG_H
#define _SPARC64_WATCHDOG_H

#include <linux/watchdog.h>

#define WIOCSTART _IO (WATCHDOG_IOCTL_BASE, 10)		
#define WIOCSTOP  _IO (WATCHDOG_IOCTL_BASE, 11)		
#define WIOCGSTAT _IOR(WATCHDOG_IOCTL_BASE, 12, int)

#define WD_FREERUN	0x01	
#define WD_EXPIRED	0x02	
#define WD_RUNNING	0x04	
#define WD_STOPPED	0x08	
#define WD_SERVICED 0x10	

#endif 

