/*
 * Copyright (c) 2010 Broadcom Corporation
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */


#ifndef _BRCMF_H_
#define _BRCMF_H_

#define BRCMF_VERSION_STR		"4.218.248.5"

#define BRCMF_C_UP				2
#define BRCMF_C_SET_PROMISC			10
#define BRCMF_C_GET_RATE			12
#define BRCMF_C_GET_INFRA			19
#define BRCMF_C_SET_INFRA			20
#define BRCMF_C_GET_AUTH			21
#define BRCMF_C_SET_AUTH			22
#define BRCMF_C_GET_BSSID			23
#define BRCMF_C_GET_SSID			25
#define BRCMF_C_SET_SSID			26
#define BRCMF_C_GET_CHANNEL			29
#define BRCMF_C_GET_SRL				31
#define BRCMF_C_GET_LRL				33
#define BRCMF_C_GET_RADIO			37
#define BRCMF_C_SET_RADIO			38
#define BRCMF_C_GET_PHYTYPE			39
#define BRCMF_C_SET_KEY				45
#define BRCMF_C_SET_PASSIVE_SCAN		49
#define BRCMF_C_SCAN				50
#define BRCMF_C_SCAN_RESULTS			51
#define BRCMF_C_DISASSOC			52
#define BRCMF_C_REASSOC				53
#define BRCMF_C_SET_ROAM_TRIGGER		55
#define BRCMF_C_SET_ROAM_DELTA			57
#define BRCMF_C_GET_DTIMPRD			77
#define BRCMF_C_SET_COUNTRY			84
#define BRCMF_C_GET_PM				85
#define BRCMF_C_SET_PM				86
#define BRCMF_C_GET_AP				117
#define BRCMF_C_SET_AP				118
#define BRCMF_C_GET_RSSI			127
#define BRCMF_C_GET_WSEC			133
#define BRCMF_C_SET_WSEC			134
#define BRCMF_C_GET_PHY_NOISE			135
#define BRCMF_C_GET_BSS_INFO			136
#define BRCMF_C_SET_SCAN_CHANNEL_TIME		185
#define BRCMF_C_SET_SCAN_UNASSOC_TIME		187
#define BRCMF_C_SCB_DEAUTHENTICATE_FOR_REASON	201
#define BRCMF_C_GET_VALID_CHANNELS		217
#define BRCMF_C_GET_KEY_PRIMARY			235
#define BRCMF_C_SET_KEY_PRIMARY			236
#define BRCMF_C_SET_SCAN_PASSIVE_TIME		258
#define BRCMF_C_GET_VAR				262
#define BRCMF_C_SET_VAR				263

#define	WLC_PHY_TYPE_A		0
#define	WLC_PHY_TYPE_B		1
#define	WLC_PHY_TYPE_G		2
#define	WLC_PHY_TYPE_N		4
#define	WLC_PHY_TYPE_LP		5
#define	WLC_PHY_TYPE_SSN	6
#define	WLC_PHY_TYPE_HT		7
#define	WLC_PHY_TYPE_LCN	8
#define	WLC_PHY_TYPE_NULL	0xf

#define BRCMF_EVENTING_MASK_LEN	16

#define TOE_TX_CSUM_OL		0x00000001
#define TOE_RX_CSUM_OL		0x00000002

#define	BRCMF_BSS_INFO_VERSION	109 

#define BRCMF_SCAN_PARAMS_FIXED_SIZE 64

#define BRCMF_SCAN_PARAMS_COUNT_MASK 0x0000ffff
#define BRCMF_SCAN_PARAMS_NSSID_SHIFT 16

#define BRCMF_SCAN_ACTION_START      1
#define BRCMF_SCAN_ACTION_CONTINUE   2
#define WL_SCAN_ACTION_ABORT      3

#define BRCMF_ISCAN_REQ_VERSION 1

#define BRCMF_SCAN_RESULTS_SUCCESS	0
#define BRCMF_SCAN_RESULTS_PARTIAL	1
#define BRCMF_SCAN_RESULTS_PENDING	2
#define BRCMF_SCAN_RESULTS_ABORTED	3
#define BRCMF_SCAN_RESULTS_NO_MEM	4

