/*
 * 2006-2009 (C) DENX Software Engineering.
 *
 * Author: Yuri Tikhonov <yur@emcraft.com>
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2.  This program is licensed "as is" without any warranty of
 * any kind, whether express or implied.
 */

#ifndef _PPC440SPE_ADMA_H
#define _PPC440SPE_ADMA_H

#include <linux/types.h>
#include "dma.h"
#include "xor.h"

#define to_ppc440spe_adma_chan(chan) \
		container_of(chan, struct ppc440spe_adma_chan, common)
#define to_ppc440spe_adma_device(dev) \
		container_of(dev, struct ppc440spe_adma_device, common)
#define tx_to_ppc440spe_adma_slot(tx) \
		container_of(tx, struct ppc440spe_adma_desc_slot, async_tx)

#define PPC440SPE_DEFAULT_POLY	0x4d

#define PPC440SPE_ADMA_ENGINES_NUM	(XOR_ENGINES_NUM + DMA_ENGINES_NUM)

#define PPC440SPE_ADMA_WATCHDOG_MSEC	3
#define PPC440SPE_ADMA_THRESHOLD	1

#define PPC440SPE_DMA0_ID	0
#define PPC440SPE_DMA1_ID	1
#define PPC440SPE_XOR_ID	2

#define PPC440SPE_ADMA_DMA_MAX_BYTE_COUNT	0xFFFFFFUL
#define PPC440SPE_ADMA_XOR_MAX_BYTE_COUNT	(1 << 31)
#define PPC440SPE_ADMA_ZERO_SUM_MAX_BYTE_COUNT PPC440SPE_ADMA_XOR_MAX_BYTE_COUNT

#define PPC440SPE_RXOR_RUN	0

#define MQ0_CF2H_RXOR_BS_MASK	0x1FF

#undef ADMA_LL_DEBUG

struct ppc440spe_adma_device {
	struct device *dev;
	struct dma_regs __iomem *dma_reg;
	struct xor_regs __iomem *xor_reg;
	struct i2o_regs __iomem *i2o_reg;
	int id;
	void *dma_desc_pool_virt;
	dma_addr_t dma_desc_pool;
	size_t pool_size;
	int irq;
	int err_irq;
	struct dma_device common;
};

struct ppc440spe_adma_chan {
	spinlock_t lock;
	struct ppc440spe_adma_device *device;
	struct list_head chain;
	struct dma_chan common;
	struct list_head all_slots;
	struct ppc440spe_adma_desc_slot *last_used;
	int pending;
	int slots_allocated;
	int hw_chain_inited;
	struct tasklet_struct irq_tasklet;
	u8 needs_unmap;
	struct page *pdest_page;
	struct page *qdest_page;
	dma_addr_t pdest;
	dma_addr_t qdest;
};

struct ppc440spe_rxor {
	u32 addrl;
	u32 addrh;
	int len;
	int xor_count;
	int addr_count;
	int desc_count;
	int state;
};

struct ppc440spe_adma_desc_slot {
	dma_addr_t phys;
	struct ppc440spe_adma_desc_slot *group_head;
	struct ppc440spe_adma_desc_slot *hw_next;
	struct dma_async_tx_descriptor async_tx;
	struct list_head slot_node;
	struct list_head chain_node; 
	struct list_head group_list; 
	unsigned int unmap_len;
	void *hw_desc;
	u16 stride;
	u16 idx;
	u16 slot_cnt;
	u8 src_cnt;
	u8 dst_cnt;
	u8 slots_per_op;
	u8 descs_per_op;
	unsigned long flags;
	unsigned long reverse_flags[8];

#define PPC440SPE_DESC_INT	0	
#define PPC440SPE_ZERO_P	1	
#define PPC440SPE_ZERO_Q	2	
#define PPC440SPE_COHERENT	3	

#define PPC440SPE_DESC_WXOR	4	
#define PPC440SPE_DESC_RXOR	5	

#define PPC440SPE_DESC_RXOR123	8	
#define PPC440SPE_DESC_RXOR124	9	
#define PPC440SPE_DESC_RXOR125	10	
#define PPC440SPE_DESC_RXOR12	11	
#define PPC440SPE_DESC_RXOR_REV	12	

#define PPC440SPE_DESC_PCHECK	13
#define PPC440SPE_DESC_QCHECK	14

#define PPC440SPE_DESC_RXOR_MSK	0x3

	struct ppc440spe_rxor rxor_cursor;

	union {
		u32 *xor_check_result;
		u32 *crc32_result;
	};
};

#endif 
