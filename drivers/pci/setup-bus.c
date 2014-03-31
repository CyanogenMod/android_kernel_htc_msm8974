/*
 *	drivers/pci/setup-bus.c
 *
 * Extruded from code written by
 *      Dave Rusling (david.rusling@reo.mts.dec.com)
 *      David Mosberger (davidm@cs.arizona.edu)
 *	David Miller (davem@redhat.com)
 *
 * Support routines for initializing a PCI subsystem.
 */


#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/cache.h>
#include <linux/slab.h>
#include <asm-generic/pci-bridge.h>
#include "pci.h"

unsigned int pci_flags;

struct pci_dev_resource {
	struct list_head list;
	struct resource *res;
	struct pci_dev *dev;
	resource_size_t start;
	resource_size_t end;
	resource_size_t add_size;
	resource_size_t min_align;
	unsigned long flags;
};

static void free_list(struct list_head *head)
{
	struct pci_dev_resource *dev_res, *tmp;

	list_for_each_entry_safe(dev_res, tmp, head, list) {
		list_del(&dev_res->list);
		kfree(dev_res);
	}
}

static int add_to_list(struct list_head *head,
		 struct pci_dev *dev, struct resource *res,
		 resource_size_t add_size, resource_size_t min_align)
{
	struct pci_dev_resource *tmp;

	tmp = kzalloc(sizeof(*tmp), GFP_KERNEL);
	if (!tmp) {
		pr_warning("add_to_list: kmalloc() failed!\n");
		return -ENOMEM;
	}

	tmp->res = res;
	tmp->dev = dev;
	tmp->start = res->start;
	tmp->end = res->end;
	tmp->flags = res->flags;
	tmp->add_size = add_size;
	tmp->min_align = min_align;

	list_add(&tmp->list, head);

	return 0;
}

static void remove_from_list(struct list_head *head,
				 struct resource *res)
{
	struct pci_dev_resource *dev_res, *tmp;

	list_for_each_entry_safe(dev_res, tmp, head, list) {
		if (dev_res->res == res) {
			list_del(&dev_res->list);
			kfree(dev_res);
			break;
		}
	}
}

static resource_size_t get_res_add_size(struct list_head *head,
					struct resource *res)
{
	struct pci_dev_resource *dev_res;

	list_for_each_entry(dev_res, head, list) {
		if (dev_res->res == res) {
			int idx = res - &dev_res->dev->resource[0];

			dev_printk(KERN_DEBUG, &dev_res->dev->dev,
				 "res[%d]=%pR get_res_add_size add_size %llx\n",
				 idx, dev_res->res,
				 (unsigned long long)dev_res->add_size);

			return dev_res->add_size;
		}
	}

	return 0;
}

static void pdev_sort_resources(struct pci_dev *dev, struct list_head *head)
{
	int i;

	for (i = 0; i < PCI_NUM_RESOURCES; i++) {
		struct resource *r;
		struct pci_dev_resource *dev_res, *tmp;
		resource_size_t r_align;
		struct list_head *n;

		r = &dev->resource[i];

		if (r->flags & IORESOURCE_PCI_FIXED)
			continue;

		if (!(r->flags) || r->parent)
			continue;

		r_align = pci_resource_alignment(dev, r);
		if (!r_align) {
			dev_warn(&dev->dev, "BAR %d: %pR has bogus alignment\n",
				 i, r);
			continue;
		}

		tmp = kzalloc(sizeof(*tmp), GFP_KERNEL);
		if (!tmp)
			panic("pdev_sort_resources(): "
			      "kmalloc() failed!\n");
		tmp->res = r;
		tmp->dev = dev;

		
		n = head;
		list_for_each_entry(dev_res, head, list) {
			resource_size_t align;

			align = pci_resource_alignment(dev_res->dev,
							 dev_res->res);

			if (r_align > align) {
				n = &dev_res->list;
				break;
			}
		}
		
		list_add_tail(&tmp->list, n);
	}
}

static void __dev_sort_resources(struct pci_dev *dev,
				 struct list_head *head)
{
	u16 class = dev->class >> 8;

	
	if (class == PCI_CLASS_NOT_DEFINED || class == PCI_CLASS_BRIDGE_HOST)
		return;

	
	if (class == PCI_CLASS_SYSTEM_PIC) {
		u16 command;
		pci_read_config_word(dev, PCI_COMMAND, &command);
		if (command & (PCI_COMMAND_IO | PCI_COMMAND_MEMORY))
			return;
	}

	pdev_sort_resources(dev, head);
}

static inline void reset_resource(struct resource *res)
{
	res->start = 0;
	res->end = 0;
	res->flags = 0;
}

static void reassign_resources_sorted(struct list_head *realloc_head,
		struct list_head *head)
{
	struct resource *res;
	struct pci_dev_resource *add_res, *tmp;
	struct pci_dev_resource *dev_res;
	resource_size_t add_size;
	int idx;

