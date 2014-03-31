/*
 * dbdcd.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Defines the DSP/BIOS Bridge Configuration Database (DCD) API.
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

#ifndef DBDCD_
#define DBDCD_

#include <dspbridge/dbdcddef.h>
#include <dspbridge/host_os.h>
#include <dspbridge/nldrdefs.h>

extern int dcd_auto_register(struct dcd_manager *hdcd_mgr,
				    char *sz_coff_path);

extern int dcd_auto_unregister(struct dcd_manager *hdcd_mgr,
				      char *sz_coff_path);

extern int dcd_create_manager(char *sz_zl_dll_name,
				     struct dcd_manager **dcd_mgr);

extern int dcd_destroy_manager(struct dcd_manager *hdcd_mgr);

extern int dcd_enumerate_object(s32 index,
				       enum dsp_dcdobjtype obj_type,
				       struct dsp_uuid *uuid_obj);

extern void dcd_exit(void);

extern int dcd_get_dep_libs(struct dcd_manager *hdcd_mgr,
				   struct dsp_uuid *uuid_obj,
				   u16 num_libs,
				   struct dsp_uuid *dep_lib_uuids,
				   bool *prstnt_dep_libs,
				   enum nldr_phase phase);

extern int dcd_get_num_dep_libs(struct dcd_manager *hdcd_mgr,
				       struct dsp_uuid *uuid_obj,
				       u16 *num_libs,
				       u16 *num_pers_libs,
				       enum nldr_phase phase);

extern int dcd_get_library_name(struct dcd_manager *hdcd_mgr,
				       struct dsp_uuid *uuid_obj,
				       char *str_lib_name,
				       u32 *buff_size,
				       enum nldr_phase phase,
				       bool *phase_split);

extern int dcd_get_object_def(struct dcd_manager *hdcd_mgr,
				     struct dsp_uuid *obj_uuid,
				     enum dsp_dcdobjtype obj_type,
				     struct dcd_genericobj *obj_def);

extern int dcd_get_objects(struct dcd_manager *hdcd_mgr,
				  char *sz_coff_path,
				  dcd_registerfxn register_fxn, void *handle);

extern bool dcd_init(void);

extern int dcd_register_object(struct dsp_uuid *uuid_obj,
				      enum dsp_dcdobjtype obj_type,
				      char *psz_path_name);

extern int dcd_unregister_object(struct dsp_uuid *uuid_obj,
					enum dsp_dcdobjtype obj_type);

#endif 
