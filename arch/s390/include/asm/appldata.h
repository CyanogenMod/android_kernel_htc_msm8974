/*
 * include/asm-s390/appldata.h
 *
 * Copyright (C) IBM Corp. 2006
 *
 * Author(s): Melissa Howland <melissah@us.ibm.com>
 */

#ifndef _ASM_S390_APPLDATA_H
#define _ASM_S390_APPLDATA_H

#include <asm/io.h>

#ifndef CONFIG_64BIT

#define APPLDATA_START_INTERVAL_REC	0x00	
#define APPLDATA_STOP_REC		0x01	
#define APPLDATA_GEN_EVENT_REC		0x02
#define APPLDATA_START_CONFIG_REC	0x03

struct appldata_parameter_list {
	u16 diag;		
	u8  function;		
	u8  parlist_length;	
	u32 product_id_addr;	
	u16 reserved;
	u16 buffer_length;	
	u32 buffer_addr;	
} __attribute__ ((packed));

#else 

#define APPLDATA_START_INTERVAL_REC	0x80
#define APPLDATA_STOP_REC		0x81
#define APPLDATA_GEN_EVENT_REC		0x82
#define APPLDATA_START_CONFIG_REC	0x83

struct appldata_parameter_list {
	u16 diag;
	u8  function;
	u8  parlist_length;
	u32 unused01;
	u16 reserved;
	u16 buffer_length;
	u32 unused02;
	u64 product_id_addr;
	u64 buffer_addr;
} __attribute__ ((packed));

#endif 

struct appldata_product_id {
	char prod_nr[7];	
	u16  prod_fn;		
	u8   record_nr; 	
	u16  version_nr;	
	u16  release_nr;	
	u16  mod_lvl;		
} __attribute__ ((packed));

static inline int appldata_asm(struct appldata_product_id *id,
			       unsigned short fn, void *buffer,
			       unsigned short length)
{
	struct appldata_parameter_list parm_list;
	int ry;

	if (!MACHINE_IS_VM)
		return -ENOSYS;
	parm_list.diag = 0xdc;
	parm_list.function = fn;
	parm_list.parlist_length = sizeof(parm_list);
	parm_list.buffer_length = length;
	parm_list.product_id_addr = (unsigned long) id;
	parm_list.buffer_addr = virt_to_phys(buffer);
	asm volatile(
		"	diag	%1,%0,0xdc"
		: "=d" (ry)
		: "d" (&parm_list), "m" (parm_list), "m" (*id)
		: "cc");
	return ry;
}

#endif 
