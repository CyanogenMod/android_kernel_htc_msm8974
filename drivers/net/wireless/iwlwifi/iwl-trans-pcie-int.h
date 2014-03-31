/******************************************************************************
 *
 * Copyright(c) 2003 - 2012 Intel Corporation. All rights reserved.
 *
 * Portions of this file are derived from the ipw3945 project, as well
 * as portions of the ieee80211 subsystem header files.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 * Contact Information:
 *  Intel Linux Wireless <ilw@linux.intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 *****************************************************************************/
#ifndef __iwl_trans_int_pcie_h__
#define __iwl_trans_int_pcie_h__

#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/skbuff.h>
#include <linux/wait.h>
#include <linux/pci.h>

#include "iwl-fh.h"
#include "iwl-csr.h"
#include "iwl-shared.h"
#include "iwl-trans.h"
#include "iwl-debug.h"
#include "iwl-io.h"
#include "iwl-op-mode.h"

struct iwl_tx_queue;
struct iwl_queue;
struct iwl_host_cmd;


struct iwl_rx_mem_buffer {
	dma_addr_t page_dma;
	struct page *page;
	struct list_head list;
};

struct isr_statistics {
	u32 hw;
	u32 sw;
	u32 err_code;
	u32 sch;
	u32 alive;
	u32 rfkill;
	u32 ctkill;
	u32 wakeup;
	u32 rx;
	u32 tx;
	u32 unhandled;
};

/**
 * struct iwl_rx_queue - Rx queue
 * @bd: driver's pointer to buffer of receive buffer descriptors (rbd)
 * @bd_dma: bus address of buffer of receive buffer descriptors (rbd)
 * @pool:
 * @queue:
 * @read: Shared index to newest available Rx buffer
 * @write: Shared index to oldest written Rx packet
 * @free_count: Number of pre-allocated buffers in rx_free
 * @write_actual:
 * @rx_free: list of free SKBs for use
 * @rx_used: List of Rx buffers with no SKB
 * @need_update: flag to indicate we need to update read/write index
 * @rb_stts: driver's pointer to receive buffer status
 * @rb_stts_dma: bus address of receive buffer status
 * @lock:
 *
 * NOTE:  rx_free and rx_used are used as a FIFO for iwl_rx_mem_buffers
 */
struct iwl_rx_queue {
	__le32 *bd;
	dma_addr_t bd_dma;
	struct iwl_rx_mem_buffer pool[RX_QUEUE_SIZE + RX_FREE_BUFFERS];
	struct iwl_rx_mem_buffer *queue[RX_QUEUE_SIZE];
	u32 read;
	u32 write;
	u32 free_count;
	u32 write_actual;
	struct list_head rx_free;
	struct list_head rx_used;
	int need_update;
	struct iwl_rb_status *rb_stts;
	dma_addr_t rb_stts_dma;
	spinlock_t lock;
};

struct iwl_dma_ptr {
	dma_addr_t dma;
	void *addr;
	size_t size;
};

static inline int iwl_queue_inc_wrap(int index, int n_bd)
{
	return ++index & (n_bd - 1);
}

static inline int iwl_queue_dec_wrap(int index, int n_bd)
{
	return --index & (n_bd - 1);
}

#define IWL_IPAN_MCAST_QUEUE		8

struct iwl_cmd_meta {
	
	struct iwl_host_cmd *source;

	u32 flags;

	DEFINE_DMA_UNMAP_ADDR(mapping);
	DEFINE_DMA_UNMAP_LEN(len);
};

struct iwl_queue {
	int n_bd;              
	int write_ptr;       
	int read_ptr;         
	
	dma_addr_t dma_addr;   
	int n_window;	       
	u32 id;
	int low_mark;	       
	int high_mark;         
};

#define TFD_TX_CMD_SLOTS 256
#define TFD_CMD_SLOTS 32

struct iwl_tx_queue {
	struct iwl_queue q;
	struct iwl_tfd *tfds;
	struct iwl_device_cmd **cmd;
	struct iwl_cmd_meta *meta;
	struct sk_buff **skbs;
	spinlock_t lock;
	unsigned long time_stamp;
	u8 need_update;
	u8 sched_retry;
	u8 active;
	u8 swq_id;

	u16 sta_id;
	u16 tid;
};

struct iwl_trans_pcie {
	struct iwl_rx_queue rxq;
	struct work_struct rx_replenish;
	struct iwl_trans *trans;

	
	__le32 *ict_tbl;
	dma_addr_t ict_tbl_dma;
	int ict_index;
	u32 inta;
	bool use_ict;
	bool irq_requested;
	struct tasklet_struct irq_tasklet;
	struct isr_statistics isr_stats;

