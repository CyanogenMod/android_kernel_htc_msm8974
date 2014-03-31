/*
 * Copyright 2004-2009 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARCH_MXC_DMA_H__
#define __ASM_ARCH_MXC_DMA_H__

#include <linux/scatterlist.h>
#include <linux/device.h>
#include <linux/dmaengine.h>

enum sdma_peripheral_type {
	IMX_DMATYPE_SSI,	
	IMX_DMATYPE_SSI_SP,	
	IMX_DMATYPE_MMC,	
	IMX_DMATYPE_SDHC,	
	IMX_DMATYPE_UART,	
	IMX_DMATYPE_UART_SP,	
	IMX_DMATYPE_FIRI,	
	IMX_DMATYPE_CSPI,	
	IMX_DMATYPE_CSPI_SP,	
	IMX_DMATYPE_SIM,	
	IMX_DMATYPE_ATA,	
	IMX_DMATYPE_CCM,	
	IMX_DMATYPE_EXT,	
	IMX_DMATYPE_MSHC,	
	IMX_DMATYPE_MSHC_SP,	
	IMX_DMATYPE_DSP,	
	IMX_DMATYPE_MEMORY,	
	IMX_DMATYPE_FIFO_MEMORY,
	IMX_DMATYPE_SPDIF,	
	IMX_DMATYPE_IPU_MEMORY,	
	IMX_DMATYPE_ASRC,	
	IMX_DMATYPE_ESAI,	
};

enum imx_dma_prio {
	DMA_PRIO_HIGH = 0,
	DMA_PRIO_MEDIUM = 1,
	DMA_PRIO_LOW = 2
};

struct imx_dma_data {
	int dma_request; 
	enum sdma_peripheral_type peripheral_type;
	int priority;
};

static inline int imx_dma_is_ipu(struct dma_chan *chan)
{
	return !strcmp(dev_name(chan->device->dev), "ipu-core");
}

static inline int imx_dma_is_general_purpose(struct dma_chan *chan)
{
	return strstr(dev_name(chan->device->dev), "sdma") ||
		!strcmp(dev_name(chan->device->dev), "imx-dma");
}

#endif
