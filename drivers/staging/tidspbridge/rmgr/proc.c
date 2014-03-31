/*
 * proc.c
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Processor interface at the driver level.
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
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <dspbridge/host_os.h>

#include <dspbridge/dbdefs.h>

#include <dspbridge/ntfy.h>
#include <dspbridge/sync.h>
#include <dspbridge/dspdefs.h>
#include <dspbridge/dspdeh.h>
#include <dspbridge/cod.h>
#include <dspbridge/dev.h>
#include <dspbridge/procpriv.h>
#include <dspbridge/dmm.h>

#include <dspbridge/mgr.h>
#include <dspbridge/node.h>
#include <dspbridge/nldr.h>
#include <dspbridge/rmm.h>

#include <dspbridge/dbdcd.h>
#include <dspbridge/msg.h>
#include <dspbridge/dspioctl.h>
#include <dspbridge/drv.h>

#include <dspbridge/proc.h>
#include <dspbridge/pwr.h>

#include <dspbridge/resourcecleanup.h>
#define MAXCMDLINELEN       255
#define PROC_ENVPROCID      "PROC_ID=%d"
#define MAXPROCIDLEN	(8 + 5)
#define PROC_DFLT_TIMEOUT   10000	
#define PWR_TIMEOUT	 500	
#define EXTEND	      "_EXT_END"	

#define DSP_CACHE_LINE 128

#define BUFMODE_MASK	(3 << 14)

#define RBUF		0x4000		
#define WBUF		0x8000		

extern struct device *bridge;


struct proc_object {
	struct list_head link;	
	struct dev_object *dev_obj;	
	u32 process;		
	struct mgr_object *mgr_obj;	
	u32 attach_count;	
	u32 processor_id;	
	u32 timeout;		
	enum dsp_procstate proc_state;	
	u32 unit;		
	bool is_already_attached;	
	struct ntfy_object *ntfy_obj;	
	
	struct bridge_dev_context *bridge_context;
	
	struct bridge_drv_interface *intf_fxns;
	char *last_coff;
	struct list_head proc_list;
};

DEFINE_MUTEX(proc_lock);	

static int proc_monitor(struct proc_object *proc_obj);
static s32 get_envp_count(char **envp);
static char **prepend_envp(char **new_envp, char **envp, s32 envp_elems,
			   s32 cnew_envp, char *sz_var);

static struct dmm_map_object *add_mapping_info(struct process_context *pr_ctxt,
				u32 mpu_addr, u32 dsp_addr, u32 size)
{
	struct dmm_map_object *map_obj;

	u32 num_usr_pgs = size / PG_SIZE4K;

	pr_debug("%s: adding map info: mpu_addr 0x%x virt 0x%x size 0x%x\n",
						__func__, mpu_addr,
						dsp_addr, size);

	map_obj = kzalloc(sizeof(struct dmm_map_object), GFP_KERNEL);
	if (!map_obj) {
		pr_err("%s: kzalloc failed\n", __func__);
		return NULL;
	}
	INIT_LIST_HEAD(&map_obj->link);

	map_obj->pages = kcalloc(num_usr_pgs, sizeof(struct page *),
							GFP_KERNEL);
	if (!map_obj->pages) {
		pr_err("%s: kzalloc failed\n", __func__);
		kfree(map_obj);
		return NULL;
	}

	map_obj->mpu_addr = mpu_addr;
	map_obj->dsp_addr = dsp_addr;
	map_obj->size = size;
	map_obj->num_usr_pgs = num_usr_pgs;

	spin_lock(&pr_ctxt->dmm_map_lock);
	list_add(&map_obj->link, &pr_ctxt->dmm_map_list);
	spin_unlock(&pr_ctxt->dmm_map_lock);

	return map_obj;
}

static int match_exact_map_obj(struct dmm_map_object *map_obj,
					u32 dsp_addr, u32 size)
{
	if (map_obj->dsp_addr == dsp_addr && map_obj->size != size)
		pr_err("%s: addr match (0x%x), size don't (0x%x != 0x%x)\n",
				__func__, dsp_addr, map_obj->size, size);

	return map_obj->dsp_addr == dsp_addr &&
		map_obj->size == size;
}

static void remove_mapping_information(struct process_context *pr_ctxt,
						u32 dsp_addr, u32 size)
{
	struct dmm_map_object *map_obj;

	pr_debug("%s: looking for virt 0x%x size 0x%x\n", __func__,
							dsp_addr, size);

	spin_lock(&pr_ctxt->dmm_map_lock);
	list_for_each_entry(map_obj, &pr_ctxt->dmm_map_list, link) {
		pr_debug("%s: candidate: mpu_addr 0x%x virt 0x%x size 0x%x\n",
							__func__,
							map_obj->mpu_addr,
							map_obj->dsp_addr,
							map_obj->size);

		if (match_exact_map_obj(map_obj, dsp_addr, size)) {
			pr_debug("%s: match, deleting map info\n", __func__);
			list_del(&map_obj->link);
			kfree(map_obj->dma_info.sg);
			kfree(map_obj->pages);
			kfree(map_obj);
			goto out;
		}
		pr_debug("%s: candidate didn't match\n", __func__);
	}

	pr_err("%s: failed to find given map info\n", __func__);
out:
	spin_unlock(&pr_ctxt->dmm_map_lock);
}

static int match_containing_map_obj(struct dmm_map_object *map_obj,
					u32 mpu_addr, u32 size)
{
	u32 map_obj_end = map_obj->mpu_addr + map_obj->size;

	return mpu_addr >= map_obj->mpu_addr &&
		mpu_addr + size <= map_obj_end;
}

static struct dmm_map_object *find_containing_mapping(
				struct process_context *pr_ctxt,
				u32 mpu_addr, u32 size)
{
	struct dmm_map_object *map_obj;
	pr_debug("%s: looking for mpu_addr 0x%x size 0x%x\n", __func__,
						mpu_addr, size);

	spin_lock(&pr_ctxt->dmm_map_lock);
	list_for_each_entry(map_obj, &pr_ctxt->dmm_map_list, link) {
		pr_debug("%s: candidate: mpu_addr 0x%x virt 0x%x size 0x%x\n",
						__func__,
						map_obj->mpu_addr,
						map_obj->dsp_addr,
						map_obj->size);
		if (match_containing_map_obj(map_obj, mpu_addr, size)) {
			pr_debug("%s: match!\n", __func__);
			goto out;
		}

		pr_debug("%s: no match!\n", __func__);
	}

	map_obj = NULL;
out:
	spin_unlock(&pr_ctxt->dmm_map_lock);
	return map_obj;
}

static int find_first_page_in_cache(struct dmm_map_object *map_obj,
					unsigned long mpu_addr)
{
	u32 mapped_base_page = map_obj->mpu_addr >> PAGE_SHIFT;
	u32 requested_base_page = mpu_addr >> PAGE_SHIFT;
	int pg_index = requested_base_page - mapped_base_page;

	if (pg_index < 0 || pg_index >= map_obj->num_usr_pgs) {
		pr_err("%s: failed (got %d)\n", __func__, pg_index);
		return -1;
	}

	pr_debug("%s: first page is %d\n", __func__, pg_index);
	return pg_index;
}

static inline struct page *get_mapping_page(struct dmm_map_object *map_obj,
								int pg_i)
{
	pr_debug("%s: looking for pg_i %d, num_usr_pgs: %d\n", __func__,
					pg_i, map_obj->num_usr_pgs);

	if (pg_i < 0 || pg_i >= map_obj->num_usr_pgs) {
		pr_err("%s: requested pg_i %d is out of mapped range\n",
				__func__, pg_i);
		return NULL;
	}

	return map_obj->pages[pg_i];
}

int
proc_attach(u32 processor_id,
	    const struct dsp_processorattrin *attr_in,
	    void **ph_processor, struct process_context *pr_ctxt)
{
	int status = 0;
	struct dev_object *hdev_obj;
	struct proc_object *p_proc_object = NULL;
	struct mgr_object *hmgr_obj = NULL;
	struct drv_object *hdrv_obj = NULL;
	struct drv_data *drv_datap = dev_get_drvdata(bridge);
	u8 dev_type;

	if (pr_ctxt->processor) {
		*ph_processor = pr_ctxt->processor;
		return status;
	}

	
	if (!drv_datap || !drv_datap->drv_object || !drv_datap->mgr_object) {
		status = -ENODATA;
		pr_err("%s: Failed to get object handles\n", __func__);
	} else {
		hdrv_obj = drv_datap->drv_object;
		hmgr_obj = drv_datap->mgr_object;
	}

	if (!status) {
		
		status = drv_get_dev_object(processor_id, hdrv_obj, &hdev_obj);
	}
	if (!status)
		status = dev_get_dev_type(hdev_obj, &dev_type);

	if (status)
		goto func_end;

	
	p_proc_object = kzalloc(sizeof(struct proc_object), GFP_KERNEL);
	
	if (p_proc_object == NULL) {
		status = -ENOMEM;
		goto func_end;
	}
	p_proc_object->dev_obj = hdev_obj;
	p_proc_object->mgr_obj = hmgr_obj;
	p_proc_object->processor_id = dev_type;
	
	p_proc_object->process = current->tgid;

	INIT_LIST_HEAD(&p_proc_object->proc_list);

	if (attr_in)
		p_proc_object->timeout = attr_in->timeout;
	else
		p_proc_object->timeout = PROC_DFLT_TIMEOUT;

	status = dev_get_intf_fxns(hdev_obj, &p_proc_object->intf_fxns);
	if (!status) {
		status = dev_get_bridge_context(hdev_obj,
					     &p_proc_object->bridge_context);
		if (status)
			kfree(p_proc_object);
	} else
		kfree(p_proc_object);

	if (status)
		goto func_end;

	
	p_proc_object->ntfy_obj = kmalloc(sizeof(struct ntfy_object),
							GFP_KERNEL);
	if (p_proc_object->ntfy_obj)
		ntfy_init(p_proc_object->ntfy_obj);
	else
		status = -ENOMEM;

	if (!status) {
		status = dev_insert_proc_object(p_proc_object->dev_obj,
						(u32) p_proc_object,
						&p_proc_object->
						is_already_attached);
		if (!status) {
			if (p_proc_object->is_already_attached)
				status = 0;
		} else {
			if (p_proc_object->ntfy_obj) {
				ntfy_delete(p_proc_object->ntfy_obj);
				kfree(p_proc_object->ntfy_obj);
			}

			kfree(p_proc_object);
		}
		if (!status) {
			*ph_processor = (void *)p_proc_object;
			pr_ctxt->processor = *ph_processor;
			(void)proc_notify_clients(p_proc_object,
						  DSP_PROCESSORATTACH);
		}
	} else {
		
		kfree(p_proc_object);
	}
func_end:
	return status;
}

static int get_exec_file(struct cfg_devnode *dev_node_obj,
				struct dev_object *hdev_obj,
				u32 size, char *exec_file)
{
	u8 dev_type;
	s32 len;
	struct drv_data *drv_datap = dev_get_drvdata(bridge);

	dev_get_dev_type(hdev_obj, (u8 *) &dev_type);

	if (!exec_file)
		return -EFAULT;

	if (dev_type == DSP_UNIT) {
		if (!drv_datap || !drv_datap->base_img)
			return -EFAULT;

		if (strlen(drv_datap->base_img) > size)
			return -EINVAL;

		strcpy(exec_file, drv_datap->base_img);
	} else if (dev_type == IVA_UNIT && iva_img) {
		len = strlen(iva_img);
		strncpy(exec_file, iva_img, len + 1);
	} else {
		return -ENOENT;
	}

	return 0;
}

int proc_auto_start(struct cfg_devnode *dev_node_obj,
			   struct dev_object *hdev_obj)
{
	int status = -EPERM;
	struct proc_object *p_proc_object;
	char sz_exec_file[MAXCMDLINELEN];
	char *argv[2];
	struct mgr_object *hmgr_obj = NULL;
	struct drv_data *drv_datap = dev_get_drvdata(bridge);
	u8 dev_type;

	
	if (!drv_datap || !drv_datap->mgr_object) {
		status = -ENODATA;
		pr_err("%s: Failed to retrieve the object handle\n", __func__);
		goto func_end;
	} else {
		hmgr_obj = drv_datap->mgr_object;
	}

	p_proc_object = kzalloc(sizeof(struct proc_object), GFP_KERNEL);
	if (p_proc_object == NULL) {
		status = -ENOMEM;
		goto func_end;
	}
	p_proc_object->dev_obj = hdev_obj;
	p_proc_object->mgr_obj = hmgr_obj;
	status = dev_get_intf_fxns(hdev_obj, &p_proc_object->intf_fxns);
	if (!status)
		status = dev_get_bridge_context(hdev_obj,
					     &p_proc_object->bridge_context);
	if (status)
		goto func_cont;

	
	status = proc_stop(p_proc_object);

	if (status)
		goto func_cont;

	
	dev_get_dev_type(hdev_obj, (u8 *) &dev_type);
	p_proc_object->processor_id = dev_type;
	status = get_exec_file(dev_node_obj, hdev_obj, sizeof(sz_exec_file),
			       sz_exec_file);
	if (!status) {
		argv[0] = sz_exec_file;
		argv[1] = NULL;
		
		status = proc_load(p_proc_object, 1, (const char **)argv, NULL);
		if (!status)
			status = proc_start(p_proc_object);
	}
	kfree(p_proc_object->last_coff);
	p_proc_object->last_coff = NULL;
func_cont:
	kfree(p_proc_object);
func_end:
	return status;
}

int proc_ctrl(void *hprocessor, u32 dw_cmd, struct dsp_cbdata * arg)
{
	int status = 0;
	struct proc_object *p_proc_object = hprocessor;
	u32 timeout = 0;

	if (p_proc_object) {
		
		if (dw_cmd == BRDIOCTL_DEEPSLEEP) {
			timeout = arg->cb_data;
			status = pwr_sleep_dsp(PWR_DEEPSLEEP, timeout);
		}
		
		else if (dw_cmd == BRDIOCTL_EMERGENCYSLEEP) {
			timeout = arg->cb_data;
			status = pwr_sleep_dsp(PWR_EMERGENCYDEEPSLEEP, timeout);
		} else if (dw_cmd == PWR_DEEPSLEEP) {
			
			status = pwr_sleep_dsp(PWR_DEEPSLEEP, timeout);
		}
		
		else if (dw_cmd == BRDIOCTL_WAKEUP) {
			timeout = arg->cb_data;
			status = pwr_wake_dsp(timeout);
		} else if (dw_cmd == PWR_WAKEUP) {
			
			status = pwr_wake_dsp(timeout);
		} else
		    if (!((*p_proc_object->intf_fxns->dev_cntrl)
				      (p_proc_object->bridge_context, dw_cmd,
				       arg))) {
			status = 0;
		} else {
			status = -EPERM;
		}
	} else {
		status = -EFAULT;
	}

	return status;
}

int proc_detach(struct process_context *pr_ctxt)
{
	int status = 0;
	struct proc_object *p_proc_object = NULL;

	p_proc_object = (struct proc_object *)pr_ctxt->processor;

	if (p_proc_object) {
		
		ntfy_notify(p_proc_object->ntfy_obj, DSP_PROCESSORDETACH);
		
		if (p_proc_object->ntfy_obj) {
			ntfy_delete(p_proc_object->ntfy_obj);
			kfree(p_proc_object->ntfy_obj);
		}

		kfree(p_proc_object->last_coff);
		p_proc_object->last_coff = NULL;
		
		(void)dev_remove_proc_object(p_proc_object->dev_obj,
					     (u32) p_proc_object);
		
		kfree(p_proc_object);
		pr_ctxt->processor = NULL;
	} else {
		status = -EFAULT;
	}

	return status;
}

int proc_enum_nodes(void *hprocessor, void **node_tab,
			   u32 node_tab_size, u32 *pu_num_nodes,
			   u32 *pu_allocated)
{
	int status = -EPERM;
	struct proc_object *p_proc_object = (struct proc_object *)hprocessor;
	struct node_mgr *hnode_mgr = NULL;

	if (p_proc_object) {
		if (!(dev_get_node_manager(p_proc_object->dev_obj,
						       &hnode_mgr))) {
			if (hnode_mgr) {
				status = node_enum_nodes(hnode_mgr, node_tab,
							 node_tab_size,
							 pu_num_nodes,
							 pu_allocated);
			}
		}
	} else {
		status = -EFAULT;
	}

	return status;
}

static int build_dma_sg(struct dmm_map_object *map_obj, unsigned long start,
						ssize_t len, int pg_i)
{
	struct page *page;
	unsigned long offset;
	ssize_t rest;
	int ret = 0, i = 0;
	struct scatterlist *sg = map_obj->dma_info.sg;

	while (len) {
		page = get_mapping_page(map_obj, pg_i);
		if (!page) {
			pr_err("%s: no page for %08lx\n", __func__, start);
			ret = -EINVAL;
			goto out;
		} else if (IS_ERR(page)) {
			pr_err("%s: err page for %08lx(%lu)\n", __func__, start,
			       PTR_ERR(page));
			ret = PTR_ERR(page);
			goto out;
		}

		offset = start & ~PAGE_MASK;
		rest = min_t(ssize_t, PAGE_SIZE - offset, len);

		sg_set_page(&sg[i], page, rest, offset);

		len -= rest;
		start += rest;
		pg_i++, i++;
	}

	if (i != map_obj->dma_info.num_pages) {
		pr_err("%s: bad number of sg iterations\n", __func__);
		ret = -EFAULT;
		goto out;
	}

out:
	return ret;
}

static int memory_regain_ownership(struct dmm_map_object *map_obj,
		unsigned long start, ssize_t len, enum dma_data_direction dir)
{
	int ret = 0;
	unsigned long first_data_page = start >> PAGE_SHIFT;
	unsigned long last_data_page = ((u32)(start + len - 1) >> PAGE_SHIFT);
	
	unsigned long num_pages = last_data_page - first_data_page + 1;
	struct bridge_dma_map_info *dma_info = &map_obj->dma_info;

	if (!dma_info->sg)
		goto out;

	if (dma_info->dir != dir || dma_info->num_pages != num_pages) {
		pr_err("%s: dma info doesn't match given params\n", __func__);
		return -EINVAL;
	}

	dma_unmap_sg(bridge, dma_info->sg, num_pages, dma_info->dir);

	pr_debug("%s: dma_map_sg unmapped\n", __func__);

	kfree(dma_info->sg);

	map_obj->dma_info.sg = NULL;

out:
	return ret;
}

static int memory_give_ownership(struct dmm_map_object *map_obj,
		unsigned long start, ssize_t len, enum dma_data_direction dir)
{
	int pg_i, ret, sg_num;
	struct scatterlist *sg;
	unsigned long first_data_page = start >> PAGE_SHIFT;
	unsigned long last_data_page = ((u32)(start + len - 1) >> PAGE_SHIFT);
	
	unsigned long num_pages = last_data_page - first_data_page + 1;

	pg_i = find_first_page_in_cache(map_obj, start);
	if (pg_i < 0) {
		pr_err("%s: failed to find first page in cache\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	sg = kcalloc(num_pages, sizeof(*sg), GFP_KERNEL);
	if (!sg) {
		pr_err("%s: kcalloc failed\n", __func__);
		ret = -ENOMEM;
		goto out;
	}

	sg_init_table(sg, num_pages);

	
	
	kfree(map_obj->dma_info.sg);

	map_obj->dma_info.sg = sg;
	map_obj->dma_info.dir = dir;
	map_obj->dma_info.num_pages = num_pages;

	ret = build_dma_sg(map_obj, start, len, pg_i);
	if (ret)
		goto kfree_sg;

	sg_num = dma_map_sg(bridge, sg, num_pages, dir);
	if (sg_num < 1) {
		pr_err("%s: dma_map_sg failed: %d\n", __func__, sg_num);
		ret = -EFAULT;
		goto kfree_sg;
	}

	pr_debug("%s: dma_map_sg mapped %d elements\n", __func__, sg_num);
	map_obj->dma_info.sg_num = sg_num;

	return 0;

kfree_sg:
	kfree(sg);
	map_obj->dma_info.sg = NULL;
out:
	return ret;
}

int proc_begin_dma(void *hprocessor, void *pmpu_addr, u32 ul_size,
				enum dma_data_direction dir)
{
	
	int status = 0;
	struct process_context *pr_ctxt = (struct process_context *) hprocessor;
	struct dmm_map_object *map_obj;

	if (!pr_ctxt) {
		status = -EFAULT;
		goto err_out;
	}

	pr_debug("%s: addr 0x%x, size 0x%x, type %d\n", __func__,
							(u32)pmpu_addr,
							ul_size, dir);

	mutex_lock(&proc_lock);

	
	map_obj = find_containing_mapping(pr_ctxt, (u32) pmpu_addr, ul_size);
	if (!map_obj) {
		pr_err("%s: find_containing_mapping failed\n", __func__);
		status = -EFAULT;
		goto no_map;
	}

	if (memory_give_ownership(map_obj, (u32) pmpu_addr, ul_size, dir)) {
		pr_err("%s: InValid address parameters %p %x\n",
			       __func__, pmpu_addr, ul_size);
		status = -EFAULT;
	}

no_map:
	mutex_unlock(&proc_lock);
err_out:

	return status;
}

int proc_end_dma(void *hprocessor, void *pmpu_addr, u32 ul_size,
			enum dma_data_direction dir)
{
	
	int status = 0;
	struct process_context *pr_ctxt = (struct process_context *) hprocessor;
	struct dmm_map_object *map_obj;

	if (!pr_ctxt) {
		status = -EFAULT;
		goto err_out;
	}

	pr_debug("%s: addr 0x%x, size 0x%x, type %d\n", __func__,
							(u32)pmpu_addr,
							ul_size, dir);

	mutex_lock(&proc_lock);

	
	map_obj = find_containing_mapping(pr_ctxt, (u32) pmpu_addr, ul_size);
	if (!map_obj) {
		pr_err("%s: find_containing_mapping failed\n", __func__);
		status = -EFAULT;
		goto no_map;
	}

	if (memory_regain_ownership(map_obj, (u32) pmpu_addr, ul_size, dir)) {
		pr_err("%s: InValid address parameters %p %x\n",
		       __func__, pmpu_addr, ul_size);
		status = -EFAULT;
	}

no_map:
	mutex_unlock(&proc_lock);
err_out:
	return status;
}

int proc_flush_memory(void *hprocessor, void *pmpu_addr,
			     u32 ul_size, u32 ul_flags)
{
	enum dma_data_direction dir = DMA_BIDIRECTIONAL;

	return proc_begin_dma(hprocessor, pmpu_addr, ul_size, dir);
}

int proc_invalidate_memory(void *hprocessor, void *pmpu_addr, u32 size)
{
	enum dma_data_direction dir = DMA_FROM_DEVICE;

	return proc_begin_dma(hprocessor, pmpu_addr, size, dir);
}

int proc_get_resource_info(void *hprocessor, u32 resource_type,
				  struct dsp_resourceinfo *resource_info,
				  u32 resource_info_size)
{
	int status = -EPERM;
	struct proc_object *p_proc_object = (struct proc_object *)hprocessor;
	struct node_mgr *hnode_mgr = NULL;
	struct nldr_object *nldr_obj = NULL;
	struct rmm_target_obj *rmm = NULL;
	struct io_mgr *hio_mgr = NULL;	

	if (!p_proc_object) {
		status = -EFAULT;
		goto func_end;
	}
	switch (resource_type) {
	case DSP_RESOURCE_DYNDARAM:
	case DSP_RESOURCE_DYNSARAM:
	case DSP_RESOURCE_DYNEXTERNAL:
	case DSP_RESOURCE_DYNSRAM:
		status = dev_get_node_manager(p_proc_object->dev_obj,
					      &hnode_mgr);
		if (!hnode_mgr) {
			status = -EFAULT;
			goto func_end;
		}

		status = node_get_nldr_obj(hnode_mgr, &nldr_obj);
		if (!status) {
			status = nldr_get_rmm_manager(nldr_obj, &rmm);
			if (rmm) {
				if (!rmm_stat(rmm,
					      (enum dsp_memtype)resource_type,
					      (struct dsp_memstat *)
					      &(resource_info->result.
						mem_stat)))
					status = -EINVAL;
			} else {
				status = -EFAULT;
			}
		}
		break;
	case DSP_RESOURCE_PROCLOAD:
		status = dev_get_io_mgr(p_proc_object->dev_obj, &hio_mgr);
		if (hio_mgr)
			status =
			    p_proc_object->intf_fxns->
			    io_get_proc_load(hio_mgr,
						 (struct dsp_procloadstat *)
						 &(resource_info->result.
						   proc_load_stat));
		else
			status = -EFAULT;
		break;
	default:
		status = -EPERM;
		break;
	}
func_end:
	return status;
}

int proc_get_dev_object(void *hprocessor,
			       struct dev_object **device_obj)
{
	int status = -EPERM;
	struct proc_object *p_proc_object = (struct proc_object *)hprocessor;

	if (p_proc_object) {
		*device_obj = p_proc_object->dev_obj;
		status = 0;
	} else {
		*device_obj = NULL;
		status = -EFAULT;
	}

	return status;
}

int proc_get_state(void *hprocessor,
			  struct dsp_processorstate *proc_state_obj,
			  u32 state_info_size)
{
	int status = 0;
	struct proc_object *p_proc_object = (struct proc_object *)hprocessor;
	int brd_status;

	if (p_proc_object) {
		
		status = (*p_proc_object->intf_fxns->brd_status)
		    (p_proc_object->bridge_context, &brd_status);
		if (!status) {
			switch (brd_status) {
			case BRD_STOPPED:
				proc_state_obj->proc_state = PROC_STOPPED;
				break;
			case BRD_SLEEP_TRANSITION:
			case BRD_DSP_HIBERNATION:
				
			case BRD_RUNNING:
				proc_state_obj->proc_state = PROC_RUNNING;
				break;
			case BRD_LOADED:
				proc_state_obj->proc_state = PROC_LOADED;
				break;
			case BRD_ERROR:
				proc_state_obj->proc_state = PROC_ERROR;
				break;
			default:
				proc_state_obj->proc_state = 0xFF;
				status = -EPERM;
				break;
			}
		}
	} else {
		status = -EFAULT;
	}
	dev_dbg(bridge, "%s, results: status: 0x%x proc_state_obj: 0x%x\n",
		__func__, status, proc_state_obj->proc_state);
	return status;
}

int proc_get_trace(void *hprocessor, u8 * pbuf, u32 max_size)
{
	int status;
	status = -ENOSYS;
	return status;
}

int proc_load(void *hprocessor, const s32 argc_index,
		     const char **user_args, const char **user_envp)
{
	int status = 0;
	struct proc_object *p_proc_object = (struct proc_object *)hprocessor;
	struct io_mgr *hio_mgr;	
	struct msg_mgr *hmsg_mgr;
	struct cod_manager *cod_mgr;	
	char *pargv0;		
	char **new_envp;	
	char sz_proc_id[MAXPROCIDLEN];	
	s32 envp_elems;		
	s32 cnew_envp;		
	s32 nproc_id = 0;	
	struct dcd_manager *hdcd_handle;
	struct dmm_object *dmm_mgr;
	u32 dw_ext_end;
	u32 proc_id;
	int brd_state;
	struct drv_data *drv_datap = dev_get_drvdata(bridge);

#ifdef OPT_LOAD_TIME_INSTRUMENTATION
	struct timeval tv1;
	struct timeval tv2;
#endif

#if defined(CONFIG_TIDSPBRIDGE_DVFS) && !defined(CONFIG_CPU_FREQ)
	struct dspbridge_platform_data *pdata =
	    omap_dspbridge_dev->dev.platform_data;
#endif

#ifdef OPT_LOAD_TIME_INSTRUMENTATION
	do_gettimeofday(&tv1);
#endif
	if (!p_proc_object) {
		status = -EFAULT;
		goto func_end;
	}
	dev_get_cod_mgr(p_proc_object->dev_obj, &cod_mgr);
	if (!cod_mgr) {
		status = -EPERM;
		goto func_end;
	}
	status = proc_stop(hprocessor);
	if (status)
		goto func_end;

	
	status = proc_monitor(hprocessor);
	if (status)
		goto func_end;

	
	pargv0 = (char *)user_args[0];
	
	envp_elems = get_envp_count((char **)user_envp);
	cnew_envp = (envp_elems ? (envp_elems + 1) : (envp_elems + 2));
	new_envp = kzalloc(cnew_envp * sizeof(char **), GFP_KERNEL);
	if (new_envp) {
		status = snprintf(sz_proc_id, MAXPROCIDLEN, PROC_ENVPROCID,
				  nproc_id);
		if (status == -1) {
			dev_dbg(bridge, "%s: Proc ID string overflow\n",
				__func__);
			status = -EPERM;
		} else {
			new_envp =
			    prepend_envp(new_envp, (char **)user_envp,
					 envp_elems, cnew_envp, sz_proc_id);
			
			status = mgr_get_dcd_handle(p_proc_object->mgr_obj,
						    (u32 *) &hdcd_handle);
			if (!status) {
				if (p_proc_object->last_coff != NULL) {
					status =
					    dcd_auto_unregister(hdcd_handle,
								p_proc_object->
								last_coff);
					kfree(p_proc_object->last_coff);
					p_proc_object->last_coff = NULL;
				}
			}
			
			status = cod_open_base(cod_mgr, (char *)user_args[0],
					       COD_SYMB);
		}
	} else {
		status = -ENOMEM;
	}
	if (!status) {
		
		
		status = mgr_get_dcd_handle(p_proc_object->mgr_obj,
					    (u32 *) &hdcd_handle);
		if (!status) {
			status =
			    dcd_auto_register(hdcd_handle,
					      (char *)user_args[0]);
			if (status == -EACCES)
				status = 0;

			if (status) {
				status = -EPERM;
			} else {
				
				p_proc_object->last_coff =
						kzalloc((strlen(user_args[0]) +
						1), GFP_KERNEL);
				
				if (p_proc_object->last_coff) {
					strncpy(p_proc_object->last_coff,
						(char *)user_args[0],
						(strlen((char *)user_args[0]) +
						 1));
				}
			}
		}
	}
	
	if (!status) {
		dev_get_msg_mgr(p_proc_object->dev_obj, &hmsg_mgr);
		if (!hmsg_mgr) {
			status = msg_create(&hmsg_mgr, p_proc_object->dev_obj,
					    (msg_onexit) node_on_exit);
			dev_set_msg_mgr(p_proc_object->dev_obj, hmsg_mgr);
		}
	}
	if (!status) {
		
		status = dev_get_io_mgr(p_proc_object->dev_obj, &hio_mgr);
		if (hio_mgr)
			status = (*p_proc_object->intf_fxns->io_on_loaded)
								(hio_mgr);
		else
			status = -EFAULT;
	}
	if (!status) {
		

		
#if defined(CONFIG_TIDSPBRIDGE_DVFS) && !defined(CONFIG_CPU_FREQ)
		if (pdata->cpu_set_freq)
			(*pdata->cpu_set_freq) (pdata->mpu_speed[VDD1_OPP5]);
#endif
		status = cod_load_base(cod_mgr, argc_index, (char **)user_args,
				       dev_brd_write_fxn,
				       p_proc_object->dev_obj, NULL);
		if (status) {
			if (status == -EBADF) {
				dev_dbg(bridge, "%s: Failure to Load the EXE\n",
					__func__);
			}
			if (status == -ESPIPE) {
				pr_err("%s: Couldn't parse the file\n",
				       __func__);
			}
		}
		
#if defined(CONFIG_TIDSPBRIDGE_DVFS) && !defined(CONFIG_CPU_FREQ)
		if (pdata->cpu_set_freq)
			(*pdata->cpu_set_freq) (pdata->mpu_speed[VDD1_OPP1]);
#endif

	}
	if (!status) {
		
		status = (*p_proc_object->intf_fxns->brd_set_state)
		    (p_proc_object->bridge_context, BRD_LOADED);
		if (!status) {
			p_proc_object->proc_state = PROC_LOADED;
			if (p_proc_object->ntfy_obj)
				proc_notify_clients(p_proc_object,
						    DSP_PROCESSORSTATECHANGE);
		}
	}
	if (!status) {
		status = proc_get_processor_id(hprocessor, &proc_id);
		if (proc_id == DSP_UNIT) {
			if (!status)
				status = cod_get_sym_value(cod_mgr, EXTEND,
							   &dw_ext_end);

			
			if (!status) {
				status =
				    dev_get_dmm_mgr(p_proc_object->dev_obj,
						    &dmm_mgr);
				if (dmm_mgr) {
					dw_ext_end =
					    (dw_ext_end + 1) * DSPWORDSIZE;
					
					status = dmm_create_tables(dmm_mgr,
								   dw_ext_end,
								   DMMPOOLSIZE);
				} else {
					status = -EFAULT;
				}
			}
		}
	}
	
	kfree(new_envp);
	user_args[0] = pargv0;
	if (!status) {
		if (!((*p_proc_object->intf_fxns->brd_status)
				(p_proc_object->bridge_context, &brd_state))) {
			pr_info("%s: Processor Loaded %s\n", __func__, pargv0);
			kfree(drv_datap->base_img);
			drv_datap->base_img = kmalloc(strlen(pargv0) + 1,
								GFP_KERNEL);
			if (drv_datap->base_img)
				strncpy(drv_datap->base_img, pargv0,
							strlen(pargv0) + 1);
			else
				status = -ENOMEM;
		}
	}

func_end:
	if (status) {
		pr_err("%s: Processor failed to load\n", __func__);
		proc_stop(p_proc_object);
	}
#ifdef OPT_LOAD_TIME_INSTRUMENTATION
	do_gettimeofday(&tv2);
	if (tv2.tv_usec < tv1.tv_usec) {
		tv2.tv_usec += 1000000;
		tv2.tv_sec--;
	}
	dev_dbg(bridge, "%s: time to load %d sec and %d usec\n", __func__,
		tv2.tv_sec - tv1.tv_sec, tv2.tv_usec - tv1.tv_usec);
#endif
	return status;
}

int proc_map(void *hprocessor, void *pmpu_addr, u32 ul_size,
		    void *req_addr, void **pp_map_addr, u32 ul_map_attr,
		    struct process_context *pr_ctxt)
{
	u32 va_align;
	u32 pa_align;
	struct dmm_object *dmm_mgr;
	u32 size_align;
	int status = 0;
	struct proc_object *p_proc_object = (struct proc_object *)hprocessor;
	struct dmm_map_object *map_obj;
	u32 tmp_addr = 0;

#ifdef CONFIG_TIDSPBRIDGE_CACHE_LINE_CHECK
	if ((ul_map_attr & BUFMODE_MASK) != RBUF) {
		if (!IS_ALIGNED((u32)pmpu_addr, DSP_CACHE_LINE) ||
		    !IS_ALIGNED(ul_size, DSP_CACHE_LINE)) {
			pr_err("%s: not aligned: 0x%x (%d)\n", __func__,
						(u32)pmpu_addr, ul_size);
			return -EFAULT;
		}
	}
#endif

	
	va_align = PG_ALIGN_LOW((u32) req_addr, PG_SIZE4K);
	pa_align = PG_ALIGN_LOW((u32) pmpu_addr, PG_SIZE4K);
	size_align = PG_ALIGN_HIGH(ul_size + (u32) pmpu_addr - pa_align,
				   PG_SIZE4K);

	if (!p_proc_object) {
		status = -EFAULT;
		goto func_end;
	}
	
	mutex_lock(&proc_lock);
	dmm_get_handle(p_proc_object, &dmm_mgr);
	if (dmm_mgr)
		status = dmm_map_memory(dmm_mgr, va_align, size_align);
	else
		status = -EFAULT;

	
	if (!status) {

		
		tmp_addr = (va_align | ((u32) pmpu_addr & (PG_SIZE4K - 1)));
		
		map_obj = add_mapping_info(pr_ctxt, pa_align, tmp_addr,
						size_align);
		if (!map_obj)
			status = -ENOMEM;
		else
			status = (*p_proc_object->intf_fxns->brd_mem_map)
			    (p_proc_object->bridge_context, pa_align, va_align,
			     size_align, ul_map_attr, map_obj->pages);
	}
	if (!status) {
		
		*pp_map_addr = (void *) tmp_addr;
	} else {
		remove_mapping_information(pr_ctxt, tmp_addr, size_align);
		dmm_un_map_memory(dmm_mgr, va_align, &size_align);
	}
	mutex_unlock(&proc_lock);

	if (status)
		goto func_end;

func_end:
	dev_dbg(bridge, "%s: hprocessor %p, pmpu_addr %p, ul_size %x, "
		"req_addr %p, ul_map_attr %x, pp_map_addr %p, va_align %x, "
		"pa_align %x, size_align %x status 0x%x\n", __func__,
		hprocessor, pmpu_addr, ul_size, req_addr, ul_map_attr,
		pp_map_addr, va_align, pa_align, size_align, status);

	return status;
}

int proc_register_notify(void *hprocessor, u32 event_mask,
				u32 notify_type, struct dsp_notification
				* hnotification)
{
	int status = 0;
	struct proc_object *p_proc_object = (struct proc_object *)hprocessor;
	struct deh_mgr *hdeh_mgr;

	
	if (!p_proc_object) {
		status = -EFAULT;
		goto func_end;
	}
	
	if (event_mask & ~(DSP_PROCESSORSTATECHANGE | DSP_PROCESSORATTACH |
			DSP_PROCESSORDETACH | DSP_PROCESSORRESTART |
			DSP_MMUFAULT | DSP_SYSERROR | DSP_PWRERROR |
			DSP_WDTOVERFLOW))
		status = -EINVAL;

	
	if (notify_type != DSP_SIGNALEVENT)
		status = -EINVAL;

	if (!status) {
		if (event_mask &
		    ~(DSP_SYSERROR | DSP_MMUFAULT | DSP_PWRERROR |
				DSP_WDTOVERFLOW)) {
			status = ntfy_register(p_proc_object->ntfy_obj,
					       hnotification, event_mask,
					       notify_type);
			if ((event_mask == 0) && status) {
				status =
				    dev_get_deh_mgr(p_proc_object->dev_obj,
						    &hdeh_mgr);
				status =
					bridge_deh_register_notify(hdeh_mgr,
							event_mask,
							notify_type,
							hnotification);
			}
		} else {
			status = dev_get_deh_mgr(p_proc_object->dev_obj,
						 &hdeh_mgr);
			status =
			    bridge_deh_register_notify(hdeh_mgr,
					    event_mask,
					    notify_type,
					    hnotification);

		}
	}
func_end:
	return status;
}

int proc_reserve_memory(void *hprocessor, u32 ul_size,
			       void **pp_rsv_addr,
			       struct process_context *pr_ctxt)
{
	struct dmm_object *dmm_mgr;
	int status = 0;
	struct proc_object *p_proc_object = (struct proc_object *)hprocessor;
	struct dmm_rsv_object *rsv_obj;

	if (!p_proc_object) {
		status = -EFAULT;
		goto func_end;
	}

	status = dmm_get_handle(p_proc_object, &dmm_mgr);
	if (!dmm_mgr) {
		status = -EFAULT;
		goto func_end;
	}

	status = dmm_reserve_memory(dmm_mgr, ul_size, (u32 *) pp_rsv_addr);
	if (status != 0)
		goto func_end;

	rsv_obj = kmalloc(sizeof(struct dmm_rsv_object), GFP_KERNEL);
	if (rsv_obj) {
		rsv_obj->dsp_reserved_addr = (u32) *pp_rsv_addr;
		spin_lock(&pr_ctxt->dmm_rsv_lock);
		list_add(&rsv_obj->link, &pr_ctxt->dmm_rsv_list);
		spin_unlock(&pr_ctxt->dmm_rsv_lock);
	}

func_end:
	dev_dbg(bridge, "%s: hprocessor: 0x%p ul_size: 0x%x pp_rsv_addr: 0x%p "
		"status 0x%x\n", __func__, hprocessor,
		ul_size, pp_rsv_addr, status);
	return status;
}

int proc_start(void *hprocessor)
{
	int status = 0;
	struct proc_object *p_proc_object = (struct proc_object *)hprocessor;
	struct cod_manager *cod_mgr;	
	u32 dw_dsp_addr;	
	int brd_state;

	if (!p_proc_object) {
		status = -EFAULT;
		goto func_end;
	}
	
	if (p_proc_object->proc_state != PROC_LOADED) {
		status = -EBADR;
		goto func_end;
	}
	status = dev_get_cod_mgr(p_proc_object->dev_obj, &cod_mgr);
	if (!cod_mgr) {
		status = -EFAULT;
		goto func_cont;
	}

	status = cod_get_entry(cod_mgr, &dw_dsp_addr);
	if (status)
		goto func_cont;

	status = (*p_proc_object->intf_fxns->brd_start)
	    (p_proc_object->bridge_context, dw_dsp_addr);
	if (status)
		goto func_cont;

	
	status = dev_create2(p_proc_object->dev_obj);
	if (!status) {
		p_proc_object->proc_state = PROC_RUNNING;

		if (p_proc_object->ntfy_obj) {
			proc_notify_clients(p_proc_object,
					    DSP_PROCESSORSTATECHANGE);
		}
	} else {
		(void)(*p_proc_object->intf_fxns->
		       brd_stop) (p_proc_object->bridge_context);
		p_proc_object->proc_state = PROC_STOPPED;
	}
func_cont:
	if (!status) {
		if (!((*p_proc_object->intf_fxns->brd_status)
				(p_proc_object->bridge_context, &brd_state))) {
			pr_info("%s: dsp in running state\n", __func__);
		}
	} else {
		pr_err("%s: Failed to start the dsp\n", __func__);
		proc_stop(p_proc_object);
	}

func_end:
	return status;
}

int proc_stop(void *hprocessor)
{
	int status = 0;
	struct proc_object *p_proc_object = (struct proc_object *)hprocessor;
	struct msg_mgr *hmsg_mgr;
	struct node_mgr *hnode_mgr;
	void *hnode;
	u32 node_tab_size = 1;
	u32 num_nodes = 0;
	u32 nodes_allocated = 0;

	if (!p_proc_object) {
		status = -EFAULT;
		goto func_end;
	}
	
	status = dev_get_node_manager(p_proc_object->dev_obj, &hnode_mgr);
	if (!status && hnode_mgr) {
		status = node_enum_nodes(hnode_mgr, &hnode, node_tab_size,
					 &num_nodes, &nodes_allocated);
		if ((status == -EINVAL) || (nodes_allocated > 0)) {
			pr_err("%s: Can't stop device, active nodes = %d \n",
			       __func__, nodes_allocated);
			return -EBADR;
		}
	}
	
	
	status =
	    (*p_proc_object->intf_fxns->
	     brd_stop) (p_proc_object->bridge_context);
	if (!status) {
		dev_dbg(bridge, "%s: processor in standby mode\n", __func__);
		p_proc_object->proc_state = PROC_STOPPED;
		
		if (!(dev_destroy2(p_proc_object->dev_obj))) {
			
			dev_get_msg_mgr(p_proc_object->dev_obj, &hmsg_mgr);
			if (hmsg_mgr) {
				msg_delete(hmsg_mgr);
				dev_set_msg_mgr(p_proc_object->dev_obj, NULL);
			}
		}
	} else {
		pr_err("%s: Failed to stop the processor\n", __func__);
	}
func_end:

	return status;
}

int proc_un_map(void *hprocessor, void *map_addr,
		       struct process_context *pr_ctxt)
{
	int status = 0;
	struct proc_object *p_proc_object = (struct proc_object *)hprocessor;
	struct dmm_object *dmm_mgr;
	u32 va_align;
	u32 size_align;

	va_align = PG_ALIGN_LOW((u32) map_addr, PG_SIZE4K);
	if (!p_proc_object) {
		status = -EFAULT;
		goto func_end;
	}

	status = dmm_get_handle(hprocessor, &dmm_mgr);
	if (!dmm_mgr) {
		status = -EFAULT;
		goto func_end;
	}

	
	mutex_lock(&proc_lock);
	status = dmm_un_map_memory(dmm_mgr, (u32) va_align, &size_align);
	
	if (!status) {
		status = (*p_proc_object->intf_fxns->brd_mem_un_map)
		    (p_proc_object->bridge_context, va_align, size_align);
	}

	if (status)
		goto unmap_failed;

	remove_mapping_information(pr_ctxt, (u32) map_addr, size_align);

unmap_failed:
	mutex_unlock(&proc_lock);

func_end:
	dev_dbg(bridge, "%s: hprocessor: 0x%p map_addr: 0x%p status: 0x%x\n",
		__func__, hprocessor, map_addr, status);
	return status;
}

int proc_un_reserve_memory(void *hprocessor, void *prsv_addr,
				  struct process_context *pr_ctxt)
{
	struct dmm_object *dmm_mgr;
	int status = 0;
	struct proc_object *p_proc_object = (struct proc_object *)hprocessor;
	struct dmm_rsv_object *rsv_obj;

	if (!p_proc_object) {
		status = -EFAULT;
		goto func_end;
	}

	status = dmm_get_handle(p_proc_object, &dmm_mgr);
	if (!dmm_mgr) {
		status = -EFAULT;
		goto func_end;
	}

	status = dmm_un_reserve_memory(dmm_mgr, (u32) prsv_addr);
	if (status != 0)
		goto func_end;

	spin_lock(&pr_ctxt->dmm_rsv_lock);
	list_for_each_entry(rsv_obj, &pr_ctxt->dmm_rsv_list, link) {
		if (rsv_obj->dsp_reserved_addr == (u32) prsv_addr) {
			list_del(&rsv_obj->link);
			kfree(rsv_obj);
			break;
		}
	}
	spin_unlock(&pr_ctxt->dmm_rsv_lock);

func_end:
	dev_dbg(bridge, "%s: hprocessor: 0x%p prsv_addr: 0x%p status: 0x%x\n",
		__func__, hprocessor, prsv_addr, status);
	return status;
}

static int proc_monitor(struct proc_object *proc_obj)
{
	int status = -EPERM;
	struct msg_mgr *hmsg_mgr;

	
	if (!dev_destroy2(proc_obj->dev_obj)) {
		
		dev_get_msg_mgr(proc_obj->dev_obj, &hmsg_mgr);
		if (hmsg_mgr) {
			msg_delete(hmsg_mgr);
			dev_set_msg_mgr(proc_obj->dev_obj, NULL);
		}
	}
	
	if (!((*proc_obj->intf_fxns->brd_monitor)
			  (proc_obj->bridge_context))) {
		status = 0;
	}

	return status;
}

static s32 get_envp_count(char **envp)
{
	s32 ret = 0;
	if (envp) {
		while (*envp++)
			ret++;

		ret += 1;	
	}

	return ret;
}

static char **prepend_envp(char **new_envp, char **envp, s32 envp_elems,
			   s32 cnew_envp, char *sz_var)
{
	char **pp_envp = new_envp;

	
	*new_envp++ = sz_var;

	
	while (envp_elems--)
		*new_envp++ = *envp++;

	
	if (envp_elems == 0)
		*new_envp = NULL;

	return pp_envp;
}

int proc_notify_clients(void *proc, u32 events)
{
	int status = 0;
	struct proc_object *p_proc_object = (struct proc_object *)proc;

	if (!p_proc_object) {
		status = -EFAULT;
		goto func_end;
	}

	ntfy_notify(p_proc_object->ntfy_obj, events);
func_end:
	return status;
}

int proc_notify_all_clients(void *proc, u32 events)
{
	int status = 0;
	struct proc_object *p_proc_object = (struct proc_object *)proc;

	if (!p_proc_object) {
		status = -EFAULT;
		goto func_end;
	}

	dev_notify_clients(p_proc_object->dev_obj, events);

func_end:
	return status;
}

int proc_get_processor_id(void *proc, u32 * proc_id)
{
	int status = 0;
	struct proc_object *p_proc_object = (struct proc_object *)proc;

	if (p_proc_object)
		*proc_id = p_proc_object->processor_id;
	else
		status = -EFAULT;

	return status;
}
