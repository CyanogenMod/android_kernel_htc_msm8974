/*
 * Copyright (C) 2009 Lemote Inc.
 * Author: Wu Zhangjin, wuzhangjin@gmail.com
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#include <asm/bootinfo.h>

#include <loongson.h>

void __init mach_prom_init_machtype(void)
{
	if (strstr(arcs_cmdline, "PMON_VER=LM")) {
		if (strstr(arcs_cmdline, "PMON_VER=LM8"))
			mips_machtype = MACH_LEMOTE_YL2F89;
		else if (strstr(arcs_cmdline, "PMON_VER=LM6"))
			mips_machtype = MACH_LEMOTE_FL2F;
		else if (strstr(arcs_cmdline, "PMON_VER=LM9"))
			mips_machtype = MACH_LEMOTE_LL2F;
		else
			mips_machtype = MACH_LEMOTE_NAS;

		strcat(arcs_cmdline, " machtype=");
		strcat(arcs_cmdline, get_system_type());
		strcat(arcs_cmdline, " ");
	}
}
