/*********************************************************************
 *                
 * Filename:      af_irda.h
 * Version:       1.0
 * Description:   IrDA sockets declarations
 * Status:        Stable
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Tue Dec  9 21:13:12 1997
 * Modified at:   Fri Jan 28 13:16:32 2000
 * Modified by:   Dag Brattli <dagb@cs.uit.no>
 * 
 *     Copyright (c) 1998-2000 Dag Brattli, All Rights Reserved.
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

#ifndef AF_IRDA_H
#define AF_IRDA_H

#include <linux/irda.h>
#include <net/irda/irda.h>
#include <net/irda/iriap.h>		
#include <net/irda/irias_object.h>	
#include <net/irda/irlmp.h>		
#include <net/irda/irttp.h>		
#include <net/irda/discovery.h>		
#include <net/sock.h>

struct irda_sock {
	
	struct sock sk;
	__u32 saddr;          
	__u32 daddr;          

	struct lsap_cb *lsap; 
	__u8  pid;            

	struct tsap_cb *tsap; 
	__u8 dtsap_sel;       
	__u8 stsap_sel;       
	
	__u32 max_sdu_size_rx;
	__u32 max_sdu_size_tx;
	__u32 max_data_size;
	__u8  max_header_size;
	struct qos_info qos_tx;

	__u16_host_order mask;           
	__u16_host_order hints;          

	void *ckey;           
	void *skey;           

	struct ias_object *ias_obj;   
	struct iriap_cb *iriap;	      
	struct ias_value *ias_result; 

	hashbin_t *cachelog;		
	__u32 cachedaddr;	

	int nslots;           

	int errno;            

	wait_queue_head_t query_wait;	
	struct timer_list watchdog;	

	LOCAL_FLOW tx_flow;
	LOCAL_FLOW rx_flow;
};

static inline struct irda_sock *irda_sk(struct sock *sk)
{
	return (struct irda_sock *)sk;
}

#endif 
