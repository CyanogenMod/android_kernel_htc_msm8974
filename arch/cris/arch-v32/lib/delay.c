/*
 * Precise Delay Loops for ETRAX FS
 *
 * Copyright (C) 2006 Axis Communications AB.
 *
 */

#include <hwregs/reg_map.h>
#include <hwregs/reg_rdwr.h>
#include <hwregs/timer_defs.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/module.h>


void cris_delay10ns(u32 n10ns)
{
	u32 t0 = REG_RD(timer, regi_timer0, r_time);
	while (REG_RD(timer, regi_timer0, r_time) - t0 < n10ns)
		;
}
EXPORT_SYMBOL(cris_delay10ns);
