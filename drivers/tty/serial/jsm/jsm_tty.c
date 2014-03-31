/************************************************************************
 * Copyright 2003 Digi International (www.digi.com)
 *
 * Copyright (C) 2004 IBM Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 * Temple Place - Suite 330, Boston,
 * MA  02111-1307, USA.
 *
 * Contact Information:
 * Scott H Kilau <Scott_Kilau@digi.com>
 * Ananda Venkatarman <mansarov@us.ibm.com>
 * Modifications:
 * 01/19/06:	changed jsm_input routine to use the dynamically allocated
 *		tty_buffer changes. Contributors: Scott Kilau and Ananda V.
 ***********************************************************************/
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial_reg.h>
#include <linux/delay.h>	
#include <linux/pci.h>
#include <linux/slab.h>

#include "jsm.h"

static DECLARE_BITMAP(linemap, MAXLINES);

static void jsm_carrier(struct jsm_channel *ch);

static inline int jsm_get_mstat(struct jsm_channel *ch)
{
	unsigned char mstat;
	unsigned result;

	jsm_printk(IOCTL, INFO, &ch->ch_bd->pci_dev, "start\n");

	mstat = (ch->ch_mostat | ch->ch_mistat);

	result = 0;

	if (mstat & UART_MCR_DTR)
		result |= TIOCM_DTR;
	if (mstat & UART_MCR_RTS)
		result |= TIOCM_RTS;
	if (mstat & UART_MSR_CTS)
		result |= TIOCM_CTS;
	if (mstat & UART_MSR_DSR)
		result |= TIOCM_DSR;
	if (mstat & UART_MSR_RI)
		result |= TIOCM_RI;
	if (mstat & UART_MSR_DCD)
		result |= TIOCM_CD;

	jsm_printk(IOCTL, INFO, &ch->ch_bd->pci_dev, "finish\n");
	return result;
}

static unsigned int jsm_tty_tx_empty(struct uart_port *port)
{
	return TIOCSER_TEMT;
}

static unsigned int jsm_tty_get_mctrl(struct uart_port *port)
{
	int result;
	struct jsm_channel *channel = (struct jsm_channel *)port;

	jsm_printk(IOCTL, INFO, &channel->ch_bd->pci_dev, "start\n");

	result = jsm_get_mstat(channel);

	if (result < 0)
		return -ENXIO;

	jsm_printk(IOCTL, INFO, &channel->ch_bd->pci_dev, "finish\n");

	return result;
}

static void jsm_tty_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	struct jsm_channel *channel = (struct jsm_channel *)port;

	jsm_printk(IOCTL, INFO, &channel->ch_bd->pci_dev, "start\n");

	if (mctrl & TIOCM_RTS)
		channel->ch_mostat |= UART_MCR_RTS;
	else
		channel->ch_mostat &= ~UART_MCR_RTS;

	if (mctrl & TIOCM_DTR)
		channel->ch_mostat |= UART_MCR_DTR;
	else
		channel->ch_mostat &= ~UART_MCR_DTR;

	channel->ch_bd->bd_ops->assert_modem_signals(channel);

	jsm_printk(IOCTL, INFO, &channel->ch_bd->pci_dev, "finish\n");
	udelay(10);
}

static void jsm_tty_write(struct uart_port *port)
{
	struct jsm_channel *channel;
	channel = container_of(port, struct jsm_channel, uart_port);
	channel->ch_bd->bd_ops->copy_data_from_queue_to_uart(channel);
}

static void jsm_tty_start_tx(struct uart_port *port)
{
	struct jsm_channel *channel = (struct jsm_channel *)port;

	jsm_printk(IOCTL, INFO, &channel->ch_bd->pci_dev, "start\n");

	channel->ch_flags &= ~(CH_STOP);
	jsm_tty_write(port);

	jsm_printk(IOCTL, INFO, &channel->ch_bd->pci_dev, "finish\n");
}

static void jsm_tty_stop_tx(struct uart_port *port)
{
	struct jsm_channel *channel = (struct jsm_channel *)port;

	jsm_printk(IOCTL, INFO, &channel->ch_bd->pci_dev, "start\n");

	channel->ch_flags |= (CH_STOP);

	jsm_printk(IOCTL, INFO, &channel->ch_bd->pci_dev, "finish\n");
}

