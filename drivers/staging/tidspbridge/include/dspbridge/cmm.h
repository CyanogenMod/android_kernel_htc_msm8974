/*
 * cmm.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * The Communication Memory Management(CMM) module provides shared memory
 * management services for DSP/BIOS Bridge data streaming and messaging.
 * Multiple shared memory segments can be registered with CMM. Memory is
 * coelesced back to the appropriate pool when a buffer is freed.
 *
 * The CMM_Xlator[xxx] functions are used for node messaging and data
 * streaming address translation to perform zero-copy inter-processor
 * data transfer(GPP<->DSP). A "translator" object is created for a node or
 * stream object that contains per thread virtual address information. This
 * translator info is used at runtime to perform SM address translation
 * to/from the DSP address space.
 *
 * Notes:
 *   cmm_xlator_alloc_buf - Used by Node and Stream modules for SM address
 *			  translation.
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

#ifndef CMM_
#define CMM_

#include <dspbridge/devdefs.h>

#include <dspbridge/cmmdefs.h>
#include <dspbridge/host_os.h>

extern void *cmm_calloc_buf(struct cmm_object *hcmm_mgr,
			    u32 usize, struct cmm_attrs *pattrs,
			    void **pp_buf_va);

extern int cmm_create(struct cmm_object **ph_cmm_mgr,
			     struct dev_object *hdev_obj,
			     const struct cmm_mgrattrs *mgr_attrts);

extern int cmm_destroy(struct cmm_object *hcmm_mgr, bool force);

extern int cmm_free_buf(struct cmm_object *hcmm_mgr,
			       void *buf_pa, u32 ul_seg_id);

extern int cmm_get_handle(void *hprocessor,
				 struct cmm_object **ph_cmm_mgr);

extern int cmm_get_info(struct cmm_object *hcmm_mgr,
			       struct cmm_info *cmm_info_obj);

extern int cmm_register_gppsm_seg(struct cmm_object *hcmm_mgr,
					 unsigned int dw_gpp_base_pa,
					 u32 ul_size,
					 u32 dsp_addr_offset,
					 s8 c_factor,
					 unsigned int dw_dsp_base,
					 u32 ul_dsp_size,
					 u32 *sgmt_id, u32 gpp_base_va);

extern int cmm_un_register_gppsm_seg(struct cmm_object *hcmm_mgr,
					    u32 ul_seg_id);

extern void *cmm_xlator_alloc_buf(struct cmm_xlatorobject *xlator,
				  void *va_buf, u32 pa_size);

extern int cmm_xlator_create(struct cmm_xlatorobject **xlator,
				    struct cmm_object *hcmm_mgr,
				    struct cmm_xlatorattrs *xlator_attrs);

extern int cmm_xlator_free_buf(struct cmm_xlatorobject *xlator,
				      void *buf_va);

extern int cmm_xlator_info(struct cmm_xlatorobject *xlator,
				  u8 **paddr,
				  u32 ul_size, u32 segm_id, bool set_info);

extern void *cmm_xlator_translate(struct cmm_xlatorobject *xlator,
				  void *paddr, enum cmm_xlatetype xtype);

#endif 