#define WL_SOFT_KEY	(1 << 0)
#define BRCMF_PRIMARY_KEY	(1 << 1)
#define WL_KF_RES_4	(1 << 4)
#define WL_KF_RES_5	(1 << 5)
#define WL_IBSS_PEER_GROUP_KEY	(1 << 6)

#define BRCMF_MAX_IFS	16

#define DOT11_BSSTYPE_ANY			2
#define DOT11_MAX_DEFAULT_KEYS	4

#define BRCMF_EVENT_MSG_LINK		0x01
#define BRCMF_EVENT_MSG_FLUSHTXQ	0x02
#define BRCMF_EVENT_MSG_GROUP		0x04

struct brcmf_event_msg {
	__be16 version;
	__be16 flags;
	__be32 event_type;
	__be32 status;
	__be32 reason;
	__be32 auth_type;
	__be32 datalen;
	u8 addr[ETH_ALEN];
	char ifname[IFNAMSIZ];
} __packed;

struct brcm_ethhdr {
	u16 subtype;
	u16 length;
	u8 version;
	u8 oui[3];
	u16 usr_subtype;
} __packed;

struct brcmf_event {
	struct ethhdr eth;
	struct brcm_ethhdr hdr;
	struct brcmf_event_msg msg;
} __packed;

#define BRCMF_E_SET_SSID			0
#define BRCMF_E_JOIN				1
#define BRCMF_E_START				2
#define BRCMF_E_AUTH				3
#define BRCMF_E_AUTH_IND			4
#define BRCMF_E_DEAUTH				5
#define BRCMF_E_DEAUTH_IND			6
#define BRCMF_E_ASSOC				7
#define BRCMF_E_ASSOC_IND			8
#define BRCMF_E_REASSOC				9
#define BRCMF_E_REASSOC_IND			10
#define BRCMF_E_DISASSOC			11
#define BRCMF_E_DISASSOC_IND			12
#define BRCMF_E_QUIET_START			13
#define BRCMF_E_QUIET_END			14
#define BRCMF_E_BEACON_RX			15
#define BRCMF_E_LINK				16
#define BRCMF_E_MIC_ERROR			17
#define BRCMF_E_NDIS_LINK			18
#define BRCMF_E_ROAM				19
#define BRCMF_E_TXFAIL				20
#define BRCMF_E_PMKID_CACHE			21
#define BRCMF_E_RETROGRADE_TSF			22
#define BRCMF_E_PRUNE				23
#define BRCMF_E_AUTOAUTH			24
#define BRCMF_E_EAPOL_MSG			25
#define BRCMF_E_SCAN_COMPLETE			26
#define BRCMF_E_ADDTS_IND			27
#define BRCMF_E_DELTS_IND			28
#define BRCMF_E_BCNSENT_IND			29
#define BRCMF_E_BCNRX_MSG			30
#define BRCMF_E_BCNLOST_MSG			31
#define BRCMF_E_ROAM_PREP			32
#define BRCMF_E_PFN_NET_FOUND			33
#define BRCMF_E_PFN_NET_LOST			34
#define BRCMF_E_RESET_COMPLETE			35
#define BRCMF_E_JOIN_START			36
#define BRCMF_E_ROAM_START			37
#define BRCMF_E_ASSOC_START			38
#define BRCMF_E_IBSS_ASSOC			39
#define BRCMF_E_RADIO				40
#define BRCMF_E_PSM_WATCHDOG			41
#define BRCMF_E_PROBREQ_MSG			44
#define BRCMF_E_SCAN_CONFIRM_IND		45
#define BRCMF_E_PSK_SUP				46
#define BRCMF_E_COUNTRY_CODE_CHANGED		47
#define	BRCMF_E_EXCEEDED_MEDIUM_TIME		48
#define BRCMF_E_ICV_ERROR			49
#define BRCMF_E_UNICAST_DECODE_ERROR		50
#define BRCMF_E_MULTICAST_DECODE_ERROR		51
#define BRCMF_E_TRACE				52
#define BRCMF_E_IF				54
#define BRCMF_E_RSSI				56
#define BRCMF_E_PFN_SCAN_COMPLETE		57
#define BRCMF_E_EXTLOG_MSG			58
#define BRCMF_E_ACTION_FRAME			59
#define BRCMF_E_ACTION_FRAME_COMPLETE		60
#define BRCMF_E_PRE_ASSOC_IND			61
#define BRCMF_E_PRE_REASSOC_IND			62
#define BRCMF_E_CHANNEL_ADOPTED			63
#define BRCMF_E_AP_STARTED			64
#define BRCMF_E_DFS_AP_STOP			65
#define BRCMF_E_DFS_AP_RESUME			66
#define BRCMF_E_RESERVED1			67
#define BRCMF_E_RESERVED2			68
#define BRCMF_E_ESCAN_RESULT			69
#define BRCMF_E_ACTION_FRAME_OFF_CHAN_COMPLETE	70
#define BRCMF_E_DCS_REQUEST			73