static void jsm_tty_send_xchar(struct uart_port *port, char ch)
{
	unsigned long lock_flags;
	struct jsm_channel *channel = (struct jsm_channel *)port;
	struct ktermios *termios;

	spin_lock_irqsave(&port->lock, lock_flags);
	termios = port->state->port.tty->termios;
	if (ch == termios->c_cc[VSTART])
		channel->ch_bd->bd_ops->send_start_character(channel);

	if (ch == termios->c_cc[VSTOP])
		channel->ch_bd->bd_ops->send_stop_character(channel);
	spin_unlock_irqrestore(&port->lock, lock_flags);
}

static void jsm_tty_stop_rx(struct uart_port *port)
{
	struct jsm_channel *channel = (struct jsm_channel *)port;

	channel->ch_bd->bd_ops->disable_receiver(channel);
}

static void jsm_tty_enable_ms(struct uart_port *port)
{
	
}

static void jsm_tty_break(struct uart_port *port, int break_state)
{
	unsigned long lock_flags;
	struct jsm_channel *channel = (struct jsm_channel *)port;

	spin_lock_irqsave(&port->lock, lock_flags);
	if (break_state == -1)
		channel->ch_bd->bd_ops->send_break(channel);
	else
		channel->ch_bd->bd_ops->clear_break(channel, 0);

	spin_unlock_irqrestore(&port->lock, lock_flags);
}

static int jsm_tty_open(struct uart_port *port)
{
	struct jsm_board *brd;
	struct jsm_channel *channel = (struct jsm_channel *)port;
	struct ktermios *termios;

	
	brd = channel->ch_bd;

	channel->ch_flags |= (CH_OPENING);

	

	if (!channel->ch_rqueue) {
		channel->ch_rqueue = kzalloc(RQUEUESIZE, GFP_KERNEL);
		if (!channel->ch_rqueue) {
			jsm_printk(INIT, ERR, &channel->ch_bd->pci_dev,
				"unable to allocate read queue buf");
			return -ENOMEM;
		}
	}
	if (!channel->ch_equeue) {
		channel->ch_equeue = kzalloc(EQUEUESIZE, GFP_KERNEL);
		if (!channel->ch_equeue) {
			jsm_printk(INIT, ERR, &channel->ch_bd->pci_dev,
				"unable to allocate error queue buf");
			return -ENOMEM;
		}
	}

	channel->ch_flags &= ~(CH_OPENING);
	jsm_printk(OPEN, INFO, &channel->ch_bd->pci_dev,
		"jsm_open: initializing channel in open...\n");

	channel->ch_r_head = channel->ch_r_tail = 0;
	channel->ch_e_head = channel->ch_e_tail = 0;

	brd->bd_ops->flush_uart_write(channel);
	brd->bd_ops->flush_uart_read(channel);

	channel->ch_flags = 0;
	channel->ch_cached_lsr = 0;
	channel->ch_stops_sent = 0;

	termios = port->state->port.tty->termios;
	channel->ch_c_cflag	= termios->c_cflag;
	channel->ch_c_iflag	= termios->c_iflag;
	channel->ch_c_oflag	= termios->c_oflag;
	channel->ch_c_lflag	= termios->c_lflag;
	channel->ch_startc	= termios->c_cc[VSTART];
	channel->ch_stopc	= termios->c_cc[VSTOP];

	
	brd->bd_ops->uart_init(channel);

	brd->bd_ops->param(channel);

	jsm_carrier(channel);

	channel->ch_open_count++;

	jsm_printk(OPEN, INFO, &channel->ch_bd->pci_dev, "finish\n");
	return 0;
}

static void jsm_tty_close(struct uart_port *port)
{
	struct jsm_board *bd;
	struct ktermios *ts;
	struct jsm_channel *channel = (struct jsm_channel *)port;

	jsm_printk(CLOSE, INFO, &channel->ch_bd->pci_dev, "start\n");

	bd = channel->ch_bd;
	ts = port->state->port.tty->termios;

	channel->ch_flags &= ~(CH_STOPI);

	channel->ch_open_count--;

	if (channel->ch_c_cflag & HUPCL) {
		jsm_printk(CLOSE, INFO, &channel->ch_bd->pci_dev,
			"Close. HUPCL set, dropping DTR/RTS\n");

		
		channel->ch_mostat &= ~(UART_MCR_DTR | UART_MCR_RTS);
		bd->bd_ops->assert_modem_signals(channel);
	}

	
	channel->ch_bd->bd_ops->uart_off(channel);

	jsm_printk(CLOSE, INFO, &channel->ch_bd->pci_dev, "finish\n");
}

static void jsm_tty_set_termios(struct uart_port *port,
				 struct ktermios *termios,
				 struct ktermios *old_termios)
{
	unsigned long lock_flags;
	struct jsm_channel *channel = (struct jsm_channel *)port;