	list_for_each_entry_safe(add_res, tmp, realloc_head, list) {
		bool found_match = false;

		res = add_res->res;
		
		if (!res->flags)
			goto out;

		
		list_for_each_entry(dev_res, head, list) {
			if (dev_res->res == res) {
				found_match = true;
				break;
			}
		}
		if (!found_match)
			continue;

		idx = res - &add_res->dev->resource[0];
		add_size = add_res->add_size;
		if (!resource_size(res)) {
			res->start = add_res->start;
			res->end = res->start + add_size - 1;
			if (pci_assign_resource(add_res->dev, idx))
				reset_resource(res);
		} else {
			resource_size_t align = add_res->min_align;
			res->flags |= add_res->flags &
				 (IORESOURCE_STARTALIGN|IORESOURCE_SIZEALIGN);
			if (pci_reassign_resource(add_res->dev, idx,
						  add_size, align))
				dev_printk(KERN_DEBUG, &add_res->dev->dev,
					   "failed to add %llx res[%d]=%pR\n",
					   (unsigned long long)add_size,
					   idx, res);
		}
out:
		list_del(&add_res->list);
		kfree(add_res);
	}
}

static void assign_requested_resources_sorted(struct list_head *head,
				 struct list_head *fail_head)
{
	struct resource *res;
	struct pci_dev_resource *dev_res;
	int idx;

	list_for_each_entry(dev_res, head, list) {
		res = dev_res->res;
		idx = res - &dev_res->dev->resource[0];
		if (resource_size(res) &&
		    pci_assign_resource(dev_res->dev, idx)) {
			if (fail_head && !pci_is_root_bus(dev_res->dev->bus)) {
				if (!((idx == PCI_ROM_RESOURCE) &&
				      (!(res->flags & IORESOURCE_ROM_ENABLE))))
					add_to_list(fail_head,
						    dev_res->dev, res,
						    0 ,
						    0 );
			}
			reset_resource(res);
		}
	}
}

static void __assign_resources_sorted(struct list_head *head,
				 struct list_head *realloc_head,
				 struct list_head *fail_head)
{
	LIST_HEAD(save_head);
	LIST_HEAD(local_fail_head);
	struct pci_dev_resource *save_res;
	struct pci_dev_resource *dev_res;

	
	if (!realloc_head || list_empty(realloc_head))
		goto requested_and_reassign;

	
	list_for_each_entry(dev_res, head, list) {
		if (add_to_list(&save_head, dev_res->dev, dev_res->res, 0, 0)) {
			free_list(&save_head);
			goto requested_and_reassign;
		}
	}

	
	list_for_each_entry(dev_res, head, list)
		dev_res->res->end += get_res_add_size(realloc_head,
							dev_res->res);

	
	assign_requested_resources_sorted(head, &local_fail_head);

	
	if (list_empty(&local_fail_head)) {
		
		list_for_each_entry(dev_res, head, list)
			remove_from_list(realloc_head, dev_res->res);
		free_list(&save_head);
		free_list(head);
		return;
	}

	free_list(&local_fail_head);
	
	list_for_each_entry(dev_res, head, list)
		if (dev_res->res->parent)
			release_resource(dev_res->res);
	
	list_for_each_entry(save_res, &save_head, list) {
		struct resource *res = save_res->res;

		res->start = save_res->start;
		res->end = save_res->end;
		res->flags = save_res->flags;
	}
	free_list(&save_head);

requested_and_reassign:
	
	assign_requested_resources_sorted(head, fail_head);

	if (realloc_head)
		reassign_resources_sorted(realloc_head, head);
	free_list(head);
}

static void pdev_assign_resources_sorted(struct pci_dev *dev,
				 struct list_head *add_head,
				 struct list_head *fail_head)
{
	LIST_HEAD(head);

	__dev_sort_resources(dev, &head);
	__assign_resources_sorted(&head, add_head, fail_head);

}

static void pbus_assign_resources_sorted(const struct pci_bus *bus,
					 struct list_head *realloc_head,
					 struct list_head *fail_head)
{
	struct pci_dev *dev;
	LIST_HEAD(head);

	list_for_each_entry(dev, &bus->devices, bus_list)
		__dev_sort_resources(dev, &head);

	__assign_resources_sorted(&head, realloc_head, fail_head);
}

void pci_setup_cardbus(struct pci_bus *bus)
{
	struct pci_dev *bridge = bus->self;
	struct resource *res;
	struct pci_bus_region region;

	dev_info(&bridge->dev, "CardBus bridge to [bus %02x-%02x]\n",
		 bus->secondary, bus->subordinate);

	res = bus->resource[0];
	pcibios_resource_to_bus(bridge, &region, res);
	if (res->flags & IORESOURCE_IO) {
		dev_info(&bridge->dev, "  bridge window %pR\n", res);
		pci_write_config_dword(bridge, PCI_CB_IO_BASE_0,
					region.start);
		pci_write_config_dword(bridge, PCI_CB_IO_LIMIT_0,
					region.end);
	}

	res = bus->resource[1];
	pcibios_resource_to_bus(bridge, &region, res);
	if (res->flags & IORESOURCE_IO) {
		dev_info(&bridge->dev, "  bridge window %pR\n", res);
		pci_write_config_dword(bridge, PCI_CB_IO_BASE_1,
					region.start);
		pci_write_config_dword(bridge, PCI_CB_IO_LIMIT_1,
					region.end);
	}

	res = bus->resource[2];
	pcibios_resource_to_bus(bridge, &region, res);
	if (res->flags & IORESOURCE_MEM) {
		dev_info(&bridge->dev, "  bridge window %pR\n", res);
		pci_write_config_dword(bridge, PCI_CB_MEMORY_BASE_0,
					region.start);
		pci_write_config_dword(bridge, PCI_CB_MEMORY_LIMIT_0,
					region.end);
	}

	res = bus->resource[3];
	pcibios_resource_to_bus(bridge, &region, res);
	if (res->flags & IORESOURCE_MEM) {
		dev_info(&bridge->dev, "  bridge window %pR\n", res);
		pci_write_config_dword(bridge, PCI_CB_MEMORY_BASE_1,
					region.start);
		pci_write_config_dword(bridge, PCI_CB_MEMORY_LIMIT_1,
					region.end);
	}
}
EXPORT_SYMBOL(pci_setup_cardbus);

