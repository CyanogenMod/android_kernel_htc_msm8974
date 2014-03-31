
#ifndef _ASM_X86_RIO_H
#define _ASM_X86_RIO_H

#define RIO_TABLE_VERSION	3

struct rio_table_hdr {
	u8 version;		
	u8 num_scal_dev;	
	u8 num_rio_dev;		
} __attribute__((packed));

struct scal_detail {
	u8 node_id;		
	u32 CBAR;		
	u8 port0node;		
	u8 port0port;		
				
	u8 port1node;		
	u8 port1port;		
				
	u8 port2node;		
	u8 port2port;		
				
	u8 chassis_num;		
} __attribute__((packed));

struct rio_detail {
	u8 node_id;		
	u32 BBAR;		
	u8 type;		
	u8 owner_id;		
				
	u8 port0node;		
	u8 port0port;		
				
	u8 port1node;		
	u8 port1port;		
				
	u8 first_slot;		
	u8 status;		
				
				
				
	u8 WP_index;		
				
	u8 chassis_num;		
} __attribute__((packed));

enum {
	HURR_SCALABILTY	= 0,	
	HURR_RIOIB	= 2,	
	COMPAT_CALGARY	= 4,	
	ALT_CALGARY	= 5,	
};

#endif 