	unsigned int irq;
	spinlock_t irq_lock;
	u32 inta_mask;
	u32 scd_base_addr;
	struct iwl_dma_ptr scd_bc_tbls;
	struct iwl_dma_ptr kw;

	const u8 *ac_to_fifo[NUM_IWL_RXON_CTX];
	const u8 *ac_to_queue[NUM_IWL_RXON_CTX];
	u8 mcast_queue[NUM_IWL_RXON_CTX];
	u8 agg_txq[IWLAGN_STATION_COUNT][IWL_MAX_TID_COUNT];

	struct iwl_tx_queue *txq;
	unsigned long txq_ctx_active_msk;
#define IWL_MAX_HW_QUEUES	32
	unsigned long queue_stopped[BITS_TO_LONGS(IWL_MAX_HW_QUEUES)];
	atomic_t queue_stop_count[4];

	
	struct pci_dev *pci_dev;
	void __iomem *hw_base;

	bool ucode_write_complete;
	wait_queue_head_t ucode_write_waitq;
	unsigned long status;
	u8 cmd_queue;
	u8 n_no_reclaim_cmds;
	u8 no_reclaim_cmds[MAX_NO_RECLAIM_CMDS];
};

#define IWL_TRANS_GET_PCIE_TRANS(_iwl_trans) \
	((struct iwl_trans_pcie *) ((_iwl_trans)->trans_specific))

void iwl_bg_rx_replenish(struct work_struct *data);
void iwl_irq_tasklet(struct iwl_trans *trans);
void iwlagn_rx_replenish(struct iwl_trans *trans);
void iwl_rx_queue_update_write_ptr(struct iwl_trans *trans,
			struct iwl_rx_queue *q);

void iwl_reset_ict(struct iwl_trans *trans);
void iwl_disable_ict(struct iwl_trans *trans);
int iwl_alloc_isr_ict(struct iwl_trans *trans);
void iwl_free_isr_ict(struct iwl_trans *trans);
irqreturn_t iwl_isr_ict(int irq, void *data);

void iwl_txq_update_write_ptr(struct iwl_trans *trans,
			struct iwl_tx_queue *txq);
int iwlagn_txq_attach_buf_to_tfd(struct iwl_trans *trans,
				 struct iwl_tx_queue *txq,
				 dma_addr_t addr, u16 len, u8 reset);
int iwl_queue_init(struct iwl_queue *q, int count, int slots_num, u32 id);
int iwl_trans_pcie_send_cmd(struct iwl_trans *trans, struct iwl_host_cmd *cmd);
void iwl_tx_cmd_complete(struct iwl_trans *trans,
			 struct iwl_rx_cmd_buffer *rxb, int handler_status);
void iwl_trans_txq_update_byte_cnt_tbl(struct iwl_trans *trans,
					   struct iwl_tx_queue *txq,
					   u16 byte_cnt);
int iwl_trans_pcie_tx_agg_disable(struct iwl_trans *trans,
				  int sta_id, int tid);
void iwl_trans_set_wr_ptrs(struct iwl_trans *trans, int txq_id, u32 index);
void iwl_trans_tx_queue_set_status(struct iwl_trans *trans,
			     struct iwl_tx_queue *txq,
			     int tx_fifo_id, int scd_retry);
int iwl_trans_pcie_tx_agg_alloc(struct iwl_trans *trans, int sta_id, int tid);
void iwl_trans_pcie_tx_agg_setup(struct iwl_trans *trans,
				 enum iwl_rxon_context_id ctx,
				 int sta_id, int tid, int frame_limit, u16 ssn);
void iwlagn_txq_free_tfd(struct iwl_trans *trans, struct iwl_tx_queue *txq,
	int index, enum dma_data_direction dma_dir);
int iwl_tx_queue_reclaim(struct iwl_trans *trans, int txq_id, int index,
			 struct sk_buff_head *skbs);
int iwl_queue_space(const struct iwl_queue *q);

int iwl_dump_nic_event_log(struct iwl_trans *trans, bool full_log,
			    char **buf, bool display);
int iwl_dump_fh(struct iwl_trans *trans, char **buf, bool display);
void iwl_dump_csr(struct iwl_trans *trans);

static inline void iwl_disable_interrupts(struct iwl_trans *trans)
{
	struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
	clear_bit(STATUS_INT_ENABLED, &trans_pcie->status);

	
	iwl_write32(trans, CSR_INT_MASK, 0x00000000);

	iwl_write32(trans, CSR_INT, 0xffffffff);
	iwl_write32(trans, CSR_FH_INT_STATUS, 0xffffffff);
	IWL_DEBUG_ISR(trans, "Disabled interrupts\n");
}

