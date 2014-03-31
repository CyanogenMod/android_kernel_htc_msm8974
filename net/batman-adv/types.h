/*
 * Copyright (C) 2007-2012 B.A.T.M.A.N. contributors:
 *
 * Marek Lindner, Simon Wunderlich
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 *
 */



#ifndef _NET_BATMAN_ADV_TYPES_H_
#define _NET_BATMAN_ADV_TYPES_H_

#include "packet.h"
#include "bitarray.h"

#define BAT_HEADER_LEN (sizeof(struct ethhdr) + \
	((sizeof(struct unicast_packet) > sizeof(struct bcast_packet) ? \
	 sizeof(struct unicast_packet) : \
	 sizeof(struct bcast_packet))))


struct hard_iface {
	struct list_head list;
	int16_t if_num;
	char if_status;
	struct net_device *net_dev;
	atomic_t seqno;
	atomic_t frag_seqno;
	unsigned char *packet_buff;
	int packet_len;
	struct kobject *hardif_obj;
	atomic_t refcount;
	struct packet_type batman_adv_ptype;
	struct net_device *soft_iface;
	struct rcu_head rcu;
};

struct orig_node {
	uint8_t orig[ETH_ALEN];
	uint8_t primary_addr[ETH_ALEN];
	struct neigh_node __rcu *router; 
	unsigned long *bcast_own;
	uint8_t *bcast_own_sum;
	unsigned long last_valid;
	unsigned long bcast_seqno_reset;
	unsigned long batman_seqno_reset;
	uint8_t gw_flags;
	uint8_t flags;
	atomic_t last_ttvn; 
	uint16_t tt_crc;
	unsigned char *tt_buff;
	int16_t tt_buff_len;
	spinlock_t tt_buff_lock; 
	atomic_t tt_size;
	bool tt_initialised;
	bool tt_poss_change;
	uint32_t last_real_seqno;
	uint8_t last_ttl;
	unsigned long bcast_bits[NUM_WORDS];
	uint32_t last_bcast_seqno;
	struct hlist_head neigh_list;
	struct list_head frag_list;
	spinlock_t neigh_list_lock; 
	atomic_t refcount;
	struct rcu_head rcu;
	struct hlist_node hash_entry;
	struct bat_priv *bat_priv;
	unsigned long last_frag_packet;
	spinlock_t ogm_cnt_lock;
	
	spinlock_t bcast_seqno_lock;
	spinlock_t tt_list_lock; 
	atomic_t bond_candidates;
	struct list_head bond_list;
};

struct gw_node {
	struct hlist_node list;
	struct orig_node *orig_node;
	unsigned long deleted;
	atomic_t refcount;
	struct rcu_head rcu;
};

struct neigh_node {
	struct hlist_node list;
	uint8_t addr[ETH_ALEN];
	uint8_t real_packet_count;
	uint8_t tq_recv[TQ_GLOBAL_WINDOW_SIZE];
	uint8_t tq_index;
	uint8_t tq_avg;
	uint8_t last_ttl;
	struct list_head bonding_list;
	unsigned long last_valid;
	unsigned long real_bits[NUM_WORDS];
	atomic_t refcount;
	struct rcu_head rcu;
	struct orig_node *orig_node;
	struct hard_iface *if_incoming;
	spinlock_t tq_lock;	
};


struct bat_priv {
	atomic_t mesh_state;
	struct net_device_stats stats;
	atomic_t aggregated_ogms;	
	atomic_t bonding;		
	atomic_t fragmentation;		
	atomic_t ap_isolation;		
	atomic_t vis_mode;		
	atomic_t gw_mode;		
	atomic_t gw_sel_class;		
	atomic_t gw_bandwidth;		
	atomic_t orig_interval;		
	atomic_t hop_penalty;		
	atomic_t log_level;		
	atomic_t bcast_seqno;
	atomic_t bcast_queue_left;
	atomic_t batman_queue_left;
	atomic_t ttvn; 
	atomic_t tt_ogm_append_cnt;
	atomic_t tt_local_changes; 
	bool tt_poss_change;
	char num_ifaces;
	struct debug_log *debug_log;
	struct kobject *mesh_obj;
	struct dentry *debug_dir;
	struct hlist_head forw_bat_list;
	struct hlist_head forw_bcast_list;
	struct hlist_head gw_list;
	struct hlist_head softif_neigh_vids;
	struct list_head tt_changes_list; 
	struct list_head vis_send_list;
	struct hashtable_t *orig_hash;
	struct hashtable_t *tt_local_hash;
	struct hashtable_t *tt_global_hash;
	struct list_head tt_req_list; 
	struct list_head tt_roam_list;
	struct hashtable_t *vis_hash;
	spinlock_t forw_bat_list_lock; 
	spinlock_t forw_bcast_list_lock; 
	spinlock_t tt_changes_list_lock; 
	spinlock_t tt_req_list_lock; 
	spinlock_t tt_roam_list_lock; 
	spinlock_t gw_list_lock; 
	spinlock_t vis_hash_lock; 
	spinlock_t vis_list_lock; 
	spinlock_t softif_neigh_lock; 
	spinlock_t softif_neigh_vid_lock; 
	atomic_t num_local_tt;
	
