/*
 * Platform specific functions
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file "COPYING" in the main directory of
 * this archive for more details.
 *
 * Copyright (C) 2001 - 2005 Tensilica Inc.
 */

#ifndef _XTENSA_PLATFORM_H
#define _XTENSA_PLATFORM_H

#include <linux/types.h>
#include <linux/pci.h>

#include <asm/bootparam.h>

extern void platform_init(bp_tag_t*);

extern void platform_setup (char **);

extern void platform_init_irq (void);

extern void platform_restart (void);

extern void platform_halt (void);

extern void platform_power_off (void);

extern void platform_idle (void);

extern void platform_heartbeat (void);

extern void platform_pcibios_init (void);

extern int platform_pcibios_fixup (void);

extern void platform_calibrate_ccount (void);

#endif	

