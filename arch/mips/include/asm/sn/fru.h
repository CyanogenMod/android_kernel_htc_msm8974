/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Derived from IRIX <sys/SN/SN0/sn0_fru.h>
 *
 * Copyright (C) 1992 - 1997, 1999 Silcon Graphics, Inc.
 * Copyright (C) 1999, 2006 Ralf Baechle (ralf@linux-mips)
 */
#ifndef __ASM_SN_FRU_H
#define __ASM_SN_FRU_H

#define MAX_DIMMS			8	 
#define MAX_PCIDEV			8	 

typedef unsigned char confidence_t;

typedef struct kf_mem_s {
	confidence_t km_confidence; 
	confidence_t km_dimm[MAX_DIMMS];

} kf_mem_t;

typedef struct kf_cpu_s {
	confidence_t  	kc_confidence; 
	confidence_t  	kc_icache; 
	confidence_t  	kc_dcache; 
	confidence_t  	kc_scache; 
	confidence_t	kc_sysbus; 
} kf_cpu_t;

typedef struct kf_pci_bus_s {
	confidence_t	kpb_belief;	
	confidence_t	kpb_pcidev_belief[MAX_PCIDEV];
	                                
} kf_pci_bus_t;

#endif 
