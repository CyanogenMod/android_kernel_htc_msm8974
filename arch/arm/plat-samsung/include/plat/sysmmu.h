/* linux/arch/arm/plat-samsung/include/plat/sysmmu.h
 *
 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung System MMU driver for S5P platform
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __PLAT_SAMSUNG_SYSMMU_H
#define __PLAT_SAMSUNG_SYSMMU_H __FILE__

enum S5P_SYSMMU_INTERRUPT_TYPE {
	SYSMMU_PAGEFAULT,
	SYSMMU_AR_MULTIHIT,
	SYSMMU_AW_MULTIHIT,
	SYSMMU_BUSERROR,
	SYSMMU_AR_SECURITY,
	SYSMMU_AR_ACCESS,
	SYSMMU_AW_SECURITY,
	SYSMMU_AW_PROTECTION, 
	SYSMMU_FAULTS_NUM
};

#ifdef CONFIG_S5P_SYSTEM_MMU

#include <mach/sysmmu.h>

void s5p_sysmmu_enable(sysmmu_ips ips, unsigned long pgd);

void s5p_sysmmu_disable(sysmmu_ips ips);

void s5p_sysmmu_set_tablebase_pgd(sysmmu_ips ips, unsigned long pgd);

void s5p_sysmmu_tlb_invalidate(sysmmu_ips ips);

void s5p_sysmmu_set_fault_handler(sysmmu_ips ips,
			int (*handler)(enum S5P_SYSMMU_INTERRUPT_TYPE itype,
					unsigned long pgtable_base,
					unsigned long fault_addr));
#else
#define s5p_sysmmu_enable(ips, pgd) do { } while (0)
#define s5p_sysmmu_disable(ips) do { } while (0)
#define s5p_sysmmu_set_tablebase_pgd(ips, pgd) do { } while (0)
#define s5p_sysmmu_tlb_invalidate(ips) do { } while (0)
#define s5p_sysmmu_set_fault_handler(ips, handler) do { } while (0)
#endif
#endif 
