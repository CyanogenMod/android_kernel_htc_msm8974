/*
 * nldr.c
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * DSP/BIOS Bridge dynamic + overlay Node loader.
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

#include <linux/types.h>

#include <dspbridge/host_os.h>

#include <dspbridge/dbdefs.h>

#include <dspbridge/cod.h>
#include <dspbridge/dev.h>

#include <dspbridge/dbll.h>
#include <dspbridge/dbdcd.h>
#include <dspbridge/rmm.h>
#include <dspbridge/uuidutil.h>

#include <dspbridge/nldr.h>
#include <linux/lcm.h>

#define DYNMEMSECT  ".dspbridge_mem"

#define DEPLIBSECT  ".dspbridge_deplibs"

#define MAXDEPTH	    5

#define MAXLIBS	 5

#define MAXMEMSEGS      32

#define MAXSEGID	3	
#define MEMINTERNALID   32	
#define MEMEXTERNALID   33	
#define NULLID	  63		
#define FLAGBIT	 7		
#define SEGMASK	 0x3f		

#define CREATEBIT	0	
#define DELETEBIT	8	
#define EXECUTEBIT      16	

#define DYNM_CODE	0x2
#define DYNM_DATA	0x4
#define DYNM_CODEDATA   (DYNM_CODE | DYNM_DATA)
#define DYNM_INTERNAL   0x8
#define DYNM_EXTERNAL   0x10

#define CREATEDATAFLAGBIT   0
#define CREATECODEFLAGBIT   1
#define EXECUTEDATAFLAGBIT  2
#define EXECUTECODEFLAGBIT  3
#define DELETEDATAFLAGBIT   4
#define DELETECODEFLAGBIT   5
#define MAXFLAGS	    6

#define PCREATE	 "create"
#define PDELETE	 "delete"
#define PEXECUTE	"execute"

static inline bool is_equal_uuid(struct dsp_uuid *uuid1,
							struct dsp_uuid *uuid2)
{
	return !memcmp(uuid1, uuid2, sizeof(struct dsp_uuid));
}

struct mem_seg_info {
	u32 segid;		
	u32 base;
	u32 len;
	u32 type;		
};

struct lib_node {
	struct dbll_library_obj *lib;	
	u16 dep_libs;		
	struct lib_node *dep_libs_tree;	
};

struct ovly_sect {
	struct ovly_sect *next_sect;
	u32 sect_load_addr;	
	u32 sect_run_addr;	
	u32 size;		
	u16 page;		
};

struct ovly_node {
	struct dsp_uuid uuid;
	char *node_name;
	struct ovly_sect *create_sects_list;
	struct ovly_sect *delete_sects_list;
	struct ovly_sect *execute_sects_list;
	struct ovly_sect *other_sects_list;
	u16 create_sects;
	u16 delete_sects;
	u16 execute_sects;
	u16 other_sects;
	u16 create_ref;
	u16 delete_ref;
	u16 execute_ref;
	u16 other_ref;
};

struct nldr_object {
	struct dev_object *dev_obj;	
	struct dcd_manager *dcd_mgr;	
	struct dbll_tar_obj *dbll;	
	struct dbll_library_obj *base_lib;	
	struct rmm_target_obj *rmm;	
	struct dbll_fxns ldr_fxns;	
	struct dbll_attrs ldr_attrs;	
	nldr_ovlyfxn ovly_fxn;	
	nldr_writefxn write_fxn;	
	struct ovly_node *ovly_table;	
	u16 ovly_nodes;		
	u16 ovly_nid;		
	u16 dload_segs;		
	u32 *seg_table;		
	u16 dsp_mau_size;	
	u16 dsp_word_size;	
};

struct nldr_nodeobject {
	struct nldr_object *nldr_obj;	
	void *priv_ref;		
	struct dsp_uuid uuid;	
	bool dynamic;		
	bool overlay;		
	bool *phase_split;	
	struct lib_node root;	
	struct lib_node create_lib;	
	struct lib_node execute_lib;	
	struct lib_node delete_lib;	
	
	struct lib_node pers_lib_table[MAXLIBS];
	s32 pers_libs;		
	
	struct dbll_library_obj *lib_path[MAXDEPTH + 1];
	enum nldr_phase phase;	

	u16 seg_id[MAXFLAGS];

	u32 code_data_flag_mask;
};

static struct dbll_fxns ldr_fxns = {
	(dbll_close_fxn) dbll_close,
	(dbll_create_fxn) dbll_create,
	(dbll_delete_fxn) dbll_delete,
	(dbll_exit_fxn) dbll_exit,
	(dbll_get_attrs_fxn) dbll_get_attrs,
	(dbll_get_addr_fxn) dbll_get_addr,
	(dbll_get_c_addr_fxn) dbll_get_c_addr,
	(dbll_get_sect_fxn) dbll_get_sect,
	(dbll_init_fxn) dbll_init,
	(dbll_load_fxn) dbll_load,
	(dbll_open_fxn) dbll_open,
	(dbll_read_sect_fxn) dbll_read_sect,
	(dbll_unload_fxn) dbll_unload,
};

static int add_ovly_info(void *handle, struct dbll_sect_info *sect_info,
				u32 addr, u32 bytes);
static int add_ovly_node(struct dsp_uuid *uuid_obj,
				enum dsp_dcdobjtype obj_type, void *handle);
static int add_ovly_sect(struct nldr_object *nldr_obj,
				struct ovly_sect **lst,
				struct dbll_sect_info *sect_inf,
				bool *exists, u32 addr, u32 bytes);
static s32 fake_ovly_write(void *handle, u32 dsp_address, void *buf, u32 bytes,
			   s32 mtype);
static void free_sects(struct nldr_object *nldr_obj,
		       struct ovly_sect *phase_sects, u16 alloc_num);
static bool get_symbol_value(void *handle, void *parg, void *rmm_handle,
			     char *sym_name, struct dbll_sym_val **sym);
static int load_lib(struct nldr_nodeobject *nldr_node_obj,
			   struct lib_node *root, struct dsp_uuid uuid,
			   bool root_prstnt,
			   struct dbll_library_obj **lib_path,
			   enum nldr_phase phase, u16 depth);
static int load_ovly(struct nldr_nodeobject *nldr_node_obj,
			    enum nldr_phase phase);
static int remote_alloc(void **ref, u16 mem_sect, u32 size,
			       u32 align, u32 *dsp_address,
			       s32 segmnt_id,
			       s32 req, bool reserve);
static int remote_free(void **ref, u16 space, u32 dsp_address, u32 size,
			      bool reserve);

static void unload_lib(struct nldr_nodeobject *nldr_node_obj,
		       struct lib_node *root);
static void unload_ovly(struct nldr_nodeobject *nldr_node_obj,
			enum nldr_phase phase);
static bool find_in_persistent_lib_array(struct nldr_nodeobject *nldr_node_obj,
					 struct dbll_library_obj *lib);

int nldr_allocate(struct nldr_object *nldr_obj, void *priv_ref,
			 const struct dcd_nodeprops *node_props,
			 struct nldr_nodeobject **nldr_nodeobj,
			 bool *pf_phase_split)
{
	struct nldr_nodeobject *nldr_node_obj = NULL;
	int status = 0;

	
	*nldr_nodeobj = NULL;
	
	nldr_node_obj = kzalloc(sizeof(struct nldr_nodeobject), GFP_KERNEL);

	if (nldr_node_obj == NULL) {
		status = -ENOMEM;
	} else {
		nldr_node_obj->phase_split = pf_phase_split;
		nldr_node_obj->pers_libs = 0;
		nldr_node_obj->nldr_obj = nldr_obj;
		nldr_node_obj->priv_ref = priv_ref;
		
		nldr_node_obj->uuid = node_props->ndb_props.ui_node_id;
		if (node_props->load_type == NLDR_DYNAMICLOAD) {
			
			nldr_node_obj->dynamic = true;
			
			nldr_node_obj->seg_id[CREATEDATAFLAGBIT] = (u16)
			    (node_props->data_mem_seg_mask >> CREATEBIT) &
			    SEGMASK;
			nldr_node_obj->code_data_flag_mask |=
			    ((node_props->data_mem_seg_mask >>
			      (CREATEBIT + FLAGBIT)) & 1) << CREATEDATAFLAGBIT;
			nldr_node_obj->seg_id[CREATECODEFLAGBIT] = (u16)
			    (node_props->code_mem_seg_mask >>
			     CREATEBIT) & SEGMASK;
			nldr_node_obj->code_data_flag_mask |=
			    ((node_props->code_mem_seg_mask >>
			      (CREATEBIT + FLAGBIT)) & 1) << CREATECODEFLAGBIT;
			
			nldr_node_obj->seg_id[EXECUTEDATAFLAGBIT] = (u16)
			    (node_props->data_mem_seg_mask >>
			     EXECUTEBIT) & SEGMASK;
			nldr_node_obj->code_data_flag_mask |=
			    ((node_props->data_mem_seg_mask >>
			      (EXECUTEBIT + FLAGBIT)) & 1) <<
			    EXECUTEDATAFLAGBIT;
			nldr_node_obj->seg_id[EXECUTECODEFLAGBIT] = (u16)
			    (node_props->code_mem_seg_mask >>
			     EXECUTEBIT) & SEGMASK;
			nldr_node_obj->code_data_flag_mask |=
			    ((node_props->code_mem_seg_mask >>
			      (EXECUTEBIT + FLAGBIT)) & 1) <<
			    EXECUTECODEFLAGBIT;
			
			nldr_node_obj->seg_id[DELETEDATAFLAGBIT] = (u16)
			    (node_props->data_mem_seg_mask >> DELETEBIT) &
			    SEGMASK;
			nldr_node_obj->code_data_flag_mask |=
			    ((node_props->data_mem_seg_mask >>
			      (DELETEBIT + FLAGBIT)) & 1) << DELETEDATAFLAGBIT;
			nldr_node_obj->seg_id[DELETECODEFLAGBIT] = (u16)
			    (node_props->code_mem_seg_mask >>
			     DELETEBIT) & SEGMASK;
			nldr_node_obj->code_data_flag_mask |=
			    ((node_props->code_mem_seg_mask >>
			      (DELETEBIT + FLAGBIT)) & 1) << DELETECODEFLAGBIT;
		} else {
			nldr_node_obj->root.lib = nldr_obj->base_lib;
			
			if (node_props->load_type == NLDR_OVLYLOAD)
				nldr_node_obj->overlay = true;

		}
		*nldr_nodeobj = (struct nldr_nodeobject *)nldr_node_obj;
	}
	
	if (status && nldr_node_obj)
		kfree(nldr_node_obj);

	return status;
}

int nldr_create(struct nldr_object **nldr,
		       struct dev_object *hdev_obj,
		       const struct nldr_attrs *pattrs)
{
	struct cod_manager *cod_mgr;	
	char *psz_coff_buf = NULL;
	char sz_zl_file[COD_MAXPATHLENGTH];
	struct nldr_object *nldr_obj = NULL;
	struct dbll_attrs save_attrs;
	struct dbll_attrs new_attrs;
	dbll_flags flags;
	u32 ul_entry;
	u16 dload_segs = 0;
	struct mem_seg_info *mem_info_obj;
	u32 ul_len = 0;
	u32 ul_addr;
	struct rmm_segment *rmm_segs = NULL;
	u16 i;
	int status = 0;

	
	nldr_obj = kzalloc(sizeof(struct nldr_object), GFP_KERNEL);
	if (nldr_obj) {
		nldr_obj->dev_obj = hdev_obj;
		
		dev_get_cod_mgr(hdev_obj, &cod_mgr);
		if (cod_mgr) {
			status = cod_get_loader(cod_mgr, &nldr_obj->dbll);
			status = cod_get_base_lib(cod_mgr, &nldr_obj->base_lib);
			status =
			    cod_get_base_name(cod_mgr, sz_zl_file,
							COD_MAXPATHLENGTH);
		}
		status = 0;
		
		nldr_obj->dsp_mau_size = pattrs->dsp_mau_size;
		nldr_obj->dsp_word_size = pattrs->dsp_word_size;
		nldr_obj->ldr_fxns = ldr_fxns;
		if (!(nldr_obj->ldr_fxns.init_fxn()))
			status = -ENOMEM;

	} else {
		status = -ENOMEM;
	}
	
	if (!status)
		status = dcd_create_manager(NULL, &nldr_obj->dcd_mgr);

	
	if (!status) {
		status =
		    nldr_obj->ldr_fxns.get_sect_fxn(nldr_obj->base_lib,
						    DYNMEMSECT, &ul_addr,
						    &ul_len);
		if (!status) {
			psz_coff_buf =
				kzalloc(ul_len * nldr_obj->dsp_mau_size,
								GFP_KERNEL);
			if (!psz_coff_buf)
				status = -ENOMEM;
		} else {
			
			status = 0;
			ul_len = 0;
			dev_dbg(bridge, "%s: failed - no dynamic loading mem "
				"segments: 0x%x\n", __func__, status);
		}
	}
	if (!status && ul_len > 0) {
		
		status =
		    nldr_obj->ldr_fxns.read_sect_fxn(nldr_obj->base_lib,
						     DYNMEMSECT, psz_coff_buf,
						     ul_len);
	}
	if (!status && ul_len > 0) {
		
		dload_segs = (u16) (*((u32 *) psz_coff_buf));
		if (dload_segs > MAXMEMSEGS)
			status = -EBADF;
	}
	
	if (!status && dload_segs > 0) {
		rmm_segs = kzalloc(sizeof(struct rmm_segment) * dload_segs,
								GFP_KERNEL);
		nldr_obj->seg_table =
				kzalloc(sizeof(u32) * dload_segs, GFP_KERNEL);
		if (rmm_segs == NULL || nldr_obj->seg_table == NULL) {
			status = -ENOMEM;
		} else {
			nldr_obj->dload_segs = dload_segs;
			mem_info_obj = (struct mem_seg_info *)(psz_coff_buf +
							       sizeof(u32));
			for (i = 0; i < dload_segs; i++) {
				rmm_segs[i].base = (mem_info_obj + i)->base;
				rmm_segs[i].length = (mem_info_obj + i)->len;
				rmm_segs[i].space = 0;
				nldr_obj->seg_table[i] =
				    (mem_info_obj + i)->type;
				dev_dbg(bridge,
					"(proc) DLL MEMSEGMENT: %d, "
					"Base: 0x%x, Length: 0x%x\n", i,
					rmm_segs[i].base, rmm_segs[i].length);
			}
		}
	}
	
	if (!status)
		status = rmm_create(&nldr_obj->rmm, rmm_segs, dload_segs);

	if (!status) {
		
		nldr_obj->ldr_fxns.get_attrs_fxn(nldr_obj->dbll, &save_attrs);
		new_attrs = save_attrs;
		new_attrs.alloc = (dbll_alloc_fxn) remote_alloc;
		new_attrs.free = (dbll_free_fxn) remote_free;
		new_attrs.sym_lookup = (dbll_sym_lookup) get_symbol_value;
		new_attrs.sym_handle = nldr_obj;
		new_attrs.write = (dbll_write_fxn) pattrs->write;
		nldr_obj->ovly_fxn = pattrs->ovly;
		nldr_obj->write_fxn = pattrs->write;
		nldr_obj->ldr_attrs = new_attrs;
	}
	kfree(rmm_segs);

	kfree(psz_coff_buf);

	
	if (!status) {
		status =
		    cod_get_base_name(cod_mgr, sz_zl_file, COD_MAXPATHLENGTH);
		
		
		status =
		    dcd_get_objects(nldr_obj->dcd_mgr, sz_zl_file,
				    add_ovly_node, (void *)nldr_obj);
		
		if (!status && nldr_obj->ovly_nodes > 0) {
			
			nldr_obj->ovly_table =
					kzalloc(sizeof(struct ovly_node) *
					nldr_obj->ovly_nodes, GFP_KERNEL);
			
			nldr_obj->ovly_nid = 0;
			status = dcd_get_objects(nldr_obj->dcd_mgr, sz_zl_file,
						 add_ovly_node,
						 (void *)nldr_obj);
		}
	}
	
	if (!status && nldr_obj->ovly_nodes > 0) {
		save_attrs.write = fake_ovly_write;
		save_attrs.log_write = add_ovly_info;
		save_attrs.log_write_handle = nldr_obj;
		flags = DBLL_CODE | DBLL_DATA | DBLL_SYMB;
		status = nldr_obj->ldr_fxns.load_fxn(nldr_obj->base_lib, flags,
						     &save_attrs, &ul_entry);
	}
	if (!status) {
		*nldr = (struct nldr_object *)nldr_obj;
	} else {
		if (nldr_obj)
			nldr_delete((struct nldr_object *)nldr_obj);

		*nldr = NULL;
	}
	
	return status;
}

void nldr_delete(struct nldr_object *nldr_obj)
{
	struct ovly_sect *ovly_section;
	struct ovly_sect *next;
	u16 i;

	nldr_obj->ldr_fxns.exit_fxn();
	if (nldr_obj->rmm)
		rmm_delete(nldr_obj->rmm);

	kfree(nldr_obj->seg_table);

	if (nldr_obj->dcd_mgr)
		dcd_destroy_manager(nldr_obj->dcd_mgr);

	
	if (nldr_obj->ovly_table) {
		for (i = 0; i < nldr_obj->ovly_nodes; i++) {
			ovly_section =
			    nldr_obj->ovly_table[i].create_sects_list;
			while (ovly_section) {
				next = ovly_section->next_sect;
				kfree(ovly_section);
				ovly_section = next;
			}
			ovly_section =
			    nldr_obj->ovly_table[i].delete_sects_list;
			while (ovly_section) {
				next = ovly_section->next_sect;
				kfree(ovly_section);
				ovly_section = next;
			}
			ovly_section =
			    nldr_obj->ovly_table[i].execute_sects_list;
			while (ovly_section) {
				next = ovly_section->next_sect;
				kfree(ovly_section);
				ovly_section = next;
			}
			ovly_section = nldr_obj->ovly_table[i].other_sects_list;
			while (ovly_section) {
				next = ovly_section->next_sect;
				kfree(ovly_section);
				ovly_section = next;
			}
		}
		kfree(nldr_obj->ovly_table);
	}
	kfree(nldr_obj);
}

int nldr_get_fxn_addr(struct nldr_nodeobject *nldr_node_obj,
			     char *str_fxn, u32 * addr)
{
	struct dbll_sym_val *dbll_sym;
	struct nldr_object *nldr_obj;
	int status = 0;
	bool status1 = false;
	s32 i = 0;
	struct lib_node root = { NULL, 0, NULL };

	nldr_obj = nldr_node_obj->nldr_obj;
	
	if (nldr_node_obj->dynamic && *nldr_node_obj->phase_split) {
		switch (nldr_node_obj->phase) {
		case NLDR_CREATE:
			root = nldr_node_obj->create_lib;
			break;
		case NLDR_EXECUTE:
			root = nldr_node_obj->execute_lib;
			break;
		case NLDR_DELETE:
			root = nldr_node_obj->delete_lib;
			break;
		default:
			break;
		}
	} else {
		
		root = nldr_node_obj->root;
	}
	status1 =
	    nldr_obj->ldr_fxns.get_c_addr_fxn(root.lib, str_fxn, &dbll_sym);
	if (!status1)
		status1 =
		    nldr_obj->ldr_fxns.get_addr_fxn(root.lib, str_fxn,
						    &dbll_sym);

	
	if (!status1) {
		for (i = 0; i < root.dep_libs; i++) {
			status1 =
			    nldr_obj->ldr_fxns.get_addr_fxn(root.dep_libs_tree
							    [i].lib, str_fxn,
							    &dbll_sym);
			if (!status1) {
				status1 =
				    nldr_obj->ldr_fxns.
				    get_c_addr_fxn(root.dep_libs_tree[i].lib,
						   str_fxn, &dbll_sym);
			}
			if (status1) {
				
				break;
			}
		}
	}
	
	if (!status1) {
		for (i = 0; i < nldr_node_obj->pers_libs; i++) {
			status1 =
			    nldr_obj->ldr_fxns.
			    get_addr_fxn(nldr_node_obj->pers_lib_table[i].lib,
					 str_fxn, &dbll_sym);
			if (!status1) {
				status1 =
				    nldr_obj->ldr_fxns.
				    get_c_addr_fxn(nldr_node_obj->pers_lib_table
						   [i].lib, str_fxn, &dbll_sym);
			}
			if (status1) {
				
				break;
			}
		}
	}

	if (status1)
		*addr = dbll_sym->value;
	else
		status = -ESPIPE;

	return status;
}

int nldr_get_rmm_manager(struct nldr_object *nldr,
				struct rmm_target_obj **rmm_mgr)
{
	int status = 0;
	struct nldr_object *nldr_obj = nldr;

	if (nldr) {
		*rmm_mgr = nldr_obj->rmm;
	} else {
		*rmm_mgr = NULL;
		status = -EFAULT;
	}

	return status;
}

int nldr_load(struct nldr_nodeobject *nldr_node_obj,
		     enum nldr_phase phase)
{
	struct nldr_object *nldr_obj;
	struct dsp_uuid lib_uuid;
	int status = 0;

	nldr_obj = nldr_node_obj->nldr_obj;

	if (nldr_node_obj->dynamic) {
		nldr_node_obj->phase = phase;

		lib_uuid = nldr_node_obj->uuid;


		status =
		    load_lib(nldr_node_obj, &nldr_node_obj->root, lib_uuid,
			     false, nldr_node_obj->lib_path, phase, 0);

		if (!status) {
			if (*nldr_node_obj->phase_split) {
				switch (phase) {
				case NLDR_CREATE:
					nldr_node_obj->create_lib =
					    nldr_node_obj->root;
					break;

				case NLDR_EXECUTE:
					nldr_node_obj->execute_lib =
					    nldr_node_obj->root;
					break;

				case NLDR_DELETE:
					nldr_node_obj->delete_lib =
					    nldr_node_obj->root;
					break;

				default:
					break;
				}
			}
		}
	} else {
		if (nldr_node_obj->overlay)
			status = load_ovly(nldr_node_obj, phase);

	}

	return status;
}

int nldr_unload(struct nldr_nodeobject *nldr_node_obj,
		       enum nldr_phase phase)
{
	int status = 0;
	struct lib_node *root_lib = NULL;
	s32 i = 0;

	if (nldr_node_obj != NULL) {
		if (nldr_node_obj->dynamic) {
			if (*nldr_node_obj->phase_split) {
				switch (phase) {
				case NLDR_CREATE:
					root_lib = &nldr_node_obj->create_lib;
					break;
				case NLDR_EXECUTE:
					root_lib = &nldr_node_obj->execute_lib;
					break;
				case NLDR_DELETE:
					root_lib = &nldr_node_obj->delete_lib;
					
					for (i = 0;
					     i < nldr_node_obj->pers_libs;
					     i++) {
						unload_lib(nldr_node_obj,
							   &nldr_node_obj->
							   pers_lib_table[i]);
					}
					nldr_node_obj->pers_libs = 0;
					break;
				default:
					break;
				}
			} else {
				
				root_lib = &nldr_node_obj->root;
			}
			if (root_lib)
				unload_lib(nldr_node_obj, root_lib);
		} else {
			if (nldr_node_obj->overlay)
				unload_ovly(nldr_node_obj, phase);

		}
	}
	return status;
}

static int add_ovly_info(void *handle, struct dbll_sect_info *sect_info,
				u32 addr, u32 bytes)
{
	char *node_name;
	char *sect_name = (char *)sect_info->name;
	bool sect_exists = false;
	char seps = ':';
	char *pch;
	u16 i;
	struct nldr_object *nldr_obj = (struct nldr_object *)handle;
	int status = 0;

	
	if (sect_info->sect_load_addr == sect_info->sect_run_addr)
		goto func_end;

	
	for (i = 0; i < nldr_obj->ovly_nodes; i++) {
		node_name = nldr_obj->ovly_table[i].node_name;
		if (strncmp(node_name, sect_name + 1, strlen(node_name)) == 0) {
			
			break;
		}
	}
	if (!(i < nldr_obj->ovly_nodes))
		goto func_end;

	
	for (pch = sect_name + 1; *pch && *pch != seps; pch++)
		;

	if (*pch) {
		pch++;		
		if (strncmp(pch, PCREATE, strlen(PCREATE)) == 0) {
			status =
			    add_ovly_sect(nldr_obj,
					  &nldr_obj->
					  ovly_table[i].create_sects_list,
					  sect_info, &sect_exists, addr, bytes);
			if (!status && !sect_exists)
				nldr_obj->ovly_table[i].create_sects++;

		} else if (strncmp(pch, PDELETE, strlen(PDELETE)) == 0) {
			status =
			    add_ovly_sect(nldr_obj,
					  &nldr_obj->
					  ovly_table[i].delete_sects_list,
					  sect_info, &sect_exists, addr, bytes);
			if (!status && !sect_exists)
				nldr_obj->ovly_table[i].delete_sects++;

		} else if (strncmp(pch, PEXECUTE, strlen(PEXECUTE)) == 0) {
			status =
			    add_ovly_sect(nldr_obj,
					  &nldr_obj->
					  ovly_table[i].execute_sects_list,
					  sect_info, &sect_exists, addr, bytes);
			if (!status && !sect_exists)
				nldr_obj->ovly_table[i].execute_sects++;

		} else {
			
			status =
			    add_ovly_sect(nldr_obj,
					  &nldr_obj->
					  ovly_table[i].other_sects_list,
					  sect_info, &sect_exists, addr, bytes);
			if (!status && !sect_exists)
				nldr_obj->ovly_table[i].other_sects++;

		}
	}
func_end:
	return status;
}

static int add_ovly_node(struct dsp_uuid *uuid_obj,
				enum dsp_dcdobjtype obj_type, void *handle)
{
	struct nldr_object *nldr_obj = (struct nldr_object *)handle;
	char *node_name = NULL;
	char *pbuf = NULL;
	u32 len;
	struct dcd_genericobj obj_def;
	int status = 0;

	if (obj_type != DSP_DCDNODETYPE)
		goto func_end;

	status =
	    dcd_get_object_def(nldr_obj->dcd_mgr, uuid_obj, obj_type,
			       &obj_def);
	if (status)
		goto func_end;

	
	if (obj_def.obj_data.node_obj.load_type == NLDR_OVLYLOAD) {
		if (nldr_obj->ovly_table == NULL) {
			nldr_obj->ovly_nodes++;
		} else {
			
			nldr_obj->ovly_table[nldr_obj->ovly_nid].uuid =
			    *uuid_obj;
			len =
			    strlen(obj_def.obj_data.node_obj.ndb_props.ac_name);
			node_name = obj_def.obj_data.node_obj.ndb_props.ac_name;
			pbuf = kzalloc(len + 1, GFP_KERNEL);
			if (pbuf == NULL) {
				status = -ENOMEM;
			} else {
				strncpy(pbuf, node_name, len);
				nldr_obj->ovly_table[nldr_obj->ovly_nid].
				    node_name = pbuf;
				nldr_obj->ovly_nid++;
			}
		}
	}
	
	kfree(obj_def.obj_data.node_obj.str_create_phase_fxn);

	kfree(obj_def.obj_data.node_obj.str_execute_phase_fxn);

	kfree(obj_def.obj_data.node_obj.str_delete_phase_fxn);

	kfree(obj_def.obj_data.node_obj.str_i_alg_name);

func_end:
	return status;
}

static int add_ovly_sect(struct nldr_object *nldr_obj,
				struct ovly_sect **lst,
				struct dbll_sect_info *sect_inf,
				bool *exists, u32 addr, u32 bytes)
{
	struct ovly_sect *new_sect = NULL;
	struct ovly_sect *last_sect;
	struct ovly_sect *ovly_section;
	int status = 0;

	ovly_section = last_sect = *lst;
	*exists = false;
	while (ovly_section) {
		if (ovly_section->sect_load_addr == addr) {
			
			*exists = true;
			break;
		}
		last_sect = ovly_section;
		ovly_section = ovly_section->next_sect;
	}

	if (!ovly_section) {
		
		new_sect = kzalloc(sizeof(struct ovly_sect), GFP_KERNEL);
		if (new_sect == NULL) {
			status = -ENOMEM;
		} else {
			new_sect->sect_load_addr = addr;
			new_sect->sect_run_addr = sect_inf->sect_run_addr +
			    (addr - sect_inf->sect_load_addr);
			new_sect->size = bytes;
			new_sect->page = sect_inf->type;
		}

		
		if (!status) {
			if (*lst == NULL) {
				
				*lst = new_sect;
			} else {
				last_sect->next_sect = new_sect;
			}
		}
	}

	return status;
}

static s32 fake_ovly_write(void *handle, u32 dsp_address, void *buf, u32 bytes,
			   s32 mtype)
{
	return (s32) bytes;
}

static void free_sects(struct nldr_object *nldr_obj,
		       struct ovly_sect *phase_sects, u16 alloc_num)
{
	struct ovly_sect *ovly_section = phase_sects;
	u16 i = 0;
	bool ret;

	while (ovly_section && i < alloc_num) {
		
		
		
		ret =
		    rmm_free(nldr_obj->rmm, 0, ovly_section->sect_run_addr,
			     ovly_section->size, true);
		ovly_section = ovly_section->next_sect;
		i++;
	}
}

static bool get_symbol_value(void *handle, void *parg, void *rmm_handle,
			     char *sym_name, struct dbll_sym_val **sym)
{
	struct nldr_object *nldr_obj = (struct nldr_object *)handle;
	struct nldr_nodeobject *nldr_node_obj =
	    (struct nldr_nodeobject *)rmm_handle;
	struct lib_node *root = (struct lib_node *)parg;
	u16 i;
	bool status = false;

	
	status = nldr_obj->ldr_fxns.get_addr_fxn(nldr_obj->base_lib,
						 sym_name, sym);
	if (!status)
		status =
		    nldr_obj->ldr_fxns.get_c_addr_fxn(nldr_obj->base_lib,
							sym_name, sym);

	if (!status) {
		status = nldr_obj->ldr_fxns.get_addr_fxn(root->lib, sym_name,
							 sym);
		if (!status) {
			status =
			    nldr_obj->ldr_fxns.get_c_addr_fxn(root->lib,
							      sym_name, sym);
		}
	}

	if (!status) {
		for (i = 0; i < root->dep_libs; i++) {
			status =
			    nldr_obj->ldr_fxns.get_addr_fxn(root->
							    dep_libs_tree
							    [i].lib,
							    sym_name, sym);
			if (!status) {
				status =
				    nldr_obj->ldr_fxns.
				    get_c_addr_fxn(root->dep_libs_tree[i].lib,
						   sym_name, sym);
			}
			if (status) {
				
				break;
			}
		}
	}
	if (!status) {
		for (i = 0; i < nldr_node_obj->pers_libs; i++) {
			status =
			    nldr_obj->ldr_fxns.
			    get_addr_fxn(nldr_node_obj->pers_lib_table[i].lib,
					 sym_name, sym);
			if (!status) {
				status = nldr_obj->ldr_fxns.get_c_addr_fxn
				    (nldr_node_obj->pers_lib_table[i].lib,
				     sym_name, sym);
			}
			if (status) {
				
				break;
			}
		}
	}

	return status;
}

static int load_lib(struct nldr_nodeobject *nldr_node_obj,
			   struct lib_node *root, struct dsp_uuid uuid,
			   bool root_prstnt,
			   struct dbll_library_obj **lib_path,
			   enum nldr_phase phase, u16 depth)
{
	struct nldr_object *nldr_obj = nldr_node_obj->nldr_obj;
	u16 nd_libs = 0;	
	u16 np_libs = 0;	
	u16 nd_libs_loaded = 0;	
	u16 i;
	u32 entry;
	u32 dw_buf_size = NLDR_MAXPATHLENGTH;
	dbll_flags flags = DBLL_SYMB | DBLL_CODE | DBLL_DATA | DBLL_DYNAMIC;
	struct dbll_attrs new_attrs;
	char *psz_file_name = NULL;
	struct dsp_uuid *dep_lib_uui_ds = NULL;
	bool *persistent_dep_libs = NULL;
	int status = 0;
	bool lib_status = false;
	struct lib_node *dep_lib;

	if (depth > MAXDEPTH) {
		
	}
	root->lib = NULL;
	
	psz_file_name = kzalloc(DBLL_MAXPATHLENGTH, GFP_KERNEL);
	if (psz_file_name == NULL)
		status = -ENOMEM;

	if (!status) {
		
		if (depth == 0) {
			status =
			    dcd_get_library_name(nldr_node_obj->nldr_obj->
						 dcd_mgr, &uuid, psz_file_name,
						 &dw_buf_size, phase,
						 nldr_node_obj->phase_split);
		} else {
			
			status =
			    dcd_get_library_name(nldr_node_obj->nldr_obj->
						 dcd_mgr, &uuid, psz_file_name,
						 &dw_buf_size, NLDR_NOPHASE,
						 NULL);
		}
	}
	if (!status) {
		
		status =
		    nldr_obj->ldr_fxns.open_fxn(nldr_obj->dbll, psz_file_name,
						DBLL_NOLOAD, &root->lib);
	}
	
	kfree(psz_file_name);

	
	if (!status && root_prstnt) {
		lib_status =
		    find_in_persistent_lib_array(nldr_node_obj, root->lib);
		
		if (lib_status) {
			nldr_obj->ldr_fxns.close_fxn(root->lib);
			return 0;
		}
	}
	if (!status) {
		
		for (i = 0; i < depth; i++) {
			if (root->lib == lib_path[i]) {
				status = -EILSEQ;
			}
		}
	}
	if (!status) {
		
		lib_path[depth] = root->lib;
		depth++;
		
		status =
		    dcd_get_num_dep_libs(nldr_node_obj->nldr_obj->dcd_mgr,
					 &uuid, &nd_libs, &np_libs, phase);
	}
	if (!status) {
		if (!(*nldr_node_obj->phase_split))
			np_libs = 0;

		
		root->dep_libs = nd_libs - np_libs;
		if (nd_libs > 0) {
			dep_lib_uui_ds = kzalloc(sizeof(struct dsp_uuid) *
							nd_libs, GFP_KERNEL);
			persistent_dep_libs =
				kzalloc(sizeof(bool) * nd_libs, GFP_KERNEL);
			if (!dep_lib_uui_ds || !persistent_dep_libs)
				status = -ENOMEM;

			if (root->dep_libs > 0) {
				root->dep_libs_tree = kzalloc
						(sizeof(struct lib_node) *
						(root->dep_libs), GFP_KERNEL);
				if (!(root->dep_libs_tree))
					status = -ENOMEM;

			}

			if (!status) {
				
				status =
				    dcd_get_dep_libs(nldr_node_obj->
						     nldr_obj->dcd_mgr, &uuid,
						     nd_libs, dep_lib_uui_ds,
						     persistent_dep_libs,
						     phase);
			}
		}
	}

	if (!status) {
		for (i = 0; i < nd_libs; i++) {
			if (!root_prstnt && persistent_dep_libs[i] &&
			    *nldr_node_obj->phase_split) {
				if ((nldr_node_obj->pers_libs) >= MAXLIBS) {
					status = -EILSEQ;
					break;
				}

				
				dep_lib =
				    &nldr_node_obj->pers_lib_table
				    [nldr_node_obj->pers_libs];
			} else {
				if (root_prstnt)
					persistent_dep_libs[i] = true;

				
				dep_lib = &root->dep_libs_tree[nd_libs_loaded];
			}

			status = load_lib(nldr_node_obj, dep_lib,
					  dep_lib_uui_ds[i],
					  persistent_dep_libs[i], lib_path,
					  phase, depth);

			if (!status) {
				if ((status != 0) &&
				    !root_prstnt && persistent_dep_libs[i] &&
				    *nldr_node_obj->phase_split) {
					(nldr_node_obj->pers_libs)++;
				} else {
					if (!persistent_dep_libs[i] ||
					    !(*nldr_node_obj->phase_split)) {
						nd_libs_loaded++;
					}
				}
			} else {
				break;
			}
		}
	}

	
	if (!status) {
		new_attrs = nldr_obj->ldr_attrs;
		new_attrs.sym_arg = root;
		new_attrs.rmm_handle = nldr_node_obj;
		new_attrs.input_params = nldr_node_obj->priv_ref;
		new_attrs.base_image = false;

		status =
		    nldr_obj->ldr_fxns.load_fxn(root->lib, flags, &new_attrs,
						&entry);
	}

	if (status) {
		if (phase != NLDR_EXECUTE) {
			for (i = 0; i < nldr_node_obj->pers_libs; i++)
				unload_lib(nldr_node_obj,
					   &nldr_node_obj->pers_lib_table[i]);

			nldr_node_obj->pers_libs = 0;
		}
		for (i = 0; i < nd_libs_loaded; i++)
			unload_lib(nldr_node_obj, &root->dep_libs_tree[i]);

		if (root->lib)
			nldr_obj->ldr_fxns.close_fxn(root->lib);

	}

	
	depth--;

	kfree(dep_lib_uui_ds);
	dep_lib_uui_ds = NULL;

	kfree(persistent_dep_libs);
	persistent_dep_libs = NULL;

	return status;
}

static int load_ovly(struct nldr_nodeobject *nldr_node_obj,
			    enum nldr_phase phase)
{
	struct nldr_object *nldr_obj = nldr_node_obj->nldr_obj;
	struct ovly_node *po_node = NULL;
	struct ovly_sect *phase_sects = NULL;
	struct ovly_sect *other_sects_list = NULL;
	u16 i;
	u16 alloc_num = 0;
	u16 other_alloc = 0;
	u16 *ref_count = NULL;
	u16 *other_ref = NULL;
	u32 bytes;
	struct ovly_sect *ovly_section;
	int status = 0;

	
	for (i = 0; i < nldr_obj->ovly_nodes; i++) {
		if (is_equal_uuid
		    (&nldr_node_obj->uuid, &nldr_obj->ovly_table[i].uuid)) {
			
			po_node = &(nldr_obj->ovly_table[i]);
			break;
		}
	}


	if (!po_node) {
		status = -ENOENT;
		goto func_end;
	}

	switch (phase) {
	case NLDR_CREATE:
		ref_count = &(po_node->create_ref);
		other_ref = &(po_node->other_ref);
		phase_sects = po_node->create_sects_list;
		other_sects_list = po_node->other_sects_list;
		break;

	case NLDR_EXECUTE:
		ref_count = &(po_node->execute_ref);
		phase_sects = po_node->execute_sects_list;
		break;

	case NLDR_DELETE:
		ref_count = &(po_node->delete_ref);
		phase_sects = po_node->delete_sects_list;
		break;

	default:
		break;
	}

	if (ref_count == NULL)
		goto func_end;

	if (*ref_count != 0)
		goto func_end;

	
	ovly_section = phase_sects;
	while (ovly_section) {
		
		
		status = rmm_alloc(nldr_obj->rmm, 0, ovly_section->size, 0,
				   &(ovly_section->sect_run_addr), true);
		if (!status) {
			ovly_section = ovly_section->next_sect;
			alloc_num++;
		} else {
			break;
		}
	}
	if (other_ref && *other_ref == 0) {
		if (!status) {
			ovly_section = other_sects_list;
			while (ovly_section) {
				
				
				status =
				    rmm_alloc(nldr_obj->rmm, 0,
					      ovly_section->size, 0,
					      &(ovly_section->sect_run_addr),
					      true);
				if (!status) {
					ovly_section = ovly_section->next_sect;
					other_alloc++;
				} else {
					break;
				}
			}
		}
	}
	if (*ref_count == 0) {
		if (!status) {
			
			ovly_section = phase_sects;
			while (ovly_section && !status) {
				bytes =
				    (*nldr_obj->ovly_fxn) (nldr_node_obj->
							   priv_ref,
							   ovly_section->
							   sect_run_addr,
							   ovly_section->
							   sect_load_addr,
							   ovly_section->size,
							   ovly_section->page);
				if (bytes != ovly_section->size)
					status = -EPERM;

				ovly_section = ovly_section->next_sect;
			}
		}
	}
	if (other_ref && *other_ref == 0) {
		if (!status) {
			
			ovly_section = other_sects_list;
			while (ovly_section && !status) {
				bytes =
				    (*nldr_obj->ovly_fxn) (nldr_node_obj->
							   priv_ref,
							   ovly_section->
							   sect_run_addr,
							   ovly_section->
							   sect_load_addr,
							   ovly_section->size,
							   ovly_section->page);
				if (bytes != ovly_section->size)
					status = -EPERM;

				ovly_section = ovly_section->next_sect;
			}
		}
	}
	if (status) {
		
		free_sects(nldr_obj, phase_sects, alloc_num);
		free_sects(nldr_obj, other_sects_list, other_alloc);
	}
func_end:
	if (!status && (ref_count != NULL)) {
		*ref_count += 1;
		if (other_ref)
			*other_ref += 1;

	}

	return status;
}

static int remote_alloc(void **ref, u16 mem_sect, u32 size,
			       u32 align, u32 *dsp_address,
			       s32 segmnt_id, s32 req,
			       bool reserve)
{
	struct nldr_nodeobject *hnode = (struct nldr_nodeobject *)ref;
	struct nldr_object *nldr_obj;
	struct rmm_target_obj *rmm;
	u16 mem_phase_bit = MAXFLAGS;
	u16 segid = 0;
	u16 i;
	u16 mem_sect_type;
	u32 word_size;
	struct rmm_addr *rmm_addr_obj = (struct rmm_addr *)dsp_address;
	bool mem_load_req = false;
	int status = -ENOMEM;	
	nldr_obj = hnode->nldr_obj;
	rmm = nldr_obj->rmm;
	
	word_size =
	    (size + nldr_obj->dsp_word_size -
	     1) / nldr_obj->dsp_word_size;
	
	align = lcm(GEM_CACHE_LINE_SIZE, align);
	dev_dbg(bridge, "%s: memory align to 0x%x\n", __func__, align);
	if (segmnt_id != -1) {
		rmm_addr_obj->segid = segmnt_id;
		segid = segmnt_id;
		mem_load_req = req;
	} else {
		switch (hnode->phase) {
		case NLDR_CREATE:
			mem_phase_bit = CREATEDATAFLAGBIT;
			break;
		case NLDR_DELETE:
			mem_phase_bit = DELETEDATAFLAGBIT;
			break;
		case NLDR_EXECUTE:
			mem_phase_bit = EXECUTEDATAFLAGBIT;
			break;
		default:
			break;
		}
		if (mem_sect == DBLL_CODE)
			mem_phase_bit++;

		if (mem_phase_bit < MAXFLAGS)
			segid = hnode->seg_id[mem_phase_bit];

		
		if ((hnode->code_data_flag_mask >> mem_phase_bit) & 0x1)
			mem_load_req = true;

	}
	mem_sect_type = (mem_sect == DBLL_CODE) ? DYNM_CODE : DYNM_DATA;

	
	if (segid == NULLID) {
		
		goto func_cont;
	}
	if (segid <= MAXSEGID) {
		
		rmm_addr_obj->segid = segid;
		status =
		    rmm_alloc(rmm, segid, word_size, align, dsp_address, false);
		if (status) {
			dev_dbg(bridge, "%s: Unable allocate from segment %d\n",
				__func__, segid);
		}
	} else {
		
		mem_sect_type |= segid == MEMINTERNALID ?
		    DYNM_INTERNAL : DYNM_EXTERNAL;
		for (i = 0; i < nldr_obj->dload_segs; i++) {
			if ((nldr_obj->seg_table[i] & mem_sect_type) !=
			    mem_sect_type)
				continue;

			status = rmm_alloc(rmm, i, word_size, align,
					dsp_address, false);
			if (!status) {
				
				rmm_addr_obj->segid = i;
				break;
			}
		}
	}
func_cont:
	
	if (status == -ENOMEM && !mem_load_req) {
		dev_dbg(bridge, "%s: Preferred segment unavailable, trying "
			"another\n", __func__);
		for (i = 0; i < nldr_obj->dload_segs; i++) {
			
			if ((nldr_obj->seg_table[i] & mem_sect_type) !=
			    mem_sect_type)
				continue;

			status = rmm_alloc(rmm, i, word_size, align,
					   dsp_address, false);
			if (!status) {
				
				rmm_addr_obj->segid = i;
				break;
			}
		}
	}

	return status;
}

static int remote_free(void **ref, u16 space, u32 dsp_address,
			      u32 size, bool reserve)
{
	struct nldr_object *nldr_obj = (struct nldr_object *)ref;
	struct rmm_target_obj *rmm;
	u32 word_size;
	int status = -ENOMEM;	

	rmm = nldr_obj->rmm;

	
	word_size =
	    (size + nldr_obj->dsp_word_size -
	     1) / nldr_obj->dsp_word_size;

	if (rmm_free(rmm, space, dsp_address, word_size, reserve))
		status = 0;

	return status;
}

static void unload_lib(struct nldr_nodeobject *nldr_node_obj,
		       struct lib_node *root)
{
	struct dbll_attrs new_attrs;
	struct nldr_object *nldr_obj = nldr_node_obj->nldr_obj;
	u16 i;


	
	for (i = 0; i < root->dep_libs; i++)
		unload_lib(nldr_node_obj, &root->dep_libs_tree[i]);

	root->dep_libs = 0;

	new_attrs = nldr_obj->ldr_attrs;
	new_attrs.rmm_handle = nldr_obj->rmm;
	new_attrs.input_params = nldr_node_obj->priv_ref;
	new_attrs.base_image = false;
	new_attrs.sym_arg = root;

	if (root->lib) {
		
		nldr_obj->ldr_fxns.unload_fxn(root->lib, &new_attrs);
		nldr_obj->ldr_fxns.close_fxn(root->lib);
	}

	
	kfree(root->dep_libs_tree);
	root->dep_libs_tree = NULL;
}

static void unload_ovly(struct nldr_nodeobject *nldr_node_obj,
			enum nldr_phase phase)
{
	struct nldr_object *nldr_obj = nldr_node_obj->nldr_obj;
	struct ovly_node *po_node = NULL;
	struct ovly_sect *phase_sects = NULL;
	struct ovly_sect *other_sects_list = NULL;
	u16 i;
	u16 alloc_num = 0;
	u16 other_alloc = 0;
	u16 *ref_count = NULL;
	u16 *other_ref = NULL;

	
	for (i = 0; i < nldr_obj->ovly_nodes; i++) {
		if (is_equal_uuid
		    (&nldr_node_obj->uuid, &nldr_obj->ovly_table[i].uuid)) {
			
			po_node = &(nldr_obj->ovly_table[i]);
			break;
		}
	}


	if (!po_node)
		
		return;

	switch (phase) {
	case NLDR_CREATE:
		ref_count = &(po_node->create_ref);
		phase_sects = po_node->create_sects_list;
		alloc_num = po_node->create_sects;
		break;
	case NLDR_EXECUTE:
		ref_count = &(po_node->execute_ref);
		phase_sects = po_node->execute_sects_list;
		alloc_num = po_node->execute_sects;
		break;
	case NLDR_DELETE:
		ref_count = &(po_node->delete_ref);
		other_ref = &(po_node->other_ref);
		phase_sects = po_node->delete_sects_list;
		
		other_sects_list = po_node->other_sects_list;
		alloc_num = po_node->delete_sects;
		other_alloc = po_node->other_sects;
		break;
	default:
		break;
	}
	if (ref_count && (*ref_count > 0)) {
		*ref_count -= 1;
		if (other_ref) {
			*other_ref -= 1;
		}
	}

	if (ref_count && *ref_count == 0) {
		
		free_sects(nldr_obj, phase_sects, alloc_num);
	}
	if (other_ref && *other_ref == 0)
		free_sects(nldr_obj, other_sects_list, other_alloc);
}

static bool find_in_persistent_lib_array(struct nldr_nodeobject *nldr_node_obj,
					 struct dbll_library_obj *lib)
{
	s32 i = 0;

	for (i = 0; i < nldr_node_obj->pers_libs; i++) {
		if (lib == nldr_node_obj->pers_lib_table[i].lib)
			return true;

	}

	return false;
}

#ifdef CONFIG_TIDSPBRIDGE_BACKTRACE
int nldr_find_addr(struct nldr_nodeobject *nldr_node, u32 sym_addr,
			u32 offset_range, void *offset_output, char *sym_name)
{
	int status = 0;
	bool status1 = false;
	s32 i = 0;
	struct lib_node root = { NULL, 0, NULL };
	pr_debug("%s(0x%x, 0x%x, 0x%x, 0x%x,  %s)\n", __func__, (u32) nldr_node,
			sym_addr, offset_range, (u32) offset_output, sym_name);

	if (nldr_node->dynamic && *nldr_node->phase_split) {
		switch (nldr_node->phase) {
		case NLDR_CREATE:
			root = nldr_node->create_lib;
			break;
		case NLDR_EXECUTE:
			root = nldr_node->execute_lib;
			break;
		case NLDR_DELETE:
			root = nldr_node->delete_lib;
			break;
		default:
			break;
		}
	} else {
		
		root = nldr_node->root;
	}

	status1 = dbll_find_dsp_symbol(root.lib, sym_addr,
			offset_range, offset_output, sym_name);

	
	if (!status1)
		for (i = 0; i < root.dep_libs; i++) {
			status1 = dbll_find_dsp_symbol(
				root.dep_libs_tree[i].lib, sym_addr,
				offset_range, offset_output, sym_name);
			if (status1)
				
				break;
		}
	
	if (!status1)
		for (i = 0; i < nldr_node->pers_libs; i++) {
			status1 = dbll_find_dsp_symbol(
				nldr_node->pers_lib_table[i].lib, sym_addr,
				offset_range, offset_output, sym_name);
			if (status1)
				
				break;
		}

	if (!status1) {
		pr_debug("%s: Address 0x%x not found in range %d.\n",
					__func__, sym_addr, offset_range);
		status = -ESPIPE;
	}

	return status;
}
#endif
