/*
 * USB Serial Console driver
 *
 * Copyright (C) 2001 - 2002 Greg Kroah-Hartman (greg@kroah.com)
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License version
 *	2 as published by the Free Software Foundation.
 *
 * Thanks to Randy Dunlap for the original version of this code.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/console.h>
#include <linux/serial.h>
#include <linux/usb.h>
#include <linux/usb/serial.h>

static int debug;

struct usbcons_info {
	int			magic;
	int			break_flag;
	struct usb_serial_port	*port;
};

static struct usbcons_info usbcons_info;
static struct console usbcons;



static int usb_console_setup(struct console *co, char *options)
{
	struct usbcons_info *info = &usbcons_info;
	int baud = 9600;
	int bits = 8;
	int parity = 'n';
	int doflow = 0;
	int cflag = CREAD | HUPCL | CLOCAL;
	char *s;
	struct usb_serial *serial;
	struct usb_serial_port *port;
	int retval;
	struct tty_struct *tty = NULL;
	struct ktermios dummy;

	dbg("%s", __func__);

	if (options) {
		baud = simple_strtoul(options, NULL, 10);
		s = options;
		while (*s >= '0' && *s <= '9')
			s++;
		if (*s)
			parity = *s++;
		if (*s)
			bits   = *s++ - '0';
		if (*s)
			doflow = (*s++ == 'r');
	}
	
	
	if (baud == 0)
		baud = 9600;

	switch (bits) {
	case 7:
		cflag |= CS7;
		break;
	default:
	case 8:
		cflag |= CS8;
		break;
	}
	switch (parity) {
	case 'o': case 'O':
		cflag |= PARODD;
		break;
	case 'e': case 'E':
		cflag |= PARENB;
		break;
	}
	co->cflag = cflag;

	serial = usb_serial_get_by_index(co->index);
	if (serial == NULL) {
		
		err("No USB device connected to ttyUSB%i", co->index);
		return -ENODEV;
	}

	retval = usb_autopm_get_interface(serial->interface);
	if (retval)
		goto error_get_interface;

	port = serial->port[co->index - serial->minor];
	tty_port_tty_set(&port->port, NULL);

	info->port = port;

	++port->port.count;
	if (!test_bit(ASYNCB_INITIALIZED, &port->port.flags)) {
		if (serial->type->set_termios) {
			tty = kzalloc(sizeof(*tty), GFP_KERNEL);
			if (!tty) {
				retval = -ENOMEM;
				err("no more memory");
				goto reset_open_count;
			}
			kref_init(&tty->kref);
			tty_port_tty_set(&port->port, tty);
			tty->driver = usb_serial_tty_driver;
			tty->index = co->index;
			if (tty_init_termios(tty)) {
				retval = -ENOMEM;
				err("no more memory");
				goto free_tty;
			}
		}

		if (serial->type->open)
			retval = serial->type->open(NULL, port);
		else
			retval = usb_serial_generic_open(NULL, port);

		if (retval) {
			err("could not open USB console port");
			goto fail;
		}

		if (serial->type->set_termios) {
			tty->termios->c_cflag = cflag;
			tty_termios_encode_baud_rate(tty->termios, baud, baud);
			memset(&dummy, 0, sizeof(struct ktermios));
			serial->type->set_termios(tty, port, &dummy);

			tty_port_tty_set(&port->port, NULL);
			kfree(tty);
		}
		set_bit(ASYNCB_INITIALIZED, &port->port.flags);
	}
	--port->port.count;
	port->port.console = 1;

	mutex_unlock(&serial->disc_mutex);
	return retval;

 fail:
	tty_port_tty_set(&port->port, NULL);
 free_tty:
	kfree(tty);
 reset_open_count:
	port->port.count = 0;
	usb_autopm_put_interface(serial->interface);
 error_get_interface:
	usb_serial_put(serial);
	mutex_unlock(&serial->disc_mutex);
	return retval;
}

static void usb_console_write(struct console *co,
					const char *buf, unsigned count)
{
	static struct usbcons_info *info = &usbcons_info;
	struct usb_serial_port *port = info->port;
	struct usb_serial *serial;
	int retval = -ENODEV;

	if (!port || port->serial->dev->state == USB_STATE_NOTATTACHED)
		return;
	serial = port->serial;

	if (count == 0)
		return;

	dbg("%s - port %d, %d byte(s)", __func__, port->number, count);

	if (!port->port.console) {
		dbg("%s - port not opened", __func__);
		return;
	}

	while (count) {
		unsigned int i;
		unsigned int lf;
		
		for (i = 0, lf = 0 ; i < count ; i++) {
			if (*(buf + i) == 10) {
				lf = 1;
				i++;
				break;
			}
		}
		if (serial->type->write)
			retval = serial->type->write(NULL, port, buf, i);
		else
			retval = usb_serial_generic_write(NULL, port, buf, i);
		dbg("%s - return value : %d", __func__, retval);
		if (lf) {
			
			unsigned char cr = 13;
			if (serial->type->write)
				retval = serial->type->write(NULL,
								port, &cr, 1);
			else
				retval = usb_serial_generic_write(NULL,
								port, &cr, 1);
			dbg("%s - return value : %d", __func__, retval);
		}
		buf += i;
		count -= i;
	}
}

static struct tty_driver *usb_console_device(struct console *co, int *index)
{
	struct tty_driver **p = (struct tty_driver **)co->data;

	if (!*p)
		return NULL;

	*index = co->index;
	return *p;
}

static struct console usbcons = {
	.name =		"ttyUSB",
	.write =	usb_console_write,
	.device =	usb_console_device,
	.setup =	usb_console_setup,
	.flags =	CON_PRINTBUFFER,
	.index =	-1,
	.data = 	&usb_serial_tty_driver,
};

void usb_serial_console_disconnect(struct usb_serial *serial)
{
	if (serial && serial->port && serial->port[0]
				&& serial->port[0] == usbcons_info.port) {
		usb_serial_console_exit();
		usb_serial_put(serial);
	}
}

void usb_serial_console_init(int serial_debug, int minor)
{
	debug = serial_debug;

	if (minor == 0) {
		dbg("registering the USB serial console.");
		register_console(&usbcons);
	}
}

void usb_serial_console_exit(void)
{
	if (usbcons_info.port) {
		unregister_console(&usbcons);
		usbcons_info.port->port.console = 0;
		usbcons_info.port = NULL;
	}
}

