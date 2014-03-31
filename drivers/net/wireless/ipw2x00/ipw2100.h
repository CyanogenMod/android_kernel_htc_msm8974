/******************************************************************************

  Copyright(c) 2003 - 2006 Intel Corporation. All rights reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.

  Contact Information:
  Intel Linux Wireless <ilw@linux.intel.com>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

******************************************************************************/
#ifndef _IPW2100_H
#define _IPW2100_H

#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/skbuff.h>
#include <asm/io.h>
#include <linux/socket.h>
#include <linux/if_arp.h>
#include <linux/wireless.h>
#include <net/iw_handler.h>	

#ifdef CONFIG_IPW2100_MONITOR
#include <net/ieee80211_radiotap.h>
#endif

#include <linux/workqueue.h>
#include <linux/mutex.h>

#include "libipw.h"

struct ipw2100_priv;
struct ipw2100_tx_packet;
struct ipw2100_rx_packet;

#define IPW_DL_UNINIT    0x80000000
#define IPW_DL_NONE      0x00000000
#define IPW_DL_ALL       0x7FFFFFFF


#define IPW_DL_ERROR         (1<<0)
#define IPW_DL_WARNING       (1<<1)
#define IPW_DL_INFO          (1<<2)
#define IPW_DL_WX            (1<<3)
#define IPW_DL_HC            (1<<5)
#define IPW_DL_STATE         (1<<6)

#define IPW_DL_NOTIF         (1<<10)
#define IPW_DL_SCAN          (1<<11)
#define IPW_DL_ASSOC         (1<<12)
#define IPW_DL_DROP          (1<<13)

#define IPW_DL_IOCTL         (1<<14)
#define IPW_DL_RF_KILL       (1<<17)

#define IPW_DL_MANAGE        (1<<15)
#define IPW_DL_FW            (1<<16)

#define IPW_DL_FRAG          (1<<21)
#define IPW_DL_WEP           (1<<22)
#define IPW_DL_TX            (1<<23)
#define IPW_DL_RX            (1<<24)
#define IPW_DL_ISR           (1<<25)
#define IPW_DL_IO            (1<<26)
#define IPW_DL_TRACE         (1<<28)

#define IPW_DEBUG_ERROR(f, a...) printk(KERN_ERR DRV_NAME ": " f, ## a)
#define IPW_DEBUG_WARNING(f, a...) printk(KERN_WARNING DRV_NAME ": " f, ## a)
#define IPW_DEBUG_INFO(f...)    IPW_DEBUG(IPW_DL_INFO, ## f)
#define IPW_DEBUG_WX(f...)     IPW_DEBUG(IPW_DL_WX, ## f)
#define IPW_DEBUG_SCAN(f...)   IPW_DEBUG(IPW_DL_SCAN, ## f)
#define IPW_DEBUG_NOTIF(f...) IPW_DEBUG(IPW_DL_NOTIF, ## f)
#define IPW_DEBUG_TRACE(f...)  IPW_DEBUG(IPW_DL_TRACE, ## f)
#define IPW_DEBUG_RX(f...)     IPW_DEBUG(IPW_DL_RX, ## f)
#define IPW_DEBUG_TX(f...)     IPW_DEBUG(IPW_DL_TX, ## f)
#define IPW_DEBUG_ISR(f...)    IPW_DEBUG(IPW_DL_ISR, ## f)
#define IPW_DEBUG_MANAGEMENT(f...) IPW_DEBUG(IPW_DL_MANAGE, ## f)
#define IPW_DEBUG_WEP(f...)    IPW_DEBUG(IPW_DL_WEP, ## f)
#define IPW_DEBUG_HC(f...) IPW_DEBUG(IPW_DL_HC, ## f)
#define IPW_DEBUG_FRAG(f...) IPW_DEBUG(IPW_DL_FRAG, ## f)
#define IPW_DEBUG_FW(f...) IPW_DEBUG(IPW_DL_FW, ## f)
#define IPW_DEBUG_RF_KILL(f...) IPW_DEBUG(IPW_DL_RF_KILL, ## f)
#define IPW_DEBUG_DROP(f...) IPW_DEBUG(IPW_DL_DROP, ## f)
#define IPW_DEBUG_IO(f...) IPW_DEBUG(IPW_DL_IO, ## f)
#define IPW_DEBUG_IOCTL(f...) IPW_DEBUG(IPW_DL_IOCTL, ## f)
#define IPW_DEBUG_STATE(f, a...) IPW_DEBUG(IPW_DL_STATE | IPW_DL_ASSOC | IPW_DL_INFO, f, ## a)
#define IPW_DEBUG_ASSOC(f, a...) IPW_DEBUG(IPW_DL_ASSOC | IPW_DL_INFO, f, ## a)

enum {
	IPW_HW_STATE_DISABLED = 1,
	IPW_HW_STATE_ENABLED = 0
};

struct ssid_context {
	char ssid[IW_ESSID_MAX_SIZE + 1];
	int ssid_len;
	unsigned char bssid[ETH_ALEN];
	int port_type;
	int channel;

};

extern const char *port_type_str[];
extern const char *band_str[];

#define NUMBER_OF_BD_PER_COMMAND_PACKET		1
#define NUMBER_OF_BD_PER_DATA_PACKET		2

#define IPW_MAX_BDS 6
#define NUMBER_OF_OVERHEAD_BDS_PER_PACKETR	2
#define NUMBER_OF_BDS_TO_LEAVE_FOR_COMMANDS	1

#define REQUIRED_SPACE_IN_RING_FOR_COMMAND_PACKET \
    (IPW_BD_QUEUE_W_R_MIN_SPARE + NUMBER_OF_BD_PER_COMMAND_PACKET)

struct bd_status {
	union {
		struct {
			u8 nlf:1, txType:2, intEnabled:1, reserved:4;
		} fields;
		u8 field;
	} info;
} __packed;

struct ipw2100_bd {
	u32 host_addr;
	u32 buf_length;
	struct bd_status status;
	u8 num_fragments;
	u8 reserved[6];
} __packed;

#define IPW_BD_QUEUE_LENGTH(n) (1<<n)
#define IPW_BD_ALIGNMENT(L)    (L*sizeof(struct ipw2100_bd))

