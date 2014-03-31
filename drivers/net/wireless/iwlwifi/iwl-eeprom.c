/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2008 - 2012 Intel Corporation. All rights reserved.
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
 *****************************************************************************/


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>

#include <net/mac80211.h>

#include "iwl-commands.h"
#include "iwl-dev.h"
#include "iwl-core.h"
#include "iwl-debug.h"
#include "iwl-agn.h"
#include "iwl-eeprom.h"
#include "iwl-io.h"
#include "iwl-prph.h"


const u8 iwl_eeprom_band_1[14] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14
};

static const u8 iwl_eeprom_band_2[] = {	
	183, 184, 185, 187, 188, 189, 192, 196, 7, 8, 11, 12, 16
};

static const u8 iwl_eeprom_band_3[] = {	
	34, 36, 38, 40, 42, 44, 46, 48, 52, 56, 60, 64
};

static const u8 iwl_eeprom_band_4[] = {	
	100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140
};

static const u8 iwl_eeprom_band_5[] = {	
	145, 149, 153, 157, 161, 165
};

static const u8 iwl_eeprom_band_6[] = {       
	1, 2, 3, 4, 5, 6, 7
};

static const u8 iwl_eeprom_band_7[] = {       
	36, 44, 52, 60, 100, 108, 116, 124, 132, 149, 157
};



#define	EEPROM_SEM_TIMEOUT 10		
#define EEPROM_SEM_RETRY_LIMIT 1000	

static int iwl_eeprom_acquire_semaphore(struct iwl_trans *trans)
{
	u16 count;
	int ret;

	for (count = 0; count < EEPROM_SEM_RETRY_LIMIT; count++) {
		
		iwl_set_bit(trans, CSR_HW_IF_CONFIG_REG,
			    CSR_HW_IF_CONFIG_REG_BIT_EEPROM_OWN_SEM);

		
		ret = iwl_poll_bit(trans, CSR_HW_IF_CONFIG_REG,
				CSR_HW_IF_CONFIG_REG_BIT_EEPROM_OWN_SEM,
				CSR_HW_IF_CONFIG_REG_BIT_EEPROM_OWN_SEM,
				EEPROM_SEM_TIMEOUT);
		if (ret >= 0) {
			IWL_DEBUG_EEPROM(trans,
				"Acquired semaphore after %d tries.\n",
				count+1);
			return ret;
		}
	}

	return ret;
}

static void iwl_eeprom_release_semaphore(struct iwl_trans *trans)
{
	iwl_clear_bit(trans, CSR_HW_IF_CONFIG_REG,
		CSR_HW_IF_CONFIG_REG_BIT_EEPROM_OWN_SEM);

}

static int iwl_eeprom_verify_signature(struct iwl_trans *trans)
{
	u32 gp = iwl_read32(trans, CSR_EEPROM_GP) &
			   CSR_EEPROM_GP_VALID_MSK;
	int ret = 0;

	IWL_DEBUG_EEPROM(trans, "EEPROM signature=0x%08x\n", gp);
	switch (gp) {
	case CSR_EEPROM_GP_BAD_SIG_EEP_GOOD_SIG_OTP:
		if (trans->nvm_device_type != NVM_DEVICE_TYPE_OTP) {
			IWL_ERR(trans, "EEPROM with bad signature: 0x%08x\n",
				gp);
			ret = -ENOENT;
		}
		break;
	case CSR_EEPROM_GP_GOOD_SIG_EEP_LESS_THAN_4K:
	case CSR_EEPROM_GP_GOOD_SIG_EEP_MORE_THAN_4K:
		if (trans->nvm_device_type != NVM_DEVICE_TYPE_EEPROM) {
			IWL_ERR(trans, "OTP with bad signature: 0x%08x\n", gp);
			ret = -ENOENT;
		}
		break;
	case CSR_EEPROM_GP_BAD_SIGNATURE_BOTH_EEP_AND_OTP:
	default:
		IWL_ERR(trans, "bad EEPROM/OTP signature, type=%s, "
			"EEPROM_GP=0x%08x\n",
			(trans->nvm_device_type == NVM_DEVICE_TYPE_OTP)
			? "OTP" : "EEPROM", gp);
		ret = -ENOENT;
		break;
	}
	return ret;
}

u16 iwl_eeprom_query16(const struct iwl_shared *shrd, size_t offset)
{
	if (!shrd->eeprom)
		return 0;
	return (u16)shrd->eeprom[offset] | ((u16)shrd->eeprom[offset + 1] << 8);
}

