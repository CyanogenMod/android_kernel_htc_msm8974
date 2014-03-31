/*
 * drivers/pci/rom.c
 *
 * (C) Copyright 2004 Jon Smirl <jonsmirl@yahoo.com>
 * (C) Copyright 2004 Silicon Graphics, Inc. Jesse Barnes <jbarnes@sgi.com>
 *
 * PCI ROM access routines
 */
#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/pci.h>
#include <linux/slab.h>

#include "pci.h"

int pci_enable_rom(struct pci_dev *pdev)
{
	struct resource *res = pdev->resource + PCI_ROM_RESOURCE;
	struct pci_bus_region region;
	u32 rom_addr;

	if (!res->flags)
		return -1;

	pcibios_resource_to_bus(pdev, &region, res);
	pci_read_config_dword(pdev, pdev->rom_base_reg, &rom_addr);
	rom_addr &= ~PCI_ROM_ADDRESS_MASK;
	rom_addr |= region.start | PCI_ROM_ADDRESS_ENABLE;
	pci_write_config_dword(pdev, pdev->rom_base_reg, rom_addr);
	return 0;
}

void pci_disable_rom(struct pci_dev *pdev)
{
	u32 rom_addr;
	pci_read_config_dword(pdev, pdev->rom_base_reg, &rom_addr);
	rom_addr &= ~PCI_ROM_ADDRESS_ENABLE;
	pci_write_config_dword(pdev, pdev->rom_base_reg, rom_addr);
}

size_t pci_get_rom_size(struct pci_dev *pdev, void __iomem *rom, size_t size)
{
	void __iomem *image;
	int last_image;

	image = rom;
	do {
		void __iomem *pds;
		
		if (readb(image) != 0x55) {
			dev_err(&pdev->dev, "Invalid ROM contents\n");
			break;
		}
		if (readb(image + 1) != 0xAA)
			break;
		
		pds = image + readw(image + 24);
		if (readb(pds) != 'P')
			break;
		if (readb(pds + 1) != 'C')
			break;
		if (readb(pds + 2) != 'I')
			break;
		if (readb(pds + 3) != 'R')
			break;
		last_image = readb(pds + 21) & 0x80;
		
		image += readw(pds + 16) * 512;
	} while (!last_image);

	
	
	return min((size_t)(image - rom), size);
}

void __iomem *pci_map_rom(struct pci_dev *pdev, size_t *size)
{
	struct resource *res = &pdev->resource[PCI_ROM_RESOURCE];
	loff_t start;
	void __iomem *rom;

	if (res->flags & IORESOURCE_ROM_SHADOW) {
		
		start = (loff_t)0xC0000;
		*size = 0x20000; 
	} else {
		if (res->flags &
			(IORESOURCE_ROM_COPY | IORESOURCE_ROM_BIOS_COPY)) {
			*size = pci_resource_len(pdev, PCI_ROM_RESOURCE);
			return (void __iomem *)(unsigned long)
				pci_resource_start(pdev, PCI_ROM_RESOURCE);
		} else {
			
			if (res->parent == NULL &&
			    pci_assign_resource(pdev,PCI_ROM_RESOURCE))
				return NULL;
			start = pci_resource_start(pdev, PCI_ROM_RESOURCE);
			*size = pci_resource_len(pdev, PCI_ROM_RESOURCE);
			if (*size == 0)
				return NULL;

			
			if (pci_enable_rom(pdev))
				return NULL;
		}
	}

	rom = ioremap(start, *size);
	if (!rom) {
		
		if (!(res->flags & (IORESOURCE_ROM_ENABLE |
				    IORESOURCE_ROM_SHADOW |
				    IORESOURCE_ROM_COPY)))
			pci_disable_rom(pdev);
		return NULL;
	}

	*size = pci_get_rom_size(pdev, rom, *size);
	return rom;
}

#if 0
void __iomem *pci_map_rom_copy(struct pci_dev *pdev, size_t *size)
{
	struct resource *res = &pdev->resource[PCI_ROM_RESOURCE];
	void __iomem *rom;

	rom = pci_map_rom(pdev, size);
	if (!rom)
		return NULL;

	if (res->flags & (IORESOURCE_ROM_COPY | IORESOURCE_ROM_SHADOW |
			  IORESOURCE_ROM_BIOS_COPY))
		return rom;

	res->start = (unsigned long)kmalloc(*size, GFP_KERNEL);
	if (!res->start)
		return rom;

	res->end = res->start + *size;
	memcpy_fromio((void*)(unsigned long)res->start, rom, *size);
	pci_unmap_rom(pdev, rom);
	res->flags |= IORESOURCE_ROM_COPY;

	return (void __iomem *)(unsigned long)res->start;
}
#endif  

void pci_unmap_rom(struct pci_dev *pdev, void __iomem *rom)
{
	struct resource *res = &pdev->resource[PCI_ROM_RESOURCE];

	if (res->flags & (IORESOURCE_ROM_COPY | IORESOURCE_ROM_BIOS_COPY))
		return;

	iounmap(rom);

	
	if (!(res->flags & (IORESOURCE_ROM_ENABLE | IORESOURCE_ROM_SHADOW)))
		pci_disable_rom(pdev);
}

#if 0
void pci_remove_rom(struct pci_dev *pdev)
{
	struct resource *res = &pdev->resource[PCI_ROM_RESOURCE];

	if (pci_resource_len(pdev, PCI_ROM_RESOURCE))
		sysfs_remove_bin_file(&pdev->dev.kobj, pdev->rom_attr);
	if (!(res->flags & (IORESOURCE_ROM_ENABLE |
			    IORESOURCE_ROM_SHADOW |
			    IORESOURCE_ROM_BIOS_COPY |
			    IORESOURCE_ROM_COPY)))
		pci_disable_rom(pdev);
}
#endif  

void pci_cleanup_rom(struct pci_dev *pdev)
{
	struct resource *res = &pdev->resource[PCI_ROM_RESOURCE];
	if (res->flags & IORESOURCE_ROM_COPY) {
		kfree((void*)(unsigned long)res->start);
		res->flags &= ~IORESOURCE_ROM_COPY;
		res->start = 0;
		res->end = 0;
	}
}

EXPORT_SYMBOL(pci_map_rom);
EXPORT_SYMBOL(pci_unmap_rom);
EXPORT_SYMBOL_GPL(pci_enable_rom);
EXPORT_SYMBOL_GPL(pci_disable_rom);
