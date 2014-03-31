/*
 * linux/amba/pl08x.h - ARM PrimeCell DMA Controller driver
 *
 * Copyright (C) 2005 ARM Ltd
 * Copyright (C) 2010 ST-Ericsson SA
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * pl08x information required by platform code
 *
 * Please credit ARM.com
 * Documentation: ARM DDI 0196D
 */

#ifndef AMBA_PL08X_H
#define AMBA_PL08X_H

#include <linux/dmaengine.h>
#include <linux/interrupt.h>

struct pl08x_lli;
struct pl08x_driver_data;

enum {
	PL08X_AHB1 = (1 << 0),
	PL08X_AHB2 = (1 << 1)
};

struct pl08x_channel_data {
	char *bus_id;
	int min_signal;
	int max_signal;
	u32 muxval;
	u32 cctl;
	dma_addr_t addr;
	bool circular_buffer;
	bool single;
	u8 periph_buses;
};

struct pl08x_bus_data {
	dma_addr_t addr;
	u8 maxwidth;
	u8 buswidth;
};

struct pl08x_phy_chan {
	unsigned int id;
	void __iomem *base;
	spinlock_t lock;
	int signal;
	struct pl08x_dma_chan *serving;
};

struct pl08x_sg {
	dma_addr_t src_addr;
	dma_addr_t dst_addr;
	size_t len;
	struct list_head node;
};

struct pl08x_txd {
	struct dma_async_tx_descriptor tx;
	struct list_head node;
	struct list_head dsg_list;
	enum dma_transfer_direction direction;
	dma_addr_t llis_bus;
	struct pl08x_lli *llis_va;
	
	u32 cctl;
	u32 ccfg;
};

enum pl08x_dma_chan_state {
	PL08X_CHAN_IDLE,
	PL08X_CHAN_RUNNING,
	PL08X_CHAN_PAUSED,
	PL08X_CHAN_WAITING,
};

struct pl08x_dma_chan {
	struct dma_chan chan;
	struct pl08x_phy_chan *phychan;
	int phychan_hold;
	struct tasklet_struct tasklet;
	char *name;
	const struct pl08x_channel_data *cd;
	dma_addr_t src_addr;
	dma_addr_t dst_addr;
	u32 src_cctl;
	u32 dst_cctl;
	enum dma_transfer_direction runtime_direction;
	struct list_head pend_list;
	struct pl08x_txd *at;
	spinlock_t lock;
	struct pl08x_driver_data *host;
	enum pl08x_dma_chan_state state;
	bool slave;
	bool device_fc;
	struct pl08x_txd *waiting;
};

struct pl08x_platform_data {
	const struct pl08x_channel_data *slave_channels;
	unsigned int num_slave_channels;
	struct pl08x_channel_data memcpy_channel;
	int (*get_signal)(struct pl08x_dma_chan *);
	void (*put_signal)(struct pl08x_dma_chan *);
	u8 lli_buses;
	u8 mem_buses;
};

#ifdef CONFIG_AMBA_PL08X
bool pl08x_filter_id(struct dma_chan *chan, void *chan_id);
#else
static inline bool pl08x_filter_id(struct dma_chan *chan, void *chan_id)
{
	return false;
}
#endif

#endif	
