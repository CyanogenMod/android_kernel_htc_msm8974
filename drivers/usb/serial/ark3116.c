/*
 * Copyright (C) 2009 by Bart Hartgers (bart.hartgers+ark3116@gmail.com)
 * Original version:
 * Copyright (C) 2006
 *   Simon Schulz (ark3116_driver <at> auctionant.de)
 *
 * ark3116
 * - implements a driver for the arkmicro ark3116 chipset (vendor=0x6547,
 *   productid=0x0232) (used in a datacable called KQ-U8A)
 *
 * Supports full modem status lines, break, hardware flow control. Does not
 * support software flow control, since I do not know how to enable it in hw.
 *
 * This driver is a essentially new implementation. I initially dug
 * into the old ark3116.c driver and suddenly realized the ark3116 is
 * a 16450 with a USB interface glued to it. See comments at the
 * bottom of this file.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/tty_flip.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/usb/serial.h>
#include <linux/serial.h>
#include <linux/serial_reg.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>

static bool debug;

#define DRIVER_VERSION "v0.7"
#define DRIVER_AUTHOR "Bart Hartgers <bart.hartgers+ark3116@gmail.com>"
#define DRIVER_DESC "USB ARK3116 serial/IrDA driver"
#define DRIVER_DEV_DESC "ARK3116 RS232/IrDA"
#define DRIVER_NAME "ark3116"

#define ARK_TIMEOUT (1*HZ)

static const struct usb_device_id id_table[] = {
	{ USB_DEVICE(0x6547, 0x0232) },
	{ USB_DEVICE(0x18ec, 0x3118) },		
	{ },
};
MODULE_DEVICE_TABLE(usb, id_table);

static int is_irda(struct usb_serial *serial)
{
	struct usb_device *dev = serial->dev;
	if (le16_to_cpu(dev->descriptor.idVendor) == 0x18ec &&
			le16_to_cpu(dev->descriptor.idProduct) == 0x3118)
		return 1;
	return 0;
}

struct ark3116_private {
	wait_queue_head_t       delta_msr_wait;
	struct async_icount	icount;
	int			irda;	

	
	struct mutex		hw_lock;

	int			quot;	
	__u32			lcr;	
	__u32			hcr;	
	__u32			mcr;	

	
	spinlock_t		status_lock;
	__u32			msr;	
	__u32			lsr;	
};

static int ark3116_write_reg(struct usb_serial *serial,
			     unsigned reg, __u8 val)
{
	int result;
	 
	result = usb_control_msg(serial->dev,
				 usb_sndctrlpipe(serial->dev, 0),
				 0xfe, 0x40, val, reg,
				 NULL, 0, ARK_TIMEOUT);
	return result;
}

static int ark3116_read_reg(struct usb_serial *serial,
			    unsigned reg, unsigned char *buf)
{
	int result;
	
	result = usb_control_msg(serial->dev,
				 usb_rcvctrlpipe(serial->dev, 0),
				 0xfe, 0xc0, 0, reg,
				 buf, 1, ARK_TIMEOUT);
	if (result < 0)
		return result;
	else
		return buf[0];
}

static inline int calc_divisor(int bps)
{
	return (12000000 + 2*bps) / (4*bps);
}

static int ark3116_attach(struct usb_serial *serial)
{
	struct usb_serial_port *port = serial->port[0];
	struct ark3116_private *priv;

	
	if ((serial->num_bulk_in == 0) ||
	    (serial->num_bulk_out == 0) ||
	    (serial->num_interrupt_in == 0)) {
		dev_err(&serial->dev->dev,
			"%s - missing endpoint - "
			"bulk in: %d, bulk out: %d, int in %d\n",
			KBUILD_MODNAME,
			serial->num_bulk_in,
			serial->num_bulk_out,
			serial->num_interrupt_in);
		return -EINVAL;
	}

	priv = kzalloc(sizeof(struct ark3116_private),
		       GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	init_waitqueue_head(&priv->delta_msr_wait);
	mutex_init(&priv->hw_lock);
	spin_lock_init(&priv->status_lock);

	priv->irda = is_irda(serial);

	usb_set_serial_port_data(port, priv);

	
	ark3116_write_reg(serial, UART_IER, 0);
	
	ark3116_write_reg(serial, UART_FCR, 0);
	
	priv->hcr = 0;
	ark3116_write_reg(serial, 0x8     , 0);
	
	priv->mcr = 0;
	ark3116_write_reg(serial, UART_MCR, 0);

	if (!(priv->irda)) {
		ark3116_write_reg(serial, 0xb , 0);
	} else {
		ark3116_write_reg(serial, 0xb , 1);
		ark3116_write_reg(serial, 0xc , 0);
		ark3116_write_reg(serial, 0xd , 0x41);
		ark3116_write_reg(serial, 0xa , 1);
	}

	
	ark3116_write_reg(serial, UART_LCR, UART_LCR_DLAB);

	
	priv->quot = calc_divisor(9600);
	ark3116_write_reg(serial, UART_DLL, priv->quot & 0xff);
	ark3116_write_reg(serial, UART_DLM, (priv->quot>>8) & 0xff);

	priv->lcr = UART_LCR_WLEN8;
	ark3116_write_reg(serial, UART_LCR, UART_LCR_WLEN8);

	ark3116_write_reg(serial, 0xe, 0);

	if (priv->irda)
		ark3116_write_reg(serial, 0x9, 0);

	dev_info(&serial->dev->dev,
		"%s using %s mode\n",
		KBUILD_MODNAME,
		priv->irda ? "IrDA" : "RS232");
	return 0;
}

static void ark3116_release(struct usb_serial *serial)
{
	struct usb_serial_port *port = serial->port[0];
	struct ark3116_private *priv = usb_get_serial_port_data(port);

	

	usb_set_serial_port_data(port, NULL);

	mutex_destroy(&priv->hw_lock);

	kfree(priv);
}

static void ark3116_init_termios(struct tty_struct *tty)
{
	struct ktermios *termios = tty->termios;
	*termios = tty_std_termios;
	termios->c_cflag = B9600 | CS8
				      | CREAD | HUPCL | CLOCAL;
	termios->c_ispeed = 9600;
	termios->c_ospeed = 9600;
}

static void ark3116_set_termios(struct tty_struct *tty,
				struct usb_serial_port *port,
				struct ktermios *old_termios)
{
	struct usb_serial *serial = port->serial;
	struct ark3116_private *priv = usb_get_serial_port_data(port);
	struct ktermios *termios = tty->termios;
	unsigned int cflag = termios->c_cflag;
	int bps = tty_get_baud_rate(tty);
	int quot;
	__u8 lcr, hcr, eval;

	
	switch (cflag & CSIZE) {
	case CS5:
		lcr = UART_LCR_WLEN5;
		break;
	case CS6:
		lcr = UART_LCR_WLEN6;
		break;
	case CS7:
		lcr = UART_LCR_WLEN7;
		break;
	default:
	case CS8:
		lcr = UART_LCR_WLEN8;
		break;
	}
	if (cflag & CSTOPB)
		lcr |= UART_LCR_STOP;
	if (cflag & PARENB)
		lcr |= UART_LCR_PARITY;
	if (!(cflag & PARODD))
		lcr |= UART_LCR_EPAR;
#ifdef CMSPAR
	if (cflag & CMSPAR)
		lcr |= UART_LCR_SPAR;
#endif
	
	hcr = (cflag & CRTSCTS) ? 0x03 : 0x00;

	
	dbg("%s - setting bps to %d", __func__, bps);
	eval = 0;
	switch (bps) {
	case 0:
		quot = calc_divisor(9600);
		break;
	default:
		if ((bps < 75) || (bps > 3000000))
			bps = 9600;
		quot = calc_divisor(bps);
		break;
	case 460800:
		eval = 1;
		quot = calc_divisor(bps);
		break;
	case 921600:
		eval = 2;
		quot = calc_divisor(bps);
		break;
	}

	
	mutex_lock(&priv->hw_lock);

	
	lcr |= (priv->lcr & UART_LCR_SBC);

	dbg("%s - setting hcr:0x%02x,lcr:0x%02x,quot:%d",
	    __func__, hcr, lcr, quot);

	
	if (priv->hcr != hcr) {
		priv->hcr = hcr;
		ark3116_write_reg(serial, 0x8, hcr);
	}

	
	if (priv->quot != quot) {
		priv->quot = quot;
		priv->lcr = lcr; 

		ark3116_write_reg(serial, UART_FCR, 0);

		ark3116_write_reg(serial, UART_LCR,
				  lcr|UART_LCR_DLAB);
		ark3116_write_reg(serial, UART_DLL, quot & 0xff);
		ark3116_write_reg(serial, UART_DLM, (quot>>8) & 0xff);

		
		ark3116_write_reg(serial, UART_LCR, lcr);
		ark3116_write_reg(serial, 0xe, eval);

		
		ark3116_write_reg(serial, UART_FCR, UART_FCR_DMA_SELECT);
	} else if (priv->lcr != lcr) {
		priv->lcr = lcr;
		ark3116_write_reg(serial, UART_LCR, lcr);
	}

	mutex_unlock(&priv->hw_lock);

	
	if (I_IXOFF(tty) || I_IXON(tty)) {
		dev_warn(&serial->dev->dev,
			 "%s: don't know how to do software flow control\n",
			 KBUILD_MODNAME);
	}

	
	if (tty_termios_baud_rate(termios))
		tty_termios_encode_baud_rate(termios, bps, bps);
}

static void ark3116_close(struct usb_serial_port *port)
{
	struct usb_serial *serial = port->serial;

	if (serial->dev) {
		
		ark3116_write_reg(serial, UART_FCR, 0);

		
		ark3116_write_reg(serial, UART_IER, 0);

		usb_serial_generic_close(port);
		if (serial->num_interrupt_in)
			usb_kill_urb(port->interrupt_in_urb);
	}

}

static int ark3116_open(struct tty_struct *tty, struct usb_serial_port *port)
{
	struct ark3116_private *priv = usb_get_serial_port_data(port);
	struct usb_serial *serial = port->serial;
	unsigned char *buf;
	int result;

	buf = kmalloc(1, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	result = usb_serial_generic_open(tty, port);
	if (result) {
		dbg("%s - usb_serial_generic_open failed: %d",
		    __func__, result);
		goto err_out;
	}

	
	ark3116_read_reg(serial, UART_RX, buf);

	
	priv->msr = ark3116_read_reg(serial, UART_MSR, buf);
	
	priv->lsr = ark3116_read_reg(serial, UART_LSR, buf);

	result = usb_submit_urb(port->interrupt_in_urb, GFP_KERNEL);
	if (result) {
		dev_err(&port->dev, "submit irq_in urb failed %d\n",
			result);
		ark3116_close(port);
		goto err_out;
	}

	
	ark3116_write_reg(port->serial, UART_IER, UART_IER_MSI|UART_IER_RLSI);

	
	ark3116_write_reg(port->serial, UART_FCR, UART_FCR_DMA_SELECT);

	
	if (tty)
		ark3116_set_termios(tty, port, NULL);

err_out:
	kfree(buf);
	return result;
}

static int ark3116_get_icount(struct tty_struct *tty,
					struct serial_icounter_struct *icount)
{
	struct usb_serial_port *port = tty->driver_data;
	struct ark3116_private *priv = usb_get_serial_port_data(port);
	struct async_icount cnow = priv->icount;
	icount->cts = cnow.cts;
	icount->dsr = cnow.dsr;
	icount->rng = cnow.rng;
	icount->dcd = cnow.dcd;
	icount->rx = cnow.rx;
	icount->tx = cnow.tx;
	icount->frame = cnow.frame;
	icount->overrun = cnow.overrun;
	icount->parity = cnow.parity;
	icount->brk = cnow.brk;
	icount->buf_overrun = cnow.buf_overrun;
	return 0;
}

static int ark3116_ioctl(struct tty_struct *tty,
			 unsigned int cmd, unsigned long arg)
{
	struct usb_serial_port *port = tty->driver_data;
	struct ark3116_private *priv = usb_get_serial_port_data(port);
	struct serial_struct serstruct;
	void __user *user_arg = (void __user *)arg;

	switch (cmd) {
	case TIOCGSERIAL:
		
		memset(&serstruct, 0, sizeof(serstruct));
		serstruct.type = PORT_16654;
		serstruct.line = port->serial->minor;
		serstruct.port = port->number;
		serstruct.custom_divisor = 0;
		serstruct.baud_base = 460800;

		if (copy_to_user(user_arg, &serstruct, sizeof(serstruct)))
			return -EFAULT;

		return 0;
	case TIOCSSERIAL:
		if (copy_from_user(&serstruct, user_arg, sizeof(serstruct)))
			return -EFAULT;
		return 0;
	case TIOCMIWAIT:
		for (;;) {
			struct async_icount prev = priv->icount;
			interruptible_sleep_on(&priv->delta_msr_wait);
			
			if (signal_pending(current))
				return -ERESTARTSYS;
			if ((prev.rng == priv->icount.rng) &&
			    (prev.dsr == priv->icount.dsr) &&
			    (prev.dcd == priv->icount.dcd) &&
			    (prev.cts == priv->icount.cts))
				return -EIO;
			if ((arg & TIOCM_RNG &&
			     (prev.rng != priv->icount.rng)) ||
			    (arg & TIOCM_DSR &&
			     (prev.dsr != priv->icount.dsr)) ||
			    (arg & TIOCM_CD  &&
			     (prev.dcd != priv->icount.dcd)) ||
			    (arg & TIOCM_CTS &&
			     (prev.cts != priv->icount.cts)))
				return 0;
		}
		break;
	}

	return -ENOIOCTLCMD;
}

static int ark3116_tiocmget(struct tty_struct *tty)
{
	struct usb_serial_port *port = tty->driver_data;
	struct ark3116_private *priv = usb_get_serial_port_data(port);
	__u32 status;
	__u32 ctrl;
	unsigned long flags;

	mutex_lock(&priv->hw_lock);
	ctrl = priv->mcr;
	mutex_unlock(&priv->hw_lock);

	spin_lock_irqsave(&priv->status_lock, flags);
	status = priv->msr;
	spin_unlock_irqrestore(&priv->status_lock, flags);

	return  (status & UART_MSR_DSR  ? TIOCM_DSR  : 0) |
		(status & UART_MSR_CTS  ? TIOCM_CTS  : 0) |
		(status & UART_MSR_RI   ? TIOCM_RI   : 0) |
		(status & UART_MSR_DCD  ? TIOCM_CD   : 0) |
		(ctrl   & UART_MCR_DTR  ? TIOCM_DTR  : 0) |
		(ctrl   & UART_MCR_RTS  ? TIOCM_RTS  : 0) |
		(ctrl   & UART_MCR_OUT1 ? TIOCM_OUT1 : 0) |
		(ctrl   & UART_MCR_OUT2 ? TIOCM_OUT2 : 0);
}

static int ark3116_tiocmset(struct tty_struct *tty,
			unsigned set, unsigned clr)
{
	struct usb_serial_port *port = tty->driver_data;
	struct ark3116_private *priv = usb_get_serial_port_data(port);


	mutex_lock(&priv->hw_lock);

	if (set & TIOCM_RTS)
		priv->mcr |= UART_MCR_RTS;
	if (set & TIOCM_DTR)
		priv->mcr |= UART_MCR_DTR;
	if (set & TIOCM_OUT1)
		priv->mcr |= UART_MCR_OUT1;
	if (set & TIOCM_OUT2)
		priv->mcr |= UART_MCR_OUT2;
	if (clr & TIOCM_RTS)
		priv->mcr &= ~UART_MCR_RTS;
	if (clr & TIOCM_DTR)
		priv->mcr &= ~UART_MCR_DTR;
	if (clr & TIOCM_OUT1)
		priv->mcr &= ~UART_MCR_OUT1;
	if (clr & TIOCM_OUT2)
		priv->mcr &= ~UART_MCR_OUT2;

	ark3116_write_reg(port->serial, UART_MCR, priv->mcr);

	mutex_unlock(&priv->hw_lock);

	return 0;
}

static void ark3116_break_ctl(struct tty_struct *tty, int break_state)
{
	struct usb_serial_port *port = tty->driver_data;
	struct ark3116_private *priv = usb_get_serial_port_data(port);

	
	mutex_lock(&priv->hw_lock);

	if (break_state)
		priv->lcr |= UART_LCR_SBC;
	else
		priv->lcr &= ~UART_LCR_SBC;

	ark3116_write_reg(port->serial, UART_LCR, priv->lcr);

	mutex_unlock(&priv->hw_lock);
}

static void ark3116_update_msr(struct usb_serial_port *port, __u8 msr)
{
	struct ark3116_private *priv = usb_get_serial_port_data(port);
	unsigned long flags;

	spin_lock_irqsave(&priv->status_lock, flags);
	priv->msr = msr;
	spin_unlock_irqrestore(&priv->status_lock, flags);

	if (msr & UART_MSR_ANY_DELTA) {
		
		if (msr & UART_MSR_DCTS)
			priv->icount.cts++;
		if (msr & UART_MSR_DDSR)
			priv->icount.dsr++;
		if (msr & UART_MSR_DDCD)
			priv->icount.dcd++;
		if (msr & UART_MSR_TERI)
			priv->icount.rng++;
		wake_up_interruptible(&priv->delta_msr_wait);
	}
}

static void ark3116_update_lsr(struct usb_serial_port *port, __u8 lsr)
{
	struct ark3116_private *priv = usb_get_serial_port_data(port);
	unsigned long flags;

	spin_lock_irqsave(&priv->status_lock, flags);
	
	priv->lsr |= lsr;
	spin_unlock_irqrestore(&priv->status_lock, flags);

	if (lsr&UART_LSR_BRK_ERROR_BITS) {
		if (lsr & UART_LSR_BI)
			priv->icount.brk++;
		if (lsr & UART_LSR_FE)
			priv->icount.frame++;
		if (lsr & UART_LSR_PE)
			priv->icount.parity++;
		if (lsr & UART_LSR_OE)
			priv->icount.overrun++;
	}
}

static void ark3116_read_int_callback(struct urb *urb)
{
	struct usb_serial_port *port = urb->context;
	int status = urb->status;
	const __u8 *data = urb->transfer_buffer;
	int result;

	switch (status) {
	case -ECONNRESET:
	case -ENOENT:
	case -ESHUTDOWN:
		
		dbg("%s - urb shutting down with status: %d",
		    __func__, status);
		return;
	default:
		dbg("%s - nonzero urb status received: %d",
		    __func__, status);
		break;
	case 0: 
		
		if ((urb->actual_length == 4) && (data[0] == 0xe8)) {
			const __u8 id = data[1]&UART_IIR_ID;
			dbg("%s: iir=%02x", __func__, data[1]);
			if (id == UART_IIR_MSI) {
				dbg("%s: msr=%02x", __func__, data[3]);
				ark3116_update_msr(port, data[3]);
				break;
			} else if (id == UART_IIR_RLSI) {
				dbg("%s: lsr=%02x", __func__, data[2]);
				ark3116_update_lsr(port, data[2]);
				break;
			}
		}
		usb_serial_debug_data(debug, &port->dev,
				      __func__,
				      urb->actual_length,
				      urb->transfer_buffer);
		break;
	}

	result = usb_submit_urb(urb, GFP_ATOMIC);
	if (result)
		dev_err(&urb->dev->dev,
			"%s - Error %d submitting interrupt urb\n",
			__func__, result);
}


static void ark3116_process_read_urb(struct urb *urb)
{
	struct usb_serial_port *port = urb->context;
	struct ark3116_private *priv = usb_get_serial_port_data(port);
	struct tty_struct *tty;
	unsigned char *data = urb->transfer_buffer;
	char tty_flag = TTY_NORMAL;
	unsigned long flags;
	__u32 lsr;

	
	spin_lock_irqsave(&priv->status_lock, flags);
	lsr = priv->lsr;
	priv->lsr &= ~UART_LSR_BRK_ERROR_BITS;
	spin_unlock_irqrestore(&priv->status_lock, flags);

	if (!urb->actual_length)
		return;

	tty = tty_port_tty_get(&port->port);
	if (!tty)
		return;

	if (lsr & UART_LSR_BRK_ERROR_BITS) {
		if (lsr & UART_LSR_BI)
			tty_flag = TTY_BREAK;
		else if (lsr & UART_LSR_PE)
			tty_flag = TTY_PARITY;
		else if (lsr & UART_LSR_FE)
			tty_flag = TTY_FRAME;

		
		if (lsr & UART_LSR_OE)
			tty_insert_flip_char(tty, 0, TTY_OVERRUN);
	}
	tty_insert_flip_string_fixed_flag(tty, data, tty_flag,
							urb->actual_length);
	tty_flip_buffer_push(tty);
	tty_kref_put(tty);
}

static struct usb_driver ark3116_driver = {
	.name =		"ark3116",
	.probe =	usb_serial_probe,
	.disconnect =	usb_serial_disconnect,
	.id_table =	id_table,
};

static struct usb_serial_driver ark3116_device = {
	.driver = {
		.owner =	THIS_MODULE,
		.name =		"ark3116",
	},
	.id_table =		id_table,
	.num_ports =		1,
	.attach =		ark3116_attach,
	.release =		ark3116_release,
	.set_termios =		ark3116_set_termios,
	.init_termios =		ark3116_init_termios,
	.ioctl =		ark3116_ioctl,
	.tiocmget =		ark3116_tiocmget,
	.tiocmset =		ark3116_tiocmset,
	.get_icount =		ark3116_get_icount,
	.open =			ark3116_open,
	.close =		ark3116_close,
	.break_ctl = 		ark3116_break_ctl,
	.read_int_callback = 	ark3116_read_int_callback,
	.process_read_urb =	ark3116_process_read_urb,
};

static struct usb_serial_driver * const serial_drivers[] = {
	&ark3116_device, NULL
};

module_usb_serial_driver(ark3116_driver, serial_drivers);

MODULE_LICENSE("GPL");

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);

module_param(debug, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "Enable debug");