#define BRCMF_E_FIFO_CREDIT_MAP			74

#define BRCMF_E_LAST				75

#define BRCMF_E_STATUS_SUCCESS			0
#define BRCMF_E_STATUS_FAIL			1
#define BRCMF_E_STATUS_TIMEOUT			2
#define BRCMF_E_STATUS_NO_NETWORKS		3
#define BRCMF_E_STATUS_ABORT			4
#define BRCMF_E_STATUS_NO_ACK			5
#define BRCMF_E_STATUS_UNSOLICITED		6
#define BRCMF_E_STATUS_ATTEMPT			7
#define BRCMF_E_STATUS_PARTIAL			8
#define BRCMF_E_STATUS_NEWSCAN			9
#define BRCMF_E_STATUS_NEWASSOC			10
#define BRCMF_E_STATUS_11HQUIET			11
#define BRCMF_E_STATUS_SUPPRESS			12
#define BRCMF_E_STATUS_NOCHANS			13
#define BRCMF_E_STATUS_CS_ABORT			15
#define BRCMF_E_STATUS_ERROR			16

#define BRCMF_E_REASON_INITIAL_ASSOC		0
#define BRCMF_E_REASON_LOW_RSSI			1
#define BRCMF_E_REASON_DEAUTH			2
#define BRCMF_E_REASON_DISASSOC			3
#define BRCMF_E_REASON_BCNS_LOST		4
#define BRCMF_E_REASON_MINTXRATE		9
#define BRCMF_E_REASON_TXFAIL			10

#define BRCMF_E_REASON_FAST_ROAM_FAILED		5
#define BRCMF_E_REASON_DIRECTED_ROAM		6
#define BRCMF_E_REASON_TSPEC_REJECTED		7
#define BRCMF_E_REASON_BETTER_AP		8

#define BRCMF_E_PRUNE_ENCR_MISMATCH		1
#define BRCMF_E_PRUNE_BCAST_BSSID		2
#define BRCMF_E_PRUNE_MAC_DENY			3
#define BRCMF_E_PRUNE_MAC_NA			4
#define BRCMF_E_PRUNE_REG_PASSV			5
#define BRCMF_E_PRUNE_SPCT_MGMT			6
#define BRCMF_E_PRUNE_RADAR			7
#define BRCMF_E_RSN_MISMATCH			8
#define BRCMF_E_PRUNE_NO_COMMON_RATES		9
#define BRCMF_E_PRUNE_BASIC_RATES		10
#define BRCMF_E_PRUNE_CIPHER_NA			12
#define BRCMF_E_PRUNE_KNOWN_STA			13
#define BRCMF_E_PRUNE_WDS_PEER			15
#define BRCMF_E_PRUNE_QBSS_LOAD			16
#define BRCMF_E_PRUNE_HOME_AP			17

#define BRCMF_E_SUP_OTHER			0
#define BRCMF_E_SUP_DECRYPT_KEY_DATA		1
#define BRCMF_E_SUP_BAD_UCAST_WEP128		2
#define BRCMF_E_SUP_BAD_UCAST_WEP40		3
#define BRCMF_E_SUP_UNSUP_KEY_LEN		4
#define BRCMF_E_SUP_PW_KEY_CIPHER		5
#define BRCMF_E_SUP_MSG3_TOO_MANY_IE		6
#define BRCMF_E_SUP_MSG3_IE_MISMATCH		7
#define BRCMF_E_SUP_NO_INSTALL_FLAG		8
#define BRCMF_E_SUP_MSG3_NO_GTK			9
#define BRCMF_E_SUP_GRP_KEY_CIPHER		10
#define BRCMF_E_SUP_GRP_MSG1_NO_GTK		11
#define BRCMF_E_SUP_GTK_DECRYPT_FAIL		12
#define BRCMF_E_SUP_SEND_FAIL			13
#define BRCMF_E_SUP_DEAUTH			14