	spin_lock_irqsave(&port->lock, lock_flags);
	channel->ch_c_cflag	= termios->c_cflag;
	channel->ch_c_iflag	= termios->c_iflag;
	channel->ch_c_oflag	= termios->c_oflag;
	channel->ch_c_lflag	= termios->c_lflag;
	channel->ch_startc	= termios->c_cc[VSTART];
	channel->ch_stopc	= termios->c_cc[VSTOP];

	channel->ch_bd->bd_ops->param(channel);
	jsm_carrier(channel);
	spin_unlock_irqrestore(&port->lock, lock_flags);
}

static const char *jsm_tty_type(struct uart_port *port)
{
	return "jsm";
}

static void jsm_tty_release_port(struct uart_port *port)
{
}

static int jsm_tty_request_port(struct uart_port *port)
{
	return 0;
}

static void jsm_config_port(struct uart_port *port, int flags)
{
	port->type = PORT_JSM;
}

static struct uart_ops jsm_ops = {
	.tx_empty	= jsm_tty_tx_empty,
	.set_mctrl	= jsm_tty_set_mctrl,
	.get_mctrl	= jsm_tty_get_mctrl,
	.stop_tx	= jsm_tty_stop_tx,
	.start_tx	= jsm_tty_start_tx,
	.send_xchar	= jsm_tty_send_xchar,
	.stop_rx	= jsm_tty_stop_rx,
	.enable_ms	= jsm_tty_enable_ms,
	.break_ctl	= jsm_tty_break,
	.startup	= jsm_tty_open,
	.shutdown	= jsm_tty_close,
	.set_termios	= jsm_tty_set_termios,
	.type		= jsm_tty_type,
	.release_port	= jsm_tty_release_port,
	.request_port	= jsm_tty_request_port,
	.config_port	= jsm_config_port,
};

int __devinit jsm_tty_init(struct jsm_board *brd)
{
	int i;
	void __iomem *vaddr;
	struct jsm_channel *ch;

	if (!brd)
		return -ENXIO;

	jsm_printk(INIT, INFO, &brd->pci_dev, "start\n");


	brd->nasync = brd->maxports;

	for (i = 0; i < brd->nasync; i++) {
		if (!brd->channels[i]) {

			brd->channels[i] = kzalloc(sizeof(struct jsm_channel), GFP_KERNEL);
			if (!brd->channels[i]) {
				jsm_printk(CORE, ERR, &brd->pci_dev,
					"%s:%d Unable to allocate memory for channel struct\n",
							 __FILE__, __LINE__);
			}
		}
	}

	ch = brd->channels[0];
	vaddr = brd->re_map_membase;

	
	for (i = 0; i < brd->nasync; i++, ch = brd->channels[i]) {

		if (!brd->channels[i])
			continue;

		spin_lock_init(&ch->ch_lock);

		if (brd->bd_uart_offset == 0x200)
			ch->ch_neo_uart =  vaddr + (brd->bd_uart_offset * i);

		ch->ch_bd = brd;
		ch->ch_portnum = i;

		
		ch->ch_close_delay = 250;

		init_waitqueue_head(&ch->ch_flags_wait);
	}

	jsm_printk(INIT, INFO, &brd->pci_dev, "finish\n");
	return 0;
}

int jsm_uart_port_init(struct jsm_board *brd)
{
	int i, rc;
	unsigned int line;
	struct jsm_channel *ch;

	if (!brd)
		return -ENXIO;

	jsm_printk(INIT, INFO, &brd->pci_dev, "start\n");


	brd->nasync = brd->maxports;

	
	for (i = 0; i < brd->nasync; i++, ch = brd->channels[i]) {

		if (!brd->channels[i])
			continue;

		brd->channels[i]->uart_port.irq = brd->irq;
		brd->channels[i]->uart_port.uartclk = 14745600;
		brd->channels[i]->uart_port.type = PORT_JSM;
		brd->channels[i]->uart_port.iotype = UPIO_MEM;
		brd->channels[i]->uart_port.membase = brd->re_map_membase;
		brd->channels[i]->uart_port.fifosize = 16;
		brd->channels[i]->uart_port.ops = &jsm_ops;
		line = find_first_zero_bit(linemap, MAXLINES);
		if (line >= MAXLINES) {
			printk(KERN_INFO "jsm: linemap is full, added device failed\n");
			continue;
		} else
			set_bit(line, linemap);
		brd->channels[i]->uart_port.line = line;
		rc = uart_add_one_port (&jsm_uart_driver, &brd->channels[i]->uart_port);
		if (rc){
			printk(KERN_INFO "jsm: Port %d failed. Aborting...\n", i);
			return rc;
		}
		else
			printk(KERN_INFO "jsm: Port %d added\n", i);
	}

	jsm_printk(INIT, INFO, &brd->pci_dev, "finish\n");
	return 0;
}

