/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2010 - 2012 Intel Corporation. All rights reserved.
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
 * Copyright(c) 2010 - 2012 Intel Corporation. All rights reserved.
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
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <net/net_namespace.h>
#include <linux/netdevice.h>
#include <net/cfg80211.h>
#include <net/mac80211.h>
#include <net/netlink.h>

#include "iwl-dev.h"
#include "iwl-core.h"
#include "iwl-debug.h"
#include "iwl-io.h"
#include "iwl-agn.h"
#include "iwl-testmode.h"
#include "iwl-trans.h"
#include "iwl-fh.h"
#include "iwl-prph.h"


#define IWL_TM_ABS_PRPH_START (0xA00000)

static
struct nla_policy iwl_testmode_gnl_msg_policy[IWL_TM_ATTR_MAX] = {
	[IWL_TM_ATTR_COMMAND] = { .type = NLA_U32, },

	[IWL_TM_ATTR_UCODE_CMD_ID] = { .type = NLA_U8, },
	[IWL_TM_ATTR_UCODE_CMD_DATA] = { .type = NLA_UNSPEC, },

	[IWL_TM_ATTR_REG_OFFSET] = { .type = NLA_U32, },
	[IWL_TM_ATTR_REG_VALUE8] = { .type = NLA_U8, },
	[IWL_TM_ATTR_REG_VALUE32] = { .type = NLA_U32, },

	[IWL_TM_ATTR_SYNC_RSP] = { .type = NLA_UNSPEC, },
	[IWL_TM_ATTR_UCODE_RX_PKT] = { .type = NLA_UNSPEC, },

	[IWL_TM_ATTR_EEPROM] = { .type = NLA_UNSPEC, },

	[IWL_TM_ATTR_TRACE_ADDR] = { .type = NLA_UNSPEC, },
	[IWL_TM_ATTR_TRACE_DUMP] = { .type = NLA_UNSPEC, },
	[IWL_TM_ATTR_TRACE_SIZE] = { .type = NLA_U32, },

	[IWL_TM_ATTR_FIXRATE] = { .type = NLA_U32, },

	[IWL_TM_ATTR_UCODE_OWNER] = { .type = NLA_U8, },

	[IWL_TM_ATTR_MEM_ADDR] = { .type = NLA_U32, },
	[IWL_TM_ATTR_BUFFER_SIZE] = { .type = NLA_U32, },
	[IWL_TM_ATTR_BUFFER_DUMP] = { .type = NLA_UNSPEC, },

	[IWL_TM_ATTR_FW_VERSION] = { .type = NLA_U32, },
	[IWL_TM_ATTR_DEVICE_ID] = { .type = NLA_U32, },
	[IWL_TM_ATTR_FW_TYPE] = { .type = NLA_U32, },
	[IWL_TM_ATTR_FW_INST_SIZE] = { .type = NLA_U32, },
	[IWL_TM_ATTR_FW_DATA_SIZE] = { .type = NLA_U32, },

	[IWL_TM_ATTR_ENABLE_NOTIFICATION] = {.type = NLA_FLAG, },
};

static inline int get_event_length(struct iwl_rx_cmd_buffer *rxb)
{
	struct iwl_rx_packet *pkt = rxb_addr(rxb);
	if (pkt)
		return le32_to_cpu(pkt->len_n_flags) & FH_RSCSR_FRAME_SIZE_MSK;
	else
		return 0;
}



static void iwl_testmode_ucode_rx_pkt(struct iwl_priv *priv,
				      struct iwl_rx_cmd_buffer *rxb)
{
	struct ieee80211_hw *hw = priv->hw;
	struct sk_buff *skb;
	void *data;
	int length;

	data = (void *)rxb_addr(rxb);
	length = get_event_length(rxb);

	if (!data || length == 0)
		return;

	skb = cfg80211_testmode_alloc_event_skb(hw->wiphy, 20 + length,
								GFP_ATOMIC);
	if (skb == NULL) {
		IWL_ERR(priv,
			 "Run out of memory for messages to user space ?\n");
		return;
	}
	NLA_PUT_U32(skb, IWL_TM_ATTR_COMMAND, IWL_TM_CMD_DEV2APP_UCODE_RX_PKT);
	
	NLA_PUT(skb, IWL_TM_ATTR_UCODE_RX_PKT, length + sizeof(__le32), data);
	cfg80211_testmode_event(skb, GFP_ATOMIC);
	return;

nla_put_failure:
	kfree_skb(skb);
	IWL_ERR(priv, "Ouch, overran buffer, check allocation!\n");
}

void iwl_testmode_init(struct iwl_priv *priv)
{
	priv->pre_rx_handler = NULL;
	priv->testmode_trace.trace_enabled = false;
	priv->testmode_mem.read_in_progress = false;
}

