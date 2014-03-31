/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1995, 1996, 2003 by Ralf Baechle
 * Copyright (C) 1995, 1996 Andreas Busse
 * Copyright (C) 1995, 1996 Stoned Elipot
 * Copyright (C) 1995, 1996 Paul M. Antoine.
 * Copyright (C) 2009       Zhang Le
 */
#ifndef _ASM_BOOTINFO_H
#define _ASM_BOOTINFO_H

#include <linux/types.h>
#include <asm/setup.h>


#define  MACH_UNKNOWN		0	

#define  MACH_DSUNKNOWN		0
#define  MACH_DS23100		1	
#define  MACH_DS5100		2	
#define  MACH_DS5000_200	3	
#define  MACH_DS5000_1XX	4	
#define  MACH_DS5000_XX		5	
#define  MACH_DS5000_2X0	6	
#define  MACH_DS5400		7	
#define  MACH_DS5500		8	
#define  MACH_DS5800		9	
#define  MACH_DS5900		10	

#define MACH_MSP4200_EVAL       0	
#define MACH_MSP4200_GW         1	
#define MACH_MSP4200_FPGA       2	
#define MACH_MSP7120_EVAL       3	
#define MACH_MSP7120_GW         4	
#define MACH_MSP7120_FPGA       5	
#define MACH_MSP_OTHER        255	

#define	MACH_MIKROTIK_RB532	0	
#define MACH_MIKROTIK_RB532A	1	

#define MACH_LOONGSON_UNKNOWN  0
#define MACH_LEMOTE_FL2E       1
#define MACH_LEMOTE_FL2F       2
#define MACH_LEMOTE_ML2F7      3
#define MACH_LEMOTE_YL2F89     4
#define MACH_DEXXON_GDIUM2F10  5
#define MACH_LEMOTE_NAS        6
#define MACH_LEMOTE_LL2F       7
#define MACH_LOONGSON_END      8

#define  MACH_INGENIC_JZ4730	0	
#define  MACH_INGENIC_JZ4740	1	

extern char *system_type;
const char *get_system_type(void);

extern unsigned long mips_machtype;

#define BOOT_MEM_MAP_MAX	32
#define BOOT_MEM_RAM		1
#define BOOT_MEM_ROM_DATA	2
#define BOOT_MEM_RESERVED	3
#define BOOT_MEM_INIT_RAM	4

struct boot_mem_map {
	int nr_map;
	struct boot_mem_map_entry {
		phys_t addr;	
		phys_t size;	
		long type;		
	} map[BOOT_MEM_MAP_MAX];
};

extern struct boot_mem_map boot_mem_map;

extern void add_memory_region(phys_t start, phys_t size, long type);

extern void prom_init(void);
extern void prom_free_prom_memory(void);

extern void free_init_pages(const char *what,
			    unsigned long begin, unsigned long end);

extern char arcs_cmdline[COMMAND_LINE_SIZE];

extern unsigned long fw_arg0, fw_arg1, fw_arg2, fw_arg3;

extern void plat_mem_setup(void);

#ifdef CONFIG_SWIOTLB
extern void plat_swiotlb_setup(void);

#else

static inline void plat_swiotlb_setup(void) {}

#endif 

#endif 
