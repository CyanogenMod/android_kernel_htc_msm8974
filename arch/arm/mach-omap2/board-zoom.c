/*
 * Copyright (C) 2009-2010 Texas Instruments Inc.
 * Mikkel Christensen <mlc@ti.com>
 * Felipe Balbi <balbi@ti.com>
 *
 * Modified from mach-omap2/board-ldp.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/i2c/twl.h>
#include <linux/mtd/nand.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include "common.h"
#include <plat/board.h>
#include <plat/usb.h>

#include <mach/board-zoom.h>

#include "board-flash.h"
#include "mux.h"
#include "sdram-micron-mt46h32m32lf-6.h"
#include "sdram-hynix-h8mbx00u0mer-0em.h"

#define ZOOM3_EHCI_RESET_GPIO		64

#ifdef CONFIG_OMAP_MUX
static struct omap_board_mux board_mux[] __initdata = {
	
	OMAP3_MUX(MCBSP1_CLKX, OMAP_MUX_MODE4 | OMAP_PIN_INPUT),
	
	OMAP3_MUX(CAM_D2, OMAP_MUX_MODE4 | OMAP_PIN_OUTPUT),
	
	OMAP3_MUX(MCSPI1_CS1, OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLUP),
	
	OMAP3_MUX(ETK_CLK, OMAP_MUX_MODE2 | OMAP_PIN_INPUT_PULLUP),
	
	OMAP3_MUX(ETK_D3, OMAP_MUX_MODE2 | OMAP_PIN_INPUT_PULLUP),
	OMAP3_MUX(ETK_D4, OMAP_MUX_MODE2 | OMAP_PIN_INPUT_PULLUP),
	OMAP3_MUX(ETK_D5, OMAP_MUX_MODE2 | OMAP_PIN_INPUT_PULLUP),
	OMAP3_MUX(ETK_D6, OMAP_MUX_MODE2 | OMAP_PIN_INPUT_PULLUP),
	{ .reg_offset = OMAP_MUX_TERMINATOR },
};
#endif

static struct mtd_partition zoom_nand_partitions[] = {
	
	{
		.name		= "X-Loader-NAND",
		.offset		= 0,
		.size		= 4 * (64 * 2048),	
		.mask_flags	= MTD_WRITEABLE,	
	},
	{
		.name		= "U-Boot-NAND",
		.offset		= MTDPART_OFS_APPEND,	
		.size		= 10 * (64 * 2048),	
		.mask_flags	= MTD_WRITEABLE,	
	},
	{
		.name		= "Boot Env-NAND",
		.offset		= MTDPART_OFS_APPEND,   
		.size		= 2 * (64 * 2048),	
	},
	{
		.name		= "Kernel-NAND",
		.offset		= MTDPART_OFS_APPEND,	
		.size		= 240 * (64 * 2048),	
	},
	{
		.name		= "system",
		.offset		= MTDPART_OFS_APPEND,	
		.size		= 3328 * (64 * 2048),	
	},
	{
		.name		= "userdata",
		.offset		= MTDPART_OFS_APPEND,	
		.size		= 256 * (64 * 2048),	
	},
	{
		.name		= "cache",
		.offset		= MTDPART_OFS_APPEND,	
		.size		= 256 * (64 * 2048),	
	},
};

static const struct usbhs_omap_board_data usbhs_bdata __initconst = {
	.port_mode[0]		= OMAP_USBHS_PORT_MODE_UNUSED,
	.port_mode[1]		= OMAP_EHCI_PORT_MODE_PHY,
	.port_mode[2]		= OMAP_USBHS_PORT_MODE_UNUSED,
	.phy_reset		= true,
	.reset_gpio_port[0]	= -EINVAL,
	.reset_gpio_port[1]	= ZOOM3_EHCI_RESET_GPIO,
	.reset_gpio_port[2]	= -EINVAL,
};

static void __init omap_zoom_init(void)
{
	if (machine_is_omap_zoom2()) {
		omap3_mux_init(board_mux, OMAP_PACKAGE_CBB);
	} else if (machine_is_omap_zoom3()) {
		omap3_mux_init(board_mux, OMAP_PACKAGE_CBP);
		omap_mux_init_gpio(ZOOM3_EHCI_RESET_GPIO, OMAP_PIN_OUTPUT);
		usbhs_init(&usbhs_bdata);
	}

	board_nand_init(zoom_nand_partitions, ARRAY_SIZE(zoom_nand_partitions),
						ZOOM_NAND_CS, NAND_BUSWIDTH_16);
	zoom_debugboard_init();
	zoom_peripherals_init();

	if (machine_is_omap_zoom2())
		omap_sdrc_init(mt46h32m32lf6_sdrc_params,
					  mt46h32m32lf6_sdrc_params);
	else if (machine_is_omap_zoom3())
		omap_sdrc_init(h8mbx00u0mer0em_sdrc_params,
					  h8mbx00u0mer0em_sdrc_params);

	zoom_display_init();
}

MACHINE_START(OMAP_ZOOM2, "OMAP Zoom2 board")
	.atag_offset	= 0x100,
	.reserve	= omap_reserve,
	.map_io		= omap3_map_io,
	.init_early	= omap3430_init_early,
	.init_irq	= omap3_init_irq,
	.handle_irq	= omap3_intc_handle_irq,
	.init_machine	= omap_zoom_init,
	.timer		= &omap3_timer,
	.restart	= omap_prcm_restart,
MACHINE_END

MACHINE_START(OMAP_ZOOM3, "OMAP Zoom3 board")
	.atag_offset	= 0x100,
	.reserve	= omap_reserve,
	.map_io		= omap3_map_io,
	.init_early	= omap3630_init_early,
	.init_irq	= omap3_init_irq,
	.handle_irq	= omap3_intc_handle_irq,
	.init_machine	= omap_zoom_init,
	.timer		= &omap3_timer,
	.restart	= omap_prcm_restart,
MACHINE_END
