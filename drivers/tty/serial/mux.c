/*
** mux.c:
**	serial driver for the Mux console found in some PA-RISC servers.
**
**	(c) Copyright 2002 Ryan Bradetich
**	(c) Copyright 2002 Hewlett-Packard Company
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This Driver currently only supports the console (port 0) on the MUX.
** Additional work will be needed on this driver to enable the full
** functionality of the MUX.
**
*/

#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/serial.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/console.h>
#include <linux/delay.h> 
#include <linux/device.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/parisc-device.h>

#ifdef CONFIG_MAGIC_SYSRQ
#include <linux/sysrq.h>
#define SUPPORT_SYSRQ
#endif

#include <linux/serial_core.h>

#define MUX_OFFSET 0x800
#define MUX_LINE_OFFSET 0x80

#define MUX_FIFO_SIZE 255
#define MUX_POLL_DELAY (30 * HZ / 1000)

#define IO_DATA_REG_OFFSET 0x3c
#define IO_DCOUNT_REG_OFFSET 0x40

#define MUX_EOFIFO(status) ((status & 0xF000) == 0xF000)
#define MUX_STATUS(status) ((status & 0xF000) == 0x8000)
#define MUX_BREAK(status) ((status & 0xF000) == 0x2000)

#define MUX_NR 256
static unsigned int port_cnt __read_mostly;
struct mux_port {
	struct uart_port port;
	int enabled;
};
static struct mux_port mux_ports[MUX_NR];

static struct uart_driver mux_driver = {
	.owner = THIS_MODULE,
	.driver_name = "ttyB",
	.dev_name = "ttyB",
	.major = MUX_MAJOR,
	.minor = 0,
	.nr = MUX_NR,
};

static struct timer_list mux_timer;

#define UART_PUT_CHAR(p, c) __raw_writel((c), (p)->membase + IO_DATA_REG_OFFSET)
#define UART_GET_FIFO_CNT(p) __raw_readl((p)->membase + IO_DCOUNT_REG_OFFSET)

static int __init get_mux_port_count(struct parisc_device *dev)
{
	int status;
	u8 iodc_data[32];
	unsigned long bytecnt;

	if(dev->id.hversion == 0x15)
		return 1;

	status = pdc_iodc_read(&bytecnt, dev->hpa.start, 0, iodc_data, 32);
	BUG_ON(status != PDC_OK);

	
	return ((((iodc_data)[4] & 0xf0) >> 4) * 8) + 8;
}

static unsigned int mux_tx_empty(struct uart_port *port)
{
	return UART_GET_FIFO_CNT(port) ? 0 : TIOCSER_TEMT;
} 

static void mux_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
}

static unsigned int mux_get_mctrl(struct uart_port *port)
{ 
	return TIOCM_CAR | TIOCM_DSR | TIOCM_CTS;
}

static void mux_stop_tx(struct uart_port *port)
{
}

static void mux_start_tx(struct uart_port *port)
{
}

static void mux_stop_rx(struct uart_port *port)
{
}

static void mux_enable_ms(struct uart_port *port)
{
}

static void mux_break_ctl(struct uart_port *port, int break_state)
{
}

