/*
 * strm.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * DSPBridge Stream Manager.
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

#ifndef STRM_
#define STRM_

#include <dspbridge/dev.h>

#include <dspbridge/strmdefs.h>
#include <dspbridge/proc.h>

extern int strm_allocate_buffer(struct strm_res_object *strmres,
				       u32 usize,
				       u8 **ap_buffer,
				       u32 num_bufs,
				       struct process_context *pr_ctxt);

extern int strm_close(struct strm_res_object *strmres,
			     struct process_context *pr_ctxt);

extern int strm_create(struct strm_mgr **strm_man,
			      struct dev_object *dev_obj);

extern void strm_delete(struct strm_mgr *strm_mgr_obj);

extern int strm_free_buffer(struct strm_res_object *strmres,
				   u8 **ap_buffer, u32 num_bufs,
				   struct process_context *pr_ctxt);

extern int strm_get_info(struct strm_object *stream_obj,
				struct stream_info *stream_info,
				u32 stream_info_size);

extern int strm_idle(struct strm_object *stream_obj, bool flush_data);

extern int strm_issue(struct strm_object *stream_obj, u8 * pbuf,
			     u32 ul_bytes, u32 ul_buf_size, u32 dw_arg);

extern int strm_open(struct node_object *hnode, u32 dir,
			    u32 index, struct strm_attr *pattr,
			    struct strm_res_object **strmres,
			    struct process_context *pr_ctxt);

/*
 *  ======== strm_reclaim ========
 *  Purpose:
 *      Request a buffer back from a stream.
 *  Parameters:
 *      stream_obj:          Stream handle returned from strm_open().
 *      buf_ptr:        Location to store pointer to reclaimed buffer.
 *      nbytes:         Location where number of bytes of data in the
 *                      buffer will be written.
 *      buff_size:      Location where actual buffer size will be written.
 *      pdw_arg:         Location where user argument that travels with
 *                      the buffer will be written.
 *  Returns:
 *      0:        Success.
 *      -EFAULT:    Invalid stream_obj.
 *      -ETIME:   A timeout occurred before a buffer could be
 *                      retrieved.
 *      -EPERM:      Failure occurred, unable to reclaim buffer.
 *  Requires:
 *      buf_ptr != NULL.
 *      nbytes != NULL.
 *      pdw_arg != NULL.
 *  Ensures:
 */
extern int strm_reclaim(struct strm_object *stream_obj,
			       u8 **buf_ptr, u32 * nbytes,
			       u32 *buff_size, u32 *pdw_arg);

extern int strm_register_notify(struct strm_object *stream_obj,
				       u32 event_mask, u32 notify_type,
				       struct dsp_notification
				       *hnotification);

extern int strm_select(struct strm_object **strm_tab,
			      u32 strms, u32 *pmask, u32 utimeout);

#endif 
