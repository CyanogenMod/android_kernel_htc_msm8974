
#ifndef _LEC_H_
#define _LEC_H_

#include <linux/atmdev.h>
#include <linux/netdevice.h>
#include <linux/atmlec.h>

#define LEC_HEADER_LEN 16

struct lecdatahdr_8023 {
	__be16 le_header;
	unsigned char h_dest[ETH_ALEN];
	unsigned char h_source[ETH_ALEN];
	__be16 h_type;
};

struct lecdatahdr_8025 {
	__be16 le_header;
	unsigned char ac_pad;
	unsigned char fc;
	unsigned char h_dest[ETH_ALEN];
	unsigned char h_source[ETH_ALEN];
};

#define LEC_MINIMUM_8023_SIZE   62
#define LEC_MINIMUM_8025_SIZE   16

struct lane2_ops {
	int (*resolve) (struct net_device *dev, const u8 *dst_mac, int force,
			u8 **tlvs, u32 *sizeoftlvs);
	int (*associate_req) (struct net_device *dev, const u8 *lan_dst,
			      const u8 *tlvs, u32 sizeoftlvs);
	void (*associate_indicator) (struct net_device *dev, const u8 *mac_addr,
				     const u8 *tlvs, u32 sizeoftlvs);
};


#define LEC_ARP_TABLE_SIZE 16

struct lec_priv {
	unsigned short lecid;			
	struct hlist_head lec_arp_empty_ones;
						
	struct hlist_head lec_arp_tables[LEC_ARP_TABLE_SIZE];
						
	struct hlist_head lec_no_forward;
	struct hlist_head mcast_fwds;
	spinlock_t lec_arp_lock;
	struct atm_vcc *mcast_vcc;		
	struct atm_vcc *lecd;
	struct delayed_work lec_arp_work;	
	unsigned int maximum_unknown_frame_count;
	unsigned long max_unknown_frame_time;
	unsigned long vcc_timeout_period;
	unsigned short max_retry_count;
	unsigned long aging_time;
	unsigned long forward_delay_time;	
	int topology_change;
	unsigned long arp_response_time;
	unsigned long flush_timeout;
	unsigned long path_switching_delay;

	u8 *tlvs;				
	u32 sizeoftlvs;				
	int lane_version;			
	int itfnum;				
	struct lane2_ops *lane2_ops;		
	int is_proxy;				
	int is_trdev;				
};

struct lec_vcc_priv {
	void (*old_pop) (struct atm_vcc *vcc, struct sk_buff *skb);
	int xoff;
};

#define LEC_VCC_PRIV(vcc)	((struct lec_vcc_priv *)((vcc)->user_back))

#endif				
