/*
 * sme_api.h
 *
 * Copyright(C) 2002 Winbond Electronics Corp.
 */

#ifndef __SME_API_H__
#define __SME_API_H__

#include <linux/types.h>

#include "localpara.h"

#define _INLINE				__inline

#define MEDIA_STATE_DISCONNECTED	0
#define MEDIA_STATE_CONNECTED		1

#define MAX_POWER_TO_DB			32



s8 sme_get_bssid(void *pcore_data, u8 *pbssid);
s8 sme_get_desired_bssid(void *pcore_data, u8 *pbssid); 
s8 sme_set_desired_bssid(void *pcore_data, u8 *pbssid);

s8 sme_get_ssid(void *pcore_data, u8 *pssid, u8 *pssid_len);
s8 sme_get_desired_ssid(void *pcore_data, u8 *pssid, u8 *pssid_len);
s8 sme_set_desired_ssid(void *pcore_data, u8 *pssid, u8 ssid_len);

s8 sme_get_bss_type(void *pcore_data, u8 *pbss_type);
s8 sme_get_desired_bss_type(void *pcore_data, u8 *pbss_type); 
s8 sme_set_desired_bss_type(void *pcore_data, u8 bss_type);

s8 sme_get_fragment_threshold(void *pcore_data, u32 *pthreshold);
s8 sme_set_fragment_threshold(void *pcore_data, u32 threshold);

s8 sme_get_rts_threshold(void *pcore_data, u32 *pthreshold);
s8 sme_set_rts_threshold(void *pcore_data, u32 threshold);

s8 sme_get_beacon_period(void *pcore_data, u16 *pbeacon_period);
s8 sme_set_beacon_period(void *pcore_data, u16 beacon_period);

s8 sme_get_atim_window(void *pcore_data, u16 *patim_window);
s8 sme_set_atim_window(void *pcore_data, u16 atim_window);

s8 sme_get_current_channel(void *pcore_data, u8 *pcurrent_channel);
s8 sme_get_current_band(void *pcore_data, u8 *pcurrent_band);
s8 sme_set_current_channel(void *pcore_data, u8 current_channel);

s8 sme_get_scan_bss_count(void *pcore_data, u8 *pcount);
s8 sme_get_scan_bss(void *pcore_data, u8 index, void **ppbss);

s8 sme_get_connected_bss(void *pcore_data, void **ppbss_now);

s8 sme_get_auth_mode(void *pcore_data, u8 *pauth_mode);
s8 sme_set_auth_mode(void *pcore_data, u8 auth_mode);

s8 sme_get_wep_mode(void *pcore_data, u8 *pwep_mode);
s8 sme_set_wep_mode(void *pcore_data, u8 wep_mode);

s8 sme_get_permanent_mac_addr(void *pcore_data, u8 *pmac_addr);

s8 sme_get_current_mac_addr(void *pcore_data, u8 *pmac_addr);

s8 sme_get_network_type_in_use(void *pcore_data, u8 *ptype);
s8 sme_set_network_type_in_use(void *pcore_data, u8 type);

s8 sme_get_supported_rate(void *pcore_data, u8 *prates);

s8 sme_set_add_wep(void *pcore_data, u32 key_index, u32 key_len,
					 u8 *Address, u8 *key);

s8 sme_set_remove_wep(void *pcre_data, u32 key_index);

s8 sme_set_disassociate(void *pcore_data);

s8 sme_get_power_mode(void *pcore_data, u8 *pmode);
s8 sme_set_power_mode(void *pcore_data, u8 mode);

s8 sme_set_bssid_list_scan(void *pcore_data, void *pscan_para);

s8 sme_set_reload_defaults(void *pcore_data, u8 reload_type);


s8 sme_get_connect_status(void *pcore_data, u8 *pstatus);

void sme_get_encryption_status(void *pcore_data, u8 *EncryptStatus);
void sme_set_encryption_status(void *pcore_data, u8 EncryptStatus);
s8 sme_add_key(void	*pcore_data,
		u32	key_index,
		u8	key_len,
		u8	key_type,
		u8	*key_bssid,
		u8	*ptx_tsc,
		u8	*prx_tsc,
		u8	*key_material);
void sme_remove_default_key(void *pcore_data, int index);
void sme_remove_mapping_key(void *pcore_data, u8 *pmac_addr);
void sme_clear_all_mapping_key(void *pcore_data);
void sme_clear_all_default_key(void *pcore_data);



s8 sme_set_preamble_mode(void *pcore_data, u8 mode);
s8 sme_get_preamble_mode(void *pcore_data, u8 *mode);
s8 sme_get_preamble_type(void *pcore_data, u8 *type);
s8 sme_set_slottime_mode(void *pcore_data, u8 mode);
s8 sme_get_slottime_mode(void *pcore_data, u8 *mode);
s8 sme_get_slottime_type(void *pcore_data, u8 *type);
s8 sme_set_txrate_policy(void *pcore_data, u8 policy);
s8 sme_get_txrate_policy(void *pcore_data, u8 *policy);
s8 sme_get_cwmin_value(void *pcore_data, u8 *cwmin);
s8 sme_get_cwmax_value(void *pcore_data, u16 *cwmax);
s8 sme_get_ms_radio_mode(void *pcore_data, u8 * pMsRadioOff);
s8 sme_set_ms_radio_mode(void *pcore_data, u8 boMsRadioOff);

void sme_get_tx_power_level(void *pcore_data, u32 *TxPower);
u8 sme_set_tx_power_level(void *pcore_data, u32 TxPower);
void sme_get_antenna_count(void *pcore_data, u32 *AntennaCount);
void sme_get_rx_antenna(void *pcore_data, u32 *RxAntenna);
u8 sme_set_rx_antenna(void *pcore_data, u32 RxAntenna);
void sme_get_tx_antenna(void *pcore_data, u32 *TxAntenna);
s8 sme_set_tx_antenna(void *pcore_data, u32 TxAntenna);
s8 sme_set_IBSS_chan(void *pcore_data, struct chan_info chan);
s8 sme_set_IE_append(void *pcore_data, u8 *buffer, u16 buf_len);

static const u32 PowerDbToMw[] = {
	56,	
	40,	
	30,	
	20,	
	15,	
	12,	
	9,	
	7,	
	5,	
	4,	
	3,	
	2,	
	2,	
	2,	
	2,	
	2,	
	2,	
	2,	
	2,	
	1,	
	1,	
	1,	
	1,	
	1,	
	1,	
	1,	
	1,	
	1,	
	1,	
	1,	
	1,	
	1	
};

#endif 


