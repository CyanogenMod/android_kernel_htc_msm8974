
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/pci_regs.h>
#include <linux/string.h>

#define OF_MAX_ADDR_CELLS	4
#define OF_CHECK_ADDR_COUNT(na)	((na) > 0 && (na) <= OF_MAX_ADDR_CELLS)
#define OF_CHECK_COUNTS(na, ns)	(OF_CHECK_ADDR_COUNT(na) && (ns) > 0)

static struct of_bus *of_match_bus(struct device_node *np);
static int __of_address_to_resource(struct device_node *dev,
		const __be32 *addrp, u64 size, unsigned int flags,
		const char *name, struct resource *r);

#ifdef DEBUG
static void of_dump_addr(const char *s, const __be32 *addr, int na)
{
	printk(KERN_DEBUG "%s", s);
	while (na--)
		printk(" %08x", be32_to_cpu(*(addr++)));
	printk("\n");
}
#else
static void of_dump_addr(const char *s, const __be32 *addr, int na) { }
#endif

struct of_bus {
	const char	*name;
	const char	*addresses;
	int		(*match)(struct device_node *parent);
	void		(*count_cells)(struct device_node *child,
				       int *addrc, int *sizec);
	u64		(*map)(u32 *addr, const __be32 *range,
				int na, int ns, int pna);
	int		(*translate)(u32 *addr, u64 offset, int na);
	unsigned int	(*get_flags)(const __be32 *addr);
};


static void of_bus_default_count_cells(struct device_node *dev,
				       int *addrc, int *sizec)
{
	if (addrc)
		*addrc = of_n_addr_cells(dev);
	if (sizec)
		*sizec = of_n_size_cells(dev);
}

static u64 of_bus_default_map(u32 *addr, const __be32 *range,
		int na, int ns, int pna)
{
	u64 cp, s, da;

	cp = of_read_number(range, na);
	s  = of_read_number(range + na + pna, ns);
	da = of_read_number(addr, na);

	pr_debug("OF: default map, cp=%llx, s=%llx, da=%llx\n",
		 (unsigned long long)cp, (unsigned long long)s,
		 (unsigned long long)da);

	if (da < cp || da >= (cp + s))
		return OF_BAD_ADDR;
	return da - cp;
}

static int of_bus_default_translate(u32 *addr, u64 offset, int na)
{
	u64 a = of_read_number(addr, na);
	memset(addr, 0, na * 4);
	a += offset;
	if (na > 1)
		addr[na - 2] = cpu_to_be32(a >> 32);
	addr[na - 1] = cpu_to_be32(a & 0xffffffffu);

	return 0;
}

static unsigned int of_bus_default_get_flags(const __be32 *addr)
{
	return IORESOURCE_MEM;
}

#ifdef CONFIG_PCI

static int of_bus_pci_match(struct device_node *np)
{
	
	return !strcmp(np->type, "pci") || !strcmp(np->type, "vci");
}

static void of_bus_pci_count_cells(struct device_node *np,
				   int *addrc, int *sizec)
{
	if (addrc)
		*addrc = 3;
	if (sizec)
		*sizec = 2;
}

static unsigned int of_bus_pci_get_flags(const __be32 *addr)
{
	unsigned int flags = 0;
	u32 w = be32_to_cpup(addr);

	switch((w >> 24) & 0x03) {
	case 0x01:
		flags |= IORESOURCE_IO;
		break;
	case 0x02: 
	case 0x03: 
		flags |= IORESOURCE_MEM;
		break;
	}
	if (w & 0x40000000)
		flags |= IORESOURCE_PREFETCH;
	return flags;
}

static u64 of_bus_pci_map(u32 *addr, const __be32 *range, int na, int ns,
		int pna)
{
	u64 cp, s, da;
	unsigned int af, rf;

	af = of_bus_pci_get_flags(addr);
	rf = of_bus_pci_get_flags(range);

	
	if ((af ^ rf) & (IORESOURCE_MEM | IORESOURCE_IO))
		return OF_BAD_ADDR;

	
	cp = of_read_number(range + 1, na - 1);
	s  = of_read_number(range + na + pna, ns);
	da = of_read_number(addr + 1, na - 1);

	pr_debug("OF: PCI map, cp=%llx, s=%llx, da=%llx\n",
		 (unsigned long long)cp, (unsigned long long)s,
		 (unsigned long long)da);

	if (da < cp || da >= (cp + s))
		return OF_BAD_ADDR;
	return da - cp;
}

