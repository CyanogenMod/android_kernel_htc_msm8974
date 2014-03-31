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

struct boot_params boot_params __attribute__((aligned(16)));

char *HEAP = _end;
char *heap_end = _end;		


static void copy_boot_params(void)
{
	struct old_cmdline {
		u16 cl_magic;
		u16 cl_offset;
	};
	const struct old_cmdline * const oldcmd =
		(const struct old_cmdline *)OLD_CL_ADDRESS;

	BUILD_BUG_ON(sizeof boot_params != 4096);
	memcpy(&boot_params.hdr, &hdr, sizeof hdr);

	if (!boot_params.hdr.cmd_line_ptr &&
	    oldcmd->cl_magic == OLD_CL_MAGIC) {
		
		u16 cmdline_seg;

		if (oldcmd->cl_offset < boot_params.hdr.setup_move_size)
			cmdline_seg = ds();
		else
			cmdline_seg = 0x9000;

		boot_params.hdr.cmd_line_ptr =
			(cmdline_seg << 4) + oldcmd->cl_offset;
	}
}

static void keyboard_set_repeat(void)
{
	struct biosregs ireg;
	initregs(&ireg);
	ireg.ax = 0x0305;
	intcall(0x16, &ireg, NULL);
}

static void query_ist(void)
{
	struct biosregs ireg, oreg;

	if (cpu.level < 6)
		return;

	initregs(&ireg);
	ireg.ax  = 0xe980;	 
	ireg.edx = 0x47534943;	 
	intcall(0x15, &ireg, &oreg);

	boot_params.ist_info.signature  = oreg.eax;
	boot_params.ist_info.command    = oreg.ebx;
	boot_params.ist_info.event      = oreg.ecx;
	boot_params.ist_info.perf_level = oreg.edx;
}

static void set_bios_mode(void)
{
#ifdef CONFIG_X86_64
	struct biosregs ireg;

	initregs(&ireg);
	ireg.ax = 0xec00;
	ireg.bx = 2;
	intcall(0x15, &ireg, NULL);
#endif
}

static void init_heap(void)
{
	char *stack_end;

	if (boot_params.hdr.loadflags & CAN_USE_HEAP) {
		asm("leal %P1(%%esp),%0"
		    : "=r" (stack_end) : "i" (-STACK_SIZE));

		heap_end = (char *)
			((size_t)boot_params.hdr.heap_end_ptr + 0x200);
		if (heap_end > stack_end)
			heap_end = stack_end;
	} else {
		
		puts("WARNING: Ancient bootloader, some functionality "
		     "may be limited!\n");
	}
}

void main(void)
{
	
	copy_boot_params();

	
	console_init();
	if (cmdline_find_option_bool("debug"))
		puts("early console in setup code\n");

	
	init_heap();

	
	if (validate_cpu()) {
		puts("Unable to boot - please use a kernel appropriate "
		     "for your CPU.\n");
		die();
	}

	
	set_bios_mode();

	
	detect_memory();

	
	keyboard_set_repeat();

	
	query_mca();

	
	query_ist();

	
#if defined(CONFIG_APM) || defined(CONFIG_APM_MODULE)
	query_apm_bios();
#endif

	
#if defined(CONFIG_EDD) || defined(CONFIG_EDD_MODULE)
	query_edd();
#endif

	
	set_video();

	
	go_to_protected_mode();
}
