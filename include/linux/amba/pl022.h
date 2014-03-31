/*
 * include/linux/amba/pl022.h
 *
 * Copyright (C) 2008-2009 ST-Ericsson AB
 * Copyright (C) 2006 STMicroelectronics Pvt. Ltd.
 *
 * Author: Linus Walleij <linus.walleij@stericsson.com>
 *
 * Initial version inspired by:
 *	linux-2.6.17-rc3-mm1/drivers/spi/pxa2xx_spi.c
 * Initial adoption to PL022 by:
 *      Sachin Verma <sachin.verma@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _SSP_PL022_H
#define _SSP_PL022_H

#include <linux/types.h>

enum ssp_loopback {
	LOOPBACK_DISABLED,
	LOOPBACK_ENABLED
};

enum ssp_interface {
	SSP_INTERFACE_MOTOROLA_SPI,
	SSP_INTERFACE_TI_SYNC_SERIAL,
	SSP_INTERFACE_NATIONAL_MICROWIRE,
	SSP_INTERFACE_UNIDIRECTIONAL
};

enum ssp_hierarchy {
	SSP_MASTER,
	SSP_SLAVE
};

struct ssp_clock_params {
	u8 cpsdvsr; 
	u8 scr;	    
};

enum ssp_rx_endian {
	SSP_RX_MSB,
	SSP_RX_LSB
};

enum ssp_tx_endian {
	SSP_TX_MSB,
	SSP_TX_LSB
};

enum ssp_data_size {
	SSP_DATA_BITS_4 = 0x03, SSP_DATA_BITS_5, SSP_DATA_BITS_6,
	SSP_DATA_BITS_7, SSP_DATA_BITS_8, SSP_DATA_BITS_9,
	SSP_DATA_BITS_10, SSP_DATA_BITS_11, SSP_DATA_BITS_12,
	SSP_DATA_BITS_13, SSP_DATA_BITS_14, SSP_DATA_BITS_15,
	SSP_DATA_BITS_16, SSP_DATA_BITS_17, SSP_DATA_BITS_18,
	SSP_DATA_BITS_19, SSP_DATA_BITS_20, SSP_DATA_BITS_21,
	SSP_DATA_BITS_22, SSP_DATA_BITS_23, SSP_DATA_BITS_24,
	SSP_DATA_BITS_25, SSP_DATA_BITS_26, SSP_DATA_BITS_27,
	SSP_DATA_BITS_28, SSP_DATA_BITS_29, SSP_DATA_BITS_30,
	SSP_DATA_BITS_31, SSP_DATA_BITS_32
};

enum ssp_mode {
	INTERRUPT_TRANSFER,
	POLLING_TRANSFER,
	DMA_TRANSFER
};

enum ssp_rx_level_trig {
	SSP_RX_1_OR_MORE_ELEM,
	SSP_RX_4_OR_MORE_ELEM,
	SSP_RX_8_OR_MORE_ELEM,
	SSP_RX_16_OR_MORE_ELEM,
	SSP_RX_32_OR_MORE_ELEM
};

enum ssp_tx_level_trig {
	SSP_TX_1_OR_MORE_EMPTY_LOC,
	SSP_TX_4_OR_MORE_EMPTY_LOC,
	SSP_TX_8_OR_MORE_EMPTY_LOC,
	SSP_TX_16_OR_MORE_EMPTY_LOC,
	SSP_TX_32_OR_MORE_EMPTY_LOC
};

enum ssp_spi_clk_phase {
	SSP_CLK_FIRST_EDGE,
	SSP_CLK_SECOND_EDGE
};

enum ssp_spi_clk_pol {
	SSP_CLK_POL_IDLE_LOW,
	SSP_CLK_POL_IDLE_HIGH
};

enum ssp_microwire_ctrl_len {
	SSP_BITS_4 = 0x03, SSP_BITS_5, SSP_BITS_6,
	SSP_BITS_7, SSP_BITS_8, SSP_BITS_9,
	SSP_BITS_10, SSP_BITS_11, SSP_BITS_12,
	SSP_BITS_13, SSP_BITS_14, SSP_BITS_15,
	SSP_BITS_16, SSP_BITS_17, SSP_BITS_18,
	SSP_BITS_19, SSP_BITS_20, SSP_BITS_21,
	SSP_BITS_22, SSP_BITS_23, SSP_BITS_24,
	SSP_BITS_25, SSP_BITS_26, SSP_BITS_27,
	SSP_BITS_28, SSP_BITS_29, SSP_BITS_30,
	SSP_BITS_31, SSP_BITS_32
};

enum ssp_microwire_wait_state {
	SSP_MWIRE_WAIT_ZERO,
	SSP_MWIRE_WAIT_ONE
};

enum ssp_duplex {
	SSP_MICROWIRE_CHANNEL_FULL_DUPLEX,
	SSP_MICROWIRE_CHANNEL_HALF_DUPLEX
};

enum ssp_clkdelay {
	SSP_FEEDBACK_CLK_DELAY_NONE,
	SSP_FEEDBACK_CLK_DELAY_1T,
	SSP_FEEDBACK_CLK_DELAY_2T,
	SSP_FEEDBACK_CLK_DELAY_3T,
	SSP_FEEDBACK_CLK_DELAY_4T,
	SSP_FEEDBACK_CLK_DELAY_5T,
	SSP_FEEDBACK_CLK_DELAY_6T,
	SSP_FEEDBACK_CLK_DELAY_7T
};

enum ssp_chip_select {
	SSP_CHIP_SELECT,
	SSP_CHIP_DESELECT
};


struct dma_chan;
struct pl022_ssp_controller {
	u16 bus_id;
	u8 num_chipselect;
	u8 enable_dma:1;
	bool (*dma_filter)(struct dma_chan *chan, void *filter_param);
	void *dma_rx_param;
	void *dma_tx_param;
	int autosuspend_delay;
	bool rt;
};

struct pl022_config_chip {
	enum ssp_interface iface;
	enum ssp_hierarchy hierarchy;
	bool slave_tx_disable;
	struct ssp_clock_params clk_freq;
	enum ssp_mode com_mode;
	enum ssp_rx_level_trig rx_lev_trig;
	enum ssp_tx_level_trig tx_lev_trig;
	enum ssp_microwire_ctrl_len ctrl_len;
	enum ssp_microwire_wait_state wait_state;
	enum ssp_duplex duplex;
	enum ssp_clkdelay clkdelay;
	void (*cs_control) (u32 control);
};

#endif 
