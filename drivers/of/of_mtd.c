/*
 * Copyright 2012 Jean-Christophe PLAGNIOL-VILLARD <plagnioj@jcrosoft.com>
 *
 * OF helpers for mtd.
 *
 * This file is released under the GPLv2
 *
 */
#include <linux/kernel.h>
#include <linux/of_mtd.h>
#include <linux/mtd/nand.h>
#include <linux/export.h>

static const char *nand_ecc_modes[] = {
	[NAND_ECC_NONE]		= "none",
	[NAND_ECC_SOFT]		= "soft",
	[NAND_ECC_HW]		= "hw",
	[NAND_ECC_HW_SYNDROME]	= "hw_syndrome",
	[NAND_ECC_HW_OOB_FIRST]	= "hw_oob_first",
	[NAND_ECC_SOFT_BCH]	= "soft_bch",
};

const int of_get_nand_ecc_mode(struct device_node *np)
{
	const char *pm;
	int err, i;

	err = of_property_read_string(np, "nand-ecc-mode", &pm);
	if (err < 0)
		return err;

	for (i = 0; i < ARRAY_SIZE(nand_ecc_modes); i++)
		if (!strcasecmp(pm, nand_ecc_modes[i]))
			return i;

	return -ENODEV;
}
EXPORT_SYMBOL_GPL(of_get_nand_ecc_mode);

int of_get_nand_bus_width(struct device_node *np)
{
	u32 val;

	if (of_property_read_u32(np, "nand-bus-width", &val))
		return 8;

	switch(val) {
	case 8:
	case 16:
		return val;
	default:
		return -EIO;
	}
}
EXPORT_SYMBOL_GPL(of_get_nand_bus_width);

bool of_get_nand_on_flash_bbt(struct device_node *np)
{
	return of_property_read_bool(np, "nand-on-flash-bbt");
}
EXPORT_SYMBOL_GPL(of_get_nand_on_flash_bbt);
