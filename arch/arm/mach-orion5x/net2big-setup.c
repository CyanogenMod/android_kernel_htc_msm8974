/*
 * arch/arm/mach-orion5x/net2big-setup.c
 *
 * LaCie 2Big Network NAS setup
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
#include <linux/mtd/physmap.h>
#include <linux/mv643xx_eth.h>
#include <linux/leds.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/ata_platform.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <mach/orion5x.h>
#include "common.h"
#include "mpp.h"



#define NET2BIG_NOR_BOOT_BASE		0xfff80000
#define NET2BIG_NOR_BOOT_SIZE		SZ_512K



static struct mtd_partition net2big_partitions[] = {
	{
		.name		= "Full512kb",
		.size		= MTDPART_SIZ_FULL,
		.offset		= 0x00000000,
		.mask_flags	= MTD_WRITEABLE,
	},
};

static struct physmap_flash_data net2big_nor_flash_data = {
	.width		= 1,
	.parts		= net2big_partitions,
	.nr_parts	= ARRAY_SIZE(net2big_partitions),
};

static struct resource net2big_nor_flash_resource = {
	.flags			= IORESOURCE_MEM,
	.start			= NET2BIG_NOR_BOOT_BASE,
	.end			= NET2BIG_NOR_BOOT_BASE
					+ NET2BIG_NOR_BOOT_SIZE - 1,
};

static struct platform_device net2big_nor_flash = {
	.name			= "physmap-flash",
	.id			= 0,
	.dev		= {
		.platform_data	= &net2big_nor_flash_data,
	},
	.num_resources		= 1,
	.resource		= &net2big_nor_flash_resource,
};


static struct mv643xx_eth_platform_data net2big_eth_data = {
	.phy_addr	= MV643XX_ETH_PHY_ADDR(8),
};


static struct i2c_board_info __initdata net2big_i2c_devices[] = {
	{
		I2C_BOARD_INFO("rs5c372b", 0x32),
	}, {
		I2C_BOARD_INFO("24c08", 0x50),
	},
};


static struct mv_sata_platform_data net2big_sata_data = {
	.n_ports	= 2,
};

#define NET2BIG_GPIO_SATA_POWER_REQ	19
#define NET2BIG_GPIO_SATA0_POWER	23
#define NET2BIG_GPIO_SATA1_POWER	25

static void __init net2big_sata_power_init(void)
{
	int err;

	
	orion_gpio_set_valid(NET2BIG_GPIO_SATA0_POWER, 1);
	orion_gpio_set_valid(NET2BIG_GPIO_SATA1_POWER, 1);

	err = gpio_request(NET2BIG_GPIO_SATA0_POWER, "SATA0 power status");
	if (err == 0) {
		err = gpio_direction_input(NET2BIG_GPIO_SATA0_POWER);
		if (err)
			gpio_free(NET2BIG_GPIO_SATA0_POWER);
	}
	if (err) {
		pr_err("net2big: failed to setup SATA0 power GPIO\n");
		return;
	}

	err = gpio_request(NET2BIG_GPIO_SATA1_POWER, "SATA1 power status");
	if (err == 0) {
		err = gpio_direction_input(NET2BIG_GPIO_SATA1_POWER);
		if (err)
			gpio_free(NET2BIG_GPIO_SATA1_POWER);
	}
	if (err) {
		pr_err("net2big: failed to setup SATA1 power GPIO\n");
		goto err_free_1;
	}

	err = gpio_request(NET2BIG_GPIO_SATA_POWER_REQ, "SATA power request");
	if (err == 0) {
		err = gpio_direction_output(NET2BIG_GPIO_SATA_POWER_REQ, 0);
		if (err)
			gpio_free(NET2BIG_GPIO_SATA_POWER_REQ);
	}
	if (err) {
		pr_err("net2big: failed to setup SATA power request GPIO\n");
		goto err_free_2;
	}

	if (gpio_get_value(NET2BIG_GPIO_SATA0_POWER) &&
		gpio_get_value(NET2BIG_GPIO_SATA1_POWER)) {
		return;
	}

	msleep(300);
	gpio_set_value(NET2BIG_GPIO_SATA_POWER_REQ, 1);
	pr_info("net2big: power up SATA hard disks\n");

	return;

err_free_2:
	gpio_free(NET2BIG_GPIO_SATA1_POWER);
err_free_1:
	gpio_free(NET2BIG_GPIO_SATA0_POWER);

	return;
}



#define NET2BIG_GPIO_PWR_RED_LED	6
#define NET2BIG_GPIO_PWR_BLUE_LED	16
#define NET2BIG_GPIO_PWR_LED_BLINK_STOP	7

#define NET2BIG_GPIO_SATA0_RED_LED	11
#define NET2BIG_GPIO_SATA1_RED_LED	10

#define NET2BIG_GPIO_SATA0_BLUE_LED	17
#define NET2BIG_GPIO_SATA1_BLUE_LED	13

static struct gpio_led net2big_leds[] = {
	{
		.name = "net2big:red:power",
		.gpio = NET2BIG_GPIO_PWR_RED_LED,
	},
	{
		.name = "net2big:blue:power",
		.gpio = NET2BIG_GPIO_PWR_BLUE_LED,
	},
	{
		.name = "net2big:red:sata0",
		.gpio = NET2BIG_GPIO_SATA0_RED_LED,
	},
	{
		.name = "net2big:red:sata1",
		.gpio = NET2BIG_GPIO_SATA1_RED_LED,
	},
};

static struct gpio_led_platform_data net2big_led_data = {
	.num_leds = ARRAY_SIZE(net2big_leds),
	.leds = net2big_leds,
};

static struct platform_device net2big_gpio_leds = {
	.name           = "leds-gpio",
	.id             = -1,
	.dev            = {
		.platform_data  = &net2big_led_data,
	},
};

static void __init net2big_gpio_leds_init(void)
{
	int err;

	
	err = gpio_request(NET2BIG_GPIO_PWR_LED_BLINK_STOP,
			   "Power LED blink stop");
	if (err == 0) {
		err = gpio_direction_output(NET2BIG_GPIO_PWR_LED_BLINK_STOP, 1);
		if (err)
			gpio_free(NET2BIG_GPIO_PWR_LED_BLINK_STOP);
	}
	if (err)
		pr_err("net2big: failed to setup power LED blink GPIO\n");

	err = gpio_request(NET2BIG_GPIO_SATA0_BLUE_LED,
			   "SATA0 blue LED control");
	if (err == 0) {
		err = gpio_direction_output(NET2BIG_GPIO_SATA0_BLUE_LED, 1);
		if (err)
			gpio_free(NET2BIG_GPIO_SATA0_BLUE_LED);
	}
	if (err)
		pr_err("net2big: failed to setup SATA0 blue LED GPIO\n");

	err = gpio_request(NET2BIG_GPIO_SATA1_BLUE_LED,
			   "SATA1 blue LED control");
	if (err == 0) {
		err = gpio_direction_output(NET2BIG_GPIO_SATA1_BLUE_LED, 1);
		if (err)
			gpio_free(NET2BIG_GPIO_SATA1_BLUE_LED);
	}
	if (err)
		pr_err("net2big: failed to setup SATA1 blue LED GPIO\n");

	platform_device_register(&net2big_gpio_leds);
}


#define NET2BIG_GPIO_PUSH_BUTTON	18
#define NET2BIG_GPIO_POWER_SWITCH_ON	8
#define NET2BIG_GPIO_POWER_SWITCH_OFF	9

#define NET2BIG_SWITCH_POWER_ON		0x1
#define NET2BIG_SWITCH_POWER_OFF	0x2

static struct gpio_keys_button net2big_buttons[] = {
	{
		.type		= EV_SW,
		.code		= NET2BIG_SWITCH_POWER_OFF,
		.gpio		= NET2BIG_GPIO_POWER_SWITCH_OFF,
		.desc		= "Power rocker switch (auto|off)",
		.active_low	= 0,
	},
	{
		.type		= EV_SW,
		.code		= NET2BIG_SWITCH_POWER_ON,
		.gpio		= NET2BIG_GPIO_POWER_SWITCH_ON,
		.desc		= "Power rocker switch (on|auto)",
		.active_low	= 0,
	},
	{
		.type		= EV_KEY,
		.code		= KEY_POWER,
		.gpio		= NET2BIG_GPIO_PUSH_BUTTON,
		.desc		= "Front Push Button",
		.active_low	= 0,
	},
};

static struct gpio_keys_platform_data net2big_button_data = {
	.buttons	= net2big_buttons,
	.nbuttons	= ARRAY_SIZE(net2big_buttons),
};

static struct platform_device net2big_gpio_buttons = {
	.name		= "gpio-keys",
	.id		= -1,
	.dev		= {
		.platform_data	= &net2big_button_data,
	},
};


static unsigned int net2big_mpp_modes[] __initdata = {
	MPP0_GPIO,	
	MPP1_GPIO,	
	MPP2_GPIO,	
	MPP3_GPIO,	
	MPP4_GPIO,	
	MPP5_GPIO,	
	MPP6_GPIO,	
	MPP7_GPIO,	
	MPP8_GPIO,	
	MPP9_GPIO,	
	MPP10_GPIO,	
	MPP11_GPIO,	
	MPP12_GPIO,	
	MPP13_GPIO,	
	MPP14_SATA_LED,
	MPP15_SATA_LED,
	MPP16_GPIO,	
	MPP17_GPIO,	
	MPP18_GPIO,	
	MPP19_GPIO,	
	0,
	
	
	
	
};

#define NET2BIG_GPIO_POWER_OFF		24

static void net2big_power_off(void)
{
	gpio_set_value(NET2BIG_GPIO_POWER_OFF, 1);
}

static void __init net2big_init(void)
{
	orion5x_init();

	orion5x_mpp_conf(net2big_mpp_modes);

	orion5x_ehci0_init();
	orion5x_ehci1_init();
	orion5x_eth_init(&net2big_eth_data);
	orion5x_i2c_init();
	orion5x_uart0_init();
	orion5x_xor_init();

	net2big_sata_power_init();
	orion5x_sata_init(&net2big_sata_data);

	orion5x_setup_dev_boot_win(NET2BIG_NOR_BOOT_BASE,
				   NET2BIG_NOR_BOOT_SIZE);
	platform_device_register(&net2big_nor_flash);

	platform_device_register(&net2big_gpio_buttons);
	net2big_gpio_leds_init();

	i2c_register_board_info(0, net2big_i2c_devices,
				ARRAY_SIZE(net2big_i2c_devices));

	orion_gpio_set_valid(NET2BIG_GPIO_POWER_OFF, 1);

	if (gpio_request(NET2BIG_GPIO_POWER_OFF, "power-off") == 0 &&
	    gpio_direction_output(NET2BIG_GPIO_POWER_OFF, 0) == 0)
		pm_power_off = net2big_power_off;
	else
		pr_err("net2big: failed to configure power-off GPIO\n");

	pr_notice("net2big: Flash writing is not yet supported.\n");
}

MACHINE_START(NET2BIG, "LaCie 2Big Network")
	.atag_offset	= 0x100,
	.init_machine	= net2big_init,
	.map_io		= orion5x_map_io,
	.init_early	= orion5x_init_early,
	.init_irq	= orion5x_init_irq,
	.timer		= &orion5x_timer,
	.fixup		= tag_fixup_mem32,
	.restart	= orion5x_restart,
MACHINE_END

