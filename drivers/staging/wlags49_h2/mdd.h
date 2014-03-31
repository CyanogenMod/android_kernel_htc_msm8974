
#ifndef MDD_H
#define MDD_H 1

/*************************************************************************************************************
*
* FILE		: mdd.h
*
* DATE		: $Date: 2004/08/05 11:47:10 $   $Revision: 1.6 $
* Original		: 2004/05/25 05:59:37    Revision: 1.57      Tag: hcf7_t20040602_01
* Original		: 2004/05/13 15:31:45    Revision: 1.54      Tag: hcf7_t7_20040513_01
* Original		: 2004/04/15 09:24:41    Revision: 1.47      Tag: hcf7_t7_20040415_01
* Original		: 2004/04/13 14:22:45    Revision: 1.46      Tag: t7_20040413_01
* Original		: 2004/04/01 15:32:55    Revision: 1.42      Tag: t7_20040401_01
* Original		: 2004/03/10 15:39:28    Revision: 1.38      Tag: t20040310_01
* Original		: 2004/03/04 11:03:37    Revision: 1.36      Tag: t20040304_01
* Original		: 2004/03/02 09:27:11    Revision: 1.34      Tag: t20040302_03
* Original		: 2004/02/24 13:00:27    Revision: 1.29      Tag: t20040224_01
* Original		: 2004/02/18 17:13:57    Revision: 1.26      Tag: t20040219_01
*
* AUTHOR	: Nico Valster
*
* DESC		: Definitions and Prototypes for HCF, DHF, MMD and MSF
*
***************************************************************************************************************
*
*
* SOFTWARE LICENSE
*
* This software is provided subject to the following terms and conditions,
* which you should read carefully before using the software.  Using this
* software indicates your acceptance of these terms and conditions.  If you do
* not agree with these terms and conditions, do not use the software.
*
* COPYRIGHT © 1994 - 1995	by AT&T.				All Rights Reserved
* COPYRIGHT © 1996 - 2000 by Lucent Technologies.	All Rights Reserved
* COPYRIGHT © 2001 - 2004	by Agere Systems Inc.	All Rights Reserved
* All rights reserved.
*
* Redistribution and use in source or binary forms, with or without
* modifications, are permitted provided that the following conditions are met:
*
* . Redistributions of source code must retain the above copyright notice, this
*    list of conditions and the following Disclaimer as comments in the code as
*    well as in the documentation and/or other materials provided with the
*    distribution.
*
* . Redistributions in binary form must reproduce the above copyright notice,
*    this list of conditions and the following Disclaimer in the documentation
*    and/or other materials provided with the distribution.
*
* . Neither the name of Agere Systems Inc. nor the names of the contributors
*    may be used to endorse or promote products derived from this software
*    without specific prior written permission.
*
* Disclaimer
*
* THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
* INCLUDING, BUT NOT LIMITED TO, INFRINGEMENT AND THE IMPLIED WARRANTIES OF
* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  ANY
* USE, MODIFICATION OR DISTRIBUTION OF THIS SOFTWARE IS SOLELY AT THE USERS OWN
* RISK. IN NO EVENT SHALL AGERE SYSTEMS INC. OR CONTRIBUTORS BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, INCLUDING, BUT NOT LIMITED TO, CONTRACT, STRICT
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
* OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
* DAMAGE.
*
*
************************************************************************************************************/




#define XX1( name, type1, par1 )	\
typedef struct {				  	\
	hcf_16	len;                	\
	hcf_16	typ;                	\
	type1	par1;               	\
} name##_STRCT;

#define XX2( name, type1, par1, type2, par2 )	\
typedef struct {				   	\
	hcf_16	len;                	\
	hcf_16	typ;                	\
	type1	par1;               	\
	type2	par2;               	\
} name##_STRCT;

#define XX3( name, type1, par1, type2, par2, type3, par3 )	\
typedef struct name##_STRCT {   	\
	hcf_16	len;                	\
	hcf_16	typ;                	\
	type1	par1;               	\
	type2	par2;               	\
	type3	par3;               	\
} name##_STRCT;

#define XX4( name, type1, par1, type2, par2, type3, par3, type4, par4 )	\
typedef struct {				  	\
	hcf_16	len;                	\
	hcf_16	typ;                	\
	type1	par1;               	\
	type2	par2;               	\
	type3	par3;               	\
	type4	par4;               	\
} name##_STRCT;

#define X1( name, par1 )	\
typedef struct name##_STRCT {   	\
	hcf_16	len;                	\
	hcf_16	typ;                	\
	hcf_16	par1;               	\
} name##_STRCT;

#define X2( name, par1, par2 )		\
typedef struct {			    	\
	hcf_16	len;                	\
	hcf_16	typ;                	\
	hcf_16	par1;               	\
	hcf_16	par2;               	\
} name##_STRCT;

#define X3( name, par1, par2, par3 )		\
typedef struct {			    	\
	hcf_16	len;                	\
	hcf_16	typ;                	\
	hcf_16	par1;               	\
	hcf_16	par2;               	\
	hcf_16	par3;               	\
} name##_STRCT;

#define X4( name, par1, par2, par3, par4 )		\
typedef struct {			    	\
	hcf_16	len;                	\
	hcf_16	typ;                	\
	hcf_16	par1;               	\
	hcf_16	par2;               	\
	hcf_16	par3;               	\
	hcf_16	par4;               	\
} name##_STRCT;

#define X5( name, par1, par2, par3, par4, par5 )		\
typedef struct {			    	\
	hcf_16	len;                	\
	hcf_16	typ;                	\
	hcf_16	par1;               	\
	hcf_16	par2;               	\
	hcf_16	par3;               	\
	hcf_16	par4;               	\
	hcf_16	par5;               	\
} name##_STRCT;

#define X6( name, par1, par2, par3, par4, par5, par6 )		\
typedef struct {			    	\
	hcf_16	len;                	\
	hcf_16	typ;                	\
	hcf_16	par1;               	\
	hcf_16	par2;               	\
	hcf_16	par3;               	\
	hcf_16	par4;               	\
	hcf_16	par5;               	\
	hcf_16	par6;               	\
} name##_STRCT;

