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

#ifndef __iwl_eeprom_h__
#define __iwl_eeprom_h__

#include <net/mac80211.h>

struct iwl_priv;
struct iwl_shared;
struct iwl_trans;

#define IWL_EEPROM_ACCESS_TIMEOUT	5000 

#define IWL_EEPROM_SEM_TIMEOUT 		10   
#define IWL_EEPROM_SEM_RETRY_LIMIT	1000 


#define IWL_NUM_TX_CALIB_GROUPS 5
enum {
	EEPROM_CHANNEL_VALID = (1 << 0),	
	EEPROM_CHANNEL_IBSS = (1 << 1),		
	
	EEPROM_CHANNEL_ACTIVE = (1 << 3),	
	EEPROM_CHANNEL_RADAR = (1 << 4),	
	EEPROM_CHANNEL_WIDE = (1 << 5),		
	
	EEPROM_CHANNEL_DFS = (1 << 7),	
};

#define EEPROM_SKU_CAP_BAND_24GHZ			(1 << 4)
#define EEPROM_SKU_CAP_BAND_52GHZ			(1 << 5)
#define EEPROM_SKU_CAP_11N_ENABLE	                (1 << 6)
#define EEPROM_SKU_CAP_AMT_ENABLE			(1 << 7)
#define EEPROM_SKU_CAP_IPAN_ENABLE	                (1 << 8)

struct iwl_eeprom_channel {
	u8 flags;		
	s8 max_power_avg;	
} __packed;

enum iwl_eeprom_enhanced_txpwr_flags {
	IWL_EEPROM_ENH_TXP_FL_VALID		= BIT(0),
	IWL_EEPROM_ENH_TXP_FL_BAND_52G		= BIT(1),
	IWL_EEPROM_ENH_TXP_FL_OFDM		= BIT(2),
	IWL_EEPROM_ENH_TXP_FL_40MHZ		= BIT(3),
	IWL_EEPROM_ENH_TXP_FL_HT_AP		= BIT(4),
	IWL_EEPROM_ENH_TXP_FL_RES1		= BIT(5),
	IWL_EEPROM_ENH_TXP_FL_RES2		= BIT(6),
	IWL_EEPROM_ENH_TXP_FL_COMMON_TYPE	= BIT(7),
};

struct iwl_eeprom_enhanced_txpwr {
	u8 flags;
	u8 channel;
	s8 chain_a_max;
	s8 chain_b_max;
	s8 chain_c_max;
	u8 delta_20_in_40;
	s8 mimo2_max;
	s8 mimo3_max;
} __packed;

struct iwl_eeprom_calib_hdr {
	u8 version;
	u8 pa_type;
	__le16 voltage;
} __packed;

#define EEPROM_CALIB_ALL	(INDIRECT_ADDRESS | INDIRECT_CALIBRATION)
#define EEPROM_XTAL		((2*0x128) | EEPROM_CALIB_ALL)

#define EEPROM_KELVIN_TEMPERATURE	((2*0x12A) | EEPROM_CALIB_ALL)
#define EEPROM_RAW_TEMPERATURE		((2*0x12B) | EEPROM_CALIB_ALL)


#define EEPROM_LINK_HOST             (2*0x64)
#define EEPROM_LINK_GENERAL          (2*0x65)
#define EEPROM_LINK_REGULATORY       (2*0x66)
#define EEPROM_LINK_CALIBRATION      (2*0x67)
#define EEPROM_LINK_PROCESS_ADJST    (2*0x68)
#define EEPROM_LINK_OTHERS           (2*0x69)
#define EEPROM_LINK_TXP_LIMIT        (2*0x6a)
#define EEPROM_LINK_TXP_LIMIT_SIZE   (2*0x6b)

#define EEPROM_REG_BAND_1_CHANNELS       ((0x08)\
		| INDIRECT_ADDRESS | INDIRECT_REGULATORY)   
#define EEPROM_REG_BAND_2_CHANNELS       ((0x26)\
		| INDIRECT_ADDRESS | INDIRECT_REGULATORY)   
#define EEPROM_REG_BAND_3_CHANNELS       ((0x42)\
		| INDIRECT_ADDRESS | INDIRECT_REGULATORY)   
#define EEPROM_REG_BAND_4_CHANNELS       ((0x5C)\
		| INDIRECT_ADDRESS | INDIRECT_REGULATORY)   
#define EEPROM_REG_BAND_5_CHANNELS       ((0x74)\
		| INDIRECT_ADDRESS | INDIRECT_REGULATORY)   
#define EEPROM_REG_BAND_24_HT40_CHANNELS  ((0x82)\
		| INDIRECT_ADDRESS | INDIRECT_REGULATORY)   
#define EEPROM_REG_BAND_52_HT40_CHANNELS  ((0x92)\
		| INDIRECT_ADDRESS | INDIRECT_REGULATORY)   

#define EEPROM_6000_REG_BAND_24_HT40_CHANNELS  ((0x80)\
		| INDIRECT_ADDRESS | INDIRECT_REGULATORY)   

