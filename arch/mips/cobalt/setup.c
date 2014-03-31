/*
 * Setup pointers to hardware dependent routines.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1996, 1997, 2004, 05 by Ralf Baechle (ralf@linux-mips.org)
 * Copyright (C) 2001, 2002, 2003 by Liam Davies (ldavies@agile.tv)
 *
 */
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/pm.h>

#include <asm/bootinfo.h>
#include <asm/reboot.h>
#include <asm/gt64120.h>

#include <cobalt.h>

extern void cobalt_machine_restart(char *command);
extern void cobalt_machine_halt(void);

const char *get_system_type(void)
{
	switch (cobalt_board_id) {
		case COBALT_BRD_ID_QUBE1:
			return "Cobalt Qube";
		case COBALT_BRD_ID_RAQ1:
			return "Cobalt RaQ";
		case COBALT_BRD_ID_QUBE2:
			return "Cobalt Qube2";
		case COBALT_BRD_ID_RAQ2:
			return "Cobalt RaQ2";
	}
	return "MIPS Cobalt";
}

static struct resource cobalt_reserved_resources[] = {
	{	
		.start	= 0x00,
		.end	= 0x1f,
		.name	= "reserved",
		.flags	= IORESOURCE_BUSY | IORESOURCE_IO,
	},
	{	
		.start	= 0x60,
		.end	= 0x6f,
		.name	= "reserved",
		.flags	= IORESOURCE_BUSY | IORESOURCE_IO,
	},
	{	
		.start	= 0x80,
		.end	= 0x8f,
		.name	= "reserved",
		.flags	= IORESOURCE_BUSY | IORESOURCE_IO,
	},
	{	
		.start	= 0xc0,
		.end	= 0xdf,
		.name	= "reserved",
		.flags	= IORESOURCE_BUSY | IORESOURCE_IO,
	},
};

void __init plat_mem_setup(void)
{
	int i;

	_machine_restart = cobalt_machine_restart;
	_machine_halt = cobalt_machine_halt;
	pm_power_off = cobalt_machine_halt;

	set_io_port_base(CKSEG1ADDR(GT_DEF_PCI0_IO_BASE));

	
	ioport_resource.end = 0x01ffffff;

	
	for (i = 0; i < ARRAY_SIZE(cobalt_reserved_resources); i++)
		request_resource(&ioport_resource, cobalt_reserved_resources + i);
}


void __init prom_init(void)
{
	unsigned long memsz;
	int argc, i;
	char **argv;

	memsz = fw_arg0 & 0x7fff0000;
	argc = fw_arg0 & 0x0000ffff;
	argv = (char **)fw_arg1;

	for (i = 1; i < argc; i++) {
		strlcat(arcs_cmdline, argv[i], COMMAND_LINE_SIZE);
		if (i < (argc - 1))
			strlcat(arcs_cmdline, " ", COMMAND_LINE_SIZE);
	}

	add_memory_region(0x0, memsz, BOOT_MEM_RAM);
}

void __init prom_free_prom_memory(void)
{
	
}