#define IPW_BD_STATUS_TX_FRAME_802_3             0x00
#define IPW_BD_STATUS_TX_FRAME_NOT_LAST_FRAGMENT 0x01
#define IPW_BD_STATUS_TX_FRAME_COMMAND		 0x02
#define IPW_BD_STATUS_TX_FRAME_802_11	         0x04
#define IPW_BD_STATUS_TX_INTERRUPT_ENABLE	 0x08

struct ipw2100_bd_queue {
	
	struct ipw2100_bd *drv;

	
	dma_addr_t nic;

	
	u32 size;

	
	u32 entries;

	
	u32 available;

	u32 oldest;

	
	u32 next;
};

#define RX_QUEUE_LENGTH 256
#define TX_QUEUE_LENGTH 256
#define HW_QUEUE_LENGTH 256

#define TX_PENDED_QUEUE_LENGTH (TX_QUEUE_LENGTH / NUMBER_OF_BD_PER_DATA_PACKET)

#define STATUS_TYPE_MASK	0x0000000f
#define COMMAND_STATUS_VAL	0
#define STATUS_CHANGE_VAL	1
#define P80211_DATA_VAL 	2
#define P8023_DATA_VAL		3
#define HOST_NOTIFICATION_VAL	4

#define IPW2100_RSSI_TO_DBM (-98)

struct ipw2100_status {
	u32 frame_size;
	u16 status_fields;
	u8 flags;
#define IPW_STATUS_FLAG_DECRYPTED	(1<<0)
#define IPW_STATUS_FLAG_WEP_ENCRYPTED	(1<<1)
#define IPW_STATUS_FLAG_CRC_ERROR       (1<<2)
	u8 rssi;
} __packed;

struct ipw2100_status_queue {
	
	struct ipw2100_status *drv;

	
	dma_addr_t nic;

	
	u32 size;
};

#define HOST_COMMAND_PARAMS_REG_LEN	100
#define CMD_STATUS_PARAMS_REG_LEN 	3

#define IPW_WPA_CAPABILITIES   0x1
#define IPW_WPA_LISTENINTERVAL 0x2
#define IPW_WPA_AP_ADDRESS     0x4

#define IPW_MAX_VAR_IE_LEN ((HOST_COMMAND_PARAMS_REG_LEN - 4) * sizeof(u32))

struct ipw2100_wpa_assoc_frame {
	u16 fixed_ie_mask;
	struct {
		u16 capab_info;
		u16 listen_interval;
		u8 current_ap[ETH_ALEN];
	} fixed_ies;
	u32 var_ie_len;
	u8 var_ie[IPW_MAX_VAR_IE_LEN];
};

#define IPW_BSS     1
#define IPW_MONITOR 2
#define IPW_IBSS    3

struct ipw2100_cmd_header {
	u32 host_command_reg;
	u32 host_command_reg1;
	u32 sequence;
	u32 host_command_len_reg;
	u32 host_command_params_reg[HOST_COMMAND_PARAMS_REG_LEN];
	u32 cmd_status_reg;
	u32 cmd_status_params_reg[CMD_STATUS_PARAMS_REG_LEN];
	u32 rxq_base_ptr;
	u32 rxq_next_ptr;
	u32 rxq_host_ptr;
	u32 txq_base_ptr;
	u32 txq_next_ptr;
	u32 txq_host_ptr;
	u32 tx_status_reg;
	u32 reserved;
	u32 status_change_reg;
	u32 reserved1[3];
	u32 *ordinal1_ptr;
	u32 *ordinal2_ptr;
} __packed;

struct ipw2100_data_header {
	u32 host_command_reg;
	u32 host_command_reg1;
	u8 encrypted;		
	u8 needs_encryption;	
	u8 wep_index;		
	u8 key_size;		
	u8 key[16];
	u8 reserved[10];	
	u8 src_addr[ETH_ALEN];
	u8 dst_addr[ETH_ALEN];
	u16 fragment_size;
} __packed;

struct host_command {
	u32 host_command;	
	u32 host_command1;	
	u32 host_command_sequence;	
	u32 host_command_length;	
	u32 host_command_parameters[HOST_COMMAND_PARAMS_REG_LEN];	
} __packed;

typedef enum {
	POWER_ON_RESET,
	EXIT_POWER_DOWN_RESET,
	SW_RESET,
	EEPROM_RW,
	SW_RE_INIT
} ipw2100_reset_event;

enum {
	COMMAND = 0xCAFE,
	DATA,
	RX
};

struct ipw2100_tx_packet {
	int type;
	int index;
	union {
		struct {	
			struct ipw2100_cmd_header *cmd;
			dma_addr_t cmd_phys;
		} c_struct;
		struct {	
			struct ipw2100_data_header *data;
			dma_addr_t data_phys;
			struct libipw_txb *txb;
		} d_struct;
	} info;
	int jiffy_start;

	struct list_head list;
};

struct ipw2100_rx_packet {
	struct ipw2100_rx *rxp;
	dma_addr_t dma_addr;
	int jiffy_start;
	struct sk_buff *skb;
	struct list_head list;
};

#define FRAG_DISABLED             (1<<31)
#define RTS_DISABLED              (1<<31)
#define MAX_RTS_THRESHOLD         2304U
#define MIN_RTS_THRESHOLD         1U
#define DEFAULT_RTS_THRESHOLD     1000U

#define DEFAULT_BEACON_INTERVAL   100U
#define	DEFAULT_SHORT_RETRY_LIMIT 7U
#define	DEFAULT_LONG_RETRY_LIMIT  4U

struct ipw2100_ordinals {
	u32 table1_addr;
	u32 table2_addr;
	u32 table1_size;
	u32 table2_size;
};

struct ipw2100_notification {
	u32 hnhdr_subtype;	
	u32 hnhdr_size;		
} __packed;

#define MAX_KEY_SIZE	16
#define	MAX_KEYS	8

#define IPW2100_WEP_ENABLE     (1<<1)
#define IPW2100_WEP_DROP_CLEAR (1<<2)

#define IPW_NONE_CIPHER   (1<<0)
#define IPW_WEP40_CIPHER  (1<<1)
#define IPW_TKIP_CIPHER   (1<<2)
#define IPW_CCMP_CIPHER   (1<<4)
#define IPW_WEP104_CIPHER (1<<5)
#define IPW_CKIP_CIPHER   (1<<6)

