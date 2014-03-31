
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/cpumask.h>
#include <linux/pci-aspm.h>
#include "pci.h"

#define CARDBUS_LATENCY_TIMER	176	
#define CARDBUS_RESERVE_BUSNR	3

static LIST_HEAD(pci_host_bridges);

LIST_HEAD(pci_root_buses);
EXPORT_SYMBOL(pci_root_buses);


static int find_anything(struct device *dev, void *data)
{
	return 1;
}

int no_pci_devices(void)
{
	struct device *dev;
	int no_devices;

	dev = bus_find_device(&pci_bus_type, NULL, NULL, find_anything);
	no_devices = (dev == NULL);
	put_device(dev);
	return no_devices;
}
EXPORT_SYMBOL(no_pci_devices);

static struct pci_host_bridge *pci_host_bridge(struct pci_dev *dev)
{
	struct pci_bus *bus;
	struct pci_host_bridge *bridge;

	bus = dev->bus;
	while (bus->parent)
		bus = bus->parent;

	list_for_each_entry(bridge, &pci_host_bridges, list) {
		if (bridge->bus == bus)
			return bridge;
	}

	return NULL;
}

static bool resource_contains(struct resource *res1, struct resource *res2)
{
	return res1->start <= res2->start && res1->end >= res2->end;
}

void pcibios_resource_to_bus(struct pci_dev *dev, struct pci_bus_region *region,
			     struct resource *res)
{
	struct pci_host_bridge *bridge = pci_host_bridge(dev);
	struct pci_host_bridge_window *window;
	resource_size_t offset = 0;

	list_for_each_entry(window, &bridge->windows, list) {
		if (resource_type(res) != resource_type(window->res))
			continue;

		if (resource_contains(window->res, res)) {
			offset = window->offset;
			break;
		}
	}

	region->start = res->start - offset;
	region->end = res->end - offset;
}
EXPORT_SYMBOL(pcibios_resource_to_bus);

static bool region_contains(struct pci_bus_region *region1,
			    struct pci_bus_region *region2)
{
	return region1->start <= region2->start && region1->end >= region2->end;
}

void pcibios_bus_to_resource(struct pci_dev *dev, struct resource *res,
			     struct pci_bus_region *region)
{
	struct pci_host_bridge *bridge = pci_host_bridge(dev);
	struct pci_host_bridge_window *window;
	struct pci_bus_region bus_region;
	resource_size_t offset = 0;

	list_for_each_entry(window, &bridge->windows, list) {
		if (resource_type(res) != resource_type(window->res))
			continue;

		bus_region.start = window->res->start - window->offset;
		bus_region.end = window->res->end - window->offset;

		if (region_contains(&bus_region, region)) {
			offset = window->offset;
			break;
		}
	}

	res->start = region->start + offset;
	res->end = region->end + offset;
}
EXPORT_SYMBOL(pcibios_bus_to_resource);

static void release_pcibus_dev(struct device *dev)
{
	struct pci_bus *pci_bus = to_pci_bus(dev);

	if (pci_bus->bridge)
		put_device(pci_bus->bridge);
	pci_bus_remove_resources(pci_bus);
	pci_release_bus_of_node(pci_bus);
	kfree(pci_bus);
}

static struct class pcibus_class = {
	.name		= "pci_bus",
	.dev_release	= &release_pcibus_dev,
	.dev_attrs	= pcibus_dev_attrs,
};

static int __init pcibus_class_init(void)
{
	return class_register(&pcibus_class);
}
postcore_initcall(pcibus_class_init);

static u64 pci_size(u64 base, u64 maxbase, u64 mask)
{
	u64 size = mask & maxbase;	
	if (!size)
		return 0;

	size = (size & ~(size-1)) - 1;

	if (base == maxbase && ((base | size) & mask) != mask)
		return 0;

	return size;
}

static inline unsigned long decode_bar(struct pci_dev *dev, u32 bar)
{
	u32 mem_type;
	unsigned long flags;

	if ((bar & PCI_BASE_ADDRESS_SPACE) == PCI_BASE_ADDRESS_SPACE_IO) {
		flags = bar & ~PCI_BASE_ADDRESS_IO_MASK;
		flags |= IORESOURCE_IO;
		return flags;
	}

	flags = bar & ~PCI_BASE_ADDRESS_MEM_MASK;
	flags |= IORESOURCE_MEM;
	if (flags & PCI_BASE_ADDRESS_MEM_PREFETCH)
		flags |= IORESOURCE_PREFETCH;

	mem_type = bar & PCI_BASE_ADDRESS_MEM_TYPE_MASK;
	switch (mem_type) {
	case PCI_BASE_ADDRESS_MEM_TYPE_32:
		break;
	case PCI_BASE_ADDRESS_MEM_TYPE_1M:
		dev_info(&dev->dev, "1M mem BAR treated as 32-bit BAR\n");
		break;
	case PCI_BASE_ADDRESS_MEM_TYPE_64:
		flags |= IORESOURCE_MEM_64;
		break;
	default:
		dev_warn(&dev->dev,
			 "mem unknown type %x treated as 32-bit BAR\n",
			 mem_type);
		break;
	}
	return flags;
}

int __pci_read_base(struct pci_dev *dev, enum pci_bar_type type,
			struct resource *res, unsigned int pos)
{
	u32 l, sz, mask;
	u16 orig_cmd;
	struct pci_bus_region region;

	mask = type ? PCI_ROM_ADDRESS_MASK : ~0;

	if (!dev->mmio_always_on) {
		pci_read_config_word(dev, PCI_COMMAND, &orig_cmd);
		pci_write_config_word(dev, PCI_COMMAND,
			orig_cmd & ~(PCI_COMMAND_MEMORY | PCI_COMMAND_IO));
	}

	res->name = pci_name(dev);

	pci_read_config_dword(dev, pos, &l);
	pci_write_config_dword(dev, pos, l | mask);
	pci_read_config_dword(dev, pos, &sz);
	pci_write_config_dword(dev, pos, l);

	if (!dev->mmio_always_on)
		pci_write_config_word(dev, PCI_COMMAND, orig_cmd);

	if (!sz || sz == 0xffffffff)
		goto fail;

	if (l == 0xffffffff)
		l = 0;

