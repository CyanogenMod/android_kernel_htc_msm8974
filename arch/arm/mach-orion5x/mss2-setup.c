/*
 * Maxtor Shared Storage II Board Setup
 *
 * Maintainer: Sylver Bruneau <sylver.bruneau@googlemail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
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
#include <mach/bridge-regs.h>
#include "common.h"
#include "mpp.h"

#define MSS2_NOR_BOOT_BASE	0xff800000
#define MSS2_NOR_BOOT_SIZE	SZ_256K




static struct physmap_flash_data mss2_nor_flash_data = {
	.width		= 1,
};

static struct resource mss2_nor_flash_resource = {
	.flags		= IORESOURCE_MEM,
	.start		= MSS2_NOR_BOOT_BASE,
	.end		= MSS2_NOR_BOOT_BASE + MSS2_NOR_BOOT_SIZE - 1,
};

static struct platform_device mss2_nor_flash = {
	.name		= "physmap-flash",
	.id		= 0,
	.dev		= {
		.platform_data	= &mss2_nor_flash_data,
	},
	.resource	= &mss2_nor_flash_resource,
	.num_resources	= 1,
};

static int __init mss2_pci_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	int irq;

	irq = orion5x_pci_map_irq(dev, slot, pin);
	if (irq != -1)
		return irq;

	return -1;
}

static struct hw_pci mss2_pci __initdata = {
	.nr_controllers = 2,
	.swizzle	= pci_std_swizzle,
	.setup		= orion5x_pci_sys_setup,
	.scan		= orion5x_pci_sys_scan_bus,
	.map_irq	= mss2_pci_map_irq,
};

static int __init mss2_pci_init(void)
{
	if (machine_is_mss2())
		pci_common_init(&mss2_pci);

	return 0;
}
subsys_initcall(mss2_pci_init);



static struct mv643xx_eth_platform_data mss2_eth_data = {
	.phy_addr	= MV643XX_ETH_PHY_ADDR(8),
};


static struct mv_sata_platform_data mss2_sata_data = {
	.n_ports	= 2,
};


#define MSS2_GPIO_KEY_RESET	12
#define MSS2_GPIO_KEY_POWER	11

static struct gpio_keys_button mss2_buttons[] = {
	{
		.code		= KEY_POWER,
		.gpio		= MSS2_GPIO_KEY_POWER,
		.desc		= "Power",
		.active_low	= 1,
	}, {
		.code		= KEY_RESTART,
		.gpio		= MSS2_GPIO_KEY_RESET,
		.desc		= "Reset",
		.active_low	= 1,
	},
};

static struct gpio_keys_platform_data mss2_button_data = {
	.buttons	= mss2_buttons,
	.nbuttons	= ARRAY_SIZE(mss2_buttons),
};

static struct platform_device mss2_button_device = {
	.name		= "gpio-keys",
	.id		= -1,
	.dev		= {
		.platform_data	= &mss2_button_data,
	},
};


#define MSS2_GPIO_RTC_IRQ	3

static struct i2c_board_info __initdata mss2_i2c_rtc = {
	I2C_BOARD_INFO("m41t81", 0x68),
};

static void mss2_power_off(void)
{
	u32 reg;

	reg = readl(RSTOUTn_MASK);
	reg |= 1 << 2;
	writel(reg, RSTOUTn_MASK);

	reg = readl(CPU_SOFT_RESET);
	reg |= 1;
	writel(reg, CPU_SOFT_RESET);
}

static unsigned int mss2_mpp_modes[] __initdata = {
	MPP0_GPIO,		
	MPP1_GPIO,		
	MPP2_UNUSED,
	MPP3_GPIO,		
	MPP4_GPIO,		
	MPP5_GPIO,		
	MPP6_GPIO,		
	MPP7_GPIO,		
	MPP8_GPIO,		
	MPP9_UNUSED,
	MPP10_GPIO,		
	MPP11_GPIO,		
	MPP12_GPIO,		
	MPP13_UNUSED,
	MPP14_SATA_LED,		
	MPP15_SATA_LED,		
	MPP16_UNUSED,
	MPP17_UNUSED,
	MPP18_UNUSED,
	MPP19_UNUSED,
	0,
};

static void __init mss2_init(void)
{
	
	orion5x_init();

	orion5x_mpp_conf(mss2_mpp_modes);


	orion5x_ehci0_init();
	orion5x_ehci1_init();
	orion5x_eth_init(&mss2_eth_data);
	orion5x_i2c_init();
	orion5x_sata_init(&mss2_sata_data);
	orion5x_uart0_init();
	orion5x_xor_init();

	orion5x_setup_dev_boot_win(MSS2_NOR_BOOT_BASE, MSS2_NOR_BOOT_SIZE);
	platform_device_register(&mss2_nor_flash);

	platform_device_register(&mss2_button_device);

	if (gpio_request(MSS2_GPIO_RTC_IRQ, "rtc") == 0) {
		if (gpio_direction_input(MSS2_GPIO_RTC_IRQ) == 0)
			mss2_i2c_rtc.irq = gpio_to_irq(MSS2_GPIO_RTC_IRQ);
		else
			gpio_free(MSS2_GPIO_RTC_IRQ);
	}
	i2c_register_board_info(0, &mss2_i2c_rtc, 1);

	
	pm_power_off = mss2_power_off;
}

MACHINE_START(MSS2, "Maxtor Shared Storage II")
	
	.atag_offset	= 0x100,
	.init_machine	= mss2_init,
	.map_io		= orion5x_map_io,
	.init_early	= orion5x_init_early,
	.init_irq	= orion5x_init_irq,
	.timer		= &orion5x_timer,
	.fixup		= tag_fixup_mem32,
	.restart	= orion5x_restart,
MACHINE_END