#define	IPW_AUTH_OPEN     	0
#define	IPW_AUTH_SHARED   	1
#define IPW_AUTH_LEAP	  	2
#define IPW_AUTH_LEAP_CISCO_ID	0x80

struct statistic {
	int value;
	int hi;
	int lo;
};

#define INIT_STAT(x) do {  \
  (x)->value = (x)->hi = 0; \
  (x)->lo = 0x7fffffff; \
} while (0)
#define SET_STAT(x,y) do { \
  (x)->value = y; \
  if ((x)->value > (x)->hi) (x)->hi = (x)->value; \
  if ((x)->value < (x)->lo) (x)->lo = (x)->value; \
} while (0)
#define INC_STAT(x) do { if (++(x)->value > (x)->hi) (x)->hi = (x)->value; } \
while (0)
#define DEC_STAT(x) do { if (--(x)->value < (x)->lo) (x)->lo = (x)->value; } \
while (0)

#define IPW2100_ERROR_QUEUE 5

enum {
#ifdef CONFIG_PM
	IPW2100_PM_DISABLED = 0,
	PM_STATE_SIZE = 16,
#else
	IPW2100_PM_DISABLED = 1,
	PM_STATE_SIZE = 0,
#endif
};

#define STATUS_POWERED          (1<<0)
#define STATUS_CMD_ACTIVE       (1<<1)	
#define STATUS_RUNNING          (1<<2)	
#define STATUS_ENABLED          (1<<3)	
#define STATUS_STOPPING         (1<<4)	
#define STATUS_INITIALIZED      (1<<5)	
#define STATUS_ASSOCIATING      (1<<9)	
#define STATUS_ASSOCIATED       (1<<10)	
#define STATUS_INT_ENABLED      (1<<11)
#define STATUS_RF_KILL_HW       (1<<12)
#define STATUS_RF_KILL_SW       (1<<13)
#define STATUS_RF_KILL_MASK     (STATUS_RF_KILL_HW | STATUS_RF_KILL_SW)
#define STATUS_EXIT_PENDING     (1<<14)

#define STATUS_SCAN_PENDING     (1<<23)
#define STATUS_SCANNING         (1<<24)
#define STATUS_SCAN_ABORTING    (1<<25)
#define STATUS_SCAN_COMPLETE    (1<<26)
#define STATUS_WX_EVENT_PENDING (1<<27)
#define STATUS_RESET_PENDING    (1<<29)
#define STATUS_SECURITY_UPDATED (1<<30)	

#define IPW_STATE_INITIALIZED	(1<<0)
#define IPW_STATE_COUNTRY_FOUND	(1<<1)
#define IPW_STATE_ASSOCIATED    (1<<2)
#define IPW_STATE_ASSN_LOST	(1<<3)
#define IPW_STATE_ASSN_CHANGED 	(1<<4)
#define IPW_STATE_SCAN_COMPLETE	(1<<5)
#define IPW_STATE_ENTERED_PSP 	(1<<6)
#define IPW_STATE_LEFT_PSP 	(1<<7)
#define IPW_STATE_RF_KILL       (1<<8)
#define IPW_STATE_DISABLED	(1<<9)
#define IPW_STATE_POWER_DOWN	(1<<10)
#define IPW_STATE_SCANNING      (1<<11)

#define CFG_STATIC_CHANNEL      (1<<0)	
#define CFG_STATIC_ESSID        (1<<1)	
#define CFG_STATIC_BSSID        (1<<2)	
#define CFG_CUSTOM_MAC          (1<<3)
#define CFG_LONG_PREAMBLE       (1<<4)
#define CFG_ASSOCIATE           (1<<6)
#define CFG_FIXED_RATE          (1<<7)
#define CFG_ADHOC_CREATE        (1<<8)
#define CFG_PASSIVE_SCAN        (1<<10)
#ifdef CONFIG_IPW2100_MONITOR
#define CFG_CRC_CHECK           (1<<11)
#endif

#define CAP_SHARED_KEY          (1<<0)	
#define CAP_PRIVACY_ON          (1<<1)	

struct ipw2100_priv {

	int stop_hang_check;	
	int stop_rf_kill;	

	struct libipw_device *ieee;
	unsigned long status;
	unsigned long config;
	unsigned long capability;

	
	int resets;
	int reset_backoff;

	
	u8 essid[IW_ESSID_MAX_SIZE];
	u8 essid_len;
	u8 bssid[ETH_ALEN];
	u8 channel;
	int last_mode;

	unsigned long connect_start;
	unsigned long last_reset;

	u32 channel_mask;
	u32 fatal_error;
	u32 fatal_errors[IPW2100_ERROR_QUEUE];
	u32 fatal_index;
	int eeprom_version;
	int firmware_version;
	unsigned long hw_features;
	int hangs;
	u32 last_rtc;
	int dump_raw;		
	u8 *snapshot[0x30];

	u8 mandatory_bssid_mac[ETH_ALEN];
	u8 mac_addr[ETH_ALEN];

	int power_mode;

	int messages_sent;

	int short_retry_limit;
	int long_retry_limit;

	u32 rts_threshold;
	u32 frag_threshold;

	int in_isr;

	u32 tx_rates;
	int tx_power;
	u32 beacon_interval;

	char nick[IW_ESSID_MAX_SIZE + 1];

	struct ipw2100_status_queue status_queue;

	struct statistic txq_stat;
	struct statistic rxq_stat;
	struct ipw2100_bd_queue rx_queue;
	struct ipw2100_bd_queue tx_queue;
	struct ipw2100_rx_packet *rx_buffers;

	struct statistic fw_pend_stat;
	struct list_head fw_pend_list;

	struct statistic msg_free_stat;
	struct statistic msg_pend_stat;
	struct list_head msg_free_list;
	struct list_head msg_pend_list;
	struct ipw2100_tx_packet *msg_buffers;

	struct statistic tx_free_stat;
	struct statistic tx_pend_stat;
	struct list_head tx_free_list;
	struct list_head tx_pend_list;
	struct ipw2100_tx_packet *tx_buffers;

	struct ipw2100_ordinals ordinals;

	struct pci_dev *pci_dev;

