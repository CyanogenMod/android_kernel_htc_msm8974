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

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/skbuff.h>
#include <linux/if_ether.h>
#include <linux/netdevice.h>
#include <linux/spinlock.h>
#include <linux/ethtool.h>
#include <linux/etherdevice.h>
#include <linux/if_bonding.h>
#include <linux/pkt_sched.h>
#include <net/net_namespace.h>
#include "bonding.h"
#include "bond_3ad.h"

#define AD_SHORT_TIMEOUT           1
#define AD_LONG_TIMEOUT            0
#define AD_STANDBY                 0x2
#define AD_MAX_TX_IN_SECOND        3
#define AD_COLLECTOR_MAX_DELAY     0

#define AD_FAST_PERIODIC_TIME      1
#define AD_SLOW_PERIODIC_TIME      30
#define AD_SHORT_TIMEOUT_TIME      (3*AD_FAST_PERIODIC_TIME)
#define AD_LONG_TIMEOUT_TIME       (3*AD_SLOW_PERIODIC_TIME)
#define AD_CHURN_DETECTION_TIME    60
#define AD_AGGREGATE_WAIT_TIME     2

#define AD_STATE_LACP_ACTIVITY   0x1
#define AD_STATE_LACP_TIMEOUT    0x2
#define AD_STATE_AGGREGATION     0x4
#define AD_STATE_SYNCHRONIZATION 0x8
#define AD_STATE_COLLECTING      0x10
#define AD_STATE_DISTRIBUTING    0x20
#define AD_STATE_DEFAULTED       0x40
#define AD_STATE_EXPIRED         0x80

#define AD_PORT_BEGIN           0x1
#define AD_PORT_LACP_ENABLED    0x2
#define AD_PORT_ACTOR_CHURN     0x4
#define AD_PORT_PARTNER_CHURN   0x8
#define AD_PORT_READY           0x10
#define AD_PORT_READY_N         0x20
#define AD_PORT_MATCHED         0x40
#define AD_PORT_STANDBY         0x80
#define AD_PORT_SELECTED        0x100
#define AD_PORT_MOVED           0x200

#define  AD_DUPLEX_KEY_BITS    0x1
#define  AD_SPEED_KEY_BITS     0x3E
#define  AD_USER_KEY_BITS      0xFFC0

#define     AD_LINK_SPEED_BITMASK_1MBPS       0x1
#define     AD_LINK_SPEED_BITMASK_10MBPS      0x2
#define     AD_LINK_SPEED_BITMASK_100MBPS     0x4
#define     AD_LINK_SPEED_BITMASK_1000MBPS    0x8
#define     AD_LINK_SPEED_BITMASK_10000MBPS   0x10

#define MAC_ADDRESS_COMPARE(A, B) memcmp(A, B, ETH_ALEN)

static struct mac_addr null_mac_addr = { { 0, 0, 0, 0, 0, 0 } };
static u16 ad_ticks_per_sec;
static const int ad_delta_in_ticks = (AD_TIMER_INTERVAL * HZ) / 1000;

static const u8 lacpdu_mcast_addr[ETH_ALEN] = MULTICAST_LACPDU_ADDR;

static int ad_lacpdu_send(struct port *port);
static int ad_marker_send(struct port *port, struct bond_marker *marker);
static void ad_mux_machine(struct port *port);
static void ad_rx_machine(struct lacpdu *lacpdu, struct port *port);
static void ad_tx_machine(struct port *port);
static void ad_periodic_machine(struct port *port);
static void ad_port_selection_logic(struct port *port);
static void ad_agg_selection_logic(struct aggregator *aggregator);
static void ad_clear_agg(struct aggregator *aggregator);
static void ad_initialize_agg(struct aggregator *aggregator);
static void ad_initialize_port(struct port *port, int lacp_fast);
static void ad_enable_collecting_distributing(struct port *port);
static void ad_disable_collecting_distributing(struct port *port);
static void ad_marker_info_received(struct bond_marker *marker_info, struct port *port);
static void ad_marker_response_received(struct bond_marker *marker, struct port *port);



static inline struct bonding *__get_bond_by_port(struct port *port)
{
	if (port->slave == NULL)
		return NULL;

	return bond_get_bond_by_slave(port->slave);
}

static inline struct port *__get_first_port(struct bonding *bond)
{
	if (bond->slave_cnt == 0)
		return NULL;

	return &(SLAVE_AD_INFO(bond->first_slave).port);
}

static inline struct port *__get_next_port(struct port *port)
{
	struct bonding *bond = __get_bond_by_port(port);
	struct slave *slave = port->slave;

	
	if ((bond == NULL) || (slave->next == bond->first_slave))
		return NULL;

	return &(SLAVE_AD_INFO(slave->next).port);
}

static inline struct aggregator *__get_first_agg(struct port *port)
{
	struct bonding *bond = __get_bond_by_port(port);

	
	if ((bond == NULL) || (bond->slave_cnt == 0))
		return NULL;

	return &(SLAVE_AD_INFO(bond->first_slave).aggregator);
}

static inline struct aggregator *__get_next_agg(struct aggregator *aggregator)
{
	struct slave *slave = aggregator->slave;
	struct bonding *bond = bond_get_bond_by_slave(slave);

	
	if ((bond == NULL) || (slave->next == bond->first_slave))
		return NULL;

	return &(SLAVE_AD_INFO(slave->next).aggregator);
}

static inline int __agg_has_partner(struct aggregator *agg)
{
	return !is_zero_ether_addr(agg->partner_system.mac_addr_value);
}

static inline void __disable_port(struct port *port)
{
	bond_set_slave_inactive_flags(port->slave);
}

static inline void __enable_port(struct port *port)
{
	struct slave *slave = port->slave;

	if ((slave->link == BOND_LINK_UP) && IS_UP(slave->dev))
		bond_set_slave_active_flags(slave);
}

static inline int __port_is_enabled(struct port *port)
{
	return bond_is_active_slave(port->slave);
}

static inline u32 __get_agg_selection_mode(struct port *port)
{
	struct bonding *bond = __get_bond_by_port(port);

	if (bond == NULL)
		return BOND_AD_STABLE;

	return bond->params.ad_select;
}

static inline int __check_agg_selection_timer(struct port *port)
{
	struct bonding *bond = __get_bond_by_port(port);

	if (bond == NULL)
		return 0;

	return BOND_AD_INFO(bond).agg_select_timer ? 1 : 0;
}

static inline void __get_state_machine_lock(struct port *port)
{
	spin_lock_bh(&(SLAVE_AD_INFO(port->slave).state_machine_lock));
}

static inline void __release_state_machine_lock(struct port *port)
{
	spin_unlock_bh(&(SLAVE_AD_INFO(port->slave).state_machine_lock));
}

static u16 __get_link_speed(struct port *port)
{
	struct slave *slave = port->slave;
	u16 speed;

	if (slave->link != BOND_LINK_UP)
		speed = 0;
	else {
		switch (slave->speed) {
		case SPEED_10:
			speed = AD_LINK_SPEED_BITMASK_10MBPS;
			break;

		case SPEED_100:
			speed = AD_LINK_SPEED_BITMASK_100MBPS;
			break;

		case SPEED_1000:
			speed = AD_LINK_SPEED_BITMASK_1000MBPS;
			break;

		case SPEED_10000:
			speed = AD_LINK_SPEED_BITMASK_10000MBPS;
			break;

		default:
			speed = 0; 
			break;
		}
	}

	pr_debug("Port %d Received link speed %d update from adapter\n",
		 port->actor_port_number, speed);
	return speed;
}

static u8 __get_duplex(struct port *port)
{
	struct slave *slave = port->slave;

	u8 retval;

	
	
	if (slave->link != BOND_LINK_UP)
		retval = 0x0;
	else {
		switch (slave->duplex) {
		case DUPLEX_FULL:
			retval = 0x1;
			pr_debug("Port %d Received status full duplex update from adapter\n",
				 port->actor_port_number);
			break;
		case DUPLEX_HALF:
		default:
			retval = 0x0;
			pr_debug("Port %d Received status NOT full duplex update from adapter\n",
				 port->actor_port_number);
			break;
		}
	}
	return retval;
}

