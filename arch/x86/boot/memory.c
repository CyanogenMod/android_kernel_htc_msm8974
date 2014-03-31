/* -*- linux-c -*- ------------------------------------------------------- *
 *
 *   Copyright (C) 1991, 1992 Linus Torvalds
 *   Copyright 2007 rPath, Inc. - All Rights Reserved
 *   Copyright 2009 Intel Corporation; author H. Peter Anvin
 *
 *   This file is part of the Linux kernel, and is made available under
 *   the terms of the GNU General Public License version 2.
 *
 * ----------------------------------------------------------------------- */


#include "boot.h"

#define SMAP	0x534d4150	

static int detect_memory_e820(void)
{
	int count = 0;
	struct biosregs ireg, oreg;
	struct e820entry *desc = boot_params.e820_map;
	static struct e820entry buf; 

	initregs(&ireg);
	ireg.ax  = 0xe820;
	ireg.cx  = sizeof buf;
	ireg.edx = SMAP;
	ireg.di  = (size_t)&buf;


	do {
		intcall(0x15, &ireg, &oreg);
		ireg.ebx = oreg.ebx; 

		if (oreg.eflags & X86_EFLAGS_CF)
			break;

		if (oreg.eax != SMAP) {
			count = 0;
			break;
		}

		*desc++ = buf;
		count++;
	} while (ireg.ebx && count < ARRAY_SIZE(boot_params.e820_map));

	return boot_params.e820_entries = count;
}

static int detect_memory_e801(void)
{
	struct biosregs ireg, oreg;

	initregs(&ireg);
	ireg.ax = 0xe801;
	intcall(0x15, &ireg, &oreg);

	if (oreg.eflags & X86_EFLAGS_CF)
		return -1;

	
	if (oreg.cx || oreg.dx) {
		oreg.ax = oreg.cx;
		oreg.bx = oreg.dx;
	}

	if (oreg.ax > 15*1024) {
		return -1;	
	} else if (oreg.ax == 15*1024) {
		boot_params.alt_mem_k = (oreg.bx << 6) + oreg.ax;
	} else {
		boot_params.alt_mem_k = oreg.ax;
	}

	return 0;
}

static int detect_memory_88(void)
{
	struct biosregs ireg, oreg;

	initregs(&ireg);
	ireg.ah = 0x88;
	intcall(0x15, &ireg, &oreg);

	boot_params.screen_info.ext_mem_k = oreg.ax;

	return -(oreg.eflags & X86_EFLAGS_CF); 
}

int detect_memory(void)
{
	int err = -1;

	if (detect_memory_e820() > 0)
		err = 0;

	if (!detect_memory_e801())
		err = 0;

	if (!detect_memory_88())
		err = 0;

	return err;
}
