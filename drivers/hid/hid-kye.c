/*
 *  HID driver for Kye/Genius devices not fully compliant with HID standard
 *
 *  Copyright (c) 2009 Jiri Kosina
 *  Copyright (c) 2009 Tomas Hanak
 *  Copyright (c) 2012 Nikolai Kondrashov
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <linux/device.h>
#include <linux/hid.h>
#include <linux/module.h>
#include <linux/usb.h>
#include "usbhid/usbhid.h"

#include "hid-ids.h"


#define EASYPEN_I405X_RDESC_ORIG_SIZE	476

static __u8 easypen_i405x_rdesc_fixed[] = {
	0x06, 0x00, 0xFF, 
	0x09, 0x01,       
	0xA1, 0x01,       
	0x85, 0x05,       
	0x09, 0x01,       
	0x15, 0x80,       
	0x25, 0x7F,       
	0x75, 0x08,       
	0x95, 0x07,       
	0xB1, 0x02,       
	0xC0,             
	0x05, 0x0D,       
	0x09, 0x02,       
	0xA1, 0x01,       
	0x85, 0x10,       
	0x09, 0x20,       
	0xA0,             
	0x14,             
	0x25, 0x01,       
	0x75, 0x01,       
	0x09, 0x42,       
	0x09, 0x44,       
	0x09, 0x46,       
	0x95, 0x03,       
	0x81, 0x02,       
	0x95, 0x04,       
	0x81, 0x03,       
	0x09, 0x32,       
	0x95, 0x01,       
	0x81, 0x02,       
	0x75, 0x10,       
	0x95, 0x01,       
	0xA4,             
	0x05, 0x01,       
	0x55, 0xFD,       
	0x65, 0x13,       
	0x34,             
	0x09, 0x30,       
	0x46, 0x7C, 0x15, 
	0x26, 0x00, 0x37, 
	0x81, 0x02,       
	0x09, 0x31,       
	0x46, 0xA0, 0x0F, 
	0x26, 0x00, 0x28, 
	0x81, 0x02,       
	0xB4,             
	0x09, 0x30,       
	0x26, 0xFF, 0x03, 
	0x81, 0x02,       
	0xC0,             
	0xC0              
};


#define MOUSEPEN_I608X_RDESC_ORIG_SIZE	476

static __u8 mousepen_i608x_rdesc_fixed[] = {
	0x06, 0x00, 0xFF, 
	0x09, 0x01,       
	0xA1, 0x01,       
	0x85, 0x05,       
	0x09, 0x01,       
	0x15, 0x80,       
	0x25, 0x7F,       
	0x75, 0x08,       
	0x95, 0x07,       
	0xB1, 0x02,       
	0xC0,             
	0x05, 0x0D,       
	0x09, 0x02,       
	0xA1, 0x01,       
	0x85, 0x10,       
	0x09, 0x20,       
	0xA0,             
	0x14,             
	0x25, 0x01,       
	0x75, 0x01,       
	0x09, 0x42,       
	0x09, 0x44,       
	0x09, 0x46,       
	0x95, 0x03,       
	0x81, 0x02,       
	0x95, 0x04,       
	0x81, 0x03,       
	0x09, 0x32,       
	0x95, 0x01,       
	0x81, 0x02,       
	0x75, 0x10,       
	0x95, 0x01,       
	0xA4,             
	0x05, 0x01,       
	0x55, 0xFD,       
	0x65, 0x13,       
	0x34,             
	0x09, 0x30,       
	0x46, 0x40, 0x1F, 
	0x26, 0x00, 0x50, 
	0x81, 0x02,       
	0x09, 0x31,       
	0x46, 0x70, 0x17, 
	0x26, 0x00, 0x3C, 
	0x81, 0x02,       
	0xB4,             
	0x09, 0x30,       
	0x26, 0xFF, 0x03, 
	0x81, 0x02,       
	0xC0,             
	0xC0,             
	0x05, 0x01,       
	0x09, 0x02,       
	0xA1, 0x01,       
	0x85, 0x11,       
	0x09, 0x01,       
	0xA0,             
	0x14,             
	0xA4,             
	0x05, 0x09,       
	0x75, 0x01,       
	0x19, 0x01,       
	0x29, 0x03,       
	0x25, 0x01,       
	0x95, 0x03,       
	0x81, 0x02,       
	0x95, 0x05,       
	0x81, 0x01,       
	0xB4,             
	0x95, 0x01,       
	0xA4,             
	0x55, 0xFD,       
	0x65, 0x13,       
	0x34,             
	0x75, 0x10,       
	0x09, 0x30,       
	0x46, 0x40, 0x1F, 
	0x26, 0x00, 0x50, 
	0x81, 0x02,       
	0x09, 0x31,       
	0x46, 0x70, 0x17, 
	0x26, 0x00, 0x3C, 
	0x81, 0x02,       
	0xB4,             
	0x75, 0x08,       
	0x09, 0x38,       
	0x15, 0xFF,       
	0x25, 0x01,       
	0x81, 0x06,       
	0x81, 0x01,       
	0xC0,             
	0xC0              
};


#define EASYPEN_M610X_RDESC_ORIG_SIZE	476

static __u8 easypen_m610x_rdesc_fixed[] = {
	0x06, 0x00, 0xFF,             
	0x09, 0x01,                   
	0xA1, 0x01,                   
	0x85, 0x05,                   
	0x09, 0x01,                   
	0x15, 0x80,                   
	0x25, 0x7F,                   
	0x75, 0x08,                   
	0x95, 0x07,                   
	0xB1, 0x02,                   
	0xC0,                         
	0x05, 0x0D,                   
	0x09, 0x02,                   
	0xA1, 0x01,                   
	0x85, 0x10,                   
	0x09, 0x20,                   
	0xA0,                         
	0x14,                         
	0x25, 0x01,                   
	0x75, 0x01,                   
	0x09, 0x42,                   
	0x09, 0x44,                   
	0x09, 0x46,                   
	0x95, 0x03,                   
	0x81, 0x02,                   
	0x95, 0x04,                   
	0x81, 0x03,                   
	0x09, 0x32,                   
	0x95, 0x01,                   
	0x81, 0x02,                   
	0x75, 0x10,                   
	0x95, 0x01,                   
	0xA4,                         
	0x05, 0x01,                   
	0x55, 0xFD,                   
	0x65, 0x13,                   
	0x34,                         
	0x09, 0x30,                   
	0x46, 0x10, 0x27,             
	0x27, 0x00, 0xA0, 0x00, 0x00, 
	0x81, 0x02,                   
	0x09, 0x31,                   
	0x46, 0x6A, 0x18,             
	0x26, 0x00, 0x64,             
	0x81, 0x02,                   
	0xB4,                         
	0x09, 0x30,                   
	0x26, 0xFF, 0x03,             
	0x81, 0x02,                   
	0xC0,                         
	0xC0,                         
	0x05, 0x0C,                   
	0x09, 0x01,                   
	0xA1, 0x01,                   
	0x85, 0x12,                   
	0x14,                         
	0x25, 0x01,                   
	0x75, 0x01,                   
	0x95, 0x04,                   
	0x0A, 0x1A, 0x02,             
	0x0A, 0x79, 0x02,             
	0x0A, 0x2D, 0x02,             
	0x0A, 0x2E, 0x02,             
	0x81, 0x02,                   
	0x95, 0x01,                   
	0x75, 0x14,                   
	0x81, 0x03,                   
	0x75, 0x20,                   
	0x81, 0x03,                   
	0xC0                          
};

static __u8 *kye_report_fixup(struct hid_device *hdev, __u8 *rdesc,
		unsigned int *rsize)
{
	switch (hdev->product) {
	case USB_DEVICE_ID_KYE_ERGO_525V:
		if (*rsize >= 74 &&
			rdesc[61] == 0x05 && rdesc[62] == 0x08 &&
			rdesc[63] == 0x19 && rdesc[64] == 0x08 &&
			rdesc[65] == 0x29 && rdesc[66] == 0x0f &&
			rdesc[71] == 0x75 && rdesc[72] == 0x08 &&
			rdesc[73] == 0x95 && rdesc[74] == 0x01) {
			hid_info(hdev,
				 "fixing up Kye/Genius Ergo Mouse "
				 "report descriptor\n");
			rdesc[62] = 0x09;
			rdesc[64] = 0x04;
			rdesc[66] = 0x07;
			rdesc[72] = 0x01;
			rdesc[74] = 0x08;
		}
		break;
	case USB_DEVICE_ID_KYE_EASYPEN_I405X:
		if (*rsize == EASYPEN_I405X_RDESC_ORIG_SIZE) {
			rdesc = easypen_i405x_rdesc_fixed;
			*rsize = sizeof(easypen_i405x_rdesc_fixed);
		}
		break;
	case USB_DEVICE_ID_KYE_MOUSEPEN_I608X:
		if (*rsize == MOUSEPEN_I608X_RDESC_ORIG_SIZE) {
			rdesc = mousepen_i608x_rdesc_fixed;
			*rsize = sizeof(mousepen_i608x_rdesc_fixed);
		}
		break;
	case USB_DEVICE_ID_KYE_EASYPEN_M610X:
		if (*rsize == EASYPEN_M610X_RDESC_ORIG_SIZE) {
			rdesc = easypen_m610x_rdesc_fixed;
			*rsize = sizeof(easypen_m610x_rdesc_fixed);
		}
		break;
	}
	return rdesc;
}

static int kye_tablet_enable(struct hid_device *hdev)
{
	struct list_head *list;
	struct list_head *head;
	struct hid_report *report;
	__s32 *value;

	list = &hdev->report_enum[HID_FEATURE_REPORT].report_list;
	list_for_each(head, list) {
		report = list_entry(head, struct hid_report, list);
		if (report->id == 5)
			break;
	}

	if (head == list) {
		hid_err(hdev, "tablet-enabling feature report not found\n");
		return -ENODEV;
	}

	if (report->maxfield < 1 || report->field[0]->report_count < 7) {
		hid_err(hdev, "invalid tablet-enabling feature report\n");
		return -ENODEV;
	}

	value = report->field[0]->value;

	value[0] = 0x12;
	value[1] = 0x10;
	value[2] = 0x11;
	value[3] = 0x12;
	value[4] = 0x00;
	value[5] = 0x00;
	value[6] = 0x00;
	usbhid_submit_report(hdev, report, USB_DIR_OUT);

	return 0;
}

static int kye_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	int ret;

	ret = hid_parse(hdev);
	if (ret) {
		hid_err(hdev, "parse failed\n");
		goto err;
	}

	ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
	if (ret) {
		hid_err(hdev, "hw start failed\n");
		goto err;
	}

	switch (id->product) {
	case USB_DEVICE_ID_KYE_EASYPEN_I405X:
	case USB_DEVICE_ID_KYE_MOUSEPEN_I608X:
	case USB_DEVICE_ID_KYE_EASYPEN_M610X:
		ret = kye_tablet_enable(hdev);
		if (ret) {
			hid_err(hdev, "tablet enabling failed\n");
			goto enabling_err;
		}
		break;
	}

	return 0;
enabling_err:
	hid_hw_stop(hdev);
err:
	return ret;
}

static const struct hid_device_id kye_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_KYE, USB_DEVICE_ID_KYE_ERGO_525V) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_KYE,
				USB_DEVICE_ID_KYE_EASYPEN_I405X) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_KYE,
				USB_DEVICE_ID_KYE_MOUSEPEN_I608X) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_KYE,
				USB_DEVICE_ID_KYE_EASYPEN_M610X) },
	{ }
};
MODULE_DEVICE_TABLE(hid, kye_devices);

static struct hid_driver kye_driver = {
	.name = "kye",
	.id_table = kye_devices,
	.probe = kye_probe,
	.report_fixup = kye_report_fixup,
};

static int __init kye_init(void)
{
	return hid_register_driver(&kye_driver);
}

static void __exit kye_exit(void)
{
	hid_unregister_driver(&kye_driver);
}

module_init(kye_init);
module_exit(kye_exit);
MODULE_LICENSE("GPL");
