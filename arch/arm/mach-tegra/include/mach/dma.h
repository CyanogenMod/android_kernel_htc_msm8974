/*
 * arch/arm/mach-tegra/include/mach/dma.h
 *
 * Copyright (c) 2008-2009, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __MACH_TEGRA_DMA_H
#define __MACH_TEGRA_DMA_H

#include <linux/list.h>

#define TEGRA_DMA_REQ_SEL_CNTR			0
#define TEGRA_DMA_REQ_SEL_I2S_2			1
#define TEGRA_DMA_REQ_SEL_I2S_1			2
#define TEGRA_DMA_REQ_SEL_SPD_I			3
#define TEGRA_DMA_REQ_SEL_UI_I			4
#define TEGRA_DMA_REQ_SEL_MIPI			5
#define TEGRA_DMA_REQ_SEL_I2S2_2		6
#define TEGRA_DMA_REQ_SEL_I2S2_1		7
#define TEGRA_DMA_REQ_SEL_UARTA			8
#define TEGRA_DMA_REQ_SEL_UARTB			9
#define TEGRA_DMA_REQ_SEL_UARTC			10
#define TEGRA_DMA_REQ_SEL_SPI			11
#define TEGRA_DMA_REQ_SEL_AC97			12
#define TEGRA_DMA_REQ_SEL_ACMODEM		13
#define TEGRA_DMA_REQ_SEL_SL4B			14
#define TEGRA_DMA_REQ_SEL_SL2B1			15
#define TEGRA_DMA_REQ_SEL_SL2B2			16
#define TEGRA_DMA_REQ_SEL_SL2B3			17
#define TEGRA_DMA_REQ_SEL_SL2B4			18
#define TEGRA_DMA_REQ_SEL_UARTD			19
#define TEGRA_DMA_REQ_SEL_UARTE			20
#define TEGRA_DMA_REQ_SEL_I2C			21
#define TEGRA_DMA_REQ_SEL_I2C2			22
#define TEGRA_DMA_REQ_SEL_I2C3			23
#define TEGRA_DMA_REQ_SEL_DVC_I2C		24
#define TEGRA_DMA_REQ_SEL_OWR			25
#define TEGRA_DMA_REQ_SEL_INVALID		31

#if defined(CONFIG_TEGRA_SYSTEM_DMA)

struct tegra_dma_req;
struct tegra_dma_channel;

enum tegra_dma_mode {
	TEGRA_DMA_SHARED = 1,
	TEGRA_DMA_MODE_CONTINOUS = 2,
	TEGRA_DMA_MODE_ONESHOT = 4,
};

enum tegra_dma_req_error {
	TEGRA_DMA_REQ_SUCCESS = 0,
	TEGRA_DMA_REQ_ERROR_ABORTED,
	TEGRA_DMA_REQ_INFLIGHT,
};

enum tegra_dma_req_buff_status {
	TEGRA_DMA_REQ_BUF_STATUS_EMPTY = 0,
	TEGRA_DMA_REQ_BUF_STATUS_HALF_FULL,
	TEGRA_DMA_REQ_BUF_STATUS_FULL,
};

struct tegra_dma_req {
	struct list_head node;
	unsigned int modid;
	int instance;

	void (*complete)(struct tegra_dma_req *req);

	void (*threshold)(struct tegra_dma_req *req);

	int to_memory;

	void *virt_addr;

	unsigned long source_addr;
	unsigned long dest_addr;
	unsigned long dest_wrap;
	unsigned long source_wrap;
	unsigned long source_bus_width;
	unsigned long dest_bus_width;
	unsigned long req_sel;
	unsigned int size;

	
	int bytes_transferred;
	int status;

	
	int buffer_status;

	
	void *dev;
};

int tegra_dma_enqueue_req(struct tegra_dma_channel *ch,
	struct tegra_dma_req *req);
int tegra_dma_dequeue_req(struct tegra_dma_channel *ch,
	struct tegra_dma_req *req);
void tegra_dma_dequeue(struct tegra_dma_channel *ch);
void tegra_dma_flush(struct tegra_dma_channel *ch);

bool tegra_dma_is_req_inflight(struct tegra_dma_channel *ch,
	struct tegra_dma_req *req);
bool tegra_dma_is_empty(struct tegra_dma_channel *ch);

struct tegra_dma_channel *tegra_dma_allocate_channel(int mode);
void tegra_dma_free_channel(struct tegra_dma_channel *ch);

int __init tegra_dma_init(void);

#endif

#endif