	if (type == pci_bar_unknown) {
		res->flags = decode_bar(dev, l);
		res->flags |= IORESOURCE_SIZEALIGN;
		if (res->flags & IORESOURCE_IO) {
			l &= PCI_BASE_ADDRESS_IO_MASK;
			mask = PCI_BASE_ADDRESS_IO_MASK & (u32) IO_SPACE_LIMIT;
		} else {
			l &= PCI_BASE_ADDRESS_MEM_MASK;
			mask = (u32)PCI_BASE_ADDRESS_MEM_MASK;
		}
	} else {
		res->flags |= (l & IORESOURCE_ROM_ENABLE);
		l &= PCI_ROM_ADDRESS_MASK;
		mask = (u32)PCI_ROM_ADDRESS_MASK;
	}

	if (res->flags & IORESOURCE_MEM_64) {
		u64 l64 = l;
		u64 sz64 = sz;
		u64 mask64 = mask | (u64)~0 << 32;

		pci_read_config_dword(dev, pos + 4, &l);
		pci_write_config_dword(dev, pos + 4, ~0);
		pci_read_config_dword(dev, pos + 4, &sz);
		pci_write_config_dword(dev, pos + 4, l);

		l64 |= ((u64)l << 32);
		sz64 |= ((u64)sz << 32);

		sz64 = pci_size(l64, sz64, mask64);

		if (!sz64)
			goto fail;

		if ((sizeof(resource_size_t) < 8) && (sz64 > 0x100000000ULL)) {
			dev_err(&dev->dev, "reg %x: can't handle 64-bit BAR\n",
				pos);
			goto fail;
		}

		if ((sizeof(resource_size_t) < 8) && l) {
			
			pci_write_config_dword(dev, pos, 0);
			pci_write_config_dword(dev, pos + 4, 0);
			region.start = 0;
			region.end = sz64;
			pcibios_bus_to_resource(dev, res, &region);
		} else {
			region.start = l64;
			region.end = l64 + sz64;
			pcibios_bus_to_resource(dev, res, &region);
			dev_printk(KERN_DEBUG, &dev->dev, "reg %x: %pR\n",
				   pos, res);
		}
	} else {
		sz = pci_size(l, sz, mask);

		if (!sz)
			goto fail;

		region.start = l;
		region.end = l + sz;
		pcibios_bus_to_resource(dev, res, &region);

		dev_printk(KERN_DEBUG, &dev->dev, "reg %x: %pR\n", pos, res);
	}

 out:
	return (res->flags & IORESOURCE_MEM_64) ? 1 : 0;
 fail:
	res->flags = 0;
	goto out;
}

static void pci_read_bases(struct pci_dev *dev, unsigned int howmany, int rom)
{
	unsigned int pos, reg;

	for (pos = 0; pos < howmany; pos++) {
		struct resource *res = &dev->resource[pos];
		reg = PCI_BASE_ADDRESS_0 + (pos << 2);
		pos += __pci_read_base(dev, pci_bar_unknown, res, reg);
	}

	if (rom) {
		struct resource *res = &dev->resource[PCI_ROM_RESOURCE];
		dev->rom_base_reg = rom;
		res->flags = IORESOURCE_MEM | IORESOURCE_PREFETCH |
				IORESOURCE_READONLY | IORESOURCE_CACHEABLE |
				IORESOURCE_SIZEALIGN;
		__pci_read_base(dev, pci_bar_mem32, res, rom);
	}
}

static void __devinit pci_read_bridge_io(struct pci_bus *child)
{
	struct pci_dev *dev = child->self;
	u8 io_base_lo, io_limit_lo;
	unsigned long base, limit;
	struct pci_bus_region region;
	struct resource *res, res2;

	res = child->resource[0];
	pci_read_config_byte(dev, PCI_IO_BASE, &io_base_lo);
	pci_read_config_byte(dev, PCI_IO_LIMIT, &io_limit_lo);
	base = (io_base_lo & PCI_IO_RANGE_MASK) << 8;
	limit = (io_limit_lo & PCI_IO_RANGE_MASK) << 8;

	if ((io_base_lo & PCI_IO_RANGE_TYPE_MASK) == PCI_IO_RANGE_TYPE_32) {
		u16 io_base_hi, io_limit_hi;
		pci_read_config_word(dev, PCI_IO_BASE_UPPER16, &io_base_hi);
		pci_read_config_word(dev, PCI_IO_LIMIT_UPPER16, &io_limit_hi);
		base |= (io_base_hi << 16);
		limit |= (io_limit_hi << 16);
	}

	if (base && base <= limit) {
		res->flags = (io_base_lo & PCI_IO_RANGE_TYPE_MASK) | IORESOURCE_IO;
		res2.flags = res->flags;
		region.start = base;
		region.end = limit + 0xfff;
		pcibios_bus_to_resource(dev, &res2, &region);
		if (!res->start)
			res->start = res2.start;
		if (!res->end)
			res->end = res2.end;
		dev_printk(KERN_DEBUG, &dev->dev, "  bridge window %pR\n", res);
	}
}

static void __devinit pci_read_bridge_mmio(struct pci_bus *child)
{
	struct pci_dev *dev = child->self;
	u16 mem_base_lo, mem_limit_lo;
	unsigned long base, limit;
	struct pci_bus_region region;
	struct resource *res;

	res = child->resource[1];
	pci_read_config_word(dev, PCI_MEMORY_BASE, &mem_base_lo);
	pci_read_config_word(dev, PCI_MEMORY_LIMIT, &mem_limit_lo);
	base = (mem_base_lo & PCI_MEMORY_RANGE_MASK) << 16;
	limit = (mem_limit_lo & PCI_MEMORY_RANGE_MASK) << 16;
	if (base && base <= limit) {
		res->flags = (mem_base_lo & PCI_MEMORY_RANGE_TYPE_MASK) | IORESOURCE_MEM;
		region.start = base;
		region.end = limit + 0xfffff;
		pcibios_bus_to_resource(dev, res, &region);
		dev_printk(KERN_DEBUG, &dev->dev, "  bridge window %pR\n", res);
	}
}

