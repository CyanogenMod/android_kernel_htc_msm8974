/*
 * PL-2301/2302 USB host-to-host link cables
 * Copyright (C) 2000-2005 by David Brownell
 *
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include <linux/module.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/workqueue.h>
#include <linux/mii.h>
#include <linux/usb.h>
#include <linux/usb/usbnet.h>



#define	PL_S_EN		(1<<7)		
#define	PL_TX_READY	(1<<5)		
#define	PL_RESET_OUT	(1<<4)		
#define	PL_RESET_IN	(1<<3)		
#define	PL_TX_C		(1<<2)		
#define	PL_TX_REQ	(1<<1)		
#define	PL_PEER_E	(1<<0)		

static inline int
pl_vendor_req(struct usbnet *dev, u8 req, u8 val, u8 index)
{
	return usb_control_msg(dev->udev,
		usb_rcvctrlpipe(dev->udev, 0),
		req,
		USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
		val, index,
		NULL, 0,
		USB_CTRL_GET_TIMEOUT);
}

static inline int
pl_clear_QuickLink_features(struct usbnet *dev, int val)
{
	return pl_vendor_req(dev, 1, (u8) val, 0);
}

static inline int
pl_set_QuickLink_features(struct usbnet *dev, int val)
{
	return pl_vendor_req(dev, 3, (u8) val, 0);
}

static int pl_reset(struct usbnet *dev)
{
	int status;

	status = pl_set_QuickLink_features(dev,
		PL_S_EN|PL_RESET_OUT|PL_RESET_IN|PL_PEER_E);
	if (status != 0 && netif_msg_probe(dev))
		netif_dbg(dev, link, dev->net, "pl_reset --> %d\n", status);
	return 0;
}

static const struct driver_info	prolific_info = {
	.description =	"Prolific PL-2301/PL-2302/PL-25A1",
	.flags =	FLAG_POINTTOPOINT | FLAG_NO_SETINT,
		
	.reset =	pl_reset,
};




static const struct usb_device_id	products [] = {

{
	USB_DEVICE(0x067b, 0x0000),	
	.driver_info =	(unsigned long) &prolific_info,
}, {
	USB_DEVICE(0x067b, 0x0001),	
	.driver_info =	(unsigned long) &prolific_info,
},

{
	USB_DEVICE(0x067b, 0x25a1),     
	.driver_info =  (unsigned long) &prolific_info,
}, {
	USB_DEVICE(0x050d, 0x258a),     
	.driver_info =  (unsigned long) &prolific_info,
},

	{ },		
};
MODULE_DEVICE_TABLE(usb, products);

static struct usb_driver plusb_driver = {
	.name =		"plusb",
	.id_table =	products,
	.probe =	usbnet_probe,
	.disconnect =	usbnet_disconnect,
	.suspend =	usbnet_suspend,
	.resume =	usbnet_resume,
};

module_usb_driver(plusb_driver);

MODULE_AUTHOR("David Brownell");
MODULE_DESCRIPTION("Prolific PL-2301/2302/25A1 USB Host to Host Link Driver");
MODULE_LICENSE("GPL");