int jsm_remove_uart_port(struct jsm_board *brd)
{
	int i;
	struct jsm_channel *ch;

	if (!brd)
		return -ENXIO;

	jsm_printk(INIT, INFO, &brd->pci_dev, "start\n");


	brd->nasync = brd->maxports;

	
	for (i = 0; i < brd->nasync; i++) {

		if (!brd->channels[i])
			continue;

		ch = brd->channels[i];

		clear_bit(ch->uart_port.line, linemap);
		uart_remove_one_port(&jsm_uart_driver, &brd->channels[i]->uart_port);
	}

	jsm_printk(INIT, INFO, &brd->pci_dev, "finish\n");
	return 0;
}

void jsm_input(struct jsm_channel *ch)
{
	struct jsm_board *bd;
	struct tty_struct *tp;
	u32 rmask;
	u16 head;
	u16 tail;
	int data_len;
	unsigned long lock_flags;
	int len = 0;
	int n = 0;
	int s = 0;
	int i = 0;

	jsm_printk(READ, INFO, &ch->ch_bd->pci_dev, "start\n");

	if (!ch)
		return;

	tp = ch->uart_port.state->port.tty;

	bd = ch->ch_bd;
	if(!bd)
		return;

	spin_lock_irqsave(&ch->ch_lock, lock_flags);


	rmask = RQUEUEMASK;

	head = ch->ch_r_head & rmask;
	tail = ch->ch_r_tail & rmask;

	data_len = (head - tail) & rmask;
	if (data_len == 0) {
		spin_unlock_irqrestore(&ch->ch_lock, lock_flags);
		return;
	}

	jsm_printk(READ, INFO, &ch->ch_bd->pci_dev, "start\n");

	if (!tp ||
		!(tp->termios->c_cflag & CREAD) ) {

		jsm_printk(READ, INFO, &ch->ch_bd->pci_dev,
			"input. dropping %d bytes on port %d...\n", data_len, ch->ch_portnum);
		ch->ch_r_head = tail;

		
		jsm_check_queue_flow_control(ch);

		spin_unlock_irqrestore(&ch->ch_lock, lock_flags);
		return;
	}

	if (ch->ch_flags & CH_STOPI) {
		spin_unlock_irqrestore(&ch->ch_lock, lock_flags);
		jsm_printk(READ, INFO, &ch->ch_bd->pci_dev,
			"Port %d throttled, not reading any data. head: %x tail: %x\n",
			ch->ch_portnum, head, tail);
		return;
	}

	jsm_printk(READ, INFO, &ch->ch_bd->pci_dev, "start 2\n");

	if (data_len <= 0) {
		spin_unlock_irqrestore(&ch->ch_lock, lock_flags);
		jsm_printk(READ, INFO, &ch->ch_bd->pci_dev, "jsm_input 1\n");
		return;
	}

	len = tty_buffer_request_room(tp, data_len);
	n = len;

	while (n) {
		s = ((head >= tail) ? head : RQUEUESIZE) - tail;
		s = min(s, n);

		if (s <= 0)
			break;


		if (I_PARMRK(tp) || I_BRKINT(tp) || I_INPCK(tp)) {
			for (i = 0; i < s; i++) {
				if (*(ch->ch_equeue +tail +i) & UART_LSR_BI)
					tty_insert_flip_char(tp, *(ch->ch_rqueue +tail +i),  TTY_BREAK);
				else if (*(ch->ch_equeue +tail +i) & UART_LSR_PE)
					tty_insert_flip_char(tp, *(ch->ch_rqueue +tail +i), TTY_PARITY);
				else if (*(ch->ch_equeue +tail +i) & UART_LSR_FE)
					tty_insert_flip_char(tp, *(ch->ch_rqueue +tail +i), TTY_FRAME);
				else
					tty_insert_flip_char(tp, *(ch->ch_rqueue +tail +i), TTY_NORMAL);
			}
		} else {
			tty_insert_flip_string(tp, ch->ch_rqueue + tail, s) ;
		}
		tail += s;
		n -= s;
		
		tail &= rmask;
	}

	ch->ch_r_tail = tail & rmask;
	ch->ch_e_tail = tail & rmask;
	jsm_check_queue_flow_control(ch);
	spin_unlock_irqrestore(&ch->ch_lock, lock_flags);

	
	tty_flip_buffer_push(tp);

	jsm_printk(IOCTL, INFO, &ch->ch_bd->pci_dev, "finish\n");
}

