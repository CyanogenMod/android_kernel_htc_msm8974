/*
 * nldrdefs.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Global Dynamic + static/overlay Node loader (NLDR) constants and types.
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

#ifndef NLDRDEFS_
#define NLDRDEFS_

#include <dspbridge/dbdcddef.h>
#include <dspbridge/devdefs.h>

#define NLDR_MAXPATHLENGTH       255
struct nldr_object;
struct nldr_nodeobject;

enum nldr_loadtype {
	NLDR_STATICLOAD,	
	NLDR_DYNAMICLOAD,	
	NLDR_OVLYLOAD		
};

typedef u32(*nldr_ovlyfxn) (void *priv_ref, u32 dsp_run_addr,
			    u32 dsp_load_addr, u32 ul_num_bytes, u32 mem_space);

/*
 *  ======== nldr_writefxn ========
 *  Write memory function. Used for dynamic load writes.
 *  Parameters:
 *      priv_ref:       Handle to identify the node.
 *      dsp_add:        Address of code or data.
 *      pbuf:           Code or data to be written
 *      ul_num_bytes:     Number of (GPP) bytes to write.
 *      mem_space:      DBLL_DATA or DBLL_CODE.
 *  Returns:
 *      ul_num_bytes:     Success.
 *      0:              Failure.
 *  Requires:
 *  Ensures:
 */
typedef u32(*nldr_writefxn) (void *priv_ref,
			     u32 dsp_add, void *pbuf,
			     u32 ul_num_bytes, u32 mem_space);

struct nldr_attrs {
	nldr_ovlyfxn ovly;
	nldr_writefxn write;
	u16 dsp_word_size;
	u16 dsp_mau_size;
};

enum nldr_phase {
	NLDR_CREATE,
	NLDR_DELETE,
	NLDR_EXECUTE,
	NLDR_NOPHASE
};


typedef int(*nldr_allocatefxn) (struct nldr_object *nldr_obj,
				       void *priv_ref,
				       const struct dcd_nodeprops
				       * node_props,
				       struct nldr_nodeobject
				       **nldr_nodeobj,
				       bool *pf_phase_split);

typedef int(*nldr_createfxn) (struct nldr_object **nldr,
				     struct dev_object *hdev_obj,
				     const struct nldr_attrs *pattrs);

typedef void (*nldr_deletefxn) (struct nldr_object *nldr_obj);

typedef void (*nldr_freefxn) (struct nldr_nodeobject *nldr_node_obj);

typedef int(*nldr_getfxnaddrfxn) (struct nldr_nodeobject
					 * nldr_node_obj,
					 char *str_fxn, u32 * addr);

typedef int(*nldr_loadfxn) (struct nldr_nodeobject *nldr_node_obj,
				   enum nldr_phase phase);

typedef int(*nldr_unloadfxn) (struct nldr_nodeobject *nldr_node_obj,
				     enum nldr_phase phase);

struct node_ldr_fxns {
	nldr_allocatefxn allocate;
	nldr_createfxn create;
	nldr_deletefxn delete;
	nldr_getfxnaddrfxn get_fxn_addr;
	nldr_loadfxn load;
	nldr_unloadfxn unload;
};

#endif 