#define X8( name, par1, par2, par3, par4, par5, par6, par7, par8 )		\
typedef struct {			    	\
	hcf_16	len;                	\
	hcf_16	typ;                	\
	hcf_16	par1;               	\
	hcf_16	par2;               	\
	hcf_16	par3;               	\
	hcf_16	par4;               	\
	hcf_16	par5;               	\
	hcf_16	par6;               	\
	hcf_16	par7;               	\
	hcf_16	par8;               	\
} name##_STRCT;

#define X11( name, par1, par2, par3, par4, par5, par6, par7, par8, par9, par10, par11 )		\
typedef struct {			    	\
	hcf_16	len;                	\
	hcf_16	typ;                	\
	hcf_16	par1;               	\
	hcf_16	par2;               	\
	hcf_16	par3;               	\
	hcf_16	par4;               	\
	hcf_16	par5;               	\
	hcf_16	par6;               	\
	hcf_16	par7;               	\
	hcf_16	par8;               	\
	hcf_16	par9;               	\
	hcf_16	par10;               	\
	hcf_16	par11;               	\
} name##_STRCT;


typedef struct CHANNEL_SET {				
	hcf_16	first_channel;
	hcf_16	number_of_channels;
	hcf_16	max_tx_output_level;
} CHANNEL_SET;

typedef struct KEY_STRCT {					
    hcf_16  len;	              				
    hcf_8   key[14];							
} KEY_STRCT;

typedef struct SCAN_RS_STRCT {				
	hcf_16	channel_id;
	hcf_16	noise_level;
	hcf_16	signal_level;
	hcf_8	bssid[6];
	hcf_16	beacon_interval_time;
	hcf_16	capability;
	hcf_16	ssid_len;
	hcf_8	ssid_val[32];
} SCAN_RS_STRCT;

typedef struct CFG_RANGE_SPEC_STRCT {		
	hcf_16	variant;
	hcf_16	bottom;
	hcf_16	top;
} CFG_RANGE_SPEC_STRCT;

typedef struct CFG_RANGE_SPEC_BYTE_STRCT {	
	hcf_8	variant[2];
	hcf_8	bottom[2];
	hcf_8	top[2];
} CFG_RANGE_SPEC_BYTE_STRCT;

XX1( RID_LOG, unsigned short FAR*, bufp )
typedef RID_LOG_STRCT  FAR *RID_LOGP;
XX1( CFG_RID_LOG, RID_LOGP, recordp )

 X1( LTV,		val[1] )												
 X1( LTV_MAX,	val[HCF_MAX_LTV] )										
XX2( CFG_REG_MB, hcf_16* , mb_addr, hcf_16, mb_size )

typedef struct CFG_MB_INFO_FRAG {	
	unsigned short FAR*	frag_addr;
	hcf_16				frag_len;
} CFG_MB_INFO_FRAG;

XX3( CFG_MB_INFO,		  hcf_16, base_typ, hcf_16, frag_cnt, CFG_MB_INFO_FRAG, frag_buf[ 1] )
XX3( CFG_MB_INFO_RANGE1,  hcf_16, base_typ, hcf_16, frag_cnt, CFG_MB_INFO_FRAG, frag_buf[ 1] )
XX3( CFG_MB_INFO_RANGE2,  hcf_16, base_typ, hcf_16, frag_cnt, CFG_MB_INFO_FRAG, frag_buf[ 2] )
XX3( CFG_MB_INFO_RANGE3,  hcf_16, base_typ, hcf_16, frag_cnt, CFG_MB_INFO_FRAG, frag_buf[ 3] )
XX3( CFG_MB_INFO_RANGE20, hcf_16, base_typ, hcf_16, frag_cnt, CFG_MB_INFO_FRAG, frag_buf[20] )

XX3( CFG_MB_ASSERT, hcf_16, line, hcf_16, trace, hcf_32, qualifier )	
#if (HCF_ASSERT) & ( HCF_ASSERT_LNK_MSF_RTN | HCF_ASSERT_RT_MSF_RTN )
typedef void (MSF_ASSERT_RTN)( unsigned int , hcf_16, hcf_32 );
typedef MSF_ASSERT_RTN  * MSF_ASSERT_RTNP;
XX2( CFG_REG_ASSERT_RTNP, hcf_16, lvl, MSF_ASSERT_RTNP, rtnp )
#endif 

 X1( CFG_HCF_OPT, val[20] )											  	
 X3( CFG_CMD_HCF, cmd, mode, add_info )									

typedef struct {
	hcf_16		len;
	hcf_16		typ;
	hcf_16		mode;			
	hcf_16		segment_size;  	
	hcf_32		nic_addr;  		
	hcf_16		flags;			
	hcf_8 FAR   *host_addr;  	
} CFG_PROG_STRCT; 

typedef struct {
    hcf_16      len;
    hcf_16      typ;
    hcf_16      msg_id, msg_par, msg_tstamp;
} CFG_FW_PRINTF_STRCT;

typedef struct {
    hcf_16      len;
    hcf_16      typ;
    hcf_32      DbMsgCount, 	
                DbMsgBuffer, 	
                DbMsgSize, 		
                DbMsgIntrvl;	
} CFG_FW_PRINTF_BUFFER_LOCATION_STRCT;

