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
#ifndef _ASYNC_TX_H_
#define _ASYNC_TX_H_
#include <linux/dmaengine.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>

#ifdef CONFIG_HAS_DMA
#define __async_inline
#else
#define __async_inline __always_inline
#endif

struct dma_chan_ref {
	struct dma_chan *chan;
	struct list_head node;
	struct rcu_head rcu;
	atomic_t count;
};

enum async_tx_flags {
	ASYNC_TX_XOR_ZERO_DST	 = (1 << 0),
	ASYNC_TX_XOR_DROP_DST	 = (1 << 1),
	ASYNC_TX_ACK		 = (1 << 2),
	ASYNC_TX_FENCE		 = (1 << 3),
};

struct async_submit_ctl {
	enum async_tx_flags flags;
	struct dma_async_tx_descriptor *depend_tx;
	dma_async_tx_callback cb_fn;
	void *cb_param;
	void *scribble;
};

#ifdef CONFIG_DMA_ENGINE
#define async_tx_issue_pending_all dma_issue_pending_all

static inline void async_tx_issue_pending(struct dma_async_tx_descriptor *tx)
{
	if (likely(tx)) {
		struct dma_chan *chan = tx->chan;
		struct dma_device *dma = chan->device;

		dma->device_issue_pending(chan);
	}
}
#ifdef CONFIG_ARCH_HAS_ASYNC_TX_FIND_CHANNEL
#include <asm/async_tx.h>
#else
#define async_tx_find_channel(dep, type, dst, dst_count, src, src_count, len) \
	 __async_tx_find_channel(dep, type)
struct dma_chan *
__async_tx_find_channel(struct async_submit_ctl *submit,
			enum dma_transaction_type tx_type);
#endif 
#else
static inline void async_tx_issue_pending_all(void)
{
	do { } while (0);
}

static inline void async_tx_issue_pending(struct dma_async_tx_descriptor *tx)
{
	do { } while (0);
}

static inline struct dma_chan *
async_tx_find_channel(struct async_submit_ctl *submit,
		      enum dma_transaction_type tx_type, struct page **dst,
		      int dst_count, struct page **src, int src_count,
		      size_t len)
{
	return NULL;
}
#endif

static inline void
async_tx_sync_epilog(struct async_submit_ctl *submit)
{
	if (submit->cb_fn)
		submit->cb_fn(submit->cb_param);
}

typedef union {
	unsigned long addr;
	struct page *page;
	dma_addr_t dma;
} addr_conv_t;

static inline void
init_async_submit(struct async_submit_ctl *args, enum async_tx_flags flags,
		  struct dma_async_tx_descriptor *tx,
		  dma_async_tx_callback cb_fn, void *cb_param,
		  addr_conv_t *scribble)
{
	args->flags = flags;
	args->depend_tx = tx;
	args->cb_fn = cb_fn;
	args->cb_param = cb_param;
	args->scribble = scribble;
}

void async_tx_submit(struct dma_chan *chan, struct dma_async_tx_descriptor *tx,
		     struct async_submit_ctl *submit);

struct dma_async_tx_descriptor *
async_xor(struct page *dest, struct page **src_list, unsigned int offset,
	  int src_cnt, size_t len, struct async_submit_ctl *submit);

struct dma_async_tx_descriptor *
async_xor_val(struct page *dest, struct page **src_list, unsigned int offset,
	      int src_cnt, size_t len, enum sum_check_flags *result,
	      struct async_submit_ctl *submit);

struct dma_async_tx_descriptor *
async_memcpy(struct page *dest, struct page *src, unsigned int dest_offset,
	     unsigned int src_offset, size_t len,
	     struct async_submit_ctl *submit);

struct dma_async_tx_descriptor *
async_memset(struct page *dest, int val, unsigned int offset,
	     size_t len, struct async_submit_ctl *submit);

struct dma_async_tx_descriptor *async_trigger_callback(struct async_submit_ctl *submit);

struct dma_async_tx_descriptor *
async_gen_syndrome(struct page **blocks, unsigned int offset, int src_cnt,
		   size_t len, struct async_submit_ctl *submit);

struct dma_async_tx_descriptor *
async_syndrome_val(struct page **blocks, unsigned int offset, int src_cnt,
		   size_t len, enum sum_check_flags *pqres, struct page *spare,
		   struct async_submit_ctl *submit);

struct dma_async_tx_descriptor *
async_raid6_2data_recov(int src_num, size_t bytes, int faila, int failb,
			struct page **ptrs, struct async_submit_ctl *submit);

struct dma_async_tx_descriptor *
async_raid6_datap_recov(int src_num, size_t bytes, int faila,
			struct page **ptrs, struct async_submit_ctl *submit);

void async_tx_quiesce(struct dma_async_tx_descriptor **tx);
#endif 
