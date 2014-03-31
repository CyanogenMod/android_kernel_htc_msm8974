/*
 * Generic SH7786 PCI-Express operations.
 *
 *  Copyright (C) 2009 - 2010  Paul Mundt
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include "pcie-sh7786.h"

enum {
	PCI_ACCESS_READ,
	PCI_ACCESS_WRITE,
};

static int sh7786_pcie_config_access(unsigned char access_type,
		struct pci_bus *bus, unsigned int devfn, int where, u32 *data)
{
	struct pci_channel *chan = bus->sysdata;
	int dev, func, type, reg;

	dev = PCI_SLOT(devfn);
	func = PCI_FUNC(devfn);
	type = !!bus->parent;
	reg = where & ~3;

	if (bus->number > 255 || dev > 31 || func > 7)
		return PCIBIOS_FUNC_NOT_SUPPORTED;

	if (pci_is_root_bus(bus)) {
		if (dev == 0) {
			if (access_type == PCI_ACCESS_READ)
				*data = pci_read_reg(chan, PCI_REG(reg));
			else
				pci_write_reg(chan, *data, PCI_REG(reg));

			return PCIBIOS_SUCCESSFUL;
		} else if (dev > 1)
			return PCIBIOS_DEVICE_NOT_FOUND;
	}

	
	pci_write_reg(chan, pci_read_reg(chan, SH4A_PCIEERRFR), SH4A_PCIEERRFR);

	
	pci_write_reg(chan, (bus->number << 24) | (dev << 19) |
				(func << 16) | reg, SH4A_PCIEPAR);

	
	pci_write_reg(chan, (1 << 31) | (type << 8), SH4A_PCIEPCTLR);

	
	if (pci_read_reg(chan, SH4A_PCIEERRFR) & 0x10)
		return PCIBIOS_DEVICE_NOT_FOUND;

	
	if (pci_read_reg(chan, SH4A_PCIEPCICONF1) & ((1 << 29) | (1 << 28)))
		return PCIBIOS_DEVICE_NOT_FOUND;

	if (access_type == PCI_ACCESS_READ)
		*data = pci_read_reg(chan, SH4A_PCIEPDR);
	else
		pci_write_reg(chan, *data, SH4A_PCIEPDR);

	
	pci_write_reg(chan, 0, SH4A_PCIEPCTLR);

	return PCIBIOS_SUCCESSFUL;
}

static int sh7786_pcie_read(struct pci_bus *bus, unsigned int devfn,
			    int where, int size, u32 *val)
{
	unsigned long flags;
	int ret;
	u32 data;

        if ((size == 2) && (where & 1))
		return PCIBIOS_BAD_REGISTER_NUMBER;
	else if ((size == 4) && (where & 3))
		return PCIBIOS_BAD_REGISTER_NUMBER;

	raw_spin_lock_irqsave(&pci_config_lock, flags);
	ret = sh7786_pcie_config_access(PCI_ACCESS_READ, bus,
					devfn, where, &data);
	if (ret != PCIBIOS_SUCCESSFUL) {
		*val = 0xffffffff;
		goto out;
	}

	if (size == 1)
		*val = (data >> ((where & 3) << 3)) & 0xff;
	else if (size == 2)
		*val = (data >> ((where & 2) << 3)) & 0xffff;
	else
		*val = data;

	dev_dbg(&bus->dev, "pcie-config-read: bus=%3d devfn=0x%04x "
		"where=0x%04x size=%d val=0x%08lx\n", bus->number,
		devfn, where, size, (unsigned long)*val);

out:
	raw_spin_unlock_irqrestore(&pci_config_lock, flags);
	return ret;
}

static int sh7786_pcie_write(struct pci_bus *bus, unsigned int devfn,
			     int where, int size, u32 val)
{
	unsigned long flags;
	int shift, ret;
	u32 data;

        if ((size == 2) && (where & 1))
		return PCIBIOS_BAD_REGISTER_NUMBER;
	else if ((size == 4) && (where & 3))
		return PCIBIOS_BAD_REGISTER_NUMBER;

	raw_spin_lock_irqsave(&pci_config_lock, flags);
	ret = sh7786_pcie_config_access(PCI_ACCESS_READ, bus,
					devfn, where, &data);
	if (ret != PCIBIOS_SUCCESSFUL)
		goto out;

	dev_dbg(&bus->dev, "pcie-config-write: bus=%3d devfn=0x%04x "
		"where=0x%04x size=%d val=%08lx\n", bus->number,
		devfn, where, size, (unsigned long)val);

	if (size == 1) {
		shift = (where & 3) << 3;
		data &= ~(0xff << shift);
		data |= ((val & 0xff) << shift);
	} else if (size == 2) {
		shift = (where & 2) << 3;
		data &= ~(0xffff << shift);
		data |= ((val & 0xffff) << shift);
	} else
		data = val;

	ret = sh7786_pcie_config_access(PCI_ACCESS_WRITE, bus,
					devfn, where, &data);
out:
	raw_spin_unlock_irqrestore(&pci_config_lock, flags);
	return ret;
}

struct pci_ops sh7786_pci_ops = {
	.read	= sh7786_pcie_read,
	.write	= sh7786_pcie_write,
};
