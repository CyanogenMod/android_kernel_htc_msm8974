/*
 * RapidIO driver support
 *
 * Copyright 2005 MontaVista Software, Inc.
 * Matt Porter <mporter@kernel.crashing.org>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/rio.h>
#include <linux/rio_ids.h>

#include "rio.h"

static const struct rio_device_id *rio_match_device(const struct rio_device_id
						    *id,
						    const struct rio_dev *rdev)
{
	while (id->vid || id->asm_vid) {
		if (((id->vid == RIO_ANY_ID) || (id->vid == rdev->vid)) &&
		    ((id->did == RIO_ANY_ID) || (id->did == rdev->did)) &&
		    ((id->asm_vid == RIO_ANY_ID)
		     || (id->asm_vid == rdev->asm_vid))
		    && ((id->asm_did == RIO_ANY_ID)
			|| (id->asm_did == rdev->asm_did)))
			return id;
		id++;
	}
	return NULL;
}

struct rio_dev *rio_dev_get(struct rio_dev *rdev)
{
	if (rdev)
		get_device(&rdev->dev);

	return rdev;
}

void rio_dev_put(struct rio_dev *rdev)
{
	if (rdev)
		put_device(&rdev->dev);
}

static int rio_device_probe(struct device *dev)
{
	struct rio_driver *rdrv = to_rio_driver(dev->driver);
	struct rio_dev *rdev = to_rio_dev(dev);
	int error = -ENODEV;
	const struct rio_device_id *id;

	if (!rdev->driver && rdrv->probe) {
		if (!rdrv->id_table)
			return error;
		id = rio_match_device(rdrv->id_table, rdev);
		rio_dev_get(rdev);
		if (id)
			error = rdrv->probe(rdev, id);
		if (error >= 0) {
			rdev->driver = rdrv;
			error = 0;
		} else
			rio_dev_put(rdev);
	}
	return error;
}

static int rio_device_remove(struct device *dev)
{
	struct rio_dev *rdev = to_rio_dev(dev);
	struct rio_driver *rdrv = rdev->driver;

	if (rdrv) {
		if (rdrv->remove)
			rdrv->remove(rdev);
		rdev->driver = NULL;
	}

	rio_dev_put(rdev);

	return 0;
}

int rio_register_driver(struct rio_driver *rdrv)
{
	
	rdrv->driver.name = rdrv->name;
	rdrv->driver.bus = &rio_bus_type;

	
	return driver_register(&rdrv->driver);
}

void rio_unregister_driver(struct rio_driver *rdrv)
{
	driver_unregister(&rdrv->driver);
}

static int rio_match_bus(struct device *dev, struct device_driver *drv)
{
	struct rio_dev *rdev = to_rio_dev(dev);
	struct rio_driver *rdrv = to_rio_driver(drv);
	const struct rio_device_id *id = rdrv->id_table;
	const struct rio_device_id *found_id;

	if (!id)
		goto out;

	found_id = rio_match_device(id, rdev);

	if (found_id)
		return 1;

      out:return 0;
}

struct device rio_bus = {
	.init_name = "rapidio",
};

struct bus_type rio_bus_type = {
	.name = "rapidio",
	.match = rio_match_bus,
	.dev_attrs = rio_dev_attrs,
	.probe = rio_device_probe,
	.remove = rio_device_remove,
};

static int __init rio_bus_init(void)
{
	if (device_register(&rio_bus) < 0)
		printk("RIO: failed to register RIO bus device\n");
	return bus_register(&rio_bus_type);
}

postcore_initcall(rio_bus_init);

EXPORT_SYMBOL_GPL(rio_register_driver);
EXPORT_SYMBOL_GPL(rio_unregister_driver);
EXPORT_SYMBOL_GPL(rio_bus_type);
EXPORT_SYMBOL_GPL(rio_dev_get);
EXPORT_SYMBOL_GPL(rio_dev_put);
