/*
 *    Copyright (C) 2006 Benjamin Herrenschmidt, IBM Corp.
 *			 <benh@kernel.crashing.org>
 *    and		 Arnd Bergmann, IBM Corp.
 *    Merged from powerpc/kernel/of_platform.c and
 *    sparc{,64}/kernel/of_device.c by Stephen Rothwell
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 *
 */
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/amba/bus.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

const struct of_device_id of_default_bus_match_table[] = {
	{ .compatible = "simple-bus", },
#ifdef CONFIG_ARM_AMBA
	{ .compatible = "arm,amba-bus", },
#endif 
	{} 
};

static int of_dev_node_match(struct device *dev, void *data)
{
	return dev->of_node == data;
}

struct platform_device *of_find_device_by_node(struct device_node *np)
{
	struct device *dev;

	dev = bus_find_device(&platform_bus_type, NULL, np, of_dev_node_match);
	return dev ? to_platform_device(dev) : NULL;
}
EXPORT_SYMBOL(of_find_device_by_node);

#if defined(CONFIG_PPC_DCR)
#include <asm/dcr.h>
#endif

#ifdef CONFIG_OF_ADDRESS

void of_device_make_bus_id(struct device *dev)
{
	static atomic_t bus_no_reg_magic;
	struct device_node *node = dev->of_node;
	const u32 *reg;
	u64 addr;
	const __be32 *addrp;
	int magic;

#ifdef CONFIG_PPC_DCR
	reg = of_get_property(node, "dcr-reg", NULL);
	if (reg) {
#ifdef CONFIG_PPC_DCR_NATIVE
		dev_set_name(dev, "d%x.%s", *reg, node->name);
#else 
		u64 addr = of_translate_dcr_address(node, *reg, NULL);
		if (addr != OF_BAD_ADDR) {
			dev_set_name(dev, "D%llx.%s",
				     (unsigned long long)addr, node->name);
			return;
		}
#endif 
	}
#endif 

	reg = of_get_property(node, "reg", NULL);
	if (reg) {
		if (of_can_translate_address(node)) {
			addr = of_translate_address(node, reg);
		} else {
			addrp = of_get_address(node, 0, NULL, NULL);
			if (addrp)
				addr = of_read_number(addrp, 1);
			else
				addr = OF_BAD_ADDR;
		}
		if (addr != OF_BAD_ADDR) {
			dev_set_name(dev, "%llx.%s",
				     (unsigned long long)addr, node->name);
			return;
		}
	}

	magic = atomic_add_return(1, &bus_no_reg_magic);
	dev_set_name(dev, "%s.%d", node->name, magic - 1);
}

struct platform_device *of_device_alloc(struct device_node *np,
				  const char *bus_id,
				  struct device *parent)
{
	struct platform_device *dev;
	int rc, i, num_reg = 0, num_irq;
	struct resource *res, temp_res;

	dev = platform_device_alloc("", -1);
	if (!dev)
		return NULL;

	
	if (of_can_translate_address(np))
		while (of_address_to_resource(np, num_reg, &temp_res) == 0)
			num_reg++;
	num_irq = of_irq_count(np);

	
	if (num_irq || num_reg) {
		res = kzalloc(sizeof(*res) * (num_irq + num_reg), GFP_KERNEL);
		if (!res) {
			platform_device_put(dev);
			return NULL;
		}

		dev->num_resources = num_reg + num_irq;
		dev->resource = res;
		for (i = 0; i < num_reg; i++, res++) {
			rc = of_address_to_resource(np, i, res);
			WARN_ON(rc);
		}
		WARN_ON(of_irq_to_resource_table(np, res, num_irq) != num_irq);
	}

	dev->dev.of_node = of_node_get(np);
#if defined(CONFIG_MICROBLAZE)
	dev->dev.dma_mask = &dev->archdata.dma_mask;
#endif
	dev->dev.parent = parent;

	if (bus_id)
		dev_set_name(&dev->dev, "%s", bus_id);
	else
		of_device_make_bus_id(&dev->dev);

	return dev;
}
EXPORT_SYMBOL(of_device_alloc);

struct platform_device *of_platform_device_create_pdata(
					struct device_node *np,
					const char *bus_id,
					void *platform_data,
					struct device *parent)
{
	struct platform_device *dev;

	if (!of_device_is_available(np))
		return NULL;

	dev = of_device_alloc(np, bus_id, parent);
	if (!dev)
		return NULL;

#if defined(CONFIG_MICROBLAZE)
	dev->archdata.dma_mask = 0xffffffffUL;
#endif
	dev->dev.coherent_dma_mask = DMA_BIT_MASK(sizeof(dma_addr_t) * 8);
	dev->dev.bus = &platform_bus_type;
	dev->dev.platform_data = platform_data;


	if (of_device_add(dev) != 0) {
		platform_device_put(dev);
		return NULL;
	}

	return dev;
}

struct platform_device *of_platform_device_create(struct device_node *np,
					    const char *bus_id,
					    struct device *parent)
{
	return of_platform_device_create_pdata(np, bus_id, NULL, parent);
}
EXPORT_SYMBOL(of_platform_device_create);

