/*
 * dmm.c
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * The Dynamic Memory Manager (DMM) module manages the DSP Virtual address
 * space that can be directly mapped to any MPU buffer or memory region
 *
 * Notes:
 *   Region: Generic memory entitiy having a start address and a size
 *   Chunk:  Reserved region
 *
 * Copyright (C) 2005-2006 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
#include <linux/types.h>

#include <dspbridge/host_os.h>

#include <dspbridge/dbdefs.h>

#include <dspbridge/sync.h>

#include <dspbridge/dev.h>
#include <dspbridge/proc.h>

#include <dspbridge/dmm.h>

#define DMM_ADDR_VIRTUAL(a) \
	(((struct map_page *)(a) - virtual_mapping_table) * PG_SIZE4K +\
	dyn_mem_map_beg)
#define DMM_ADDR_TO_INDEX(a) (((a) - dyn_mem_map_beg) / PG_SIZE4K)

struct dmm_object {
	spinlock_t dmm_lock;	
};

struct map_page {
	u32 region_size:15;
	u32 mapped_size:15;
	u32 reserved:1;
	u32 mapped:1;
};

static struct map_page *virtual_mapping_table;
static u32 free_region;		
static u32 free_size;
static u32 dyn_mem_map_beg;	
static u32 table_size;		

static struct map_page *get_region(u32 addr);
static struct map_page *get_free_region(u32 len);
static struct map_page *get_mapped_region(u32 addrs);

int dmm_create_tables(struct dmm_object *dmm_mgr, u32 addr, u32 size)
{
	struct dmm_object *dmm_obj = (struct dmm_object *)dmm_mgr;
	int status = 0;

	status = dmm_delete_tables(dmm_obj);
	if (!status) {
		dyn_mem_map_beg = addr;
		table_size = PG_ALIGN_HIGH(size, PG_SIZE4K) / PG_SIZE4K;
		
		virtual_mapping_table = __vmalloc(table_size *
				sizeof(struct map_page), GFP_KERNEL |
				__GFP_HIGHMEM | __GFP_ZERO, PAGE_KERNEL);
		if (virtual_mapping_table == NULL)
			status = -ENOMEM;
		else {
			free_region = 0;
			free_size = table_size * PG_SIZE4K;
			virtual_mapping_table[0].region_size = table_size;
		}
	}

	if (status)
		pr_err("%s: failure, status 0x%x\n", __func__, status);

	return status;
}

int dmm_create(struct dmm_object **dmm_manager,
		      struct dev_object *hdev_obj,
		      const struct dmm_mgrattrs *mgr_attrts)
{
	struct dmm_object *dmm_obj = NULL;
	int status = 0;

	*dmm_manager = NULL;
	
	dmm_obj = kzalloc(sizeof(struct dmm_object), GFP_KERNEL);
	if (dmm_obj != NULL) {
		spin_lock_init(&dmm_obj->dmm_lock);
		*dmm_manager = dmm_obj;
	} else {
		status = -ENOMEM;
	}

	return status;
}

int dmm_destroy(struct dmm_object *dmm_mgr)
{
	struct dmm_object *dmm_obj = (struct dmm_object *)dmm_mgr;
	int status = 0;

	if (dmm_mgr) {
		status = dmm_delete_tables(dmm_obj);
		if (!status)
			kfree(dmm_obj);
	} else
		status = -EFAULT;

	return status;
}

int dmm_delete_tables(struct dmm_object *dmm_mgr)
{
	int status = 0;

	
	if (dmm_mgr)
		vfree(virtual_mapping_table);
	else
		status = -EFAULT;
	return status;
}

int dmm_get_handle(void *hprocessor, struct dmm_object **dmm_manager)
{
	int status = 0;
	struct dev_object *hdev_obj;

	if (hprocessor != NULL)
		status = proc_get_dev_object(hprocessor, &hdev_obj);
	else
		hdev_obj = dev_get_first();	

	if (!status)
		status = dev_get_dmm_mgr(hdev_obj, dmm_manager);

	return status;
}

int dmm_map_memory(struct dmm_object *dmm_mgr, u32 addr, u32 size)
{
	struct dmm_object *dmm_obj = (struct dmm_object *)dmm_mgr;
	struct map_page *chunk;
	int status = 0;

	spin_lock(&dmm_obj->dmm_lock);
	chunk = (struct map_page *)get_region(addr);
	if (chunk != NULL) {
		
		chunk->mapped = true;
		chunk->mapped_size = (size / PG_SIZE4K);
	} else
		status = -ENOENT;
	spin_unlock(&dmm_obj->dmm_lock);

	dev_dbg(bridge, "%s dmm_mgr %p, addr %x, size %x\n\tstatus %x, "
		"chunk %p", __func__, dmm_mgr, addr, size, status, chunk);

	return status;
}

int dmm_reserve_memory(struct dmm_object *dmm_mgr, u32 size,
			      u32 *prsv_addr)
{
	int status = 0;
	struct dmm_object *dmm_obj = (struct dmm_object *)dmm_mgr;
	struct map_page *node;
	u32 rsv_addr = 0;
	u32 rsv_size = 0;

	spin_lock(&dmm_obj->dmm_lock);

	
	node = get_free_region(size);
	if (node != NULL) {
		
		rsv_addr = DMM_ADDR_VIRTUAL(node);
		
		rsv_size = size / PG_SIZE4K;
		if (rsv_size < node->region_size) {
			
			node[rsv_size].mapped = false;
			node[rsv_size].reserved = false;
			node[rsv_size].region_size =
			    node->region_size - rsv_size;
			node[rsv_size].mapped_size = 0;
		}
		node->mapped = false;
		node->reserved = true;
		node->region_size = rsv_size;
		node->mapped_size = 0;
		
		*prsv_addr = rsv_addr;
	} else
		
		status = -ENOMEM;

	spin_unlock(&dmm_obj->dmm_lock);

	dev_dbg(bridge, "%s dmm_mgr %p, size %x, prsv_addr %p\n\tstatus %x, "
		"rsv_addr %x, rsv_size %x\n", __func__, dmm_mgr, size,
		prsv_addr, status, rsv_addr, rsv_size);

	return status;
}

int dmm_un_map_memory(struct dmm_object *dmm_mgr, u32 addr, u32 *psize)
{
	struct dmm_object *dmm_obj = (struct dmm_object *)dmm_mgr;
	struct map_page *chunk;
	int status = 0;

	spin_lock(&dmm_obj->dmm_lock);
	chunk = get_mapped_region(addr);
	if (chunk == NULL)
		status = -ENOENT;

	if (!status) {
		
		*psize = chunk->mapped_size * PG_SIZE4K;
		chunk->mapped = false;
		chunk->mapped_size = 0;
	}
	spin_unlock(&dmm_obj->dmm_lock);

	dev_dbg(bridge, "%s: dmm_mgr %p, addr %x, psize %p\n\tstatus %x, "
		"chunk %p\n", __func__, dmm_mgr, addr, psize, status, chunk);

	return status;
}

int dmm_un_reserve_memory(struct dmm_object *dmm_mgr, u32 rsv_addr)
{
	struct dmm_object *dmm_obj = (struct dmm_object *)dmm_mgr;
	struct map_page *chunk;
	u32 i;
	int status = 0;
	u32 chunk_size;

	spin_lock(&dmm_obj->dmm_lock);

	
	chunk = get_mapped_region(rsv_addr);
	if (chunk == NULL)
		status = -ENOENT;

	if (!status) {
		
		i = 0;
		while (i < chunk->region_size) {
			if (chunk[i].mapped) {
				
				chunk_size = chunk[i].mapped_size;
				
				chunk[i].mapped = false;
				chunk[i].mapped_size = 0;
				i += chunk_size;
			} else
				i++;
		}
		
		chunk->reserved = false;
	}
	spin_unlock(&dmm_obj->dmm_lock);

	dev_dbg(bridge, "%s: dmm_mgr %p, rsv_addr %x\n\tstatus %x chunk %p",
		__func__, dmm_mgr, rsv_addr, status, chunk);

	return status;
}

static struct map_page *get_region(u32 addr)
{
	struct map_page *curr_region = NULL;
	u32 i = 0;

	if (virtual_mapping_table != NULL) {
		
		i = DMM_ADDR_TO_INDEX(addr);
		if (i < table_size)
			curr_region = virtual_mapping_table + i;
	}

	dev_dbg(bridge, "%s: curr_region %p, free_region %d, free_size %d\n",
		__func__, curr_region, free_region, free_size);
	return curr_region;
}

static struct map_page *get_free_region(u32 len)
{
	struct map_page *curr_region = NULL;
	u32 i = 0;
	u32 region_size = 0;
	u32 next_i = 0;

	if (virtual_mapping_table == NULL)
		return curr_region;
	if (len > free_size) {
		while (i < table_size) {
			region_size = virtual_mapping_table[i].region_size;
			next_i = i + region_size;
			if (virtual_mapping_table[i].reserved == false) {
				
				if (next_i < table_size &&
				    virtual_mapping_table[next_i].reserved
				    == false) {
					virtual_mapping_table[i].region_size +=
					    virtual_mapping_table
					    [next_i].region_size;
					continue;
				}
				region_size *= PG_SIZE4K;
				if (region_size > free_size) {
					free_region = i;
					free_size = region_size;
				}
			}
			i = next_i;
		}
	}
	if (len <= free_size) {
		curr_region = virtual_mapping_table + free_region;
		free_region += (len / PG_SIZE4K);
		free_size -= len;
	}
	return curr_region;
}

static struct map_page *get_mapped_region(u32 addrs)
{
	u32 i = 0;
	struct map_page *curr_region = NULL;

	if (virtual_mapping_table == NULL)
		return curr_region;

	i = DMM_ADDR_TO_INDEX(addrs);
	if (i < table_size && (virtual_mapping_table[i].mapped ||
			       virtual_mapping_table[i].reserved))
		curr_region = virtual_mapping_table + i;
	return curr_region;
}

#ifdef DSP_DMM_DEBUG
u32 dmm_mem_map_dump(struct dmm_object *dmm_mgr)
{
	struct map_page *curr_node = NULL;
	u32 i;
	u32 freemem = 0;
	u32 bigsize = 0;

	spin_lock(&dmm_mgr->dmm_lock);

	if (virtual_mapping_table != NULL) {
		for (i = 0; i < table_size; i +=
		     virtual_mapping_table[i].region_size) {
			curr_node = virtual_mapping_table + i;
			if (curr_node->reserved) {
			} else {
				freemem += (curr_node->region_size * PG_SIZE4K);
				if (curr_node->region_size > bigsize)
					bigsize = curr_node->region_size;
			}
		}
	}
	spin_unlock(&dmm_mgr->dmm_lock);
	printk(KERN_INFO "Total DSP VA FREE memory = %d Mbytes\n",
	       freemem / (1024 * 1024));
	printk(KERN_INFO "Total DSP VA USED memory= %d Mbytes \n",
	       (((table_size * PG_SIZE4K) - freemem)) / (1024 * 1024));
	printk(KERN_INFO "DSP VA - Biggest FREE block = %d Mbytes \n\n",
	       (bigsize * PG_SIZE4K / (1024 * 1024)));

	return 0;
}
#endif
