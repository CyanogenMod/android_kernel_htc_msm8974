/*
 * _chnl_sm.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Private header file defining channel manager and channel objects for
 * a shared memory channel driver.
 *
 * Shared between the modules implementing the shared memory channel class
 * library.
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

#ifndef _CHNL_SM_
#define _CHNL_SM_

#include <dspbridge/dspapi.h>
#include <dspbridge/dspdefs.h>

#include <linux/list.h>
#include <dspbridge/ntfy.h>

#define CHNL_SHARED_BUFFER_BASE_SYM "_SHM_BEG"
#define CHNL_SHARED_BUFFER_LIMIT_SYM "_SHM_END"
#define BRIDGEINIT_BIOSGPTIMER "_BRIDGEINIT_BIOSGPTIMER"
#define BRIDGEINIT_LOADMON_GPTIMER "_BRIDGEINIT_LOADMON_GPTIMER"

#ifndef _CHNL_WORDSIZE
#define _CHNL_WORDSIZE 4	
#endif

#define MAXOPPS 16

#define SHM_CURROPP	0	
#define SHM_OPPINFO	1	
#define SHM_GETOPP	2	

struct opp_table_entry {
	u32 voltage;
	u32 frequency;
	u32 min_freq;
	u32 max_freq;
};

struct opp_struct {
	u32 curr_opp_pt;
	u32 num_opp_pts;
	struct opp_table_entry opp_point[MAXOPPS];
};

struct opp_rqst_struct {
	u32 rqst_dsp_freq;
	u32 rqst_opp_pt;
};

struct load_mon_struct {
	u32 curr_dsp_load;
	u32 curr_dsp_freq;
	u32 pred_dsp_load;
	u32 pred_dsp_freq;
};

struct shm {
	u32 dsp_free_mask;	/* Written by DSP, read by PC. */
	u32 host_free_mask;	/* Written by PC, read by DSP */

	u32 input_full;		
	u32 input_id;		
	u32 input_size;		

	u32 output_full;	
	u32 output_id;		
	u32 output_size;	

	u32 arg;		
	u32 resvd;		

	
	struct opp_struct opp_table_struct;
	
	struct opp_rqst_struct opp_request;
	
	struct load_mon_struct load_mon_info;
	
	u32 wdt_setclocks;
	u32 wdt_overflow;	
	char dummy[176];	
	u32 shm_dbg_var[64];	
};

	
struct chnl_mgr {
	
	struct bridge_drv_interface *intf_fxns;
	struct io_mgr *iomgr;	
	
	struct dev_object *dev_obj;

	
	u32 output_mask;	
	u32 last_output;	
	
	spinlock_t chnl_mgr_lock;
	u32 word_size;		
	u8 max_channels;	
	u8 open_channels;	
	struct chnl_object **channels;		
	u8 type;		
	
	int chnl_open_status;
};

struct chnl_object {
	
	struct chnl_mgr *chnl_mgr_obj;
	u32 chnl_id;		
	u8 state;		
	s8 chnl_mode;		
	
	void *user_event;
	
	struct sync_object *sync_event;
	u32 process;		
	u32 cb_arg;		
	struct list_head io_requests;	
	s32 cio_cs;		
	s32 cio_reqs;		
	s32 chnl_packets;	
	
	struct list_head io_completions;
	struct list_head free_packets_list;	
	struct ntfy_object *ntfy_obj;
	u32 bytes_moved;	

	

	
	u32 chnl_type;
};

struct chnl_irp {
	struct list_head link;	
	
	u8 *host_user_buf;
	
	u8 *host_sys_buf;
	u32 arg;		
	u32 dsp_tx_addr;	
	u32 byte_size;		
	u32 buf_size;		
	u32 status;		
};

#endif 