	atomic_t tt_crc;
	unsigned char *tt_buff;
	int16_t tt_buff_len;
	spinlock_t tt_buff_lock; 
	struct delayed_work tt_work;
	struct delayed_work orig_work;
	struct delayed_work vis_work;
	struct gw_node __rcu *curr_gw;  
	atomic_t gw_reselect;
	struct hard_iface __rcu *primary_if;  
	struct vis_info *my_vis_info;
	struct bat_algo_ops *bat_algo_ops;
};

struct socket_client {
	struct list_head queue_list;
	unsigned int queue_len;
	unsigned char index;
	spinlock_t lock; 
	wait_queue_head_t queue_wait;
	struct bat_priv *bat_priv;
};

struct socket_packet {
	struct list_head list;
	size_t icmp_len;
	struct icmp_packet_rr icmp_packet;
};

struct tt_common_entry {
	uint8_t addr[ETH_ALEN];
	struct hlist_node hash_entry;
	uint16_t flags;
	atomic_t refcount;
	struct rcu_head rcu;
};

struct tt_local_entry {
	struct tt_common_entry common;
	unsigned long last_seen;
};

struct tt_global_entry {
	struct tt_common_entry common;
	struct orig_node *orig_node;
	uint8_t ttvn;
	unsigned long roam_at; 
};

struct tt_change_node {
	struct list_head list;
	struct tt_change change;
};

struct tt_req_node {
	uint8_t addr[ETH_ALEN];
	unsigned long issued_at;
	struct list_head list;
};

struct tt_roam_node {
	uint8_t addr[ETH_ALEN];
	atomic_t counter;
	unsigned long first_time;
	struct list_head list;
};

struct forw_packet {
	struct hlist_node list;
	unsigned long send_time;
	uint8_t own;
	struct sk_buff *skb;
	uint16_t packet_len;
	uint32_t direct_link_flags;
	uint8_t num_packets;
	struct delayed_work delayed_work;
	struct hard_iface *if_incoming;
};

struct if_list_entry {
	uint8_t addr[ETH_ALEN];
	bool primary;
	struct hlist_node list;
};

struct debug_log {
	char log_buff[LOG_BUF_LEN];
	unsigned long log_start;
	unsigned long log_end;
	spinlock_t lock; 
	wait_queue_head_t queue_wait;
};

struct frag_packet_list_entry {
	struct list_head list;
	uint16_t seqno;
	struct sk_buff *skb;
};

struct vis_info {
	unsigned long first_seen;
	struct list_head recv_list;
	struct list_head send_list;
	struct kref refcount;
	struct hlist_node hash_entry;
	struct bat_priv *bat_priv;
	
	struct sk_buff *skb_packet;
	
} __packed;

struct vis_info_entry {
	uint8_t  src[ETH_ALEN];
	uint8_t  dest[ETH_ALEN];
	uint8_t  quality;	
} __packed;

struct recvlist_node {
	struct list_head list;
	uint8_t mac[ETH_ALEN];
};

struct softif_neigh_vid {
	struct hlist_node list;
	struct bat_priv *bat_priv;
	short vid;
	atomic_t refcount;
	struct softif_neigh __rcu *softif_neigh;
	struct rcu_head rcu;
	struct hlist_head softif_neigh_list;
};

struct softif_neigh {
	struct hlist_node list;
	uint8_t addr[ETH_ALEN];
	unsigned long last_seen;
	atomic_t refcount;
	struct rcu_head rcu;
};

struct bat_algo_ops {
	struct hlist_node list;
	char *name;
	
	void (*bat_ogm_init)(struct hard_iface *hard_iface);
	
	void (*bat_ogm_init_primary)(struct hard_iface *hard_iface);
	
	void (*bat_ogm_update_mac)(struct hard_iface *hard_iface);
	
	void (*bat_ogm_schedule)(struct hard_iface *hard_iface,
				 int tt_num_changes);
	
	void (*bat_ogm_emit)(struct forw_packet *forw_packet);
	
	void (*bat_ogm_receive)(struct hard_iface *if_incoming,
				struct sk_buff *skb);
};

#endif 
