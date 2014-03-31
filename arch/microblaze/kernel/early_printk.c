/*
 * Early printk support for Microblaze.
 *
 * Copyright (C) 2007-2009 Michal Simek <monstr@monstr.eu>
 * Copyright (C) 2007-2009 PetaLogix
 * Copyright (C) 2003-2006 Yasushi SHOJI <yashi@atmark-techno.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#include <linux/console.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/tty.h>
#include <linux/io.h>
#include <asm/processor.h>
#include <linux/fcntl.h>
#include <asm/setup.h>
#include <asm/prom.h>

static u32 early_console_initialized;
static u32 base_addr;

#ifdef CONFIG_SERIAL_UARTLITE_CONSOLE
static void early_printk_uartlite_putc(char c)
{

	unsigned retries = 1000000;
	
	while (--retries && (in_be32(base_addr + 8) & (1 << 3)))
		;

	
	
	if (retries)
		out_be32(base_addr + 4, c & 0xff);
}

static void early_printk_uartlite_write(struct console *unused,
					const char *s, unsigned n)
{
	while (*s && n-- > 0) {
		if (*s == '\n')
			early_printk_uartlite_putc('\r');
		early_printk_uartlite_putc(*s);
		s++;
	}
}

static struct console early_serial_uartlite_console = {
	.name = "earlyser",
	.write = early_printk_uartlite_write,
	.flags = CON_PRINTBUFFER | CON_BOOT,
	.index = -1,
};
#endif 

#ifdef CONFIG_SERIAL_8250_CONSOLE
static void early_printk_uart16550_putc(char c)
{

	#define UART_LSR_TEMT	0x40 
	#define UART_LSR_THRE	0x20 
	#define BOTH_EMPTY (UART_LSR_TEMT | UART_LSR_THRE)

	unsigned retries = 10000;

	while (--retries &&
		!((in_be32(base_addr + 0x14) & BOTH_EMPTY) == BOTH_EMPTY))
		;

	if (retries)
		out_be32(base_addr, c & 0xff);
}

static void early_printk_uart16550_write(struct console *unused,
					const char *s, unsigned n)
{
	while (*s && n-- > 0) {
		if (*s == '\n')
			early_printk_uart16550_putc('\r');
		early_printk_uart16550_putc(*s);
		s++;
	}
}

static struct console early_serial_uart16550_console = {
	.name = "earlyser",
	.write = early_printk_uart16550_write,
	.flags = CON_PRINTBUFFER | CON_BOOT,
	.index = -1,
};
#endif 

static struct console *early_console;

void early_printk(const char *fmt, ...)
{
	char buf[512];
	int n;
	va_list ap;

	if (early_console_initialized) {
		va_start(ap, fmt);
		n = vscnprintf(buf, 512, fmt, ap);
		early_console->write(early_console, buf, n);
		va_end(ap);
	}
}

int __init setup_early_printk(char *opt)
{
	int version = 0;

	if (early_console_initialized)
		return 1;

	base_addr = of_early_console(&version);
	if (base_addr) {
#ifdef CONFIG_MMU
		early_console_reg_tlb_alloc(base_addr);
#endif
		switch (version) {
#ifdef CONFIG_SERIAL_UARTLITE_CONSOLE
		case UARTLITE:
			printk(KERN_INFO "Early console on uartlite "
						"at 0x%08x\n", base_addr);
			early_console = &early_serial_uartlite_console;
			break;
#endif
#ifdef CONFIG_SERIAL_8250_CONSOLE
		case UART16550:
			printk(KERN_INFO "Early console on uart16650 "
						"at 0x%08x\n", base_addr);
			early_console = &early_serial_uart16550_console;
			break;
#endif
		default:
			printk(KERN_INFO  "Unsupported early console %d\n",
								version);
			return 1;
		}

		register_console(early_console);
		early_console_initialized = 1;
		return 0;
	}
	return 1;
}

void __init remap_early_printk(void)
{
	if (!early_console_initialized || !early_console)
		return;
	printk(KERN_INFO "early_printk_console remapping from 0x%x to ",
								base_addr);
	base_addr = (u32) ioremap(base_addr, PAGE_SIZE);
	printk(KERN_CONT "0x%x\n", base_addr);

#ifdef CONFIG_MMU
	tlb_skip -= 1;
#endif
}

void __init disable_early_printk(void)
{
	if (!early_console_initialized || !early_console)
		return;
	printk(KERN_WARNING "disabling early console\n");
	unregister_console(early_console);
	early_console_initialized = 0;
}