int iwl_eeprom_check_version(struct iwl_priv *priv)
{
	u16 eeprom_ver;
	u16 calib_ver;

	eeprom_ver = iwl_eeprom_query16(priv->shrd, EEPROM_VERSION);
	calib_ver = iwl_eeprom_calib_version(priv->shrd);

	if (eeprom_ver < cfg(priv)->eeprom_ver ||
	    calib_ver < cfg(priv)->eeprom_calib_ver)
		goto err;

	IWL_INFO(priv, "device EEPROM VER=0x%x, CALIB=0x%x\n",
		 eeprom_ver, calib_ver);

	return 0;
err:
	IWL_ERR(priv, "Unsupported (too old) EEPROM VER=0x%x < 0x%x "
		  "CALIB=0x%x < 0x%x\n",
		  eeprom_ver, cfg(priv)->eeprom_ver,
		  calib_ver,  cfg(priv)->eeprom_calib_ver);
	return -EINVAL;

}

int iwl_eeprom_init_hw_params(struct iwl_priv *priv)
{
	struct iwl_shared *shrd = priv->shrd;
	u16 radio_cfg;

	hw_params(priv).sku = iwl_eeprom_query16(shrd, EEPROM_SKU_CAP);
	if (hw_params(priv).sku & EEPROM_SKU_CAP_11N_ENABLE &&
	    !cfg(priv)->ht_params) {
		IWL_ERR(priv, "Invalid 11n configuration\n");
		return -EINVAL;
	}

	if (!hw_params(priv).sku) {
		IWL_ERR(priv, "Invalid device sku\n");
		return -EINVAL;
	}

	IWL_INFO(priv, "Device SKU: 0x%X\n", hw_params(priv).sku);

	radio_cfg = iwl_eeprom_query16(shrd, EEPROM_RADIO_CONFIG);

	hw_params(priv).valid_tx_ant = EEPROM_RF_CFG_TX_ANT_MSK(radio_cfg);
	hw_params(priv).valid_rx_ant = EEPROM_RF_CFG_RX_ANT_MSK(radio_cfg);

	
	if (cfg(priv)->valid_tx_ant)
		hw_params(priv).valid_tx_ant = cfg(priv)->valid_tx_ant;
	if (cfg(priv)->valid_rx_ant)
		hw_params(priv).valid_rx_ant = cfg(priv)->valid_rx_ant;

	if (!hw_params(priv).valid_tx_ant || !hw_params(priv).valid_rx_ant) {
		IWL_ERR(priv, "Invalid chain (0x%X, 0x%X)\n",
			hw_params(priv).valid_tx_ant,
			hw_params(priv).valid_rx_ant);
		return -EINVAL;
	}

	IWL_INFO(priv, "Valid Tx ant: 0x%X, Valid Rx ant: 0x%X\n",
		 hw_params(priv).valid_tx_ant, hw_params(priv).valid_rx_ant);

	return 0;
}

void iwl_eeprom_get_mac(const struct iwl_shared *shrd, u8 *mac)
{
	const u8 *addr = iwl_eeprom_query_addr(shrd,
					EEPROM_MAC_ADDRESS);
	memcpy(mac, addr, ETH_ALEN);
}


static void iwl_set_otp_access(struct iwl_trans *trans,
			       enum iwl_access_mode mode)
{
	iwl_read32(trans, CSR_OTP_GP_REG);

	if (mode == IWL_OTP_ACCESS_ABSOLUTE)
		iwl_clear_bit(trans, CSR_OTP_GP_REG,
			      CSR_OTP_GP_REG_OTP_ACCESS_MODE);
	else
		iwl_set_bit(trans, CSR_OTP_GP_REG,
			    CSR_OTP_GP_REG_OTP_ACCESS_MODE);
}

static int iwl_get_nvm_type(struct iwl_trans *trans, u32 hw_rev)
{
	u32 otpgp;
	int nvm_type;

	
	switch (hw_rev & CSR_HW_REV_TYPE_MSK) {
	case CSR_HW_REV_TYPE_NONE:
		IWL_ERR(trans, "Unknown hardware type\n");
		return -ENOENT;
	case CSR_HW_REV_TYPE_5300:
	case CSR_HW_REV_TYPE_5350:
	case CSR_HW_REV_TYPE_5100:
	case CSR_HW_REV_TYPE_5150:
		nvm_type = NVM_DEVICE_TYPE_EEPROM;
		break;
	default:
		otpgp = iwl_read32(trans, CSR_OTP_GP_REG);
		if (otpgp & CSR_OTP_GP_REG_DEVICE_SELECT)
			nvm_type = NVM_DEVICE_TYPE_OTP;
		else
			nvm_type = NVM_DEVICE_TYPE_EEPROM;
		break;
	}
	return  nvm_type;
}

