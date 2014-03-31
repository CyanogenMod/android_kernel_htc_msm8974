/*
 * zero.c -- Gadget Zero, for USB development
 *
 * Copyright (C) 2003-2008 David Brownell
 * Copyright (C) 2008 by Nokia Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */





#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/utsname.h>
#include <linux/device.h>

#include "g_zero.h"
#include "gadget_chips.h"



#include "composite.c"
#include "usbstring.c"
#include "config.c"
#include "epautoconf.c"

#include "f_sourcesink.c"
#include "f_loopback.c"


#define DRIVER_VERSION		"Cinco de Mayo 2008"

static const char longname[] = "Gadget Zero";

unsigned buflen = 4096;
module_param(buflen, uint, 0);

static bool loopdefault = 0;
module_param(loopdefault, bool, S_IRUGO|S_IWUSR);


#ifndef	CONFIG_USB_ZERO_HNPTEST
#define DRIVER_VENDOR_NUM	0x0525		
#define DRIVER_PRODUCT_NUM	0xa4a0		
#define DEFAULT_AUTORESUME	0
#else
#define DRIVER_VENDOR_NUM	0x1a0a		
#define DRIVER_PRODUCT_NUM	0xbadd
#define DEFAULT_AUTORESUME	5
#endif

unsigned autoresume = DEFAULT_AUTORESUME;
module_param(autoresume, uint, S_IRUGO);
MODULE_PARM_DESC(autoresume, "zero, or seconds before remote wakeup");


static struct usb_device_descriptor device_desc = {
	.bLength =		sizeof device_desc,
	.bDescriptorType =	USB_DT_DEVICE,

	.bcdUSB =		cpu_to_le16(0x0200),
	.bDeviceClass =		USB_CLASS_VENDOR_SPEC,

	.idVendor =		cpu_to_le16(DRIVER_VENDOR_NUM),
	.idProduct =		cpu_to_le16(DRIVER_PRODUCT_NUM),
	.bNumConfigurations =	2,
};

#ifdef CONFIG_USB_OTG
static struct usb_otg_descriptor otg_descriptor = {
	.bLength =		sizeof otg_descriptor,
	.bDescriptorType =	USB_DT_OTG,

	.bmAttributes =		USB_OTG_SRP | USB_OTG_HNP,
};

const struct usb_descriptor_header *otg_desc[] = {
	(struct usb_descriptor_header *) &otg_descriptor,
	NULL,
};
#endif


#define STRING_MANUFACTURER_IDX		0
#define STRING_PRODUCT_IDX		1
#define STRING_SERIAL_IDX		2

static char manufacturer[50];

static char serial[] = "0123456789.0123456789.0123456789";

static struct usb_string strings_dev[] = {
	[STRING_MANUFACTURER_IDX].s = manufacturer,
	[STRING_PRODUCT_IDX].s = longname,
	[STRING_SERIAL_IDX].s = serial,
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


struct usb_request *alloc_ep_req(struct usb_ep *ep)
{
	struct usb_request	*req;

	req = usb_ep_alloc_request(ep, GFP_ATOMIC);
	if (req) {
		req->length = buflen;
		req->buf = kmalloc(buflen, GFP_ATOMIC);
		if (!req->buf) {
			usb_ep_free_request(ep, req);
			req = NULL;
		}
	}
	return req;
}

void free_ep_req(struct usb_ep *ep, struct usb_request *req)
{
	kfree(req->buf);
	usb_ep_free_request(ep, req);
}

static void disable_ep(struct usb_composite_dev *cdev, struct usb_ep *ep)
{
	int			value;

	if (ep->driver_data) {
		value = usb_ep_disable(ep);
		if (value < 0)
			DBG(cdev, "disable %s --> %d\n",
					ep->name, value);
		ep->driver_data = NULL;
	}
}

void disable_endpoints(struct usb_composite_dev *cdev,
		struct usb_ep *in, struct usb_ep *out)
{
	disable_ep(cdev, in);
	disable_ep(cdev, out);
}


static struct timer_list	autoresume_timer;

static void zero_autoresume(unsigned long _c)
{
	struct usb_composite_dev	*cdev = (void *)_c;
	struct usb_gadget		*g = cdev->gadget;

	
	if (!cdev->config)
		return;

	if (g->speed != USB_SPEED_UNKNOWN) {
		int status = usb_gadget_wakeup(g);
		INFO(cdev, "%s --> %d\n", __func__, status);
	}
}

static void zero_suspend(struct usb_composite_dev *cdev)
{
	if (cdev->gadget->speed == USB_SPEED_UNKNOWN)
		return;

	if (autoresume) {
		mod_timer(&autoresume_timer, jiffies + (HZ * autoresume));
		DBG(cdev, "suspend, wakeup in %d seconds\n", autoresume);
	} else
		DBG(cdev, "%s\n", __func__);
}

static void zero_resume(struct usb_composite_dev *cdev)
{
	DBG(cdev, "%s\n", __func__);
	del_timer(&autoresume_timer);
}


static int __init zero_bind(struct usb_composite_dev *cdev)
{
	int			gcnum;
	struct usb_gadget	*gadget = cdev->gadget;
	int			id;

	id = usb_string_id(cdev);
	if (id < 0)
		return id;
	strings_dev[STRING_MANUFACTURER_IDX].id = id;
	device_desc.iManufacturer = id;

	id = usb_string_id(cdev);
	if (id < 0)
		return id;
	strings_dev[STRING_PRODUCT_IDX].id = id;
	device_desc.iProduct = id;

	id = usb_string_id(cdev);
	if (id < 0)
		return id;
	strings_dev[STRING_SERIAL_IDX].id = id;
	device_desc.iSerialNumber = id;

	setup_timer(&autoresume_timer, zero_autoresume, (unsigned long) cdev);

	if (loopdefault) {
		loopback_add(cdev, autoresume != 0);
		sourcesink_add(cdev, autoresume != 0);
	} else {
		sourcesink_add(cdev, autoresume != 0);
		loopback_add(cdev, autoresume != 0);
	}

	gcnum = usb_gadget_controller_number(gadget);
	if (gcnum >= 0)
		device_desc.bcdDevice = cpu_to_le16(0x0200 + gcnum);
	else {
		pr_warning("%s: controller '%s' not recognized\n",
			longname, gadget->name);
		device_desc.bcdDevice = cpu_to_le16(0x9999);
	}


	INFO(cdev, "%s, version: " DRIVER_VERSION "\n", longname);

	snprintf(manufacturer, sizeof manufacturer, "%s %s with %s",
		init_utsname()->sysname, init_utsname()->release,
		gadget->name);

	return 0;
}

static int zero_unbind(struct usb_composite_dev *cdev)
{
	del_timer_sync(&autoresume_timer);
	return 0;
}

static struct usb_composite_driver zero_driver = {
	.name		= "zero",
	.dev		= &device_desc,
	.strings	= dev_strings,
	.max_speed	= USB_SPEED_SUPER,
	.unbind		= zero_unbind,
	.suspend	= zero_suspend,
	.resume		= zero_resume,
};

MODULE_AUTHOR("David Brownell");
MODULE_LICENSE("GPL");

static int __init init(void)
{
	return usb_composite_probe(&zero_driver, zero_bind);
}
module_init(init);

static void __exit cleanup(void)
{
	usb_composite_unregister(&zero_driver);
}
module_exit(cleanup);
