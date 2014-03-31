/*
 * dev.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Bridge Bridge driver device operations.
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

#ifndef DEV_
#define DEV_

#include <dspbridge/chnldefs.h>
#include <dspbridge/cmm.h>
#include <dspbridge/cod.h>
#include <dspbridge/dspdeh.h>
#include <dspbridge/nodedefs.h>
#include <dspbridge/disp.h>
#include <dspbridge/dspdefs.h>
#include <dspbridge/dmm.h>
#include <dspbridge/host_os.h>

#include <dspbridge/devdefs.h>

/*
 *  ======== dev_brd_write_fxn ========
 *  Purpose:
 *      Exported function to be used as the COD write function.  This function
 *      is passed a handle to a DEV_hObject by ZL in arb, then calls the
 *      device's bridge_brd_write() function.
 *  Parameters:
 *      arb:           Handle to a Device Object.
 *      dev_ctxt:    Handle to Bridge driver defined device info.
 *      dsp_addr:       Address on DSP board (Destination).
 *      host_buf:       Pointer to host buffer (Source).
 *      ul_num_bytes:     Number of bytes to transfer.
 *      mem_type:       Memory space on DSP to which to transfer.
 *  Returns:
 *      Number of bytes written.  Returns 0 if the DEV_hObject passed in via
 *      arb is invalid.
 *  Requires:
 *      DEV Initialized.
 *      host_buf != NULL
 *  Ensures:
 */
extern u32 dev_brd_write_fxn(void *arb,
			     u32 dsp_add,
			     void *host_buf, u32 ul_num_bytes, u32 mem_space);

extern int dev_create_device(struct dev_object
				    **device_obj,
				    const char *driver_file_name,
				    struct cfg_devnode *dev_node_obj);

extern int dev_create2(struct dev_object *hdev_obj);

extern int dev_destroy2(struct dev_object *hdev_obj);

extern int dev_destroy_device(struct dev_object
				     *hdev_obj);

extern int dev_get_chnl_mgr(struct dev_object *hdev_obj,
				   struct chnl_mgr **mgr);

extern int dev_get_cmm_mgr(struct dev_object *hdev_obj,
				  struct cmm_object **mgr);

extern int dev_get_dmm_mgr(struct dev_object *hdev_obj,
				  struct dmm_object **mgr);

extern int dev_get_cod_mgr(struct dev_object *hdev_obj,
				  struct cod_manager **cod_mgr);

extern int dev_get_deh_mgr(struct dev_object *hdev_obj,
				  struct deh_mgr **deh_manager);

extern int dev_get_dev_node(struct dev_object *hdev_obj,
				   struct cfg_devnode **dev_nde);

extern int dev_get_dev_type(struct dev_object *device_obj,
					u8 *dev_type);

extern struct dev_object *dev_get_first(void);

extern int dev_get_intf_fxns(struct dev_object *hdev_obj,
			    struct bridge_drv_interface **if_fxns);

extern int dev_get_io_mgr(struct dev_object *hdev_obj,
				 struct io_mgr **mgr);

extern struct dev_object *dev_get_next(struct dev_object
				       *hdev_obj);

extern void dev_get_msg_mgr(struct dev_object *hdev_obj,
			    struct msg_mgr **msg_man);

extern int dev_get_node_manager(struct dev_object
				       *hdev_obj,
				       struct node_mgr **node_man);

extern int dev_get_symbol(struct dev_object *hdev_obj,
				 const char *str_sym, u32 * pul_value);

extern int dev_get_bridge_context(struct dev_object *hdev_obj,
				      struct bridge_dev_context
				      **phbridge_context);

extern int dev_insert_proc_object(struct dev_object
					 *hdev_obj,
					 u32 proc_obj,
					 bool *already_attached);

extern int dev_remove_proc_object(struct dev_object
					 *hdev_obj, u32 proc_obj);

extern int dev_notify_clients(struct dev_object *hdev_obj, u32 ret);

extern int dev_remove_device(struct cfg_devnode *dev_node_obj);

extern int dev_set_chnl_mgr(struct dev_object *hdev_obj,
				   struct chnl_mgr *hmgr);

extern void dev_set_msg_mgr(struct dev_object *hdev_obj, struct msg_mgr *hmgr);

extern int dev_start_device(struct cfg_devnode *dev_node_obj);

#endif 
