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

#ifndef _wl_cfg80211_h_
#define _wl_cfg80211_h_

struct brcmf_cfg80211_conf;
struct brcmf_cfg80211_iface;
struct brcmf_cfg80211_priv;
struct brcmf_cfg80211_security;
struct brcmf_cfg80211_ibss;

#define WL_DBG_NONE		0
#define WL_DBG_CONN		(1 << 5)
#define WL_DBG_SCAN		(1 << 4)
#define WL_DBG_TRACE		(1 << 3)
#define WL_DBG_INFO		(1 << 1)
#define WL_DBG_ERR		(1 << 0)
#define WL_DBG_MASK		((WL_DBG_INFO | WL_DBG_ERR | WL_DBG_TRACE) | \
				(WL_DBG_SCAN) | (WL_DBG_CONN))

#define	WL_ERR(fmt, ...)					\
do {								\
	if (brcmf_dbg_level & WL_DBG_ERR) {			\
		if (net_ratelimit()) {				\
			pr_err("ERROR @%s : " fmt,		\
			       __func__, ##__VA_ARGS__);	\
		}						\
	}							\
} while (0)

#if (defined DEBUG)
#define	WL_INFO(fmt, ...)					\
do {								\
	if (brcmf_dbg_level & WL_DBG_INFO) {			\
		if (net_ratelimit()) {				\
			pr_err("INFO @%s : " fmt,		\
			       __func__, ##__VA_ARGS__);	\
		}						\
	}							\
} while (0)

#define	WL_TRACE(fmt, ...)					\
do {								\
	if (brcmf_dbg_level & WL_DBG_TRACE) {			\
		if (net_ratelimit()) {				\
			pr_err("TRACE @%s : " fmt,		\
			       __func__, ##__VA_ARGS__);	\
		}						\
	}							\
} while (0)

#define	WL_SCAN(fmt, ...)					\
do {								\
	if (brcmf_dbg_level & WL_DBG_SCAN) {			\
		if (net_ratelimit()) {				\
			pr_err("SCAN @%s : " fmt,		\
			       __func__, ##__VA_ARGS__);	\
		}						\
	}							\
} while (0)

#define	WL_CONN(fmt, ...)					\
do {								\
	if (brcmf_dbg_level & WL_DBG_CONN) {			\
		if (net_ratelimit()) {				\
			pr_err("CONN @%s : " fmt,		\
			       __func__, ##__VA_ARGS__);	\
		}						\
	}							\
} while (0)

#else 
#define	WL_INFO(fmt, args...)
#define	WL_TRACE(fmt, args...)
#define	WL_SCAN(fmt, args...)
#define	WL_CONN(fmt, args...)
#endif 

#define WL_NUM_SCAN_MAX		1
#define WL_NUM_PMKIDS_MAX	MAXPMKID	
#define WL_SCAN_BUF_MAX			(1024 * 8)
#define WL_TLV_INFO_MAX			1024
#define WL_BSS_INFO_MAX			2048
#define WL_ASSOC_INFO_MAX	512	
#define WL_DCMD_LEN_MAX	1024
#define WL_EXTRA_BUF_MAX	2048
#define WL_ISCAN_BUF_MAX	2048	
#define WL_ISCAN_TIMER_INTERVAL_MS	3000
#define WL_SCAN_ERSULTS_LAST	(BRCMF_SCAN_RESULTS_NO_MEM+1)
#define WL_AP_MAX	256	

#define WL_ROAM_TRIGGER_LEVEL		-75
#define WL_ROAM_DELTA			20
#define WL_BEACON_TIMEOUT		3

#define WL_SCAN_CHANNEL_TIME		40
#define WL_SCAN_UNASSOC_TIME		40
#define WL_SCAN_PASSIVE_TIME		120

enum wl_status {
	WL_STATUS_READY,
	WL_STATUS_SCANNING,
	WL_STATUS_SCAN_ABORTING,
	WL_STATUS_CONNECTING,
	WL_STATUS_CONNECTED
};

enum wl_mode {
	WL_MODE_BSS,
	WL_MODE_IBSS,
	WL_MODE_AP
};

enum wl_prof_list {
	WL_PROF_MODE,
	WL_PROF_SSID,
	WL_PROF_SEC,
	WL_PROF_IBSS,
	WL_PROF_BAND,
	WL_PROF_BSSID,
	WL_PROF_ACT,
	WL_PROF_BEACONINT,
	WL_PROF_DTIMPERIOD
};

enum wl_iscan_state {
	WL_ISCAN_STATE_IDLE,
	WL_ISCAN_STATE_SCANING
};

struct brcmf_cfg80211_conf {
	u32 mode;		
	u32 frag_threshold;
	u32 rts_threshold;
	u32 retry_short;
	u32 retry_long;
	s32 tx_power;
	struct ieee80211_channel channel;
};

struct brcmf_cfg80211_event_loop {
	s32(*handler[BRCMF_E_LAST]) (struct brcmf_cfg80211_priv *cfg_priv,
				     struct net_device *ndev,
				     const struct brcmf_event_msg *e,
				     void *data);
};

struct brcmf_cfg80211_iface {
	struct brcmf_cfg80211_priv *cfg_priv;
};

struct brcmf_cfg80211_dev {
	void *driver_data;	
};

struct brcmf_cfg80211_scan_req {
	struct brcmf_ssid_le ssid_le;
};

struct brcmf_cfg80211_ie {
	u16 offset;
	u8 buf[WL_TLV_INFO_MAX];
};

struct brcmf_cfg80211_event_q {
	struct list_head evt_q_list;
	u32 etype;
	struct brcmf_event_msg emsg;
	s8 edata[1];
};

struct brcmf_cfg80211_security {
	u32 wpa_versions;
	u32 auth_type;
	u32 cipher_pairwise;
	u32 cipher_group;
	u32 wpa_auth;
};

struct brcmf_cfg80211_ibss {
	u8 beacon_interval;	
	u8 atim;		
	s8 join_only;
	u8 band;
	u8 channel;
};

struct brcmf_cfg80211_profile {
	u32 mode;
	struct brcmf_ssid ssid;
	u8 bssid[ETH_ALEN];
	u16 beacon_interval;
	u8 dtim_period;
	struct brcmf_cfg80211_security sec;
	struct brcmf_cfg80211_ibss ibss;
	s32 band;
};

struct brcmf_cfg80211_iscan_eloop {
	s32 (*handler[WL_SCAN_ERSULTS_LAST])
		(struct brcmf_cfg80211_priv *cfg_priv);
};

struct brcmf_cfg80211_iscan_ctrl {
	struct net_device *ndev;
	struct timer_list timer;
	u32 timer_ms;
	u32 timer_on;
	s32 state;
	struct work_struct work;
	struct brcmf_cfg80211_iscan_eloop el;
	void *data;
	s8 dcmd_buf[BRCMF_DCMD_SMLEN];
	s8 scan_buf[WL_ISCAN_BUF_MAX];
};

struct brcmf_cfg80211_connect_info {
	u8 *req_ie;
	s32 req_ie_len;
	u8 *resp_ie;
	s32 resp_ie_len;
};

struct brcmf_cfg80211_assoc_ielen_le {
	__le32 req_len;
	__le32 resp_len;
};

struct brcmf_cfg80211_pmk_list {
	struct pmkid_list pmkids;
	struct pmkid foo[MAXPMKID - 1];
};

struct brcmf_cfg80211_priv {
	struct wireless_dev *wdev;	
	struct brcmf_cfg80211_conf *conf;	
	struct cfg80211_scan_request *scan_request;	
	struct brcmf_cfg80211_event_loop el;	
	struct list_head evt_q_list;	
	spinlock_t	 evt_q_lock;	
	struct mutex usr_sync;	
	struct brcmf_scan_results *bss_list;	
	struct brcmf_scan_results *scan_results;
	struct brcmf_cfg80211_scan_req *scan_req_int;	
	struct wl_cfg80211_bss_info *bss_info;	
	struct brcmf_cfg80211_ie ie;	
	struct brcmf_cfg80211_profile *profile;	
	struct brcmf_cfg80211_iscan_ctrl *iscan;	
	struct brcmf_cfg80211_connect_info conn_info; 
	struct brcmf_cfg80211_pmk_list *pmk_list;	
	struct work_struct event_work;	
	unsigned long status;		
	void *pub;
	u32 channel;		
	bool iscan_on;		
	bool iscan_kickstart;	
	bool active_scan;	
	bool ibss_starter;	
	bool link_up;		
	bool pwr_save;		
	bool dongle_up;		
	bool roam_on;		
	bool scan_tried;	
	u8 *dcmd_buf;		
	u8 *extra_buf;		
	struct dentry *debugfsdir;
	u8 ci[0] __aligned(NETDEV_ALIGN);
};

static inline struct wiphy *cfg_to_wiphy(struct brcmf_cfg80211_priv *w)
{
	return w->wdev->wiphy;
}

static inline struct brcmf_cfg80211_priv *wiphy_to_cfg(struct wiphy *w)
{
	return (struct brcmf_cfg80211_priv *)(wiphy_priv(w));
}

static inline struct brcmf_cfg80211_priv *wdev_to_cfg(struct wireless_dev *wd)
{
	return (struct brcmf_cfg80211_priv *)(wdev_priv(wd));
}

static inline struct net_device *cfg_to_ndev(struct brcmf_cfg80211_priv *cfg)
{
	return cfg->wdev->netdev;
}

static inline struct brcmf_cfg80211_priv *ndev_to_cfg(struct net_device *ndev)
{
	return wdev_to_cfg(ndev->ieee80211_ptr);
}

#define iscan_to_cfg(i) ((struct brcmf_cfg80211_priv *)(i->data))
#define cfg_to_iscan(w) (w->iscan)

static inline struct
brcmf_cfg80211_connect_info *cfg_to_conn(struct brcmf_cfg80211_priv *cfg)
{
	return &cfg->conn_info;
}

extern struct brcmf_cfg80211_dev *brcmf_cfg80211_attach(struct net_device *ndev,
							struct device *busdev,
							void *data);
extern void brcmf_cfg80211_detach(struct brcmf_cfg80211_dev *cfg);

extern void brcmf_cfg80211_event(struct net_device *ndev,
				 const struct brcmf_event_msg *e, void *data);
extern s32 brcmf_cfg80211_up(struct brcmf_cfg80211_dev *cfg_dev);
extern s32 brcmf_cfg80211_down(struct brcmf_cfg80211_dev *cfg_dev);

#endif				
