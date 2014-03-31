/*
 * arch/arm/mach-omap2/serial.c
 *
 * OMAP2 serial support.
 *
 * Copyright (C) 2005-2008 Nokia Corporation
 * Author: Paul Mundt <paul.mundt@nokia.com>
 *
 * Major rework for PM support by Kevin Hilman
 *
 * Based off of arch/arm/mach-omap/omap1/serial.c
 *
 * Copyright (C) 2009 Texas Instruments
 * Added OMAP4 support - Santosh Shilimkar <santosh.shilimkar@ti.com
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/console.h>

#include <plat/omap-serial.h>
#include "common.h"
#include <plat/board.h>
#include <plat/dma.h>
#include <plat/omap_hwmod.h>
#include <plat/omap_device.h>
#include <plat/omap-pm.h>

#include "prm2xxx_3xxx.h"
#include "pm.h"
#include "cm2xxx_3xxx.h"
#include "prm-regbits-34xx.h"
#include "control.h"
#include "mux.h"

#define DEFAULT_AUTOSUSPEND_DELAY	-1

#define MAX_UART_HWMOD_NAME_LEN		16

struct omap_uart_state {
	int num;

	struct list_head node;
	struct omap_hwmod *oh;
};

static LIST_HEAD(uart_list);
static u8 num_uarts;
static u8 console_uart_id = -1;
static u8 no_console_suspend;
static u8 uart_debug;

#define DEFAULT_RXDMA_POLLRATE		1	
#define DEFAULT_RXDMA_BUFSIZE		4096	
#define DEFAULT_RXDMA_TIMEOUT		(3 * HZ)

static struct omap_uart_port_info omap_serial_default_info[] __initdata = {
	{
		.dma_enabled	= false,
		.dma_rx_buf_size = DEFAULT_RXDMA_BUFSIZE,
		.dma_rx_poll_rate = DEFAULT_RXDMA_POLLRATE,
		.dma_rx_timeout = DEFAULT_RXDMA_TIMEOUT,
		.autosuspend_timeout = DEFAULT_AUTOSUSPEND_DELAY,
	},
};

#ifdef CONFIG_PM
static void omap_uart_enable_wakeup(struct platform_device *pdev, bool enable)
{
	struct omap_device *od = to_omap_device(pdev);

	if (!od)
		return;

	if (enable)
		omap_hwmod_enable_wakeup(od->hwmods[0]);
	else
		omap_hwmod_disable_wakeup(od->hwmods[0]);
}

static void omap_uart_set_noidle(struct platform_device *pdev)
{
	struct omap_device *od = to_omap_device(pdev);

	omap_hwmod_set_slave_idlemode(od->hwmods[0], HWMOD_IDLEMODE_NO);
}

static void omap_uart_set_smartidle(struct platform_device *pdev)
{
	struct omap_device *od = to_omap_device(pdev);
	u8 idlemode;

	if (od->hwmods[0]->class->sysc->idlemodes & SIDLE_SMART_WKUP)
		idlemode = HWMOD_IDLEMODE_SMART_WKUP;
	else
		idlemode = HWMOD_IDLEMODE_SMART;

	omap_hwmod_set_slave_idlemode(od->hwmods[0], idlemode);
}

#else
static void omap_uart_enable_wakeup(struct platform_device *pdev, bool enable)
{}
static void omap_uart_set_noidle(struct platform_device *pdev) {}
static void omap_uart_set_smartidle(struct platform_device *pdev) {}
#endif 

#ifdef CONFIG_OMAP_MUX
static void omap_serial_fill_default_pads(struct omap_board_data *bdata)
{
}
#else
static void omap_serial_fill_default_pads(struct omap_board_data *bdata) {}
#endif

char *cmdline_find_option(char *str)
{
	extern char *saved_command_line;

	return strstr(saved_command_line, str);
}

static int __init omap_serial_early_init(void)
{
	do {
		char oh_name[MAX_UART_HWMOD_NAME_LEN];
		struct omap_hwmod *oh;
		struct omap_uart_state *uart;
		char uart_name[MAX_UART_HWMOD_NAME_LEN];

		snprintf(oh_name, MAX_UART_HWMOD_NAME_LEN,
			 "uart%d", num_uarts + 1);
		oh = omap_hwmod_lookup(oh_name);
		if (!oh)
			break;

		uart = kzalloc(sizeof(struct omap_uart_state), GFP_KERNEL);
		if (WARN_ON(!uart))
			return -ENODEV;

		uart->oh = oh;
		uart->num = num_uarts++;
		list_add_tail(&uart->node, &uart_list);
		snprintf(uart_name, MAX_UART_HWMOD_NAME_LEN,
				"%s%d", OMAP_SERIAL_NAME, uart->num);

		if (cmdline_find_option(uart_name)) {
			console_uart_id = uart->num;

			if (console_loglevel >= 10) {
				uart_debug = true;
				pr_info("%s used as console in debug mode"
						" uart%d clocks will not be"
						" gated", uart_name, uart->num);
			}

			if (cmdline_find_option("no_console_suspend"))
				no_console_suspend = true;

			oh->flags |= HWMOD_INIT_NO_IDLE | HWMOD_INIT_NO_RESET;
		}
	} while (1);

	return 0;
}
core_initcall(omap_serial_early_init);

void __init omap_serial_init_port(struct omap_board_data *bdata,
			struct omap_uart_port_info *info)
{
	struct omap_uart_state *uart;
	struct omap_hwmod *oh;
	struct platform_device *pdev;
	void *pdata = NULL;
	u32 pdata_size = 0;
	char *name;
	struct omap_uart_port_info omap_up;

	if (WARN_ON(!bdata))
		return;
	if (WARN_ON(bdata->id < 0))
		return;
	if (WARN_ON(bdata->id >= num_uarts))
		return;

	list_for_each_entry(uart, &uart_list, node)
		if (bdata->id == uart->num)
			break;
	if (!info)
		info = omap_serial_default_info;

	oh = uart->oh;
	name = DRIVER_NAME;

	omap_up.dma_enabled = info->dma_enabled;
	omap_up.uartclk = OMAP24XX_BASE_BAUD * 16;
	omap_up.flags = UPF_BOOT_AUTOCONF;
	omap_up.get_context_loss_count = omap_pm_get_dev_context_loss_count;
	omap_up.set_forceidle = omap_uart_set_smartidle;
	omap_up.set_noidle = omap_uart_set_noidle;
	omap_up.enable_wakeup = omap_uart_enable_wakeup;
	omap_up.dma_rx_buf_size = info->dma_rx_buf_size;
	omap_up.dma_rx_timeout = info->dma_rx_timeout;
	omap_up.dma_rx_poll_rate = info->dma_rx_poll_rate;
	omap_up.autosuspend_timeout = info->autosuspend_timeout;

	
	if (!cpu_is_omap2420() && !cpu_is_ti816x())
		omap_up.errata |= UART_ERRATA_i202_MDR1_ACCESS;

	
	if (cpu_is_omap34xx() || cpu_is_omap3630())
		omap_up.errata |= UART_ERRATA_i291_DMA_FORCEIDLE;

	pdata = &omap_up;
	pdata_size = sizeof(struct omap_uart_port_info);

	if (WARN_ON(!oh))
		return;

	pdev = omap_device_build(name, uart->num, oh, pdata, pdata_size,
				 NULL, 0, false);
	WARN(IS_ERR(pdev), "Could not build omap_device for %s: %s.\n",
	     name, oh->name);

	if ((console_uart_id == bdata->id) && no_console_suspend)
		omap_device_disable_idle_on_suspend(pdev);

	oh->mux = omap_hwmod_mux_init(bdata->pads, bdata->pads_cnt);

	oh->dev_attr = uart;

	if (((cpu_is_omap34xx() || cpu_is_omap44xx()) && bdata->pads)
			&& !uart_debug)
		device_init_wakeup(&pdev->dev, true);
}

void __init omap_serial_board_init(struct omap_uart_port_info *info)
{
	struct omap_uart_state *uart;
	struct omap_board_data bdata;

	list_for_each_entry(uart, &uart_list, node) {
		bdata.id = uart->num;
		bdata.flags = 0;
		bdata.pads = NULL;
		bdata.pads_cnt = 0;

		if (cpu_is_omap44xx() || cpu_is_omap34xx())
			omap_serial_fill_default_pads(&bdata);

		if (!info)
			omap_serial_init_port(&bdata, NULL);
		else
			omap_serial_init_port(&bdata, &info[uart->num]);
	}
}

void __init omap_serial_init(void)
{
	omap_serial_board_init(NULL);
}