static void __devinit pci_read_bridge_mmio_pref(struct pci_bus *child)
{
	struct pci_dev *dev = child->self;
	u16 mem_base_lo, mem_limit_lo;
	unsigned long base, limit;
	struct pci_bus_region region;
	struct resource *res;

	res = child->resource[2];
	pci_read_config_word(dev, PCI_PREF_MEMORY_BASE, &mem_base_lo);
	pci_read_config_word(dev, PCI_PREF_MEMORY_LIMIT, &mem_limit_lo);
	base = (mem_base_lo & PCI_PREF_RANGE_MASK) << 16;
	limit = (mem_limit_lo & PCI_PREF_RANGE_MASK) << 16;

	if ((mem_base_lo & PCI_PREF_RANGE_TYPE_MASK) == PCI_PREF_RANGE_TYPE_64) {
		u32 mem_base_hi, mem_limit_hi;
		pci_read_config_dword(dev, PCI_PREF_BASE_UPPER32, &mem_base_hi);
		pci_read_config_dword(dev, PCI_PREF_LIMIT_UPPER32, &mem_limit_hi);

		if (mem_base_hi <= mem_limit_hi) {
#if BITS_PER_LONG == 64
			base |= ((long) mem_base_hi) << 32;
			limit |= ((long) mem_limit_hi) << 32;
#else
			if (mem_base_hi || mem_limit_hi) {
				dev_err(&dev->dev, "can't handle 64-bit "
					"address space for bridge\n");
				return;
			}
#endif
		}
	}
	if (base && base <= limit) {
		res->flags = (mem_base_lo & PCI_PREF_RANGE_TYPE_MASK) |
					 IORESOURCE_MEM | IORESOURCE_PREFETCH;
		if (res->flags & PCI_PREF_RANGE_TYPE_64)
			res->flags |= IORESOURCE_MEM_64;
		region.start = base;
		region.end = limit + 0xfffff;
		pcibios_bus_to_resource(dev, res, &region);
		dev_printk(KERN_DEBUG, &dev->dev, "  bridge window %pR\n", res);
	}
}

void __devinit pci_read_bridge_bases(struct pci_bus *child)
{
	struct pci_dev *dev = child->self;
	struct resource *res;
	int i;

	if (pci_is_root_bus(child))	
		return;

	dev_info(&dev->dev, "PCI bridge to [bus %02x-%02x]%s\n",
		 child->secondary, child->subordinate,
		 dev->transparent ? " (subtractive decode)" : "");

	pci_bus_remove_resources(child);
	for (i = 0; i < PCI_BRIDGE_RESOURCE_NUM; i++)
		child->resource[i] = &dev->resource[PCI_BRIDGE_RESOURCES+i];

	pci_read_bridge_io(child);
	pci_read_bridge_mmio(child);
	pci_read_bridge_mmio_pref(child);

	if (dev->transparent) {
		pci_bus_for_each_resource(child->parent, res, i) {
			if (res) {
				pci_bus_add_resource(child, res,
						     PCI_SUBTRACTIVE_DECODE);
				dev_printk(KERN_DEBUG, &dev->dev,
					   "  bridge window %pR (subtractive decode)\n",
					   res);
			}
		}
	}
}

static struct pci_bus * pci_alloc_bus(void)
{
	struct pci_bus *b;

	b = kzalloc(sizeof(*b), GFP_KERNEL);
	if (b) {
		INIT_LIST_HEAD(&b->node);
		INIT_LIST_HEAD(&b->children);
		INIT_LIST_HEAD(&b->devices);
		INIT_LIST_HEAD(&b->slots);
		INIT_LIST_HEAD(&b->resources);
		b->max_bus_speed = PCI_SPEED_UNKNOWN;
		b->cur_bus_speed = PCI_SPEED_UNKNOWN;
	}
	return b;
}

static unsigned char pcix_bus_speed[] = {
	PCI_SPEED_UNKNOWN,		
	PCI_SPEED_66MHz_PCIX,		
	PCI_SPEED_100MHz_PCIX,		
	PCI_SPEED_133MHz_PCIX,		
	PCI_SPEED_UNKNOWN,		
	PCI_SPEED_66MHz_PCIX_ECC,	
	PCI_SPEED_100MHz_PCIX_ECC,	
	PCI_SPEED_133MHz_PCIX_ECC,	
	PCI_SPEED_UNKNOWN,		
	PCI_SPEED_66MHz_PCIX_266,	
	PCI_SPEED_100MHz_PCIX_266,	
	PCI_SPEED_133MHz_PCIX_266,	
	PCI_SPEED_UNKNOWN,		
	PCI_SPEED_66MHz_PCIX_533,	
	PCI_SPEED_100MHz_PCIX_533,	
	PCI_SPEED_133MHz_PCIX_533	
};

static unsigned char pcie_link_speed[] = {
	PCI_SPEED_UNKNOWN,		
	PCIE_SPEED_2_5GT,		
	PCIE_SPEED_5_0GT,		
	PCIE_SPEED_8_0GT,		
	PCI_SPEED_UNKNOWN,		
	PCI_SPEED_UNKNOWN,		
	PCI_SPEED_UNKNOWN,		
	PCI_SPEED_UNKNOWN,		
	PCI_SPEED_UNKNOWN,		
	PCI_SPEED_UNKNOWN,		
	PCI_SPEED_UNKNOWN,		
	PCI_SPEED_UNKNOWN,		
	PCI_SPEED_UNKNOWN,		
	PCI_SPEED_UNKNOWN,		
	PCI_SPEED_UNKNOWN,		
	PCI_SPEED_UNKNOWN		
};

void pcie_update_link_speed(struct pci_bus *bus, u16 linksta)
{
	bus->cur_bus_speed = pcie_link_speed[linksta & 0xf];
}
EXPORT_SYMBOL_GPL(pcie_update_link_speed);

static unsigned char agp_speeds[] = {
	AGP_UNKNOWN,
	AGP_1X,
	AGP_2X,
	AGP_4X,
	AGP_8X
};

static enum pci_bus_speed agp_speed(int agp3, int agpstat)
{
	int index = 0;

	if (agpstat & 4)
		index = 3;
	else if (agpstat & 2)
		index = 2;
	else if (agpstat & 1)
		index = 1;
	else
		goto out;
	
	if (agp3) {
		index += 2;
		if (index == 5)
			index = 0;
	}

 out:
	return agp_speeds[index];
}


static void pci_set_bus_speed(struct pci_bus *bus)
{
	struct pci_dev *bridge = bus->self;
	int pos;

	pos = pci_find_capability(bridge, PCI_CAP_ID_AGP);
	if (!pos)
		pos = pci_find_capability(bridge, PCI_CAP_ID_AGP3);
	if (pos) {
		u32 agpstat, agpcmd;

		pci_read_config_dword(bridge, pos + PCI_AGP_STATUS, &agpstat);
		bus->max_bus_speed = agp_speed(agpstat & 8, agpstat & 7);

		pci_read_config_dword(bridge, pos + PCI_AGP_COMMAND, &agpcmd);
		bus->cur_bus_speed = agp_speed(agpstat & 8, agpcmd & 7);
	}

	pos = pci_find_capability(bridge, PCI_CAP_ID_PCIX);
	if (pos) {
		u16 status;
		enum pci_bus_speed max;
		pci_read_config_word(bridge, pos + 2, &status);

		if (status & 0x8000) {
			max = PCI_SPEED_133MHz_PCIX_533;
		} else if (status & 0x4000) {
			max = PCI_SPEED_133MHz_PCIX_266;
		} else if (status & 0x0002) {
			if (((status >> 12) & 0x3) == 2) {
				max = PCI_SPEED_133MHz_PCIX_ECC;
			} else {
				max = PCI_SPEED_133MHz_PCIX;
			}
		} else {
			max = PCI_SPEED_66MHz_PCIX;
		}

		bus->max_bus_speed = max;
		bus->cur_bus_speed = pcix_bus_speed[(status >> 6) & 0xf];

		return;
	}

	pos = pci_find_capability(bridge, PCI_CAP_ID_EXP);
	if (pos) {
		u32 linkcap;
		u16 linksta;

		pci_read_config_dword(bridge, pos + PCI_EXP_LNKCAP, &linkcap);
		bus->max_bus_speed = pcie_link_speed[linkcap & 0xf];

		pci_read_config_word(bridge, pos + PCI_EXP_LNKSTA, &linksta);
		pcie_update_link_speed(bus, linksta);
	}
}