	struct proc_dir_entry *dir_dev;

	struct net_device *net_dev;
	struct iw_statistics wstats;

	struct iw_public_data wireless_data;

	struct tasklet_struct irq_tasklet;

	struct delayed_work reset_work;
	struct delayed_work security_work;
	struct delayed_work wx_event_work;
	struct delayed_work hang_check;
	struct delayed_work rf_kill;
	struct work_struct scan_event_now;
	struct delayed_work scan_event_later;

	int user_requested_scan;

	
	unsigned long suspend_at;
	unsigned long suspend_time;

	u32 interrupts;
	int tx_interrupts;
	int rx_interrupts;
	int inta_other;

	spinlock_t low_lock;
	struct mutex action_mutex;
	struct mutex adapter_mutex;

	wait_queue_head_t wait_command_queue;
};


#define HOST_COMPLETE           2
#define SYSTEM_CONFIG           6
#define SSID                    8
#define MANDATORY_BSSID         9
#define AUTHENTICATION_TYPE    10
#define ADAPTER_ADDRESS        11
#define PORT_TYPE              12
#define INTERNATIONAL_MODE     13
#define CHANNEL                14
#define RTS_THRESHOLD          15
#define FRAG_THRESHOLD         16
#define POWER_MODE             17
#define TX_RATES               18
#define BASIC_TX_RATES         19
#define WEP_KEY_INFO           20
#define WEP_KEY_INDEX          25
#define WEP_FLAGS              26
#define ADD_MULTICAST          27
#define CLEAR_ALL_MULTICAST    28
#define BEACON_INTERVAL        29
#define ATIM_WINDOW            30
#define CLEAR_STATISTICS       31
#define SEND		       33
#define TX_POWER_INDEX         36
#define BROADCAST_SCAN         43
#define CARD_DISABLE           44
#define PREFERRED_BSSID        45
#define SET_SCAN_OPTIONS       46
#define SCAN_DWELL_TIME        47
#define SWEEP_TABLE            48
#define AP_OR_STATION_TABLE    49
#define GROUP_ORDINALS         50
#define SHORT_RETRY_LIMIT      51
#define LONG_RETRY_LIMIT       52

#define HOST_PRE_POWER_DOWN    58
#define CARD_DISABLE_PHY_OFF   61
#define MSDU_TX_RATES          62

#define SET_STATION_STAT_BITS      64
#define CLEAR_STATIONS_STAT_BITS   65
#define LEAP_ROGUE_MODE            66	
#define SET_SECURITY_INFORMATION   67
#define DISASSOCIATION_BSSID	   68
#define SET_WPA_IE                 69

#define IPW_CFG_MONITOR               0x00004
#define IPW_CFG_PREAMBLE_AUTO        0x00010
#define IPW_CFG_IBSS_AUTO_START     0x00020
#define IPW_CFG_LOOPBACK            0x00100
#define IPW_CFG_ANSWER_BCSSID_PROBE 0x00800
#define IPW_CFG_BT_SIDEBAND_SIGNAL	0x02000
#define IPW_CFG_802_1x_ENABLE       0x04000
#define IPW_CFG_BSS_MASK		0x08000
#define IPW_CFG_IBSS_MASK		0x10000

#define IPW_SCAN_NOASSOCIATE (1<<0)
#define IPW_SCAN_MIXED_CELL (1<<1)
#define IPW_SCAN_PASSIVE (1<<3)

#define IPW_NIC_FATAL_ERROR 0x2A7F0
#define IPW_ERROR_ADDR(x) (x & 0x3FFFF)
#define IPW_ERROR_CODE(x) ((x & 0xFF000000) >> 24)
#define IPW2100_ERR_C3_CORRUPTION (0x10 << 24)
#define IPW2100_ERR_MSG_TIMEOUT   (0x11 << 24)
#define IPW2100_ERR_FW_LOAD       (0x12 << 24)

#define IPW_MEM_SRAM_HOST_SHARED_LOWER_BOUND			0x200
#define IPW_MEM_SRAM_HOST_INTERRUPT_AREA_LOWER_BOUND  	IPW_MEM_SRAM_HOST_SHARED_LOWER_BOUND + 0x0D80

#define IPW_MEM_HOST_SHARED_RX_BD_BASE                  (IPW_MEM_SRAM_HOST_SHARED_LOWER_BOUND + 0x40)
#define IPW_MEM_HOST_SHARED_RX_STATUS_BASE              (IPW_MEM_SRAM_HOST_SHARED_LOWER_BOUND + 0x44)
#define IPW_MEM_HOST_SHARED_RX_BD_SIZE                  (IPW_MEM_SRAM_HOST_SHARED_LOWER_BOUND + 0x48)
#define IPW_MEM_HOST_SHARED_RX_READ_INDEX               (IPW_MEM_SRAM_HOST_SHARED_LOWER_BOUND + 0xa0)

#define IPW_MEM_HOST_SHARED_TX_QUEUE_BD_BASE          (IPW_MEM_SRAM_HOST_SHARED_LOWER_BOUND + 0x00)
#define IPW_MEM_HOST_SHARED_TX_QUEUE_BD_SIZE          (IPW_MEM_SRAM_HOST_SHARED_LOWER_BOUND + 0x04)
#define IPW_MEM_HOST_SHARED_TX_QUEUE_READ_INDEX       (IPW_MEM_SRAM_HOST_SHARED_LOWER_BOUND + 0x80)

#define IPW_MEM_HOST_SHARED_RX_WRITE_INDEX \
    (IPW_MEM_SRAM_HOST_INTERRUPT_AREA_LOWER_BOUND + 0x20)

#define IPW_MEM_HOST_SHARED_TX_QUEUE_WRITE_INDEX \
    (IPW_MEM_SRAM_HOST_INTERRUPT_AREA_LOWER_BOUND)

#define IPW_MEM_HOST_SHARED_ORDINALS_TABLE_1   (IPW_MEM_SRAM_HOST_SHARED_LOWER_BOUND + 0x180)
#define IPW_MEM_HOST_SHARED_ORDINALS_TABLE_2   (IPW_MEM_SRAM_HOST_SHARED_LOWER_BOUND + 0x184)

