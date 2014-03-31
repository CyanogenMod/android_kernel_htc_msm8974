/******************************************************************************
 *
 * Copyright(c) 2007 - 2010 Realtek Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 * Modifications for inclusion into the Linux staging tree are
 * Copyright(c) 2010 Larry Finger. All rights reserved.
 *
 * Contact information:
 * WLAN FAE <wlanfae@realtek.com>
 * Larry Finger <Larry.Finger@lwfinger.net>
 *
 ******************************************************************************/
#ifndef _RTL871X_MP_IOCTL_H
#define _RTL871X_MP_IOCTL_H

#include "osdep_service.h"
#include "drv_types.h"
#include "mp_custom_oid.h"
#include "rtl871x_ioctl.h"
#include "rtl871x_ioctl_rtl.h"
#include "rtl8712_efuse.h"

#define TESTFWCMDNUMBER			1000000
#define TEST_H2CINT_WAIT_TIME		500
#define TEST_C2HINT_WAIT_TIME		500
#define HCI_TEST_SYSCFG_HWMASK		1
#define _BUSCLK_40M			(4 << 2)

struct CFG_DBG_MSG_STRUCT {
	u32 DebugLevel;
	u32 DebugComponent_H32;
	u32 DebugComponent_L32;
};

struct mp_rw_reg {
	uint offset;
	uint width;
	u32 value;
};

struct eeprom_rw_param {
	uint offset;
	u16 value;
};

struct EFUSE_ACCESS_STRUCT {
	u16	start_addr;
	u16	cnts;
	u8	data[0];
};

struct burst_rw_reg {
	uint offset;
	uint len;
	u8 Data[256];
};

struct usb_vendor_req {
	u8	bRequest;
	u16	wValue;
	u16	wIndex;
	u16	wLength;
	u8	u8Dir;
	u8	u8InData;
};

struct DR_VARIABLE_STRUCT {
	u8 offset;
	u32 variable;
};

int mp_start_joinbss(struct _adapter *padapter, struct ndis_802_11_ssid *pssid);