static inline void __initialize_port_locks(struct port *port)
{
	
	spin_lock_init(&(SLAVE_AD_INFO(port->slave).state_machine_lock));
}


static u16 __ad_timer_to_ticks(u16 timer_type, u16 par)
{
	u16 retval = 0; 

	switch (timer_type) {
	case AD_CURRENT_WHILE_TIMER:   
		if (par)
			retval = (AD_SHORT_TIMEOUT_TIME*ad_ticks_per_sec); 
		else
			retval = (AD_LONG_TIMEOUT_TIME*ad_ticks_per_sec); 
		break;
	case AD_ACTOR_CHURN_TIMER:	    
		retval = (AD_CHURN_DETECTION_TIME*ad_ticks_per_sec);
		break;
	case AD_PERIODIC_TIMER:	    
		retval = (par*ad_ticks_per_sec); 
		break;
	case AD_PARTNER_CHURN_TIMER:   
		retval = (AD_CHURN_DETECTION_TIME*ad_ticks_per_sec);
		break;
	case AD_WAIT_WHILE_TIMER:	    
		retval = (AD_AGGREGATE_WAIT_TIME*ad_ticks_per_sec);
		break;
	}
	return retval;
}



static void __choose_matched(struct lacpdu *lacpdu, struct port *port)
{
	
	if (((ntohs(lacpdu->partner_port) == port->actor_port_number) &&
	     (ntohs(lacpdu->partner_port_priority) == port->actor_port_priority) &&
	     !MAC_ADDRESS_COMPARE(&(lacpdu->partner_system), &(port->actor_system)) &&
	     (ntohs(lacpdu->partner_system_priority) == port->actor_system_priority) &&
	     (ntohs(lacpdu->partner_key) == port->actor_oper_port_key) &&
	     ((lacpdu->partner_state & AD_STATE_AGGREGATION) == (port->actor_oper_port_state & AD_STATE_AGGREGATION))) ||
	    
	    ((lacpdu->actor_state & AD_STATE_AGGREGATION) == 0)
		) {
		
		port->sm_vars |= AD_PORT_MATCHED;
	} else {
		port->sm_vars &= ~AD_PORT_MATCHED;
	}
}

static void __record_pdu(struct lacpdu *lacpdu, struct port *port)
{
	if (lacpdu && port) {
		struct port_params *partner = &port->partner_oper;

		__choose_matched(lacpdu, port);
		
		partner->port_number = ntohs(lacpdu->actor_port);
		partner->port_priority = ntohs(lacpdu->actor_port_priority);
		partner->system = lacpdu->actor_system;
		partner->system_priority = ntohs(lacpdu->actor_system_priority);
		partner->key = ntohs(lacpdu->actor_key);
		partner->port_state = lacpdu->actor_state;

		
		port->actor_oper_port_state &= ~AD_STATE_DEFAULTED;

		
		if ((port->sm_vars & AD_PORT_MATCHED)
		    && (lacpdu->actor_state & AD_STATE_SYNCHRONIZATION))
			partner->port_state |= AD_STATE_SYNCHRONIZATION;
		else
			partner->port_state &= ~AD_STATE_SYNCHRONIZATION;
	}
}

static void __record_default(struct port *port)
{
	if (port) {
		
		memcpy(&port->partner_oper, &port->partner_admin,
		       sizeof(struct port_params));

		
		port->actor_oper_port_state |= AD_STATE_DEFAULTED;
	}
}

static void __update_selected(struct lacpdu *lacpdu, struct port *port)
{
	if (lacpdu && port) {
		const struct port_params *partner = &port->partner_oper;

		
		if (ntohs(lacpdu->actor_port) != partner->port_number ||
		    ntohs(lacpdu->actor_port_priority) != partner->port_priority ||
		    MAC_ADDRESS_COMPARE(&lacpdu->actor_system, &partner->system) ||
		    ntohs(lacpdu->actor_system_priority) != partner->system_priority ||
		    ntohs(lacpdu->actor_key) != partner->key ||
		    (lacpdu->actor_state & AD_STATE_AGGREGATION) != (partner->port_state & AD_STATE_AGGREGATION)) {
			
			port->sm_vars &= ~AD_PORT_SELECTED;
		}
	}
}

static void __update_default_selected(struct port *port)
{
	if (port) {
		const struct port_params *admin = &port->partner_admin;
		const struct port_params *oper = &port->partner_oper;

		
		if (admin->port_number != oper->port_number ||
		    admin->port_priority != oper->port_priority ||
		    MAC_ADDRESS_COMPARE(&admin->system, &oper->system) ||
		    admin->system_priority != oper->system_priority ||
		    admin->key != oper->key ||
		    (admin->port_state & AD_STATE_AGGREGATION)
			!= (oper->port_state & AD_STATE_AGGREGATION)) {
			
			port->sm_vars &= ~AD_PORT_SELECTED;
		}
	}
}

static void __update_ntt(struct lacpdu *lacpdu, struct port *port)
{
	
	if (lacpdu && port) {
		
		if ((ntohs(lacpdu->partner_port) != port->actor_port_number) ||
		    (ntohs(lacpdu->partner_port_priority) != port->actor_port_priority) ||
		    MAC_ADDRESS_COMPARE(&(lacpdu->partner_system), &(port->actor_system)) ||
		    (ntohs(lacpdu->partner_system_priority) != port->actor_system_priority) ||
		    (ntohs(lacpdu->partner_key) != port->actor_oper_port_key) ||
		    ((lacpdu->partner_state & AD_STATE_LACP_ACTIVITY) != (port->actor_oper_port_state & AD_STATE_LACP_ACTIVITY)) ||
		    ((lacpdu->partner_state & AD_STATE_LACP_TIMEOUT) != (port->actor_oper_port_state & AD_STATE_LACP_TIMEOUT)) ||
		    ((lacpdu->partner_state & AD_STATE_SYNCHRONIZATION) != (port->actor_oper_port_state & AD_STATE_SYNCHRONIZATION)) ||
		    ((lacpdu->partner_state & AD_STATE_AGGREGATION) != (port->actor_oper_port_state & AD_STATE_AGGREGATION))
		   ) {

			port->ntt = true;
		}
	}
}

static void __attach_bond_to_agg(struct port *port)
{
	port = NULL; 
	
	
}

static void __detach_bond_from_agg(struct port *port)
{
	port = NULL; 
	
	
}

static int __agg_ports_are_ready(struct aggregator *aggregator)
{
	struct port *port;
	int retval = 1;

	if (aggregator) {
		
		for (port = aggregator->lag_ports;
		     port;
		     port = port->next_port_in_aggregator) {
			if (!(port->sm_vars & AD_PORT_READY_N)) {
				retval = 0;
				break;
			}
		}
	}

	return retval;
}

static void __set_agg_ports_ready(struct aggregator *aggregator, int val)
{
	struct port *port;

	for (port = aggregator->lag_ports; port;
	     port = port->next_port_in_aggregator) {
		if (val)
			port->sm_vars |= AD_PORT_READY;
		else
			port->sm_vars &= ~AD_PORT_READY;
	}
}

static u32 __get_agg_bandwidth(struct aggregator *aggregator)
{
	u32 bandwidth = 0;

	if (aggregator->num_of_ports) {
		switch (__get_link_speed(aggregator->lag_ports)) {
		case AD_LINK_SPEED_BITMASK_1MBPS:
			bandwidth = aggregator->num_of_ports;
			break;
		case AD_LINK_SPEED_BITMASK_10MBPS:
			bandwidth = aggregator->num_of_ports * 10;
			break;
		case AD_LINK_SPEED_BITMASK_100MBPS:
			bandwidth = aggregator->num_of_ports * 100;
			break;
		case AD_LINK_SPEED_BITMASK_1000MBPS:
			bandwidth = aggregator->num_of_ports * 1000;
			break;
		case AD_LINK_SPEED_BITMASK_10000MBPS:
			bandwidth = aggregator->num_of_ports * 10000;
			break;
		default:
			bandwidth = 0; 
		}
	}
	return bandwidth;
}