XX3( CFG_RANGES,	hcf_16, role, hcf_16, id, CFG_RANGE_SPEC_STRCT, var_rec[ 1] ) 
XX3( CFG_RANGE1,	hcf_16, role, hcf_16, id, CFG_RANGE_SPEC_STRCT, var_rec[ 1] ) 
XX3( CFG_RANGE2,	hcf_16, role, hcf_16, id, CFG_RANGE_SPEC_STRCT, var_rec[ 2] ) 
XX3( CFG_RANGE3,	hcf_16, role, hcf_16, id, CFG_RANGE_SPEC_STRCT, var_rec[ 3] ) 
XX3( CFG_RANGE4,	hcf_16, role, hcf_16, id, CFG_RANGE_SPEC_STRCT, var_rec[ 4] ) 
XX3( CFG_RANGE5,	hcf_16, role, hcf_16, id, CFG_RANGE_SPEC_STRCT, var_rec[ 5] ) 
XX3( CFG_RANGE6,	hcf_16, role, hcf_16, id, CFG_RANGE_SPEC_STRCT, var_rec[ 6] ) 
XX3( CFG_RANGE7,	hcf_16, role, hcf_16, id, CFG_RANGE_SPEC_STRCT, var_rec[ 7] ) 
XX3( CFG_RANGE20,	hcf_16, role, hcf_16, id, CFG_RANGE_SPEC_STRCT, var_rec[20] ) 

 X3( CFG_ASSOC_STAT,  assoc_stat, station_addr[3], val[46] ) 	
 X2( CFG_ASSOC_STAT3, assoc_stat, station_addr[3] ) 								
 X3( CFG_ASSOC_STAT1, assoc_stat, station_addr[3], frame_body[43] )					
 X4( CFG_ASSOC_STAT2, assoc_stat, station_addr[3], old_ap_addr[3], frame_body[43] )	

 X1( CFG_CNF_PORT_TYPE,				port_type			 ) 
 X1( CFG_MAC_ADDR,					mac_addr[3] 		 ) 
 X1( CFG_CNF_OWN_MAC_ADDR,			mac_addr[3]			 )
 X1( CFG_ID,						ssid[17]			 ) 
 X1( CFG_CNF_OWN_CHANNEL,			channel				 ) 
 X1( CFG_CNF_OWN_SSID,				ssid[17]			 )
 X1( CFG_CNF_OWN_ATIM_WINDOW,		atim_window			 )
 X1( CFG_CNF_SYSTEM_SCALE,			system_scale		 )
 X1( CFG_CNF_MAX_DATA_LEN,			max_data_len		 )
 X1( CFG_CNF_WDS_ADDR,				mac_addr[3]			 ) 
 X1( CFG_CNF_PM_ENABLED,			pm_enabled			 ) 
 X1( CFG_CNF_PM_EPS,				pm_eps				 ) 
 X1( CFG_CNF_MCAST_RX,				mcast_rx			 ) 
 X1( CFG_CNF_MAX_SLEEP_DURATION,	duration			 ) 
 X1( CFG_CNF_PM_HOLDOVER_DURATION,	duration			 ) 
 X1( CFG_CNF_OWN_NAME,				ssid[17]			 ) 
 X1( CFG_CNF_OWN_DTIM_PERIOD,		period				 ) 
 X1( CFG_CNF_WDS_ADDR1,				mac_addr[3]			 ) 
 X1( CFG_CNF_WDS_ADDR2,				mac_addr[3]			 ) 
 X1( CFG_CNF_WDS_ADDR3,				mac_addr[3]			 ) 
 X1( CFG_CNF_WDS_ADDR4,				mac_addr[3]			 ) 
 X1( CFG_CNF_WDS_ADDR5,				mac_addr[3]			 ) 
 X1( CFG_CNF_WDS_ADDR6,				mac_addr[3]			 ) 
 X1( CFG_CNF_MCAST_PM_BUF,			mcast_pm_buf		 ) 
 X1( CFG_CNF_REJECT_ANY,			reject_any			 ) 
 X1( CFG_CNF_ENCRYPTION,			encryption			 ) 
 X1( CFG_CNF_AUTHENTICATION,		authentication		 ) 
 X1( CFG_CNF_EXCL_UNENCRYPTED,		exclude_unencrypted	 ) 
 X1( CFG_CNF_MCAST_RATE,			mcast_rate			 ) 
 X1( CFG_CNF_INTRA_BSS_RELAY,		intra_bss_relay		 ) 
 X1( CFG_CNF_MICRO_WAVE,			micro_wave			 ) 
 X1( CFG_CNF_LOAD_BALANCING,		load_balancing		 ) 
 X1( CFG_CNF_MEDIUM_DISTRIBUTION,	medium_distribution	 ) 
 X1( CFG_CNF_GROUP_ADDR_FILTER,		group_addr_filter	 ) 
 X1( CFG_CNF_TX_POW_LVL,			tx_pow_lvl			 ) 
XX4( CFG_CNF_COUNTRY_INFO,								 \
		hcf_16, n_channel_sets, hcf_16, country_code[2], \
		hcf_16, environment, CHANNEL_SET, channel_set[1] ) 
XX4( CFG_CNF_COUNTRY_INFO_MAX,							 \
		hcf_16, n_channel_sets, hcf_16, country_code[2], \
		hcf_16, environment, CHANNEL_SET, channel_set[14]) 

 X1( CFG_DESIRED_SSID,			ssid[17]					 )	
#define GROUP_ADDR_SIZE			(32 * 6)						
 X1( CFG_GROUP_ADDR,			mac_addr[GROUP_ADDR_SIZE/2]	 )	
 X1( CFG_CREATE_IBSS,			create_ibss					 )	
 X1( CFG_RTS_THRH,				rts_thrh					 )	
 X1( CFG_TX_RATE_CNTL,			tx_rate_cntl				 )	
 X1( CFG_PROMISCUOUS_MODE,		promiscuous_mode			 )	
 X1( CFG_WOL,					wake_on_lan					 )	
 X1( CFG_RTS_THRH0,				rts_thrh					 )	
 X1( CFG_RTS_THRH1,				rts_thrh					 )	
 X1( CFG_RTS_THRH2,				rts_thrh					 )	
 X1( CFG_RTS_THRH3,				rts_thrh					 )	
 X1( CFG_RTS_THRH4,				rts_thrh					 )	
 X1( CFG_RTS_THRH5,				rts_thrh					 )	
 X1( CFG_RTS_THRH6,				rts_thrh					 )	
 X1( CFG_TX_RATE_CNTL0,			rate_cntl 					 )	
 X1( CFG_TX_RATE_CNTL1,			rate_cntl					 )	
 X1( CFG_TX_RATE_CNTL2,			rate_cntl					 )	
 X1( CFG_TX_RATE_CNTL3,			rate_cntl					 )	
 X1( CFG_TX_RATE_CNTL4,			rate_cntl					 )	
 X1( CFG_TX_RATE_CNTL5,			rate_cntl					 )	
 X1( CFG_TX_RATE_CNTL6,			rate_cntl					 )	