static int iwl_init_otp_access(struct iwl_trans *trans)
{
	int ret;

	
	iwl_write32(trans, CSR_GP_CNTRL,
		    iwl_read32(trans, CSR_GP_CNTRL) |
		    CSR_GP_CNTRL_REG_FLAG_INIT_DONE);

	
	ret = iwl_poll_bit(trans, CSR_GP_CNTRL,
				 CSR_GP_CNTRL_REG_FLAG_MAC_CLOCK_READY,
				 CSR_GP_CNTRL_REG_FLAG_MAC_CLOCK_READY,
				 25000);
	if (ret < 0)
		IWL_ERR(trans, "Time out access OTP\n");
	else {
		iwl_set_bits_prph(trans, APMG_PS_CTRL_REG,
				  APMG_PS_CTRL_VAL_RESET_REQ);
		udelay(5);
		iwl_clear_bits_prph(trans, APMG_PS_CTRL_REG,
				    APMG_PS_CTRL_VAL_RESET_REQ);

		if (cfg(trans)->base_params->shadow_ram_support)
			iwl_set_bit(trans, CSR_DBG_LINK_PWR_MGMT_REG,
				CSR_RESET_LINK_PWR_MGMT_DISABLED);
	}
	return ret;
}

static int iwl_read_otp_word(struct iwl_trans *trans, u16 addr,
			     __le16 *eeprom_data)
{
	int ret = 0;
	u32 r;
	u32 otpgp;

	iwl_write32(trans, CSR_EEPROM_REG,
		    CSR_EEPROM_REG_MSK_ADDR & (addr << 1));
	ret = iwl_poll_bit(trans, CSR_EEPROM_REG,
				 CSR_EEPROM_REG_READ_VALID_MSK,
				 CSR_EEPROM_REG_READ_VALID_MSK,
				 IWL_EEPROM_ACCESS_TIMEOUT);
	if (ret < 0) {
		IWL_ERR(trans, "Time out reading OTP[%d]\n", addr);
		return ret;
	}
	r = iwl_read32(trans, CSR_EEPROM_REG);
	
	otpgp = iwl_read32(trans, CSR_OTP_GP_REG);
	if (otpgp & CSR_OTP_GP_REG_ECC_UNCORR_STATUS_MSK) {
		
		
		iwl_set_bit(trans, CSR_OTP_GP_REG,
			CSR_OTP_GP_REG_ECC_UNCORR_STATUS_MSK);
		IWL_ERR(trans, "Uncorrectable OTP ECC error, abort OTP read\n");
		return -EINVAL;
	}
	if (otpgp & CSR_OTP_GP_REG_ECC_CORR_STATUS_MSK) {
		
		
		iwl_set_bit(trans, CSR_OTP_GP_REG,
				CSR_OTP_GP_REG_ECC_CORR_STATUS_MSK);
		IWL_ERR(trans, "Correctable OTP ECC error, continue read\n");
	}
	*eeprom_data = cpu_to_le16(r >> 16);
	return 0;
}

static bool iwl_is_otp_empty(struct iwl_trans *trans)
{
	u16 next_link_addr = 0;
	__le16 link_value;
	bool is_empty = false;

	
	if (!iwl_read_otp_word(trans, next_link_addr, &link_value)) {
		if (!link_value) {
			IWL_ERR(trans, "OTP is empty\n");
			is_empty = true;
		}
	} else {
		IWL_ERR(trans, "Unable to read first block of OTP list.\n");
		is_empty = true;
	}

	return is_empty;
}


static int iwl_find_otp_image(struct iwl_trans *trans,
					u16 *validblockaddr)
{
	u16 next_link_addr = 0, valid_addr;
	__le16 link_value = 0;
	int usedblocks = 0;

	
	iwl_set_otp_access(trans, IWL_OTP_ACCESS_ABSOLUTE);

	
	if (iwl_is_otp_empty(trans))
		return -EINVAL;

	do {
		valid_addr = next_link_addr;
		next_link_addr = le16_to_cpu(link_value) * sizeof(u16);
		IWL_DEBUG_EEPROM(trans, "OTP blocks %d addr 0x%x\n",
			       usedblocks, next_link_addr);
		if (iwl_read_otp_word(trans, next_link_addr, &link_value))
			return -EINVAL;
		if (!link_value) {
			*validblockaddr = valid_addr;
			
			*validblockaddr += 2;
			return 0;
		}
		
		usedblocks++;
	} while (usedblocks <= cfg(trans)->base_params->max_ll_items);

	
	IWL_DEBUG_EEPROM(trans, "OTP has no valid blocks\n");
	return -EINVAL;
}