static struct aggregator *__get_active_agg(struct aggregator *aggregator)
{
	struct aggregator *retval = NULL;

	for (; aggregator; aggregator = __get_next_agg(aggregator)) {
		if (aggregator->is_active) {
			retval = aggregator;
			break;
		}
	}

	return retval;
}

static inline void __update_lacpdu_from_port(struct port *port)
{
	struct lacpdu *lacpdu = &port->lacpdu;
	const struct port_params *partner = &port->partner_oper;

	

	lacpdu->actor_system_priority = htons(port->actor_system_priority);
	lacpdu->actor_system = port->actor_system;
	lacpdu->actor_key = htons(port->actor_oper_port_key);
	lacpdu->actor_port_priority = htons(port->actor_port_priority);
	lacpdu->actor_port = htons(port->actor_port_number);
	lacpdu->actor_state = port->actor_oper_port_state;


	lacpdu->partner_system_priority = htons(partner->system_priority);
	lacpdu->partner_system = partner->system;
	lacpdu->partner_key = htons(partner->key);
	lacpdu->partner_port_priority = htons(partner->port_priority);
	lacpdu->partner_port = htons(partner->port_number);
	lacpdu->partner_state = partner->port_state;

}


static int ad_lacpdu_send(struct port *port)
{
	struct slave *slave = port->slave;
	struct sk_buff *skb;
	struct lacpdu_header *lacpdu_header;
	int length = sizeof(struct lacpdu_header);

	skb = dev_alloc_skb(length);
	if (!skb)
		return -ENOMEM;

	skb->dev = slave->dev;
	skb_reset_mac_header(skb);
	skb->network_header = skb->mac_header + ETH_HLEN;
	skb->protocol = PKT_TYPE_LACPDU;
	skb->priority = TC_PRIO_CONTROL;

	lacpdu_header = (struct lacpdu_header *)skb_put(skb, length);

	memcpy(lacpdu_header->hdr.h_dest, lacpdu_mcast_addr, ETH_ALEN);
	memcpy(lacpdu_header->hdr.h_source, slave->perm_hwaddr, ETH_ALEN);
	lacpdu_header->hdr.h_proto = PKT_TYPE_LACPDU;

	lacpdu_header->lacpdu = port->lacpdu; 

	dev_queue_xmit(skb);

	return 0;
}

static int ad_marker_send(struct port *port, struct bond_marker *marker)
{
	struct slave *slave = port->slave;
	struct sk_buff *skb;
	struct bond_marker_header *marker_header;
	int length = sizeof(struct bond_marker_header);

	skb = dev_alloc_skb(length + 16);
	if (!skb)
		return -ENOMEM;

	skb_reserve(skb, 16);

	skb->dev = slave->dev;
	skb_reset_mac_header(skb);
	skb->network_header = skb->mac_header + ETH_HLEN;
	skb->protocol = PKT_TYPE_LACPDU;

	marker_header = (struct bond_marker_header *)skb_put(skb, length);

	memcpy(marker_header->hdr.h_dest, lacpdu_mcast_addr, ETH_ALEN);
	memcpy(marker_header->hdr.h_source, slave->perm_hwaddr, ETH_ALEN);
	marker_header->hdr.h_proto = PKT_TYPE_LACPDU;

	marker_header->marker = *marker; 

	dev_queue_xmit(skb);

	return 0;
}

static void ad_mux_machine(struct port *port)
{
	mux_states_t last_state;

	
	last_state = port->sm_mux_state;

	if (port->sm_vars & AD_PORT_BEGIN) {
		port->sm_mux_state = AD_MUX_DETACHED;		 
	} else {
		switch (port->sm_mux_state) {
		case AD_MUX_DETACHED:
			if ((port->sm_vars & AD_PORT_SELECTED)
			    || (port->sm_vars & AD_PORT_STANDBY))
				
				port->sm_mux_state = AD_MUX_WAITING; 
			break;
		case AD_MUX_WAITING:
			
			if (!(port->sm_vars & AD_PORT_SELECTED)) { 
				port->sm_vars &= ~AD_PORT_READY_N;
				
				
				__set_agg_ports_ready(port->aggregator, __agg_ports_are_ready(port->aggregator));
				port->sm_mux_state = AD_MUX_DETACHED;	 
				break;
			}

			
			if (port->sm_mux_timer_counter
			    && !(--port->sm_mux_timer_counter))
				port->sm_vars |= AD_PORT_READY_N;

			
			
			__set_agg_ports_ready(port->aggregator, __agg_ports_are_ready(port->aggregator));

			
			if ((port->sm_vars & AD_PORT_READY)
			    && !port->sm_mux_timer_counter)
				port->sm_mux_state = AD_MUX_ATTACHED;	 
			break;
		case AD_MUX_ATTACHED:
			
			if ((port->sm_vars & AD_PORT_SELECTED) && (port->partner_oper.port_state & AD_STATE_SYNCHRONIZATION) && !__check_agg_selection_timer(port)) {
				port->sm_mux_state = AD_MUX_COLLECTING_DISTRIBUTING;
			} else if (!(port->sm_vars & AD_PORT_SELECTED) || (port->sm_vars & AD_PORT_STANDBY)) {	  
				port->sm_vars &= ~AD_PORT_READY_N;
				
				
				__set_agg_ports_ready(port->aggregator, __agg_ports_are_ready(port->aggregator));
				port->sm_mux_state = AD_MUX_DETACHED;
			}
			break;
		case AD_MUX_COLLECTING_DISTRIBUTING:
			if (!(port->sm_vars & AD_PORT_SELECTED) || (port->sm_vars & AD_PORT_STANDBY) ||
			    !(port->partner_oper.port_state & AD_STATE_SYNCHRONIZATION)
			   ) {
				port->sm_mux_state = AD_MUX_ATTACHED;

			} else {
				
				
				
				if (port->aggregator &&
				    port->aggregator->is_active &&
				    !__port_is_enabled(port)) {

					__enable_port(port);
				}
			}
			break;
		default:    
			break;
		}
	}

	
	if (port->sm_mux_state != last_state) {
		pr_debug("Mux Machine: Port=%d, Last State=%d, Curr State=%d\n",
			 port->actor_port_number, last_state,
			 port->sm_mux_state);
		switch (port->sm_mux_state) {
		case AD_MUX_DETACHED:
			__detach_bond_from_agg(port);
			port->actor_oper_port_state &= ~AD_STATE_SYNCHRONIZATION;
			ad_disable_collecting_distributing(port);
			port->actor_oper_port_state &= ~AD_STATE_COLLECTING;
			port->actor_oper_port_state &= ~AD_STATE_DISTRIBUTING;
			port->ntt = true;
			break;
		case AD_MUX_WAITING:
			port->sm_mux_timer_counter = __ad_timer_to_ticks(AD_WAIT_WHILE_TIMER, 0);
			break;
		case AD_MUX_ATTACHED:
			__attach_bond_to_agg(port);
			port->actor_oper_port_state |= AD_STATE_SYNCHRONIZATION;
			port->actor_oper_port_state &= ~AD_STATE_COLLECTING;
			port->actor_oper_port_state &= ~AD_STATE_DISTRIBUTING;
			ad_disable_collecting_distributing(port);
			port->ntt = true;
			break;
		case AD_MUX_COLLECTING_DISTRIBUTING:
			port->actor_oper_port_state |= AD_STATE_COLLECTING;
			port->actor_oper_port_state |= AD_STATE_DISTRIBUTING;
			ad_enable_collecting_distributing(port);
			port->ntt = true;
			break;
		default:    
			break;
		}
	}
}