XX1( CFG_DEFAULT_KEYS,			KEY_STRCT, key[4]			 )	
 X1( CFG_TX_KEY_ID,				tx_key_id					 )	
 X1( CFG_SCAN_SSID,				ssid[17]					 )	
 X5( CFG_ADD_TKIP_DEFAULT_KEY,								 \
		 tkip_key_id_info, tkip_key_iv_info[4], tkip_key[8], \
		 tx_mic_key[4], rx_mic_key[4] 						 )	
 X6( CFG_ADD_TKIP_MAPPED_KEY,	bssid[3], tkip_key[8], 		 \
		 tsc[4], rsc[4], tx_mic_key[4], rx_mic_key[4] 		 )	
 X1( CFG_SET_WPA_AUTHENTICATION_SUITE, 						 \
		 ssn_authentication_suite							 )	
 X1( CFG_REMOVE_TKIP_DEFAULT_KEY,tkip_key_id				 )	
 X1( CFG_TICK_TIME,				tick_time					 )	
 X1( CFG_DDS_TICK_TIME,			tick_time					 )	

#define WOL_PATTERNS				5		
#define WOL_PATTERN_LEN				124		
#define WOL_MASK_LEN			 	30		
#define WOL_BUF_SIZE	(WOL_PATTERNS * (WOL_PATTERN_LEN + WOL_MASK_LEN + 6) / 2)
X2( CFG_WOL_PATTERNS, nPatterns, buffer[WOL_BUF_SIZE]		 )  

 X5( CFG_SUP_RANGE,		role, id, variant, bottom, top				   ) 
 X4( CFG_IDENTITY,			comp_id, variant, version_major, version_minor ) 
#define CFG_DRV_IDENTITY_STRCT	CFG_IDENTITY_STRCT
#define CFG_PRI_IDENTITY_STRCT	CFG_IDENTITY_STRCT
#define CFG_NIC_IDENTITY_STRCT	CFG_IDENTITY_STRCT
#define CFG_FW_IDENTITY_STRCT	CFG_IDENTITY_STRCT
 X1( CFG_RID_INF_MIN,		y											   ) 
 X1( CFG_MAX_LOAD_TIME,		max_load_time								   ) 
 X3( CFG_DL_BUF,			buf_page, buf_offset, buf_len				   ) 
 X5( CFG_CFI_ACT_RANGES_PRI,role, id, variant, bottom, top				   ) 
 X1( CFG_NIC_SERIAL_NUMBER,	serial_number[17]							   ) 
 X5( CFG_NIC_MFI_SUP_RANGE,	role, id, variant, bottom, top				   ) 
 X5( CFG_NIC_CFI_SUP_RANGE,	role, id, variant, bottom, top				   ) 
 X1( CFG_NIC_TEMP_TYPE,		temp_type									   ) 
 X5( CFG_NIC_PROFILE,													   \
		 profile_code, capability_options, allowed_data_rates, val4, val5  ) 
 X5( CFG_MFI_ACT_RANGES,	role, id, variant, bottom, top				   ) 
 X5( CFG_CFI_ACT_RANGES_STA,role, id, variant, bottom, top				   ) 
 X5( CFG_MFI_ACT_RANGES_STA,role, id, variant, bottom, top				   ) 
 X1( CFG_NIC_BUS_TYPE,		nic_bus_type								   ) 

 X1( CFG_PORT_STAT,				port_stat							 ) 
 X1( CFG_CUR_SSID,				ssid[17]							 ) 
 X1( CFG_CUR_BSSID,				mac_addr[3]							 ) 
 X3( CFG_COMMS_QUALITY,			coms_qual, signal_lvl, noise_lvl	 ) 
 X1( CFG_CUR_TX_RATE,			rate								 ) 
 X1( CFG_CUR_BEACON_INTERVAL,	interval							 ) 
#if (HCF_TYPE) & HCF_TYPE_WARP
 X11( CFG_CUR_SCALE_THRH,											 \
	 carrier_detect_thrh_cck, carrier_detect_thrh_ofdm, defer_thrh,	 \
	 energy_detect_thrh, rssi_on_thrh_deviation, 					 \
	 rssi_off_thrh_deviation, cck_drop_thrh, ofdm_drop_thrh, 		 \
	 cell_search_thrh, out_of_range_thrh, delta_snr				 )
#else
 X6( CFG_CUR_SCALE_THRH,											 \
	 energy_detect_thrh, carrier_detect_thrh, defer_thrh, 			 \
	 cell_search_thrh, out_of_range_thrh, delta_snr					 ) 
#endif 
 X1( CFG_PROTOCOL_RSP_TIME,		time								 ) 
 X1( CFG_CUR_SHORT_RETRY_LIMIT,	limit								 ) 
 X1( CFG_CUR_LONG_RETRY_LIMIT,	limit								 ) 
 X1( CFG_MAX_TX_LIFETIME,		time								 ) 
 X1( CFG_MAX_RX_LIFETIME,		time								 ) 
 X1( CFG_CF_POLLABLE,			cf_pollable							 ) 
 X2( CFG_AUTHENTICATION_ALGORITHMS,authentication_type, type_enabled ) 
 X1( CFG_PRIVACY_OPT_IMPLEMENTED,privacy_opt_implemented			 ) 
 X1( CFG_CUR_REMOTE_RATES,		rates								 ) 
 X1( CFG_CUR_USED_RATES,		rates								 ) 
 X1( CFG_CUR_SYSTEM_SCALE,		current_system_scale				 ) 
 X1( CFG_CUR_TX_RATE1,			rate 								 ) 
 X1( CFG_CUR_TX_RATE2,			rate								 ) 
 X1( CFG_CUR_TX_RATE3,			rate								 ) 
 X1( CFG_CUR_TX_RATE4,			rate								 ) 
 X1( CFG_CUR_TX_RATE5,			rate								 ) 
 X1( CFG_CUR_TX_RATE6,			rate								 ) 
 X1( CFG_OWN_MAC_ADDR,			mac_addr[3]							 ) 
 X3( CFG_PCF_INFO,				medium_occupancy_limit, 			 \
		 						cfp_period, cfp_max_duration 		 ) 
 X1( CFG_CUR_WPA_INFO_ELEMENT, ssn_info_element[1]				 	 ) 
 X4( CFG_CUR_TKIP_IV_INFO, 											 \
		 tkip_seq_cnt0[4], tkip_seq_cnt1[4], 						 \
		 tkip_seq_cnt2[4], tkip_seq_cnt3[4]  						 ) 
 X2( CFG_CUR_ASSOC_REQ_INFO,	frame_type, frame_body[1]			 ) 
 X2( CFG_CUR_ASSOC_RESP_INFO,	frame_type, frame_body[1]			 ) 


 X1( CFG_PHY_TYPE,				phy_type 							 ) 
 X1( CFG_CUR_CHANNEL,			current_channel						 ) 
 X1( CFG_CUR_POWER_STATE,		current_power_state					 ) 
 X1( CFG_CCAMODE,				cca_mode							 ) 
 X1( CFG_SUPPORTED_DATA_RATES,	rates[5]							 ) 


