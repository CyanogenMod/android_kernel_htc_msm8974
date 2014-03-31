/*
 * ether.c -- Ethernet gadget driver, with CDC and non-CDC options
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


#if defined USB_ETH_RNDIS
#  undef USB_ETH_RNDIS
#endif
#ifdef CONFIG_USB_ETH_RNDIS
#  define USB_ETH_RNDIS y
#endif

#include "u_ether.h"



#define DRIVER_DESC		"Ethernet Gadget"
#define DRIVER_VERSION		"Memorial Day 2008"

#ifdef USB_ETH_RNDIS
#define PREFIX			"RNDIS/"
#else
#define PREFIX			""
#endif


static inline bool has_rndis(void)
{
#ifdef	USB_ETH_RNDIS
	return true;
#else
	return false;
#endif
}


#include "composite.c"
#include "usbstring.c"
#include "config.c"
#include "epautoconf.c"

#include "f_ecm.c"
#include "f_subset.c"
#ifdef	USB_ETH_RNDIS
#include "f_rndis.c"
#include "rndis.c"
#endif
#include "f_eem.c"
#include "u_ether.c"



#define CDC_VENDOR_NUM		0x0525	
#define CDC_PRODUCT_NUM		0xa4a1	

#define	SIMPLE_VENDOR_NUM	0x049f
#define	SIMPLE_PRODUCT_NUM	0x505a

#define RNDIS_VENDOR_NUM	0x0525	
#define RNDIS_PRODUCT_NUM	0xa4a2	

#define EEM_VENDOR_NUM		0x1d6b	
#define EEM_PRODUCT_NUM		0x0102	


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
	[STRING_PRODUCT_IDX].s = PREFIX DRIVER_DESC,
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


static int __init rndis_do_config(struct usb_configuration *c)
{
	

	if (gadget_is_otg(c->cdev->gadget)) {
		c->descriptors = otg_desc;
		c->bmAttributes |= USB_CONFIG_ATT_WAKEUP;
	}

	return rndis_bind_config(c, hostaddr);
}

static struct usb_configuration rndis_config_driver = {
	.label			= "RNDIS",
	.bConfigurationValue	= 2,
	
	.bmAttributes		= USB_CONFIG_ATT_SELFPOWER,
};


#ifdef CONFIG_USB_ETH_EEM
static bool use_eem = 1;
#else
static bool use_eem;
#endif
module_param(use_eem, bool, 0);
MODULE_PARM_DESC(use_eem, "use CDC EEM mode");

static int __init eth_do_config(struct usb_configuration *c)
{
	

	if (gadget_is_otg(c->cdev->gadget)) {
		c->descriptors = otg_desc;
		c->bmAttributes |= USB_CONFIG_ATT_WAKEUP;
	}

	if (use_eem)
		return eem_bind_config(c);
	else if (can_support_ecm(c->cdev->gadget))
		return ecm_bind_config(c, hostaddr);
	else
		return geth_bind_config(c, hostaddr);
}

static struct usb_configuration eth_config_driver = {
	
	.bConfigurationValue	= 1,
	
	.bmAttributes		= USB_CONFIG_ATT_SELFPOWER,
};


static int __init eth_bind(struct usb_composite_dev *cdev)
{
	int			gcnum;
	struct usb_gadget	*gadget = cdev->gadget;
	int			status;

	
	status = gether_setup(cdev->gadget, hostaddr);
	if (status < 0)
		return status;

	
	if (use_eem) {
		
		eth_config_driver.label = "CDC Ethernet (EEM)";
		device_desc.idVendor = cpu_to_le16(EEM_VENDOR_NUM);
		device_desc.idProduct = cpu_to_le16(EEM_PRODUCT_NUM);
	} else if (can_support_ecm(cdev->gadget)) {
		
		eth_config_driver.label = "CDC Ethernet (ECM)";
	} else {
		
		eth_config_driver.label = "CDC Subset/SAFE";

		device_desc.idVendor = cpu_to_le16(SIMPLE_VENDOR_NUM);
		device_desc.idProduct = cpu_to_le16(SIMPLE_PRODUCT_NUM);
		if (!has_rndis())
			device_desc.bDeviceClass = USB_CLASS_VENDOR_SPEC;
	}

	if (has_rndis()) {
		
		device_desc.idVendor = cpu_to_le16(RNDIS_VENDOR_NUM);
		device_desc.idProduct = cpu_to_le16(RNDIS_PRODUCT_NUM);
		device_desc.bNumConfigurations = 2;
	}

	gcnum = usb_gadget_controller_number(gadget);
	if (gcnum >= 0)
		device_desc.bcdDevice = cpu_to_le16(0x0300 | gcnum);
	else {
		dev_warn(&gadget->dev,
				"controller '%s' not recognized; trying %s\n",
				gadget->name,
				eth_config_driver.label);
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

	
	if (has_rndis()) {
		status = usb_add_config(cdev, &rndis_config_driver,
				rndis_do_config);
		if (status < 0)
			goto fail;
	}

	status = usb_add_config(cdev, &eth_config_driver, eth_do_config);
	if (status < 0)
		goto fail;

	dev_info(&gadget->dev, "%s, version: " DRIVER_VERSION "\n",
			DRIVER_DESC);

	return 0;

fail:
	gether_cleanup();
	return status;
}

static int __exit eth_unbind(struct usb_composite_dev *cdev)
{
	gether_cleanup();
	return 0;
}

static struct usb_composite_driver eth_driver = {
	.name		= "g_ether",
	.dev		= &device_desc,
	.strings	= dev_strings,
	.max_speed	= USB_SPEED_SUPER,
	.unbind		= __exit_p(eth_unbind),
};

MODULE_DESCRIPTION(PREFIX DRIVER_DESC);
MODULE_AUTHOR("David Brownell, Benedikt Spanger");
MODULE_LICENSE("GPL");

static int __init init(void)
{
	return usb_composite_probe(&eth_driver, eth_bind);
}
module_init(init);

static void __exit cleanup(void)
{
	usb_composite_unregister(&eth_driver);
}
module_exit(cleanup);
