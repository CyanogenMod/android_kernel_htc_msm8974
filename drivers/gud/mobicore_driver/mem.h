/*
 * MobiCore driver module.(interface to the secure world SWD)
 *
 * <-- Copyright Giesecke & Devrient GmbH 2009-2012 -->
 * <-- Copyright Trustonic Limited 2013 -->
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _MC_MEM_H_
#define _MC_MEM_H_

#define FREE_FROM_SWD	1
#define FREE_FROM_NWD	0

#define LOCKED_BY_APP	(1U << 0)
#define LOCKED_BY_MC	(1U << 1)

#define MC_ARM_L2_TABLE_ENTRIES		256

struct l2table {
	pte_t	table_entries[MC_ARM_L2_TABLE_ENTRIES];
};

#define L2_TABLES_PER_PAGE		4

struct mc_l2_table_store {
	struct l2table table[L2_TABLES_PER_PAGE];
};

struct mc_l2_tables_set {
	struct list_head		list;
	
	struct mc_l2_table_store	*kernel_virt;
	
	struct mc_l2_table_store	*phys;
	
	struct page			*page;
	
	atomic_t			used_tables;
};

struct mc_l2_table {
	struct list_head	list;
	
	struct mutex		lock;
	
	unsigned int		handle;
	
	atomic_t		usage;
	
	struct mc_instance	*owner;
	
	struct mc_l2_tables_set	*set;
	
	unsigned int		idx;
	
	unsigned int		pages;
	
	void			*virt;
	unsigned long		phys;
};

struct mc_mem_context {
	struct mc_instance	*daemon_inst;
	
	struct list_head	l2_tables_sets;
	
	struct list_head	l2_tables;
	
	struct list_head	free_l2_tables;
	
	struct mutex		table_lock;
};

struct mc_l2_table *mc_alloc_l2_table(struct mc_instance *instance,
	struct task_struct *task, void *wsm_buffer, unsigned int wsm_len);

void mc_clear_l2_tables(struct mc_instance *instance);

void mc_clean_l2_tables(void);

int mc_free_l2_table(struct mc_instance *instance, uint32_t handle);

int mc_lock_l2_table(struct mc_instance *instance, uint32_t handle);
int mc_unlock_l2_table(struct mc_instance *instance, uint32_t handle);
uint32_t mc_find_l2_table(uint32_t handle, int32_t fd);
void mc_release_l2_tables(void);

int mc_init_l2_tables(void);

#endif 
