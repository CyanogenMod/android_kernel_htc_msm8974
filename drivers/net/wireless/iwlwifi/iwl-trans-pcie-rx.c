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
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/gfp.h>

#include "iwl-prph.h"
#include "iwl-io.h"
#include "iwl-trans-pcie-int.h"
#include "iwl-op-mode.h"

#ifdef CONFIG_IWLWIFI_IDI
#include "iwl-amfh.h"
#endif



static int iwl_rx_queue_space(const struct iwl_rx_queue *q)
{
	int s = q->read - q->write;
	if (s <= 0)
		s += RX_QUEUE_SIZE;
	
	s -= 2;
	if (s < 0)
		s = 0;
	return s;
}

void iwl_rx_queue_update_write_ptr(struct iwl_trans *trans,
			struct iwl_rx_queue *q)
{
	unsigned long flags;
	u32 reg;

	spin_lock_irqsave(&q->lock, flags);

	if (q->need_update == 0)
		goto exit_unlock;

	if (cfg(trans)->base_params->shadow_reg_enable) {
		
		
		q->write_actual = (q->write & ~0x7);
		iwl_write32(trans, FH_RSCSR_CHNL0_WPTR, q->write_actual);
	} else {
		
		if (test_bit(STATUS_POWER_PMI, &trans->shrd->status)) {
			reg = iwl_read32(trans, CSR_UCODE_DRV_GP1);

			if (reg & CSR_UCODE_DRV_GP1_BIT_MAC_SLEEP) {
				IWL_DEBUG_INFO(trans,
					"Rx queue requesting wakeup,"
					" GP1 = 0x%x\n", reg);
				iwl_set_bit(trans, CSR_GP_CNTRL,
					CSR_GP_CNTRL_REG_FLAG_MAC_ACCESS_REQ);
				goto exit_unlock;
			}

			q->write_actual = (q->write & ~0x7);
			iwl_write_direct32(trans, FH_RSCSR_CHNL0_WPTR,
					q->write_actual);

		
		} else {
			
			q->write_actual = (q->write & ~0x7);
			iwl_write_direct32(trans, FH_RSCSR_CHNL0_WPTR,
				q->write_actual);
		}
	}
	q->need_update = 0;

 exit_unlock:
	spin_unlock_irqrestore(&q->lock, flags);
}

static inline __le32 iwlagn_dma_addr2rbd_ptr(dma_addr_t dma_addr)
{
	return cpu_to_le32((u32)(dma_addr >> 8));
}

static void iwlagn_rx_queue_restock(struct iwl_trans *trans)
{
	struct iwl_trans_pcie *trans_pcie =
		IWL_TRANS_GET_PCIE_TRANS(trans);

	struct iwl_rx_queue *rxq = &trans_pcie->rxq;
	struct list_head *element;
	struct iwl_rx_mem_buffer *rxb;
	unsigned long flags;

	spin_lock_irqsave(&rxq->lock, flags);
	while ((iwl_rx_queue_space(rxq) > 0) && (rxq->free_count)) {
		/* The overwritten rxb must be a used one */
		rxb = rxq->queue[rxq->write];
		BUG_ON(rxb && rxb->page);

		
		element = rxq->rx_free.next;
		rxb = list_entry(element, struct iwl_rx_mem_buffer, list);
		list_del(element);

		
		rxq->bd[rxq->write] = iwlagn_dma_addr2rbd_ptr(rxb->page_dma);
		rxq->queue[rxq->write] = rxb;
		rxq->write = (rxq->write + 1) & RX_QUEUE_MASK;
		rxq->free_count--;
	}
	spin_unlock_irqrestore(&rxq->lock, flags);
	if (rxq->free_count <= RX_LOW_WATERMARK)
		schedule_work(&trans_pcie->rx_replenish);


	if (rxq->write_actual != (rxq->write & ~0x7)) {
		spin_lock_irqsave(&rxq->lock, flags);
		rxq->need_update = 1;
		spin_unlock_irqrestore(&rxq->lock, flags);
		iwl_rx_queue_update_write_ptr(trans, rxq);
	}
}

