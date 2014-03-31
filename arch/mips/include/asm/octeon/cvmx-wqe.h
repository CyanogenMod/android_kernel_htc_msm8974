/***********************license start***************
 * Author: Cavium Networks
 *
 * Contact: support@caviumnetworks.com
 * This file is part of the OCTEON SDK
 *
 * Copyright (c) 2003-2008 Cavium Networks
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, Version 2, as
 * published by the Free Software Foundation.
 *
 * This file is distributed in the hope that it will be useful, but
 * AS-IS and WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE, TITLE, or
 * NONINFRINGEMENT.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 * or visit http://www.gnu.org/licenses/.
 *
 * This file may also be available under a different license from Cavium.
 * Contact Cavium Networks for more information
 ***********************license end**************************************/


#ifndef __CVMX_WQE_H__
#define __CVMX_WQE_H__

#include "cvmx-packet.h"


#define OCT_TAG_TYPE_STRING(x)						\
	(((x) == CVMX_POW_TAG_TYPE_ORDERED) ?  "ORDERED" :		\
		(((x) == CVMX_POW_TAG_TYPE_ATOMIC) ?  "ATOMIC" :	\
			(((x) == CVMX_POW_TAG_TYPE_NULL) ?  "NULL" :	\
				"NULL_NULL")))

typedef union {
	uint64_t u64;

	
	struct {
		
		uint64_t bufs:8;
		
		uint64_t ip_offset:8;
		
		uint64_t vlan_valid:1;
		
		uint64_t vlan_stacked:1;
		uint64_t unassigned:1;
		
		uint64_t vlan_cfi:1;
		
		uint64_t vlan_id:12;
		
		uint64_t pr:4;
		uint64_t unassigned2:8;
		
		uint64_t dec_ipcomp:1;
		
		uint64_t tcp_or_udp:1;
		
		uint64_t dec_ipsec:1;
		
		uint64_t is_v6:1;


		uint64_t software:1;
		
		uint64_t L4_error:1;
		
		uint64_t is_frag:1;
		uint64_t IP_exc:1;
		uint64_t is_bcast:1;
		uint64_t is_mcast:1;
		uint64_t not_IP:1;
		uint64_t rcv_error:1;
		
		uint64_t err_code:8;
	} s;

	
	struct {
		uint64_t unused1:16;
		uint64_t vlan:16;
		uint64_t unused2:32;
	} svlan;

	struct {
		uint64_t bufs:8;
		uint64_t unused:8;
		
		uint64_t vlan_valid:1;
		
		uint64_t vlan_stacked:1;
		uint64_t unassigned:1;
		uint64_t vlan_cfi:1;
		uint64_t vlan_id:12;
		uint64_t pr:4;
		uint64_t unassigned2:12;
		uint64_t software:1;
		uint64_t unassigned3:1;
		uint64_t is_rarp:1;
		uint64_t is_arp:1;
		uint64_t is_bcast:1;
		uint64_t is_mcast:1;
		uint64_t not_IP:1;

		uint64_t rcv_error:1;
		
		uint64_t err_code:8;
	} snoip;

} cvmx_pip_wqe_word2;

typedef struct {


	uint16_t hw_chksum;
	uint8_t unused;
    /**
     * Next pointer used by hardware for list maintenance.
     * May be written/read by HW before the work queue
     *           entry is scheduled to a PP
     * (Only 36 bits used in Octeon 1)
     */
	uint64_t next_ptr:40;


	uint64_t len:16;
	uint64_t ipprt:6;

	uint64_t qos:3;

	uint64_t grp:4;
	uint64_t tag_type:3;
	uint64_t tag:32;

	cvmx_pip_wqe_word2 word2;

	union cvmx_buf_ptr packet_ptr;

	uint8_t packet_data[96];


} CVMX_CACHE_LINE_ALIGNED cvmx_wqe_t;

#endif 