static struct pci_bus *pci_alloc_child_bus(struct pci_bus *parent,
					   struct pci_dev *bridge, int busnr)
{
	struct pci_bus *child;
	int i;

	child = pci_alloc_bus();
	if (!child)
		return NULL;

	child->parent = parent;
	child->ops = parent->ops;
	child->sysdata = parent->sysdata;
	child->bus_flags = parent->bus_flags;

	child->dev.class = &pcibus_class;
	dev_set_name(&child->dev, "%04x:%02x", pci_domain_nr(child), busnr);

	child->number = child->secondary = busnr;
	child->primary = parent->secondary;
	child->subordinate = 0xff;

	if (!bridge)
		return child;

	child->self = bridge;
	child->bridge = get_device(&bridge->dev);
	pci_set_bus_of_node(child);
	pci_set_bus_speed(child);

	
	for (i = 0; i < PCI_BRIDGE_RESOURCE_NUM; i++) {
		child->resource[i] = &bridge->resource[PCI_BRIDGE_RESOURCES+i];
		child->resource[i]->name = child->name;
	}
	bridge->subordinate = child;

	return child;
}

struct pci_bus *__ref pci_add_new_bus(struct pci_bus *parent, struct pci_dev *dev, int busnr)
{
	struct pci_bus *child;

	child = pci_alloc_child_bus(parent, dev, busnr);
	if (child) {
		down_write(&pci_bus_sem);
		list_add_tail(&child->node, &parent->children);
		up_write(&pci_bus_sem);
	}
	return child;
}

static void pci_fixup_parent_subordinate_busnr(struct pci_bus *child, int max)
{
	struct pci_bus *parent = child->parent;

	if (!pcibios_assign_all_busses())
		return;

	while (parent->parent && parent->subordinate < max) {
		parent->subordinate = max;
		pci_write_config_byte(parent->self, PCI_SUBORDINATE_BUS, max);
		parent = parent->parent;
	}
}

int __devinit pci_scan_bridge(struct pci_bus *bus, struct pci_dev *dev, int max, int pass)
{
	struct pci_bus *child;
	int is_cardbus = (dev->hdr_type == PCI_HEADER_TYPE_CARDBUS);
	u32 buses, i, j = 0;
	u16 bctl;
	u8 primary, secondary, subordinate;
	int broken = 0;

	pci_read_config_dword(dev, PCI_PRIMARY_BUS, &buses);
	primary = buses & 0xFF;
	secondary = (buses >> 8) & 0xFF;
	subordinate = (buses >> 16) & 0xFF;

	dev_dbg(&dev->dev, "scanning [bus %02x-%02x] behind bridge, pass %d\n",
		secondary, subordinate, pass);

	if (!primary && (primary != bus->number) && secondary && subordinate) {
		dev_warn(&dev->dev, "Primary bus is hard wired to 0\n");
		primary = bus->number;
	}

	
	if (!pass &&
	    (primary != bus->number || secondary <= bus->number)) {
		dev_dbg(&dev->dev, "bus configuration invalid, reconfiguring\n");
		broken = 1;
	}

 
	pci_read_config_word(dev, PCI_BRIDGE_CONTROL, &bctl);
	pci_write_config_word(dev, PCI_BRIDGE_CONTROL,
			      bctl & ~PCI_BRIDGE_CTL_MASTER_ABORT);

	if ((secondary || subordinate) && !pcibios_assign_all_busses() &&
	    !is_cardbus && !broken) {
		unsigned int cmax;
		if (pass)
			goto out;

		child = pci_find_bus(pci_domain_nr(bus), secondary);
		if (!child) {
			child = pci_add_new_bus(bus, dev, secondary);
			if (!child)
				goto out;
			child->primary = primary;
			child->subordinate = subordinate;
			child->bridge_ctl = bctl;
		}

		cmax = pci_scan_child_bus(child);
		if (cmax > max)
			max = cmax;
		if (child->subordinate > max)
			max = child->subordinate;
	} else {
		if (!pass) {
			if (pcibios_assign_all_busses() || broken)
				pci_write_config_dword(dev, PCI_PRIMARY_BUS,
						       buses & ~0xffffff);
			goto out;
		}

		
		pci_write_config_word(dev, PCI_STATUS, 0xffff);

		child = pci_find_bus(pci_domain_nr(bus), max+1);
		if (!child) {
			child = pci_add_new_bus(bus, dev, ++max);
			if (!child)
				goto out;
		}
		buses = (buses & 0xff000000)
		      | ((unsigned int)(child->primary)     <<  0)
		      | ((unsigned int)(child->secondary)   <<  8)
		      | ((unsigned int)(child->subordinate) << 16);

		if (is_cardbus) {
			buses &= ~0xff000000;
			buses |= CARDBUS_LATENCY_TIMER << 24;
		}

		pci_write_config_dword(dev, PCI_PRIMARY_BUS, buses);

		if (!is_cardbus) {
			child->bridge_ctl = bctl;
			pci_fixup_parent_subordinate_busnr(child, max);
			
			max = pci_scan_child_bus(child);
			pci_fixup_parent_subordinate_busnr(child, max);
		} else {
			for (i=0; i<CARDBUS_RESERVE_BUSNR; i++) {
				struct pci_bus *parent = bus;
				if (pci_find_bus(pci_domain_nr(bus),
							max+i+1))
					break;
				while (parent->parent) {
					if ((!pcibios_assign_all_busses()) &&
					    (parent->subordinate > max) &&
					    (parent->subordinate <= max+i)) {
						j = 1;
					}
					parent = parent->parent;
				}
				if (j) {
					i /= 2;
					break;
				}
			}
			max += i;
			pci_fixup_parent_subordinate_busnr(child, max);
		}
		child->subordinate = max;
		pci_write_config_byte(dev, PCI_SUBORDINATE_BUS, max);
	}

	sprintf(child->name,
		(is_cardbus ? "PCI CardBus %04x:%02x" : "PCI Bus %04x:%02x"),
		pci_domain_nr(bus), child->number);

	
	while (bus->parent) {
		if ((child->subordinate > bus->subordinate) ||
		    (child->number > bus->subordinate) ||
		    (child->number < bus->number) ||
		    (child->subordinate < bus->number)) {
			dev_info(&child->dev, "[bus %02x-%02x] %s "
				"hidden behind%s bridge %s [bus %02x-%02x]\n",
				child->number, child->subordinate,
				(bus->number > child->subordinate &&
				 bus->subordinate < child->number) ?
					"wholly" : "partially",
				bus->self->transparent ? " transparent" : "",
				dev_name(&bus->dev),
				bus->number, bus->subordinate);
		}
		bus = bus->parent;
	}

out:
	pci_write_config_word(dev, PCI_BRIDGE_CONTROL, bctl);

	return max;
}

