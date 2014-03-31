/*
 * Copyright © 2006, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */
#ifndef IOP_ADMA_H
#define IOP_ADMA_H
#include <linux/types.h>
#include <linux/dmaengine.h>
#include <linux/interrupt.h>

#define IOP_ADMA_SLOT_SIZE 32
#define IOP_ADMA_THRESHOLD 4
#ifdef DEBUG
#define IOP_PARANOIA 1
#else
#define IOP_PARANOIA 0
#endif
#define iop_paranoia(x) BUG_ON(IOP_PARANOIA && (x))

struct iop_adma_device {
	struct platform_device *pdev;
	int id;
	dma_addr_t dma_desc_pool;
	void *dma_desc_pool_virt;
	struct dma_device common;
};

struct iop_adma_chan {
	int pending;
	spinlock_t lock; 
	void __iomem *mmr_base;
	struct list_head chain;
	struct iop_adma_device *device;
	struct dma_chan common;
	struct iop_adma_desc_slot *last_used;
	struct list_head all_slots;
	int slots_allocated;
	struct tasklet_struct irq_tasklet;
};

struct iop_adma_desc_slot {
	struct list_head slot_node;
	struct list_head chain_node;
	void *hw_desc;
	struct iop_adma_desc_slot *group_head;
	u16 slot_cnt;
	u16 slots_per_op;
	u16 idx;
	u16 unmap_src_cnt;
	size_t unmap_len;
	struct list_head tx_list;
	struct dma_async_tx_descriptor async_tx;
	union {
		u32 *xor_check_result;
		u32 *crc32_result;
		u32 *pq_check_result;
	};
};

struct iop_adma_platform_data {
	int hw_id;
	dma_cap_mask_t cap_mask;
	size_t pool_size;
};

#define to_iop_sw_desc(addr_hw_desc) \
	container_of(addr_hw_desc, struct iop_adma_desc_slot, hw_desc)
#define iop_hw_desc_slot_idx(hw_desc, idx) \
	( (void *) (((unsigned long) hw_desc) + ((idx) << 5)) )
#endif