static void pci_setup_bridge_io(struct pci_bus *bus)
{
	struct pci_dev *bridge = bus->self;
	struct resource *res;
	struct pci_bus_region region;
	u32 l, io_upper16;

	
	res = bus->resource[0];
	pcibios_resource_to_bus(bridge, &region, res);
	if (res->flags & IORESOURCE_IO) {
		pci_read_config_dword(bridge, PCI_IO_BASE, &l);
		l &= 0xffff0000;
		l |= (region.start >> 8) & 0x00f0;
		l |= region.end & 0xf000;
		
		io_upper16 = (region.end & 0xffff0000) | (region.start >> 16);
		dev_info(&bridge->dev, "  bridge window %pR\n", res);
	} else {
		
		io_upper16 = 0;
		l = 0x00f0;
	}
	
	pci_write_config_dword(bridge, PCI_IO_BASE_UPPER16, 0x0000ffff);
	
	pci_write_config_dword(bridge, PCI_IO_BASE, l);
	
	pci_write_config_dword(bridge, PCI_IO_BASE_UPPER16, io_upper16);
}

static void pci_setup_bridge_mmio(struct pci_bus *bus)
{
	struct pci_dev *bridge = bus->self;
	struct resource *res;
	struct pci_bus_region region;
	u32 l;

	
	res = bus->resource[1];
	pcibios_resource_to_bus(bridge, &region, res);
	if (res->flags & IORESOURCE_MEM) {
		l = (region.start >> 16) & 0xfff0;
		l |= region.end & 0xfff00000;
		dev_info(&bridge->dev, "  bridge window %pR\n", res);
	} else {
		l = 0x0000fff0;
	}
	pci_write_config_dword(bridge, PCI_MEMORY_BASE, l);
}

static void pci_setup_bridge_mmio_pref(struct pci_bus *bus)
{
	struct pci_dev *bridge = bus->self;
	struct resource *res;
	struct pci_bus_region region;
	u32 l, bu, lu;

	pci_write_config_dword(bridge, PCI_PREF_LIMIT_UPPER32, 0);

	
	bu = lu = 0;
	res = bus->resource[2];
	pcibios_resource_to_bus(bridge, &region, res);
	if (res->flags & IORESOURCE_PREFETCH) {
		l = (region.start >> 16) & 0xfff0;
		l |= region.end & 0xfff00000;
		if (res->flags & IORESOURCE_MEM_64) {
			bu = upper_32_bits(region.start);
			lu = upper_32_bits(region.end);
		}
		dev_info(&bridge->dev, "  bridge window %pR\n", res);
	} else {
		l = 0x0000fff0;
	}
	pci_write_config_dword(bridge, PCI_PREF_MEMORY_BASE, l);

	
	pci_write_config_dword(bridge, PCI_PREF_BASE_UPPER32, bu);
	pci_write_config_dword(bridge, PCI_PREF_LIMIT_UPPER32, lu);
}

static void __pci_setup_bridge(struct pci_bus *bus, unsigned long type)
{
	struct pci_dev *bridge = bus->self;

	dev_info(&bridge->dev, "PCI bridge to [bus %02x-%02x]\n",
		 bus->secondary, bus->subordinate);

	if (type & IORESOURCE_IO)
		pci_setup_bridge_io(bus);

	if (type & IORESOURCE_MEM)
		pci_setup_bridge_mmio(bus);

	if (type & IORESOURCE_PREFETCH)
		pci_setup_bridge_mmio_pref(bus);

	pci_write_config_word(bridge, PCI_BRIDGE_CONTROL, bus->bridge_ctl);
}

void pci_setup_bridge(struct pci_bus *bus)
{
	unsigned long type = IORESOURCE_IO | IORESOURCE_MEM |
				  IORESOURCE_PREFETCH;

	__pci_setup_bridge(bus, type);
}

