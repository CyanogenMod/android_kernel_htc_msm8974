/*
 *  linux/include/linux/inet_lro.h
 *
 *  Large Receive Offload (ipv4 / tcp)
 *
 *  (C) Copyright IBM Corp. 2007
 *
 *  Authors:
 *       Jan-Bernd Themann <themann@de.ibm.com>
 *       Christoph Raisch <raisch@de.ibm.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
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

#ifndef __INET_LRO_H_
#define __INET_LRO_H_

#include <net/ip.h>
#include <net/tcp.h>


struct net_lro_stats {
	unsigned long aggregated;
	unsigned long flushed;
	unsigned long no_desc;
};

struct net_lro_desc {
	struct sk_buff *parent;
	struct sk_buff *last_skb;
	struct skb_frag_struct *next_frag;
	struct iphdr *iph;
	struct tcphdr *tcph;
	__wsum  data_csum;
	__be32 tcp_rcv_tsecr;
	__be32 tcp_rcv_tsval;
	__be32 tcp_ack;
	u32 tcp_next_seq;
	u32 skb_tot_frags_len;
	u16 ip_tot_len;
	u16 tcp_saw_tstamp; 		
	__be16 tcp_window;
	int pkt_aggr_cnt;		
	int vlan_packet;
	int mss;
	int active;
};


struct net_lro_mgr {
	struct net_device *dev;
	struct net_lro_stats stats;

	
	unsigned long features;
#define LRO_F_NAPI            1  
#define LRO_F_EXTRACT_VLAN_ID 2  

	u32 ip_summed;
	u32 ip_summed_aggr; 

	int max_desc; 
	int max_aggr; 

	int frag_align_pad; 

	struct net_lro_desc *lro_arr; 

	int (*get_skb_header)(struct sk_buff *skb, void **ip_hdr,
			      void **tcpudp_hdr, u64 *hdr_flags, void *priv);

	
#define LRO_IPV4 1 
#define LRO_TCP  2 

	int (*get_frag_header)(struct skb_frag_struct *frag, void **mac_hdr,
			       void **ip_hdr, void **tcpudp_hdr, u64 *hdr_flags,
			       void *priv);
};


void lro_receive_skb(struct net_lro_mgr *lro_mgr,
		     struct sk_buff *skb,
		     void *priv);


void lro_receive_frags(struct net_lro_mgr *lro_mgr,
		       struct skb_frag_struct *frags,
		       int len, int true_size, void *priv, __wsum sum);


void lro_flush_all(struct net_lro_mgr *lro_mgr);

void lro_flush_pkt(struct net_lro_mgr *lro_mgr,
		   struct iphdr *iph, struct tcphdr *tcph);

#endif
