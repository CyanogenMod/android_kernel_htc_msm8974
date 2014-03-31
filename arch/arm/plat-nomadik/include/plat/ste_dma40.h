/*
 * Copyright (C) ST-Ericsson SA 2007-2010
 * Author: Per Forlin <per.forlin@stericsson.com> for ST-Ericsson
 * Author: Jonas Aaberg <jonas.aberg@stericsson.com> for ST-Ericsson
 * License terms: GNU General Public License (GPL) version 2
 */


#ifndef STE_DMA40_H
#define STE_DMA40_H

#include <linux/dmaengine.h>
#include <linux/scatterlist.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>

#define STEDMA40_MAX_SEG_SIZE 0xFFFF

#define STEDMA40_DEV_DST_MEMORY (-1)
#define	STEDMA40_DEV_SRC_MEMORY (-1)

enum stedma40_mode {
	STEDMA40_MODE_LOGICAL = 0,
	STEDMA40_MODE_PHYSICAL,
	STEDMA40_MODE_OPERATION,
};

enum stedma40_mode_opt {
	STEDMA40_PCHAN_BASIC_MODE = 0,
	STEDMA40_LCHAN_SRC_LOG_DST_LOG = 0,
	STEDMA40_PCHAN_MODULO_MODE,
	STEDMA40_PCHAN_DOUBLE_DST_MODE,
	STEDMA40_LCHAN_SRC_PHY_DST_LOG,
	STEDMA40_LCHAN_SRC_LOG_DST_PHY,
};

#define STEDMA40_ESIZE_8_BIT  0x0
#define STEDMA40_ESIZE_16_BIT 0x1
#define STEDMA40_ESIZE_32_BIT 0x2
#define STEDMA40_ESIZE_64_BIT 0x3

#define STEDMA40_PSIZE_PHY_1  0x4
#define STEDMA40_PSIZE_PHY_2  0x0
#define STEDMA40_PSIZE_PHY_4  0x1
#define STEDMA40_PSIZE_PHY_8  0x2
#define STEDMA40_PSIZE_PHY_16 0x3

#define STEDMA40_PSIZE_LOG_1  STEDMA40_PSIZE_PHY_2
#define STEDMA40_PSIZE_LOG_4  STEDMA40_PSIZE_PHY_4
#define STEDMA40_PSIZE_LOG_8  STEDMA40_PSIZE_PHY_8
#define STEDMA40_PSIZE_LOG_16 STEDMA40_PSIZE_PHY_16

#define STEDMA40_MAX_PHYS 32

enum stedma40_flow_ctrl {
	STEDMA40_NO_FLOW_CTRL,
	STEDMA40_FLOW_CTRL,
};

enum stedma40_periph_data_width {
	STEDMA40_BYTE_WIDTH = STEDMA40_ESIZE_8_BIT,
	STEDMA40_HALFWORD_WIDTH = STEDMA40_ESIZE_16_BIT,
	STEDMA40_WORD_WIDTH = STEDMA40_ESIZE_32_BIT,
	STEDMA40_DOUBLEWORD_WIDTH = STEDMA40_ESIZE_64_BIT
};

enum stedma40_xfer_dir {
	STEDMA40_MEM_TO_MEM = 1,
	STEDMA40_MEM_TO_PERIPH,
	STEDMA40_PERIPH_TO_MEM,
	STEDMA40_PERIPH_TO_PERIPH
};


struct stedma40_half_channel_info {
	bool big_endian;
	enum stedma40_periph_data_width data_width;
	int psize;
	enum stedma40_flow_ctrl flow_ctrl;
};

struct stedma40_chan_cfg {
	enum stedma40_xfer_dir			 dir;
	bool					 high_priority;
	bool					 realtime;
	enum stedma40_mode			 mode;
	enum stedma40_mode_opt			 mode_opt;
	int					 src_dev_type;
	int					 dst_dev_type;
	struct stedma40_half_channel_info	 src_info;
	struct stedma40_half_channel_info	 dst_info;

	bool					 use_fixed_channel;
	int					 phy_channel;
};

struct stedma40_platform_data {
	u32				 dev_len;
	const dma_addr_t		*dev_tx;
	const dma_addr_t		*dev_rx;
	int				*memcpy;
	u32				 memcpy_len;
	struct stedma40_chan_cfg	*memcpy_conf_phy;
	struct stedma40_chan_cfg	*memcpy_conf_log;
	int				 disabled_channels[STEDMA40_MAX_PHYS];
	bool				 use_esram_lcla;
};

#ifdef CONFIG_STE_DMA40


bool stedma40_filter(struct dma_chan *chan, void *data);


static inline struct
dma_async_tx_descriptor *stedma40_slave_mem(struct dma_chan *chan,
					    dma_addr_t addr,
					    unsigned int size,
					    enum dma_transfer_direction direction,
					    unsigned long flags)
{
	struct scatterlist sg;
	sg_init_table(&sg, 1);
	sg.dma_address = addr;
	sg.length = size;

	return dmaengine_prep_slave_sg(chan, &sg, 1, direction, flags);
}

#else
static inline bool stedma40_filter(struct dma_chan *chan, void *data)
{
	return false;
}

static inline struct
dma_async_tx_descriptor *stedma40_slave_mem(struct dma_chan *chan,
					    dma_addr_t addr,
					    unsigned int size,
					    enum dma_transfer_direction direction,
					    unsigned long flags)
{
	return NULL;
}
#endif

#endif
