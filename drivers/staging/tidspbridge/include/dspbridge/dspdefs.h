/*
 * dspdefs.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Bridge driver entry point and interface function declarations.
 *
 * Notes:
 *   The DSP API obtains it's function interface to
 *   the Bridge driver via a call to bridge_drv_entry().
 *
 *   Bridge services exported to Bridge drivers are initialized by the
 *   DSP API on behalf of the Bridge driver.
 *
 *   Bridge function DBC Requires and Ensures are also made by the DSP API on
 *   behalf of the Bridge driver, to simplify the Bridge driver code.
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

#ifndef DSPDEFS_
#define DSPDEFS_

#include <dspbridge/brddefs.h>
#include <dspbridge/cfgdefs.h>
#include <dspbridge/chnlpriv.h>
#include <dspbridge/dspdeh.h>
#include <dspbridge/devdefs.h>
#include <dspbridge/io.h>
#include <dspbridge/msgdefs.h>

struct bridge_dev_context;


typedef int(*fxn_brd_monitor) (struct bridge_dev_context *dev_ctxt);

typedef int(*fxn_brd_setstate) (struct bridge_dev_context
				       * dev_ctxt, u32 brd_state);

typedef int(*fxn_brd_start) (struct bridge_dev_context
				    * dev_ctxt, u32 dsp_addr);

typedef int(*fxn_brd_memcopy) (struct bridge_dev_context
				      * dev_ctxt,
				      u32 dsp_dest_addr,
				      u32 dsp_src_addr,
				      u32 ul_num_bytes, u32 mem_type);
typedef int(*fxn_brd_memwrite) (struct bridge_dev_context
				       * dev_ctxt,
				       u8 *host_buf,
				       u32 dsp_addr, u32 ul_num_bytes,
				       u32 mem_type);

typedef int(*fxn_brd_memmap) (struct bridge_dev_context
				     * dev_ctxt, u32 ul_mpu_addr,
				     u32 virt_addr, u32 ul_num_bytes,
				     u32 map_attr,
				     struct page **mapped_pages);

typedef int(*fxn_brd_memunmap) (struct bridge_dev_context
				       * dev_ctxt,
				       u32 virt_addr, u32 ul_num_bytes);

typedef int(*fxn_brd_stop) (struct bridge_dev_context *dev_ctxt);

typedef int(*fxn_brd_status) (struct bridge_dev_context *dev_ctxt,
				     int *board_state);

typedef int(*fxn_brd_read) (struct bridge_dev_context *dev_ctxt,
				   u8 *host_buf,
				   u32 dsp_addr,
				   u32 ul_num_bytes, u32 mem_type);

typedef int(*fxn_brd_write) (struct bridge_dev_context *dev_ctxt,
				    u8 *host_buf,
				    u32 dsp_addr,
				    u32 ul_num_bytes, u32 mem_type);

typedef int(*fxn_chnl_create) (struct chnl_mgr
				      **channel_mgr,
				      struct dev_object
				      * hdev_obj,
				      const struct
				      chnl_mgrattrs * mgr_attrts);

typedef int(*fxn_chnl_destroy) (struct chnl_mgr *hchnl_mgr);
typedef void (*fxn_deh_notify) (struct deh_mgr *hdeh_mgr,
				u32 evnt_mask, u32 error_info);

typedef int(*fxn_chnl_open) (struct chnl_object
				    **chnl,
				    struct chnl_mgr *hchnl_mgr,
				    s8 chnl_mode,
				    u32 ch_id,
				    const struct
				    chnl_attr * pattrs);

typedef int(*fxn_chnl_close) (struct chnl_object *chnl_obj);

typedef int(*fxn_chnl_addioreq) (struct chnl_object
					* chnl_obj,
					void *host_buf,
					u32 byte_size,
					u32 buf_size,
					u32 dw_dsp_addr, u32 dw_arg);

typedef int(*fxn_chnl_getioc) (struct chnl_object *chnl_obj,
				      u32 timeout,
				      struct chnl_ioc *chan_ioc);

typedef int(*fxn_chnl_cancelio) (struct chnl_object *chnl_obj);

typedef int(*fxn_chnl_flushio) (struct chnl_object *chnl_obj,
				       u32 timeout);

typedef int(*fxn_chnl_getinfo) (struct chnl_object *chnl_obj,
				       struct chnl_info *channel_info);

typedef int(*fxn_chnl_getmgrinfo) (struct chnl_mgr
					  * hchnl_mgr,
					  u32 ch_id,
					  struct chnl_mgrinfo *mgr_info);

typedef int(*fxn_chnl_idle) (struct chnl_object *chnl_obj,
				    u32 timeout, bool flush_data);

typedef int(*fxn_chnl_registernotify)
 (struct chnl_object *chnl_obj,
  u32 event_mask, u32 notify_type, struct dsp_notification *hnotification);

typedef int(*fxn_dev_create) (struct bridge_dev_context
				     **device_ctx,
				     struct dev_object
				     * hdev_obj,
				     struct cfg_hostres
				     * config_param);

typedef int(*fxn_dev_ctrl) (struct bridge_dev_context *dev_ctxt,
				   u32 dw_cmd, void *pargs);

typedef int(*fxn_dev_destroy) (struct bridge_dev_context *dev_ctxt);

typedef int(*fxn_io_create) (struct io_mgr **io_man,
				    struct dev_object *hdev_obj,
				    const struct io_attrs *mgr_attrts);

typedef int(*fxn_io_destroy) (struct io_mgr *hio_mgr);

typedef int(*fxn_io_onloaded) (struct io_mgr *hio_mgr);

typedef int(*fxn_io_getprocload) (struct io_mgr *hio_mgr,
					 struct dsp_procloadstat *
					 proc_load_stat);

typedef int(*fxn_msg_create)
 (struct msg_mgr **msg_man,
  struct dev_object *hdev_obj, msg_onexit msg_callback);

typedef int(*fxn_msg_createqueue)
 (struct msg_mgr *hmsg_mgr,
  struct msg_queue **msgq, u32 msgq_id, u32 max_msgs, void *h);

typedef void (*fxn_msg_delete) (struct msg_mgr *hmsg_mgr);

typedef void (*fxn_msg_deletequeue) (struct msg_queue *msg_queue_obj);

typedef int(*fxn_msg_get) (struct msg_queue *msg_queue_obj,
				  struct dsp_msg *pmsg, u32 utimeout);

typedef int(*fxn_msg_put) (struct msg_queue *msg_queue_obj,
				  const struct dsp_msg *pmsg, u32 utimeout);

typedef int(*fxn_msg_registernotify)
 (struct msg_queue *msg_queue_obj,
  u32 event_mask, u32 notify_type, struct dsp_notification *hnotification);

typedef void (*fxn_msg_setqueueid) (struct msg_queue *msg_queue_obj,
				    u32 msgq_id);

struct bridge_drv_interface {
	u32 brd_api_major_version;	
	u32 brd_api_minor_version;	
	fxn_dev_create dev_create;	
	fxn_dev_destroy dev_destroy;	
	fxn_dev_ctrl dev_cntrl;	
	fxn_brd_monitor brd_monitor;	
	fxn_brd_start brd_start;	
	fxn_brd_stop brd_stop;	
	fxn_brd_status brd_status;	
	fxn_brd_read brd_read;	
	fxn_brd_write brd_write;	
	fxn_brd_setstate brd_set_state;	
	fxn_brd_memcopy brd_mem_copy;	
	fxn_brd_memwrite brd_mem_write;	
	fxn_brd_memmap brd_mem_map;	
	fxn_brd_memunmap brd_mem_un_map;	
	fxn_chnl_create chnl_create;	
	fxn_chnl_destroy chnl_destroy;	
	fxn_chnl_open chnl_open;	
	fxn_chnl_close chnl_close;	
	fxn_chnl_addioreq chnl_add_io_req;	
	fxn_chnl_getioc chnl_get_ioc;	
	fxn_chnl_cancelio chnl_cancel_io;	
	fxn_chnl_flushio chnl_flush_io;	
	fxn_chnl_getinfo chnl_get_info;	
	
	fxn_chnl_getmgrinfo chnl_get_mgr_info;
	fxn_chnl_idle chnl_idle;	
	
	fxn_chnl_registernotify chnl_register_notify;
	fxn_io_create io_create;	
	fxn_io_destroy io_destroy;	
	fxn_io_onloaded io_on_loaded;	
	
	fxn_io_getprocload io_get_proc_load;
	fxn_msg_create msg_create;	
	
	fxn_msg_createqueue msg_create_queue;
	fxn_msg_delete msg_delete;	
	
	fxn_msg_deletequeue msg_delete_queue;
	fxn_msg_get msg_get;	
	fxn_msg_put msg_put;	
	
	fxn_msg_registernotify msg_register_notify;
	
	fxn_msg_setqueueid msg_set_queue_id;
};

void bridge_drv_entry(struct bridge_drv_interface **drv_intf,
		   const char *driver_file_name);

#endif 