static inline void iwl_enable_interrupts(struct iwl_trans *trans)
{
	struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);

	IWL_DEBUG_ISR(trans, "Enabling interrupts\n");
	set_bit(STATUS_INT_ENABLED, &trans_pcie->status);
	iwl_write32(trans, CSR_INT_MASK, trans_pcie->inta_mask);
}

static inline void iwl_enable_rfkill_int(struct iwl_trans *trans)
{
	IWL_DEBUG_ISR(trans, "Enabling rfkill interrupt\n");
	iwl_write32(trans, CSR_INT_MASK, CSR_INT_BIT_RF_KILL);
}

static inline void iwl_set_swq_id(struct iwl_tx_queue *txq, u8 ac, u8 hwq)
{
	BUG_ON(ac > 3);   
	BUG_ON(hwq > 31); 

	txq->swq_id = (hwq << 2) | ac;
}

static inline u8 iwl_get_queue_ac(struct iwl_tx_queue *txq)
{
	return txq->swq_id & 0x3;
}

static inline void iwl_wake_queue(struct iwl_trans *trans,
				  struct iwl_tx_queue *txq)
{
	u8 queue = txq->swq_id;
	u8 ac = queue & 3;
	u8 hwq = (queue >> 2) & 0x1f;
	struct iwl_trans_pcie *trans_pcie =
		IWL_TRANS_GET_PCIE_TRANS(trans);

	if (test_and_clear_bit(hwq, trans_pcie->queue_stopped)) {
		if (atomic_dec_return(&trans_pcie->queue_stop_count[ac]) <= 0) {
			iwl_op_mode_queue_not_full(trans->op_mode, ac);
			IWL_DEBUG_TX_QUEUES(trans, "Wake hwq %d ac %d",
					    hwq, ac);
		} else {
			IWL_DEBUG_TX_QUEUES(trans,
				"Don't wake hwq %d ac %d stop count %d",
				hwq, ac,
				atomic_read(&trans_pcie->queue_stop_count[ac]));
		}
	}
}

static inline void iwl_stop_queue(struct iwl_trans *trans,
				  struct iwl_tx_queue *txq)
{
	u8 queue = txq->swq_id;
	u8 ac = queue & 3;
	u8 hwq = (queue >> 2) & 0x1f;
	struct iwl_trans_pcie *trans_pcie =
		IWL_TRANS_GET_PCIE_TRANS(trans);

	if (!test_and_set_bit(hwq, trans_pcie->queue_stopped)) {
		if (atomic_inc_return(&trans_pcie->queue_stop_count[ac]) > 0) {
			iwl_op_mode_queue_full(trans->op_mode, ac);
			IWL_DEBUG_TX_QUEUES(trans,
				"Stop hwq %d ac %d stop count %d",
				hwq, ac,
				atomic_read(&trans_pcie->queue_stop_count[ac]));
		} else {
			IWL_DEBUG_TX_QUEUES(trans,
				"Don't stop hwq %d ac %d stop count %d",
				hwq, ac,
				atomic_read(&trans_pcie->queue_stop_count[ac]));
		}
	} else {
		IWL_DEBUG_TX_QUEUES(trans, "stop hwq %d, but it is stopped",
				    hwq);
	}
}

static inline void iwl_txq_ctx_activate(struct iwl_trans_pcie *trans_pcie,
					int txq_id)
{
	set_bit(txq_id, &trans_pcie->txq_ctx_active_msk);
}

static inline void iwl_txq_ctx_deactivate(struct iwl_trans_pcie *trans_pcie,
					  int txq_id)
{
	clear_bit(txq_id, &trans_pcie->txq_ctx_active_msk);
}

static inline int iwl_queue_used(const struct iwl_queue *q, int i)
{
	return q->write_ptr >= q->read_ptr ?
		(i >= q->read_ptr && i < q->write_ptr) :
		!(i < q->read_ptr && i >= q->write_ptr);
}

static inline u8 get_cmd_index(struct iwl_queue *q, u32 index)
{
	return index & (q->n_window - 1);
}

#define IWL_TX_FIFO_BK		0	
#define IWL_TX_FIFO_BE		1
#define IWL_TX_FIFO_VI		2	
#define IWL_TX_FIFO_VO		3
#define IWL_TX_FIFO_BK_IPAN	IWL_TX_FIFO_BK
#define IWL_TX_FIFO_BE_IPAN	4
#define IWL_TX_FIFO_VI_IPAN	IWL_TX_FIFO_VI
#define IWL_TX_FIFO_VO_IPAN	5
#define IWL_TX_FIFO_AUX		5
#define IWL_TX_FIFO_UNUSED	-1

#define IWL_AUX_QUEUE		10

#endif 
