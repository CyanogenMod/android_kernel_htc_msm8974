/*
 * dbdcd.c
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * This file contains the implementation of the DSP/BIOS Bridge
 * Configuration Database (DCD).
 *
 * Notes:
 *   The fxn dcd_get_objects can apply a callback fxn to each DCD object
 *   that is located in a specified COFF file.  At the moment,
 *   dcd_auto_register, dcd_auto_unregister, and NLDR module all use
 *   dcd_get_objects.
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

#include <dspbridge/uuidutil.h>

#include <dspbridge/dbdcd.h>

#define MAX_INT2CHAR_LENGTH     16	

#define DEPLIBSECT		".dspbridge_deplibs"

struct dcd_manager {
	struct cod_manager *cod_mgr;	
};

static struct list_head reg_key_list;
static DEFINE_SPINLOCK(dbdcd_lock);

static u32 refs;
static u32 enum_refs;

static s32 atoi(char *psz_buf);
static int get_attrs_from_buf(char *psz_buf, u32 ul_buf_size,
				     enum dsp_dcdobjtype obj_type,
				     struct dcd_genericobj *gen_obj);
static void compress_buf(char *psz_buf, u32 ul_buf_size, s32 char_size);
static char dsp_char2_gpp_char(char *word, s32 dsp_char_size);
static int get_dep_lib_info(struct dcd_manager *hdcd_mgr,
				   struct dsp_uuid *uuid_obj,
				   u16 *num_libs,
				   u16 *num_pers_libs,
				   struct dsp_uuid *dep_lib_uuids,
				   bool *prstnt_dep_libs,
				   enum nldr_phase phase);

int dcd_auto_register(struct dcd_manager *hdcd_mgr,
			     char *sz_coff_path)
{
	int status = 0;

	if (hdcd_mgr)
		status = dcd_get_objects(hdcd_mgr, sz_coff_path,
					 (dcd_registerfxn) dcd_register_object,
					 (void *)sz_coff_path);
	else
		status = -EFAULT;

	return status;
}

int dcd_auto_unregister(struct dcd_manager *hdcd_mgr,
			       char *sz_coff_path)
{
	int status = 0;

	if (hdcd_mgr)
		status = dcd_get_objects(hdcd_mgr, sz_coff_path,
					 (dcd_registerfxn) dcd_register_object,
					 NULL);
	else
		status = -EFAULT;

	return status;
}

int dcd_create_manager(char *sz_zl_dll_name,
			      struct dcd_manager **dcd_mgr)
{
	struct cod_manager *cod_mgr;	
	struct dcd_manager *dcd_mgr_obj = NULL;	
	int status = 0;

	status = cod_create(&cod_mgr, sz_zl_dll_name);
	if (status)
		goto func_end;

	
	dcd_mgr_obj = kzalloc(sizeof(struct dcd_manager), GFP_KERNEL);
	if (dcd_mgr_obj != NULL) {
		
		dcd_mgr_obj->cod_mgr = cod_mgr;

		
		*dcd_mgr = dcd_mgr_obj;
	} else {
		status = -ENOMEM;

		cod_delete(cod_mgr);
	}

func_end:
	return status;
}

int dcd_destroy_manager(struct dcd_manager *hdcd_mgr)
{
	struct dcd_manager *dcd_mgr_obj = hdcd_mgr;
	int status = -EFAULT;

	if (hdcd_mgr) {
		
		cod_delete(dcd_mgr_obj->cod_mgr);

		
		kfree(dcd_mgr_obj);

		status = 0;
	}

	return status;
}

int dcd_enumerate_object(s32 index, enum dsp_dcdobjtype obj_type,
				struct dsp_uuid *uuid_obj)
{
	int status = 0;
	char sz_reg_key[DCD_MAXPATHLENGTH];
	char sz_value[DCD_MAXPATHLENGTH];
	struct dsp_uuid dsp_uuid_obj;
	char sz_obj_type[MAX_INT2CHAR_LENGTH];	
	u32 dw_key_len = 0;
	struct dcd_key_elem *dcd_key;
	int len;

	if ((index != 0) && (enum_refs == 0)) {
		status = -EIDRM;
	} else {
		dw_key_len = strlen(DCD_REGKEY) + 1 + sizeof(sz_obj_type) + 1;

		strncpy(sz_reg_key, DCD_REGKEY, strlen(DCD_REGKEY) + 1);
		if ((strlen(sz_reg_key) + strlen("_\0")) <
		    DCD_MAXPATHLENGTH) {
			strncat(sz_reg_key, "_\0", 2);
		} else {
			status = -EPERM;
		}

		status = snprintf(sz_obj_type, MAX_INT2CHAR_LENGTH, "%d",
				  obj_type);

		if (status == -1) {
			status = -EPERM;
		} else {
			status = 0;
			if ((strlen(sz_reg_key) + strlen(sz_obj_type)) <
			    DCD_MAXPATHLENGTH) {
				strncat(sz_reg_key, sz_obj_type,
					strlen(sz_obj_type) + 1);
			} else {
				status = -EPERM;
			}
		}

		if (!status) {
			len = strlen(sz_reg_key);
			spin_lock(&dbdcd_lock);
			list_for_each_entry(dcd_key, &reg_key_list, link) {
				if (!strncmp(dcd_key->name, sz_reg_key, len)
						&& !index--) {
					strncpy(sz_value, &dcd_key->name[len],
					       strlen(&dcd_key->name[len]) + 1);
						break;
				}
			}
			spin_unlock(&dbdcd_lock);

			if (&dcd_key->link == &reg_key_list)
				status = -ENODATA;
		}

		if (!status) {
			uuid_uuid_from_string(sz_value, &dsp_uuid_obj);

			*uuid_obj = dsp_uuid_obj;

			
			enum_refs++;

			status = 0;
		} else if (status == -ENODATA) {
			
			enum_refs = 0;

			status = ENODATA;
		} else {
			status = -EPERM;
		}
	}

	return status;
}

void dcd_exit(void)
{
	struct dcd_key_elem *rv, *rv_tmp;

	refs--;
	if (refs == 0) {
		list_for_each_entry_safe(rv, rv_tmp, &reg_key_list, link) {
			list_del(&rv->link);
			kfree(rv->path);
			kfree(rv);
		}
	}

}

int dcd_get_dep_libs(struct dcd_manager *hdcd_mgr,
			    struct dsp_uuid *uuid_obj,
			    u16 num_libs, struct dsp_uuid *dep_lib_uuids,
			    bool *prstnt_dep_libs,
			    enum nldr_phase phase)
{
	int status = 0;

	status =
	    get_dep_lib_info(hdcd_mgr, uuid_obj, &num_libs, NULL, dep_lib_uuids,
			     prstnt_dep_libs, phase);

	return status;
}

int dcd_get_num_dep_libs(struct dcd_manager *hdcd_mgr,
				struct dsp_uuid *uuid_obj,
				u16 *num_libs, u16 *num_pers_libs,
				enum nldr_phase phase)
{
	int status = 0;

	status = get_dep_lib_info(hdcd_mgr, uuid_obj, num_libs, num_pers_libs,
				  NULL, NULL, phase);

	return status;
}

int dcd_get_object_def(struct dcd_manager *hdcd_mgr,
			      struct dsp_uuid *obj_uuid,
			      enum dsp_dcdobjtype obj_type,
			      struct dcd_genericobj *obj_def)
{
	struct dcd_manager *dcd_mgr_obj = hdcd_mgr;	
	struct cod_libraryobj *lib = NULL;
	int status = 0;
	u32 ul_addr = 0;	
	u32 ul_len = 0;		
	u32 dw_buf_size;	
	char sz_reg_key[DCD_MAXPATHLENGTH];
	char *sz_uuid;		
	struct dcd_key_elem *dcd_key = NULL;
	char sz_sect_name[MAXUUIDLEN + 2];	
	char *psz_coff_buf;
	u32 dw_key_len;		
	char sz_obj_type[MAX_INT2CHAR_LENGTH];	

	sz_uuid = kzalloc(MAXUUIDLEN, GFP_KERNEL);
	if (!sz_uuid) {
		status = -ENOMEM;
		goto func_end;
	}

	if (!hdcd_mgr) {
		status = -EFAULT;
		goto func_end;
	}

	dw_key_len = strlen(DCD_REGKEY) + 1 + sizeof(sz_obj_type) + 1;

	
	strncpy(sz_reg_key, DCD_REGKEY, strlen(DCD_REGKEY) + 1);

	if ((strlen(sz_reg_key) + strlen("_\0")) < DCD_MAXPATHLENGTH)
		strncat(sz_reg_key, "_\0", 2);
	else
		status = -EPERM;

	status = snprintf(sz_obj_type, MAX_INT2CHAR_LENGTH, "%d", obj_type);
	if (status == -1) {
		status = -EPERM;
	} else {
		status = 0;

		if ((strlen(sz_reg_key) + strlen(sz_obj_type)) <
		    DCD_MAXPATHLENGTH) {
			strncat(sz_reg_key, sz_obj_type,
				strlen(sz_obj_type) + 1);
		} else {
			status = -EPERM;
		}

		
		uuid_uuid_to_string(obj_uuid, sz_uuid, MAXUUIDLEN);

		if ((strlen(sz_reg_key) + MAXUUIDLEN) < DCD_MAXPATHLENGTH)
			strncat(sz_reg_key, sz_uuid, MAXUUIDLEN);
		else
			status = -EPERM;

		
		dw_buf_size = DCD_MAXPATHLENGTH;
	}
	if (!status) {
		spin_lock(&dbdcd_lock);
		list_for_each_entry(dcd_key, &reg_key_list, link) {
			if (!strncmp(dcd_key->name, sz_reg_key,
						strlen(sz_reg_key) + 1))
				break;
		}
		spin_unlock(&dbdcd_lock);
		if (&dcd_key->link == &reg_key_list) {
			status = -ENOKEY;
			goto func_end;
		}
	}


	
	status = cod_open(dcd_mgr_obj->cod_mgr, dcd_key->path,
							COD_NOLOAD, &lib);
	if (status) {
		status = -EACCES;
		goto func_end;
	}

	

	strncpy(sz_sect_name, ".", 2);
	strncat(sz_sect_name, sz_uuid, strlen(sz_uuid));

	
	status = cod_get_section(lib, sz_sect_name, &ul_addr, &ul_len);
	if (status) {
		status = -EACCES;
		goto func_end;
	}

	
	psz_coff_buf = kzalloc(ul_len + 4, GFP_KERNEL);
	if (psz_coff_buf == NULL) {
		status = -ENOMEM;
		goto func_end;
	}
#ifdef _DB_TIOMAP
	if (strstr(dcd_key->path, "iva") == NULL) {
		
		status =
		    cod_read_section(lib, sz_sect_name, psz_coff_buf, ul_len);
	} else {
		status =
		    cod_read_section(lib, sz_sect_name, psz_coff_buf, ul_len);
		dev_dbg(bridge, "%s: Skipped Byte swap for IVA!!\n", __func__);
	}
#else
	status = cod_read_section(lib, sz_sect_name, psz_coff_buf, ul_len);
#endif
	if (!status) {
		
		if (strstr(dcd_key->path, "iva") == NULL) {
			compress_buf(psz_coff_buf, ul_len, DSPWORDSIZE);
		} else {
			compress_buf(psz_coff_buf, ul_len, 1);
			dev_dbg(bridge, "%s: Compressing IVA COFF buffer by 1 "
				"for IVA!!\n", __func__);
		}

		
		status =
		    get_attrs_from_buf(psz_coff_buf, ul_len, obj_type, obj_def);
		if (status)
			status = -EACCES;
	} else {
		status = -EACCES;
	}

	
	kfree(psz_coff_buf);
func_end:
	if (lib)
		cod_close(lib);

	kfree(sz_uuid);

	return status;
}

int dcd_get_objects(struct dcd_manager *hdcd_mgr,
			   char *sz_coff_path, dcd_registerfxn register_fxn,
			   void *handle)
{
	struct dcd_manager *dcd_mgr_obj = hdcd_mgr;
	int status = 0;
	char *psz_coff_buf;
	char *psz_cur;
	struct cod_libraryobj *lib = NULL;
	u32 ul_addr = 0;	
	u32 ul_len = 0;		
	char seps[] = ":, ";
	char *token = NULL;
	struct dsp_uuid dsp_uuid_obj;
	s32 object_type;

	if (!hdcd_mgr) {
		status = -EFAULT;
		goto func_end;
	}

	
	status = cod_open(dcd_mgr_obj->cod_mgr, sz_coff_path, COD_NOLOAD, &lib);
	if (status) {
		status = -EACCES;
		goto func_cont;
	}

	
	status = cod_get_section(lib, DCD_REGISTER_SECTION, &ul_addr, &ul_len);
	if (status || !(ul_len > 0)) {
		status = -EACCES;
		goto func_cont;
	}

	
	psz_coff_buf = kzalloc(ul_len + 4, GFP_KERNEL);
	if (psz_coff_buf == NULL) {
		status = -ENOMEM;
		goto func_cont;
	}
#ifdef _DB_TIOMAP
	if (strstr(sz_coff_path, "iva") == NULL) {
		
		status = cod_read_section(lib, DCD_REGISTER_SECTION,
					  psz_coff_buf, ul_len);
	} else {
		dev_dbg(bridge, "%s: Skipped Byte swap for IVA!!\n", __func__);
		status = cod_read_section(lib, DCD_REGISTER_SECTION,
					  psz_coff_buf, ul_len);
	}
#else
	status =
	    cod_read_section(lib, DCD_REGISTER_SECTION, psz_coff_buf, ul_len);
#endif
	if (!status) {
		
		if (strstr(sz_coff_path, "iva") == NULL) {
			compress_buf(psz_coff_buf, ul_len, DSPWORDSIZE);
		} else {
			compress_buf(psz_coff_buf, ul_len, 1);
			dev_dbg(bridge, "%s: Compress COFF buffer with 1 word "
				"for IVA!!\n", __func__);
		}

		
		psz_cur = psz_coff_buf;
		while ((token = strsep(&psz_cur, seps)) && *token != '\0') {
			
			uuid_uuid_from_string(token, &dsp_uuid_obj);

			
			token = strsep(&psz_cur, seps);

			
			object_type = atoi(token);

			status =
			    register_fxn(&dsp_uuid_obj, object_type, handle);
			if (status) {
				
				break;
			}
		}
	} else {
		status = -EACCES;
	}

	
	kfree(psz_coff_buf);
func_cont:
	if (lib)
		cod_close(lib);

func_end:
	return status;
}

int dcd_get_library_name(struct dcd_manager *hdcd_mgr,
				struct dsp_uuid *uuid_obj,
				char *str_lib_name,
				u32 *buff_size,
				enum nldr_phase phase, bool *phase_split)
{
	char sz_reg_key[DCD_MAXPATHLENGTH];
	char sz_uuid[MAXUUIDLEN];
	u32 dw_key_len;		
	char sz_obj_type[MAX_INT2CHAR_LENGTH];	
	int status = 0;
	struct dcd_key_elem *dcd_key = NULL;

	dev_dbg(bridge, "%s: hdcd_mgr %p, uuid_obj %p, str_lib_name %p,"
		" buff_size %p\n", __func__, hdcd_mgr, uuid_obj, str_lib_name,
		buff_size);

	dw_key_len = strlen(DCD_REGKEY) + 1 + sizeof(sz_obj_type) + 1;

	
	strncpy(sz_reg_key, DCD_REGKEY, strlen(DCD_REGKEY) + 1);
	if ((strlen(sz_reg_key) + strlen("_\0")) < DCD_MAXPATHLENGTH)
		strncat(sz_reg_key, "_\0", 2);
	else
		status = -EPERM;

	switch (phase) {
	case NLDR_CREATE:
		
		sprintf(sz_obj_type, "%d", DSP_DCDCREATELIBTYPE);
		break;
	case NLDR_EXECUTE:
		
		sprintf(sz_obj_type, "%d", DSP_DCDEXECUTELIBTYPE);
		break;
	case NLDR_DELETE:
		
		sprintf(sz_obj_type, "%d", DSP_DCDDELETELIBTYPE);
		break;
	case NLDR_NOPHASE:
		
		sprintf(sz_obj_type, "%d", DSP_DCDLIBRARYTYPE);
		break;
	default:
		status = -EINVAL;
	}
	if (!status) {
		if ((strlen(sz_reg_key) + strlen(sz_obj_type)) <
		    DCD_MAXPATHLENGTH) {
			strncat(sz_reg_key, sz_obj_type,
				strlen(sz_obj_type) + 1);
		} else {
			status = -EPERM;
		}
		
		uuid_uuid_to_string(uuid_obj, sz_uuid, MAXUUIDLEN);
		if ((strlen(sz_reg_key) + MAXUUIDLEN) < DCD_MAXPATHLENGTH)
			strncat(sz_reg_key, sz_uuid, MAXUUIDLEN);
		else
			status = -EPERM;
	}
	if (!status) {
		spin_lock(&dbdcd_lock);
		list_for_each_entry(dcd_key, &reg_key_list, link) {
			
			if (!strncmp(dcd_key->name, sz_reg_key,
						strlen(sz_reg_key) + 1))
				break;
		}
		spin_unlock(&dbdcd_lock);
	}

	if (&dcd_key->link == &reg_key_list)
		status = -ENOKEY;

	
	if (status && phase != NLDR_NOPHASE) {
		if (phase_split)
			*phase_split = false;

		strncpy(sz_reg_key, DCD_REGKEY, strlen(DCD_REGKEY) + 1);
		if ((strlen(sz_reg_key) + strlen("_\0")) <
		    DCD_MAXPATHLENGTH) {
			strncat(sz_reg_key, "_\0", 2);
		} else {
			status = -EPERM;
		}
		sprintf(sz_obj_type, "%d", DSP_DCDLIBRARYTYPE);
		if ((strlen(sz_reg_key) + strlen(sz_obj_type))
		    < DCD_MAXPATHLENGTH) {
			strncat(sz_reg_key, sz_obj_type,
				strlen(sz_obj_type) + 1);
		} else {
			status = -EPERM;
		}
		uuid_uuid_to_string(uuid_obj, sz_uuid, MAXUUIDLEN);
		if ((strlen(sz_reg_key) + MAXUUIDLEN) < DCD_MAXPATHLENGTH)
			strncat(sz_reg_key, sz_uuid, MAXUUIDLEN);
		else
			status = -EPERM;

		spin_lock(&dbdcd_lock);
		list_for_each_entry(dcd_key, &reg_key_list, link) {
			
			if (!strncmp(dcd_key->name, sz_reg_key,
						strlen(sz_reg_key) + 1))
				break;
		}
		spin_unlock(&dbdcd_lock);

		status = (&dcd_key->link != &reg_key_list) ?
						0 : -ENOKEY;
	}

	if (!status)
		memcpy(str_lib_name, dcd_key->path, strlen(dcd_key->path) + 1);
	return status;
}

bool dcd_init(void)
{
	bool ret = true;

	if (refs == 0)
		INIT_LIST_HEAD(&reg_key_list);

	if (ret)
		refs++;

	return ret;
}

int dcd_register_object(struct dsp_uuid *uuid_obj,
			       enum dsp_dcdobjtype obj_type,
			       char *psz_path_name)
{
	int status = 0;
	char sz_reg_key[DCD_MAXPATHLENGTH];
	char sz_uuid[MAXUUIDLEN + 1];
	u32 dw_path_size = 0;
	u32 dw_key_len;		
	char sz_obj_type[MAX_INT2CHAR_LENGTH];	
	struct dcd_key_elem *dcd_key = NULL;

	dev_dbg(bridge, "%s: object UUID %p, obj_type %d, szPathName %s\n",
		__func__, uuid_obj, obj_type, psz_path_name);

	dw_key_len = strlen(DCD_REGKEY) + 1 + sizeof(sz_obj_type) + 1;

	
	strncpy(sz_reg_key, DCD_REGKEY, strlen(DCD_REGKEY) + 1);
	if ((strlen(sz_reg_key) + strlen("_\0")) < DCD_MAXPATHLENGTH)
		strncat(sz_reg_key, "_\0", 2);
	else {
		status = -EPERM;
		goto func_end;
	}

	status = snprintf(sz_obj_type, MAX_INT2CHAR_LENGTH, "%d", obj_type);
	if (status == -1) {
		status = -EPERM;
	} else {
		status = 0;
		if ((strlen(sz_reg_key) + strlen(sz_obj_type)) <
		    DCD_MAXPATHLENGTH) {
			strncat(sz_reg_key, sz_obj_type,
				strlen(sz_obj_type) + 1);
		} else
			status = -EPERM;

		
		uuid_uuid_to_string(uuid_obj, sz_uuid, MAXUUIDLEN);
		if ((strlen(sz_reg_key) + MAXUUIDLEN) < DCD_MAXPATHLENGTH)
			strncat(sz_reg_key, sz_uuid, MAXUUIDLEN);
		else
			status = -EPERM;
	}

	if (status)
		goto func_end;


	if (psz_path_name) {
		dw_path_size = strlen(psz_path_name) + 1;
		spin_lock(&dbdcd_lock);
		list_for_each_entry(dcd_key, &reg_key_list, link) {
			
			if (!strncmp(dcd_key->name, sz_reg_key,
						strlen(sz_reg_key) + 1))
				break;
		}
		spin_unlock(&dbdcd_lock);
		if (&dcd_key->link == &reg_key_list) {

			dcd_key = kmalloc(sizeof(struct dcd_key_elem),
								GFP_KERNEL);
			if (!dcd_key) {
				status = -ENOMEM;
				goto func_end;
			}

			dcd_key->path = kmalloc(strlen(sz_reg_key) + 1,
								GFP_KERNEL);

			if (!dcd_key->path) {
				kfree(dcd_key);
				status = -ENOMEM;
				goto func_end;
			}

			strncpy(dcd_key->name, sz_reg_key,
						strlen(sz_reg_key) + 1);
			strncpy(dcd_key->path, psz_path_name ,
						dw_path_size);
			spin_lock(&dbdcd_lock);
			list_add_tail(&dcd_key->link, &reg_key_list);
			spin_unlock(&dbdcd_lock);
		} else {
			
			if (strncmp(dcd_key->path, psz_path_name,
							dw_path_size)) {
				
				kfree(dcd_key->path);
				dcd_key->path = kmalloc(dw_path_size,
								GFP_KERNEL);
				if (dcd_key->path == NULL) {
					status = -ENOMEM;
					goto func_end;
				}
			}

			
			memcpy(dcd_key->path, psz_path_name, dw_path_size);
		}
		dev_dbg(bridge, "%s: psz_path_name=%s, dw_path_size=%d\n",
			__func__, psz_path_name, dw_path_size);
	} else {
		
		spin_lock(&dbdcd_lock);
		list_for_each_entry(dcd_key, &reg_key_list, link) {
			if (!strncmp(dcd_key->name, sz_reg_key,
						strlen(sz_reg_key) + 1)) {
				list_del(&dcd_key->link);
				kfree(dcd_key->path);
				kfree(dcd_key);
				break;
			}
		}
		spin_unlock(&dbdcd_lock);
		if (&dcd_key->link == &reg_key_list)
			status = -EPERM;
	}

	if (!status) {
		enum_refs = 0;
	}
func_end:
	return status;
}

int dcd_unregister_object(struct dsp_uuid *uuid_obj,
				 enum dsp_dcdobjtype obj_type)
{
	int status = 0;

	status = dcd_register_object(uuid_obj, obj_type, NULL);

	return status;
}


static s32 atoi(char *psz_buf)
{
	char *pch = psz_buf;
	s32 base = 0;

	while (isspace(*pch))
		pch++;

	if (*pch == '-' || *pch == '+') {
		base = 10;
		pch++;
	} else if (*pch && tolower(pch[strlen(pch) - 1]) == 'h') {
		base = 16;
	}

	return simple_strtoul(pch, NULL, base);
}

static int get_attrs_from_buf(char *psz_buf, u32 ul_buf_size,
				     enum dsp_dcdobjtype obj_type,
				     struct dcd_genericobj *gen_obj)
{
	int status = 0;
	char seps[] = ", ";
	char *psz_cur;
	char *token;
	s32 token_len = 0;
	u32 i = 0;
#ifdef _DB_TIOMAP
	s32 entry_id;
#endif

	switch (obj_type) {
	case DSP_DCDNODETYPE:
		psz_cur = psz_buf;
		token = strsep(&psz_cur, seps);

		
		gen_obj->obj_data.node_obj.ndb_props.cb_struct =
		    (u32) atoi(token);
		token = strsep(&psz_cur, seps);

		
		uuid_uuid_from_string(token,
				      &gen_obj->obj_data.node_obj.ndb_props.
				      ui_node_id);
		token = strsep(&psz_cur, seps);

		
		token_len = strlen(token);
		if (token_len > DSP_MAXNAMELEN - 1)
			token_len = DSP_MAXNAMELEN - 1;

		strncpy(gen_obj->obj_data.node_obj.ndb_props.ac_name,
			token, token_len);
		gen_obj->obj_data.node_obj.ndb_props.ac_name[token_len] = '\0';
		token = strsep(&psz_cur, seps);
		
		gen_obj->obj_data.node_obj.ndb_props.ntype = atoi(token);
		token = strsep(&psz_cur, seps);
		
		gen_obj->obj_data.node_obj.ndb_props.cache_on_gpp = atoi(token);
		token = strsep(&psz_cur, seps);
		
		gen_obj->obj_data.node_obj.ndb_props.dsp_resource_reqmts.
		    cb_struct = (u32) atoi(token);
		token = strsep(&psz_cur, seps);

		gen_obj->obj_data.node_obj.ndb_props.
		    dsp_resource_reqmts.static_data_size = atoi(token);
		token = strsep(&psz_cur, seps);
		gen_obj->obj_data.node_obj.ndb_props.
		    dsp_resource_reqmts.global_data_size = atoi(token);
		token = strsep(&psz_cur, seps);
		gen_obj->obj_data.node_obj.ndb_props.
		    dsp_resource_reqmts.program_mem_size = atoi(token);
		token = strsep(&psz_cur, seps);
		gen_obj->obj_data.node_obj.ndb_props.
		    dsp_resource_reqmts.wc_execution_time = atoi(token);
		token = strsep(&psz_cur, seps);
		gen_obj->obj_data.node_obj.ndb_props.
		    dsp_resource_reqmts.wc_period = atoi(token);
		token = strsep(&psz_cur, seps);

		gen_obj->obj_data.node_obj.ndb_props.
		    dsp_resource_reqmts.wc_deadline = atoi(token);
		token = strsep(&psz_cur, seps);

		gen_obj->obj_data.node_obj.ndb_props.
		    dsp_resource_reqmts.avg_exection_time = atoi(token);
		token = strsep(&psz_cur, seps);

		gen_obj->obj_data.node_obj.ndb_props.
		    dsp_resource_reqmts.minimum_period = atoi(token);
		token = strsep(&psz_cur, seps);

		
		gen_obj->obj_data.node_obj.ndb_props.prio = atoi(token);
		token = strsep(&psz_cur, seps);

		
		gen_obj->obj_data.node_obj.ndb_props.stack_size = atoi(token);
		token = strsep(&psz_cur, seps);

		
		gen_obj->obj_data.node_obj.ndb_props.sys_stack_size =
		    atoi(token);
		token = strsep(&psz_cur, seps);

		
		gen_obj->obj_data.node_obj.ndb_props.stack_seg = atoi(token);
		token = strsep(&psz_cur, seps);

		
		gen_obj->obj_data.node_obj.ndb_props.message_depth =
		    atoi(token);
		token = strsep(&psz_cur, seps);

		
		gen_obj->obj_data.node_obj.ndb_props.num_input_streams =
		    atoi(token);
		token = strsep(&psz_cur, seps);

		
		gen_obj->obj_data.node_obj.ndb_props.num_output_streams =
		    atoi(token);
		token = strsep(&psz_cur, seps);

		
		gen_obj->obj_data.node_obj.ndb_props.timeout = atoi(token);
		token = strsep(&psz_cur, seps);

		
		token_len = strlen(token);
		gen_obj->obj_data.node_obj.str_create_phase_fxn =
					kzalloc(token_len + 1, GFP_KERNEL);
		strncpy(gen_obj->obj_data.node_obj.str_create_phase_fxn,
			token, token_len);
		gen_obj->obj_data.node_obj.str_create_phase_fxn[token_len] =
		    '\0';
		token = strsep(&psz_cur, seps);

		
		token_len = strlen(token);
		gen_obj->obj_data.node_obj.str_execute_phase_fxn =
					kzalloc(token_len + 1, GFP_KERNEL);
		strncpy(gen_obj->obj_data.node_obj.str_execute_phase_fxn,
			token, token_len);
		gen_obj->obj_data.node_obj.str_execute_phase_fxn[token_len] =
		    '\0';
		token = strsep(&psz_cur, seps);

		
		token_len = strlen(token);
		gen_obj->obj_data.node_obj.str_delete_phase_fxn =
					kzalloc(token_len + 1, GFP_KERNEL);
		strncpy(gen_obj->obj_data.node_obj.str_delete_phase_fxn,
			token, token_len);
		gen_obj->obj_data.node_obj.str_delete_phase_fxn[token_len] =
		    '\0';
		token = strsep(&psz_cur, seps);

		
		gen_obj->obj_data.node_obj.msg_segid = atoi(token);
		token = strsep(&psz_cur, seps);

		
		gen_obj->obj_data.node_obj.msg_notify_type = atoi(token);
		token = strsep(&psz_cur, seps);

		
		if (token) {
			token_len = strlen(token);
			gen_obj->obj_data.node_obj.str_i_alg_name =
					kzalloc(token_len + 1, GFP_KERNEL);
			strncpy(gen_obj->obj_data.node_obj.str_i_alg_name,
				token, token_len);
			gen_obj->obj_data.node_obj.str_i_alg_name[token_len] =
			    '\0';
			token = strsep(&psz_cur, seps);
		}

		
		if (token) {
			gen_obj->obj_data.node_obj.load_type = atoi(token);
			token = strsep(&psz_cur, seps);
		}

		
		if (token) {
			gen_obj->obj_data.node_obj.data_mem_seg_mask =
			    atoi(token);
			token = strsep(&psz_cur, seps);
		}

		
		if (token) {
			gen_obj->obj_data.node_obj.code_mem_seg_mask =
			    atoi(token);
			token = strsep(&psz_cur, seps);
		}

		
		if (token) {

			gen_obj->obj_data.node_obj.ndb_props.count_profiles =
			    atoi(token);
			for (i = 0;
			     i <
			     gen_obj->obj_data.node_obj.
			     ndb_props.count_profiles; i++) {
				token = strsep(&psz_cur, seps);
				if (token) {
					
					gen_obj->obj_data.node_obj.
					    ndb_props.node_profiles[i].
					    heap_size = atoi(token);
				}
			}
		}
		token = strsep(&psz_cur, seps);
		if (token) {
			gen_obj->obj_data.node_obj.ndb_props.stack_seg_name =
			    (u32) (token);
		}

		break;

	case DSP_DCDPROCESSORTYPE:
		psz_cur = psz_buf;
		token = strsep(&psz_cur, seps);

		gen_obj->obj_data.proc_info.cb_struct = atoi(token);
		token = strsep(&psz_cur, seps);

		gen_obj->obj_data.proc_info.processor_family = atoi(token);
		token = strsep(&psz_cur, seps);

		gen_obj->obj_data.proc_info.processor_type = atoi(token);
		token = strsep(&psz_cur, seps);

		gen_obj->obj_data.proc_info.clock_rate = atoi(token);
		token = strsep(&psz_cur, seps);

		gen_obj->obj_data.proc_info.internal_mem_size = atoi(token);
		token = strsep(&psz_cur, seps);

		gen_obj->obj_data.proc_info.external_mem_size = atoi(token);
		token = strsep(&psz_cur, seps);

		gen_obj->obj_data.proc_info.processor_id = atoi(token);
		token = strsep(&psz_cur, seps);

		gen_obj->obj_data.proc_info.ty_running_rtos = atoi(token);
		token = strsep(&psz_cur, seps);

		gen_obj->obj_data.proc_info.node_min_priority = atoi(token);
		token = strsep(&psz_cur, seps);

		gen_obj->obj_data.proc_info.node_max_priority = atoi(token);

#ifdef _DB_TIOMAP
		
		
		for (entry_id = 0; entry_id < 7; entry_id++) {
			token = strsep(&psz_cur, seps);
			gen_obj->obj_data.ext_proc_obj.ty_tlb[entry_id].
			    gpp_phys = atoi(token);

			token = strsep(&psz_cur, seps);
			gen_obj->obj_data.ext_proc_obj.ty_tlb[entry_id].
			    dsp_virt = atoi(token);
		}
#endif

		break;

	default:
		status = -EPERM;
		break;
	}

	return status;
}

static void compress_buf(char *psz_buf, u32 ul_buf_size, s32 char_size)
{
	char *p;
	char ch;
	char *q;

	p = psz_buf;
	if (p == NULL)
		return;

	for (q = psz_buf; q < (psz_buf + ul_buf_size);) {
		ch = dsp_char2_gpp_char(q, char_size);
		if (ch == '\\') {
			q += char_size;
			ch = dsp_char2_gpp_char(q, char_size);
			switch (ch) {
			case 't':
				*p = '\t';
				break;

			case 'n':
				*p = '\n';
				break;

			case 'r':
				*p = '\r';
				break;

			case '0':
				*p = '\0';
				break;

			default:
				*p = ch;
				break;
			}
		} else {
			*p = ch;
		}
		p++;
		q += char_size;
	}

	
	while (p < q)
		*p++ = '\0';
}

static char dsp_char2_gpp_char(char *word, s32 dsp_char_size)
{
	char ch = '\0';
	char *ch_src;
	s32 i;

	for (ch_src = word, i = dsp_char_size; i > 0; i--)
		ch |= *ch_src++;

	return ch;
}

static int get_dep_lib_info(struct dcd_manager *hdcd_mgr,
				   struct dsp_uuid *uuid_obj,
				   u16 *num_libs,
				   u16 *num_pers_libs,
				   struct dsp_uuid *dep_lib_uuids,
				   bool *prstnt_dep_libs,
				   enum nldr_phase phase)
{
	struct dcd_manager *dcd_mgr_obj = hdcd_mgr;
	char *psz_coff_buf = NULL;
	char *psz_cur;
	char *psz_file_name = NULL;
	struct cod_libraryobj *lib = NULL;
	u32 ul_addr = 0;	
	u32 ul_len = 0;		
	u32 dw_data_size = COD_MAXPATHLENGTH;
	char seps[] = ", ";
	char *token = NULL;
	bool get_uuids = (dep_lib_uuids != NULL);
	u16 dep_libs = 0;
	int status = 0;

	if (!get_uuids) {
		*num_libs = 0;
		*num_pers_libs = 0;
	}

	
	psz_file_name = kzalloc(dw_data_size, GFP_KERNEL);
	if (psz_file_name == NULL) {
		status = -ENOMEM;
	} else {
		
		status = dcd_get_library_name(hdcd_mgr, uuid_obj, psz_file_name,
					      &dw_data_size, phase, NULL);
	}

	
	if (!status) {
		status = cod_open(dcd_mgr_obj->cod_mgr, psz_file_name,
				  COD_NOLOAD, &lib);
	}
	if (!status) {
		
		status = cod_get_section(lib, DEPLIBSECT, &ul_addr, &ul_len);

		if (status) {
			
			ul_len = 0;
			status = 0;
		}
	}

	if (status || !(ul_len > 0))
		goto func_cont;

	
	psz_coff_buf = kzalloc(ul_len + 4, GFP_KERNEL);
	if (psz_coff_buf == NULL)
		status = -ENOMEM;

	
	status = cod_read_section(lib, DEPLIBSECT, psz_coff_buf, ul_len);
	if (status)
		goto func_cont;

	
	compress_buf(psz_coff_buf, ul_len, DSPWORDSIZE);

	
	psz_cur = psz_coff_buf;
	while ((token = strsep(&psz_cur, seps)) && *token != '\0') {
		if (get_uuids) {
			if (dep_libs >= *num_libs) {
				
				break;
			} else {
				
				uuid_uuid_from_string(token,
						      &(dep_lib_uuids
							[dep_libs]));
				
				token = strsep(&psz_cur, seps);
				prstnt_dep_libs[dep_libs] = atoi(token);
				dep_libs++;
			}
		} else {
			
			token = strsep(&psz_cur, seps);
			if (atoi(token))
				(*num_pers_libs)++;

			
			(*num_libs)++;
		}
	}
func_cont:
	if (lib)
		cod_close(lib);

	
	kfree(psz_file_name);

	kfree(psz_coff_buf);

	return status;
}