static void iwlagn_rx_allocate(struct iwl_trans *trans, gfp_t priority)
{
	struct iwl_trans_pcie *trans_pcie =
		IWL_TRANS_GET_PCIE_TRANS(trans);

	struct iwl_rx_queue *rxq = &trans_pcie->rxq;
	struct list_head *element;
	struct iwl_rx_mem_buffer *rxb;
	struct page *page;
	unsigned long flags;
	gfp_t gfp_mask = priority;

	while (1) {
		spin_lock_irqsave(&rxq->lock, flags);
		if (list_empty(&rxq->rx_used)) {
			spin_unlock_irqrestore(&rxq->lock, flags);
			return;
		}
		spin_unlock_irqrestore(&rxq->lock, flags);

		if (rxq->free_count > RX_LOW_WATERMARK)
			gfp_mask |= __GFP_NOWARN;

		if (hw_params(trans).rx_page_order > 0)
			gfp_mask |= __GFP_COMP;

		
		page = alloc_pages(gfp_mask,
				  hw_params(trans).rx_page_order);
		if (!page) {
			if (net_ratelimit())
				IWL_DEBUG_INFO(trans, "alloc_pages failed, "
					   "order: %d\n",
					   hw_params(trans).rx_page_order);

			if ((rxq->free_count <= RX_LOW_WATERMARK) &&
			    net_ratelimit())
				IWL_CRIT(trans, "Failed to alloc_pages with %s."
					 "Only %u free buffers remaining.\n",
					 priority == GFP_ATOMIC ?
					 "GFP_ATOMIC" : "GFP_KERNEL",
					 rxq->free_count);
			return;
		}

		spin_lock_irqsave(&rxq->lock, flags);

		if (list_empty(&rxq->rx_used)) {
			spin_unlock_irqrestore(&rxq->lock, flags);
			__free_pages(page, hw_params(trans).rx_page_order);
			return;
		}
		element = rxq->rx_used.next;
		rxb = list_entry(element, struct iwl_rx_mem_buffer, list);
		list_del(element);

		spin_unlock_irqrestore(&rxq->lock, flags);

		BUG_ON(rxb->page);
		rxb->page = page;
		
		rxb->page_dma = dma_map_page(trans->dev, page, 0,
				PAGE_SIZE << hw_params(trans).rx_page_order,
				DMA_FROM_DEVICE);
		
		BUG_ON(rxb->page_dma & ~DMA_BIT_MASK(36));
		
		BUG_ON(rxb->page_dma & DMA_BIT_MASK(8));

		spin_lock_irqsave(&rxq->lock, flags);

		list_add_tail(&rxb->list, &rxq->rx_free);
		rxq->free_count++;

		spin_unlock_irqrestore(&rxq->lock, flags);
	}
}

void iwlagn_rx_replenish(struct iwl_trans *trans)
{
	struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
	unsigned long flags;

	iwlagn_rx_allocate(trans, GFP_KERNEL);

	spin_lock_irqsave(&trans_pcie->irq_lock, flags);
	iwlagn_rx_queue_restock(trans);
	spin_unlock_irqrestore(&trans_pcie->irq_lock, flags);
}

static void iwlagn_rx_replenish_now(struct iwl_trans *trans)
{
	iwlagn_rx_allocate(trans, GFP_ATOMIC);

	iwlagn_rx_queue_restock(trans);
}

void iwl_bg_rx_replenish(struct work_struct *data)
{
	struct iwl_trans_pcie *trans_pcie =
	    container_of(data, struct iwl_trans_pcie, rx_replenish);

	iwlagn_rx_replenish(trans_pcie->trans);
}

static void iwl_rx_handle_rxbuf(struct iwl_trans *trans,
				struct iwl_rx_mem_buffer *rxb)
{
	struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
	struct iwl_rx_queue *rxq = &trans_pcie->rxq;
	struct iwl_tx_queue *txq = &trans_pcie->txq[trans_pcie->cmd_queue];
	struct iwl_device_cmd *cmd;
	unsigned long flags;
	int len, err;
	u16 sequence;
	struct iwl_rx_cmd_buffer rxcb;
	struct iwl_rx_packet *pkt;
	bool reclaim;
	int index, cmd_index;

	if (WARN_ON(!rxb))
		return;

	rxcb.truesize = PAGE_SIZE << hw_params(trans).rx_page_order;
	dma_unmap_page(trans->dev, rxb->page_dma,
		       rxcb.truesize,
		       DMA_FROM_DEVICE);

	rxcb._page = rxb->page;
	pkt = rxb_addr(&rxcb);

	IWL_DEBUG_RX(trans, "%s, 0x%02x\n",
		     get_cmd_string(pkt->hdr.cmd), pkt->hdr.cmd);


	len = le32_to_cpu(pkt->len_n_flags) & FH_RSCSR_FRAME_SIZE_MSK;
	len += sizeof(u32); 
	trace_iwlwifi_dev_rx(trans->dev, pkt, len);

	reclaim = !(pkt->hdr.sequence & SEQ_RX_FRAME);
	if (reclaim) {
		int i;

		for (i = 0; i < trans_pcie->n_no_reclaim_cmds; i++) {
			if (trans_pcie->no_reclaim_cmds[i] == pkt->hdr.cmd) {
				reclaim = false;
				break;
			}
		}
	}

	sequence = le16_to_cpu(pkt->hdr.sequence);
	index = SEQ_TO_INDEX(sequence);
	cmd_index = get_cmd_index(&txq->q, index);

	if (reclaim)
		cmd = txq->cmd[cmd_index];
	else
		cmd = NULL;

	err = iwl_op_mode_rx(trans->op_mode, &rxcb, cmd);


	if (reclaim) {
		if (rxcb._page)
			iwl_tx_cmd_complete(trans, &rxcb, err);
		else
			IWL_WARN(trans, "Claim null rxb?\n");
	}

	
	if (rxcb._page == NULL)
		rxb->page = NULL;

	spin_lock_irqsave(&rxq->lock, flags);
	if (rxb->page != NULL) {
		rxb->page_dma =
			dma_map_page(trans->dev, rxb->page, 0,
				PAGE_SIZE << hw_params(trans).rx_page_order,
				DMA_FROM_DEVICE);
		list_add_tail(&rxb->list, &rxq->rx_free);
		rxq->free_count++;
	} else
		list_add_tail(&rxb->list, &rxq->rx_used);
	spin_unlock_irqrestore(&rxq->lock, flags);
}

