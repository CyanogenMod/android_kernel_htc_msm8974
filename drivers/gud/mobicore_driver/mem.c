/*
 * MobiCore Driver Kernel Module.
 *
 * This module is written as a Linux device driver.
 * This driver represents the command proxy on the lowest layer, from the
 * secure world to the non secure world, and vice versa.
 * This driver is located in the non secure world (Linux).
 * This driver offers IOCTL commands, for access to the secure world, and has
 * the interface from the secure world to the normal world.
 * The access to the driver is possible with a file descriptor,
 * which has to be created by the fd = open(/dev/mobicore) command.
 *
 * <-- Copyright Giesecke & Devrient GmbH 2009-2012 -->
 * <-- Copyright Trustonic Limited 2013 -->
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "main.h"
#include "debug.h"
#include "mem.h"

#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/pagemap.h>
#include <linux/device.h>


struct mc_mem_context mem_ctx;

static inline struct page *l2_pte_to_page(pte_t pte)
{
	unsigned long phys_page_addr = ((unsigned long)pte & PAGE_MASK);
	unsigned int pfn = phys_page_addr >> PAGE_SHIFT;
	struct page *page = pfn_to_page(pfn);
	return page;
}

static inline pte_t page_to_l2_pte(struct page *page)
{
	unsigned long pfn = page_to_pfn(page);
	unsigned long phys_addr = (pfn << PAGE_SHIFT);
	pte_t pte = (pte_t)(phys_addr & PAGE_MASK);
	return pte;
}

static inline void release_page(struct page *page)
{
	SetPageDirty(page);

	page_cache_release(page);
}

static int lock_pages(struct task_struct *task, void *virt_start_page_addr,
	int pages_no, struct page **pages)
{
	int locked_pages;

	
	down_read(&(task->mm->mmap_sem));
	locked_pages = get_user_pages(
				task,
				task->mm,
				(unsigned long)virt_start_page_addr,
				pages_no,
				1, 
				0,
				pages,
				NULL);
	up_read(&(task->mm->mmap_sem));

	
	if (locked_pages != pages_no) {
		MCDRV_DBG_ERROR(mcd, "get_user_pages() failed, locked_pages=%d",
				locked_pages);
		if (locked_pages > 0) {
			
			release_pages(pages, locked_pages, 0);
		}
		return -ENOMEM;
	}

	return 0;
}

struct l2table *get_l2_table_kernel_virt(struct mc_l2_table *table)
{
	if (WARN(!table, "Invalid L2 table"))
		return NULL;

	if (WARN(!table->set, "Invalid L2 table set"))
		return NULL;

	if (WARN(!table->set->kernel_virt, "Invalid L2 pointer"))
		return NULL;

	return &(table->set->kernel_virt->table[table->idx]);
}

struct l2table *get_l2_table_phys(struct mc_l2_table *table)
{
	if (WARN(!table, "Invalid L2 table"))
		return NULL;
	if (WARN(!table->set, "Invalid L2 table set"))
		return NULL;
	if (WARN(!table->set->kernel_virt, "Invalid L2 phys pointer"))
		return NULL;

	return &(table->set->phys->table[table->idx]);
}

static inline int in_use(struct mc_l2_table *table)
{
	return atomic_read(&table->usage) > 0;
}

struct mc_l2_table *find_l2_table(unsigned int handle)
{
	struct mc_l2_table *table;

	list_for_each_entry(table, &mem_ctx.l2_tables, list) {
		if (table->handle == handle)
			return table;
	}
	return NULL;
}

static int alloc_table_store(void)
{
	unsigned long store;
	struct mc_l2_tables_set *l2table_set;
	struct mc_l2_table *l2table, *l2table2;
	struct page *page;
	int ret = 0, i;
	
	LIST_HEAD(temp);

	store = get_zeroed_page(GFP_KERNEL);
	if (!store)
		return -ENOMEM;

	page = virt_to_page(store);
	SetPageReserved(page);

	
	l2table_set = kmalloc(sizeof(*l2table_set), GFP_KERNEL | __GFP_ZERO);
	if (l2table_set == NULL) {
		ret = -ENOMEM;
		goto free_store;
	}
	
	l2table_set->kernel_virt = (void *)store;
	l2table_set->page = page;
	l2table_set->phys = (void *)virt_to_phys((void *)store);
	
	atomic_set(&l2table_set->used_tables, 0);

	
	INIT_LIST_HEAD(&(l2table_set->list));
	list_add(&l2table_set->list, &mem_ctx.l2_tables_sets);

	for (i = 0; i < L2_TABLES_PER_PAGE; i++) {
		
		l2table  = kmalloc(sizeof(*l2table), GFP_KERNEL | __GFP_ZERO);
		if (l2table == NULL) {
			ret = -ENOMEM;
			MCDRV_DBG_ERROR(mcd, "out of memory\n");
			
			goto free_temp_list;
		}

		
		l2table->set = l2table_set;
		l2table->idx = i;
		l2table->virt = get_l2_table_kernel_virt(l2table);
		l2table->phys = (unsigned long)get_l2_table_phys(l2table);
		atomic_set(&l2table->usage, 0);

		
		INIT_LIST_HEAD(&l2table->list);
		list_add_tail(&l2table->list, &temp);
	}

	list_splice_tail(&temp, &mem_ctx.free_l2_tables);
	return 0;
free_temp_list:
	list_for_each_entry_safe(l2table, l2table2, &temp, list) {
		kfree(l2table);
	}

	list_del(&l2table_set->list);

free_store:
	free_page(store);
	return ret;

}
static struct mc_l2_table *alloc_l2_table(struct mc_instance *instance)
{
	int ret = 0;
	struct mc_l2_table *table = NULL;

	if (list_empty(&mem_ctx.free_l2_tables)) {
		ret = alloc_table_store();
		if (ret) {
			MCDRV_DBG_ERROR(mcd, "Failed to allocate new store!");
			return ERR_PTR(-ENOMEM);
		}
		
		if (list_empty(&mem_ctx.free_l2_tables)) {
			MCDRV_DBG_ERROR(mcd,
					"Free list not updated correctly!");
			return ERR_PTR(-EFAULT);
		}
	}

	
	table  = list_first_entry(&mem_ctx.free_l2_tables,
		struct mc_l2_table, list);
	if (table == NULL) {
		MCDRV_DBG_ERROR(mcd, "out of memory\n");
		return ERR_PTR(-ENOMEM);
	}
	
	list_move_tail(&table->list, &mem_ctx.l2_tables);

	table->handle = get_unique_id();
	table->owner = instance;

	atomic_inc(&table->set->used_tables);
	atomic_inc(&table->usage);

	MCDRV_DBG_VERBOSE(mcd,
			  "chunkPhys=%p,idx=%d", table->set->phys, table->idx);

	return table;
}

static void free_l2_table(struct mc_l2_table *table)
{
	struct mc_l2_tables_set *l2table_set;

	if (WARN(!table, "Invalid table"))
		return;

	l2table_set = table->set;
	if (WARN(!l2table_set, "Invalid table set"))
		return;

	list_move_tail(&table->list, &mem_ctx.free_l2_tables);

	
	if (atomic_dec_and_test(&l2table_set->used_tables)) {
		struct mc_l2_table *tmp;

		
		list_del(&l2table_set->list);
		list_for_each_entry_safe(table, tmp, &mem_ctx.free_l2_tables,
					 list) {
			if (table->set == l2table_set) {
				list_del(&table->list);
				kfree(table);
			}
		} 

		BUG_ON(!l2table_set->page);
		ClearPageReserved(l2table_set->page);

		BUG_ON(!l2table_set->kernel_virt);
		free_page((unsigned long)l2table_set->kernel_virt);

		kfree(l2table_set);
	}
}

static int map_buffer(struct task_struct *task, void *wsm_buffer,
		      unsigned int wsm_len, struct mc_l2_table *table)
{
	int		ret = 0;
	unsigned int	i, nr_of_pages;
	
	void		*virt_addr_page;
	struct page	*page;
	struct l2table	*l2table;
	struct page	**l2table_as_array_of_pointers_to_page;
	
	unsigned int offset;

	if (WARN(!wsm_buffer, "Invalid WSM buffer pointer"))
		return -EINVAL;

	if (WARN(wsm_len == 0, "Invalid WSM buffer length"))
		return -EINVAL;

	if (WARN(!table, "Invalid mapping table for WSM"))
		return -EINVAL;

	
	if (wsm_len > SZ_1M) {
		MCDRV_DBG_ERROR(mcd, "size > 1 MiB\n");
		return -EINVAL;
	}

	MCDRV_DBG_VERBOSE(mcd, "WSM addr=0x%p, len=0x%08x\n", wsm_buffer,
			  wsm_len);


	
	virt_addr_page = (void *)(((unsigned long)(wsm_buffer)) & PAGE_MASK);
	offset = (unsigned int)	(((unsigned long)(wsm_buffer)) & (~PAGE_MASK));
	nr_of_pages  = PAGE_ALIGN(offset + wsm_len) / PAGE_SIZE;

	MCDRV_DBG_VERBOSE(mcd, "virt addr page start=0x%p, pages=%d\n",
			  virt_addr_page, nr_of_pages);

	
	if ((nr_of_pages * PAGE_SIZE) > SZ_1M) {
		MCDRV_DBG_ERROR(mcd, "WSM paged exceed 1 MiB\n");
		return -EINVAL;
	}

	l2table = table->virt;
	l2table_as_array_of_pointers_to_page = (struct page **)l2table;

	
	if (task != NULL && !is_vmalloc_addr(wsm_buffer)) {
		ret = lock_pages(task, virt_addr_page, nr_of_pages,
				 l2table_as_array_of_pointers_to_page);
		if (ret != 0) {
			MCDRV_DBG_ERROR(mcd, "lock_user_pages() failed\n");
			return ret;
		}
	}
	
	else if (task == NULL && !is_vmalloc_addr(wsm_buffer)) {
		void *uaddr = wsm_buffer;
		for (i = 0; i < nr_of_pages; i++) {
			page = virt_to_page(uaddr);
			if (!page) {
				MCDRV_DBG_ERROR(mcd, "failed to map address");
				return -EINVAL;
			}
			get_page(page);
			l2table_as_array_of_pointers_to_page[i] = page;
			uaddr += PAGE_SIZE;
		}
	}
	
	else {
		void *uaddr = wsm_buffer;
		for (i = 0; i < nr_of_pages; i++) {
			page = vmalloc_to_page(uaddr);
			if (!page) {
				MCDRV_DBG_ERROR(mcd, "failed to map address");
				return -EINVAL;
			}
			get_page(page);
			l2table_as_array_of_pointers_to_page[i] = page;
			uaddr += PAGE_SIZE;
		}
	}

	table->pages = nr_of_pages;

	for (i = 0; i < nr_of_pages; i++) {
		pte_t pte;
		page = l2table_as_array_of_pointers_to_page[i];

		pte = page_to_l2_pte(page)
				| PTE_EXT_AP1 | PTE_EXT_AP0
				| PTE_CACHEABLE | PTE_BUFFERABLE
				| PTE_TYPE_SMALL | PTE_TYPE_EXT | PTE_EXT_NG;
#ifdef CONFIG_SMP
		pte |= PTE_EXT_SHARED | PTE_EXT_TEX(1);
#endif

		l2table->table_entries[i] = pte;
		MCDRV_DBG_VERBOSE(mcd, "L2 entry %d:  0x%08x\n", i,
				  (unsigned int)(pte));
	}

	
	while (i < 255)
		l2table->table_entries[i++] = (pte_t)0;


	return ret;
}

static void unmap_buffers(struct mc_l2_table *table)
{
	struct l2table *l2table;
	int i;

	if (WARN_ON(!table))
		return;

	
	MCDRV_DBG_VERBOSE(mcd, "clear L2 table, phys_base=%p, nr_of_pages=%d\n",
			  (void *)table->phys, table->pages);

	l2table = table->virt;

	
	for (i = 0; i < table->pages; i++) {
		
		pte_t pte = l2table->table_entries[i];
		struct page *page = l2_pte_to_page(pte);
		release_page(page);
	}

	
	table->pages = 0;
}

static void unmap_l2_table(struct mc_l2_table *table)
{
	
	if (!atomic_dec_and_test(&table->usage))
		return;

	
	unmap_buffers(table);
	free_l2_table(table);
}

int mc_free_l2_table(struct mc_instance *instance, uint32_t handle)
{
	struct mc_l2_table *table;
	int ret = 0;

	if (WARN(!instance, "No instance data available"))
		return -EFAULT;

	mutex_lock(&mem_ctx.table_lock);
	table = find_l2_table(handle);

	if (table == NULL) {
		MCDRV_DBG_VERBOSE(mcd, "entry not found");
		ret = -EINVAL;
		goto err_unlock;
	}
	if (instance != table->owner && !is_daemon(instance)) {
		MCDRV_DBG_ERROR(mcd, "instance does no own it");
		ret = -EPERM;
		goto err_unlock;
	}
	
	unmap_l2_table(table);
err_unlock:
	mutex_unlock(&mem_ctx.table_lock);

	return ret;
}

int mc_lock_l2_table(struct mc_instance *instance, uint32_t handle)
{
	int ret = 0;
	struct mc_l2_table *table = NULL;

	if (WARN(!instance, "No instance data available"))
		return -EFAULT;

	mutex_lock(&mem_ctx.table_lock);
	table = find_l2_table(handle);

	if (table == NULL) {
		MCDRV_DBG_VERBOSE(mcd, "entry not found %u\n", handle);
		ret = -EINVAL;
		goto table_err;
	}
	if (instance != table->owner && !is_daemon(instance)) {
		MCDRV_DBG_ERROR(mcd, "instance does no own it\n");
		ret = -EPERM;
		goto table_err;
	}

	
	atomic_inc(&table->usage);
table_err:
	mutex_unlock(&mem_ctx.table_lock);
	return ret;
}
struct mc_l2_table *mc_alloc_l2_table(struct mc_instance *instance,
	struct task_struct *task, void *wsm_buffer, unsigned int wsm_len)
{
	int ret = 0;
	struct mc_l2_table *table;

	if (WARN(!instance, "No instance data available"))
		return ERR_PTR(-EFAULT);

	mutex_lock(&mem_ctx.table_lock);
	table = alloc_l2_table(instance);
	if (IS_ERR(table)) {
		MCDRV_DBG_ERROR(mcd, "allocate_used_l2_table() failed\n");
		ret = -ENOMEM;
		goto err_no_mem;
	}

	
	ret = map_buffer(task, wsm_buffer, wsm_len, table);

	if (ret != 0) {
		MCDRV_DBG_ERROR(mcd, "map_buffer() failed\n");
		unmap_l2_table(table);
		goto err_no_mem;
	}
	MCDRV_DBG(mcd, "mapped buffer %p to table with handle %d @ %lx",
		  wsm_buffer, table->handle, table->phys);

	mutex_unlock(&mem_ctx.table_lock);
	return table;
err_no_mem:
	mutex_unlock(&mem_ctx.table_lock);
	return ERR_PTR(ret);
}

uint32_t mc_find_l2_table(uint32_t handle, int32_t fd)
{
	uint32_t ret = 0;
	struct mc_l2_table *table = NULL;

	mutex_lock(&mem_ctx.table_lock);
	table = find_l2_table(handle);

	if (table == NULL) {
		MCDRV_DBG_ERROR(mcd, "entry not found %u\n", handle);
		ret = 0;
		goto table_err;
	}

	if (!mc_check_owner_fd(table->owner, fd)) {
		MCDRV_DBG_ERROR(mcd, "not valid owner%u\n", handle);
		ret = 0;
		goto table_err;
	}

	ret = table->phys;
table_err:
	mutex_unlock(&mem_ctx.table_lock);
	return ret;
}

void mc_clean_l2_tables(void)
{
	struct mc_l2_table *table, *tmp;

	mutex_lock(&mem_ctx.table_lock);
	
	list_for_each_entry_safe(table, tmp, &mem_ctx.l2_tables, list) {
		if (table->owner == NULL) {
			MCDRV_DBG(mcd,
				  "clearing orphaned WSM L2: p=%lx pages=%d\n",
				  table->phys, table->pages);
			unmap_l2_table(table);
		}
	}
	mutex_unlock(&mem_ctx.table_lock);
}

void mc_clear_l2_tables(struct mc_instance *instance)
{
	struct mc_l2_table *table, *tmp;

	mutex_lock(&mem_ctx.table_lock);
	
	list_for_each_entry_safe(table, tmp, &mem_ctx.l2_tables, list) {
		if (table->owner == instance) {
			MCDRV_DBG(mcd, "release WSM L2: p=%lx pages=%d\n",
				  table->phys, table->pages);
			
			table->owner = NULL;
			unmap_l2_table(table);
		}
	}
	mutex_unlock(&mem_ctx.table_lock);
}

int mc_init_l2_tables(void)
{
	
	INIT_LIST_HEAD(&mem_ctx.l2_tables_sets);

	
	INIT_LIST_HEAD(&mem_ctx.l2_tables);

	
	INIT_LIST_HEAD(&mem_ctx.free_l2_tables);

	mutex_init(&mem_ctx.table_lock);

	return 0;
}

void mc_release_l2_tables()
{
	struct mc_l2_table *table;
	
	list_for_each_entry(table, &mem_ctx.l2_tables, list) {
		WARN(1, "WSM L2 still in use: phys=%lx ,nr_of_pages=%d\n",
		     table->phys, table->pages);
	}
}
