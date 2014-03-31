/*
 * linux/arch/arm/mach-mx1/mach-scb9328.c
 *
 * Copyright (c) 2004 Sascha Hauer <saschahauer@web.de>
 * Copyright (c) 2006-2008 Juergen Beisert <jbeisert@netscape.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/platform_device.h>
#include <linux/mtd/physmap.h>
#include <linux/interrupt.h>
#include <linux/dm9000.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>

#include <mach/common.h>
#include <mach/hardware.h>
#include <mach/irqs.h>
#include <mach/iomux-mx1.h>

#include "devices-imx1.h"

static struct resource flash_resource = {
	.start	= MX1_CS0_PHYS,
	.end	= MX1_CS0_PHYS + (32 * 1024 * 1024) - 1,
	.flags	= IORESOURCE_MEM,
};

static struct physmap_flash_data scb_flash_data = {
	.width  = 2,
};

static struct platform_device scb_flash_device = {
	.name		= "physmap-flash",
	.id		= 0,
	.dev = {
		.platform_data = &scb_flash_data,
	},
	.resource = &flash_resource,
	.num_resources = 1,
};


static struct dm9000_plat_data dm9000_platdata = {
	.flags	= DM9000_PLATF_16BITONLY,
};

static struct resource dm9000x_resources[] = {
	{
		.name	= "address area",
		.start	= MX1_CS5_PHYS,
		.end	= MX1_CS5_PHYS + 1,
		.flags	= IORESOURCE_MEM,	
	}, {
		.name	= "data area",
		.start	= MX1_CS5_PHYS + 4,
		.end	= MX1_CS5_PHYS + 5,
		.flags	= IORESOURCE_MEM,	
	}, {
		.start	= IRQ_GPIOC(3),
		.end	= IRQ_GPIOC(3),
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_LOWLEVEL,
	},
};

static struct platform_device dm9000x_device = {
	.name		= "dm9000",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(dm9000x_resources),
	.resource	= dm9000x_resources,
	.dev		= {
		.platform_data = &dm9000_platdata,
	}
};

static const int mxc_uart1_pins[] = {
	PC9_PF_UART1_CTS,
	PC10_PF_UART1_RTS,
	PC11_PF_UART1_TXD,
	PC12_PF_UART1_RXD,
};

static const struct imxuart_platform_data uart_pdata __initconst = {
	.flags = IMXUART_HAVE_RTSCTS,
};

static struct platform_device *devices[] __initdata = {
	&scb_flash_device,
	&dm9000x_device,
};

static void __init scb9328_init(void)
{
	imx1_soc_init();

	mxc_gpio_setup_multiple_pins(mxc_uart1_pins,
			ARRAY_SIZE(mxc_uart1_pins), "UART1");

	imx1_add_imx_uart0(&uart_pdata);

	printk(KERN_INFO"Scb9328: Adding devices\n");
	platform_add_devices(devices, ARRAY_SIZE(devices));
}

static void __init scb9328_timer_init(void)
{
	mx1_clocks_init(32000);
}

static struct sys_timer scb9328_timer = {
	.init	= scb9328_timer_init,
};

MACHINE_START(SCB9328, "Synertronixx scb9328")
	
	.atag_offset = 100,
	.map_io = mx1_map_io,
	.init_early = imx1_init_early,
	.init_irq = mx1_init_irq,
	.handle_irq = imx1_handle_irq,
	.timer = &scb9328_timer,
	.init_machine = scb9328_init,
	.restart	= mxc_restart,
MACHINE_END
