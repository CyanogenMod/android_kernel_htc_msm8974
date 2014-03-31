/*
 * dblldefs.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
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

#ifndef DBLLDEFS_
#define DBLLDEFS_

#define DBLL_NOLOAD   0x0	
#define DBLL_SYMB     0x1	
#define DBLL_CODE     0x2	
#define DBLL_DATA     0x4	
#define DBLL_DYNAMIC  0x8	
#define DBLL_BSS      0x20	

#define DBLL_MAXPATHLENGTH       255

struct dbll_tar_obj;

typedef s32 dbll_flags;

struct dbll_library_obj;

struct dbll_sect_info {
	const char *name;	
	u32 sect_run_addr;	
	u32 sect_load_addr;	
	u32 size;		
	dbll_flags type;	
};

struct dbll_sym_val {
	u32 value;
};

typedef s32(*dbll_alloc_fxn) (void *hdl, s32 space, u32 size, u32 align,
			      u32 *dsp_address, s32 seg_id, s32 req,
			      bool reserved);

typedef s32(*dbll_f_close_fxn) (void *);

typedef bool(*dbll_free_fxn) (void *hdl, u32 addr, s32 space, u32 size,
			      bool reserved);

typedef void *(*dbll_f_open_fxn) (const char *, const char *);

typedef int(*dbll_log_write_fxn) (void *handle,
					 struct dbll_sect_info *sect, u32 addr,
					 u32 bytes);

typedef s32(*dbll_read_fxn) (void *, size_t, size_t, void *);

typedef s32(*dbll_seek_fxn) (void *, long, int);

typedef bool(*dbll_sym_lookup) (void *handle, void *parg, void *rmm_handle,
				const char *name, struct dbll_sym_val ** sym);

typedef s32(*dbll_tell_fxn) (void *);

typedef s32(*dbll_write_fxn) (void *hdl, u32 dsp_address, void *buf,
			      u32 n, s32 mtype);

struct dbll_attrs {
	dbll_alloc_fxn alloc;
	dbll_free_fxn free;
	void *rmm_handle;	
	dbll_write_fxn write;
	void *input_params;	
	bool base_image;
	dbll_log_write_fxn log_write;
	void *log_write_handle;

	
	dbll_sym_lookup sym_lookup;
	void *sym_handle;
	void *sym_arg;

	 s32(*fread) (void *, size_t, size_t, void *);
	 s32(*fseek) (void *, long, int);
	 s32(*ftell) (void *);
	 s32(*fclose) (void *);
	void *(*fopen) (const char *, const char *);
};

typedef void (*dbll_close_fxn) (struct dbll_library_obj *library);

typedef int(*dbll_create_fxn) (struct dbll_tar_obj **target_obj,
				      struct dbll_attrs *attrs);

typedef void (*dbll_delete_fxn) (struct dbll_tar_obj *target);

typedef void (*dbll_exit_fxn) (void);

typedef bool(*dbll_get_addr_fxn) (struct dbll_library_obj *lib, char *name,
				  struct dbll_sym_val **sym_val);

typedef void (*dbll_get_attrs_fxn) (struct dbll_tar_obj *target,
				    struct dbll_attrs *attrs);

typedef bool(*dbll_get_c_addr_fxn) (struct dbll_library_obj *lib, char *name,
				    struct dbll_sym_val **sym_val);

typedef int(*dbll_get_sect_fxn) (struct dbll_library_obj *lib,
					char *name, u32 * addr, u32 * size);

typedef bool(*dbll_init_fxn) (void);

typedef int(*dbll_load_fxn) (struct dbll_library_obj *lib,
				    dbll_flags flags,
				    struct dbll_attrs *attrs, u32 *entry);
typedef int(*dbll_open_fxn) (struct dbll_tar_obj *target, char *file,
				    dbll_flags flags,
				    struct dbll_library_obj **lib_obj);

typedef int(*dbll_read_sect_fxn) (struct dbll_library_obj *lib,
					 char *name, char *content,
					 u32 cont_size);
typedef void (*dbll_unload_fxn) (struct dbll_library_obj *library,
				 struct dbll_attrs *attrs);
struct dbll_fxns {
	dbll_close_fxn close_fxn;
	dbll_create_fxn create_fxn;
	dbll_delete_fxn delete_fxn;
	dbll_exit_fxn exit_fxn;
	dbll_get_attrs_fxn get_attrs_fxn;
	dbll_get_addr_fxn get_addr_fxn;
	dbll_get_c_addr_fxn get_c_addr_fxn;
	dbll_get_sect_fxn get_sect_fxn;
	dbll_init_fxn init_fxn;
	dbll_load_fxn load_fxn;
	dbll_open_fxn open_fxn;
	dbll_read_sect_fxn read_sect_fxn;
	dbll_unload_fxn unload_fxn;
};

#endif 