static void ad_rx_machine(struct lacpdu *lacpdu, struct port *port)
{
	rx_states_t last_state;

	
	last_state = port->sm_rx_state;

	
	
	if (port->sm_vars & AD_PORT_BEGIN)
		
		port->sm_rx_state = AD_RX_INITIALIZE;
	
	else if (!(port->sm_vars & AD_PORT_BEGIN)
		 && !port->is_enabled && !(port->sm_vars & AD_PORT_MOVED))
		
		port->sm_rx_state = AD_RX_PORT_DISABLED;
	
	else if (lacpdu && ((port->sm_rx_state == AD_RX_EXPIRED) || (port->sm_rx_state == AD_RX_DEFAULTED) || (port->sm_rx_state == AD_RX_CURRENT))) {
		port->sm_rx_timer_counter = 0; 
		port->sm_rx_state = AD_RX_CURRENT;
	} else {
		
		if (port->sm_rx_timer_counter && !(--port->sm_rx_timer_counter)) {
			switch (port->sm_rx_state) {
			case AD_RX_EXPIRED:
				port->sm_rx_state = AD_RX_DEFAULTED;		
				break;
			case AD_RX_CURRENT:
				port->sm_rx_state = AD_RX_EXPIRED;	    
				break;
			default:    
				break;
			}
		} else {
			
			switch (port->sm_rx_state) {
			case AD_RX_PORT_DISABLED:
				if (port->sm_vars & AD_PORT_MOVED)
					port->sm_rx_state = AD_RX_INITIALIZE;	    
				else if (port->is_enabled
					 && (port->sm_vars
					     & AD_PORT_LACP_ENABLED))
					port->sm_rx_state = AD_RX_EXPIRED;	
				else if (port->is_enabled
					 && ((port->sm_vars
					      & AD_PORT_LACP_ENABLED) == 0))
					port->sm_rx_state = AD_RX_LACP_DISABLED;    
				break;
			default:    
				break;

			}
		}
	}

	
	if ((port->sm_rx_state != last_state) || (lacpdu)) {
		pr_debug("Rx Machine: Port=%d, Last State=%d, Curr State=%d\n",
			 port->actor_port_number, last_state,
			 port->sm_rx_state);
		switch (port->sm_rx_state) {
		case AD_RX_INITIALIZE:
			if (!(port->actor_oper_port_key & AD_DUPLEX_KEY_BITS))
				port->sm_vars &= ~AD_PORT_LACP_ENABLED;
			else
				port->sm_vars |= AD_PORT_LACP_ENABLED;
			port->sm_vars &= ~AD_PORT_SELECTED;
			__record_default(port);
			port->actor_oper_port_state &= ~AD_STATE_EXPIRED;
			port->sm_vars &= ~AD_PORT_MOVED;
			port->sm_rx_state = AD_RX_PORT_DISABLED;	

			

		case AD_RX_PORT_DISABLED:
			port->sm_vars &= ~AD_PORT_MATCHED;
			break;
		case AD_RX_LACP_DISABLED:
			port->sm_vars &= ~AD_PORT_SELECTED;
			__record_default(port);
			port->partner_oper.port_state &= ~AD_STATE_AGGREGATION;
			port->sm_vars |= AD_PORT_MATCHED;
			port->actor_oper_port_state &= ~AD_STATE_EXPIRED;
			break;
		case AD_RX_EXPIRED:
			
			
			
			port->partner_oper.port_state &= ~AD_STATE_SYNCHRONIZATION;
			port->sm_vars &= ~AD_PORT_MATCHED;
			port->partner_oper.port_state |=
				AD_STATE_LACP_ACTIVITY;
			port->sm_rx_timer_counter = __ad_timer_to_ticks(AD_CURRENT_WHILE_TIMER, (u16)(AD_SHORT_TIMEOUT));
			port->actor_oper_port_state |= AD_STATE_EXPIRED;
			break;
		case AD_RX_DEFAULTED:
			__update_default_selected(port);
			__record_default(port);
			port->sm_vars |= AD_PORT_MATCHED;
			port->actor_oper_port_state &= ~AD_STATE_EXPIRED;
			break;
		case AD_RX_CURRENT:
			
			if (!MAC_ADDRESS_COMPARE(&(lacpdu->actor_system), &(port->actor_system))) {
				
				pr_err("%s: An illegal loopback occurred on adapter (%s).\n"
				       "Check the configuration to verify that all adapters are connected to 802.3ad compliant switch ports\n",
				       port->slave->dev->master->name, port->slave->dev->name);
				return;
			}
			__update_selected(lacpdu, port);
			__update_ntt(lacpdu, port);
			__record_pdu(lacpdu, port);
			port->sm_rx_timer_counter = __ad_timer_to_ticks(AD_CURRENT_WHILE_TIMER, (u16)(port->actor_oper_port_state & AD_STATE_LACP_TIMEOUT));
			port->actor_oper_port_state &= ~AD_STATE_EXPIRED;
			break;
		default:    
			break;
		}
	}
}

static void ad_tx_machine(struct port *port)
{
	
	if (port->sm_tx_timer_counter && !(--port->sm_tx_timer_counter)) {
		
		if (port->ntt && (port->sm_vars & AD_PORT_LACP_ENABLED)) {
			__update_lacpdu_from_port(port);

			if (ad_lacpdu_send(port) >= 0) {
				pr_debug("Sent LACPDU on port %d\n",
					 port->actor_port_number);

				port->ntt = false;
			}
		}
		
		port->sm_tx_timer_counter =
			ad_ticks_per_sec/AD_MAX_TX_IN_SECOND;
	}
}

static void ad_periodic_machine(struct port *port)
{
	periodic_states_t last_state;

	
	last_state = port->sm_periodic_state;

	
	if (((port->sm_vars & AD_PORT_BEGIN) || !(port->sm_vars & AD_PORT_LACP_ENABLED) || !port->is_enabled) ||
	    (!(port->actor_oper_port_state & AD_STATE_LACP_ACTIVITY) && !(port->partner_oper.port_state & AD_STATE_LACP_ACTIVITY))
	   ) {
		port->sm_periodic_state = AD_NO_PERIODIC;	     
	}
	
	else if (port->sm_periodic_timer_counter) {
		
		if (!(--port->sm_periodic_timer_counter)) {
			
			port->sm_periodic_state = AD_PERIODIC_TX;    
		} else {
			
			switch (port->sm_periodic_state) {
			case AD_FAST_PERIODIC:
				if (!(port->partner_oper.port_state
				      & AD_STATE_LACP_TIMEOUT))
					port->sm_periodic_state = AD_SLOW_PERIODIC;  
				break;
			case AD_SLOW_PERIODIC:
				if ((port->partner_oper.port_state & AD_STATE_LACP_TIMEOUT)) {
					
					port->sm_periodic_timer_counter = 0;
					port->sm_periodic_state = AD_PERIODIC_TX;	 
				}
				break;
			default:    
				break;
			}
		}
	} else {
		switch (port->sm_periodic_state) {
		case AD_NO_PERIODIC:
			port->sm_periodic_state = AD_FAST_PERIODIC;	 
			break;
		case AD_PERIODIC_TX:
			if (!(port->partner_oper.port_state
			      & AD_STATE_LACP_TIMEOUT))
				port->sm_periodic_state = AD_SLOW_PERIODIC;  
			else
				port->sm_periodic_state = AD_FAST_PERIODIC;  
			break;
		default:    
			break;
		}
	}

	
	if (port->sm_periodic_state != last_state) {
		pr_debug("Periodic Machine: Port=%d, Last State=%d, Curr State=%d\n",
			 port->actor_port_number, last_state,
			 port->sm_periodic_state);
		switch (port->sm_periodic_state) {
		case AD_NO_PERIODIC:
			port->sm_periodic_timer_counter = 0;	   
			break;
		case AD_FAST_PERIODIC:
			port->sm_periodic_timer_counter = __ad_timer_to_ticks(AD_PERIODIC_TIMER, (u16)(AD_FAST_PERIODIC_TIME))-1; 
			break;
		case AD_SLOW_PERIODIC:
			port->sm_periodic_timer_counter = __ad_timer_to_ticks(AD_PERIODIC_TIMER, (u16)(AD_SLOW_PERIODIC_TIME))-1; 
			break;
		case AD_PERIODIC_TX:
			port->ntt = true;
			break;
		default:    
			break;
		}
	}
}

