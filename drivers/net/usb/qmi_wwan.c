/*
 * Copyright (c) 2012  Bjørn Mork <bjorn@mork.no>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/usb.h>
#include <linux/usb/cdc.h>
#include <linux/usb/usbnet.h>
#include <linux/usb/cdc-wdm.h>

#define DM_DRIVER "cdc_wdm"


static int qmi_wwan_bind(struct usbnet *dev, struct usb_interface *intf)
{
	int status = -1;
	struct usb_interface *control = NULL;
	u8 *buf = intf->cur_altsetting->extra;
	int len = intf->cur_altsetting->extralen;
	struct usb_interface_descriptor *desc = &intf->cur_altsetting->desc;
	struct usb_cdc_union_desc *cdc_union = NULL;
	struct usb_cdc_ether_desc *cdc_ether = NULL;
	u32 required = 1 << USB_CDC_HEADER_TYPE | 1 << USB_CDC_UNION_TYPE;
	u32 found = 0;
	atomic_t *pmcount = (void *)&dev->data[1];

	atomic_set(pmcount, 0);

	if (len == 0 && desc->bInterfaceNumber > 0) {
		control = usb_ifnum_to_if(dev->udev, desc->bInterfaceNumber - 1);
		if (!control)
			goto err;

		buf = control->cur_altsetting->extra;
		len = control->cur_altsetting->extralen;
		dev_dbg(&intf->dev, "guessing \"control\" => %s, \"data\" => this\n",
			dev_name(&control->dev));
	}

	while (len > 3) {
		struct usb_descriptor_header *h = (void *)buf;

		
		if (h->bDescriptorType != USB_DT_CS_INTERFACE)
			goto next_desc;

		
		switch (buf[2]) {
		case USB_CDC_HEADER_TYPE:
			if (found & 1 << USB_CDC_HEADER_TYPE) {
				dev_dbg(&intf->dev, "extra CDC header\n");
				goto err;
			}
			if (h->bLength != sizeof(struct usb_cdc_header_desc)) {
				dev_dbg(&intf->dev, "CDC header len %u\n", h->bLength);
				goto err;
			}
			break;
		case USB_CDC_UNION_TYPE:
			if (found & 1 << USB_CDC_UNION_TYPE) {
				dev_dbg(&intf->dev, "extra CDC union\n");
				goto err;
			}
			if (h->bLength != sizeof(struct usb_cdc_union_desc)) {
				dev_dbg(&intf->dev, "CDC union len %u\n", h->bLength);
				goto err;
			}
			cdc_union = (struct usb_cdc_union_desc *)buf;
			break;
		case USB_CDC_ETHERNET_TYPE:
			if (found & 1 << USB_CDC_ETHERNET_TYPE) {
				dev_dbg(&intf->dev, "extra CDC ether\n");
				goto err;
			}
			if (h->bLength != sizeof(struct usb_cdc_ether_desc)) {
				dev_dbg(&intf->dev, "CDC ether len %u\n",  h->bLength);
				goto err;
			}
			cdc_ether = (struct usb_cdc_ether_desc *)buf;
			break;
		}

		if (buf[2] < 32)
			found |= 1 << buf[2];

next_desc:
		len -= h->bLength;
		buf += h->bLength;
	}

	
	if ((found & required) != required) {
		dev_err(&intf->dev, "CDC functional descriptors missing\n");
		goto err;
	}

	
	if (cdc_union && desc->bInterfaceNumber == cdc_union->bMasterInterface0) {
		dev_err(&intf->dev, "leaving \"control\" interface for " DM_DRIVER " - try binding to %s instead!\n",
			dev_name(&usb_ifnum_to_if(dev->udev, cdc_union->bSlaveInterface0)->dev));
		goto err;
	}

	
	if (cdc_ether) {
		dev->hard_mtu = le16_to_cpu(cdc_ether->wMaxSegmentSize);
		usbnet_get_ethernet_addr(dev, cdc_ether->iMACAddress);
	}

	
	if (control)
		dev_info(&intf->dev, "Use \"" DM_DRIVER "\" for QMI interface %s\n",
			dev_name(&control->dev));

	

	
	status = usbnet_get_endpoints(dev, intf);

err:
	return status;
}

static int qmi_wwan_manage_power(struct usbnet *dev, int on)
{
	atomic_t *pmcount = (void *)&dev->data[1];
	int rv = 0;

	dev_dbg(&dev->intf->dev, "%s() pmcount=%d, on=%d\n", __func__, atomic_read(pmcount), on);

	if ((on && atomic_add_return(1, pmcount) == 1) || (!on && atomic_dec_and_test(pmcount))) {
		
		rv = usb_autopm_get_interface(dev->intf);
		if (rv < 0)
			goto err;
		dev->intf->needs_remote_wakeup = on;
		usb_autopm_put_interface(dev->intf);
	}
err:
	return rv;
}

static int qmi_wwan_cdc_wdm_manage_power(struct usb_interface *intf, int on)
{
	struct usbnet *dev = usb_get_intfdata(intf);
	return qmi_wwan_manage_power(dev, on);
}

static int qmi_wwan_bind_shared(struct usbnet *dev, struct usb_interface *intf)
{
	int rv;
	struct usb_driver *subdriver = NULL;
	atomic_t *pmcount = (void *)&dev->data[1];

	if (dev->driver_info->data &&
	    !test_bit(intf->cur_altsetting->desc.bInterfaceNumber, &dev->driver_info->data)) {
		dev_info(&intf->dev, "not on our whitelist - ignored");
		rv = -ENODEV;
		goto err;
	}

	atomic_set(pmcount, 0);

	
	rv = usbnet_get_endpoints(dev, intf);
	if (rv < 0)
		goto err;

	
	if (!dev->status) {
		rv = -EINVAL;
		goto err;
	}

	subdriver = usb_cdc_wdm_register(intf, &dev->status->desc, 512, &qmi_wwan_cdc_wdm_manage_power);
	if (IS_ERR(subdriver)) {
		rv = PTR_ERR(subdriver);
		goto err;
	}

	
	dev->status = NULL;

	
	dev->data[0] = (unsigned long)subdriver;

err:
	return rv;
}

static int qmi_wwan_bind_gobi(struct usbnet *dev, struct usb_interface *intf)
{
	int rv = -EINVAL;

	
	if (intf->cur_altsetting->extralen)
		goto err;

	rv = qmi_wwan_bind_shared(dev, intf);
err:
	return rv;
}

static void qmi_wwan_unbind_shared(struct usbnet *dev, struct usb_interface *intf)
{
	struct usb_driver *subdriver = (void *)dev->data[0];

	if (subdriver && subdriver->disconnect)
		subdriver->disconnect(intf);

	dev->data[0] = (unsigned long)NULL;
}

static int qmi_wwan_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct usbnet *dev = usb_get_intfdata(intf);
	struct usb_driver *subdriver = (void *)dev->data[0];
	int ret;

	ret = usbnet_suspend(intf, message);
	if (ret < 0)
		goto err;

	if (subdriver && subdriver->suspend)
		ret = subdriver->suspend(intf, message);
	if (ret < 0)
		usbnet_resume(intf);
err:
	return ret;
}

static int qmi_wwan_resume(struct usb_interface *intf)
{
	struct usbnet *dev = usb_get_intfdata(intf);
	struct usb_driver *subdriver = (void *)dev->data[0];
	int ret = 0;

	if (subdriver && subdriver->resume)
		ret = subdriver->resume(intf);
	if (ret < 0)
		goto err;
	ret = usbnet_resume(intf);
	if (ret < 0 && subdriver && subdriver->resume && subdriver->suspend)
		subdriver->suspend(intf, PMSG_SUSPEND);
err:
	return ret;
}


static const struct driver_info	qmi_wwan_info = {
	.description	= "QMI speaking wwan device",
	.flags		= FLAG_WWAN,
	.bind		= qmi_wwan_bind,
	.manage_power	= qmi_wwan_manage_power,
};

static const struct driver_info	qmi_wwan_shared = {
	.description	= "QMI speaking wwan device with combined interface",
	.flags		= FLAG_WWAN,
	.bind		= qmi_wwan_bind_shared,
	.unbind		= qmi_wwan_unbind_shared,
	.manage_power	= qmi_wwan_manage_power,
};

static const struct driver_info	qmi_wwan_gobi = {
	.description	= "Qualcomm Gobi wwan/QMI device",
	.flags		= FLAG_WWAN,
	.bind		= qmi_wwan_bind_gobi,
	.unbind		= qmi_wwan_unbind_shared,
	.manage_power	= qmi_wwan_manage_power,
};

static const struct driver_info	qmi_wwan_force_int4 = {
	.description	= "Qualcomm Gobi wwan/QMI device",
	.flags		= FLAG_WWAN,
	.bind		= qmi_wwan_bind_gobi,
	.unbind		= qmi_wwan_unbind_shared,
	.manage_power	= qmi_wwan_manage_power,
	.data		= BIT(4), 
};

static const struct driver_info	qmi_wwan_sierra = {
	.description	= "Sierra Wireless wwan/QMI device",
	.flags		= FLAG_WWAN,
	.bind		= qmi_wwan_bind_gobi,
	.unbind		= qmi_wwan_unbind_shared,
	.manage_power	= qmi_wwan_manage_power,
	.data		= BIT(8) | BIT(19), 
};

#define HUAWEI_VENDOR_ID	0x12D1
#define QMI_GOBI_DEVICE(vend, prod) \
	USB_DEVICE(vend, prod), \
	.driver_info = (unsigned long)&qmi_wwan_gobi

static const struct usb_device_id products[] = {
	{	
		.match_flags        = USB_DEVICE_ID_MATCH_VENDOR | USB_DEVICE_ID_MATCH_INT_INFO,
		.idVendor           = HUAWEI_VENDOR_ID,
		.bInterfaceClass    = USB_CLASS_VENDOR_SPEC,
		.bInterfaceSubClass = 1,
		.bInterfaceProtocol = 8, 
		.driver_info        = (unsigned long)&qmi_wwan_info,
	},
	{	
		.match_flags        = USB_DEVICE_ID_MATCH_VENDOR | USB_DEVICE_ID_MATCH_INT_INFO,
		.idVendor           = HUAWEI_VENDOR_ID,
		.bInterfaceClass    = USB_CLASS_VENDOR_SPEC,
		.bInterfaceSubClass = 1,
		.bInterfaceProtocol = 17,
		.driver_info        = (unsigned long)&qmi_wwan_shared,
	},
	{	
		.match_flags	    = USB_DEVICE_ID_MATCH_DEVICE | USB_DEVICE_ID_MATCH_INT_INFO,
		.idVendor           = 0x106c,
		.idProduct          = 0x3718,
		.bInterfaceClass    = 0xff,
		.bInterfaceSubClass = 0xf0,
		.bInterfaceProtocol = 0xff,
		.driver_info        = (unsigned long)&qmi_wwan_shared,
	},
	{	
		.match_flags	    = USB_DEVICE_ID_MATCH_DEVICE | USB_DEVICE_ID_MATCH_INT_INFO,
		.idVendor           = 0x19d2,
		.idProduct          = 0x0167,
		.bInterfaceClass    = 0xff,
		.bInterfaceSubClass = 0xff,
		.bInterfaceProtocol = 0xff,
		.driver_info        = (unsigned long)&qmi_wwan_force_int4,
	},
	{	
		.match_flags	    = USB_DEVICE_ID_MATCH_DEVICE | USB_DEVICE_ID_MATCH_INT_INFO,
		.idVendor           = 0x19d2,
		.idProduct          = 0x0063,
		.bInterfaceClass    = 0xff,
		.bInterfaceSubClass = 0xff,
		.bInterfaceProtocol = 0xff,
		.driver_info        = (unsigned long)&qmi_wwan_force_int4,
	},
	{	
		.match_flags	    = USB_DEVICE_ID_MATCH_DEVICE | USB_DEVICE_ID_MATCH_INT_INFO,
		.idVendor           = 0x19d2,
		.idProduct          = 0x1008,
		.bInterfaceClass    = 0xff,
		.bInterfaceSubClass = 0xff,
		.bInterfaceProtocol = 0xff,
		.driver_info        = (unsigned long)&qmi_wwan_force_int4,
	},
	{	
		.match_flags	    = USB_DEVICE_ID_MATCH_DEVICE | USB_DEVICE_ID_MATCH_INT_INFO,
		.idVendor           = 0x19d2,
		.idProduct          = 0x1010,
		.bInterfaceClass    = 0xff,
		.bInterfaceSubClass = 0xff,
		.bInterfaceProtocol = 0xff,
		.driver_info        = (unsigned long)&qmi_wwan_force_int4,
	},
	{	
		.match_flags	    = USB_DEVICE_ID_MATCH_DEVICE | USB_DEVICE_ID_MATCH_INT_INFO,
		.idVendor           = 0x19d2,
		.idProduct          = 0x0104,
		.bInterfaceClass    = 0xff,
		.bInterfaceSubClass = 0xff,
		.bInterfaceProtocol = 0xff,
		.driver_info        = (unsigned long)&qmi_wwan_force_int4,
	},
	{	
		.match_flags	    = USB_DEVICE_ID_MATCH_DEVICE | USB_DEVICE_ID_MATCH_INT_INFO,
		.idVendor           = 0x1199,
		.idProduct          = 0x68a2,
		.bInterfaceClass    = 0xff,
		.bInterfaceSubClass = 0xff,
		.bInterfaceProtocol = 0xff,
		.driver_info        = (unsigned long)&qmi_wwan_sierra,
	},
	{QMI_GOBI_DEVICE(0x05c6, 0x9212)},	
	{QMI_GOBI_DEVICE(0x03f0, 0x1f1d)},	
	{QMI_GOBI_DEVICE(0x03f0, 0x371d)},	
	{QMI_GOBI_DEVICE(0x04da, 0x250d)},	
	{QMI_GOBI_DEVICE(0x413c, 0x8172)},	
	{QMI_GOBI_DEVICE(0x1410, 0xa001)},	
	{QMI_GOBI_DEVICE(0x0b05, 0x1776)},	
	{QMI_GOBI_DEVICE(0x19d2, 0xfff3)},	
	{QMI_GOBI_DEVICE(0x05c6, 0x9001)},	
	{QMI_GOBI_DEVICE(0x05c6, 0x9002)},	
	{QMI_GOBI_DEVICE(0x05c6, 0x9202)},	
	{QMI_GOBI_DEVICE(0x05c6, 0x9203)},	
	{QMI_GOBI_DEVICE(0x05c6, 0x9222)},	
	{QMI_GOBI_DEVICE(0x05c6, 0x9009)},	
	{QMI_GOBI_DEVICE(0x413c, 0x8186)},	
	{QMI_GOBI_DEVICE(0x05c6, 0x920b)},	
	{QMI_GOBI_DEVICE(0x05c6, 0x9225)},	
	{QMI_GOBI_DEVICE(0x05c6, 0x9245)},	
	{QMI_GOBI_DEVICE(0x03f0, 0x251d)},	
	{QMI_GOBI_DEVICE(0x05c6, 0x9215)},	
	{QMI_GOBI_DEVICE(0x05c6, 0x9265)},	
	{QMI_GOBI_DEVICE(0x05c6, 0x9235)},	
	{QMI_GOBI_DEVICE(0x05c6, 0x9275)},	
	{QMI_GOBI_DEVICE(0x1199, 0x9001)},	
	{QMI_GOBI_DEVICE(0x1199, 0x9002)},	
	{QMI_GOBI_DEVICE(0x1199, 0x9003)},	
	{QMI_GOBI_DEVICE(0x1199, 0x9004)},	
	{QMI_GOBI_DEVICE(0x1199, 0x9005)},	
	{QMI_GOBI_DEVICE(0x1199, 0x9006)},	
	{QMI_GOBI_DEVICE(0x1199, 0x9007)},	
	{QMI_GOBI_DEVICE(0x1199, 0x9008)},	
	{QMI_GOBI_DEVICE(0x1199, 0x9009)},	
	{QMI_GOBI_DEVICE(0x1199, 0x900a)},	
	{QMI_GOBI_DEVICE(0x1199, 0x9011)},	
	{QMI_GOBI_DEVICE(0x16d8, 0x8002)},	
	{QMI_GOBI_DEVICE(0x05c6, 0x9205)},	
	{QMI_GOBI_DEVICE(0x1199, 0x9013)},	
	{ }					
};
MODULE_DEVICE_TABLE(usb, products);

static struct usb_driver qmi_wwan_driver = {
	.name		      = "qmi_wwan",
	.id_table	      = products,
	.probe		      =	usbnet_probe,
	.disconnect	      = usbnet_disconnect,
	.suspend	      = qmi_wwan_suspend,
	.resume		      =	qmi_wwan_resume,
	.reset_resume         = qmi_wwan_resume,
	.supports_autosuspend = 1,
};

static int __init qmi_wwan_init(void)
{
	return usb_register(&qmi_wwan_driver);
}
module_init(qmi_wwan_init);

static void __exit qmi_wwan_exit(void)
{
	usb_deregister(&qmi_wwan_driver);
}
module_exit(qmi_wwan_exit);

MODULE_AUTHOR("Bjørn Mork <bjorn@mork.no>");
MODULE_DESCRIPTION("Qualcomm MSM Interface (QMI) WWAN driver");
MODULE_LICENSE("GPL");
