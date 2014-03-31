/*
 *  PS3 gelic network driver.
 *
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * Copyright 2007 Sony Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef _GELIC_WIRELESS_H
#define _GELIC_WIRELESS_H

#include <linux/wireless.h>
#include <net/iw_handler.h>


enum gelic_lv1_wl_event {
	GELIC_LV1_WL_EVENT_DEVICE_READY   = 0x01, 
	GELIC_LV1_WL_EVENT_SCAN_COMPLETED = 0x02, 
	GELIC_LV1_WL_EVENT_DEAUTH         = 0x04, 
	GELIC_LV1_WL_EVENT_BEACON_LOST    = 0x08, 
	GELIC_LV1_WL_EVENT_CONNECTED      = 0x10, 
	GELIC_LV1_WL_EVENT_WPA_CONNECTED  = 0x20, 
	GELIC_LV1_WL_EVENT_WPA_ERROR      = 0x40, 
};

enum gelic_eurus_command {
	GELIC_EURUS_CMD_ASSOC		=  1, 
	GELIC_EURUS_CMD_DISASSOC	=  2, 
	GELIC_EURUS_CMD_START_SCAN	=  3, 
	GELIC_EURUS_CMD_GET_SCAN	=  4, 
	GELIC_EURUS_CMD_SET_COMMON_CFG	=  5, 
	GELIC_EURUS_CMD_GET_COMMON_CFG	=  6, 
	GELIC_EURUS_CMD_SET_WEP_CFG	=  7, 
	GELIC_EURUS_CMD_GET_WEP_CFG	=  8, 
	GELIC_EURUS_CMD_SET_WPA_CFG	=  9, 
	GELIC_EURUS_CMD_GET_WPA_CFG	= 10, 
	GELIC_EURUS_CMD_GET_RSSI_CFG	= 11, 
	GELIC_EURUS_CMD_MAX_INDEX
};

enum gelic_eurus_bss_type {
	GELIC_EURUS_BSS_INFRA = 0,
	GELIC_EURUS_BSS_ADHOC = 1, 
};

enum gelic_eurus_auth_method {
	GELIC_EURUS_AUTH_OPEN = 0, 
	GELIC_EURUS_AUTH_SHARED = 1, 
};

enum gelic_eurus_opmode {
	GELIC_EURUS_OPMODE_11BG = 0, 
	GELIC_EURUS_OPMODE_11B = 1, 
	GELIC_EURUS_OPMODE_11G = 2, 
};

struct gelic_eurus_common_cfg {
	
	u16 scan_index;
	u16 bss_type;    
	u16 auth_method; 
	u16 op_mode; 
} __packed;


enum gelic_eurus_wep_security {
	GELIC_EURUS_WEP_SEC_NONE	= 0,
	GELIC_EURUS_WEP_SEC_40BIT	= 1,
	GELIC_EURUS_WEP_SEC_104BIT	= 2,
};

struct gelic_eurus_wep_cfg {
	
	u16 security;
	u8 key[4][16];
} __packed;

enum gelic_eurus_wpa_security {
	GELIC_EURUS_WPA_SEC_NONE		= 0x0000,
	
	GELIC_EURUS_WPA_SEC_WPA_TKIP_TKIP	= 0x0001,
	
	GELIC_EURUS_WPA_SEC_WPA_AES_AES		= 0x0002,
	
	GELIC_EURUS_WPA_SEC_WPA2_TKIP_TKIP	= 0x0004,
	
	GELIC_EURUS_WPA_SEC_WPA2_AES_AES	= 0x0008,
	
	GELIC_EURUS_WPA_SEC_WPA_TKIP_AES	= 0x0010,
	
	GELIC_EURUS_WPA_SEC_WPA2_TKIP_AES	= 0x0020,
};

enum gelic_eurus_wpa_psk_type {
	GELIC_EURUS_WPA_PSK_PASSPHRASE	= 0, 
	GELIC_EURUS_WPA_PSK_BIN		= 1, 
};

#define GELIC_WL_EURUS_PSK_MAX_LEN	64
#define WPA_PSK_LEN			32 

struct gelic_eurus_wpa_cfg {
	
	u16 security;
	u16 psk_type; 
	u8 psk[GELIC_WL_EURUS_PSK_MAX_LEN]; 
} __packed;

enum gelic_eurus_scan_capability {
	GELIC_EURUS_SCAN_CAP_ADHOC	= 0x0000,
	GELIC_EURUS_SCAN_CAP_INFRA	= 0x0001,
	GELIC_EURUS_SCAN_CAP_MASK	= 0x0001,
};

enum gelic_eurus_scan_sec_type {
	GELIC_EURUS_SCAN_SEC_NONE	= 0x0000,
	GELIC_EURUS_SCAN_SEC_WEP	= 0x0100,
	GELIC_EURUS_SCAN_SEC_WPA	= 0x0200,
	GELIC_EURUS_SCAN_SEC_WPA2	= 0x0400,
	GELIC_EURUS_SCAN_SEC_MASK	= 0x0f00,
};

enum gelic_eurus_scan_sec_wep_type {
	GELIC_EURUS_SCAN_SEC_WEP_UNKNOWN	= 0x0000,
	GELIC_EURUS_SCAN_SEC_WEP_40		= 0x0001,
	GELIC_EURUS_SCAN_SEC_WEP_104		= 0x0002,
	GELIC_EURUS_SCAN_SEC_WEP_MASK		= 0x0003,
};

enum gelic_eurus_scan_sec_wpa_type {
	GELIC_EURUS_SCAN_SEC_WPA_UNKNOWN	= 0x0000,
	GELIC_EURUS_SCAN_SEC_WPA_TKIP		= 0x0001,
	GELIC_EURUS_SCAN_SEC_WPA_AES		= 0x0002,
	GELIC_EURUS_SCAN_SEC_WPA_MASK		= 0x0003,
};

struct gelic_eurus_scan_info {
	
	__be16 size;
	__be16 rssi; 
	__be16 channel; 
	__be16 beacon_period; 
	__be16 capability;
	__be16 security;
	u8  bssid[8]; 
	u8  essid[32]; 
	u8  rate[16]; 
	u8  ext_rate[16]; 
	__be32 reserved1;
	__be32 reserved2;
	__be32 reserved3;
	__be32 reserved4;
	u8 elements[0]; 
} __packed;

#define GELIC_EURUS_MAX_SCAN  (16)
struct gelic_wl_scan_info {
	struct list_head list;
	struct gelic_eurus_scan_info *hwinfo;

	int valid; 
	unsigned int eurus_index; 
	unsigned long last_scanned; 

	unsigned int rate_len;
	unsigned int rate_ext_len;
	unsigned int essid_len;
};

struct gelic_eurus_rssi_info {
	
	__be16 rssi;
} __packed;


enum gelic_wl_info_status_bit {
	GELIC_WL_STAT_CONFIGURED,
	GELIC_WL_STAT_CH_INFO,   
	GELIC_WL_STAT_ESSID_SET, 
	GELIC_WL_STAT_BSSID_SET, 
	GELIC_WL_STAT_WPA_PSK_SET, 
	GELIC_WL_STAT_WPA_LEVEL_SET, 
};

enum gelic_wl_scan_state {
	
	GELIC_WL_SCAN_STAT_INIT,
	
	GELIC_WL_SCAN_STAT_SCANNING,
	
	GELIC_WL_SCAN_STAT_GOT_LIST,
};

enum gelic_wl_cipher_method {
	GELIC_WL_CIPHER_NONE,
	GELIC_WL_CIPHER_WEP,
	GELIC_WL_CIPHER_TKIP,
	GELIC_WL_CIPHER_AES,
};

enum gelic_wl_wpa_level {
	GELIC_WL_WPA_LEVEL_NONE,
	GELIC_WL_WPA_LEVEL_WPA,
	GELIC_WL_WPA_LEVEL_WPA2,
};

enum gelic_wl_assoc_state {
	GELIC_WL_ASSOC_STAT_DISCONN,
	GELIC_WL_ASSOC_STAT_ASSOCIATING,
	GELIC_WL_ASSOC_STAT_ASSOCIATED,
};
#define GELIC_WEP_KEYS 4
struct gelic_wl_info {
	
	struct mutex scan_lock;
	struct list_head network_list;
	struct list_head network_free_list;
	struct gelic_wl_scan_info *networks;

	unsigned long scan_age; 
	enum gelic_wl_scan_state scan_stat;
	struct completion scan_done;

	
	struct workqueue_struct *eurus_cmd_queue;
	struct completion cmd_done_intr;

	
	struct workqueue_struct *event_queue;
	struct delayed_work event_work;

	
	unsigned long stat;
	enum gelic_eurus_auth_method auth_method; 
	enum gelic_wl_cipher_method group_cipher_method;
	enum gelic_wl_cipher_method pairwise_cipher_method;
	enum gelic_wl_wpa_level wpa_level; 

	
	struct mutex assoc_stat_lock;
	struct delayed_work assoc_work;
	enum gelic_wl_assoc_state assoc_stat;
	struct completion assoc_done;

	spinlock_t lock;
	u16 ch_info; 
	
	u8 key[GELIC_WEP_KEYS][IW_ENCODING_TOKEN_MAX];
	unsigned long key_enabled;
	unsigned int key_len[GELIC_WEP_KEYS];
	unsigned int current_key;
	
	u8 psk[GELIC_WL_EURUS_PSK_MAX_LEN];
	enum gelic_eurus_wpa_psk_type psk_type;
	unsigned int psk_len;

	u8 essid[IW_ESSID_MAX_SIZE];
	u8 bssid[ETH_ALEN]; 
	u8 active_bssid[ETH_ALEN]; 
	unsigned int essid_len;

	struct iw_public_data wireless_data;
	struct iw_statistics iwstat;
};

#define GELIC_WL_BSS_MAX_ENT 32
#define GELIC_WL_ASSOC_RETRY 50
static inline struct gelic_port *wl_port(struct gelic_wl_info *wl)
{
	return container_of((void *)wl, struct gelic_port, priv);
}
static inline struct gelic_wl_info *port_wl(struct gelic_port *port)
{
	return port_priv(port);
}

struct gelic_eurus_cmd {
	struct work_struct work;
	struct gelic_wl_info *wl;
	unsigned int cmd; 
	u64 tag;
	u64 size;
	void *buffer;
	unsigned int buf_size;
	struct completion done;
	int status;
	u64 cmd_status;
};

#define GELIC_WL_PRIV_SET_PSK		(SIOCIWFIRSTPRIV + 0)
#define GELIC_WL_PRIV_GET_PSK		(SIOCIWFIRSTPRIV + 1)

extern int gelic_wl_driver_probe(struct gelic_card *card);
extern int gelic_wl_driver_remove(struct gelic_card *card);
extern void gelic_wl_interrupt(struct net_device *netdev, u64 status);
#endif 