static void ad_port_selection_logic(struct port *port)
{
	struct aggregator *aggregator, *free_aggregator = NULL, *temp_aggregator;
	struct port *last_port = NULL, *curr_port;
	int found = 0;

	
	if (port->sm_vars & AD_PORT_SELECTED)
		return;

	
	if (port->aggregator) {
		
		temp_aggregator = port->aggregator;
		for (curr_port = temp_aggregator->lag_ports; curr_port;
		     last_port = curr_port,
			     curr_port = curr_port->next_port_in_aggregator) {
			if (curr_port == port) {
				temp_aggregator->num_of_ports--;
				if (!last_port) {
					temp_aggregator->lag_ports =
						port->next_port_in_aggregator;
				} else {
					last_port->next_port_in_aggregator =
						port->next_port_in_aggregator;
				}

				
				port->aggregator = NULL;
				port->next_port_in_aggregator = NULL;
				port->actor_port_aggregator_identifier = 0;

				pr_debug("Port %d left LAG %d\n",
					 port->actor_port_number,
					 temp_aggregator->aggregator_identifier);
				
				if (!temp_aggregator->lag_ports)
					ad_clear_agg(temp_aggregator);
				break;
			}
		}
		if (!curr_port) { 
			pr_warning("%s: Warning: Port %d (on %s) was related to aggregator %d but was not on its port list\n",
				   port->slave->dev->master->name,
				   port->actor_port_number,
				   port->slave->dev->name,
				   port->aggregator->aggregator_identifier);
		}
	}
	
	for (aggregator = __get_first_agg(port); aggregator;
	     aggregator = __get_next_agg(aggregator)) {

		
		if (!aggregator->lag_ports) {
			if (!free_aggregator)
				free_aggregator = aggregator;
			continue;
		}
		
		if (((aggregator->actor_oper_aggregator_key == port->actor_oper_port_key) && 
		     !MAC_ADDRESS_COMPARE(&(aggregator->partner_system), &(port->partner_oper.system)) &&
		     (aggregator->partner_system_priority == port->partner_oper.system_priority) &&
		     (aggregator->partner_oper_aggregator_key == port->partner_oper.key)
		    ) &&
		    ((MAC_ADDRESS_COMPARE(&(port->partner_oper.system), &(null_mac_addr)) && 
		      !aggregator->is_individual)  
		    )
		   ) {
			
			port->aggregator = aggregator;
			port->actor_port_aggregator_identifier =
				port->aggregator->aggregator_identifier;
			port->next_port_in_aggregator = aggregator->lag_ports;
			port->aggregator->num_of_ports++;
			aggregator->lag_ports = port;
			pr_debug("Port %d joined LAG %d(existing LAG)\n",
				 port->actor_port_number,
				 port->aggregator->aggregator_identifier);

			
			port->sm_vars |= AD_PORT_SELECTED;
			found = 1;
			break;
		}
	}

	
	if (!found) {
		if (free_aggregator) {
			
			port->aggregator = free_aggregator;
			port->actor_port_aggregator_identifier =
				port->aggregator->aggregator_identifier;

			
			
			if (port->actor_oper_port_key & AD_DUPLEX_KEY_BITS)
				
				port->aggregator->is_individual = false;
			else
				port->aggregator->is_individual = true;

			port->aggregator->actor_admin_aggregator_key = port->actor_admin_port_key;
			port->aggregator->actor_oper_aggregator_key = port->actor_oper_port_key;
			port->aggregator->partner_system =
				port->partner_oper.system;
			port->aggregator->partner_system_priority =
				port->partner_oper.system_priority;
			port->aggregator->partner_oper_aggregator_key = port->partner_oper.key;
			port->aggregator->receive_state = 1;
			port->aggregator->transmit_state = 1;
			port->aggregator->lag_ports = port;
			port->aggregator->num_of_ports++;

			
			port->sm_vars |= AD_PORT_SELECTED;

			pr_debug("Port %d joined LAG %d(new LAG)\n",
				 port->actor_port_number,
				 port->aggregator->aggregator_identifier);
		} else {
			pr_err("%s: Port %d (on %s) did not find a suitable aggregator\n",
			       port->slave->dev->master->name,
			       port->actor_port_number, port->slave->dev->name);
		}
	}
	
	
	__set_agg_ports_ready(port->aggregator, __agg_ports_are_ready(port->aggregator));

	aggregator = __get_first_agg(port);
	ad_agg_selection_logic(aggregator);
}

static struct aggregator *ad_agg_selection_test(struct aggregator *best,
						struct aggregator *curr)
{
	if (!best)
		return curr;

	if (!curr->is_individual && best->is_individual)
		return curr;

	if (curr->is_individual && !best->is_individual)
		return best;

	if (__agg_has_partner(curr) && !__agg_has_partner(best))
		return curr;

	if (!__agg_has_partner(curr) && __agg_has_partner(best))
		return best;

	switch (__get_agg_selection_mode(curr->lag_ports)) {
	case BOND_AD_COUNT:
		if (curr->num_of_ports > best->num_of_ports)
			return curr;

		if (curr->num_of_ports < best->num_of_ports)
			return best;

		
	case BOND_AD_STABLE:
	case BOND_AD_BANDWIDTH:
		if (__get_agg_bandwidth(curr) > __get_agg_bandwidth(best))
			return curr;

		break;

	default:
		pr_warning("%s: Impossible agg select mode %d\n",
			   curr->slave->dev->master->name,
			   __get_agg_selection_mode(curr->lag_ports));
		break;
	}

	return best;
}

static int agg_device_up(const struct aggregator *agg)
{
	struct port *port = agg->lag_ports;
	if (!port)
		return 0;
	return (netif_running(port->slave->dev) &&
		netif_carrier_ok(port->slave->dev));
}

static void ad_agg_selection_logic(struct aggregator *agg)
{
	struct aggregator *best, *active, *origin;
	struct port *port;

	origin = agg;
	active = __get_active_agg(agg);
	best = (active && agg_device_up(active)) ? active : NULL;

	do {
		agg->is_active = 0;

		if (agg->num_of_ports && agg_device_up(agg))
			best = ad_agg_selection_test(best, agg);

	} while ((agg = __get_next_agg(agg)));

	if (best &&
	    __get_agg_selection_mode(best->lag_ports) == BOND_AD_STABLE) {
		if (active && active->lag_ports &&
		    active->lag_ports->is_enabled &&
		    (__agg_has_partner(active) ||
		     (!__agg_has_partner(active) && !__agg_has_partner(best)))) {
			if (!(!active->actor_oper_aggregator_key &&
			      best->actor_oper_aggregator_key)) {
				best = NULL;
				active->is_active = 1;
			}
		}
	}

	if (best && (best == active)) {
		best = NULL;
		active->is_active = 1;
	}

	
	if (best) {
		pr_debug("best Agg=%d; P=%d; a k=%d; p k=%d; Ind=%d; Act=%d\n",
			 best->aggregator_identifier, best->num_of_ports,
			 best->actor_oper_aggregator_key,
			 best->partner_oper_aggregator_key,
			 best->is_individual, best->is_active);
		pr_debug("best ports %p slave %p %s\n",
			 best->lag_ports, best->slave,
			 best->slave ? best->slave->dev->name : "NULL");

		for (agg = __get_first_agg(best->lag_ports); agg;
		     agg = __get_next_agg(agg)) {

			pr_debug("Agg=%d; P=%d; a k=%d; p k=%d; Ind=%d; Act=%d\n",
				 agg->aggregator_identifier, agg->num_of_ports,
				 agg->actor_oper_aggregator_key,
				 agg->partner_oper_aggregator_key,
				 agg->is_individual, agg->is_active);
		}

		
		if (best->is_individual) {
			pr_warning("%s: Warning: No 802.3ad response from the link partner for any adapters in the bond\n",
				   best->slave ? best->slave->dev->master->name : "NULL");
		}

		best->is_active = 1;
		pr_debug("LAG %d chosen as the active LAG\n",
			 best->aggregator_identifier);
		pr_debug("Agg=%d; P=%d; a k=%d; p k=%d; Ind=%d; Act=%d\n",
			 best->aggregator_identifier, best->num_of_ports,
			 best->actor_oper_aggregator_key,
			 best->partner_oper_aggregator_key,
			 best->is_individual, best->is_active);

		
		if (active) {
			for (port = active->lag_ports; port;
			     port = port->next_port_in_aggregator) {
				__disable_port(port);
			}
		}
	}

	active = __get_active_agg(origin);

	if (active) {
		if (!__agg_has_partner(active)) {
			for (port = active->lag_ports; port;
			     port = port->next_port_in_aggregator) {
				__enable_port(port);
			}
		}
	}

	if (origin->slave) {
		struct bonding *bond;

		bond = bond_get_bond_by_slave(origin->slave);
		if (bond)
			bond_3ad_set_carrier(bond);
	}
}

