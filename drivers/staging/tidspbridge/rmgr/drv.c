/*
 * drv.c
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * DSP/BIOS Bridge resource allocation module.
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
#include <linux/list.h>

#include <dspbridge/host_os.h>

#include <dspbridge/dbdefs.h>

#include <dspbridge/drv.h>
#include <dspbridge/dev.h>

#include <dspbridge/node.h>
#include <dspbridge/proc.h>
#include <dspbridge/strm.h>
#include <dspbridge/nodepriv.h>
#include <dspbridge/dspchnl.h>
#include <dspbridge/resourcecleanup.h>

struct drv_object {
	struct list_head dev_list;
	struct list_head dev_node_string;
};

struct drv_ext {
	struct list_head link;
	char sz_string[MAXREGPATHLENGTH];
};

static bool ext_phys_mem_pool_enabled;
struct ext_phys_mem_pool {
	u32 phys_mem_base;
	u32 phys_mem_size;
	u32 virt_mem_base;
	u32 next_phys_alloc_ptr;
};
static struct ext_phys_mem_pool ext_mem_pool;

static int request_bridge_resources(struct cfg_hostres *res);



static int drv_proc_free_node_res(int id, void *p, void *data);

int drv_insert_node_res_element(void *hnode, void *node_resource,
				       void *process_ctxt)
{
	struct node_res_object **node_res_obj =
	    (struct node_res_object **)node_resource;
	struct process_context *ctxt = (struct process_context *)process_ctxt;
	int status = 0;
	int retval;

	*node_res_obj = kzalloc(sizeof(struct node_res_object), GFP_KERNEL);
	if (!*node_res_obj) {
		status = -ENOMEM;
		goto func_end;
	}

	(*node_res_obj)->node = hnode;
	retval = idr_get_new(ctxt->node_id, *node_res_obj,
						&(*node_res_obj)->id);
	if (retval == -EAGAIN) {
		if (!idr_pre_get(ctxt->node_id, GFP_KERNEL)) {
			pr_err("%s: OUT OF MEMORY\n", __func__);
			status = -ENOMEM;
			goto func_end;
		}

		retval = idr_get_new(ctxt->node_id, *node_res_obj,
						&(*node_res_obj)->id);
	}
	if (retval) {
		pr_err("%s: FAILED, IDR is FULL\n", __func__);
		status = -EFAULT;
	}
func_end:
	if (status)
		kfree(*node_res_obj);

	return status;
}

static int drv_proc_free_node_res(int id, void *p, void *data)
{
	struct process_context *ctxt = data;
	int status;
	struct node_res_object *node_res_obj = p;
	u32 node_state;

	if (node_res_obj->node_allocated) {
		node_state = node_get_state(node_res_obj->node);
		if (node_state <= NODE_DELETING) {
			if ((node_state == NODE_RUNNING) ||
			    (node_state == NODE_PAUSED) ||
			    (node_state == NODE_TERMINATING))
				node_terminate
				    (node_res_obj->node, &status);

			node_delete(node_res_obj, ctxt);
		}
	}

	return 0;
}

int drv_remove_all_dmm_res_elements(void *process_ctxt)
{
	struct process_context *ctxt = (struct process_context *)process_ctxt;
	int status = 0;
	struct dmm_map_object *temp_map, *map_obj;
	struct dmm_rsv_object *temp_rsv, *rsv_obj;

	
	list_for_each_entry_safe(map_obj, temp_map, &ctxt->dmm_map_list, link) {
		status = proc_un_map(ctxt->processor,
				     (void *)map_obj->dsp_addr, ctxt);
		if (status)
			pr_err("%s: proc_un_map failed!"
			       " status = 0x%xn", __func__, status);
	}

	
	list_for_each_entry_safe(rsv_obj, temp_rsv, &ctxt->dmm_rsv_list, link) {
		status = proc_un_reserve_memory(ctxt->processor, (void *)
						rsv_obj->dsp_reserved_addr,
						ctxt);
		if (status)
			pr_err("%s: proc_un_reserve_memory failed!"
			       " status = 0x%xn", __func__, status);
	}
	return status;
}

void drv_proc_node_update_status(void *node_resource, s32 status)
{
	struct node_res_object *node_res_obj =
	    (struct node_res_object *)node_resource;
	node_res_obj->node_allocated = status;
}

void drv_proc_node_update_heap_status(void *node_resource, s32 status)
{
	struct node_res_object *node_res_obj =
	    (struct node_res_object *)node_resource;
	node_res_obj->heap_allocated = status;
}

int drv_remove_all_node_res_elements(void *process_ctxt)
{
	struct process_context *ctxt = process_ctxt;

	idr_for_each(ctxt->node_id, drv_proc_free_node_res, ctxt);
	idr_destroy(ctxt->node_id);

	return 0;
}

int drv_proc_insert_strm_res_element(void *stream_obj,
					    void *strm_res, void *process_ctxt)
{
	struct strm_res_object **pstrm_res =
	    (struct strm_res_object **)strm_res;
	struct process_context *ctxt = (struct process_context *)process_ctxt;
	int status = 0;
	int retval;

	*pstrm_res = kzalloc(sizeof(struct strm_res_object), GFP_KERNEL);
	if (*pstrm_res == NULL) {
		status = -EFAULT;
		goto func_end;
	}

	(*pstrm_res)->stream = stream_obj;
	retval = idr_get_new(ctxt->stream_id, *pstrm_res,
						&(*pstrm_res)->id);
	if (retval == -EAGAIN) {
		if (!idr_pre_get(ctxt->stream_id, GFP_KERNEL)) {
			pr_err("%s: OUT OF MEMORY\n", __func__);
			status = -ENOMEM;
			goto func_end;
		}

		retval = idr_get_new(ctxt->stream_id, *pstrm_res,
						&(*pstrm_res)->id);
	}
	if (retval) {
		pr_err("%s: FAILED, IDR is FULL\n", __func__);
		status = -EPERM;
	}

func_end:
	return status;
}

static int drv_proc_free_strm_res(int id, void *p, void *process_ctxt)
{
	struct process_context *ctxt = process_ctxt;
	struct strm_res_object *strm_res = p;
	struct stream_info strm_info;
	struct dsp_streaminfo user;
	u8 **ap_buffer = NULL;
	u8 *buf_ptr;
	u32 ul_bytes;
	u32 dw_arg;
	s32 ul_buf_size;

	if (strm_res->num_bufs) {
		ap_buffer = kmalloc((strm_res->num_bufs *
				       sizeof(u8 *)), GFP_KERNEL);
		if (ap_buffer) {
			strm_free_buffer(strm_res,
						  ap_buffer,
						  strm_res->num_bufs,
						  ctxt);
			kfree(ap_buffer);
		}
	}
	strm_info.user_strm = &user;
	user.number_bufs_in_stream = 0;
	strm_get_info(strm_res->stream, &strm_info, sizeof(strm_info));
	while (user.number_bufs_in_stream--)
		strm_reclaim(strm_res->stream, &buf_ptr, &ul_bytes,
			     (u32 *) &ul_buf_size, &dw_arg);
	strm_close(strm_res, ctxt);
	return 0;
}

int drv_remove_all_strm_res_elements(void *process_ctxt)
{
	struct process_context *ctxt = process_ctxt;

	idr_for_each(ctxt->stream_id, drv_proc_free_strm_res, ctxt);
	idr_destroy(ctxt->stream_id);

	return 0;
}

int drv_proc_update_strm_res(u32 num_bufs, void *strm_resources)
{
	int status = 0;
	struct strm_res_object **strm_res =
	    (struct strm_res_object **)strm_resources;

	(*strm_res)->num_bufs = num_bufs;
	return status;
}


int drv_create(struct drv_object **drv_obj)
{
	int status = 0;
	struct drv_object *pdrv_object = NULL;
	struct drv_data *drv_datap = dev_get_drvdata(bridge);

	pdrv_object = kzalloc(sizeof(struct drv_object), GFP_KERNEL);
	if (pdrv_object) {
		
		INIT_LIST_HEAD(&pdrv_object->dev_list);
		INIT_LIST_HEAD(&pdrv_object->dev_node_string);
	} else {
		status = -ENOMEM;
	}
	
	if (!status) {
		if (drv_datap) {
			drv_datap->drv_object = (void *)pdrv_object;
		} else {
			status = -EPERM;
			pr_err("%s: Failed to store DRV object\n", __func__);
		}
	}

	if (!status) {
		*drv_obj = pdrv_object;
	} else {
		
		kfree(pdrv_object);
	}

	return status;
}

int drv_destroy(struct drv_object *driver_obj)
{
	int status = 0;
	struct drv_object *pdrv_object = (struct drv_object *)driver_obj;
	struct drv_data *drv_datap = dev_get_drvdata(bridge);

	kfree(pdrv_object);
	
	if (drv_datap) {
		drv_datap->drv_object = NULL;
	} else {
		status = -EPERM;
		pr_err("%s: Failed to store DRV object\n", __func__);
	}

	return status;
}

int drv_get_dev_object(u32 index, struct drv_object *hdrv_obj,
			      struct dev_object **device_obj)
{
	int status = 0;
	struct dev_object *dev_obj;
	u32 i;

	dev_obj = (struct dev_object *)drv_get_first_dev_object();
	for (i = 0; i < index; i++) {
		dev_obj =
		    (struct dev_object *)drv_get_next_dev_object((u32) dev_obj);
	}
	if (dev_obj) {
		*device_obj = (struct dev_object *)dev_obj;
	} else {
		*device_obj = NULL;
		status = -EPERM;
	}

	return status;
}

u32 drv_get_first_dev_object(void)
{
	u32 dw_dev_object = 0;
	struct drv_object *pdrv_obj;
	struct drv_data *drv_datap = dev_get_drvdata(bridge);

	if (drv_datap && drv_datap->drv_object) {
		pdrv_obj = drv_datap->drv_object;
		if (!list_empty(&pdrv_obj->dev_list))
			dw_dev_object = (u32) pdrv_obj->dev_list.next;
	} else {
		pr_err("%s: Failed to retrieve the object handle\n", __func__);
	}

	return dw_dev_object;
}

u32 drv_get_first_dev_extension(void)
{
	u32 dw_dev_extension = 0;
	struct drv_object *pdrv_obj;
	struct drv_data *drv_datap = dev_get_drvdata(bridge);

	if (drv_datap && drv_datap->drv_object) {
		pdrv_obj = drv_datap->drv_object;
		if (!list_empty(&pdrv_obj->dev_node_string)) {
			dw_dev_extension =
			    (u32) pdrv_obj->dev_node_string.next;
		}
	} else {
		pr_err("%s: Failed to retrieve the object handle\n", __func__);
	}

	return dw_dev_extension;
}

u32 drv_get_next_dev_object(u32 hdev_obj)
{
	u32 dw_next_dev_object = 0;
	struct drv_object *pdrv_obj;
	struct drv_data *drv_datap = dev_get_drvdata(bridge);
	struct list_head *curr;

	if (drv_datap && drv_datap->drv_object) {
		pdrv_obj = drv_datap->drv_object;
		if (!list_empty(&pdrv_obj->dev_list)) {
			curr = (struct list_head *)hdev_obj;
			if (list_is_last(curr, &pdrv_obj->dev_list))
				return 0;
			dw_next_dev_object = (u32) curr->next;
		}
	} else {
		pr_err("%s: Failed to retrieve the object handle\n", __func__);
	}

	return dw_next_dev_object;
}

u32 drv_get_next_dev_extension(u32 dev_extension)
{
	u32 dw_dev_extension = 0;
	struct drv_object *pdrv_obj;
	struct drv_data *drv_datap = dev_get_drvdata(bridge);
	struct list_head *curr;

	if (drv_datap && drv_datap->drv_object) {
		pdrv_obj = drv_datap->drv_object;
		if (!list_empty(&pdrv_obj->dev_node_string)) {
			curr = (struct list_head *)dev_extension;
			if (list_is_last(curr, &pdrv_obj->dev_node_string))
				return 0;
			dw_dev_extension = (u32) curr->next;
		}
	} else {
		pr_err("%s: Failed to retrieve the object handle\n", __func__);
	}

	return dw_dev_extension;
}

int drv_insert_dev_object(struct drv_object *driver_obj,
				 struct dev_object *hdev_obj)
{
	struct drv_object *pdrv_object = (struct drv_object *)driver_obj;

	list_add_tail((struct list_head *)hdev_obj, &pdrv_object->dev_list);

	return 0;
}

int drv_remove_dev_object(struct drv_object *driver_obj,
				 struct dev_object *hdev_obj)
{
	int status = -EPERM;
	struct drv_object *pdrv_object = (struct drv_object *)driver_obj;
	struct list_head *cur_elem;

	
	list_for_each(cur_elem, &pdrv_object->dev_list) {
		
		if ((struct dev_object *)cur_elem == hdev_obj) {
			list_del(cur_elem);
			status = 0;
			break;
		}
	}

	return status;
}

int drv_request_resources(u32 dw_context, u32 *dev_node_strg)
{
	int status = 0;
	struct drv_object *pdrv_object;
	struct drv_ext *pszdev_node;
	struct drv_data *drv_datap = dev_get_drvdata(bridge);


	if (!drv_datap || !drv_datap->drv_object)
		status = -ENODATA;
	else
		pdrv_object = drv_datap->drv_object;

	if (!status) {
		pszdev_node = kzalloc(sizeof(struct drv_ext), GFP_KERNEL);
		if (pszdev_node) {
			strncpy(pszdev_node->sz_string,
				(char *)dw_context, MAXREGPATHLENGTH - 1);
			pszdev_node->sz_string[MAXREGPATHLENGTH - 1] = '\0';
			
			*dev_node_strg = (u32) pszdev_node->sz_string;
			list_add_tail(&pszdev_node->link,
					&pdrv_object->dev_node_string);
		} else {
			status = -ENOMEM;
			*dev_node_strg = 0;
		}
	} else {
		dev_dbg(bridge, "%s: Failed to get Driver Object from Registry",
			__func__);
		*dev_node_strg = 0;
	}

	return status;
}

int drv_release_resources(u32 dw_context, struct drv_object *hdrv_obj)
{
	int status = 0;
	struct drv_ext *pszdev_node;

	for (pszdev_node = (struct drv_ext *)drv_get_first_dev_extension();
	     pszdev_node != NULL; pszdev_node = (struct drv_ext *)
	     drv_get_next_dev_extension((u32) pszdev_node)) {
		if ((u32) pszdev_node == dw_context) {
			
			
			list_del(&pszdev_node->link);
			kfree(pszdev_node);
			break;
		}
	}
	return status;
}

static int request_bridge_resources(struct cfg_hostres *res)
{
	struct cfg_hostres *host_res = res;

	
	host_res->num_mem_windows = 2;

	
	dev_dbg(bridge, "mem_base[0] 0x%x\n", host_res->mem_base[0]);
	dev_dbg(bridge, "mem_base[3] 0x%x\n", host_res->mem_base[3]);
	dev_dbg(bridge, "dmmu_base %p\n", host_res->dmmu_base);

	

	
	host_res->birq_registers = 0;
	host_res->birq_attrib = 0;
	host_res->offset_for_monitor = 0;
	host_res->chnl_offset = 0;
	
	host_res->num_chnls = CHNL_MAXCHANNELS;
	host_res->chnl_buf_size = 0x400;

	return 0;
}

int drv_request_bridge_res_dsp(void **phost_resources)
{
	int status = 0;
	struct cfg_hostres *host_res;
	u32 dw_buff_size;
	u32 dma_addr;
	u32 shm_size;
	struct drv_data *drv_datap = dev_get_drvdata(bridge);

	dw_buff_size = sizeof(struct cfg_hostres);

	host_res = kzalloc(dw_buff_size, GFP_KERNEL);

	if (host_res != NULL) {
		request_bridge_resources(host_res);
		
		host_res->num_mem_windows = 4;

		host_res->mem_base[0] = 0;
		host_res->mem_base[2] = (u32) ioremap(OMAP_DSP_MEM1_BASE,
							 OMAP_DSP_MEM1_SIZE);
		host_res->mem_base[3] = (u32) ioremap(OMAP_DSP_MEM2_BASE,
							 OMAP_DSP_MEM2_SIZE);
		host_res->mem_base[4] = (u32) ioremap(OMAP_DSP_MEM3_BASE,
							 OMAP_DSP_MEM3_SIZE);
		host_res->per_base = ioremap(OMAP_PER_CM_BASE,
						OMAP_PER_CM_SIZE);
		host_res->per_pm_base = (u32) ioremap(OMAP_PER_PRM_BASE,
							 OMAP_PER_PRM_SIZE);
		host_res->core_pm_base = (u32) ioremap(OMAP_CORE_PRM_BASE,
							  OMAP_CORE_PRM_SIZE);
		host_res->dmmu_base = ioremap(OMAP_DMMU_BASE,
						 OMAP_DMMU_SIZE);

		dev_dbg(bridge, "mem_base[0] 0x%x\n",
			host_res->mem_base[0]);
		dev_dbg(bridge, "mem_base[1] 0x%x\n",
			host_res->mem_base[1]);
		dev_dbg(bridge, "mem_base[2] 0x%x\n",
			host_res->mem_base[2]);
		dev_dbg(bridge, "mem_base[3] 0x%x\n",
			host_res->mem_base[3]);
		dev_dbg(bridge, "mem_base[4] 0x%x\n",
			host_res->mem_base[4]);
		dev_dbg(bridge, "dmmu_base %p\n", host_res->dmmu_base);

		shm_size = drv_datap->shm_size;
		if (shm_size >= 0x10000) {
			host_res->mem_base[1] =
			    (u32) mem_alloc_phys_mem(shm_size, 0x100000,
						     &dma_addr);
			if (host_res->mem_base[1] == 0) {
				status = -ENOMEM;
				pr_err("shm reservation Failed\n");
			} else {
				host_res->mem_length[1] = shm_size;
				host_res->mem_phys[1] = dma_addr;

				dev_dbg(bridge, "%s: Bridge shm address 0x%x "
					"dma_addr %x size %x\n", __func__,
					host_res->mem_base[1],
					dma_addr, shm_size);
			}
		}
		if (!status) {
			
			host_res->birq_registers = 0;
			host_res->birq_attrib = 0;
			host_res->offset_for_monitor = 0;
			host_res->chnl_offset = 0;
			
			host_res->num_chnls = CHNL_MAXCHANNELS;
			host_res->chnl_buf_size = 0x400;
			dw_buff_size = sizeof(struct cfg_hostres);
		}
		*phost_resources = host_res;
	}
	
	return status;
}

void mem_ext_phys_pool_init(u32 pool_phys_base, u32 pool_size)
{
	u32 pool_virt_base;

	
	pool_virt_base = (u32) ioremap(pool_phys_base, pool_size);

	if ((void **)pool_virt_base == NULL) {
		pr_err("%s: external physical memory map failed\n", __func__);
		ext_phys_mem_pool_enabled = false;
	} else {
		ext_mem_pool.phys_mem_base = pool_phys_base;
		ext_mem_pool.phys_mem_size = pool_size;
		ext_mem_pool.virt_mem_base = pool_virt_base;
		ext_mem_pool.next_phys_alloc_ptr = pool_phys_base;
		ext_phys_mem_pool_enabled = true;
	}
}

void mem_ext_phys_pool_release(void)
{
	if (ext_phys_mem_pool_enabled) {
		iounmap((void *)(ext_mem_pool.virt_mem_base));
		ext_phys_mem_pool_enabled = false;
	}
}


static void *mem_ext_phys_mem_alloc(u32 bytes, u32 align, u32 * phys_addr)
{
	u32 new_alloc_ptr;
	u32 offset;
	u32 virt_addr;

	if (align == 0)
		align = 1;

	if (bytes > ((ext_mem_pool.phys_mem_base + ext_mem_pool.phys_mem_size)
		     - ext_mem_pool.next_phys_alloc_ptr)) {
		phys_addr = NULL;
		return NULL;
	} else {
		offset = (ext_mem_pool.next_phys_alloc_ptr & (align - 1));
		if (offset == 0)
			new_alloc_ptr = ext_mem_pool.next_phys_alloc_ptr;
		else
			new_alloc_ptr = (ext_mem_pool.next_phys_alloc_ptr) +
			    (align - offset);
		if ((new_alloc_ptr + bytes) <=
		    (ext_mem_pool.phys_mem_base + ext_mem_pool.phys_mem_size)) {
			
			*phys_addr = new_alloc_ptr;
			ext_mem_pool.next_phys_alloc_ptr =
			    new_alloc_ptr + bytes;
			virt_addr =
			    ext_mem_pool.virt_mem_base + (new_alloc_ptr -
							  ext_mem_pool.
							  phys_mem_base);
			return (void *)virt_addr;
		} else {
			*phys_addr = 0;
			return NULL;
		}
	}
}

void *mem_alloc_phys_mem(u32 byte_size, u32 align_mask,
				u32 *physical_address)
{
	void *va_mem = NULL;
	dma_addr_t pa_mem;

	if (byte_size > 0) {
		if (ext_phys_mem_pool_enabled) {
			va_mem = mem_ext_phys_mem_alloc(byte_size, align_mask,
							(u32 *) &pa_mem);
		} else
			va_mem = dma_alloc_coherent(NULL, byte_size, &pa_mem,
								GFP_KERNEL);
		if (va_mem == NULL)
			*physical_address = 0;
		else
			*physical_address = pa_mem;
	}
	return va_mem;
}

void mem_free_phys_mem(void *virtual_address, u32 physical_address,
		       u32 byte_size)
{
	if (!ext_phys_mem_pool_enabled)
		dma_free_coherent(NULL, byte_size, virtual_address,
				  physical_address);
}