XX1( CFG_SCAN,					SCAN_RS_STRCT, scan_result[32]		 ) 




#define MDD_ACT_SCAN			0x06					
#define MDD_ACT_PRS_SCAN 		0x07					

#define UIL_FUN_CONNECT			0x00					
#define UIL_FUN_DISCONNECT		0x01					
#define UIL_FUN_ACTION			0x02					
#define UIL_FUN_SEND_DIAG_MSG	0x03					
#define UIL_FUN_GET_INFO		0x04					
#define UIL_FUN_PUT_INFO		0x05					

#define UIL_ACT_SCAN			MDD_ACT_SCAN
#define UIL_ACT_PRS_SCAN 		MDD_ACT_PRS_SCAN
#define UIL_ACT_BLOCK	 		0x0B
#define UIL_ACT_UNBLOCK	 		0x0C
#define UIL_ACT_RESET	 		0x80
#define UIL_ACT_REBIND	 		0x81
#define UIL_ACT_APPLY	 		0x82
#define UIL_ACT_DISCONNECT		0x83	

#define HCF_DISCONNECT			0x01					
#define HCF_ACT_TALLIES 		0x05					
#if ( (HCF_TYPE) & HCF_TYPE_WARP ) == 0
#define HCF_ACT_SCAN			MDD_ACT_SCAN
#endif 
#define HCF_ACT_PRS_SCAN		MDD_ACT_PRS_SCAN
#if HCF_INT_ON
#define HCF_ACT_INT_OFF 		0x0D					
#define HCF_ACT_INT_ON			0x0E					
#define HCF_ACT_INT_FORCE_ON	0x0F					
#endif 
#define HCF_ACT_RX_ACK			0x15					
#if (HCF_TYPE) & HCF_TYPE_CCX
#define HCF_ACT_CCX_ON			0x1A					
#define HCF_ACT_CCX_OFF			0x1B					
#endif 
#if (HCF_SLEEP) & HCF_DDS
#define HCF_ACT_SLEEP			0x1C					
#endif 


#define CFG_RID_FW_MIN							0xFA00	

#define CFG_RID_CFG_MIN					0xFC00		


#define CFG_CNF_PORT_TYPE				0xFC00		
#define CFG_CNF_OWN_MAC_ADDR			0xFC01		
#define CFG_CNF_OWN_CHANNEL				0xFC03		
#define CFG_CNF_OWN_SSID				0xFC04		
#define CFG_CNF_OWN_ATIM_WINDOW			0xFC05		
#define CFG_CNF_SYSTEM_SCALE			0xFC06		
#define CFG_CNF_MAX_DATA_LEN			0xFC07		
#define CFG_CNF_PM_ENABLED				0xFC09		
#define CFG_CNF_MCAST_RX				0xFC0B		
#define CFG_CNF_MAX_SLEEP_DURATION		0xFC0C		
#define CFG_CNF_HOLDOVER_DURATION		0xFC0D		
#define CFG_CNF_OWN_NAME				0xFC0E		

#define CFG_CNF_OWN_DTIM_PERIOD			0xFC10		
#define CFG_CNF_WDS_ADDR1				0xFC11		
#define CFG_CNF_WDS_ADDR2				0xFC12		
#define CFG_CNF_WDS_ADDR3				0xFC13		
#define CFG_CNF_WDS_ADDR4				0xFC14		
#define CFG_CNF_WDS_ADDR5				0xFC15		
#define CFG_CNF_WDS_ADDR6				0xFC16		
#define CFG_CNF_PM_MCAST_BUF			0xFC17		
#define CFG_CNF_MCAST_PM_BUF			CFG_CNF_PM_MCAST_BUF	
#define CFG_CNF_REJECT_ANY				0xFC18		

#define CFG_CNF_ENCRYPTION				0xFC20		
#define CFG_CNF_AUTHENTICATION			0xFC21		
#define CFG_CNF_EXCL_UNENCRYPTED		0xFC22		
#define CFG_CNF_MCAST_RATE				0xFC23		
#define CFG_CNF_INTRA_BSS_RELAY			0xFC24		
#define CFG_CNF_MICRO_WAVE				0xFC25		
#define CFG_CNF_LOAD_BALANCING		 	0xFC26		
#define CFG_CNF_MEDIUM_DISTRIBUTION	 	0xFC27		
#define CFG_CNF_RX_ALL_GROUP_ADDR		0xFC28		
#define CFG_CNF_COUNTRY_INFO			0xFC29		
#if (HCF_TYPE) & HCF_TYPE_WARP
#define CFG_CNF_TX_POW_LVL				0xFC2A		
#define CFG_CNF_CONNECTION_CNTL			0xFC30		
#define CFG_CNF_OWN_BEACON_INTERVAL		0xFC31		
#define CFG_CNF_SHORT_RETRY_LIMIT		0xFC32		
#define CFG_CNF_LONG_RETRY_LIMIT		0xFC33		
#define CFG_CNF_TX_EVENT_MODE			0xFC34		
#define CFG_CNF_WIFI_COMPATIBLE			0xFC35		
#endif 
#if (HCF_TYPE) & HCF_TYPE_BEAGLE_HII5
#define CFG_VOICE_RETRY_LIMIT			0xFC36		
#define CFG_VOICE_CONTENTION_WINDOW		0xFC37		
#endif	