static void ad_clear_agg(struct aggregator *aggregator)
{
	if (aggregator) {
		aggregator->is_individual = false;
		aggregator->actor_admin_aggregator_key = 0;
		aggregator->actor_oper_aggregator_key = 0;
		aggregator->partner_system = null_mac_addr;
		aggregator->partner_system_priority = 0;
		aggregator->partner_oper_aggregator_key = 0;
		aggregator->receive_state = 0;
		aggregator->transmit_state = 0;
		aggregator->lag_ports = NULL;
		aggregator->is_active = 0;
		aggregator->num_of_ports = 0;
		pr_debug("LAG %d was cleared\n",
			 aggregator->aggregator_identifier);
	}
}

static void ad_initialize_agg(struct aggregator *aggregator)
{
	if (aggregator) {
		ad_clear_agg(aggregator);

		aggregator->aggregator_mac_address = null_mac_addr;
		aggregator->aggregator_identifier = 0;
		aggregator->slave = NULL;
	}
}

static void ad_initialize_port(struct port *port, int lacp_fast)
{
	static const struct port_params tmpl = {
		.system_priority = 0xffff,
		.key             = 1,
		.port_number     = 1,
		.port_priority   = 0xff,
		.port_state      = 1,
	};
	static const struct lacpdu lacpdu = {
		.subtype		= 0x01,
		.version_number = 0x01,
		.tlv_type_actor_info = 0x01,
		.actor_information_length = 0x14,
		.tlv_type_partner_info = 0x02,
		.partner_information_length = 0x14,
		.tlv_type_collector_info = 0x03,
		.collector_information_length = 0x10,
		.collector_max_delay = htons(AD_COLLECTOR_MAX_DELAY),
	};

	if (port) {
		port->actor_port_number = 1;
		port->actor_port_priority = 0xff;
		port->actor_system = null_mac_addr;
		port->actor_system_priority = 0xffff;
		port->actor_port_aggregator_identifier = 0;
		port->ntt = false;
		port->actor_admin_port_key = 1;
		port->actor_oper_port_key  = 1;
		port->actor_admin_port_state = AD_STATE_AGGREGATION | AD_STATE_LACP_ACTIVITY;
		port->actor_oper_port_state  = AD_STATE_AGGREGATION | AD_STATE_LACP_ACTIVITY;

		if (lacp_fast)
			port->actor_oper_port_state |= AD_STATE_LACP_TIMEOUT;

		memcpy(&port->partner_admin, &tmpl, sizeof(tmpl));
		memcpy(&port->partner_oper, &tmpl, sizeof(tmpl));

		port->is_enabled = true;
		
		port->sm_vars = 0x3;
		port->sm_rx_state = 0;
		port->sm_rx_timer_counter = 0;
		port->sm_periodic_state = 0;
		port->sm_periodic_timer_counter = 0;
		port->sm_mux_state = 0;
		port->sm_mux_timer_counter = 0;
		port->sm_tx_state = 0;
		port->sm_tx_timer_counter = 0;
		port->slave = NULL;
		port->aggregator = NULL;
		port->next_port_in_aggregator = NULL;
		port->transaction_id = 0;

		memcpy(&port->lacpdu, &lacpdu, sizeof(lacpdu));
	}
}

static void ad_enable_collecting_distributing(struct port *port)
{
	if (port->aggregator->is_active) {
		pr_debug("Enabling port %d(LAG %d)\n",
			 port->actor_port_number,
			 port->aggregator->aggregator_identifier);
		__enable_port(port);
	}
}

static void ad_disable_collecting_distributing(struct port *port)
{
	if (port->aggregator && MAC_ADDRESS_COMPARE(&(port->aggregator->partner_system), &(null_mac_addr))) {
		pr_debug("Disabling port %d(LAG %d)\n",
			 port->actor_port_number,
			 port->aggregator->aggregator_identifier);
		__disable_port(port);
	}
}

#if 0
static void ad_marker_info_send(struct port *port)
{
	struct bond_marker marker;
	u16 index;

	
	marker.subtype = 0x02;
	marker.version_number = 0x01;
	marker.tlv_type = AD_MARKER_INFORMATION_SUBTYPE;
	marker.marker_length = 0x16;
	
	marker.requester_port = (((port->actor_port_number & 0xFF) << 8) |((u16)(port->actor_port_number & 0xFF00) >> 8));
	marker.requester_system = port->actor_system;
	
	marker.requester_transaction_id =
		(((++port->transaction_id & 0xFF) << 24)
		 | ((port->transaction_id & 0xFF00) << 8)
		 | ((port->transaction_id & 0xFF0000) >> 8)
		 | ((port->transaction_id & 0xFF000000) >> 24));
	marker.pad = 0;
	marker.tlv_type_terminator = 0x00;
	marker.terminator_length = 0x00;
	for (index = 0; index < 90; index++)
		marker.reserved_90[index] = 0;

	
	if (ad_marker_send(port, &marker) >= 0) {
		pr_debug("Sent Marker Information on port %d\n",
			 port->actor_port_number);
	}
}
#endif

static void ad_marker_info_received(struct bond_marker *marker_info,
	struct port *port)
{
	struct bond_marker marker;

	
	
	memcpy(&marker, marker_info, sizeof(struct bond_marker));
	
	marker.tlv_type = AD_MARKER_RESPONSE_SUBTYPE;
	

	if (ad_marker_send(port, &marker) >= 0) {
		pr_debug("Sent Marker Response on port %d\n",
			 port->actor_port_number);
	}
}

static void ad_marker_response_received(struct bond_marker *marker,
	struct port *port)
{
	marker = NULL; 
	port = NULL;  
	
}


#define AD_AGGREGATOR_SELECTION_TIMER  8

void bond_3ad_initiate_agg_selection(struct bonding *bond, int timeout)
{
	BOND_AD_INFO(bond).agg_select_timer = timeout;
}

static u16 aggregator_identifier;

void bond_3ad_initialize(struct bonding *bond, u16 tick_resolution)
{
	
	if (MAC_ADDRESS_COMPARE(&(BOND_AD_INFO(bond).system.sys_mac_addr),
				bond->dev->dev_addr)) {

		aggregator_identifier = 0;

		BOND_AD_INFO(bond).system.sys_priority = 0xFFFF;
		BOND_AD_INFO(bond).system.sys_mac_addr = *((struct mac_addr *)bond->dev->dev_addr);

		
		ad_ticks_per_sec = tick_resolution;

		bond_3ad_initiate_agg_selection(bond,
						AD_AGGREGATOR_SELECTION_TIMER *
						ad_ticks_per_sec);
	}
}

