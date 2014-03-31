/*
 * bfin_watchdog.h - Blackfin watchdog definitions
 *
 * Copyright 2006-2010 Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later.
 */

#ifndef _BFIN_WATCHDOG_H
#define _BFIN_WATCHDOG_H

#define SWRST_RESET_WDOG 0x4000

#define WDOG_EXPIRED 0x8000

#define ICTL_RESET   0x0
#define ICTL_NMI     0x2
#define ICTL_GPI     0x4
#define ICTL_NONE    0x6
#define ICTL_MASK    0x6

#define WDEN_MASK    0x0FF0
#define WDEN_ENABLE  0x0000
#define WDEN_DISABLE 0x0AD0

#endif
