/*
 *  Architecture specific OF callbacks.
 *
 *  Copyright (C) 2011 Texas Instruments Incorporated
 *  Author: Mark Salter <msalter@redhat.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 */
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/initrd.h>
#include <linux/memblock.h>

void __init early_init_devtree(void *params)
{
	
	initial_boot_params = params;

	of_scan_flat_dt(early_init_dt_scan_chosen, c6x_command_line);

	
	of_scan_flat_dt(early_init_dt_scan_root, NULL);
	of_scan_flat_dt(early_init_dt_scan_memory, NULL);
}


#ifdef CONFIG_BLK_DEV_INITRD
void __init early_init_dt_setup_initrd_arch(unsigned long start,
		unsigned long end)
{
	initrd_start = (unsigned long)__va(start);
	initrd_end = (unsigned long)__va(end);
	initrd_below_start_ok = 1;
}
#endif

void __init early_init_dt_add_memory_arch(u64 base, u64 size)
{
	c6x_add_memory(base, size);
}

void * __init early_init_dt_alloc_memory_arch(u64 size, u64 align)
{
	return __va(memblock_alloc(size, align));
}
