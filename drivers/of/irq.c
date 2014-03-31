/*
 *  Derived from arch/i386/kernel/irq.c
 *    Copyright (C) 1992 Linus Torvalds
 *  Adapted from arch/i386 by Gary Thomas
 *    Copyright (C) 1995-1996 Gary Thomas (gdt@linuxppc.org)
 *  Updated and modified by Cort Dougan <cort@fsmlabs.com>
 *    Copyright (C) 1996-2001 Cort Dougan
 *  Adapted for Power Macintosh by Paul Mackerras
 *    Copyright (C) 1996 Paul Mackerras (paulus@cs.anu.edu.au)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * This file contains the code used to make IRQ descriptions in the
 * device tree to actual irq numbers on an interrupt controller
 * driver.
 */

#include <linux/errno.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/string.h>
#include <linux/slab.h>

unsigned int irq_of_parse_and_map(struct device_node *dev, int index)
{
	struct of_irq oirq;

	if (of_irq_map_one(dev, index, &oirq))
		return 0;

	return irq_create_of_mapping(oirq.controller, oirq.specifier,
				     oirq.size);
}
EXPORT_SYMBOL_GPL(irq_of_parse_and_map);

struct device_node *of_irq_find_parent(struct device_node *child)
{
	struct device_node *p;
	const __be32 *parp;

	if (!of_node_get(child))
		return NULL;

	do {
		parp = of_get_property(child, "interrupt-parent", NULL);
		if (parp == NULL)
			p = of_get_parent(child);
		else {
			if (of_irq_workarounds & OF_IMAP_NO_PHANDLE)
				p = of_node_get(of_irq_dflt_pic);
			else
				p = of_find_node_by_phandle(be32_to_cpup(parp));
		}
		of_node_put(child);
		child = p;
	} while (p && of_get_property(p, "#interrupt-cells", NULL) == NULL);

	return p;
}

