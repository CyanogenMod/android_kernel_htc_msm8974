/*
 * Copyright 2010 Tilera Corporation. All Rights Reserved.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *   NON INFRINGEMENT.  See the GNU General Public License for
 *   more details.
 *
 * Structure definitions for migration, exposed here for use by
 * arch/tile/kernel/asm-offsets.c.
 */

#ifndef MM_MIGRATE_H
#define MM_MIGRATE_H

#include <linux/cpumask.h>
#include <hv/hypervisor.h>

extern int flush_and_install_context(HV_PhysAddr page_table, HV_PTE access,
				     HV_ASID asid,
				     const unsigned long *cpumask);

extern int homecache_migrate_stack_and_flush(pte_t stack_pte, unsigned long va,
				     size_t length, pte_t *stack_ptep,
				     const struct cpumask *cache_cpumask,
				     const struct cpumask *tlb_cpumask,
				     HV_Remote_ASID *asids,
				     int asidcount);

#endif 
