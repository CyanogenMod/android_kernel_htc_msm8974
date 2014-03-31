/*
 * This file is part of the Chelsio T4 Ethernet driver for Linux.
 *
 * Copyright (c) 2003-2010 Chelsio Communications, Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __CXGB4_L2T_H
#define __CXGB4_L2T_H

#include <linux/spinlock.h>
#include <linux/if_ether.h>
#include <linux/atomic.h>

struct adapter;
struct l2t_data;
struct neighbour;
struct net_device;
struct file_operations;
struct cpl_l2t_write_rpl;

struct l2t_entry {
	u16 state;                  
	u16 idx;                    
	u32 addr[4];                
	int ifindex;                
	struct neighbour *neigh;    
	struct l2t_entry *first;    
	struct l2t_entry *next;     
	struct sk_buff *arpq_head;  
	struct sk_buff *arpq_tail;
	spinlock_t lock;
	atomic_t refcnt;            
	u16 hash;                   
	u16 vlan;                   
	u8 v6;                      
	u8 lport;                   
	u8 dmac[ETH_ALEN];          
};

typedef void (*arp_err_handler_t)(void *handle, struct sk_buff *skb);

struct l2t_skb_cb {
	void *handle;
	arp_err_handler_t arp_err_handler;
};

#define L2T_SKB_CB(skb) ((struct l2t_skb_cb *)(skb)->cb)

static inline void t4_set_arp_err_handler(struct sk_buff *skb, void *handle,
					  arp_err_handler_t handler)
{
	L2T_SKB_CB(skb)->handle = handle;
	L2T_SKB_CB(skb)->arp_err_handler = handler;
}

void cxgb4_l2t_release(struct l2t_entry *e);
int cxgb4_l2t_send(struct net_device *dev, struct sk_buff *skb,
		   struct l2t_entry *e);
struct l2t_entry *cxgb4_l2t_get(struct l2t_data *d, struct neighbour *neigh,
				const struct net_device *physdev,
				unsigned int priority);

void t4_l2t_update(struct adapter *adap, struct neighbour *neigh);
struct l2t_data *t4_init_l2t(void);
void do_l2t_write_rpl(struct adapter *p, const struct cpl_l2t_write_rpl *rpl);

extern const struct file_operations t4_l2t_fops;
#endif  
