/*
 * resource.c - Contains functions for registering and analyzing resource information
 *
 * based on isapnp.c resource management (c) Jaroslav Kysela <perex@perex.cz>
 * Copyright 2003 Adam Belay <ambx1@neo.rr.com>
 * Copyright (C) 2008 Hewlett-Packard Development Company, L.P.
 *	Bjorn Helgaas <bjorn.helgaas@hp.com>
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <asm/irq.h>
#include <linux/pci.h>
#include <linux/ioport.h>
#include <linux/init.h>

#include <linux/pnp.h>
#include "base.h"

static int pnp_reserve_irq[16] = {[0 ... 15] = -1 };	
static int pnp_reserve_dma[8] = {[0 ... 7] = -1 };	
static int pnp_reserve_io[16] = {[0 ... 15] = -1 };	
static int pnp_reserve_mem[16] = {[0 ... 15] = -1 };	


struct pnp_option *pnp_build_option(struct pnp_dev *dev, unsigned long type,
				    unsigned int option_flags)
{
	struct pnp_option *option;

	option = kzalloc(sizeof(struct pnp_option), GFP_KERNEL);
	if (!option)
		return NULL;

	option->flags = option_flags;
	option->type = type;

	list_add_tail(&option->list, &dev->options);
	return option;
}

int pnp_register_irq_resource(struct pnp_dev *dev, unsigned int option_flags,
			      pnp_irq_mask_t *map, unsigned char flags)
{
	struct pnp_option *option;
	struct pnp_irq *irq;

	option = pnp_build_option(dev, IORESOURCE_IRQ, option_flags);
	if (!option)
		return -ENOMEM;

	irq = &option->u.irq;
	irq->map = *map;
	irq->flags = flags;

#ifdef CONFIG_PCI
	{
		int i;

		for (i = 0; i < 16; i++)
			if (test_bit(i, irq->map.bits))
				pcibios_penalize_isa_irq(i, 0);
	}
#endif

	dbg_pnp_show_option(dev, option);
	return 0;
}

int pnp_register_dma_resource(struct pnp_dev *dev, unsigned int option_flags,
			      unsigned char map, unsigned char flags)
{
	struct pnp_option *option;
	struct pnp_dma *dma;

	option = pnp_build_option(dev, IORESOURCE_DMA, option_flags);
	if (!option)
		return -ENOMEM;

	dma = &option->u.dma;
	dma->map = map;
	dma->flags = flags;

	dbg_pnp_show_option(dev, option);
	return 0;
}

int pnp_register_port_resource(struct pnp_dev *dev, unsigned int option_flags,
			       resource_size_t min, resource_size_t max,
			       resource_size_t align, resource_size_t size,
			       unsigned char flags)
{
	struct pnp_option *option;
	struct pnp_port *port;

	option = pnp_build_option(dev, IORESOURCE_IO, option_flags);
	if (!option)
		return -ENOMEM;

	port = &option->u.port;
	port->min = min;
	port->max = max;
	port->align = align;
	port->size = size;
	port->flags = flags;

	dbg_pnp_show_option(dev, option);
	return 0;
}

int pnp_register_mem_resource(struct pnp_dev *dev, unsigned int option_flags,
			      resource_size_t min, resource_size_t max,
			      resource_size_t align, resource_size_t size,
			      unsigned char flags)
{
	struct pnp_option *option;
	struct pnp_mem *mem;

	option = pnp_build_option(dev, IORESOURCE_MEM, option_flags);
	if (!option)
		return -ENOMEM;

	mem = &option->u.mem;
	mem->min = min;
	mem->max = max;
	mem->align = align;
	mem->size = size;
	mem->flags = flags;

	dbg_pnp_show_option(dev, option);
	return 0;
}

void pnp_free_options(struct pnp_dev *dev)
{
	struct pnp_option *option, *tmp;

	list_for_each_entry_safe(option, tmp, &dev->options, list) {
		list_del(&option->list);
		kfree(option);
	}
}


#define length(start, end) (*(end) - *(start) + 1)

#define ranged_conflict(starta, enda, startb, endb) \
	!((*(enda) < *(startb)) || (*(endb) < *(starta)))

#define cannot_compare(flags) \
((flags) & IORESOURCE_DISABLED)

int pnp_check_port(struct pnp_dev *dev, struct resource *res)
{
	int i;
	struct pnp_dev *tdev;
	struct resource *tres;
	resource_size_t *port, *end, *tport, *tend;

	port = &res->start;
	end = &res->end;

	
	if (cannot_compare(res->flags))
		return 1;

	if (!dev->active) {
		if (__check_region(&ioport_resource, *port, length(port, end)))
			return 0;
	}

	
	for (i = 0; i < 8; i++) {
		int rport = pnp_reserve_io[i << 1];
		int rend = pnp_reserve_io[(i << 1) + 1] + rport - 1;
		if (ranged_conflict(port, end, &rport, &rend))
			return 0;
	}

	
	for (i = 0; (tres = pnp_get_resource(dev, IORESOURCE_IO, i)); i++) {
		if (tres != res && tres->flags & IORESOURCE_IO) {
			tport = &tres->start;
			tend = &tres->end;
			if (ranged_conflict(port, end, tport, tend))
				return 0;
		}
	}

	
	pnp_for_each_dev(tdev) {
		if (tdev == dev)
			continue;
		for (i = 0;
		     (tres = pnp_get_resource(tdev, IORESOURCE_IO, i));
		     i++) {
			if (tres->flags & IORESOURCE_IO) {
				if (cannot_compare(tres->flags))
					continue;
				if (tres->flags & IORESOURCE_WINDOW)
					continue;
				tport = &tres->start;
				tend = &tres->end;
				if (ranged_conflict(port, end, tport, tend))
					return 0;
			}
		}
	}

	return 1;
}

int pnp_check_mem(struct pnp_dev *dev, struct resource *res)
{
	int i;
	struct pnp_dev *tdev;
	struct resource *tres;
	resource_size_t *addr, *end, *taddr, *tend;

	addr = &res->start;
	end = &res->end;

	
	if (cannot_compare(res->flags))
		return 1;

	if (!dev->active) {
		if (check_mem_region(*addr, length(addr, end)))
			return 0;
	}

	
	for (i = 0; i < 8; i++) {
		int raddr = pnp_reserve_mem[i << 1];
		int rend = pnp_reserve_mem[(i << 1) + 1] + raddr - 1;
		if (ranged_conflict(addr, end, &raddr, &rend))
			return 0;
	}

	
	for (i = 0; (tres = pnp_get_resource(dev, IORESOURCE_MEM, i)); i++) {
		if (tres != res && tres->flags & IORESOURCE_MEM) {
			taddr = &tres->start;
			tend = &tres->end;
			if (ranged_conflict(addr, end, taddr, tend))
				return 0;
		}
	}

	
	pnp_for_each_dev(tdev) {
		if (tdev == dev)
			continue;
		for (i = 0;
		     (tres = pnp_get_resource(tdev, IORESOURCE_MEM, i));
		     i++) {
			if (tres->flags & IORESOURCE_MEM) {
				if (cannot_compare(tres->flags))
					continue;
				if (tres->flags & IORESOURCE_WINDOW)
					continue;
				taddr = &tres->start;
				tend = &tres->end;
				if (ranged_conflict(addr, end, taddr, tend))
					return 0;
			}
		}
	}

	return 1;
}

static irqreturn_t pnp_test_handler(int irq, void *dev_id)
{
	return IRQ_HANDLED;
}

#ifdef CONFIG_PCI
static int pci_dev_uses_irq(struct pnp_dev *pnp, struct pci_dev *pci,
			    unsigned int irq)
{
	u32 class;
	u8 progif;

	if (pci->irq == irq) {
		pnp_dbg(&pnp->dev, "  device %s using irq %d\n",
			pci_name(pci), irq);
		return 1;
	}

	pci_read_config_dword(pci, PCI_CLASS_REVISION, &class);
	class >>= 8;		
	progif = class & 0xff;
	class >>= 8;

	if (class == PCI_CLASS_STORAGE_IDE) {
		if ((progif & 0x5) != 0x5)
			if (pci_get_legacy_ide_irq(pci, 0) == irq ||
			    pci_get_legacy_ide_irq(pci, 1) == irq) {
				pnp_dbg(&pnp->dev, "  legacy IDE device %s "
					"using irq %d\n", pci_name(pci), irq);
				return 1;
			}
	}

	return 0;
}
#endif

static int pci_uses_irq(struct pnp_dev *pnp, unsigned int irq)
{
#ifdef CONFIG_PCI
	struct pci_dev *pci = NULL;

	for_each_pci_dev(pci) {
		if (pci_dev_uses_irq(pnp, pci, irq)) {
			pci_dev_put(pci);
			return 1;
		}
	}
#endif
	return 0;
}

int pnp_check_irq(struct pnp_dev *dev, struct resource *res)
{
	int i;
	struct pnp_dev *tdev;
	struct resource *tres;
	resource_size_t *irq;

	irq = &res->start;

	
	if (cannot_compare(res->flags))
		return 1;

	
	if (*irq < 0 || *irq > 15)
		return 0;

	
	for (i = 0; i < 16; i++) {
		if (pnp_reserve_irq[i] == *irq)
			return 0;
	}

	
	for (i = 0; (tres = pnp_get_resource(dev, IORESOURCE_IRQ, i)); i++) {
		if (tres != res && tres->flags & IORESOURCE_IRQ) {
			if (tres->start == *irq)
				return 0;
		}
	}

	
	if (pci_uses_irq(dev, *irq))
		return 0;

	if (!dev->active) {
		if (request_irq(*irq, pnp_test_handler,
				IRQF_DISABLED | IRQF_PROBE_SHARED, "pnp", NULL))
			return 0;
		free_irq(*irq, NULL);
	}

	
	pnp_for_each_dev(tdev) {
		if (tdev == dev)
			continue;
		for (i = 0;
		     (tres = pnp_get_resource(tdev, IORESOURCE_IRQ, i));
		     i++) {
			if (tres->flags & IORESOURCE_IRQ) {
				if (cannot_compare(tres->flags))
					continue;
				if (tres->start == *irq)
					return 0;
			}
		}
	}

	return 1;
}

#ifdef CONFIG_ISA_DMA_API
int pnp_check_dma(struct pnp_dev *dev, struct resource *res)
{
	int i;
	struct pnp_dev *tdev;
	struct resource *tres;
	resource_size_t *dma;

	dma = &res->start;

	
	if (cannot_compare(res->flags))
		return 1;

	
	if (*dma < 0 || *dma == 4 || *dma > 7)
		return 0;

	
	for (i = 0; i < 8; i++) {
		if (pnp_reserve_dma[i] == *dma)
			return 0;
	}

	
	for (i = 0; (tres = pnp_get_resource(dev, IORESOURCE_DMA, i)); i++) {
		if (tres != res && tres->flags & IORESOURCE_DMA) {
			if (tres->start == *dma)
				return 0;
		}
	}

	if (!dev->active) {
		if (request_dma(*dma, "pnp"))
			return 0;
		free_dma(*dma);
	}

	
	pnp_for_each_dev(tdev) {
		if (tdev == dev)
			continue;
		for (i = 0;
		     (tres = pnp_get_resource(tdev, IORESOURCE_DMA, i));
		     i++) {
			if (tres->flags & IORESOURCE_DMA) {
				if (cannot_compare(tres->flags))
					continue;
				if (tres->start == *dma)
					return 0;
			}
		}
	}

	return 1;
}
#endif 

unsigned long pnp_resource_type(struct resource *res)
{
	return res->flags & (IORESOURCE_IO  | IORESOURCE_MEM |
			     IORESOURCE_IRQ | IORESOURCE_DMA |
			     IORESOURCE_BUS);
}

struct resource *pnp_get_resource(struct pnp_dev *dev,
				  unsigned long type, unsigned int num)
{
	struct pnp_resource *pnp_res;
	struct resource *res;

	list_for_each_entry(pnp_res, &dev->resources, list) {
		res = &pnp_res->res;
		if (pnp_resource_type(res) == type && num-- == 0)
			return res;
	}
	return NULL;
}
EXPORT_SYMBOL(pnp_get_resource);

static struct pnp_resource *pnp_new_resource(struct pnp_dev *dev)
{
	struct pnp_resource *pnp_res;

	pnp_res = kzalloc(sizeof(struct pnp_resource), GFP_KERNEL);
	if (!pnp_res)
		return NULL;

	list_add_tail(&pnp_res->list, &dev->resources);
	return pnp_res;
}

struct pnp_resource *pnp_add_irq_resource(struct pnp_dev *dev, int irq,
					  int flags)
{
	struct pnp_resource *pnp_res;
	struct resource *res;

	pnp_res = pnp_new_resource(dev);
	if (!pnp_res) {
		dev_err(&dev->dev, "can't add resource for IRQ %d\n", irq);
		return NULL;
	}

	res = &pnp_res->res;
	res->flags = IORESOURCE_IRQ | flags;
	res->start = irq;
	res->end = irq;

	dev_printk(KERN_DEBUG, &dev->dev, "%pR\n", res);
	return pnp_res;
}

struct pnp_resource *pnp_add_dma_resource(struct pnp_dev *dev, int dma,
					  int flags)
{
	struct pnp_resource *pnp_res;
	struct resource *res;

	pnp_res = pnp_new_resource(dev);
	if (!pnp_res) {
		dev_err(&dev->dev, "can't add resource for DMA %d\n", dma);
		return NULL;
	}

	res = &pnp_res->res;
	res->flags = IORESOURCE_DMA | flags;
	res->start = dma;
	res->end = dma;

	dev_printk(KERN_DEBUG, &dev->dev, "%pR\n", res);
	return pnp_res;
}

struct pnp_resource *pnp_add_io_resource(struct pnp_dev *dev,
					 resource_size_t start,
					 resource_size_t end, int flags)
{
	struct pnp_resource *pnp_res;
	struct resource *res;

	pnp_res = pnp_new_resource(dev);
	if (!pnp_res) {
		dev_err(&dev->dev, "can't add resource for IO %#llx-%#llx\n",
			(unsigned long long) start,
			(unsigned long long) end);
		return NULL;
	}

	res = &pnp_res->res;
	res->flags = IORESOURCE_IO | flags;
	res->start = start;
	res->end = end;

	dev_printk(KERN_DEBUG, &dev->dev, "%pR\n", res);
	return pnp_res;
}

struct pnp_resource *pnp_add_mem_resource(struct pnp_dev *dev,
					  resource_size_t start,
					  resource_size_t end, int flags)
{
	struct pnp_resource *pnp_res;
	struct resource *res;

	pnp_res = pnp_new_resource(dev);
	if (!pnp_res) {
		dev_err(&dev->dev, "can't add resource for MEM %#llx-%#llx\n",
			(unsigned long long) start,
			(unsigned long long) end);
		return NULL;
	}

	res = &pnp_res->res;
	res->flags = IORESOURCE_MEM | flags;
	res->start = start;
	res->end = end;

	dev_printk(KERN_DEBUG, &dev->dev, "%pR\n", res);
	return pnp_res;
}

struct pnp_resource *pnp_add_bus_resource(struct pnp_dev *dev,
					  resource_size_t start,
					  resource_size_t end)
{
	struct pnp_resource *pnp_res;
	struct resource *res;

	pnp_res = pnp_new_resource(dev);
	if (!pnp_res) {
		dev_err(&dev->dev, "can't add resource for BUS %#llx-%#llx\n",
			(unsigned long long) start,
			(unsigned long long) end);
		return NULL;
	}

	res = &pnp_res->res;
	res->flags = IORESOURCE_BUS;
	res->start = start;
	res->end = end;

	dev_printk(KERN_DEBUG, &dev->dev, "%pR\n", res);
	return pnp_res;
}

int pnp_possible_config(struct pnp_dev *dev, int type, resource_size_t start,
			resource_size_t size)
{
	struct pnp_option *option;
	struct pnp_port *port;
	struct pnp_mem *mem;
	struct pnp_irq *irq;
	struct pnp_dma *dma;

	list_for_each_entry(option, &dev->options, list) {
		if (option->type != type)
			continue;

		switch (option->type) {
		case IORESOURCE_IO:
			port = &option->u.port;
			if (port->min == start && port->size == size)
				return 1;
			break;
		case IORESOURCE_MEM:
			mem = &option->u.mem;
			if (mem->min == start && mem->size == size)
				return 1;
			break;
		case IORESOURCE_IRQ:
			irq = &option->u.irq;
			if (start < PNP_IRQ_NR &&
			    test_bit(start, irq->map.bits))
				return 1;
			break;
		case IORESOURCE_DMA:
			dma = &option->u.dma;
			if (dma->map & (1 << start))
				return 1;
			break;
		}
	}

	return 0;
}
EXPORT_SYMBOL(pnp_possible_config);

int pnp_range_reserved(resource_size_t start, resource_size_t end)
{
	struct pnp_dev *dev;
	struct pnp_resource *pnp_res;
	resource_size_t *dev_start, *dev_end;

	pnp_for_each_dev(dev) {
		list_for_each_entry(pnp_res, &dev->resources, list) {
			dev_start = &pnp_res->res.start;
			dev_end   = &pnp_res->res.end;
			if (ranged_conflict(&start, &end, dev_start, dev_end))
				return 1;
		}
	}
	return 0;
}
EXPORT_SYMBOL(pnp_range_reserved);

static int __init pnp_setup_reserve_irq(char *str)
{
	int i;

	for (i = 0; i < 16; i++)
		if (get_option(&str, &pnp_reserve_irq[i]) != 2)
			break;
	return 1;
}

__setup("pnp_reserve_irq=", pnp_setup_reserve_irq);

static int __init pnp_setup_reserve_dma(char *str)
{
	int i;

	for (i = 0; i < 8; i++)
		if (get_option(&str, &pnp_reserve_dma[i]) != 2)
			break;
	return 1;
}

__setup("pnp_reserve_dma=", pnp_setup_reserve_dma);

static int __init pnp_setup_reserve_io(char *str)
{
	int i;

	for (i = 0; i < 16; i++)
		if (get_option(&str, &pnp_reserve_io[i]) != 2)
			break;
	return 1;
}

__setup("pnp_reserve_io=", pnp_setup_reserve_io);

static int __init pnp_setup_reserve_mem(char *str)
{
	int i;

	for (i = 0; i < 16; i++)
		if (get_option(&str, &pnp_reserve_mem[i]) != 2)
			break;
	return 1;
}

__setup("pnp_reserve_mem=", pnp_setup_reserve_mem);