static s8 iwl_get_max_txpower_avg(const struct iwl_cfg *cfg,
		struct iwl_eeprom_enhanced_txpwr *enhanced_txpower,
		int element, s8 *max_txpower_in_half_dbm)
{
	s8 max_txpower_avg = 0; 

	
	if ((cfg->valid_tx_ant & ANT_A) &&
	    (enhanced_txpower[element].chain_a_max > max_txpower_avg))
		max_txpower_avg = enhanced_txpower[element].chain_a_max;
	if ((cfg->valid_tx_ant & ANT_B) &&
	    (enhanced_txpower[element].chain_b_max > max_txpower_avg))
		max_txpower_avg = enhanced_txpower[element].chain_b_max;
	if ((cfg->valid_tx_ant & ANT_C) &&
	    (enhanced_txpower[element].chain_c_max > max_txpower_avg))
		max_txpower_avg = enhanced_txpower[element].chain_c_max;
	if (((cfg->valid_tx_ant == ANT_AB) |
	    (cfg->valid_tx_ant == ANT_BC) |
	    (cfg->valid_tx_ant == ANT_AC)) &&
	    (enhanced_txpower[element].mimo2_max > max_txpower_avg))
		max_txpower_avg =  enhanced_txpower[element].mimo2_max;
	if ((cfg->valid_tx_ant == ANT_ABC) &&
	    (enhanced_txpower[element].mimo3_max > max_txpower_avg))
		max_txpower_avg = enhanced_txpower[element].mimo3_max;

	*max_txpower_in_half_dbm = max_txpower_avg;
	return (max_txpower_avg & 0x01) + (max_txpower_avg >> 1);
}

static void
iwl_eeprom_enh_txp_read_element(struct iwl_priv *priv,
				    struct iwl_eeprom_enhanced_txpwr *txp,
				    s8 max_txpower_avg)
{
	int ch_idx;
	bool is_ht40 = txp->flags & IWL_EEPROM_ENH_TXP_FL_40MHZ;
	enum ieee80211_band band;

	band = txp->flags & IWL_EEPROM_ENH_TXP_FL_BAND_52G ?
		IEEE80211_BAND_5GHZ : IEEE80211_BAND_2GHZ;

	for (ch_idx = 0; ch_idx < priv->channel_count; ch_idx++) {
		struct iwl_channel_info *ch_info = &priv->channel_info[ch_idx];

		
		if (txp->channel != 0 && ch_info->channel != txp->channel)
			continue;

		
		if (band != ch_info->band)
			continue;

		if (ch_info->max_power_avg < max_txpower_avg && !is_ht40) {
			ch_info->max_power_avg = max_txpower_avg;
			ch_info->curr_txpow = max_txpower_avg;
			ch_info->scan_power = max_txpower_avg;
		}

		if (is_ht40 && ch_info->ht40_max_power_avg < max_txpower_avg)
			ch_info->ht40_max_power_avg = max_txpower_avg;
	}
}

#define EEPROM_TXP_OFFS	(0x00 | INDIRECT_ADDRESS | INDIRECT_TXP_LIMIT)
#define EEPROM_TXP_ENTRY_LEN sizeof(struct iwl_eeprom_enhanced_txpwr)
#define EEPROM_TXP_SZ_OFFS (0x00 | INDIRECT_ADDRESS | INDIRECT_TXP_LIMIT_SIZE)

#define TXP_CHECK_AND_PRINT(x) ((txp->flags & IWL_EEPROM_ENH_TXP_FL_##x) \
			    ? # x " " : "")

