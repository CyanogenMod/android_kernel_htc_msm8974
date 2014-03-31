/*
 * Generic PowerPC 40x platform support
 *
 * Copyright 2008 IBM Corporation
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; version 2 of the License.
 *
 * This implements simple platform support for PowerPC 44x chips.  This is
 * mostly used for eval boards or other simple and "generic" 44x boards.  If
 * your board has custom functions or hardware, then you will likely want to
 * implement your own board.c file to accommodate it.
 */

#include <asm/machdep.h>
#include <asm/pci-bridge.h>
#include <asm/ppc4xx.h>
#include <asm/prom.h>
#include <asm/time.h>
#include <asm/udbg.h>
#include <asm/uic.h>

#include <linux/init.h>
#include <linux/of_platform.h>

static __initdata struct of_device_id ppc40x_of_bus[] = {
	{ .compatible = "ibm,plb3", },
	{ .compatible = "ibm,plb4", },
	{ .compatible = "ibm,opb", },
	{ .compatible = "ibm,ebc", },
	{ .compatible = "simple-bus", },
	{},
};

static int __init ppc40x_device_probe(void)
{
	of_platform_bus_probe(NULL, ppc40x_of_bus, NULL);

	return 0;
}
machine_device_initcall(ppc40x_simple, ppc40x_device_probe);

static const char *board[] __initdata = {
	"amcc,acadia",
	"amcc,haleakala",
	"amcc,kilauea",
	"amcc,makalu",
	"apm,klondike",
	"est,hotfoot",
	"plathome,obs600"
};

static int __init ppc40x_probe(void)
{
	if (of_flat_dt_match(of_get_flat_dt_root(), board)) {
		pci_set_flags(PCI_REASSIGN_ALL_RSRC);
		return 1;
	}

	return 0;
}

define_machine(ppc40x_simple) {
	.name = "PowerPC 40x Platform",
	.probe = ppc40x_probe,
	.progress = udbg_progress,
	.init_IRQ = uic_init_tree,
	.get_irq = uic_get_irq,
	.restart = ppc4xx_reset_system,
	.calibrate_decr = generic_calibrate_decr,
};
