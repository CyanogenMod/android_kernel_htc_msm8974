/*
 *  Copyright (C) 2008, cozybit Inc.
 *  Copyright (C) 2007, Red Hat, Inc.
 *  Copyright (C) 2003-2006, Marvell International Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or (at
 *  your option) any later version.
 */
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/kthread.h>
#include <net/mac80211.h>

#include "deb_defs.h"

#ifndef DRV_NAME
#define DRV_NAME "libertas_tf"
#endif

#define	MRVL_DEFAULT_RETRIES			9
#define MRVL_PER_PACKET_RATE			0x10
#define MRVL_MAX_BCN_SIZE			440
#define CMD_OPTION_WAITFORRSP			0x0002

#define CMD_RET(cmd)			(0x8000 | cmd)

#define CMD_GET_HW_SPEC				0x0003
#define CMD_802_11_RESET			0x0005
#define CMD_MAC_MULTICAST_ADR			0x0010
#define CMD_802_11_RADIO_CONTROL		0x001c
#define CMD_802_11_RF_CHANNEL			0x001d
#define CMD_802_11_RF_TX_POWER			0x001e
#define CMD_MAC_CONTROL				0x0028
#define CMD_802_11_MAC_ADDRESS			0x004d
#define	CMD_SET_BOOT2_VER			0x00a5
#define CMD_802_11_BEACON_CTRL			0x00b0
#define CMD_802_11_BEACON_SET			0x00cb
#define CMD_802_11_SET_MODE			0x00cc
#define CMD_802_11_SET_BSSID			0x00cd

#define CMD_ACT_GET			0x0000
#define CMD_ACT_SET			0x0001

#define CMD_ACT_HALT			0x0003

#define CMD_ACT_MAC_RX_ON			0x0001
#define CMD_ACT_MAC_TX_ON			0x0002
#define CMD_ACT_MAC_MULTICAST_ENABLE		0x0020
#define CMD_ACT_MAC_BROADCAST_ENABLE		0x0040
#define CMD_ACT_MAC_PROMISCUOUS_ENABLE		0x0080
#define CMD_ACT_MAC_ALL_MULTICAST_ENABLE	0x0100

#define CMD_TYPE_AUTO_PREAMBLE		0x0001
#define CMD_TYPE_SHORT_PREAMBLE		0x0002
#define CMD_TYPE_LONG_PREAMBLE		0x0003

#define TURN_ON_RF			0x01
#define RADIO_ON			0x01
#define RADIO_OFF			0x00

#define SET_AUTO_PREAMBLE		0x05
#define SET_SHORT_PREAMBLE		0x03
#define SET_LONG_PREAMBLE		0x01

#define CMD_OPT_802_11_RF_CHANNEL_GET	0x00
#define CMD_OPT_802_11_RF_CHANNEL_SET	0x01

enum lbtf_mode {
	LBTF_PASSIVE_MODE,
	LBTF_STA_MODE,
	LBTF_AP_MODE,
};

#define MACREG_INT_CODE_FIRMWARE_READY		48


#define MRVDRV_MAX_MULTICAST_LIST_SIZE	32
#define LBS_NUM_CMD_BUFFERS             10
#define LBS_CMD_BUFFER_SIZE             (2 * 1024)
#define MRVDRV_MAX_CHANNEL_SIZE		14
#define MRVDRV_SNAP_HEADER_LEN          8

#define	LBS_UPLD_SIZE			2312
#define DEV_NAME_LEN			32


#define MRVDRV_MAX_REGION_CODE			6
#define LBTF_REGDOMAIN_US	0x10
#define LBTF_REGDOMAIN_CA	0x20
#define LBTF_REGDOMAIN_EU	0x30
#define LBTF_REGDOMAIN_SP	0x31
#define LBTF_REGDOMAIN_FR	0x32
#define LBTF_REGDOMAIN_JP	0x40

#define SBI_EVENT_CAUSE_SHIFT		3


#define MRVDRV_RXPD_STATUS_OK                0x0001


#define EXTRA_LEN	36

#define MRVDRV_ETH_TX_PACKET_BUFFER_SIZE \
	(ETH_FRAME_LEN + sizeof(struct txpd) + EXTRA_LEN)

#define MRVDRV_ETH_RX_PACKET_BUFFER_SIZE \
	(ETH_FRAME_LEN + sizeof(struct rxpd) \
	 + MRVDRV_SNAP_HEADER_LEN + EXTRA_LEN)

#define	CMD_F_HOSTCMD		(1 << 0)
#define FW_CAPINFO_WPA  	(1 << 0)