static void jsm_carrier(struct jsm_channel *ch)
{
	struct jsm_board *bd;

	int virt_carrier = 0;
	int phys_carrier = 0;

	jsm_printk(CARR, INFO, &ch->ch_bd->pci_dev, "start\n");
	if (!ch)
		return;

	bd = ch->ch_bd;

	if (!bd)
		return;

	if (ch->ch_mistat & UART_MSR_DCD) {
		jsm_printk(CARR, INFO, &ch->ch_bd->pci_dev,
			"mistat: %x D_CD: %x\n", ch->ch_mistat, ch->ch_mistat & UART_MSR_DCD);
		phys_carrier = 1;
	}

	if (ch->ch_c_cflag & CLOCAL)
		virt_carrier = 1;

	jsm_printk(CARR, INFO, &ch->ch_bd->pci_dev,
		"DCD: physical: %d virt: %d\n", phys_carrier, virt_carrier);

	if (((ch->ch_flags & CH_FCAR) == 0) && (virt_carrier == 1)) {


		jsm_printk(CARR, INFO, &ch->ch_bd->pci_dev,
			"carrier: virt DCD rose\n");

		if (waitqueue_active(&(ch->ch_flags_wait)))
			wake_up_interruptible(&ch->ch_flags_wait);
	}

	if (((ch->ch_flags & CH_CD) == 0) && (phys_carrier == 1)) {


		jsm_printk(CARR, INFO, &ch->ch_bd->pci_dev,
			"carrier: physical DCD rose\n");

		if (waitqueue_active(&(ch->ch_flags_wait)))
			wake_up_interruptible(&ch->ch_flags_wait);
	}

	if ((virt_carrier == 0) && ((ch->ch_flags & CH_CD) != 0)
			&& (phys_carrier == 0)) {
		if (waitqueue_active(&(ch->ch_flags_wait)))
			wake_up_interruptible(&ch->ch_flags_wait);
	}

	if (virt_carrier == 1)
		ch->ch_flags |= CH_FCAR;
	else
		ch->ch_flags &= ~CH_FCAR;

	if (phys_carrier == 1)
		ch->ch_flags |= CH_CD;
	else
		ch->ch_flags &= ~CH_CD;
}


void jsm_check_queue_flow_control(struct jsm_channel *ch)
{
	struct board_ops *bd_ops = ch->ch_bd->bd_ops;
	int qleft;

	
	if ((qleft = ch->ch_r_tail - ch->ch_r_head - 1) < 0)
		qleft += RQUEUEMASK + 1;

	if (qleft < 256) {
		
		if (ch->ch_c_cflag & CRTSCTS) {
			if(!(ch->ch_flags & CH_RECEIVER_OFF)) {
				bd_ops->disable_receiver(ch);
				ch->ch_flags |= (CH_RECEIVER_OFF);
				jsm_printk(READ, INFO, &ch->ch_bd->pci_dev,
					"Internal queue hit hilevel mark (%d)! Turning off interrupts.\n",
					qleft);
			}
		}
		
		else if (ch->ch_c_iflag & IXOFF) {
			if (ch->ch_stops_sent <= MAX_STOPS_SENT) {
				bd_ops->send_stop_character(ch);
				ch->ch_stops_sent++;
				jsm_printk(READ, INFO, &ch->ch_bd->pci_dev,
					"Sending stop char! Times sent: %x\n", ch->ch_stops_sent);
			}
		}
	}

	if (qleft > (RQUEUESIZE / 2)) {
		
		if (ch->ch_c_cflag & CRTSCTS) {
			if (ch->ch_flags & CH_RECEIVER_OFF) {
				bd_ops->enable_receiver(ch);
				ch->ch_flags &= ~(CH_RECEIVER_OFF);
				jsm_printk(READ, INFO, &ch->ch_bd->pci_dev,
					"Internal queue hit lowlevel mark (%d)! Turning on interrupts.\n",
					qleft);
			}
		}
		
		else if (ch->ch_c_iflag & IXOFF && ch->ch_stops_sent) {
			ch->ch_stops_sent = 0;
			bd_ops->send_start_character(ch);
			jsm_printk(READ, INFO, &ch->ch_bd->pci_dev, "Sending start char!\n");
		}
	}
}
