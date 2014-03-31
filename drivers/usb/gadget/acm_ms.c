/*
 * acm_ms.c -- Composite driver, with ACM and mass storage support
 *
 * Copyright (C) 2008 David Brownell
 * Copyright (C) 2008 Nokia Corporation
 * Author: David Brownell
 * Modified: Klaus Schwarzkopf <schwarzkopf@sensortherm.de>
 *
 * Heavily based on multi.c and cdc2.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/utsname.h>

#include "u_serial.h"

#define DRIVER_DESC		"Composite Gadget (ACM + MS)"
#define DRIVER_VERSION		"2011/10/10"


#define ACM_MS_VENDOR_NUM	0x1d6b	
#define ACM_MS_PRODUCT_NUM	0x0106	



#include "composite.c"
#include "usbstring.c"
#include "config.c"
#include "epautoconf.c"
#include "u_serial.c"
#include "f_acm.c"
#include "f_mass_storage.c"


static struct usb_device_descriptor device_desc = {
	.bLength =		sizeof device_desc,
	.bDescriptorType =	USB_DT_DEVICE,

	.bcdUSB =		cpu_to_le16(0x0200),

	.bDeviceClass =		USB_CLASS_MISC ,
	.bDeviceSubClass =	2,
	.bDeviceProtocol =	1,

	

	
	.idVendor =		cpu_to_le16(ACM_MS_VENDOR_NUM),
	.idProduct =		cpu_to_le16(ACM_MS_PRODUCT_NUM),
	
	
	
	
	
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


static struct fsg_module_parameters fsg_mod_data = { .stall = 1 };
FSG_MODULE_PARAMETERS(, fsg_mod_data);

static struct fsg_common fsg_common;


static int __init acm_ms_do_config(struct usb_configuration *c)
{
	int	status;

	if (gadget_is_otg(c->cdev->gadget)) {
		c->descriptors = otg_desc;
		c->bmAttributes |= USB_CONFIG_ATT_WAKEUP;
	}


	status = acm_bind_config(c, 0);
	if (status < 0)
		return status;

	status = fsg_bind_config(c->cdev, c, &fsg_common);
	if (status < 0)
		return status;

	return 0;
}

static struct usb_configuration acm_ms_config_driver = {
	.label			= DRIVER_DESC,
	.bConfigurationValue	= 1,
	
	.bmAttributes		= USB_CONFIG_ATT_SELFPOWER,
};


static int __init acm_ms_bind(struct usb_composite_dev *cdev)
{
	int			gcnum;
	struct usb_gadget	*gadget = cdev->gadget;
	int			status;
	void			*retp;

	
	status = gserial_setup(cdev->gadget, 1);
	if (status < 0)
		return status;

	
	retp = fsg_common_from_params(&fsg_common, cdev, &fsg_mod_data);
	if (IS_ERR(retp)) {
		status = PTR_ERR(retp);
		goto fail0;
	}

	
	gcnum = usb_gadget_controller_number(gadget);
	if (gcnum >= 0) {
		device_desc.bcdDevice = cpu_to_le16(0x0300 | gcnum);
	} else {
		WARNING(cdev, "controller '%s' not recognized; trying %s\n",
				gadget->name,
				acm_ms_config_driver.label);
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

	
	status = usb_add_config(cdev, &acm_ms_config_driver, acm_ms_do_config);
	if (status < 0)
		goto fail1;

	dev_info(&gadget->dev, "%s, version: " DRIVER_VERSION "\n",
			DRIVER_DESC);
	fsg_common_put(&fsg_common);
	return 0;

	
fail1:
	fsg_common_put(&fsg_common);
fail0:
	gserial_cleanup();
	return status;
}

static int __exit acm_ms_unbind(struct usb_composite_dev *cdev)
{
	gserial_cleanup();

	return 0;
}

static struct usb_composite_driver acm_ms_driver = {
	.name		= "g_acm_ms",
	.dev		= &device_desc,
	.strings	= dev_strings,
	.unbind		= __exit_p(acm_ms_unbind),
};

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR("Klaus Schwarzkopf <schwarzkopf@sensortherm.de>");
MODULE_LICENSE("GPL v2");

static int __init init(void)
{
	return usb_composite_probe(&acm_ms_driver, acm_ms_bind);
}
module_init(init);

static void __exit cleanup(void)
{
	usb_composite_unregister(&acm_ms_driver);
}
module_exit(cleanup);