#define RF_ANTENNA_1		0x1
#define RF_ANTENNA_2		0x2
#define RF_ANTENNA_AUTO		0xFFFF

#define LBTF_EVENT_BCN_SENT	55

enum mv_ms_type {
	MVMS_DAT = 0,
	MVMS_CMD = 1,
	MVMS_TXDONE = 2,
	MVMS_EVENT
};

extern struct workqueue_struct *lbtf_wq;

struct lbtf_private;

struct lbtf_offset_value {
	u32 offset;
	u32 value;
};

struct channel_range {
	u8 regdomain;
	u8 start;
	u8 end; 
};

struct if_usb_card;

struct lbtf_private {
	void *card;
	struct ieee80211_hw *hw;

	
	u8 cmd_resp_buff[LBS_UPLD_SIZE];
	struct ieee80211_vif *vif;

	struct work_struct cmd_work;
	struct work_struct tx_work;
	
	int (*hw_host_to_card) (struct lbtf_private *priv, u8 type, u8 *payload, u16 nb);
	int (*hw_prog_firmware) (struct if_usb_card *cardp);
	int (*hw_reset_device) (struct if_usb_card *cardp);


	
	
	u32 fwrelease;
	u32 fwcapinfo;
	

	struct mutex lock;

	
	u16 seqnum;
	

	struct cmd_ctrl_node *cmd_array;
	
	struct cmd_ctrl_node *cur_cmd;
	
	
	struct list_head cmdfreeq;
	
	struct list_head cmdpendingq;

	
	spinlock_t driver_lock;

	
	struct timer_list command_timer;
	int nr_retries;
	int cmd_timed_out;

	u8 cmd_response_rxed;

	
	u16 capability;

	
	u8 current_addr[ETH_ALEN];
	u8 multicastlist[MRVDRV_MAX_MULTICAST_LIST_SIZE][ETH_ALEN];
	u32 nr_of_multicastmacaddr;
	int cur_freq;

	struct sk_buff *skb_to_tx;
	struct sk_buff *tx_skb;

	
	u16 mac_control;
	u16 regioncode;
	struct channel_range range;

	u8 radioon;
	u32 preamble;

	struct ieee80211_channel channels[14];
	struct ieee80211_rate rates[12];
	struct ieee80211_supported_band band;
	struct lbtf_offset_value offsetvalue;

	u8 fw_ready;
	u8 surpriseremoved;
	struct sk_buff_head bc_ps_buf;

	
	s8 noise;
};


struct txpd {
	
	__le32 tx_status;
	
	__le32 tx_control;
	__le32 tx_packet_location;
	
	__le16 tx_packet_length;
	
	u8 tx_dest_addr_high[2];
	
	u8 tx_dest_addr_low[4];
	
	u8 priority;
	
	u8 powermgmt;
	
	u8 pktdelay_2ms;
	
	u8 reserved1;
};

struct rxpd {
	
	__le16 status;

	
	u8 snr;

	
	u8 rx_control;

	
	__le16 pkt_len;

	
	u8 nf;

	
	u8 rx_rate;

	
	__le32 pkt_ptr;

	
	__le32 next_rxpd_ptr;

	
	u8 priority;
	u8 reserved[3];
};

struct cmd_header {
	__le16 command;
	__le16 size;
	__le16 seqnum;
	__le16 result;
} __packed;

struct cmd_ctrl_node {
	struct list_head list;
	int result;
	
	int (*callback)(struct lbtf_private *,
			unsigned long, struct cmd_header *);
	unsigned long callback_arg;
	
	struct cmd_header *cmdbuf;
	
	u16 cmdwaitqwoken;
	wait_queue_head_t cmdwait_q;
};

struct cmd_ds_get_hw_spec {
	struct cmd_header hdr;

	
	__le16 hwifversion;
	
	__le16 version;
	
	__le16 nr_txpd;
	
	__le16 nr_mcast_adr;
	
	u8 permanentaddr[6];

	
	__le16 regioncode;

	
	__le16 nr_antenna;

	
	__le32 fwrelease;

	
	__le32 wcb_base;
	
	__le32 rxpd_rdptr;

	
	__le32 rxpd_wrptr;

	
	__le32 fwcapinfo;
} __packed;

struct cmd_ds_mac_control {
	struct cmd_header hdr;
	__le16 action;
	u16 reserved;
};

struct cmd_ds_802_11_mac_address {
	struct cmd_header hdr;

	__le16 action;
	uint8_t macadd[ETH_ALEN];
};

