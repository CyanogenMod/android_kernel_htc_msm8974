/* linux/arch/arm/mach-s3c2410/s3c2410.c
 *
 * Copyright (c) 2003-2005 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * http://www.simtec.co.uk/products/EB2410ITX/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/syscore_ops.h>
#include <linux/serial_core.h>
#include <linux/platform_device.h>
#include <linux/io.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <mach/hardware.h>
#include <asm/irq.h>
#include <asm/system_misc.h>

#include <plat/cpu-freq.h>

#include <mach/regs-clock.h>
#include <plat/regs-serial.h>

#include <plat/s3c2410.h>
#include <plat/cpu.h>
#include <plat/devs.h>
#include <plat/clock.h>
#include <plat/pll.h>
#include <plat/pm.h>
#include <plat/watchdog-reset.h>

#include <plat/gpio-core.h>
#include <plat/gpio-cfg.h>
#include <plat/gpio-cfg-helpers.h>


static struct map_desc s3c2410_iodesc[] __initdata = {
	IODESC_ENT(CLKPWR),
	IODESC_ENT(TIMER),
	IODESC_ENT(WATCHDOG),
};



void __init s3c2410_init_uarts(struct s3c2410_uartcfg *cfg, int no)
{
	s3c24xx_init_uartdevs("s3c2410-uart", s3c2410_uart_resources, cfg, no);
}


void __init s3c2410_map_io(void)
{
	s3c24xx_gpiocfg_default.set_pull = s3c24xx_gpio_setpull_1up;
	s3c24xx_gpiocfg_default.get_pull = s3c24xx_gpio_getpull_1up;

	iotable_init(s3c2410_iodesc, ARRAY_SIZE(s3c2410_iodesc));
}

void __init_or_cpufreq s3c2410_setup_clocks(void)
{
	struct clk *xtal_clk;
	unsigned long tmp;
	unsigned long xtal;
	unsigned long fclk;
	unsigned long hclk;
	unsigned long pclk;

	xtal_clk = clk_get(NULL, "xtal");
	xtal = clk_get_rate(xtal_clk);
	clk_put(xtal_clk);


	fclk = s3c24xx_get_pll(__raw_readl(S3C2410_MPLLCON), xtal);

	tmp = __raw_readl(S3C2410_CLKDIVN);

	

	hclk = fclk / ((tmp & S3C2410_CLKDIVN_HDIVN) ? 2 : 1);
	pclk = hclk / ((tmp & S3C2410_CLKDIVN_PDIVN) ? 2 : 1);

	

	printk("S3C2410: core %ld.%03ld MHz, memory %ld.%03ld MHz, peripheral %ld.%03ld MHz\n",
	       print_mhz(fclk), print_mhz(hclk), print_mhz(pclk));


	s3c24xx_setup_clocks(fclk, hclk, pclk);
}


static struct clk s3c2410_armclk = {
	.name	= "armclk",
	.parent	= &clk_f,
	.id	= -1,
};

static struct clk_lookup s3c2410_clk_lookup[] = {
	CLKDEV_INIT(NULL, "clk_uart_baud0", &clk_p),
	CLKDEV_INIT(NULL, "clk_uart_baud1", &s3c24xx_uclk),
};

void __init s3c2410_init_clocks(int xtal)
{
	s3c24xx_register_baseclocks(xtal);
	s3c2410_setup_clocks();
	s3c2410_baseclk_add();
	s3c24xx_register_clock(&s3c2410_armclk);
	clkdev_add_table(s3c2410_clk_lookup, ARRAY_SIZE(s3c2410_clk_lookup));
}

struct bus_type s3c2410_subsys = {
	.name = "s3c2410-core",
	.dev_name = "s3c2410-core",
};

struct bus_type s3c2410a_subsys = {
	.name = "s3c2410a-core",
	.dev_name = "s3c2410a-core",
};

static struct device s3c2410_dev = {
	.bus		= &s3c2410_subsys,
};


static int __init s3c2410_core_init(void)
{
	return subsys_system_register(&s3c2410_subsys, NULL);
}

core_initcall(s3c2410_core_init);

static int __init s3c2410a_core_init(void)
{
	return subsys_system_register(&s3c2410a_subsys, NULL);
}

core_initcall(s3c2410a_core_init);

int __init s3c2410_init(void)
{
	printk("S3C2410: Initialising architecture\n");

#ifdef CONFIG_PM
	register_syscore_ops(&s3c2410_pm_syscore_ops);
#endif
	register_syscore_ops(&s3c24xx_irq_syscore_ops);

	return device_register(&s3c2410_dev);
}

int __init s3c2410a_init(void)
{
	s3c2410_dev.bus = &s3c2410a_subsys;
	return s3c2410_init();
}

void s3c2410_restart(char mode, const char *cmd)
{
	if (mode == 's') {
		soft_restart(0);
	}

	arch_wdt_reset();

	
	soft_restart(0);
}
