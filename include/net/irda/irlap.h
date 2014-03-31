/*********************************************************************
 *                
 * Filename:      irlap.h
 * Version:       0.8
 * Description:   An IrDA LAP driver for Linux
 * Status:        Experimental.
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Mon Aug  4 20:40:53 1997
 * Modified at:   Fri Dec 10 13:21:17 1999
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

#ifndef IRLAP_H
#define IRLAP_H

#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/timer.h>

#include <net/irda/irqueue.h>		
#include <net/irda/qos.h>		
#include <net/irda/discovery.h>		
#include <net/irda/irlap_event.h>	
#include <net/irda/irmod.h>		

#define CONFIG_IRDA_DYNAMIC_WINDOW 1

#define LAP_RELIABLE   1
#define LAP_UNRELIABLE 0

#define LAP_ADDR_HEADER 1  
#define LAP_CTRL_HEADER 1  

#define LAP_MAX_HEADER (LAP_ADDR_HEADER + LAP_CTRL_HEADER)

#define LAP_ALEN 4

#define BROADCAST  0xffffffff 
#define CBROADCAST 0xfe       
#define XID_FORMAT 0x01       

#define LAP_WINDOW_SIZE 8
#define LAP_HIGH_THRESHOLD     2
#define LAP_MAX_QUEUE 10

#define NR_EXPECTED     1
#define NR_UNEXPECTED   0
#define NR_INVALID     -1

#define NS_EXPECTED     1
#define NS_UNEXPECTED   0
#define NS_INVALID     -1

struct irlap_info {
	__u8 caddr;   
	__u8 control; 
        __u8 cmd;

	__u32 saddr;
	__u32 daddr;
	
	int pf;        

	__u8  nr;      
	__u8  ns;      

	int  S;        
	int  slot;     
	int  s;        

	discovery_t *discovery; 
};

struct irlap_cb {
	irda_queue_t q;     
	magic_t magic;

	
	struct net_device  *netdev;
	char		hw_name[2*IFNAMSIZ + 1];

	
	volatile IRLAP_STATE state;       

	
	struct timer_list query_timer;
	struct timer_list slot_timer;
	struct timer_list discovery_timer;
	struct timer_list final_timer;
	struct timer_list poll_timer;
	struct timer_list wd_timer;
	struct timer_list backoff_timer;

	
	struct timer_list media_busy_timer;
	int media_busy;

	
	int slot_timeout;
	int poll_timeout;
	int final_timeout;
	int wd_timeout;

	struct sk_buff_head txq;  
	struct sk_buff_head txq_ultra;

 	__u8    caddr;        
	__u32   saddr;        
	__u32   daddr;        

	int     retry_count;  
	int     add_wait;     

	__u8    connect_pending;
	__u8    disconnect_pending;

	
#ifdef CONFIG_IRDA_FAST_RR
	int     fast_RR_timeout;
	int     fast_RR;      
#endif 
	
	int N1; 
	int N2; 
	int N3; 

	int     local_busy;
	int     remote_busy;
	int     xmitflag;

	__u8    vs;            
	__u8    vr;            
	__u8    va;            
 	int     window;        
	int     window_size;   

#ifdef CONFIG_IRDA_DYNAMIC_WINDOW
	__u32   line_capacity; 
	__u32   bytes_left;    
#endif 

	struct sk_buff_head wx_list;

	__u8    ack_required;
	
	
 	__u8    S;           
	__u8    slot;        
 	__u8    s;           
	int     frame_sent;  

	hashbin_t   *discovery_log;
 	discovery_t *discovery_cmd;

	__u32 speed;		

	struct qos_info  qos_tx;   
	struct qos_info  qos_rx;   
	struct qos_info *qos_dev;  

	notify_t notify; 

	int    mtt_required;  
	int    xbofs_delay;   
	int    bofs_count;    
	int    next_bofs;     

	int    mode;     
};

int irlap_init(void);
void irlap_cleanup(void);

struct irlap_cb *irlap_open(struct net_device *dev, struct qos_info *qos,
			    const char *hw_name);
void irlap_close(struct irlap_cb *self);

void irlap_connect_request(struct irlap_cb *self, __u32 daddr, 
			   struct qos_info *qos, int sniff);
void irlap_connect_response(struct irlap_cb *self, struct sk_buff *skb);
void irlap_connect_indication(struct irlap_cb *self, struct sk_buff *skb);
void irlap_connect_confirm(struct irlap_cb *, struct sk_buff *skb);

void irlap_data_indication(struct irlap_cb *, struct sk_buff *, int unreliable);
void irlap_data_request(struct irlap_cb *, struct sk_buff *, int unreliable);

#ifdef CONFIG_IRDA_ULTRA
void irlap_unitdata_request(struct irlap_cb *, struct sk_buff *);
void irlap_unitdata_indication(struct irlap_cb *, struct sk_buff *);
#endif 

void irlap_disconnect_request(struct irlap_cb *);
void irlap_disconnect_indication(struct irlap_cb *, LAP_REASON reason);

void irlap_status_indication(struct irlap_cb *, int quality_of_link);

void irlap_test_request(__u8 *info, int len);

void irlap_discovery_request(struct irlap_cb *, discovery_t *discovery);
void irlap_discovery_confirm(struct irlap_cb *, hashbin_t *discovery_log);
void irlap_discovery_indication(struct irlap_cb *, discovery_t *discovery);

void irlap_reset_indication(struct irlap_cb *self);
void irlap_reset_confirm(void);

void irlap_update_nr_received(struct irlap_cb *, int nr);
int irlap_validate_nr_received(struct irlap_cb *, int nr);
int irlap_validate_ns_received(struct irlap_cb *, int ns);

int  irlap_generate_rand_time_slot(int S, int s);
void irlap_initiate_connection_state(struct irlap_cb *);
void irlap_flush_all_queues(struct irlap_cb *);
void irlap_wait_min_turn_around(struct irlap_cb *, struct qos_info *);

void irlap_apply_default_connection_parameters(struct irlap_cb *self);
void irlap_apply_connection_parameters(struct irlap_cb *self, int now);

#define IRLAP_GET_HEADER_SIZE(self) (LAP_MAX_HEADER)
#define IRLAP_GET_TX_QUEUE_LEN(self) skb_queue_len(&self->txq)

static inline int irlap_is_primary(struct irlap_cb *self)
{
	int ret;
	switch(self->state) {
	case LAP_XMIT_P:
	case LAP_NRM_P:
		ret = 1;
		break;
	case LAP_XMIT_S:
	case LAP_NRM_S:
		ret = 0;
		break;
	default:
		ret = -1;
	}
	return ret;
}

static inline void irlap_clear_disconnect(struct irlap_cb *self)
{
	self->disconnect_pending = FALSE;
}

static inline void irlap_next_state(struct irlap_cb *self, IRLAP_STATE state)
{
	self->state = state;
}

#endif