static int of_bus_pci_translate(u32 *addr, u64 offset, int na)
{
	return of_bus_default_translate(addr + 1, offset, na - 1);
}

const __be32 *of_get_pci_address(struct device_node *dev, int bar_no, u64 *size,
			unsigned int *flags)
{
	const __be32 *prop;
	unsigned int psize;
	struct device_node *parent;
	struct of_bus *bus;
	int onesize, i, na, ns;

	
	parent = of_get_parent(dev);
	if (parent == NULL)
		return NULL;
	bus = of_match_bus(parent);
	if (strcmp(bus->name, "pci")) {
		of_node_put(parent);
		return NULL;
	}
	bus->count_cells(dev, &na, &ns);
	of_node_put(parent);
	if (!OF_CHECK_ADDR_COUNT(na))
		return NULL;

	
	prop = of_get_property(dev, bus->addresses, &psize);
	if (prop == NULL)
		return NULL;
	psize /= 4;

	onesize = na + ns;
	for (i = 0; psize >= onesize; psize -= onesize, prop += onesize, i++) {
		u32 val = be32_to_cpu(prop[0]);
		if ((val & 0xff) == ((bar_no * 4) + PCI_BASE_ADDRESS_0)) {
			if (size)
				*size = of_read_number(prop + na, ns);
			if (flags)
				*flags = bus->get_flags(prop);
			return prop;
		}
	}
	return NULL;
}
EXPORT_SYMBOL(of_get_pci_address);

int of_pci_address_to_resource(struct device_node *dev, int bar,
			       struct resource *r)
{
	const __be32	*addrp;
	u64		size;
	unsigned int	flags;

	addrp = of_get_pci_address(dev, bar, &size, &flags);
	if (addrp == NULL)
		return -EINVAL;
	return __of_address_to_resource(dev, addrp, size, flags, NULL, r);
}
EXPORT_SYMBOL_GPL(of_pci_address_to_resource);
#endif 


static int of_bus_isa_match(struct device_node *np)
{
	return !strcmp(np->name, "isa");
}

static void of_bus_isa_count_cells(struct device_node *child,
				   int *addrc, int *sizec)
{
	if (addrc)
		*addrc = 2;
	if (sizec)
		*sizec = 1;
}

static u64 of_bus_isa_map(u32 *addr, const __be32 *range, int na, int ns,
		int pna)
{
	u64 cp, s, da;

	
	if ((addr[0] ^ range[0]) & cpu_to_be32(1))
		return OF_BAD_ADDR;

	
	cp = of_read_number(range + 1, na - 1);
	s  = of_read_number(range + na + pna, ns);
	da = of_read_number(addr + 1, na - 1);

	pr_debug("OF: ISA map, cp=%llx, s=%llx, da=%llx\n",
		 (unsigned long long)cp, (unsigned long long)s,
		 (unsigned long long)da);

	if (da < cp || da >= (cp + s))
		return OF_BAD_ADDR;
	return da - cp;
}

static int of_bus_isa_translate(u32 *addr, u64 offset, int na)
{
	return of_bus_default_translate(addr + 1, offset, na - 1);
}

static unsigned int of_bus_isa_get_flags(const __be32 *addr)
{
	unsigned int flags = 0;
	u32 w = be32_to_cpup(addr);

	if (w & 1)
		flags |= IORESOURCE_IO;
	else
		flags |= IORESOURCE_MEM;
	return flags;
}


static struct of_bus of_busses[] = {
#ifdef CONFIG_PCI
	
	{
		.name = "pci",
		.addresses = "assigned-addresses",
		.match = of_bus_pci_match,
		.count_cells = of_bus_pci_count_cells,
		.map = of_bus_pci_map,
		.translate = of_bus_pci_translate,
		.get_flags = of_bus_pci_get_flags,
	},
#endif 
	