static void pci_bridge_check_ranges(struct pci_bus *bus)
{
	u16 io;
	u32 pmem;
	struct pci_dev *bridge = bus->self;
	struct resource *b_res;

	b_res = &bridge->resource[PCI_BRIDGE_RESOURCES];
	b_res[1].flags |= IORESOURCE_MEM;

	pci_read_config_word(bridge, PCI_IO_BASE, &io);
	if (!io) {
		pci_write_config_word(bridge, PCI_IO_BASE, 0xf0f0);
		pci_read_config_word(bridge, PCI_IO_BASE, &io);
 		pci_write_config_word(bridge, PCI_IO_BASE, 0x0);
 	}
 	if (io)
		b_res[0].flags |= IORESOURCE_IO;
	if (bridge->vendor == PCI_VENDOR_ID_DEC && bridge->device == 0x0001)
		return;
	pci_read_config_dword(bridge, PCI_PREF_MEMORY_BASE, &pmem);
	if (!pmem) {
		pci_write_config_dword(bridge, PCI_PREF_MEMORY_BASE,
					       0xfff0fff0);
		pci_read_config_dword(bridge, PCI_PREF_MEMORY_BASE, &pmem);
		pci_write_config_dword(bridge, PCI_PREF_MEMORY_BASE, 0x0);
	}
	if (pmem) {
		b_res[2].flags |= IORESOURCE_MEM | IORESOURCE_PREFETCH;
		if ((pmem & PCI_PREF_RANGE_TYPE_MASK) ==
		    PCI_PREF_RANGE_TYPE_64) {
			b_res[2].flags |= IORESOURCE_MEM_64;
			b_res[2].flags |= PCI_PREF_RANGE_TYPE_64;
		}
	}

	
	if (b_res[2].flags & IORESOURCE_MEM_64) {
		u32 mem_base_hi, tmp;
		pci_read_config_dword(bridge, PCI_PREF_BASE_UPPER32,
					 &mem_base_hi);
		pci_write_config_dword(bridge, PCI_PREF_BASE_UPPER32,
					       0xffffffff);
		pci_read_config_dword(bridge, PCI_PREF_BASE_UPPER32, &tmp);
		if (!tmp)
			b_res[2].flags &= ~IORESOURCE_MEM_64;
		pci_write_config_dword(bridge, PCI_PREF_BASE_UPPER32,
				       mem_base_hi);
	}
}

static struct resource *find_free_bus_resource(struct pci_bus *bus, unsigned long type)
{
	int i;
	struct resource *r;
	unsigned long type_mask = IORESOURCE_IO | IORESOURCE_MEM |
				  IORESOURCE_PREFETCH;

	pci_bus_for_each_resource(bus, r, i) {
		if (r == &ioport_resource || r == &iomem_resource)
			continue;
		if (r && (r->flags & type_mask) == type && !r->parent)
			return r;
	}
	return NULL;
}

static resource_size_t calculate_iosize(resource_size_t size,
		resource_size_t min_size,
		resource_size_t size1,
		resource_size_t old_size,
		resource_size_t align)
{
	if (size < min_size)
		size = min_size;
	if (old_size == 1 )
		old_size = 0;
#if defined(CONFIG_ISA) || defined(CONFIG_EISA)
	size = (size & 0xff) + ((size & ~0xffUL) << 2);
#endif
	size = ALIGN(size + size1, align);
	if (size < old_size)
		size = old_size;
	return size;
}

static resource_size_t calculate_memsize(resource_size_t size,
		resource_size_t min_size,
		resource_size_t size1,
		resource_size_t old_size,
		resource_size_t align)
{
	if (size < min_size)
		size = min_size;
	if (old_size == 1 )
		old_size = 0;
	if (size < old_size)
		size = old_size;
	size = ALIGN(size + size1, align);
	return size;
}

static void pbus_size_io(struct pci_bus *bus, resource_size_t min_size,
		resource_size_t add_size, struct list_head *realloc_head)
{
	struct pci_dev *dev;
	struct resource *b_res = find_free_bus_resource(bus, IORESOURCE_IO);
	unsigned long size = 0, size0 = 0, size1 = 0;
	resource_size_t children_add_size = 0;

	if (!b_res)
 		return;

	list_for_each_entry(dev, &bus->devices, bus_list) {
		int i;

		for (i = 0; i < PCI_NUM_RESOURCES; i++) {
			struct resource *r = &dev->resource[i];
			unsigned long r_size;

			if (r->parent || !(r->flags & IORESOURCE_IO))
				continue;
			r_size = resource_size(r);

			if (r_size < 0x400)
				
				size += r_size;
			else
				size1 += r_size;

			if (realloc_head)
				children_add_size += get_res_add_size(realloc_head, r);
		}
	}
	size0 = calculate_iosize(size, min_size, size1,
			resource_size(b_res), 4096);
	if (children_add_size > add_size)
		add_size = children_add_size;
	size1 = (!realloc_head || (realloc_head && !add_size)) ? size0 :
		calculate_iosize(size, min_size, add_size + size1,
			resource_size(b_res), 4096);
	if (!size0 && !size1) {
		if (b_res->start || b_res->end)
			dev_info(&bus->self->dev, "disabling bridge window "
				 "%pR to [bus %02x-%02x] (unused)\n", b_res,
				 bus->secondary, bus->subordinate);
		b_res->flags = 0;
		return;
	}
	
	b_res->start = 4096;
	b_res->end = b_res->start + size0 - 1;
	b_res->flags |= IORESOURCE_STARTALIGN;
	if (size1 > size0 && realloc_head) {
		add_to_list(realloc_head, bus->self, b_res, size1-size0, 4096);
		dev_printk(KERN_DEBUG, &bus->self->dev, "bridge window "
				 "%pR to [bus %02x-%02x] add_size %lx\n", b_res,
				 bus->secondary, bus->subordinate, size1-size0);
	}
}

static int pbus_size_mem(struct pci_bus *bus, unsigned long mask,
			 unsigned long type, resource_size_t min_size,
			resource_size_t add_size,
			struct list_head *realloc_head)
{
	struct pci_dev *dev;
	resource_size_t min_align, align, size, size0, size1;
	resource_size_t aligns[12];	
	int order, max_order;
	struct resource *b_res = find_free_bus_resource(bus, type);
	unsigned int mem64_mask = 0;
	resource_size_t children_add_size = 0;

