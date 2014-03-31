/*
 * Core driver for WM8400.
 *
 * Copyright 2008 Wolfson Microelectronics PLC.
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/bug.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/mfd/core.h>
#include <linux/mfd/wm8400-private.h>
#include <linux/mfd/wm8400-audio.h>
#include <linux/regmap.h>
#include <linux/slab.h>

static struct {
	u16  readable;    
	u16  writable;    
	u16  vol;         
	int  is_codec;    
	u16  default_val; 
} reg_data[] = {
	{ 0xFFFF, 0xFFFF, 0x0000, 0, 0x6172 }, 
	{ 0x7000, 0x0000, 0x8000, 0, 0x0000 }, 
	{ 0xFF17, 0xFF17, 0x0000, 0, 0x0000 }, 
	{ 0xEBF3, 0xEBF3, 0x0000, 1, 0x6000 }, 
	{ 0x3CF3, 0x3CF3, 0x0000, 1, 0x0000 }, 
	{ 0xF1F8, 0xF1F8, 0x0000, 1, 0x4050 }, 
	{ 0xFC1F, 0xFC1F, 0x0000, 1, 0x4000 }, 
	{ 0xDFDE, 0xDFDE, 0x0000, 1, 0x01C8 }, 
	{ 0xFCFC, 0xFCFC, 0x0000, 1, 0x0000 }, 
	{ 0xEFFF, 0xEFFF, 0x0000, 1, 0x0040 }, 
	{ 0xEFFF, 0xEFFF, 0x0000, 1, 0x0040 }, 
	{ 0x27F7, 0x27F7, 0x0000, 1, 0x0004 }, 
	{ 0x01FF, 0x01FF, 0x0000, 1, 0x00C0 }, 
	{ 0x01FF, 0x01FF, 0x0000, 1, 0x00C0 }, 
	{ 0x1FEF, 0x1FEF, 0x0000, 1, 0x0000 }, 
	{ 0x0163, 0x0163, 0x0000, 1, 0x0100 }, 
	{ 0x01FF, 0x01FF, 0x0000, 1, 0x00C0 }, 
	{ 0x01FF, 0x01FF, 0x0000, 1, 0x00C0 }, 
	{ 0x1FFF, 0x0FFF, 0x0000, 1, 0x0000 }, 
	{ 0xFFFF, 0xFFFF, 0x0000, 1, 0x1000 }, 
	{ 0xFFFF, 0xFFFF, 0x0000, 1, 0x1010 }, 
	{ 0xFFFF, 0xFFFF, 0x0000, 1, 0x1010 }, 
	{ 0x0FDD, 0x0FDD, 0x0000, 1, 0x8000 }, 
	{ 0x1FFF, 0x1FFF, 0x0000, 1, 0x0800 }, 
	{ 0x0000, 0x01DF, 0x0000, 1, 0x008B }, 
	{ 0x0000, 0x01DF, 0x0000, 1, 0x008B }, 
	{ 0x0000, 0x01DF, 0x0000, 1, 0x008B }, 
	{ 0x0000, 0x01DF, 0x0000, 1, 0x008B }, 
	{ 0x0000, 0x01FF, 0x0000, 1, 0x0000 }, 
	{ 0x0000, 0x01FF, 0x0000, 1, 0x0000 }, 
	{ 0x0000, 0x0077, 0x0000, 1, 0x0066 }, 
	{ 0x0000, 0x0033, 0x0000, 1, 0x0022 }, 
	{ 0x0000, 0x01FF, 0x0000, 1, 0x0079 }, 
	{ 0x0000, 0x01FF, 0x0000, 1, 0x0079 }, 
	{ 0x0000, 0x0003, 0x0000, 1, 0x0003 }, 
	{ 0x0000, 0x01FF, 0x0000, 1, 0x0003 }, 
	{ 0x0000, 0x0000, 0x0000, 0, 0x0000 }, 
	{ 0x0000, 0x003F, 0x0000, 1, 0x0100 }, 
	{ 0x0000, 0x0000, 0x0000, 0, 0x0000 }, 
	{ 0x0000, 0x000F, 0x0000, 0, 0x0000 }, 
	{ 0x0000, 0x00FF, 0x0000, 1, 0x0000 }, 
	{ 0x0000, 0x01B7, 0x0000, 1, 0x0000 }, 
	{ 0x0000, 0x01B7, 0x0000, 1, 0x0000 }, 
	{ 0x0000, 0x01FF, 0x0000, 1, 0x0000 }, 
	{ 0x0000, 0x01FF, 0x0000, 1, 0x0000 }, 
	{ 0x0000, 0x00FD, 0x0000, 1, 0x0000 }, 
	{ 0x0000, 0x00FD, 0x0000, 1, 0x0000 }, 
	{ 0x0000, 0x01FF, 0x0000, 1, 0x0000 }, 
	{ 0x0000, 0x01FF, 0x0000, 1, 0x0000 }, 
	{ 0x0000, 0x01FF, 0x0000, 1, 0x0000 }, 
	{ 0x0000, 0x01FF, 0x0000, 1, 0x0000 }, 
	{ 0x0000, 0x01B3, 0x0000, 1, 0x0180 }, 
	{ 0x0000, 0x0077, 0x0000, 1, 0x0000 }, 
	{ 0x0000, 0x0077, 0x0000, 1, 0x0000 }, 
	{ 0x0000, 0x00FF, 0x0000, 1, 0x0000 }, 
	{ 0x0000, 0x0001, 0x0000, 1, 0x0000 }, 
	{ 0x0000, 0x003F, 0x0000, 1, 0x0000 }, 
	{ 0x0000, 0x004F, 0x0000, 1, 0x0000 }, 
	{ 0x0000, 0x00FD, 0x0000, 1, 0x0000 }, 
	{ 0x0000, 0x0000, 0x0000, 0, 0x0000 }, 
	{ 0x1FFF, 0x1FFF, 0x0000, 1, 0x0000 }, 
	{ 0xFFFF, 0xFFFF, 0x0000, 1, 0x0000 }, 
	{ 0x03FF, 0x03FF, 0x0000, 1, 0x0000 }, 
	{ 0x007F, 0x007F, 0x0000, 1, 0x0000 }, 
	{ 0x0000, 0x0000, 0x0000, 0, 0x0000 }, 
	{ 0xDFFF, 0xDFFF, 0x0000, 0, 0x0000 }, 
	{ 0xDFFF, 0xDFFF, 0x0000, 0, 0x0000 }, 
	{ 0xDFFF, 0xDFFF, 0x0000, 0, 0x0000 }, 
	{ 0xDFFF, 0xDFFF, 0x0000, 0, 0x0000 }, 
	{ 0x0000, 0x0000, 0x0000, 0, 0x0000 }, 
	{ 0xFFFF, 0xFFFF, 0x0000, 0, 0x4400 }, 
	{ 0x23FF, 0x23FF, 0x0000, 0, 0x0000 }, 
	{ 0xFFFF, 0xFFFF, 0x0000, 0, 0x4400 }, 
	{ 0x23FF, 0x23FF, 0x0000, 0, 0x0000 }, 
	{ 0x0000, 0x0000, 0x0000, 0, 0x0000 }, 
	{ 0x000E, 0x000E, 0x0000, 0, 0x0008 }, 
	{ 0xE00F, 0xE00F, 0x0000, 0, 0x0000 }, 
	{ 0x0000, 0x0000, 0x0000, 0, 0x0000 }, 
	{ 0x03C0, 0x03C0, 0x0000, 0, 0x02C0 }, 
	{ 0xFFFF, 0x0000, 0xffff, 0, 0x0000 }, 
	{ 0xFFFF, 0xFFFF, 0x0000, 0, 0x0000 }, 
	{ 0xFFFF, 0x0000, 0xffff, 0, 0x0000 }, 
	{ 0x2BFF, 0x0000, 0xffff, 0, 0x0000 }, 
	{ 0x0000, 0x0000, 0x0000, 0, 0x0000 }, 
	{ 0x80FF, 0x80FF, 0x0000, 0, 0x00ff }, 
};

static int wm8400_read(struct wm8400 *wm8400, u8 reg, int num_regs, u16 *dest)
{
	int i, ret = 0;

	BUG_ON(reg + num_regs > ARRAY_SIZE(wm8400->reg_cache));

	
	for (i = reg; i < reg + num_regs; i++)
		if (reg_data[i].vol) {
			ret = regmap_bulk_read(wm8400->regmap, reg, dest,
					       num_regs);
			return ret;
		}

	
	memcpy(dest, &wm8400->reg_cache[reg], num_regs * sizeof(u16));

	return 0;
}

static int wm8400_write(struct wm8400 *wm8400, u8 reg, int num_regs,
			u16 *src)
{
	int ret, i;

	BUG_ON(reg + num_regs > ARRAY_SIZE(wm8400->reg_cache));

	for (i = 0; i < num_regs; i++) {
		BUG_ON(!reg_data[reg + i].writable);
		wm8400->reg_cache[reg + i] = src[i];
		ret = regmap_write(wm8400->regmap, reg, src[i]);
		if (ret != 0)
			return ret;
	}

	return 0;
}

u16 wm8400_reg_read(struct wm8400 *wm8400, u8 reg)
{
	u16 val;

	mutex_lock(&wm8400->io_lock);

	wm8400_read(wm8400, reg, 1, &val);

	mutex_unlock(&wm8400->io_lock);

	return val;
}
EXPORT_SYMBOL_GPL(wm8400_reg_read);

int wm8400_block_read(struct wm8400 *wm8400, u8 reg, int count, u16 *data)
{
	int ret;

	mutex_lock(&wm8400->io_lock);

	ret = wm8400_read(wm8400, reg, count, data);

	mutex_unlock(&wm8400->io_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(wm8400_block_read);

int wm8400_set_bits(struct wm8400 *wm8400, u8 reg, u16 mask, u16 val)
{
	u16 tmp;
	int ret;

	mutex_lock(&wm8400->io_lock);

	ret = wm8400_read(wm8400, reg, 1, &tmp);
	tmp = (tmp & ~mask) | val;
	if (ret == 0)
		ret = wm8400_write(wm8400, reg, 1, &tmp);

	mutex_unlock(&wm8400->io_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(wm8400_set_bits);

void wm8400_reset_codec_reg_cache(struct wm8400 *wm8400)
{
	int i;

	mutex_lock(&wm8400->io_lock);

	
	for (i = 0; i < ARRAY_SIZE(wm8400->reg_cache); i++)
		if (reg_data[i].is_codec)
			wm8400->reg_cache[i] = reg_data[i].default_val;

	mutex_unlock(&wm8400->io_lock);
}
EXPORT_SYMBOL_GPL(wm8400_reset_codec_reg_cache);

static int wm8400_register_codec(struct wm8400 *wm8400)
{
	struct mfd_cell cell = {
		.name = "wm8400-codec",
		.platform_data = wm8400,
		.pdata_size = sizeof(*wm8400),
	};

	return mfd_add_devices(wm8400->dev, -1, &cell, 1, NULL, 0);
}

static int wm8400_init(struct wm8400 *wm8400,
		       struct wm8400_platform_data *pdata)
{
	u16 reg;
	int ret, i;

	mutex_init(&wm8400->io_lock);

	dev_set_drvdata(wm8400->dev, wm8400);

	
	ret = regmap_read(wm8400->regmap, WM8400_RESET_ID, &i);
	if (ret != 0) {
		dev_err(wm8400->dev, "Chip ID register read failed\n");
		return -EIO;
	}
	if (i != reg_data[WM8400_RESET_ID].default_val) {
		dev_err(wm8400->dev, "Device is not a WM8400, ID is %x\n", i);
		return -ENODEV;
	}

	ret = regmap_raw_read(wm8400->regmap, 0, wm8400->reg_cache,
			      ARRAY_SIZE(wm8400->reg_cache));
	if (ret != 0) {
		dev_err(wm8400->dev, "Register cache read failed\n");
		return -EIO;
	}
	for (i = 0; i < ARRAY_SIZE(wm8400->reg_cache); i++)
		wm8400->reg_cache[i] = be16_to_cpu(wm8400->reg_cache[i]);

	
	if (!(wm8400->reg_cache[WM8400_POWER_MANAGEMENT_1] & WM8400_CODEC_ENA))
		for (i = 0; i < ARRAY_SIZE(wm8400->reg_cache); i++)
			if (reg_data[i].is_codec)
				wm8400->reg_cache[i] = reg_data[i].default_val;

	ret = wm8400_read(wm8400, WM8400_ID, 1, &reg);
	if (ret != 0) {
		dev_err(wm8400->dev, "ID register read failed: %d\n", ret);
		return ret;
	}
	reg = (reg & WM8400_CHIP_REV_MASK) >> WM8400_CHIP_REV_SHIFT;
	dev_info(wm8400->dev, "WM8400 revision %x\n", reg);

	ret = wm8400_register_codec(wm8400);
	if (ret != 0) {
		dev_err(wm8400->dev, "Failed to register codec\n");
		goto err_children;
	}

	if (pdata && pdata->platform_init) {
		ret = pdata->platform_init(wm8400->dev);
		if (ret != 0) {
			dev_err(wm8400->dev, "Platform init failed: %d\n",
				ret);
			goto err_children;
		}
	} else
		dev_warn(wm8400->dev, "No platform initialisation supplied\n");

	return 0;

err_children:
	mfd_remove_devices(wm8400->dev);
	return ret;
}

static void wm8400_release(struct wm8400 *wm8400)
{
	mfd_remove_devices(wm8400->dev);
}

static const struct regmap_config wm8400_regmap_config = {
	.reg_bits = 8,
	.val_bits = 16,
	.max_register = WM8400_REGISTER_COUNT - 1,
};

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
static int wm8400_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct wm8400 *wm8400;
	int ret;

	wm8400 = devm_kzalloc(&i2c->dev, sizeof(struct wm8400), GFP_KERNEL);
	if (wm8400 == NULL) {
		ret = -ENOMEM;
		goto err;
	}

	wm8400->regmap = devm_regmap_init_i2c(i2c, &wm8400_regmap_config);
	if (IS_ERR(wm8400->regmap)) {
		ret = PTR_ERR(wm8400->regmap);
		goto err;
	}

	wm8400->dev = &i2c->dev;
	i2c_set_clientdata(i2c, wm8400);

	ret = wm8400_init(wm8400, i2c->dev.platform_data);
	if (ret != 0)
		goto err;

	return 0;

err:
	return ret;
}

static int wm8400_i2c_remove(struct i2c_client *i2c)
{
	struct wm8400 *wm8400 = i2c_get_clientdata(i2c);

	wm8400_release(wm8400);

	return 0;
}

static const struct i2c_device_id wm8400_i2c_id[] = {
       { "wm8400", 0 },
       { }
};
MODULE_DEVICE_TABLE(i2c, wm8400_i2c_id);

static struct i2c_driver wm8400_i2c_driver = {
	.driver = {
		.name = "WM8400",
		.owner = THIS_MODULE,
	},
	.probe    = wm8400_i2c_probe,
	.remove   = wm8400_i2c_remove,
	.id_table = wm8400_i2c_id,
};
#endif

static int __init wm8400_module_init(void)
{
	int ret = -ENODEV;

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	ret = i2c_add_driver(&wm8400_i2c_driver);
	if (ret != 0)
		pr_err("Failed to register I2C driver: %d\n", ret);
#endif

	return ret;
}
subsys_initcall(wm8400_module_init);

static void __exit wm8400_module_exit(void)
{
#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	i2c_del_driver(&wm8400_i2c_driver);
#endif
}
module_exit(wm8400_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mark Brown <broonie@opensource.wolfsonmicro.com>");