#define CFG_DESIRED_SSID				0xFC02		

#define CFG_GROUP_ADDR					0xFC80		
#define CFG_CREATE_IBSS					0xFC81		
#define CFG_RTS_THRH					0xFC83		
#define CFG_TX_RATE_CNTL				0xFC84		
#define CFG_PROMISCUOUS_MODE			0xFC85		
#define CFG_WOL							0xFC86		
#define CFG_WOL_PATTERNS				0xFC87		
#define CFG_SUPPORTED_RATE_SET_CNTL		0xFC88		
#define CFG_BASIC_RATE_SET_CNTL			0xFC89		

#define CFG_SOFTWARE_ACK_MODE			0xFC90		
#define CFG_RTS_THRH0					0xFC97		
#define CFG_RTS_THRH1					0xFC98		
#define CFG_RTS_THRH2					0xFC99		
#define CFG_RTS_THRH3					0xFC9A		
#define CFG_RTS_THRH4					0xFC9B		
#define CFG_RTS_THRH5					0xFC9C		
#define CFG_RTS_THRH6					0xFC9D		

#define CFG_TX_RATE_CNTL0				0xFC9E		
#define CFG_TX_RATE_CNTL1				0xFC9F		
#define CFG_TX_RATE_CNTL2				0xFCA0		
#define CFG_TX_RATE_CNTL3				0xFCA1		
#define CFG_TX_RATE_CNTL4				0xFCA2		
#define CFG_TX_RATE_CNTL5				0xFCA3		
#define CFG_TX_RATE_CNTL6				0xFCA4		

#define CFG_DEFAULT_KEYS				0xFCB0		
#define CFG_TX_KEY_ID					0xFCB1		
#define CFG_SCAN_SSID					0xFCB2		
#define CFG_ADD_TKIP_DEFAULT_KEY		0xFCB4		
#define 	KEY_ID							0x0003		
#define 	TX_KEY							0x8000		
#define CFG_SET_WPA_AUTH_KEY_MGMT_SUITE	0xFCB5		
#define CFG_REMOVE_TKIP_DEFAULT_KEY		0xFCB6		
#define CFG_ADD_TKIP_MAPPED_KEY			0xFCB7		
#define CFG_REMOVE_TKIP_MAPPED_KEY		0xFCB8		
#define CFG_SET_WPA_CAPABILITIES_INFO	0xFCB9		
#define CFG_CACHED_PMK_ADDR				0xFCBA		
#define CFG_REMOVE_CACHED_PMK_ADDR		0xFCBB		
#define CFG_FCBC	0xFCBC	
#define CFG_FCBD	0xFCBD	
#define CFG_FCBE	0xFCBE	
#define CFG_FCBF	0xFCBF	

#define CFG_HANDOVER_ADDR				0xFCC0		
#define CFG_SCAN_CHANNEL				0xFCC2		
#define CFG_DISASSOCIATE_ADDR			0xFCC4		
#define CFG_PROBE_DATA_RATE				0xFCC5		
#define CFG_FRAME_BURST_LIMIT			0xFCC6		
#define CFG_COEXISTENSE_BEHAVIOUR		0xFCC7		
#define CFG_DEAUTHENTICATE_ADDR			0xFCC8		

#define CFG_TICK_TIME					0xFCE0		
#define CFG_DDS_TICK_TIME				0xFCE1		
#define CFG_RID_CFG_MAX					0xFCFF		


#define CFG_RID_INF_MIN					0xFD00	
#define CFG_MAX_LOAD_TIME				0xFD00	
#define CFG_DL_BUF						0xFD01	
#define CFG_PRI_IDENTITY				0xFD02	
#define CFG_PRI_SUP_RANGE				0xFD03	
#define CFG_NIC_HSI_SUP_RANGE			0xFD09	
#define CFG_NIC_SERIAL_NUMBER			0xFD0A	
#define CFG_NIC_IDENTITY				0xFD0B	
#define CFG_NIC_MFI_SUP_RANGE			0xFD0C	
#define CFG_NIC_CFI_SUP_RANGE			0xFD0D	
#define CFG_CHANNEL_LIST				0xFD10	
#define CFG_NIC_TEMP_TYPE  				0xFD12	
#define CFG_CIS							0xFD13	
#define CFG_NIC_PROFILE					0xFD14	
#define CFG_FW_IDENTITY					0xFD20	
#define CFG_FW_SUP_RANGE				0xFD21	
#define CFG_MFI_ACT_RANGES_STA			0xFD22	
#define CFG_CFI_ACT_RANGES_STA			0xFD23	
#define CFG_NIC_BUS_TYPE				0xFD24	
#define 	CFG_NIC_BUS_TYPE_PCCARD_CF		0x0000	
#define 	CFG_NIC_BUS_TYPE_USB			0x0001	
#define 	CFG_NIC_BUS_TYPE_CARDBUS		0x0002	
#define 	CFG_NIC_BUS_TYPE_PCI			0x0003	
#define CFG_DOMAIN_CODE						0xFD25

#define CFG_PORT_STAT					0xFD40	
#define CFG_CUR_SSID					0xFD41	
#define CFG_CUR_BSSID					0xFD42	
#define CFG_COMMS_QUALITY				0xFD43	
#define CFG_CUR_TX_RATE					0xFD44	
#define CFG_CUR_BEACON_INTERVAL			0xFD45	
#define CFG_CUR_SCALE_THRH				0xFD46	
#define CFG_PROTOCOL_RSP_TIME			0xFD47	
#define CFG_CUR_SHORT_RETRY_LIMIT		0xFD48	
#define CFG_CUR_LONG_RETRY_LIMIT		0xFD49	
#define CFG_MAX_TX_LIFETIME				0xFD4A	
#define CFG_MAX_RX_LIFETIME				0xFD4B	
#define CFG_CF_POLLABLE					0xFD4C	
#define CFG_AUTHENTICATION_ALGORITHMS	0xFD4D	
#define CFG_PRIVACY_OPT_IMPLEMENTED		0xFD4F	

