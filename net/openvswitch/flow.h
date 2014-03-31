/*
 * Copyright (c) 2007-2011 Nicira Networks.
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
 */

#ifndef FLOW_H
#define FLOW_H 1

#include <linux/kernel.h>
#include <linux/netlink.h>
#include <linux/openvswitch.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/rcupdate.h>
#include <linux/if_ether.h>
#include <linux/in6.h>
#include <linux/jiffies.h>
#include <linux/time.h>
#include <linux/flex_array.h>
#include <net/inet_ecn.h>

struct sk_buff;

struct sw_flow_actions {
	struct rcu_head rcu;
	u32 actions_len;
	struct nlattr actions[];
};

struct sw_flow_key {
	struct {
		u32	priority;	
		u16	in_port;	
	} phy;
	struct {
		u8     src[ETH_ALEN];	
		u8     dst[ETH_ALEN];	
		__be16 tci;		
		__be16 type;		
	} eth;
	struct {
		u8     proto;		
		u8     tos;		
		u8     ttl;		
		u8     frag;		
	} ip;
	union {
		struct {
			struct {
				__be32 src;	
				__be32 dst;	
			} addr;
			union {
				struct {
					__be16 src;		
					__be16 dst;		
				} tp;
				struct {
					u8 sha[ETH_ALEN];	
					u8 tha[ETH_ALEN];	
				} arp;
			};
		} ipv4;
		struct {
			struct {
				struct in6_addr src;	
				struct in6_addr dst;	
			} addr;
			__be32 label;			
			struct {
				__be16 src;		
				__be16 dst;		
			} tp;
			struct {
				struct in6_addr target;	
				u8 sll[ETH_ALEN];	
				u8 tll[ETH_ALEN];	
			} nd;
		} ipv6;
	};
};

struct sw_flow {
	struct rcu_head rcu;
	struct hlist_node hash_node[2];
	u32 hash;

	struct sw_flow_key key;
	struct sw_flow_actions __rcu *sf_acts;

	spinlock_t lock;	
	unsigned long used;	
	u64 packet_count;	
	u64 byte_count;		
	u8 tcp_flags;		
};

struct arp_eth_header {
	__be16      ar_hrd;	
	__be16      ar_pro;	
	unsigned char   ar_hln;	
	unsigned char   ar_pln;	
	__be16      ar_op;	

	
	unsigned char       ar_sha[ETH_ALEN];	
	unsigned char       ar_sip[4];		
	unsigned char       ar_tha[ETH_ALEN];	
	unsigned char       ar_tip[4];		
} __packed;

int ovs_flow_init(void);
void ovs_flow_exit(void);

struct sw_flow *ovs_flow_alloc(void);
void ovs_flow_deferred_free(struct sw_flow *);
void ovs_flow_free(struct sw_flow *flow);

struct sw_flow_actions *ovs_flow_actions_alloc(const struct nlattr *);
void ovs_flow_deferred_free_acts(struct sw_flow_actions *);

int ovs_flow_extract(struct sk_buff *, u16 in_port, struct sw_flow_key *,
		     int *key_lenp);
void ovs_flow_used(struct sw_flow *, struct sk_buff *);
u64 ovs_flow_used_time(unsigned long flow_jiffies);

#define FLOW_BUFSIZE 132

int ovs_flow_to_nlattrs(const struct sw_flow_key *, struct sk_buff *);
int ovs_flow_from_nlattrs(struct sw_flow_key *swkey, int *key_lenp,
		      const struct nlattr *);
int ovs_flow_metadata_from_nlattrs(u32 *priority, u16 *in_port,
			       const struct nlattr *);

#define TBL_MIN_BUCKETS		1024

struct flow_table {
	struct flex_array *buckets;
	unsigned int count, n_buckets;
	struct rcu_head rcu;
	int node_ver;
	u32 hash_seed;
	bool keep_flows;
};

static inline int ovs_flow_tbl_count(struct flow_table *table)
{
	return table->count;
}

static inline int ovs_flow_tbl_need_to_expand(struct flow_table *table)
{
	return (table->count > table->n_buckets);
}

struct sw_flow *ovs_flow_tbl_lookup(struct flow_table *table,
				    struct sw_flow_key *key, int len);
void ovs_flow_tbl_destroy(struct flow_table *table);
void ovs_flow_tbl_deferred_destroy(struct flow_table *table);
struct flow_table *ovs_flow_tbl_alloc(int new_size);
struct flow_table *ovs_flow_tbl_expand(struct flow_table *table);
struct flow_table *ovs_flow_tbl_rehash(struct flow_table *table);
void ovs_flow_tbl_insert(struct flow_table *table, struct sw_flow *flow);
void ovs_flow_tbl_remove(struct flow_table *table, struct sw_flow *flow);
u32 ovs_flow_hash(const struct sw_flow_key *key, int key_len);

struct sw_flow *ovs_flow_tbl_next(struct flow_table *table, u32 *bucket, u32 *idx);
extern const int ovs_key_lens[OVS_KEY_ATTR_MAX + 1];

#endif 