	if (!b_res)
		return 0;

	memset(aligns, 0, sizeof(aligns));
	max_order = 0;
	size = 0;

	mem64_mask = b_res->flags & IORESOURCE_MEM_64;
	b_res->flags &= ~IORESOURCE_MEM_64;

	list_for_each_entry(dev, &bus->devices, bus_list) {
		int i;

		for (i = 0; i < PCI_NUM_RESOURCES; i++) {
			struct resource *r = &dev->resource[i];
			resource_size_t r_size;

			if (r->parent || (r->flags & mask) != type)
				continue;
			r_size = resource_size(r);
#ifdef CONFIG_PCI_IOV
			
			if (realloc_head && i >= PCI_IOV_RESOURCES &&
					i <= PCI_IOV_RESOURCE_END) {
				r->end = r->start - 1;
				add_to_list(realloc_head, dev, r, r_size, 0);
				children_add_size += r_size;
				continue;
			}
#endif
			
			align = pci_resource_alignment(dev, r);
			order = __ffs(align) - 20;
			if (order > 11) {
				dev_warn(&dev->dev, "disabling BAR %d: %pR "
					 "(bad alignment %#llx)\n", i, r,
					 (unsigned long long) align);
				r->flags = 0;
				continue;
			}
			size += r_size;
			if (order < 0)
				order = 0;
			if (r_size == align)
				aligns[order] += align;
			if (order > max_order)
				max_order = order;
			mem64_mask &= r->flags & IORESOURCE_MEM_64;

			if (realloc_head)
				children_add_size += get_res_add_size(realloc_head, r);
		}
	}
	align = 0;
	min_align = 0;
	for (order = 0; order <= max_order; order++) {
		resource_size_t align1 = 1;

		align1 <<= (order + 20);

		if (!align)
			min_align = align1;
		else if (ALIGN(align + min_align, min_align) < align1)
			min_align = align1 >> 1;
		align += aligns[order];
	}
	size0 = calculate_memsize(size, min_size, 0, resource_size(b_res), min_align);
	if (children_add_size > add_size)
		add_size = children_add_size;
	size1 = (!realloc_head || (realloc_head && !add_size)) ? size0 :
		calculate_memsize(size, min_size, add_size,
				resource_size(b_res), min_align);
	if (!size0 && !size1) {
		if (b_res->start || b_res->end)
			dev_info(&bus->self->dev, "disabling bridge window "
				 "%pR to [bus %02x-%02x] (unused)\n", b_res,
				 bus->secondary, bus->subordinate);
		b_res->flags = 0;
		return 1;
	}
	b_res->start = min_align;
	b_res->end = size0 + min_align - 1;
	b_res->flags |= IORESOURCE_STARTALIGN | mem64_mask;
	if (size1 > size0 && realloc_head) {
		add_to_list(realloc_head, bus->self, b_res, size1-size0, min_align);
		dev_printk(KERN_DEBUG, &bus->self->dev, "bridge window "
				 "%pR to [bus %02x-%02x] add_size %llx\n", b_res,
				 bus->secondary, bus->subordinate, (unsigned long long)size1-size0);
	}
	return 1;
}

unsigned long pci_cardbus_resource_alignment(struct resource *res)
{
	if (res->flags & IORESOURCE_IO)
		return pci_cardbus_io_size;
	if (res->flags & IORESOURCE_MEM)
		return pci_cardbus_mem_size;
	return 0;
}

static void pci_bus_size_cardbus(struct pci_bus *bus,
			struct list_head *realloc_head)
{
	struct pci_dev *bridge = bus->self;
	struct resource *b_res = &bridge->resource[PCI_BRIDGE_RESOURCES];
	resource_size_t b_res_3_size = pci_cardbus_mem_size * 2;
	u16 ctrl;

	if (b_res[0].parent)
		goto handle_b_res_1;
	b_res[0].start = pci_cardbus_io_size;
	b_res[0].end = b_res[0].start + pci_cardbus_io_size - 1;
	b_res[0].flags |= IORESOURCE_IO | IORESOURCE_STARTALIGN;
	if (realloc_head) {
		b_res[0].end -= pci_cardbus_io_size;
		add_to_list(realloc_head, bridge, b_res, pci_cardbus_io_size,
				pci_cardbus_io_size);
	}

handle_b_res_1:
	if (b_res[1].parent)
		goto handle_b_res_2;
	b_res[1].start = pci_cardbus_io_size;
	b_res[1].end = b_res[1].start + pci_cardbus_io_size - 1;
	b_res[1].flags |= IORESOURCE_IO | IORESOURCE_STARTALIGN;
	if (realloc_head) {
		b_res[1].end -= pci_cardbus_io_size;
		add_to_list(realloc_head, bridge, b_res+1, pci_cardbus_io_size,
				 pci_cardbus_io_size);
	}

handle_b_res_2:
	
	pci_read_config_word(bridge, PCI_CB_BRIDGE_CONTROL, &ctrl);
	if (ctrl & PCI_CB_BRIDGE_CTL_PREFETCH_MEM1) {
		ctrl &= ~PCI_CB_BRIDGE_CTL_PREFETCH_MEM1;
		pci_write_config_word(bridge, PCI_CB_BRIDGE_CONTROL, ctrl);
		pci_read_config_word(bridge, PCI_CB_BRIDGE_CONTROL, &ctrl);
	}