static void iwl_mem_cleanup(struct iwl_priv *priv)
{
	if (priv->testmode_mem.read_in_progress) {
		kfree(priv->testmode_mem.buff_addr);
		priv->testmode_mem.buff_addr = NULL;
		priv->testmode_mem.buff_size = 0;
		priv->testmode_mem.num_chunks = 0;
		priv->testmode_mem.read_in_progress = false;
	}
}

static void iwl_trace_cleanup(struct iwl_priv *priv)
{
	if (priv->testmode_trace.trace_enabled) {
		if (priv->testmode_trace.cpu_addr &&
		    priv->testmode_trace.dma_addr)
			dma_free_coherent(trans(priv)->dev,
					priv->testmode_trace.total_size,
					priv->testmode_trace.cpu_addr,
					priv->testmode_trace.dma_addr);
		priv->testmode_trace.trace_enabled = false;
		priv->testmode_trace.cpu_addr = NULL;
		priv->testmode_trace.trace_addr = NULL;
		priv->testmode_trace.dma_addr = 0;
		priv->testmode_trace.buff_size = 0;
		priv->testmode_trace.total_size = 0;
	}
}


void iwl_testmode_cleanup(struct iwl_priv *priv)
{
	iwl_trace_cleanup(priv);
	iwl_mem_cleanup(priv);
}


static int iwl_testmode_ucode(struct ieee80211_hw *hw, struct nlattr **tb)
{
	struct iwl_priv *priv = IWL_MAC80211_GET_DVM(hw);
	struct iwl_host_cmd cmd;
	struct iwl_rx_packet *pkt;
	struct sk_buff *skb;
	void *reply_buf;
	u32 reply_len;
	int ret;
	bool cmd_want_skb;

	memset(&cmd, 0, sizeof(struct iwl_host_cmd));

	if (!tb[IWL_TM_ATTR_UCODE_CMD_ID] ||
	    !tb[IWL_TM_ATTR_UCODE_CMD_DATA]) {
		IWL_ERR(priv, "Missing ucode command mandatory fields\n");
		return -ENOMSG;
	}

	cmd.flags = CMD_ON_DEMAND | CMD_SYNC;
	cmd_want_skb = nla_get_flag(tb[IWL_TM_ATTR_UCODE_CMD_SKB]);
	if (cmd_want_skb)
		cmd.flags |= CMD_WANT_SKB;

	cmd.id = nla_get_u8(tb[IWL_TM_ATTR_UCODE_CMD_ID]);
	cmd.data[0] = nla_data(tb[IWL_TM_ATTR_UCODE_CMD_DATA]);
	cmd.len[0] = nla_len(tb[IWL_TM_ATTR_UCODE_CMD_DATA]);
	cmd.dataflags[0] = IWL_HCMD_DFL_NOCOPY;
	IWL_DEBUG_INFO(priv, "testmode ucode command ID 0x%x, flags 0x%x,"
				" len %d\n", cmd.id, cmd.flags, cmd.len[0]);

	ret = iwl_dvm_send_cmd(priv, &cmd);
	if (ret) {
		IWL_ERR(priv, "Failed to send hcmd\n");
		return ret;
	}
	if (!cmd_want_skb)
		return ret;

	
	pkt = cmd.resp_pkt;
	if (!pkt) {
		IWL_ERR(priv, "HCMD received a null response packet\n");
		return ret;
	}

	reply_len = le32_to_cpu(pkt->len_n_flags) & FH_RSCSR_FRAME_SIZE_MSK;
	skb = cfg80211_testmode_alloc_reply_skb(hw->wiphy, reply_len + 20);
	reply_buf = kmalloc(reply_len, GFP_KERNEL);
	if (!skb || !reply_buf) {
		kfree_skb(skb);
		kfree(reply_buf);
		return -ENOMEM;
	}

	
	memcpy(reply_buf, &(pkt->hdr), reply_len);
	iwl_free_resp(&cmd);

	NLA_PUT_U32(skb, IWL_TM_ATTR_COMMAND, IWL_TM_CMD_DEV2APP_UCODE_RX_PKT);
	NLA_PUT(skb, IWL_TM_ATTR_UCODE_RX_PKT, reply_len, reply_buf);
	return cfg80211_testmode_reply(skb);

nla_put_failure:
	IWL_DEBUG_INFO(priv, "Failed creating NL attributes\n");
	return -ENOMSG;
}