int bond_3ad_bind_slave(struct slave *slave)
{
	struct bonding *bond = bond_get_bond_by_slave(slave);
	struct port *port;
	struct aggregator *aggregator;

	if (bond == NULL) {
		pr_err("%s: The slave %s is not attached to its bond\n",
		       slave->dev->master->name, slave->dev->name);
		return -1;
	}

	
	if (SLAVE_AD_INFO(slave).port.slave != slave) {

		
		port = &(SLAVE_AD_INFO(slave).port);

		ad_initialize_port(port, bond->params.lacp_fast);

		port->slave = slave;
		port->actor_port_number = SLAVE_AD_INFO(slave).id;
		
		
		
		
		
		port->actor_admin_port_key = 0;	
		port->actor_admin_port_key |= __get_duplex(port);
		port->actor_admin_port_key |= (__get_link_speed(port) << 1);
		port->actor_oper_port_key = port->actor_admin_port_key;
		
		if (!(port->actor_oper_port_key & AD_DUPLEX_KEY_BITS))
			port->sm_vars &= ~AD_PORT_LACP_ENABLED;
		
		port->actor_system = BOND_AD_INFO(bond).system.sys_mac_addr;
		
		port->sm_tx_timer_counter = ad_ticks_per_sec/AD_MAX_TX_IN_SECOND;
		port->aggregator = NULL;
		port->next_port_in_aggregator = NULL;

		__disable_port(port);
		__initialize_port_locks(port);


		
		aggregator = &(SLAVE_AD_INFO(slave).aggregator);

		ad_initialize_agg(aggregator);

		aggregator->aggregator_mac_address = *((struct mac_addr *)bond->dev->dev_addr);
		aggregator->aggregator_identifier = (++aggregator_identifier);
		aggregator->slave = slave;
		aggregator->is_active = 0;
		aggregator->num_of_ports = 0;
	}

	return 0;
}

void bond_3ad_unbind_slave(struct slave *slave)
{
	struct port *port, *prev_port, *temp_port;
	struct aggregator *aggregator, *new_aggregator, *temp_aggregator;
	int select_new_active_agg = 0;

	
	aggregator = &(SLAVE_AD_INFO(slave).aggregator);

	
	port = &(SLAVE_AD_INFO(slave).port);

	
	if (!port->slave) {
		pr_warning("Warning: %s: Trying to unbind an uninitialized port on %s\n",
			   slave->dev->master->name, slave->dev->name);
		return;
	}

	pr_debug("Unbinding Link Aggregation Group %d\n",
		 aggregator->aggregator_identifier);

	
	port->actor_oper_port_state &= ~AD_STATE_AGGREGATION;
	__update_lacpdu_from_port(port);
	ad_lacpdu_send(port);

	
	if (aggregator->lag_ports) {
		
		
		
		if ((aggregator->lag_ports != port) || (aggregator->lag_ports->next_port_in_aggregator)) {
			
			new_aggregator = __get_first_agg(port);
			for (; new_aggregator; new_aggregator = __get_next_agg(new_aggregator)) {
				
				if (!new_aggregator->lag_ports
				    || ((new_aggregator->lag_ports == port)
					&& !new_aggregator->lag_ports->next_port_in_aggregator))
					break;
			}
			
			
			if ((new_aggregator) && ((!new_aggregator->lag_ports) || ((new_aggregator->lag_ports == port) && !new_aggregator->lag_ports->next_port_in_aggregator))) {
				pr_debug("Some port(s) related to LAG %d - replaceing with LAG %d\n",
					 aggregator->aggregator_identifier,
					 new_aggregator->aggregator_identifier);

				if ((new_aggregator->lag_ports == port) && new_aggregator->is_active) {
					pr_info("%s: Removing an active aggregator\n",
						aggregator->slave->dev->master->name);
					
					 select_new_active_agg = 1;
				}

				new_aggregator->is_individual = aggregator->is_individual;
				new_aggregator->actor_admin_aggregator_key = aggregator->actor_admin_aggregator_key;
				new_aggregator->actor_oper_aggregator_key = aggregator->actor_oper_aggregator_key;
				new_aggregator->partner_system = aggregator->partner_system;
				new_aggregator->partner_system_priority = aggregator->partner_system_priority;
				new_aggregator->partner_oper_aggregator_key = aggregator->partner_oper_aggregator_key;
				new_aggregator->receive_state = aggregator->receive_state;
				new_aggregator->transmit_state = aggregator->transmit_state;
				new_aggregator->lag_ports = aggregator->lag_ports;
				new_aggregator->is_active = aggregator->is_active;
				new_aggregator->num_of_ports = aggregator->num_of_ports;

				// update the information that is written on the ports about the aggregator
				for (temp_port = aggregator->lag_ports; temp_port;
				     temp_port = temp_port->next_port_in_aggregator) {
					temp_port->aggregator = new_aggregator;
					temp_port->actor_port_aggregator_identifier = new_aggregator->aggregator_identifier;
				}

				
				ad_clear_agg(aggregator);

				if (select_new_active_agg)
					ad_agg_selection_logic(__get_first_agg(port));
			} else {
				pr_warning("%s: Warning: unbinding aggregator, and could not find a new aggregator for its ports\n",
					   slave->dev->master->name);
			}
		} else { 
			select_new_active_agg = aggregator->is_active;
			
			ad_clear_agg(aggregator);
			if (select_new_active_agg) {
				pr_info("%s: Removing an active aggregator\n",
					slave->dev->master->name);
				
				ad_agg_selection_logic(__get_first_agg(port));
			}
		}
	}

	pr_debug("Unbinding port %d\n", port->actor_port_number);
	
	temp_aggregator = __get_first_agg(port);
	for (; temp_aggregator; temp_aggregator = __get_next_agg(temp_aggregator)) {
		prev_port = NULL;
		
		for (temp_port = temp_aggregator->lag_ports; temp_port;
		     prev_port = temp_port,
			     temp_port = temp_port->next_port_in_aggregator) {
			if (temp_port == port) { 
				if (prev_port)
					prev_port->next_port_in_aggregator = temp_port->next_port_in_aggregator;
				else
					temp_aggregator->lag_ports = temp_port->next_port_in_aggregator;
				temp_aggregator->num_of_ports--;
				if (temp_aggregator->num_of_ports == 0) {
					select_new_active_agg = temp_aggregator->is_active;
					
					ad_clear_agg(temp_aggregator);
					if (select_new_active_agg) {
						pr_info("%s: Removing an active aggregator\n",
							slave->dev->master->name);
						
						ad_agg_selection_logic(__get_first_agg(port));
					}
				}
				break;
			}
		}
	}
	port->slave = NULL;
}

void bond_3ad_state_machine_handler(struct work_struct *work)
{
	struct bonding *bond = container_of(work, struct bonding,
					    ad_work.work);
	struct port *port;
	struct aggregator *aggregator;

	read_lock(&bond->lock);

	
	if (bond->slave_cnt == 0)
		goto re_arm;

	
	if (BOND_AD_INFO(bond).agg_select_timer && !(--BOND_AD_INFO(bond).agg_select_timer)) {
		
		if ((port = __get_first_port(bond))) {
			if (!port->slave) {
				pr_warning("%s: Warning: bond's first port is uninitialized\n",
					   bond->dev->name);
				goto re_arm;
			}

			aggregator = __get_first_agg(port);
			ad_agg_selection_logic(aggregator);
		}
		bond_3ad_set_carrier(bond);
	}

	
	for (port = __get_first_port(bond); port; port = __get_next_port(port)) {
		if (!port->slave) {
			pr_warning("%s: Warning: Found an uninitialized port\n",
				   bond->dev->name);
			goto re_arm;
		}

		__get_state_machine_lock(port);

		ad_rx_machine(NULL, port);
		ad_periodic_machine(port);
		ad_port_selection_logic(port);
		ad_mux_machine(port);
		ad_tx_machine(port);

		
		if (port->sm_vars & AD_PORT_BEGIN)
			port->sm_vars &= ~AD_PORT_BEGIN;

		__release_state_machine_lock(port);
	}

re_arm:
	queue_delayed_work(bond->wq, &bond->ad_work, ad_delta_in_ticks);

	read_unlock(&bond->lock);
}

