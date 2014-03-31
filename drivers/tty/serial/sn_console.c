/*
 * C-Brick Serial Port (and console) driver for SGI Altix machines.
 *
 * This driver is NOT suitable for talking to the l1-controller for
 * anything other than 'console activities' --- please use the l1
 * driver for that.
 *
 *
 * Copyright (c) 2004-2006 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * Contact information:  Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA  94043, or:
 *
 * http://www.sgi.com
 *
 * For further information regarding this notice, see:
 *
 * http://oss.sgi.com/projects/GenInfo/NoticeExplan
 */

#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/console.h>
#include <linux/module.h>
#include <linux/sysrq.h>
#include <linux/circ_buf.h>
#include <linux/serial_reg.h>
#include <linux/delay.h> 
#include <linux/miscdevice.h>
#include <linux/serial_core.h>

#include <asm/io.h>
#include <asm/sn/simulator.h>
#include <asm/sn/sn_sal.h>

#define SN_SAL_MAX_CHARS 120

#define SN_SAL_BUFFER_SIZE (64 * (1 << 10))

#define SN_SAL_UART_FIFO_DEPTH 16
#define SN_SAL_UART_FIFO_SPEED_CPS (9600/10)

#define TRANSMIT_BUFFERED	0
#define TRANSMIT_RAW		1

				  
#define USE_DYNAMIC_MINOR 0	

#define DEVICE_NAME "ttySG"
#define DEVICE_NAME_DYNAMIC "ttySG0"	
#define DEVICE_MAJOR 204
#define DEVICE_MINOR 40

#ifdef CONFIG_MAGIC_SYSRQ
static char sysrq_serial_str[] = "\eSYS";
static char *sysrq_serial_ptr = sysrq_serial_str;
static unsigned long sysrq_requested;
#endif 

struct sn_cons_port {
	struct timer_list sc_timer;
	struct uart_port sc_port;
	struct sn_sal_ops {
		int (*sal_puts_raw) (const char *s, int len);
		int (*sal_puts) (const char *s, int len);
		int (*sal_getc) (void);
		int (*sal_input_pending) (void);
		void (*sal_wakeup_transmit) (struct sn_cons_port *, int);
	} *sc_ops;
	unsigned long sc_interrupt_timeout;
	int sc_is_asynch;
};

static struct sn_cons_port sal_console_port;
static int sn_process_input;

static struct miscdevice misc;	

extern void early_sn_setup(void);

#undef DEBUG
#ifdef DEBUG
static int sn_debug_printf(const char *fmt, ...);
#define DPRINTF(x...) sn_debug_printf(x)
#else
#define DPRINTF(x...) do { } while (0)
#endif

static int snt_hw_puts_raw(const char *, int);
static int snt_hw_puts_buffered(const char *, int);
static int snt_poll_getc(void);
static int snt_poll_input_pending(void);
static int snt_intr_getc(void);
static int snt_intr_input_pending(void);
static void sn_transmit_chars(struct sn_cons_port *, int);

static struct sn_sal_ops poll_ops = {
	.sal_puts_raw = snt_hw_puts_raw,
	.sal_puts = snt_hw_puts_raw,
	.sal_getc = snt_poll_getc,
	.sal_input_pending = snt_poll_input_pending
};

static struct sn_sal_ops intr_ops = {
	.sal_puts_raw = snt_hw_puts_raw,
	.sal_puts = snt_hw_puts_buffered,
	.sal_getc = snt_intr_getc,
	.sal_input_pending = snt_intr_input_pending,
	.sal_wakeup_transmit = sn_transmit_chars
};

/* the console does output in two distinctly different ways:
 * synchronous (raw) and asynchronous (buffered).  initially, early_printk
 * does synchronous output.  any data written goes directly to the SAL
 * to be output (incidentally, it is internally buffered by the SAL)
 * after interrupts and timers are initialized and available for use,
 * the console init code switches to asynchronous output.  this is
 * also the earliest opportunity to begin polling for console input.
 * after console initialization, console output and tty (serial port)
 * output is buffered and sent to the SAL asynchronously (either by
 * timer callback or by UART interrupt) */