static void pci_read_irq(struct pci_dev *dev)
{
	unsigned char irq;

	pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &irq);
	dev->pin = irq;
	if (irq)
		pci_read_config_byte(dev, PCI_INTERRUPT_LINE, &irq);
	dev->irq = irq;
}

void set_pcie_port_type(struct pci_dev *pdev)
{
	int pos;
	u16 reg16;

	pos = pci_find_capability(pdev, PCI_CAP_ID_EXP);
	if (!pos)
		return;
	pdev->is_pcie = 1;
	pdev->pcie_cap = pos;
	pci_read_config_word(pdev, pos + PCI_EXP_FLAGS, &reg16);
	pdev->pcie_type = (reg16 & PCI_EXP_FLAGS_TYPE) >> 4;
	pci_read_config_word(pdev, pos + PCI_EXP_DEVCAP, &reg16);
	pdev->pcie_mpss = reg16 & PCI_EXP_DEVCAP_PAYLOAD;
}

void set_pcie_hotplug_bridge(struct pci_dev *pdev)
{
	int pos;
	u16 reg16;
	u32 reg32;

	pos = pci_pcie_cap(pdev);
	if (!pos)
		return;
	pci_read_config_word(pdev, pos + PCI_EXP_FLAGS, &reg16);
	if (!(reg16 & PCI_EXP_FLAGS_SLOT))
		return;
	pci_read_config_dword(pdev, pos + PCI_EXP_SLTCAP, &reg32);
	if (reg32 & PCI_EXP_SLTCAP_HPC)
		pdev->is_hotplug_bridge = 1;
}

#define LEGACY_IO_RESOURCE	(IORESOURCE_IO | IORESOURCE_PCI_FIXED)

int pci_setup_device(struct pci_dev *dev)
{
	u32 class;
	u8 hdr_type;
	struct pci_slot *slot;
	int pos = 0;
	struct pci_bus_region region;
	struct resource *res;

	if (pci_read_config_byte(dev, PCI_HEADER_TYPE, &hdr_type))
		return -EIO;

	dev->sysdata = dev->bus->sysdata;
	dev->dev.parent = dev->bus->bridge;
	dev->dev.bus = &pci_bus_type;
	dev->hdr_type = hdr_type & 0x7f;
	dev->multifunction = !!(hdr_type & 0x80);
	dev->error_state = pci_channel_io_normal;
	set_pcie_port_type(dev);

	list_for_each_entry(slot, &dev->bus->slots, list)
		if (PCI_SLOT(dev->devfn) == slot->number)
			dev->slot = slot;

	dev->dma_mask = 0xffffffff;

	dev_set_name(&dev->dev, "%04x:%02x:%02x.%d", pci_domain_nr(dev->bus),
		     dev->bus->number, PCI_SLOT(dev->devfn),
		     PCI_FUNC(dev->devfn));

	pci_read_config_dword(dev, PCI_CLASS_REVISION, &class);
	dev->revision = class & 0xff;
	dev->class = class >> 8;		    

	dev_printk(KERN_DEBUG, &dev->dev, "[%04x:%04x] type %02x class %#08x\n",
		   dev->vendor, dev->device, dev->hdr_type, dev->class);

	
	dev->cfg_size = pci_cfg_space_size(dev);

	
	dev->current_state = PCI_UNKNOWN;

	
	pci_fixup_device(pci_fixup_early, dev);
	
	class = dev->class >> 8;

	switch (dev->hdr_type) {		    
	case PCI_HEADER_TYPE_NORMAL:		    
		if (class == PCI_CLASS_BRIDGE_PCI)
			goto bad;
		pci_read_irq(dev);
		pci_read_bases(dev, 6, PCI_ROM_ADDRESS);
		pci_read_config_word(dev, PCI_SUBSYSTEM_VENDOR_ID, &dev->subsystem_vendor);
		pci_read_config_word(dev, PCI_SUBSYSTEM_ID, &dev->subsystem_device);

		if (class == PCI_CLASS_STORAGE_IDE) {
			u8 progif;
			pci_read_config_byte(dev, PCI_CLASS_PROG, &progif);
			if ((progif & 1) == 0) {
				region.start = 0x1F0;
				region.end = 0x1F7;
				res = &dev->resource[0];
				res->flags = LEGACY_IO_RESOURCE;
				pcibios_bus_to_resource(dev, res, &region);
				region.start = 0x3F6;
				region.end = 0x3F6;
				res = &dev->resource[1];
				res->flags = LEGACY_IO_RESOURCE;
				pcibios_bus_to_resource(dev, res, &region);
			}
			if ((progif & 4) == 0) {
				region.start = 0x170;
				region.end = 0x177;
				res = &dev->resource[2];
				res->flags = LEGACY_IO_RESOURCE;
				pcibios_bus_to_resource(dev, res, &region);
				region.start = 0x376;
				region.end = 0x376;
				res = &dev->resource[3];
				res->flags = LEGACY_IO_RESOURCE;
				pcibios_bus_to_resource(dev, res, &region);
			}
		}
		break;

	case PCI_HEADER_TYPE_BRIDGE:		    
		if (class != PCI_CLASS_BRIDGE_PCI)
			goto bad;
 
		pci_read_irq(dev);
		dev->transparent = ((dev->class & 0xff) == 1);
		pci_read_bases(dev, 2, PCI_ROM_ADDRESS1);
		set_pcie_hotplug_bridge(dev);
		pos = pci_find_capability(dev, PCI_CAP_ID_SSVID);
		if (pos) {
			pci_read_config_word(dev, pos + PCI_SSVID_VENDOR_ID, &dev->subsystem_vendor);
			pci_read_config_word(dev, pos + PCI_SSVID_DEVICE_ID, &dev->subsystem_device);
		}
		break;

	case PCI_HEADER_TYPE_CARDBUS:		    
		if (class != PCI_CLASS_BRIDGE_CARDBUS)
			goto bad;
		pci_read_irq(dev);
		pci_read_bases(dev, 1, 0);
		pci_read_config_word(dev, PCI_CB_SUBSYSTEM_VENDOR_ID, &dev->subsystem_vendor);
		pci_read_config_word(dev, PCI_CB_SUBSYSTEM_ID, &dev->subsystem_device);
		break;

	default:				    
		dev_err(&dev->dev, "unknown header type %02x, "
			"ignoring device\n", dev->hdr_type);
		return -EIO;

	bad:
		dev_err(&dev->dev, "ignoring class %#08x (doesn't match header "
			"type %02x)\n", dev->class, dev->hdr_type);
		dev->class = PCI_CLASS_NOT_DEFINED;
	}

	
	return 0;
}

