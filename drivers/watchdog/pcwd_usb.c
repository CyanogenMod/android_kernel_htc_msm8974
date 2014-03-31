/*
 *	Berkshire USB-PC Watchdog Card Driver
 *
 *	(c) Copyright 2004-2007 Wim Van Sebroeck <wim@iguana.be>.
 *
 *	Based on source code of the following authors:
 *	  Ken Hollis <kenji@bitgate.com>,
 *	  Alan Cox <alan@lxorguk.ukuu.org.uk>,
 *	  Matt Domsch <Matt_Domsch@dell.com>,
 *	  Rob Radez <rob@osinvestor.com>,
 *	  Greg Kroah-Hartman <greg@kroah.com>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 *	Neither Wim Van Sebroeck nor Iguana vzw. admit liability nor
 *	provide warranty for any of this software. This material is
 *	provided "AS-IS" and at no charge.
 *
 *	Thanks also to Simon Machell at Berkshire Products Inc. for
 *	providing the test hardware. More info is available at
 *	http://www.berkprod.com/ or http://www.pcwatchdog.com/
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>	
#include <linux/moduleparam.h>	
#include <linux/types.h>	
#include <linux/errno.h>	
#include <linux/kernel.h>	
#include <linux/delay.h>	
#include <linux/miscdevice.h>	
#include <linux/watchdog.h>	
#include <linux/notifier.h>	
#include <linux/reboot.h>	
#include <linux/init.h>		
#include <linux/fs.h>		
#include <linux/usb.h>		
#include <linux/slab.h>		
#include <linux/mutex.h>	
#include <linux/hid.h>		
#include <linux/uaccess.h>	

#ifdef CONFIG_USB_DEBUG
static int debug = 1;
#else
static int debug;
#endif


#undef dbg
#ifndef DEBUG
#define DEBUG
#endif
#define dbg(format, ...)				\
do {							\
	if (debug)					\
		pr_debug(format "\n", ##__VA_ARGS__);	\
} while (0)

#define DRIVER_VERSION "1.02"
#define DRIVER_AUTHOR "Wim Van Sebroeck <wim@iguana.be>"
#define DRIVER_DESC "Berkshire USB-PC Watchdog driver"
#define DRIVER_LICENSE "GPL"
#define DRIVER_NAME "pcwd_usb"

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE(DRIVER_LICENSE);
MODULE_ALIAS_MISCDEV(WATCHDOG_MINOR);
MODULE_ALIAS_MISCDEV(TEMP_MINOR);

module_param(debug, int, 0);
MODULE_PARM_DESC(debug, "Debug enabled or not");

#define WATCHDOG_HEARTBEAT 0	
static int heartbeat = WATCHDOG_HEARTBEAT;
module_param(heartbeat, int, 0);
MODULE_PARM_DESC(heartbeat, "Watchdog heartbeat in seconds. "
	"(0<heartbeat<65536 or 0=delay-time from dip-switches, default="
				__MODULE_STRING(WATCHDOG_HEARTBEAT) ")");

static bool nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started (default="
				__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

#define USB_PCWD_VENDOR_ID	0x0c98
#define USB_PCWD_PRODUCT_ID	0x1140

static struct usb_device_id usb_pcwd_table[] = {
	{ USB_DEVICE(USB_PCWD_VENDOR_ID, USB_PCWD_PRODUCT_ID) },
	{ }					
};
MODULE_DEVICE_TABLE(usb, usb_pcwd_table);

#define USB_COMMAND_TIMEOUT	250

#define CMD_READ_TEMP			0x02	
#define CMD_TRIGGER			CMD_READ_TEMP
#define CMD_GET_STATUS			0x04	
#define CMD_GET_FIRMWARE_VERSION	0x08	
#define CMD_GET_DIP_SWITCH_SETTINGS	0x0c	
#define CMD_READ_WATCHDOG_TIMEOUT	0x18	
#define CMD_WRITE_WATCHDOG_TIMEOUT	0x19	
#define CMD_ENABLE_WATCHDOG		0x30	
#define CMD_DISABLE_WATCHDOG		CMD_ENABLE_WATCHDOG

static const int heartbeat_tbl[] = {
	5,	
	10,	
	30,	
	60,	
	300,	
	600,	
	1800,	
	3600,	
};

static int cards_found;

static unsigned long is_active;
static char expect_release;

struct usb_pcwd_private {
	
	struct usb_device	*udev;
	
	struct usb_interface	*interface;

	
	unsigned int		interface_number;

	
	unsigned char		*intr_buffer;
	
	dma_addr_t		intr_dma;
	
	size_t			intr_size;
	
	struct urb		*intr_urb;

	
	unsigned char		cmd_command;
	
	unsigned char		cmd_data_msb;
	
	unsigned char		cmd_data_lsb;
	
	atomic_t		cmd_received;

	
	int			exists;
	
	struct mutex		mtx;
};
static struct usb_pcwd_private *usb_pcwd_device;

static DEFINE_MUTEX(disconnect_mutex);

static int usb_pcwd_probe(struct usb_interface *interface,
						const struct usb_device_id *id);
static void usb_pcwd_disconnect(struct usb_interface *interface);

static struct usb_driver usb_pcwd_driver = {
	.name =		DRIVER_NAME,
	.probe =	usb_pcwd_probe,
	.disconnect =	usb_pcwd_disconnect,
	.id_table =	usb_pcwd_table,
};


static void usb_pcwd_intr_done(struct urb *urb)
{
	struct usb_pcwd_private *usb_pcwd =
				(struct usb_pcwd_private *)urb->context;
	unsigned char *data = usb_pcwd->intr_buffer;
	int retval;

	switch (urb->status) {
	case 0:			
		break;
	case -ECONNRESET:	
	case -ENOENT:
	case -ESHUTDOWN:
		
		dbg("%s - urb shutting down with status: %d", __func__,
								urb->status);
		return;
	
	default:		
		dbg("%s - nonzero urb status received: %d", __func__,
								urb->status);
		goto resubmit;
	}

	dbg("received following data cmd=0x%02x msb=0x%02x lsb=0x%02x",
		data[0], data[1], data[2]);

	usb_pcwd->cmd_command  = data[0];
	usb_pcwd->cmd_data_msb = data[1];
	usb_pcwd->cmd_data_lsb = data[2];

	
	atomic_set(&usb_pcwd->cmd_received, 1);

resubmit:
	retval = usb_submit_urb(urb, GFP_ATOMIC);
	if (retval)
		pr_err("can't resubmit intr, usb_submit_urb failed with result %d\n",
		       retval);
}

static int usb_pcwd_send_command(struct usb_pcwd_private *usb_pcwd,
		unsigned char cmd, unsigned char *msb, unsigned char *lsb)
{
	int got_response, count;
	unsigned char buf[6];

	if ((!usb_pcwd) || (!usb_pcwd->exists))
		return -1;

	buf[0] = cmd;			
	buf[1] = *msb;			
	buf[2] = *lsb;			
	buf[3] = buf[4] = buf[5] = 0;	

	dbg("sending following data cmd=0x%02x msb=0x%02x lsb=0x%02x",
		buf[0], buf[1], buf[2]);

	atomic_set(&usb_pcwd->cmd_received, 0);

	if (usb_control_msg(usb_pcwd->udev, usb_sndctrlpipe(usb_pcwd->udev, 0),
			HID_REQ_SET_REPORT, HID_DT_REPORT,
			0x0200, usb_pcwd->interface_number, buf, sizeof(buf),
			USB_COMMAND_TIMEOUT) != sizeof(buf)) {
		dbg("usb_pcwd_send_command: error in usb_control_msg for "
				"cmd 0x%x 0x%x 0x%x\n", cmd, *msb, *lsb);
	}
	got_response = 0;
	for (count = 0; (count < USB_COMMAND_TIMEOUT) && (!got_response);
								count++) {
		mdelay(1);
		if (atomic_read(&usb_pcwd->cmd_received))
			got_response = 1;
	}

	if ((got_response) && (cmd == usb_pcwd->cmd_command)) {
		
		*msb = usb_pcwd->cmd_data_msb;
		*lsb = usb_pcwd->cmd_data_lsb;
	}

	return got_response;
}

static int usb_pcwd_start(struct usb_pcwd_private *usb_pcwd)
{
	unsigned char msb = 0x00;
	unsigned char lsb = 0x00;
	int retval;

	
	retval = usb_pcwd_send_command(usb_pcwd, CMD_ENABLE_WATCHDOG,
								&msb, &lsb);

	if ((retval == 0) || (lsb == 0)) {
		pr_err("Card did not acknowledge enable attempt\n");
		return -1;
	}

	return 0;
}

static int usb_pcwd_stop(struct usb_pcwd_private *usb_pcwd)
{
	unsigned char msb = 0xA5;
	unsigned char lsb = 0xC3;
	int retval;

	
	retval = usb_pcwd_send_command(usb_pcwd, CMD_DISABLE_WATCHDOG,
								&msb, &lsb);

	if ((retval == 0) || (lsb != 0)) {
		pr_err("Card did not acknowledge disable attempt\n");
		return -1;
	}

	return 0;
}

static int usb_pcwd_keepalive(struct usb_pcwd_private *usb_pcwd)
{
	unsigned char dummy;

	
	usb_pcwd_send_command(usb_pcwd, CMD_TRIGGER, &dummy, &dummy);

	return 0;
}

static int usb_pcwd_set_heartbeat(struct usb_pcwd_private *usb_pcwd, int t)
{
	unsigned char msb = t / 256;
	unsigned char lsb = t % 256;

	if ((t < 0x0001) || (t > 0xFFFF))
		return -EINVAL;

	
	usb_pcwd_send_command(usb_pcwd, CMD_WRITE_WATCHDOG_TIMEOUT, &msb, &lsb);

	heartbeat = t;
	return 0;
}

static int usb_pcwd_get_temperature(struct usb_pcwd_private *usb_pcwd,
							int *temperature)
{
	unsigned char msb, lsb;

	usb_pcwd_send_command(usb_pcwd, CMD_READ_TEMP, &msb, &lsb);

	*temperature = (lsb * 9 / 5) + 32;

	return 0;
}

static int usb_pcwd_get_timeleft(struct usb_pcwd_private *usb_pcwd,
								int *time_left)
{
	unsigned char msb, lsb;

	
	
	usb_pcwd_send_command(usb_pcwd, CMD_READ_WATCHDOG_TIMEOUT, &msb, &lsb);

	*time_left = (msb << 8) + lsb;

	return 0;
}


static ssize_t usb_pcwd_write(struct file *file, const char __user *data,
						size_t len, loff_t *ppos)
{
	
	if (len) {
		if (!nowayout) {
			size_t i;

			expect_release = 0;

			for (i = 0; i != len; i++) {
				char c;
				if (get_user(c, data + i))
					return -EFAULT;
				if (c == 'V')
					expect_release = 42;
			}
		}

		
		usb_pcwd_keepalive(usb_pcwd_device);
	}
	return len;
}

static long usb_pcwd_ioctl(struct file *file, unsigned int cmd,
						unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int __user *p = argp;
	static const struct watchdog_info ident = {
		.options =		WDIOF_KEEPALIVEPING |
					WDIOF_SETTIMEOUT |
					WDIOF_MAGICCLOSE,
		.firmware_version =	1,
		.identity =		DRIVER_NAME,
	};

	switch (cmd) {
	case WDIOC_GETSUPPORT:
		return copy_to_user(argp, &ident, sizeof(ident)) ? -EFAULT : 0;

	case WDIOC_GETSTATUS:
	case WDIOC_GETBOOTSTATUS:
		return put_user(0, p);

	case WDIOC_GETTEMP:
	{
		int temperature;

		if (usb_pcwd_get_temperature(usb_pcwd_device, &temperature))
			return -EFAULT;

		return put_user(temperature, p);
	}

	case WDIOC_SETOPTIONS:
	{
		int new_options, retval = -EINVAL;

		if (get_user(new_options, p))
			return -EFAULT;

		if (new_options & WDIOS_DISABLECARD) {
			usb_pcwd_stop(usb_pcwd_device);
			retval = 0;
		}

		if (new_options & WDIOS_ENABLECARD) {
			usb_pcwd_start(usb_pcwd_device);
			retval = 0;
		}

		return retval;
	}

	case WDIOC_KEEPALIVE:
		usb_pcwd_keepalive(usb_pcwd_device);
		return 0;

	case WDIOC_SETTIMEOUT:
	{
		int new_heartbeat;

		if (get_user(new_heartbeat, p))
			return -EFAULT;

		if (usb_pcwd_set_heartbeat(usb_pcwd_device, new_heartbeat))
			return -EINVAL;

		usb_pcwd_keepalive(usb_pcwd_device);
		
	}

	case WDIOC_GETTIMEOUT:
		return put_user(heartbeat, p);

	case WDIOC_GETTIMELEFT:
	{
		int time_left;

		if (usb_pcwd_get_timeleft(usb_pcwd_device, &time_left))
			return -EFAULT;

		return put_user(time_left, p);
	}

	default:
		return -ENOTTY;
	}
}

static int usb_pcwd_open(struct inode *inode, struct file *file)
{
	
	if (test_and_set_bit(0, &is_active))
		return -EBUSY;

	
	usb_pcwd_start(usb_pcwd_device);
	usb_pcwd_keepalive(usb_pcwd_device);
	return nonseekable_open(inode, file);
}

static int usb_pcwd_release(struct inode *inode, struct file *file)
{
	if (expect_release == 42) {
		usb_pcwd_stop(usb_pcwd_device);
	} else {
		pr_crit("Unexpected close, not stopping watchdog!\n");
		usb_pcwd_keepalive(usb_pcwd_device);
	}
	expect_release = 0;
	clear_bit(0, &is_active);
	return 0;
}


static ssize_t usb_pcwd_temperature_read(struct file *file, char __user *data,
				size_t len, loff_t *ppos)
{
	int temperature;

	if (usb_pcwd_get_temperature(usb_pcwd_device, &temperature))
		return -EFAULT;

	if (copy_to_user(data, &temperature, 1))
		return -EFAULT;

	return 1;
}

static int usb_pcwd_temperature_open(struct inode *inode, struct file *file)
{
	return nonseekable_open(inode, file);
}

static int usb_pcwd_temperature_release(struct inode *inode, struct file *file)
{
	return 0;
}


static int usb_pcwd_notify_sys(struct notifier_block *this, unsigned long code,
								void *unused)
{
	if (code == SYS_DOWN || code == SYS_HALT)
		usb_pcwd_stop(usb_pcwd_device);	

	return NOTIFY_DONE;
}


static const struct file_operations usb_pcwd_fops = {
	.owner =	THIS_MODULE,
	.llseek =	no_llseek,
	.write =	usb_pcwd_write,
	.unlocked_ioctl = usb_pcwd_ioctl,
	.open =		usb_pcwd_open,
	.release =	usb_pcwd_release,
};

static struct miscdevice usb_pcwd_miscdev = {
	.minor =	WATCHDOG_MINOR,
	.name =		"watchdog",
	.fops =		&usb_pcwd_fops,
};

static const struct file_operations usb_pcwd_temperature_fops = {
	.owner =	THIS_MODULE,
	.llseek =	no_llseek,
	.read =		usb_pcwd_temperature_read,
	.open =		usb_pcwd_temperature_open,
	.release =	usb_pcwd_temperature_release,
};

static struct miscdevice usb_pcwd_temperature_miscdev = {
	.minor =	TEMP_MINOR,
	.name =		"temperature",
	.fops =		&usb_pcwd_temperature_fops,
};

static struct notifier_block usb_pcwd_notifier = {
	.notifier_call =	usb_pcwd_notify_sys,
};

static inline void usb_pcwd_delete(struct usb_pcwd_private *usb_pcwd)
{
	usb_free_urb(usb_pcwd->intr_urb);
	if (usb_pcwd->intr_buffer != NULL)
		usb_free_coherent(usb_pcwd->udev, usb_pcwd->intr_size,
				  usb_pcwd->intr_buffer, usb_pcwd->intr_dma);
	kfree(usb_pcwd);
}

static int usb_pcwd_probe(struct usb_interface *interface,
						const struct usb_device_id *id)
{
	struct usb_device *udev = interface_to_usbdev(interface);
	struct usb_host_interface *iface_desc;
	struct usb_endpoint_descriptor *endpoint;
	struct usb_pcwd_private *usb_pcwd = NULL;
	int pipe, maxp;
	int retval = -ENOMEM;
	int got_fw_rev;
	unsigned char fw_rev_major, fw_rev_minor;
	char fw_ver_str[20];
	unsigned char option_switches, dummy;

	cards_found++;
	if (cards_found > 1) {
		pr_err("This driver only supports 1 device\n");
		return -ENODEV;
	}

	
	iface_desc = interface->cur_altsetting;

	
	if (!(iface_desc->desc.bInterfaceClass == USB_CLASS_HID)) {
		pr_err("The device isn't a Human Interface Device\n");
		return -ENODEV;
	}

	
	endpoint = &iface_desc->endpoint[0].desc;

	if (!usb_endpoint_is_int_in(endpoint)) {
		
		pr_err("Couldn't find an INTR & IN endpoint\n");
		return -ENODEV;
	}

	
	pipe = usb_rcvintpipe(udev, endpoint->bEndpointAddress);
	maxp = usb_maxpacket(udev, pipe, usb_pipeout(pipe));

	
	usb_pcwd = kzalloc(sizeof(struct usb_pcwd_private), GFP_KERNEL);
	if (usb_pcwd == NULL) {
		pr_err("Out of memory\n");
		goto error;
	}

	usb_pcwd_device = usb_pcwd;

	mutex_init(&usb_pcwd->mtx);
	usb_pcwd->udev = udev;
	usb_pcwd->interface = interface;
	usb_pcwd->interface_number = iface_desc->desc.bInterfaceNumber;
	usb_pcwd->intr_size = (le16_to_cpu(endpoint->wMaxPacketSize) > 8 ?
				le16_to_cpu(endpoint->wMaxPacketSize) : 8);

	
	usb_pcwd->intr_buffer = usb_alloc_coherent(udev, usb_pcwd->intr_size,
					GFP_ATOMIC, &usb_pcwd->intr_dma);
	if (!usb_pcwd->intr_buffer) {
		pr_err("Out of memory\n");
		goto error;
	}

	
	usb_pcwd->intr_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!usb_pcwd->intr_urb) {
		pr_err("Out of memory\n");
		goto error;
	}

	
	usb_fill_int_urb(usb_pcwd->intr_urb, udev, pipe,
			usb_pcwd->intr_buffer, usb_pcwd->intr_size,
			usb_pcwd_intr_done, usb_pcwd, endpoint->bInterval);
	usb_pcwd->intr_urb->transfer_dma = usb_pcwd->intr_dma;
	usb_pcwd->intr_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	
	if (usb_submit_urb(usb_pcwd->intr_urb, GFP_KERNEL)) {
		pr_err("Problem registering interrupt URB\n");
		retval = -EIO; 
		goto error;
	}

	
	usb_pcwd->exists = 1;

	
	usb_pcwd_stop(usb_pcwd);

	
	got_fw_rev = usb_pcwd_send_command(usb_pcwd, CMD_GET_FIRMWARE_VERSION,
						&fw_rev_major, &fw_rev_minor);
	if (got_fw_rev)
		sprintf(fw_ver_str, "%u.%02u", fw_rev_major, fw_rev_minor);
	else
		sprintf(fw_ver_str, "<card no answer>");

	pr_info("Found card (Firmware: %s) with temp option\n", fw_ver_str);

	
	usb_pcwd_send_command(usb_pcwd, CMD_GET_DIP_SWITCH_SETTINGS, &dummy,
							&option_switches);

	pr_info("Option switches (0x%02x): Temperature Reset Enable=%s, Power On Delay=%s\n",
		option_switches,
		((option_switches & 0x10) ? "ON" : "OFF"),
		((option_switches & 0x08) ? "ON" : "OFF"));

	
	if (heartbeat == 0)
		heartbeat = heartbeat_tbl[(option_switches & 0x07)];

	if (usb_pcwd_set_heartbeat(usb_pcwd, heartbeat)) {
		usb_pcwd_set_heartbeat(usb_pcwd, WATCHDOG_HEARTBEAT);
		pr_info("heartbeat value must be 0<heartbeat<65536, using %d\n",
			WATCHDOG_HEARTBEAT);
	}

	retval = register_reboot_notifier(&usb_pcwd_notifier);
	if (retval != 0) {
		pr_err("cannot register reboot notifier (err=%d)\n", retval);
		goto error;
	}

	retval = misc_register(&usb_pcwd_temperature_miscdev);
	if (retval != 0) {
		pr_err("cannot register miscdev on minor=%d (err=%d)\n",
		       TEMP_MINOR, retval);
		goto err_out_unregister_reboot;
	}

	retval = misc_register(&usb_pcwd_miscdev);
	if (retval != 0) {
		pr_err("cannot register miscdev on minor=%d (err=%d)\n",
		       WATCHDOG_MINOR, retval);
		goto err_out_misc_deregister;
	}

	
	usb_set_intfdata(interface, usb_pcwd);

	pr_info("initialized. heartbeat=%d sec (nowayout=%d)\n",
		heartbeat, nowayout);

	return 0;

err_out_misc_deregister:
	misc_deregister(&usb_pcwd_temperature_miscdev);
err_out_unregister_reboot:
	unregister_reboot_notifier(&usb_pcwd_notifier);
error:
	if (usb_pcwd)
		usb_pcwd_delete(usb_pcwd);
	usb_pcwd_device = NULL;
	return retval;
}


static void usb_pcwd_disconnect(struct usb_interface *interface)
{
	struct usb_pcwd_private *usb_pcwd;

	
	mutex_lock(&disconnect_mutex);

	usb_pcwd = usb_get_intfdata(interface);
	usb_set_intfdata(interface, NULL);

	mutex_lock(&usb_pcwd->mtx);

	
	if (!nowayout)
		usb_pcwd_stop(usb_pcwd);

	
	usb_pcwd->exists = 0;

	
	misc_deregister(&usb_pcwd_miscdev);
	misc_deregister(&usb_pcwd_temperature_miscdev);
	unregister_reboot_notifier(&usb_pcwd_notifier);

	mutex_unlock(&usb_pcwd->mtx);

	
	usb_pcwd_delete(usb_pcwd);

	cards_found--;

	mutex_unlock(&disconnect_mutex);

	pr_info("USB PC Watchdog disconnected\n");
}

module_usb_driver(usb_pcwd_driver);
