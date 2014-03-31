#ifndef _LEC_ARP_H_
#define _LEC_ARP_H_
#include <linux/atm.h>
#include <linux/atmdev.h>
#include <linux/if_ether.h>
#include <linux/atmlec.h>

struct lec_arp_table {
	struct hlist_node next;		
	unsigned char atm_addr[ATM_ESA_LEN];	
	unsigned char mac_addr[ETH_ALEN];	
	int is_rdesc;			
	struct atm_vcc *vcc;		
	struct atm_vcc *recv_vcc;	

	void (*old_push) (struct atm_vcc *vcc, struct sk_buff *skb);
					

	void (*old_recv_push) (struct atm_vcc *vcc, struct sk_buff *skb);
					

	unsigned long last_used;	
	unsigned long timestamp;	
	unsigned char no_tries;		
	unsigned char status;		
	unsigned short flags;		
	unsigned short packets_flooded;	
	unsigned long flush_tran_id;	
	struct timer_list timer;	
	struct lec_priv *priv;		
	u8 *tlvs;
	u32 sizeoftlvs;			
	struct sk_buff_head tx_wait;	
	atomic_t usage;			
};

struct tlv {
	u32 type;
	u8 length;
	u8 value[255];
};

#define ESI_UNKNOWN 0		
#define ESI_ARP_PENDING 1	
#define ESI_VC_PENDING 2	
#define ESI_FLUSH_PENDING 4	
#define ESI_FORWARD_DIRECT 5	

#define LEC_REMOTE_FLAG      0x0001
#define LEC_PERMANENT_FLAG   0x0002

#endif 