static void iwl_rx_handle(struct iwl_trans *trans)
{
	struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
	struct iwl_rx_queue *rxq = &trans_pcie->rxq;
	u32 r, i;
	u8 fill_rx = 0;
	u32 count = 8;
	int total_empty;

	r = le16_to_cpu(rxq->rb_stts->closed_rb_num) &  0x0FFF;
	i = rxq->read;

	
	if (i == r)
		IWL_DEBUG_RX(trans, "r = %d, i = %d\n", r, i);

	
	total_empty = r - rxq->write_actual;
	if (total_empty < 0)
		total_empty += RX_QUEUE_SIZE;

	if (total_empty > (RX_QUEUE_SIZE / 2))
		fill_rx = 1;

	while (i != r) {
		struct iwl_rx_mem_buffer *rxb;

		rxb = rxq->queue[i];
		rxq->queue[i] = NULL;

		IWL_DEBUG_RX(trans, "rxbuf: r = %d, i = %d (%p)\n", rxb);

		iwl_rx_handle_rxbuf(trans, rxb);

		i = (i + 1) & RX_QUEUE_MASK;
		if (fill_rx) {
			count++;
			if (count >= 8) {
				rxq->read = i;
				iwlagn_rx_replenish_now(trans);
				count = 0;
			}
		}
	}

	
	rxq->read = i;
	if (fill_rx)
		iwlagn_rx_replenish_now(trans);
	else
		iwlagn_rx_queue_restock(trans);
}

static const char * const desc_lookup_text[] = {
	"OK",
	"FAIL",
	"BAD_PARAM",
	"BAD_CHECKSUM",
	"NMI_INTERRUPT_WDG",
	"SYSASSERT",
	"FATAL_ERROR",
	"BAD_COMMAND",
	"HW_ERROR_TUNE_LOCK",
	"HW_ERROR_TEMPERATURE",
	"ILLEGAL_CHAN_FREQ",
	"VCC_NOT_STABLE",
	"FH_ERROR",
	"NMI_INTERRUPT_HOST",
	"NMI_INTERRUPT_ACTION_PT",
	"NMI_INTERRUPT_UNKNOWN",
	"UCODE_VERSION_MISMATCH",
	"HW_ERROR_ABS_LOCK",
	"HW_ERROR_CAL_LOCK_FAIL",
	"NMI_INTERRUPT_INST_ACTION_PT",
	"NMI_INTERRUPT_DATA_ACTION_PT",
	"NMI_TRM_HW_ER",
	"NMI_INTERRUPT_TRM",
	"NMI_INTERRUPT_BREAK_POINT",
	"DEBUG_0",
	"DEBUG_1",
	"DEBUG_2",
	"DEBUG_3",
};

static struct { char *name; u8 num; } advanced_lookup[] = {
	{ "NMI_INTERRUPT_WDG", 0x34 },
	{ "SYSASSERT", 0x35 },
	{ "UCODE_VERSION_MISMATCH", 0x37 },
	{ "BAD_COMMAND", 0x38 },
	{ "NMI_INTERRUPT_DATA_ACTION_PT", 0x3C },
	{ "FATAL_ERROR", 0x3D },
	{ "NMI_TRM_HW_ERR", 0x46 },
	{ "NMI_INTERRUPT_TRM", 0x4C },
	{ "NMI_INTERRUPT_BREAK_POINT", 0x54 },
	{ "NMI_INTERRUPT_WDG_RXF_FULL", 0x5C },
	{ "NMI_INTERRUPT_WDG_NO_RBD_RXF_FULL", 0x64 },
	{ "NMI_INTERRUPT_HOST", 0x66 },
	{ "NMI_INTERRUPT_ACTION_PT", 0x7C },
	{ "NMI_INTERRUPT_UNKNOWN", 0x84 },
	{ "NMI_INTERRUPT_INST_ACTION_PT", 0x86 },
	{ "ADVANCED_SYSASSERT", 0 },
};

static const char *desc_lookup(u32 num)
{
	int i;
	int max = ARRAY_SIZE(desc_lookup_text);

	if (num < max)
		return desc_lookup_text[num];

	max = ARRAY_SIZE(advanced_lookup) - 1;
	for (i = 0; i < max; i++) {
		if (advanced_lookup[i].num == num)
			break;
	}
	return advanced_lookup[i].name;
}

#define ERROR_START_OFFSET  (1 * sizeof(u32))
#define ERROR_ELEM_SIZE     (7 * sizeof(u32))