static int iwl_testmode_reg(struct ieee80211_hw *hw, struct nlattr **tb)
{
	struct iwl_priv *priv = IWL_MAC80211_GET_DVM(hw);
	u32 ofs, val32, cmd;
	u8 val8;
	struct sk_buff *skb;
	int status = 0;

	if (!tb[IWL_TM_ATTR_REG_OFFSET]) {
		IWL_ERR(priv, "Missing register offset\n");
		return -ENOMSG;
	}
	ofs = nla_get_u32(tb[IWL_TM_ATTR_REG_OFFSET]);
	IWL_INFO(priv, "testmode register access command offset 0x%x\n", ofs);

	cmd = nla_get_u32(tb[IWL_TM_ATTR_COMMAND]);
	if ((cmd == IWL_TM_CMD_APP2DEV_DIRECT_REG_READ32 ||
		cmd == IWL_TM_CMD_APP2DEV_DIRECT_REG_WRITE32 ||
		cmd == IWL_TM_CMD_APP2DEV_DIRECT_REG_WRITE8) &&
		(ofs >= FH_MEM_UPPER_BOUND)) {
		IWL_ERR(priv, "offset out of segment (0x0 - 0x%x)\n",
			FH_MEM_UPPER_BOUND);
		return -EINVAL;
	}

	switch (cmd) {
	case IWL_TM_CMD_APP2DEV_DIRECT_REG_READ32:
		val32 = iwl_read_direct32(trans(priv), ofs);
		IWL_INFO(priv, "32bit value to read 0x%x\n", val32);

		skb = cfg80211_testmode_alloc_reply_skb(hw->wiphy, 20);
		if (!skb) {
			IWL_ERR(priv, "Memory allocation fail\n");
			return -ENOMEM;
		}
		NLA_PUT_U32(skb, IWL_TM_ATTR_REG_VALUE32, val32);
		status = cfg80211_testmode_reply(skb);
		if (status < 0)
			IWL_ERR(priv, "Error sending msg : %d\n", status);
		break;
	case IWL_TM_CMD_APP2DEV_DIRECT_REG_WRITE32:
		if (!tb[IWL_TM_ATTR_REG_VALUE32]) {
			IWL_ERR(priv, "Missing value to write\n");
			return -ENOMSG;
		} else {
			val32 = nla_get_u32(tb[IWL_TM_ATTR_REG_VALUE32]);
			IWL_INFO(priv, "32bit value to write 0x%x\n", val32);
			iwl_write_direct32(trans(priv), ofs, val32);
		}
		break;
	case IWL_TM_CMD_APP2DEV_DIRECT_REG_WRITE8:
		if (!tb[IWL_TM_ATTR_REG_VALUE8]) {
			IWL_ERR(priv, "Missing value to write\n");
			return -ENOMSG;
		} else {
			val8 = nla_get_u8(tb[IWL_TM_ATTR_REG_VALUE8]);
			IWL_INFO(priv, "8bit value to write 0x%x\n", val8);
			iwl_write8(trans(priv), ofs, val8);
		}
		break;
	default:
		IWL_ERR(priv, "Unknown testmode register command ID\n");
		return -ENOSYS;
	}

	return status;

nla_put_failure:
	kfree_skb(skb);
	return -EMSGSIZE;
}


static int iwl_testmode_cfg_init_calib(struct iwl_priv *priv)
{
	struct iwl_notification_wait calib_wait;
	int ret;

	iwl_init_notification_wait(&priv->notif_wait, &calib_wait,
				   CALIBRATION_COMPLETE_NOTIFICATION,
				   NULL, NULL);
	ret = iwl_init_alive_start(priv);
	if (ret) {
		IWL_ERR(priv, "Fail init calibration: %d\n", ret);
		goto cfg_init_calib_error;
	}

	ret = iwl_wait_notification(&priv->notif_wait, &calib_wait, 2 * HZ);
	if (ret)
		IWL_ERR(priv, "Error detecting"
			" CALIBRATION_COMPLETE_NOTIFICATION: %d\n", ret);
	return ret;

cfg_init_calib_error:
	iwl_remove_notification(&priv->notif_wait, &calib_wait);
	return ret;
}

