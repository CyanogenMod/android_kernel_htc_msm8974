/*
 * Written by: Patricia Gaughen, IBM Corporation
 *
 * Copyright (C) 2002, IBM Corp.
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 * NON INFRINGEMENT.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Send feedback to <gone@us.ibm.com>
 */

#ifndef _ASM_X86_NUMAQ_H
#define _ASM_X86_NUMAQ_H

#ifdef CONFIG_X86_NUMAQ

extern int found_numaq;
extern int numaq_numa_init(void);
extern int pci_numaq_init(void);

extern void *xquad_portio;

#define XQUAD_PORTIO_BASE 0xfe400000
#define XQUAD_PORTIO_QUAD 0x40000  
#define XQUAD_PORT_ADDR(port, quad) (xquad_portio + (XQUAD_PORTIO_QUAD*quad) + port)

#define SYS_CFG_DATA_PRIV_ADDR		0x0009d000 

struct eachquadmem {
	unsigned int	priv_mem_start;		
						
						
						
	unsigned int	priv_mem_size;		
						
						
	unsigned int	low_shrd_mem_strp_start;
						
						
						
	unsigned int	low_shrd_mem_start;	
						
						
						
	unsigned int	low_shrd_mem_size;	
						
						
	unsigned int	lmmio_copb_start;	
						
						
						
						
	unsigned int	lmmio_copb_size;	
						
						
						
	unsigned int	lmmio_nopb_start;	
						
						
						
						
	unsigned int	lmmio_nopb_size;	
						
						
						
	unsigned int	io_apic_0_start;	
						
	unsigned int	io_apic_0_sz;		
	unsigned int	io_apic_1_start;	
						
	unsigned int	io_apic_1_sz;		
	unsigned int	hi_shrd_mem_start;	
						
						
	unsigned int	hi_shrd_mem_size;	
						
						
	unsigned int	mps_table_addr;		
						
						
	unsigned int	lcl_MDC_pio_addr;	
						
	unsigned int	rmt_MDC_mmpio_addr;	
						
	unsigned int	mm_port_io_start;	
						
						
	unsigned int	mm_port_io_size;	
						
	unsigned int	mm_rmt_io_apic_start;	
						
						
	unsigned int	mm_rmt_io_apic_size;	
						
						
	unsigned int	mm_isa_start;		
						
						
						
	unsigned int	mm_isa_size;		
						
						
	unsigned int	rmt_qmi_addr;		
	unsigned int	lcl_qmi_addr;		
};

struct sys_cfg_data {
	unsigned int	quad_id;
	unsigned int	bsp_proc_id; 
	unsigned int	scd_version; 
	unsigned int	first_quad_id;
	unsigned int	quads_present31_0; 
	unsigned int	quads_present63_32; 
	unsigned int	config_flags;
	unsigned int	boot_flags;
	unsigned int	csr_start_addr; 
	unsigned int	csr_size; 
	unsigned int	lcl_apic_start_addr; 
	unsigned int	lcl_apic_size; 
	unsigned int	low_shrd_mem_base; 
	unsigned int	low_shrd_mem_quad_offset; 
					
	unsigned int	split_mem_enbl; 
	unsigned int	mmio_sz; 
				 
	unsigned int	quad_spin_lock; 
					
	unsigned int	nonzero55; 
	unsigned int	nonzeroaa; 
	unsigned int	scd_magic_number;
	unsigned int	system_type;
	unsigned int	checksum;
	struct		eachquadmem eq[MAX_NUMNODES];	
};

void numaq_tsc_disable(void);

#endif 
#endif 