#define IPW2100_INTA_TX_TRANSFER               (0x00000001)	
#define IPW2100_INTA_RX_TRANSFER               (0x00000002)	
#define IPW2100_INTA_TX_COMPLETE	       (0x00000004)	
#define IPW2100_INTA_EVENT_INTERRUPT           (0x00000008)	
#define IPW2100_INTA_STATUS_CHANGE             (0x00000010)	
#define IPW2100_INTA_BEACON_PERIOD_EXPIRED     (0x00000020)	
#define IPW2100_INTA_SLAVE_MODE_HOST_COMMAND_DONE  (0x00010000)	
#define IPW2100_INTA_FW_INIT_DONE              (0x01000000)	
#define IPW2100_INTA_FW_CALIBRATION_CALC       (0x02000000)	
#define IPW2100_INTA_FATAL_ERROR               (0x40000000)	
#define IPW2100_INTA_PARITY_ERROR              (0x80000000)	

#define IPW_AUX_HOST_RESET_REG_PRINCETON_RESET              (0x00000001)
#define IPW_AUX_HOST_RESET_REG_FORCE_NMI                    (0x00000002)
#define IPW_AUX_HOST_RESET_REG_PCI_HOST_CLUSTER_FATAL_NMI   (0x00000004)
#define IPW_AUX_HOST_RESET_REG_CORE_FATAL_NMI               (0x00000008)
#define IPW_AUX_HOST_RESET_REG_SW_RESET                     (0x00000080)
#define IPW_AUX_HOST_RESET_REG_MASTER_DISABLED              (0x00000100)
#define IPW_AUX_HOST_RESET_REG_STOP_MASTER                  (0x00000200)

#define IPW_AUX_HOST_GP_CNTRL_BIT_CLOCK_READY           (0x00000001)	
#define IPW_AUX_HOST_GP_CNTRL_BIT_HOST_ALLOWS_STANDBY   (0x00000002)	
#define IPW_AUX_HOST_GP_CNTRL_BIT_INIT_DONE             (0x00000004)	
#define IPW_AUX_HOST_GP_CNTRL_BITS_SYS_CONFIG           (0x000007c0)	
#define IPW_AUX_HOST_GP_CNTRL_BIT_BUS_TYPE              (0x00000200)	
#define IPW_AUX_HOST_GP_CNTRL_BIT_BAR0_BLOCK_SIZE       (0x00000400)	
#define IPW_AUX_HOST_GP_CNTRL_BIT_USB_MODE              (0x20000000)	
#define IPW_AUX_HOST_GP_CNTRL_BIT_HOST_FORCES_SYS_CLK   (0x40000000)	
#define IPW_AUX_HOST_GP_CNTRL_BIT_FW_FORCES_SYS_CLK     (0x80000000)	

#define IPW_BIT_GPIO_GPIO1_MASK         0x0000000C
#define IPW_BIT_GPIO_GPIO3_MASK         0x000000C0
#define IPW_BIT_GPIO_GPIO1_ENABLE       0x00000008
#define IPW_BIT_GPIO_RF_KILL            0x00010000

#define IPW_BIT_GPIO_LED_OFF            0x00002000	

#define IPW_REG_DOMAIN_0_OFFSET 	0x0000
#define IPW_REG_DOMAIN_1_OFFSET 	IPW_MEM_SRAM_HOST_SHARED_LOWER_BOUND

#define IPW_REG_INTA			IPW_REG_DOMAIN_0_OFFSET + 0x0008
#define IPW_REG_INTA_MASK		IPW_REG_DOMAIN_0_OFFSET + 0x000C
#define IPW_REG_INDIRECT_ACCESS_ADDRESS	IPW_REG_DOMAIN_0_OFFSET + 0x0010
#define IPW_REG_INDIRECT_ACCESS_DATA	IPW_REG_DOMAIN_0_OFFSET + 0x0014
#define IPW_REG_AUTOINCREMENT_ADDRESS	IPW_REG_DOMAIN_0_OFFSET + 0x0018
#define IPW_REG_AUTOINCREMENT_DATA	IPW_REG_DOMAIN_0_OFFSET + 0x001C
#define IPW_REG_RESET_REG		IPW_REG_DOMAIN_0_OFFSET + 0x0020
#define IPW_REG_GP_CNTRL		IPW_REG_DOMAIN_0_OFFSET + 0x0024
#define IPW_REG_GPIO			IPW_REG_DOMAIN_0_OFFSET + 0x0030
#define IPW_REG_FW_TYPE                 IPW_REG_DOMAIN_1_OFFSET + 0x0188
#define IPW_REG_FW_VERSION 		IPW_REG_DOMAIN_1_OFFSET + 0x018C
#define IPW_REG_FW_COMPATABILITY_VERSION IPW_REG_DOMAIN_1_OFFSET + 0x0190

#define IPW_REG_INDIRECT_ADDR_MASK	0x00FFFFFC

#define IPW_INTERRUPT_MASK		0xC1010013

#define IPW2100_CONTROL_REG             0x220000
#define IPW2100_CONTROL_PHY_OFF         0x8

#define IPW2100_COMMAND			0x00300004
#define IPW2100_COMMAND_PHY_ON		0x0
#define IPW2100_COMMAND_PHY_OFF		0x1

#define IPW_REG_DOA_DEBUG_AREA_START    IPW_REG_DOMAIN_0_OFFSET + 0x0090
#define IPW_REG_DOA_DEBUG_AREA_END      IPW_REG_DOMAIN_0_OFFSET + 0x00FF
#define IPW_DATA_DOA_DEBUG_VALUE        0xd55555d5

#define IPW_INTERNAL_REGISTER_HALT_AND_RESET	0x003000e0

#define IPW_WAIT_CLOCK_STABILIZATION_DELAY	    50	
#define IPW_WAIT_RESET_ARC_COMPLETE_DELAY	    10	
#define IPW_WAIT_RESET_MASTER_ASSERT_COMPLETE_DELAY 10	

#define IPW_BD_QUEUE_W_R_MIN_SPARE 2

#define IPW_CACHE_LINE_LENGTH_DEFAULT		    0x80

#define IPW_CARD_DISABLE_PHY_OFF_COMPLETE_WAIT	    100	
#define IPW_PREPARE_POWER_DOWN_COMPLETE_WAIT	    100	