	pci_read_config_word(bridge, PCI_CB_BRIDGE_CONTROL, &ctrl);
	if (!(ctrl & PCI_CB_BRIDGE_CTL_PREFETCH_MEM0)) {
		ctrl |= PCI_CB_BRIDGE_CTL_PREFETCH_MEM0;
		pci_write_config_word(bridge, PCI_CB_BRIDGE_CONTROL, ctrl);
		pci_read_config_word(bridge, PCI_CB_BRIDGE_CONTROL, &ctrl);
	}

	if (b_res[2].parent)
		goto handle_b_res_3;
	if (ctrl & PCI_CB_BRIDGE_CTL_PREFETCH_MEM0) {
		b_res[2].start = pci_cardbus_mem_size;
		b_res[2].end = b_res[2].start + pci_cardbus_mem_size - 1;
		b_res[2].flags |= IORESOURCE_MEM | IORESOURCE_PREFETCH |
				  IORESOURCE_STARTALIGN;
		if (realloc_head) {
			b_res[2].end -= pci_cardbus_mem_size;
			add_to_list(realloc_head, bridge, b_res+2,
				 pci_cardbus_mem_size, pci_cardbus_mem_size);
		}

		
		b_res_3_size = pci_cardbus_mem_size;
	}

handle_b_res_3:
	if (b_res[3].parent)
		goto handle_done;
	b_res[3].start = pci_cardbus_mem_size;
	b_res[3].end = b_res[3].start + b_res_3_size - 1;
	b_res[3].flags |= IORESOURCE_MEM | IORESOURCE_STARTALIGN;
	if (realloc_head) {
		b_res[3].end -= b_res_3_size;
		add_to_list(realloc_head, bridge, b_res+3, b_res_3_size,
				 pci_cardbus_mem_size);
	}

handle_done:
	;
}

void __ref __pci_bus_size_bridges(struct pci_bus *bus,
			struct list_head *realloc_head)
{
	struct pci_dev *dev;
	unsigned long mask, prefmask;
	resource_size_t additional_mem_size = 0, additional_io_size = 0;

	list_for_each_entry(dev, &bus->devices, bus_list) {
		struct pci_bus *b = dev->subordinate;
		if (!b)
			continue;

		switch (dev->class >> 8) {
		case PCI_CLASS_BRIDGE_CARDBUS:
			pci_bus_size_cardbus(b, realloc_head);
			break;

		case PCI_CLASS_BRIDGE_PCI:
		default:
			__pci_bus_size_bridges(b, realloc_head);
			break;
		}
	}

	
	if (!bus->self)
		return;

	switch (bus->self->class >> 8) {
	case PCI_CLASS_BRIDGE_CARDBUS:
		
		break;

	case PCI_CLASS_BRIDGE_PCI:
		pci_bridge_check_ranges(bus);
		if (bus->self->is_hotplug_bridge) {
			additional_io_size  = pci_hotplug_io_size;
			additional_mem_size = pci_hotplug_mem_size;
		}
	default:
		pbus_size_io(bus, realloc_head ? 0 : additional_io_size,
			     additional_io_size, realloc_head);
		mask = IORESOURCE_MEM;
		prefmask = IORESOURCE_MEM | IORESOURCE_PREFETCH;
		if (pbus_size_mem(bus, prefmask, prefmask,
				  realloc_head ? 0 : additional_mem_size,
				  additional_mem_size, realloc_head))
			mask = prefmask; 
		else
			additional_mem_size += additional_mem_size;
		pbus_size_mem(bus, mask, IORESOURCE_MEM,
				realloc_head ? 0 : additional_mem_size,
				additional_mem_size, realloc_head);
		break;
	}
}

void __ref pci_bus_size_bridges(struct pci_bus *bus)
{
	__pci_bus_size_bridges(bus, NULL);
}
EXPORT_SYMBOL(pci_bus_size_bridges);

static void __ref __pci_bus_assign_resources(const struct pci_bus *bus,
					 struct list_head *realloc_head,
					 struct list_head *fail_head)
{
	struct pci_bus *b;
	struct pci_dev *dev;

	pbus_assign_resources_sorted(bus, realloc_head, fail_head);

	list_for_each_entry(dev, &bus->devices, bus_list) {
		b = dev->subordinate;
		if (!b)
			continue;

		__pci_bus_assign_resources(b, realloc_head, fail_head);

		switch (dev->class >> 8) {
		case PCI_CLASS_BRIDGE_PCI:
			if (!pci_is_enabled(dev))
				pci_setup_bridge(b);
			break;

		case PCI_CLASS_BRIDGE_CARDBUS:
			pci_setup_cardbus(b);
			break;

		default:
			dev_info(&dev->dev, "not setting up bridge for bus "
				 "%04x:%02x\n", pci_domain_nr(b), b->number);
			break;
		}
	}
}

void __ref pci_bus_assign_resources(const struct pci_bus *bus)
{
	__pci_bus_assign_resources(bus, NULL, NULL);
}
EXPORT_SYMBOL(pci_bus_assign_resources);

static void __ref __pci_bridge_assign_resources(const struct pci_dev *bridge,
					 struct list_head *add_head,
					 struct list_head *fail_head)
{
	struct pci_bus *b;

	pdev_assign_resources_sorted((struct pci_dev *)bridge,
					 add_head, fail_head);

