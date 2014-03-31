/*
 * _msg_sm.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Private header file defining msg_ctrl manager objects and defines needed
 * by IO manager.
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

#ifndef _MSG_SM_
#define _MSG_SM_

#include <linux/list.h>
#include <dspbridge/msgdefs.h>

#define MSG_SHARED_BUFFER_BASE_SYM      "_MSG_BEG"
#define MSG_SHARED_BUFFER_LIMIT_SYM     "_MSG_END"

#ifndef _CHNL_WORDSIZE
#define _CHNL_WORDSIZE 4	
#endif

/*
 *  ======== msg_ctrl ========
 *  There is a control structure for messages to the DSP, and a control
 *  structure for messages from the DSP. The shared memory region for
 *  transferring messages is partitioned as follows:
 *
 *  ----------------------------------------------------------
 *  |Control | Messages from DSP | Control | Messages to DSP |
 *  ----------------------------------------------------------
 *
 *  msg_ctrl control structure for messages to the DSP is used in the following
 *  way:
 *
 *  buf_empty -      This flag is set to FALSE by the GPP after it has output
 *                  messages for the DSP. The DSP host driver sets it to
 *                  TRUE after it has copied the messages.
 *  post_swi -       Set to 1 by the GPP after it has written the messages,
 *                  set the size, and set buf_empty to FALSE.
 *                  The DSP Host driver uses SWI_andn of the post_swi field
 *                  when a host interrupt occurs. The host driver clears
 *                  this after posting the SWI.
 *  size -          Number of messages to be read by the DSP.
 *
 *  For messages from the DSP:
 *  buf_empty -      This flag is set to FALSE by the DSP after it has output
 *                  messages for the GPP. The DPC on the GPP sets it to
 *                  TRUE after it has copied the messages.
 *  post_swi -       Set to 1 the DPC on the GPP after copying the messages.
 *  size -          Number of messages to be read by the GPP.
 */
struct msg_ctrl {
	u32 buf_empty;		
	u32 post_swi;		
	u32 size;		
	u32 resvd;
};

struct msg_mgr {
	

	
	struct bridge_drv_interface *intf_fxns;

	struct io_mgr *iomgr;		
	struct list_head queue_list;	
	spinlock_t msg_mgr_lock;	
	
	struct sync_object *sync_event;
	struct list_head msg_free_list;	
	struct list_head msg_used_list;	
	u32 msgs_pending;	
	u32 max_msgs;		
	msg_onexit on_exit;	
};

struct msg_queue {
	struct list_head list_elem;
	struct msg_mgr *msg_mgr;
	u32 max_msgs;		
	u32 msgq_id;		
	struct list_head msg_free_list;	
	
	struct list_head msg_used_list;
	void *arg;		
	struct sync_object *sync_event;	
	struct sync_object *sync_done;	
	struct sync_object *sync_done_ack;	
	struct ntfy_object *ntfy_obj;	
	bool done;		
	u32 io_msg_pend;	
};

struct msg_dspmsg {
	struct dsp_msg msg;
	u32 msgq_id;		
};

struct msg_frame {
	struct list_head list_elem;
	struct msg_dspmsg msg_data;
};

#endif 