#define IPW_HEADER_802_11_SIZE		 sizeof(struct libipw_hdr_3addr)
#define IPW_MAX_80211_PAYLOAD_SIZE              2304U
#define IPW_MAX_802_11_PAYLOAD_LENGTH		2312
#define IPW_MAX_ACCEPTABLE_TX_FRAME_LENGTH	1536
#define IPW_MIN_ACCEPTABLE_RX_FRAME_LENGTH	60
#define IPW_MAX_ACCEPTABLE_RX_FRAME_LENGTH \
	(IPW_MAX_ACCEPTABLE_TX_FRAME_LENGTH + IPW_HEADER_802_11_SIZE - \
        sizeof(struct ethhdr))

#define IPW_802_11_FCS_LENGTH 4
#define IPW_RX_NIC_BUFFER_LENGTH \
        (IPW_MAX_802_11_PAYLOAD_LENGTH + IPW_HEADER_802_11_SIZE + \
		IPW_802_11_FCS_LENGTH)

#define IPW_802_11_PAYLOAD_OFFSET \
        (sizeof(struct libipw_hdr_3addr) + \
         sizeof(struct libipw_snap_hdr))

struct ipw2100_rx {
	union {
		unsigned char payload[IPW_RX_NIC_BUFFER_LENGTH];
		struct libipw_hdr_4addr header;
		u32 status;
		struct ipw2100_notification notification;
		struct ipw2100_cmd_header command;
	} rx_data;
} __packed;

#define TX_RATE_1_MBIT              0x0001
#define TX_RATE_2_MBIT              0x0002
#define TX_RATE_5_5_MBIT            0x0004
#define TX_RATE_11_MBIT             0x0008
#define TX_RATE_MASK                0x000F
#define DEFAULT_TX_RATES            0x000F

#define IPW_POWER_MODE_CAM           0x00	
#define IPW_POWER_INDEX_1            0x01
#define IPW_POWER_INDEX_2            0x02
#define IPW_POWER_INDEX_3            0x03
#define IPW_POWER_INDEX_4            0x04
#define IPW_POWER_INDEX_5            0x05
#define IPW_POWER_AUTO               0x06
#define IPW_POWER_MASK               0x0F
#define IPW_POWER_ENABLED            0x10
#define IPW_POWER_LEVEL(x)           ((x) & IPW_POWER_MASK)

#define IPW_TX_POWER_AUTO            0
#define IPW_TX_POWER_ENHANCED        1

#define IPW_TX_POWER_DEFAULT         32
#define IPW_TX_POWER_MIN             0
#define IPW_TX_POWER_MAX             16
#define IPW_TX_POWER_MIN_DBM         (-12)
#define IPW_TX_POWER_MAX_DBM         16

#define FW_SCAN_DONOT_ASSOCIATE     0x0001	
#define FW_SCAN_PASSIVE             0x0008	

#define REG_MIN_CHANNEL             0
#define REG_MAX_CHANNEL             14

#define REG_CHANNEL_MASK            0x00003FFF
#define IPW_IBSS_11B_DEFAULT_MASK   0x87ff

#define DIVERSITY_EITHER            0	
#define DIVERSITY_ANTENNA_A         1	
#define DIVERSITY_ANTENNA_B         2	

#define HOST_COMMAND_WAIT 0
#define HOST_COMMAND_NO_WAIT 1

#define LOCK_NONE 0
#define LOCK_DRIVER 1
#define LOCK_FW 2

#define TYPE_SWEEP_ORD                  0x000D
#define TYPE_IBSS_STTN_ORD              0x000E
#define TYPE_BSS_AP_ORD                 0x000F
#define TYPE_RAW_BEACON_ENTRY           0x0010
#define TYPE_CALIBRATION_DATA           0x0011
#define TYPE_ROGUE_AP_DATA              0x0012
#define TYPE_ASSOCIATION_REQUEST	0x0013
#define TYPE_REASSOCIATION_REQUEST	0x0014

#define HW_FEATURE_RFKILL 0x0001
#define RF_KILLSWITCH_OFF 1
#define RF_KILLSWITCH_ON  0

#define IPW_COMMAND_POOL_SIZE        40

#define IPW_START_ORD_TAB_1			1
#define IPW_START_ORD_TAB_2			1000

#define IPW_ORD_TAB_1_ENTRY_SIZE		sizeof(u32)

#define IS_ORDINAL_TABLE_ONE(mgr,id) \
    ((id >= IPW_START_ORD_TAB_1) && (id < mgr->table1_size))
#define IS_ORDINAL_TABLE_TWO(mgr,id) \
    ((id >= IPW_START_ORD_TAB_2) && (id < (mgr->table2_size + IPW_START_ORD_TAB_2)))

#define BSS_ID_LENGTH               6

