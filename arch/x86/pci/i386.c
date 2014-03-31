/*
 *	Low-Level PCI Access for i386 machines
 *
 * Copyright 1993, 1994 Drew Eckhardt
 *      Visionary Computing
 *      (Unix and Linux consulting and custom programming)
 *      Drew@Colorado.EDU
 *      +1 (303) 786-7975
 *
 * Drew's work was sponsored by:
 *	iX Multiuser Multitasking Magazine
 *	Hannover, Germany
 *	hm@ix.de
 *
 * Copyright 1997--2000 Martin Mares <mj@ucw.cz>
 *
 * For more information, please consult the following manuals (look at
 * http://www.pcisig.com/ for how to get them):
 *
 * PCI BIOS Specification
 * PCI Local Bus Specification
 * PCI to PCI Bridge Specification
 * PCI System Design Guide
 *
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/errno.h>
#include <linux/bootmem.h>

#include <asm/pat.h>
#include <asm/e820.h>
#include <asm/pci_x86.h>
#include <asm/io_apic.h>


struct pcibios_fwaddrmap {
	struct list_head list;
	struct pci_dev *dev;
	resource_size_t fw_addr[DEVICE_COUNT_RESOURCE];
};

static LIST_HEAD(pcibios_fwaddrmappings);
static DEFINE_SPINLOCK(pcibios_fwaddrmap_lock);

static struct pcibios_fwaddrmap *pcibios_fwaddrmap_lookup(struct pci_dev *dev)
{
	struct pcibios_fwaddrmap *map;

	WARN_ON(!spin_is_locked(&pcibios_fwaddrmap_lock));

	list_for_each_entry(map, &pcibios_fwaddrmappings, list)
		if (map->dev == dev)
			return map;

	return NULL;
}

static void
pcibios_save_fw_addr(struct pci_dev *dev, int idx, resource_size_t fw_addr)
{
	unsigned long flags;
	struct pcibios_fwaddrmap *map;

	spin_lock_irqsave(&pcibios_fwaddrmap_lock, flags);
	map = pcibios_fwaddrmap_lookup(dev);
	if (!map) {
		spin_unlock_irqrestore(&pcibios_fwaddrmap_lock, flags);
		map = kzalloc(sizeof(*map), GFP_KERNEL);
		if (!map)
			return;

		map->dev = pci_dev_get(dev);
		map->fw_addr[idx] = fw_addr;
		INIT_LIST_HEAD(&map->list);

		spin_lock_irqsave(&pcibios_fwaddrmap_lock, flags);
		list_add_tail(&map->list, &pcibios_fwaddrmappings);
	} else
		map->fw_addr[idx] = fw_addr;
	spin_unlock_irqrestore(&pcibios_fwaddrmap_lock, flags);
}

resource_size_t pcibios_retrieve_fw_addr(struct pci_dev *dev, int idx)
{
	unsigned long flags;
	struct pcibios_fwaddrmap *map;
	resource_size_t fw_addr = 0;

	spin_lock_irqsave(&pcibios_fwaddrmap_lock, flags);
	map = pcibios_fwaddrmap_lookup(dev);
	if (map)
		fw_addr = map->fw_addr[idx];
	spin_unlock_irqrestore(&pcibios_fwaddrmap_lock, flags);

	return fw_addr;
}

static void pcibios_fw_addr_list_del(void)
{
	unsigned long flags;
	struct pcibios_fwaddrmap *entry, *next;

	spin_lock_irqsave(&pcibios_fwaddrmap_lock, flags);
	list_for_each_entry_safe(entry, next, &pcibios_fwaddrmappings, list) {
		list_del(&entry->list);
		pci_dev_put(entry->dev);
		kfree(entry);
	}
	spin_unlock_irqrestore(&pcibios_fwaddrmap_lock, flags);
}

static int
skip_isa_ioresource_align(struct pci_dev *dev) {

	if ((pci_probe & PCI_CAN_SKIP_ISA_ALIGN) &&
	    !(dev->bus->bridge_ctl & PCI_BRIDGE_CTL_ISA))
		return 1;
	return 0;
}

resource_size_t
pcibios_align_resource(void *data, const struct resource *res,
			resource_size_t size, resource_size_t align)
{
	struct pci_dev *dev = data;
	resource_size_t start = res->start;

	if (res->flags & IORESOURCE_IO) {
		if (skip_isa_ioresource_align(dev))
			return start;
		if (start & 0x300)
			start = (start + 0x3ff) & ~0x3ff;
	}
	return start;
}
EXPORT_SYMBOL(pcibios_align_resource);


static void __init pcibios_allocate_bus_resources(struct list_head *bus_list)
{
	struct pci_bus *bus;
	struct pci_dev *dev;
	int idx;
	struct resource *r;

	
	list_for_each_entry(bus, bus_list, node) {
		if ((dev = bus->self)) {
			for (idx = PCI_BRIDGE_RESOURCES;
			    idx < PCI_NUM_RESOURCES; idx++) {
				r = &dev->resource[idx];
				if (!r->flags)
					continue;
				if (!r->start ||
				    pci_claim_resource(dev, idx) < 0) {
					r->start = r->end = 0;
					r->flags = 0;
				}
			}
		}
		pcibios_allocate_bus_resources(&bus->children);
	}
}

struct pci_check_idx_range {
	int start;
	int end;
};

static void __init pcibios_allocate_resources(int pass)
{
	struct pci_dev *dev = NULL;
	int idx, disabled, i;
	u16 command;
	struct resource *r;

	struct pci_check_idx_range idx_range[] = {
		{ PCI_STD_RESOURCES, PCI_STD_RESOURCE_END },
#ifdef CONFIG_PCI_IOV
		{ PCI_IOV_RESOURCES, PCI_IOV_RESOURCE_END },
#endif
	};

	for_each_pci_dev(dev) {
		pci_read_config_word(dev, PCI_COMMAND, &command);
		for (i = 0; i < ARRAY_SIZE(idx_range); i++)
		for (idx = idx_range[i].start; idx <= idx_range[i].end; idx++) {
			r = &dev->resource[idx];
			if (r->parent)		
				continue;
			if (!r->start)		
				continue;
			if (r->flags & IORESOURCE_IO)
				disabled = !(command & PCI_COMMAND_IO);
			else
				disabled = !(command & PCI_COMMAND_MEMORY);
			if (pass == disabled) {
				dev_dbg(&dev->dev,
					"BAR %d: reserving %pr (d=%d, p=%d)\n",
					idx, r, disabled, pass);
				if (pci_claim_resource(dev, idx) < 0) {
					
					pcibios_save_fw_addr(dev,
							idx, r->start);
					r->end -= r->start;
					r->start = 0;
				}
			}
		}
		if (!pass) {
			r = &dev->resource[PCI_ROM_RESOURCE];
			if (r->flags & IORESOURCE_ROM_ENABLE) {
				u32 reg;
				dev_dbg(&dev->dev, "disabling ROM %pR\n", r);
				r->flags &= ~IORESOURCE_ROM_ENABLE;
				pci_read_config_dword(dev,
						dev->rom_base_reg, &reg);
				pci_write_config_dword(dev, dev->rom_base_reg,
						reg & ~PCI_ROM_ADDRESS_ENABLE);
			}
		}
	}
}

static int __init pcibios_assign_resources(void)
{
	struct pci_dev *dev = NULL;
	struct resource *r;

	if (!(pci_probe & PCI_ASSIGN_ROMS)) {
		for_each_pci_dev(dev) {
			r = &dev->resource[PCI_ROM_RESOURCE];
			if (!r->flags || !r->start)
				continue;
			if (pci_claim_resource(dev, PCI_ROM_RESOURCE) < 0) {
				r->end -= r->start;
				r->start = 0;
			}
		}
	}

	pci_assign_unassigned_resources();
	pcibios_fw_addr_list_del();

	return 0;
}

void __init pcibios_resource_survey(void)
{
	DBG("PCI: Allocating resources\n");
	pcibios_allocate_bus_resources(&pci_root_buses);
	pcibios_allocate_resources(0);
	pcibios_allocate_resources(1);

	e820_reserve_resources_late();
	ioapic_insert_resources();
}

fs_initcall(pcibios_assign_resources);

static const struct vm_operations_struct pci_mmap_ops = {
	.access = generic_access_phys,
};

int pci_mmap_page_range(struct pci_dev *dev, struct vm_area_struct *vma,
			enum pci_mmap_state mmap_state, int write_combine)
{
	unsigned long prot;

	if (mmap_state == pci_mmap_io)
		return -EINVAL;

	prot = pgprot_val(vma->vm_page_prot);

	if (!pat_enabled && write_combine)
		return -EINVAL;

	if (pat_enabled && write_combine)
		prot |= _PAGE_CACHE_WC;
	else if (pat_enabled || boot_cpu_data.x86 > 3)
		prot |= _PAGE_CACHE_UC_MINUS;

	prot |= _PAGE_IOMAP;	

	vma->vm_page_prot = __pgprot(prot);

	if (io_remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
			       vma->vm_end - vma->vm_start,
			       vma->vm_page_prot))
		return -EAGAIN;

	vma->vm_ops = &pci_mmap_ops;

	return 0;
}
