/*
 * disp.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * DSP/BIOS Bridge Node Dispatcher.
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

#ifndef DISP_
#define DISP_

#include <dspbridge/dbdefs.h>
#include <dspbridge/nodedefs.h>
#include <dspbridge/nodepriv.h>

struct disp_object;

struct disp_attr {
	u32 chnl_offset;	
	
	u32 chnl_buf_size;
	int proc_family;	
	int proc_type;		
	void *reserved1;	
	u32 reserved2;		
};


extern int disp_create(struct disp_object **dispatch_obj,
			      struct dev_object *hdev_obj,
			      const struct disp_attr *disp_attrs);

extern void disp_delete(struct disp_object *disp_obj);

extern int disp_node_change_priority(struct disp_object
					    *disp_obj,
					    struct node_object *hnode,
					    u32 rms_fxn,
					    nodeenv node_env, s32 prio);

extern int disp_node_create(struct disp_object *disp_obj,
				   struct node_object *hnode,
				   u32 rms_fxn,
				   u32 ul_create_fxn,
				   const struct node_createargs
				   *pargs, nodeenv *node_env);

extern int disp_node_delete(struct disp_object *disp_obj,
				   struct node_object *hnode,
				   u32 rms_fxn,
				   u32 ul_delete_fxn, nodeenv node_env);

extern int disp_node_run(struct disp_object *disp_obj,
				struct node_object *hnode,
				u32 rms_fxn,
				u32 ul_execute_fxn, nodeenv node_env);

#endif 