static void iwl_eeprom_enhanced_txpower(struct iwl_priv *priv)
{
	struct iwl_shared *shrd = priv->shrd;
	struct iwl_eeprom_enhanced_txpwr *txp_array, *txp;
	int idx, entries;
	__le16 *txp_len;
	s8 max_txp_avg, max_txp_avg_halfdbm;

	BUILD_BUG_ON(sizeof(struct iwl_eeprom_enhanced_txpwr) != 8);

	
	txp_len = (__le16 *) iwl_eeprom_query_addr(shrd, EEPROM_TXP_SZ_OFFS);
	entries = le16_to_cpup(txp_len) * 2 / EEPROM_TXP_ENTRY_LEN;

	txp_array = (void *) iwl_eeprom_query_addr(shrd, EEPROM_TXP_OFFS);

	for (idx = 0; idx < entries; idx++) {
		txp = &txp_array[idx];
		
		if (!(txp->flags & IWL_EEPROM_ENH_TXP_FL_VALID))
			continue;

		IWL_DEBUG_EEPROM(priv, "%s %d:\t %s%s%s%s%s%s%s%s (0x%02x)\n",
				 (txp->channel && (txp->flags &
					IWL_EEPROM_ENH_TXP_FL_COMMON_TYPE)) ?
					"Common " : (txp->channel) ?
					"Channel" : "Common",
				 (txp->channel),
				 TXP_CHECK_AND_PRINT(VALID),
				 TXP_CHECK_AND_PRINT(BAND_52G),
				 TXP_CHECK_AND_PRINT(OFDM),
				 TXP_CHECK_AND_PRINT(40MHZ),
				 TXP_CHECK_AND_PRINT(HT_AP),
				 TXP_CHECK_AND_PRINT(RES1),
				 TXP_CHECK_AND_PRINT(RES2),
				 TXP_CHECK_AND_PRINT(COMMON_TYPE),
				 txp->flags);
		IWL_DEBUG_EEPROM(priv, "\t\t chain_A: 0x%02x "
				 "chain_B: 0X%02x chain_C: 0X%02x\n",
				 txp->chain_a_max, txp->chain_b_max,
				 txp->chain_c_max);
		IWL_DEBUG_EEPROM(priv, "\t\t MIMO2: 0x%02x "
				 "MIMO3: 0x%02x High 20_on_40: 0x%02x "
				 "Low 20_on_40: 0x%02x\n",
				 txp->mimo2_max, txp->mimo3_max,
				 ((txp->delta_20_in_40 & 0xf0) >> 4),
				 (txp->delta_20_in_40 & 0x0f));

		max_txp_avg = iwl_get_max_txpower_avg(cfg(priv), txp_array, idx,
						      &max_txp_avg_halfdbm);

		if (max_txp_avg > priv->tx_power_user_lmt)
			priv->tx_power_user_lmt = max_txp_avg;
		if (max_txp_avg_halfdbm > priv->tx_power_lmt_in_half_dbm)
			priv->tx_power_lmt_in_half_dbm = max_txp_avg_halfdbm;

		iwl_eeprom_enh_txp_read_element(priv, txp, max_txp_avg);
	}
}

int iwl_eeprom_init(struct iwl_trans *trans, u32 hw_rev)
{
	__le16 *e;
	u32 gp = iwl_read32(trans, CSR_EEPROM_GP);
	int sz;
	int ret;
	u16 addr;
	u16 validblockaddr = 0;
	u16 cache_addr = 0;

	trans->nvm_device_type = iwl_get_nvm_type(trans, hw_rev);
	if (trans->nvm_device_type == -ENOENT)
		return -ENOENT;
	
	sz = cfg(trans)->base_params->eeprom_size;
	IWL_DEBUG_EEPROM(trans, "NVM size = %d\n", sz);
	trans->shrd->eeprom = kzalloc(sz, GFP_KERNEL);
	if (!trans->shrd->eeprom) {
		ret = -ENOMEM;
		goto alloc_err;
	}
	e = (__le16 *)trans->shrd->eeprom;

	ret = iwl_eeprom_verify_signature(trans);
	if (ret < 0) {
		IWL_ERR(trans, "EEPROM not found, EEPROM_GP=0x%08x\n", gp);
		ret = -ENOENT;
		goto err;
	}

	
	ret = iwl_eeprom_acquire_semaphore(trans);
	if (ret < 0) {
		IWL_ERR(trans, "Failed to acquire EEPROM semaphore.\n");
		ret = -ENOENT;
		goto err;
	}

	if (trans->nvm_device_type == NVM_DEVICE_TYPE_OTP) {

		ret = iwl_init_otp_access(trans);
		if (ret) {
			IWL_ERR(trans, "Failed to initialize OTP access.\n");
			ret = -ENOENT;
			goto done;
		}
		iwl_write32(trans, CSR_EEPROM_GP,
			    iwl_read32(trans, CSR_EEPROM_GP) &
			    ~CSR_EEPROM_GP_IF_OWNER_MSK);

		iwl_set_bit(trans, CSR_OTP_GP_REG,
			     CSR_OTP_GP_REG_ECC_CORR_STATUS_MSK |
			     CSR_OTP_GP_REG_ECC_UNCORR_STATUS_MSK);
		
		if (!cfg(trans)->base_params->shadow_ram_support) {
			if (iwl_find_otp_image(trans, &validblockaddr)) {
				ret = -ENOENT;
				goto done;
			}
		}
		for (addr = validblockaddr; addr < validblockaddr + sz;
		     addr += sizeof(u16)) {
			__le16 eeprom_data;

			ret = iwl_read_otp_word(trans, addr, &eeprom_data);
			if (ret)
				goto done;
			e[cache_addr / 2] = eeprom_data;
			cache_addr += sizeof(u16);
		}
	} else {
		
		for (addr = 0; addr < sz; addr += sizeof(u16)) {
			u32 r;

			iwl_write32(trans, CSR_EEPROM_REG,
				    CSR_EEPROM_REG_MSK_ADDR & (addr << 1));

			ret = iwl_poll_bit(trans, CSR_EEPROM_REG,
						  CSR_EEPROM_REG_READ_VALID_MSK,
						  CSR_EEPROM_REG_READ_VALID_MSK,
						  IWL_EEPROM_ACCESS_TIMEOUT);
			if (ret < 0) {
				IWL_ERR(trans,
					"Time out reading EEPROM[%d]\n", addr);
				goto done;
			}
			r = iwl_read32(trans, CSR_EEPROM_REG);
			e[addr / 2] = cpu_to_le16(r >> 16);
		}
	}

	IWL_DEBUG_EEPROM(trans, "NVM Type: %s, version: 0x%x\n",
		       (trans->nvm_device_type == NVM_DEVICE_TYPE_OTP)
		       ? "OTP" : "EEPROM",
		       iwl_eeprom_query16(trans->shrd, EEPROM_VERSION));

	ret = 0;
done:
	iwl_eeprom_release_semaphore(trans);

err:
	if (ret)
		iwl_eeprom_free(trans->shrd);
alloc_err:
	return ret;
}

