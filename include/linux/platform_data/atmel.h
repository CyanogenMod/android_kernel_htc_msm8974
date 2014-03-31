/*
 * atmel platform data
 *
 * GPL v2 Only
 */

#ifndef __ATMEL_H__
#define __ATMEL_H__

#include <linux/mtd/nand.h>

 
struct atmel_nand_data {
	int		enable_pin;		
	int		det_pin;		
	int		rdy_pin;		
	u8		rdy_pin_active_low;	
	u8		ale;			
	u8		cle;			
	u8		bus_width_16;		
	u8		ecc_mode;		
	u8		on_flash_bbt;		
	struct mtd_partition *parts;
	unsigned int	num_parts;
};

#endif 
