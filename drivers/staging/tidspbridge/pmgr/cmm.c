/*
 * cmm.c
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * The Communication(Shared) Memory Management(CMM) module provides
 * shared memory management services for DSP/BIOS Bridge data streaming
 * and messaging.
 *
 * Multiple shared memory segments can be registered with CMM.
 * Each registered SM segment is represented by a SM "allocator" that
 * describes a block of physically contiguous shared memory used for
 * future allocations by CMM.
 *
 * Memory is coalesced back to the appropriate heap when a buffer is
 * freed.
 *
 * Notes:
 *   Va: Virtual address.
 *   Pa: Physical or kernel system address.
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

#include <dspbridge/dbdefs.h>

#include <dspbridge/sync.h>

#include <dspbridge/dev.h>
#include <dspbridge/proc.h>

#include <dspbridge/cmm.h>

#define NEXT_PA(pnode)   (pnode->pa + pnode->size)

#define DSPPA2GPPPA(base, x, y)  ((x)+(y))
#define GPPPA2DSPPA(base, x, y)  ((x)-(y))

struct cmm_allocator {		
	unsigned int shm_base;	
	u32 sm_size;		
	unsigned int vm_base;	
	u32 dsp_phys_addr_offset;	
	s8 c_factor;		
	unsigned int dsp_base;	
	u32 dsp_size;	
	struct cmm_object *cmm_mgr;	
	
	struct list_head free_list;
	
	struct list_head in_use_list;
};

struct cmm_xlator {		
	
	struct cmm_object *cmm_mgr;
	unsigned int virt_base;	
	u32 virt_size;		
	u32 seg_id;		
};

struct cmm_object {
	struct mutex cmm_lock;	
	struct list_head node_free_list;	
	u32 min_block_size;	
	u32 page_size;	
	
	struct cmm_allocator *pa_gppsm_seg_tab[CMM_MAXGPPSEGS];
};

static struct cmm_mgrattrs cmm_dfltmgrattrs = {
	
	16
};

static struct cmm_attrs cmm_dfltalctattrs = {
	1		
};

static struct cmm_xlatorattrs cmm_dfltxlatorattrs = {
	
	1,
	0,			
	0,			
	NULL,			
	0,			
};

struct cmm_mnode {
	struct list_head link;	
	u32 pa;		
	u32 va;			
	u32 size;		
	u32 client_proc;	
};

static void add_to_free_list(struct cmm_allocator *allocator,
			     struct cmm_mnode *pnode);
static struct cmm_allocator *get_allocator(struct cmm_object *cmm_mgr_obj,
					   u32 ul_seg_id);
static struct cmm_mnode *get_free_block(struct cmm_allocator *allocator,
					u32 usize);
static struct cmm_mnode *get_node(struct cmm_object *cmm_mgr_obj, u32 dw_pa,
				  u32 dw_va, u32 ul_size);
static s32 get_slot(struct cmm_object *cmm_mgr_obj);
static void un_register_gppsm_seg(struct cmm_allocator *psma);

void *cmm_calloc_buf(struct cmm_object *hcmm_mgr, u32 usize,
		     struct cmm_attrs *pattrs, void **pp_buf_va)
{
	struct cmm_object *cmm_mgr_obj = (struct cmm_object *)hcmm_mgr;
	void *buf_pa = NULL;
	struct cmm_mnode *pnode = NULL;
	struct cmm_mnode *new_node = NULL;
	struct cmm_allocator *allocator = NULL;
	u32 delta_size;
	u8 *pbyte = NULL;
	s32 cnt;

	if (pattrs == NULL)
		pattrs = &cmm_dfltalctattrs;

	if (pp_buf_va != NULL)
		*pp_buf_va = NULL;

	if (cmm_mgr_obj && (usize != 0)) {
		if (pattrs->seg_id > 0) {
			
			
			allocator =
			    get_allocator(cmm_mgr_obj, pattrs->seg_id);
			
			usize =
			    ((usize - 1) & ~(cmm_mgr_obj->min_block_size -
					     1))
			    + cmm_mgr_obj->min_block_size;
			mutex_lock(&cmm_mgr_obj->cmm_lock);
			pnode = get_free_block(allocator, usize);
		}
		if (pnode) {
			delta_size = (pnode->size - usize);
			if (delta_size >= cmm_mgr_obj->min_block_size) {
				new_node =
				    get_node(cmm_mgr_obj, pnode->pa + usize,
					     pnode->va + usize,
					     (u32) delta_size);
				
				add_to_free_list(allocator, new_node);
				
				pnode->size = usize;
			}
			
			pnode->client_proc = current->tgid;

			
			list_add_tail(&pnode->link, &allocator->in_use_list);
			buf_pa = (void *)pnode->pa;	
			
			pbyte = (u8 *) pnode->va;
			for (cnt = 0; cnt < (s32) usize; cnt++, pbyte++)
				*pbyte = 0;

			if (pp_buf_va != NULL) {
				
				*pp_buf_va = (void *)pnode->va;
			}
		}
		mutex_unlock(&cmm_mgr_obj->cmm_lock);
	}
	return buf_pa;
}

int cmm_create(struct cmm_object **ph_cmm_mgr,
		      struct dev_object *hdev_obj,
		      const struct cmm_mgrattrs *mgr_attrts)
{
	struct cmm_object *cmm_obj = NULL;
	int status = 0;

	*ph_cmm_mgr = NULL;
	
	cmm_obj = kzalloc(sizeof(struct cmm_object), GFP_KERNEL);
	if (!cmm_obj)
		return -ENOMEM;

	if (mgr_attrts == NULL)
		mgr_attrts = &cmm_dfltmgrattrs;	

	
	cmm_obj->min_block_size = mgr_attrts->min_block_size;
	cmm_obj->page_size = PAGE_SIZE;

	
	INIT_LIST_HEAD(&cmm_obj->node_free_list);
	mutex_init(&cmm_obj->cmm_lock);
	*ph_cmm_mgr = cmm_obj;

	return status;
}

int cmm_destroy(struct cmm_object *hcmm_mgr, bool force)
{
	struct cmm_object *cmm_mgr_obj = (struct cmm_object *)hcmm_mgr;
	struct cmm_info temp_info;
	int status = 0;
	s32 slot_seg;
	struct cmm_mnode *node, *tmp;

	if (!hcmm_mgr) {
		status = -EFAULT;
		return status;
	}
	mutex_lock(&cmm_mgr_obj->cmm_lock);
	
	if (!force) {
		
		status = cmm_get_info(hcmm_mgr, &temp_info);
		if (!status) {
			if (temp_info.total_in_use_cnt > 0) {
				
				status = -EPERM;
			}
		}
	}
	if (!status) {
		
		for (slot_seg = 0; slot_seg < CMM_MAXGPPSEGS; slot_seg++) {
			if (cmm_mgr_obj->pa_gppsm_seg_tab[slot_seg] != NULL) {
				un_register_gppsm_seg
				    (cmm_mgr_obj->pa_gppsm_seg_tab[slot_seg]);
				
				cmm_mgr_obj->pa_gppsm_seg_tab[slot_seg] = NULL;
			}
		}
	}
	list_for_each_entry_safe(node, tmp, &cmm_mgr_obj->node_free_list,
			link) {
		list_del(&node->link);
		kfree(node);
	}
	mutex_unlock(&cmm_mgr_obj->cmm_lock);
	if (!status) {
		
		mutex_destroy(&cmm_mgr_obj->cmm_lock);
		kfree(cmm_mgr_obj);
	}
	return status;
}

int cmm_free_buf(struct cmm_object *hcmm_mgr, void *buf_pa, u32 ul_seg_id)
{
	struct cmm_object *cmm_mgr_obj = (struct cmm_object *)hcmm_mgr;
	int status = -EFAULT;
	struct cmm_mnode *curr, *tmp;
	struct cmm_allocator *allocator;
	struct cmm_attrs *pattrs;

	if (ul_seg_id == 0) {
		pattrs = &cmm_dfltalctattrs;
		ul_seg_id = pattrs->seg_id;
	}
	if (!hcmm_mgr || !(ul_seg_id > 0)) {
		status = -EFAULT;
		return status;
	}

	allocator = get_allocator(cmm_mgr_obj, ul_seg_id);
	if (!allocator)
		return status;

	mutex_lock(&cmm_mgr_obj->cmm_lock);
	list_for_each_entry_safe(curr, tmp, &allocator->in_use_list, link) {
		if (curr->pa == (u32) buf_pa) {
			list_del(&curr->link);
			add_to_free_list(allocator, curr);
			status = 0;
			break;
		}
	}
	mutex_unlock(&cmm_mgr_obj->cmm_lock);

	return status;
}

int cmm_get_handle(void *hprocessor, struct cmm_object ** ph_cmm_mgr)
{
	int status = 0;
	struct dev_object *hdev_obj;

	if (hprocessor != NULL)
		status = proc_get_dev_object(hprocessor, &hdev_obj);
	else
		hdev_obj = dev_get_first();	

	if (!status)
		status = dev_get_cmm_mgr(hdev_obj, ph_cmm_mgr);

	return status;
}

int cmm_get_info(struct cmm_object *hcmm_mgr,
			struct cmm_info *cmm_info_obj)
{
	struct cmm_object *cmm_mgr_obj = (struct cmm_object *)hcmm_mgr;
	u32 ul_seg;
	int status = 0;
	struct cmm_allocator *altr;
	struct cmm_mnode *curr;

	if (!hcmm_mgr) {
		status = -EFAULT;
		return status;
	}
	mutex_lock(&cmm_mgr_obj->cmm_lock);
	cmm_info_obj->num_gppsm_segs = 0;	
	
	cmm_info_obj->total_in_use_cnt = 0;
	
	cmm_info_obj->min_block_size = cmm_mgr_obj->min_block_size;
	
	for (ul_seg = 1; ul_seg <= CMM_MAXGPPSEGS; ul_seg++) {
		
		altr = get_allocator(cmm_mgr_obj, ul_seg);
		if (!altr)
			continue;
		cmm_info_obj->num_gppsm_segs++;
		cmm_info_obj->seg_info[ul_seg - 1].seg_base_pa =
			altr->shm_base - altr->dsp_size;
		cmm_info_obj->seg_info[ul_seg - 1].total_seg_size =
			altr->dsp_size + altr->sm_size;
		cmm_info_obj->seg_info[ul_seg - 1].gpp_base_pa =
			altr->shm_base;
		cmm_info_obj->seg_info[ul_seg - 1].gpp_size =
			altr->sm_size;
		cmm_info_obj->seg_info[ul_seg - 1].dsp_base_va =
			altr->dsp_base;
		cmm_info_obj->seg_info[ul_seg - 1].dsp_size =
			altr->dsp_size;
		cmm_info_obj->seg_info[ul_seg - 1].seg_base_va =
			altr->vm_base - altr->dsp_size;
		cmm_info_obj->seg_info[ul_seg - 1].in_use_cnt = 0;

		list_for_each_entry(curr, &altr->in_use_list, link) {
			cmm_info_obj->total_in_use_cnt++;
			cmm_info_obj->seg_info[ul_seg - 1].in_use_cnt++;
		}
	}
	mutex_unlock(&cmm_mgr_obj->cmm_lock);
	return status;
}

int cmm_register_gppsm_seg(struct cmm_object *hcmm_mgr,
				  u32 dw_gpp_base_pa, u32 ul_size,
				  u32 dsp_addr_offset, s8 c_factor,
				  u32 dw_dsp_base, u32 ul_dsp_size,
				  u32 *sgmt_id, u32 gpp_base_va)
{
	struct cmm_object *cmm_mgr_obj = (struct cmm_object *)hcmm_mgr;
	struct cmm_allocator *psma = NULL;
	int status = 0;
	struct cmm_mnode *new_node;
	s32 slot_seg;

	dev_dbg(bridge, "%s: dw_gpp_base_pa %x ul_size %x dsp_addr_offset %x "
			"dw_dsp_base %x ul_dsp_size %x gpp_base_va %x\n",
			__func__, dw_gpp_base_pa, ul_size, dsp_addr_offset,
			dw_dsp_base, ul_dsp_size, gpp_base_va);

	if (!hcmm_mgr)
		return -EFAULT;

	
	mutex_lock(&cmm_mgr_obj->cmm_lock);

	slot_seg = get_slot(cmm_mgr_obj);
	if (slot_seg < 0) {
		status = -EPERM;
		goto func_end;
	}

	
	if (ul_size < cmm_mgr_obj->min_block_size) {
		status = -EINVAL;
		goto func_end;
	}

	
	psma = kzalloc(sizeof(struct cmm_allocator), GFP_KERNEL);
	if (!psma) {
		status = -ENOMEM;
		goto func_end;
	}

	psma->cmm_mgr = hcmm_mgr;	
	psma->shm_base = dw_gpp_base_pa;	
	psma->sm_size = ul_size;	
	psma->vm_base = gpp_base_va;
	psma->dsp_phys_addr_offset = dsp_addr_offset;
	psma->c_factor = c_factor;
	psma->dsp_base = dw_dsp_base;
	psma->dsp_size = ul_dsp_size;
	if (psma->vm_base == 0) {
		status = -EPERM;
		goto func_end;
	}
	
	*sgmt_id = (u32) slot_seg + 1;

	INIT_LIST_HEAD(&psma->free_list);
	INIT_LIST_HEAD(&psma->in_use_list);

	
	new_node = get_node(cmm_mgr_obj, dw_gpp_base_pa,
			psma->vm_base, ul_size);
	
	if (new_node) {
		list_add_tail(&new_node->link, &psma->free_list);
	} else {
		status = -ENOMEM;
		goto func_end;
	}
	
	cmm_mgr_obj->pa_gppsm_seg_tab[slot_seg] = psma;

func_end:
	
	if (status && psma)
		un_register_gppsm_seg(psma);
	mutex_unlock(&cmm_mgr_obj->cmm_lock);

	return status;
}

int cmm_un_register_gppsm_seg(struct cmm_object *hcmm_mgr,
				     u32 ul_seg_id)
{
	struct cmm_object *cmm_mgr_obj = (struct cmm_object *)hcmm_mgr;
	int status = 0;
	struct cmm_allocator *psma;
	u32 ul_id = ul_seg_id;

	if (!hcmm_mgr)
		return -EFAULT;

	if (ul_seg_id == CMM_ALLSEGMENTS)
		ul_id = 1;

	if ((ul_id <= 0) || (ul_id > CMM_MAXGPPSEGS))
		return -EINVAL;

	while (ul_id <= CMM_MAXGPPSEGS) {
		mutex_lock(&cmm_mgr_obj->cmm_lock);
		
		psma = cmm_mgr_obj->pa_gppsm_seg_tab[ul_id - 1];
		if (psma != NULL) {
			un_register_gppsm_seg(psma);
			
			cmm_mgr_obj->pa_gppsm_seg_tab[ul_id - 1] = NULL;
		} else if (ul_seg_id != CMM_ALLSEGMENTS) {
			status = -EPERM;
		}
		mutex_unlock(&cmm_mgr_obj->cmm_lock);
		if (ul_seg_id != CMM_ALLSEGMENTS)
			break;

		ul_id++;
	}	
	return status;
}

static void un_register_gppsm_seg(struct cmm_allocator *psma)
{
	struct cmm_mnode *curr, *tmp;

	
	list_for_each_entry_safe(curr, tmp, &psma->free_list, link) {
		list_del(&curr->link);
		kfree(curr);
	}

	
	list_for_each_entry_safe(curr, tmp, &psma->in_use_list, link) {
		list_del(&curr->link);
		kfree(curr);
	}

	if ((void *)psma->vm_base != NULL)
		MEM_UNMAP_LINEAR_ADDRESS((void *)psma->vm_base);

	
	kfree(psma);
}

static s32 get_slot(struct cmm_object *cmm_mgr_obj)
{
	s32 slot_seg = -1;	
	
	for (slot_seg = 0; slot_seg < CMM_MAXGPPSEGS; slot_seg++) {
		if (cmm_mgr_obj->pa_gppsm_seg_tab[slot_seg] == NULL)
			break;

	}
	if (slot_seg == CMM_MAXGPPSEGS)
		slot_seg = -1;	

	return slot_seg;
}

static struct cmm_mnode *get_node(struct cmm_object *cmm_mgr_obj, u32 dw_pa,
				  u32 dw_va, u32 ul_size)
{
	struct cmm_mnode *pnode;

	
	if (list_empty(&cmm_mgr_obj->node_free_list)) {
		pnode = kzalloc(sizeof(struct cmm_mnode), GFP_KERNEL);
		if (!pnode)
			return NULL;
	} else {
		
		pnode = list_first_entry(&cmm_mgr_obj->node_free_list,
				struct cmm_mnode, link);
		list_del_init(&pnode->link);
	}

	pnode->pa = dw_pa;
	pnode->va = dw_va;
	pnode->size = ul_size;

	return pnode;
}

static void delete_node(struct cmm_object *cmm_mgr_obj, struct cmm_mnode *pnode)
{
	list_add_tail(&pnode->link, &cmm_mgr_obj->node_free_list);
}

static struct cmm_mnode *get_free_block(struct cmm_allocator *allocator,
					u32 usize)
{
	struct cmm_mnode *node, *tmp;

	if (!allocator)
		return NULL;

	list_for_each_entry_safe(node, tmp, &allocator->free_list, link) {
		if (usize <= node->size) {
			list_del(&node->link);
			return node;
		}
	}

	return NULL;
}

static void add_to_free_list(struct cmm_allocator *allocator,
			     struct cmm_mnode *node)
{
	struct cmm_mnode *curr;

	if (!node) {
		pr_err("%s: failed - node is NULL\n", __func__);
		return;
	}

	list_for_each_entry(curr, &allocator->free_list, link) {
		if (NEXT_PA(curr) == node->pa) {
			curr->size += node->size;
			delete_node(allocator->cmm_mgr, node);
			return;
		}
		if (curr->pa == NEXT_PA(node)) {
			curr->pa = node->pa;
			curr->va = node->va;
			curr->size += node->size;
			delete_node(allocator->cmm_mgr, node);
			return;
		}
	}
	list_for_each_entry(curr, &allocator->free_list, link) {
		if (curr->size >= node->size) {
			list_add_tail(&node->link, &curr->link);
			return;
		}
	}
	list_add_tail(&node->link, &allocator->free_list);
}

static struct cmm_allocator *get_allocator(struct cmm_object *cmm_mgr_obj,
					   u32 ul_seg_id)
{
	return cmm_mgr_obj->pa_gppsm_seg_tab[ul_seg_id - 1];
}


int cmm_xlator_create(struct cmm_xlatorobject **xlator,
			     struct cmm_object *hcmm_mgr,
			     struct cmm_xlatorattrs *xlator_attrs)
{
	struct cmm_xlator *xlator_object = NULL;
	int status = 0;

	*xlator = NULL;
	if (xlator_attrs == NULL)
		xlator_attrs = &cmm_dfltxlatorattrs;	

	xlator_object = kzalloc(sizeof(struct cmm_xlator), GFP_KERNEL);
	if (xlator_object != NULL) {
		xlator_object->cmm_mgr = hcmm_mgr;	
		
		xlator_object->seg_id = xlator_attrs->seg_id;
	} else {
		status = -ENOMEM;
	}
	if (!status)
		*xlator = (struct cmm_xlatorobject *)xlator_object;

	return status;
}

void *cmm_xlator_alloc_buf(struct cmm_xlatorobject *xlator, void *va_buf,
			   u32 pa_size)
{
	struct cmm_xlator *xlator_obj = (struct cmm_xlator *)xlator;
	void *pbuf = NULL;
	void *tmp_va_buff;
	struct cmm_attrs attrs;

	if (xlator_obj) {
		attrs.seg_id = xlator_obj->seg_id;
		__raw_writel(0, va_buf);
		
		pbuf =
		    cmm_calloc_buf(xlator_obj->cmm_mgr, pa_size, &attrs, NULL);
		if (pbuf) {
			 tmp_va_buff = cmm_xlator_translate(xlator,
							 pbuf, CMM_PA2VA);
			__raw_writel((u32)tmp_va_buff, va_buf);
		}
	}
	return pbuf;
}

int cmm_xlator_free_buf(struct cmm_xlatorobject *xlator, void *buf_va)
{
	struct cmm_xlator *xlator_obj = (struct cmm_xlator *)xlator;
	int status = -EPERM;
	void *buf_pa = NULL;

	if (xlator_obj) {
		
		buf_pa = cmm_xlator_translate(xlator, buf_va, CMM_VA2PA);
		if (buf_pa) {
			status = cmm_free_buf(xlator_obj->cmm_mgr, buf_pa,
					      xlator_obj->seg_id);
			if (status) {
				pr_err("%s, line %d: Assertion failed\n",
				       __FILE__, __LINE__);
			}
		}
	}
	return status;
}

int cmm_xlator_info(struct cmm_xlatorobject *xlator, u8 ** paddr,
			   u32 ul_size, u32 segm_id, bool set_info)
{
	struct cmm_xlator *xlator_obj = (struct cmm_xlator *)xlator;
	int status = 0;

	if (xlator_obj) {
		if (set_info) {
			
			xlator_obj->virt_base = (u32) *paddr;
			xlator_obj->virt_size = ul_size;
		} else {	
			*paddr = (u8 *) xlator_obj->virt_base;
		}
	} else {
		status = -EFAULT;
	}
	return status;
}

void *cmm_xlator_translate(struct cmm_xlatorobject *xlator, void *paddr,
			   enum cmm_xlatetype xtype)
{
	u32 dw_addr_xlate = 0;
	struct cmm_xlator *xlator_obj = (struct cmm_xlator *)xlator;
	struct cmm_object *cmm_mgr_obj = NULL;
	struct cmm_allocator *allocator = NULL;
	u32 dw_offset = 0;

	if (!xlator_obj)
		goto loop_cont;

	cmm_mgr_obj = (struct cmm_object *)xlator_obj->cmm_mgr;
	
	allocator = cmm_mgr_obj->pa_gppsm_seg_tab[xlator_obj->seg_id - 1];
	if (!allocator)
		goto loop_cont;

	if ((xtype == CMM_VA2DSPPA) || (xtype == CMM_VA2PA) ||
	    (xtype == CMM_PA2VA)) {
		if (xtype == CMM_PA2VA) {
			
			dw_offset = (u8 *) paddr - (u8 *) (allocator->shm_base -
							   allocator->
							   dsp_size);
			dw_addr_xlate = xlator_obj->virt_base + dw_offset;
			
			if ((dw_addr_xlate < xlator_obj->virt_base) ||
			    (dw_addr_xlate >=
			     (xlator_obj->virt_base +
			      xlator_obj->virt_size))) {
				dw_addr_xlate = 0;	
			}
		} else {
			
			dw_offset =
			    (u8 *) paddr - (u8 *) xlator_obj->virt_base;
			dw_addr_xlate =
			    allocator->shm_base - allocator->dsp_size +
			    dw_offset;
		}
	} else {
		dw_addr_xlate = (u32) paddr;
	}
	
	if ((xtype == CMM_VA2DSPPA) || (xtype == CMM_PA2DSPPA)) {
		
		dw_addr_xlate =
		    GPPPA2DSPPA((allocator->shm_base - allocator->dsp_size),
				dw_addr_xlate,
				allocator->dsp_phys_addr_offset *
				allocator->c_factor);
	} else if (xtype == CMM_DSPPA2PA) {
		
		dw_addr_xlate =
		    DSPPA2GPPPA(allocator->shm_base - allocator->dsp_size,
				dw_addr_xlate,
				allocator->dsp_phys_addr_offset *
				allocator->c_factor);
	}
loop_cont:
	return (void *)dw_addr_xlate;
}
