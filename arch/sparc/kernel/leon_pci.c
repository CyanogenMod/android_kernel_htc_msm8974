/*
 * leon_pci.c: LEON Host PCI support
 *
 * Copyright (C) 2011 Aeroflex Gaisler AB, Daniel Hellstrom
 *
 * Code is partially derived from pcic.c
 */

#include <linux/of_device.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/export.h>
#include <asm/leon.h>
#include <asm/leon_pci.h>

void leon_pci_init(struct platform_device *ofdev, struct leon_pci_info *info)
{
	LIST_HEAD(resources);
	struct pci_bus *root_bus;

	pci_add_resource_offset(&resources, &info->io_space,
				info->io_space.start - 0x1000);
	pci_add_resource(&resources, &info->mem_space);

	root_bus = pci_scan_root_bus(&ofdev->dev, 0, info->ops, info,
				     &resources);
	if (root_bus) {
		
		pci_fixup_irqs(pci_common_swizzle, info->map_irq);

		
		pci_assign_unassigned_resources();
	} else {
		pci_free_resource_list(&resources);
	}
}

void __devinit pcibios_fixup_bus(struct pci_bus *pbus)
{
	struct pci_dev *dev;
	int i, has_io, has_mem;
	u16 cmd;

	list_for_each_entry(dev, &pbus->devices, bus_list) {
		has_io = has_mem = 0;
		for (i = 0; i < PCI_ROM_RESOURCE; i++) {
			unsigned long f = dev->resource[i].flags;
			if (f & IORESOURCE_IO)
				has_io = 1;
			else if (f & IORESOURCE_MEM)
				has_mem = 1;
		}
		
		if (dev->resource[PCI_ROM_RESOURCE].end != 0) {
			dev->resource[PCI_ROM_RESOURCE].flags |=
							IORESOURCE_ROM_ENABLE;
			has_mem = 1;
		}
		pci_bus_read_config_word(pbus, dev->devfn, PCI_COMMAND, &cmd);
		if (has_io && !(cmd & PCI_COMMAND_IO)) {
#ifdef CONFIG_PCI_DEBUG
			printk(KERN_INFO "LEONPCI: Enabling I/O for dev %s\n",
					 pci_name(dev));
#endif
			cmd |= PCI_COMMAND_IO;
			pci_bus_write_config_word(pbus, dev->devfn, PCI_COMMAND,
									cmd);
		}
		if (has_mem && !(cmd & PCI_COMMAND_MEMORY)) {
#ifdef CONFIG_PCI_DEBUG
			printk(KERN_INFO "LEONPCI: Enabling MEMORY for dev"
					 "%s\n", pci_name(dev));
#endif
			cmd |= PCI_COMMAND_MEMORY;
			pci_bus_write_config_word(pbus, dev->devfn, PCI_COMMAND,
									cmd);
		}
	}
}

char * __devinit pcibios_setup(char *str)
{
	return str;
}

resource_size_t pcibios_align_resource(void *data, const struct resource *res,
				resource_size_t size, resource_size_t align)
{
	return res->start;
}

int pcibios_enable_device(struct pci_dev *dev, int mask)
{
	return pci_enable_resources(dev, mask);
}

void __devinit pcibios_update_irq(struct pci_dev *dev, int irq)
{
#ifdef CONFIG_PCI_DEBUG
	printk(KERN_DEBUG "LEONPCI: Assigning IRQ %02d to %s\n", irq,
		pci_name(dev));
#endif
	pci_write_config_byte(dev, PCI_INTERRUPT_LINE, irq);
}

void outsb(unsigned long addr, const void *src, unsigned long count)
{
	while (count) {
		count -= 1;
		outb(*(const char *)src, addr);
		src += 1;
		
	}
}
EXPORT_SYMBOL(outsb);

void outsw(unsigned long addr, const void *src, unsigned long count)
{
	while (count) {
		count -= 2;
		outw(*(const short *)src, addr);
		src += 2;
		
	}
}
EXPORT_SYMBOL(outsw);

void outsl(unsigned long addr, const void *src, unsigned long count)
{
	while (count) {
		count -= 4;
		outl(*(const long *)src, addr);
		src += 4;
		
	}
}
EXPORT_SYMBOL(outsl);

void insb(unsigned long addr, void *dst, unsigned long count)
{
	while (count) {
		count -= 1;
		*(unsigned char *)dst = inb(addr);
		dst += 1;
		
	}
}
EXPORT_SYMBOL(insb);

void insw(unsigned long addr, void *dst, unsigned long count)
{
	while (count) {
		count -= 2;
		*(unsigned short *)dst = inw(addr);
		dst += 2;
		
	}
}
EXPORT_SYMBOL(insw);

void insl(unsigned long addr, void *dst, unsigned long count)
{
	while (count) {
		count -= 4;
		*(unsigned long *)dst = inl(addr);
		dst += 4;
		
	}
}
EXPORT_SYMBOL(insl);
