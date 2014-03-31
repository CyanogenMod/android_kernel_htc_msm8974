/*
 * USB Keyspan PDA / Xircom / Entregra Converter driver
 *
 * Copyright (C) 1999 - 2001 Greg Kroah-Hartman	<greg@kroah.com>
 * Copyright (C) 1999, 2000 Brian Warner	<warner@lothar.com>
 * Copyright (C) 2000 Al Borchers		<borchers@steinerpoint.com>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 * See Documentation/usb/usb-serial.txt for more information on using this
 * driver
 */


#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/firmware.h>
#include <linux/ihex.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <linux/usb/serial.h>

static bool debug;

#if defined(CONFIG_USB_SERIAL_KEYSPAN_PDA) || defined(CONFIG_USB_SERIAL_KEYSPAN_PDA_MODULE)
	#define KEYSPAN
#else
	#undef KEYSPAN
#endif
#if defined(CONFIG_USB_SERIAL_XIRCOM) || defined(CONFIG_USB_SERIAL_XIRCOM_MODULE)
	#define XIRCOM
#else
	#undef XIRCOM
#endif

#define DRIVER_VERSION "v1.1"
#define DRIVER_AUTHOR "Brian Warner <warner@lothar.com>"
#define DRIVER_DESC "USB Keyspan PDA Converter driver"

struct keyspan_pda_private {
	int			tx_room;
	int			tx_throttled;
	struct work_struct			wakeup_work;
	struct work_struct			unthrottle_work;
	struct usb_serial	*serial;
	struct usb_serial_port	*port;
};


#define KEYSPAN_VENDOR_ID		0x06cd
#define KEYSPAN_PDA_FAKE_ID		0x0103
#define KEYSPAN_PDA_ID			0x0104 

#define XIRCOM_VENDOR_ID		0x085a
#define XIRCOM_FAKE_ID			0x8027
#define ENTREGRA_VENDOR_ID		0x1645
#define ENTREGRA_FAKE_ID		0x8093

static const struct usb_device_id id_table_combined[] = {
#ifdef KEYSPAN
	{ USB_DEVICE(KEYSPAN_VENDOR_ID, KEYSPAN_PDA_FAKE_ID) },
#endif
#ifdef XIRCOM
	{ USB_DEVICE(XIRCOM_VENDOR_ID, XIRCOM_FAKE_ID) },
	{ USB_DEVICE(ENTREGRA_VENDOR_ID, ENTREGRA_FAKE_ID) },
#endif
	{ USB_DEVICE(KEYSPAN_VENDOR_ID, KEYSPAN_PDA_ID) },
	{ }						
};

MODULE_DEVICE_TABLE(usb, id_table_combined);

static struct usb_driver keyspan_pda_driver = {
	.name =		"keyspan_pda",
	.probe =	usb_serial_probe,
	.disconnect =	usb_serial_disconnect,
	.id_table =	id_table_combined,
};

static const struct usb_device_id id_table_std[] = {
	{ USB_DEVICE(KEYSPAN_VENDOR_ID, KEYSPAN_PDA_ID) },
	{ }						
};

#ifdef KEYSPAN
static const struct usb_device_id id_table_fake[] = {
	{ USB_DEVICE(KEYSPAN_VENDOR_ID, KEYSPAN_PDA_FAKE_ID) },
	{ }						
};
#endif

#ifdef XIRCOM
static const struct usb_device_id id_table_fake_xircom[] = {
	{ USB_DEVICE(XIRCOM_VENDOR_ID, XIRCOM_FAKE_ID) },
	{ USB_DEVICE(ENTREGRA_VENDOR_ID, ENTREGRA_FAKE_ID) },
	{ }
};
#endif

static void keyspan_pda_wakeup_write(struct work_struct *work)
{
	struct keyspan_pda_private *priv =
		container_of(work, struct keyspan_pda_private, wakeup_work);
	struct usb_serial_port *port = priv->port;
	struct tty_struct *tty = tty_port_tty_get(&port->port);
	if (tty)
		tty_wakeup(tty);
	tty_kref_put(tty);
}

static void keyspan_pda_request_unthrottle(struct work_struct *work)
{
	struct keyspan_pda_private *priv =
		container_of(work, struct keyspan_pda_private, unthrottle_work);
	struct usb_serial *serial = priv->serial;
	int result;

	dbg(" request_unthrottle");
	result = usb_control_msg(serial->dev,
				 usb_sndctrlpipe(serial->dev, 0),
				 7, 
				 USB_TYPE_VENDOR | USB_RECIP_INTERFACE
				 | USB_DIR_OUT,
				 16, 
				 0, 
				 NULL,
				 0,
				 2000);
	if (result < 0)
		dbg("%s - error %d from usb_control_msg",
		    __func__, result);
}


