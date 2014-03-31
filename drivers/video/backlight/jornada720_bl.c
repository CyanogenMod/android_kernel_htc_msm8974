/*
 *
 * Backlight driver for HP Jornada 700 series (710/720/728)
 * Copyright (C) 2006-2009 Kristoffer Ericson <kristoffer.ericson@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 or any later version as published by the Free Software Foundation.
 *
 */

#include <linux/backlight.h>
#include <linux/device.h>
#include <linux/fb.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <mach/jornada720.h>
#include <mach/hardware.h>

#include <video/s1d13xxxfb.h>

#define BL_MAX_BRIGHT	255
#define BL_DEF_BRIGHT	25

static int jornada_bl_get_brightness(struct backlight_device *bd)
{
	int ret;

	
	if (!(PPSR & PPC_LDD1))
		return 0;

	jornada_ssp_start();

	
	ret = jornada_ssp_byte(GETBRIGHTNESS);

	if (jornada_ssp_byte(GETBRIGHTNESS) != TXDUMMY) {
		printk(KERN_ERR "bl : get brightness timeout\n");
		jornada_ssp_end();
		return -ETIMEDOUT;
	} else 
		ret = jornada_ssp_byte(TXDUMMY);

	jornada_ssp_end();

	return (BL_MAX_BRIGHT - ret);
}

static int jornada_bl_update_status(struct backlight_device *bd)
{
	int ret = 0;

	jornada_ssp_start();

	
	if ((bd->props.power != FB_BLANK_UNBLANK) || (bd->props.fb_blank != FB_BLANK_UNBLANK)) {
		ret = jornada_ssp_byte(BRIGHTNESSOFF);
		if (ret != TXDUMMY) {
			printk(KERN_INFO "bl : brightness off timeout\n");
			
			PPSR &= ~PPC_LDD1;
			PPDR |= PPC_LDD1;
			ret = -ETIMEDOUT;
		}
	} else  
		PPSR |= PPC_LDD1;

		
		if (jornada_ssp_byte(SETBRIGHTNESS) != TXDUMMY) {
			printk(KERN_INFO "bl : failed to set brightness\n");
			ret = -ETIMEDOUT;
			goto out;
		}

		if (jornada_ssp_byte(BL_MAX_BRIGHT - bd->props.brightness) != TXDUMMY) {
			printk(KERN_ERR "bl : set brightness failed\n");
			ret = -ETIMEDOUT;
		}

out:
	jornada_ssp_end();

	return ret;
}

static const struct backlight_ops jornada_bl_ops = {
	.get_brightness = jornada_bl_get_brightness,
	.update_status = jornada_bl_update_status,
	.options = BL_CORE_SUSPENDRESUME,
};

static int jornada_bl_probe(struct platform_device *pdev)
{
	struct backlight_properties props;
	int ret;
	struct backlight_device *bd;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = BL_MAX_BRIGHT;
	bd = backlight_device_register(S1D_DEVICENAME, &pdev->dev, NULL,
				       &jornada_bl_ops, &props);

	if (IS_ERR(bd)) {
		ret = PTR_ERR(bd);
		printk(KERN_ERR "bl : failed to register device, err=%x\n", ret);
		return ret;
	}

	bd->props.power = FB_BLANK_UNBLANK;
	bd->props.brightness = BL_DEF_BRIGHT;
	jornada_bl_update_status(bd);

	platform_set_drvdata(pdev, bd);
	printk(KERN_INFO "HP Jornada 700 series backlight driver\n");

	return 0;
}

static int jornada_bl_remove(struct platform_device *pdev)
{
	struct backlight_device *bd = platform_get_drvdata(pdev);

	backlight_device_unregister(bd);

	return 0;
}

static struct platform_driver jornada_bl_driver = {
	.probe		= jornada_bl_probe,
	.remove		= jornada_bl_remove,
	.driver	= {
		.name	= "jornada_bl",
	},
};

module_platform_driver(jornada_bl_driver);

MODULE_AUTHOR("Kristoffer Ericson <kristoffer.ericson>");
MODULE_DESCRIPTION("HP Jornada 710/720/728 Backlight driver");
MODULE_LICENSE("GPL");
