/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2007 - 2012 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110,
 * USA
 *
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.GPL.
 *
 * Contact Information:
 *  Intel Linux Wireless <ilw@linux.intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 * BSD LICENSE
 *
 * Copyright(c) 2005 - 2012 Intel Corporation. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
#ifndef __iwl_trans_h__
#define __iwl_trans_h__

#include <linux/ieee80211.h>
#include <linux/mm.h> 

#include "iwl-shared.h"
#include "iwl-debug.h"



struct iwl_priv;
struct iwl_shared;
struct iwl_op_mode;
struct fw_img;
struct sk_buff;
struct dentry;

#define SEQ_TO_SN(seq) (((seq) & IEEE80211_SCTL_SEQ) >> 4)
#define SN_TO_SEQ(ssn) (((ssn) << 4) & IEEE80211_SCTL_SEQ)
#define MAX_SN ((IEEE80211_SCTL_SEQ) >> 4)
#define SEQ_TO_QUEUE(s)	(((s) >> 8) & 0x1f)
#define QUEUE_TO_SEQ(q)	(((q) & 0x1f) << 8)
#define SEQ_TO_INDEX(s)	((s) & 0xff)
#define INDEX_TO_SEQ(i)	((i) & 0xff)
#define SEQ_RX_FRAME	cpu_to_le16(0x8000)

struct iwl_cmd_header {
	u8 cmd;		
	u8 flags;	
	__le16 sequence;
} __packed;


#define FH_RSCSR_FRAME_SIZE_MSK		0x00003FFF	

struct iwl_rx_packet {
	__le32 len_n_flags;
	struct iwl_cmd_header hdr;
	u8 data[];
} __packed;

enum CMD_MODE {
	CMD_SYNC = 0,
	CMD_ASYNC = BIT(0),
	CMD_WANT_SKB = BIT(1),
	CMD_ON_DEMAND = BIT(2),
};

#define DEF_CMD_PAYLOAD_SIZE 320

struct iwl_device_cmd {
	struct iwl_cmd_header hdr;	
	u8 payload[DEF_CMD_PAYLOAD_SIZE];
} __packed;

#define TFD_MAX_PAYLOAD_SIZE (sizeof(struct iwl_device_cmd))

#define IWL_MAX_CMD_TFDS	2

enum iwl_hcmd_dataflag {
	IWL_HCMD_DFL_NOCOPY	= BIT(0),
};

struct iwl_host_cmd {
	const void *data[IWL_MAX_CMD_TFDS];
	struct iwl_rx_packet *resp_pkt;
	unsigned long _rx_page_addr;
	u32 _rx_page_order;
	int handler_status;

	u32 flags;
	u16 len[IWL_MAX_CMD_TFDS];
	u8 dataflags[IWL_MAX_CMD_TFDS];
	u8 id;
};

static inline void iwl_free_resp(struct iwl_host_cmd *cmd)
{
	free_pages(cmd->_rx_page_addr, cmd->_rx_page_order);
}

struct iwl_rx_cmd_buffer {
	struct page *_page;
	unsigned int truesize;
};

static inline void *rxb_addr(struct iwl_rx_cmd_buffer *r)
{
	return page_address(r->_page);
}

static inline struct page *rxb_steal_page(struct iwl_rx_cmd_buffer *r)
{
	struct page *p = r->_page;
	r->_page = NULL;
	return p;
}

#define MAX_NO_RECLAIM_CMDS	6

struct iwl_trans_config {
	struct iwl_op_mode *op_mode;
	u8 cmd_queue;
	const u8 *no_reclaim_cmds;
	int n_no_reclaim_cmds;
};

struct iwl_trans_ops {

	int (*start_hw)(struct iwl_trans *iwl_trans);
	void (*stop_hw)(struct iwl_trans *iwl_trans);
	int (*start_fw)(struct iwl_trans *trans, const struct fw_img *fw);
	void (*fw_alive)(struct iwl_trans *trans);
	void (*stop_device)(struct iwl_trans *trans);

	void (*wowlan_suspend)(struct iwl_trans *trans);

	int (*send_cmd)(struct iwl_trans *trans, struct iwl_host_cmd *cmd);