void iwl_eeprom_free(struct iwl_shared *shrd)
{
	kfree(shrd->eeprom);
	shrd->eeprom = NULL;
}

static void iwl_init_band_reference(const struct iwl_priv *priv,
			int eep_band, int *eeprom_ch_count,
			const struct iwl_eeprom_channel **eeprom_ch_info,
			const u8 **eeprom_ch_index)
{
	struct iwl_shared *shrd = priv->shrd;
	u32 offset = cfg(priv)->lib->
			eeprom_ops.regulatory_bands[eep_band - 1];
	switch (eep_band) {
	case 1:		
		*eeprom_ch_count = ARRAY_SIZE(iwl_eeprom_band_1);
		*eeprom_ch_info = (struct iwl_eeprom_channel *)
				iwl_eeprom_query_addr(shrd, offset);
		*eeprom_ch_index = iwl_eeprom_band_1;
		break;
	case 2:		
		*eeprom_ch_count = ARRAY_SIZE(iwl_eeprom_band_2);
		*eeprom_ch_info = (struct iwl_eeprom_channel *)
				iwl_eeprom_query_addr(shrd, offset);
		*eeprom_ch_index = iwl_eeprom_band_2;
		break;
	case 3:		
		*eeprom_ch_count = ARRAY_SIZE(iwl_eeprom_band_3);
		*eeprom_ch_info = (struct iwl_eeprom_channel *)
				iwl_eeprom_query_addr(shrd, offset);
		*eeprom_ch_index = iwl_eeprom_band_3;
		break;
	case 4:		
		*eeprom_ch_count = ARRAY_SIZE(iwl_eeprom_band_4);
		*eeprom_ch_info = (struct iwl_eeprom_channel *)
				iwl_eeprom_query_addr(shrd, offset);
		*eeprom_ch_index = iwl_eeprom_band_4;
		break;
	case 5:		
		*eeprom_ch_count = ARRAY_SIZE(iwl_eeprom_band_5);
		*eeprom_ch_info = (struct iwl_eeprom_channel *)
				iwl_eeprom_query_addr(shrd, offset);
		*eeprom_ch_index = iwl_eeprom_band_5;
		break;
	case 6:		
		*eeprom_ch_count = ARRAY_SIZE(iwl_eeprom_band_6);
		*eeprom_ch_info = (struct iwl_eeprom_channel *)
				iwl_eeprom_query_addr(shrd, offset);
		*eeprom_ch_index = iwl_eeprom_band_6;
		break;
	case 7:		
		*eeprom_ch_count = ARRAY_SIZE(iwl_eeprom_band_7);
		*eeprom_ch_info = (struct iwl_eeprom_channel *)
				iwl_eeprom_query_addr(shrd, offset);
		*eeprom_ch_index = iwl_eeprom_band_7;
		break;
	default:
		BUG();
		return;
	}
}