static int snt_poll_getc(void)
{
	int ch;

	ia64_sn_console_getc(&ch);
	return ch;
}

static int snt_poll_input_pending(void)
{
	int status, input;

	status = ia64_sn_console_check(&input);
	return !status && input;
}


static int snt_intr_getc(void)
{
	return ia64_sn_console_readc();
}

static int snt_intr_input_pending(void)
{
	return ia64_sn_console_intr_status() & SAL_CONSOLE_INTR_RECV;
}


static int snt_hw_puts_raw(const char *s, int len)
{
	
	return ia64_sn_console_putb(s, len);
}

static int snt_hw_puts_buffered(const char *s, int len)
{
	
	return ia64_sn_console_xmit_chars((char *)s, len);
}


static const char *snp_type(struct uart_port *port)
{
	return ("SGI SN L1");
}

static unsigned int snp_tx_empty(struct uart_port *port)
{
	return 1;
}

static void snp_stop_tx(struct uart_port *port)
{
}

static void snp_release_port(struct uart_port *port)
{
}

static void snp_enable_ms(struct uart_port *port)
{
}

static void snp_shutdown(struct uart_port *port)
{
}

static void snp_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
}

static unsigned int snp_get_mctrl(struct uart_port *port)
{
	return TIOCM_CAR | TIOCM_RNG | TIOCM_DSR | TIOCM_CTS;
}

static void snp_stop_rx(struct uart_port *port)
{
}

static void snp_start_tx(struct uart_port *port)
{
	if (sal_console_port.sc_ops->sal_wakeup_transmit)
		sal_console_port.sc_ops->sal_wakeup_transmit(&sal_console_port,
							     TRANSMIT_BUFFERED);

}

static void snp_break_ctl(struct uart_port *port, int break_state)
{
}

static int snp_startup(struct uart_port *port)
{
	return 0;
}

static void
snp_set_termios(struct uart_port *port, struct ktermios *termios,
		struct ktermios *old)
{
}

static int snp_request_port(struct uart_port *port)
{
	return 0;
}

static void snp_config_port(struct uart_port *port, int flags)
{
}


static struct uart_ops sn_console_ops = {
	.tx_empty = snp_tx_empty,
	.set_mctrl = snp_set_mctrl,
	.get_mctrl = snp_get_mctrl,
	.stop_tx = snp_stop_tx,
	.start_tx = snp_start_tx,
	.stop_rx = snp_stop_rx,
	.enable_ms = snp_enable_ms,
	.break_ctl = snp_break_ctl,
	.startup = snp_startup,
	.shutdown = snp_shutdown,
	.set_termios = snp_set_termios,
	.pm = NULL,
	.type = snp_type,
	.release_port = snp_release_port,
	.request_port = snp_request_port,
	.config_port = snp_config_port,
	.verify_port = NULL,
};


#ifdef DEBUG

static int sn_debug_printf(const char *fmt, ...)
{
	static char printk_buf[1024];
	int printed_len;
	va_list args;

	va_start(args, fmt);
	printed_len = vsnprintf(printk_buf, sizeof(printk_buf), fmt, args);

	if (!sal_console_port.sc_ops) {
		sal_console_port.sc_ops = &poll_ops;
		early_sn_setup();
	}
	sal_console_port.sc_ops->sal_puts_raw(printk_buf, printed_len);

	va_end(args);
	return printed_len;
}
#endif				