#define CFG_CUR_REMOTE_RATES			0xFD50	
#define CFG_CUR_USED_RATES				0xFD51	
#define CFG_CUR_SYSTEM_SCALE			0xFD52	

#define CFG_CUR_TX_RATE1				0xFD80	
#define CFG_CUR_TX_RATE2				0xFD81	
#define CFG_CUR_TX_RATE3				0xFD82	
#define CFG_CUR_TX_RATE4				0xFD83	
#define CFG_CUR_TX_RATE5				0xFD84	
#define CFG_CUR_TX_RATE6				0xFD85	
#define CFG_NIC_MAC_ADDR				0xFD86	
#define CFG_PCF_INFO					0xFD87	
#define CFG_CUR_COUNTRY_INFO			0xFD89	
#define CFG_CUR_WPA_INFO_ELEMENT		0xFD8A	
#define CFG_CUR_TKIP_IV_INFO			0xFD8B	
#define CFG_CUR_ASSOC_REQ_INFO			0xFD8C	
#define CFG_CUR_ASSOC_RESP_INFO			0xFD8D	
#define CFG_CUR_LOAD					0xFD8E	

#define CFG_SECURITY_CAPABILITIES		0xFD90	

#define CFG_PHY_TYPE					0xFDC0	
#define CFG_CUR_CHANNEL					0xFDC1	
#define CFG_CUR_POWER_STATE				0xFDC2	
#define CFG_CCA_MODE					0xFDC3	
#define CFG_SUPPORTED_DATA_RATES		0xFDC6	

#define CFG_RID_INF_MAX					0xFDFF	

#define CFG_RID_ENG_MIN					0xFFE0	




#define CARD_STAT_INCOMP_PRI			0x2000U	
#define CARD_STAT_INCOMP_FW				0x1000U	
#define CARD_STAT_DEFUNCT				0x0100U	
#define RX_STAT_PRIO					0x00E0U	
#define RX_STAT_ERR						0x000FU	
#define 	RX_STAT_UNDECR				0x0002U	
#define 	RX_STAT_FCS_ERR				0x0001U	

#define ENC_NONE			            0xFF
#define ENC_1042    			        0x00
#define ENC_TUNNEL      	    		0xF8


#define HCF_SUCCESS					0x00	
#define HCF_ERR_TIME_OUT			0x04	
#define HCF_ERR_NO_NIC				0x05	
#define HCF_ERR_LEN					0x08	
#define HCF_ERR_INCOMP_PRI			0x09	
#define HCF_ERR_INCOMP_FW			0x0A	
#define HCF_ERR_MIC					0x0D	
#define HCF_ERR_SLEEP				0x0E	
#define HCF_ERR_MAX					0x3F	
#define HCF_ERR_DEFUNCT				0x80	
#define HCF_ERR_DEFUNCT_AUX			0x82	
#define HCF_ERR_DEFUNCT_TIMER		0x83	
#define HCF_ERR_DEFUNCT_TIME_OUT	0x84	
#define HCF_ERR_DEFUNCT_CMD_SEQ		0x86	

#define HCF_INT_PENDING				0x01	

#define HCF_PORT_0 					0x0000	
#define HCF_PORT_1 					0x0100	
#define HCF_PORT_2 					0x0200
#define HCF_PORT_3 					0x0300
#define HCF_PORT_4 					0x0400
#define HCF_PORT_5 					0x0500
#define HCF_PORT_6 					0x0600

#define HCF_CNTL_ENABLE				0x01
#define HCF_CNTL_DISABLE			0x02
#define HCF_CNTL_CONNECT			0x03
#define HCF_CNTL_DISCONNECT			0x05
#define HCF_CNTL_CONTINUE			0x07

#define USE_DMA 					0x0001
#define USE_16BIT 					0x0002
#define DMA_ENABLED					0x8000	

#define HCF_DMA_RX_BUF1_SIZE		(HFS_ADDR_DEST + 8)			
#define HCF_DMA_TX_BUF1_SIZE		(HFS_ADDR_DEST + 2*6 + 8)	

#if (HCF_TYPE) & HCF_TYPE_WARP
#define  HCF_TX_CNTL_MASK	0x27E7	
#else
#define  HCF_TX_CNTL_MASK	0x67E7	
#endif 

#define HFS_TX_CNTL_TX_EX			0x0004U

#if (HCF_TYPE) & HCF_TYPE_WPA
#define HFS_TX_CNTL_MIC				0x0010U	
#define HFS_TX_CNTL_MIC_KEY_ID		0x1800U	
#endif 

#define HFS_TX_CNTL_PORT			0x0700U	

#if (HCF_TYPE) & HCF_TYPE_CCX
#define HFS_TX_CNTL_CKIP			0x2000U	
#endif 

#if (HCF_TYPE) & HCF_TYPE_TX_DELAY
#define HFS_TX_CNTL_TX_DELAY		0x4000U	
#endif 
#define HFS_TX_CNTL_TX_CONT			0x4000u	

#define CFG_PROD_DATA					0x0800 		
#define CFG_DL_EEPROM					0x0806		
#define		CFG_PDA							0x0002		
#define		CFG_MEM_I2PROM					0x0004		

#define		CFG_MEM_READ					0x0000
#define		CFG_MEM_WRITE					0x0001

#define CFG_NULL						0x0820		
#define CFG_MB_INFO						0x0820		
#define CFG_WMP							0x0822		

#if defined MSF_COMPONENT_ID
#define CFG_DRV_INFO					0x0825		
#define CFG_DRV_IDENTITY				0x0826		
#define CFG_DRV_SUP_RANGE				0x0827      
#define CFG_DRV_ACT_RANGES_PRI			0x0828      
#define CFG_DRV_ACT_RANGES_STA			0x0829      
#define CFG_DRV_ACT_RANGES_HSI 			0x082A      
#define CFG_DRV_ACT_RANGES_APF			0x082B		
#define CFG_HCF_OPT						0x082C		
#endif 