static int iwl_testmode_driver(struct ieee80211_hw *hw, struct nlattr **tb)
{
	struct iwl_priv *priv = IWL_MAC80211_GET_DVM(hw);
	struct iwl_trans *trans = trans(priv);
	struct sk_buff *skb;
	unsigned char *rsp_data_ptr = NULL;
	int status = 0, rsp_data_len = 0;
	u32 devid, inst_size = 0, data_size = 0;
	const struct fw_img *img;

	switch (nla_get_u32(tb[IWL_TM_ATTR_COMMAND])) {
	case IWL_TM_CMD_APP2DEV_GET_DEVICENAME:
		rsp_data_ptr = (unsigned char *)cfg(priv)->name;
		rsp_data_len = strlen(cfg(priv)->name);
		skb = cfg80211_testmode_alloc_reply_skb(hw->wiphy,
							rsp_data_len + 20);
		if (!skb) {
			IWL_ERR(priv, "Memory allocation fail\n");
			return -ENOMEM;
		}
		NLA_PUT_U32(skb, IWL_TM_ATTR_COMMAND,
			    IWL_TM_CMD_DEV2APP_SYNC_RSP);
		NLA_PUT(skb, IWL_TM_ATTR_SYNC_RSP,
			rsp_data_len, rsp_data_ptr);
		status = cfg80211_testmode_reply(skb);
		if (status < 0)
			IWL_ERR(priv, "Error sending msg : %d\n", status);
		break;

	case IWL_TM_CMD_APP2DEV_LOAD_INIT_FW:
		status = iwl_load_ucode_wait_alive(priv, IWL_UCODE_INIT);
		if (status)
			IWL_ERR(priv, "Error loading init ucode: %d\n", status);
		break;

	case IWL_TM_CMD_APP2DEV_CFG_INIT_CALIB:
		iwl_testmode_cfg_init_calib(priv);
		priv->ucode_loaded = false;
		iwl_trans_stop_device(trans);
		break;

	case IWL_TM_CMD_APP2DEV_LOAD_RUNTIME_FW:
		status = iwl_load_ucode_wait_alive(priv, IWL_UCODE_REGULAR);
		if (status) {
			IWL_ERR(priv,
				"Error loading runtime ucode: %d\n", status);
			break;
		}
		status = iwl_alive_start(priv);
		if (status)
			IWL_ERR(priv,
				"Error starting the device: %d\n", status);
		break;

	case IWL_TM_CMD_APP2DEV_LOAD_WOWLAN_FW:
		iwl_scan_cancel_timeout(priv, 200);
		priv->ucode_loaded = false;
		iwl_trans_stop_device(trans);
		status = iwl_load_ucode_wait_alive(priv, IWL_UCODE_WOWLAN);
		if (status) {
			IWL_ERR(priv,
				"Error loading WOWLAN ucode: %d\n", status);
			break;
		}
		status = iwl_alive_start(priv);
		if (status)
			IWL_ERR(priv,
				"Error starting the device: %d\n", status);
		break;

	case IWL_TM_CMD_APP2DEV_GET_EEPROM:
		if (priv->shrd->eeprom) {
			skb = cfg80211_testmode_alloc_reply_skb(hw->wiphy,
				cfg(priv)->base_params->eeprom_size + 20);
			if (!skb) {
				IWL_ERR(priv, "Memory allocation fail\n");
				return -ENOMEM;
			}
			NLA_PUT_U32(skb, IWL_TM_ATTR_COMMAND,
				IWL_TM_CMD_DEV2APP_EEPROM_RSP);
			NLA_PUT(skb, IWL_TM_ATTR_EEPROM,
				cfg(priv)->base_params->eeprom_size,
				priv->shrd->eeprom);
			status = cfg80211_testmode_reply(skb);
			if (status < 0)
				IWL_ERR(priv, "Error sending msg : %d\n",
					status);
		} else
			return -EFAULT;
		break;

	case IWL_TM_CMD_APP2DEV_FIXRATE_REQ:
		if (!tb[IWL_TM_ATTR_FIXRATE]) {
			IWL_ERR(priv, "Missing fixrate setting\n");
			return -ENOMSG;
		}
		priv->tm_fixed_rate = nla_get_u32(tb[IWL_TM_ATTR_FIXRATE]);
		break;

	case IWL_TM_CMD_APP2DEV_GET_FW_VERSION:
		IWL_INFO(priv, "uCode version raw: 0x%x\n",
			 priv->fw->ucode_ver);

		skb = cfg80211_testmode_alloc_reply_skb(hw->wiphy, 20);
		if (!skb) {
			IWL_ERR(priv, "Memory allocation fail\n");
			return -ENOMEM;
		}
		NLA_PUT_U32(skb, IWL_TM_ATTR_FW_VERSION,
			    priv->fw->ucode_ver);
		status = cfg80211_testmode_reply(skb);
		if (status < 0)
			IWL_ERR(priv, "Error sending msg : %d\n", status);
		break;

	case IWL_TM_CMD_APP2DEV_GET_DEVICE_ID:
		devid = trans(priv)->hw_id;
		IWL_INFO(priv, "hw version: 0x%x\n", devid);

		skb = cfg80211_testmode_alloc_reply_skb(hw->wiphy, 20);
		if (!skb) {
			IWL_ERR(priv, "Memory allocation fail\n");
			return -ENOMEM;
		}
		NLA_PUT_U32(skb, IWL_TM_ATTR_DEVICE_ID, devid);
		status = cfg80211_testmode_reply(skb);
		if (status < 0)
			IWL_ERR(priv, "Error sending msg : %d\n", status);
		break;

	case IWL_TM_CMD_APP2DEV_GET_FW_INFO:
		skb = cfg80211_testmode_alloc_reply_skb(hw->wiphy, 20 + 8);
		if (!skb) {
			IWL_ERR(priv, "Memory allocation fail\n");
			return -ENOMEM;
		}
		if (!priv->ucode_loaded) {
			IWL_ERR(priv, "No uCode has not been loaded\n");
			return -EINVAL;
		} else {
			img = &priv->fw->img[priv->shrd->ucode_type];
			inst_size = img->sec[IWL_UCODE_SECTION_INST].len;
			data_size = img->sec[IWL_UCODE_SECTION_DATA].len;
		}
		NLA_PUT_U32(skb, IWL_TM_ATTR_FW_TYPE, priv->shrd->ucode_type);
		NLA_PUT_U32(skb, IWL_TM_ATTR_FW_INST_SIZE, inst_size);
		NLA_PUT_U32(skb, IWL_TM_ATTR_FW_DATA_SIZE, data_size);
		status = cfg80211_testmode_reply(skb);
		if (status < 0)
			IWL_ERR(priv, "Error sending msg : %d\n", status);
		break;

	default:
		IWL_ERR(priv, "Unknown testmode driver command ID\n");
		return -ENOSYS;
	}
	return status;

nla_put_failure:
	kfree_skb(skb);
	return -EMSGSIZE;
}


