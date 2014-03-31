/******************************************************************************
 *
 * Copyright(c) 2009-2012  Realtek Corporation.
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
 * wlanfae <wlanfae@realtek.com>
 * Realtek Corporation, No. 2, Innovation Road II, Hsinchu Science Park,
 * Hsinchu 300, Taiwan.
 *
 * Larry Finger <Larry.Finger@lwfinger.net>
 *
 *****************************************************************************/
#ifndef __REALTEK_FIRMWARE92S_H__
#define __REALTEK_FIRMWARE92S_H__

#define RTL8190_MAX_FIRMWARE_CODE_SIZE		64000
#define RTL8190_MAX_RAW_FIRMWARE_CODE_SIZE	90000
#define RTL8190_CPU_START_OFFSET		0x80
#define	MAX_FIRMWARE_CODE_SIZE			0xFF00

#define	RT_8192S_FIRMWARE_HDR_SIZE		80
#define RT_8192S_FIRMWARE_HDR_EXCLUDE_PRI_SIZE	32

#define MAX_DEV_ADDR_SIZE			8
#define MAX_FIRMWARE_INFORMATION_SIZE		32
#define MAX_802_11_HEADER_LENGTH		(40 + \
						MAX_FIRMWARE_INFORMATION_SIZE)
#define ENCRYPTION_MAX_OVERHEAD			128
#define MAX_FRAGMENT_COUNT			8
#define MAX_TRANSMIT_BUFFER_SIZE		(1600 + \
						(MAX_802_11_HEADER_LENGTH + \
						ENCRYPTION_MAX_OVERHEAD) *\
						MAX_FRAGMENT_COUNT)

#define H2C_TX_CMD_HDR_LEN			8

#define	FW_DIG_ENABLE_CTL			BIT(0)
#define	FW_HIGH_PWR_ENABLE_CTL			BIT(1)
#define	FW_SS_CTL				BIT(2)
#define	FW_RA_INIT_CTL				BIT(3)
#define	FW_RA_BG_CTL				BIT(4)
#define	FW_RA_N_CTL				BIT(5)
#define	FW_PWR_TRK_CTL				BIT(6)
#define	FW_IQK_CTL				BIT(7)
#define	FW_FA_CTL				BIT(8)
#define	FW_DRIVER_CTRL_DM_CTL			BIT(9)
#define	FW_PAPE_CTL_BY_SW_HW			BIT(10)
#define	FW_DISABLE_ALL_DM			0
#define	FW_PWR_TRK_PARAM_CLR			0x0000ffff
#define	FW_RA_PARAM_CLR				0xffff0000

enum desc_packet_type {
	DESC_PACKET_TYPE_INIT = 0,
	DESC_PACKET_TYPE_NORMAL = 1,
};

struct fw_priv {
	
	
	u8 signature_0;
	
	u8 signature_1;
	u8 hci_sel;
	
	u8 chip_version;
	
	u8 customer_id_0;
	
	u8 customer_id_1;
	u8 rf_config;
	
	u8 usb_ep_num;

	
	
	u8 regulatory_class_0;
	
	u8 regulatory_class_1;
	
	u8 regulatory_class_2;
	
	u8 regulatory_class_3;
	
	u8 rfintfs;
	u8 def_nettype;
	u8 rsvd010;
	u8 rsvd011;

	
	
	u8 lbk_mode;
	u8 mp_mode;
	u8 rsvd020;
	u8 rsvd021;
	u8 rsvd022;
	u8 rsvd023;
	u8 rsvd024;
	u8 rsvd025;

	
	
	u8 qos_en;
	
	
	u8 bw_40mhz_en;
	u8 amsdu2ampdu_en;
	
	u8 ampdu_en;
	
	u8 rate_control_offload;
	
	u8 aggregation_offload;
	u8 rsvd030;
	u8 rsvd031;

	
	
	u8 beacon_offload;
	
	u8 mlme_offload;
	
	u8 hwpc_offload;
	
	u8 tcp_checksum_offload;
	
	u8 tcp_offload;
	
	u8 ps_control_offload;
	
	u8 wwlan_offload;
	u8 rsvd040;

	
	
	u8 tcp_tx_frame_len_L;
	
	u8 tcp_tx_frame_len_H;
	
	u8 tcp_rx_frame_len_L;
	
	u8 tcp_rx_frame_len_H;
	u8 rsvd050;
	u8 rsvd051;
	u8 rsvd052;
	u8 rsvd053;
};

struct fw_hdr {

	
	u16 signature;
	u16 version;
	
	u32 dmem_size;


	
	
	u32 img_imem_size;
	
	u32 img_sram_size;

	
	
	u32 fw_priv_size;
	u32 rsvd0;

	
	u32 rsvd1;
	u32 rsvd2;

	struct fw_priv fwpriv;

} ;

enum fw_status {
	FW_STATUS_INIT = 0,
	FW_STATUS_LOAD_IMEM = 1,
	FW_STATUS_LOAD_EMEM = 2,
	FW_STATUS_LOAD_DMEM = 3,
	FW_STATUS_READY = 4,
};

struct rt_firmware {
	struct fw_hdr *pfwheader;
	enum fw_status fwstatus;
	u16 firmwareversion;
	u8 fw_imem[RTL8190_MAX_FIRMWARE_CODE_SIZE];
	u8 fw_emem[RTL8190_MAX_FIRMWARE_CODE_SIZE];
	u32 fw_imem_len;
	u32 fw_emem_len;
	u8 sz_fw_tmpbuffer[RTL8190_MAX_RAW_FIRMWARE_CODE_SIZE];
	u32 sz_fw_tmpbufferlen;
	u16 cmdpacket_fragthresold;
};

