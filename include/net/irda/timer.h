/*********************************************************************
 *                
 * Filename:      timer.h
 * Version:       
 * Description:   
 * Status:        Experimental.
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Sat Aug 16 00:59:29 1997
 * Modified at:   Thu Oct  7 12:25:24 1999
 * Modified by:   Dag Brattli <dagb@cs.uit.no>
 * 
 *     Copyright (c) 1997, 1998-1999 Dag Brattli <dagb@cs.uit.no>, 
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

#ifndef TIMER_H
#define TIMER_H

#include <linux/timer.h>
#include <linux/jiffies.h>

#include <asm/param.h>  

#include <net/irda/irda.h>

struct irlmp_cb;
struct irlap_cb;
struct lsap_cb;
struct lap_cb;

#define POLL_TIMEOUT        (450*HZ/1000)    
#define FINAL_TIMEOUT       (500*HZ/1000)    

#define WD_TIMEOUT          (POLL_TIMEOUT*2)

#define MEDIABUSY_TIMEOUT   (500*HZ/1000)    
#define SMALLBUSY_TIMEOUT   (100*HZ/1000)    

#define SLOT_TIMEOUT            (90*HZ/1000)

#define XIDEXTRA_TIMEOUT        (34*HZ/1000)  

#define WATCHDOG_TIMEOUT        (20*HZ)       

typedef void (*TIMER_CALLBACK)(void *);

static inline void irda_start_timer(struct timer_list *ptimer, int timeout, 
				    void* data, TIMER_CALLBACK callback)
{
	ptimer->function = (void (*)(unsigned long)) callback;
	ptimer->data = (unsigned long) data;
	
	mod_timer(ptimer, jiffies + timeout);
}


void irlap_start_slot_timer(struct irlap_cb *self, int timeout);
void irlap_start_query_timer(struct irlap_cb *self, int S, int s);
void irlap_start_final_timer(struct irlap_cb *self, int timeout);
void irlap_start_wd_timer(struct irlap_cb *self, int timeout);
void irlap_start_backoff_timer(struct irlap_cb *self, int timeout);

void irlap_start_mbusy_timer(struct irlap_cb *self, int timeout);
void irlap_stop_mbusy_timer(struct irlap_cb *);

void irlmp_start_watchdog_timer(struct lsap_cb *, int timeout);
void irlmp_start_discovery_timer(struct irlmp_cb *, int timeout);
void irlmp_start_idle_timer(struct lap_cb *, int timeout);
void irlmp_stop_idle_timer(struct lap_cb *self);

#endif

