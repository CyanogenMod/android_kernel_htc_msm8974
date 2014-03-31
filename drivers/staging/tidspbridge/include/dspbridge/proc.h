/*
 * proc.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * This is the DSP API RM module interface.
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

#ifndef PROC_
#define PROC_

#include <dspbridge/cfgdefs.h>
#include <dspbridge/devdefs.h>
#include <dspbridge/drv.h>

extern char *iva_img;

extern int proc_attach(u32 processor_id,
			      const struct dsp_processorattrin
			      *attr_in, void **ph_processor,
			      struct process_context *pr_ctxt);

extern int proc_auto_start(struct cfg_devnode *dev_node_obj,
				  struct dev_object *hdev_obj);

extern int proc_ctrl(void *hprocessor,
			    u32 dw_cmd, struct dsp_cbdata *arg);

extern int proc_detach(struct process_context *pr_ctxt);

/*
 *  ======== proc_enum_nodes ========
 *  Purpose:
 *      Enumerate the nodes currently allocated on a processor.
 *  Parameters:
 *      hprocessor  :   The processor handle.
 *      node_tab    :   The first Location of an array allocated for node
 *		      handles.
 *      node_tab_size:   The number of (DSP_HNODE) handles that can be held
 *		      to the memory the client has allocated for node_tab
 *      pu_num_nodes  :   Location where DSPProcessor_EnumNodes will return
 *		      the number of valid handles written to node_tab
 *      pu_allocated :   Location where DSPProcessor_EnumNodes will return
 *		      the number of nodes that are allocated on the DSP.
 *  Returns:
 *      0     :   Success.
 *      -EFAULT :   Invalid processor handle.
 *      -EINVAL   :   The amount of memory allocated for node_tab is
 *		      insufficent. That is the number of nodes actually
 *		      allocated on the DSP is greater than the value
 *		      specified for node_tab_size.
 *      -EPERM   :   Unable to get Resource Information.
 *  Details:
 *  Requires
 *      pu_num_nodes is not NULL.
 *      pu_allocated is not NULL.
 *      node_tab is not NULL.
 *      PROC Initialized.
 *  Ensures:
 *  Details:
 */
extern int proc_enum_nodes(void *hprocessor,
				  void **node_tab,
				  u32 node_tab_size,
				  u32 *pu_num_nodes,
				  u32 *pu_allocated);

extern int proc_get_resource_info(void *hprocessor,
					 u32 resource_type,
					 struct dsp_resourceinfo
					 *resource_info,
					 u32 resource_info_size);

extern int proc_get_dev_object(void *hprocessor,
				      struct dev_object **device_obj);

extern int proc_get_state(void *hprocessor, struct dsp_processorstate
				 *proc_state_obj, u32 state_info_size);

extern int proc_get_processor_id(void *proc, u32 * proc_id);

extern int proc_get_trace(void *hprocessor, u8 * pbuf, u32 max_size);

extern int proc_load(void *hprocessor,
			    const s32 argc_index, const char **user_args,
			    const char **user_envp);

extern int proc_register_notify(void *hprocessor,
				       u32 event_mask, u32 notify_type,
				       struct dsp_notification
				       *hnotification);

extern int proc_notify_clients(void *proc, u32 events);

extern int proc_notify_all_clients(void *proc, u32 events);

extern int proc_start(void *hprocessor);

extern int proc_stop(void *hprocessor);

extern int proc_end_dma(void *hprocessor, void *pmpu_addr, u32 ul_size,
						enum dma_data_direction dir);
extern int proc_begin_dma(void *hprocessor, void *pmpu_addr, u32 ul_size,
						enum dma_data_direction dir);

extern int proc_flush_memory(void *hprocessor,
				    void *pmpu_addr, u32 ul_size, u32 ul_flags);

extern int proc_invalidate_memory(void *hprocessor,
					 void *pmpu_addr, u32 ul_size);

extern int proc_map(void *hprocessor,
			   void *pmpu_addr,
			   u32 ul_size,
			   void *req_addr,
			   void **pp_map_addr, u32 ul_map_attr,
			   struct process_context *pr_ctxt);

extern int proc_reserve_memory(void *hprocessor,
				      u32 ul_size, void **pp_rsv_addr,
				      struct process_context *pr_ctxt);

extern int proc_un_map(void *hprocessor, void *map_addr,
			      struct process_context *pr_ctxt);

extern int proc_un_reserve_memory(void *hprocessor,
					 void *prsv_addr,
					 struct process_context *pr_ctxt);

#endif 