static int bond_3ad_rx_indication(struct lacpdu *lacpdu, struct slave *slave, u16 length)
{
	struct port *port;
	int ret = RX_HANDLER_ANOTHER;

	if (length >= sizeof(struct lacpdu)) {

		port = &(SLAVE_AD_INFO(slave).port);

		if (!port->slave) {
			pr_warning("%s: Warning: port of slave %s is uninitialized\n",
				   slave->dev->name, slave->dev->master->name);
			return ret;
		}

		switch (lacpdu->subtype) {
		case AD_TYPE_LACPDU:
			ret = RX_HANDLER_CONSUMED;
			pr_debug("Received LACPDU on port %d\n",
				 port->actor_port_number);
			
			__get_state_machine_lock(port);
			ad_rx_machine(lacpdu, port);
			__release_state_machine_lock(port);
			break;

		case AD_TYPE_MARKER:
			ret = RX_HANDLER_CONSUMED;
			

			switch (((struct bond_marker *)lacpdu)->tlv_type) {
			case AD_MARKER_INFORMATION_SUBTYPE:
				pr_debug("Received Marker Information on port %d\n",
					 port->actor_port_number);
				ad_marker_info_received((struct bond_marker *)lacpdu, port);
				break;

			case AD_MARKER_RESPONSE_SUBTYPE:
				pr_debug("Received Marker Response on port %d\n",
					 port->actor_port_number);
				ad_marker_response_received((struct bond_marker *)lacpdu, port);
				break;

			default:
				pr_debug("Received an unknown Marker subtype on slot %d\n",
					 port->actor_port_number);
			}
		}
	}
	return ret;
}

void bond_3ad_adapter_speed_changed(struct slave *slave)
{
	struct port *port;

	port = &(SLAVE_AD_INFO(slave).port);

	
	if (!port->slave) {
		pr_warning("Warning: %s: speed changed for uninitialized port on %s\n",
			   slave->dev->master->name, slave->dev->name);
		return;
	}

	port->actor_admin_port_key &= ~AD_SPEED_KEY_BITS;
	port->actor_oper_port_key = port->actor_admin_port_key |=
		(__get_link_speed(port) << 1);
	pr_debug("Port %d changed speed\n", port->actor_port_number);
	
	
	port->sm_vars |= AD_PORT_BEGIN;
}

void bond_3ad_adapter_duplex_changed(struct slave *slave)
{
	struct port *port;

	port = &(SLAVE_AD_INFO(slave).port);

	
	if (!port->slave) {
		pr_warning("%s: Warning: duplex changed for uninitialized port on %s\n",
			   slave->dev->master->name, slave->dev->name);
		return;
	}

	port->actor_admin_port_key &= ~AD_DUPLEX_KEY_BITS;
	port->actor_oper_port_key = port->actor_admin_port_key |=
		__get_duplex(port);
	pr_debug("Port %d changed duplex\n", port->actor_port_number);
	
	
	port->sm_vars |= AD_PORT_BEGIN;
}

void bond_3ad_handle_link_change(struct slave *slave, char link)
{
	struct port *port;

	port = &(SLAVE_AD_INFO(slave).port);

	
	if (!port->slave) {
		pr_warning("Warning: %s: link status changed for uninitialized port on %s\n",
			   slave->dev->master->name, slave->dev->name);
		return;
	}

	
	
	if (link == BOND_LINK_UP) {
		port->is_enabled = true;
		port->actor_admin_port_key &= ~AD_DUPLEX_KEY_BITS;
		port->actor_oper_port_key = port->actor_admin_port_key |=
			__get_duplex(port);
		port->actor_admin_port_key &= ~AD_SPEED_KEY_BITS;
		port->actor_oper_port_key = port->actor_admin_port_key |=
			(__get_link_speed(port) << 1);
	} else {
		
		port->is_enabled = false;
		port->actor_admin_port_key &= ~AD_DUPLEX_KEY_BITS;
		port->actor_oper_port_key = (port->actor_admin_port_key &=
					     ~AD_SPEED_KEY_BITS);
	}
	
	
	
	port->sm_vars |= AD_PORT_BEGIN;
}

int bond_3ad_set_carrier(struct bonding *bond)
{
	struct aggregator *active;

	active = __get_active_agg(&(SLAVE_AD_INFO(bond->first_slave).aggregator));
	if (active) {
		
		if (active->num_of_ports < bond->params.min_links) {
			if (netif_carrier_ok(bond->dev)) {
				netif_carrier_off(bond->dev);
				return 1;
			}
		} else if (!netif_carrier_ok(bond->dev)) {
			netif_carrier_on(bond->dev);
			return 1;
		}
		return 0;
	}

	if (netif_carrier_ok(bond->dev)) {
		netif_carrier_off(bond->dev);
		return 1;
	}
	return 0;
}

int bond_3ad_get_active_agg_info(struct bonding *bond, struct ad_info *ad_info)
{
	struct aggregator *aggregator = NULL;
	struct port *port;

	for (port = __get_first_port(bond); port; port = __get_next_port(port)) {
		if (port->aggregator && port->aggregator->is_active) {
			aggregator = port->aggregator;
			break;
		}
	}

	if (aggregator) {
		ad_info->aggregator_id = aggregator->aggregator_identifier;
		ad_info->ports = aggregator->num_of_ports;
		ad_info->actor_key = aggregator->actor_oper_aggregator_key;
		ad_info->partner_key = aggregator->partner_oper_aggregator_key;
		memcpy(ad_info->partner_system, aggregator->partner_system.mac_addr_value, ETH_ALEN);
		return 0;
	}

	return -1;
}

int bond_3ad_xmit_xor(struct sk_buff *skb, struct net_device *dev)
{
	struct slave *slave, *start_at;
	struct bonding *bond = netdev_priv(dev);
	int slave_agg_no;
	int slaves_in_agg;
	int agg_id;
	int i;
	struct ad_info ad_info;
	int res = 1;

	if (bond_3ad_get_active_agg_info(bond, &ad_info)) {
		pr_debug("%s: Error: bond_3ad_get_active_agg_info failed\n",
			 dev->name);
		goto out;
	}

	slaves_in_agg = ad_info.ports;
	agg_id = ad_info.aggregator_id;

	if (slaves_in_agg == 0) {
		
		pr_debug("%s: Error: active aggregator is empty\n", dev->name);
		goto out;
	}

	slave_agg_no = bond->xmit_hash_policy(skb, slaves_in_agg);

	bond_for_each_slave(bond, slave, i) {
		struct aggregator *agg = SLAVE_AD_INFO(slave).port.aggregator;

		if (agg && (agg->aggregator_identifier == agg_id)) {
			slave_agg_no--;
			if (slave_agg_no < 0)
				break;
		}
	}

	if (slave_agg_no >= 0) {
		pr_err("%s: Error: Couldn't find a slave to tx on for aggregator ID %d\n",
		       dev->name, agg_id);
		goto out;
	}

	start_at = slave;

	bond_for_each_slave_from(bond, slave, i, start_at) {
		int slave_agg_id = 0;
		struct aggregator *agg = SLAVE_AD_INFO(slave).port.aggregator;

		if (agg)
			slave_agg_id = agg->aggregator_identifier;

		if (SLAVE_IS_OK(slave) && agg && (slave_agg_id == agg_id)) {
			res = bond_dev_queue_xmit(bond, skb, slave->dev);
			break;
		}
	}

out:
	if (res) {
		
		dev_kfree_skb(skb);
	}

	return NETDEV_TX_OK;
}

int bond_3ad_lacpdu_recv(struct sk_buff *skb, struct bonding *bond,
			  struct slave *slave)
{
	int ret = RX_HANDLER_ANOTHER;
	if (skb->protocol != PKT_TYPE_LACPDU)
		return ret;

	if (!pskb_may_pull(skb, sizeof(struct lacpdu)))
		return ret;

	read_lock(&bond->lock);
	ret = bond_3ad_rx_indication((struct lacpdu *) skb->data, slave, skb->len);
	read_unlock(&bond->lock);
	return ret;
}

void bond_3ad_update_lacp_rate(struct bonding *bond)
{
	int i;
	struct slave *slave;
	struct port *port = NULL;
	int lacp_fast;

	read_lock(&bond->lock);
	lacp_fast = bond->params.lacp_fast;

	bond_for_each_slave(bond, slave, i) {
		port = &(SLAVE_AD_INFO(slave).port);
		__get_state_machine_lock(port);
		if (lacp_fast)
			port->actor_oper_port_state |= AD_STATE_LACP_TIMEOUT;
		else
			port->actor_oper_port_state &= ~AD_STATE_LACP_TIMEOUT;
		__release_state_machine_lock(port);
	}

	read_unlock(&bond->lock);
}
