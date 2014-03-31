/*
 * mgr.h
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

#ifndef MGR_
#define MGR_

#include <dspbridge/mgrpriv.h>

#define MAX_EVENTS 32


int mgr_wait_for_bridge_events(struct dsp_notification
				      **anotifications,
				      u32 count, u32 *pu_index,
				      u32 utimeout);

extern int mgr_create(struct mgr_object **mgr_obj,
			     struct cfg_devnode *dev_node_obj);

extern int mgr_destroy(struct mgr_object *hmgr_obj);

extern int mgr_enum_node_info(u32 node_id,
				     struct dsp_ndbprops *pndb_props,
				     u32 undb_props_size,
				     u32 *pu_num_nodes);

extern int mgr_enum_processor_info(u32 processor_id,
					  struct dsp_processorinfo
					  *processor_info,
					  u32 processor_info_size,
					  u8 *pu_num_procs);
extern void mgr_exit(void);

extern int mgr_get_dcd_handle(struct mgr_object
				     *mgr_handle, u32 *dcd_handle);

extern bool mgr_init(void);

#endif 
