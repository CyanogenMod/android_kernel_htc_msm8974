/*
 * node.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * DSP/BIOS Bridge Node Manager.
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

#ifndef NODE_
#define NODE_

#include <dspbridge/procpriv.h>

#include <dspbridge/nodedefs.h>
#include <dspbridge/disp.h>
#include <dspbridge/nldrdefs.h>
#include <dspbridge/drv.h>

extern int node_allocate(struct proc_object *hprocessor,
				const struct dsp_uuid *node_uuid,
				const struct dsp_cbdata
				*pargs, const struct dsp_nodeattrin
				*attr_in,
				struct node_res_object **noderes,
				struct process_context *pr_ctxt);

extern int node_alloc_msg_buf(struct node_object *hnode,
				     u32 usize, struct dsp_bufferattr
				     *pattr, u8 **pbuffer);

extern int node_change_priority(struct node_object *hnode, s32 prio);

extern int node_connect(struct node_object *node1,
			       u32 stream1,
			       struct node_object *node2,
			       u32 stream2,
			       struct dsp_strmattr *pattrs,
			       struct dsp_cbdata
			       *conn_param);

extern int node_create(struct node_object *hnode);

extern int node_create_mgr(struct node_mgr **node_man,
				  struct dev_object *hdev_obj);

extern int node_delete(struct node_res_object *noderes,
			      struct process_context *pr_ctxt);

extern int node_delete_mgr(struct node_mgr *hnode_mgr);

/*
 *  ======== node_enum_nodes ========
 *  Purpose:
 *      Enumerate the nodes currently allocated for the DSP.
 *  Parameters:
 *      hnode_mgr:       Node manager returned from node_create_mgr().
 *      node_tab:       Array to copy node handles into.
 *      node_tab_size:   Number of handles that can be written to node_tab.
 *      pu_num_nodes:     Location where number of node handles written to
 *                      node_tab will be written.
 *      pu_allocated:    Location to write total number of allocated nodes.
 *  Returns:
 *      0:        Success.
 *      -EINVAL:      node_tab is too small to hold all node handles.
 *  Requires:
 *      Valid hnode_mgr.
 *      node_tab != NULL || node_tab_size == 0.
 *      pu_num_nodes != NULL.
 *      pu_allocated != NULL.
 *  Ensures:
 *      - (-EINVAL && *pu_num_nodes == 0)
 *      - || (0 && *pu_num_nodes <= node_tab_size)  &&
 *        (*pu_allocated == *pu_num_nodes)
 */
extern int node_enum_nodes(struct node_mgr *hnode_mgr,
				  void **node_tab,
				  u32 node_tab_size,
				  u32 *pu_num_nodes,
				  u32 *pu_allocated);

extern int node_free_msg_buf(struct node_object *hnode,
				    u8 *pbuffer,
				    struct dsp_bufferattr
				    *pattr);

extern int node_get_attr(struct node_object *hnode,
				struct dsp_nodeattr *pattr, u32 attr_size);

extern int node_get_message(struct node_object *hnode,
				   struct dsp_msg *message, u32 utimeout);

extern int node_get_nldr_obj(struct node_mgr *hnode_mgr,
				    struct nldr_object **nldr_ovlyobj);

void node_on_exit(struct node_object *hnode, s32 node_status);

extern int node_pause(struct node_object *hnode);

extern int node_put_message(struct node_object *hnode,
				   const struct dsp_msg *pmsg, u32 utimeout);

extern int node_register_notify(struct node_object *hnode,
				       u32 event_mask, u32 notify_type,
				       struct dsp_notification
				       *hnotification);

extern int node_run(struct node_object *hnode);

extern int node_terminate(struct node_object *hnode,
				 int *pstatus);

extern int node_get_uuid_props(void *hprocessor,
				      const struct dsp_uuid *node_uuid,
				      struct dsp_ndbprops
				      *node_props);

#ifdef CONFIG_TIDSPBRIDGE_BACKTRACE
int node_find_addr(struct node_mgr *node_mgr, u32 sym_addr,
				u32 offset_range, void *sym_addr_output,
				char *sym_name);

enum node_state node_get_state(void *hnode);
#endif

#endif 
