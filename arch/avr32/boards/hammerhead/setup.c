/*
 * Board-specific setup code for the Miromico Hammerhead board
 *
 * Copyright (C) 2008 Miromico AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/atmel-mci.h>
#include <linux/clk.h>
#include <linux/fb.h>
#include <linux/etherdevice.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/init.h>
#include <linux/linkage.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/spi/spi.h>

#include <video/atmel_lcdc.h>

#include <linux/io.h>
#include <asm/setup.h>

#include <mach/at32ap700x.h>
#include <mach/board.h>
#include <mach/init.h>
#include <mach/portmux.h>

#include <sound/atmel-ac97c.h>

#include "../../mach-at32ap/clock.h"
#include "flash.h"

unsigned long at32_board_osc_rates[3] = {
	[0] = 32768,	
	[1] = 25000000, 
	[2] = 12000000,	
};

struct tag *bootloader_tags __initdata;

#ifdef CONFIG_BOARD_HAMMERHEAD_LCD
static struct fb_videomode __initdata hda350tlv_modes[] = {
	{
		.name		= "320x240 @ 75",
		.refresh	= 75,
		.xres		= 320,
		.yres		= 240,
		.pixclock	= KHZ2PICOS(6891),

		.left_margin	= 48,
		.right_margin	= 18,
		.upper_margin	= 18,
		.lower_margin	= 4,
		.hsync_len	= 20,
		.vsync_len	= 2,

		.sync		= 0,
		.vmode		= FB_VMODE_NONINTERLACED,
	},
};

static struct fb_monspecs __initdata hammerhead_hda350t_monspecs = {
	.manufacturer		= "HAN",
	.monitor		= "HDA350T-LV",
	.modedb			= hda350tlv_modes,
	.modedb_len		= ARRAY_SIZE(hda350tlv_modes),
	.hfmin			= 14900,
	.hfmax			= 22350,
	.vfmin			= 60,
	.vfmax			= 90,
	.dclkmax		= 10000000,
};

struct atmel_lcdfb_info __initdata hammerhead_lcdc_data = {
	.default_bpp		= 24,
	.default_dmacon		= ATMEL_LCDC_DMAEN | ATMEL_LCDC_DMA2DEN,
	.default_lcdcon2	= (ATMEL_LCDC_DISTYPE_TFT
				   | ATMEL_LCDC_INVCLK
				   | ATMEL_LCDC_CLKMOD_ALWAYSACTIVE
				   | ATMEL_LCDC_MEMOR_BIG),
	.default_monspecs	= &hammerhead_hda350t_monspecs,
	.guard_time		= 2,
};
#endif

static struct mci_platform_data __initdata mci0_data = {
	.slot[0] = {
		.bus_width	= 4,
		.detect_pin	= -ENODEV,
		.wp_pin		= -ENODEV,
	},
};

struct eth_addr {
	u8 addr[6];
};

static struct eth_addr __initdata hw_addr[1];
static struct macb_platform_data __initdata eth_data[1];

static int __init parse_tag_ethernet(struct tag *tag)
{
	int i = tag->u.ethernet.mac_index;

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

	__raw_writel((addr[3] << 24) | (addr[2] << 16) | (addr[1] << 8) |
		     addr[0], regs + 0x98);
	__raw_writel((addr[5] << 8) | addr[4], regs + 0x9c);

	clk_disable(pclk);
	clk_put(pclk);
}

void __init setup_board(void)
{
	at32_map_usart(1, 0, 0);	
	at32_setup_serial_console(0);
}

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
	.dev		= { .platform_data = &i2c_gpio_data, },
};

static struct i2c_board_info __initdata i2c_info[] = {};

#ifdef CONFIG_BOARD_HAMMERHEAD_SND
static struct ac97c_platform_data ac97c_data = {
	.reset_pin = GPIO_PIN_PA(16),
};
#endif

static int __init hammerhead_init(void)
{
	at32_reserve_pin(GPIO_PIOE_BASE, ATMEL_EBI_PE_DATA_ALL);

	at32_add_device_usart(0);

	at32_reserve_pin(GPIO_PIOB_BASE, (1<<29));

	set_hw_addr(at32_add_device_eth(0, &eth_data[0]));

#ifdef CONFIG_BOARD_HAMMERHEAD_FPGA
	at32_add_device_hh_fpga();
#endif
	at32_add_device_mci(0, &mci0_data);

#ifdef CONFIG_BOARD_HAMMERHEAD_USB
	at32_add_device_usba(0, NULL);
#endif
#ifdef CONFIG_BOARD_HAMMERHEAD_LCD
	at32_add_device_lcdc(0, &hammerhead_lcdc_data, fbmem_start,
			     fbmem_size, ATMEL_LCDC_PRI_24BIT);
#endif

	at32_select_gpio(i2c_gpio_data.sda_pin,
			 AT32_GPIOF_MULTIDRV | AT32_GPIOF_OUTPUT |
			 AT32_GPIOF_HIGH);
	at32_select_gpio(i2c_gpio_data.scl_pin,
			 AT32_GPIOF_MULTIDRV | AT32_GPIOF_OUTPUT |
			 AT32_GPIOF_HIGH);
	platform_device_register(&i2c_gpio_device);
	i2c_register_board_info(0, i2c_info, ARRAY_SIZE(i2c_info));

#ifdef CONFIG_BOARD_HAMMERHEAD_SND
	at32_add_device_ac97c(0, &ac97c_data, AC97C_BOTH);
#endif

	
	at32_select_periph(GPIO_PIOB_BASE, 0x08000000, GPIO_PERIPH_A, 0);

	return 0;
}

postcore_initcall(hammerhead_init);