static int iwl_testmode_trace(struct ieee80211_hw *hw, struct nlattr **tb)
{
	struct iwl_priv *priv = IWL_MAC80211_GET_DVM(hw);
	struct sk_buff *skb;
	int status = 0;
	struct device *dev = trans(priv)->dev;

	switch (nla_get_u32(tb[IWL_TM_ATTR_COMMAND])) {
	case IWL_TM_CMD_APP2DEV_BEGIN_TRACE:
		if (priv->testmode_trace.trace_enabled)
			return -EBUSY;

		if (!tb[IWL_TM_ATTR_TRACE_SIZE])
			priv->testmode_trace.buff_size = TRACE_BUFF_SIZE_DEF;
		else
			priv->testmode_trace.buff_size =
				nla_get_u32(tb[IWL_TM_ATTR_TRACE_SIZE]);
		if (!priv->testmode_trace.buff_size)
			return -EINVAL;
		if (priv->testmode_trace.buff_size < TRACE_BUFF_SIZE_MIN ||
		    priv->testmode_trace.buff_size > TRACE_BUFF_SIZE_MAX)
			return -EINVAL;

		priv->testmode_trace.total_size =
			priv->testmode_trace.buff_size + TRACE_BUFF_PADD;
		priv->testmode_trace.cpu_addr =
			dma_alloc_coherent(dev,
					   priv->testmode_trace.total_size,
					   &priv->testmode_trace.dma_addr,
					   GFP_KERNEL);
		if (!priv->testmode_trace.cpu_addr)
			return -ENOMEM;
		priv->testmode_trace.trace_enabled = true;
		priv->testmode_trace.trace_addr = (u8 *)PTR_ALIGN(
			priv->testmode_trace.cpu_addr, 0x100);
		memset(priv->testmode_trace.trace_addr, 0x03B,
			priv->testmode_trace.buff_size);
		skb = cfg80211_testmode_alloc_reply_skb(hw->wiphy,
			sizeof(priv->testmode_trace.dma_addr) + 20);
		if (!skb) {
			IWL_ERR(priv, "Memory allocation fail\n");
			iwl_trace_cleanup(priv);
			return -ENOMEM;
		}
		NLA_PUT(skb, IWL_TM_ATTR_TRACE_ADDR,
			sizeof(priv->testmode_trace.dma_addr),
			(u64 *)&priv->testmode_trace.dma_addr);
		status = cfg80211_testmode_reply(skb);
		if (status < 0) {
			IWL_ERR(priv, "Error sending msg : %d\n", status);
		}
		priv->testmode_trace.num_chunks =
			DIV_ROUND_UP(priv->testmode_trace.buff_size,
				     DUMP_CHUNK_SIZE);
		break;

	case IWL_TM_CMD_APP2DEV_END_TRACE:
		iwl_trace_cleanup(priv);
		break;
	default:
		IWL_ERR(priv, "Unknown testmode mem command ID\n");
		return -ENOSYS;
	}
	return status;

nla_put_failure:
	kfree_skb(skb);
	if (nla_get_u32(tb[IWL_TM_ATTR_COMMAND]) ==
	    IWL_TM_CMD_APP2DEV_BEGIN_TRACE)
		iwl_trace_cleanup(priv);
	return -EMSGSIZE;
}

