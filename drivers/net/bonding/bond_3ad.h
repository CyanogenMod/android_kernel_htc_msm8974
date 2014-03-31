/*
 * Copyright(c) 1999 - 2004 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 */

#ifndef __BOND_3AD_H__
#define __BOND_3AD_H__

#include <asm/byteorder.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/if_ether.h>

#define PKT_TYPE_LACPDU         cpu_to_be16(ETH_P_SLOW)
#define AD_TIMER_INTERVAL       100 

#define MULTICAST_LACPDU_ADDR    {0x01, 0x80, 0xC2, 0x00, 0x00, 0x02}

#define AD_LACP_SLOW 0
#define AD_LACP_FAST 1

typedef struct mac_addr {
	u8 mac_addr_value[ETH_ALEN];
} __packed mac_addr_t;

enum {
	BOND_AD_STABLE = 0,
	BOND_AD_BANDWIDTH = 1,
	BOND_AD_COUNT = 2,
};

typedef enum {
	AD_RX_DUMMY,
	AD_RX_INITIALIZE,     
	AD_RX_PORT_DISABLED,  
	AD_RX_LACP_DISABLED,  
	AD_RX_EXPIRED,	      
	AD_RX_DEFAULTED,      
	AD_RX_CURRENT	      
} rx_states_t;

typedef enum {
	AD_PERIODIC_DUMMY,
	AD_NO_PERIODIC,	       
	AD_FAST_PERIODIC,      
	AD_SLOW_PERIODIC,      
	AD_PERIODIC_TX	   
} periodic_states_t;

typedef enum {
	AD_MUX_DUMMY,
	AD_MUX_DETACHED,       
	AD_MUX_WAITING,	       
	AD_MUX_ATTACHED,       
	AD_MUX_COLLECTING_DISTRIBUTING 
} mux_states_t;

typedef enum {
	AD_TX_DUMMY,
	AD_TRANSMIT	   
} tx_states_t;

typedef enum {
	AD_TYPE_LACPDU = 1,    
	AD_TYPE_MARKER	   
} pdu_type_t;

typedef enum {
	AD_MARKER_INFORMATION_SUBTYPE = 1, 
	AD_MARKER_RESPONSE_SUBTYPE     
} bond_marker_subtype_t;

typedef enum {
	AD_CURRENT_WHILE_TIMER,
	AD_ACTOR_CHURN_TIMER,
	AD_PERIODIC_TIMER,
	AD_PARTNER_CHURN_TIMER,
	AD_WAIT_WHILE_TIMER
} ad_timers_t;

#pragma pack(1)

typedef struct lacpdu {
	u8 subtype;		     
	u8 version_number;
	u8 tlv_type_actor_info;	      
	u8 actor_information_length; 
	__be16 actor_system_priority;
	struct mac_addr actor_system;
	__be16 actor_key;
	__be16 actor_port_priority;
	__be16 actor_port;
	u8 actor_state;
	u8 reserved_3_1[3];	     
	u8 tlv_type_partner_info;     
	u8 partner_information_length;	 
	__be16 partner_system_priority;
	struct mac_addr partner_system;
	__be16 partner_key;
	__be16 partner_port_priority;
	__be16 partner_port;
	u8 partner_state;
	u8 reserved_3_2[3];	     
	u8 tlv_type_collector_info;	  
	u8 collector_information_length; 
	__be16 collector_max_delay;
	u8 reserved_12[12];
	u8 tlv_type_terminator;	     
	u8 terminator_length;	     
	u8 reserved_50[50];	     
} __packed lacpdu_t;

typedef struct lacpdu_header {
	struct ethhdr hdr;
	struct lacpdu lacpdu;
} __packed lacpdu_header_t;

