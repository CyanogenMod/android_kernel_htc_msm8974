/* Copyright (c) 2002-3 Patrick Mochel
 * Copyright (c) 2002-3 Open Source Development Labs
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Resource handling based on platform.c.
 */

#include <linux/export.h>
#include <linux/spmi.h>
#include <linux/string.h>

struct resource *spmi_get_resource(struct spmi_device *dev,
				   struct spmi_resource *node,
				   unsigned int type, unsigned int res_num)
{
	int i;

	
	if (!node)
		node = &dev->res;

	for (i = 0; i < node->num_resources; i++) {
		struct resource *r = &node->resource[i];

		if (type == resource_type(r) && res_num-- == 0)
			return r;
	}
	return NULL;
}
EXPORT_SYMBOL_GPL(spmi_get_resource);

#define SPMI_MAX_RES_NAME 256

struct resource *spmi_get_resource_byname(struct spmi_device *dev,
					  struct spmi_resource *node,
					  unsigned int type,
					  const char *name)
{
	int i;

	
	if (!node)
		node = &dev->res;

	for (i = 0; i < node->num_resources; i++) {
		struct resource *r = &node->resource[i];

		if (type == resource_type(r) && r->name &&
				!strncmp(r->name, name, SPMI_MAX_RES_NAME))
			return r;
	}
	return NULL;
}
EXPORT_SYMBOL_GPL(spmi_get_resource_byname);

int spmi_get_irq(struct spmi_device *dev, struct spmi_resource *node,
					  unsigned int res_num)
{
	struct resource *r = spmi_get_resource(dev, node,
						IORESOURCE_IRQ, res_num);

	return r ? r->start : -ENXIO;
}
EXPORT_SYMBOL_GPL(spmi_get_irq);

int spmi_get_irq_byname(struct spmi_device *dev,
			struct spmi_resource *node, const char *name)
{
	struct resource *r = spmi_get_resource_byname(dev, node,
							IORESOURCE_IRQ, name);
	return r ? r->start : -ENXIO;
}
EXPORT_SYMBOL_GPL(spmi_get_irq_byname);

struct spmi_resource *spmi_get_dev_container_byname(struct spmi_device *dev,
						    const char *label)
{
	int i;

	if (!label)
		return NULL;

	for (i = 0; i < dev->num_dev_node; i++) {
		struct spmi_resource *r = &dev->dev_node[i];

		if (r && r->label && !strncmp(r->label,
					label, SPMI_MAX_RES_NAME))
			return r;
	}
	return NULL;
}
EXPORT_SYMBOL(spmi_get_dev_container_byname);
