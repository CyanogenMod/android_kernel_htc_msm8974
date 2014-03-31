/*
 *  FM Driver for Connectivity chip of Texas Instruments.
 *
 *  This sub-module of FM driver is common for FM RX and TX
 *  functionality. This module is responsible for:
 *  1) Forming group of Channel-8 commands to perform particular
 *     functionality (eg., frequency set require more than
 *     one Channel-8 command to be sent to the chip).
 *  2) Sending each Channel-8 command to the chip and reading
 *     response back over Shared Transport.
 *  3) Managing TX and RX Queues and Tasklets.
 *  4) Handling FM Interrupt packet and taking appropriate action.
 *  5) Loading FM firmware to the chip (common, FM TX, and FM RX
 *     firmware files based on mode selection)
 *
 *  Copyright (C) 2011 Texas Instruments
 *  Author: Raja Mani <raja_mani@ti.com>
 *  Author: Manjunatha Halli <manjunatha_halli@ti.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/module.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include "fmdrv.h"
#include "fmdrv_v4l2.h"
#include "fmdrv_common.h"
#include <linux/ti_wilink_st.h>
#include "fmdrv_rx.h"
#include "fmdrv_tx.h"

static struct region_info region_configs[] = {
	
	{
	 .chanl_space = FM_CHANNEL_SPACING_200KHZ * FM_FREQ_MUL,
	 .bot_freq = 87500,	
	 .top_freq = 108000,	
	 .fm_band = 0,
	 },
	
	{
	 .chanl_space = FM_CHANNEL_SPACING_200KHZ * FM_FREQ_MUL,
	 .bot_freq = 76000,	
	 .top_freq = 90000,	
	 .fm_band = 1,
	 },
};

static u8 default_radio_region;	
module_param(default_radio_region, byte, 0);
MODULE_PARM_DESC(default_radio_region, "Region: 0=Europe/US, 1=Japan");

static u32 default_rds_buf = 300;
module_param(default_rds_buf, uint, 0444);
MODULE_PARM_DESC(rds_buf, "RDS buffer entries");

static u32 radio_nr = -1;
module_param(radio_nr, int, 0444);
MODULE_PARM_DESC(radio_nr, "Radio Nr");

static void fm_irq_send_flag_getcmd(struct fmdev *);
static void fm_irq_handle_flag_getcmd_resp(struct fmdev *);
static void fm_irq_handle_hw_malfunction(struct fmdev *);
static void fm_irq_handle_rds_start(struct fmdev *);
static void fm_irq_send_rdsdata_getcmd(struct fmdev *);
static void fm_irq_handle_rdsdata_getcmd_resp(struct fmdev *);
static void fm_irq_handle_rds_finish(struct fmdev *);
static void fm_irq_handle_tune_op_ended(struct fmdev *);
static void fm_irq_handle_power_enb(struct fmdev *);
static void fm_irq_handle_low_rssi_start(struct fmdev *);
static void fm_irq_afjump_set_pi(struct fmdev *);
static void fm_irq_handle_set_pi_resp(struct fmdev *);
static void fm_irq_afjump_set_pimask(struct fmdev *);
static void fm_irq_handle_set_pimask_resp(struct fmdev *);
static void fm_irq_afjump_setfreq(struct fmdev *);
static void fm_irq_handle_setfreq_resp(struct fmdev *);
static void fm_irq_afjump_enableint(struct fmdev *);
static void fm_irq_afjump_enableint_resp(struct fmdev *);
static void fm_irq_start_afjump(struct fmdev *);
static void fm_irq_handle_start_afjump_resp(struct fmdev *);
static void fm_irq_afjump_rd_freq(struct fmdev *);
static void fm_irq_afjump_rd_freq_resp(struct fmdev *);
static void fm_irq_handle_low_rssi_finish(struct fmdev *);
static void fm_irq_send_intmsk_cmd(struct fmdev *);
static void fm_irq_handle_intmsk_cmd_resp(struct fmdev *);

enum fmc_irq_handler_index {
	FM_SEND_FLAG_GETCMD_IDX,
	FM_HANDLE_FLAG_GETCMD_RESP_IDX,

	
	FM_HW_MAL_FUNC_IDX,

	
	FM_RDS_START_IDX,
	FM_RDS_SEND_RDS_GETCMD_IDX,
	FM_RDS_HANDLE_RDS_GETCMD_RESP_IDX,
	FM_RDS_FINISH_IDX,

	
	FM_HW_TUNE_OP_ENDED_IDX,

	
	FM_HW_POWER_ENB_IDX,

	
	FM_LOW_RSSI_START_IDX,
	FM_AF_JUMP_SETPI_IDX,
	FM_AF_JUMP_HANDLE_SETPI_RESP_IDX,
	FM_AF_JUMP_SETPI_MASK_IDX,
	FM_AF_JUMP_HANDLE_SETPI_MASK_RESP_IDX,
	FM_AF_JUMP_SET_AF_FREQ_IDX,
	FM_AF_JUMP_HANDLE_SET_AFFREQ_RESP_IDX,
	FM_AF_JUMP_ENABLE_INT_IDX,
	FM_AF_JUMP_ENABLE_INT_RESP_IDX,
	FM_AF_JUMP_START_AFJUMP_IDX,
	FM_AF_JUMP_HANDLE_START_AFJUMP_RESP_IDX,
	FM_AF_JUMP_RD_FREQ_IDX,
	FM_AF_JUMP_RD_FREQ_RESP_IDX,
	FM_LOW_RSSI_FINISH_IDX,

	
	FM_SEND_INTMSK_CMD_IDX,
	FM_HANDLE_INTMSK_CMD_RESP_IDX,
};

static int_handler_prototype int_handler_table[] = {
	fm_irq_send_flag_getcmd,
	fm_irq_handle_flag_getcmd_resp,
	fm_irq_handle_hw_malfunction,
	fm_irq_handle_rds_start, 
	fm_irq_send_rdsdata_getcmd,
	fm_irq_handle_rdsdata_getcmd_resp,
	fm_irq_handle_rds_finish,
	fm_irq_handle_tune_op_ended,
	fm_irq_handle_power_enb, 
	fm_irq_handle_low_rssi_start,
	fm_irq_afjump_set_pi,
	fm_irq_handle_set_pi_resp,
	fm_irq_afjump_set_pimask,
	fm_irq_handle_set_pimask_resp,
	fm_irq_afjump_setfreq,
	fm_irq_handle_setfreq_resp,
	fm_irq_afjump_enableint,
	fm_irq_afjump_enableint_resp,
	fm_irq_start_afjump,
	fm_irq_handle_start_afjump_resp,
	fm_irq_afjump_rd_freq,
	fm_irq_afjump_rd_freq_resp,
	fm_irq_handle_low_rssi_finish,
	fm_irq_send_intmsk_cmd, 
	fm_irq_handle_intmsk_cmd_resp
};

long (*g_st_write) (struct sk_buff *skb);
static struct completion wait_for_fmdrv_reg_comp;

static inline void fm_irq_call(struct fmdev *fmdev)
{
	fmdev->irq_info.handlers[fmdev->irq_info.stage](fmdev);
}

static inline void fm_irq_call_stage(struct fmdev *fmdev, u8 stage)
{
	fmdev->irq_info.stage = stage;
	fm_irq_call(fmdev);
}

static inline void fm_irq_timeout_stage(struct fmdev *fmdev, u8 stage)
{
	fmdev->irq_info.stage = stage;
	mod_timer(&fmdev->irq_info.timer, jiffies + FM_DRV_TX_TIMEOUT);
}

#ifdef FM_DUMP_TXRX_PKT
 
inline void dump_tx_skb_data(struct sk_buff *skb)
{
	int len, len_org;
	u8 index;
	struct fm_cmd_msg_hdr *cmd_hdr;

	cmd_hdr = (struct fm_cmd_msg_hdr *)skb->data;
	printk(KERN_INFO "<<%shdr:%02x len:%02x opcode:%02x type:%s dlen:%02x",
	       fm_cb(skb)->completion ? " " : "*", cmd_hdr->hdr,
	       cmd_hdr->len, cmd_hdr->op,
	       cmd_hdr->rd_wr ? "RD" : "WR", cmd_hdr->dlen);

	len_org = skb->len - FM_CMD_MSG_HDR_SIZE;
	if (len_org > 0) {
		printk("\n   data(%d): ", cmd_hdr->dlen);
		len = min(len_org, 14);
		for (index = 0; index < len; index++)
			printk("%x ",
			       skb->data[FM_CMD_MSG_HDR_SIZE + index]);
		printk("%s", (len_org > 14) ? ".." : "");
	}
	printk("\n");
}

 
inline void dump_rx_skb_data(struct sk_buff *skb)
{
	int len, len_org;
	u8 index;
	struct fm_event_msg_hdr *evt_hdr;

	evt_hdr = (struct fm_event_msg_hdr *)skb->data;
	printk(KERN_INFO ">> hdr:%02x len:%02x sts:%02x numhci:%02x "
	    "opcode:%02x type:%s dlen:%02x", evt_hdr->hdr, evt_hdr->len,
	    evt_hdr->status, evt_hdr->num_fm_hci_cmds, evt_hdr->op,
	    (evt_hdr->rd_wr) ? "RD" : "WR", evt_hdr->dlen);

	len_org = skb->len - FM_EVT_MSG_HDR_SIZE;
	if (len_org > 0) {
		printk("\n   data(%d): ", evt_hdr->dlen);
		len = min(len_org, 14);
		for (index = 0; index < len; index++)
			printk("%x ",
			       skb->data[FM_EVT_MSG_HDR_SIZE + index]);
		printk("%s", (len_org > 14) ? ".." : "");
	}
	printk("\n");
}
#endif

void fmc_update_region_info(struct fmdev *fmdev, u8 region_to_set)
{
	fmdev->rx.region = region_configs[region_to_set];
}

static void recv_tasklet(unsigned long arg)
{
	struct fmdev *fmdev;
	struct fm_irq *irq_info;
	struct fm_event_msg_hdr *evt_hdr;
	struct sk_buff *skb;
	u8 num_fm_hci_cmds;
	unsigned long flags;

	fmdev = (struct fmdev *)arg;
	irq_info = &fmdev->irq_info;
	
	while ((skb = skb_dequeue(&fmdev->rx_q))) {
		if (skb->len < sizeof(struct fm_event_msg_hdr)) {
			fmerr("skb(%p) has only %d bytes, "
				"at least need %zu bytes to decode\n", skb,
				skb->len, sizeof(struct fm_event_msg_hdr));
			kfree_skb(skb);
			continue;
		}

		evt_hdr = (void *)skb->data;
		num_fm_hci_cmds = evt_hdr->num_fm_hci_cmds;

		
		if (evt_hdr->op == FM_INTERRUPT) {
			
			if (!test_bit(FM_INTTASK_RUNNING, &fmdev->flag)) {
				set_bit(FM_INTTASK_RUNNING, &fmdev->flag);
				if (irq_info->stage != 0) {
					fmerr("Inval stage resetting to zero\n");
					irq_info->stage = 0;
				}

				irq_info->handlers[irq_info->stage](fmdev);
			} else {
				set_bit(FM_INTTASK_SCHEDULE_PENDING, &fmdev->flag);
			}
			kfree_skb(skb);
		}
		
		else if (evt_hdr->op == fmdev->pre_op && fmdev->resp_comp != NULL) {

			spin_lock_irqsave(&fmdev->resp_skb_lock, flags);
			fmdev->resp_skb = skb;
			spin_unlock_irqrestore(&fmdev->resp_skb_lock, flags);
			complete(fmdev->resp_comp);

			fmdev->resp_comp = NULL;
			atomic_set(&fmdev->tx_cnt, 1);
		}
		
		else if (evt_hdr->op == fmdev->pre_op && fmdev->resp_comp == NULL) {
			if (fmdev->resp_skb != NULL)
				fmerr("Response SKB ptr not NULL\n");

			spin_lock_irqsave(&fmdev->resp_skb_lock, flags);
			fmdev->resp_skb = skb;
			spin_unlock_irqrestore(&fmdev->resp_skb_lock, flags);

			
			irq_info->handlers[irq_info->stage](fmdev);

			kfree_skb(skb);
			atomic_set(&fmdev->tx_cnt, 1);
		} else {
			fmerr("Nobody claimed SKB(%p),purging\n", skb);
		}

		if (num_fm_hci_cmds && atomic_read(&fmdev->tx_cnt))
			if (!skb_queue_empty(&fmdev->tx_q))
				tasklet_schedule(&fmdev->tx_task);
	}
}

static void send_tasklet(unsigned long arg)
{
	struct fmdev *fmdev;
	struct sk_buff *skb;
	int len;

	fmdev = (struct fmdev *)arg;

	if (!atomic_read(&fmdev->tx_cnt))
		return;

	
	if ((jiffies - fmdev->last_tx_jiffies) > FM_DRV_TX_TIMEOUT) {
		fmerr("TX timeout occurred\n");
		atomic_set(&fmdev->tx_cnt, 1);
	}

	
	skb = skb_dequeue(&fmdev->tx_q);
	if (!skb)
		return;

	atomic_dec(&fmdev->tx_cnt);
	fmdev->pre_op = fm_cb(skb)->fm_op;

	if (fmdev->resp_comp != NULL)
		fmerr("Response completion handler is not NULL\n");

	fmdev->resp_comp = fm_cb(skb)->completion;

	
	len = g_st_write(skb);
	if (len < 0) {
		kfree_skb(skb);
		fmdev->resp_comp = NULL;
		fmerr("TX tasklet failed to send skb(%p)\n", skb);
		atomic_set(&fmdev->tx_cnt, 1);
	} else {
		fmdev->last_tx_jiffies = jiffies;
	}
}

static int fm_send_cmd(struct fmdev *fmdev, u8 fm_op, u16 type,	void *payload,
		int payload_len, struct completion *wait_completion)
{
	struct sk_buff *skb;
	struct fm_cmd_msg_hdr *hdr;
	int size;

	if (fm_op >= FM_INTERRUPT) {
		fmerr("Invalid fm opcode - %d\n", fm_op);
		return -EINVAL;
	}
	if (test_bit(FM_FW_DW_INPROGRESS, &fmdev->flag) && payload == NULL) {
		fmerr("Payload data is NULL during fw download\n");
		return -EINVAL;
	}
	if (!test_bit(FM_FW_DW_INPROGRESS, &fmdev->flag))
		size =
		    FM_CMD_MSG_HDR_SIZE + ((payload == NULL) ? 0 : payload_len);
	else
		size = payload_len;

	skb = alloc_skb(size, GFP_ATOMIC);
	if (!skb) {
		fmerr("No memory to create new SKB\n");
		return -ENOMEM;
	}
	if (!test_bit(FM_FW_DW_INPROGRESS, &fmdev->flag) ||
			test_bit(FM_INTTASK_RUNNING, &fmdev->flag)) {
		
		hdr = (struct fm_cmd_msg_hdr *)skb_put(skb, FM_CMD_MSG_HDR_SIZE);
		hdr->hdr = FM_PKT_LOGICAL_CHAN_NUMBER;	

		
		hdr->len = ((payload == NULL) ? 0 : payload_len) + 3;

		
		hdr->op = fm_op;

		
		hdr->rd_wr = type;
		hdr->dlen = payload_len;
		fm_cb(skb)->fm_op = fm_op;

		if (payload != NULL)
			*(u16 *)payload = cpu_to_be16(*(u16 *)payload);

	} else if (payload != NULL) {
		fm_cb(skb)->fm_op = *((u8 *)payload + 2);
	}
	if (payload != NULL)
		memcpy(skb_put(skb, payload_len), payload, payload_len);

	fm_cb(skb)->completion = wait_completion;
	skb_queue_tail(&fmdev->tx_q, skb);
	tasklet_schedule(&fmdev->tx_task);

	return 0;
}

int fmc_send_cmd(struct fmdev *fmdev, u8 fm_op, u16 type, void *payload,
		unsigned int payload_len, void *response, int *response_len)
{
	struct sk_buff *skb;
	struct fm_event_msg_hdr *evt_hdr;
	unsigned long flags;
	int ret;

	init_completion(&fmdev->maintask_comp);
	ret = fm_send_cmd(fmdev, fm_op, type, payload, payload_len,
			    &fmdev->maintask_comp);
	if (ret)
		return ret;

	if (!wait_for_completion_timeout(&fmdev->maintask_comp,
					 FM_DRV_TX_TIMEOUT)) {
		fmerr("Timeout(%d sec),didn't get reg"
			   "completion signal from RX tasklet\n",
			   jiffies_to_msecs(FM_DRV_TX_TIMEOUT) / 1000);
		return -ETIMEDOUT;
	}
	if (!fmdev->resp_skb) {
		fmerr("Response SKB is missing\n");
		return -EFAULT;
	}
	spin_lock_irqsave(&fmdev->resp_skb_lock, flags);
	skb = fmdev->resp_skb;
	fmdev->resp_skb = NULL;
	spin_unlock_irqrestore(&fmdev->resp_skb_lock, flags);

	evt_hdr = (void *)skb->data;
	if (evt_hdr->status != 0) {
		fmerr("Received event pkt status(%d) is not zero\n",
			   evt_hdr->status);
		kfree_skb(skb);
		return -EIO;
	}
	
	if (response != NULL && response_len != NULL && evt_hdr->dlen) {
		
		skb_pull(skb, sizeof(struct fm_event_msg_hdr));
		memcpy(response, skb->data, evt_hdr->dlen);
		*response_len = evt_hdr->dlen;
	} else if (response_len != NULL && evt_hdr->dlen == 0) {
		*response_len = 0;
	}
	kfree_skb(skb);

	return 0;
}

static inline int check_cmdresp_status(struct fmdev *fmdev,
		struct sk_buff **skb)
{
	struct fm_event_msg_hdr *fm_evt_hdr;
	unsigned long flags;

	del_timer(&fmdev->irq_info.timer);

	spin_lock_irqsave(&fmdev->resp_skb_lock, flags);
	*skb = fmdev->resp_skb;
	fmdev->resp_skb = NULL;
	spin_unlock_irqrestore(&fmdev->resp_skb_lock, flags);

	fm_evt_hdr = (void *)(*skb)->data;
	if (fm_evt_hdr->status != 0) {
		fmerr("irq: opcode %x response status is not zero "
				"Initiating irq recovery process\n",
				fm_evt_hdr->op);

		mod_timer(&fmdev->irq_info.timer, jiffies + FM_DRV_TX_TIMEOUT);
		return -1;
	}

	return 0;
}

static inline void fm_irq_common_cmd_resp_helper(struct fmdev *fmdev, u8 stage)
{
	struct sk_buff *skb;

	if (!check_cmdresp_status(fmdev, &skb))
		fm_irq_call_stage(fmdev, stage);
}

static void int_timeout_handler(unsigned long data)
{
	struct fmdev *fmdev;
	struct fm_irq *fmirq;

	fmdbg("irq: timeout,trying to re-enable fm interrupts\n");
	fmdev = (struct fmdev *)data;
	fmirq = &fmdev->irq_info;
	fmirq->retry++;

	if (fmirq->retry > FM_IRQ_TIMEOUT_RETRY_MAX) {
		fmirq->stage = 0;
		fmirq->retry = 0;
		fmerr("Recovery action failed during"
				"irq processing, max retry reached\n");
		return;
	}
	fm_irq_call_stage(fmdev, FM_SEND_INTMSK_CMD_IDX);
}

static void fm_irq_send_flag_getcmd(struct fmdev *fmdev)
{
	u16 flag;

	
	if (!fm_send_cmd(fmdev, FLAG_GET, REG_RD, NULL, sizeof(flag), NULL))
		fm_irq_timeout_stage(fmdev, FM_HANDLE_FLAG_GETCMD_RESP_IDX);
}

static void fm_irq_handle_flag_getcmd_resp(struct fmdev *fmdev)
{
	struct sk_buff *skb;
	struct fm_event_msg_hdr *fm_evt_hdr;

	if (check_cmdresp_status(fmdev, &skb))
		return;

	fm_evt_hdr = (void *)skb->data;

	
	skb_pull(skb, sizeof(struct fm_event_msg_hdr));
	memcpy(&fmdev->irq_info.flag, skb->data, fm_evt_hdr->dlen);

	fmdev->irq_info.flag = be16_to_cpu(fmdev->irq_info.flag);
	fmdbg("irq: flag register(0x%x)\n", fmdev->irq_info.flag);

	
	fm_irq_call_stage(fmdev, FM_HW_MAL_FUNC_IDX);
}

static void fm_irq_handle_hw_malfunction(struct fmdev *fmdev)
{
	if (fmdev->irq_info.flag & FM_MAL_EVENT & fmdev->irq_info.mask)
		fmerr("irq: HW MAL int received - do nothing\n");

	
	fm_irq_call_stage(fmdev, FM_RDS_START_IDX);
}

static void fm_irq_handle_rds_start(struct fmdev *fmdev)
{
	if (fmdev->irq_info.flag & FM_RDS_EVENT & fmdev->irq_info.mask) {
		fmdbg("irq: rds threshold reached\n");
		fmdev->irq_info.stage = FM_RDS_SEND_RDS_GETCMD_IDX;
	} else {
		
		fmdev->irq_info.stage = FM_HW_TUNE_OP_ENDED_IDX;
	}

	fm_irq_call(fmdev);
}

static void fm_irq_send_rdsdata_getcmd(struct fmdev *fmdev)
{
	
	if (!fm_send_cmd(fmdev, RDS_DATA_GET, REG_RD, NULL,
			    (FM_RX_RDS_FIFO_THRESHOLD * 3), NULL))
		fm_irq_timeout_stage(fmdev, FM_RDS_HANDLE_RDS_GETCMD_RESP_IDX);
}

static void fm_rx_update_af_cache(struct fmdev *fmdev, u8 af)
{
	struct tuned_station_info *stat_info = &fmdev->rx.stat_info;
	u8 reg_idx = fmdev->rx.region.fm_band;
	u8 index;
	u32 freq;

	
	if ((af >= FM_RDS_1_AF_FOLLOWS) && (af <= FM_RDS_25_AF_FOLLOWS)) {
		fmdev->rx.stat_info.af_list_max = (af - FM_RDS_1_AF_FOLLOWS + 1);
		fmdev->rx.stat_info.afcache_size = 0;
		fmdbg("No of expected AF : %d\n", fmdev->rx.stat_info.af_list_max);
		return;
	}

	if (af < FM_RDS_MIN_AF)
		return;
	if (reg_idx == FM_BAND_EUROPE_US && af > FM_RDS_MAX_AF)
		return;
	if (reg_idx == FM_BAND_JAPAN && af > FM_RDS_MAX_AF_JAPAN)
		return;

	freq = fmdev->rx.region.bot_freq + (af * 100);
	if (freq == fmdev->rx.freq) {
		fmdbg("Current freq(%d) is matching with received AF(%d)\n",
				fmdev->rx.freq, freq);
		return;
	}
	
	for (index = 0; index < stat_info->afcache_size; index++) {
		if (stat_info->af_cache[index] == freq)
			break;
	}
	
	if (index == stat_info->af_list_max) {
		fmdbg("AF cache is full\n");
		return;
	}
	if (index == stat_info->afcache_size) {
		fmdbg("Storing AF %d to cache index %d\n", freq, index);
		stat_info->af_cache[index] = freq;
		stat_info->afcache_size++;
	}
}

static void fm_rdsparse_swapbytes(struct fmdev *fmdev,
		struct fm_rdsdata_format *rds_format)
{
	u8 byte1;
	u8 index = 0;
	u8 *rds_buff;

	if (fmdev->asci_id != 0x6350) {
		rds_buff = &rds_format->data.groupdatabuff.buff[0];
		while (index + 1 < FM_RX_RDS_INFO_FIELD_MAX) {
			byte1 = rds_buff[index];
			rds_buff[index] = rds_buff[index + 1];
			rds_buff[index + 1] = byte1;
			index += 2;
		}
	}
}

static void fm_irq_handle_rdsdata_getcmd_resp(struct fmdev *fmdev)
{
	struct sk_buff *skb;
	struct fm_rdsdata_format rds_fmt;
	struct fm_rds *rds = &fmdev->rx.rds;
	unsigned long group_idx, flags;
	u8 *rds_data, meta_data, tmpbuf[3];
	u8 type, blk_idx;
	u16 cur_picode;
	u32 rds_len;

	if (check_cmdresp_status(fmdev, &skb))
		return;

	
	skb_pull(skb, sizeof(struct fm_event_msg_hdr));
	rds_data = skb->data;
	rds_len = skb->len;

	
	while (rds_len >= FM_RDS_BLK_SIZE) {
		meta_data = rds_data[2];
		
		type = (meta_data & 0x07);

		
		blk_idx = (type <= FM_RDS_BLOCK_C ? type : (type - 1));
		fmdbg("Block index:%d(%s)\n", blk_idx,
			   (meta_data & FM_RDS_STATUS_ERR_MASK) ? "Bad" : "Ok");

		if ((meta_data & FM_RDS_STATUS_ERR_MASK) != 0)
			break;

		if (blk_idx < FM_RDS_BLK_IDX_A || blk_idx > FM_RDS_BLK_IDX_D) {
			fmdbg("Block sequence mismatch\n");
			rds->last_blk_idx = -1;
			break;
		}

		
		memcpy(&rds_fmt.data.groupdatabuff.
				buff[blk_idx * (FM_RDS_BLK_SIZE - 1)],
				rds_data, (FM_RDS_BLK_SIZE - 1));

		rds->last_blk_idx = blk_idx;

		
		if (blk_idx == FM_RDS_BLK_IDX_D) {
			fmdbg("Good block received\n");
			fm_rdsparse_swapbytes(fmdev, &rds_fmt);

			cur_picode = be16_to_cpu(rds_fmt.data.groupgeneral.pidata);
			if (fmdev->rx.stat_info.picode != cur_picode)
				fmdev->rx.stat_info.picode = cur_picode;

			fmdbg("picode:%d\n", cur_picode);

			group_idx = (rds_fmt.data.groupgeneral.blk_b[0] >> 3);
			fmdbg("(fmdrv):Group:%ld%s\n", group_idx/2,
					(group_idx % 2) ? "B" : "A");

			group_idx = 1 << (rds_fmt.data.groupgeneral.blk_b[0] >> 3);
			if (group_idx == FM_RDS_GROUP_TYPE_MASK_0A) {
				fm_rx_update_af_cache(fmdev, rds_fmt.data.group0A.af[0]);
				fm_rx_update_af_cache(fmdev, rds_fmt.data.group0A.af[1]);
			}
		}
		rds_len -= FM_RDS_BLK_SIZE;
		rds_data += FM_RDS_BLK_SIZE;
	}

	
	rds_data = skb->data;
	rds_len = skb->len;

	spin_lock_irqsave(&fmdev->rds_buff_lock, flags);
	while (rds_len > 0) {
		type = (rds_data[2] & 0x07);
		blk_idx = (type <= FM_RDS_BLOCK_C ? type : (type - 1));
		tmpbuf[2] = blk_idx;	
		tmpbuf[2] |= blk_idx << 3;	

		
		tmpbuf[0] = rds_data[0];
		tmpbuf[1] = rds_data[1];

		memcpy(&rds->buff[rds->wr_idx], &tmpbuf, FM_RDS_BLK_SIZE);
		rds->wr_idx = (rds->wr_idx + FM_RDS_BLK_SIZE) % rds->buf_size;

		
		if (rds->wr_idx == rds->rd_idx) {
			fmdbg("RDS buffer overflow\n");
			rds->wr_idx = 0;
			rds->rd_idx = 0;
			break;
		}
		rds_len -= FM_RDS_BLK_SIZE;
		rds_data += FM_RDS_BLK_SIZE;
	}
	spin_unlock_irqrestore(&fmdev->rds_buff_lock, flags);

	
	if (rds->wr_idx != rds->rd_idx)
		wake_up_interruptible(&rds->read_queue);

	fm_irq_call_stage(fmdev, FM_RDS_FINISH_IDX);
}

static void fm_irq_handle_rds_finish(struct fmdev *fmdev)
{
	fm_irq_call_stage(fmdev, FM_HW_TUNE_OP_ENDED_IDX);
}

static void fm_irq_handle_tune_op_ended(struct fmdev *fmdev)
{
	if (fmdev->irq_info.flag & (FM_FR_EVENT | FM_BL_EVENT) & fmdev->
	    irq_info.mask) {
		fmdbg("irq: tune ended/bandlimit reached\n");
		if (test_and_clear_bit(FM_AF_SWITCH_INPROGRESS, &fmdev->flag)) {
			fmdev->irq_info.stage = FM_AF_JUMP_RD_FREQ_IDX;
		} else {
			complete(&fmdev->maintask_comp);
			fmdev->irq_info.stage = FM_HW_POWER_ENB_IDX;
		}
	} else
		fmdev->irq_info.stage = FM_HW_POWER_ENB_IDX;

	fm_irq_call(fmdev);
}

static void fm_irq_handle_power_enb(struct fmdev *fmdev)
{
	if (fmdev->irq_info.flag & FM_POW_ENB_EVENT) {
		fmdbg("irq: Power Enabled/Disabled\n");
		complete(&fmdev->maintask_comp);
	}

	fm_irq_call_stage(fmdev, FM_LOW_RSSI_START_IDX);
}

static void fm_irq_handle_low_rssi_start(struct fmdev *fmdev)
{
	if ((fmdev->rx.af_mode == FM_RX_RDS_AF_SWITCH_MODE_ON) &&
	    (fmdev->irq_info.flag & FM_LEV_EVENT & fmdev->irq_info.mask) &&
	    (fmdev->rx.freq != FM_UNDEFINED_FREQ) &&
	    (fmdev->rx.stat_info.afcache_size != 0)) {
		fmdbg("irq: rssi level has fallen below threshold level\n");

		
		fmdev->irq_info.mask &= ~FM_LEV_EVENT;

		fmdev->rx.afjump_idx = 0;
		fmdev->rx.freq_before_jump = fmdev->rx.freq;
		fmdev->irq_info.stage = FM_AF_JUMP_SETPI_IDX;
	} else {
		
		fmdev->irq_info.stage = FM_SEND_INTMSK_CMD_IDX;
	}

	fm_irq_call(fmdev);
}

static void fm_irq_afjump_set_pi(struct fmdev *fmdev)
{
	u16 payload;

	
	payload = fmdev->rx.stat_info.picode;
	if (!fm_send_cmd(fmdev, RDS_PI_SET, REG_WR, &payload, sizeof(payload), NULL))
		fm_irq_timeout_stage(fmdev, FM_AF_JUMP_HANDLE_SETPI_RESP_IDX);
}

static void fm_irq_handle_set_pi_resp(struct fmdev *fmdev)
{
	fm_irq_common_cmd_resp_helper(fmdev, FM_AF_JUMP_SETPI_MASK_IDX);
}

static void fm_irq_afjump_set_pimask(struct fmdev *fmdev)
{
	u16 payload;

	payload = 0x0000;
	if (!fm_send_cmd(fmdev, RDS_PI_MASK_SET, REG_WR, &payload, sizeof(payload), NULL))
		fm_irq_timeout_stage(fmdev, FM_AF_JUMP_HANDLE_SETPI_MASK_RESP_IDX);
}

static void fm_irq_handle_set_pimask_resp(struct fmdev *fmdev)
{
	fm_irq_common_cmd_resp_helper(fmdev, FM_AF_JUMP_SET_AF_FREQ_IDX);
}

static void fm_irq_afjump_setfreq(struct fmdev *fmdev)
{
	u16 frq_index;
	u16 payload;

	fmdbg("Swtich to %d KHz\n", fmdev->rx.stat_info.af_cache[fmdev->rx.afjump_idx]);
	frq_index = (fmdev->rx.stat_info.af_cache[fmdev->rx.afjump_idx] -
	     fmdev->rx.region.bot_freq) / FM_FREQ_MUL;

	payload = frq_index;
	if (!fm_send_cmd(fmdev, AF_FREQ_SET, REG_WR, &payload, sizeof(payload), NULL))
		fm_irq_timeout_stage(fmdev, FM_AF_JUMP_HANDLE_SET_AFFREQ_RESP_IDX);
}

static void fm_irq_handle_setfreq_resp(struct fmdev *fmdev)
{
	fm_irq_common_cmd_resp_helper(fmdev, FM_AF_JUMP_ENABLE_INT_IDX);
}

static void fm_irq_afjump_enableint(struct fmdev *fmdev)
{
	u16 payload;

	
	payload = FM_FR_EVENT;
	if (!fm_send_cmd(fmdev, INT_MASK_SET, REG_WR, &payload, sizeof(payload), NULL))
		fm_irq_timeout_stage(fmdev, FM_AF_JUMP_ENABLE_INT_RESP_IDX);
}

static void fm_irq_afjump_enableint_resp(struct fmdev *fmdev)
{
	fm_irq_common_cmd_resp_helper(fmdev, FM_AF_JUMP_START_AFJUMP_IDX);
}

static void fm_irq_start_afjump(struct fmdev *fmdev)
{
	u16 payload;

	payload = FM_TUNER_AF_JUMP_MODE;
	if (!fm_send_cmd(fmdev, TUNER_MODE_SET, REG_WR, &payload,
			sizeof(payload), NULL))
		fm_irq_timeout_stage(fmdev, FM_AF_JUMP_HANDLE_START_AFJUMP_RESP_IDX);
}

static void fm_irq_handle_start_afjump_resp(struct fmdev *fmdev)
{
	struct sk_buff *skb;

	if (check_cmdresp_status(fmdev, &skb))
		return;

	fmdev->irq_info.stage = FM_SEND_FLAG_GETCMD_IDX;
	set_bit(FM_AF_SWITCH_INPROGRESS, &fmdev->flag);
	clear_bit(FM_INTTASK_RUNNING, &fmdev->flag);
}

static void fm_irq_afjump_rd_freq(struct fmdev *fmdev)
{
	u16 payload;

	if (!fm_send_cmd(fmdev, FREQ_SET, REG_RD, NULL, sizeof(payload), NULL))
		fm_irq_timeout_stage(fmdev, FM_AF_JUMP_RD_FREQ_RESP_IDX);
}

static void fm_irq_afjump_rd_freq_resp(struct fmdev *fmdev)
{
	struct sk_buff *skb;
	u16 read_freq;
	u32 curr_freq, jumped_freq;

	if (check_cmdresp_status(fmdev, &skb))
		return;

	
	skb_pull(skb, sizeof(struct fm_event_msg_hdr));
	memcpy(&read_freq, skb->data, sizeof(read_freq));
	read_freq = be16_to_cpu(read_freq);
	curr_freq = fmdev->rx.region.bot_freq + ((u32)read_freq * FM_FREQ_MUL);

	jumped_freq = fmdev->rx.stat_info.af_cache[fmdev->rx.afjump_idx];

	
	if ((curr_freq != fmdev->rx.freq_before_jump) && (curr_freq == jumped_freq)) {
		fmdbg("Successfully switched to alternate freq %d\n", curr_freq);
		fmdev->rx.freq = curr_freq;
		fm_rx_reset_rds_cache(fmdev);

		
		if (fmdev->rx.af_mode == FM_RX_RDS_AF_SWITCH_MODE_ON)
			fmdev->irq_info.mask |= FM_LEV_EVENT;

		fmdev->irq_info.stage = FM_LOW_RSSI_FINISH_IDX;
	} else {		
		fmdev->rx.afjump_idx++;

		
		if (fmdev->rx.afjump_idx >= fmdev->rx.stat_info.afcache_size) {
			fmdbg("AF switch processing failed\n");
			fmdev->irq_info.stage = FM_LOW_RSSI_FINISH_IDX;
		} else {	

			fmdbg("Trying next freq in AF cache\n");
			fmdev->irq_info.stage = FM_AF_JUMP_SETPI_IDX;
		}
	}
	fm_irq_call(fmdev);
}

static void fm_irq_handle_low_rssi_finish(struct fmdev *fmdev)
{
	fm_irq_call_stage(fmdev, FM_SEND_INTMSK_CMD_IDX);
}

static void fm_irq_send_intmsk_cmd(struct fmdev *fmdev)
{
	u16 payload;

	
	payload = fmdev->irq_info.mask;

	if (!fm_send_cmd(fmdev, INT_MASK_SET, REG_WR, &payload,
			sizeof(payload), NULL))
		fm_irq_timeout_stage(fmdev, FM_HANDLE_INTMSK_CMD_RESP_IDX);
}

static void fm_irq_handle_intmsk_cmd_resp(struct fmdev *fmdev)
{
	struct sk_buff *skb;

	if (check_cmdresp_status(fmdev, &skb))
		return;
	fmdev->irq_info.stage = FM_SEND_FLAG_GETCMD_IDX;

	
	if (test_and_clear_bit(FM_INTTASK_SCHEDULE_PENDING, &fmdev->flag))
		fmdev->irq_info.handlers[fmdev->irq_info.stage](fmdev);
	else
		clear_bit(FM_INTTASK_RUNNING, &fmdev->flag);
}

int fmc_is_rds_data_available(struct fmdev *fmdev, struct file *file,
				struct poll_table_struct *pts)
{
	poll_wait(file, &fmdev->rx.rds.read_queue, pts);
	if (fmdev->rx.rds.rd_idx != fmdev->rx.rds.wr_idx)
		return 0;

	return -EAGAIN;
}

int fmc_transfer_rds_from_internal_buff(struct fmdev *fmdev, struct file *file,
		u8 __user *buf, size_t count)
{
	u32 block_count;
	unsigned long flags;
	int ret;

	if (fmdev->rx.rds.wr_idx == fmdev->rx.rds.rd_idx) {
		if (file->f_flags & O_NONBLOCK)
			return -EWOULDBLOCK;

		ret = wait_event_interruptible(fmdev->rx.rds.read_queue,
				(fmdev->rx.rds.wr_idx != fmdev->rx.rds.rd_idx));
		if (ret)
			return -EINTR;
	}

	
	count /= 3;
	block_count = 0;
	ret = 0;

	spin_lock_irqsave(&fmdev->rds_buff_lock, flags);

	while (block_count < count) {
		if (fmdev->rx.rds.wr_idx == fmdev->rx.rds.rd_idx)
			break;

		if (copy_to_user(buf, &fmdev->rx.rds.buff[fmdev->rx.rds.rd_idx],
					FM_RDS_BLK_SIZE))
			break;

		fmdev->rx.rds.rd_idx += FM_RDS_BLK_SIZE;
		if (fmdev->rx.rds.rd_idx >= fmdev->rx.rds.buf_size)
			fmdev->rx.rds.rd_idx = 0;

		block_count++;
		buf += FM_RDS_BLK_SIZE;
		ret += FM_RDS_BLK_SIZE;
	}
	spin_unlock_irqrestore(&fmdev->rds_buff_lock, flags);
	return ret;
}

int fmc_set_freq(struct fmdev *fmdev, u32 freq_to_set)
{
	switch (fmdev->curr_fmmode) {
	case FM_MODE_RX:
		return fm_rx_set_freq(fmdev, freq_to_set);

	case FM_MODE_TX:
		return fm_tx_set_freq(fmdev, freq_to_set);

	default:
		return -EINVAL;
	}
}

int fmc_get_freq(struct fmdev *fmdev, u32 *cur_tuned_frq)
{
	if (fmdev->rx.freq == FM_UNDEFINED_FREQ) {
		fmerr("RX frequency is not set\n");
		return -EPERM;
	}
	if (cur_tuned_frq == NULL) {
		fmerr("Invalid memory\n");
		return -ENOMEM;
	}

	switch (fmdev->curr_fmmode) {
	case FM_MODE_RX:
		*cur_tuned_frq = fmdev->rx.freq;
		return 0;

	case FM_MODE_TX:
		*cur_tuned_frq = 0;	
		return 0;

	default:
		return -EINVAL;
	}

}

int fmc_set_region(struct fmdev *fmdev, u8 region_to_set)
{
	switch (fmdev->curr_fmmode) {
	case FM_MODE_RX:
		return fm_rx_set_region(fmdev, region_to_set);

	case FM_MODE_TX:
		return fm_tx_set_region(fmdev, region_to_set);

	default:
		return -EINVAL;
	}
}

int fmc_set_mute_mode(struct fmdev *fmdev, u8 mute_mode_toset)
{
	switch (fmdev->curr_fmmode) {
	case FM_MODE_RX:
		return fm_rx_set_mute_mode(fmdev, mute_mode_toset);

	case FM_MODE_TX:
		return fm_tx_set_mute_mode(fmdev, mute_mode_toset);

	default:
		return -EINVAL;
	}
}

int fmc_set_stereo_mono(struct fmdev *fmdev, u16 mode)
{
	switch (fmdev->curr_fmmode) {
	case FM_MODE_RX:
		return fm_rx_set_stereo_mono(fmdev, mode);

	case FM_MODE_TX:
		return fm_tx_set_stereo_mono(fmdev, mode);

	default:
		return -EINVAL;
	}
}

int fmc_set_rds_mode(struct fmdev *fmdev, u8 rds_en_dis)
{
	switch (fmdev->curr_fmmode) {
	case FM_MODE_RX:
		return fm_rx_set_rds_mode(fmdev, rds_en_dis);

	case FM_MODE_TX:
		return fm_tx_set_rds_mode(fmdev, rds_en_dis);

	default:
		return -EINVAL;
	}
}

static int fm_power_down(struct fmdev *fmdev)
{
	u16 payload;
	int ret;

	if (!test_bit(FM_CORE_READY, &fmdev->flag)) {
		fmerr("FM core is not ready\n");
		return -EPERM;
	}
	if (fmdev->curr_fmmode == FM_MODE_OFF) {
		fmdbg("FM chip is already in OFF state\n");
		return 0;
	}

	payload = 0x0;
	ret = fmc_send_cmd(fmdev, FM_POWER_MODE, REG_WR, &payload,
		sizeof(payload), NULL, NULL);
	if (ret < 0)
		return ret;

	return fmc_release(fmdev);
}

static int fm_download_firmware(struct fmdev *fmdev, const u8 *fw_name)
{
	const struct firmware *fw_entry;
	struct bts_header *fw_header;
	struct bts_action *action;
	struct bts_action_delay *delay;
	u8 *fw_data;
	int ret, fw_len, cmd_cnt;

	cmd_cnt = 0;
	set_bit(FM_FW_DW_INPROGRESS, &fmdev->flag);

	ret = request_firmware(&fw_entry, fw_name,
				&fmdev->radio_dev->dev);
	if (ret < 0) {
		fmerr("Unable to read firmware(%s) content\n", fw_name);
		return ret;
	}
	fmdbg("Firmware(%s) length : %d bytes\n", fw_name, fw_entry->size);

	fw_data = (void *)fw_entry->data;
	fw_len = fw_entry->size;

	fw_header = (struct bts_header *)fw_data;
	if (fw_header->magic != FM_FW_FILE_HEADER_MAGIC) {
		fmerr("%s not a legal TI firmware file\n", fw_name);
		ret = -EINVAL;
		goto rel_fw;
	}
	fmdbg("FW(%s) magic number : 0x%x\n", fw_name, fw_header->magic);

	
	fw_data += sizeof(struct bts_header);
	fw_len -= sizeof(struct bts_header);

	while (fw_data && fw_len > 0) {
		action = (struct bts_action *)fw_data;

		switch (action->type) {
		case ACTION_SEND_COMMAND:	
			if (fmc_send_cmd(fmdev, 0, 0, action->data,
						action->size, NULL, NULL))
				goto rel_fw;

			cmd_cnt++;
			break;

		case ACTION_DELAY:	
			delay = (struct bts_action_delay *)action->data;
			mdelay(delay->msec);
			break;
		}

		fw_data += (sizeof(struct bts_action) + (action->size));
		fw_len -= (sizeof(struct bts_action) + (action->size));
	}
	fmdbg("Firmware commands(%d) loaded to chip\n", cmd_cnt);
rel_fw:
	release_firmware(fw_entry);
	clear_bit(FM_FW_DW_INPROGRESS, &fmdev->flag);

	return ret;
}

static int load_default_rx_configuration(struct fmdev *fmdev)
{
	int ret;

	ret = fm_rx_set_volume(fmdev, FM_DEFAULT_RX_VOLUME);
	if (ret < 0)
		return ret;

	return fm_rx_set_rssi_threshold(fmdev, FM_DEFAULT_RSSI_THRESHOLD);
}

static int fm_power_up(struct fmdev *fmdev, u8 mode)
{
	u16 payload, asic_id, asic_ver;
	int resp_len, ret;
	u8 fw_name[50];

	if (mode >= FM_MODE_ENTRY_MAX) {
		fmerr("Invalid firmware download option\n");
		return -EINVAL;
	}

	ret = fmc_prepare(fmdev);
	if (ret < 0) {
		fmerr("Unable to prepare FM Common\n");
		return ret;
	}

	payload = FM_ENABLE;
	if (fmc_send_cmd(fmdev, FM_POWER_MODE, REG_WR, &payload,
			sizeof(payload), NULL, NULL))
		goto rel;

	
	msleep(20);

	if (fmc_send_cmd(fmdev, ASIC_ID_GET, REG_RD, NULL,
			sizeof(asic_id), &asic_id, &resp_len))
		goto rel;

	if (fmc_send_cmd(fmdev, ASIC_VER_GET, REG_RD, NULL,
			sizeof(asic_ver), &asic_ver, &resp_len))
		goto rel;

	fmdbg("ASIC ID: 0x%x , ASIC Version: %d\n",
		be16_to_cpu(asic_id), be16_to_cpu(asic_ver));

	sprintf(fw_name, "%s_%x.%d.bts", FM_FMC_FW_FILE_START,
		be16_to_cpu(asic_id), be16_to_cpu(asic_ver));

	ret = fm_download_firmware(fmdev, fw_name);
	if (ret < 0) {
		fmdbg("Failed to download firmware file %s\n", fw_name);
		goto rel;
	}
	sprintf(fw_name, "%s_%x.%d.bts", (mode == FM_MODE_RX) ?
			FM_RX_FW_FILE_START : FM_TX_FW_FILE_START,
			be16_to_cpu(asic_id), be16_to_cpu(asic_ver));

	ret = fm_download_firmware(fmdev, fw_name);
	if (ret < 0) {
		fmdbg("Failed to download firmware file %s\n", fw_name);
		goto rel;
	} else
		return ret;
rel:
	return fmc_release(fmdev);
}

int fmc_set_mode(struct fmdev *fmdev, u8 fm_mode)
{
	int ret = 0;

	if (fm_mode >= FM_MODE_ENTRY_MAX) {
		fmerr("Invalid FM mode\n");
		return -EINVAL;
	}
	if (fmdev->curr_fmmode == fm_mode) {
		fmdbg("Already fm is in mode(%d)\n", fm_mode);
		return ret;
	}

	switch (fm_mode) {
	case FM_MODE_OFF:	
		ret = fm_power_down(fmdev);
		if (ret < 0) {
			fmerr("Failed to set OFF mode\n");
			return ret;
		}
		break;

	case FM_MODE_TX:	
	case FM_MODE_RX:	
		
		if (fmdev->curr_fmmode != FM_MODE_OFF) {
			ret = fm_power_down(fmdev);
			if (ret < 0) {
				fmerr("Failed to set OFF mode\n");
				return ret;
			}
			msleep(30);
		}
		ret = fm_power_up(fmdev, fm_mode);
		if (ret < 0) {
			fmerr("Failed to load firmware\n");
			return ret;
		}
	}
	fmdev->curr_fmmode = fm_mode;

	
	if (fmdev->curr_fmmode == FM_MODE_RX) {
		fmdbg("Loading default rx configuration..\n");
		ret = load_default_rx_configuration(fmdev);
		if (ret < 0)
			fmerr("Failed to load default values\n");
	}

	return ret;
}

int fmc_get_mode(struct fmdev *fmdev, u8 *fmmode)
{
	if (!test_bit(FM_CORE_READY, &fmdev->flag)) {
		fmerr("FM core is not ready\n");
		return -EPERM;
	}
	if (fmmode == NULL) {
		fmerr("Invalid memory\n");
		return -ENOMEM;
	}

	*fmmode = fmdev->curr_fmmode;
	return 0;
}

static long fm_st_receive(void *arg, struct sk_buff *skb)
{
	struct fmdev *fmdev;

	fmdev = (struct fmdev *)arg;

	if (skb == NULL) {
		fmerr("Invalid SKB received from ST\n");
		return -EFAULT;
	}

	if (skb->cb[0] != FM_PKT_LOGICAL_CHAN_NUMBER) {
		fmerr("Received SKB (%p) is not FM Channel 8 pkt\n", skb);
		return -EINVAL;
	}

	memcpy(skb_push(skb, 1), &skb->cb[0], 1);
	skb_queue_tail(&fmdev->rx_q, skb);
	tasklet_schedule(&fmdev->rx_task);

	return 0;
}

static void fm_st_reg_comp_cb(void *arg, char data)
{
	struct fmdev *fmdev;

	fmdev = (struct fmdev *)arg;
	fmdev->streg_cbdata = data;
	complete(&wait_for_fmdrv_reg_comp);
}

int fmc_prepare(struct fmdev *fmdev)
{
	static struct st_proto_s fm_st_proto;
	int ret;

	if (test_bit(FM_CORE_READY, &fmdev->flag)) {
		fmdbg("FM Core is already up\n");
		return 0;
	}

	memset(&fm_st_proto, 0, sizeof(fm_st_proto));
	fm_st_proto.recv = fm_st_receive;
	fm_st_proto.match_packet = NULL;
	fm_st_proto.reg_complete_cb = fm_st_reg_comp_cb;
	fm_st_proto.write = NULL; 
	fm_st_proto.priv_data = fmdev;
	fm_st_proto.chnl_id = 0x08;
	fm_st_proto.max_frame_size = 0xff;
	fm_st_proto.hdr_len = 1;
	fm_st_proto.offset_len_in_hdr = 0;
	fm_st_proto.len_size = 1;
	fm_st_proto.reserve = 1;

	ret = st_register(&fm_st_proto);
	if (ret == -EINPROGRESS) {
		init_completion(&wait_for_fmdrv_reg_comp);
		fmdev->streg_cbdata = -EINPROGRESS;
		fmdbg("%s waiting for ST reg completion signal\n", __func__);

		if (!wait_for_completion_timeout(&wait_for_fmdrv_reg_comp,
						 FM_ST_REG_TIMEOUT)) {
			fmerr("Timeout(%d sec), didn't get reg "
					"completion signal from ST\n",
					jiffies_to_msecs(FM_ST_REG_TIMEOUT) / 1000);
			return -ETIMEDOUT;
		}
		if (fmdev->streg_cbdata != 0) {
			fmerr("ST reg comp CB called with error "
					"status %d\n", fmdev->streg_cbdata);
			return -EAGAIN;
		}

		ret = 0;
	} else if (ret == -1) {
		fmerr("st_register failed %d\n", ret);
		return -EAGAIN;
	}

	if (fm_st_proto.write != NULL) {
		g_st_write = fm_st_proto.write;
	} else {
		fmerr("Failed to get ST write func pointer\n");
		ret = st_unregister(&fm_st_proto);
		if (ret < 0)
			fmerr("st_unregister failed %d\n", ret);
		return -EAGAIN;
	}

	spin_lock_init(&fmdev->rds_buff_lock);
	spin_lock_init(&fmdev->resp_skb_lock);

	
	skb_queue_head_init(&fmdev->tx_q);
	tasklet_init(&fmdev->tx_task, send_tasklet, (unsigned long)fmdev);

	
	skb_queue_head_init(&fmdev->rx_q);
	tasklet_init(&fmdev->rx_task, recv_tasklet, (unsigned long)fmdev);

	fmdev->irq_info.stage = 0;
	atomic_set(&fmdev->tx_cnt, 1);
	fmdev->resp_comp = NULL;

	init_timer(&fmdev->irq_info.timer);
	fmdev->irq_info.timer.function = &int_timeout_handler;
	fmdev->irq_info.timer.data = (unsigned long)fmdev;
	
	fmdev->irq_info.mask = FM_MAL_EVENT;

	
	memcpy(&fmdev->rx.region, &region_configs[default_radio_region],
			sizeof(struct region_info));

	fmdev->rx.mute_mode = FM_MUTE_OFF;
	fmdev->rx.rf_depend_mute = FM_RX_RF_DEPENDENT_MUTE_OFF;
	fmdev->rx.rds.flag = FM_RDS_DISABLE;
	fmdev->rx.freq = FM_UNDEFINED_FREQ;
	fmdev->rx.rds_mode = FM_RDS_SYSTEM_RDS;
	fmdev->rx.af_mode = FM_RX_RDS_AF_SWITCH_MODE_OFF;
	fmdev->irq_info.retry = 0;

	fm_rx_reset_rds_cache(fmdev);
	init_waitqueue_head(&fmdev->rx.rds.read_queue);

	fm_rx_reset_station_info(fmdev);
	set_bit(FM_CORE_READY, &fmdev->flag);

	return ret;
}

int fmc_release(struct fmdev *fmdev)
{
	static struct st_proto_s fm_st_proto;
	int ret;

	if (!test_bit(FM_CORE_READY, &fmdev->flag)) {
		fmdbg("FM Core is already down\n");
		return 0;
	}
	
	wake_up_interruptible(&fmdev->rx.rds.read_queue);

	tasklet_kill(&fmdev->tx_task);
	tasklet_kill(&fmdev->rx_task);

	skb_queue_purge(&fmdev->tx_q);
	skb_queue_purge(&fmdev->rx_q);

	fmdev->resp_comp = NULL;
	fmdev->rx.freq = 0;

	memset(&fm_st_proto, 0, sizeof(fm_st_proto));
	fm_st_proto.chnl_id = 0x08;

	ret = st_unregister(&fm_st_proto);

	if (ret < 0)
		fmerr("Failed to de-register FM from ST %d\n", ret);
	else
		fmdbg("Successfully unregistered from ST\n");

	clear_bit(FM_CORE_READY, &fmdev->flag);
	return ret;
}

static int __init fm_drv_init(void)
{
	struct fmdev *fmdev = NULL;
	int ret = -ENOMEM;

	fmdbg("FM driver version %s\n", FM_DRV_VERSION);

	fmdev = kzalloc(sizeof(struct fmdev), GFP_KERNEL);
	if (NULL == fmdev) {
		fmerr("Can't allocate operation structure memory\n");
		return ret;
	}
	fmdev->rx.rds.buf_size = default_rds_buf * FM_RDS_BLK_SIZE;
	fmdev->rx.rds.buff = kzalloc(fmdev->rx.rds.buf_size, GFP_KERNEL);
	if (NULL == fmdev->rx.rds.buff) {
		fmerr("Can't allocate rds ring buffer\n");
		goto rel_dev;
	}

	ret = fm_v4l2_init_video_device(fmdev, radio_nr);
	if (ret < 0)
		goto rel_rdsbuf;

	fmdev->irq_info.handlers = int_handler_table;
	fmdev->curr_fmmode = FM_MODE_OFF;
	fmdev->tx_data.pwr_lvl = FM_PWR_LVL_DEF;
	fmdev->tx_data.preemph = FM_TX_PREEMPH_50US;
	return ret;

rel_rdsbuf:
	kfree(fmdev->rx.rds.buff);
rel_dev:
	kfree(fmdev);

	return ret;
}

static void __exit fm_drv_exit(void)
{
	struct fmdev *fmdev = NULL;

	fmdev = fm_v4l2_deinit_video_device();
	if (fmdev != NULL) {
		kfree(fmdev->rx.rds.buff);
		kfree(fmdev);
	}
}

module_init(fm_drv_init);
module_exit(fm_drv_exit);

MODULE_AUTHOR("Manjunatha Halli <manjunatha_halli@ti.com>");
MODULE_DESCRIPTION("FM Driver for TI's Connectivity chip. " FM_DRV_VERSION);
MODULE_VERSION(FM_DRV_VERSION);
MODULE_LICENSE("GPL");