static void iwl_dump_nic_error_log(struct iwl_trans *trans)
{
	u32 base;
	struct iwl_error_event_table table;
	struct iwl_trans_pcie *trans_pcie =
		IWL_TRANS_GET_PCIE_TRANS(trans);

	base = trans->shrd->device_pointers.error_event_table;
	if (trans->shrd->ucode_type == IWL_UCODE_INIT) {
		if (!base)
			base = trans->shrd->fw->init_errlog_ptr;
	} else {
		if (!base)
			base = trans->shrd->fw->inst_errlog_ptr;
	}

	if (!iwlagn_hw_valid_rtc_data_addr(base)) {
		IWL_ERR(trans,
			"Not valid error log pointer 0x%08X for %s uCode\n",
			base,
			(trans->shrd->ucode_type == IWL_UCODE_INIT)
					? "Init" : "RT");
		return;
	}

	iwl_read_targ_mem_words(trans, base, &table, sizeof(table));

	if (ERROR_START_OFFSET <= table.valid * ERROR_ELEM_SIZE) {
		IWL_ERR(trans, "Start IWL Error Log Dump:\n");
		IWL_ERR(trans, "Status: 0x%08lX, count: %d\n",
			trans->shrd->status, table.valid);
	}

	trans_pcie->isr_stats.err_code = table.error_id;

	trace_iwlwifi_dev_ucode_error(trans->dev, table.error_id, table.tsf_low,
				      table.data1, table.data2, table.line,
				      table.blink1, table.blink2, table.ilink1,
				      table.ilink2, table.bcon_time, table.gp1,
				      table.gp2, table.gp3, table.ucode_ver,
				      table.hw_ver, table.brd_ver);
	IWL_ERR(trans, "0x%08X | %-28s\n", table.error_id,
		desc_lookup(table.error_id));
	IWL_ERR(trans, "0x%08X | uPc\n", table.pc);
	IWL_ERR(trans, "0x%08X | branchlink1\n", table.blink1);
	IWL_ERR(trans, "0x%08X | branchlink2\n", table.blink2);
	IWL_ERR(trans, "0x%08X | interruptlink1\n", table.ilink1);
	IWL_ERR(trans, "0x%08X | interruptlink2\n", table.ilink2);
	IWL_ERR(trans, "0x%08X | data1\n", table.data1);
	IWL_ERR(trans, "0x%08X | data2\n", table.data2);
	IWL_ERR(trans, "0x%08X | line\n", table.line);
	IWL_ERR(trans, "0x%08X | beacon time\n", table.bcon_time);
	IWL_ERR(trans, "0x%08X | tsf low\n", table.tsf_low);
	IWL_ERR(trans, "0x%08X | tsf hi\n", table.tsf_hi);
	IWL_ERR(trans, "0x%08X | time gp1\n", table.gp1);
	IWL_ERR(trans, "0x%08X | time gp2\n", table.gp2);
	IWL_ERR(trans, "0x%08X | time gp3\n", table.gp3);
	IWL_ERR(trans, "0x%08X | uCode version\n", table.ucode_ver);
	IWL_ERR(trans, "0x%08X | hw version\n", table.hw_ver);
	IWL_ERR(trans, "0x%08X | board version\n", table.brd_ver);
	IWL_ERR(trans, "0x%08X | hcmd\n", table.hcmd);

	IWL_ERR(trans, "0x%08X | isr0\n", table.isr0);
	IWL_ERR(trans, "0x%08X | isr1\n", table.isr1);
	IWL_ERR(trans, "0x%08X | isr2\n", table.isr2);
	IWL_ERR(trans, "0x%08X | isr3\n", table.isr3);
	IWL_ERR(trans, "0x%08X | isr4\n", table.isr4);
	IWL_ERR(trans, "0x%08X | isr_pref\n", table.isr_pref);
	IWL_ERR(trans, "0x%08X | wait_event\n", table.wait_event);
	IWL_ERR(trans, "0x%08X | l2p_control\n", table.l2p_control);
	IWL_ERR(trans, "0x%08X | l2p_duration\n", table.l2p_duration);
	IWL_ERR(trans, "0x%08X | l2p_mhvalid\n", table.l2p_mhvalid);
	IWL_ERR(trans, "0x%08X | l2p_addr_match\n", table.l2p_addr_match);
	IWL_ERR(trans, "0x%08X | lmpm_pmg_sel\n", table.lmpm_pmg_sel);
	IWL_ERR(trans, "0x%08X | timestamp\n", table.u_timestamp);
	IWL_ERR(trans, "0x%08X | flow_handler\n", table.flow_handler);
}

static void iwl_irq_handle_error(struct iwl_trans *trans)
{
	
	if (cfg(trans)->internal_wimax_coex &&
	    (!(iwl_read_prph(trans, APMG_CLK_CTRL_REG) &
			APMS_CLK_VAL_MRB_FUNC_MODE) ||
	     (iwl_read_prph(trans, APMG_PS_CTRL_REG) &
			APMG_PS_CTRL_VAL_RESET_REQ))) {
		clear_bit(STATUS_READY, &trans->shrd->status);
		clear_bit(STATUS_HCMD_ACTIVE, &trans->shrd->status);
		wake_up(&trans->wait_command_queue);
		IWL_ERR(trans, "RF is used by WiMAX\n");
		return;
	}

	IWL_ERR(trans, "Loaded firmware version: %s\n",
		trans->shrd->fw->fw_version);

	iwl_dump_nic_error_log(trans);
	iwl_dump_csr(trans);
	iwl_dump_fh(trans, NULL, false);
	iwl_dump_nic_event_log(trans, false, NULL, false);

	iwl_op_mode_nic_error(trans->op_mode);
}

#define EVENT_START_OFFSET  (4 * sizeof(u32))

