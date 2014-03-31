/*
 * arch/arm/mach-u300/spi.c
 *
 * Copyright (C) 2009 ST-Ericsson AB
 * License terms: GNU General Public License (GPL) version 2
 *
 * Author: Linus Walleij <linus.walleij@stericsson.com>
 */
#include <linux/device.h>
#include <linux/amba/bus.h>
#include <linux/spi/spi.h>
#include <linux/amba/pl022.h>
#include <linux/err.h>
#include <mach/coh901318.h>
#include <mach/dma_channels.h>

#ifdef CONFIG_MACH_U300_SPIDUMMY
static void select_dummy_chip(u32 chipselect)
{
	pr_debug("CORE: %s called with CS=0x%x (%s)\n",
		 __func__,
		 chipselect,
		 chipselect ? "unselect chip" : "select chip");
}

struct pl022_config_chip dummy_chip_info = {
	
	.com_mode = DMA_TRANSFER,
	.iface = SSP_INTERFACE_MOTOROLA_SPI,
	
	.hierarchy = SSP_MASTER,
	
	.slave_tx_disable = 0,
	.rx_lev_trig = SSP_RX_4_OR_MORE_ELEM,
	.tx_lev_trig = SSP_TX_4_OR_MORE_EMPTY_LOC,
	.ctrl_len = SSP_BITS_12,
	.wait_state = SSP_MWIRE_WAIT_ZERO,
	.duplex = SSP_MICROWIRE_CHANNEL_FULL_DUPLEX,
	.cs_control = select_dummy_chip,
};
#endif

static struct spi_board_info u300_spi_devices[] = {
#ifdef CONFIG_MACH_U300_SPIDUMMY
	{
		
		.modalias       = "spi-dummy",
		
		.platform_data  = NULL,
		
		.controller_data = &dummy_chip_info,
		
		.max_speed_hz   = 1000000,
		.bus_num        = 0, 
		.chip_select    = 0,
		
		.mode           = SPI_MODE_1 | SPI_LOOP,
	},
#endif
};

static struct pl022_ssp_controller ssp_platform_data = {
	
	.bus_id = 0,
	.num_chipselect = 3,
#ifdef CONFIG_COH901318
	.enable_dma = 1,
	.dma_filter = coh901318_filter_id,
	.dma_rx_param = (void *) U300_DMA_SPI_RX,
	.dma_tx_param = (void *) U300_DMA_SPI_TX,
#else
	.enable_dma = 0,
#endif
};


void __init u300_spi_init(struct amba_device *adev)
{
	adev->dev.platform_data = &ssp_platform_data;
}

void __init u300_spi_register_board_devices(void)
{
	
	spi_register_board_info(u300_spi_devices, ARRAY_SIZE(u300_spi_devices));
}
