/*
 * cod.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Code management module for DSPs. This module provides an interface
 * interface for loading both static and dynamic code objects onto DSP
 * systems.
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

#ifndef COD_
#define COD_

#include <dspbridge/dblldefs.h>

#define COD_MAXPATHLENGTH       255
#define COD_TRACEBEG            "SYS_PUTCBEG"
#define COD_TRACEEND            "SYS_PUTCEND"
#define COD_TRACECURPOS	"BRIDGE_SYS_PUTC_current"

#define COD_NOLOAD              DBLL_NOLOAD
#define COD_SYMB                DBLL_SYMB

struct cod_manager;

struct cod_libraryobj;

typedef u32(*cod_writefxn) (void *priv_ref, u32 dsp_add,
			    void *pbuf, u32 ul_num_bytes, u32 mem_space);

extern void cod_close(struct cod_libraryobj *lib);

extern int cod_create(struct cod_manager **mgr,
			     char *str_zl_file);

extern void cod_delete(struct cod_manager *cod_mgr_obj);

extern int cod_get_base_lib(struct cod_manager *cod_mgr_obj,
				   struct dbll_library_obj **plib);

extern int cod_get_base_name(struct cod_manager *cod_mgr_obj,
				    char *sz_name, u32 usize);

extern int cod_get_entry(struct cod_manager *cod_mgr_obj,
				u32 *entry_pt);

extern int cod_get_loader(struct cod_manager *cod_mgr_obj,
				 struct dbll_tar_obj **loader);

extern int cod_get_section(struct cod_libraryobj *lib,
				  char *str_sect,
				  u32 *addr, u32 *len);

extern int cod_get_sym_value(struct cod_manager *cod_mgr_obj,
				    char *str_sym, u32 * pul_value);

extern int cod_load_base(struct cod_manager *cod_mgr_obj,
				u32 num_argc, char *args[],
				cod_writefxn pfn_write, void *arb,
				char *envp[]);

extern int cod_open(struct cod_manager *hmgr,
			   char *sz_coff_path,
			   u32 flags, struct cod_libraryobj **lib_obj);

extern int cod_open_base(struct cod_manager *hmgr, char *sz_coff_path,
				dbll_flags flags);

extern int cod_read_section(struct cod_libraryobj *lib,
				   char *str_sect,
				   char *str_content, u32 content_size);

#endif 
