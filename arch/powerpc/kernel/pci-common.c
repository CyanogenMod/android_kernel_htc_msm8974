/*
 * Contains common pci routines for ALL ppc platform
 * (based on pci_32.c and pci_64.c)
 *
 * Port for PPC64 David Engebretsen, IBM Corp.
 * Contains common pci routines for ppc64 platform, pSeries and iSeries brands.
 *
 * Copyright (C) 2003 Anton Blanchard <anton@au.ibm.com>, IBM
 *   Rework, based on alpha PCI code.
 *
 * Common pmac/prep/chrp pci routines. -- Cort
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/bootmem.h>
#include <linux/export.h>
#include <linux/of_address.h>
#include <linux/of_pci.h>
#include <linux/mm.h>
#include <linux/list.h>
#include <linux/syscalls.h>
#include <linux/irq.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>

#include <asm/processor.h>
#include <asm/io.h>
#include <asm/prom.h>
#include <asm/pci-bridge.h>
#include <asm/byteorder.h>
#include <asm/machdep.h>
#include <asm/ppc-pci.h>
#include <asm/eeh.h>

static DEFINE_SPINLOCK(hose_spinlock);
LIST_HEAD(hose_list);

static int global_phb_number;		

resource_size_t isa_mem_base;


static struct dma_map_ops *pci_dma_ops = &dma_direct_ops;

void set_pci_dma_ops(struct dma_map_ops *dma_ops)
{
	pci_dma_ops = dma_ops;
}

struct dma_map_ops *get_pci_dma_ops(void)
{
	return pci_dma_ops;
}
EXPORT_SYMBOL(get_pci_dma_ops);

struct pci_controller *pcibios_alloc_controller(struct device_node *dev)
{
	struct pci_controller *phb;

	phb = zalloc_maybe_bootmem(sizeof(struct pci_controller), GFP_KERNEL);
	if (phb == NULL)
		return NULL;
	spin_lock(&hose_spinlock);
	phb->global_number = global_phb_number++;
	list_add_tail(&phb->list_node, &hose_list);
	spin_unlock(&hose_spinlock);
	phb->dn = dev;
	phb->is_dynamic = mem_init_done;
#ifdef CONFIG_PPC64
	if (dev) {
		int nid = of_node_to_nid(dev);

		if (nid < 0 || !node_online(nid))
			nid = -1;

		PHB_SET_NODE(phb, nid);
	}
#endif
	return phb;
}

void pcibios_free_controller(struct pci_controller *phb)
{
	spin_lock(&hose_spinlock);
	list_del(&phb->list_node);
	spin_unlock(&hose_spinlock);

	if (phb->is_dynamic)
		kfree(phb);
}

static resource_size_t pcibios_io_size(const struct pci_controller *hose)
{
#ifdef CONFIG_PPC64
	return hose->pci_io_size;
#else
	return resource_size(&hose->io_resource);
#endif
}

int pcibios_vaddr_is_ioport(void __iomem *address)
{
	int ret = 0;
	struct pci_controller *hose;
	resource_size_t size;

	spin_lock(&hose_spinlock);
	list_for_each_entry(hose, &hose_list, list_node) {
		size = pcibios_io_size(hose);
		if (address >= hose->io_base_virt &&
		    address < (hose->io_base_virt + size)) {
			ret = 1;
			break;
		}
	}
	spin_unlock(&hose_spinlock);
	return ret;
}

unsigned long pci_address_to_pio(phys_addr_t address)
{
	struct pci_controller *hose;
	resource_size_t size;
	unsigned long ret = ~0;

	spin_lock(&hose_spinlock);
	list_for_each_entry(hose, &hose_list, list_node) {
		size = pcibios_io_size(hose);
		if (address >= hose->io_base_phys &&
		    address < (hose->io_base_phys + size)) {
			unsigned long base =
				(unsigned long)hose->io_base_virt - _IO_BASE;
			ret = base + (address - hose->io_base_phys);
			break;
		}
	}
	spin_unlock(&hose_spinlock);

	return ret;
}
EXPORT_SYMBOL_GPL(pci_address_to_pio);

int pci_domain_nr(struct pci_bus *bus)
{
	struct pci_controller *hose = pci_bus_to_host(bus);

	return hose->global_number;
}
EXPORT_SYMBOL(pci_domain_nr);

struct pci_controller* pci_find_hose_for_OF_device(struct device_node* node)
{
	while(node) {
		struct pci_controller *hose, *tmp;
		list_for_each_entry_safe(hose, tmp, &hose_list, list_node)
			if (hose->dn == node)
				return hose;
		node = node->parent;
	}
	return NULL;
}

static ssize_t pci_show_devspec(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct pci_dev *pdev;
	struct device_node *np;

	pdev = to_pci_dev (dev);
	np = pci_device_to_OF_node(pdev);
	if (np == NULL || np->full_name == NULL)
		return 0;
	return sprintf(buf, "%s", np->full_name);
}
static DEVICE_ATTR(devspec, S_IRUGO, pci_show_devspec, NULL);

int pcibios_add_platform_entries(struct pci_dev *pdev)
{
	return device_create_file(&pdev->dev, &dev_attr_devspec);
}

char __devinit *pcibios_setup(char *str)
{
	return str;
}

static int pci_read_irq_line(struct pci_dev *pci_dev)
{
	struct of_irq oirq;
	unsigned int virq;

	pr_debug("PCI: Try to map irq for %s...\n", pci_name(pci_dev));

#ifdef DEBUG
	memset(&oirq, 0xff, sizeof(oirq));
#endif
	
	if (of_irq_map_pci(pci_dev, &oirq)) {
		u8 line, pin;

		if (pci_read_config_byte(pci_dev, PCI_INTERRUPT_PIN, &pin))
			return -1;
		if (pin == 0)
			return -1;
		if (pci_read_config_byte(pci_dev, PCI_INTERRUPT_LINE, &line) ||
		    line == 0xff || line == 0) {
			return -1;
		}
		pr_debug(" No map ! Using line %d (pin %d) from PCI config\n",
			 line, pin);

		virq = irq_create_mapping(NULL, line);
		if (virq != NO_IRQ)
			irq_set_irq_type(virq, IRQ_TYPE_LEVEL_LOW);
	} else {
		pr_debug(" Got one, spec %d cells (0x%08x 0x%08x...) on %s\n",
			 oirq.size, oirq.specifier[0], oirq.specifier[1],
			 oirq.controller ? oirq.controller->full_name :
			 "<default>");

		virq = irq_create_of_mapping(oirq.controller, oirq.specifier,
					     oirq.size);
	}
	if(virq == NO_IRQ) {
		pr_debug(" Failed to map !\n");
		return -1;
	}

	pr_debug(" Mapped to linux irq %d\n", virq);

	pci_dev->irq = virq;

	return 0;
}


static struct resource *__pci_mmap_make_offset(struct pci_dev *dev,
					       resource_size_t *offset,
					       enum pci_mmap_state mmap_state)
{
	struct pci_controller *hose = pci_bus_to_host(dev->bus);
	unsigned long io_offset = 0;
	int i, res_bit;

	if (hose == 0)
		return NULL;		

	
	if (mmap_state == pci_mmap_mem) {
#if 0 
		*offset += hose->pci_mem_offset;
#endif
		res_bit = IORESOURCE_MEM;
	} else {
		io_offset = (unsigned long)hose->io_base_virt - _IO_BASE;
		*offset += io_offset;
		res_bit = IORESOURCE_IO;
	}

	for (i = 0; i <= PCI_ROM_RESOURCE; i++) {
		struct resource *rp = &dev->resource[i];
		int flags = rp->flags;

		
		if (i == PCI_ROM_RESOURCE)
			flags |= IORESOURCE_MEM;

		
		if ((flags & res_bit) == 0)
			continue;

		
		if (*offset < (rp->start & PAGE_MASK) || *offset > rp->end)
			continue;

		
		if (mmap_state == pci_mmap_io)
			*offset += hose->io_base_phys - io_offset;
		return rp;
	}

	return NULL;
}

static pgprot_t __pci_mmap_set_pgprot(struct pci_dev *dev, struct resource *rp,
				      pgprot_t protection,
				      enum pci_mmap_state mmap_state,
				      int write_combine)
{
	unsigned long prot = pgprot_val(protection);

	if (mmap_state != pci_mmap_mem)
		write_combine = 0;
	else if (write_combine == 0) {
		if (rp->flags & IORESOURCE_PREFETCH)
			write_combine = 1;
	}

	
	if (write_combine)
		return pgprot_noncached_wc(prot);
	else
		return pgprot_noncached(prot);
}

pgprot_t pci_phys_mem_access_prot(struct file *file,
				  unsigned long pfn,
				  unsigned long size,
				  pgprot_t prot)
{
	struct pci_dev *pdev = NULL;
	struct resource *found = NULL;
	resource_size_t offset = ((resource_size_t)pfn) << PAGE_SHIFT;
	int i;

	if (page_is_ram(pfn))
		return prot;

	prot = pgprot_noncached(prot);
	for_each_pci_dev(pdev) {
		for (i = 0; i <= PCI_ROM_RESOURCE; i++) {
			struct resource *rp = &pdev->resource[i];
			int flags = rp->flags;

			
			if ((flags & IORESOURCE_MEM) == 0)
				continue;
			
			if (offset < (rp->start & PAGE_MASK) ||
			    offset > rp->end)
				continue;
			found = rp;
			break;
		}
		if (found)
			break;
	}
	if (found) {
		if (found->flags & IORESOURCE_PREFETCH)
			prot = pgprot_noncached_wc(prot);
		pci_dev_put(pdev);
	}

	pr_debug("PCI: Non-PCI map for %llx, prot: %lx\n",
		 (unsigned long long)offset, pgprot_val(prot));

	return prot;
}


int pci_mmap_page_range(struct pci_dev *dev, struct vm_area_struct *vma,
			enum pci_mmap_state mmap_state, int write_combine)
{
	resource_size_t offset =
		((resource_size_t)vma->vm_pgoff) << PAGE_SHIFT;
	struct resource *rp;
	int ret;

	rp = __pci_mmap_make_offset(dev, &offset, mmap_state);
	if (rp == NULL)
		return -EINVAL;

	vma->vm_pgoff = offset >> PAGE_SHIFT;
	vma->vm_page_prot = __pci_mmap_set_pgprot(dev, rp,
						  vma->vm_page_prot,
						  mmap_state, write_combine);

	ret = remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
			       vma->vm_end - vma->vm_start, vma->vm_page_prot);

	return ret;
}

int pci_legacy_read(struct pci_bus *bus, loff_t port, u32 *val, size_t size)
{
	unsigned long offset;
	struct pci_controller *hose = pci_bus_to_host(bus);
	struct resource *rp = &hose->io_resource;
	void __iomem *addr;

	offset = (unsigned long)hose->io_base_virt - _IO_BASE;
	offset += port;

	if (!(rp->flags & IORESOURCE_IO))
		return -ENXIO;
	if (offset < rp->start || (offset + size) > rp->end)
		return -ENXIO;
	addr = hose->io_base_virt + port;

	switch(size) {
	case 1:
		*((u8 *)val) = in_8(addr);
		return 1;
	case 2:
		if (port & 1)
			return -EINVAL;
		*((u16 *)val) = in_le16(addr);
		return 2;
	case 4:
		if (port & 3)
			return -EINVAL;
		*((u32 *)val) = in_le32(addr);
		return 4;
	}
	return -EINVAL;
}

int pci_legacy_write(struct pci_bus *bus, loff_t port, u32 val, size_t size)
{
	unsigned long offset;
	struct pci_controller *hose = pci_bus_to_host(bus);
	struct resource *rp = &hose->io_resource;
	void __iomem *addr;

	offset = (unsigned long)hose->io_base_virt - _IO_BASE;
	offset += port;

	if (!(rp->flags & IORESOURCE_IO))
		return -ENXIO;
	if (offset < rp->start || (offset + size) > rp->end)
		return -ENXIO;
	addr = hose->io_base_virt + port;

	switch(size) {
	case 1:
		out_8(addr, val >> 24);
		return 1;
	case 2:
		if (port & 1)
			return -EINVAL;
		out_le16(addr, val >> 16);
		return 2;
	case 4:
		if (port & 3)
			return -EINVAL;
		out_le32(addr, val);
		return 4;
	}
	return -EINVAL;
}

int pci_mmap_legacy_page_range(struct pci_bus *bus,
			       struct vm_area_struct *vma,
			       enum pci_mmap_state mmap_state)
{
	struct pci_controller *hose = pci_bus_to_host(bus);
	resource_size_t offset =
		((resource_size_t)vma->vm_pgoff) << PAGE_SHIFT;
	resource_size_t size = vma->vm_end - vma->vm_start;
	struct resource *rp;

	pr_debug("pci_mmap_legacy_page_range(%04x:%02x, %s @%llx..%llx)\n",
		 pci_domain_nr(bus), bus->number,
		 mmap_state == pci_mmap_mem ? "MEM" : "IO",
		 (unsigned long long)offset,
		 (unsigned long long)(offset + size - 1));

	if (mmap_state == pci_mmap_mem) {
		if ((offset + size) > hose->isa_mem_size) {
			printk(KERN_DEBUG
			       "Process %s (pid:%d) mapped non-existing PCI legacy memory for 0%04x:%02x\n",
			       current->comm, current->pid, pci_domain_nr(bus), bus->number);
			if (vma->vm_flags & VM_SHARED)
				return shmem_zero_setup(vma);
			return 0;
		}
		offset += hose->isa_mem_phys;
	} else {
		unsigned long io_offset = (unsigned long)hose->io_base_virt - _IO_BASE;
		unsigned long roffset = offset + io_offset;
		rp = &hose->io_resource;
		if (!(rp->flags & IORESOURCE_IO))
			return -ENXIO;
		if (roffset < rp->start || (roffset + size) > rp->end)
			return -ENXIO;
		offset += hose->io_base_phys;
	}
	pr_debug(" -> mapping phys %llx\n", (unsigned long long)offset);

	vma->vm_pgoff = offset >> PAGE_SHIFT;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	return remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
			       vma->vm_end - vma->vm_start,
			       vma->vm_page_prot);
}

void pci_resource_to_user(const struct pci_dev *dev, int bar,
			  const struct resource *rsrc,
			  resource_size_t *start, resource_size_t *end)
{
	struct pci_controller *hose = pci_bus_to_host(dev->bus);
	resource_size_t offset = 0;

	if (hose == NULL)
		return;

	if (rsrc->flags & IORESOURCE_IO)
		offset = (unsigned long)hose->io_base_virt - _IO_BASE;

#if 0
	else if (rsrc->flags & IORESOURCE_MEM)
		offset = hose->pci_mem_offset;
#endif

	*start = rsrc->start - offset;
	*end = rsrc->end - offset;
}

void __devinit pci_process_bridge_OF_ranges(struct pci_controller *hose,
					    struct device_node *dev,
					    int primary)
{
	const u32 *ranges;
	int rlen;
	int pna = of_n_addr_cells(dev);
	int np = pna + 5;
	int memno = 0, isa_hole = -1;
	u32 pci_space;
	unsigned long long pci_addr, cpu_addr, pci_next, cpu_next, size;
	unsigned long long isa_mb = 0;
	struct resource *res;

	printk(KERN_INFO "PCI host bridge %s %s ranges:\n",
	       dev->full_name, primary ? "(primary)" : "");

	
	ranges = of_get_property(dev, "ranges", &rlen);
	if (ranges == NULL)
		return;

	
	while ((rlen -= np * 4) >= 0) {
		
		pci_space = ranges[0];
		pci_addr = of_read_number(ranges + 1, 2);
		cpu_addr = of_translate_address(dev, ranges + 3);
		size = of_read_number(ranges + pna + 3, 2);
		ranges += np;

		if (cpu_addr == OF_BAD_ADDR || size == 0)
			continue;

		
		for (; rlen >= np * sizeof(u32);
		     ranges += np, rlen -= np * 4) {
			if (ranges[0] != pci_space)
				break;
			pci_next = of_read_number(ranges + 1, 2);
			cpu_next = of_translate_address(dev, ranges + 3);
			if (pci_next != pci_addr + size ||
			    cpu_next != cpu_addr + size)
				break;
			size += of_read_number(ranges + pna + 3, 2);
		}

		
		res = NULL;
		switch ((pci_space >> 24) & 0x3) {
		case 1:		
			printk(KERN_INFO
			       "  IO 0x%016llx..0x%016llx -> 0x%016llx\n",
			       cpu_addr, cpu_addr + size - 1, pci_addr);

			
			if (hose->pci_io_size) {
				printk(KERN_INFO
				       " \\--> Skipped (too many) !\n");
				continue;
			}
#ifdef CONFIG_PPC32
			
			if (size > 0x01000000)
				size = 0x01000000;

			
			hose->io_base_virt = ioremap(cpu_addr, size);

			
			if (primary)
				isa_io_base =
					(unsigned long)hose->io_base_virt;
#endif 
			hose->pci_io_size = pci_addr + size;
			hose->io_base_phys = cpu_addr - pci_addr;

			
			res = &hose->io_resource;
			res->flags = IORESOURCE_IO;
			res->start = pci_addr;
			break;
		case 2:		
		case 3:		
			printk(KERN_INFO
			       " MEM 0x%016llx..0x%016llx -> 0x%016llx %s\n",
			       cpu_addr, cpu_addr + size - 1, pci_addr,
			       (pci_space & 0x40000000) ? "Prefetch" : "");

			
			if (memno >= 3) {
				printk(KERN_INFO
				       " \\--> Skipped (too many) !\n");
				continue;
			}
			
			if (pci_addr == 0) {
				isa_mb = cpu_addr;
				isa_hole = memno;
				if (primary || isa_mem_base == 0)
					isa_mem_base = cpu_addr;
				hose->isa_mem_phys = cpu_addr;
				hose->isa_mem_size = size;
			}

			if (memno == 0 ||
			    (isa_hole >= 0 && pci_addr != 0 &&
			     hose->pci_mem_offset == isa_mb))
				hose->pci_mem_offset = cpu_addr - pci_addr;
			else if (pci_addr != 0 &&
				 hose->pci_mem_offset != cpu_addr - pci_addr) {
				printk(KERN_INFO
				       " \\--> Skipped (offset mismatch) !\n");
				continue;
			}

			
			res = &hose->mem_resources[memno++];
			res->flags = IORESOURCE_MEM;
			if (pci_space & 0x40000000)
				res->flags |= IORESOURCE_PREFETCH;
			res->start = cpu_addr;
			break;
		}
		if (res != NULL) {
			res->name = dev->full_name;
			res->end = res->start + size - 1;
			res->parent = NULL;
			res->sibling = NULL;
			res->child = NULL;
		}
	}

	if (isa_hole >= 0 && hose->pci_mem_offset != isa_mb) {
		unsigned int next = isa_hole + 1;
		printk(KERN_INFO " Removing ISA hole at 0x%016llx\n", isa_mb);
		if (next < memno)
			memmove(&hose->mem_resources[isa_hole],
				&hose->mem_resources[next],
				sizeof(struct resource) * (memno - next));
		hose->mem_resources[--memno].flags = 0;
	}
}

int pci_proc_domain(struct pci_bus *bus)
{
	struct pci_controller *hose = pci_bus_to_host(bus);

	if (!pci_has_flag(PCI_ENABLE_PROC_DOMAINS))
		return 0;
	if (pci_has_flag(PCI_COMPAT_DOMAIN_0))
		return hose->global_number != 0;
	return 1;
}

static void __devinit pcibios_fixup_resources(struct pci_dev *dev)
{
	struct pci_controller *hose = pci_bus_to_host(dev->bus);
	int i;

	if (!hose) {
		printk(KERN_ERR "No host bridge for PCI dev %s !\n",
		       pci_name(dev));
		return;
	}
	for (i = 0; i < DEVICE_COUNT_RESOURCE; i++) {
		struct resource *res = dev->resource + i;
		if (!res->flags)
			continue;

		if (pci_has_flag(PCI_REASSIGN_ALL_RSRC) ||
		    (res->start == 0 && !pci_has_flag(PCI_PROBE_ONLY))) {
			
			if (!pci_has_flag(PCI_REASSIGN_ALL_RSRC))
				pr_debug("PCI:%s Resource %d %016llx-%016llx [%x] "
					 "is unassigned\n",
					 pci_name(dev), i,
					 (unsigned long long)res->start,
					 (unsigned long long)res->end,
					 (unsigned int)res->flags);
			res->end -= res->start;
			res->start = 0;
			res->flags |= IORESOURCE_UNSET;
			continue;
		}

		pr_debug("PCI:%s Resource %d %016llx-%016llx [%x]\n",
			 pci_name(dev), i,
			 (unsigned long long)res->start,\
			 (unsigned long long)res->end,
			 (unsigned int)res->flags);
	}

	
	if (ppc_md.pcibios_fixup_resources)
		ppc_md.pcibios_fixup_resources(dev);
}
DECLARE_PCI_FIXUP_HEADER(PCI_ANY_ID, PCI_ANY_ID, pcibios_fixup_resources);

static int __devinit pcibios_uninitialized_bridge_resource(struct pci_bus *bus,
							   struct resource *res)
{
	struct pci_controller *hose = pci_bus_to_host(bus);
	struct pci_dev *dev = bus->self;
	resource_size_t offset;
	u16 command;
	int i;

	
	if (pci_has_flag(PCI_PROBE_ONLY))
		return 0;

	
	if (res->flags & IORESOURCE_MEM) {
		if (res->start != hose->pci_mem_offset)
			return 0;

		pci_read_config_word(dev, PCI_COMMAND, &command);
		if ((command & PCI_COMMAND_MEMORY) == 0)
			return 1;

		for (i = 0; i < 3; i++) {
			if ((hose->mem_resources[i].flags & IORESOURCE_MEM) &&
			    hose->mem_resources[i].start == hose->pci_mem_offset)
				return 0;
		}

		return 1;
	} else {
		
		offset = (unsigned long)hose->io_base_virt - _IO_BASE;
		if (((res->start - offset) & 0xfffffffful) != 0)
			return 0;

		pci_read_config_word(dev, PCI_COMMAND, &command);
		if (command & PCI_COMMAND_IO)
			return 0;

		return 1;
	}
}

static void __devinit pcibios_fixup_bridge(struct pci_bus *bus)
{
	struct resource *res;
	int i;

	struct pci_dev *dev = bus->self;

	pci_bus_for_each_resource(bus, res, i) {
		if (!res || !res->flags)
			continue;
		if (i >= 3 && bus->self->transparent)
			continue;

		if (pci_has_flag(PCI_REASSIGN_ALL_RSRC)) {
			res->flags |= IORESOURCE_UNSET;
			res->end -= res->start;
			res->start = 0;
			continue;
		}

		pr_debug("PCI:%s Bus rsrc %d %016llx-%016llx [%x]\n",
			 pci_name(dev), i,
			 (unsigned long long)res->start,\
			 (unsigned long long)res->end,
			 (unsigned int)res->flags);

		if (pcibios_uninitialized_bridge_resource(bus, res)) {
			res->flags = 0;
			pr_debug("PCI:%s            (unassigned)\n", pci_name(dev));
		}
	}
}

void __devinit pcibios_setup_bus_self(struct pci_bus *bus)
{
	
	if (bus->self != NULL)
		pcibios_fixup_bridge(bus);

	if (ppc_md.pcibios_fixup_bus)
		ppc_md.pcibios_fixup_bus(bus);

	
	if (ppc_md.pci_dma_bus_setup)
		ppc_md.pci_dma_bus_setup(bus);
}

void __devinit pcibios_setup_bus_devices(struct pci_bus *bus)
{
	struct pci_dev *dev;

	pr_debug("PCI: Fixup bus devices %d (%s)\n",
		 bus->number, bus->self ? pci_name(bus->self) : "PHB");

	list_for_each_entry(dev, &bus->devices, bus_list) {
		if (dev->is_added)
			continue;

		set_dev_node(&dev->dev, pcibus_to_node(dev->bus));

		
		set_dma_ops(&dev->dev, pci_dma_ops);
		set_dma_offset(&dev->dev, PCI_DRAM_OFFSET);

		
		if (ppc_md.pci_dma_dev_setup)
			ppc_md.pci_dma_dev_setup(dev);

		
		pci_read_irq_line(dev);
		if (ppc_md.pci_irq_fixup)
			ppc_md.pci_irq_fixup(dev);
	}
}

void pcibios_set_master(struct pci_dev *dev)
{
	
}

void __devinit pcibios_fixup_bus(struct pci_bus *bus)
{
	if (bus->self != NULL)
		pci_read_bridge_bases(bus);

	
	pcibios_setup_bus_self(bus);

	
	pcibios_setup_bus_devices(bus);
}
EXPORT_SYMBOL(pcibios_fixup_bus);

void __devinit pci_fixup_cardbus(struct pci_bus *bus)
{
	
	pcibios_setup_bus_devices(bus);
}


static int skip_isa_ioresource_align(struct pci_dev *dev)
{
	if (pci_has_flag(PCI_CAN_SKIP_ISA_ALIGN) &&
	    !(dev->bus->bridge_ctl & PCI_BRIDGE_CTL_ISA))
		return 1;
	return 0;
}

resource_size_t pcibios_align_resource(void *data, const struct resource *res,
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

static int reparent_resources(struct resource *parent,
				     struct resource *res)
{
	struct resource *p, **pp;
	struct resource **firstpp = NULL;

	for (pp = &parent->child; (p = *pp) != NULL; pp = &p->sibling) {
		if (p->end < res->start)
			continue;
		if (res->end < p->start)
			break;
		if (p->start < res->start || p->end > res->end)
			return -1;	
		if (firstpp == NULL)
			firstpp = pp;
	}
	if (firstpp == NULL)
		return -1;	
	res->parent = parent;
	res->child = *firstpp;
	res->sibling = *pp;
	*firstpp = res;
	*pp = NULL;
	for (p = res->child; p != NULL; p = p->sibling) {
		p->parent = res;
		pr_debug("PCI: Reparented %s [%llx..%llx] under %s\n",
			 p->name,
			 (unsigned long long)p->start,
			 (unsigned long long)p->end, res->name);
	}
	return 0;
}


void pcibios_allocate_bus_resources(struct pci_bus *bus)
{
	struct pci_bus *b;
	int i;
	struct resource *res, *pr;

	pr_debug("PCI: Allocating bus resources for %04x:%02x...\n",
		 pci_domain_nr(bus), bus->number);

	pci_bus_for_each_resource(bus, res, i) {
		if (!res || !res->flags || res->start > res->end || res->parent)
			continue;

		
		if (res->flags & IORESOURCE_UNSET)
			goto clear_resource;

		if (bus->parent == NULL)
			pr = (res->flags & IORESOURCE_IO) ?
				&ioport_resource : &iomem_resource;
		else {
			pr = pci_find_parent_resource(bus->self, res);
			if (pr == res) {
				continue;
			}
		}

		pr_debug("PCI: %s (bus %d) bridge rsrc %d: %016llx-%016llx "
			 "[0x%x], parent %p (%s)\n",
			 bus->self ? pci_name(bus->self) : "PHB",
			 bus->number, i,
			 (unsigned long long)res->start,
			 (unsigned long long)res->end,
			 (unsigned int)res->flags,
			 pr, (pr && pr->name) ? pr->name : "nil");

		if (pr && !(pr->flags & IORESOURCE_UNSET)) {
			if (request_resource(pr, res) == 0)
				continue;
			if (reparent_resources(pr, res) == 0)
				continue;
		}
		pr_warning("PCI: Cannot allocate resource region "
			   "%d of PCI bridge %d, will remap\n", i, bus->number);
	clear_resource:
		res->start = res->end = 0;
		res->flags = 0;
	}

	list_for_each_entry(b, &bus->children, node)
		pcibios_allocate_bus_resources(b);
}

static inline void __devinit alloc_resource(struct pci_dev *dev, int idx)
{
	struct resource *pr, *r = &dev->resource[idx];

	pr_debug("PCI: Allocating %s: Resource %d: %016llx..%016llx [%x]\n",
		 pci_name(dev), idx,
		 (unsigned long long)r->start,
		 (unsigned long long)r->end,
		 (unsigned int)r->flags);

	pr = pci_find_parent_resource(dev, r);
	if (!pr || (pr->flags & IORESOURCE_UNSET) ||
	    request_resource(pr, r) < 0) {
		printk(KERN_WARNING "PCI: Cannot allocate resource region %d"
		       " of device %s, will remap\n", idx, pci_name(dev));
		if (pr)
			pr_debug("PCI:  parent is %p: %016llx-%016llx [%x]\n",
				 pr,
				 (unsigned long long)pr->start,
				 (unsigned long long)pr->end,
				 (unsigned int)pr->flags);
		
		r->flags |= IORESOURCE_UNSET;
		r->end -= r->start;
		r->start = 0;
	}
}

static void __init pcibios_allocate_resources(int pass)
{
	struct pci_dev *dev = NULL;
	int idx, disabled;
	u16 command;
	struct resource *r;

	for_each_pci_dev(dev) {
		pci_read_config_word(dev, PCI_COMMAND, &command);
		for (idx = 0; idx <= PCI_ROM_RESOURCE; idx++) {
			r = &dev->resource[idx];
			if (r->parent)		
				continue;
			if (!r->flags || (r->flags & IORESOURCE_UNSET))
				continue;	
			if (idx == PCI_ROM_RESOURCE )
				disabled = 1;
			if (r->flags & IORESOURCE_IO)
				disabled = !(command & PCI_COMMAND_IO);
			else
				disabled = !(command & PCI_COMMAND_MEMORY);
			if (pass == disabled)
				alloc_resource(dev, idx);
		}
		if (pass)
			continue;
		r = &dev->resource[PCI_ROM_RESOURCE];
		if (r->flags) {
			u32 reg;
			pci_read_config_dword(dev, dev->rom_base_reg, &reg);
			if (reg & PCI_ROM_ADDRESS_ENABLE) {
				pr_debug("PCI: Switching off ROM of %s\n",
					 pci_name(dev));
				r->flags &= ~IORESOURCE_ROM_ENABLE;
				pci_write_config_dword(dev, dev->rom_base_reg,
						       reg & ~PCI_ROM_ADDRESS_ENABLE);
			}
		}
	}
}

static void __init pcibios_reserve_legacy_regions(struct pci_bus *bus)
{
	struct pci_controller *hose = pci_bus_to_host(bus);
	resource_size_t	offset;
	struct resource *res, *pres;
	int i;

	pr_debug("Reserving legacy ranges for domain %04x\n", pci_domain_nr(bus));

	
	if (!(hose->io_resource.flags & IORESOURCE_IO))
		goto no_io;
	offset = (unsigned long)hose->io_base_virt - _IO_BASE;
	res = kzalloc(sizeof(struct resource), GFP_KERNEL);
	BUG_ON(res == NULL);
	res->name = "Legacy IO";
	res->flags = IORESOURCE_IO;
	res->start = offset;
	res->end = (offset + 0xfff) & 0xfffffffful;
	pr_debug("Candidate legacy IO: %pR\n", res);
	if (request_resource(&hose->io_resource, res)) {
		printk(KERN_DEBUG
		       "PCI %04x:%02x Cannot reserve Legacy IO %pR\n",
		       pci_domain_nr(bus), bus->number, res);
		kfree(res);
	}

 no_io:
	
	offset = hose->pci_mem_offset;
	pr_debug("hose mem offset: %016llx\n", (unsigned long long)offset);
	for (i = 0; i < 3; i++) {
		pres = &hose->mem_resources[i];
		if (!(pres->flags & IORESOURCE_MEM))
			continue;
		pr_debug("hose mem res: %pR\n", pres);
		if ((pres->start - offset) <= 0xa0000 &&
		    (pres->end - offset) >= 0xbffff)
			break;
	}
	if (i >= 3)
		return;
	res = kzalloc(sizeof(struct resource), GFP_KERNEL);
	BUG_ON(res == NULL);
	res->name = "Legacy VGA memory";
	res->flags = IORESOURCE_MEM;
	res->start = 0xa0000 + offset;
	res->end = 0xbffff + offset;
	pr_debug("Candidate VGA memory: %pR\n", res);
	if (request_resource(pres, res)) {
		printk(KERN_DEBUG
		       "PCI %04x:%02x Cannot reserve VGA memory %pR\n",
		       pci_domain_nr(bus), bus->number, res);
		kfree(res);
	}
}

void __init pcibios_resource_survey(void)
{
	struct pci_bus *b;

	
	list_for_each_entry(b, &pci_root_buses, node)
		pcibios_allocate_bus_resources(b);
	pcibios_allocate_resources(0);
	pcibios_allocate_resources(1);

	if (!pci_has_flag(PCI_PROBE_ONLY)) {
		list_for_each_entry(b, &pci_root_buses, node)
			pcibios_reserve_legacy_regions(b);
	}

	if (!pci_has_flag(PCI_PROBE_ONLY)) {
		pr_debug("PCI: Assigning unassigned resources...\n");
		pci_assign_unassigned_resources();
	}

	
	if (ppc_md.pcibios_fixup)
		ppc_md.pcibios_fixup();
}

#ifdef CONFIG_HOTPLUG

void pcibios_claim_one_bus(struct pci_bus *bus)
{
	struct pci_dev *dev;
	struct pci_bus *child_bus;

	list_for_each_entry(dev, &bus->devices, bus_list) {
		int i;

		for (i = 0; i < PCI_NUM_RESOURCES; i++) {
			struct resource *r = &dev->resource[i];

			if (r->parent || !r->start || !r->flags)
				continue;

			pr_debug("PCI: Claiming %s: "
				 "Resource %d: %016llx..%016llx [%x]\n",
				 pci_name(dev), i,
				 (unsigned long long)r->start,
				 (unsigned long long)r->end,
				 (unsigned int)r->flags);

			pci_claim_resource(dev, i);
		}
	}

	list_for_each_entry(child_bus, &bus->children, node)
		pcibios_claim_one_bus(child_bus);
}


void pcibios_finish_adding_to_bus(struct pci_bus *bus)
{
	pr_debug("PCI: Finishing adding to hotplug bus %04x:%02x\n",
		 pci_domain_nr(bus), bus->number);

	
	pcibios_allocate_bus_resources(bus);
	pcibios_claim_one_bus(bus);

	
	pci_bus_add_devices(bus);

	
	eeh_add_device_tree_late(bus);
}
EXPORT_SYMBOL_GPL(pcibios_finish_adding_to_bus);

#endif 

int pcibios_enable_device(struct pci_dev *dev, int mask)
{
	if (ppc_md.pcibios_enable_device_hook)
		if (ppc_md.pcibios_enable_device_hook(dev))
			return -EINVAL;

	return pci_enable_resources(dev, mask);
}

resource_size_t pcibios_io_space_offset(struct pci_controller *hose)
{
	return (unsigned long) hose->io_base_virt - _IO_BASE;
}

static void __devinit pcibios_setup_phb_resources(struct pci_controller *hose, struct list_head *resources)
{
	struct resource *res;
	int i;

	
	res = &hose->io_resource;

	if (!res->flags) {
		printk(KERN_WARNING "PCI: I/O resource not set for host"
		       " bridge %s (domain %d)\n",
		       hose->dn->full_name, hose->global_number);
#ifdef CONFIG_PPC32
		
		res->start = (unsigned long)hose->io_base_virt - isa_io_base;
		res->end = res->start + IO_SPACE_LIMIT;
		res->flags = IORESOURCE_IO;
#endif 
	}

	pr_debug("PCI: PHB IO resource    = %016llx-%016llx [%lx]\n",
		 (unsigned long long)res->start,
		 (unsigned long long)res->end,
		 (unsigned long)res->flags);
	pci_add_resource_offset(resources, res, pcibios_io_space_offset(hose));

	
	for (i = 0; i < 3; ++i) {
		res = &hose->mem_resources[i];
		if (!res->flags) {
			if (i > 0)
				continue;
			printk(KERN_ERR "PCI: Memory resource 0 not set for "
			       "host bridge %s (domain %d)\n",
			       hose->dn->full_name, hose->global_number);
#ifdef CONFIG_PPC32
			
			res->start = hose->pci_mem_offset;
			res->end = (resource_size_t)-1LL;
			res->flags = IORESOURCE_MEM;
#endif 
		}

		pr_debug("PCI: PHB MEM resource %d = %016llx-%016llx [%lx]\n", i,
			 (unsigned long long)res->start,
			 (unsigned long long)res->end,
			 (unsigned long)res->flags);
		pci_add_resource_offset(resources, res, hose->pci_mem_offset);
	}

	pr_debug("PCI: PHB MEM offset     = %016llx\n",
		 (unsigned long long)hose->pci_mem_offset);
	pr_debug("PCI: PHB IO  offset     = %08lx\n",
		 (unsigned long)hose->io_base_virt - _IO_BASE);

}

#define NULL_PCI_OP(rw, size, type)					\
static int								\
null_##rw##_config_##size(struct pci_dev *dev, int offset, type val)	\
{									\
	return PCIBIOS_DEVICE_NOT_FOUND;    				\
}

static int
null_read_config(struct pci_bus *bus, unsigned int devfn, int offset,
		 int len, u32 *val)
{
	return PCIBIOS_DEVICE_NOT_FOUND;
}

static int
null_write_config(struct pci_bus *bus, unsigned int devfn, int offset,
		  int len, u32 val)
{
	return PCIBIOS_DEVICE_NOT_FOUND;
}

static struct pci_ops null_pci_ops =
{
	.read = null_read_config,
	.write = null_write_config,
};

static struct pci_bus *
fake_pci_bus(struct pci_controller *hose, int busnr)
{
	static struct pci_bus bus;

	if (hose == 0) {
		printk(KERN_ERR "Can't find hose for PCI bus %d!\n", busnr);
	}
	bus.number = busnr;
	bus.sysdata = hose;
	bus.ops = hose? hose->ops: &null_pci_ops;
	return &bus;
}

#define EARLY_PCI_OP(rw, size, type)					\
int early_##rw##_config_##size(struct pci_controller *hose, int bus,	\
			       int devfn, int offset, type value)	\
{									\
	return pci_bus_##rw##_config_##size(fake_pci_bus(hose, bus),	\
					    devfn, offset, value);	\
}

EARLY_PCI_OP(read, byte, u8 *)
EARLY_PCI_OP(read, word, u16 *)
EARLY_PCI_OP(read, dword, u32 *)
EARLY_PCI_OP(write, byte, u8)
EARLY_PCI_OP(write, word, u16)
EARLY_PCI_OP(write, dword, u32)

extern int pci_bus_find_capability (struct pci_bus *bus, unsigned int devfn, int cap);
int early_find_capability(struct pci_controller *hose, int bus, int devfn,
			  int cap)
{
	return pci_bus_find_capability(fake_pci_bus(hose, bus), devfn, cap);
}

struct device_node *pcibios_get_phb_of_node(struct pci_bus *bus)
{
	struct pci_controller *hose = bus->sysdata;

	return of_node_get(hose->dn);
}

void __devinit pcibios_scan_phb(struct pci_controller *hose)
{
	LIST_HEAD(resources);
	struct pci_bus *bus;
	struct device_node *node = hose->dn;
	int mode;

	pr_debug("PCI: Scanning PHB %s\n",
		 node ? node->full_name : "<NO NAME>");

	
	pcibios_setup_phb_io_space(hose);

	
	pcibios_setup_phb_resources(hose, &resources);

	
	bus = pci_create_root_bus(hose->parent, hose->first_busno,
				  hose->ops, hose, &resources);
	if (bus == NULL) {
		pr_err("Failed to create bus for PCI domain %04x\n",
			hose->global_number);
		pci_free_resource_list(&resources);
		return;
	}
	bus->secondary = hose->first_busno;
	hose->bus = bus;

	
	mode = PCI_PROBE_NORMAL;
	if (node && ppc_md.pci_probe_mode)
		mode = ppc_md.pci_probe_mode(bus);
	pr_debug("    probe mode: %d\n", mode);
	if (mode == PCI_PROBE_DEVTREE) {
		bus->subordinate = hose->last_busno;
		of_scan_bus(node, bus);
	}

	if (mode == PCI_PROBE_NORMAL)
		hose->last_busno = bus->subordinate = pci_scan_child_bus(bus);

	if (ppc_md.pcibios_fixup_phb)
		ppc_md.pcibios_fixup_phb(hose);

	
	if (bus && !pci_has_flag(PCI_PROBE_ONLY)) {
		struct pci_bus *child;
		list_for_each_entry(child, &bus->children, node) {
			struct pci_dev *self = child->self;
			if (!self)
				continue;
			pcie_bus_configure_settings(child, self->pcie_mpss);
		}
	}
}

static void fixup_hide_host_resource_fsl(struct pci_dev *dev)
{
	int i, class = dev->class >> 8;
	
	int prog_if = dev->class & 0xf;

	if ((class == PCI_CLASS_PROCESSOR_POWERPC ||
	     class == PCI_CLASS_BRIDGE_OTHER) &&
		(dev->hdr_type == PCI_HEADER_TYPE_NORMAL) &&
		(prog_if == 0) &&
		(dev->bus->parent == NULL)) {
		for (i = 0; i < DEVICE_COUNT_RESOURCE; i++) {
			dev->resource[i].start = 0;
			dev->resource[i].end = 0;
			dev->resource[i].flags = 0;
		}
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_MOTOROLA, PCI_ANY_ID, fixup_hide_host_resource_fsl);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_FREESCALE, PCI_ANY_ID, fixup_hide_host_resource_fsl);