static void
sn_receive_chars(struct sn_cons_port *port, unsigned long flags)
{
	int ch;
	struct tty_struct *tty;

	if (!port) {
		printk(KERN_ERR "sn_receive_chars - port NULL so can't receive\n");
		return;
	}

	if (!port->sc_ops) {
		printk(KERN_ERR "sn_receive_chars - port->sc_ops  NULL so can't receive\n");
		return;
	}

	if (port->sc_port.state) {
		
		tty = port->sc_port.state->port.tty;
	}
	else {
		
		tty = NULL;
	}

	while (port->sc_ops->sal_input_pending()) {
		ch = port->sc_ops->sal_getc();
		if (ch < 0) {
			printk(KERN_ERR "sn_console: An error occurred while "
			       "obtaining data from the console (0x%0x)\n", ch);
			break;
		}
#ifdef CONFIG_MAGIC_SYSRQ
                if (sysrq_requested) {
                        unsigned long sysrq_timeout = sysrq_requested + HZ*5;

                        sysrq_requested = 0;
                        if (ch && time_before(jiffies, sysrq_timeout)) {
                                spin_unlock_irqrestore(&port->sc_port.lock, flags);
                                handle_sysrq(ch);
                                spin_lock_irqsave(&port->sc_port.lock, flags);
                                
                                continue;
                        }
                }
                if (ch == *sysrq_serial_ptr) {
                        if (!(*++sysrq_serial_ptr)) {
                                sysrq_requested = jiffies;
                                sysrq_serial_ptr = sysrq_serial_str;
                        }
			if (ch != '\e')
				continue;
                }
                else
			sysrq_serial_ptr = sysrq_serial_str;
#endif 

		
		if (tty) {
			if(tty_insert_flip_char(tty, ch, TTY_NORMAL) == 0)
				break;
		}
		port->sc_port.icount.rx++;
	}

	if (tty)
		tty_flip_buffer_push(tty);
}

static void sn_transmit_chars(struct sn_cons_port *port, int raw)
{
	int xmit_count, tail, head, loops, ii;
	int result;
	char *start;
	struct circ_buf *xmit;

	if (!port)
		return;

	BUG_ON(!port->sc_is_asynch);

	if (port->sc_port.state) {
		
		xmit = &port->sc_port.state->xmit;
	} else {
		return;
	}

	if (uart_circ_empty(xmit) || uart_tx_stopped(&port->sc_port)) {
		
		ia64_sn_console_intr_disable(SAL_CONSOLE_INTR_XMIT);
		return;
	}

	head = xmit->head;
	tail = xmit->tail;
	start = &xmit->buf[tail];

	loops = (head < tail) ? 2 : 1;

	for (ii = 0; ii < loops; ii++) {
		xmit_count = (head < tail) ?
		    (UART_XMIT_SIZE - tail) : (head - tail);

		if (xmit_count > 0) {
			if (raw == TRANSMIT_RAW)
				result =
				    port->sc_ops->sal_puts_raw(start,
							       xmit_count);
			else
				result =
				    port->sc_ops->sal_puts(start, xmit_count);
#ifdef DEBUG
			if (!result)
				DPRINTF("`");
#endif
			if (result > 0) {
				xmit_count -= result;
				port->sc_port.icount.tx += result;
				tail += result;
				tail &= UART_XMIT_SIZE - 1;
				xmit->tail = tail;
				start = &xmit->buf[tail];
			}
		}
	}

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(&port->sc_port);

	if (uart_circ_empty(xmit))
		snp_stop_tx(&port->sc_port);	
}

static irqreturn_t sn_sal_interrupt(int irq, void *dev_id)
{
	struct sn_cons_port *port = (struct sn_cons_port *)dev_id;
	unsigned long flags;
	int status = ia64_sn_console_intr_status();

	if (!port)
		return IRQ_NONE;

	spin_lock_irqsave(&port->sc_port.lock, flags);
	if (status & SAL_CONSOLE_INTR_RECV) {
		sn_receive_chars(port, flags);
	}
	if (status & SAL_CONSOLE_INTR_XMIT) {
		sn_transmit_chars(port, TRANSMIT_BUFFERED);
	}
	spin_unlock_irqrestore(&port->sc_port.lock, flags);
	return IRQ_HANDLED;
}

static void sn_sal_timer_poll(unsigned long data)
{
	struct sn_cons_port *port = (struct sn_cons_port *)data;
	unsigned long flags;

	if (!port)
		return;

	if (!port->sc_port.irq) {
		spin_lock_irqsave(&port->sc_port.lock, flags);
		if (sn_process_input)
			sn_receive_chars(port, flags);
		sn_transmit_chars(port, TRANSMIT_RAW);
		spin_unlock_irqrestore(&port->sc_port.lock, flags);
		mod_timer(&port->sc_timer,
			  jiffies + port->sc_interrupt_timeout);
	}
}