	{
		.name = "isa",
		.addresses = "reg",
		.match = of_bus_isa_match,
		.count_cells = of_bus_isa_count_cells,
		.map = of_bus_isa_map,
		.translate = of_bus_isa_translate,
		.get_flags = of_bus_isa_get_flags,
	},
	
	{
		.name = "default",
		.addresses = "reg",
		.match = NULL,
		.count_cells = of_bus_default_count_cells,
		.map = of_bus_default_map,
		.translate = of_bus_default_translate,
		.get_flags = of_bus_default_get_flags,
	},
};

static struct of_bus *of_match_bus(struct device_node *np)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(of_busses); i++)
		if (!of_busses[i].match || of_busses[i].match(np))
			return &of_busses[i];
	BUG();
	return NULL;
}

static int of_translate_one(struct device_node *parent, struct of_bus *bus,
			    struct of_bus *pbus, u32 *addr,
			    int na, int ns, int pna, const char *rprop)
{
	const __be32 *ranges;
	unsigned int rlen;
	int rone;
	u64 offset = OF_BAD_ADDR;

	ranges = of_get_property(parent, rprop, &rlen);
#if !defined(CONFIG_PPC)
	if (ranges == NULL) {
		pr_err("OF: no ranges; cannot translate\n");
		return 1;
	}
#endif 
	if (ranges == NULL || rlen == 0) {
		offset = of_read_number(addr, na);
		memset(addr, 0, pna * 4);
		pr_debug("OF: empty ranges; 1:1 translation\n");
		goto finish;
	}

	pr_debug("OF: walking ranges...\n");

	
	rlen /= 4;
	rone = na + pna + ns;
	for (; rlen >= rone; rlen -= rone, ranges += rone) {
		offset = bus->map(addr, ranges, na, ns, pna);
		if (offset != OF_BAD_ADDR)
			break;
	}
	if (offset == OF_BAD_ADDR) {
		pr_debug("OF: not found !\n");
		return 1;
	}
	memcpy(addr, ranges + na, 4 * pna);

 finish:
	of_dump_addr("OF: parent translation for:", addr, pna);
	pr_debug("OF: with offset: %llx\n", (unsigned long long)offset);

	
	return pbus->translate(addr, offset, pna);
}

u64 __of_translate_address(struct device_node *dev, const __be32 *in_addr,
			   const char *rprop)
{
	struct device_node *parent = NULL;
	struct of_bus *bus, *pbus;
	u32 addr[OF_MAX_ADDR_CELLS];
	int na, ns, pna, pns;
	u64 result = OF_BAD_ADDR;

	pr_debug("OF: ** translation for device %s **\n", dev->full_name);

	
	of_node_get(dev);

	
	parent = of_get_parent(dev);
	if (parent == NULL)
		goto bail;
	bus = of_match_bus(parent);

	
	bus->count_cells(dev, &na, &ns);
	if (!OF_CHECK_COUNTS(na, ns)) {
		printk(KERN_ERR "prom_parse: Bad cell count for %s\n",
		       dev->full_name);
		goto bail;
	}
	memcpy(addr, in_addr, na * 4);

	pr_debug("OF: bus is %s (na=%d, ns=%d) on %s\n",
	    bus->name, na, ns, parent->full_name);
	of_dump_addr("OF: translating address:", addr, na);

	
	for (;;) {
		
		of_node_put(dev);
		dev = parent;
		parent = of_get_parent(dev);

		
		if (parent == NULL) {
			pr_debug("OF: reached root node\n");
			result = of_read_number(addr, na);
			break;
		}

		
		pbus = of_match_bus(parent);
		pbus->count_cells(dev, &pna, &pns);
		if (!OF_CHECK_COUNTS(pna, pns)) {
			printk(KERN_ERR "prom_parse: Bad cell count for %s\n",
			       dev->full_name);
			break;
		}

		pr_debug("OF: parent bus is %s (na=%d, ns=%d) on %s\n",
		    pbus->name, pna, pns, parent->full_name);

		
		if (of_translate_one(dev, bus, pbus, addr, na, ns, pna, rprop))
			break;

		
		na = pna;
		ns = pns;
		bus = pbus;

		of_dump_addr("OF: one level translation:", addr, na);
	}
 bail:
	of_node_put(parent);
	of_node_put(dev);

	return result;
}