static void pci_release_capabilities(struct pci_dev *dev)
{
	pci_vpd_release(dev);
	pci_iov_release(dev);
	pci_free_cap_save_buffers(dev);
}

static void pci_release_dev(struct device *dev)
{
	struct pci_dev *pci_dev;

	pci_dev = to_pci_dev(dev);
	pci_release_capabilities(pci_dev);
	pci_release_of_node(pci_dev);
	kfree(pci_dev);
}

int pci_cfg_space_size_ext(struct pci_dev *dev)
{
	u32 status;
	int pos = PCI_CFG_SPACE_SIZE;

	if (pci_read_config_dword(dev, pos, &status) != PCIBIOS_SUCCESSFUL)
		goto fail;
	if (status == 0xffffffff)
		goto fail;

	return PCI_CFG_SPACE_EXP_SIZE;

 fail:
	return PCI_CFG_SPACE_SIZE;
}

int pci_cfg_space_size(struct pci_dev *dev)
{
	int pos;
	u32 status;
	u16 class;

	class = dev->class >> 8;
	if (class == PCI_CLASS_BRIDGE_HOST)
		return pci_cfg_space_size_ext(dev);

	pos = pci_pcie_cap(dev);
	if (!pos) {
		pos = pci_find_capability(dev, PCI_CAP_ID_PCIX);
		if (!pos)
			goto fail;

		pci_read_config_dword(dev, pos + PCI_X_STATUS, &status);
		if (!(status & (PCI_X_STATUS_266MHZ | PCI_X_STATUS_533MHZ)))
			goto fail;
	}

	return pci_cfg_space_size_ext(dev);

 fail:
	return PCI_CFG_SPACE_SIZE;
}

static void pci_release_bus_bridge_dev(struct device *dev)
{
	kfree(dev);
}

struct pci_dev *alloc_pci_dev(void)
{
	struct pci_dev *dev;

	dev = kzalloc(sizeof(struct pci_dev), GFP_KERNEL);
	if (!dev)
		return NULL;

	INIT_LIST_HEAD(&dev->bus_list);

	return dev;
}
EXPORT_SYMBOL(alloc_pci_dev);

bool pci_bus_read_dev_vendor_id(struct pci_bus *bus, int devfn, u32 *l,
				 int crs_timeout)
{
	int delay = 1;

	if (pci_bus_read_config_dword(bus, devfn, PCI_VENDOR_ID, l))
		return false;

	
	if (*l == 0xffffffff || *l == 0x00000000 ||
	    *l == 0x0000ffff || *l == 0xffff0000)
		return false;

	
	while (*l == 0xffff0001) {
		if (!crs_timeout)
			return false;

		msleep(delay);
		delay *= 2;
		if (pci_bus_read_config_dword(bus, devfn, PCI_VENDOR_ID, l))
			return false;
		
		if (delay > crs_timeout) {
			printk(KERN_WARNING "pci %04x:%02x:%02x.%d: not "
					"responding\n", pci_domain_nr(bus),
					bus->number, PCI_SLOT(devfn),
					PCI_FUNC(devfn));
			return false;
		}
	}

	return true;
}
EXPORT_SYMBOL(pci_bus_read_dev_vendor_id);

static struct pci_dev *pci_scan_device(struct pci_bus *bus, int devfn)
{
	struct pci_dev *dev;
	u32 l;

	if (!pci_bus_read_dev_vendor_id(bus, devfn, &l, 60*1000))
		return NULL;

	dev = alloc_pci_dev();
	if (!dev)
		return NULL;

	dev->bus = bus;
	dev->devfn = devfn;
	dev->vendor = l & 0xffff;
	dev->device = (l >> 16) & 0xffff;

	pci_set_of_node(dev);

	if (pci_setup_device(dev)) {
		kfree(dev);
		return NULL;
	}

	return dev;
}

static void pci_init_capabilities(struct pci_dev *dev)
{
	
	pci_msi_init_pci_dev(dev);

	
	pci_allocate_cap_save_buffers(dev);

	
	pci_pm_init(dev);
	platform_pci_wakeup_init(dev);

	
	pci_vpd_pci22_init(dev);

	
	pci_enable_ari(dev);

	
	pci_iov_init(dev);

	
	pci_enable_acs(dev);
}

void pci_device_add(struct pci_dev *dev, struct pci_bus *bus)
{
	device_initialize(&dev->dev);
	dev->dev.release = pci_release_dev;
	pci_dev_get(dev);

	dev->dev.dma_mask = &dev->dma_mask;
	dev->dev.dma_parms = &dev->dma_parms;
	dev->dev.coherent_dma_mask = 0xffffffffull;

	pci_set_dma_max_seg_size(dev, 65536);
	pci_set_dma_seg_boundary(dev, 0xffffffff);

	
	pci_fixup_device(pci_fixup_header, dev);

	
	pci_reassigndev_resource_alignment(dev);

	
	dev->state_saved = false;

	
	pci_init_capabilities(dev);

	down_write(&pci_bus_sem);
	list_add_tail(&dev->bus_list, &bus->devices);
	up_write(&pci_bus_sem);
}

struct pci_dev *__ref pci_scan_single_device(struct pci_bus *bus, int devfn)
{
	struct pci_dev *dev;

	dev = pci_get_slot(bus, devfn);
	if (dev) {
		pci_dev_put(dev);
		return dev;
	}

