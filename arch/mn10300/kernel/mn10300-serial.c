/* MN10300 On-chip serial port UART driver
 *
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */

static const char serial_name[] = "MN10300 Serial driver";
static const char serial_version[] = "mn10300_serial-1.0";
static const char serial_revdate[] = "2007-11-06";

#if defined(CONFIG_MN10300_TTYSM_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
#define SUPPORT_SYSRQ
#endif

#include <linux/module.h>
#include <linux/serial.h>
#include <linux/circ_buf.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/sysrq.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/bitops.h>
#include <asm/serial-regs.h>
#include <unit/timex.h>
#include "mn10300-serial.h"

#ifdef CONFIG_SMP
#undef  GxICR
#define GxICR(X) CROSS_GxICR(X, 0)
#endif 

#define kenter(FMT, ...) \
	printk(KERN_DEBUG "-->%s(" FMT ")\n", __func__, ##__VA_ARGS__)
#define _enter(FMT, ...) \
	no_printk(KERN_DEBUG "-->%s(" FMT ")\n", __func__, ##__VA_ARGS__)
#define kdebug(FMT, ...) \
	printk(KERN_DEBUG "--- " FMT "\n", ##__VA_ARGS__)
#define _debug(FMT, ...) \
	no_printk(KERN_DEBUG "--- " FMT "\n", ##__VA_ARGS__)
#define kproto(FMT, ...) \
	printk(KERN_DEBUG "### MNSERIAL " FMT " ###\n", ##__VA_ARGS__)
#define _proto(FMT, ...) \
	no_printk(KERN_DEBUG "### MNSERIAL " FMT " ###\n", ##__VA_ARGS__)

#ifndef CODMSB
#define CODMSB	004000000000	
#endif

#define NR_UARTS 3

#ifdef CONFIG_MN10300_TTYSM_CONSOLE
static void mn10300_serial_console_write(struct console *co,
					   const char *s, unsigned count);
static int __init mn10300_serial_console_setup(struct console *co,
						 char *options);

static struct uart_driver mn10300_serial_driver;
static struct console mn10300_serial_console = {
	.name		= "ttySM",
	.write		= mn10300_serial_console_write,
	.device		= uart_console_device,
	.setup		= mn10300_serial_console_setup,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
	.data		= &mn10300_serial_driver,
};
#endif

static struct uart_driver mn10300_serial_driver = {
	.owner		= NULL,
	.driver_name	= "mn10300-serial",
	.dev_name	= "ttySM",
	.major		= TTY_MAJOR,
	.minor		= 128,
	.nr		= NR_UARTS,
#ifdef CONFIG_MN10300_TTYSM_CONSOLE
	.cons		= &mn10300_serial_console,
#endif
};

static unsigned int mn10300_serial_tx_empty(struct uart_port *);
static void mn10300_serial_set_mctrl(struct uart_port *, unsigned int mctrl);
static unsigned int mn10300_serial_get_mctrl(struct uart_port *);
static void mn10300_serial_stop_tx(struct uart_port *);
static void mn10300_serial_start_tx(struct uart_port *);
static void mn10300_serial_send_xchar(struct uart_port *, char ch);
static void mn10300_serial_stop_rx(struct uart_port *);
static void mn10300_serial_enable_ms(struct uart_port *);
static void mn10300_serial_break_ctl(struct uart_port *, int ctl);
static int mn10300_serial_startup(struct uart_port *);
static void mn10300_serial_shutdown(struct uart_port *);
static void mn10300_serial_set_termios(struct uart_port *,
					 struct ktermios *new,
					 struct ktermios *old);
static const char *mn10300_serial_type(struct uart_port *);
static void mn10300_serial_release_port(struct uart_port *);
static int mn10300_serial_request_port(struct uart_port *);
static void mn10300_serial_config_port(struct uart_port *, int);
static int mn10300_serial_verify_port(struct uart_port *,
					struct serial_struct *);
#ifdef CONFIG_CONSOLE_POLL
static void mn10300_serial_poll_put_char(struct uart_port *, unsigned char);
static int mn10300_serial_poll_get_char(struct uart_port *);
#endif

static const struct uart_ops mn10300_serial_ops = {
	.tx_empty	= mn10300_serial_tx_empty,
	.set_mctrl	= mn10300_serial_set_mctrl,
	.get_mctrl	= mn10300_serial_get_mctrl,
	.stop_tx	= mn10300_serial_stop_tx,
	.start_tx	= mn10300_serial_start_tx,
	.send_xchar	= mn10300_serial_send_xchar,
	.stop_rx	= mn10300_serial_stop_rx,
	.enable_ms	= mn10300_serial_enable_ms,
	.break_ctl	= mn10300_serial_break_ctl,
	.startup	= mn10300_serial_startup,
	.shutdown	= mn10300_serial_shutdown,
	.set_termios	= mn10300_serial_set_termios,
	.type		= mn10300_serial_type,
	.release_port	= mn10300_serial_release_port,
	.request_port	= mn10300_serial_request_port,
	.config_port	= mn10300_serial_config_port,
	.verify_port	= mn10300_serial_verify_port,
#ifdef CONFIG_CONSOLE_POLL
	.poll_put_char	= mn10300_serial_poll_put_char,
	.poll_get_char	= mn10300_serial_poll_get_char,
#endif
};

static irqreturn_t mn10300_serial_interrupt(int irq, void *dev_id);

#ifdef CONFIG_MN10300_TTYSM0
struct mn10300_serial_port mn10300_serial_port_sif0 = {
	.uart.ops	= &mn10300_serial_ops,
	.uart.membase	= (void __iomem *) &SC0CTR,
	.uart.mapbase	= (unsigned long) &SC0CTR,
	.uart.iotype	= UPIO_MEM,
	.uart.irq	= 0,
	.uart.uartclk	= 0, 
	.uart.fifosize	= 1,
	.uart.flags	= UPF_BOOT_AUTOCONF,
	.uart.line	= 0,
	.uart.type	= PORT_MN10300,
	.uart.lock	=
	__SPIN_LOCK_UNLOCKED(mn10300_serial_port_sif0.uart.lock),
	.name		= "ttySM0",
	._iobase	= &SC0CTR,
	._control	= &SC0CTR,
	._status	= (volatile u8 *)&SC0STR,
	._intr		= &SC0ICR,
	._rxb		= &SC0RXB,
	._txb		= &SC0TXB,
	.rx_name	= "ttySM0:Rx",
	.tx_name	= "ttySM0:Tx",
#if defined(CONFIG_MN10300_TTYSM0_TIMER8)
	.tm_name	= "ttySM0:Timer8",
	._tmxmd		= &TM8MD,
	._tmxbr		= &TM8BR,
	._tmicr		= &TM8ICR,
	.tm_irq		= TM8IRQ,
	.div_timer	= MNSCx_DIV_TIMER_16BIT,
#elif defined(CONFIG_MN10300_TTYSM0_TIMER0)
	.tm_name	= "ttySM0:Timer0",
	._tmxmd		= &TM0MD,
	._tmxbr		= (volatile u16 *)&TM0BR,
	._tmicr		= &TM0ICR,
	.tm_irq		= TM0IRQ,
	.div_timer	= MNSCx_DIV_TIMER_8BIT,
#elif defined(CONFIG_MN10300_TTYSM0_TIMER2)
	.tm_name	= "ttySM0:Timer2",
	._tmxmd		= &TM2MD,
	._tmxbr		= (volatile u16 *)&TM2BR,
	._tmicr		= &TM2ICR,
	.tm_irq		= TM2IRQ,
	.div_timer	= MNSCx_DIV_TIMER_8BIT,
#else
#error "Unknown config for ttySM0"
#endif
	.rx_irq		= SC0RXIRQ,
	.tx_irq		= SC0TXIRQ,
	.rx_icr		= &GxICR(SC0RXIRQ),
	.tx_icr		= &GxICR(SC0TXIRQ),
	.clock_src	= MNSCx_CLOCK_SRC_IOCLK,
	.options	= 0,
#ifdef CONFIG_GDBSTUB_ON_TTYSM0
	.gdbstub	= 1,
#endif
};
#endif 

#ifdef CONFIG_MN10300_TTYSM1
struct mn10300_serial_port mn10300_serial_port_sif1 = {
	.uart.ops	= &mn10300_serial_ops,
	.uart.membase	= (void __iomem *) &SC1CTR,
	.uart.mapbase	= (unsigned long) &SC1CTR,
	.uart.iotype	= UPIO_MEM,
	.uart.irq	= 0,
	.uart.uartclk	= 0, 
	.uart.fifosize	= 1,
	.uart.flags	= UPF_BOOT_AUTOCONF,
	.uart.line	= 1,
	.uart.type	= PORT_MN10300,
	.uart.lock	=
	__SPIN_LOCK_UNLOCKED(mn10300_serial_port_sif1.uart.lock),
	.name		= "ttySM1",
	._iobase	= &SC1CTR,
	._control	= &SC1CTR,
	._status	= (volatile u8 *)&SC1STR,
	._intr		= &SC1ICR,
	._rxb		= &SC1RXB,
	._txb		= &SC1TXB,
	.rx_name	= "ttySM1:Rx",
	.tx_name	= "ttySM1:Tx",
#if defined(CONFIG_MN10300_TTYSM1_TIMER9)
	.tm_name	= "ttySM1:Timer9",
	._tmxmd		= &TM9MD,
	._tmxbr		= &TM9BR,
	._tmicr		= &TM9ICR,
	.tm_irq		= TM9IRQ,
	.div_timer	= MNSCx_DIV_TIMER_16BIT,
#elif defined(CONFIG_MN10300_TTYSM1_TIMER3)
	.tm_name	= "ttySM1:Timer3",
	._tmxmd		= &TM3MD,
	._tmxbr		= (volatile u16 *)&TM3BR,
	._tmicr		= &TM3ICR,
	.tm_irq		= TM3IRQ,
	.div_timer	= MNSCx_DIV_TIMER_8BIT,
#elif defined(CONFIG_MN10300_TTYSM1_TIMER12)
	.tm_name	= "ttySM1/Timer12",
	._tmxmd		= &TM12MD,
	._tmxbr		= &TM12BR,
	._tmicr		= &TM12ICR,
	.tm_irq		= TM12IRQ,
	.div_timer	= MNSCx_DIV_TIMER_16BIT,
#else
#error "Unknown config for ttySM1"
#endif
	.rx_irq		= SC1RXIRQ,
	.tx_irq		= SC1TXIRQ,
	.rx_icr		= &GxICR(SC1RXIRQ),
	.tx_icr		= &GxICR(SC1TXIRQ),
	.clock_src	= MNSCx_CLOCK_SRC_IOCLK,
	.options	= 0,
#ifdef CONFIG_GDBSTUB_ON_TTYSM1
	.gdbstub	= 1,
#endif
};
#endif 

#ifdef CONFIG_MN10300_TTYSM2
struct mn10300_serial_port mn10300_serial_port_sif2 = {
	.uart.ops	= &mn10300_serial_ops,
	.uart.membase	= (void __iomem *) &SC2CTR,
	.uart.mapbase	= (unsigned long) &SC2CTR,
	.uart.iotype	= UPIO_MEM,
	.uart.irq	= 0,
	.uart.uartclk	= 0, 
	.uart.fifosize	= 1,
	.uart.flags	= UPF_BOOT_AUTOCONF,
	.uart.line	= 2,
#ifdef CONFIG_MN10300_TTYSM2_CTS
	.uart.type	= PORT_MN10300_CTS,
#else
	.uart.type	= PORT_MN10300,
#endif
	.uart.lock	=
	__SPIN_LOCK_UNLOCKED(mn10300_serial_port_sif2.uart.lock),
	.name		= "ttySM2",
	._iobase	= &SC2CTR,
	._control	= &SC2CTR,
	._status	= (volatile u8 *)&SC2STR,
	._intr		= &SC2ICR,
	._rxb		= &SC2RXB,
	._txb		= &SC2TXB,
	.rx_name	= "ttySM2:Rx",
	.tx_name	= "ttySM2:Tx",
#if defined(CONFIG_MN10300_TTYSM2_TIMER10)
	.tm_name	= "ttySM2/Timer10",
	._tmxmd		= &TM10MD,
	._tmxbr		= &TM10BR,
	._tmicr		= &TM10ICR,
	.tm_irq		= TM10IRQ,
	.div_timer	= MNSCx_DIV_TIMER_16BIT,
#elif defined(CONFIG_MN10300_TTYSM2_TIMER9)
	.tm_name	= "ttySM2/Timer9",
	._tmxmd		= &TM9MD,
	._tmxbr		= &TM9BR,
	._tmicr		= &TM9ICR,
	.tm_irq		= TM9IRQ,
	.div_timer	= MNSCx_DIV_TIMER_16BIT,
#elif defined(CONFIG_MN10300_TTYSM2_TIMER1)
	.tm_name	= "ttySM2/Timer1",
	._tmxmd		= &TM1MD,
	._tmxbr		= (volatile u16 *)&TM1BR,
	._tmicr		= &TM1ICR,
	.tm_irq		= TM1IRQ,
	.div_timer	= MNSCx_DIV_TIMER_8BIT,
#elif defined(CONFIG_MN10300_TTYSM2_TIMER3)
	.tm_name	= "ttySM2/Timer3",
	._tmxmd		= &TM3MD,
	._tmxbr		= (volatile u16 *)&TM3BR,
	._tmicr		= &TM3ICR,
	.tm_irq		= TM3IRQ,
	.div_timer	= MNSCx_DIV_TIMER_8BIT,
#else
#error "Unknown config for ttySM2"
#endif
	.rx_irq		= SC2RXIRQ,
	.tx_irq		= SC2TXIRQ,
	.rx_icr		= &GxICR(SC2RXIRQ),
	.tx_icr		= &GxICR(SC2TXIRQ),
	.clock_src	= MNSCx_CLOCK_SRC_IOCLK,
#ifdef CONFIG_MN10300_TTYSM2_CTS
	.options	= MNSCx_OPT_CTS,
#else
	.options	= 0,
#endif
#ifdef CONFIG_GDBSTUB_ON_TTYSM2
	.gdbstub	= 1,
#endif
};
#endif 


struct mn10300_serial_port *mn10300_serial_ports[NR_UARTS + 1] = {
#ifdef CONFIG_MN10300_TTYSM0
	[0]	= &mn10300_serial_port_sif0,
#endif
#ifdef CONFIG_MN10300_TTYSM1
	[1]	= &mn10300_serial_port_sif1,
#endif
#ifdef CONFIG_MN10300_TTYSM2
	[2]	= &mn10300_serial_port_sif2,
#endif
	[NR_UARTS] = NULL,
};


static void mn10300_serial_mask_ack(unsigned int irq)
{
	unsigned long flags;
	u16 tmp;

	flags = arch_local_cli_save();
	GxICR(irq) = GxICR_LEVEL_6;
	tmp = GxICR(irq); 
	arch_local_irq_restore(flags);
}

static void mn10300_serial_chip_mask_ack(struct irq_data *d)
{
	mn10300_serial_mask_ack(d->irq);
}

static void mn10300_serial_nop(struct irq_data *d)
{
}

static struct irq_chip mn10300_serial_pic = {
	.name		= "mnserial",
	.irq_ack	= mn10300_serial_chip_mask_ack,
	.irq_mask	= mn10300_serial_chip_mask_ack,
	.irq_mask_ack	= mn10300_serial_chip_mask_ack,
	.irq_unmask	= mn10300_serial_nop,
};


struct mn10300_serial_int mn10300_serial_int_tbl[NR_IRQS];

static void mn10300_serial_dis_tx_intr(struct mn10300_serial_port *port)
{
	unsigned long flags;
	u16 x;

	flags = arch_local_cli_save();
	*port->tx_icr = NUM2GxICR_LEVEL(CONFIG_MN10300_SERIAL_IRQ_LEVEL);
	x = *port->tx_icr;
	arch_local_irq_restore(flags);
}

static void mn10300_serial_en_tx_intr(struct mn10300_serial_port *port)
{
	unsigned long flags;
	u16 x;

	flags = arch_local_cli_save();
	*port->tx_icr =
		NUM2GxICR_LEVEL(CONFIG_MN10300_SERIAL_IRQ_LEVEL) | GxICR_ENABLE;
	x = *port->tx_icr;
	arch_local_irq_restore(flags);
}

static void mn10300_serial_dis_rx_intr(struct mn10300_serial_port *port)
{
	unsigned long flags;
	u16 x;

	flags = arch_local_cli_save();
	*port->rx_icr = NUM2GxICR_LEVEL(CONFIG_MN10300_SERIAL_IRQ_LEVEL);
	x = *port->rx_icr;
	arch_local_irq_restore(flags);
}

static int mask_test_and_clear(volatile u8 *ptr, u8 mask)
{
	u32 epsw;
	asm volatile("	bclr	%1,(%2)		\n"
		     "	mov	epsw,%0		\n"
		     : "=d"(epsw) : "d"(mask), "a"(ptr)
		     : "cc", "memory");
	return !(epsw & EPSW_FLAG_Z);
}

static void mn10300_serial_receive_interrupt(struct mn10300_serial_port *port)
{
	struct uart_icount *icount = &port->uart.icount;
	struct tty_struct *tty = port->uart.state->port.tty;
	unsigned ix;
	int count;
	u8 st, ch, push, status, overrun;

	_enter("%s", port->name);

	push = 0;

	count = CIRC_CNT(port->rx_inp, port->rx_outp, MNSC_BUFFER_SIZE);
	count = tty_buffer_request_room(tty, count);
	if (count == 0) {
		if (!tty->low_latency)
			tty_flip_buffer_push(tty);
		return;
	}

try_again:
	
	ix = port->rx_outp;
	if (ix == port->rx_inp) {
		if (push && !tty->low_latency)
			tty_flip_buffer_push(tty);
		return;
	}

	ch = port->rx_buffer[ix++];
	st = port->rx_buffer[ix++];
	smp_rmb();
	port->rx_outp = ix & (MNSC_BUFFER_SIZE - 1);
	port->uart.icount.rx++;

	st &= SC01STR_FEF | SC01STR_PEF | SC01STR_OEF;
	status = 0;
	overrun = 0;

	switch (port->rx_brk) {
	case 0:
		
		break;

	case 1:
		if (st & SC01STR_FEF && ch == 0) {
			port->rx_brk = 2;
			goto try_again;
		}
		goto not_break;

	case 2:
		if (st & SC01STR_FEF && ch == 0) {
			port->rx_brk = 3;
			_proto("Rx Break Detected");
			icount->brk++;
			if (uart_handle_break(&port->uart))
				goto ignore_char;
			status |= 1 << TTY_BREAK;
			goto insert;
		}
		goto not_break;

	default:
		if (st & (SC01STR_FEF | SC01STR_PEF | SC01STR_OEF))
			goto try_again; 

		port->rx_brk = 0; 

		switch (ch) {
		case 0xFF:
		case 0xFE:
		case 0xFC:
		case 0xF8:
		case 0xF0:
		case 0xE0:
		case 0xC0:
		case 0x80:
		case 0x00:
			
			goto try_again;
		}
		break;
	}

process_errors:
	
	if (st & SC01STR_FEF) {
		if (ch == 0) {
			
			port->rx_brk = 1;
			goto try_again;
		}

		_proto("Rx Framing Error");
		icount->frame++;
		status |= 1 << TTY_FRAME;
	}

	
	if (st & SC01STR_PEF) {
		_proto("Rx Parity Error");
		icount->parity++;
		status = TTY_PARITY;
	}

	
	if (status == 0) {
		if (uart_handle_sysrq_char(&port->uart, ch))
			goto ignore_char;
		status = (1 << TTY_NORMAL);
	}

	
	if (st & SC01STR_OEF) {
		if (port->rx_brk)
			goto try_again;

		_proto("Rx Overrun Error");
		icount->overrun++;
		overrun = 1;
	}

insert:
	status &= port->uart.read_status_mask;

	if (!overrun && !(status & port->uart.ignore_status_mask)) {
		int flag;

		if (status & (1 << TTY_BREAK))
			flag = TTY_BREAK;
		else if (status & (1 << TTY_PARITY))
			flag = TTY_PARITY;
		else if (status & (1 << TTY_FRAME))
			flag = TTY_FRAME;
		else
			flag = TTY_NORMAL;

		tty_insert_flip_char(tty, ch, flag);
	}

	if (overrun)
		tty_insert_flip_char(tty, 0, TTY_OVERRUN);

	count--;
	if (count <= 0) {
		if (!tty->low_latency)
			tty_flip_buffer_push(tty);
		return;
	}

ignore_char:
	push = 1;
	goto try_again;

not_break:
	port->rx_brk = 0;
	goto process_errors;
}

static void mn10300_serial_transmit_interrupt(struct mn10300_serial_port *port)
{
	_enter("%s", port->name);

	if (!port->uart.state || !port->uart.state->port.tty) {
		mn10300_serial_dis_tx_intr(port);
		return;
	}

	if (uart_tx_stopped(&port->uart) ||
	    uart_circ_empty(&port->uart.state->xmit))
		mn10300_serial_dis_tx_intr(port);

	if (uart_circ_chars_pending(&port->uart.state->xmit) < WAKEUP_CHARS)
		uart_write_wakeup(&port->uart);
}

static void mn10300_serial_cts_changed(struct mn10300_serial_port *port, u8 st)
{
	u16 ctr;

	port->tx_cts = st;
	port->uart.icount.cts++;

	ctr = *port->_control;
	ctr ^= SC2CTR_TWS;
	*port->_control = ctr;

	uart_handle_cts_change(&port->uart, st & SC2STR_CTS);
	wake_up_interruptible(&port->uart.state->port.delta_msr_wait);
}

static irqreturn_t mn10300_serial_interrupt(int irq, void *dev_id)
{
	struct mn10300_serial_port *port = dev_id;
	u8 st;

	spin_lock(&port->uart.lock);

	if (port->intr_flags) {
		_debug("INT %s: %x", port->name, port->intr_flags);

		if (mask_test_and_clear(&port->intr_flags, MNSCx_RX_AVAIL))
			mn10300_serial_receive_interrupt(port);

		if (mask_test_and_clear(&port->intr_flags,
					MNSCx_TX_SPACE | MNSCx_TX_EMPTY))
			mn10300_serial_transmit_interrupt(port);
	}

	if (port->type == PORT_MN10300_CTS) {
		st = *port->_status;
		if ((port->tx_cts ^ st) & SC2STR_CTS)
			mn10300_serial_cts_changed(port, st);
	}

	spin_unlock(&port->uart.lock);

	return IRQ_HANDLED;
}

static unsigned int mn10300_serial_tx_empty(struct uart_port *_port)
{
	struct mn10300_serial_port *port =
		container_of(_port, struct mn10300_serial_port, uart);

	_enter("%s", port->name);

	return (*port->_status & (SC01STR_TXF | SC01STR_TBF)) ?
		0 : TIOCSER_TEMT;
}

static void mn10300_serial_set_mctrl(struct uart_port *_port,
				     unsigned int mctrl)
{
	struct mn10300_serial_port *port __attribute__ ((unused)) =
		container_of(_port, struct mn10300_serial_port, uart);

	_enter("%s,%x", port->name, mctrl);
}

static unsigned int mn10300_serial_get_mctrl(struct uart_port *_port)
{
	struct mn10300_serial_port *port =
		container_of(_port, struct mn10300_serial_port, uart);

	_enter("%s", port->name);

	if (port->type == PORT_MN10300_CTS && !(*port->_status & SC2STR_CTS))
		return TIOCM_CAR | TIOCM_DSR;

	return TIOCM_CAR | TIOCM_CTS | TIOCM_DSR;
}

static void mn10300_serial_stop_tx(struct uart_port *_port)
{
	struct mn10300_serial_port *port =
		container_of(_port, struct mn10300_serial_port, uart);

	_enter("%s", port->name);

	
	mn10300_serial_dis_tx_intr(port);
}

static void mn10300_serial_start_tx(struct uart_port *_port)
{
	struct mn10300_serial_port *port =
		container_of(_port, struct mn10300_serial_port, uart);

	u16 x;

	_enter("%s{%lu}",
	       port->name,
	       CIRC_CNT(&port->uart.state->xmit.head,
			&port->uart.state->xmit.tail,
			UART_XMIT_SIZE));

	
	arch_local_cli();
	x = *port->tx_icr;
	x |= GxICR_ENABLE;

	if (*port->_status & SC01STR_TBF)
		x &= ~(GxICR_REQUEST | GxICR_DETECT);
	else
		x |= GxICR_REQUEST | GxICR_DETECT;

	_debug("CTR=%04hx ICR=%02hx STR=%04x TMD=%02hx TBR=%04hx ICR=%04hx",
	       *port->_control, *port->_intr, *port->_status,
	       *port->_tmxmd,
	       (port->div_timer == MNSCx_DIV_TIMER_8BIT) ?
	           *(volatile u8 *)port->_tmxbr : *port->_tmxbr,
	       *port->tx_icr);

	*port->tx_icr = x;
	x = *port->tx_icr;
	arch_local_sti();
}

static void mn10300_serial_send_xchar(struct uart_port *_port, char ch)
{
	struct mn10300_serial_port *port =
		container_of(_port, struct mn10300_serial_port, uart);

	_enter("%s,%02x", port->name, ch);

	if (likely(port->gdbstub)) {
		port->tx_xchar = ch;
		if (ch)
			mn10300_serial_en_tx_intr(port);
	}
}

static void mn10300_serial_stop_rx(struct uart_port *_port)
{
	struct mn10300_serial_port *port =
		container_of(_port, struct mn10300_serial_port, uart);

	u16 ctr;

	_enter("%s", port->name);

	ctr = *port->_control;
	ctr &= ~SC01CTR_RXE;
	*port->_control = ctr;

	mn10300_serial_dis_rx_intr(port);
}

static void mn10300_serial_enable_ms(struct uart_port *_port)
{
	struct mn10300_serial_port *port =
		container_of(_port, struct mn10300_serial_port, uart);

	u16 ctr, cts;

	_enter("%s", port->name);

	if (port->type == PORT_MN10300_CTS) {
		port->tx_cts = *port->_status;

		cts = (port->tx_cts & SC2STR_CTS) ?
			SC2CTR_TWE : SC2CTR_TWE | SC2CTR_TWS;

		ctr = *port->_control;
		ctr &= ~SC2CTR_TWS;
		ctr |= cts;
		*port->_control = ctr;

		mn10300_serial_en_tx_intr(port);
	}
}

static void mn10300_serial_break_ctl(struct uart_port *_port, int ctl)
{
	struct mn10300_serial_port *port =
		container_of(_port, struct mn10300_serial_port, uart);

	_enter("%s,%d", port->name, ctl);

	if (ctl) {
		
		port->tx_break = 1;
		mn10300_serial_en_tx_intr(port);
	} else {
		port->tx_break = 0;
		*port->_control &= ~SC01CTR_BKE;
		mn10300_serial_en_tx_intr(port);
	}
}

static int mn10300_serial_startup(struct uart_port *_port)
{
	struct mn10300_serial_port *port =
		container_of(_port, struct mn10300_serial_port, uart);
	struct mn10300_serial_int *pint;

	_enter("%s{%d}", port->name, port->gdbstub);

	if (unlikely(port->gdbstub))
		return -EBUSY;

	
	port->rx_buffer = kmalloc(MNSC_BUFFER_SIZE, GFP_KERNEL);
	if (!port->rx_buffer)
		return -ENOMEM;

	port->rx_inp = port->rx_outp = 0;

	
	*port->_intr = SC01ICR_TI;
	*port->_control |= SC01CTR_TXE | SC01CTR_RXE;

	pint = &mn10300_serial_int_tbl[port->rx_irq];
	pint->port = port;
	pint->vdma = mn10300_serial_vdma_rx_handler;
	pint = &mn10300_serial_int_tbl[port->tx_irq];
	pint->port = port;
	pint->vdma = mn10300_serial_vdma_tx_handler;

	set_intr_level(port->rx_irq,
		NUM2GxICR_LEVEL(CONFIG_MN10300_SERIAL_IRQ_LEVEL));
	set_intr_level(port->tx_irq,
		NUM2GxICR_LEVEL(CONFIG_MN10300_SERIAL_IRQ_LEVEL));
	irq_set_chip(port->tm_irq, &mn10300_serial_pic);

	if (request_irq(port->rx_irq, mn10300_serial_interrupt,
			IRQF_DISABLED, port->rx_name, port) < 0)
		goto error;

	if (request_irq(port->tx_irq, mn10300_serial_interrupt,
			IRQF_DISABLED, port->tx_name, port) < 0)
		goto error2;

	if (request_irq(port->tm_irq, mn10300_serial_interrupt,
			IRQF_DISABLED, port->tm_name, port) < 0)
		goto error3;
	mn10300_serial_mask_ack(port->tm_irq);

	return 0;

error3:
	free_irq(port->tx_irq, port);
error2:
	free_irq(port->rx_irq, port);
error:
	kfree(port->rx_buffer);
	port->rx_buffer = NULL;
	return -EBUSY;
}

static void mn10300_serial_shutdown(struct uart_port *_port)
{
	u16 x;
	struct mn10300_serial_port *port =
		container_of(_port, struct mn10300_serial_port, uart);

	_enter("%s", port->name);

	
	port->tx_break = 0;
	*port->_control &= ~(SC01CTR_TXE | SC01CTR_RXE | SC01CTR_BKE);
	*port->_tmxmd = 0;

	if (port->rx_buffer) {
		void *buf = port->rx_buffer;
		port->rx_buffer = NULL;
		kfree(buf);
	}

	
	free_irq(port->tm_irq, port);
	free_irq(port->rx_irq, port);
	free_irq(port->tx_irq, port);

	arch_local_cli();
	*port->rx_icr = NUM2GxICR_LEVEL(CONFIG_MN10300_SERIAL_IRQ_LEVEL);
	x = *port->rx_icr;
	*port->tx_icr = NUM2GxICR_LEVEL(CONFIG_MN10300_SERIAL_IRQ_LEVEL);
	x = *port->tx_icr;
	arch_local_sti();
}

static void mn10300_serial_change_speed(struct mn10300_serial_port *port,
					  struct ktermios *new,
					  struct ktermios *old)
{
	unsigned long flags;
	unsigned long ioclk = port->ioclk;
	unsigned cflag;
	int baud, bits, xdiv, tmp;
	u16 tmxbr, scxctr;
	u8 tmxmd, battempt;
	u8 div_timer = port->div_timer;

	_enter("%s{%lu}", port->name, ioclk);

	
	cflag = new->c_cflag;
	switch (cflag & CSIZE) {
	case CS7: scxctr = SC01CTR_CLN_7BIT; bits = 9;  break;
	case CS8: scxctr = SC01CTR_CLN_8BIT; bits = 10; break;
	default:  scxctr = SC01CTR_CLN_8BIT; bits = 10; break;
	}

	if (cflag & CSTOPB) {
		scxctr |= SC01CTR_STB_2BIT;
		bits++;
	}

	if (cflag & PARENB) {
		bits++;
		if (cflag & PARODD)
			scxctr |= SC01CTR_PB_ODD;
#ifdef CMSPAR
		else if (cflag & CMSPAR)
			scxctr |= SC01CTR_PB_FIXED0;
#endif
		else
			scxctr |= SC01CTR_PB_EVEN;
	}

	
	battempt = 0;

	switch (port->uart.line) {
#ifdef CONFIG_MN10300_TTYSM0
	case 0: 
#if   defined(CONFIG_MN10300_TTYSM0_TIMER8)
		scxctr |= SC0CTR_CK_TM8UFLOW_8;
#elif defined(CONFIG_MN10300_TTYSM0_TIMER0)
		scxctr |= SC0CTR_CK_TM0UFLOW_8;
#elif defined(CONFIG_MN10300_TTYSM0_TIMER2)
		scxctr |= SC0CTR_CK_TM2UFLOW_8;
#else
#error "Unknown config for ttySM0"
#endif
		break;
#endif 

#ifdef CONFIG_MN10300_TTYSM1
	case 1: 
#if defined(CONFIG_AM33_2) || defined(CONFIG_AM33_3)
#if   defined(CONFIG_MN10300_TTYSM1_TIMER9)
		scxctr |= SC1CTR_CK_TM9UFLOW_8;
#elif defined(CONFIG_MN10300_TTYSM1_TIMER3)
		scxctr |= SC1CTR_CK_TM3UFLOW_8;
#else
#error "Unknown config for ttySM1"
#endif
#else 
#if defined(CONFIG_MN10300_TTYSM1_TIMER12)
		scxctr |= SC1CTR_CK_TM12UFLOW_8;
#else
#error "Unknown config for ttySM1"
#endif
#endif 
		break;
#endif 

#ifdef CONFIG_MN10300_TTYSM2
	case 2: 
#if defined(CONFIG_AM33_2)
#if   defined(CONFIG_MN10300_TTYSM2_TIMER10)
		scxctr |= SC2CTR_CK_TM10UFLOW;
#else
#error "Unknown config for ttySM2"
#endif
#else 
#if   defined(CONFIG_MN10300_TTYSM2_TIMER9)
		scxctr |= SC2CTR_CK_TM9UFLOW_8;
#elif defined(CONFIG_MN10300_TTYSM2_TIMER1)
		scxctr |= SC2CTR_CK_TM1UFLOW_8;
#elif defined(CONFIG_MN10300_TTYSM2_TIMER3)
		scxctr |= SC2CTR_CK_TM3UFLOW_8;
#else
#error "Unknown config for ttySM2"
#endif
#endif 
		break;
#endif 

	default:
		break;
	}

try_alternative:
	baud = uart_get_baud_rate(&port->uart, new, old, 0,
				  port->ioclk / 8);

	_debug("ALT %d [baud %d]", battempt, baud);

	if (!baud)
		baud = 9600;	
	xdiv = 1;
	if (baud == 134) {
		baud = 269;	
		xdiv = 2;
	}

	if (baud == 38400 &&
	    (port->uart.flags & UPF_SPD_MASK) == UPF_SPD_CUST
	    ) {
		_debug("CUSTOM %u", port->uart.custom_divisor);

		if (div_timer == MNSCx_DIV_TIMER_16BIT) {
			if (port->uart.custom_divisor <= 65535) {
				tmxmd = TM8MD_SRC_IOCLK;
				tmxbr = port->uart.custom_divisor;
				port->uart.uartclk = ioclk;
				goto timer_okay;
			}
			if (port->uart.custom_divisor / 8 <= 65535) {
				tmxmd = TM8MD_SRC_IOCLK_8;
				tmxbr = port->uart.custom_divisor / 8;
				port->uart.custom_divisor = tmxbr * 8;
				port->uart.uartclk = ioclk / 8;
				goto timer_okay;
			}
			if (port->uart.custom_divisor / 32 <= 65535) {
				tmxmd = TM8MD_SRC_IOCLK_32;
				tmxbr = port->uart.custom_divisor / 32;
				port->uart.custom_divisor = tmxbr * 32;
				port->uart.uartclk = ioclk / 32;
				goto timer_okay;
			}

		} else if (div_timer == MNSCx_DIV_TIMER_8BIT) {
			if (port->uart.custom_divisor <= 255) {
				tmxmd = TM2MD_SRC_IOCLK;
				tmxbr = port->uart.custom_divisor;
				port->uart.uartclk = ioclk;
				goto timer_okay;
			}
			if (port->uart.custom_divisor / 8 <= 255) {
				tmxmd = TM2MD_SRC_IOCLK_8;
				tmxbr = port->uart.custom_divisor / 8;
				port->uart.custom_divisor = tmxbr * 8;
				port->uart.uartclk = ioclk / 8;
				goto timer_okay;
			}
			if (port->uart.custom_divisor / 32 <= 255) {
				tmxmd = TM2MD_SRC_IOCLK_32;
				tmxbr = port->uart.custom_divisor / 32;
				port->uart.custom_divisor = tmxbr * 32;
				port->uart.uartclk = ioclk / 32;
				goto timer_okay;
			}
		}
	}

	switch (div_timer) {
	case MNSCx_DIV_TIMER_16BIT:
		port->uart.uartclk = ioclk;
		tmxmd = TM8MD_SRC_IOCLK;
		tmxbr = tmp = (ioclk / (baud * xdiv) + 4) / 8 - 1;
		if (tmp > 0 && tmp <= 65535)
			goto timer_okay;

		port->uart.uartclk = ioclk / 8;
		tmxmd = TM8MD_SRC_IOCLK_8;
		tmxbr = tmp = (ioclk / (baud * 8 * xdiv) + 4) / 8 - 1;
		if (tmp > 0 && tmp <= 65535)
			goto timer_okay;

		port->uart.uartclk = ioclk / 32;
		tmxmd = TM8MD_SRC_IOCLK_32;
		tmxbr = tmp = (ioclk / (baud * 32 * xdiv) + 4) / 8 - 1;
		if (tmp > 0 && tmp <= 65535)
			goto timer_okay;
		break;

	case MNSCx_DIV_TIMER_8BIT:
		port->uart.uartclk = ioclk;
		tmxmd = TM2MD_SRC_IOCLK;
		tmxbr = tmp = (ioclk / (baud * xdiv) + 4) / 8 - 1;
		if (tmp > 0 && tmp <= 255)
			goto timer_okay;

		port->uart.uartclk = ioclk / 8;
		tmxmd = TM2MD_SRC_IOCLK_8;
		tmxbr = tmp = (ioclk / (baud * 8 * xdiv) + 4) / 8 - 1;
		if (tmp > 0 && tmp <= 255)
			goto timer_okay;

		port->uart.uartclk = ioclk / 32;
		tmxmd = TM2MD_SRC_IOCLK_32;
		tmxbr = tmp = (ioclk / (baud * 32 * xdiv) + 4) / 8 - 1;
		if (tmp > 0 && tmp <= 255)
			goto timer_okay;
		break;

	default:
		BUG();
		return;
	}

	
	_debug("CAN'T SUPPORT");

	switch (battempt) {
	case 0:
		if (old) {
			new->c_cflag &= ~CBAUD;
			new->c_cflag |= (old->c_cflag & CBAUD);
			battempt = 1;
			goto try_alternative;
		}

	case 1:
		new->c_cflag &= ~CBAUD;
		new->c_cflag |= B9600;
		battempt = 2;
		goto try_alternative;

	default:
		new->c_cflag &= ~CBAUD;
		new->c_cflag |= B0;

		if (div_timer == MNSCx_DIV_TIMER_16BIT)
			tmxmd = TM8MD_SRC_IOCLK_32;
		else if (div_timer == MNSCx_DIV_TIMER_8BIT)
			tmxmd = TM2MD_SRC_IOCLK_32;
		tmxbr = 1;

		port->uart.uartclk = ioclk / 32;
		break;
	}
timer_okay:

	_debug("UARTCLK: %u / %hu", port->uart.uartclk, tmxbr);

	
	spin_lock_irqsave(&port->uart.lock, flags);

	uart_update_timeout(&port->uart, new->c_cflag, baud);

	
	switch (div_timer) {
	case MNSCx_DIV_TIMER_16BIT:
		*port->_tmxmd = 0;
		*port->_tmxbr = tmxbr;
		*port->_tmxmd = TM8MD_INIT_COUNTER;
		*port->_tmxmd = tmxmd | TM8MD_COUNT_ENABLE;
		break;

	case MNSCx_DIV_TIMER_8BIT:
		*port->_tmxmd = 0;
		*(volatile u8 *) port->_tmxbr = (u8) tmxbr;
		*port->_tmxmd = TM2MD_INIT_COUNTER;
		*port->_tmxmd = tmxmd | TM2MD_COUNT_ENABLE;
		break;
	}

	
	scxctr &= ~(SC2CTR_TWE | SC2CTR_TWS);

	if (port->type == PORT_MN10300_CTS && cflag & CRTSCTS) {
		port->tx_cts = *port->_status;

		if (port->tx_cts & SC2STR_CTS)
			scxctr |= SC2CTR_TWE;
		else
			scxctr |= SC2CTR_TWE | SC2CTR_TWS;
	}

	
	port->uart.read_status_mask = (1 << TTY_NORMAL) | (1 << TTY_OVERRUN);
	if (new->c_iflag & INPCK)
		port->uart.read_status_mask |=
			(1 << TTY_PARITY) | (1 << TTY_FRAME);
	if (new->c_iflag & (BRKINT | PARMRK))
		port->uart.read_status_mask |= (1 << TTY_BREAK);

	
	port->uart.ignore_status_mask = 0;
	if (new->c_iflag & IGNPAR)
		port->uart.ignore_status_mask |=
			(1 << TTY_PARITY) | (1 << TTY_FRAME);
	if (new->c_iflag & IGNBRK) {
		port->uart.ignore_status_mask |= (1 << TTY_BREAK);
		if (new->c_iflag & IGNPAR)
			port->uart.ignore_status_mask |= (1 << TTY_OVERRUN);
	}

	
	if ((new->c_cflag & CREAD) == 0)
		port->uart.ignore_status_mask |= (1 << TTY_NORMAL);

	scxctr |= *port->_control & (SC01CTR_TXE | SC01CTR_RXE | SC01CTR_BKE);
	*port->_control = scxctr;

	spin_unlock_irqrestore(&port->uart.lock, flags);
}

static void mn10300_serial_set_termios(struct uart_port *_port,
					 struct ktermios *new,
					 struct ktermios *old)
{
	struct mn10300_serial_port *port =
		container_of(_port, struct mn10300_serial_port, uart);

	_enter("%s,%p,%p", port->name, new, old);

	mn10300_serial_change_speed(port, new, old);

	
	if (!(new->c_cflag & CRTSCTS)) {
		u16 ctr = *port->_control;
		ctr &= ~SC2CTR_TWE;
		*port->_control = ctr;
	}

	
	if (new->c_cflag & CODMSB)
		*port->_control |= SC01CTR_OD_MSBFIRST; 
	else
		*port->_control &= ~SC01CTR_OD_MSBFIRST; 
}

static const char *mn10300_serial_type(struct uart_port *_port)
{
	struct mn10300_serial_port *port =
		container_of(_port, struct mn10300_serial_port, uart);

	if (port->uart.type == PORT_MN10300_CTS)
		return "MN10300 SIF_CTS";

	return "MN10300 SIF";
}

static void mn10300_serial_release_port(struct uart_port *_port)
{
	struct mn10300_serial_port *port =
		container_of(_port, struct mn10300_serial_port, uart);

	_enter("%s", port->name);

	release_mem_region((unsigned long) port->_iobase, 16);
}

static int mn10300_serial_request_port(struct uart_port *_port)
{
	struct mn10300_serial_port *port =
		container_of(_port, struct mn10300_serial_port, uart);

	_enter("%s", port->name);

	request_mem_region((unsigned long) port->_iobase, 16, port->name);
	return 0;
}

static void mn10300_serial_config_port(struct uart_port *_port, int type)
{
	struct mn10300_serial_port *port =
		container_of(_port, struct mn10300_serial_port, uart);

	_enter("%s", port->name);

	port->uart.type = PORT_MN10300;

	if (port->options & MNSCx_OPT_CTS)
		port->uart.type = PORT_MN10300_CTS;

	mn10300_serial_request_port(_port);
}

static int mn10300_serial_verify_port(struct uart_port *_port,
					struct serial_struct *ss)
{
	struct mn10300_serial_port *port =
		container_of(_port, struct mn10300_serial_port, uart);
	void *mapbase = (void *) (unsigned long) port->uart.mapbase;

	_enter("%s", port->name);

	
	if (ss->irq		!= port->uart.irq	||
	    ss->port		!= port->uart.iobase	||
	    ss->io_type		!= port->uart.iotype	||
	    ss->iomem_base	!= mapbase ||
	    ss->iomem_reg_shift	!= port->uart.regshift	||
	    ss->hub6		!= port->uart.hub6	||
	    ss->xmit_fifo_size	!= port->uart.fifosize)
		return -EINVAL;

	
	if (ss->type != port->uart.type) {
		if (!(port->options & MNSCx_OPT_CTS))
			return -EINVAL;

		if (ss->type != PORT_MN10300 &&
		    ss->type != PORT_MN10300_CTS)
			return -EINVAL;
	}

	return 0;
}

static int __init mn10300_serial_init(void)
{
	struct mn10300_serial_port *port;
	int ret, i;

	printk(KERN_INFO "%s version %s (%s)\n",
	       serial_name, serial_version, serial_revdate);

#if defined(CONFIG_MN10300_TTYSM2) && defined(CONFIG_AM33_2)
	{
		int tmp;
		SC2TIM = 8; 
		tmp = SC2TIM;
	}
#endif

	set_intr_stub(NUM2EXCEP_IRQ_LEVEL(CONFIG_MN10300_SERIAL_IRQ_LEVEL),
		mn10300_serial_vdma_interrupt);

	ret = uart_register_driver(&mn10300_serial_driver);
	if (!ret) {
		for (i = 0 ; i < NR_PORTS ; i++) {
			port = mn10300_serial_ports[i];
			if (!port || port->gdbstub)
				continue;

			switch (port->clock_src) {
			case MNSCx_CLOCK_SRC_IOCLK:
				port->ioclk = MN10300_IOCLK;
				break;

#ifdef MN10300_IOBCLK
			case MNSCx_CLOCK_SRC_IOBCLK:
				port->ioclk = MN10300_IOBCLK;
				break;
#endif
			default:
				BUG();
			}

			ret = uart_add_one_port(&mn10300_serial_driver,
						&port->uart);

			if (ret < 0) {
				_debug("ERROR %d", -ret);
				break;
			}
		}

		if (ret)
			uart_unregister_driver(&mn10300_serial_driver);
	}

	return ret;
}

__initcall(mn10300_serial_init);


#ifdef CONFIG_MN10300_TTYSM_CONSOLE

static void mn10300_serial_console_write(struct console *co,
					   const char *s, unsigned count)
{
	struct mn10300_serial_port *port;
	unsigned i;
	u16 scxctr, txicr, tmp;
	u8 tmxmd;

	port = mn10300_serial_ports[co->index];

	
	arch_local_cli();
	txicr = *port->tx_icr;
	*port->tx_icr = NUM2GxICR_LEVEL(CONFIG_MN10300_SERIAL_IRQ_LEVEL);
	tmp = *port->tx_icr;
	arch_local_sti();

	
	scxctr = *port->_control;
	if (!(scxctr & SC01CTR_TXE)) {
		
		tmxmd = *port->_tmxmd;

		switch (port->div_timer) {
		case MNSCx_DIV_TIMER_16BIT:
			*port->_tmxmd = 0;
			*port->_tmxmd = TM8MD_INIT_COUNTER;
			*port->_tmxmd = tmxmd | TM8MD_COUNT_ENABLE;
			break;

		case MNSCx_DIV_TIMER_8BIT:
			*port->_tmxmd = 0;
			*port->_tmxmd = TM2MD_INIT_COUNTER;
			*port->_tmxmd = tmxmd | TM2MD_COUNT_ENABLE;
			break;
		}

		
		*port->_control = (scxctr & ~SC01CTR_BKE) | SC01CTR_TXE;

	} else if (scxctr & SC01CTR_BKE) {
		
		*port->_control = (scxctr & ~SC01CTR_BKE);
	}

	
	for (i = 0; i < count; i++) {
		char ch = *s++;

		while (*port->_status & SC01STR_TBF)
			continue;
		*(u8 *) port->_txb = ch;

		if (ch == 0x0a) {
			while (*port->_status & SC01STR_TBF)
				continue;
			*(u8 *) port->_txb = 0xd;
		}
	}

	while (*port->_status & (SC01STR_TXF | SC01STR_TBF))
		continue;

	
	if (!(scxctr & SC01CTR_TXE))
		*port->_control = scxctr;

	arch_local_cli();
	*port->tx_icr = txicr;
	tmp = *port->tx_icr;
	arch_local_sti();
}

static int __init mn10300_serial_console_setup(struct console *co,
						 char *options)
{
	struct mn10300_serial_port *port;
	int i, parity = 'n', baud = 9600, bits = 8, flow = 0;

	for (i = 0 ; i < NR_PORTS ; i++) {
		port = mn10300_serial_ports[i];
		if (port && !port->gdbstub && port->uart.line == co->index)
			goto found_device;
	}

	return -ENODEV;

found_device:
	switch (port->clock_src) {
	case MNSCx_CLOCK_SRC_IOCLK:
		port->ioclk = MN10300_IOCLK;
		break;

#ifdef MN10300_IOBCLK
	case MNSCx_CLOCK_SRC_IOBCLK:
		port->ioclk = MN10300_IOBCLK;
		break;
#endif
	default:
		BUG();
	}

	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);

	return uart_set_options(&port->uart, co, baud, parity, bits, flow);
}

static int __init mn10300_serial_console_init(void)
{
	register_console(&mn10300_serial_console);
	return 0;
}

console_initcall(mn10300_serial_console_init);
#endif

#ifdef CONFIG_CONSOLE_POLL
static int mn10300_serial_poll_get_char(struct uart_port *_port)
{
	struct mn10300_serial_port *port =
		container_of(_port, struct mn10300_serial_port, uart);
	unsigned ix;
	u8 st, ch;

	_enter("%s", port->name);

	do {
		
		ix = port->rx_outp;
		if (ix == port->rx_inp)
			return NO_POLL_CHAR;

		ch = port->rx_buffer[ix++];
		st = port->rx_buffer[ix++];
		smp_rmb();
		port->rx_outp = ix & (MNSC_BUFFER_SIZE - 1);

	} while (st & (SC01STR_FEF | SC01STR_PEF | SC01STR_OEF));

	return ch;
}


static void mn10300_serial_poll_put_char(struct uart_port *_port,
					 unsigned char ch)
{
	struct mn10300_serial_port *port =
		container_of(_port, struct mn10300_serial_port, uart);
	u8 intr, tmp;

	while (*port->_status & (SC01STR_TBF | SC01STR_TXF))
		continue;

	
	intr = *port->_intr;
	*port->_intr = intr & ~SC01ICR_TI;
	tmp = *port->_intr;

	if (ch == 0x0a) {
		*(u8 *) port->_txb = 0x0d;
		while (*port->_status & SC01STR_TBF)
			continue;
	}

	*(u8 *) port->_txb = ch;
	while (*port->_status & SC01STR_TBF)
		continue;

	
	*port->_intr = intr;
	tmp = *port->_intr;
}

#endif 