uint oid_rt_pro8711_join_bss_hdl(struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_read_register_hdl(struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_write_register_hdl(struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_burst_read_register_hdl(struct oid_par_priv*
					       poid_par_priv);
uint oid_rt_pro_burst_write_register_hdl(struct oid_par_priv*
						poid_par_priv);
uint oid_rt_pro_write_txcmd_hdl(struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_read16_eeprom_hdl(struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_write16_eeprom_hdl(struct oid_par_priv *poid_par_priv);
uint oid_rt_pro8711_wi_poll_hdl(struct oid_par_priv *poid_par_priv);
uint oid_rt_pro8711_pkt_loss_hdl(struct oid_par_priv *poid_par_priv);
uint oid_rt_rd_attrib_mem_hdl(struct oid_par_priv *poid_par_priv);
uint oid_rt_wr_attrib_mem_hdl(struct oid_par_priv *poid_par_priv);
uint  oid_rt_pro_set_rf_intfs_hdl(struct oid_par_priv *poid_par_priv);
uint oid_rt_poll_rx_status_hdl(struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_cfg_debug_message_hdl(
				struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_set_data_rate_ex_hdl(
				struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_set_basic_rate_hdl(
				struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_set_power_tracking_hdl(
				struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_qry_pwrstate_hdl(
				struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_set_pwrstate_hdl(
				struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_h2c_set_rate_table_hdl(
				struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_h2c_get_rate_table_hdl(
				struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_set_data_rate_hdl(
				struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_start_test_hdl(struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_stop_test_hdl(struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_set_channel_direct_call_hdl(
				struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_set_antenna_bb_hdl(
				struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_set_tx_power_control_hdl(
				struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_query_tx_packet_sent_hdl(
				struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_query_rx_packet_received_hdl(
				struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_query_rx_packet_crc32_error_hdl(
				struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_reset_tx_packet_sent_hdl(
				struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_reset_rx_packet_received_hdl(
				struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_set_modulation_hdl(struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_set_continuous_tx_hdl(
				struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_set_single_carrier_tx_hdl(
				struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_set_carrier_suppression_tx_hdl(
				struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_set_single_tone_tx_hdl(
				struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_write_bb_reg_hdl(struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_read_bb_reg_hdl(struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_write_rf_reg_hdl(struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_read_rf_reg_hdl(struct oid_par_priv *poid_par_priv);
uint oid_rt_wireless_mode_hdl(struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_encryption_ctrl_hdl(struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_add_sta_info_hdl(struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_dele_sta_info_hdl(struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_query_dr_variable_hdl(
				struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_rx_packet_type_hdl(struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_read_efuse_hdl(struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_write_efuse_hdl(struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_rw_efuse_pgpkt_hdl(struct oid_par_priv *poid_par_priv);
uint oid_rt_get_efuse_current_size_hdl(
				struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_efuse_hdl(struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_efuse_map_hdl(struct oid_par_priv *poid_par_priv);
uint oid_rt_set_bandwidth_hdl(struct oid_par_priv *poid_par_priv);
uint oid_rt_set_crystal_cap_hdl(struct oid_par_priv *poid_par_priv);
uint oid_rt_set_rx_packet_type_hdl(struct oid_par_priv *poid_par_priv);
uint oid_rt_get_efuse_max_size_hdl(struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_set_tx_agc_offset_hdl(
				struct oid_par_priv *poid_par_priv);
uint oid_rt_pro_set_pkt_test_mode_hdl(
				struct oid_par_priv *poid_par_priv);
uint oid_rt_get_thermal_meter_hdl(
				struct oid_par_priv *poid_par_priv);
uint oid_rt_reset_phy_rx_packet_count_hdl(
				struct oid_par_priv *poid_par_priv);
uint oid_rt_get_phy_rx_packet_received_hdl(
				struct oid_par_priv *poid_par_priv);
uint oid_rt_get_phy_rx_packet_crc32_error_hdl(
				struct oid_par_priv *poid_par_priv);
uint oid_rt_set_power_down_hdl(
				struct oid_par_priv *poid_par_priv);
uint oid_rt_get_power_mode_hdl(
				struct oid_par_priv *poid_par_priv);
#ifdef _RTL871X_MP_IOCTL_C_ 
static const struct oid_obj_priv oid_rtl_seg_81_80_00[] = {
	{1, &oid_null_function},	
	{1, &oid_rt_pro_set_data_rate_hdl},	
	{1, &oid_rt_pro_start_test_hdl},
	{1, &oid_rt_pro_stop_test_hdl},	
	{1, &oid_null_function},	
	{1, &oid_null_function},	
	{1, &oid_null_function},	
	{1, &oid_null_function},	
	{1, &oid_rt_pro_set_channel_direct_call_hdl},	
	{1, &oid_null_function},	
	{1, &oid_null_function},	
	{1, &oid_rt_pro_set_continuous_tx_hdl},	
	{1, &oid_rt_pro_set_single_carrier_tx_hdl}, 
	{1, &oid_null_function},	
	{1, &oid_rt_pro_set_antenna_bb_hdl},		
	{1, &oid_null_function},	
	{1, &oid_null_function},	
	{1, &oid_rt_pro_set_tx_power_control_hdl}, 
	{1, &oid_null_function},	
	{1, &oid_null_function},	
	{1, &oid_null_function},	
	{1, &oid_null_function},	
	{1, &oid_null_function},	
	{1, &oid_null_function},	
	{1, &oid_null_function},	
	{1, &oid_null_function},	
	{1, &oid_null_function},	
	{1, &oid_null_function},	
	{1, &oid_null_function},	
	{1, &oid_null_function},	
	{1, &oid_null_function},	
	{1, &oid_null_function}		
};

static const struct oid_obj_priv oid_rtl_seg_81_80_20[] = {
	{1, &oid_null_function},	
	{1, &oid_null_function},	
	{1, &oid_null_function},	
	{1, &oid_rt_pro_reset_tx_packet_sent_hdl},	
	{1, &oid_rt_pro_query_tx_packet_sent_hdl},	
	{1, &oid_rt_pro_reset_rx_packet_received_hdl},	
	{1, &oid_rt_pro_query_rx_packet_received_hdl},	
	{1, &oid_rt_pro_query_rx_packet_crc32_error_hdl},
	{1, &oid_null_function},	
	{1, &oid_null_function},	
	{1, &oid_null_function},	
	{1, &oid_rt_pro_set_carrier_suppression_tx_hdl},
	{1, &oid_null_function},	
	{1, &oid_null_function},	
	{1, &oid_null_function},	
	{1, &oid_rt_pro_set_modulation_hdl}		
};

static const struct oid_obj_priv oid_rtl_seg_81_80_40[] = {
	{1, &oid_null_function},			
	{1, &oid_null_function},			
	{1, &oid_null_function},			
	{1, &oid_rt_pro_set_single_tone_tx_hdl},	
	{1, &oid_null_function},			
	{1, &oid_null_function}				
};

static const struct oid_obj_priv oid_rtl_seg_81_80_80[] = {
	{1, &oid_null_function},	
	{1, &oid_null_function},	
	{1, &oid_null_function}		

};

static const struct oid_obj_priv oid_rtl_seg_81_85[] = {
	{1, &oid_rt_wireless_mode_hdl}	
};

#else 
extern struct oid_obj_priv oid_rtl_seg_81_80_00[32];
extern struct oid_obj_priv oid_rtl_seg_81_80_20[16];
extern struct oid_obj_priv oid_rtl_seg_81_80_40[6];
extern struct oid_obj_priv oid_rtl_seg_81_80_80[3];
extern struct oid_obj_priv oid_rtl_seg_81_85[1];
extern struct oid_obj_priv oid_rtl_seg_81_87[5];
extern struct oid_obj_priv oid_rtl_seg_87_11_00[32];
extern struct oid_obj_priv oid_rtl_seg_87_11_20[5];
extern struct oid_obj_priv oid_rtl_seg_87_11_50[2];
extern struct oid_obj_priv oid_rtl_seg_87_11_80[1];
extern struct oid_obj_priv oid_rtl_seg_87_11_B0[1];
extern struct oid_obj_priv oid_rtl_seg_87_11_F0[16];
extern struct oid_obj_priv oid_rtl_seg_87_12_00[32];

#endif 


enum MP_MODE {
	MP_START_MODE,
	MP_STOP_MODE,
	MP_ERR_MODE
};

struct rwreg_param {
	unsigned int offset;
	unsigned int width;
	unsigned int value;
};

struct bbreg_param {
	unsigned int offset;
	unsigned int phymask;
	unsigned int value;
};

struct txpower_param {
	unsigned int pwr_index;
};

struct datarate_param {
	unsigned int rate_index;
};

struct rfintfs_parm {
	unsigned int rfintfs;
};

struct mp_xmit_packet {
	unsigned int len;
};

struct psmode_param {
	unsigned int ps_mode;
	unsigned int smart_ps;
};

struct mp_ioctl_handler {
	unsigned int paramsize;
	unsigned int (*handler)(struct oid_par_priv *poid_par_priv);
	unsigned int oid;
};

struct mp_ioctl_param {
	unsigned int subcode;
	unsigned int len;
	unsigned char data[0];
};

#define GEN_MP_IOCTL_SUBCODE(code) _MP_IOCTL_ ## code ## _CMD_

enum RTL871X_MP_IOCTL_SUBCODE {
	GEN_MP_IOCTL_SUBCODE(MP_START),			
	GEN_MP_IOCTL_SUBCODE(MP_STOP),			
	GEN_MP_IOCTL_SUBCODE(READ_REG),			
	GEN_MP_IOCTL_SUBCODE(WRITE_REG),
	GEN_MP_IOCTL_SUBCODE(SET_CHANNEL),		
	GEN_MP_IOCTL_SUBCODE(SET_TXPOWER),		
	GEN_MP_IOCTL_SUBCODE(SET_DATARATE),		
	GEN_MP_IOCTL_SUBCODE(READ_BB_REG),		
	GEN_MP_IOCTL_SUBCODE(WRITE_BB_REG),
	GEN_MP_IOCTL_SUBCODE(READ_RF_REG),		
	GEN_MP_IOCTL_SUBCODE(WRITE_RF_REG),
	GEN_MP_IOCTL_SUBCODE(SET_RF_INTFS),
	GEN_MP_IOCTL_SUBCODE(IOCTL_XMIT_PACKET),	
	GEN_MP_IOCTL_SUBCODE(PS_STATE),			
	GEN_MP_IOCTL_SUBCODE(READ16_EEPROM),		
	GEN_MP_IOCTL_SUBCODE(WRITE16_EEPROM),		
	GEN_MP_IOCTL_SUBCODE(SET_PTM),			
	GEN_MP_IOCTL_SUBCODE(READ_TSSI),		
	GEN_MP_IOCTL_SUBCODE(CNTU_TX),			
	GEN_MP_IOCTL_SUBCODE(SET_BANDWIDTH),		
	GEN_MP_IOCTL_SUBCODE(SET_RX_PKT_TYPE),		
	GEN_MP_IOCTL_SUBCODE(RESET_PHY_RX_PKT_CNT),	
	GEN_MP_IOCTL_SUBCODE(GET_PHY_RX_PKT_RECV),	
	GEN_MP_IOCTL_SUBCODE(GET_PHY_RX_PKT_ERROR),	
	GEN_MP_IOCTL_SUBCODE(SET_POWER_DOWN),		
	GEN_MP_IOCTL_SUBCODE(GET_THERMAL_METER),	
	GEN_MP_IOCTL_SUBCODE(GET_POWER_MODE),		
	GEN_MP_IOCTL_SUBCODE(EFUSE),			
	GEN_MP_IOCTL_SUBCODE(EFUSE_MAP),		
	GEN_MP_IOCTL_SUBCODE(GET_EFUSE_MAX_SIZE),	
	GEN_MP_IOCTL_SUBCODE(GET_EFUSE_CURRENT_SIZE),	
	GEN_MP_IOCTL_SUBCODE(SC_TX),			
	GEN_MP_IOCTL_SUBCODE(CS_TX),			
	GEN_MP_IOCTL_SUBCODE(ST_TX),			
	GEN_MP_IOCTL_SUBCODE(SET_ANTENNA),		
	MAX_MP_IOCTL_SUBCODE,
};

unsigned int mp_ioctl_xmit_packet_hdl(struct oid_par_priv *poid_par_priv);

#ifdef _RTL871X_MP_IOCTL_C_ 

static struct mp_ioctl_handler mp_ioctl_hdl[] = {
	{sizeof(u32), oid_rt_pro_start_test_hdl,
			     OID_RT_PRO_START_TEST},
	{sizeof(u32), oid_rt_pro_stop_test_hdl,
			     OID_RT_PRO_STOP_TEST},
	{sizeof(struct rwreg_param),
			     oid_rt_pro_read_register_hdl,
			     OID_RT_PRO_READ_REGISTER},
	{sizeof(struct rwreg_param),
			     oid_rt_pro_write_register_hdl,
			     OID_RT_PRO_WRITE_REGISTER},
	{sizeof(u32),
			     oid_rt_pro_set_channel_direct_call_hdl,
			     OID_RT_PRO_SET_CHANNEL_DIRECT_CALL},
	{sizeof(struct txpower_param),
			     oid_rt_pro_set_tx_power_control_hdl,
			     OID_RT_PRO_SET_TX_POWER_CONTROL},
	{sizeof(u32),
			     oid_rt_pro_set_data_rate_hdl,
			     OID_RT_PRO_SET_DATA_RATE},
	{sizeof(struct bb_reg_param),
			     oid_rt_pro_read_bb_reg_hdl,
			     OID_RT_PRO_READ_BB_REG},
	{sizeof(struct bb_reg_param),
			     oid_rt_pro_write_bb_reg_hdl,
			     OID_RT_PRO_WRITE_BB_REG},
	{sizeof(struct rwreg_param),
			     oid_rt_pro_read_rf_reg_hdl,
			     OID_RT_PRO_RF_READ_REGISTRY},
	{sizeof(struct rwreg_param),
			     oid_rt_pro_write_rf_reg_hdl,
			     OID_RT_PRO_RF_WRITE_REGISTRY},
	{sizeof(struct rfintfs_parm), NULL, 0},
	{0, &mp_ioctl_xmit_packet_hdl, 0},
	{sizeof(struct psmode_param), NULL, 0},
	{sizeof(struct eeprom_rw_param), NULL, 0},
	{sizeof(struct eeprom_rw_param), NULL, 0},
	{sizeof(unsigned char), NULL, 0},
	{sizeof(u32), NULL, 0},
	{sizeof(u32), oid_rt_pro_set_continuous_tx_hdl,
			     OID_RT_PRO_SET_CONTINUOUS_TX},
	{sizeof(u32), oid_rt_set_bandwidth_hdl,
			     OID_RT_SET_BANDWIDTH},
	{sizeof(u32), oid_rt_set_rx_packet_type_hdl,
			     OID_RT_SET_RX_PACKET_TYPE},
	{0, oid_rt_reset_phy_rx_packet_count_hdl,
			     OID_RT_RESET_PHY_RX_PACKET_COUNT},
	{sizeof(u32), oid_rt_get_phy_rx_packet_received_hdl,
			     OID_RT_GET_PHY_RX_PACKET_RECEIVED},
	{sizeof(u32), oid_rt_get_phy_rx_packet_crc32_error_hdl,
			     OID_RT_GET_PHY_RX_PACKET_CRC32_ERROR},
	{sizeof(unsigned char), oid_rt_set_power_down_hdl,
			     OID_RT_SET_POWER_DOWN},
	{sizeof(u32), oid_rt_get_thermal_meter_hdl,
			     OID_RT_PRO_GET_THERMAL_METER},
	{sizeof(u32), oid_rt_get_power_mode_hdl,
			     OID_RT_GET_POWER_MODE},
	{sizeof(struct EFUSE_ACCESS_STRUCT),
			     oid_rt_pro_efuse_hdl, OID_RT_PRO_EFUSE},
	{EFUSE_MAP_MAX_SIZE, oid_rt_pro_efuse_map_hdl,
			     OID_RT_PRO_EFUSE_MAP},
	{sizeof(u32), oid_rt_get_efuse_max_size_hdl,
			     OID_RT_GET_EFUSE_MAX_SIZE},
	{sizeof(u32), oid_rt_get_efuse_current_size_hdl,
			     OID_RT_GET_EFUSE_CURRENT_SIZE},
	{sizeof(u32), oid_rt_pro_set_single_carrier_tx_hdl,
			     OID_RT_PRO_SET_SINGLE_CARRIER_TX},
	{sizeof(u32), oid_rt_pro_set_carrier_suppression_tx_hdl,
			     OID_RT_PRO_SET_CARRIER_SUPPRESSION_TX},
	{sizeof(u32), oid_rt_pro_set_single_tone_tx_hdl,
			     OID_RT_PRO_SET_SINGLE_TONE_TX},
	{sizeof(u32), oid_rt_pro_set_antenna_bb_hdl,
			     OID_RT_PRO_SET_ANTENNA_BB},
};

#else 
extern struct mp_ioctl_handler mp_ioctl_hdl[];
#endif 

#endif