#define BRCMF_E_IF_ADD				1
#define BRCMF_E_IF_DEL				2
#define BRCMF_E_IF_CHANGE			3

#define BRCMF_E_IF_ROLE_STA			0
#define BRCMF_E_IF_ROLE_AP			1
#define BRCMF_E_IF_ROLE_WDS			2

#define BRCMF_E_LINK_BCN_LOSS			1
#define BRCMF_E_LINK_DISASSOC			2
#define BRCMF_E_LINK_ASSOC_REC			3
#define BRCMF_E_LINK_BSSCFG_DIS			4

struct brcmf_pkt_filter_pattern_le {
	__le32 offset;
	
	__le32 size_bytes;
	u8 mask_and_pattern[1];
};

struct brcmf_pkt_filter_le {
	__le32 id;		
	__le32 type;		
	__le32 negate_match;	
	union {			
		struct brcmf_pkt_filter_pattern_le pattern; 
	} u;
};

struct brcmf_pkt_filter_enable_le {
	__le32 id;		
	__le32 enable;		
};

struct brcmf_bss_info_le {
	__le32 version;		
	__le32 length;		
	u8 BSSID[ETH_ALEN];
	__le16 beacon_period;	
	__le16 capability;	
	u8 SSID_len;
	u8 SSID[32];
	struct {
		__le32 count;   
		u8 rates[16]; 
	} rateset;		
	__le16 chanspec;	
	__le16 atim_window;	
	u8 dtim_period;	
	__le16 RSSI;		
	s8 phy_noise;		

	u8 n_cap;		
	
	__le32 nbss_cap;
	u8 ctl_ch;		
	__le32 reserved32[1];	
	u8 flags;		
	u8 reserved[3];	
	u8 basic_mcs[MCSSET_LEN];	

	__le16 ie_offset;	
	__le32 ie_length;	
	__le16 SNR;		
	
	
};

struct brcm_rateset_le {
	
	__le32 count;
	
	u8 rates[WL_NUMRATES];
};

struct brcmf_ssid {
	u32 SSID_len;
	unsigned char SSID[32];
};

struct brcmf_ssid_le {
	__le32 SSID_len;
	unsigned char SSID[32];
};

struct brcmf_scan_params_le {
	struct brcmf_ssid_le ssid_le;	
	u8 bssid[ETH_ALEN];	
	s8 bss_type;		
	u8 scan_type;	
	__le32 nprobes;	  
	__le32 active_time;	
	__le32 passive_time;	
	__le32 home_time;	
	__le32 channel_num;	
	__le16 channel_list[1];	
};

struct brcmf_iscan_params_le {
	__le32 version;
	__le16 action;
	__le16 scan_duration;
	struct brcmf_scan_params_le params_le;
};

struct brcmf_scan_results {
	u32 buflen;
	u32 version;
	u32 count;
	struct brcmf_bss_info_le bss_info_le[];
};

struct brcmf_scan_results_le {
	__le32 buflen;
	__le32 version;
	__le32 count;
};

struct brcmf_assoc_params_le {
	
	u8 bssid[ETH_ALEN];
	__le32 chanspec_num;
	
	__le16 chanspec_list[1];
};

struct brcmf_join_params {
	struct brcmf_ssid_le ssid_le;
	struct brcmf_assoc_params_le params_le;
};

struct brcmf_iscan_results {
	union {
		u32 status;
		__le32 status_le;
	};
	union {
		struct brcmf_scan_results results;
		struct brcmf_scan_results_le results_le;
	};
};

#define BRCMF_ISCAN_RESULTS_FIXED_SIZE \
	(sizeof(struct brcmf_scan_results) + \
	 offsetof(struct brcmf_iscan_results, results))

struct brcmf_wsec_key {
	u32 index;		
	u32 len;		
	u8 data[WLAN_MAX_KEY_LEN];	
	u32 pad_1[18];
	u32 algo;	
	u32 flags;	
	u32 pad_2[3];
	u32 iv_initialized;	
	u32 pad_3;
	