static void __init sn_sal_switch_to_asynch(struct sn_cons_port *port)
{
	unsigned long flags;

	if (!port)
		return;

	DPRINTF("sn_console: about to switch to asynchronous console\n");

	spin_lock_irqsave(&port->sc_port.lock, flags);

	
	if (!port->sc_ops)
		port->sc_ops = &poll_ops;

	init_timer(&port->sc_timer);
	port->sc_timer.function = sn_sal_timer_poll;
	port->sc_timer.data = (unsigned long)port;

	if (IS_RUNNING_ON_SIMULATOR())
		port->sc_interrupt_timeout = 6;
	else {
		port->sc_interrupt_timeout =
		    HZ * SN_SAL_UART_FIFO_DEPTH / SN_SAL_UART_FIFO_SPEED_CPS;
	}
	mod_timer(&port->sc_timer, jiffies + port->sc_interrupt_timeout);

	port->sc_is_asynch = 1;
	spin_unlock_irqrestore(&port->sc_port.lock, flags);
}

static void __init sn_sal_switch_to_interrupts(struct sn_cons_port *port)
{
	unsigned long flags;

	if (port) {
		DPRINTF("sn_console: switching to interrupt driven console\n");

		if (request_irq(SGI_UART_VECTOR, sn_sal_interrupt,
				IRQF_SHARED,
				"SAL console driver", port) >= 0) {
			spin_lock_irqsave(&port->sc_port.lock, flags);
			port->sc_port.irq = SGI_UART_VECTOR;
			port->sc_ops = &intr_ops;
			irq_set_handler(port->sc_port.irq, handle_level_irq);

			
			ia64_sn_console_intr_enable(SAL_CONSOLE_INTR_RECV);
			spin_unlock_irqrestore(&port->sc_port.lock, flags);
		}
		else {
			printk(KERN_INFO
			    "sn_console: console proceeding in polled mode\n");
		}
	}
}


static void sn_sal_console_write(struct console *, const char *, unsigned);
static int sn_sal_console_setup(struct console *, char *);
static struct uart_driver sal_console_uart;
extern struct tty_driver *uart_console_device(struct console *, int *);

static struct console sal_console = {
	.name = DEVICE_NAME,
	.write = sn_sal_console_write,
	.device = uart_console_device,
	.setup = sn_sal_console_setup,
	.index = -1,		
	.data = &sal_console_uart,
};

#define SAL_CONSOLE	&sal_console

static struct uart_driver sal_console_uart = {
	.owner = THIS_MODULE,
	.driver_name = "sn_console",
	.dev_name = DEVICE_NAME,
	.major = 0,		
	.minor = 0,
	.nr = 1,		
	.cons = SAL_CONSOLE,
};

static int __init sn_sal_module_init(void)
{
	int retval;

	if (!ia64_platform_is("sn2"))
		return 0;

	printk(KERN_INFO "sn_console: Console driver init\n");

	if (USE_DYNAMIC_MINOR == 1) {
		misc.minor = MISC_DYNAMIC_MINOR;
		misc.name = DEVICE_NAME_DYNAMIC;
		retval = misc_register(&misc);
		if (retval != 0) {
			printk(KERN_WARNING "Failed to register console "
			       "device using misc_register.\n");
			return -ENODEV;
		}
		sal_console_uart.major = MISC_MAJOR;
		sal_console_uart.minor = misc.minor;
	} else {
		sal_console_uart.major = DEVICE_MAJOR;
		sal_console_uart.minor = DEVICE_MINOR;
	}


	if (uart_register_driver(&sal_console_uart) < 0) {
		printk
		    ("ERROR sn_sal_module_init failed uart_register_driver, line %d\n",
		     __LINE__);
		return -ENODEV;
	}

	spin_lock_init(&sal_console_port.sc_port.lock);

	
	sal_console_port.sc_port.membase = (char *)1;	
	sal_console_port.sc_port.type = PORT_16550A;
	sal_console_port.sc_port.fifosize = SN_SAL_MAX_CHARS;
	sal_console_port.sc_port.ops = &sn_console_ops;
	sal_console_port.sc_port.line = 0;

	if (uart_add_one_port(&sal_console_uart, &sal_console_port.sc_port) < 0) {
		
		printk(KERN_ERR "%s: unable to add port\n", __func__);
	}

	if (!sal_console_port.sc_is_asynch) {
		sn_sal_switch_to_asynch(&sal_console_port);
	}

	
	if (!IS_RUNNING_ON_SIMULATOR()) {
		sn_sal_switch_to_interrupts(&sal_console_port);
	}
	sn_process_input = 1;
	return 0;
}