struct h2c_set_pwrmode_parm {
	u8 mode;
	u8 flag_low_traffic_en;
	u8 flag_lpnav_en;
	u8 flag_rf_low_snr_en;
	
	u8 flag_dps_en;
	u8 bcn_rx_en;
	u8 bcn_pass_cnt;
	
	u8 bcn_to;
	u16	bcn_itv;
	
	u8 app_itv;
	u8 awake_bcn_itvl;
	u8 smart_ps;
	
	u8 bcn_pass_period;
};

struct h2c_joinbss_rpt_parm {
	u8 opmode;
	u8 ps_qos_info;
	u8 bssid[6];
	u16 bcnitv;
	u16 aid;
} ;

struct h2c_wpa_ptk {
	
	u8 kck[16];
	
	u8 kek[16];
	
	u8 tk1[16];
	union {
		
		u8 tk2[16];
		struct {
			u8 tx_mic_key[8];
			u8 rx_mic_key[8];
		} athu;
	} u;
};

struct h2c_wpa_two_way_parm {
	
	u8 pairwise_en_alg;
	u8 group_en_alg;
	struct h2c_wpa_ptk wpa_ptk_value;
} ;

enum h2c_cmd {
	FW_H2C_SETPWRMODE = 0,
	FW_H2C_JOINBSSRPT = 1,
	FW_H2C_WOWLAN_UPDATE_GTK = 2,
	FW_H2C_WOWLAN_UPDATE_IV = 3,
	FW_H2C_WOWLAN_OFFLOAD = 4,
};

enum fw_h2c_cmd {
	H2C_READ_MACREG_CMD,				
	H2C_WRITE_MACREG_CMD,
	H2C_READBB_CMD,
	H2C_WRITEBB_CMD,
	H2C_READRF_CMD,
	H2C_WRITERF_CMD,				
	H2C_READ_EEPROM_CMD,
	H2C_WRITE_EEPROM_CMD,
	H2C_READ_EFUSE_CMD,
	H2C_WRITE_EFUSE_CMD,
	H2C_READ_CAM_CMD,				
	H2C_WRITE_CAM_CMD,
	H2C_SETBCNITV_CMD,
	H2C_SETMBIDCFG_CMD,
	H2C_JOINBSS_CMD,
	H2C_DISCONNECT_CMD,				
	H2C_CREATEBSS_CMD,
	H2C_SETOPMode_CMD,
	H2C_SITESURVEY_CMD,
	H2C_SETAUTH_CMD,
	H2C_SETKEY_CMD,					
	H2C_SETSTAKEY_CMD,
	H2C_SETASSOCSTA_CMD,
	H2C_DELASSOCSTA_CMD,
	H2C_SETSTAPWRSTATE_CMD,
	H2C_SETBASICRATE_CMD,				
	H2C_GETBASICRATE_CMD,
	H2C_SETDATARATE_CMD,
	H2C_GETDATARATE_CMD,
	H2C_SETPHYINFO_CMD,
	H2C_GETPHYINFO_CMD,				
	H2C_SETPHY_CMD,
	H2C_GETPHY_CMD,
	H2C_READRSSI_CMD,
	H2C_READGAIN_CMD,
	H2C_SETATIM_CMD,				
	H2C_SETPWRMODE_CMD,
	H2C_JOINBSSRPT_CMD,
	H2C_SETRATABLE_CMD,
	H2C_GETRATABLE_CMD,
	H2C_GETCCXREPORT_CMD,				
	H2C_GETDTMREPORT_CMD,
	H2C_GETTXRATESTATICS_CMD,
	H2C_SETUSBSUSPEND_CMD,
	H2C_SETH2CLBK_CMD,
	H2C_TMP1,					
	H2C_WOWLAN_UPDATE_GTK_CMD,
	H2C_WOWLAN_FW_OFFLOAD,
	H2C_TMP2,
	H2C_TMP3,
	H2C_WOWLAN_UPDATE_IV_CMD,			
	H2C_TMP4,
	MAX_H2CCMD					
};

#define FW_CMD_IO_CLR(rtlpriv, _Bit)				\
	do {							\
		udelay(1000);					\
		rtlpriv->rtlhal.fwcmd_iomap &= (~_Bit);		\
	} while (0);

#define FW_CMD_IO_UPDATE(rtlpriv, _val)				\
	rtlpriv->rtlhal.fwcmd_iomap = _val;

#define FW_CMD_IO_SET(rtlpriv, _val)				\
	do {							\
		rtl_write_word(rtlpriv, LBUS_MON_ADDR, (u16)_val);	\
		FW_CMD_IO_UPDATE(rtlpriv, _val);		\
	} while (0);

#define FW_CMD_PARA_SET(rtlpriv, _val)				\
	do {							\
		rtl_write_dword(rtlpriv, LBUS_ADDR_MASK, _val);	\
		rtlpriv->rtlhal.fwcmd_ioparam = _val;		\
	} while (0);

#define FW_CMD_IO_QUERY(rtlpriv)				\
	(u16)(rtlpriv->rtlhal.fwcmd_iomap)
#define FW_CMD_IO_PARA_QUERY(rtlpriv)				\
	((u32)(rtlpriv->rtlhal.fwcmd_ioparam))

int rtl92s_download_fw(struct ieee80211_hw *hw);
void rtl92s_set_fw_pwrmode_cmd(struct ieee80211_hw *hw, u8 mode);
void rtl92s_set_fw_joinbss_report_cmd(struct ieee80211_hw *hw,
				      u8 mstatus, u8 ps_qosinfo);

#endif