struct cmd_ds_mac_multicast_addr {
	struct cmd_header hdr;

	__le16 action;
	__le16 nr_of_adrs;
	u8 maclist[ETH_ALEN * MRVDRV_MAX_MULTICAST_LIST_SIZE];
};

struct cmd_ds_set_mode {
	struct cmd_header hdr;

	__le16 mode;
};

struct cmd_ds_set_bssid {
	struct cmd_header hdr;

	u8 bssid[6];
	u8 activate;
};

struct cmd_ds_802_11_radio_control {
	struct cmd_header hdr;

	__le16 action;
	__le16 control;
};


struct cmd_ds_802_11_rf_channel {
	struct cmd_header hdr;

	__le16 action;
	__le16 channel;
	__le16 rftype;      
	__le16 reserved;    
	u8 channellist[32]; 
};

struct cmd_ds_set_boot2_ver {
	struct cmd_header hdr;

	__le16 action;
	__le16 version;
};

struct cmd_ds_802_11_reset {
	struct cmd_header hdr;

	__le16 action;
};

struct cmd_ds_802_11_beacon_control {
	struct cmd_header hdr;

	__le16 action;
	__le16 beacon_enable;
	__le16 beacon_period;
};

struct cmd_ds_802_11_beacon_set {
	struct cmd_header hdr;

	__le16 len;
	u8 beacon[MRVL_MAX_BCN_SIZE];
};

struct lbtf_private;
struct cmd_ctrl_node;

void lbtf_set_mac_control(struct lbtf_private *priv);

int lbtf_free_cmd_buffer(struct lbtf_private *priv);

int lbtf_allocate_cmd_buffer(struct lbtf_private *priv);
int lbtf_execute_next_command(struct lbtf_private *priv);
int lbtf_set_radio_control(struct lbtf_private *priv);
int lbtf_update_hw_spec(struct lbtf_private *priv);
int lbtf_cmd_set_mac_multicast_addr(struct lbtf_private *priv);
void lbtf_set_mode(struct lbtf_private *priv, enum lbtf_mode mode);
void lbtf_set_bssid(struct lbtf_private *priv, bool activate, const u8 *bssid);
int lbtf_set_mac_address(struct lbtf_private *priv, uint8_t *mac_addr);

int lbtf_set_channel(struct lbtf_private *priv, u8 channel);

int lbtf_beacon_set(struct lbtf_private *priv, struct sk_buff *beacon);
int lbtf_beacon_ctrl(struct lbtf_private *priv, bool beacon_enable,
		     int beacon_int);


int lbtf_process_rx_command(struct lbtf_private *priv);
void lbtf_complete_command(struct lbtf_private *priv, struct cmd_ctrl_node *cmd,
			  int result);
void lbtf_cmd_response_rx(struct lbtf_private *priv);

struct chan_freq_power *lbtf_get_region_cfp_table(u8 region,
	int *cfp_no);
struct lbtf_private *lbtf_add_card(void *card, struct device *dmdev);
int lbtf_remove_card(struct lbtf_private *priv);
int lbtf_start_card(struct lbtf_private *priv);
int lbtf_rx(struct lbtf_private *priv, struct sk_buff *skb);
void lbtf_send_tx_feedback(struct lbtf_private *priv, u8 retrycnt, u8 fail);
void lbtf_bcn_sent(struct lbtf_private *priv);

#define lbtf_cmd(priv, cmdnr, cmd, cb, cb_arg)	({		\
	uint16_t __sz = le16_to_cpu((cmd)->hdr.size);		\
	(cmd)->hdr.size = cpu_to_le16(sizeof(*(cmd)));		\
	__lbtf_cmd(priv, cmdnr, &(cmd)->hdr, __sz, cb, cb_arg);	\
})

#define lbtf_cmd_with_response(priv, cmdnr, cmd)	\
	lbtf_cmd(priv, cmdnr, cmd, lbtf_cmd_copyback, (unsigned long) (cmd))

void lbtf_cmd_async(struct lbtf_private *priv, uint16_t command,
	struct cmd_header *in_cmd, int in_cmd_size);

int __lbtf_cmd(struct lbtf_private *priv, uint16_t command,
	      struct cmd_header *in_cmd, int in_cmd_size,
	      int (*callback)(struct lbtf_private *, unsigned long,
			      struct cmd_header *),
	      unsigned long callback_arg);

int lbtf_cmd_copyback(struct lbtf_private *priv, unsigned long extra,
		     struct cmd_header *resp);
