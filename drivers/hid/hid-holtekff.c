/*
 *  Force feedback support for Holtek On Line Grip based gamepads
 *
 *  These include at least a Brazilian "Clone Joypad Super Power Fire"
 *  which uses vendor ID 0x1241 and identifies as "HOLTEK On Line Grip".
 *
 *  Copyright (c) 2011 Anssi Hannula <anssi.hannula@iki.fi>
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/hid.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/usb.h>

#include "hid-ids.h"

#ifdef CONFIG_HOLTEK_FF
#include "usbhid/usbhid.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anssi Hannula <anssi.hannula@iki.fi>");
MODULE_DESCRIPTION("Force feedback support for Holtek On Line Grip based devices");


#define HOLTEKFF_MSG_LENGTH     7

static const u8 start_effect_1[] = { 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00 };
static const u8 stop_all4[] =	   { 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static const u8 stop_all6[] =	   { 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

struct holtekff_device {
	struct hid_field *field;
};

static void holtekff_send(struct holtekff_device *holtekff,
			  struct hid_device *hid,
			  const u8 data[HOLTEKFF_MSG_LENGTH])
{
	int i;

	for (i = 0; i < HOLTEKFF_MSG_LENGTH; i++) {
		holtekff->field->value[i] = data[i];
	}

	dbg_hid("sending %02x %02x %02x %02x %02x %02x %02x\n", data[0],
		data[1], data[2], data[3], data[4], data[5], data[6]);

	usbhid_submit_report(hid, holtekff->field->report, USB_DIR_OUT);
}

static int holtekff_play(struct input_dev *dev, void *data,
			 struct ff_effect *effect)
{
	struct hid_device *hid = input_get_drvdata(dev);
	struct holtekff_device *holtekff = data;
	int left, right;
	
	u8 buf[HOLTEKFF_MSG_LENGTH] =
		{ 0x01, 0x01, 0xff, 0xff, 0x10, 0xe0, 0x00 };

	left = effect->u.rumble.strong_magnitude;
	right = effect->u.rumble.weak_magnitude;
	dbg_hid("called with 0x%04x 0x%04x\n", left, right);

	if (!left && !right) {
		holtekff_send(holtekff, hid, stop_all6);
		return 0;
	}

	if (left)
		buf[1] |= 0x80;
	if (right)
		buf[1] |= 0x40;

	
	buf[6] = min(0xf, (left >> 12) + (right >> 12));

	holtekff_send(holtekff, hid, buf);
	holtekff_send(holtekff, hid, start_effect_1);

	return 0;
}

static int holtekff_init(struct hid_device *hid)
{
	struct holtekff_device *holtekff;
	struct hid_report *report;
	struct hid_input *hidinput = list_entry(hid->inputs.next,
						struct hid_input, list);
	struct list_head *report_list =
			&hid->report_enum[HID_OUTPUT_REPORT].report_list;
	struct input_dev *dev = hidinput->input;
	int error;

	if (list_empty(report_list)) {
		hid_err(hid, "no output report found\n");
		return -ENODEV;
	}

	report = list_entry(report_list->next, struct hid_report, list);

	if (report->maxfield < 1 || report->field[0]->report_count != 7) {
		hid_err(hid, "unexpected output report layout\n");
		return -ENODEV;
	}

	holtekff = kzalloc(sizeof(*holtekff), GFP_KERNEL);
	if (!holtekff)
		return -ENOMEM;

	set_bit(FF_RUMBLE, dev->ffbit);

	holtekff->field = report->field[0];

	
	holtekff_send(holtekff, hid, stop_all4);
	holtekff_send(holtekff, hid, stop_all6);

	error = input_ff_create_memless(dev, holtekff, holtekff_play);
	if (error) {
		kfree(holtekff);
		return error;
	}

	hid_info(hid, "Force feedback for Holtek On Line Grip based devices by Anssi Hannula <anssi.hannula@iki.fi>\n");

	return 0;
}
#else
static inline int holtekff_init(struct hid_device *hid)
{
	return 0;
}
#endif

static int holtek_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	int ret;

	ret = hid_parse(hdev);
	if (ret) {
		hid_err(hdev, "parse failed\n");
		goto err;
	}

	ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT & ~HID_CONNECT_FF);
	if (ret) {
		hid_err(hdev, "hw start failed\n");
		goto err;
	}

	holtekff_init(hdev);

	return 0;
err:
	return ret;
}

static const struct hid_device_id holtek_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_HOLTEK, USB_DEVICE_ID_HOLTEK_ON_LINE_GRIP) },
	{ }
};
MODULE_DEVICE_TABLE(hid, holtek_devices);

static struct hid_driver holtek_driver = {
	.name = "holtek",
	.id_table = holtek_devices,
	.probe = holtek_probe,
};

static int __init holtek_init(void)
{
	return hid_register_driver(&holtek_driver);
}

static void __exit holtek_exit(void)
{
	hid_unregister_driver(&holtek_driver);
}

module_init(holtek_init);
module_exit(holtek_exit);