	dev = pci_scan_device(bus, devfn);
	if (!dev)
		return NULL;

	pci_device_add(dev, bus);

	return dev;
}
EXPORT_SYMBOL(pci_scan_single_device);

static unsigned next_ari_fn(struct pci_dev *dev, unsigned fn)
{
	u16 cap;
	unsigned pos, next_fn;

	if (!dev)
		return 0;

	pos = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_ARI);
	if (!pos)
		return 0;
	pci_read_config_word(dev, pos + 4, &cap);
	next_fn = cap >> 8;
	if (next_fn <= fn)
		return 0;
	return next_fn;
}

static unsigned next_trad_fn(struct pci_dev *dev, unsigned fn)
{
	return (fn + 1) % 8;
}

static unsigned no_next_fn(struct pci_dev *dev, unsigned fn)
{
	return 0;
}

static int only_one_child(struct pci_bus *bus)
{
	struct pci_dev *parent = bus->self;
	if (!parent || !pci_is_pcie(parent))
		return 0;
	if (parent->pcie_type == PCI_EXP_TYPE_ROOT_PORT ||
	    parent->pcie_type == PCI_EXP_TYPE_DOWNSTREAM)
		return 1;
	return 0;
}

int pci_scan_slot(struct pci_bus *bus, int devfn)
{
	unsigned fn, nr = 0;
	struct pci_dev *dev;
	unsigned (*next_fn)(struct pci_dev *, unsigned) = no_next_fn;

	if (only_one_child(bus) && (devfn > 0))
		return 0; 

	dev = pci_scan_single_device(bus, devfn);
	if (!dev)
		return 0;
	if (!dev->is_added)
		nr++;

	if (pci_ari_enabled(bus))
		next_fn = next_ari_fn;
	else if (dev->multifunction)
		next_fn = next_trad_fn;

	for (fn = next_fn(dev, 0); fn > 0; fn = next_fn(dev, fn)) {
		dev = pci_scan_single_device(bus, devfn + fn);
		if (dev) {
			if (!dev->is_added)
				nr++;
			dev->multifunction = 1;
		}
	}

	
	if (bus->self && nr)
		pcie_aspm_init_link_state(bus->self);

	return nr;
}

static int pcie_find_smpss(struct pci_dev *dev, void *data)
{
	u8 *smpss = data;

	if (!pci_is_pcie(dev))
		return 0;

	if (dev->is_hotplug_bridge && (!list_is_singular(&dev->bus->devices) ||
	     (dev->bus->self &&
	      dev->bus->self->pcie_type != PCI_EXP_TYPE_ROOT_PORT)))
		*smpss = 0;

	if (*smpss > dev->pcie_mpss)
		*smpss = dev->pcie_mpss;

	return 0;
}

static void pcie_write_mps(struct pci_dev *dev, int mps)
{
	int rc;

	if (pcie_bus_config == PCIE_BUS_PERFORMANCE) {
		mps = 128 << dev->pcie_mpss;

		if (dev->pcie_type != PCI_EXP_TYPE_ROOT_PORT && dev->bus->self)
			mps = min(mps, pcie_get_mps(dev->bus->self));
	}

	rc = pcie_set_mps(dev, mps);
	if (rc)
		dev_err(&dev->dev, "Failed attempting to set the MPS\n");
}

static void pcie_write_mrrs(struct pci_dev *dev)
{
	int rc, mrrs;

	if (pcie_bus_config != PCIE_BUS_PERFORMANCE)
		return;

	mrrs = pcie_get_mps(dev);

	/* MRRS is a R/W register.  Invalid values can be written, but a
	 * subsequent read will verify if the value is acceptable or not.
	 * If the MRRS value provided is not acceptable (e.g., too large),
	 * shrink the value until it is acceptable to the HW.
 	 */
	while (mrrs != pcie_get_readrq(dev) && mrrs >= 128) {
		rc = pcie_set_readrq(dev, mrrs);
		if (!rc)
			break;

		dev_warn(&dev->dev, "Failed attempting to set the MRRS\n");
		mrrs /= 2;
	}

	if (mrrs < 128)
		dev_err(&dev->dev, "MRRS was unable to be configured with a "
			"safe value.  If problems are experienced, try running "
			"with pci=pcie_bus_safe.\n");
}

static int pcie_bus_configure_set(struct pci_dev *dev, void *data)
{
	int mps, orig_mps;

	if (!pci_is_pcie(dev))
		return 0;

	mps = 128 << *(u8 *)data;
	orig_mps = pcie_get_mps(dev);

	pcie_write_mps(dev, mps);
	pcie_write_mrrs(dev);

	dev_info(&dev->dev, "PCI-E Max Payload Size set to %4d/%4d (was %4d), "
		 "Max Read Rq %4d\n", pcie_get_mps(dev), 128 << dev->pcie_mpss,
		 orig_mps, pcie_get_readrq(dev));

	return 0;
}

void pcie_bus_configure_settings(struct pci_bus *bus, u8 mpss)
{
	u8 smpss;

	if (!pci_is_pcie(bus->self))
		return;

	if (pcie_bus_config == PCIE_BUS_TUNE_OFF)
		return;

	if (pcie_bus_config == PCIE_BUS_PEER2PEER)
		smpss = 0;

	if (pcie_bus_config == PCIE_BUS_SAFE) {
		smpss = mpss;

		pcie_find_smpss(bus->self, &smpss);
		pci_walk_bus(bus, pcie_find_smpss, &smpss);
	}

	pcie_bus_configure_set(bus->self, &smpss);
	pci_walk_bus(bus, pcie_bus_configure_set, &smpss);
}
EXPORT_SYMBOL_GPL(pcie_bus_configure_settings);

unsigned int __devinit pci_scan_child_bus(struct pci_bus *bus)
{
	unsigned int devfn, pass, max = bus->secondary;
	struct pci_dev *dev;

	dev_dbg(&bus->dev, "scanning bus\n");

	
	for (devfn = 0; devfn < 0x100; devfn += 8)
		pci_scan_slot(bus, devfn);

	
	max += pci_iov_bus_range(bus);

	if (!bus->is_added) {
		dev_dbg(&bus->dev, "fixups for bus\n");
		pcibios_fixup_bus(bus);
		if (pci_is_root_bus(bus))
			bus->is_added = 1;
	}

	for (pass=0; pass < 2; pass++)
		list_for_each_entry(dev, &bus->devices, bus_list) {
			if (dev->hdr_type == PCI_HEADER_TYPE_BRIDGE ||
			    dev->hdr_type == PCI_HEADER_TYPE_CARDBUS)
				max = pci_scan_bridge(bus, dev, max, pass);
		}

	dev_dbg(&bus->dev, "bus scan returning with max=%02x\n", max);
	return max;
}