static int iwl_print_event_log(struct iwl_trans *trans, u32 start_idx,
			       u32 num_events, u32 mode,
			       int pos, char **buf, size_t bufsz)
{
	u32 i;
	u32 base;       
	u32 event_size; 
	u32 ptr;        
	u32 ev, time, data; 
	unsigned long reg_flags;

	if (num_events == 0)
		return pos;

	base = trans->shrd->device_pointers.log_event_table;
	if (trans->shrd->ucode_type == IWL_UCODE_INIT) {
		if (!base)
			base = trans->shrd->fw->init_evtlog_ptr;
	} else {
		if (!base)
			base = trans->shrd->fw->inst_evtlog_ptr;
	}

	if (mode == 0)
		event_size = 2 * sizeof(u32);
	else
		event_size = 3 * sizeof(u32);

	ptr = base + EVENT_START_OFFSET + (start_idx * event_size);

	
	spin_lock_irqsave(&trans->reg_lock, reg_flags);
	if (unlikely(!iwl_grab_nic_access(trans)))
		goto out_unlock;

	
	iwl_write32(trans, HBUS_TARG_MEM_RADDR, ptr);

	for (i = 0; i < num_events; i++) {
		ev = iwl_read32(trans, HBUS_TARG_MEM_RDAT);
		time = iwl_read32(trans, HBUS_TARG_MEM_RDAT);
		if (mode == 0) {
			
			if (bufsz) {
				pos += scnprintf(*buf + pos, bufsz - pos,
						"EVT_LOG:0x%08x:%04u\n",
						time, ev);
			} else {
				trace_iwlwifi_dev_ucode_event(trans->dev, 0,
					time, ev);
				IWL_ERR(trans, "EVT_LOG:0x%08x:%04u\n",
					time, ev);
			}
		} else {
			data = iwl_read32(trans, HBUS_TARG_MEM_RDAT);
			if (bufsz) {
				pos += scnprintf(*buf + pos, bufsz - pos,
						"EVT_LOGT:%010u:0x%08x:%04u\n",
						 time, data, ev);
			} else {
				IWL_ERR(trans, "EVT_LOGT:%010u:0x%08x:%04u\n",
					time, data, ev);
				trace_iwlwifi_dev_ucode_event(trans->dev, time,
					data, ev);
			}
		}
	}

	
	iwl_release_nic_access(trans);
out_unlock:
	spin_unlock_irqrestore(&trans->reg_lock, reg_flags);
	return pos;
}

static int iwl_print_last_event_logs(struct iwl_trans *trans, u32 capacity,
				    u32 num_wraps, u32 next_entry,
				    u32 size, u32 mode,
				    int pos, char **buf, size_t bufsz)
{
	if (num_wraps) {
		if (next_entry < size) {
			pos = iwl_print_event_log(trans,
						capacity - (size - next_entry),
						size - next_entry, mode,
						pos, buf, bufsz);
			pos = iwl_print_event_log(trans, 0,
						  next_entry, mode,
						  pos, buf, bufsz);
		} else
			pos = iwl_print_event_log(trans, next_entry - size,
						  size, mode, pos, buf, bufsz);
	} else {
		if (next_entry < size) {
			pos = iwl_print_event_log(trans, 0, next_entry,
						  mode, pos, buf, bufsz);
		} else {
			pos = iwl_print_event_log(trans, next_entry - size,
						  size, mode, pos, buf, bufsz);
		}
	}
	return pos;
}

#define DEFAULT_DUMP_EVENT_LOG_ENTRIES (20)

int iwl_dump_nic_event_log(struct iwl_trans *trans, bool full_log,
			    char **buf, bool display)
{
	u32 base;       
	u32 capacity;   
	u32 mode;       
	u32 num_wraps;  
	u32 next_entry; /* index of next entry to be written by uCode */
	u32 size;       
	u32 logsize;
	int pos = 0;
	size_t bufsz = 0;

	base = trans->shrd->device_pointers.log_event_table;
	if (trans->shrd->ucode_type == IWL_UCODE_INIT) {
		logsize = trans->shrd->fw->init_evtlog_size;
		if (!base)
			base = trans->shrd->fw->init_evtlog_ptr;
	} else {
		logsize = trans->shrd->fw->inst_evtlog_size;
		if (!base)
			base = trans->shrd->fw->inst_evtlog_ptr;
	}

	if (!iwlagn_hw_valid_rtc_data_addr(base)) {
		IWL_ERR(trans,
			"Invalid event log pointer 0x%08X for %s uCode\n",
			base,
			(trans->shrd->ucode_type == IWL_UCODE_INIT)
					? "Init" : "RT");
		return -EINVAL;
	}

	
	capacity = iwl_read_targ_mem(trans, base);
	mode = iwl_read_targ_mem(trans, base + (1 * sizeof(u32)));
	num_wraps = iwl_read_targ_mem(trans, base + (2 * sizeof(u32)));
	next_entry = iwl_read_targ_mem(trans, base + (3 * sizeof(u32)));

	if (capacity > logsize) {
		IWL_ERR(trans, "Log capacity %d is bogus, limit to %d "
			"entries\n", capacity, logsize);
		capacity = logsize;
	}

	if (next_entry > logsize) {
		IWL_ERR(trans, "Log write index %d is bogus, limit to %d\n",
			next_entry, logsize);
		next_entry = logsize;
	}

	size = num_wraps ? capacity : next_entry;

	
	if (size == 0) {
		IWL_ERR(trans, "Start IWL Event Log Dump: nothing in log\n");
		return pos;
	}

#ifdef CONFIG_IWLWIFI_DEBUG
	if (!(iwl_have_debug_level(IWL_DL_FW_ERRORS)) && !full_log)
		size = (size > DEFAULT_DUMP_EVENT_LOG_ENTRIES)
			? DEFAULT_DUMP_EVENT_LOG_ENTRIES : size;
#else
	size = (size > DEFAULT_DUMP_EVENT_LOG_ENTRIES)
		? DEFAULT_DUMP_EVENT_LOG_ENTRIES : size;
#endif
	IWL_ERR(trans, "Start IWL Event Log Dump: display last %u entries\n",
		size);

#ifdef CONFIG_IWLWIFI_DEBUG
	if (display) {
		if (full_log)
			bufsz = capacity * 48;
		else
			bufsz = size * 48;
		*buf = kmalloc(bufsz, GFP_KERNEL);
		if (!*buf)
			return -ENOMEM;
	}
	if (iwl_have_debug_level(IWL_DL_FW_ERRORS) || full_log) {
		if (num_wraps)
			pos = iwl_print_event_log(trans, next_entry,
						capacity - next_entry, mode,
						pos, buf, bufsz);
		
		pos = iwl_print_event_log(trans, 0,
					  next_entry, mode, pos, buf, bufsz);
	} else
		pos = iwl_print_last_event_logs(trans, capacity, num_wraps,
						next_entry, size, mode,
						pos, buf, bufsz);
#else
	pos = iwl_print_last_event_logs(trans, capacity, num_wraps,
					next_entry, size, mode,
					pos, buf, bufsz);
#endif
	return pos;
}

