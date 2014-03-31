/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _ARCH_ARM_MACH_MSM_SMEM_PRIVATE_H_
#define _ARCH_ARM_MACH_MSM_SMEM_PRIVATE_H_

#include <linux/remote_spinlock.h>

#include <mach/ramdump.h>

#define SMD_HEAP_SIZE 512

struct smem_heap_info {
	unsigned initialized;
	unsigned free_offset;
	unsigned heap_remaining;
	unsigned reserved;
};

struct smem_heap_entry {
	unsigned allocated;
	unsigned offset;
	unsigned size;
	unsigned reserved; 
};
#define BASE_ADDR_MASK 0xfffffffc

struct smem_proc_comm {
	unsigned command;
	unsigned status;
	unsigned data1;
	unsigned data2;
};

struct smem_shared {
	struct smem_proc_comm proc_comm[4];
	unsigned version[32];
	struct smem_heap_info heap_info;
	struct smem_heap_entry heap_toc[SMD_HEAP_SIZE];
};

struct smem_area {
	phys_addr_t phys_addr;
	resource_size_t size;
	void __iomem *virt_addr;
};

remote_spinlock_t *smem_get_remote_spinlock(void);

bool smem_initialized_check(void);

int smem_module_init_notifier_register(struct notifier_block *nb);

int smem_module_init_notifier_unregister(struct notifier_block *nb);

unsigned smem_get_free_space(unsigned to_proc);
#endif 
