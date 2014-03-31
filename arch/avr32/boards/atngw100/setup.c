/*
 * Board-specific setup code for the ATNGW100 Network Gateway
 *
 * Copyright (C) 2005-2006 Atmel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/clk.h>
#include <linux/etherdevice.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/init.h>
#include <linux/linkage.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/leds.h>
#include <linux/spi/spi.h>
#include <linux/atmel-mci.h>
#include <linux/usb/atmel_usba_udc.h>

#include <asm/io.h>
#include <asm/setup.h>

#include <mach/at32ap700x.h>
#include <mach/board.h>
#include <mach/init.h>
#include <mach/portmux.h>

unsigned long at32_board_osc_rates[3] = {
	[0] = 32768,	
	[1] = 20000000,	
	[2] = 12000000,	
};

#ifdef CONFIG_BOARD_ATNGW100_MKII
#include <linux/mtd/partitions.h>
#include <mach/smc.h>

static struct smc_timing nand_timing __initdata = {
	.ncs_read_setup		= 0,
	.nrd_setup		= 10,
	.ncs_write_setup	= 0,
	.nwe_setup		= 10,

	.ncs_read_pulse		= 30,
	.nrd_pulse		= 15,
	.ncs_write_pulse	= 30,
	.nwe_pulse		= 15,

	.read_cycle		= 30,
	.write_cycle		= 30,

	.ncs_read_recover	= 0,
	.nrd_recover		= 15,
	.ncs_write_recover	= 0,
	
	.nwe_recover		= 50,
};

static struct smc_config nand_config __initdata = {
	.bus_width		= 2,
	.nrd_controlled		= 1,
	.nwe_controlled		= 1,
	.nwait_mode		= 0,
	.byte_write		= 0,
	.tdf_cycles		= 2,
	.tdf_mode		= 0,
};

static struct mtd_partition nand_partitions[] = {
	{
		.name		= "main",
		.offset		= 0x00000000,
		.size		= MTDPART_SIZ_FULL,
	},
};


static struct atmel_nand_data atngw100mkii_nand_data __initdata = {
	.cle		= 21,
	.ale		= 22,
	.rdy_pin	= GPIO_PIN_PB(28),
	.enable_pin	= GPIO_PIN_PE(23),
	.bus_width_16	= true,
	.ecc_mode	= NAND_ECC_SOFT,
	.parts		= nand_partitions,
	.num_parts	= ARRAY_SIZE(nand_partitions),
};
#endif

struct tag *bootloader_tags __initdata;

struct eth_addr {
	u8 addr[6];
};
static struct eth_addr __initdata hw_addr[2];
static struct macb_platform_data __initdata eth_data[2];

static struct spi_board_info spi0_board_info[] __initdata = {
	{
		.modalias	= "mtd_dataflash",
		.max_speed_hz	= 8000000,
		.chip_select	= 0,
	},
};

static struct mci_platform_data __initdata mci0_data = {
	.slot[0] = {
		.bus_width	= 4,
#if defined(CONFIG_BOARD_ATNGW100_MKII)
		.detect_pin	= GPIO_PIN_PC(25),
		.wp_pin		= GPIO_PIN_PE(22),
#else
		.detect_pin	= GPIO_PIN_PC(25),
		.wp_pin		= GPIO_PIN_PE(0),
#endif
	},
};

static struct usba_platform_data atngw100_usba_data __initdata = {
#if defined(CONFIG_BOARD_ATNGW100_MKII)
	.vbus_pin	= GPIO_PIN_PE(26),
#else
	.vbus_pin	= -ENODEV,
#endif
};

static int __init parse_tag_ethernet(struct tag *tag)
{
	int i;

	i = tag->u.ethernet.mac_index;
	if (i < ARRAY_SIZE(hw_addr))
		memcpy(hw_addr[i].addr, tag->u.ethernet.hw_address,
		       sizeof(hw_addr[i].addr));

	return 0;
}
__tagtable(ATAG_ETHERNET, parse_tag_ethernet);

static void __init set_hw_addr(struct platform_device *pdev)
{
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	const u8 *addr;
	void __iomem *regs;
	struct clk *pclk;

	if (!res)
		return;
	if (pdev->id >= ARRAY_SIZE(hw_addr))
		return;

	addr = hw_addr[pdev->id].addr;
	if (!is_valid_ether_addr(addr))
		return;

	regs = (void __iomem __force *)res->start;
	pclk = clk_get(&pdev->dev, "pclk");
	if (IS_ERR(pclk))
		return;

	clk_enable(pclk);
	__raw_writel((addr[3] << 24) | (addr[2] << 16)
		     | (addr[1] << 8) | addr[0], regs + 0x98);
	__raw_writel((addr[5] << 8) | addr[4], regs + 0x9c);
	clk_disable(pclk);
	clk_put(pclk);
}

void __init setup_board(void)
{
	at32_map_usart(1, 0, 0);	
	at32_setup_serial_console(0);
}

static const struct gpio_led ngw_leds[] = {
	{ .name = "sys", .gpio = GPIO_PIN_PA(16), .active_low = 1,
		.default_trigger = "heartbeat",
	},
	{ .name = "a", .gpio = GPIO_PIN_PA(19), .active_low = 1, },
	{ .name = "b", .gpio = GPIO_PIN_PE(19), .active_low = 1, },
};

static const struct gpio_led_platform_data ngw_led_data = {
	.num_leds =	ARRAY_SIZE(ngw_leds),
	.leds =		(void *) ngw_leds,
};

static struct platform_device ngw_gpio_leds = {
	.name =		"leds-gpio",
	.id =		-1,
	.dev = {
		.platform_data = (void *) &ngw_led_data,
	}
};

static struct i2c_gpio_platform_data i2c_gpio_data = {
	.sda_pin		= GPIO_PIN_PA(6),
	.scl_pin		= GPIO_PIN_PA(7),
	.sda_is_open_drain	= 1,
	.scl_is_open_drain	= 1,
	.udelay			= 2,	
};

static struct platform_device i2c_gpio_device = {
	.name		= "i2c-gpio",
	.id		= 0,
	.dev		= {
		.platform_data	= &i2c_gpio_data,
	},
};

static struct i2c_board_info __initdata i2c_info[] = {
	
};

static int __init atngw100_init(void)
{
	unsigned	i;

#ifdef CONFIG_BOARD_ATNGW100_MKII
	at32_reserve_pin(GPIO_PIOE_BASE, ATMEL_EBI_PE_DATA_ALL);

	smc_set_timing(&nand_config, &nand_timing);
	smc_set_configuration(3, &nand_config);
	at32_add_device_nand(0, &atngw100mkii_nand_data);
#endif

	at32_add_device_usart(0);

	set_hw_addr(at32_add_device_eth(0, &eth_data[0]));
#ifndef CONFIG_BOARD_ATNGW100_MKII_LCD
	set_hw_addr(at32_add_device_eth(1, &eth_data[1]));
#endif

	at32_add_device_spi(0, spi0_board_info, ARRAY_SIZE(spi0_board_info));
	at32_add_device_mci(0, &mci0_data);
	at32_add_device_usba(0, &atngw100_usba_data);

	for (i = 0; i < ARRAY_SIZE(ngw_leds); i++) {
		at32_select_gpio(ngw_leds[i].gpio,
				AT32_GPIOF_OUTPUT | AT32_GPIOF_HIGH);
	}
	platform_device_register(&ngw_gpio_leds);

#ifdef CONFIG_BOARD_ATNGW100_MKII
	at32_select_periph(GPIO_PIOE_BASE, 1 << 21, 0, AT32_GPIOF_PULLUP);
#else
	at32_select_periph(GPIO_PIOB_BASE, 1 << 28, 0, AT32_GPIOF_PULLUP);
#endif
	at32_select_gpio(i2c_gpio_data.sda_pin,
		AT32_GPIOF_MULTIDRV | AT32_GPIOF_OUTPUT | AT32_GPIOF_HIGH);
	at32_select_gpio(i2c_gpio_data.scl_pin,
		AT32_GPIOF_MULTIDRV | AT32_GPIOF_OUTPUT | AT32_GPIOF_HIGH);
	platform_device_register(&i2c_gpio_device);
	i2c_register_board_info(0, i2c_info, ARRAY_SIZE(i2c_info));

	return 0;
}
postcore_initcall(atngw100_init);

static int __init atngw100_arch_init(void)
{
#ifdef CONFIG_BOARD_ATNGW100_MKII
	at32_select_gpio(GPIO_PIN_PE(30), 0);
	gpio_request(GPIO_PIN_PE(30), "j15");
	gpio_direction_input(GPIO_PIN_PE(30));
	gpio_export(GPIO_PIN_PE(30), false);
#else
	at32_select_gpio(GPIO_PIN_PB(30), 0);
	gpio_request(GPIO_PIN_PB(30), "j15");
	gpio_direction_input(GPIO_PIN_PB(30));
	gpio_export(GPIO_PIN_PB(30), false);
#endif

	return irq_set_irq_type(AT32_EXTINT(3), IRQ_TYPE_EDGE_FALLING);
}
arch_initcall(atngw100_arch_init);