void iwl_irq_tasklet(struct iwl_trans *trans)
{
	u32 inta = 0;
	u32 handled = 0;
	unsigned long flags;
	u32 i;
#ifdef CONFIG_IWLWIFI_DEBUG
	u32 inta_mask;
#endif

	struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
	struct isr_statistics *isr_stats = &trans_pcie->isr_stats;


	spin_lock_irqsave(&trans_pcie->irq_lock, flags);

	iwl_write32(trans, CSR_INT,
		trans_pcie->inta | ~trans_pcie->inta_mask);

	inta = trans_pcie->inta;

#ifdef CONFIG_IWLWIFI_DEBUG
	if (iwl_have_debug_level(IWL_DL_ISR)) {
		
		inta_mask = iwl_read32(trans, CSR_INT_MASK);
		IWL_DEBUG_ISR(trans, "inta 0x%08x, enabled 0x%08x\n ",
				inta, inta_mask);
	}
#endif

	
	trans_pcie->inta = 0;

	spin_unlock_irqrestore(&trans_pcie->irq_lock, flags);

	
	if (inta & CSR_INT_BIT_HW_ERR) {
		IWL_ERR(trans, "Hardware error detected.  Restarting.\n");

		
		iwl_disable_interrupts(trans);

		isr_stats->hw++;
		iwl_irq_handle_error(trans);

		handled |= CSR_INT_BIT_HW_ERR;

		return;
	}

#ifdef CONFIG_IWLWIFI_DEBUG
	if (iwl_have_debug_level(IWL_DL_ISR)) {
		
		if (inta & CSR_INT_BIT_SCD) {
			IWL_DEBUG_ISR(trans, "Scheduler finished to transmit "
				      "the frame/frames.\n");
			isr_stats->sch++;
		}

		
		if (inta & CSR_INT_BIT_ALIVE) {
			IWL_DEBUG_ISR(trans, "Alive interrupt\n");
			isr_stats->alive++;
		}
	}
#endif
	
	inta &= ~(CSR_INT_BIT_SCD | CSR_INT_BIT_ALIVE);

	
	if (inta & CSR_INT_BIT_RF_KILL) {
		bool hw_rfkill;

		hw_rfkill = !(iwl_read32(trans, CSR_GP_CNTRL) &
				CSR_GP_CNTRL_REG_FLAG_HW_RF_KILL_SW);
		IWL_WARN(trans, "RF_KILL bit toggled to %s.\n",
				hw_rfkill ? "disable radio" : "enable radio");

		isr_stats->rfkill++;

		iwl_op_mode_hw_rf_kill(trans->op_mode, hw_rfkill);

		handled |= CSR_INT_BIT_RF_KILL;
	}

	
	if (inta & CSR_INT_BIT_CT_KILL) {
		IWL_ERR(trans, "Microcode CT kill error detected.\n");
		isr_stats->ctkill++;
		handled |= CSR_INT_BIT_CT_KILL;
	}

	
	if (inta & CSR_INT_BIT_SW_ERR) {
		IWL_ERR(trans, "Microcode SW error detected. "
			" Restarting 0x%X.\n", inta);
		isr_stats->sw++;
		iwl_irq_handle_error(trans);
		handled |= CSR_INT_BIT_SW_ERR;
	}

	
	if (inta & CSR_INT_BIT_WAKEUP) {
		IWL_DEBUG_ISR(trans, "Wakeup interrupt\n");
		iwl_rx_queue_update_write_ptr(trans, &trans_pcie->rxq);
		for (i = 0; i < cfg(trans)->base_params->num_of_queues; i++)
			iwl_txq_update_write_ptr(trans,
						 &trans_pcie->txq[i]);

		isr_stats->wakeup++;

		handled |= CSR_INT_BIT_WAKEUP;
	}

	if (inta & (CSR_INT_BIT_FH_RX | CSR_INT_BIT_SW_RX |
			CSR_INT_BIT_RX_PERIODIC)) {
		IWL_DEBUG_ISR(trans, "Rx interrupt\n");
		if (inta & (CSR_INT_BIT_FH_RX | CSR_INT_BIT_SW_RX)) {
			handled |= (CSR_INT_BIT_FH_RX | CSR_INT_BIT_SW_RX);
			iwl_write32(trans, CSR_FH_INT_STATUS,
					CSR_FH_INT_RX_MASK);
		}
		if (inta & CSR_INT_BIT_RX_PERIODIC) {
			handled |= CSR_INT_BIT_RX_PERIODIC;
			iwl_write32(trans,
				CSR_INT, CSR_INT_BIT_RX_PERIODIC);
		}

		
		iwl_write8(trans, CSR_INT_PERIODIC_REG,
			    CSR_INT_PERIODIC_DIS);
#ifdef CONFIG_IWLWIFI_IDI
		iwl_amfh_rx_handler();
#else
		iwl_rx_handle(trans);
#endif
		if (inta & (CSR_INT_BIT_FH_RX | CSR_INT_BIT_SW_RX))
			iwl_write8(trans, CSR_INT_PERIODIC_REG,
				    CSR_INT_PERIODIC_ENA);

		isr_stats->rx++;
	}

	
	if (inta & CSR_INT_BIT_FH_TX) {
		iwl_write32(trans, CSR_FH_INT_STATUS, CSR_FH_INT_TX_MASK);
		IWL_DEBUG_ISR(trans, "uCode load interrupt\n");
		isr_stats->tx++;
		handled |= CSR_INT_BIT_FH_TX;
		
		trans_pcie->ucode_write_complete = true;
		wake_up(&trans_pcie->ucode_write_waitq);
	}

	if (inta & ~handled) {
		IWL_ERR(trans, "Unhandled INTA bits 0x%08x\n", inta & ~handled);
		isr_stats->unhandled++;
	}

	if (inta & ~(trans_pcie->inta_mask)) {
		IWL_WARN(trans, "Disabled INTA bits 0x%08x were pending\n",
			 inta & ~trans_pcie->inta_mask);
	}

	
	
	if (test_bit(STATUS_INT_ENABLED, &trans_pcie->status))
		iwl_enable_interrupts(trans);
	
	else if (handled & CSR_INT_BIT_RF_KILL)
		iwl_enable_rfkill_int(trans);
}