static void __exit sn_sal_module_exit(void)
{
	del_timer_sync(&sal_console_port.sc_timer);
	uart_remove_one_port(&sal_console_uart, &sal_console_port.sc_port);
	uart_unregister_driver(&sal_console_uart);
	misc_deregister(&misc);
}

module_init(sn_sal_module_init);
module_exit(sn_sal_module_exit);


static void puts_raw_fixed(int (*puts_raw) (const char *s, int len),
			   const char *s, int count)
{
	const char *s1;

	
	while ((s1 = memchr(s, '\n', count)) != NULL) {
		puts_raw(s, s1 - s);
		puts_raw("\r\n", 2);
		count -= s1 + 1 - s;
		s = s1 + 1;
	}
	puts_raw(s, count);
}

static void
sn_sal_console_write(struct console *co, const char *s, unsigned count)
{
	unsigned long flags = 0;
	struct sn_cons_port *port = &sal_console_port;
	static int stole_lock = 0;

	BUG_ON(!port->sc_is_asynch);

	if (!port->sc_port.state) {
		
		puts_raw_fixed(port->sc_ops->sal_puts_raw, s, count);
		return;
	}

	if (spin_is_locked(&port->sc_port.lock)) {
		int lhead = port->sc_port.state->xmit.head;
		int ltail = port->sc_port.state->xmit.tail;
		int counter, got_lock = 0;


		for (counter = 0; counter < 150; mdelay(125), counter++) {
			if (!spin_is_locked(&port->sc_port.lock)
			    || stole_lock) {
				if (!stole_lock) {
					spin_lock_irqsave(&port->sc_port.lock,
							  flags);
					got_lock = 1;
				}
				break;
			} else {
				
				if ((lhead != port->sc_port.state->xmit.head)
				    || (ltail !=
					port->sc_port.state->xmit.tail)) {
					lhead =
						port->sc_port.state->xmit.head;
					ltail =
						port->sc_port.state->xmit.tail;
					counter = 0;
				}
			}
		}
		
		sn_transmit_chars(port, 1);
		if (got_lock) {
			spin_unlock_irqrestore(&port->sc_port.lock, flags);
			stole_lock = 0;
		} else {
			
			stole_lock = 1;
		}
		puts_raw_fixed(port->sc_ops->sal_puts_raw, s, count);
	} else {
		stole_lock = 0;
		spin_lock_irqsave(&port->sc_port.lock, flags);
		sn_transmit_chars(port, 1);
		spin_unlock_irqrestore(&port->sc_port.lock, flags);

		puts_raw_fixed(port->sc_ops->sal_puts_raw, s, count);
	}
}


static int sn_sal_console_setup(struct console *co, char *options)
{
	return 0;
}

static void __init
sn_sal_console_write_early(struct console *co, const char *s, unsigned count)
{
	puts_raw_fixed(sal_console_port.sc_ops->sal_puts_raw, s, count);
}

static struct console sal_console_early __initdata = {
	.name = "sn_sal",
	.write = sn_sal_console_write_early,
	.flags = CON_PRINTBUFFER,
	.index = -1,
};

int __init sn_serial_console_early_setup(void)
{
	if (!ia64_platform_is("sn2"))
		return -1;

	sal_console_port.sc_ops = &poll_ops;
	spin_lock_init(&sal_console_port.sc_port.lock);
	early_sn_setup();	
	register_console(&sal_console_early);

	return 0;
}

static int __init sn_sal_serial_console_init(void)
{
	if (ia64_platform_is("sn2")) {
		sn_sal_switch_to_asynch(&sal_console_port);
		DPRINTF("sn_sal_serial_console_init : register console\n");
		register_console(&sal_console);
		unregister_console(&sal_console_early);
	}
	return 0;
}

console_initcall(sn_sal_serial_console_init);
