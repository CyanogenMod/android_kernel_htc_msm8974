/*
 * nokia.c -- Nokia Composite Gadget Driver
 *
 * Copyright (C) 2008-2010 Nokia Corporation
 * Contact: Felipe Balbi <felipe.balbi@nokia.com>
 *
 * This gadget driver borrows from serial.c which is:
 *
 * Copyright (C) 2003 Al Borchers (alborchers@steinerpoint.com)
 * Copyright (C) 2008 by David Brownell
 * Copyright (C) 2008 by Nokia Corporation
 *
 * This software is distributed under the terms of the GNU General
 * Public License ("GPL") as published by the Free Software Foundation,
 * version 2 of that License.
 */

#include <linux/kernel.h>
#include <linux/utsname.h>
#include <linux/device.h>

#include "u_serial.h"
#include "u_ether.h"
#include "u_phonet.h"
#include "gadget_chips.h"


#define NOKIA_VERSION_NUM		0x0211
#define NOKIA_LONG_NAME			"N900 (PC-Suite Mode)"


#include "composite.c"
#include "usbstring.c"
#include "config.c"
#include "epautoconf.c"

#include "u_serial.c"
#include "f_acm.c"
#include "f_ecm.c"
#include "f_obex.c"
#include "f_serial.c"
#include "f_phonet.c"
#include "u_ether.c"


#define NOKIA_VENDOR_ID			0x0421	
#define NOKIA_PRODUCT_ID		0x01c8	


#define STRING_MANUFACTURER_IDX		0
#define STRING_PRODUCT_IDX		1
#define STRING_DESCRIPTION_IDX		2

static char manufacturer_nokia[] = "Nokia";
static const char product_nokia[] = NOKIA_LONG_NAME;
static const char description_nokia[] = "PC-Suite Configuration";

static struct usb_string strings_dev[] = {
	[STRING_MANUFACTURER_IDX].s = manufacturer_nokia,
	[STRING_PRODUCT_IDX].s = NOKIA_LONG_NAME,
	[STRING_DESCRIPTION_IDX].s = description_nokia,
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

static struct usb_device_descriptor device_desc = {
	.bLength		= USB_DT_DEVICE_SIZE,
	.bDescriptorType	= USB_DT_DEVICE,
	.bcdUSB			= __constant_cpu_to_le16(0x0200),
	.bDeviceClass		= USB_CLASS_COMM,
	.idVendor		= __constant_cpu_to_le16(NOKIA_VENDOR_ID),
	.idProduct		= __constant_cpu_to_le16(NOKIA_PRODUCT_ID),
	
	
	.bNumConfigurations =	1,
};


MODULE_DESCRIPTION("Nokia composite gadget driver for N900");
MODULE_AUTHOR("Felipe Balbi");
MODULE_LICENSE("GPL");


static u8 hostaddr[ETH_ALEN];

static int __init nokia_bind_config(struct usb_configuration *c)
{
	int status = 0;

	status = phonet_bind_config(c);
	if (status)
		printk(KERN_DEBUG "could not bind phonet config\n");

	status = obex_bind_config(c, 0);
	if (status)
		printk(KERN_DEBUG "could not bind obex config %d\n", 0);

	status = obex_bind_config(c, 1);
	if (status)
		printk(KERN_DEBUG "could not bind obex config %d\n", 0);

	status = acm_bind_config(c, 2);
	if (status)
		printk(KERN_DEBUG "could not bind acm config\n");

	status = ecm_bind_config(c, hostaddr);
	if (status)
		printk(KERN_DEBUG "could not bind ecm config\n");

	return status;
}

static struct usb_configuration nokia_config_500ma_driver = {
	.label		= "Bus Powered",
	.bConfigurationValue = 1,
	
	.bmAttributes	= USB_CONFIG_ATT_ONE,
	.bMaxPower	= 250, 
};

static struct usb_configuration nokia_config_100ma_driver = {
	.label		= "Self Powered",
	.bConfigurationValue = 2,
	
	.bmAttributes	= USB_CONFIG_ATT_ONE | USB_CONFIG_ATT_SELFPOWER,
	.bMaxPower	= 50, 
};

static int __init nokia_bind(struct usb_composite_dev *cdev)
{
	int			gcnum;
	struct usb_gadget	*gadget = cdev->gadget;
	int			status;

	status = gphonet_setup(cdev->gadget);
	if (status < 0)
		goto err_phonet;

	status = gserial_setup(cdev->gadget, 3);
	if (status < 0)
		goto err_serial;

	status = gether_setup(cdev->gadget, hostaddr);
	if (status < 0)
		goto err_ether;

	status = usb_string_id(cdev);
	if (status < 0)
		goto err_usb;
	strings_dev[STRING_MANUFACTURER_IDX].id = status;

	device_desc.iManufacturer = status;

	status = usb_string_id(cdev);
	if (status < 0)
		goto err_usb;
	strings_dev[STRING_PRODUCT_IDX].id = status;

	device_desc.iProduct = status;

	
	status = usb_string_id(cdev);
	if (status < 0)
		goto err_usb;
	strings_dev[STRING_DESCRIPTION_IDX].id = status;

	nokia_config_500ma_driver.iConfiguration = status;
	nokia_config_100ma_driver.iConfiguration = status;

	
	gcnum = usb_gadget_controller_number(gadget);
	if (gcnum >= 0)
		device_desc.bcdDevice = cpu_to_le16(NOKIA_VERSION_NUM);
	else {
		pr_err("nokia_bind: controller '%s' not recognized\n",
			gadget->name);
		goto err_usb;
	}

	
	status = usb_add_config(cdev, &nokia_config_500ma_driver,
			nokia_bind_config);
	if (status < 0)
		goto err_usb;

	status = usb_add_config(cdev, &nokia_config_100ma_driver,
			nokia_bind_config);
	if (status < 0)
		goto err_usb;

	dev_info(&gadget->dev, "%s\n", NOKIA_LONG_NAME);

	return 0;

err_usb:
	gether_cleanup();
err_ether:
	gserial_cleanup();
err_serial:
	gphonet_cleanup();
err_phonet:
	return status;
}

static int __exit nokia_unbind(struct usb_composite_dev *cdev)
{
	gphonet_cleanup();
	gserial_cleanup();
	gether_cleanup();

	return 0;
}

static struct usb_composite_driver nokia_driver = {
	.name		= "g_nokia",
	.dev		= &device_desc,
	.strings	= dev_strings,
	.max_speed	= USB_SPEED_HIGH,
	.unbind		= __exit_p(nokia_unbind),
};

static int __init nokia_init(void)
{
	return usb_composite_probe(&nokia_driver, nokia_bind);
}
module_init(nokia_init);

static void __exit nokia_cleanup(void)
{
	usb_composite_unregister(&nokia_driver);
}
module_exit(nokia_cleanup);

