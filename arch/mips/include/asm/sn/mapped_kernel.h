/*
 * File created by Kanoj Sarcar 06/06/00.
 * Copyright 2000 Silicon Graphics, Inc.
 */
#ifndef __ASM_SN_MAPPED_KERNEL_H
#define __ASM_SN_MAPPED_KERNEL_H

#include <linux/mmzone.h>

#include <asm/addrspace.h>

#define REP_BASE	CAC_BASE

#ifdef CONFIG_MAPPED_KERNEL

#define MAPPED_ADDR_RO_TO_PHYS(x)	(x - REP_BASE)
#define MAPPED_ADDR_RW_TO_PHYS(x)	(x - REP_BASE - 16777216)

#define MAPPED_KERN_RO_PHYSBASE(n) (hub_data(n)->kern_vars.kv_ro_baseaddr)
#define MAPPED_KERN_RW_PHYSBASE(n) (hub_data(n)->kern_vars.kv_rw_baseaddr)

#define MAPPED_KERN_RO_TO_PHYS(x) \
				((unsigned long)MAPPED_ADDR_RO_TO_PHYS(x) | \
				MAPPED_KERN_RO_PHYSBASE(get_compact_nodeid()))
#define MAPPED_KERN_RW_TO_PHYS(x) \
				((unsigned long)MAPPED_ADDR_RW_TO_PHYS(x) | \
				MAPPED_KERN_RW_PHYSBASE(get_compact_nodeid()))

#else 

#define MAPPED_KERN_RO_TO_PHYS(x)	(x - REP_BASE)
#define MAPPED_KERN_RW_TO_PHYS(x)	(x - REP_BASE)

#endif 

#define MAPPED_KERN_RO_TO_K0(x)	PHYS_TO_K0(MAPPED_KERN_RO_TO_PHYS(x))
#define MAPPED_KERN_RW_TO_K0(x)	PHYS_TO_K0(MAPPED_KERN_RW_TO_PHYS(x))

#endif 
