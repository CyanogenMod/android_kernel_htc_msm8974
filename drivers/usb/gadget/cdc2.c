/*
 * cdc2.c -- CDC Composite driver, with ECM and ACM support
 *
 * Copyright (C) 2008 David Brownell
 * Copyright (C) 2008 Nokia Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/utsname.h>
#include <linux/module.h>

#include "u_ether.h"
#include "u_serial.h"


#define DRIVER_DESC		"CDC Composite Gadget"
#define DRIVER_VERSION		"King Kamehameha Day 2008"



#define CDC_VENDOR_NUM		0x0525	
#define CDC_PRODUCT_NUM		0xa4aa	



#include "composite.c"
#include "usbstring.c"
#include "config.c"
#include "epautoconf.c"
#include "u_serial.c"
#include "f_acm.c"
#include "f_ecm.c"
#include "u_ether.c"


static struct usb_device_descriptor device_desc = {
	.bLength =		sizeof device_desc,
	.bDescriptorType =	USB_DT_DEVICE,

	.bcdUSB =		cpu_to_le16(0x0200),

	.bDeviceClass =		USB_CLASS_COMM,
	.bDeviceSubClass =	0,
	.bDeviceProtocol =	0,
	

	
	.idVendor =		cpu_to_le16(CDC_VENDOR_NUM),
	.idProduct =		cpu_to_le16(CDC_PRODUCT_NUM),
	
	
	
	
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


static int __init cdc_do_config(struct usb_configuration *c)
{
	int	status;

	if (gadget_is_otg(c->cdev->gadget)) {
		c->descriptors = otg_desc;
		c->bmAttributes |= USB_CONFIG_ATT_WAKEUP;
	}

	status = ecm_bind_config(c, hostaddr);
	if (status < 0)
		return status;

	status = acm_bind_config(c, 0);
	if (status < 0)
		return status;

	return 0;
}

static struct usb_configuration cdc_config_driver = {
	.label			= "CDC Composite (ECM + ACM)",
	.bConfigurationValue	= 1,
	
	.bmAttributes		= USB_CONFIG_ATT_SELFPOWER,
};


static int __init cdc_bind(struct usb_composite_dev *cdev)
{
	int			gcnum;
	struct usb_gadget	*gadget = cdev->gadget;
	int			status;

	if (!can_support_ecm(cdev->gadget)) {
		dev_err(&gadget->dev, "controller '%s' not usable\n",
				gadget->name);
		return -EINVAL;
	}

	
	status = gether_setup(cdev->gadget, hostaddr);
	if (status < 0)
		return status;

	
	status = gserial_setup(cdev->gadget, 1);
	if (status < 0)
		goto fail0;

	gcnum = usb_gadget_controller_number(gadget);
	if (gcnum >= 0)
		device_desc.bcdDevice = cpu_to_le16(0x0300 | gcnum);
	else {
		WARNING(cdev, "controller '%s' not recognized; trying %s\n",
				gadget->name,
				cdc_config_driver.label);
		device_desc.bcdDevice =
			cpu_to_le16(0x0300 | 0x0099);
	}



	
	snprintf(manufacturer, sizeof manufacturer, "%s %s with %s",
		init_utsname()->sysname, init_utsname()->release,
		gadget->name);
	status = usb_string_id(cdev);
	if (status < 0)
		goto fail1;
	strings_dev[STRING_MANUFACTURER_IDX].id = status;
	device_desc.iManufacturer = status;

	status = usb_string_id(cdev);
	if (status < 0)
		goto fail1;
	strings_dev[STRING_PRODUCT_IDX].id = status;
	device_desc.iProduct = status;

	
	status = usb_add_config(cdev, &cdc_config_driver, cdc_do_config);
	if (status < 0)
		goto fail1;

	dev_info(&gadget->dev, "%s, version: " DRIVER_VERSION "\n",
			DRIVER_DESC);

	return 0;

fail1:
	gserial_cleanup();
fail0:
	gether_cleanup();
	return status;
}

static int __exit cdc_unbind(struct usb_composite_dev *cdev)
{
	gserial_cleanup();
	gether_cleanup();
	return 0;
}

static struct usb_composite_driver cdc_driver = {
	.name		= "g_cdc",
	.dev		= &device_desc,
	.strings	= dev_strings,
	.max_speed	= USB_SPEED_HIGH,
	.unbind		= __exit_p(cdc_unbind),
};

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR("David Brownell");
MODULE_LICENSE("GPL");

static int __init init(void)
{
	return usb_composite_probe(&cdc_driver, cdc_bind);
}
module_init(init);

static void __exit cleanup(void)
{
	usb_composite_unregister(&cdc_driver);
}
module_exit(cleanup);
