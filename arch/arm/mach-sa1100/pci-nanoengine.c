/*
 * linux/arch/arm/mach-sa1100/pci-nanoengine.c
 *
 * PCI functions for BSE nanoEngine PCI
 *
 * Copyright (C) 2010 Marcelo Roberto Jimenez <mroberto@cpti.cetuc.puc-rio.br>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/pci.h>
#include <linux/spinlock.h>

#include <asm/mach/pci.h>
#include <asm/mach-types.h>

#include <mach/nanoengine.h>
#include <mach/hardware.h>

static DEFINE_SPINLOCK(nano_lock);

static int nanoengine_get_pci_address(struct pci_bus *bus,
	unsigned int devfn, int where, unsigned long *address)
{
	int ret = PCIBIOS_DEVICE_NOT_FOUND;
	unsigned int busnr = bus->number;

	*address = NANO_PCI_CONFIG_SPACE_VIRT +
		((bus->number << 16) | (devfn << 8) | (where & ~3));

	ret = (busnr > 255 || devfn > 255 || where > 255) ?
		PCIBIOS_DEVICE_NOT_FOUND : PCIBIOS_SUCCESSFUL;

	return ret;
}

static int nanoengine_read_config(struct pci_bus *bus, unsigned int devfn, int where,
	int size, u32 *val)
{
	int ret;
	unsigned long address;
	unsigned long flags;
	u32 v;

	if (bus->number != 0 || (devfn >> 3) != 0) {
		v = -1;
		nanoengine_get_pci_address(bus, devfn, where, &address);
		goto exit_function;
	}

	spin_lock_irqsave(&nano_lock, flags);

	ret = nanoengine_get_pci_address(bus, devfn, where, &address);
	if (ret != PCIBIOS_SUCCESSFUL)
		return ret;
	v = __raw_readl(address);

	spin_unlock_irqrestore(&nano_lock, flags);

	v >>= ((where & 3) * 8);
	v &= (unsigned long)(-1) >> ((4 - size) * 8);

exit_function:
	*val = v;
	return PCIBIOS_SUCCESSFUL;
}

static int nanoengine_write_config(struct pci_bus *bus, unsigned int devfn, int where,
	int size, u32 val)
{
	int ret;
	unsigned long address;
	unsigned long flags;
	unsigned shift;
	u32 v;

	shift = (where & 3) * 8;

	spin_lock_irqsave(&nano_lock, flags);

	ret = nanoengine_get_pci_address(bus, devfn, where, &address);
	if (ret != PCIBIOS_SUCCESSFUL)
		return ret;
	v = __raw_readl(address);
	switch (size) {
	case 1:
		v &= ~(0xFF << shift);
		v |= val << shift;
		break;
	case 2:
		v &= ~(0xFFFF << shift);
		v |= val << shift;
		break;
	case 4:
		v = val;
		break;
	}
	__raw_writel(v, address);

	spin_unlock_irqrestore(&nano_lock, flags);

	return PCIBIOS_SUCCESSFUL;
}

static struct pci_ops pci_nano_ops = {
	.read	= nanoengine_read_config,
	.write	= nanoengine_write_config,
};

static int __init pci_nanoengine_map_irq(const struct pci_dev *dev, u8 slot,
	u8 pin)
{
	return NANOENGINE_IRQ_GPIO_PCI;
}

struct pci_bus * __init pci_nanoengine_scan_bus(int nr, struct pci_sys_data *sys)
{
	return pci_scan_root_bus(NULL, sys->busnr, &pci_nano_ops, sys,
				 &sys->resources);
}

static struct resource pci_io_ports =
	DEFINE_RES_IO_NAMED(0x400, 0x400, "PCI IO");

static struct resource pci_non_prefetchable_memory = {
	.name	= "PCI non-prefetchable",
	.start	= NANO_PCI_MEM_RW_PHYS,
	.end	= NANO_PCI_MEM_RW_PHYS + NANO_PCI_MEM_RW_SIZE - 1,
	.flags	= IORESOURCE_MEM,
};

static struct resource pci_prefetchable_memory = {
	.name	= "PCI prefetchable",
	.start	= 0x78000000,
	.end	= 0x78000000 + NANO_PCI_MEM_RW_SIZE - 1,
	.flags	= IORESOURCE_MEM  | IORESOURCE_PREFETCH,
};

static int __init pci_nanoengine_setup_resources(struct pci_sys_data *sys)
{
	if (request_resource(&ioport_resource, &pci_io_ports)) {
		printk(KERN_ERR "PCI: unable to allocate io port region\n");
		return -EBUSY;
	}
	if (request_resource(&iomem_resource, &pci_non_prefetchable_memory)) {
		release_resource(&pci_io_ports);
		printk(KERN_ERR "PCI: unable to allocate non prefetchable\n");
		return -EBUSY;
	}
	if (request_resource(&iomem_resource, &pci_prefetchable_memory)) {
		release_resource(&pci_io_ports);
		release_resource(&pci_non_prefetchable_memory);
		printk(KERN_ERR "PCI: unable to allocate prefetchable\n");
		return -EBUSY;
	}
	pci_add_resource_offset(&sys->resources, &pci_io_ports, sys->io_offset);
	pci_add_resource_offset(&sys->resources,
				&pci_non_prefetchable_memory, sys->mem_offset);
	pci_add_resource_offset(&sys->resources,
				&pci_prefetchable_memory, sys->mem_offset);

	return 1;
}

int __init pci_nanoengine_setup(int nr, struct pci_sys_data *sys)
{
	int ret = 0;

	pcibios_min_io = 0;
	pcibios_min_mem = 0;

	if (nr == 0) {
		sys->mem_offset = NANO_PCI_MEM_RW_PHYS;
		sys->io_offset = 0x400;
		ret = pci_nanoengine_setup_resources(sys);
		GPDR = (GPDR & ~GPIO_MBREQ) | GPIO_MBGNT;
		GAFR |= GPIO_MBGNT | GPIO_MBREQ;
		TUCR |= TUCR_MBGPIO;
	}

	return ret;
}

static struct hw_pci nanoengine_pci __initdata = {
	.map_irq		= pci_nanoengine_map_irq,
	.nr_controllers		= 1,
	.scan			= pci_nanoengine_scan_bus,
	.setup			= pci_nanoengine_setup,
};

static int __init nanoengine_pci_init(void)
{
	if (machine_is_nanoengine())
		pci_common_init(&nanoengine_pci);
	return 0;
}

subsys_initcall(nanoengine_pci_init);