static int iwl_testmode_trace_dump(struct ieee80211_hw *hw,
				   struct sk_buff *skb,
				   struct netlink_callback *cb)
{
	struct iwl_priv *priv = IWL_MAC80211_GET_DVM(hw);
	int idx, length;

	if (priv->testmode_trace.trace_enabled &&
	    priv->testmode_trace.trace_addr) {
		idx = cb->args[4];
		if (idx >= priv->testmode_trace.num_chunks)
			return -ENOENT;
		length = DUMP_CHUNK_SIZE;
		if (((idx + 1) == priv->testmode_trace.num_chunks) &&
		    (priv->testmode_trace.buff_size % DUMP_CHUNK_SIZE))
			length = priv->testmode_trace.buff_size %
				DUMP_CHUNK_SIZE;

		NLA_PUT(skb, IWL_TM_ATTR_TRACE_DUMP, length,
			priv->testmode_trace.trace_addr +
			(DUMP_CHUNK_SIZE * idx));
		idx++;
		cb->args[4] = idx;
		return 0;
	} else
		return -EFAULT;

 nla_put_failure:
	return -ENOBUFS;
}

static int iwl_testmode_ownership(struct ieee80211_hw *hw, struct nlattr **tb)
{
	struct iwl_priv *priv = IWL_MAC80211_GET_DVM(hw);
	u8 owner;

	if (!tb[IWL_TM_ATTR_UCODE_OWNER]) {
		IWL_ERR(priv, "Missing ucode owner\n");
		return -ENOMSG;
	}

	owner = nla_get_u8(tb[IWL_TM_ATTR_UCODE_OWNER]);
	if (owner == IWL_OWNERSHIP_DRIVER) {
		priv->ucode_owner = owner;
		priv->pre_rx_handler = NULL;
	} else if (owner == IWL_OWNERSHIP_TM) {
		priv->pre_rx_handler = iwl_testmode_ucode_rx_pkt;
		priv->ucode_owner = owner;
	} else {
		IWL_ERR(priv, "Invalid owner\n");
		return -EINVAL;
	}
	return 0;
}

static int iwl_testmode_indirect_read(struct iwl_priv *priv, u32 addr, u32 size)
{
	struct iwl_trans *trans = trans(priv);
	unsigned long flags;
	int i;

	if (size & 0x3)
		return -EINVAL;
	priv->testmode_mem.buff_size = size;
	priv->testmode_mem.buff_addr =
		kmalloc(priv->testmode_mem.buff_size, GFP_KERNEL);
	if (priv->testmode_mem.buff_addr == NULL)
		return -ENOMEM;

	
	if (IWL_TM_ABS_PRPH_START <= addr &&
		addr < IWL_TM_ABS_PRPH_START + PRPH_END) {
			spin_lock_irqsave(&trans->reg_lock, flags);
			iwl_grab_nic_access(trans);
			iwl_write32(trans, HBUS_TARG_PRPH_RADDR,
				addr | (3 << 24));
			for (i = 0; i < size; i += 4)
				*(u32 *)(priv->testmode_mem.buff_addr + i) =
					iwl_read32(trans, HBUS_TARG_PRPH_RDAT);
			iwl_release_nic_access(trans);
			spin_unlock_irqrestore(&trans->reg_lock, flags);
	} else { 
		_iwl_read_targ_mem_words(trans, addr,
			priv->testmode_mem.buff_addr,
			priv->testmode_mem.buff_size / 4);
	}

	priv->testmode_mem.num_chunks =
		DIV_ROUND_UP(priv->testmode_mem.buff_size, DUMP_CHUNK_SIZE);
	priv->testmode_mem.read_in_progress = true;
	return 0;

}

static int iwl_testmode_indirect_write(struct iwl_priv *priv, u32 addr,
	u32 size, unsigned char *buf)
{
	struct iwl_trans *trans = trans(priv);
	u32 val, i;
	unsigned long flags;

	if (IWL_TM_ABS_PRPH_START <= addr &&
		addr < IWL_TM_ABS_PRPH_START + PRPH_END) {
			
			if (size < 4) {
				memcpy(&val, buf, size);
				spin_lock_irqsave(&trans->reg_lock, flags);
				iwl_grab_nic_access(trans);
				iwl_write32(trans, HBUS_TARG_PRPH_WADDR,
					    (addr & 0x0000FFFF) |
					    ((size - 1) << 24));
				iwl_write32(trans, HBUS_TARG_PRPH_WDAT, val);
				iwl_release_nic_access(trans);
				
				mmiowb();
				spin_unlock_irqrestore(&trans->reg_lock, flags);
			} else {
				if (size % 4)
					return -EINVAL;
				for (i = 0; i < size; i += 4)
					iwl_write_prph(trans, addr+i,
						*(u32 *)(buf+i));
			}
	} else if (iwlagn_hw_valid_rtc_data_addr(addr) ||
		(IWLAGN_RTC_INST_LOWER_BOUND <= addr &&
		addr < IWLAGN_RTC_INST_UPPER_BOUND)) {
			_iwl_write_targ_mem_words(trans, addr, buf, size/4);
	} else
		return -EINVAL;
	return 0;
}

