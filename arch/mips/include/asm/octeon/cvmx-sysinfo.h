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


#ifndef __CVMX_SYSINFO_H__
#define __CVMX_SYSINFO_H__

#define OCTEON_SERIAL_LEN 20
struct cvmx_sysinfo {
	
	
	uint64_t system_dram_size;

	
	void *phy_mem_desc_ptr;


	
	
	uint64_t stack_top;
	
	uint64_t heap_base;
	
	uint32_t stack_size;
	
	uint32_t heap_size;
	
	uint32_t core_mask;
	
	uint32_t init_core;

	
	uint64_t exception_base_addr;

	
	uint32_t cpu_clock_hz;

	
	uint32_t dram_data_rate_hz;


	uint16_t board_type;
	uint8_t board_rev_major;
	uint8_t board_rev_minor;
	uint8_t mac_addr_base[6];
	uint8_t mac_addr_count;
	char board_serial_number[OCTEON_SERIAL_LEN];
	uint64_t compact_flash_common_base_addr;
	uint64_t compact_flash_attribute_base_addr;
	uint64_t led_display_base_addr;
	
	uint32_t dfa_ref_clock_hz;
	
	uint32_t bootloader_config_flags;

	
	uint8_t console_uart_num;
};


extern struct cvmx_sysinfo *cvmx_sysinfo_get(void);

extern int cvmx_sysinfo_minimal_initialize(void *phy_mem_desc_ptr,
					   uint16_t board_type,
					   uint8_t board_rev_major,
					   uint8_t board_rev_minor,
					   uint32_t cpu_clock_hz);

#endif 
