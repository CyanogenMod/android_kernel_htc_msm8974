/*
 * system.c - a driver for reserving pnp system resources
 *
 * Some code is based on pnpbios_core.c
 * Copyright 2002 Adam Belay <ambx1@neo.rr.com>
 * (c) Copyright 2007 Hewlett-Packard Development Company, L.P.
 *	Bjorn Helgaas <bjorn.helgaas@hp.com>
 */

#include <linux/pnp.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/ioport.h>

static const struct pnp_device_id pnp_dev_table[] = {
	
	{"PNP0c02", 0},
	
	{"PNP0c01", 0},
	{"", 0}
};

static void reserve_range(struct pnp_dev *dev, struct resource *r, int port)
{
	char *regionid;
	const char *pnpid = dev_name(&dev->dev);
	resource_size_t start = r->start, end = r->end;
	struct resource *res;

	regionid = kmalloc(16, GFP_KERNEL);
	if (!regionid)
		return;

	snprintf(regionid, 16, "pnp %s", pnpid);
	if (port)
		res = request_region(start, end - start + 1, regionid);
	else
		res = request_mem_region(start, end - start + 1, regionid);
	if (res)
		res->flags &= ~IORESOURCE_BUSY;
	else
		kfree(regionid);

	dev_info(&dev->dev, "%pR %s reserved\n", r,
		 res ? "has been" : "could not be");
}

static void reserve_resources_of_dev(struct pnp_dev *dev)
{
	struct resource *res;
	int i;

	for (i = 0; (res = pnp_get_resource(dev, IORESOURCE_IO, i)); i++) {
		if (res->flags & IORESOURCE_DISABLED)
			continue;
		if (res->start == 0)
			continue;	
		if (res->start < 0x100)
			continue;
		if (res->end < res->start)
			continue;	

		reserve_range(dev, res, 1);
	}

	for (i = 0; (res = pnp_get_resource(dev, IORESOURCE_MEM, i)); i++) {
		if (res->flags & IORESOURCE_DISABLED)
			continue;

		reserve_range(dev, res, 0);
	}
}

static int system_pnp_probe(struct pnp_dev *dev,
			    const struct pnp_device_id *dev_id)
{
	reserve_resources_of_dev(dev);
	return 0;
}

static struct pnp_driver system_pnp_driver = {
	.name     = "system",
	.id_table = pnp_dev_table,
	.flags    = PNP_DRIVER_RES_DO_NOT_CHANGE,
	.probe    = system_pnp_probe,
};

static int __init pnp_system_init(void)
{
	return pnp_register_driver(&system_pnp_driver);
}

fs_initcall(pnp_system_init);
