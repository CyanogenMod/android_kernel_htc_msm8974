/* mmu.h: memory management context for FR-V with or without MMU support
 *
 * Copyright (C) 2004 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#ifndef _ASM_MMU_H
#define _ASM_MMU_H

typedef struct {
#ifdef CONFIG_MMU
	struct list_head id_link;		
	unsigned short	id;			
	unsigned short	id_busy;		
	unsigned long	itlb_cached_pge;	
	unsigned long	itlb_ptd_mapping;	
	unsigned long	dtlb_cached_pge;	
	unsigned long	dtlb_ptd_mapping;	

#else
	unsigned long		end_brk;

#endif

#ifdef CONFIG_BINFMT_ELF_FDPIC
	unsigned long	exec_fdpic_loadmap;
	unsigned long	interp_fdpic_loadmap;
#endif

} mm_context_t;

#ifdef CONFIG_MMU
extern int __nongpreldata cxn_pinned;
extern int cxn_pin_by_pid(pid_t pid);
#endif

#endif 