typedef enum _ORDINAL_TABLE_1 {	
	IPW_ORD_STAT_TX_HOST_REQUESTS = 1,	
	IPW_ORD_STAT_TX_HOST_COMPLETE,	
	IPW_ORD_STAT_TX_DIR_DATA,	

	IPW_ORD_STAT_TX_DIR_DATA1 = 4,	
	IPW_ORD_STAT_TX_DIR_DATA2,	
	IPW_ORD_STAT_TX_DIR_DATA5_5,	
	IPW_ORD_STAT_TX_DIR_DATA11,	
	IPW_ORD_STAT_TX_DIR_DATA22,	

	IPW_ORD_STAT_TX_NODIR_DATA1 = 13,	
	IPW_ORD_STAT_TX_NODIR_DATA2,	
	IPW_ORD_STAT_TX_NODIR_DATA5_5,	
	IPW_ORD_STAT_TX_NODIR_DATA11,	

	IPW_ORD_STAT_NULL_DATA = 21,	
	IPW_ORD_STAT_TX_RTS,	
	IPW_ORD_STAT_TX_CTS,	
	IPW_ORD_STAT_TX_ACK,	
	IPW_ORD_STAT_TX_ASSN,	
	IPW_ORD_STAT_TX_ASSN_RESP,	
	IPW_ORD_STAT_TX_REASSN,	
	IPW_ORD_STAT_TX_REASSN_RESP,	
	IPW_ORD_STAT_TX_PROBE,	
	IPW_ORD_STAT_TX_PROBE_RESP,	
	IPW_ORD_STAT_TX_BEACON,	
	IPW_ORD_STAT_TX_ATIM,	
	IPW_ORD_STAT_TX_DISASSN,	
	IPW_ORD_STAT_TX_AUTH,	
	IPW_ORD_STAT_TX_DEAUTH,	

	IPW_ORD_STAT_TX_TOTAL_BYTES = 41,	
	IPW_ORD_STAT_TX_RETRIES,	
	IPW_ORD_STAT_TX_RETRY1,	
	IPW_ORD_STAT_TX_RETRY2,	
	IPW_ORD_STAT_TX_RETRY5_5,	
	IPW_ORD_STAT_TX_RETRY11,	

	IPW_ORD_STAT_TX_FAILURES = 51,	
	IPW_ORD_STAT_TX_ABORT_AT_HOP,	
	IPW_ORD_STAT_TX_MAX_TRIES_IN_HOP,	
	IPW_ORD_STAT_TX_ABORT_LATE_DMA,	
	IPW_ORD_STAT_TX_ABORT_STX,	
	IPW_ORD_STAT_TX_DISASSN_FAIL,	
	IPW_ORD_STAT_TX_ERR_CTS,	
	IPW_ORD_STAT_TX_BPDU,	
	IPW_ORD_STAT_TX_ERR_ACK,	

	
	IPW_ORD_STAT_RX_HOST = 61,	
	IPW_ORD_STAT_RX_DIR_DATA,	
	IPW_ORD_STAT_RX_DIR_DATA1,	
	IPW_ORD_STAT_RX_DIR_DATA2,	
	IPW_ORD_STAT_RX_DIR_DATA5_5,	
	IPW_ORD_STAT_RX_DIR_DATA11,	
	IPW_ORD_STAT_RX_DIR_DATA22,	

	IPW_ORD_STAT_RX_NODIR_DATA = 71,	
	IPW_ORD_STAT_RX_NODIR_DATA1,	
	IPW_ORD_STAT_RX_NODIR_DATA2,	
	IPW_ORD_STAT_RX_NODIR_DATA5_5,	
	IPW_ORD_STAT_RX_NODIR_DATA11,	

	IPW_ORD_STAT_RX_NULL_DATA = 80,	
	IPW_ORD_STAT_RX_POLL,	
	IPW_ORD_STAT_RX_RTS,	
	IPW_ORD_STAT_RX_CTS,	
	IPW_ORD_STAT_RX_ACK,	
	IPW_ORD_STAT_RX_CFEND,	
	IPW_ORD_STAT_RX_CFEND_ACK,	
	IPW_ORD_STAT_RX_ASSN,	
	IPW_ORD_STAT_RX_ASSN_RESP,	
	IPW_ORD_STAT_RX_REASSN,	
	IPW_ORD_STAT_RX_REASSN_RESP,	
	IPW_ORD_STAT_RX_PROBE,	
	IPW_ORD_STAT_RX_PROBE_RESP,	
	IPW_ORD_STAT_RX_BEACON,	
	IPW_ORD_STAT_RX_ATIM,	
	IPW_ORD_STAT_RX_DISASSN,	
	IPW_ORD_STAT_RX_AUTH,	
	IPW_ORD_STAT_RX_DEAUTH,	

	IPW_ORD_STAT_RX_TOTAL_BYTES = 101,	
	IPW_ORD_STAT_RX_ERR_CRC,	
	IPW_ORD_STAT_RX_ERR_CRC1,	
	IPW_ORD_STAT_RX_ERR_CRC2,	
	IPW_ORD_STAT_RX_ERR_CRC5_5,	
	IPW_ORD_STAT_RX_ERR_CRC11,	

	IPW_ORD_STAT_RX_DUPLICATE1 = 112,	
	IPW_ORD_STAT_RX_DUPLICATE2,	
	IPW_ORD_STAT_RX_DUPLICATE5_5,	
	IPW_ORD_STAT_RX_DUPLICATE11,	
	IPW_ORD_STAT_RX_DUPLICATE = 119,	

	IPW_ORD_PERS_DB_LOCK = 120,	
	IPW_ORD_PERS_DB_SIZE,	
	IPW_ORD_PERS_DB_ADDR,	
	IPW_ORD_STAT_RX_INVALID_PROTOCOL,	
	IPW_ORD_SYS_BOOT_TIME,	
	IPW_ORD_STAT_RX_NO_BUFFER,	
	IPW_ORD_STAT_RX_ABORT_LATE_DMA,	
	IPW_ORD_STAT_RX_ABORT_AT_HOP,	
	IPW_ORD_STAT_RX_MISSING_FRAG,	
	IPW_ORD_STAT_RX_ORPHAN_FRAG,	
	IPW_ORD_STAT_RX_ORPHAN_FRAME,	
	IPW_ORD_STAT_RX_FRAG_AGEOUT,	
	IPW_ORD_STAT_RX_BAD_SSID,	
	IPW_ORD_STAT_RX_ICV_ERRORS,	

	IPW_ORD_STAT_PSP_SUSPENSION = 137,	
	IPW_ORD_STAT_PSP_BCN_TIMEOUT,	
	IPW_ORD_STAT_PSP_POLL_TIMEOUT,	
	IPW_ORD_STAT_PSP_NONDIR_TIMEOUT,	
	IPW_ORD_STAT_PSP_RX_DTIMS,	
	IPW_ORD_STAT_PSP_RX_TIMS,	
	IPW_ORD_STAT_PSP_STATION_ID,	

	IPW_ORD_LAST_ASSN_TIME = 147,	
	IPW_ORD_STAT_PERCENT_MISSED_BCNS,	
	IPW_ORD_STAT_PERCENT_RETRIES,	
	IPW_ORD_ASSOCIATED_AP_PTR,	
	
	IPW_ORD_AVAILABLE_AP_CNT,	
	IPW_ORD_AP_LIST_PTR,	
	IPW_ORD_STAT_AP_ASSNS,	
	IPW_ORD_STAT_ASSN_FAIL,	
	IPW_ORD_STAT_ASSN_RESP_FAIL,	
	IPW_ORD_STAT_FULL_SCANS,	

	IPW_ORD_CARD_DISABLED,	
	IPW_ORD_STAT_ROAM_INHIBIT,	
	IPW_FILLER_40,
	IPW_ORD_RSSI_AT_ASSN = 160,	
	IPW_ORD_STAT_ASSN_CAUSE1,	
	
	IPW_ORD_STAT_ASSN_CAUSE2,	
	IPW_ORD_STAT_ASSN_CAUSE3,	
	
	IPW_ORD_STAT_ASSN_CAUSE4,	
	
	IPW_ORD_STAT_ASSN_CAUSE5,	
	IPW_ORD_STAT_ASSN_CAUSE6,	
	IPW_FILLER_41,
	IPW_FILLER_42,
	IPW_FILLER_43,
	IPW_ORD_STAT_AUTH_FAIL,	
	IPW_ORD_STAT_AUTH_RESP_FAIL,	
	IPW_ORD_STATION_TABLE_CNT,	

	IPW_ORD_RSSI_AVG_CURR = 173,	
	IPW_ORD_STEST_RESULTS_CURR,	
	IPW_ORD_STEST_RESULTS_CUM,	
	IPW_ORD_SELF_TEST_STATUS,	
	IPW_ORD_POWER_MGMT_MODE,	
	IPW_ORD_POWER_MGMT_INDEX,	
	IPW_ORD_COUNTRY_CODE,	
	IPW_ORD_COUNTRY_CHANNELS,	
	IPW_ORD_RESET_CNT,	
	IPW_ORD_BEACON_INTERVAL,	

	IPW_ORD_PRINCETON_VERSION = 184,	
	IPW_ORD_ANTENNA_DIVERSITY,	
	IPW_ORD_CCA_RSSI,	
	IPW_ORD_STAT_EEPROM_UPDATE,	
	IPW_ORD_DTIM_PERIOD,	
	IPW_ORD_OUR_FREQ,	

	IPW_ORD_RTC_TIME = 190,	
	IPW_ORD_PORT_TYPE,	
	IPW_ORD_CURRENT_TX_RATE,	
	IPW_ORD_SUPPORTED_RATES,	
	IPW_ORD_ATIM_WINDOW,	
	IPW_ORD_BASIC_RATES,	
	IPW_ORD_NIC_HIGHEST_RATE,	
	IPW_ORD_AP_HIGHEST_RATE,	
	IPW_ORD_CAPABILITIES,	
	IPW_ORD_AUTH_TYPE,	
	IPW_ORD_RADIO_TYPE,	
	IPW_ORD_RTS_THRESHOLD = 201,	
	IPW_ORD_INT_MODE,	
	IPW_ORD_FRAGMENTATION_THRESHOLD,	
	IPW_ORD_EEPROM_SRAM_DB_BLOCK_START_ADDRESS,	
	IPW_ORD_EEPROM_SRAM_DB_BLOCK_SIZE,	
	IPW_ORD_EEPROM_SKU_CAPABILITY,	
	IPW_ORD_EEPROM_IBSS_11B_CHANNELS,	

	IPW_ORD_MAC_VERSION = 209,	
	IPW_ORD_MAC_REVISION,	
	IPW_ORD_RADIO_VERSION,	
	IPW_ORD_NIC_MANF_DATE_TIME,	
	IPW_ORD_UCODE_VERSION,	
	IPW_ORD_HW_RF_SWITCH_STATE = 214,	
} ORDINALTABLE1;