	b = bridge->subordinate;
	if (!b)
		return;

	__pci_bus_assign_resources(b, add_head, fail_head);

	switch (bridge->class >> 8) {
	case PCI_CLASS_BRIDGE_PCI:
		pci_setup_bridge(b);
		break;

	case PCI_CLASS_BRIDGE_CARDBUS:
		pci_setup_cardbus(b);
		break;

	default:
		dev_info(&bridge->dev, "not setting up bridge for bus "
			 "%04x:%02x\n", pci_domain_nr(b), b->number);
		break;
	}
}
static void pci_bridge_release_resources(struct pci_bus *bus,
					  unsigned long type)
{
	int idx;
	bool changed = false;
	struct pci_dev *dev;
	struct resource *r;
	unsigned long type_mask = IORESOURCE_IO | IORESOURCE_MEM |
				  IORESOURCE_PREFETCH;

	dev = bus->self;
	for (idx = PCI_BRIDGE_RESOURCES; idx <= PCI_BRIDGE_RESOURCE_END;
	     idx++) {
		r = &dev->resource[idx];
		if ((r->flags & type_mask) != type)
			continue;
		if (!r->parent)
			continue;
		release_child_resources(r);
		if (!release_resource(r)) {
			dev_printk(KERN_DEBUG, &dev->dev,
				 "resource %d %pR released\n", idx, r);
			
			r->end = resource_size(r) - 1;
			r->start = 0;
			r->flags = 0;
			changed = true;
		}
	}

	if (changed) {
		
		if (type & IORESOURCE_PREFETCH)
			type = IORESOURCE_PREFETCH;
		__pci_setup_bridge(bus, type);
	}
}

enum release_type {
	leaf_only,
	whole_subtree,
};
static void __ref pci_bus_release_bridge_resources(struct pci_bus *bus,
						   unsigned long type,
						   enum release_type rel_type)
{
	struct pci_dev *dev;
	bool is_leaf_bridge = true;

	list_for_each_entry(dev, &bus->devices, bus_list) {
		struct pci_bus *b = dev->subordinate;
		if (!b)
			continue;

		is_leaf_bridge = false;

		if ((dev->class >> 8) != PCI_CLASS_BRIDGE_PCI)
			continue;

		if (rel_type == whole_subtree)
			pci_bus_release_bridge_resources(b, type,
						 whole_subtree);
	}

	if (pci_is_root_bus(bus))
		return;

	if ((bus->self->class >> 8) != PCI_CLASS_BRIDGE_PCI)
		return;

	if ((rel_type == whole_subtree) || is_leaf_bridge)
		pci_bridge_release_resources(bus, type);
}

static void pci_bus_dump_res(struct pci_bus *bus)
{
	struct resource *res;
	int i;

	pci_bus_for_each_resource(bus, res, i) {
		if (!res || !res->end || !res->flags)
                        continue;

		dev_printk(KERN_DEBUG, &bus->dev, "resource %d %pR\n", i, res);
        }
}

static void pci_bus_dump_resources(struct pci_bus *bus)
{
	struct pci_bus *b;
	struct pci_dev *dev;


	pci_bus_dump_res(bus);

	list_for_each_entry(dev, &bus->devices, bus_list) {
		b = dev->subordinate;
		if (!b)
			continue;

		pci_bus_dump_resources(b);
	}
}

static int __init pci_bus_get_depth(struct pci_bus *bus)
{
	int depth = 0;
	struct pci_dev *dev;

	list_for_each_entry(dev, &bus->devices, bus_list) {
		int ret;
		struct pci_bus *b = dev->subordinate;
		if (!b)
			continue;

		ret = pci_bus_get_depth(b);
		if (ret + 1 > depth)
			depth = ret + 1;
	}

	return depth;
}
static int __init pci_get_max_depth(void)
{
	int depth = 0;
	struct pci_bus *bus;

	list_for_each_entry(bus, &pci_root_buses, node) {
		int ret;

		ret = pci_bus_get_depth(bus);
		if (ret > depth)
			depth = ret;
	}

	return depth;
}

enum enable_type {
	undefined = -1,
	user_disabled,
	auto_disabled,
	user_enabled,
	auto_enabled,
};

static enum enable_type pci_realloc_enable __initdata = undefined;
void __init pci_realloc_get_opt(char *str)
{
	if (!strncmp(str, "off", 3))
		pci_realloc_enable = user_disabled;
	else if (!strncmp(str, "on", 2))
		pci_realloc_enable = user_enabled;
}
static bool __init pci_realloc_enabled(void)
{
	return pci_realloc_enable >= user_enabled;
}

static void __init pci_realloc_detect(void)
{
#if defined(CONFIG_PCI_IOV) && defined(CONFIG_PCI_REALLOC_ENABLE_AUTO)
	struct pci_dev *dev = NULL;

	if (pci_realloc_enable != undefined)
		return;

	for_each_pci_dev(dev) {
		int i;

		for (i = PCI_IOV_RESOURCES; i <= PCI_IOV_RESOURCE_END; i++) {
			struct resource *r = &dev->resource[i];

			
			if (r->flags && !r->start) {
				pci_realloc_enable = auto_enabled;

				return;
			}
		}
	}
#endif
}

