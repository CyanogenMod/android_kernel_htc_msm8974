/*
 * reboot.h - shutdown/reboot header
 *
 * Copyright 2004-2008 Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later.
 */

#ifndef __ASM_REBOOT_H__
#define __ASM_REBOOT_H__

extern void native_machine_restart(char *cmd);
extern void native_machine_halt(void);
extern void native_machine_power_off(void);

extern void bfin_reset_boot_spi_cs(unsigned short pin);

#endif
