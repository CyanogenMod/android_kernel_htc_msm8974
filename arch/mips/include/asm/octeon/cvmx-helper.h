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


#ifndef __CVMX_HELPER_H__
#define __CVMX_HELPER_H__

#include "cvmx-config.h"
#include "cvmx-fpa.h"
#include "cvmx-wqe.h"

typedef enum {
	CVMX_HELPER_INTERFACE_MODE_DISABLED,
	CVMX_HELPER_INTERFACE_MODE_RGMII,
	CVMX_HELPER_INTERFACE_MODE_GMII,
	CVMX_HELPER_INTERFACE_MODE_SPI,
	CVMX_HELPER_INTERFACE_MODE_PCIE,
	CVMX_HELPER_INTERFACE_MODE_XAUI,
	CVMX_HELPER_INTERFACE_MODE_SGMII,
	CVMX_HELPER_INTERFACE_MODE_PICMG,
	CVMX_HELPER_INTERFACE_MODE_NPI,
	CVMX_HELPER_INTERFACE_MODE_LOOP,
} cvmx_helper_interface_mode_t;

typedef union {
	uint64_t u64;
	struct {
		uint64_t reserved_20_63:44;
		uint64_t link_up:1;	    
		uint64_t full_duplex:1;	    
		uint64_t speed:18;	    
	} s;
} cvmx_helper_link_info_t;

#include "cvmx-helper-fpa.h"

#include <asm/octeon/cvmx-helper-errata.h>
#include "cvmx-helper-loop.h"
#include "cvmx-helper-npi.h"
#include "cvmx-helper-rgmii.h"
#include "cvmx-helper-sgmii.h"
#include "cvmx-helper-spi.h"
#include "cvmx-helper-util.h"
#include "cvmx-helper-xaui.h"

extern void (*cvmx_override_pko_queue_priority) (int pko_port,
						 uint64_t priorities[16]);

extern void (*cvmx_override_ipd_port_setup) (int ipd_port);

extern int cvmx_helper_ipd_and_packet_input_enable(void);

extern int cvmx_helper_initialize_packet_io_global(void);

extern int cvmx_helper_initialize_packet_io_local(void);

extern int cvmx_helper_ports_on_interface(int interface);

extern int cvmx_helper_get_number_of_interfaces(void);

extern cvmx_helper_interface_mode_t cvmx_helper_interface_get_mode(int
								   interface);

extern cvmx_helper_link_info_t cvmx_helper_link_autoconf(int ipd_port);

extern cvmx_helper_link_info_t cvmx_helper_link_get(int ipd_port);

extern int cvmx_helper_link_set(int ipd_port,
				cvmx_helper_link_info_t link_info);

extern int cvmx_helper_interface_probe(int interface);
extern int cvmx_helper_interface_enumerate(int interface);

extern int cvmx_helper_configure_loopback(int ipd_port, int enable_internal,
					  int enable_external);

#endif 
