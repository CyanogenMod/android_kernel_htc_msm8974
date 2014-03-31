/*
 * setup.c: Setup pointers to hardware dependent routines.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1996, 1997, 2004 by Ralf Baechle (ralf@linux-mips.org)
 * Copyright (C) 2006, Wind River System Inc. Rongkai.zhan <rongkai.zhan@windriver.com>
 */
#include <linux/init.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/pm.h>

#include <asm/io.h>
#include <asm/bootinfo.h>
#include <asm/reboot.h>
#include <asm/time.h>
#include <asm/gt64120.h>

unsigned long gt64120_base = KSEG1ADDR(0x14000000);

#ifdef WRPPMC_EARLY_DEBUG

static volatile unsigned char * wrppmc_led = \
	(volatile unsigned char *)KSEG1ADDR(WRPPMC_LED_BASE);

void wrppmc_led_on(int mask)
{
	unsigned char value = *wrppmc_led;

	value &= (0xF8 | mask);
	*wrppmc_led = value;
}

void wrppmc_led_off(int mask)
{
	unsigned char value = *wrppmc_led;

	value |= (0x7 & mask);
	*wrppmc_led = value;
}

void __init wrppmc_early_putc(char ch)
{
	static volatile unsigned char *wrppmc_uart = \
		(volatile unsigned char *)KSEG1ADDR(WRPPMC_UART16550_BASE);
	unsigned char value;

	
	while (1) {
		value = *(wrppmc_uart + 5);
		if (value & 0x20)
			break;
	}

	*wrppmc_uart = ch;
}

void __init wrppmc_early_printk(const char *fmt, ...)
{
	static char pbuf[256] = {'\0', };
	char *ch = pbuf;
	va_list args;
	unsigned int i;

	memset(pbuf, 0, 256);
	va_start(args, fmt);
	i = vsprintf(pbuf, fmt, args);
	va_end(args);

	
	while (*ch != '\0') {
		wrppmc_early_putc(*ch);
		
		if (*ch++ == '\n')
			wrppmc_early_putc('\r');
	}
}
#endif 

void __init prom_free_prom_memory(void)
{
}

void __init plat_mem_setup(void)
{
	extern void wrppmc_machine_restart(char *command);
	extern void wrppmc_machine_halt(void);

	_machine_restart = wrppmc_machine_restart;
	_machine_halt	 = wrppmc_machine_halt;
	pm_power_off	 = wrppmc_machine_halt;

	set_io_port_base(KSEG1);
}

const char *get_system_type(void)
{
	return "Wind River PPMC (GT64120)";
}

void __init prom_init(void)
{
	add_memory_region(WRPPMC_SDRAM_SCS0_BASE, WRPPMC_SDRAM_SCS0_SIZE, BOOT_MEM_RAM);
	add_memory_region(WRPPMC_BOOTROM_BASE, WRPPMC_BOOTROM_SIZE, BOOT_MEM_ROM_DATA);

	wrppmc_early_printk("prom_init: GT64120 SDRAM Bank 0: 0x%x - 0x%08lx\n",
			WRPPMC_SDRAM_SCS0_BASE, (WRPPMC_SDRAM_SCS0_BASE + WRPPMC_SDRAM_SCS0_SIZE));
}