#define IPW_FIRST_VARIABLE_LENGTH_ORDINAL   1001

typedef enum _ORDINAL_TABLE_2 {	
	IPW_ORD_STAT_BASE = 1000,	
	IPW_ORD_STAT_ADAPTER_MAC = 1001,	
	IPW_ORD_STAT_PREFERRED_BSSID = 1002,	
	IPW_ORD_STAT_MANDATORY_BSSID = 1003,	
	IPW_FILL_1,		
	IPW_ORD_STAT_COUNTRY_TEXT = 1005,	
	IPW_ORD_STAT_ASSN_SSID = 1006,	
	IPW_ORD_STATION_TABLE = 1007,	
	IPW_ORD_STAT_SWEEP_TABLE = 1008,	
	IPW_ORD_STAT_ROAM_LOG = 1009,	
	IPW_ORD_STAT_RATE_LOG = 1010,	
	IPW_ORD_STAT_FIFO = 1011,	
	IPW_ORD_STAT_FW_VER_NUM = 1012,	
	IPW_ORD_STAT_FW_DATE = 1013,	
	IPW_ORD_STAT_ASSN_AP_BSSID = 1014,	
	IPW_ORD_STAT_DEBUG = 1015,	
	IPW_ORD_STAT_NIC_BPA_NUM = 1016,	
	IPW_ORD_STAT_UCODE_DATE = 1017,	
	IPW_ORD_SECURITY_NGOTIATION_RESULT = 1018,
} ORDINALTABLE2;		

#define IPW_LAST_VARIABLE_LENGTH_ORDINAL   1018

#ifndef WIRELESS_SPY
#define WIRELESS_SPY		
#endif

#define IPW_HOST_FW_SHARED_AREA0 	0x0002f200
#define IPW_HOST_FW_SHARED_AREA0_END 	0x0002f510	

#define IPW_HOST_FW_SHARED_AREA1 	0x0002f610
#define IPW_HOST_FW_SHARED_AREA1_END 	0x0002f630	

#define IPW_HOST_FW_SHARED_AREA2 	0x0002fa00
#define IPW_HOST_FW_SHARED_AREA2_END 	0x0002fa20	

#define IPW_HOST_FW_SHARED_AREA3 	0x0002fc00
#define IPW_HOST_FW_SHARED_AREA3_END 	0x0002fc10	

#define IPW_HOST_FW_INTERRUPT_AREA 	0x0002ff80
#define IPW_HOST_FW_INTERRUPT_AREA_END 	0x00030000	

struct ipw2100_fw_chunk {
	unsigned char *buf;
	long len;
	long pos;
	struct list_head list;
};

struct ipw2100_fw_chunk_set {
	const void *data;
	unsigned long size;
};

struct ipw2100_fw {
	int version;
	struct ipw2100_fw_chunk_set fw;
	struct ipw2100_fw_chunk_set uc;
	const struct firmware *fw_entry;
};

#define MAX_FW_VERSION_LEN 14

#endif				