#define CFG_REG_MB						0x0830		
#define CFG_MB_ASSERT					0x0831		
#define CFG_REG_ASSERT_RTNP				0x0832		
#if (HCF_EXT) & HCF_EXT_INFO_LOG
#define CFG_REG_INFO_LOG				0x0839		
#endif 
#define CFG_CNTL_OPT					0x083A		

#define CFG_PROG						0x0857		
#define 	CFG_PROG_STOP					0x0000
#define 	CFG_PROG_VOLATILE				0x0100
#define 	CFG_PROG_SEEPROM_READBACK 		0x0400

#define CFG_FW_PRINTF                       0x0858      
#define CFG_FW_PRINTF_BUFFER_LOCATION       0x0859      

#define CFG_CMD_NIC						0x0860		
#define CFG_CMD_HCF						0x0863		
#define 	CFG_CMD_HCF_REG_ACCESS			0x0000	
#define 	CFG_CMD_HCF_RX_MON				0x0001	


#define CFG_ENCRYPT_STRING				0x0900		
#define CFG_AP_MODE						0x0901		
#define CFG_DRIVER_ENABLE				0x0902		
#define CFG_PCI_COMMAND					0x0903		
#define CFG_WOLAS_ENABLE				0x0904		
#define CFG_COUNTRY_STRING				0x0905		
#define CFG_FW_DUMP						0x0906		
#define CFG_POWER_MODE					0x0907		
#define CFG_CONNECTION_MODE				0x0908		
#define CFG_IFB							0x0909		
#define CFG_MSF_TALLIES					0x090A		
#define CFG_CURRENT_LINK_STATUS			0x090B		

#define CFG_INFO_FRAME_MIN				0xF000		

#define CFG_TALLIES						0xF100		
#define CFG_SCAN						0xF101		
#define CFG_PRS_SCAN					0xF102		

#define CFG_LINK_STAT 					0xF200		
#define 	CFG_LINK_STAT_CONNECTED			0x0001
#define 	CFG_LINK_STAT_DISCONNECTED		0x0002
#define 	CFG_LINK_STAT_AP_CHANGE			0x0003
#define 	CFG_LINK_STAT_AP_OOR			0x0004
#define 	CFG_LINK_STAT_AP_IR				0x0005
#define 	CFG_LINK_STAT_FW				0x000F	
#define 	CFG_LINK_STAT_CHANGE			0x8000
#define CFG_ASSOC_STAT					0xF201		
#define CFG_SECURITY_STAT				0xF202		
#define CFG_UPDATED_INFO_RECORD			0xF204		



typedef LTV_STRCT FAR *	LTVP;   

#if defined WVLAN_42 || defined WVLAN_43 
typedef struct DUI_STRCT {			
	void  FAR	*ifbp;				
	hcf_16		stat;				
	hcf_16		fun;				
	LTV_STRCT	ltv;				
} DUI_STRCT;
typedef DUI_STRCT FAR *	DUIP;
#endif 


typedef struct CFG_CMD_NIC_STRCT {	
	hcf_16	len;					
	hcf_16	typ;					
	hcf_16	cmd;					
	hcf_16	parm0;					
	hcf_16	parm1;					
	hcf_16	parm2;					
	hcf_16	stat;					
	hcf_16	resp0;					
	hcf_16	resp1;					
	hcf_16	resp2;					
	hcf_16	hcf_stat;				
	hcf_16	ifb_err_cmd;			
	hcf_16	ifb_err_qualifier;		
} CFG_CMD_NIC_STRCT;


typedef struct CFG_DRV_INFO_STRCT {		
	hcf_16	len;					
	hcf_16	typ;					
	hcf_8	driver_name[8];			
	hcf_16	driver_version;			
	hcf_16	HCF_version;   			
	hcf_16	driver_stat;			
	hcf_16	IO_address;				
	hcf_16	IO_range;				
	hcf_16	IRQ_number;				
	hcf_16	card_stat;				
	hcf_16	frame_type;				
	hcf_32	drv_info;				
}CFG_DRV_INFO_STRCT;

#define COMP_ID_FW_PRI					21		
#define COMP_ID_FW_INTERMEDIATE			22		
#define COMP_ID_FW_STA					31		
#define COMP_ID_FW_AP					32		
#define COMP_ID_FW_AP_FAKE			   331		

#define COMP_ID_MINIPORT_NDIS_31		41		
#define COMP_ID_PACKET					42		
#define COMP_ID_ODI_16					43		
#define COMP_ID_ODI_32					44		
#define COMP_ID_MAC_OS					45		
#define COMP_ID_WIN_CE					46		
#define COMP_ID_MINIPORT_NDIS_50		48		
#define COMP_ID_LINUX					49		/*Linux, GPL'ed HCF based, full source code in Public Domain
										  		 *thanks to Andreas Neuhaus								*/
#define COMP_ID_QNX						50		
#define COMP_ID_MINIPORT_NDIS_50_USB	51		
#define COMP_ID_MINIPORT_NDIS_40		52		
#define COMP_ID_VX_WORKS_ENDSTA			53		
#define COMP_ID_VX_WORKS_ENDAP			54		
#define COMP_ID_VX_WORKS_END			56		
#define COMP_ID_WSU						63		
#define COMP_ID_AP1						81		
#define COMP_ID_EC						83		
#define COMP_ID_UBL						87		

#define COMP_ROLE_SUPL					0x00	
#define COMP_ROLE_ACT					0x01	

												
#define COMP_ID_MFI						0x01	
#define COMP_ID_CFI						0x02	
#define COMP_ID_PRI						0x03	
#define COMP_ID_STA						0x04	
#define COMP_ID_DUI						0x05	
#define COMP_ID_HSI						0x06	
#define COMP_ID_DAI						0x07	
#define COMP_ID_APF						0x08	
#define COMP_ID_INT						0x09	

#ifdef HCF_LEGACY
#define HCF_ACT_ACS_SCAN				HCF_ACT_PRS_SCAN
#define UIL_ACT_ACS_SCAN 				UIL_ACT_PRS_SCAN
#define MDD_ACT_ACS_SCAN	 			MDD_ACT_PRS_SCAN
#define CFG_ACS_SCAN					CFG_PRS_SCAN
#endif 

#endif 

