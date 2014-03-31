#ifndef HOSTAP_AP_H
#define HOSTAP_AP_H

#include "hostap_80211.h"


#define STA_MAX_TX_BUFFER 32

#define WLAN_STA_AUTH BIT(0)
#define WLAN_STA_ASSOC BIT(1)
#define WLAN_STA_PS BIT(2)
#define WLAN_STA_TIM BIT(3) 
#define WLAN_STA_PERM BIT(4) 
#define WLAN_STA_AUTHORIZED BIT(5) 
#define WLAN_STA_PENDING_POLL BIT(6) 

#define WLAN_RATE_1M BIT(0)
#define WLAN_RATE_2M BIT(1)
#define WLAN_RATE_5M5 BIT(2)
#define WLAN_RATE_11M BIT(3)
#define WLAN_RATE_COUNT 4

#define WLAN_SUPP_RATES_MAX 32

#define WLAN_RATE_UPDATE_COUNT 50

#define WLAN_RATE_DECREASE_THRESHOLD 2

struct sta_info {
	struct list_head list;
	struct sta_info *hnext; 
	atomic_t users; 
	struct proc_dir_entry *proc;

	u8 addr[6];
	u16 aid; 
	u32 flags;
	u16 capability;
	u16 listen_interval; 
	u8 supported_rates[WLAN_SUPP_RATES_MAX];

	unsigned long last_auth;
	unsigned long last_assoc;
	unsigned long last_rx;
	unsigned long last_tx;
	unsigned long rx_packets, tx_packets;
	unsigned long rx_bytes, tx_bytes;
	struct sk_buff_head tx_buf;

	s8 last_rx_silence; 
	s8 last_rx_signal; 
	u8 last_rx_rate; 
	u8 last_rx_updated; 

	u8 tx_supp_rates; 
	u8 tx_rate; 
	u8 tx_rate_idx; 
	u8 tx_max_rate; 
	u32 tx_count[WLAN_RATE_COUNT]; 
	u32 rx_count[WLAN_RATE_COUNT]; 
	u32 tx_since_last_failure;
	u32 tx_consecutive_exc;

	struct lib80211_crypt_data *crypt;

	int ap; 

	local_info_t *local;

#ifndef PRISM2_NO_KERNEL_IEEE80211_MGMT
	union {
		struct {
			char *challenge; 
		} sta;
		struct {
			int ssid_len;
			unsigned char ssid[MAX_SSID_LEN + 1]; 
			int channel;
			unsigned long last_beacon; 
		} ap;
	} u;

	struct timer_list timer;
	enum { STA_NULLFUNC = 0, STA_DISASSOC, STA_DEAUTH } timeout_next;
#endif 
};


#define MAX_STA_COUNT 1024

#define MAX_AID_TABLE_SIZE 128

#define STA_HASH_SIZE 256
#define STA_HASH(sta) (sta[5])


#define AP_MAX_INACTIVITY_SEC (5 * 60)
#define AP_DISASSOC_DELAY (HZ)
#define AP_DEAUTH_DELAY (HZ)

typedef enum {
	AP_OTHER_AP_SKIP_ALL = 0,
	AP_OTHER_AP_SAME_SSID = 1,
	AP_OTHER_AP_ALL = 2,
	AP_OTHER_AP_EVEN_IBSS = 3
} ap_policy_enum;

#define PRISM2_AUTH_OPEN BIT(0)
#define PRISM2_AUTH_SHARED_KEY BIT(1)


struct mac_entry {
	struct list_head list;
	u8 addr[6];
};

struct mac_restrictions {
	enum { MAC_POLICY_OPEN = 0, MAC_POLICY_ALLOW, MAC_POLICY_DENY } policy;
	unsigned int entries;
	struct list_head mac_list;
	spinlock_t lock;
};


struct add_sta_proc_data {
	u8 addr[ETH_ALEN];
	struct add_sta_proc_data *next;
};