static void mux_write(struct uart_port *port)
{
	int count;
	struct circ_buf *xmit = &port->state->xmit;

	if(port->x_char) {
		UART_PUT_CHAR(port, port->x_char);
		port->icount.tx++;
		port->x_char = 0;
		return;
	}

	if(uart_circ_empty(xmit) || uart_tx_stopped(port)) {
		mux_stop_tx(port);
		return;
	}

	count = (port->fifosize) - UART_GET_FIFO_CNT(port);
	do {
		UART_PUT_CHAR(port, xmit->buf[xmit->tail]);
		xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
		port->icount.tx++;
		if(uart_circ_empty(xmit))
			break;

	} while(--count > 0);

	while(UART_GET_FIFO_CNT(port)) 
		udelay(1);

	if(uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(port);

	if (uart_circ_empty(xmit))
		mux_stop_tx(port);
}

static void mux_read(struct uart_port *port)
{
	int data;
	struct tty_struct *tty = port->state->port.tty;
	__u32 start_count = port->icount.rx;

	while(1) {
		data = __raw_readl(port->membase + IO_DATA_REG_OFFSET);

		if (MUX_STATUS(data))
			continue;

		if (MUX_EOFIFO(data))
			break;

		port->icount.rx++;

		if (MUX_BREAK(data)) {
			port->icount.brk++;
			if(uart_handle_break(port))
				continue;
		}

		if (uart_handle_sysrq_char(port, data & 0xffu))
			continue;

		tty_insert_flip_char(tty, data & 0xFF, TTY_NORMAL);
	}
	
	if (start_count != port->icount.rx) {
		tty_flip_buffer_push(tty);
	}
}

static int mux_startup(struct uart_port *port)
{
	mux_ports[port->line].enabled = 1;
	return 0;
}

static void mux_shutdown(struct uart_port *port)
{
	mux_ports[port->line].enabled = 0;
}

static void
mux_set_termios(struct uart_port *port, struct ktermios *termios,
	        struct ktermios *old)
{
}

static const char *mux_type(struct uart_port *port)
{
	return "Mux";
}

static void mux_release_port(struct uart_port *port)
{
}

static int mux_request_port(struct uart_port *port)
{
	return 0;
}

static void mux_config_port(struct uart_port *port, int type)
{
	port->type = PORT_MUX;
}

static int mux_verify_port(struct uart_port *port, struct serial_struct *ser)
{
	if(port->membase == NULL)
		return -EINVAL;

	return 0;
}

static void mux_poll(unsigned long unused)
{  
	int i;

	for(i = 0; i < port_cnt; ++i) {
		if(!mux_ports[i].enabled)
			continue;

		mux_read(&mux_ports[i].port);
		mux_write(&mux_ports[i].port);
	}

	mod_timer(&mux_timer, jiffies + MUX_POLL_DELAY);
}


#ifdef CONFIG_SERIAL_MUX_CONSOLE
static void mux_console_write(struct console *co, const char *s, unsigned count)
{
	
	while(UART_GET_FIFO_CNT(&mux_ports[0].port))
		udelay(1);

	while(count--) {
		if(*s == '\n') {
			UART_PUT_CHAR(&mux_ports[0].port, '\r');
		}
		UART_PUT_CHAR(&mux_ports[0].port, *s++);
	}

}

static int mux_console_setup(struct console *co, char *options)
{
        return 0;
}

struct tty_driver *mux_console_device(struct console *co, int *index)
{
        *index = co->index;
	return mux_driver.tty_driver;
}

static struct console mux_console = {
	.name =		"ttyB",
	.write =	mux_console_write,
	.device =	mux_console_device,
	.setup =	mux_console_setup,
	.flags =	CON_ENABLED | CON_PRINTBUFFER,
	.index =	0,
};

#define MUX_CONSOLE	&mux_console
#else
#define MUX_CONSOLE	NULL
#endif

static struct uart_ops mux_pops = {
	.tx_empty =		mux_tx_empty,
	.set_mctrl =		mux_set_mctrl,
	.get_mctrl =		mux_get_mctrl,
	.stop_tx =		mux_stop_tx,
	.start_tx =		mux_start_tx,
	.stop_rx =		mux_stop_rx,
	.enable_ms =		mux_enable_ms,
	.break_ctl =		mux_break_ctl,
	.startup =		mux_startup,
	.shutdown =		mux_shutdown,
	.set_termios =		mux_set_termios,
	.type =			mux_type,
	.release_port =		mux_release_port,
	.request_port =		mux_request_port,
	.config_port =		mux_config_port,
	.verify_port =		mux_verify_port,
};

static int __init mux_probe(struct parisc_device *dev)
{
	int i, status;

	int port_count = get_mux_port_count(dev);
	printk(KERN_INFO "Serial mux driver (%d ports) Revision: 0.6\n", port_count);

	dev_set_drvdata(&dev->dev, (void *)(long)port_count);
	request_mem_region(dev->hpa.start + MUX_OFFSET,
                           port_count * MUX_LINE_OFFSET, "Mux");

	if(!port_cnt) {
		mux_driver.cons = MUX_CONSOLE;

		status = uart_register_driver(&mux_driver);
		if(status) {
			printk(KERN_ERR "Serial mux: Unable to register driver.\n");
			return 1;
		}
	}

	for(i = 0; i < port_count; ++i, ++port_cnt) {
		struct uart_port *port = &mux_ports[port_cnt].port;
		port->iobase	= 0;
		port->mapbase	= dev->hpa.start + MUX_OFFSET +
						(i * MUX_LINE_OFFSET);
		port->membase	= ioremap_nocache(port->mapbase, MUX_LINE_OFFSET);
		port->iotype	= UPIO_MEM;
		port->type	= PORT_MUX;
		port->irq	= 0;
		port->uartclk	= 0;
		port->fifosize	= MUX_FIFO_SIZE;
		port->ops	= &mux_pops;
		port->flags	= UPF_BOOT_AUTOCONF;
		port->line	= port_cnt;

		port->timeout   = HZ / 50;
		spin_lock_init(&port->lock);

		status = uart_add_one_port(&mux_driver, port);
		BUG_ON(status);
	}

	return 0;
}

static int __devexit mux_remove(struct parisc_device *dev)
{
	int i, j;
	int port_count = (long)dev_get_drvdata(&dev->dev);

	
	for(i = 0; i < port_cnt; ++i) {
		if(mux_ports[i].port.mapbase == dev->hpa.start + MUX_OFFSET)
			break;
	}
	BUG_ON(i + port_count > port_cnt);

	
	for(j = 0; j < port_count; ++j, ++i) {
		struct uart_port *port = &mux_ports[i].port;

		uart_remove_one_port(&mux_driver, port);
		if(port->membase)
			iounmap(port->membase);
	}

	release_mem_region(dev->hpa.start + MUX_OFFSET, port_count * MUX_LINE_OFFSET);
	return 0;
}

static struct parisc_device_id builtin_mux_tbl[] = {
	{ HPHW_A_DIRECT, HVERSION_REV_ANY_ID, 0x15, 0x0000D }, 
	{ HPHW_A_DIRECT, HVERSION_REV_ANY_ID, 0x44, 0x0000D }, 
	{ 0, }
};

static struct parisc_device_id mux_tbl[] = {
	{ HPHW_A_DIRECT, HVERSION_REV_ANY_ID, HVERSION_ANY_ID, 0x0000D },
	{ 0, }
};

MODULE_DEVICE_TABLE(parisc, builtin_mux_tbl);
MODULE_DEVICE_TABLE(parisc, mux_tbl);

static struct parisc_driver builtin_serial_mux_driver = {
	.name =		"builtin_serial_mux",
	.id_table =	builtin_mux_tbl,
	.probe =	mux_probe,
	.remove =       __devexit_p(mux_remove),
};

static struct parisc_driver serial_mux_driver = {
	.name =		"serial_mux",
	.id_table =	mux_tbl,
	.probe =	mux_probe,
	.remove =       __devexit_p(mux_remove),
};

static int __init mux_init(void)
{
	register_parisc_driver(&builtin_serial_mux_driver);
	register_parisc_driver(&serial_mux_driver);

	if(port_cnt > 0) {
		
		init_timer(&mux_timer);
		mux_timer.function = mux_poll;
		mod_timer(&mux_timer, jiffies + MUX_POLL_DELAY);

#ifdef CONFIG_SERIAL_MUX_CONSOLE
	        register_console(&mux_console);
#endif
	}

	return 0;
}

static void __exit mux_exit(void)
{
	
	if(port_cnt > 0) {
		del_timer(&mux_timer);
#ifdef CONFIG_SERIAL_MUX_CONSOLE
		unregister_console(&mux_console);
#endif
	}

	unregister_parisc_driver(&builtin_serial_mux_driver);
	unregister_parisc_driver(&serial_mux_driver);
	uart_unregister_driver(&mux_driver);
}

module_init(mux_init);
module_exit(mux_exit);

MODULE_AUTHOR("Ryan Bradetich");
MODULE_DESCRIPTION("Serial MUX driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS_CHARDEV_MAJOR(MUX_MAJOR);
