/*
  USB Driver for Sierra Wireless

  Copyright (C) 2006, 2007, 2008  Kevin Lloyd <klloyd@sierrawireless.com>,

  Copyright (C) 2008, 2009  Elina Pasheva, Matthew Safar, Rory Filer
			<linux@sierrawireless.com>

  IMPORTANT DISCLAIMER: This driver is not commercially supported by
  Sierra Wireless. Use at your own risk.

  This driver is free software; you can redistribute it and/or modify
  it under the terms of Version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  Portions based on the option driver by Matthias Urlichs <smurf@smurf.noris.de>
  Whom based his on the Keyspan driver by Hugh Blemings <hugh@blemings.org>
*/
#define DRIVER_VERSION "v.1.7.16"
#define DRIVER_AUTHOR "Kevin Lloyd, Elina Pasheva, Matthew Safar, Rory Filer"
#define DRIVER_DESC "USB Driver for Sierra Wireless USB modems"

#include <linux/kernel.h>
#include <linux/jiffies.h>
#include <linux/errno.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/tty_flip.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/usb/serial.h>

#define SWIMS_USB_REQUEST_SetPower	0x00
#define SWIMS_USB_REQUEST_SetNmea	0x07

#define N_IN_URB_HM	8
#define N_OUT_URB_HM	64
#define N_IN_URB	4
#define N_OUT_URB	4
#define IN_BUFLEN	4096

#define MAX_TRANSFER		(PAGE_SIZE - 512)

static bool debug;
static bool nmea;

struct sierra_iface_info {
	const u32 infolen;	
	const u8  *ifaceinfo;	
};

struct sierra_intf_private {
	spinlock_t susp_lock;
	unsigned int suspended:1;
	int in_flight;
};

static int sierra_set_power_state(struct usb_device *udev, __u16 swiState)
{
	int result;
	dev_dbg(&udev->dev, "%s\n", __func__);
	result = usb_control_msg(udev, usb_sndctrlpipe(udev, 0),
			SWIMS_USB_REQUEST_SetPower,	
			USB_TYPE_VENDOR,		
			swiState,			
			0,				
			NULL,				
			0,				
			USB_CTRL_SET_TIMEOUT);		
	return result;
}

static int sierra_vsc_set_nmea(struct usb_device *udev, __u16 enable)
{
	int result;
	dev_dbg(&udev->dev, "%s\n", __func__);
	result = usb_control_msg(udev, usb_sndctrlpipe(udev, 0),
			SWIMS_USB_REQUEST_SetNmea,	
			USB_TYPE_VENDOR,		
			enable,				
			0x0000,				
			NULL,				
			0,				
			USB_CTRL_SET_TIMEOUT);		
	return result;
}

static int sierra_calc_num_ports(struct usb_serial *serial)
{
	int num_ports = 0;
	u8 ifnum, numendpoints;

	dev_dbg(&serial->dev->dev, "%s\n", __func__);

	ifnum = serial->interface->cur_altsetting->desc.bInterfaceNumber;
	numendpoints = serial->interface->cur_altsetting->desc.bNumEndpoints;

	
	if (ifnum == 0x99)
		num_ports = 0;
	else if (numendpoints <= 3)
		num_ports = 1;
	else
		num_ports = (numendpoints-1)/2;
	return num_ports;
}

static int is_blacklisted(const u8 ifnum,
				const struct sierra_iface_info *blacklist)
{
	const u8  *info;
	int i;

	if (blacklist) {
		info = blacklist->ifaceinfo;

		for (i = 0; i < blacklist->infolen; i++) {
			if (info[i] == ifnum)
				return 1;
		}
	}
	return 0;
}

static int is_himemory(const u8 ifnum,
				const struct sierra_iface_info *himemorylist)
{
	const u8  *info;
	int i;

	if (himemorylist) {
		info = himemorylist->ifaceinfo;

		for (i=0; i < himemorylist->infolen; i++) {
			if (info[i] == ifnum)
				return 1;
		}
	}
	return 0;
}

static int sierra_calc_interface(struct usb_serial *serial)
{
	int interface;
	struct usb_interface *p_interface;
	struct usb_host_interface *p_host_interface;
	dev_dbg(&serial->dev->dev, "%s\n", __func__);

	
	p_interface = serial->interface;

	
	p_host_interface = p_interface->cur_altsetting;

	interface = p_host_interface->desc.bInterfaceNumber;

	return interface;
}

