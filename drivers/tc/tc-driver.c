/*
 *	TURBOchannel driver services.
 *
 *	Copyright (c) 2005  James Simmons
 *	Copyright (c) 2006  Maciej W. Rozycki
 *
 *	Loosely based on drivers/dio/dio-driver.c and
 *	drivers/pci/pci-driver.c.
 *
 *	This file is subject to the terms and conditions of the GNU
 *	General Public License.  See the file "COPYING" in the main
 *	directory of this archive for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/tc.h>

int tc_register_driver(struct tc_driver *tdrv)
{
	return driver_register(&tdrv->driver);
}
EXPORT_SYMBOL(tc_register_driver);

void tc_unregister_driver(struct tc_driver *tdrv)
{
	driver_unregister(&tdrv->driver);
}
EXPORT_SYMBOL(tc_unregister_driver);

const struct tc_device_id *tc_match_device(struct tc_driver *tdrv,
					   struct tc_dev *tdev)
{
	const struct tc_device_id *id = tdrv->id_table;

	if (id) {
		while (id->name[0] || id->vendor[0]) {
			if (strcmp(tdev->name, id->name) == 0 &&
			    strcmp(tdev->vendor, id->vendor) == 0)
				return id;
			id++;
		}
	}
	return NULL;
}
EXPORT_SYMBOL(tc_match_device);

static int tc_bus_match(struct device *dev, struct device_driver *drv)
{
	struct tc_dev *tdev = to_tc_dev(dev);
	struct tc_driver *tdrv = to_tc_driver(drv);
	const struct tc_device_id *id;

	id = tc_match_device(tdrv, tdev);
	if (id)
		return 1;

	return 0;
}

struct bus_type tc_bus_type = {
	.name	= "tc",
	.match	= tc_bus_match,
};
EXPORT_SYMBOL(tc_bus_type);

static int __init tc_driver_init(void)
{
	return bus_register(&tc_bus_type);
}

postcore_initcall(tc_driver_init);
