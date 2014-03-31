/*
 * Silicon Laboratories CP210x USB to RS232 serial adaptor driver
 *
 * Copyright (C) 2005 Craig Shelley (craig@microtron.org.uk)
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License version
 *	2 as published by the Free Software Foundation.
 *
 * Support to set flow control line levels using TIOCMGET and TIOCMSET
 * thanks to Karl Hiramoto karl@hiramoto.org. RTSCTS hardware flow
 * control thanks to Munir Nassar nassarmu@real-time.com
 *
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/usb.h>
#include <linux/uaccess.h>
#include <linux/usb/serial.h>

#define DRIVER_VERSION "v0.09"
#define DRIVER_DESC "Silicon Labs CP210x RS232 serial adaptor driver"

static int cp210x_open(struct tty_struct *tty, struct usb_serial_port *);
static void cp210x_close(struct usb_serial_port *);
static void cp210x_get_termios(struct tty_struct *,
	struct usb_serial_port *port);
static void cp210x_get_termios_port(struct usb_serial_port *port,
	unsigned int *cflagp, unsigned int *baudp);
static void cp210x_change_speed(struct tty_struct *, struct usb_serial_port *,
							struct ktermios *);
static void cp210x_set_termios(struct tty_struct *, struct usb_serial_port *,
							struct ktermios*);
static int cp210x_tiocmget(struct tty_struct *);
static int cp210x_tiocmset(struct tty_struct *, unsigned int, unsigned int);
static int cp210x_tiocmset_port(struct usb_serial_port *port,
		unsigned int, unsigned int);
static void cp210x_break_ctl(struct tty_struct *, int);
static int cp210x_startup(struct usb_serial *);
static void cp210x_release(struct usb_serial *);
static void cp210x_dtr_rts(struct usb_serial_port *p, int on);

static bool debug;

static const struct usb_device_id id_table[] = {
	{ USB_DEVICE(0x045B, 0x0053) }, 
	{ USB_DEVICE(0x0471, 0x066A) }, 
	{ USB_DEVICE(0x0489, 0xE000) }, 
	{ USB_DEVICE(0x0489, 0xE003) }, 
	{ USB_DEVICE(0x0745, 0x1000) }, 
	{ USB_DEVICE(0x08e6, 0x5501) }, 
	{ USB_DEVICE(0x08FD, 0x000A) }, 
	{ USB_DEVICE(0x0BED, 0x1100) }, 
	{ USB_DEVICE(0x0BED, 0x1101) }, 
	{ USB_DEVICE(0x0FCF, 0x1003) }, 
	{ USB_DEVICE(0x0FCF, 0x1004) }, 
	{ USB_DEVICE(0x0FCF, 0x1006) }, 
	{ USB_DEVICE(0x10A6, 0xAA26) }, 
	{ USB_DEVICE(0x10AB, 0x10C5) }, 
	{ USB_DEVICE(0x10B5, 0xAC70) }, 
	{ USB_DEVICE(0x10C4, 0x0F91) }, 
	{ USB_DEVICE(0x10C4, 0x1101) }, 
	{ USB_DEVICE(0x10C4, 0x1601) }, 
	{ USB_DEVICE(0x10C4, 0x800A) }, 
	{ USB_DEVICE(0x10C4, 0x803B) }, 
	{ USB_DEVICE(0x10C4, 0x8044) }, 
	{ USB_DEVICE(0x10C4, 0x804E) }, 
	{ USB_DEVICE(0x10C4, 0x8053) }, 
	{ USB_DEVICE(0x10C4, 0x8054) }, 
	{ USB_DEVICE(0x10C4, 0x8066) }, 
	{ USB_DEVICE(0x10C4, 0x806F) }, 
	{ USB_DEVICE(0x10C4, 0x807A) }, 
	{ USB_DEVICE(0x10C4, 0x80CA) }, 
	{ USB_DEVICE(0x10C4, 0x80DD) }, 
	{ USB_DEVICE(0x10C4, 0x80F6) }, 
	{ USB_DEVICE(0x10C4, 0x8115) }, 
	{ USB_DEVICE(0x10C4, 0x813D) }, 
	{ USB_DEVICE(0x10C4, 0x813F) }, 
	{ USB_DEVICE(0x10C4, 0x814A) }, 
	{ USB_DEVICE(0x10C4, 0x814B) }, 
	{ USB_DEVICE(0x10C4, 0x8156) }, 
	{ USB_DEVICE(0x10C4, 0x815E) }, 
	{ USB_DEVICE(0x10C4, 0x818B) }, 
	{ USB_DEVICE(0x10C4, 0x819F) }, 
	{ USB_DEVICE(0x10C4, 0x81A6) }, 
	{ USB_DEVICE(0x10C4, 0x81A9) }, 
	{ USB_DEVICE(0x10C4, 0x81AC) }, 
	{ USB_DEVICE(0x10C4, 0x81AD) }, 
	{ USB_DEVICE(0x10C4, 0x81C8) }, 
	{ USB_DEVICE(0x10C4, 0x81E2) }, 
	{ USB_DEVICE(0x10C4, 0x81E7) }, 
	{ USB_DEVICE(0x10C4, 0x81E8) }, 
	{ USB_DEVICE(0x10C4, 0x81F2) }, 
	{ USB_DEVICE(0x10C4, 0x8218) }, 
	{ USB_DEVICE(0x10C4, 0x822B) }, 
	{ USB_DEVICE(0x10C4, 0x826B) }, 
	{ USB_DEVICE(0x10C4, 0x8293) }, 
	{ USB_DEVICE(0x10C4, 0x82F9) }, 
	{ USB_DEVICE(0x10C4, 0x8341) }, 
	{ USB_DEVICE(0x10C4, 0x8382) }, 
	{ USB_DEVICE(0x10C4, 0x83A8) }, 
	{ USB_DEVICE(0x10C4, 0x83D8) }, 
	{ USB_DEVICE(0x10C4, 0x8411) }, 
	{ USB_DEVICE(0x10C4, 0x8418) }, 
	{ USB_DEVICE(0x10C4, 0x846E) }, 
	{ USB_DEVICE(0x10C4, 0x8477) }, 
	{ USB_DEVICE(0x10C4, 0x85EA) }, 
	{ USB_DEVICE(0x10C4, 0x85EB) }, 
	{ USB_DEVICE(0x10C4, 0x8664) }, 
	{ USB_DEVICE(0x10C4, 0x8665) }, 
	{ USB_DEVICE(0x10C4, 0xEA60) }, 
	{ USB_DEVICE(0x10C4, 0xEA61) }, 
	{ USB_DEVICE(0x10C4, 0xEA70) }, 
	{ USB_DEVICE(0x10C4, 0xEA80) }, 
	{ USB_DEVICE(0x10C4, 0xEA71) }, 
	{ USB_DEVICE(0x10C4, 0xF001) }, 
	{ USB_DEVICE(0x10C4, 0xF002) }, 
	{ USB_DEVICE(0x10C4, 0xF003) }, 
	{ USB_DEVICE(0x10C4, 0xF004) }, 
	{ USB_DEVICE(0x10C5, 0xEA61) }, 
	{ USB_DEVICE(0x10CE, 0xEA6A) }, 
	{ USB_DEVICE(0x13AD, 0x9999) }, 
	{ USB_DEVICE(0x1555, 0x0004) }, 
	{ USB_DEVICE(0x166A, 0x0303) }, 
	{ USB_DEVICE(0x16D6, 0x0001) }, 
	{ USB_DEVICE(0x16DC, 0x0010) }, 
	{ USB_DEVICE(0x16DC, 0x0011) }, 
	{ USB_DEVICE(0x16DC, 0x0012) }, 
	{ USB_DEVICE(0x16DC, 0x0015) }, 
	{ USB_DEVICE(0x17A8, 0x0001) }, 
	{ USB_DEVICE(0x17A8, 0x0005) }, 
	{ USB_DEVICE(0x17F4, 0xAAAA) }, 
	{ USB_DEVICE(0x1843, 0x0200) }, 
	{ USB_DEVICE(0x18EF, 0xE00F) }, 
	{ USB_DEVICE(0x1BE3, 0x07A6) }, 
	{ USB_DEVICE(0x3195, 0xF190) }, 
	{ USB_DEVICE(0x413C, 0x9500) }, 
	{ } 
};

MODULE_DEVICE_TABLE(usb, id_table);

struct cp210x_port_private {
	__u8			bInterfaceNumber;
};

static struct usb_driver cp210x_driver = {
	.name		= "cp210x",
	.probe		= usb_serial_probe,
	.disconnect	= usb_serial_disconnect,
	.id_table	= id_table,
};

static struct usb_serial_driver cp210x_device = {
	.driver = {
		.owner =	THIS_MODULE,
		.name = 	"cp210x",
	},
	.id_table		= id_table,
	.num_ports		= 1,
	.bulk_in_size		= 256,
	.bulk_out_size		= 256,
	.open			= cp210x_open,
	.close			= cp210x_close,
	.break_ctl		= cp210x_break_ctl,
	.set_termios		= cp210x_set_termios,
	.tiocmget 		= cp210x_tiocmget,
	.tiocmset		= cp210x_tiocmset,
	.attach			= cp210x_startup,
	.release		= cp210x_release,
	.dtr_rts		= cp210x_dtr_rts
};

static struct usb_serial_driver * const serial_drivers[] = {
	&cp210x_device, NULL
};

#define REQTYPE_HOST_TO_DEVICE	0x41
#define REQTYPE_DEVICE_TO_HOST	0xc1

#define CP210X_IFC_ENABLE	0x00
#define CP210X_SET_BAUDDIV	0x01
#define CP210X_GET_BAUDDIV	0x02
#define CP210X_SET_LINE_CTL	0x03
#define CP210X_GET_LINE_CTL	0x04
#define CP210X_SET_BREAK	0x05
#define CP210X_IMM_CHAR		0x06
#define CP210X_SET_MHS		0x07
#define CP210X_GET_MDMSTS	0x08
#define CP210X_SET_XON		0x09
#define CP210X_SET_XOFF		0x0A
#define CP210X_SET_EVENTMASK	0x0B
#define CP210X_GET_EVENTMASK	0x0C
#define CP210X_SET_CHAR		0x0D
#define CP210X_GET_CHARS	0x0E
#define CP210X_GET_PROPS	0x0F
#define CP210X_GET_COMM_STATUS	0x10
#define CP210X_RESET		0x11
#define CP210X_PURGE		0x12
#define CP210X_SET_FLOW		0x13
#define CP210X_GET_FLOW		0x14
#define CP210X_EMBED_EVENTS	0x15
#define CP210X_GET_EVENTSTATE	0x16
#define CP210X_SET_CHARS	0x19
#define CP210X_GET_BAUDRATE	0x1D
#define CP210X_SET_BAUDRATE	0x1E

#define UART_ENABLE		0x0001
#define UART_DISABLE		0x0000

#define BAUD_RATE_GEN_FREQ	0x384000

#define BITS_DATA_MASK		0X0f00
#define BITS_DATA_5		0X0500
#define BITS_DATA_6		0X0600
#define BITS_DATA_7		0X0700
#define BITS_DATA_8		0X0800
#define BITS_DATA_9		0X0900

#define BITS_PARITY_MASK	0x00f0
#define BITS_PARITY_NONE	0x0000
#define BITS_PARITY_ODD		0x0010
#define BITS_PARITY_EVEN	0x0020
#define BITS_PARITY_MARK	0x0030
#define BITS_PARITY_SPACE	0x0040

#define BITS_STOP_MASK		0x000f
#define BITS_STOP_1		0x0000
#define BITS_STOP_1_5		0x0001
#define BITS_STOP_2		0x0002

#define BREAK_ON		0x0001
#define BREAK_OFF		0x0000

#define CONTROL_DTR		0x0001
#define CONTROL_RTS		0x0002
#define CONTROL_CTS		0x0010
#define CONTROL_DSR		0x0020
#define CONTROL_RING		0x0040
#define CONTROL_DCD		0x0080
#define CONTROL_WRITE_DTR	0x0100
#define CONTROL_WRITE_RTS	0x0200

static int cp210x_get_config(struct usb_serial_port *port, u8 request,
		unsigned int *data, int size)
{
	struct usb_serial *serial = port->serial;
	struct cp210x_port_private *port_priv = usb_get_serial_port_data(port);
	__le32 *buf;
	int result, i, length;

	
	length = (((size - 1) | 3) + 1)/4;

	buf = kcalloc(length, sizeof(__le32), GFP_KERNEL);
	if (!buf) {
		dev_err(&port->dev, "%s - out of memory.\n", __func__);
		return -ENOMEM;
	}

	
	result = usb_control_msg(serial->dev, usb_rcvctrlpipe(serial->dev, 0),
				request, REQTYPE_DEVICE_TO_HOST, 0x0000,
				port_priv->bInterfaceNumber, buf, size,
				USB_CTRL_GET_TIMEOUT);

	
	for (i = 0; i < length; i++)
		data[i] = le32_to_cpu(buf[i]);

	kfree(buf);

	if (result != size) {
		dbg("%s - Unable to send config request, "
				"request=0x%x size=%d result=%d",
				__func__, request, size, result);
		if (result > 0)
			result = -EPROTO;

		return result;
	}

	return 0;
}

static int cp210x_set_config(struct usb_serial_port *port, u8 request,
		unsigned int *data, int size)
{
	struct usb_serial *serial = port->serial;
	struct cp210x_port_private *port_priv = usb_get_serial_port_data(port);
	__le32 *buf;
	int result, i, length;

	
	length = (((size - 1) | 3) + 1)/4;

	buf = kmalloc(length * sizeof(__le32), GFP_KERNEL);
	if (!buf) {
		dev_err(&port->dev, "%s - out of memory.\n",
				__func__);
		return -ENOMEM;
	}

	
	for (i = 0; i < length; i++)
		buf[i] = cpu_to_le32(data[i]);

	if (size > 2) {
		result = usb_control_msg(serial->dev,
				usb_sndctrlpipe(serial->dev, 0),
				request, REQTYPE_HOST_TO_DEVICE, 0x0000,
				port_priv->bInterfaceNumber, buf, size,
				USB_CTRL_SET_TIMEOUT);
	} else {
		result = usb_control_msg(serial->dev,
				usb_sndctrlpipe(serial->dev, 0),
				request, REQTYPE_HOST_TO_DEVICE, data[0],
				port_priv->bInterfaceNumber, NULL, 0,
				USB_CTRL_SET_TIMEOUT);
	}

	kfree(buf);

	if ((size > 2 && result != size) || result < 0) {
		dbg("%s - Unable to send request, "
				"request=0x%x size=%d result=%d",
				__func__, request, size, result);
		if (result > 0)
			result = -EPROTO;

		return result;
	}

	return 0;
}

static inline int cp210x_set_config_single(struct usb_serial_port *port,
		u8 request, unsigned int data)
{
	return cp210x_set_config(port, request, &data, 2);
}

static unsigned int cp210x_quantise_baudrate(unsigned int baud) {
	if (baud <= 300)
		baud = 300;
	else if (baud <= 600)      baud = 600;
	else if (baud <= 1200)     baud = 1200;
	else if (baud <= 1800)     baud = 1800;
	else if (baud <= 2400)     baud = 2400;
	else if (baud <= 4000)     baud = 4000;
	else if (baud <= 4803)     baud = 4800;
	else if (baud <= 7207)     baud = 7200;
	else if (baud <= 9612)     baud = 9600;
	else if (baud <= 14428)    baud = 14400;
	else if (baud <= 16062)    baud = 16000;
	else if (baud <= 19250)    baud = 19200;
	else if (baud <= 28912)    baud = 28800;
	else if (baud <= 38601)    baud = 38400;
	else if (baud <= 51558)    baud = 51200;
	else if (baud <= 56280)    baud = 56000;
	else if (baud <= 58053)    baud = 57600;
	else if (baud <= 64111)    baud = 64000;
	else if (baud <= 77608)    baud = 76800;
	else if (baud <= 117028)   baud = 115200;
	else if (baud <= 129347)   baud = 128000;
	else if (baud <= 156868)   baud = 153600;
	else if (baud <= 237832)   baud = 230400;
	else if (baud <= 254234)   baud = 250000;
	else if (baud <= 273066)   baud = 256000;
	else if (baud <= 491520)   baud = 460800;
	else if (baud <= 567138)   baud = 500000;
	else if (baud <= 670254)   baud = 576000;
	else if (baud < 1000000)
		baud = 921600;
	else if (baud > 2000000)
		baud = 2000000;
	return baud;
}

static int cp210x_open(struct tty_struct *tty, struct usb_serial_port *port)
{
	int result;

	dbg("%s - port %d", __func__, port->number);

	result = cp210x_set_config_single(port, CP210X_IFC_ENABLE,
								UART_ENABLE);
	if (result) {
		dev_err(&port->dev, "%s - Unable to enable UART\n", __func__);
		return result;
	}

	
	cp210x_get_termios(tty, port);

	
	if (tty)
		cp210x_change_speed(tty, port, NULL);

	return usb_serial_generic_open(tty, port);
}

static void cp210x_close(struct usb_serial_port *port)
{
	dbg("%s - port %d", __func__, port->number);

	usb_serial_generic_close(port);

	mutex_lock(&port->serial->disc_mutex);
	if (!port->serial->disconnected)
		cp210x_set_config_single(port, CP210X_IFC_ENABLE, UART_DISABLE);
	mutex_unlock(&port->serial->disc_mutex);
}

static void cp210x_get_termios(struct tty_struct *tty,
	struct usb_serial_port *port)
{
	unsigned int baud;

	if (tty) {
		cp210x_get_termios_port(tty->driver_data,
			&tty->termios->c_cflag, &baud);
		tty_encode_baud_rate(tty, baud, baud);
	}

	else {
		unsigned int cflag;
		cflag = 0;
		cp210x_get_termios_port(port, &cflag, &baud);
	}
}

static void cp210x_get_termios_port(struct usb_serial_port *port,
	unsigned int *cflagp, unsigned int *baudp)
{
	unsigned int cflag, modem_ctl[4];
	unsigned int baud;
	unsigned int bits;

	dbg("%s - port %d", __func__, port->number);

	cp210x_get_config(port, CP210X_GET_BAUDRATE, &baud, 4);

	dbg("%s - baud rate = %d", __func__, baud);
	*baudp = baud;

	cflag = *cflagp;

	cp210x_get_config(port, CP210X_GET_LINE_CTL, &bits, 2);
	cflag &= ~CSIZE;
	switch (bits & BITS_DATA_MASK) {
	case BITS_DATA_5:
		dbg("%s - data bits = 5", __func__);
		cflag |= CS5;
		break;
	case BITS_DATA_6:
		dbg("%s - data bits = 6", __func__);
		cflag |= CS6;
		break;
	case BITS_DATA_7:
		dbg("%s - data bits = 7", __func__);
		cflag |= CS7;
		break;
	case BITS_DATA_8:
		dbg("%s - data bits = 8", __func__);
		cflag |= CS8;
		break;
	case BITS_DATA_9:
		dbg("%s - data bits = 9 (not supported, using 8 data bits)",
								__func__);
		cflag |= CS8;
		bits &= ~BITS_DATA_MASK;
		bits |= BITS_DATA_8;
		cp210x_set_config(port, CP210X_SET_LINE_CTL, &bits, 2);
		break;
	default:
		dbg("%s - Unknown number of data bits, using 8", __func__);
		cflag |= CS8;
		bits &= ~BITS_DATA_MASK;
		bits |= BITS_DATA_8;
		cp210x_set_config(port, CP210X_SET_LINE_CTL, &bits, 2);
		break;
	}

	switch (bits & BITS_PARITY_MASK) {
	case BITS_PARITY_NONE:
		dbg("%s - parity = NONE", __func__);
		cflag &= ~PARENB;
		break;
	case BITS_PARITY_ODD:
		dbg("%s - parity = ODD", __func__);
		cflag |= (PARENB|PARODD);
		break;
	case BITS_PARITY_EVEN:
		dbg("%s - parity = EVEN", __func__);
		cflag &= ~PARODD;
		cflag |= PARENB;
		break;
	case BITS_PARITY_MARK:
		dbg("%s - parity = MARK", __func__);
		cflag |= (PARENB|PARODD|CMSPAR);
		break;
	case BITS_PARITY_SPACE:
		dbg("%s - parity = SPACE", __func__);
		cflag &= ~PARODD;
		cflag |= (PARENB|CMSPAR);
		break;
	default:
		dbg("%s - Unknown parity mode, disabling parity", __func__);
		cflag &= ~PARENB;
		bits &= ~BITS_PARITY_MASK;
		cp210x_set_config(port, CP210X_SET_LINE_CTL, &bits, 2);
		break;
	}

	cflag &= ~CSTOPB;
	switch (bits & BITS_STOP_MASK) {
	case BITS_STOP_1:
		dbg("%s - stop bits = 1", __func__);
		break;
	case BITS_STOP_1_5:
		dbg("%s - stop bits = 1.5 (not supported, using 1 stop bit)",
								__func__);
		bits &= ~BITS_STOP_MASK;
		cp210x_set_config(port, CP210X_SET_LINE_CTL, &bits, 2);
		break;
	case BITS_STOP_2:
		dbg("%s - stop bits = 2", __func__);
		cflag |= CSTOPB;
		break;
	default:
		dbg("%s - Unknown number of stop bits, using 1 stop bit",
								__func__);
		bits &= ~BITS_STOP_MASK;
		cp210x_set_config(port, CP210X_SET_LINE_CTL, &bits, 2);
		break;
	}

	cp210x_get_config(port, CP210X_GET_FLOW, modem_ctl, 16);
	if (modem_ctl[0] & 0x0008) {
		dbg("%s - flow control = CRTSCTS", __func__);
		cflag |= CRTSCTS;
	} else {
		dbg("%s - flow control = NONE", __func__);
		cflag &= ~CRTSCTS;
	}

	*cflagp = cflag;
}

static void cp210x_change_speed(struct tty_struct *tty,
		struct usb_serial_port *port, struct ktermios *old_termios)
{
	u32 baud;

	baud = tty->termios->c_ospeed;

	baud = cp210x_quantise_baudrate(baud);

	dbg("%s - setting baud rate to %u", __func__, baud);
	if (cp210x_set_config(port, CP210X_SET_BAUDRATE, &baud,
							sizeof(baud))) {
		dev_warn(&port->dev, "failed to set baud rate to %u\n", baud);
		if (old_termios)
			baud = old_termios->c_ospeed;
		else
			baud = 9600;
	}

	tty_encode_baud_rate(tty, baud, baud);
}

static void cp210x_set_termios(struct tty_struct *tty,
		struct usb_serial_port *port, struct ktermios *old_termios)
{
	unsigned int cflag, old_cflag;
	unsigned int bits;
	unsigned int modem_ctl[4];

	dbg("%s - port %d", __func__, port->number);

	if (!tty)
		return;

	cflag = tty->termios->c_cflag;
	old_cflag = old_termios->c_cflag;

	if (tty->termios->c_ospeed != old_termios->c_ospeed)
		cp210x_change_speed(tty, port, old_termios);

	
	if ((cflag & CSIZE) != (old_cflag & CSIZE)) {
		cp210x_get_config(port, CP210X_GET_LINE_CTL, &bits, 2);
		bits &= ~BITS_DATA_MASK;
		switch (cflag & CSIZE) {
		case CS5:
			bits |= BITS_DATA_5;
			dbg("%s - data bits = 5", __func__);
			break;
		case CS6:
			bits |= BITS_DATA_6;
			dbg("%s - data bits = 6", __func__);
			break;
		case CS7:
			bits |= BITS_DATA_7;
			dbg("%s - data bits = 7", __func__);
			break;
		case CS8:
			bits |= BITS_DATA_8;
			dbg("%s - data bits = 8", __func__);
			break;
		default:
			dbg("cp210x driver does not "
					"support the number of bits requested,"
					" using 8 bit mode");
				bits |= BITS_DATA_8;
				break;
		}
		if (cp210x_set_config(port, CP210X_SET_LINE_CTL, &bits, 2))
			dbg("Number of data bits requested "
					"not supported by device");
	}

	if ((cflag     & (PARENB|PARODD|CMSPAR)) !=
	    (old_cflag & (PARENB|PARODD|CMSPAR))) {
		cp210x_get_config(port, CP210X_GET_LINE_CTL, &bits, 2);
		bits &= ~BITS_PARITY_MASK;
		if (cflag & PARENB) {
			if (cflag & CMSPAR) {
			    if (cflag & PARODD) {
				    bits |= BITS_PARITY_MARK;
				    dbg("%s - parity = MARK", __func__);
			    } else {
				    bits |= BITS_PARITY_SPACE;
				    dbg("%s - parity = SPACE", __func__);
			    }
			} else {
			    if (cflag & PARODD) {
				    bits |= BITS_PARITY_ODD;
				    dbg("%s - parity = ODD", __func__);
			    } else {
				    bits |= BITS_PARITY_EVEN;
				    dbg("%s - parity = EVEN", __func__);
			    }
			}
		}
		if (cp210x_set_config(port, CP210X_SET_LINE_CTL, &bits, 2))
			dbg("Parity mode not supported by device");
	}

	if ((cflag & CSTOPB) != (old_cflag & CSTOPB)) {
		cp210x_get_config(port, CP210X_GET_LINE_CTL, &bits, 2);
		bits &= ~BITS_STOP_MASK;
		if (cflag & CSTOPB) {
			bits |= BITS_STOP_2;
			dbg("%s - stop bits = 2", __func__);
		} else {
			bits |= BITS_STOP_1;
			dbg("%s - stop bits = 1", __func__);
		}
		if (cp210x_set_config(port, CP210X_SET_LINE_CTL, &bits, 2))
			dbg("Number of stop bits requested "
					"not supported by device");
	}

	if ((cflag & CRTSCTS) != (old_cflag & CRTSCTS)) {
		cp210x_get_config(port, CP210X_GET_FLOW, modem_ctl, 16);
		dbg("%s - read modem controls = 0x%.4x 0x%.4x 0x%.4x 0x%.4x",
				__func__, modem_ctl[0], modem_ctl[1],
				modem_ctl[2], modem_ctl[3]);

		if (cflag & CRTSCTS) {
			modem_ctl[0] &= ~0x7B;
			modem_ctl[0] |= 0x09;
			modem_ctl[1] = 0x80;
			dbg("%s - flow control = CRTSCTS", __func__);
		} else {
			modem_ctl[0] &= ~0x7B;
			modem_ctl[0] |= 0x01;
			modem_ctl[1] |= 0x40;
			dbg("%s - flow control = NONE", __func__);
		}

		dbg("%s - write modem controls = 0x%.4x 0x%.4x 0x%.4x 0x%.4x",
				__func__, modem_ctl[0], modem_ctl[1],
				modem_ctl[2], modem_ctl[3]);
		cp210x_set_config(port, CP210X_SET_FLOW, modem_ctl, 16);
	}

}

static int cp210x_tiocmset (struct tty_struct *tty,
		unsigned int set, unsigned int clear)
{
	struct usb_serial_port *port = tty->driver_data;
	return cp210x_tiocmset_port(port, set, clear);
}

static int cp210x_tiocmset_port(struct usb_serial_port *port,
		unsigned int set, unsigned int clear)
{
	unsigned int control = 0;

	dbg("%s - port %d", __func__, port->number);

	if (set & TIOCM_RTS) {
		control |= CONTROL_RTS;
		control |= CONTROL_WRITE_RTS;
	}
	if (set & TIOCM_DTR) {
		control |= CONTROL_DTR;
		control |= CONTROL_WRITE_DTR;
	}
	if (clear & TIOCM_RTS) {
		control &= ~CONTROL_RTS;
		control |= CONTROL_WRITE_RTS;
	}
	if (clear & TIOCM_DTR) {
		control &= ~CONTROL_DTR;
		control |= CONTROL_WRITE_DTR;
	}

	dbg("%s - control = 0x%.4x", __func__, control);

	return cp210x_set_config(port, CP210X_SET_MHS, &control, 2);
}

static void cp210x_dtr_rts(struct usb_serial_port *p, int on)
{
	if (on)
		cp210x_tiocmset_port(p, TIOCM_DTR|TIOCM_RTS, 0);
	else
		cp210x_tiocmset_port(p, 0, TIOCM_DTR|TIOCM_RTS);
}

static int cp210x_tiocmget (struct tty_struct *tty)
{
	struct usb_serial_port *port = tty->driver_data;
	unsigned int control;
	int result;

	dbg("%s - port %d", __func__, port->number);

	cp210x_get_config(port, CP210X_GET_MDMSTS, &control, 1);

	result = ((control & CONTROL_DTR) ? TIOCM_DTR : 0)
		|((control & CONTROL_RTS) ? TIOCM_RTS : 0)
		|((control & CONTROL_CTS) ? TIOCM_CTS : 0)
		|((control & CONTROL_DSR) ? TIOCM_DSR : 0)
		|((control & CONTROL_RING)? TIOCM_RI  : 0)
		|((control & CONTROL_DCD) ? TIOCM_CD  : 0);

	dbg("%s - control = 0x%.2x", __func__, control);

	return result;
}

static void cp210x_break_ctl (struct tty_struct *tty, int break_state)
{
	struct usb_serial_port *port = tty->driver_data;
	unsigned int state;

	dbg("%s - port %d", __func__, port->number);
	if (break_state == 0)
		state = BREAK_OFF;
	else
		state = BREAK_ON;
	dbg("%s - turning break %s", __func__,
			state == BREAK_OFF ? "off" : "on");
	cp210x_set_config(port, CP210X_SET_BREAK, &state, 2);
}

static int cp210x_startup(struct usb_serial *serial)
{
	struct cp210x_port_private *port_priv;
	int i;

	
	usb_reset_device(serial->dev);

	for (i = 0; i < serial->num_ports; i++) {
		port_priv = kzalloc(sizeof(*port_priv), GFP_KERNEL);
		if (!port_priv)
			return -ENOMEM;

		memset(port_priv, 0x00, sizeof(*port_priv));
		port_priv->bInterfaceNumber =
		    serial->interface->cur_altsetting->desc.bInterfaceNumber;

		usb_set_serial_port_data(serial->port[i], port_priv);
	}

	return 0;
}

static void cp210x_release(struct usb_serial *serial)
{
	struct cp210x_port_private *port_priv;
	int i;

	for (i = 0; i < serial->num_ports; i++) {
		port_priv = usb_get_serial_port_data(serial->port[i]);
		kfree(port_priv);
		usb_set_serial_port_data(serial->port[i], NULL);
	}
}

module_usb_serial_driver(cp210x_driver, serial_drivers);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPL");

module_param(debug, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "Enable verbose debugging messages");
