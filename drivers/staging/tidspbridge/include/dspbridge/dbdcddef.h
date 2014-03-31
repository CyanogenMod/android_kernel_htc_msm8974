/*
 * dbdcddef.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * DCD (DSP/BIOS Bridge Configuration Database) constants and types.
 *
 * Copyright (C) 2008 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef DBDCDDEF_
#define DBDCDDEF_

#include <dspbridge/dbdefs.h>
#include <dspbridge/mgrpriv.h>	

#define DCD_REGKEY              "Software\\TexasInstruments\\DspBridge\\DCD"
#define DCD_REGISTER_SECTION    ".dcd_register"

#define DCD_MAXPATHLENGTH    255

struct dcd_manager;

struct dcd_key_elem {
	struct list_head link;	
	char name[DCD_MAXPATHLENGTH];	
	char *path;		
};

struct dcd_nodeprops {
	struct dsp_ndbprops ndb_props;
	u32 msg_segid;
	u32 msg_notify_type;
	char *str_create_phase_fxn;
	char *str_delete_phase_fxn;
	char *str_execute_phase_fxn;
	char *str_i_alg_name;

	
	u16 load_type;	
	u32 data_mem_seg_mask;		
	u32 code_mem_seg_mask;		
};

struct dcd_genericobj {
	union dcd_obj {
		struct dcd_nodeprops node_obj;	
		
		struct dsp_processorinfo proc_info;
		
		struct mgr_processorextinfo ext_proc_obj;
	} obj_data;
};

typedef int(*dcd_registerfxn) (struct dsp_uuid *uuid_obj,
				      enum dsp_dcdobjtype obj_type,
				      void *handle);

#endif 