	int (*tx)(struct iwl_trans *trans, struct sk_buff *skb,
		struct iwl_device_cmd *dev_cmd, enum iwl_rxon_context_id ctx,
		u8 sta_id, u8 tid);
	int (*reclaim)(struct iwl_trans *trans, int sta_id, int tid,
			int txq_id, int ssn, struct sk_buff_head *skbs);

	int (*tx_agg_disable)(struct iwl_trans *trans,
			      int sta_id, int tid);
	int (*tx_agg_alloc)(struct iwl_trans *trans,
			    int sta_id, int tid);
	void (*tx_agg_setup)(struct iwl_trans *trans,
			     enum iwl_rxon_context_id ctx, int sta_id, int tid,
			     int frame_limit, u16 ssn);

	void (*free)(struct iwl_trans *trans);

	int (*dbgfs_register)(struct iwl_trans *trans, struct dentry* dir);
	int (*check_stuck_queue)(struct iwl_trans *trans, int q);
	int (*wait_tx_queue_empty)(struct iwl_trans *trans);
#ifdef CONFIG_PM_SLEEP
	int (*suspend)(struct iwl_trans *trans);
	int (*resume)(struct iwl_trans *trans);
#endif
	void (*write8)(struct iwl_trans *trans, u32 ofs, u8 val);
	void (*write32)(struct iwl_trans *trans, u32 ofs, u32 val);
	u32 (*read32)(struct iwl_trans *trans, u32 ofs);
	void (*configure)(struct iwl_trans *trans,
			  const struct iwl_trans_config *trans_cfg);
};

enum iwl_trans_state {
	IWL_TRANS_NO_FW = 0,
	IWL_TRANS_FW_ALIVE	= 1,
};

struct iwl_trans {
	const struct iwl_trans_ops *ops;
	struct iwl_op_mode *op_mode;
	struct iwl_shared *shrd;
	enum iwl_trans_state state;
	spinlock_t reg_lock;

	struct device *dev;
	u32 hw_rev;
	u32 hw_id;
	char hw_id_str[52];

	int    nvm_device_type;
	bool pm_support;

	wait_queue_head_t wait_command_queue;

	
	
	char trans_specific[0] __aligned(sizeof(void *));
};

static inline void iwl_trans_configure(struct iwl_trans *trans,
				       const struct iwl_trans_config *trans_cfg)
{
	trans->op_mode = trans_cfg->op_mode;

	trans->ops->configure(trans, trans_cfg);
}

static inline int iwl_trans_start_hw(struct iwl_trans *trans)
{
	might_sleep();

	return trans->ops->start_hw(trans);
}

static inline void iwl_trans_stop_hw(struct iwl_trans *trans)
{
	might_sleep();

	trans->ops->stop_hw(trans);

	trans->state = IWL_TRANS_NO_FW;
}

static inline void iwl_trans_fw_alive(struct iwl_trans *trans)
{
	might_sleep();

	trans->ops->fw_alive(trans);

	trans->state = IWL_TRANS_FW_ALIVE;
}

static inline int iwl_trans_start_fw(struct iwl_trans *trans,
				     const struct fw_img *fw)
{
	might_sleep();

	return trans->ops->start_fw(trans, fw);
}

static inline void iwl_trans_stop_device(struct iwl_trans *trans)
{
	might_sleep();

	trans->ops->stop_device(trans);

	trans->state = IWL_TRANS_NO_FW;
}

static inline void iwl_trans_wowlan_suspend(struct iwl_trans *trans)
{
	might_sleep();
	trans->ops->wowlan_suspend(trans);
}

static inline int iwl_trans_send_cmd(struct iwl_trans *trans,
				struct iwl_host_cmd *cmd)
{
	WARN_ONCE(trans->state != IWL_TRANS_FW_ALIVE,
		  "%s bad state = %d", __func__, trans->state);

	return trans->ops->send_cmd(trans, cmd);
}