struct pci_bus *pci_create_root_bus(struct device *parent, int bus,
		struct pci_ops *ops, void *sysdata, struct list_head *resources)
{
	int error;
	struct pci_host_bridge *bridge;
	struct pci_bus *b, *b2;
	struct device *dev;
	struct pci_host_bridge_window *window, *n;
	struct resource *res;
	resource_size_t offset;
	char bus_addr[64];
	char *fmt;

	bridge = kzalloc(sizeof(*bridge), GFP_KERNEL);
	if (!bridge)
		return NULL;

	b = pci_alloc_bus();
	if (!b)
		goto err_bus;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		goto err_dev;

	b->sysdata = sysdata;
	b->ops = ops;

	b2 = pci_find_bus(pci_domain_nr(b), bus);
	if (b2) {
		
		dev_dbg(&b2->dev, "bus already known\n");
		goto err_out;
	}

	dev->parent = parent;
	dev->release = pci_release_bus_bridge_dev;
	dev_set_name(dev, "pci%04x:%02x", pci_domain_nr(b), bus);
	error = device_register(dev);
	if (error)
		goto dev_reg_err;
	b->bridge = get_device(dev);
	device_enable_async_suspend(b->bridge);
	pci_set_bus_of_node(b);

	if (!parent)
		set_dev_node(b->bridge, pcibus_to_node(b));

	b->dev.class = &pcibus_class;
	b->dev.parent = b->bridge;
	dev_set_name(&b->dev, "%04x:%02x", pci_domain_nr(b), bus);
	error = device_register(&b->dev);
	if (error)
		goto class_dev_reg_err;

	
	pci_create_legacy_files(b);

	b->number = b->secondary = bus;

	bridge->bus = b;
	INIT_LIST_HEAD(&bridge->windows);

	if (parent)
		dev_info(parent, "PCI host bridge to bus %s\n", dev_name(&b->dev));
	else
		printk(KERN_INFO "PCI host bridge to bus %s\n", dev_name(&b->dev));

	
	list_for_each_entry_safe(window, n, resources, list) {
		list_move_tail(&window->list, &bridge->windows);
		res = window->res;
		offset = window->offset;
		pci_bus_add_resource(b, res, 0);
		if (offset) {
			if (resource_type(res) == IORESOURCE_IO)
				fmt = " (bus address [%#06llx-%#06llx])";
			else
				fmt = " (bus address [%#010llx-%#010llx])";
			snprintf(bus_addr, sizeof(bus_addr), fmt,
				 (unsigned long long) (res->start - offset),
				 (unsigned long long) (res->end - offset));
		} else
			bus_addr[0] = '\0';
		dev_info(&b->dev, "root bus resource %pR%s\n", res, bus_addr);
	}

	down_write(&pci_bus_sem);
	list_add_tail(&bridge->list, &pci_host_bridges);
	list_add_tail(&b->node, &pci_root_buses);
	up_write(&pci_bus_sem);

	return b;

class_dev_reg_err:
	device_unregister(dev);
dev_reg_err:
	down_write(&pci_bus_sem);
	list_del(&bridge->list);
	list_del(&b->node);
	up_write(&pci_bus_sem);
err_out:
	kfree(dev);
err_dev:
	kfree(b);
err_bus:
	kfree(bridge);
	return NULL;
}

struct pci_bus * __devinit pci_scan_root_bus(struct device *parent, int bus,
		struct pci_ops *ops, void *sysdata, struct list_head *resources)
{
	struct pci_bus *b;

	b = pci_create_root_bus(parent, bus, ops, sysdata, resources);
	if (!b)
		return NULL;

	b->subordinate = pci_scan_child_bus(b);
	pci_bus_add_devices(b);
	return b;
}
EXPORT_SYMBOL(pci_scan_root_bus);

struct pci_bus * __devinit pci_scan_bus_parented(struct device *parent,
		int bus, struct pci_ops *ops, void *sysdata)
{
	LIST_HEAD(resources);
	struct pci_bus *b;

	pci_add_resource(&resources, &ioport_resource);
	pci_add_resource(&resources, &iomem_resource);
	b = pci_create_root_bus(parent, bus, ops, sysdata, &resources);
	if (b)
		b->subordinate = pci_scan_child_bus(b);
	else
		pci_free_resource_list(&resources);
	return b;
}
EXPORT_SYMBOL(pci_scan_bus_parented);

struct pci_bus * __devinit pci_scan_bus(int bus, struct pci_ops *ops,
					void *sysdata)
{
	LIST_HEAD(resources);
	struct pci_bus *b;

	pci_add_resource(&resources, &ioport_resource);
	pci_add_resource(&resources, &iomem_resource);
	b = pci_create_root_bus(NULL, bus, ops, sysdata, &resources);
	if (b) {
		b->subordinate = pci_scan_child_bus(b);
		pci_bus_add_devices(b);
	} else {
		pci_free_resource_list(&resources);
	}
	return b;
}
EXPORT_SYMBOL(pci_scan_bus);

#ifdef CONFIG_HOTPLUG
unsigned int __ref pci_rescan_bus_bridge_resize(struct pci_dev *bridge)
{
	unsigned int max;
	struct pci_bus *bus = bridge->subordinate;

	max = pci_scan_child_bus(bus);

	pci_assign_unassigned_bridge_resources(bridge);

	pci_bus_add_devices(bus);

	return max;
}

EXPORT_SYMBOL(pci_add_new_bus);
EXPORT_SYMBOL(pci_scan_slot);
EXPORT_SYMBOL(pci_scan_bridge);
EXPORT_SYMBOL_GPL(pci_scan_child_bus);
#endif

static int __init pci_sort_bf_cmp(const struct device *d_a, const struct device *d_b)
{
	const struct pci_dev *a = to_pci_dev(d_a);
	const struct pci_dev *b = to_pci_dev(d_b);

	if      (pci_domain_nr(a->bus) < pci_domain_nr(b->bus)) return -1;
	else if (pci_domain_nr(a->bus) > pci_domain_nr(b->bus)) return  1;

	if      (a->bus->number < b->bus->number) return -1;
	else if (a->bus->number > b->bus->number) return  1;

	if      (a->devfn < b->devfn) return -1;
	else if (a->devfn > b->devfn) return  1;

	return 0;
}

void __init pci_sort_breadthfirst(void)
{
	bus_sort_breadthfirst(&pci_bus_type, &pci_sort_bf_cmp);
}
