/*
 * drv.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * DRV Resource allocation module. Driver Object gets Created
 * at the time of Loading. It holds the List of Device Objects
 * in the system.
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

#ifndef DRV_
#define DRV_

#include <dspbridge/devdefs.h>

#include <linux/idr.h>

struct drv_object;


#define OMAP_GEM_BASE   0x107F8000
#define OMAP_DSP_SIZE   0x00720000

#define OMAP_DSP_MEM1_BASE 0x5C7F8000
#define OMAP_DSP_MEM1_SIZE 0x18000

#define OMAP_DSP_MEM2_BASE 0x5CE00000
#define OMAP_DSP_MEM2_SIZE 0x8000

#define OMAP_DSP_MEM3_BASE 0x5CF04000
#define OMAP_DSP_MEM3_SIZE 0x14000

#define OMAP_PER_CM_BASE 0x48005000
#define OMAP_PER_CM_SIZE 0x1000

#define OMAP_PER_PRM_BASE 0x48307000
#define OMAP_PER_PRM_SIZE 0x1000

#define OMAP_CORE_PRM_BASE 0x48306A00
#define OMAP_CORE_PRM_SIZE 0x1000

#define OMAP_DMMU_BASE 0x5D000000
#define OMAP_DMMU_SIZE 0x1000


struct node_res_object {
	void *node;
	s32 node_allocated;	
	s32 heap_allocated;	
	s32 streams_allocated;	
	int id;
};

struct bridge_dma_map_info {
	
	enum dma_data_direction dir;
	
	int num_pages;
	
	int sg_num;
	
	struct scatterlist *sg;
};

struct dmm_map_object {
	struct list_head link;
	u32 dsp_addr;
	u32 mpu_addr;
	u32 size;
	u32 num_usr_pgs;
	struct page **pages;
	struct bridge_dma_map_info dma_info;
};

struct dmm_rsv_object {
	struct list_head link;
	u32 dsp_reserved_addr;
};

struct strm_res_object {
	s32 stream_allocated;	
	void *stream;
	u32 num_bufs;
	u32 dir;
	int id;
};

enum gpp_proc_res_state {
	PROC_RES_ALLOCATED,
	PROC_RES_FREED
};

struct drv_data {
	char *base_img;
	s32 shm_size;
	int tc_wordswapon;
	void *drv_object;
	void *dev_object;
	void *mgr_object;
};

struct process_context {
	
	enum gpp_proc_res_state res_state;

	
	void *processor;

	
	struct idr *node_id;

	
	struct list_head dmm_map_list;
	spinlock_t dmm_map_lock;

	
	struct list_head dmm_rsv_list;
	spinlock_t dmm_rsv_lock;

	
	struct idr *stream_id;
};

extern int drv_create(struct drv_object **drv_obj);

extern int drv_destroy(struct drv_object *driver_obj);

extern u32 drv_get_first_dev_object(void);

extern u32 drv_get_first_dev_extension(void);

extern int drv_get_dev_object(u32 index,
				     struct drv_object *hdrv_obj,
				     struct dev_object **device_obj);

extern u32 drv_get_next_dev_object(u32 hdev_obj);

extern u32 drv_get_next_dev_extension(u32 dev_extension);

extern int drv_insert_dev_object(struct drv_object *driver_obj,
					struct dev_object *hdev_obj);

extern int drv_remove_dev_object(struct drv_object *driver_obj,
					struct dev_object *hdev_obj);

extern int drv_request_resources(u32 dw_context,
					u32 *dev_node_strg);

extern int drv_release_resources(u32 dw_context,
					struct drv_object *hdrv_obj);

int drv_request_bridge_res_dsp(void **phost_resources);

#ifdef CONFIG_TIDSPBRIDGE_RECOVERY
void bridge_recover_schedule(void);
#endif

extern void mem_ext_phys_pool_init(u32 pool_phys_base, u32 pool_size);

extern void mem_ext_phys_pool_release(void);

extern void *mem_alloc_phys_mem(u32 byte_size,
				u32 align_mask, u32 *physical_address);

extern void mem_free_phys_mem(void *virtual_address,
			      u32 physical_address, u32 byte_size);

#define MEM_LINEAR_ADDRESS(phy_addr, byte_size) phy_addr

#define MEM_UNMAP_LINEAR_ADDRESS(base_addr) {}

#endif 
