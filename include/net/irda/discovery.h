/*********************************************************************
 *                
 * Filename:      discovery.h
 * Version:       
 * Description:   
 * Status:        Experimental.
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Tue Apr  6 16:53:53 1999
 * Modified at:   Tue Oct  5 10:05:10 1999
 * Modified by:   Dag Brattli <dagb@cs.uit.no>
 * 
 *     Copyright (c) 1999 Dag Brattli, All Rights Reserved.
 *     Copyright (c) 2000-2002 Jean Tourrilhes <jt@hpl.hp.com>
 *     
 *     This program is free software; you can redistribute it and/or 
 *     modify it under the terms of the GNU General Public License as 
 *     published by the Free Software Foundation; either version 2 of 
 *     the License, or (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License 
 *     along with this program; if not, write to the Free Software 
 *     Foundation, Inc., 59 Temple Place, Suite 330, Boston, 
 *     MA 02111-1307 USA
 *     
 ********************************************************************/

#ifndef DISCOVERY_H
#define DISCOVERY_H

#include <asm/param.h>

#include <net/irda/irda.h>
#include <net/irda/irqueue.h>		
#include <net/irda/irlap_event.h>	

#define DISCOVERY_EXPIRE_TIMEOUT (2*sysctl_discovery_timeout*HZ)
#define DISCOVERY_DEFAULT_SLOTS  0

typedef union {
	__u16 word;
	__u8  byte[2];
} __u16_host_order;

typedef enum {
	DISCOVERY_LOG,		
	DISCOVERY_ACTIVE,	
	DISCOVERY_PASSIVE,	
	EXPIRY_TIMEOUT,		
} DISCOVERY_MODE;

#define NICKNAME_MAX_LEN 21

typedef struct irda_device_info		discinfo_t;	

typedef struct discovery_t {
	irda_queue_t	q;		

	discinfo_t	data;		
	int		name_len;	

	LAP_REASON	condition;	
	int		gen_addr_bit;	
	int		nslots;		
	unsigned long	timestamp;	
	unsigned long	firststamp;	
} discovery_t;

void irlmp_add_discovery(hashbin_t *cachelog, discovery_t *discovery);
void irlmp_add_discovery_log(hashbin_t *cachelog, hashbin_t *log);
void irlmp_expire_discoveries(hashbin_t *log, __u32 saddr, int force);
struct irda_device_info *irlmp_copy_discoveries(hashbin_t *log, int *pn,
						__u16 mask, int old_entries);

#endif