#define ICT_SHIFT	12
#define ICT_SIZE	(1 << ICT_SHIFT)
#define ICT_COUNT	(ICT_SIZE / sizeof(u32))

void iwl_free_isr_ict(struct iwl_trans *trans)
{
	struct iwl_trans_pcie *trans_pcie =
		IWL_TRANS_GET_PCIE_TRANS(trans);

	if (trans_pcie->ict_tbl) {
		dma_free_coherent(trans->dev, ICT_SIZE,
				  trans_pcie->ict_tbl,
				  trans_pcie->ict_tbl_dma);
		trans_pcie->ict_tbl = NULL;
		trans_pcie->ict_tbl_dma = 0;
	}
}


int iwl_alloc_isr_ict(struct iwl_trans *trans)
{
	struct iwl_trans_pcie *trans_pcie =
		IWL_TRANS_GET_PCIE_TRANS(trans);

	trans_pcie->ict_tbl =
		dma_alloc_coherent(trans->dev, ICT_SIZE,
				   &trans_pcie->ict_tbl_dma,
				   GFP_KERNEL);
	if (!trans_pcie->ict_tbl)
		return -ENOMEM;

	
	if (WARN_ON(trans_pcie->ict_tbl_dma & (ICT_SIZE - 1))) {
		iwl_free_isr_ict(trans);
		return -EINVAL;
	}

	IWL_DEBUG_ISR(trans, "ict dma addr %Lx\n",
		      (unsigned long long)trans_pcie->ict_tbl_dma);

	IWL_DEBUG_ISR(trans, "ict vir addr %p\n", trans_pcie->ict_tbl);

	
	memset(trans_pcie->ict_tbl, 0, ICT_SIZE);
	trans_pcie->ict_index = 0;

	
	trans_pcie->inta_mask |= CSR_INT_BIT_RX_PERIODIC;
	return 0;
}

void iwl_reset_ict(struct iwl_trans *trans)
{
	u32 val;
	unsigned long flags;
	struct iwl_trans_pcie *trans_pcie =
		IWL_TRANS_GET_PCIE_TRANS(trans);

	if (!trans_pcie->ict_tbl)
		return;

	spin_lock_irqsave(&trans_pcie->irq_lock, flags);
	iwl_disable_interrupts(trans);

	memset(trans_pcie->ict_tbl, 0, ICT_SIZE);

	val = trans_pcie->ict_tbl_dma >> ICT_SHIFT;

	val |= CSR_DRAM_INT_TBL_ENABLE;
	val |= CSR_DRAM_INIT_TBL_WRAP_CHECK;

	IWL_DEBUG_ISR(trans, "CSR_DRAM_INT_TBL_REG =0x%x\n", val);

	iwl_write32(trans, CSR_DRAM_INT_TBL_REG, val);
	trans_pcie->use_ict = true;
	trans_pcie->ict_index = 0;
	iwl_write32(trans, CSR_INT, trans_pcie->inta_mask);
	iwl_enable_interrupts(trans);
	spin_unlock_irqrestore(&trans_pcie->irq_lock, flags);
}

