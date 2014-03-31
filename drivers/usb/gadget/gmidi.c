/*
 * gmidi.c -- USB MIDI Gadget Driver
 *
 * Copyright (C) 2006 Thumtronics Pty Ltd.
 * Developed for Thumtronics by Grey Innovation
 * Ben Williamson <ben.williamson@greyinnovation.com>
 *
 * This software is distributed under the terms of the GNU General Public
 * License ("GPL") version 2, as published by the Free Software Foundation.
 *
 * This code is based in part on:
 *
 * Gadget Zero driver, Copyright (C) 2003-2004 David Brownell.
 * USB Audio driver, Copyright (C) 2002 by Takashi Iwai.
 * USB MIDI driver, Copyright (C) 2002-2005 Clemens Ladisch.
 *
 * Refer to the USB Device Class Definition for MIDI Devices:
 * http://www.usb.org/developers/devclass_docs/midi10.pdf
 */


#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/utsname.h>
#include <linux/module.h>
#include <linux/device.h>

#include <sound/core.h>
#include <sound/initval.h>
#include <sound/rawmidi.h>

#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb/audio.h>
#include <linux/usb/midi.h>

#include "gadget_chips.h"

#include "composite.c"
#include "usbstring.c"
#include "config.c"
#include "epautoconf.c"
#include "f_midi.c"


MODULE_AUTHOR("Ben Williamson");
MODULE_LICENSE("GPL v2");

static const char shortname[] = "g_midi";
static const char longname[] = "MIDI Gadget";

static int index = SNDRV_DEFAULT_IDX1;
module_param(index, int, S_IRUGO);
MODULE_PARM_DESC(index, "Index value for the USB MIDI Gadget adapter.");

static char *id = SNDRV_DEFAULT_STR1;
module_param(id, charp, S_IRUGO);
MODULE_PARM_DESC(id, "ID string for the USB MIDI Gadget adapter.");

static unsigned int buflen = 256;
module_param(buflen, uint, S_IRUGO);
MODULE_PARM_DESC(buflen, "MIDI buffer length");

static unsigned int qlen = 32;
module_param(qlen, uint, S_IRUGO);
MODULE_PARM_DESC(qlen, "USB read request queue length");

static unsigned int in_ports = 1;
module_param(in_ports, uint, S_IRUGO);
MODULE_PARM_DESC(in_ports, "Number of MIDI input ports");

static unsigned int out_ports = 1;
module_param(out_ports, uint, S_IRUGO);
MODULE_PARM_DESC(out_ports, "Number of MIDI output ports");

#define DRIVER_VENDOR_NUM	0x17b3		
#define DRIVER_PRODUCT_NUM	0x0004		


#define STRING_MANUFACTURER_IDX		0
#define STRING_PRODUCT_IDX		1
#define STRING_DESCRIPTION_IDX		2

static struct usb_device_descriptor device_desc = {
	.bLength =		USB_DT_DEVICE_SIZE,
	.bDescriptorType =	USB_DT_DEVICE,
	.bcdUSB =		__constant_cpu_to_le16(0x0200),
	.bDeviceClass =		USB_CLASS_PER_INTERFACE,
	.idVendor =		__constant_cpu_to_le16(DRIVER_VENDOR_NUM),
	.idProduct =		__constant_cpu_to_le16(DRIVER_PRODUCT_NUM),
	
	
	.bNumConfigurations =	1,
};

static struct usb_string strings_dev[] = {
	[STRING_MANUFACTURER_IDX].s	= "Grey Innovation",
	[STRING_PRODUCT_IDX].s		= "MIDI Gadget",
	[STRING_DESCRIPTION_IDX].s	= "MIDI",
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

static int __exit midi_unbind(struct usb_composite_dev *dev)
{
	return 0;
}

static struct usb_configuration midi_config = {
	.label		= "MIDI Gadget",
	.bConfigurationValue = 1,
	
	.bmAttributes	= USB_CONFIG_ATT_ONE,
	.bMaxPower	= CONFIG_USB_GADGET_VBUS_DRAW / 2,
};

static int __init midi_bind_config(struct usb_configuration *c)
{
	return f_midi_bind_config(c, index, id,
				  in_ports, out_ports,
				  buflen, qlen);
}

static int __init midi_bind(struct usb_composite_dev *cdev)
{
	struct usb_gadget *gadget = cdev->gadget;
	int gcnum, status;

	status = usb_string_id(cdev);
	if (status < 0)
		return status;
	strings_dev[STRING_MANUFACTURER_IDX].id = status;
	device_desc.iManufacturer = status;

	status = usb_string_id(cdev);
	if (status < 0)
		return status;
	strings_dev[STRING_PRODUCT_IDX].id = status;
	device_desc.iProduct = status;

	
	status = usb_string_id(cdev);
	if (status < 0)
		return status;
	strings_dev[STRING_DESCRIPTION_IDX].id = status;

	midi_config.iConfiguration = status;

	gcnum = usb_gadget_controller_number(gadget);
	if (gcnum < 0) {
		pr_warning("%s: controller '%s' not recognized\n",
			   __func__, gadget->name);
		device_desc.bcdDevice = cpu_to_le16(0x9999);
	} else {
		device_desc.bcdDevice = cpu_to_le16(0x0200 + gcnum);
	}

	status = usb_add_config(cdev, &midi_config, midi_bind_config);
	if (status < 0)
		return status;

	pr_info("%s\n", longname);
	return 0;
}

static struct usb_composite_driver midi_driver = {
	.name		= (char *) longname,
	.dev		= &device_desc,
	.strings	= dev_strings,
	.max_speed	= USB_SPEED_HIGH,
	.unbind		= __exit_p(midi_unbind),
};

static int __init midi_init(void)
{
	return usb_composite_probe(&midi_driver, midi_bind);
}
module_init(midi_init);

static void __exit midi_cleanup(void)
{
	usb_composite_unregister(&midi_driver);
}
module_exit(midi_cleanup);