u64 of_translate_address(struct device_node *dev, const __be32 *in_addr)
{
	return __of_translate_address(dev, in_addr, "ranges");
}
EXPORT_SYMBOL(of_translate_address);

u64 of_translate_dma_address(struct device_node *dev, const __be32 *in_addr)
{
	return __of_translate_address(dev, in_addr, "dma-ranges");
}
EXPORT_SYMBOL(of_translate_dma_address);

bool of_can_translate_address(struct device_node *dev)
{
	struct device_node *parent;
	struct of_bus *bus;
	int na, ns;

	parent = of_get_parent(dev);
	if (parent == NULL)
		return false;

	bus = of_match_bus(parent);
	bus->count_cells(dev, &na, &ns);

	of_node_put(parent);

	return OF_CHECK_COUNTS(na, ns);
}
EXPORT_SYMBOL(of_can_translate_address);

const __be32 *of_get_address(struct device_node *dev, int index, u64 *size,
		    unsigned int *flags)
{
	const __be32 *prop;
	unsigned int psize;
	struct device_node *parent;
	struct of_bus *bus;
	int onesize, i, na, ns;

	
	parent = of_get_parent(dev);
	if (parent == NULL)
		return NULL;
	bus = of_match_bus(parent);
	bus->count_cells(dev, &na, &ns);
	of_node_put(parent);
	if (!OF_CHECK_ADDR_COUNT(na))
		return NULL;

	
	prop = of_get_property(dev, bus->addresses, &psize);
	if (prop == NULL)
		return NULL;
	psize /= 4;

	onesize = na + ns;
	for (i = 0; psize >= onesize; psize -= onesize, prop += onesize, i++)
		if (i == index) {
			if (size)
				*size = of_read_number(prop + na, ns);
			if (flags)
				*flags = bus->get_flags(prop);
			return prop;
		}
	return NULL;
}
EXPORT_SYMBOL(of_get_address);

static int __of_address_to_resource(struct device_node *dev,
		const __be32 *addrp, u64 size, unsigned int flags,
		const char *name, struct resource *r)
{
	u64 taddr;

	if ((flags & (IORESOURCE_IO | IORESOURCE_MEM)) == 0)
		return -EINVAL;
	taddr = of_translate_address(dev, addrp);
	if (taddr == OF_BAD_ADDR)
		return -EINVAL;
	memset(r, 0, sizeof(struct resource));
	if (flags & IORESOURCE_IO) {
		unsigned long port;
		port = pci_address_to_pio(taddr);
		if (port == (unsigned long)-1)
			return -EINVAL;
		r->start = port;
		r->end = port + size - 1;
	} else {
		r->start = taddr;
		r->end = taddr + size - 1;
	}
	r->flags = flags;
	r->name = name ? name : dev->full_name;

	return 0;
}

int of_address_to_resource(struct device_node *dev, int index,
			   struct resource *r)
{
	const __be32	*addrp;
	u64		size;
	unsigned int	flags;
	const char	*name = NULL;

	addrp = of_get_address(dev, index, &size, &flags);
	if (addrp == NULL)
		return -EINVAL;

	
	of_property_read_string_index(dev, "reg-names",	index, &name);

	return __of_address_to_resource(dev, addrp, size, flags, name, r);
}
EXPORT_SYMBOL_GPL(of_address_to_resource);

struct device_node *of_find_matching_node_by_address(struct device_node *from,
					const struct of_device_id *matches,
					u64 base_address)
{
	struct device_node *dn = of_find_matching_node(from, matches);
	struct resource res;

	while (dn) {
		if (of_address_to_resource(dn, 0, &res))
			continue;
		if (res.start == base_address)
			return dn;
		dn = of_find_matching_node(dn, matches);
	}

	return NULL;
}


void __iomem *of_iomap(struct device_node *np, int index)
{
	struct resource res;

	if (of_address_to_resource(np, index, &res))
		return NULL;

	return ioremap(res.start, resource_size(&res));
}
EXPORT_SYMBOL(of_iomap);