#ifdef CONFIG_ARM_AMBA
static struct amba_device *of_amba_device_create(struct device_node *node,
						 const char *bus_id,
						 void *platform_data,
						 struct device *parent)
{
	struct amba_device *dev;
	const void *prop;
	int i, ret;

	pr_debug("Creating amba device %s\n", node->full_name);

	if (!of_device_is_available(node))
		return NULL;

	dev = amba_device_alloc(NULL, 0, 0);
	if (!dev)
		return NULL;

	
	dev->dev.coherent_dma_mask = ~0;
	dev->dev.of_node = of_node_get(node);
	dev->dev.parent = parent;
	dev->dev.platform_data = platform_data;
	if (bus_id)
		dev_set_name(&dev->dev, "%s", bus_id);
	else
		of_device_make_bus_id(&dev->dev);

	
	dev->dma_mask = ~0;

	
	prop = of_get_property(node, "arm,primecell-periphid", NULL);
	if (prop)
		dev->periphid = of_read_ulong(prop, 1);

	
	for (i = 0; i < AMBA_NR_IRQS; i++)
		dev->irq[i] = irq_of_parse_and_map(node, i);

	ret = of_address_to_resource(node, 0, &dev->res);
	if (ret)
		goto err_free;

	ret = amba_device_add(dev, &iomem_resource);
	if (ret)
		goto err_free;

	return dev;

err_free:
	amba_device_put(dev);
	return NULL;
}
#else 
static struct amba_device *of_amba_device_create(struct device_node *node,
						 const char *bus_id,
						 void *platform_data,
						 struct device *parent)
{
	return NULL;
}
#endif 

static const struct of_dev_auxdata *of_dev_lookup(const struct of_dev_auxdata *lookup,
				 struct device_node *np)
{
	struct resource res;

	if (!lookup)
		return NULL;

	for(; lookup->compatible != NULL; lookup++) {
		if (!of_device_is_compatible(np, lookup->compatible))
			continue;
		if (!of_address_to_resource(np, 0, &res))
			if (res.start != lookup->phys_addr)
				continue;
		pr_debug("%s: devname=%s\n", np->full_name, lookup->name);
		return lookup;
	}

	return NULL;
}

static int of_platform_bus_create(struct device_node *bus,
				  const struct of_device_id *matches,
				  const struct of_dev_auxdata *lookup,
				  struct device *parent, bool strict)
{
	const struct of_dev_auxdata *auxdata;
	struct device_node *child;
	struct platform_device *dev;
	const char *bus_id = NULL;
	void *platform_data = NULL;
	int rc = 0;

	
	if (strict && (!of_get_property(bus, "compatible", NULL))) {
		pr_debug("%s() - skipping %s, no compatible prop\n",
			 __func__, bus->full_name);
		return 0;
	}

	auxdata = of_dev_lookup(lookup, bus);
	if (auxdata) {
		bus_id = auxdata->name;
		platform_data = auxdata->platform_data;
	}

	if (of_device_is_compatible(bus, "arm,primecell")) {
		of_amba_device_create(bus, bus_id, platform_data, parent);
		return 0;
	}

	dev = of_platform_device_create_pdata(bus, bus_id, platform_data, parent);
	if (!dev || !of_match_node(matches, bus))
		return 0;

	for_each_child_of_node(bus, child) {
		pr_debug("   create child: %s\n", child->full_name);
		rc = of_platform_bus_create(child, matches, lookup, &dev->dev, strict);
		if (rc) {
			of_node_put(child);
			break;
		}
	}
	return rc;
}

int of_platform_bus_probe(struct device_node *root,
			  const struct of_device_id *matches,
			  struct device *parent)
{
	struct device_node *child;
	int rc = 0;

	root = root ? of_node_get(root) : of_find_node_by_path("/");
	if (!root)
		return -EINVAL;

	pr_debug("of_platform_bus_probe()\n");
	pr_debug(" starting at: %s\n", root->full_name);

	
	if (of_match_node(matches, root)) {
		rc = of_platform_bus_create(root, matches, NULL, parent, false);
	} else for_each_child_of_node(root, child) {
		if (!of_match_node(matches, child))
			continue;
		rc = of_platform_bus_create(child, matches, NULL, parent, false);
		if (rc)
			break;
	}

	of_node_put(root);
	return rc;
}
EXPORT_SYMBOL(of_platform_bus_probe);

int of_platform_populate(struct device_node *root,
			const struct of_device_id *matches,
			const struct of_dev_auxdata *lookup,
			struct device *parent)
{
	struct device_node *child;
	int rc = 0;

	root = root ? of_node_get(root) : of_find_node_by_path("/");
	if (!root)
		return -EINVAL;

	for_each_child_of_node(root, child) {
		rc = of_platform_bus_create(child, matches, lookup, parent, true);
		if (rc)
			break;
	}

	of_node_put(root);
	return rc;
}
#endif 