static int iwl_testmode_indirect_mem(struct ieee80211_hw *hw,
	struct nlattr **tb)
{
	struct iwl_priv *priv = IWL_MAC80211_GET_DVM(hw);
	u32 addr, size, cmd;
	unsigned char *buf;

	
	if (priv->testmode_mem.read_in_progress)
		return -EBUSY;

	cmd = nla_get_u32(tb[IWL_TM_ATTR_COMMAND]);
	if (!tb[IWL_TM_ATTR_MEM_ADDR]) {
		IWL_ERR(priv, "Error finding memory offset address\n");
		return -ENOMSG;
	}
	addr = nla_get_u32(tb[IWL_TM_ATTR_MEM_ADDR]);
	if (!tb[IWL_TM_ATTR_BUFFER_SIZE]) {
		IWL_ERR(priv, "Error finding size for memory reading\n");
		return -ENOMSG;
	}
	size = nla_get_u32(tb[IWL_TM_ATTR_BUFFER_SIZE]);

	if (cmd == IWL_TM_CMD_APP2DEV_INDIRECT_BUFFER_READ)
		return iwl_testmode_indirect_read(priv, addr,  size);
	else {
		if (!tb[IWL_TM_ATTR_BUFFER_DUMP])
			return -EINVAL;
		buf = (unsigned char *) nla_data(tb[IWL_TM_ATTR_BUFFER_DUMP]);
		return iwl_testmode_indirect_write(priv, addr, size, buf);
	}
}

static int iwl_testmode_buffer_dump(struct ieee80211_hw *hw,
				    struct sk_buff *skb,
				    struct netlink_callback *cb)
{
	struct iwl_priv *priv = IWL_MAC80211_GET_DVM(hw);
	int idx, length;

	if (priv->testmode_mem.read_in_progress) {
		idx = cb->args[4];
		if (idx >= priv->testmode_mem.num_chunks) {
			iwl_mem_cleanup(priv);
			return -ENOENT;
		}
		length = DUMP_CHUNK_SIZE;
		if (((idx + 1) == priv->testmode_mem.num_chunks) &&
		    (priv->testmode_mem.buff_size % DUMP_CHUNK_SIZE))
			length = priv->testmode_mem.buff_size %
				DUMP_CHUNK_SIZE;

		NLA_PUT(skb, IWL_TM_ATTR_BUFFER_DUMP, length,
			priv->testmode_mem.buff_addr +
			(DUMP_CHUNK_SIZE * idx));
		idx++;
		cb->args[4] = idx;
		return 0;
	} else
		return -EFAULT;

 nla_put_failure:
	return -ENOBUFS;
}

static int iwl_testmode_notifications(struct ieee80211_hw *hw,
	struct nlattr **tb)
{
	struct iwl_priv *priv = IWL_MAC80211_GET_DVM(hw);
	bool enable;

	enable = nla_get_flag(tb[IWL_TM_ATTR_ENABLE_NOTIFICATION]);
	if (enable)
		priv->pre_rx_handler = iwl_testmode_ucode_rx_pkt;
	else
		priv->pre_rx_handler = NULL;
	return 0;
}