int of_irq_map_raw(struct device_node *parent, const __be32 *intspec,
		   u32 ointsize, const __be32 *addr, struct of_irq *out_irq)
{
	struct device_node *ipar, *tnode, *old = NULL, *newpar = NULL;
	const __be32 *tmp, *imap, *imask;
	u32 intsize = 1, addrsize, newintsize = 0, newaddrsize = 0;
	int imaplen, match, i;

	pr_debug("of_irq_map_raw: par=%s,intspec=[0x%08x 0x%08x...],ointsize=%d\n",
		 parent->full_name, be32_to_cpup(intspec),
		 be32_to_cpup(intspec + 1), ointsize);

	ipar = of_node_get(parent);

	do {
		tmp = of_get_property(ipar, "#interrupt-cells", NULL);
		if (tmp != NULL) {
			intsize = be32_to_cpu(*tmp);
			break;
		}
		tnode = ipar;
		ipar = of_irq_find_parent(ipar);
		of_node_put(tnode);
	} while (ipar);
	if (ipar == NULL) {
		pr_debug(" -> no parent found !\n");
		goto fail;
	}

	pr_debug("of_irq_map_raw: ipar=%s, size=%d\n", ipar->full_name, intsize);

	if (ointsize != intsize)
		return -EINVAL;

	old = of_node_get(ipar);
	do {
		tmp = of_get_property(old, "#address-cells", NULL);
		tnode = of_get_parent(old);
		of_node_put(old);
		old = tnode;
	} while (old && tmp == NULL);
	of_node_put(old);
	old = NULL;
	addrsize = (tmp == NULL) ? 2 : be32_to_cpu(*tmp);

	pr_debug(" -> addrsize=%d\n", addrsize);

	
	while (ipar != NULL) {
		if (of_get_property(ipar, "interrupt-controller", NULL) !=
				NULL) {
			pr_debug(" -> got it !\n");
			for (i = 0; i < intsize; i++)
				out_irq->specifier[i] =
						of_read_number(intspec +i, 1);
			out_irq->size = intsize;
			out_irq->controller = ipar;
			of_node_put(old);
			return 0;
		}

		
		imap = of_get_property(ipar, "interrupt-map", &imaplen);
		
		if (imap == NULL) {
			pr_debug(" -> no map, getting parent\n");
			newpar = of_irq_find_parent(ipar);
			goto skiplevel;
		}
		imaplen /= sizeof(u32);

		
		imask = of_get_property(ipar, "interrupt-map-mask", NULL);

		if (addr == NULL && addrsize != 0) {
			pr_debug(" -> no reg passed in when needed !\n");
			goto fail;
		}

		
		match = 0;
		while (imaplen > (addrsize + intsize + 1) && !match) {
			
			match = 1;
			for (i = 0; i < addrsize && match; ++i) {
				u32 mask = imask ? imask[i] : 0xffffffffu;
				match = ((addr[i] ^ imap[i]) & mask) == 0;
			}
			for (; i < (addrsize + intsize) && match; ++i) {
				u32 mask = imask ? imask[i] : 0xffffffffu;
				match =
				   ((intspec[i-addrsize] ^ imap[i]) & mask) == 0;
			}
			imap += addrsize + intsize;
			imaplen -= addrsize + intsize;

			pr_debug(" -> match=%d (imaplen=%d)\n", match, imaplen);

			
			if (of_irq_workarounds & OF_IMAP_NO_PHANDLE)
				newpar = of_node_get(of_irq_dflt_pic);
			else
				newpar = of_find_node_by_phandle(be32_to_cpup(imap));
			imap++;
			--imaplen;

			
			if (newpar == NULL) {
				pr_debug(" -> imap parent not found !\n");
				goto fail;
			}

			tmp = of_get_property(newpar, "#interrupt-cells", NULL);
			if (tmp == NULL) {
				pr_debug(" -> parent lacks #interrupt-cells!\n");
				goto fail;
			}
			newintsize = be32_to_cpu(*tmp);
			tmp = of_get_property(newpar, "#address-cells", NULL);
			newaddrsize = (tmp == NULL) ? 0 : be32_to_cpu(*tmp);

			pr_debug(" -> newintsize=%d, newaddrsize=%d\n",
			    newintsize, newaddrsize);

			
			if (imaplen < (newaddrsize + newintsize))
				goto fail;

			imap += newaddrsize + newintsize;
			imaplen -= newaddrsize + newintsize;

			pr_debug(" -> imaplen=%d\n", imaplen);
		}
		if (!match)
			goto fail;

		of_node_put(old);
		old = of_node_get(newpar);
		addrsize = newaddrsize;
		intsize = newintsize;
		intspec = imap - intsize;
		addr = intspec - addrsize;

	skiplevel:
		
		pr_debug(" -> new parent: %s\n", newpar ? newpar->full_name : "<>");
		of_node_put(ipar);
		ipar = newpar;
		newpar = NULL;
	}
 fail:
	of_node_put(ipar);
	of_node_put(old);
	of_node_put(newpar);

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(of_irq_map_raw);

int of_irq_map_one(struct device_node *device, int index, struct of_irq *out_irq)
{
	struct device_node *p;
	const __be32 *intspec, *tmp, *addr;
	u32 intsize, intlen;
	int res = -EINVAL;

	pr_debug("of_irq_map_one: dev=%s, index=%d\n", device->full_name, index);

	
	if (of_irq_workarounds & OF_IMAP_OLDWORLD_MAC)
		return of_irq_map_oldworld(device, index, out_irq);

	
	intspec = of_get_property(device, "interrupts", &intlen);
	if (intspec == NULL)
		return -EINVAL;
	intlen /= sizeof(*intspec);

	pr_debug(" intspec=%d intlen=%d\n", be32_to_cpup(intspec), intlen);

	
	addr = of_get_property(device, "reg", NULL);

	
	p = of_irq_find_parent(device);
	if (p == NULL)
		return -EINVAL;

	
	tmp = of_get_property(p, "#interrupt-cells", NULL);
	if (tmp == NULL)
		goto out;
	intsize = be32_to_cpu(*tmp);

	pr_debug(" intsize=%d intlen=%d\n", intsize, intlen);

	
	if ((index + 1) * intsize > intlen)
		goto out;

	
	res = of_irq_map_raw(p, intspec + index * intsize, intsize,
			     addr, out_irq);
 out:
	of_node_put(p);
	return res;
}
EXPORT_SYMBOL_GPL(of_irq_map_one);

int of_irq_to_resource(struct device_node *dev, int index, struct resource *r)
{
	int irq = irq_of_parse_and_map(dev, index);

	if (r && irq) {
		const char *name = NULL;

		of_property_read_string_index(dev, "interrupt-names", index,
					      &name);

		r->start = r->end = irq;
		r->flags = IORESOURCE_IRQ;
		r->name = name ? name : dev->full_name;
	}

	return irq;
}
EXPORT_SYMBOL_GPL(of_irq_to_resource);

int of_irq_count(struct device_node *dev)
{
	int nr = 0;

	while (of_irq_to_resource(dev, nr, NULL))
		nr++;

	return nr;
}

int of_irq_to_resource_table(struct device_node *dev, struct resource *res,
		int nr_irqs)
{
	int i;

	for (i = 0; i < nr_irqs; i++, res++)
		if (!of_irq_to_resource(dev, i, res))
			break;

	return i;
}

struct intc_desc {
	struct list_head	list;
	struct device_node	*dev;
	struct device_node	*interrupt_parent;
};

void __init of_irq_init(const struct of_device_id *matches)
{
	struct device_node *np, *parent = NULL;
	struct intc_desc *desc, *temp_desc;
	struct list_head intc_desc_list, intc_parent_list;

	INIT_LIST_HEAD(&intc_desc_list);
	INIT_LIST_HEAD(&intc_parent_list);

	for_each_matching_node(np, matches) {
		if (!of_find_property(np, "interrupt-controller", NULL))
			continue;
		desc = kzalloc(sizeof(*desc), GFP_KERNEL);
		if (WARN_ON(!desc))
			goto err;

		desc->dev = np;
		desc->interrupt_parent = of_irq_find_parent(np);
		if (desc->interrupt_parent == np)
			desc->interrupt_parent = NULL;
		list_add_tail(&desc->list, &intc_desc_list);
	}

	while (!list_empty(&intc_desc_list)) {
		list_for_each_entry_safe(desc, temp_desc, &intc_desc_list, list) {
			const struct of_device_id *match;
			int ret;
			of_irq_init_cb_t irq_init_cb;

			if (desc->interrupt_parent != parent)
				continue;

			list_del(&desc->list);
			match = of_match_node(matches, desc->dev);
			if (unlikely(!match)) {
				pr_debug("of_irq_init: match is null\n");
				kfree(desc);
				continue;
			}
			
			if (WARN(!match->data,
			    "of_irq_init: no init function for %s\n",
			    match->compatible)) {
				kfree(desc);
				continue;
			}

			pr_debug("of_irq_init: init %s @ %p, parent %p\n",
				 match->compatible,
				 desc->dev, desc->interrupt_parent);
			irq_init_cb = match->data;
			ret = irq_init_cb(desc->dev, desc->interrupt_parent);
			if (ret) {
				kfree(desc);
				continue;
			}

			list_add_tail(&desc->list, &intc_parent_list);
		}

		
		desc = list_first_entry(&intc_parent_list, typeof(*desc), list);
		if (list_empty(&intc_parent_list) || !desc) {
			pr_err("of_irq_init: children remain, but no parents\n");
			break;
		}
		list_del(&desc->list);
		parent = desc->dev;
		kfree(desc);
	}

	list_for_each_entry_safe(desc, temp_desc, &intc_parent_list, list) {
		list_del(&desc->list);
		kfree(desc);
	}
err:
	list_for_each_entry_safe(desc, temp_desc, &intc_desc_list, list) {
		list_del(&desc->list);
		kfree(desc);
	}
}
