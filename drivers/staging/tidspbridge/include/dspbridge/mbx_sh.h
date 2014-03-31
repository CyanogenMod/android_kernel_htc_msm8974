/*
 * mbx_sh.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Definitions for shared mailbox cmd/data values.(used on both
 * the GPP and DSP sides).
 *
 * Copyright (C) 2008 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */


#ifndef _MBX_SH_H
#define _MBX_SH_H

#define MBX_PCPY_CLASS     0x0800	
#define MBX_PM_CLASS       0x2000	
#define MBX_DBG_CLASS      0x4000	

#define MBX_DEH_BASE        0x0
#define MBX_DEH_USERS_BASE  0x100	
#define MBX_DEH_LIMIT       0x3FF	
#define MBX_DEH_RESET       0x101	


#define MBX_PM_DSPIDLE                  (MBX_PM_CLASS + 0x0)
#define MBX_PM_DSPWAKEUP                (MBX_PM_CLASS + 0x1)
#define MBX_PM_EMERGENCYSLEEP           (MBX_PM_CLASS + 0x2)
#define MBX_PM_SETPOINT_PRENOTIFY       (MBX_PM_CLASS + 0x6)
#define MBX_PM_SETPOINT_POSTNOTIFY      (MBX_PM_CLASS + 0x7)
#define MBX_PM_DSPRETENTION        (MBX_PM_CLASS + 0x8)
#define MBX_PM_DSPHIBERNATE        (MBX_PM_CLASS + 0x9)
#define MBX_PM_HIBERNATE_EN        (MBX_PM_CLASS + 0xA)
#define MBX_PM_OPP_REQ                  (MBX_PM_CLASS + 0xB)

#define MBX_DBG_SYSPRINTF       (MBX_DBG_CLASS + 0x0)

#endif 
