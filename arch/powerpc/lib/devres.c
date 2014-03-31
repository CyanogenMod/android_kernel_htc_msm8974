/*
 * Copyright (C) 2008 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/device.h>	
#include <linux/gfp.h>
#include <linux/io.h>		
#include <linux/export.h>	

void __iomem *devm_ioremap_prot(struct device *dev, resource_size_t offset,
				 size_t size, unsigned long flags)
{
	void __iomem **ptr, *addr;

	ptr = devres_alloc(devm_ioremap_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return NULL;

	addr = ioremap_prot(offset, size, flags);
	if (addr) {
		*ptr = addr;
		devres_add(dev, ptr);
	} else
		devres_free(ptr);

	return addr;
}
EXPORT_SYMBOL(devm_ioremap_prot);
