/*********************************************************************
 *                
 * Filename:      irttp.h
 * Version:       1.0
 * Description:   Tiny Transport Protocol (TTP) definitions
 * Status:        Experimental.
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Sun Aug 31 20:14:31 1997
 * Modified at:   Sun Dec 12 13:09:07 1999
 * Modified by:   Dag Brattli <dagb@cs.uit.no>
 * 
 *     Copyright (c) 1998-1999 Dag Brattli <dagb@cs.uit.no>, 
 *     All Rights Reserved.
 *     Copyright (c) 2000-2002 Jean Tourrilhes <jt@hpl.hp.com>
 *     
 *     This program is free software; you can redistribute it and/or 
 *     modify it under the terms of the GNU General Public License as 
 *     published by the Free Software Foundation; either version 2 of 
 *     the License, or (at your option) any later version.
 *
 *     Neither Dag Brattli nor University of Tromsø admit liability nor
 *     provide warranty for any of this software. This material is 
 *     provided "AS-IS" and at no charge.
 *
 ********************************************************************/

#ifndef IRTTP_H
#define IRTTP_H

#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>

#include <net/irda/irda.h>
#include <net/irda/irlmp.h>		
#include <net/irda/qos.h>		
#include <net/irda/irqueue.h>

#define TTP_MAX_CONNECTIONS    LM_MAX_CONNECTIONS
#define TTP_HEADER             1
#define TTP_MAX_HEADER         (TTP_HEADER + LMP_MAX_HEADER)
#define TTP_SAR_HEADER         5
#define TTP_PARAMETERS         0x80
#define TTP_MORE               0x80

#define TTP_TX_MAX_QUEUE	14
#define TTP_TX_LOW_THRESHOLD	5
#define TTP_TX_HIGH_THRESHOLD	7

#define TTP_RX_MIN_CREDIT	8
#define TTP_RX_DEFAULT_CREDIT	16
#define TTP_RX_MAX_CREDIT	21

#define DEFAULT_INITIAL_CREDIT	TTP_RX_DEFAULT_CREDIT

#define P_NORMAL    0
#define P_HIGH      1

#define TTP_SAR_DISABLE 0
#define TTP_SAR_UNBOUND 0xffffffff

#define TTP_MAX_SDU_SIZE 0x01

struct tsap_cb {
	irda_queue_t q;            
	magic_t magic;        

	__u8 stsap_sel;       
	__u8 dtsap_sel;       

	struct lsap_cb *lsap; 

	__u8 connected;       
	 
	__u8 initial_credit;  

        int avail_credit;    
	int remote_credit;   
	int send_credit;     
	
	struct sk_buff_head tx_queue; 
	struct sk_buff_head rx_queue; 
	struct sk_buff_head rx_fragments;
	int tx_queue_lock;
	int rx_queue_lock;
	spinlock_t lock;

	notify_t notify;       

	struct net_device_stats stats;
	struct timer_list todo_timer; 

	__u32 max_seg_size;     
	__u8  max_header_size;

	int   rx_sdu_busy;     
	__u32 rx_sdu_size;     
	__u32 rx_max_sdu_size; 

	int tx_sdu_busy;       
	__u32 tx_max_sdu_size; 

	int close_pend;        
	unsigned long disconnect_pend; 
	struct sk_buff *disconnect_skb;
};

struct irttp_cb {
	magic_t    magic;	
	hashbin_t *tsaps;
};

int  irttp_init(void);
void irttp_cleanup(void);

struct tsap_cb *irttp_open_tsap(__u8 stsap_sel, int credit, notify_t *notify);
int irttp_close_tsap(struct tsap_cb *self);

int irttp_data_request(struct tsap_cb *self, struct sk_buff *skb);
int irttp_udata_request(struct tsap_cb *self, struct sk_buff *skb);

int irttp_connect_request(struct tsap_cb *self, __u8 dtsap_sel, 
			  __u32 saddr, __u32 daddr,
			  struct qos_info *qos, __u32 max_sdu_size, 
			  struct sk_buff *userdata);
int irttp_connect_response(struct tsap_cb *self, __u32 max_sdu_size, 
			    struct sk_buff *userdata);
int irttp_disconnect_request(struct tsap_cb *self, struct sk_buff *skb,
			     int priority);
void irttp_flow_request(struct tsap_cb *self, LOCAL_FLOW flow);
struct tsap_cb *irttp_dup(struct tsap_cb *self, void *instance);

static inline __u32 irttp_get_saddr(struct tsap_cb *self)
{
	return irlmp_get_saddr(self->lsap);
}

static inline __u32 irttp_get_daddr(struct tsap_cb *self)
{
	return irlmp_get_daddr(self->lsap);
}

static inline __u32 irttp_get_max_seg_size(struct tsap_cb *self)
{
	return self->max_seg_size;
}

static inline void irttp_listen(struct tsap_cb *self)
{
	irlmp_listen(self->lsap);
	self->dtsap_sel = LSAP_ANY;
}

static inline int irttp_is_primary(struct tsap_cb *self)
{
	if ((self == NULL) ||
	    (self->lsap == NULL) ||
	    (self->lsap->lap == NULL) ||
	    (self->lsap->lap->irlap == NULL))
		return -2;
	return irlap_is_primary(self->lsap->lap->irlap);
}

#endif 