typedef enum { WDS_ADD, WDS_DEL } wds_oper_type;
struct wds_oper_data {
	wds_oper_type type;
	u8 addr[ETH_ALEN];
	struct wds_oper_data *next;
};


struct ap_data {
	int initialized; 
	local_info_t *local;
	int bridge_packets; 
	unsigned int bridged_unicast; 
	unsigned int bridged_multicast; 
	unsigned int tx_drop_nonassoc; 
	int nullfunc_ack; 

	spinlock_t sta_table_lock;
	int num_sta; 
	struct list_head sta_list; 
	struct sta_info *sta_hash[STA_HASH_SIZE];

	struct proc_dir_entry *proc;

	ap_policy_enum ap_policy;
	unsigned int max_inactivity;
	int autom_ap_wds;

	struct mac_restrictions mac_restrictions; 
	int last_tx_rate;

	struct work_struct add_sta_proc_queue;
	struct add_sta_proc_data *add_sta_proc_entries;

	struct work_struct wds_oper_queue;
	struct wds_oper_data *wds_oper_entries;

	u16 tx_callback_idx;

#ifndef PRISM2_NO_KERNEL_IEEE80211_MGMT
	struct sta_info *sta_aid[MAX_AID_TABLE_SIZE];

	u16 tx_callback_auth, tx_callback_assoc, tx_callback_poll;

	struct lib80211_crypto_ops *crypt;
	void *crypt_priv;
#endif 
};


void hostap_rx(struct net_device *dev, struct sk_buff *skb,
	       struct hostap_80211_rx_status *rx_stats);
void hostap_init_data(local_info_t *local);
void hostap_init_ap_proc(local_info_t *local);
void hostap_free_data(struct ap_data *ap);
void hostap_check_sta_fw_version(struct ap_data *ap, int sta_fw_ver);

typedef enum {
	AP_TX_CONTINUE, AP_TX_DROP, AP_TX_RETRY, AP_TX_BUFFERED,
	AP_TX_CONTINUE_NOT_AUTHORIZED
} ap_tx_ret;
struct hostap_tx_data {
	struct sk_buff *skb;
	int host_encrypt;
	struct lib80211_crypt_data *crypt;
	void *sta_ptr;
};
ap_tx_ret hostap_handle_sta_tx(local_info_t *local, struct hostap_tx_data *tx);
void hostap_handle_sta_release(void *ptr);
void hostap_handle_sta_tx_exc(local_info_t *local, struct sk_buff *skb);
int hostap_update_sta_ps(local_info_t *local, struct ieee80211_hdr *hdr);
typedef enum {
	AP_RX_CONTINUE, AP_RX_DROP, AP_RX_EXIT, AP_RX_CONTINUE_NOT_AUTHORIZED
} ap_rx_ret;
ap_rx_ret hostap_handle_sta_rx(local_info_t *local, struct net_device *dev,
			       struct sk_buff *skb,
			       struct hostap_80211_rx_status *rx_stats,
			       int wds);
int hostap_handle_sta_crypto(local_info_t *local, struct ieee80211_hdr *hdr,
			     struct lib80211_crypt_data **crypt,
			     void **sta_ptr);
int hostap_is_sta_assoc(struct ap_data *ap, u8 *sta_addr);
int hostap_is_sta_authorized(struct ap_data *ap, u8 *sta_addr);
int hostap_add_sta(struct ap_data *ap, u8 *sta_addr);
int hostap_update_rx_stats(struct ap_data *ap, struct ieee80211_hdr *hdr,
			   struct hostap_80211_rx_status *rx_stats);
void hostap_update_rates(local_info_t *local);
void hostap_add_wds_links(local_info_t *local);
void hostap_wds_link_oper(local_info_t *local, u8 *addr, wds_oper_type type);

#ifndef PRISM2_NO_KERNEL_IEEE80211_MGMT
void hostap_deauth_all_stas(struct net_device *dev, struct ap_data *ap,
			    int resend);
#endif 

#endif 
