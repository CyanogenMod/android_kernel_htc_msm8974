/*
 * ncm.c -- NCM gadget driver
 *
 * Copyright (C) 2010 Nokia Corporation
 * Contact: Yauheni Kaliuta <yauheni.kaliuta@nokia.com>
 *
 * The driver borrows from ether.c which is:
 *
 * Copyright (C) 2003-2005,2008 David Brownell
 * Copyright (C) 2003-2004 Robert Schwebel, Benedikt Spranger
 * Copyright (C) 2008 Nokia Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */


#include <linux/kernel.h>
#include <linux/utsname.h>


#include "u_ether.h"

#define DRIVER_DESC		"NCM Gadget"


#include "composite.c"
#include "usbstring.c"
#include "config.c"
#include "epautoconf.c"

#include "f_ncm.c"
#include "u_ether.c"



#define CDC_VENDOR_NUM		0x0525	
#define CDC_PRODUCT_NUM		0xa4a1	


static struct usb_device_descriptor device_desc = {
	.bLength =		sizeof device_desc,
	.bDescriptorType =	USB_DT_DEVICE,

	.bcdUSB =		cpu_to_le16 (0x0200),

	.bDeviceClass =		USB_CLASS_COMM,
	.bDeviceSubClass =	0,
	.bDeviceProtocol =	0,
	

	.idVendor =		cpu_to_le16 (CDC_VENDOR_NUM),
	.idProduct =		cpu_to_le16 (CDC_PRODUCT_NUM),
	
	
	
	
	.bNumConfigurations =	1,
};

static struct usb_otg_descriptor otg_descriptor = {
	.bLength =		sizeof otg_descriptor,
	.bDescriptorType =	USB_DT_OTG,

	.bmAttributes =		USB_OTG_SRP | USB_OTG_HNP,
};

static const struct usb_descriptor_header *otg_desc[] = {
	(struct usb_descriptor_header *) &otg_descriptor,
	NULL,
};



#define STRING_MANUFACTURER_IDX		0
#define STRING_PRODUCT_IDX		1

static char manufacturer[50];

static struct usb_string strings_dev[] = {
	[STRING_MANUFACTURER_IDX].s = manufacturer,
	[STRING_PRODUCT_IDX].s = DRIVER_DESC,
	{  } 
};

static struct usb_gadget_strings stringtab_dev = {
	.language	= 0x0409,	
	.strings	= strings_dev,
};

static struct usb_gadget_strings *dev_strings[] = {
	&stringtab_dev,
	NULL,
};

static u8 hostaddr[ETH_ALEN];


static int __init ncm_do_config(struct usb_configuration *c)
{
	

	if (gadget_is_otg(c->cdev->gadget)) {
		c->descriptors = otg_desc;
		c->bmAttributes |= USB_CONFIG_ATT_WAKEUP;
	}

	return ncm_bind_config(c, hostaddr);
}

static struct usb_configuration ncm_config_driver = {
	
	.label			= "CDC Ethernet (NCM)",
	.bConfigurationValue	= 1,
	
	.bmAttributes		= USB_CONFIG_ATT_SELFPOWER,
};


static int __init gncm_bind(struct usb_composite_dev *cdev)
{
	int			gcnum;
	struct usb_gadget	*gadget = cdev->gadget;
	int			status;

	
	status = gether_setup(cdev->gadget, hostaddr);
	if (status < 0)
		return status;

	gcnum = usb_gadget_controller_number(gadget);
	if (gcnum >= 0)
		device_desc.bcdDevice = cpu_to_le16(0x0300 | gcnum);
	else {
		dev_warn(&gadget->dev,
			 "controller '%s' not recognized; trying %s\n",
			 gadget->name,
			 ncm_config_driver.label);
		device_desc.bcdDevice =
			cpu_to_le16(0x0300 | 0x0099);
	}



	
	snprintf(manufacturer, sizeof manufacturer, "%s %s with %s",
		init_utsname()->sysname, init_utsname()->release,
		gadget->name);
	status = usb_string_id(cdev);
	if (status < 0)
		goto fail;
	strings_dev[STRING_MANUFACTURER_IDX].id = status;
	device_desc.iManufacturer = status;

	status = usb_string_id(cdev);
	if (status < 0)
		goto fail;
	strings_dev[STRING_PRODUCT_IDX].id = status;
	device_desc.iProduct = status;

	status = usb_add_config(cdev, &ncm_config_driver,
				ncm_do_config);
	if (status < 0)
		goto fail;

	dev_info(&gadget->dev, "%s\n", DRIVER_DESC);

	return 0;

fail:
	gether_cleanup();
	return status;
}

static int __exit gncm_unbind(struct usb_composite_dev *cdev)
{
	gether_cleanup();
	return 0;
}

static struct usb_composite_driver ncm_driver = {
	.name		= "g_ncm",
	.dev		= &device_desc,
	.strings	= dev_strings,
	.max_speed	= USB_SPEED_HIGH,
	.unbind		= __exit_p(gncm_unbind),
};

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR("Yauheni Kaliuta");
MODULE_LICENSE("GPL");

static int __init init(void)
{
	return usb_composite_probe(&ncm_driver, gncm_bind);
}
module_init(init);

static void __exit cleanup(void)
{
	usb_composite_unregister(&ncm_driver);
}
module_exit(cleanup);
