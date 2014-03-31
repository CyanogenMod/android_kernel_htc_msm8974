/*
 * Freescale GPMI NAND Flash Driver
 *
 * Copyright (C) 2010-2011 Freescale Semiconductor, Inc.
 * Copyright (C) 2008 Embedded Alley Solutions, Inc.
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
#ifndef __DRIVERS_MTD_NAND_GPMI_NAND_H
#define __DRIVERS_MTD_NAND_GPMI_NAND_H

#include <linux/mtd/nand.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/fsl/mxs-dma.h>

struct resources {
	void          *gpmi_regs;
	void          *bch_regs;
	unsigned int  bch_low_interrupt;
	unsigned int  bch_high_interrupt;
	unsigned int  dma_low_channel;
	unsigned int  dma_high_channel;
	struct clk    *clock;
};

struct bch_geometry {
	unsigned int  gf_len;
	unsigned int  ecc_strength;
	unsigned int  page_size;
	unsigned int  metadata_size;
	unsigned int  ecc_chunk_size;
	unsigned int  ecc_chunk_count;
	unsigned int  payload_size;
	unsigned int  auxiliary_size;
	unsigned int  auxiliary_status_offset;
	unsigned int  block_mark_byte_offset;
	unsigned int  block_mark_bit_offset;
};

struct boot_rom_geometry {
	unsigned int  stride_size_in_pages;
	unsigned int  search_area_stride_exponent;
};

enum dma_ops_type {
	DMA_FOR_COMMAND = 1,
	DMA_FOR_READ_DATA,
	DMA_FOR_WRITE_DATA,
	DMA_FOR_READ_ECC_PAGE,
	DMA_FOR_WRITE_ECC_PAGE
};

struct nand_timing {
	int8_t  data_setup_in_ns;
	int8_t  data_hold_in_ns;
	int8_t  address_setup_in_ns;
	int8_t  gpmi_sample_delay_in_ns;
	int8_t  tREA_in_ns;
	int8_t  tRLOH_in_ns;
	int8_t  tRHOH_in_ns;
};

struct gpmi_nand_data {
	
	struct device		*dev;
	struct platform_device	*pdev;
	struct gpmi_nand_platform_data	*pdata;

	
	struct resources	resources;

	
	struct nand_timing	timing;

	
	struct bch_geometry	bch_geometry;
	struct completion	bch_done;

	
	bool			swap_block_mark;
	struct boot_rom_geometry rom_geometry;

	
	struct nand_chip	nand;
	struct mtd_info		mtd;

	
	int			current_chip;
	unsigned int		command_length;

	
	uint8_t			*upper_buf;
	int			upper_len;

	
	bool			direct_dma_map_ok;

	struct scatterlist	cmd_sgl;
	char			*cmd_buffer;

	struct scatterlist	data_sgl;
	char			*data_buffer_dma;

	void			*page_buffer_virt;
	dma_addr_t		page_buffer_phys;
	unsigned int		page_buffer_size;

	void			*payload_virt;
	dma_addr_t		payload_phys;

	void			*auxiliary_virt;
	dma_addr_t		auxiliary_phys;

	
#define DMA_CHANS		8
	struct dma_chan		*dma_chans[DMA_CHANS];
	struct mxs_dma_data	dma_data;
	enum dma_ops_type	last_dma_type;
	enum dma_ops_type	dma_type;
	struct completion	dma_done;

	
	void			*private;
};

struct gpmi_nfc_hardware_timing {
	uint8_t  data_setup_in_cycles;
	uint8_t  data_hold_in_cycles;
	uint8_t  address_setup_in_cycles;
	bool     use_half_periods;
	uint8_t  sample_delay_factor;
};

struct timing_threshod {
	const unsigned int      max_chip_count;
	const unsigned int      max_data_setup_cycles;
	const unsigned int      internal_data_setup_in_ns;
	const unsigned int      max_sample_delay_factor;
	const unsigned int      max_dll_clock_period_in_ns;
	const unsigned int      max_dll_delay_in_ns;
	unsigned long           clock_frequency_in_hz;

};

extern int common_nfc_set_geometry(struct gpmi_nand_data *);
extern struct dma_chan *get_dma_chan(struct gpmi_nand_data *);
extern void prepare_data_dma(struct gpmi_nand_data *,
				enum dma_data_direction dr);
extern int start_dma_without_bch_irq(struct gpmi_nand_data *,
				struct dma_async_tx_descriptor *);
extern int start_dma_with_bch_irq(struct gpmi_nand_data *,
				struct dma_async_tx_descriptor *);

extern int gpmi_init(struct gpmi_nand_data *);
extern void gpmi_clear_bch(struct gpmi_nand_data *);
extern void gpmi_dump_info(struct gpmi_nand_data *);
extern int bch_set_geometry(struct gpmi_nand_data *);
extern int gpmi_is_ready(struct gpmi_nand_data *, unsigned chip);
extern int gpmi_send_command(struct gpmi_nand_data *);
extern void gpmi_begin(struct gpmi_nand_data *);
extern void gpmi_end(struct gpmi_nand_data *);
extern int gpmi_read_data(struct gpmi_nand_data *);
extern int gpmi_send_data(struct gpmi_nand_data *);
extern int gpmi_send_page(struct gpmi_nand_data *,
			dma_addr_t payload, dma_addr_t auxiliary);
extern int gpmi_read_page(struct gpmi_nand_data *,
			dma_addr_t payload, dma_addr_t auxiliary);

#define STATUS_GOOD		0x00
#define STATUS_ERASED		0xff
#define STATUS_UNCORRECTABLE	0xfe

#define IS_MX23			0x1
#define IS_MX28			0x2
#define GPMI_IS_MX23(x)		((x)->pdev->id_entry->driver_data == IS_MX23)
#define GPMI_IS_MX28(x)		((x)->pdev->id_entry->driver_data == IS_MX28)
#endif
