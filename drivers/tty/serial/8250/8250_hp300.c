#include <linux/module.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/serial_8250.h>
#include <linux/delay.h>
#include <linux/dio.h>
#include <linux/console.h>
#include <linux/slab.h>
#include <asm/io.h>

#include "8250.h"

#if !defined(CONFIG_HPDCA) && !defined(CONFIG_HPAPCI)
#warning CONFIG_8250 defined but neither CONFIG_HPDCA nor CONFIG_HPAPCI defined, are you sure?
#endif

#ifdef CONFIG_HPAPCI
struct hp300_port
{
	struct hp300_port *next;	
	int line;			
};

static struct hp300_port *hp300_ports;
#endif

#ifdef CONFIG_HPDCA

static int __devinit hpdca_init_one(struct dio_dev *d,
					const struct dio_device_id *ent);
static void __devexit hpdca_remove_one(struct dio_dev *d);

static struct dio_device_id hpdca_dio_tbl[] = {
	{ DIO_ID_DCA0 },
	{ DIO_ID_DCA0REM },
	{ DIO_ID_DCA1 },
	{ DIO_ID_DCA1REM },
	{ 0 }
};

static struct dio_driver hpdca_driver = {
	.name      = "hpdca",
	.id_table  = hpdca_dio_tbl,
	.probe     = hpdca_init_one,
	.remove    = __devexit_p(hpdca_remove_one),
};

#endif

static unsigned int num_ports;

extern int hp300_uart_scode;

#define UART_OFFSET	17

#define DCA_ID		0x01	
#define DCA_IC		0x03	

#define DCA_IC_IE	0x80	

#define HPDCA_BAUD_BASE 153600

#define FRODO_BASE	(0x41c000)

#define FRODO_APCIBASE		0x0
#define FRODO_APCISPACE		0x20
#define FRODO_APCI_OFFSET(x)	(FRODO_APCIBASE + ((x) * FRODO_APCISPACE))

#define HPAPCI_BAUD_BASE 500400

#ifdef CONFIG_SERIAL_8250_CONSOLE
int __init hp300_setup_serial_console(void)
{
	int scode;
	struct uart_port port;

	memset(&port, 0, sizeof(port));

	if (hp300_uart_scode < 0 || hp300_uart_scode > DIO_SCMAX)
		return 0;

	if (DIO_SCINHOLE(hp300_uart_scode))
		return 0;

	scode = hp300_uart_scode;

	
	port.iotype = UPIO_MEM;
	port.flags = UPF_SKIP_TEST | UPF_SHARE_IRQ | UPF_BOOT_AUTOCONF;
	port.type = PORT_UNKNOWN;

	
	if (scode == 256) {
#ifdef CONFIG_HPAPCI
		printk(KERN_INFO "Serial console is HP APCI 1\n");

		port.uartclk = HPAPCI_BAUD_BASE * 16;
		port.mapbase = (FRODO_BASE + FRODO_APCI_OFFSET(1));
		port.membase = (char *)(port.mapbase + DIO_VIRADDRBASE);
		port.regshift = 2;
		add_preferred_console("ttyS", port.line, "9600n8");
#else
		printk(KERN_WARNING "Serial console is APCI but support is disabled (CONFIG_HPAPCI)!\n");
		return 0;
#endif
	} else {
#ifdef CONFIG_HPDCA
		unsigned long pa = dio_scodetophysaddr(scode);
		if (!pa)
			return 0;

		printk(KERN_INFO "Serial console is HP DCA at select code %d\n", scode);

		port.uartclk = HPDCA_BAUD_BASE * 16;
		port.mapbase = (pa + UART_OFFSET);
		port.membase = (char *)(port.mapbase + DIO_VIRADDRBASE);
		port.regshift = 1;
		port.irq = DIO_IPL(pa + DIO_VIRADDRBASE);

		
		out_8(pa + DIO_VIRADDRBASE + DCA_IC, DCA_IC_IE);

		if (DIO_ID(pa + DIO_VIRADDRBASE) & 0x80)
			add_preferred_console("ttyS", port.line, "9600n8");
#else
		printk(KERN_WARNING "Serial console is DCA but support is disabled (CONFIG_HPDCA)!\n");
		return 0;
#endif
	}

	if (early_serial_setup(&port) < 0)
		printk(KERN_WARNING "hp300_setup_serial_console(): early_serial_setup() failed.\n");
	return 0;
}
#endif 