#define CHECK_AND_PRINT(x) ((eeprom_ch->flags & EEPROM_CHANNEL_##x) \
			    ? # x " " : "")
static int iwl_mod_ht40_chan_info(struct iwl_priv *priv,
			      enum ieee80211_band band, u16 channel,
			      const struct iwl_eeprom_channel *eeprom_ch,
			      u8 clear_ht40_extension_channel)
{
	struct iwl_channel_info *ch_info;

	ch_info = (struct iwl_channel_info *)
			iwl_get_channel_info(priv, band, channel);

	if (!is_channel_valid(ch_info))
		return -1;

	IWL_DEBUG_EEPROM(priv, "HT40 Ch. %d [%sGHz] %s%s%s%s%s(0x%02x %ddBm):"
			" Ad-Hoc %ssupported\n",
			ch_info->channel,
			is_channel_a_band(ch_info) ?
			"5.2" : "2.4",
			CHECK_AND_PRINT(IBSS),
			CHECK_AND_PRINT(ACTIVE),
			CHECK_AND_PRINT(RADAR),
			CHECK_AND_PRINT(WIDE),
			CHECK_AND_PRINT(DFS),
			eeprom_ch->flags,
			eeprom_ch->max_power_avg,
			((eeprom_ch->flags & EEPROM_CHANNEL_IBSS)
			 && !(eeprom_ch->flags & EEPROM_CHANNEL_RADAR)) ?
			"" : "not ");

	ch_info->ht40_eeprom = *eeprom_ch;
	ch_info->ht40_max_power_avg = eeprom_ch->max_power_avg;
	ch_info->ht40_flags = eeprom_ch->flags;
	if (eeprom_ch->flags & EEPROM_CHANNEL_VALID)
		ch_info->ht40_extension_channel &= ~clear_ht40_extension_channel;

	return 0;
}

#define CHECK_AND_PRINT_I(x) ((eeprom_ch_info[ch].flags & EEPROM_CHANNEL_##x) \
			    ? # x " " : "")

int iwl_init_channel_map(struct iwl_priv *priv)
{
	int eeprom_ch_count = 0;
	const u8 *eeprom_ch_index = NULL;
	const struct iwl_eeprom_channel *eeprom_ch_info = NULL;
	int band, ch;
	struct iwl_channel_info *ch_info;

	if (priv->channel_count) {
		IWL_DEBUG_EEPROM(priv, "Channel map already initialized.\n");
		return 0;
	}

	IWL_DEBUG_EEPROM(priv, "Initializing regulatory info from EEPROM\n");

	priv->channel_count =
	    ARRAY_SIZE(iwl_eeprom_band_1) +
	    ARRAY_SIZE(iwl_eeprom_band_2) +
	    ARRAY_SIZE(iwl_eeprom_band_3) +
	    ARRAY_SIZE(iwl_eeprom_band_4) +
	    ARRAY_SIZE(iwl_eeprom_band_5);

	IWL_DEBUG_EEPROM(priv, "Parsing data for %d channels.\n",
			priv->channel_count);

	priv->channel_info = kcalloc(priv->channel_count,
				     sizeof(struct iwl_channel_info),
				     GFP_KERNEL);
	if (!priv->channel_info) {
		IWL_ERR(priv, "Could not allocate channel_info\n");
		priv->channel_count = 0;
		return -ENOMEM;
	}

	ch_info = priv->channel_info;

	for (band = 1; band <= 5; band++) {

		iwl_init_band_reference(priv, band, &eeprom_ch_count,
					&eeprom_ch_info, &eeprom_ch_index);

		
		for (ch = 0; ch < eeprom_ch_count; ch++) {
			ch_info->channel = eeprom_ch_index[ch];
			ch_info->band = (band == 1) ? IEEE80211_BAND_2GHZ :
			    IEEE80211_BAND_5GHZ;

			ch_info->eeprom = eeprom_ch_info[ch];

			ch_info->flags = eeprom_ch_info[ch].flags;
			ch_info->ht40_extension_channel =
					IEEE80211_CHAN_NO_HT40;

			if (!(is_channel_valid(ch_info))) {
				IWL_DEBUG_EEPROM(priv,
					       "Ch. %d Flags %x [%sGHz] - "
					       "No traffic\n",
					       ch_info->channel,
					       ch_info->flags,
					       is_channel_a_band(ch_info) ?
					       "5.2" : "2.4");
				ch_info++;
				continue;
			}

			
			ch_info->max_power_avg = ch_info->curr_txpow =
			    eeprom_ch_info[ch].max_power_avg;
			ch_info->scan_power = eeprom_ch_info[ch].max_power_avg;
			ch_info->min_power = 0;

			IWL_DEBUG_EEPROM(priv, "Ch. %d [%sGHz] "
				       "%s%s%s%s%s%s(0x%02x %ddBm):"
				       " Ad-Hoc %ssupported\n",
				       ch_info->channel,
				       is_channel_a_band(ch_info) ?
				       "5.2" : "2.4",
				       CHECK_AND_PRINT_I(VALID),
				       CHECK_AND_PRINT_I(IBSS),
				       CHECK_AND_PRINT_I(ACTIVE),
				       CHECK_AND_PRINT_I(RADAR),
				       CHECK_AND_PRINT_I(WIDE),
				       CHECK_AND_PRINT_I(DFS),
				       eeprom_ch_info[ch].flags,
				       eeprom_ch_info[ch].max_power_avg,
				       ((eeprom_ch_info[ch].
					 flags & EEPROM_CHANNEL_IBSS)
					&& !(eeprom_ch_info[ch].
					     flags & EEPROM_CHANNEL_RADAR))
				       ? "" : "not ");

			ch_info++;
		}
	}

	
	if (cfg(priv)->lib->eeprom_ops.regulatory_bands[5] ==
	    EEPROM_REGULATORY_BAND_NO_HT40 &&
	    cfg(priv)->lib->eeprom_ops.regulatory_bands[6] ==
	    EEPROM_REGULATORY_BAND_NO_HT40)
		return 0;

	
	for (band = 6; band <= 7; band++) {
		enum ieee80211_band ieeeband;

		iwl_init_band_reference(priv, band, &eeprom_ch_count,
					&eeprom_ch_info, &eeprom_ch_index);

		
		ieeeband =
			(band == 6) ? IEEE80211_BAND_2GHZ : IEEE80211_BAND_5GHZ;

		
		for (ch = 0; ch < eeprom_ch_count; ch++) {
			
			iwl_mod_ht40_chan_info(priv, ieeeband,
						eeprom_ch_index[ch],
						&eeprom_ch_info[ch],
						IEEE80211_CHAN_NO_HT40PLUS);

			
			iwl_mod_ht40_chan_info(priv, ieeeband,
						eeprom_ch_index[ch] + 4,
						&eeprom_ch_info[ch],
						IEEE80211_CHAN_NO_HT40MINUS);
		}
	}

	if (cfg(priv)->lib->eeprom_ops.enhanced_txpower)
		iwl_eeprom_enhanced_txpower(priv);

	return 0;
}

