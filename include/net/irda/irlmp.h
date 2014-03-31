/*********************************************************************
 *                
 * Filename:      irlmp.h
 * Version:       0.9
 * Description:   IrDA Link Management Protocol (LMP) layer
 * Status:        Experimental.
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Sun Aug 17 20:54:32 1997
 * Modified at:   Fri Dec 10 13:23:01 1999
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

#ifndef IRLMP_H
#define IRLMP_H

#include <asm/param.h>  

#include <linux/types.h>

#include <net/irda/irda.h>
#include <net/irda/qos.h>
#include <net/irda/irlap.h>		
#include <net/irda/irlmp_event.h>
#include <net/irda/irqueue.h>
#include <net/irda/discovery.h>

#define LSAP_MASK     0x7f
#define LSAP_IAS      0x00
#define LSAP_ANY      0xff
#define LSAP_MAX      0x6f 
#define LSAP_CONNLESS 0x70 

#define DEV_ADDR_ANY  0xffffffff

#define LMP_HEADER          2    
#define LMP_CONTROL_HEADER  4    
#define LMP_PID_HEADER      1    
#define LMP_MAX_HEADER      (LMP_CONTROL_HEADER+LAP_MAX_HEADER)

#define LM_MAX_CONNECTIONS  10

#define LM_IDLE_TIMEOUT     2*HZ 

typedef enum {
	S_PNP = 0,
	S_PDA,
	S_COMPUTER,
	S_PRINTER,
	S_MODEM,
	S_FAX,
	S_LAN,
	S_TELEPHONY,
	S_COMM,
	S_OBEX,
	S_ANY,
	S_END,
} SERVICE;

typedef void (*DISCOVERY_CALLBACK1) (discinfo_t *, DISCOVERY_MODE, void *);
typedef void (*DISCOVERY_CALLBACK2) (discinfo_t *, DISCOVERY_MODE, void *);

typedef struct {
	irda_queue_t queue; 

	__u16_host_order hints; 
} irlmp_service_t;

typedef struct {
	irda_queue_t queue; 

	__u16_host_order hint_mask;

	DISCOVERY_CALLBACK1 disco_callback;	
	DISCOVERY_CALLBACK2 expir_callback;	
	void *priv;                
} irlmp_client_t;

struct lsap_cb {
	irda_queue_t queue;      
	magic_t magic;

	unsigned long connected;	
	int  persistent;

	__u8 slsap_sel;   
	__u8 dlsap_sel;   
#ifdef CONFIG_IRDA_ULTRA
	__u8 pid;         
#endif 
	struct sk_buff *conn_skb; 

	struct timer_list watchdog_timer;

	LSAP_STATE      lsap_state;  
	notify_t        notify;      
	struct qos_info qos;         

	struct lap_cb *lap; 
};

typedef struct {
	int valid;

	__u8 slsap_sel;
	__u8 dlsap_sel;
	struct lsap_cb *lsap;
} CACHE_ENTRY;

struct lap_cb {
	irda_queue_t queue; 
	magic_t magic;

	int reason;    

	IRLMP_STATE lap_state;

	struct irlap_cb *irlap;   
	hashbin_t *lsaps;         
	struct lsap_cb *flow_next;	

	__u8  caddr;  
 	__u32 saddr;  
 	__u32 daddr;  
	
	struct qos_info *qos;  
	struct timer_list idle_timer;
	
#ifdef CONFIG_IRDA_CACHE_LAST_LSAP
	CACHE_ENTRY cache;  
#endif
};

struct irlmp_cb {
	magic_t magic;

	__u8 conflict_flag;
	
	discovery_t discovery_cmd; 
	discovery_t discovery_rsp; 

	
	int	last_lsap_sel;

	struct timer_list discovery_timer;

 	hashbin_t *links;         
	hashbin_t *unconnected_lsaps;
 	hashbin_t *clients;
	hashbin_t *services;

	hashbin_t *cachelog;	

	int running;

	__u16_host_order hints; 
};

int  irlmp_init(void);
void irlmp_cleanup(void);
struct lsap_cb *irlmp_open_lsap(__u8 slsap, notify_t *notify, __u8 pid);
void irlmp_close_lsap( struct lsap_cb *self);

__u16 irlmp_service_to_hint(int service);
void *irlmp_register_service(__u16 hints);
int irlmp_unregister_service(void *handle);
void *irlmp_register_client(__u16 hint_mask, DISCOVERY_CALLBACK1 disco_clb,
			    DISCOVERY_CALLBACK2 expir_clb, void *priv);
int irlmp_unregister_client(void *handle);
int irlmp_update_client(void *handle, __u16 hint_mask, 
			DISCOVERY_CALLBACK1 disco_clb,
			DISCOVERY_CALLBACK2 expir_clb, void *priv);

void irlmp_register_link(struct irlap_cb *, __u32 saddr, notify_t *);
void irlmp_unregister_link(__u32 saddr);

int  irlmp_connect_request(struct lsap_cb *, __u8 dlsap_sel, 
			   __u32 saddr, __u32 daddr,
			   struct qos_info *, struct sk_buff *);
void irlmp_connect_indication(struct lsap_cb *self, struct sk_buff *skb);
int  irlmp_connect_response(struct lsap_cb *, struct sk_buff *);
void irlmp_connect_confirm(struct lsap_cb *, struct sk_buff *);
struct lsap_cb *irlmp_dup(struct lsap_cb *self, void *instance);

void irlmp_disconnect_indication(struct lsap_cb *self, LM_REASON reason, 
				 struct sk_buff *userdata);
int  irlmp_disconnect_request(struct lsap_cb *, struct sk_buff *userdata);

void irlmp_discovery_confirm(hashbin_t *discovery_log, DISCOVERY_MODE mode);
void irlmp_discovery_request(int nslots);
discinfo_t *irlmp_get_discoveries(int *pn, __u16 mask, int nslots);
void irlmp_do_expiry(void);
void irlmp_do_discovery(int nslots);
discovery_t *irlmp_get_discovery_response(void);
void irlmp_discovery_expiry(discinfo_t *expiry, int number);

int  irlmp_data_request(struct lsap_cb *, struct sk_buff *);
void irlmp_data_indication(struct lsap_cb *, struct sk_buff *);

int  irlmp_udata_request(struct lsap_cb *, struct sk_buff *);
void irlmp_udata_indication(struct lsap_cb *, struct sk_buff *);

#ifdef CONFIG_IRDA_ULTRA
int  irlmp_connless_data_request(struct lsap_cb *, struct sk_buff *, __u8);
void irlmp_connless_data_indication(struct lsap_cb *, struct sk_buff *);
#endif 

void irlmp_status_indication(struct lap_cb *, LINK_STATUS link, LOCK_STATUS lock);
void irlmp_flow_indication(struct lap_cb *self, LOCAL_FLOW flow);

LM_REASON irlmp_convert_lap_reason(LAP_REASON);

static inline __u32 irlmp_get_saddr(const struct lsap_cb *self)
{
	return (self && self->lap) ? self->lap->saddr : 0;
}

static inline __u32 irlmp_get_daddr(const struct lsap_cb *self)
{
	return (self && self->lap) ? self->lap->daddr : 0;
}

extern const char *irlmp_reasons[];
extern int sysctl_discovery_timeout;
extern int sysctl_discovery_slots;
extern int sysctl_discovery;
extern int sysctl_lap_keepalive_time;	
extern struct irlmp_cb *irlmp;

static inline int irlmp_lap_tx_queue_full(struct lsap_cb *self)
{
	if (self == NULL)
		return 0;
	if (self->lap == NULL)
		return 0;
	if (self->lap->irlap == NULL)
		return 0;

	return IRLAP_GET_TX_QUEUE_LEN(self->lap->irlap) >= LAP_HIGH_THRESHOLD;
}

static inline void irlmp_listen(struct lsap_cb *self)
{
	self->dlsap_sel = LSAP_ANY;
	self->lap = NULL;
	self->lsap_state = LSAP_DISCONNECTED;
	
	del_timer(&self->watchdog_timer);
}

#endif
