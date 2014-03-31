/*
 * arch/arm/mach-orion5x/d2net-setup.c
 *
 * LaCie d2Network and Big Disk Network NAS setup
 *
 * Copyright (C) 2009 Simon Guinot <sguinot@lacie.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/pci.h>
#include <linux/irq.h>
#include <linux/mtd/physmap.h>
#include <linux/mv643xx_eth.h>
#include <linux/leds.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/ata_platform.h>
#include <linux/gpio.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/pci.h>
#include <mach/orion5x.h>
#include "common.h"
#include "mpp.h"



#define D2NET_NOR_BOOT_BASE		0xfff80000
#define D2NET_NOR_BOOT_SIZE		SZ_512K



static struct mtd_partition d2net_partitions[] = {
	{
		.name		= "Full512kb",
		.size		= MTDPART_SIZ_FULL,
		.offset		= 0,
		.mask_flags	= MTD_WRITEABLE,
	},
};

static struct physmap_flash_data d2net_nor_flash_data = {
	.width		= 1,
	.parts		= d2net_partitions,
	.nr_parts	= ARRAY_SIZE(d2net_partitions),
};

static struct resource d2net_nor_flash_resource = {
	.flags			= IORESOURCE_MEM,
	.start			= D2NET_NOR_BOOT_BASE,
	.end			= D2NET_NOR_BOOT_BASE
					+ D2NET_NOR_BOOT_SIZE - 1,
};

static struct platform_device d2net_nor_flash = {
	.name			= "physmap-flash",
	.id			= 0,
	.dev		= {
		.platform_data	= &d2net_nor_flash_data,
	},
	.num_resources		= 1,
	.resource		= &d2net_nor_flash_resource,
};


static struct mv643xx_eth_platform_data d2net_eth_data = {
	.phy_addr	= MV643XX_ETH_PHY_ADDR(8),
};


static struct i2c_board_info __initdata d2net_i2c_devices[] = {
	{
		I2C_BOARD_INFO("rs5c372b", 0x32),
	}, {
		I2C_BOARD_INFO("24c08", 0x50),
	},
};


static struct mv_sata_platform_data d2net_sata_data = {
	.n_ports	= 2,
};

#define D2NET_GPIO_SATA0_POWER	3
#define D2NET_GPIO_SATA1_POWER	12

static void __init d2net_sata_power_init(void)
{
	int err;

	err = gpio_request(D2NET_GPIO_SATA0_POWER, "SATA0 power");
	if (err == 0) {
		err = gpio_direction_output(D2NET_GPIO_SATA0_POWER, 1);
		if (err)
			gpio_free(D2NET_GPIO_SATA0_POWER);
	}
	if (err)
		pr_err("d2net: failed to configure SATA0 power GPIO\n");

	err = gpio_request(D2NET_GPIO_SATA1_POWER, "SATA1 power");
	if (err == 0) {
		err = gpio_direction_output(D2NET_GPIO_SATA1_POWER, 1);
		if (err)
			gpio_free(D2NET_GPIO_SATA1_POWER);
	}
	if (err)
		pr_err("d2net: failed to configure SATA1 power GPIO\n");
}



#define D2NET_GPIO_RED_LED		6
#define D2NET_GPIO_BLUE_LED_BLINK_CTRL	16
#define D2NET_GPIO_BLUE_LED_OFF		23

static struct gpio_led d2net_leds[] = {
	{
		.name = "d2net:blue:sata",
		.default_trigger = "default-on",
		.gpio = D2NET_GPIO_BLUE_LED_OFF,
		.active_low = 1,
	},
	{
		.name = "d2net:red:fail",
		.gpio = D2NET_GPIO_RED_LED,
	},
};

static struct gpio_led_platform_data d2net_led_data = {
	.num_leds = ARRAY_SIZE(d2net_leds),
	.leds = d2net_leds,
};

static struct platform_device d2net_gpio_leds = {
	.name           = "leds-gpio",
	.id             = -1,
	.dev            = {
		.platform_data  = &d2net_led_data,
	},
};

static void __init d2net_gpio_leds_init(void)
{
	int err;

	
	orion_gpio_set_valid(D2NET_GPIO_BLUE_LED_OFF, 1);

	
	err = gpio_request(D2NET_GPIO_BLUE_LED_BLINK_CTRL, "blue LED blink");
	if (err == 0) {
		err = gpio_direction_output(D2NET_GPIO_BLUE_LED_BLINK_CTRL, 1);
		if (err)
			gpio_free(D2NET_GPIO_BLUE_LED_BLINK_CTRL);
	}
	if (err)
		pr_err("d2net: failed to configure blue LED blink GPIO\n");

	platform_device_register(&d2net_gpio_leds);
}


#define D2NET_GPIO_PUSH_BUTTON		18
#define D2NET_GPIO_POWER_SWITCH_ON	8
#define D2NET_GPIO_POWER_SWITCH_OFF	9

#define D2NET_SWITCH_POWER_ON		0x1
#define D2NET_SWITCH_POWER_OFF		0x2

static struct gpio_keys_button d2net_buttons[] = {
	{
		.type		= EV_SW,
		.code		= D2NET_SWITCH_POWER_OFF,
		.gpio		= D2NET_GPIO_POWER_SWITCH_OFF,
		.desc		= "Power rocker switch (auto|off)",
		.active_low	= 0,
	},
	{
		.type		= EV_SW,
		.code		= D2NET_SWITCH_POWER_ON,
		.gpio		= D2NET_GPIO_POWER_SWITCH_ON,
		.desc		= "Power rocker switch (on|auto)",
		.active_low	= 0,
	},
	{
		.type		= EV_KEY,
		.code		= KEY_POWER,
		.gpio		= D2NET_GPIO_PUSH_BUTTON,
		.desc		= "Front Push Button",
		.active_low	= 0,
	},
};

static struct gpio_keys_platform_data d2net_button_data = {
	.buttons	= d2net_buttons,
	.nbuttons	= ARRAY_SIZE(d2net_buttons),
};

static struct platform_device d2net_gpio_buttons = {
	.name		= "gpio-keys",
	.id		= -1,
	.dev		= {
		.platform_data	= &d2net_button_data,
	},
};


static unsigned int d2net_mpp_modes[] __initdata = {
	MPP0_GPIO,	
	MPP1_GPIO,	
	MPP2_GPIO,	
	MPP3_GPIO,	
	MPP4_UNUSED,
	MPP5_GPIO,	
	MPP6_GPIO,	
	MPP7_UNUSED,
	MPP8_GPIO,	
	MPP9_GPIO,	
	MPP10_UNUSED,
	MPP11_UNUSED,
	MPP12_GPIO,	
	MPP13_UNUSED,
	MPP14_SATA_LED,	
	MPP15_SATA_LED,	
	MPP16_GPIO,	
	MPP17_UNUSED,
	MPP18_GPIO,	
	MPP19_UNUSED,
	0,
	
	
	
};

#define D2NET_GPIO_INHIBIT_POWER_OFF    24

static void __init d2net_init(void)
{
	orion5x_init();

	orion5x_mpp_conf(d2net_mpp_modes);

	orion5x_ehci0_init();
	orion5x_eth_init(&d2net_eth_data);
	orion5x_i2c_init();
	orion5x_uart0_init();

	d2net_sata_power_init();
	orion5x_sata_init(&d2net_sata_data);

	orion5x_setup_dev_boot_win(D2NET_NOR_BOOT_BASE,
				D2NET_NOR_BOOT_SIZE);
	platform_device_register(&d2net_nor_flash);

	platform_device_register(&d2net_gpio_buttons);

	d2net_gpio_leds_init();

	pr_notice("d2net: Flash write are not yet supported.\n");

	i2c_register_board_info(0, d2net_i2c_devices,
				ARRAY_SIZE(d2net_i2c_devices));

	orion_gpio_set_valid(D2NET_GPIO_INHIBIT_POWER_OFF, 1);
}


#ifdef CONFIG_MACH_D2NET
MACHINE_START(D2NET, "LaCie d2 Network")
	.atag_offset	= 0x100,
	.init_machine	= d2net_init,
	.map_io		= orion5x_map_io,
	.init_early	= orion5x_init_early,
	.init_irq	= orion5x_init_irq,
	.timer		= &orion5x_timer,
	.fixup		= tag_fixup_mem32,
	.restart	= orion5x_restart,
MACHINE_END
#endif

#ifdef CONFIG_MACH_BIGDISK
MACHINE_START(BIGDISK, "LaCie Big Disk Network")
	.atag_offset	= 0x100,
	.init_machine	= d2net_init,
	.map_io		= orion5x_map_io,
	.init_early	= orion5x_init_early,
	.init_irq	= orion5x_init_irq,
	.timer		= &orion5x_timer,
	.fixup		= tag_fixup_mem32,
	.restart	= orion5x_restart,
MACHINE_END
#endif