static void keyspan_pda_rx_interrupt(struct urb *urb)
{
	struct usb_serial_port *port = urb->context;
	struct tty_struct *tty;
	unsigned char *data = urb->transfer_buffer;
	int retval;
	int status = urb->status;
	struct keyspan_pda_private *priv;
	priv = usb_get_serial_port_data(port);

	switch (status) {
	case 0:
		
		break;
	case -ECONNRESET:
	case -ENOENT:
	case -ESHUTDOWN:
		
		dbg("%s - urb shutting down with status: %d",
		    __func__, status);
		return;
	default:
		dbg("%s - nonzero urb status received: %d",
		    __func__, status);
		goto exit;
	}

	
	switch (data[0]) {
	case 0:
		tty = tty_port_tty_get(&port->port);
		 
		if (tty && urb->actual_length) {
			tty_insert_flip_string(tty, data + 1,
						urb->actual_length - 1);
			tty_flip_buffer_push(tty);
		}
		tty_kref_put(tty);
		break;
	case 1:
		
		dbg(" rx int, d1=%d, d2=%d", data[1], data[2]);
		switch (data[1]) {
		case 1: 
			break;
		case 2: 
			priv->tx_throttled = 0;
			
			schedule_work(&priv->wakeup_work);
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}

exit:
	retval = usb_submit_urb(urb, GFP_ATOMIC);
	if (retval)
		dev_err(&port->dev,
			"%s - usb_submit_urb failed with result %d",
			__func__, retval);
}


static void keyspan_pda_rx_throttle(struct tty_struct *tty)
{
	struct usb_serial_port *port = tty->driver_data;
	dbg("keyspan_pda_rx_throttle port %d", port->number);
	usb_kill_urb(port->interrupt_in_urb);
}


static void keyspan_pda_rx_unthrottle(struct tty_struct *tty)
{
	struct usb_serial_port *port = tty->driver_data;
	
	dbg("keyspan_pda_rx_unthrottle port %d", port->number);
	if (usb_submit_urb(port->interrupt_in_urb, GFP_KERNEL))
		dbg(" usb_submit_urb(read urb) failed");
}


static speed_t keyspan_pda_setbaud(struct usb_serial *serial, speed_t baud)
{
	int rc;
	int bindex;

	switch (baud) {
	case 110:
		bindex = 0;
		break;
	case 300:
		bindex = 1;
		break;
	case 1200:
		bindex = 2;
		break;
	case 2400:
		bindex = 3;
		break;
	case 4800:
		bindex = 4;
		break;
	case 9600:
		bindex = 5;
		break;
	case 19200:
		bindex = 6;
		break;
	case 38400:
		bindex = 7;
		break;
	case 57600:
		bindex = 8;
		break;
	case 115200:
		bindex = 9;
		break;
	default:
		bindex = 5;	
		baud = 9600;
	}

	rc = usb_control_msg(serial->dev, usb_sndctrlpipe(serial->dev, 0),
			     0, 
			     USB_TYPE_VENDOR
			     | USB_RECIP_INTERFACE
			     | USB_DIR_OUT, 
			     bindex, 
			     0, 
			     NULL, 
			     0, 
			     2000); 
	if (rc < 0)
		return 0;
	return baud;
}


static void keyspan_pda_break_ctl(struct tty_struct *tty, int break_state)
{
	struct usb_serial_port *port = tty->driver_data;
	struct usb_serial *serial = port->serial;
	int value;
	int result;

	if (break_state == -1)
		value = 1; 
	else
		value = 0; 
	result = usb_control_msg(serial->dev, usb_sndctrlpipe(serial->dev, 0),
			4, 
			USB_TYPE_VENDOR | USB_RECIP_INTERFACE | USB_DIR_OUT,
			value, 0, NULL, 0, 2000);
	if (result < 0)
		dbg("%s - error %d from usb_control_msg",
		    __func__, result);
}


static void keyspan_pda_set_termios(struct tty_struct *tty,
		struct usb_serial_port *port, struct ktermios *old_termios)
{
	struct usb_serial *serial = port->serial;
	speed_t speed;


	speed = tty_get_baud_rate(tty);
	speed = keyspan_pda_setbaud(serial, speed);

	if (speed == 0) {
		dbg("can't handle requested baud rate");
		
		speed = tty_termios_baud_rate(old_termios);
	}
	tty_termios_copy_hw(tty->termios, old_termios);
	tty_encode_baud_rate(tty, speed, speed);
}



static int keyspan_pda_get_modem_info(struct usb_serial *serial,
				      unsigned char *value)
{
	int rc;
	u8 *data;

	data = kmalloc(1, GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	rc = usb_control_msg(serial->dev, usb_rcvctrlpipe(serial->dev, 0),
			     3, 
			     USB_TYPE_VENDOR|USB_RECIP_INTERFACE|USB_DIR_IN,
			     0, 0, data, 1, 2000);
	if (rc >= 0)
		*value = *data;

	kfree(data);
	return rc;
}


static int keyspan_pda_set_modem_info(struct usb_serial *serial,
				      unsigned char value)
{
	int rc;
	rc = usb_control_msg(serial->dev, usb_sndctrlpipe(serial->dev, 0),
			     3, 
			     USB_TYPE_VENDOR|USB_RECIP_INTERFACE|USB_DIR_OUT,
			     value, 0, NULL, 0, 2000);
	return rc;
}

static int keyspan_pda_tiocmget(struct tty_struct *tty)
{
	struct usb_serial_port *port = tty->driver_data;
	struct usb_serial *serial = port->serial;
	int rc;
	unsigned char status;
	int value;

	rc = keyspan_pda_get_modem_info(serial, &status);
	if (rc < 0)
		return rc;
	value =
		((status & (1<<7)) ? TIOCM_DTR : 0) |
		((status & (1<<6)) ? TIOCM_CAR : 0) |
		((status & (1<<5)) ? TIOCM_RNG : 0) |
		((status & (1<<4)) ? TIOCM_DSR : 0) |
		((status & (1<<3)) ? TIOCM_CTS : 0) |
		((status & (1<<2)) ? TIOCM_RTS : 0);
	return value;
}

static int keyspan_pda_tiocmset(struct tty_struct *tty,
				unsigned int set, unsigned int clear)
{
	struct usb_serial_port *port = tty->driver_data;
	struct usb_serial *serial = port->serial;
	int rc;
	unsigned char status;

	rc = keyspan_pda_get_modem_info(serial, &status);
	if (rc < 0)
		return rc;

	if (set & TIOCM_RTS)
		status |= (1<<2);
	if (set & TIOCM_DTR)
		status |= (1<<7);

	if (clear & TIOCM_RTS)
		status &= ~(1<<2);
	if (clear & TIOCM_DTR)
		status &= ~(1<<7);
	rc = keyspan_pda_set_modem_info(serial, status);
	return rc;
}

static int keyspan_pda_write(struct tty_struct *tty,
	struct usb_serial_port *port, const unsigned char *buf, int count)
{
	struct usb_serial *serial = port->serial;
	int request_unthrottle = 0;
	int rc = 0;
	struct keyspan_pda_private *priv;

	priv = usb_get_serial_port_data(port);
	dbg("keyspan_pda_write(%d)", count);
	if (count == 0) {
		dbg(" write request of 0 bytes");
		return 0;
	}

	spin_lock_bh(&port->lock);
	if (!test_bit(0, &port->write_urbs_free) || priv->tx_throttled) {
		spin_unlock_bh(&port->lock);
		return 0;
	}
	clear_bit(0, &port->write_urbs_free);
	spin_unlock_bh(&port->lock);


	count = (count > port->bulk_out_size) ? port->bulk_out_size : count;

	if (count > priv->tx_room && !in_interrupt()) {
		u8 *room;

		room = kmalloc(1, GFP_KERNEL);
		if (!room) {
			rc = -ENOMEM;
			goto exit;
		}

		rc = usb_control_msg(serial->dev,
				     usb_rcvctrlpipe(serial->dev, 0),
				     6, 
				     USB_TYPE_VENDOR | USB_RECIP_INTERFACE
				     | USB_DIR_IN,
				     0, 
				     0, 
				     room,
				     1,
				     2000);
		if (rc > 0) {
			dbg(" roomquery says %d", *room);
			priv->tx_room = *room;
		}
		kfree(room);
		if (rc < 0) {
			dbg(" roomquery failed");
			goto exit;
		}
		if (rc == 0) {
			dbg(" roomquery returned 0 bytes");
			rc = -EIO; 
			goto exit;
		}
	}
	if (count > priv->tx_room) {
		count = priv->tx_room;
		request_unthrottle = 1;
	}

	if (count) {
		
		memcpy(port->write_urb->transfer_buffer, buf, count);
		
		port->write_urb->transfer_buffer_length = count;

		priv->tx_room -= count;

		rc = usb_submit_urb(port->write_urb, GFP_ATOMIC);
		if (rc) {
			dbg(" usb_submit_urb(write bulk) failed");
			goto exit;
		}
	} else {
		request_unthrottle = 1;
	}

	if (request_unthrottle) {
		priv->tx_throttled = 1; 
		schedule_work(&priv->unthrottle_work);
	}

	rc = count;
exit:
	if (rc < 0)
		set_bit(0, &port->write_urbs_free);
	return rc;
}


static void keyspan_pda_write_bulk_callback(struct urb *urb)
{
	struct usb_serial_port *port = urb->context;
	struct keyspan_pda_private *priv;

	set_bit(0, &port->write_urbs_free);
	priv = usb_get_serial_port_data(port);

	
	schedule_work(&priv->wakeup_work);
}


static int keyspan_pda_write_room(struct tty_struct *tty)
{
	struct usb_serial_port *port = tty->driver_data;
	struct keyspan_pda_private *priv;
	priv = usb_get_serial_port_data(port);
	return priv->tx_room;
}


static int keyspan_pda_chars_in_buffer(struct tty_struct *tty)
{
	struct usb_serial_port *port = tty->driver_data;
	struct keyspan_pda_private *priv;
	unsigned long flags;
	int ret = 0;

	priv = usb_get_serial_port_data(port);


	spin_lock_irqsave(&port->lock, flags);
	if (!test_bit(0, &port->write_urbs_free) || priv->tx_throttled)
		ret = 256;
	spin_unlock_irqrestore(&port->lock, flags);
	return ret;
}


static void keyspan_pda_dtr_rts(struct usb_serial_port *port, int on)
{
	struct usb_serial *serial = port->serial;

	if (serial->dev) {
		if (on)
			keyspan_pda_set_modem_info(serial, (1<<7) | (1<< 2));
		else
			keyspan_pda_set_modem_info(serial, 0);
	}
}


static int keyspan_pda_open(struct tty_struct *tty,
					struct usb_serial_port *port)
{
	struct usb_serial *serial = port->serial;
	u8 *room;
	int rc = 0;
	struct keyspan_pda_private *priv;

	
	room = kmalloc(1, GFP_KERNEL);
	if (!room)
		return -ENOMEM;

	rc = usb_control_msg(serial->dev, usb_rcvctrlpipe(serial->dev, 0),
			     6, 
			     USB_TYPE_VENDOR | USB_RECIP_INTERFACE
			     | USB_DIR_IN,
			     0, 
			     0, 
			     room,
			     1,
			     2000);
	if (rc < 0) {
		dbg("%s - roomquery failed", __func__);
		goto error;
	}
	if (rc == 0) {
		dbg("%s - roomquery returned 0 bytes", __func__);
		rc = -EIO;
		goto error;
	}
	priv = usb_get_serial_port_data(port);
	priv->tx_room = *room;
	priv->tx_throttled = *room ? 0 : 1;

	
	rc = usb_submit_urb(port->interrupt_in_urb, GFP_KERNEL);
	if (rc) {
		dbg("%s - usb_submit_urb(read int) failed", __func__);
		goto error;
	}
error:
	kfree(room);
	return rc;
}
static void keyspan_pda_close(struct usb_serial_port *port)
{
	struct usb_serial *serial = port->serial;

	if (serial->dev) {
		
		usb_kill_urb(port->write_urb);
		usb_kill_urb(port->interrupt_in_urb);
	}
}


static int keyspan_pda_fake_startup(struct usb_serial *serial)
{
	int response;
	const char *fw_name;
	const struct ihex_binrec *record;
	const struct firmware *fw;

	
	response = ezusb_set_reset(serial, 1);

	if (0) { ; }
#ifdef KEYSPAN
	else if (le16_to_cpu(serial->dev->descriptor.idVendor) == KEYSPAN_VENDOR_ID)
		fw_name = "keyspan_pda/keyspan_pda.fw";
#endif
#ifdef XIRCOM
	else if ((le16_to_cpu(serial->dev->descriptor.idVendor) == XIRCOM_VENDOR_ID) ||
		 (le16_to_cpu(serial->dev->descriptor.idVendor) == ENTREGRA_VENDOR_ID))
		fw_name = "keyspan_pda/xircom_pgs.fw";
#endif
	else {
		dev_err(&serial->dev->dev, "%s: unknown vendor, aborting.\n",
			__func__);
		return -ENODEV;
	}
	if (request_ihex_firmware(&fw, fw_name, &serial->dev->dev)) {
		dev_err(&serial->dev->dev, "failed to load firmware \"%s\"\n",
			fw_name);
		return -ENOENT;
	}
	record = (const struct ihex_binrec *)fw->data;

	while (record) {
		response = ezusb_writememory(serial, be32_to_cpu(record->addr),
					     (unsigned char *)record->data,
					     be16_to_cpu(record->len), 0xa0);
		if (response < 0) {
			dev_err(&serial->dev->dev, "ezusb_writememory failed "
				"for Keyspan PDA firmware (%d %04X %p %d)\n",
				response, be32_to_cpu(record->addr),
				record->data, be16_to_cpu(record->len));
			break;
		}
		record = ihex_next_binrec(record);
	}
	release_firmware(fw);
	response = ezusb_set_reset(serial, 0);

	
	return 1;
}

#ifdef KEYSPAN
MODULE_FIRMWARE("keyspan_pda/keyspan_pda.fw");
#endif
#ifdef XIRCOM
MODULE_FIRMWARE("keyspan_pda/xircom_pgs.fw");
#endif

static int keyspan_pda_startup(struct usb_serial *serial)
{

	struct keyspan_pda_private *priv;


	priv = kmalloc(sizeof(struct keyspan_pda_private), GFP_KERNEL);
	if (!priv)
		return 1; 
	usb_set_serial_port_data(serial->port[0], priv);
	init_waitqueue_head(&serial->port[0]->write_wait);
	INIT_WORK(&priv->wakeup_work, keyspan_pda_wakeup_write);
	INIT_WORK(&priv->unthrottle_work, keyspan_pda_request_unthrottle);
	priv->serial = serial;
	priv->port = serial->port[0];
	return 0;
}

static void keyspan_pda_release(struct usb_serial *serial)
{
	dbg("%s", __func__);

	kfree(usb_get_serial_port_data(serial->port[0]));
}

#ifdef KEYSPAN
static struct usb_serial_driver keyspan_pda_fake_device = {
	.driver = {
		.owner =	THIS_MODULE,
		.name =		"keyspan_pda_pre",
	},
	.description =		"Keyspan PDA - (prerenumeration)",
	.id_table =		id_table_fake,
	.num_ports =		1,
	.attach =		keyspan_pda_fake_startup,
};
#endif

#ifdef XIRCOM
static struct usb_serial_driver xircom_pgs_fake_device = {
	.driver = {
		.owner =	THIS_MODULE,
		.name =		"xircom_no_firm",
	},
	.description =		"Xircom / Entregra PGS - (prerenumeration)",
	.id_table =		id_table_fake_xircom,
	.num_ports =		1,
	.attach =		keyspan_pda_fake_startup,
};
#endif

static struct usb_serial_driver keyspan_pda_device = {
	.driver = {
		.owner =	THIS_MODULE,
		.name =		"keyspan_pda",
	},
	.description =		"Keyspan PDA",
	.id_table =		id_table_std,
	.num_ports =		1,
	.dtr_rts =		keyspan_pda_dtr_rts,
	.open =			keyspan_pda_open,
	.close =		keyspan_pda_close,
	.write =		keyspan_pda_write,
	.write_room =		keyspan_pda_write_room,
	.write_bulk_callback = 	keyspan_pda_write_bulk_callback,
	.read_int_callback =	keyspan_pda_rx_interrupt,
	.chars_in_buffer =	keyspan_pda_chars_in_buffer,
	.throttle =		keyspan_pda_rx_throttle,
	.unthrottle =		keyspan_pda_rx_unthrottle,
	.set_termios =		keyspan_pda_set_termios,
	.break_ctl =		keyspan_pda_break_ctl,
	.tiocmget =		keyspan_pda_tiocmget,
	.tiocmset =		keyspan_pda_tiocmset,
	.attach =		keyspan_pda_startup,
	.release =		keyspan_pda_release,
};

static struct usb_serial_driver * const serial_drivers[] = {
	&keyspan_pda_device,
#ifdef KEYSPAN
	&keyspan_pda_fake_device,
#endif
#ifdef XIRCOM
	&xircom_pgs_fake_device,
#endif
	NULL
};

module_usb_serial_driver(keyspan_pda_driver, serial_drivers);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

module_param(debug, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "Debug enabled or not");
