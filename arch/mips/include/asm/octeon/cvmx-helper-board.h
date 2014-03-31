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

#ifndef __CVMX_HELPER_BOARD_H__
#define __CVMX_HELPER_BOARD_H__

#include "cvmx-helper.h"

typedef enum {
	set_phy_link_flags_autoneg = 0x1,
	set_phy_link_flags_flow_control_dont_touch = 0x0 << 1,
	set_phy_link_flags_flow_control_enable = 0x1 << 1,
	set_phy_link_flags_flow_control_disable = 0x2 << 1,
	set_phy_link_flags_flow_control_mask = 0x3 << 1,	
} cvmx_helper_board_set_phy_link_flags_types_t;

#define CVMX_HELPER_BOARD_MGMT_IPD_PORT     -10

extern cvmx_helper_link_info_t(*cvmx_override_board_link_get) (int ipd_port);

extern int cvmx_helper_board_get_mii_address(int ipd_port);

int cvmx_helper_board_link_set_phy(int phy_addr,
				   cvmx_helper_board_set_phy_link_flags_types_t
				   link_flags,
				   cvmx_helper_link_info_t link_info);

extern cvmx_helper_link_info_t __cvmx_helper_board_link_get(int ipd_port);

extern int __cvmx_helper_board_interface_probe(int interface,
					       int supported_ports);

extern int __cvmx_helper_board_hardware_enable(int interface);

#endif 
