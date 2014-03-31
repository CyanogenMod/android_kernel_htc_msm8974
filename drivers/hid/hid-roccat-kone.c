/*
 * Roccat Kone driver for Linux
 *
 * Copyright (c) 2010 Stefan Achatz <erazor_de@users.sourceforge.net>
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */


#include <linux/device.h>
#include <linux/input.h>
#include <linux/hid.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/hid-roccat.h>
#include "hid-ids.h"
#include "hid-roccat-common.h"
#include "hid-roccat-kone.h"

static uint profile_numbers[5] = {0, 1, 2, 3, 4};

static void kone_profile_activated(struct kone_device *kone, uint new_profile)
{
	kone->actual_profile = new_profile;
	kone->actual_dpi = kone->profiles[new_profile - 1].startup_dpi;
}

static void kone_profile_report(struct kone_device *kone, uint new_profile)
{
	struct kone_roccat_report roccat_report;
	roccat_report.event = kone_mouse_event_switch_profile;
	roccat_report.value = new_profile;
	roccat_report.key = 0;
	roccat_report_event(kone->chrdev_minor, (uint8_t *)&roccat_report);
}

static int kone_receive(struct usb_device *usb_dev, uint usb_command,
		void *data, uint size)
{
	char *buf;
	int len;

	buf = kmalloc(size, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	len = usb_control_msg(usb_dev, usb_rcvctrlpipe(usb_dev, 0),
			HID_REQ_GET_REPORT,
			USB_TYPE_CLASS | USB_RECIP_INTERFACE | USB_DIR_IN,
			usb_command, 0, buf, size, USB_CTRL_SET_TIMEOUT);

	memcpy(data, buf, size);
	kfree(buf);
	return ((len < 0) ? len : ((len != size) ? -EIO : 0));
}

static int kone_send(struct usb_device *usb_dev, uint usb_command,
		void const *data, uint size)
{
	char *buf;
	int len;

	buf = kmemdup(data, size, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	len = usb_control_msg(usb_dev, usb_sndctrlpipe(usb_dev, 0),
			HID_REQ_SET_REPORT,
			USB_TYPE_CLASS | USB_RECIP_INTERFACE | USB_DIR_OUT,
			usb_command, 0, buf, size, USB_CTRL_SET_TIMEOUT);

	kfree(buf);
	return ((len < 0) ? len : ((len != size) ? -EIO : 0));
}

static struct class *kone_class;

static void kone_set_settings_checksum(struct kone_settings *settings)
{
	uint16_t checksum = 0;
	unsigned char *address = (unsigned char *)settings;
	int i;

	for (i = 0; i < sizeof(struct kone_settings) - 2; ++i, ++address)
		checksum += *address;
	settings->checksum = cpu_to_le16(checksum);
}

static int kone_check_write(struct usb_device *usb_dev)
{
	int retval;
	uint8_t data;

	do {
		msleep(80);

		retval = kone_receive(usb_dev,
				kone_command_confirm_write, &data, 1);
		if (retval)
			return retval;

	} while (data == 3);

	if (data == 1) 
		return 0;

	
	hid_err(usb_dev, "got retval %d when checking write\n", data);
	return -EIO;
}

static int kone_get_settings(struct usb_device *usb_dev,
		struct kone_settings *buf)
{
	return kone_receive(usb_dev, kone_command_settings, buf,
			sizeof(struct kone_settings));
}

static int kone_set_settings(struct usb_device *usb_dev,
		struct kone_settings const *settings)
{
	int retval;
	retval = kone_send(usb_dev, kone_command_settings,
			settings, sizeof(struct kone_settings));
	if (retval)
		return retval;
	return kone_check_write(usb_dev);
}

static int kone_get_profile(struct usb_device *usb_dev,
		struct kone_profile *buf, int number)
{
	int len;

	if (number < 1 || number > 5)
		return -EINVAL;

	len = usb_control_msg(usb_dev, usb_rcvctrlpipe(usb_dev, 0),
			USB_REQ_CLEAR_FEATURE,
			USB_TYPE_CLASS | USB_RECIP_INTERFACE | USB_DIR_IN,
			kone_command_profile, number, buf,
			sizeof(struct kone_profile), USB_CTRL_SET_TIMEOUT);

	if (len != sizeof(struct kone_profile))
		return -EIO;

	return 0;
}

static int kone_set_profile(struct usb_device *usb_dev,
		struct kone_profile const *profile, int number)
{
	int len;

	if (number < 1 || number > 5)
		return -EINVAL;

	len = usb_control_msg(usb_dev, usb_sndctrlpipe(usb_dev, 0),
			USB_REQ_SET_CONFIGURATION,
			USB_TYPE_CLASS | USB_RECIP_INTERFACE | USB_DIR_OUT,
			kone_command_profile, number, (void *)profile,
			sizeof(struct kone_profile),
			USB_CTRL_SET_TIMEOUT);

	if (len != sizeof(struct kone_profile))
		return len;

	if (kone_check_write(usb_dev))
		return -EIO;

	return 0;
}

static int kone_get_weight(struct usb_device *usb_dev, int *result)
{
	int retval;
	uint8_t data;

	retval = kone_receive(usb_dev, kone_command_weight, &data, 1);

	if (retval)
		return retval;

	*result = (int)data;
	return 0;
}

static int kone_get_firmware_version(struct usb_device *usb_dev, int *result)
{
	int retval;
	uint16_t data;

	retval = kone_receive(usb_dev, kone_command_firmware_version,
			&data, 2);
	if (retval)
		return retval;

	*result = le16_to_cpu(data);
	return 0;
}

static ssize_t kone_sysfs_read_settings(struct file *fp, struct kobject *kobj,
		struct bin_attribute *attr, char *buf,
		loff_t off, size_t count) {
	struct device *dev =
			container_of(kobj, struct device, kobj)->parent->parent;
	struct kone_device *kone = hid_get_drvdata(dev_get_drvdata(dev));

	if (off >= sizeof(struct kone_settings))
		return 0;

	if (off + count > sizeof(struct kone_settings))
		count = sizeof(struct kone_settings) - off;

	mutex_lock(&kone->kone_lock);
	memcpy(buf, ((char const *)&kone->settings) + off, count);
	mutex_unlock(&kone->kone_lock);

	return count;
}

static ssize_t kone_sysfs_write_settings(struct file *fp, struct kobject *kobj,
		struct bin_attribute *attr, char *buf,
		loff_t off, size_t count) {
	struct device *dev =
			container_of(kobj, struct device, kobj)->parent->parent;
	struct kone_device *kone = hid_get_drvdata(dev_get_drvdata(dev));
	struct usb_device *usb_dev = interface_to_usbdev(to_usb_interface(dev));
	int retval = 0, difference, old_profile;

	
	if (off != 0 || count != sizeof(struct kone_settings))
		return -EINVAL;

	mutex_lock(&kone->kone_lock);
	difference = memcmp(buf, &kone->settings, sizeof(struct kone_settings));
	if (difference) {
		retval = kone_set_settings(usb_dev,
				(struct kone_settings const *)buf);
		if (retval) {
			mutex_unlock(&kone->kone_lock);
			return retval;
		}

		old_profile = kone->settings.startup_profile;
		memcpy(&kone->settings, buf, sizeof(struct kone_settings));

		kone_profile_activated(kone, kone->settings.startup_profile);

		if (kone->settings.startup_profile != old_profile)
			kone_profile_report(kone, kone->settings.startup_profile);
	}
	mutex_unlock(&kone->kone_lock);

	return sizeof(struct kone_settings);
}

static ssize_t kone_sysfs_read_profilex(struct file *fp,
		struct kobject *kobj, struct bin_attribute *attr,
		char *buf, loff_t off, size_t count) {
	struct device *dev =
			container_of(kobj, struct device, kobj)->parent->parent;
	struct kone_device *kone = hid_get_drvdata(dev_get_drvdata(dev));

	if (off >= sizeof(struct kone_profile))
		return 0;

	if (off + count > sizeof(struct kone_profile))
		count = sizeof(struct kone_profile) - off;

	mutex_lock(&kone->kone_lock);
	memcpy(buf, ((char const *)&kone->profiles[*(uint *)(attr->private)]) + off, count);
	mutex_unlock(&kone->kone_lock);

	return count;
}

static ssize_t kone_sysfs_write_profilex(struct file *fp,
		struct kobject *kobj, struct bin_attribute *attr,
		char *buf, loff_t off, size_t count) {
	struct device *dev =
			container_of(kobj, struct device, kobj)->parent->parent;
	struct kone_device *kone = hid_get_drvdata(dev_get_drvdata(dev));
	struct usb_device *usb_dev = interface_to_usbdev(to_usb_interface(dev));
	struct kone_profile *profile;
	int retval = 0, difference;

	
	if (off != 0 || count != sizeof(struct kone_profile))
		return -EINVAL;

	profile = &kone->profiles[*(uint *)(attr->private)];

	mutex_lock(&kone->kone_lock);
	difference = memcmp(buf, profile, sizeof(struct kone_profile));
	if (difference) {
		retval = kone_set_profile(usb_dev,
				(struct kone_profile const *)buf,
				*(uint *)(attr->private) + 1);
		if (!retval)
			memcpy(profile, buf, sizeof(struct kone_profile));
	}
	mutex_unlock(&kone->kone_lock);

	if (retval)
		return retval;

	return sizeof(struct kone_profile);
}

static ssize_t kone_sysfs_show_actual_profile(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct kone_device *kone =
			hid_get_drvdata(dev_get_drvdata(dev->parent->parent));
	return snprintf(buf, PAGE_SIZE, "%d\n", kone->actual_profile);
}

static ssize_t kone_sysfs_show_actual_dpi(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct kone_device *kone =
			hid_get_drvdata(dev_get_drvdata(dev->parent->parent));
	return snprintf(buf, PAGE_SIZE, "%d\n", kone->actual_dpi);
}

static ssize_t kone_sysfs_show_weight(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct kone_device *kone;
	struct usb_device *usb_dev;
	int weight = 0;
	int retval;

	dev = dev->parent->parent;
	kone = hid_get_drvdata(dev_get_drvdata(dev));
	usb_dev = interface_to_usbdev(to_usb_interface(dev));

	mutex_lock(&kone->kone_lock);
	retval = kone_get_weight(usb_dev, &weight);
	mutex_unlock(&kone->kone_lock);

	if (retval)
		return retval;
	return snprintf(buf, PAGE_SIZE, "%d\n", weight);
}

static ssize_t kone_sysfs_show_firmware_version(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct kone_device *kone =
			hid_get_drvdata(dev_get_drvdata(dev->parent->parent));
	return snprintf(buf, PAGE_SIZE, "%d\n", kone->firmware_version);
}

static ssize_t kone_sysfs_show_tcu(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct kone_device *kone =
			hid_get_drvdata(dev_get_drvdata(dev->parent->parent));
	return snprintf(buf, PAGE_SIZE, "%d\n", kone->settings.tcu);
}

static int kone_tcu_command(struct usb_device *usb_dev, int number)
{
	unsigned char value;
	value = number;
	return kone_send(usb_dev, kone_command_calibrate, &value, 1);
}

static ssize_t kone_sysfs_set_tcu(struct device *dev,
		struct device_attribute *attr, char const *buf, size_t size)
{
	struct kone_device *kone;
	struct usb_device *usb_dev;
	int retval;
	unsigned long state;

	dev = dev->parent->parent;
	kone = hid_get_drvdata(dev_get_drvdata(dev));
	usb_dev = interface_to_usbdev(to_usb_interface(dev));

	retval = strict_strtoul(buf, 10, &state);
	if (retval)
		return retval;

	if (state != 0 && state != 1)
		return -EINVAL;

	mutex_lock(&kone->kone_lock);

	if (state == 1) { 
		retval = kone_tcu_command(usb_dev, 1);
		if (retval)
			goto exit_unlock;
		retval = kone_tcu_command(usb_dev, 2);
		if (retval)
			goto exit_unlock;
		ssleep(5); 
		retval = kone_tcu_command(usb_dev, 3);
		if (retval)
			goto exit_unlock;
		retval = kone_tcu_command(usb_dev, 0);
		if (retval)
			goto exit_unlock;
		retval = kone_tcu_command(usb_dev, 4);
		if (retval)
			goto exit_unlock;
		ssleep(1);
	}

	
	retval = kone_get_settings(usb_dev, &kone->settings);
	if (retval)
		goto exit_no_settings;

	
	if (kone->settings.tcu != state) {
		kone->settings.tcu = state;
		kone_set_settings_checksum(&kone->settings);

		retval = kone_set_settings(usb_dev, &kone->settings);
		if (retval) {
			hid_err(usb_dev, "couldn't set tcu state\n");
			retval = kone_get_settings(usb_dev, &kone->settings);
			if (retval)
				goto exit_no_settings;
			goto exit_unlock;
		}
		
		kone_profile_activated(kone, kone->settings.startup_profile);
	}

	retval = size;
exit_no_settings:
	hid_err(usb_dev, "couldn't read settings\n");
exit_unlock:
	mutex_unlock(&kone->kone_lock);
	return retval;
}

static ssize_t kone_sysfs_show_startup_profile(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct kone_device *kone =
			hid_get_drvdata(dev_get_drvdata(dev->parent->parent));
	return snprintf(buf, PAGE_SIZE, "%d\n", kone->settings.startup_profile);
}

static ssize_t kone_sysfs_set_startup_profile(struct device *dev,
		struct device_attribute *attr, char const *buf, size_t size)
{
	struct kone_device *kone;
	struct usb_device *usb_dev;
	int retval;
	unsigned long new_startup_profile;

	dev = dev->parent->parent;
	kone = hid_get_drvdata(dev_get_drvdata(dev));
	usb_dev = interface_to_usbdev(to_usb_interface(dev));

	retval = strict_strtoul(buf, 10, &new_startup_profile);
	if (retval)
		return retval;

	if (new_startup_profile  < 1 || new_startup_profile > 5)
		return -EINVAL;

	mutex_lock(&kone->kone_lock);

	kone->settings.startup_profile = new_startup_profile;
	kone_set_settings_checksum(&kone->settings);

	retval = kone_set_settings(usb_dev, &kone->settings);
	if (retval) {
		mutex_unlock(&kone->kone_lock);
		return retval;
	}

	
	kone_profile_activated(kone, new_startup_profile);
	kone_profile_report(kone, new_startup_profile);

	mutex_unlock(&kone->kone_lock);
	return size;
}

static struct device_attribute kone_attributes[] = {
	__ATTR(actual_dpi, 0440, kone_sysfs_show_actual_dpi, NULL),
	__ATTR(actual_profile, 0440, kone_sysfs_show_actual_profile, NULL),

	__ATTR(weight, 0440, kone_sysfs_show_weight, NULL),

	__ATTR(firmware_version, 0440,
			kone_sysfs_show_firmware_version, NULL),

	__ATTR(tcu, 0660, kone_sysfs_show_tcu, kone_sysfs_set_tcu),

	
	__ATTR(startup_profile, 0660,
			kone_sysfs_show_startup_profile,
			kone_sysfs_set_startup_profile),
	__ATTR_NULL
};

static struct bin_attribute kone_bin_attributes[] = {
	{
		.attr = { .name = "settings", .mode = 0660 },
		.size = sizeof(struct kone_settings),
		.read = kone_sysfs_read_settings,
		.write = kone_sysfs_write_settings
	},
	{
		.attr = { .name = "profile1", .mode = 0660 },
		.size = sizeof(struct kone_profile),
		.read = kone_sysfs_read_profilex,
		.write = kone_sysfs_write_profilex,
		.private = &profile_numbers[0]
	},
	{
		.attr = { .name = "profile2", .mode = 0660 },
		.size = sizeof(struct kone_profile),
		.read = kone_sysfs_read_profilex,
		.write = kone_sysfs_write_profilex,
		.private = &profile_numbers[1]
	},
	{
		.attr = { .name = "profile3", .mode = 0660 },
		.size = sizeof(struct kone_profile),
		.read = kone_sysfs_read_profilex,
		.write = kone_sysfs_write_profilex,
		.private = &profile_numbers[2]
	},
	{
		.attr = { .name = "profile4", .mode = 0660 },
		.size = sizeof(struct kone_profile),
		.read = kone_sysfs_read_profilex,
		.write = kone_sysfs_write_profilex,
		.private = &profile_numbers[3]
	},
	{
		.attr = { .name = "profile5", .mode = 0660 },
		.size = sizeof(struct kone_profile),
		.read = kone_sysfs_read_profilex,
		.write = kone_sysfs_write_profilex,
		.private = &profile_numbers[4]
	},
	__ATTR_NULL
};

static int kone_init_kone_device_struct(struct usb_device *usb_dev,
		struct kone_device *kone)
{
	uint i;
	int retval;

	mutex_init(&kone->kone_lock);

	for (i = 0; i < 5; ++i) {
		retval = kone_get_profile(usb_dev, &kone->profiles[i], i + 1);
		if (retval)
			return retval;
	}

	retval = kone_get_settings(usb_dev, &kone->settings);
	if (retval)
		return retval;

	retval = kone_get_firmware_version(usb_dev, &kone->firmware_version);
	if (retval)
		return retval;

	kone_profile_activated(kone, kone->settings.startup_profile);

	return 0;
}

static int kone_init_specials(struct hid_device *hdev)
{
	struct usb_interface *intf = to_usb_interface(hdev->dev.parent);
	struct usb_device *usb_dev = interface_to_usbdev(intf);
	struct kone_device *kone;
	int retval;

	if (intf->cur_altsetting->desc.bInterfaceProtocol
			== USB_INTERFACE_PROTOCOL_MOUSE) {

		kone = kzalloc(sizeof(*kone), GFP_KERNEL);
		if (!kone) {
			hid_err(hdev, "can't alloc device descriptor\n");
			return -ENOMEM;
		}
		hid_set_drvdata(hdev, kone);

		retval = kone_init_kone_device_struct(usb_dev, kone);
		if (retval) {
			hid_err(hdev, "couldn't init struct kone_device\n");
			goto exit_free;
		}

		retval = roccat_connect(kone_class, hdev,
				sizeof(struct kone_roccat_report));
		if (retval < 0) {
			hid_err(hdev, "couldn't init char dev\n");
			
		} else {
			kone->roccat_claimed = 1;
			kone->chrdev_minor = retval;
		}
	} else {
		hid_set_drvdata(hdev, NULL);
	}

	return 0;
exit_free:
	kfree(kone);
	return retval;
}

static void kone_remove_specials(struct hid_device *hdev)
{
	struct usb_interface *intf = to_usb_interface(hdev->dev.parent);
	struct kone_device *kone;

	if (intf->cur_altsetting->desc.bInterfaceProtocol
			== USB_INTERFACE_PROTOCOL_MOUSE) {
		kone = hid_get_drvdata(hdev);
		if (kone->roccat_claimed)
			roccat_disconnect(kone->chrdev_minor);
		kfree(hid_get_drvdata(hdev));
	}
}

static int kone_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	int retval;

	retval = hid_parse(hdev);
	if (retval) {
		hid_err(hdev, "parse failed\n");
		goto exit;
	}

	retval = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
	if (retval) {
		hid_err(hdev, "hw start failed\n");
		goto exit;
	}

	retval = kone_init_specials(hdev);
	if (retval) {
		hid_err(hdev, "couldn't install mouse\n");
		goto exit_stop;
	}

	return 0;

exit_stop:
	hid_hw_stop(hdev);
exit:
	return retval;
}

static void kone_remove(struct hid_device *hdev)
{
	kone_remove_specials(hdev);
	hid_hw_stop(hdev);
}

static void kone_keep_values_up_to_date(struct kone_device *kone,
		struct kone_mouse_event const *event)
{
	switch (event->event) {
	case kone_mouse_event_switch_profile:
		kone->actual_dpi = kone->profiles[event->value - 1].
				startup_dpi;
	case kone_mouse_event_osd_profile:
		kone->actual_profile = event->value;
		break;
	case kone_mouse_event_switch_dpi:
	case kone_mouse_event_osd_dpi:
		kone->actual_dpi = event->value;
		break;
	}
}

static void kone_report_to_chrdev(struct kone_device const *kone,
		struct kone_mouse_event const *event)
{
	struct kone_roccat_report roccat_report;

	switch (event->event) {
	case kone_mouse_event_switch_profile:
	case kone_mouse_event_switch_dpi:
	case kone_mouse_event_osd_profile:
	case kone_mouse_event_osd_dpi:
		roccat_report.event = event->event;
		roccat_report.value = event->value;
		roccat_report.key = 0;
		roccat_report_event(kone->chrdev_minor,
				(uint8_t *)&roccat_report);
		break;
	case kone_mouse_event_call_overlong_macro:
		if (event->value == kone_keystroke_action_press) {
			roccat_report.event = kone_mouse_event_call_overlong_macro;
			roccat_report.value = kone->actual_profile;
			roccat_report.key = event->macro_key;
			roccat_report_event(kone->chrdev_minor,
					(uint8_t *)&roccat_report);
		}
		break;
	}

}

static int kone_raw_event(struct hid_device *hdev, struct hid_report *report,
		u8 *data, int size)
{
	struct kone_device *kone = hid_get_drvdata(hdev);
	struct kone_mouse_event *event = (struct kone_mouse_event *)data;

	
	if (size != sizeof(struct kone_mouse_event))
		return 0;

	if (kone == NULL)
		return 0;

	if (memcmp(&kone->last_mouse_event.tilt, &event->tilt, 5))
		memcpy(&kone->last_mouse_event, event,
				sizeof(struct kone_mouse_event));
	else
		memset(&event->tilt, 0, 5);

	kone_keep_values_up_to_date(kone, event);

	if (kone->roccat_claimed)
		kone_report_to_chrdev(kone, event);

	return 0; 
}

static const struct hid_device_id kone_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_ROCCAT, USB_DEVICE_ID_ROCCAT_KONE) },
	{ }
};

MODULE_DEVICE_TABLE(hid, kone_devices);

static struct hid_driver kone_driver = {
		.name = "kone",
		.id_table = kone_devices,
		.probe = kone_probe,
		.remove = kone_remove,
		.raw_event = kone_raw_event
};

static int __init kone_init(void)
{
	int retval;

	
	kone_class = class_create(THIS_MODULE, "kone");
	if (IS_ERR(kone_class))
		return PTR_ERR(kone_class);
	kone_class->dev_attrs = kone_attributes;
	kone_class->dev_bin_attrs = kone_bin_attributes;

	retval = hid_register_driver(&kone_driver);
	if (retval)
		class_destroy(kone_class);
	return retval;
}

static void __exit kone_exit(void)
{
	hid_unregister_driver(&kone_driver);
	class_destroy(kone_class);
}

module_init(kone_init);
module_exit(kone_exit);

MODULE_AUTHOR("Stefan Achatz");
MODULE_DESCRIPTION("USB Roccat Kone driver");
MODULE_LICENSE("GPL v2");