static inline int iwl_trans_tx(struct iwl_trans *trans, struct sk_buff *skb,
		struct iwl_device_cmd *dev_cmd, enum iwl_rxon_context_id ctx,
		u8 sta_id, u8 tid)
{
	if (trans->state != IWL_TRANS_FW_ALIVE)
		IWL_ERR(trans, "%s bad state = %d", __func__, trans->state);

	return trans->ops->tx(trans, skb, dev_cmd, ctx, sta_id, tid);
}

static inline int iwl_trans_reclaim(struct iwl_trans *trans, int sta_id,
				 int tid, int txq_id, int ssn,
				 struct sk_buff_head *skbs)
{
	WARN_ONCE(trans->state != IWL_TRANS_FW_ALIVE,
		  "%s bad state = %d", __func__, trans->state);

	return trans->ops->reclaim(trans, sta_id, tid, txq_id, ssn, skbs);
}

static inline int iwl_trans_tx_agg_disable(struct iwl_trans *trans,
					    int sta_id, int tid)
{
	WARN_ONCE(trans->state != IWL_TRANS_FW_ALIVE,
		  "%s bad state = %d", __func__, trans->state);

	return trans->ops->tx_agg_disable(trans, sta_id, tid);
}

static inline int iwl_trans_tx_agg_alloc(struct iwl_trans *trans,
					 int sta_id, int tid)
{
	WARN_ONCE(trans->state != IWL_TRANS_FW_ALIVE,
		  "%s bad state = %d", __func__, trans->state);

	return trans->ops->tx_agg_alloc(trans, sta_id, tid);
}


static inline void iwl_trans_tx_agg_setup(struct iwl_trans *trans,
					   enum iwl_rxon_context_id ctx,
					   int sta_id, int tid,
					   int frame_limit, u16 ssn)
{
	might_sleep();

	WARN_ONCE(trans->state != IWL_TRANS_FW_ALIVE,
		  "%s bad state = %d", __func__, trans->state);

	trans->ops->tx_agg_setup(trans, ctx, sta_id, tid, frame_limit, ssn);
}

static inline void iwl_trans_free(struct iwl_trans *trans)
{
	trans->ops->free(trans);
}

static inline int iwl_trans_wait_tx_queue_empty(struct iwl_trans *trans)
{
	WARN_ONCE(trans->state != IWL_TRANS_FW_ALIVE,
		  "%s bad state = %d", __func__, trans->state);

	return trans->ops->wait_tx_queue_empty(trans);
}

static inline int iwl_trans_check_stuck_queue(struct iwl_trans *trans, int q)
{
	WARN_ONCE(trans->state != IWL_TRANS_FW_ALIVE,
		  "%s bad state = %d", __func__, trans->state);

	return trans->ops->check_stuck_queue(trans, q);
}
static inline int iwl_trans_dbgfs_register(struct iwl_trans *trans,
					    struct dentry *dir)
{
	return trans->ops->dbgfs_register(trans, dir);
}

#ifdef CONFIG_PM_SLEEP
static inline int iwl_trans_suspend(struct iwl_trans *trans)
{
	return trans->ops->suspend(trans);
}

static inline int iwl_trans_resume(struct iwl_trans *trans)
{
	return trans->ops->resume(trans);
}
#endif

static inline void iwl_trans_write8(struct iwl_trans *trans, u32 ofs, u8 val)
{
	trans->ops->write8(trans, ofs, val);
}

static inline void iwl_trans_write32(struct iwl_trans *trans, u32 ofs, u32 val)
{
	trans->ops->write32(trans, ofs, val);
}

static inline u32 iwl_trans_read32(struct iwl_trans *trans, u32 ofs)
{
	return trans->ops->read32(trans, ofs);
}

struct pci_dev;
struct pci_device_id;
extern const struct iwl_trans_ops trans_ops_pcie;
struct iwl_trans *iwl_trans_pcie_alloc(struct iwl_shared *shrd,
				       struct pci_dev *pdev,
				       const struct pci_device_id *ent);
int __must_check iwl_pci_register_driver(void);
void iwl_pci_unregister_driver(void);

extern const struct iwl_trans_ops trans_ops_idi;
struct iwl_trans *iwl_trans_idi_alloc(struct iwl_shared *shrd,
				      void *pdev_void,
				      const void *ent_void);
#endif 
