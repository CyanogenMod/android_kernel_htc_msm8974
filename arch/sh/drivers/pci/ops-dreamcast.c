/*
 * PCI operations for the Sega Dreamcast
 *
 * Copyright (C) 2001, 2002  M. R. Brown
 * Copyright (C) 2002, 2003  Paul Mundt
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/pci.h>
#include <linux/module.h>
#include <linux/io.h>
#include <mach/pci.h>

static int gapspci_config_access(unsigned char bus, unsigned int devfn)
{
	return (bus == 0) && (devfn == 0);
}

static int gapspci_read(struct pci_bus *bus, unsigned int devfn, int where, int size, u32 *val)
{
	*val = 0xffffffff;

	if (!gapspci_config_access(bus->number, devfn))
		return PCIBIOS_DEVICE_NOT_FOUND;

	switch (size) {
	case 1: *val = inb(GAPSPCI_BBA_CONFIG+where); break;
	case 2: *val = inw(GAPSPCI_BBA_CONFIG+where); break;
	case 4: *val = inl(GAPSPCI_BBA_CONFIG+where); break;
	}

        return PCIBIOS_SUCCESSFUL;
}

static int gapspci_write(struct pci_bus *bus, unsigned int devfn, int where, int size, u32 val)
{
	if (!gapspci_config_access(bus->number, devfn))
		return PCIBIOS_DEVICE_NOT_FOUND;

	switch (size) {
	case 1: outb(( u8)val, GAPSPCI_BBA_CONFIG+where); break;
	case 2: outw((u16)val, GAPSPCI_BBA_CONFIG+where); break;
	case 4: outl((u32)val, GAPSPCI_BBA_CONFIG+where); break;
	}

        return PCIBIOS_SUCCESSFUL;
}

struct pci_ops gapspci_pci_ops = {
	.read	= gapspci_read,
	.write	= gapspci_write,
};