#define EEPROM_5000_TX_POWER_VERSION    (4)
#define EEPROM_5000_EEPROM_VERSION	(0x11A)

#define EEPROM_5050_TX_POWER_VERSION    (4)
#define EEPROM_5050_EEPROM_VERSION	(0x21E)

#define EEPROM_1000_TX_POWER_VERSION    (4)
#define EEPROM_1000_EEPROM_VERSION	(0x15C)

#define EEPROM_6000_TX_POWER_VERSION    (4)
#define EEPROM_6000_EEPROM_VERSION	(0x423)

#define EEPROM_6050_TX_POWER_VERSION    (4)
#define EEPROM_6050_EEPROM_VERSION	(0x532)

#define EEPROM_6150_TX_POWER_VERSION    (6)
#define EEPROM_6150_EEPROM_VERSION	(0x553)

#define EEPROM_6005_TX_POWER_VERSION    (6)
#define EEPROM_6005_EEPROM_VERSION	(0x709)

#define EEPROM_6030_TX_POWER_VERSION    (6)
#define EEPROM_6030_EEPROM_VERSION	(0x709)

#define EEPROM_2000_TX_POWER_VERSION    (6)
#define EEPROM_2000_EEPROM_VERSION	(0x805)

#define EEPROM_6035_TX_POWER_VERSION    (6)
#define EEPROM_6035_EEPROM_VERSION	(0x753)


#define OTP_LOW_IMAGE_SIZE		(2 * 512 * sizeof(u16)) 
#define OTP_HIGH_IMAGE_SIZE_6x00        (6 * 512 * sizeof(u16)) 
#define OTP_HIGH_IMAGE_SIZE_1000        (0x200 * sizeof(u16)) 
#define OTP_MAX_LL_ITEMS_1000		(3)	
#define OTP_MAX_LL_ITEMS_6x00		(4)	
#define OTP_MAX_LL_ITEMS_6x50		(7)	
#define OTP_MAX_LL_ITEMS_2x00		(4)	

extern const u8 iwl_eeprom_band_1[14];

#define ADDRESS_MSK                 0x0000FFFF
#define INDIRECT_TYPE_MSK           0x000F0000
#define INDIRECT_HOST               0x00010000
#define INDIRECT_GENERAL            0x00020000
#define INDIRECT_REGULATORY         0x00030000
#define INDIRECT_CALIBRATION        0x00040000
#define INDIRECT_PROCESS_ADJST      0x00050000
#define INDIRECT_OTHERS             0x00060000
#define INDIRECT_TXP_LIMIT          0x00070000
#define INDIRECT_TXP_LIMIT_SIZE     0x00080000
#define INDIRECT_ADDRESS            0x00100000

#define EEPROM_DEVICE_ID                    (2*0x08)	
#define EEPROM_SUBSYSTEM_ID		    (2*0x0A)	
#define EEPROM_MAC_ADDRESS                  (2*0x15)	
#define EEPROM_BOARD_REVISION               (2*0x35)	
#define EEPROM_BOARD_PBA_NUMBER             (2*0x3B+1)	
#define EEPROM_VERSION                      (2*0x44)	
#define EEPROM_SKU_CAP                      (2*0x45)	
#define EEPROM_OEM_MODE                     (2*0x46)	
#define EEPROM_RADIO_CONFIG                 (2*0x48)	
#define EEPROM_NUM_MAC_ADDRESS              (2*0x4C)	

#define EEPROM_RF_CFG_TYPE_MSK(x)   (x & 0x3)         
#define EEPROM_RF_CFG_STEP_MSK(x)   ((x >> 2)  & 0x3) 
#define EEPROM_RF_CFG_DASH_MSK(x)   ((x >> 4)  & 0x3) 
#define EEPROM_RF_CFG_PNUM_MSK(x)   ((x >> 6)  & 0x3) 
#define EEPROM_RF_CFG_TX_ANT_MSK(x) ((x >> 8)  & 0xF) 
#define EEPROM_RF_CFG_RX_ANT_MSK(x) ((x >> 12) & 0xF) 

#define EEPROM_RF_CONFIG_TYPE_MAX	0x3

#define EEPROM_REGULATORY_BAND_NO_HT40			(0)

struct iwl_eeprom_ops {
	const u32 regulatory_bands[7];
	bool enhanced_txpower;
};


int iwl_eeprom_init(struct iwl_trans *trans, u32 hw_rev);
void iwl_eeprom_free(struct iwl_shared *shrd);
int  iwl_eeprom_check_version(struct iwl_priv *priv);
int iwl_eeprom_init_hw_params(struct iwl_priv *priv);
const u8 *iwl_eeprom_query_addr(const struct iwl_shared *shrd, size_t offset);
u16 iwl_eeprom_query16(const struct iwl_shared *shrd, size_t offset);
int iwl_init_channel_map(struct iwl_priv *priv);
void iwl_free_channel_map(struct iwl_priv *priv);
const struct iwl_channel_info *iwl_get_channel_info(
		const struct iwl_priv *priv,
		enum ieee80211_band band, u16 channel);
void iwl_rf_config(struct iwl_priv *priv);

#endif  
