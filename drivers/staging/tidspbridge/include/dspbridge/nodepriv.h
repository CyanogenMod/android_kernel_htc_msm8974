/*
 * nodepriv.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Private node header shared by NODE and DISP.
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

#ifndef NODEPRIV_
#define NODEPRIV_

#include <dspbridge/strmdefs.h>
#include <dspbridge/nodedefs.h>
#include <dspbridge/nldrdefs.h>

typedef u32 nodeenv;


struct node_msgargs {
	u32 max_msgs;		
	u32 seg_id;		
	u32 notify_type;	
	u32 arg_length;		
	u8 *pdata;		
};

struct node_strmdef {
	u32 buf_size;		
	u32 num_bufs;		
	u32 seg_id;		
	u32 timeout;		
	u32 buf_alignment;	
	char *sz_device;	
};

struct node_taskargs {
	struct node_msgargs node_msg_args;
	s32 prio;
	u32 stack_size;
	u32 sys_stack_size;
	u32 stack_seg;
	u32 dsp_heap_res_addr;	
	u32 dsp_heap_addr;	
	u32 heap_size;		
	u32 gpp_heap_addr;	
	u32 profile_id;		
	u32 num_inputs;
	u32 num_outputs;
	u32 dais_arg;	
	struct node_strmdef *strm_in_def;
	struct node_strmdef *strm_out_def;
};

struct node_createargs {
	union {
		struct node_msgargs node_msg_args;
		struct node_taskargs task_arg_obj;
	} asa;
};

extern int node_get_channel_id(struct node_object *hnode,
				      u32 dir, u32 index, u32 *chan_id);

extern int node_get_strm_mgr(struct node_object *hnode,
				    struct strm_mgr **strm_man);

extern u32 node_get_timeout(struct node_object *hnode);

extern enum node_type node_get_type(struct node_object *hnode);

extern void get_node_info(struct node_object *hnode,
			  struct dsp_nodeinfo *node_info);

extern enum nldr_loadtype node_get_load_type(struct node_object *hnode);

#endif 