	struct {
		u32 hi;	
		u16 lo;	
	} rxiv;
	u32 pad_4[2];
	u8 ea[ETH_ALEN];	
};

struct brcmf_wsec_key_le {
	__le32 index;		
	__le32 len;		
	u8 data[WLAN_MAX_KEY_LEN];	
	__le32 pad_1[18];
	__le32 algo;	
	__le32 flags;	
	__le32 pad_2[3];
	__le32 iv_initialized;	
	__le32 pad_3;
	
	struct {
		__le32 hi;	
		__le16 lo;	
	} rxiv;
	__le32 pad_4[2];
	u8 ea[ETH_ALEN];	
};

struct brcmf_scb_val_le {
	__le32 val;
	u8 ea[ETH_ALEN];
};

struct brcmf_channel_info_le {
	__le32 hw_channel;
	__le32 target_channel;
	__le32 scan_channel;
};

struct brcmf_dcmd {
	uint cmd;		
	void *buf;		
	uint len;		
	u8 set;			
	uint used;		/* bytes read or written (optional) */
	uint needed;		
};

struct brcmf_proto;	
struct brcmf_cfg80211_dev; 

struct brcmf_pub {
	
	struct brcmf_bus *bus_if;
	struct brcmf_proto *prot;
	struct brcmf_cfg80211_dev *config;
	struct device *dev;		

	
	uint hdrlen;		
	uint rxsz;		
	u8 wme_dp;		

	
	bool iswl;		
	unsigned long drv_version;	
	u8 mac[ETH_ALEN];		

	

	
	unsigned long tx_multicast;
	
	unsigned long rx_flushed;
	
	unsigned long wd_dpc_sched;

	
	unsigned long fc_packets;

	
	int bcmerror;

	
	int dongle_error;

	
	int suspend_disable_flag;	
	int in_suspend;		
	int dtim_skip;		

	
	char *pktfilter[100];
	int pktfilter_count;

	u8 country_code[BRCM_CNTRY_BUF_SZ];
	char eventmask[BRCMF_EVENTING_MASK_LEN];

	struct brcmf_if *iflist[BRCMF_MAX_IFS];

	struct mutex proto_block;

	struct work_struct setmacaddr_work;
	struct work_struct multicast_work;
	u8 macvalue[ETH_ALEN];
	atomic_t pend_8021x_cnt;
};

struct brcmf_if_event {
	u8 ifidx;
	u8 action;
	u8 flags;
	u8 bssidx;
};

struct bcmevent_name {
	uint event;
	const char *name;
};

extern const struct bcmevent_name bcmevent_names[];

extern uint brcmf_c_mkiovar(char *name, char *data, uint datalen,
			  char *buf, uint len);

extern int brcmf_net_attach(struct brcmf_pub *drvr, int idx);
extern int brcmf_netdev_wait_pend8021x(struct net_device *ndev);

extern s32 brcmf_exec_dcmd(struct net_device *dev, u32 cmd, void *arg, u32 len);

extern char *brcmf_ifname(struct brcmf_pub *drvr, int idx);

extern int brcmf_proto_cdc_query_dcmd(struct brcmf_pub *drvr, int ifidx,
				       uint cmd, void *buf, uint len);

#ifdef DEBUG
extern int brcmf_write_to_file(struct brcmf_pub *drvr, const u8 *buf, int size);
#endif				

extern int brcmf_ifname2idx(struct brcmf_pub *drvr, char *name);
extern int brcmf_c_host_event(struct brcmf_pub *drvr, int *idx,
			      void *pktdata, struct brcmf_event_msg *,
			      void **data_ptr);

extern void brcmf_del_if(struct brcmf_pub *drvr, int ifidx);

extern int brcmf_sendpkt(struct brcmf_pub *drvr, int ifidx,\
			 struct sk_buff *pkt);

extern void brcmf_c_pktfilter_offload_set(struct brcmf_pub *drvr, char *arg);
extern void brcmf_c_pktfilter_offload_enable(struct brcmf_pub *drvr, char *arg,
					     int enable, int master_mode);

#define	BRCMF_DCMD_SMLEN	256	
#define BRCMF_DCMD_MEDLEN	1536	
#define	BRCMF_DCMD_MAXLEN	8192	

#endif				