int iwlagn_mac_testmode_cmd(struct ieee80211_hw *hw, void *data, int len)
{
	struct nlattr *tb[IWL_TM_ATTR_MAX];
	struct iwl_priv *priv = IWL_MAC80211_GET_DVM(hw);
	int result;

	result = nla_parse(tb, IWL_TM_ATTR_MAX - 1, data, len,
			iwl_testmode_gnl_msg_policy);
	if (result != 0) {
		IWL_ERR(priv, "Error parsing the gnl message : %d\n", result);
		return result;
	}

	
	if (!tb[IWL_TM_ATTR_COMMAND]) {
		IWL_ERR(priv, "Missing testmode command type\n");
		return -ENOMSG;
	}
	
	mutex_lock(&priv->mutex);

	switch (nla_get_u32(tb[IWL_TM_ATTR_COMMAND])) {
	case IWL_TM_CMD_APP2DEV_UCODE:
		IWL_DEBUG_INFO(priv, "testmode cmd to uCode\n");
		result = iwl_testmode_ucode(hw, tb);
		break;
	case IWL_TM_CMD_APP2DEV_DIRECT_REG_READ32:
	case IWL_TM_CMD_APP2DEV_DIRECT_REG_WRITE32:
	case IWL_TM_CMD_APP2DEV_DIRECT_REG_WRITE8:
		IWL_DEBUG_INFO(priv, "testmode cmd to register\n");
		result = iwl_testmode_reg(hw, tb);
		break;
	case IWL_TM_CMD_APP2DEV_GET_DEVICENAME:
	case IWL_TM_CMD_APP2DEV_LOAD_INIT_FW:
	case IWL_TM_CMD_APP2DEV_CFG_INIT_CALIB:
	case IWL_TM_CMD_APP2DEV_LOAD_RUNTIME_FW:
	case IWL_TM_CMD_APP2DEV_GET_EEPROM:
	case IWL_TM_CMD_APP2DEV_FIXRATE_REQ:
	case IWL_TM_CMD_APP2DEV_LOAD_WOWLAN_FW:
	case IWL_TM_CMD_APP2DEV_GET_FW_VERSION:
	case IWL_TM_CMD_APP2DEV_GET_DEVICE_ID:
	case IWL_TM_CMD_APP2DEV_GET_FW_INFO:
		IWL_DEBUG_INFO(priv, "testmode cmd to driver\n");
		result = iwl_testmode_driver(hw, tb);
		break;

	case IWL_TM_CMD_APP2DEV_BEGIN_TRACE:
	case IWL_TM_CMD_APP2DEV_END_TRACE:
	case IWL_TM_CMD_APP2DEV_READ_TRACE:
		IWL_DEBUG_INFO(priv, "testmode uCode trace cmd to driver\n");
		result = iwl_testmode_trace(hw, tb);
		break;

	case IWL_TM_CMD_APP2DEV_OWNERSHIP:
		IWL_DEBUG_INFO(priv, "testmode change uCode ownership\n");
		result = iwl_testmode_ownership(hw, tb);
		break;

	case IWL_TM_CMD_APP2DEV_INDIRECT_BUFFER_READ:
	case IWL_TM_CMD_APP2DEV_INDIRECT_BUFFER_WRITE:
		IWL_DEBUG_INFO(priv, "testmode indirect memory cmd "
			"to driver\n");
		result = iwl_testmode_indirect_mem(hw, tb);
		break;

	case IWL_TM_CMD_APP2DEV_NOTIFICATIONS:
		IWL_DEBUG_INFO(priv, "testmode notifications cmd "
			"to driver\n");
		result = iwl_testmode_notifications(hw, tb);
		break;

	default:
		IWL_ERR(priv, "Unknown testmode command\n");
		result = -ENOSYS;
		break;
	}

	mutex_unlock(&priv->mutex);
	return result;
}

int iwlagn_mac_testmode_dump(struct ieee80211_hw *hw, struct sk_buff *skb,
		      struct netlink_callback *cb,
		      void *data, int len)
{
	struct nlattr *tb[IWL_TM_ATTR_MAX];
	struct iwl_priv *priv = IWL_MAC80211_GET_DVM(hw);
	int result;
	u32 cmd;

	if (cb->args[3]) {
		
		cmd = cb->args[3] - 1;
	} else {
		result = nla_parse(tb, IWL_TM_ATTR_MAX - 1, data, len,
				iwl_testmode_gnl_msg_policy);
		if (result) {
			IWL_ERR(priv,
				"Error parsing the gnl message : %d\n", result);
			return result;
		}

		
		if (!tb[IWL_TM_ATTR_COMMAND]) {
			IWL_ERR(priv, "Missing testmode command type\n");
			return -ENOMSG;
		}
		cmd = nla_get_u32(tb[IWL_TM_ATTR_COMMAND]);
		cb->args[3] = cmd + 1;
	}

	
	mutex_lock(&priv->mutex);
	switch (cmd) {
	case IWL_TM_CMD_APP2DEV_READ_TRACE:
		IWL_DEBUG_INFO(priv, "uCode trace cmd to driver\n");
		result = iwl_testmode_trace_dump(hw, skb, cb);
		break;
	case IWL_TM_CMD_APP2DEV_INDIRECT_BUFFER_DUMP:
		IWL_DEBUG_INFO(priv, "testmode sram dump cmd to driver\n");
		result = iwl_testmode_buffer_dump(hw, skb, cb);
		break;
	default:
		result = -EINVAL;
		break;
	}

	mutex_unlock(&priv->mutex);
	return result;
}