void iwl_free_channel_map(struct iwl_priv *priv)
{
	kfree(priv->channel_info);
	priv->channel_count = 0;
}

const struct iwl_channel_info *iwl_get_channel_info(const struct iwl_priv *priv,
					enum ieee80211_band band, u16 channel)
{
	int i;

	switch (band) {
	case IEEE80211_BAND_5GHZ:
		for (i = 14; i < priv->channel_count; i++) {
			if (priv->channel_info[i].channel == channel)
				return &priv->channel_info[i];
		}
		break;
	case IEEE80211_BAND_2GHZ:
		if (channel >= 1 && channel <= 14)
			return &priv->channel_info[channel - 1];
		break;
	default:
		BUG();
	}

	return NULL;
}

void iwl_rf_config(struct iwl_priv *priv)
{
	u16 radio_cfg;

	radio_cfg = iwl_eeprom_query16(priv->shrd, EEPROM_RADIO_CONFIG);

	
	if (EEPROM_RF_CFG_TYPE_MSK(radio_cfg) <= EEPROM_RF_CONFIG_TYPE_MAX) {
		iwl_set_bit(trans(priv), CSR_HW_IF_CONFIG_REG,
			    EEPROM_RF_CFG_TYPE_MSK(radio_cfg) |
			    EEPROM_RF_CFG_STEP_MSK(radio_cfg) |
			    EEPROM_RF_CFG_DASH_MSK(radio_cfg));
		IWL_INFO(priv, "Radio type=0x%x-0x%x-0x%x\n",
			 EEPROM_RF_CFG_TYPE_MSK(radio_cfg),
			 EEPROM_RF_CFG_STEP_MSK(radio_cfg),
			 EEPROM_RF_CFG_DASH_MSK(radio_cfg));
	} else
		WARN_ON(1);

	
	iwl_set_bit(trans(priv), CSR_HW_IF_CONFIG_REG,
		    CSR_HW_IF_CONFIG_REG_BIT_RADIO_SI |
		    CSR_HW_IF_CONFIG_REG_BIT_MAC_SI);
}