typedef struct bond_marker {
	u8 subtype;		 
	u8 version_number;	 
	u8 tlv_type;		 
	
	u8 marker_length;	 
	u16 requester_port;	 
	struct mac_addr requester_system;      
	u32 requester_transaction_id;	
	u16 pad;		 
	u8 tlv_type_terminator;	     
	u8 terminator_length;	     
	u8 reserved_90[90];	     
} __packed bond_marker_t;

typedef struct bond_marker_header {
	struct ethhdr hdr;
	struct bond_marker marker;
} __packed bond_marker_header_t;

#pragma pack()

struct slave;
struct bonding;
struct ad_info;
struct port;

#ifdef __ia64__
#pragma pack(8)
#endif

typedef struct aggregator {
	struct mac_addr aggregator_mac_address;
	u16 aggregator_identifier;
	bool is_individual;
	u16 actor_admin_aggregator_key;
	u16 actor_oper_aggregator_key;
	struct mac_addr partner_system;
	u16 partner_system_priority;
	u16 partner_oper_aggregator_key;
	u16 receive_state;		
	u16 transmit_state;		
	struct port *lag_ports;
	
	struct slave *slave;	    
	u16 is_active;	    
	u16 num_of_ports;
} aggregator_t;

struct port_params {
	struct mac_addr system;
	u16 system_priority;
	u16 key;
	u16 port_number;
	u16 port_priority;
	u16 port_state;
};

typedef struct port {
	u16 actor_port_number;
	u16 actor_port_priority;
	struct mac_addr actor_system;	       
	u16 actor_system_priority;	 
	u16 actor_port_aggregator_identifier;
	bool ntt;
	u16 actor_admin_port_key;
	u16 actor_oper_port_key;
	u8 actor_admin_port_state;
	u8 actor_oper_port_state;

	struct port_params partner_admin;
	struct port_params partner_oper;

	bool is_enabled;

	
	u16 sm_vars;	      
	rx_states_t sm_rx_state;	
	u16 sm_rx_timer_counter;    
	periodic_states_t sm_periodic_state;
	u16 sm_periodic_timer_counter;	
	mux_states_t sm_mux_state;	
	u16 sm_mux_timer_counter;   
	tx_states_t sm_tx_state;	
	u16 sm_tx_timer_counter;    
	struct slave *slave;	    
	struct aggregator *aggregator;	   
	struct port *next_port_in_aggregator; 
	u32 transaction_id;	    
	struct lacpdu lacpdu;	       
} port_t;

struct ad_system {
	u16 sys_priority;
	struct mac_addr sys_mac_addr;
};

#ifdef __ia64__
#pragma pack()
#endif

#define BOND_AD_INFO(bond)   ((bond)->ad_info)
#define SLAVE_AD_INFO(slave) ((slave)->ad_info)

struct ad_bond_info {
	struct ad_system system;	    
	u32 agg_select_timer;	    
};

struct ad_slave_info {
	struct aggregator aggregator;	    
	struct port port;		    
	spinlock_t state_machine_lock; 
	u16 id;
};

void bond_3ad_initialize(struct bonding *bond, u16 tick_resolution);
int  bond_3ad_bind_slave(struct slave *slave);
void bond_3ad_unbind_slave(struct slave *slave);
void bond_3ad_state_machine_handler(struct work_struct *);
void bond_3ad_initiate_agg_selection(struct bonding *bond, int timeout);
void bond_3ad_adapter_speed_changed(struct slave *slave);
void bond_3ad_adapter_duplex_changed(struct slave *slave);
void bond_3ad_handle_link_change(struct slave *slave, char link);
int  bond_3ad_get_active_agg_info(struct bonding *bond, struct ad_info *ad_info);
int bond_3ad_xmit_xor(struct sk_buff *skb, struct net_device *dev);
int bond_3ad_lacpdu_recv(struct sk_buff *skb, struct bonding *bond,
			  struct slave *slave);
int bond_3ad_set_carrier(struct bonding *bond);
void bond_3ad_update_lacp_rate(struct bonding *bond);
#endif 