void iwl_disable_ict(struct iwl_trans *trans)
{
	struct iwl_trans_pcie *trans_pcie =
		IWL_TRANS_GET_PCIE_TRANS(trans);

	unsigned long flags;

	spin_lock_irqsave(&trans_pcie->irq_lock, flags);
	trans_pcie->use_ict = false;
	spin_unlock_irqrestore(&trans_pcie->irq_lock, flags);
}

static irqreturn_t iwl_isr(int irq, void *data)
{
	struct iwl_trans *trans = data;
	struct iwl_trans_pcie *trans_pcie;
	u32 inta, inta_mask;
	unsigned long flags;
#ifdef CONFIG_IWLWIFI_DEBUG
	u32 inta_fh;
#endif
	if (!trans)
		return IRQ_NONE;

	trace_iwlwifi_dev_irq(trans->dev);

	trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);

	spin_lock_irqsave(&trans_pcie->irq_lock, flags);

	inta_mask = iwl_read32(trans, CSR_INT_MASK);  
	iwl_write32(trans, CSR_INT_MASK, 0x00000000);

	
	inta = iwl_read32(trans, CSR_INT);

	if (!inta) {
		IWL_DEBUG_ISR(trans, "Ignore interrupt, inta == 0\n");
		goto none;
	}

	if ((inta == 0xFFFFFFFF) || ((inta & 0xFFFFFFF0) == 0xa5a5a5a0)) {
		IWL_WARN(trans, "HARDWARE GONE?? INTA == 0x%08x\n", inta);
		goto unplugged;
	}

#ifdef CONFIG_IWLWIFI_DEBUG
	if (iwl_have_debug_level(IWL_DL_ISR)) {
		inta_fh = iwl_read32(trans, CSR_FH_INT_STATUS);
		IWL_DEBUG_ISR(trans, "ISR inta 0x%08x, enabled 0x%08x, "
			      "fh 0x%08x\n", inta, inta_mask, inta_fh);
	}
#endif

	trans_pcie->inta |= inta;
	
	if (likely(inta))
		tasklet_schedule(&trans_pcie->irq_tasklet);
	else if (test_bit(STATUS_INT_ENABLED, &trans_pcie->status) &&
			!trans_pcie->inta)
		iwl_enable_interrupts(trans);

 unplugged:
	spin_unlock_irqrestore(&trans_pcie->irq_lock, flags);
	return IRQ_HANDLED;

 none:
	
	
	if (test_bit(STATUS_INT_ENABLED, &trans_pcie->status) &&
		!trans_pcie->inta)
		iwl_enable_interrupts(trans);

	spin_unlock_irqrestore(&trans_pcie->irq_lock, flags);
	return IRQ_NONE;
}

irqreturn_t iwl_isr_ict(int irq, void *data)
{
	struct iwl_trans *trans = data;
	struct iwl_trans_pcie *trans_pcie;
	u32 inta, inta_mask;
	u32 val = 0;
	u32 read;
	unsigned long flags;

	if (!trans)
		return IRQ_NONE;

	trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);

	if (!trans_pcie->use_ict)
		return iwl_isr(irq, data);

	trace_iwlwifi_dev_irq(trans->dev);

	spin_lock_irqsave(&trans_pcie->irq_lock, flags);

	inta_mask = iwl_read32(trans, CSR_INT_MASK);  
	iwl_write32(trans, CSR_INT_MASK, 0x00000000);


	read = le32_to_cpu(trans_pcie->ict_tbl[trans_pcie->ict_index]);
	trace_iwlwifi_dev_ict_read(trans->dev, trans_pcie->ict_index, read);
	if (!read) {
		IWL_DEBUG_ISR(trans, "Ignore interrupt, inta == 0\n");
		goto none;
	}

	do {
		val |= read;
		IWL_DEBUG_ISR(trans, "ICT index %d value 0x%08X\n",
				trans_pcie->ict_index, read);
		trans_pcie->ict_tbl[trans_pcie->ict_index] = 0;
		trans_pcie->ict_index =
			iwl_queue_inc_wrap(trans_pcie->ict_index, ICT_COUNT);

		read = le32_to_cpu(trans_pcie->ict_tbl[trans_pcie->ict_index]);
		trace_iwlwifi_dev_ict_read(trans->dev, trans_pcie->ict_index,
					   read);
	} while (read);

	
	if (val == 0xffffffff)
		val = 0;

	if (val & 0xC0000)
		val |= 0x8000;

	inta = (0xff & val) | ((0xff00 & val) << 16);
	IWL_DEBUG_ISR(trans, "ISR inta 0x%08x, enabled 0x%08x ict 0x%08x\n",
			inta, inta_mask, val);

	inta &= trans_pcie->inta_mask;
	trans_pcie->inta |= inta;

	
	if (likely(inta))
		tasklet_schedule(&trans_pcie->irq_tasklet);
	else if (test_bit(STATUS_INT_ENABLED, &trans_pcie->status) &&
		 !trans_pcie->inta) {
		iwl_enable_interrupts(trans);
	}

	spin_unlock_irqrestore(&trans_pcie->irq_lock, flags);
	return IRQ_HANDLED;

 none:
	if (test_bit(STATUS_INT_ENABLED, &trans_pcie->status) &&
	    !trans_pcie->inta)
		iwl_enable_interrupts(trans);

	spin_unlock_irqrestore(&trans_pcie->irq_lock, flags);
	return IRQ_NONE;
}
