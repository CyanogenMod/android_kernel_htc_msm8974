/*
 * arch/arm/mach-orion5x/ls_hgl-setup.c
 *
 * Maintainer: Zhu Qingsen <zhuqs@cn.fujitsu.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
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
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <mach/orion5x.h>
#include "common.h"
#include "mpp.h"



#define LS_HGL_NOR_BOOT_BASE	0xf4000000
#define LS_HGL_NOR_BOOT_SIZE	SZ_256K


static struct physmap_flash_data ls_hgl_nor_flash_data = {
	.width		= 1,
};

static struct resource ls_hgl_nor_flash_resource = {
	.flags	= IORESOURCE_MEM,
	.start	= LS_HGL_NOR_BOOT_BASE,
	.end	= LS_HGL_NOR_BOOT_BASE + LS_HGL_NOR_BOOT_SIZE - 1,
};

static struct platform_device ls_hgl_nor_flash = {
	.name			= "physmap-flash",
	.id			= 0,
	.dev		= {
		.platform_data	= &ls_hgl_nor_flash_data,
	},
	.num_resources		= 1,
	.resource		= &ls_hgl_nor_flash_resource,
};


static struct mv643xx_eth_platform_data ls_hgl_eth_data = {
	.phy_addr	= 8,
};


static struct i2c_board_info __initdata ls_hgl_i2c_rtc = {
	I2C_BOARD_INFO("rs5c372a", 0x32),
};


#define LS_HGL_GPIO_LED_ALARM   2
#define LS_HGL_GPIO_LED_INFO    3
#define LS_HGL_GPIO_LED_FUNC    17
#define LS_HGL_GPIO_LED_PWR     0


static struct gpio_led ls_hgl_led_pins[] = {
	{
		.name	   = "alarm:red",
		.gpio	   = LS_HGL_GPIO_LED_ALARM,
		.active_low     = 1,
	}, {
		.name	   = "info:amber",
		.gpio	   = LS_HGL_GPIO_LED_INFO,
		.active_low     = 1,
	}, {
		.name	   = "func:blue:top",
		.gpio	   = LS_HGL_GPIO_LED_FUNC,
		.active_low     = 1,
	}, {
		.name	   = "power:blue:bottom",
		.gpio	   = LS_HGL_GPIO_LED_PWR,
	},
};

static struct gpio_led_platform_data ls_hgl_led_data = {
	.leds	   = ls_hgl_led_pins,
	.num_leds       = ARRAY_SIZE(ls_hgl_led_pins),
};

static struct platform_device ls_hgl_leds = {
	.name   = "leds-gpio",
	.id     = -1,
	.dev    = {
		.platform_data  = &ls_hgl_led_data,
	},
};

#define LS_HGL_GPIO_KEY_FUNC       15
#define LS_HGL_GPIO_KEY_POWER      8
#define LS_HGL_GPIO_KEY_AUTOPOWER  10

#define LS_HGL_SW_POWER     0x00
#define LS_HGL_SW_AUTOPOWER 0x01

static struct gpio_keys_button ls_hgl_buttons[] = {
	{
		.code	   = KEY_OPTION,
		.gpio	   = LS_HGL_GPIO_KEY_FUNC,
		.desc	   = "Function Button",
		.active_low     = 1,
	}, {
		.type		= EV_SW,
		.code	   = LS_HGL_SW_POWER,
		.gpio	   = LS_HGL_GPIO_KEY_POWER,
		.desc	   = "Power-on Switch",
		.active_low     = 1,
	}, {
		.type		= EV_SW,
		.code	   = LS_HGL_SW_AUTOPOWER,
		.gpio	   = LS_HGL_GPIO_KEY_AUTOPOWER,
		.desc	   = "Power-auto Switch",
		.active_low     = 1,
	},
};

static struct gpio_keys_platform_data ls_hgl_button_data = {
	.buttons	= ls_hgl_buttons,
	.nbuttons       = ARRAY_SIZE(ls_hgl_buttons),
};

static struct platform_device ls_hgl_button_device = {
	.name	   = "gpio-keys",
	.id	     = -1,
	.num_resources  = 0,
	.dev	    = {
		.platform_data  = &ls_hgl_button_data,
	},
};


static struct mv_sata_platform_data ls_hgl_sata_data = {
	.n_ports	= 2,
};



static void ls_hgl_power_off(void)
{
	orion5x_restart('h', NULL);
}



#define LS_HGL_GPIO_USB_POWER	9
#define LS_HGL_GPIO_AUTO_POWER	10
#define LS_HGL_GPIO_POWER	    8

#define LS_HGL_GPIO_HDD_POWER	1

static unsigned int ls_hgl_mpp_modes[] __initdata = {
	MPP0_GPIO, 
	MPP1_GPIO, 
	MPP2_GPIO, 
	MPP3_GPIO, 
	MPP4_UNUSED,
	MPP5_UNUSED,
	MPP6_GPIO, 
	MPP7_GPIO, 
	MPP8_GPIO, 
	MPP9_GPIO, 
	MPP10_GPIO, 
	MPP11_UNUSED, 
	MPP12_UNUSED,
	MPP13_UNUSED,
	MPP14_UNUSED,
	MPP15_GPIO, 
	MPP16_UNUSED,
	MPP17_GPIO, 
	MPP18_UNUSED,
	MPP19_UNUSED,
	0,
};

static void __init ls_hgl_init(void)
{
	orion5x_init();

	orion5x_mpp_conf(ls_hgl_mpp_modes);

	orion5x_ehci0_init();
	orion5x_ehci1_init();
	orion5x_eth_init(&ls_hgl_eth_data);
	orion5x_i2c_init();
	orion5x_sata_init(&ls_hgl_sata_data);
	orion5x_uart0_init();
	orion5x_xor_init();

	orion5x_setup_dev_boot_win(LS_HGL_NOR_BOOT_BASE,
				   LS_HGL_NOR_BOOT_SIZE);
	platform_device_register(&ls_hgl_nor_flash);

	platform_device_register(&ls_hgl_button_device);

	platform_device_register(&ls_hgl_leds);

	i2c_register_board_info(0, &ls_hgl_i2c_rtc, 1);

	
	gpio_set_value(LS_HGL_GPIO_USB_POWER, 1);

	
	pm_power_off = ls_hgl_power_off;

	pr_info("%s: finished\n", __func__);
}

MACHINE_START(LINKSTATION_LS_HGL, "Buffalo Linkstation LS-HGL")
	
	.atag_offset	= 0x100,
	.init_machine	= ls_hgl_init,
	.map_io		= orion5x_map_io,
	.init_early	= orion5x_init_early,
	.init_irq	= orion5x_init_irq,
	.timer		= &orion5x_timer,
	.fixup		= tag_fixup_mem32,
	.restart	= orion5x_restart,
MACHINE_END
