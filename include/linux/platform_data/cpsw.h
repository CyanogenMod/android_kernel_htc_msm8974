/*
 * Texas Instruments Ethernet Switch Driver
 *
 * Copyright (C) 2012 Texas Instruments
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __CPSW_H__
#define __CPSW_H__

#include <linux/if_ether.h>

struct cpsw_slave_data {
	u32		slave_reg_ofs;
	u32		sliver_reg_ofs;
	const char	*phy_id;
	int		phy_if;
	u8		mac_addr[ETH_ALEN];
};

struct cpsw_platform_data {
	u32	ss_reg_ofs;	
	u32	channels;	
	u32	cpdma_reg_ofs;	
	u32	cpdma_sram_ofs;	

	u32	slaves;		
	struct cpsw_slave_data	*slave_data;

	u32	ale_reg_ofs;	
	u32	ale_entries;	

	u32	host_port_reg_ofs; 
	u32     host_port_num; 

	u32	hw_stats_reg_ofs;  

	u32	bd_ram_ofs;   
	u32	bd_ram_size;  
	u32	hw_ram_addr; 
	bool	no_bd_ram; 

	u32	rx_descs;	

	u32	mac_control;	
};

#endif 