static int sierra_probe(struct usb_serial *serial,
			const struct usb_device_id *id)
{
	int result = 0;
	struct usb_device *udev;
	struct sierra_intf_private *data;
	u8 ifnum;

	udev = serial->dev;
	dev_dbg(&udev->dev, "%s\n", __func__);

	ifnum = sierra_calc_interface(serial);
	if (serial->interface->num_altsetting == 2) {
		dev_dbg(&udev->dev, "Selecting alt setting for interface %d\n",
			ifnum);
		
		usb_set_interface(udev, ifnum, 1);
	}

	
	ifnum = sierra_calc_interface(serial);

	if (is_blacklisted(ifnum,
				(struct sierra_iface_info *)id->driver_info)) {
		dev_dbg(&serial->dev->dev,
			"Ignoring blacklisted interface #%d\n", ifnum);
		return -ENODEV;
	}

	data = serial->private = kzalloc(sizeof(struct sierra_intf_private), GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	spin_lock_init(&data->susp_lock);

	return result;
}

static const u8 hi_memory_typeA_ifaces[] = { 0, 2 };
static const struct sierra_iface_info typeA_interface_list = {
	.infolen = ARRAY_SIZE(hi_memory_typeA_ifaces),
	.ifaceinfo = hi_memory_typeA_ifaces,
};

static const u8 hi_memory_typeB_ifaces[] = { 3, 4, 5, 6 };
static const struct sierra_iface_info typeB_interface_list = {
	.infolen = ARRAY_SIZE(hi_memory_typeB_ifaces),
	.ifaceinfo = hi_memory_typeB_ifaces,
};

static const u8 direct_ip_non_serial_ifaces[] = { 7, 8, 9, 10, 11, 19, 20 };
static const struct sierra_iface_info direct_ip_interface_blacklist = {
	.infolen = ARRAY_SIZE(direct_ip_non_serial_ifaces),
	.ifaceinfo = direct_ip_non_serial_ifaces,
};

static const struct usb_device_id id_table[] = {
	{ USB_DEVICE(0x0F3D, 0x0112) }, 
	{ USB_DEVICE(0x03F0, 0x1B1D) },	
	{ USB_DEVICE(0x03F0, 0x211D) }, 
	{ USB_DEVICE(0x03F0, 0x1E1D) },	

	{ USB_DEVICE(0x1199, 0x0017) },	
	{ USB_DEVICE(0x1199, 0x0018) },	
	{ USB_DEVICE(0x1199, 0x0218) },	
	{ USB_DEVICE(0x1199, 0x0020) },	
	{ USB_DEVICE(0x1199, 0x0220) },	
	{ USB_DEVICE(0x1199, 0x0022) },	
	{ USB_DEVICE(0x1199, 0x0024) },	
	{ USB_DEVICE(0x1199, 0x0224) },	
	{ USB_DEVICE(0x1199, 0x0019) },	
	{ USB_DEVICE(0x1199, 0x0021) },	
	{ USB_DEVICE(0x1199, 0x0112) }, 
	{ USB_DEVICE(0x1199, 0x0120) },	
	{ USB_DEVICE(0x1199, 0x0301) },	
	
	{ USB_DEVICE_AND_INTERFACE_INFO(0x1199, 0x0023, 0xFF, 0xFF, 0xFF) },
	
	{ USB_DEVICE_AND_INTERFACE_INFO(0x1199, 0x0025, 0xFF, 0xFF, 0xFF) },
	{ USB_DEVICE(0x1199, 0x0026) }, 
	{ USB_DEVICE(0x1199, 0x0027) }, 
	{ USB_DEVICE(0x1199, 0x0028) }, 
	{ USB_DEVICE(0x1199, 0x0029) }, 

	{ USB_DEVICE(0x1199, 0x6802) },	
	{ USB_DEVICE(0x1199, 0x6803) },	
	{ USB_DEVICE(0x1199, 0x6804) },	
	{ USB_DEVICE(0x1199, 0x6805) },	
	{ USB_DEVICE(0x1199, 0x6808) },	
	{ USB_DEVICE(0x1199, 0x6809) },	
	{ USB_DEVICE(0x1199, 0x6812) },	
	{ USB_DEVICE(0x1199, 0x6813) },	
	{ USB_DEVICE(0x1199, 0x6815) },	
	{ USB_DEVICE(0x1199, 0x6816) },	
	{ USB_DEVICE(0x1199, 0x6820) },	
	{ USB_DEVICE(0x1199, 0x6821) },	
	{ USB_DEVICE(0x1199, 0x6822) },	
	{ USB_DEVICE(0x1199, 0x6832) },	
	{ USB_DEVICE(0x1199, 0x6833) },	
	{ USB_DEVICE(0x1199, 0x6834) },	
	{ USB_DEVICE(0x1199, 0x6835) },	
	{ USB_DEVICE(0x1199, 0x6838) },	
	{ USB_DEVICE(0x1199, 0x6839) },	
	{ USB_DEVICE(0x1199, 0x683A) },	
	{ USB_DEVICE(0x1199, 0x683B) },	
	
	{ USB_DEVICE(0x1199, 0x683C) },
	{ USB_DEVICE(0x1199, 0x683D) },	
	
	{ USB_DEVICE(0x1199, 0x683E) },
	{ USB_DEVICE(0x1199, 0x6850) },	
	{ USB_DEVICE(0x1199, 0x6851) },	
	{ USB_DEVICE(0x1199, 0x6852) },	
	{ USB_DEVICE(0x1199, 0x6853) },	
	{ USB_DEVICE(0x1199, 0x6855) },	
	{ USB_DEVICE(0x1199, 0x6856) },	
	{ USB_DEVICE(0x1199, 0x6859) },	
	{ USB_DEVICE(0x1199, 0x685A) },	
	
	{ USB_DEVICE_AND_INTERFACE_INFO(0x1199, 0x6880, 0xFF, 0xFF, 0xFF)},
	
	{ USB_DEVICE_AND_INTERFACE_INFO(0x1199, 0x6890, 0xFF, 0xFF, 0xFF)},
	
	{ USB_DEVICE_AND_INTERFACE_INFO(0x1199, 0x6891, 0xFF, 0xFF, 0xFF)},
	
	{ USB_DEVICE_AND_INTERFACE_INFO(0x1199, 0x6892, 0xFF, 0xFF, 0xFF)},
	{ USB_DEVICE(0x1199, 0x6893) },	
	{ USB_DEVICE(0x1199, 0x68A2),   
	  .driver_info = (kernel_ulong_t)&direct_ip_interface_blacklist
	},
	{ USB_DEVICE(0x1199, 0x68A3), 	
	  .driver_info = (kernel_ulong_t)&direct_ip_interface_blacklist
	},
	{ USB_DEVICE(0x0f3d, 0x68A3), 	
	  .driver_info = (kernel_ulong_t)&direct_ip_interface_blacklist
	},
       { USB_DEVICE(0x413C, 0x08133) }, 

	{ }
};
MODULE_DEVICE_TABLE(usb, id_table);


struct sierra_port_private {
	spinlock_t lock;	
	int outstanding_urbs;	
	struct usb_anchor active;
	struct usb_anchor delayed;

	int num_out_urbs;
	int num_in_urbs;
	
	struct urb *in_urbs[N_IN_URB_HM];

	
	int rts_state;	
	int dtr_state;
	int cts_state;	
	int dsr_state;
	int dcd_state;
	int ri_state;
	unsigned int opened:1;
};

static int sierra_send_setup(struct usb_serial_port *port)
{
	struct usb_serial *serial = port->serial;
	struct sierra_port_private *portdata;
	__u16 interface = 0;
	int val = 0;
	int do_send = 0;
	int retval;

	dev_dbg(&port->dev, "%s\n", __func__);

	portdata = usb_get_serial_port_data(port);

	if (portdata->dtr_state)
		val |= 0x01;
	if (portdata->rts_state)
		val |= 0x02;

	
	if (serial->num_ports == 1) {
		interface = sierra_calc_interface(serial);
		if (port->interrupt_in_urb) {
			
			do_send = 1;
		}
	}

	
	else {
		if (port->bulk_out_endpointAddress == 2)
			interface = 0;
		else if (port->bulk_out_endpointAddress == 4)
			interface = 1;
		else if (port->bulk_out_endpointAddress == 5)
			interface = 2;

		do_send = 1;
	}
	if (!do_send)
		return 0;

	retval = usb_autopm_get_interface(serial->interface);
	if (retval < 0)
		return retval;

	retval = usb_control_msg(serial->dev, usb_rcvctrlpipe(serial->dev, 0),
		0x22, 0x21, val, interface, NULL, 0, USB_CTRL_SET_TIMEOUT);
	usb_autopm_put_interface(serial->interface);

	return retval;
}

static void sierra_set_termios(struct tty_struct *tty,
		struct usb_serial_port *port, struct ktermios *old_termios)
{
	dev_dbg(&port->dev, "%s\n", __func__);
	tty_termios_copy_hw(tty->termios, old_termios);
	sierra_send_setup(port);
}

static int sierra_tiocmget(struct tty_struct *tty)
{
	struct usb_serial_port *port = tty->driver_data;
	unsigned int value;
	struct sierra_port_private *portdata;

	dev_dbg(&port->dev, "%s\n", __func__);
	portdata = usb_get_serial_port_data(port);

	value = ((portdata->rts_state) ? TIOCM_RTS : 0) |
		((portdata->dtr_state) ? TIOCM_DTR : 0) |
		((portdata->cts_state) ? TIOCM_CTS : 0) |
		((portdata->dsr_state) ? TIOCM_DSR : 0) |
		((portdata->dcd_state) ? TIOCM_CAR : 0) |
		((portdata->ri_state) ? TIOCM_RNG : 0);

	return value;
}

static int sierra_tiocmset(struct tty_struct *tty,
			unsigned int set, unsigned int clear)
{
	struct usb_serial_port *port = tty->driver_data;
	struct sierra_port_private *portdata;

	portdata = usb_get_serial_port_data(port);

	if (set & TIOCM_RTS)
		portdata->rts_state = 1;
	if (set & TIOCM_DTR)
		portdata->dtr_state = 1;

	if (clear & TIOCM_RTS)
		portdata->rts_state = 0;
	if (clear & TIOCM_DTR)
		portdata->dtr_state = 0;
	return sierra_send_setup(port);
}

static void sierra_release_urb(struct urb *urb)
{
	struct usb_serial_port *port;
	if (urb) {
		port =  urb->context;
		dev_dbg(&port->dev, "%s: %p\n", __func__, urb);
		kfree(urb->transfer_buffer);
		usb_free_urb(urb);
	}
}

static void sierra_outdat_callback(struct urb *urb)
{
	struct usb_serial_port *port = urb->context;
	struct sierra_port_private *portdata = usb_get_serial_port_data(port);
	struct sierra_intf_private *intfdata;
	int status = urb->status;

	dev_dbg(&port->dev, "%s - port %d\n", __func__, port->number);
	intfdata = port->serial->private;

	
	kfree(urb->transfer_buffer);
	usb_autopm_put_interface_async(port->serial->interface);
	if (status)
		dev_dbg(&port->dev, "%s - nonzero write bulk status "
		    "received: %d\n", __func__, status);

	spin_lock(&portdata->lock);
	--portdata->outstanding_urbs;
	spin_unlock(&portdata->lock);
	spin_lock(&intfdata->susp_lock);
	--intfdata->in_flight;
	spin_unlock(&intfdata->susp_lock);

	usb_serial_port_softint(port);
}

static int sierra_write(struct tty_struct *tty, struct usb_serial_port *port,
					const unsigned char *buf, int count)
{
	struct sierra_port_private *portdata;
	struct sierra_intf_private *intfdata;
	struct usb_serial *serial = port->serial;
	unsigned long flags;
	unsigned char *buffer;
	struct urb *urb;
	size_t writesize = min((size_t)count, (size_t)MAX_TRANSFER);
	int retval = 0;

	
	if (count == 0)
		return 0;

	portdata = usb_get_serial_port_data(port);
	intfdata = serial->private;

	dev_dbg(&port->dev, "%s: write (%zd bytes)\n", __func__, writesize);
	spin_lock_irqsave(&portdata->lock, flags);
	dev_dbg(&port->dev, "%s - outstanding_urbs: %d\n", __func__,
		portdata->outstanding_urbs);
	if (portdata->outstanding_urbs > portdata->num_out_urbs) {
		spin_unlock_irqrestore(&portdata->lock, flags);
		dev_dbg(&port->dev, "%s - write limit hit\n", __func__);
		return 0;
	}
	portdata->outstanding_urbs++;
	dev_dbg(&port->dev, "%s - 1, outstanding_urbs: %d\n", __func__,
		portdata->outstanding_urbs);
	spin_unlock_irqrestore(&portdata->lock, flags);

	retval = usb_autopm_get_interface_async(serial->interface);
	if (retval < 0) {
		spin_lock_irqsave(&portdata->lock, flags);
		portdata->outstanding_urbs--;
		spin_unlock_irqrestore(&portdata->lock, flags);
		goto error_simple;
	}

	buffer = kmalloc(writesize, GFP_ATOMIC);
	if (!buffer) {
		dev_err(&port->dev, "out of memory\n");
		retval = -ENOMEM;
		goto error_no_buffer;
	}

	urb = usb_alloc_urb(0, GFP_ATOMIC);
	if (!urb) {
		dev_err(&port->dev, "no more free urbs\n");
		retval = -ENOMEM;
		goto error_no_urb;
	}

	memcpy(buffer, buf, writesize);

	usb_serial_debug_data(debug, &port->dev, __func__, writesize, buffer);

	usb_fill_bulk_urb(urb, serial->dev,
			  usb_sndbulkpipe(serial->dev,
					  port->bulk_out_endpointAddress),
			  buffer, writesize, sierra_outdat_callback, port);

	
	urb->transfer_flags |= URB_ZERO_PACKET;

	spin_lock_irqsave(&intfdata->susp_lock, flags);

	if (intfdata->suspended) {
		usb_anchor_urb(urb, &portdata->delayed);
		spin_unlock_irqrestore(&intfdata->susp_lock, flags);
		goto skip_power;
	} else {
		usb_anchor_urb(urb, &portdata->active);
	}
	
	retval = usb_submit_urb(urb, GFP_ATOMIC);
	if (retval) {
		usb_unanchor_urb(urb);
		spin_unlock_irqrestore(&intfdata->susp_lock, flags);
		dev_err(&port->dev, "%s - usb_submit_urb(write bulk) failed "
			"with status = %d\n", __func__, retval);
		goto error;
	} else {
		intfdata->in_flight++;
		spin_unlock_irqrestore(&intfdata->susp_lock, flags);
	}

skip_power:
	usb_free_urb(urb);

	return writesize;
error:
	usb_free_urb(urb);
error_no_urb:
	kfree(buffer);
error_no_buffer:
	spin_lock_irqsave(&portdata->lock, flags);
	--portdata->outstanding_urbs;
	dev_dbg(&port->dev, "%s - 2. outstanding_urbs: %d\n", __func__,
		portdata->outstanding_urbs);
	spin_unlock_irqrestore(&portdata->lock, flags);
	usb_autopm_put_interface_async(serial->interface);
error_simple:
	return retval;
}

static void sierra_indat_callback(struct urb *urb)
{
	int err;
	int endpoint;
	struct usb_serial_port *port;
	struct tty_struct *tty;
	unsigned char *data = urb->transfer_buffer;
	int status = urb->status;

	endpoint = usb_pipeendpoint(urb->pipe);
	port = urb->context;

	dev_dbg(&port->dev, "%s: %p\n", __func__, urb);

	if (status) {
		dev_dbg(&port->dev, "%s: nonzero status: %d on"
			" endpoint %02x\n", __func__, status, endpoint);
	} else {
		if (urb->actual_length) {
			tty = tty_port_tty_get(&port->port);
			if (tty) {
				tty_insert_flip_string(tty, data,
					urb->actual_length);
				tty_flip_buffer_push(tty);

				tty_kref_put(tty);
				usb_serial_debug_data(debug, &port->dev,
					__func__, urb->actual_length, data);
			}
		} else {
			dev_dbg(&port->dev, "%s: empty read urb"
				" received\n", __func__);
		}
	}

	
	if (status != -ESHUTDOWN && status != -EPERM) {
		usb_mark_last_busy(port->serial->dev);
		err = usb_submit_urb(urb, GFP_ATOMIC);
		if (err && err != -EPERM)
			dev_err(&port->dev, "resubmit read urb failed."
				"(%d)\n", err);
	}
}

static void sierra_instat_callback(struct urb *urb)
{
	int err;
	int status = urb->status;
	struct usb_serial_port *port =  urb->context;
	struct sierra_port_private *portdata = usb_get_serial_port_data(port);
	struct usb_serial *serial = port->serial;

	dev_dbg(&port->dev, "%s: urb %p port %p has data %p\n", __func__,
		urb, port, portdata);

	if (status == 0) {
		struct usb_ctrlrequest *req_pkt =
				(struct usb_ctrlrequest *)urb->transfer_buffer;

		if (!req_pkt) {
			dev_dbg(&port->dev, "%s: NULL req_pkt\n",
				__func__);
			return;
		}
		if ((req_pkt->bRequestType == 0xA1) &&
				(req_pkt->bRequest == 0x20)) {
			int old_dcd_state;
			unsigned char signals = *((unsigned char *)
					urb->transfer_buffer +
					sizeof(struct usb_ctrlrequest));
			struct tty_struct *tty;

			dev_dbg(&port->dev, "%s: signal x%x\n", __func__,
				signals);

			old_dcd_state = portdata->dcd_state;
			portdata->cts_state = 1;
			portdata->dcd_state = ((signals & 0x01) ? 1 : 0);
			portdata->dsr_state = ((signals & 0x02) ? 1 : 0);
			portdata->ri_state = ((signals & 0x08) ? 1 : 0);

			tty = tty_port_tty_get(&port->port);
			if (tty && !C_CLOCAL(tty) &&
					old_dcd_state && !portdata->dcd_state)
				tty_hangup(tty);
			tty_kref_put(tty);
		} else {
			dev_dbg(&port->dev, "%s: type %x req %x\n",
				__func__, req_pkt->bRequestType,
				req_pkt->bRequest);
		}
	} else
		dev_dbg(&port->dev, "%s: error %d\n", __func__, status);

	
	if (status != -ESHUTDOWN && status != -ENOENT) {
		usb_mark_last_busy(serial->dev);
		err = usb_submit_urb(urb, GFP_ATOMIC);
		if (err && err != -EPERM)
			dev_err(&port->dev, "%s: resubmit intr urb "
				"failed. (%d)\n", __func__, err);
	}
}

static int sierra_write_room(struct tty_struct *tty)
{
	struct usb_serial_port *port = tty->driver_data;
	struct sierra_port_private *portdata = usb_get_serial_port_data(port);
	unsigned long flags;

	dev_dbg(&port->dev, "%s - port %d\n", __func__, port->number);

	spin_lock_irqsave(&portdata->lock, flags);
	if (portdata->outstanding_urbs > (portdata->num_out_urbs * 2) / 3) {
		spin_unlock_irqrestore(&portdata->lock, flags);
		dev_dbg(&port->dev, "%s - write limit hit\n", __func__);
		return 0;
	}
	spin_unlock_irqrestore(&portdata->lock, flags);

	return 2048;
}

static void sierra_stop_rx_urbs(struct usb_serial_port *port)
{
	int i;
	struct sierra_port_private *portdata = usb_get_serial_port_data(port);

	for (i = 0; i < portdata->num_in_urbs; i++)
		usb_kill_urb(portdata->in_urbs[i]);

	usb_kill_urb(port->interrupt_in_urb);
}

static int sierra_submit_rx_urbs(struct usb_serial_port *port, gfp_t mem_flags)
{
	int ok_cnt;
	int err = -EINVAL;
	int i;
	struct urb *urb;
	struct sierra_port_private *portdata = usb_get_serial_port_data(port);

	ok_cnt = 0;
	for (i = 0; i < portdata->num_in_urbs; i++) {
		urb = portdata->in_urbs[i];
		if (!urb)
			continue;
		err = usb_submit_urb(urb, mem_flags);
		if (err) {
			dev_err(&port->dev, "%s: submit urb failed: %d\n",
				__func__, err);
		} else {
			ok_cnt++;
		}
	}

	if (ok_cnt && port->interrupt_in_urb) {
		err = usb_submit_urb(port->interrupt_in_urb, mem_flags);
		if (err) {
			dev_err(&port->dev, "%s: submit intr urb failed: %d\n",
				__func__, err);
		}
	}

	if (ok_cnt > 0) 
		return 0;
	else
		return err;
}

static struct urb *sierra_setup_urb(struct usb_serial *serial, int endpoint,
					int dir, void *ctx, int len,
					gfp_t mem_flags,
					usb_complete_t callback)
{
	struct urb	*urb;
	u8		*buf;

	if (endpoint == -1)
		return NULL;

	urb = usb_alloc_urb(0, mem_flags);
	if (urb == NULL) {
		dev_dbg(&serial->dev->dev, "%s: alloc for endpoint %d failed\n",
			__func__, endpoint);
		return NULL;
	}

	buf = kmalloc(len, mem_flags);
	if (buf) {
		
		usb_fill_bulk_urb(urb, serial->dev,
			usb_sndbulkpipe(serial->dev, endpoint) | dir,
			buf, len, callback, ctx);

		
		dev_dbg(&serial->dev->dev, "%s %c u : %p d:%p\n", __func__,
				dir == USB_DIR_IN ? 'i' : 'o', urb, buf);
	} else {
		dev_dbg(&serial->dev->dev, "%s %c u:%p d:%p\n", __func__,
				dir == USB_DIR_IN ? 'i' : 'o', urb, buf);

		sierra_release_urb(urb);
		urb = NULL;
	}

	return urb;
}

static void sierra_close(struct usb_serial_port *port)
{
	int i;
	struct usb_serial *serial = port->serial;
	struct sierra_port_private *portdata;
	struct sierra_intf_private *intfdata = port->serial->private;


	dev_dbg(&port->dev, "%s\n", __func__);
	portdata = usb_get_serial_port_data(port);

	portdata->rts_state = 0;
	portdata->dtr_state = 0;

	if (serial->dev) {
		mutex_lock(&serial->disc_mutex);
		if (!serial->disconnected) {
			serial->interface->needs_remote_wakeup = 0;
			
			if (!usb_autopm_get_interface(serial->interface))
				sierra_send_setup(port);
			else
				usb_autopm_get_interface_no_resume(serial->interface);
				
		}
		mutex_unlock(&serial->disc_mutex);
		spin_lock_irq(&intfdata->susp_lock);
		portdata->opened = 0;
		spin_unlock_irq(&intfdata->susp_lock);


		
		sierra_stop_rx_urbs(port);
		
		for (i = 0; i < portdata->num_in_urbs; i++) {
			sierra_release_urb(portdata->in_urbs[i]);
			portdata->in_urbs[i] = NULL;
		}
	}
}

static int sierra_open(struct tty_struct *tty, struct usb_serial_port *port)
{
	struct sierra_port_private *portdata;
	struct usb_serial *serial = port->serial;
	struct sierra_intf_private *intfdata = serial->private;
	int i;
	int err;
	int endpoint;
	struct urb *urb;

	portdata = usb_get_serial_port_data(port);

	dev_dbg(&port->dev, "%s\n", __func__);

	
	portdata->rts_state = 1;
	portdata->dtr_state = 1;


	endpoint = port->bulk_in_endpointAddress;
	for (i = 0; i < portdata->num_in_urbs; i++) {
		urb = sierra_setup_urb(serial, endpoint, USB_DIR_IN, port,
					IN_BUFLEN, GFP_KERNEL,
					sierra_indat_callback);
		portdata->in_urbs[i] = urb;
	}
	
	usb_clear_halt(serial->dev,
			usb_sndbulkpipe(serial->dev, endpoint) | USB_DIR_IN);

	err = sierra_submit_rx_urbs(port, GFP_KERNEL);
	if (err) {
		
		sierra_close(port);
		
		if (!serial->disconnected)
			usb_autopm_put_interface(serial->interface);
		return err;
	}
	sierra_send_setup(port);

	serial->interface->needs_remote_wakeup = 1;
	spin_lock_irq(&intfdata->susp_lock);
	portdata->opened = 1;
	spin_unlock_irq(&intfdata->susp_lock);
	usb_autopm_put_interface(serial->interface);

	return 0;
}


static void sierra_dtr_rts(struct usb_serial_port *port, int on)
{
	struct usb_serial *serial = port->serial;
	struct sierra_port_private *portdata;

	portdata = usb_get_serial_port_data(port);
	portdata->rts_state = on;
	portdata->dtr_state = on;

	if (serial->dev) {
		mutex_lock(&serial->disc_mutex);
		if (!serial->disconnected)
			sierra_send_setup(port);
		mutex_unlock(&serial->disc_mutex);
	}
}

static int sierra_startup(struct usb_serial *serial)
{
	struct usb_serial_port *port;
	struct sierra_port_private *portdata;
	struct sierra_iface_info *himemoryp = NULL;
	int i;
	u8 ifnum;

	dev_dbg(&serial->dev->dev, "%s\n", __func__);

	
	sierra_set_power_state(serial->dev, 0x0000);

	
	if (nmea)
		sierra_vsc_set_nmea(serial->dev, 1);

	
	for (i = 0; i < serial->num_ports; i++) {
		port = serial->port[i];
		portdata = kzalloc(sizeof(*portdata), GFP_KERNEL);
		if (!portdata) {
			dev_dbg(&port->dev, "%s: kmalloc for "
				"sierra_port_private (%d) failed!\n",
				__func__, i);
			return -ENOMEM;
		}
		spin_lock_init(&portdata->lock);
		init_usb_anchor(&portdata->active);
		init_usb_anchor(&portdata->delayed);
		ifnum = i;
		
		portdata->num_out_urbs = N_OUT_URB;
		portdata->num_in_urbs  = N_IN_URB;

		
		if (serial->num_ports == 1) {
			
			ifnum = sierra_calc_interface(serial);
			himemoryp =
			    (struct sierra_iface_info *)&typeB_interface_list;
			if (is_himemory(ifnum, himemoryp)) {
				portdata->num_out_urbs = N_OUT_URB_HM;
				portdata->num_in_urbs  = N_IN_URB_HM;
			}
		}
		else {
			himemoryp =
			    (struct sierra_iface_info *)&typeA_interface_list;
			if (is_himemory(i, himemoryp)) {
				portdata->num_out_urbs = N_OUT_URB_HM;
				portdata->num_in_urbs  = N_IN_URB_HM;
			}
		}
		dev_dbg(&serial->dev->dev,
			"Memory usage (urbs) interface #%d, in=%d, out=%d\n",
			ifnum,portdata->num_in_urbs, portdata->num_out_urbs );
		
		usb_set_serial_port_data(port, portdata);
	}

	return 0;
}

static void sierra_release(struct usb_serial *serial)
{
	int i;
	struct usb_serial_port *port;
	struct sierra_port_private *portdata;

	dev_dbg(&serial->dev->dev, "%s\n", __func__);

	for (i = 0; i < serial->num_ports; ++i) {
		port = serial->port[i];
		if (!port)
			continue;
		portdata = usb_get_serial_port_data(port);
		if (!portdata)
			continue;
		kfree(portdata);
	}
}

#ifdef CONFIG_PM
static void stop_read_write_urbs(struct usb_serial *serial)
{
	int i;
	struct usb_serial_port *port;
	struct sierra_port_private *portdata;

	
	for (i = 0; i < serial->num_ports; ++i) {
		port = serial->port[i];
		portdata = usb_get_serial_port_data(port);
		sierra_stop_rx_urbs(port);
		usb_kill_anchored_urbs(&portdata->active);
	}
}

static int sierra_suspend(struct usb_serial *serial, pm_message_t message)
{
	struct sierra_intf_private *intfdata;
	int b;

	if (PMSG_IS_AUTO(message)) {
		intfdata = serial->private;
		spin_lock_irq(&intfdata->susp_lock);
		b = intfdata->in_flight;

		if (b) {
			spin_unlock_irq(&intfdata->susp_lock);
			return -EBUSY;
		} else {
			intfdata->suspended = 1;
			spin_unlock_irq(&intfdata->susp_lock);
		}
	}
	stop_read_write_urbs(serial);

	return 0;
}

static int sierra_resume(struct usb_serial *serial)
{
	struct usb_serial_port *port;
	struct sierra_intf_private *intfdata = serial->private;
	struct sierra_port_private *portdata;
	struct urb *urb;
	int ec = 0;
	int i, err;

	spin_lock_irq(&intfdata->susp_lock);
	for (i = 0; i < serial->num_ports; i++) {
		port = serial->port[i];
		portdata = usb_get_serial_port_data(port);

		while ((urb = usb_get_from_anchor(&portdata->delayed))) {
			usb_anchor_urb(urb, &portdata->active);
			intfdata->in_flight++;
			err = usb_submit_urb(urb, GFP_ATOMIC);
			if (err < 0) {
				intfdata->in_flight--;
				usb_unanchor_urb(urb);
				usb_scuttle_anchored_urbs(&portdata->delayed);
				break;
			}
		}

		if (portdata->opened) {
			err = sierra_submit_rx_urbs(port, GFP_ATOMIC);
			if (err)
				ec++;
		}
	}
	intfdata->suspended = 0;
	spin_unlock_irq(&intfdata->susp_lock);

	return ec ? -EIO : 0;
}

static int sierra_reset_resume(struct usb_interface *intf)
{
	struct usb_serial *serial = usb_get_intfdata(intf);
	dev_err(&serial->dev->dev, "%s\n", __func__);
	return usb_serial_resume(intf);
}
#else
#define sierra_suspend NULL
#define sierra_resume NULL
#define sierra_reset_resume NULL
#endif

static struct usb_driver sierra_driver = {
	.name       = "sierra",
	.probe      = usb_serial_probe,
	.disconnect = usb_serial_disconnect,
	.suspend    = usb_serial_suspend,
	.resume     = usb_serial_resume,
	.reset_resume = sierra_reset_resume,
	.id_table   = id_table,
	.supports_autosuspend =	1,
};

static struct usb_serial_driver sierra_device = {
	.driver = {
		.owner =	THIS_MODULE,
		.name =		"sierra",
	},
	.description       = "Sierra USB modem",
	.id_table          = id_table,
	.calc_num_ports	   = sierra_calc_num_ports,
	.probe		   = sierra_probe,
	.open              = sierra_open,
	.close             = sierra_close,
	.dtr_rts	   = sierra_dtr_rts,
	.write             = sierra_write,
	.write_room        = sierra_write_room,
	.set_termios       = sierra_set_termios,
	.tiocmget          = sierra_tiocmget,
	.tiocmset          = sierra_tiocmset,
	.attach            = sierra_startup,
	.release           = sierra_release,
	.suspend	   = sierra_suspend,
	.resume		   = sierra_resume,
	.read_int_callback = sierra_instat_callback,
};

static struct usb_serial_driver * const serial_drivers[] = {
	&sierra_device, NULL
};

module_usb_serial_driver(sierra_driver, serial_drivers);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPL");

module_param(nmea, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(nmea, "NMEA streaming");

module_param(debug, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "Debug messages");