#ifdef CONFIG_HPDCA
static int __devinit hpdca_init_one(struct dio_dev *d,
				const struct dio_device_id *ent)
{
	struct uart_port port;
	int line;

#ifdef CONFIG_SERIAL_8250_CONSOLE
	if (hp300_uart_scode == d->scode) {
		
		return 0;
	}
#endif
	memset(&port, 0, sizeof(struct uart_port));

	
	port.iotype = UPIO_MEM;
	port.flags = UPF_SKIP_TEST | UPF_SHARE_IRQ | UPF_BOOT_AUTOCONF;
	port.irq = d->ipl;
	port.uartclk = HPDCA_BAUD_BASE * 16;
	port.mapbase = (d->resource.start + UART_OFFSET);
	port.membase = (char *)(port.mapbase + DIO_VIRADDRBASE);
	port.regshift = 1;
	port.dev = &d->dev;
	line = serial8250_register_port(&port);

	if (line < 0) {
		printk(KERN_NOTICE "8250_hp300: register_serial() DCA scode %d"
		       " irq %d failed\n", d->scode, port.irq);
		return -ENOMEM;
	}

	
	out_8(d->resource.start + DIO_VIRADDRBASE + DCA_IC, DCA_IC_IE);
	dio_set_drvdata(d, (void *)line);

	
	out_8(d->resource.start + DIO_VIRADDRBASE + DCA_ID, 0xff);
	udelay(100);

	num_ports++;

	return 0;
}
#endif

static int __init hp300_8250_init(void)
{
	static int called;
#ifdef CONFIG_HPAPCI
	int line;
	unsigned long base;
	struct uart_port uport;
	struct hp300_port *port;
	int i;
#endif
	if (called)
		return -ENODEV;
	called = 1;

	if (!MACH_IS_HP300)
		return -ENODEV;

#ifdef CONFIG_HPDCA
	dio_register_driver(&hpdca_driver);
#endif
#ifdef CONFIG_HPAPCI
	if (hp300_model < HP_400) {
		if (!num_ports)
			return -ENODEV;
		return 0;
	}
	for (i = 1; i < 4; i++) {
#ifdef CONFIG_SERIAL_8250_CONSOLE
		if (i == 1)
			continue;
#endif

		
		port = kmalloc(sizeof(struct hp300_port), GFP_KERNEL);
		if (!port)
			return -ENOMEM;

		memset(&uport, 0, sizeof(struct uart_port));

		base = (FRODO_BASE + FRODO_APCI_OFFSET(i));

		
		uport.iotype = UPIO_MEM;
		uport.flags = UPF_SKIP_TEST | UPF_SHARE_IRQ \
			      | UPF_BOOT_AUTOCONF;
		
		uport.irq = 0;
		uport.uartclk = HPAPCI_BAUD_BASE * 16;
		uport.mapbase = base;
		uport.membase = (char *)(base + DIO_VIRADDRBASE);
		uport.regshift = 2;

		line = serial8250_register_port(&uport);

		if (line < 0) {
			printk(KERN_NOTICE "8250_hp300: register_serial() APCI"
			       " %d irq %d failed\n", i, uport.irq);
			kfree(port);
			continue;
		}

		port->line = line;
		port->next = hp300_ports;
		hp300_ports = port;

		num_ports++;
	}
#endif

	
	if (!num_ports)
		return -ENODEV;

	return 0;
}

#ifdef CONFIG_HPDCA
static void __devexit hpdca_remove_one(struct dio_dev *d)
{
	int line;

	line = (int) dio_get_drvdata(d);
	if (d->resource.start) {
		
		out_8(d->resource.start + DIO_VIRADDRBASE + DCA_IC, 0);
	}
	serial8250_unregister_port(line);
}
#endif

static void __exit hp300_8250_exit(void)
{
#ifdef CONFIG_HPAPCI
	struct hp300_port *port, *to_free;

	for (port = hp300_ports; port; ) {
		serial8250_unregister_port(port->line);
		to_free = port;
		port = port->next;
		kfree(to_free);
	}

	hp300_ports = NULL;
#endif
#ifdef CONFIG_HPDCA
	dio_unregister_driver(&hpdca_driver);
#endif
}

module_init(hp300_8250_init);
module_exit(hp300_8250_exit);
MODULE_DESCRIPTION("HP DCA/APCI serial driver");
MODULE_AUTHOR("Kars de Jong <jongk@linux-m68k.org>");
MODULE_LICENSE("GPL");