void __init
pci_assign_unassigned_resources(void)
{
	struct pci_bus *bus;
	LIST_HEAD(realloc_head); 
	struct list_head *add_list = NULL;
	int tried_times = 0;
	enum release_type rel_type = leaf_only;
	LIST_HEAD(fail_head);
	struct pci_dev_resource *fail_res;
	unsigned long type_mask = IORESOURCE_IO | IORESOURCE_MEM |
				  IORESOURCE_PREFETCH;
	int pci_try_num = 1;

	
	pci_realloc_detect();
	if (pci_realloc_enabled()) {
		int max_depth = pci_get_max_depth();

		pci_try_num = max_depth + 1;
		printk(KERN_DEBUG "PCI: max bus depth: %d pci_try_num: %d\n",
			 max_depth, pci_try_num);
	}

again:
	if (tried_times + 1 == pci_try_num)
		add_list = &realloc_head;
	list_for_each_entry(bus, &pci_root_buses, node)
		__pci_bus_size_bridges(bus, add_list);

	
	list_for_each_entry(bus, &pci_root_buses, node)
		__pci_bus_assign_resources(bus, add_list, &fail_head);
	if (add_list)
		BUG_ON(!list_empty(add_list));
	tried_times++;

	
	if (list_empty(&fail_head))
		goto enable_and_dump;

	if (tried_times >= pci_try_num) {
		if (pci_realloc_enable == undefined)
			printk(KERN_INFO "Some PCI device resources are unassigned, try booting with pci=realloc\n");
		else if (pci_realloc_enable == auto_enabled)
			printk(KERN_INFO "Automatically enabled pci realloc, if you have problem, try booting with pci=realloc=off\n");

		free_list(&fail_head);
		goto enable_and_dump;
	}

	printk(KERN_DEBUG "PCI: No. %d try to assign unassigned res\n",
			 tried_times + 1);

	
	if ((tried_times + 1) > 2)
		rel_type = whole_subtree;

	list_for_each_entry(fail_res, &fail_head, list) {
		bus = fail_res->dev->bus;
		pci_bus_release_bridge_resources(bus,
						 fail_res->flags & type_mask,
						 rel_type);
	}
	
	list_for_each_entry(fail_res, &fail_head, list) {
		struct resource *res = fail_res->res;

		res->start = fail_res->start;
		res->end = fail_res->end;
		res->flags = fail_res->flags;
		if (fail_res->dev->subordinate)
			res->flags = 0;
	}
	free_list(&fail_head);

	goto again;

enable_and_dump:
	
	list_for_each_entry(bus, &pci_root_buses, node)
		pci_enable_bridges(bus);

	
	list_for_each_entry(bus, &pci_root_buses, node)
		pci_bus_dump_resources(bus);
}

void pci_assign_unassigned_bridge_resources(struct pci_dev *bridge)
{
	struct pci_bus *parent = bridge->subordinate;
	LIST_HEAD(add_list); 
	int tried_times = 0;
	LIST_HEAD(fail_head);
	struct pci_dev_resource *fail_res;
	int retval;
	unsigned long type_mask = IORESOURCE_IO | IORESOURCE_MEM |
				  IORESOURCE_PREFETCH;

again:
	__pci_bus_size_bridges(parent, &add_list);
	__pci_bridge_assign_resources(bridge, &add_list, &fail_head);
	BUG_ON(!list_empty(&add_list));
	tried_times++;

	if (list_empty(&fail_head))
		goto enable_all;

	if (tried_times >= 2) {
		
		free_list(&fail_head);
		goto enable_all;
	}

	printk(KERN_DEBUG "PCI: No. %d try to assign unassigned res\n",
			 tried_times + 1);

	list_for_each_entry(fail_res, &fail_head, list) {
		struct pci_bus *bus = fail_res->dev->bus;
		unsigned long flags = fail_res->flags;

		pci_bus_release_bridge_resources(bus, flags & type_mask,
						 whole_subtree);
	}
	
	list_for_each_entry(fail_res, &fail_head, list) {
		struct resource *res = fail_res->res;

		res->start = fail_res->start;
		res->end = fail_res->end;
		res->flags = fail_res->flags;
		if (fail_res->dev->subordinate)
			res->flags = 0;
	}
	free_list(&fail_head);

	goto again;

enable_all:
	retval = pci_reenable_device(bridge);
	pci_set_master(bridge);
	pci_enable_bridges(parent);
}
EXPORT_SYMBOL_GPL(pci_assign_unassigned_bridge_resources);

#ifdef CONFIG_HOTPLUG
unsigned int __ref pci_rescan_bus(struct pci_bus *bus)
{
	unsigned int max;
	struct pci_dev *dev;
	LIST_HEAD(add_list); 

	max = pci_scan_child_bus(bus);

	down_read(&pci_bus_sem);
	list_for_each_entry(dev, &bus->devices, bus_list)
		if (dev->hdr_type == PCI_HEADER_TYPE_BRIDGE ||
		    dev->hdr_type == PCI_HEADER_TYPE_CARDBUS)
			if (dev->subordinate)
				__pci_bus_size_bridges(dev->subordinate,
							 &add_list);
	up_read(&pci_bus_sem);
	__pci_bus_assign_resources(bus, &add_list, NULL);
	BUG_ON(!list_empty(&add_list));

	pci_enable_bridges(bus);
	pci_bus_add_devices(bus);

	return max;
}
EXPORT_SYMBOL_GPL(pci_rescan_bus);
#endif
