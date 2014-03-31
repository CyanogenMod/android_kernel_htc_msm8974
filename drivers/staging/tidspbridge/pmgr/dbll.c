/*
 * dbll.c
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Copyright (C) 2005-2006 Texas Instruments, Inc.
 *
 * This package is free software;  you can redistribute it and/or modify
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

#include <dspbridge/gh.h>


#include <dspbridge/dynamic_loader.h>
#include <dspbridge/getsection.h>

#include <dspbridge/dbll.h>
#include <dspbridge/rmm.h>

#define MAXBUCKETS 211

#define MAXEXPR 128

#define DOFF_ALIGN(x) (((x) + 3) & ~3UL)

struct dbll_tar_obj {
	struct dbll_attrs attrs;
	struct dbll_library_obj *head;	
};

struct dbll_stream {
	struct dynamic_loader_stream dl_stream;
	struct dbll_library_obj *lib;
};

struct ldr_symbol {
	struct dynamic_loader_sym dl_symbol;
	struct dbll_library_obj *lib;
};

struct dbll_alloc {
	struct dynamic_loader_allocate dl_alloc;
	struct dbll_library_obj *lib;
};

struct dbll_init_obj {
	struct dynamic_loader_initialize dl_init;
	struct dbll_library_obj *lib;
};


struct dbll_library_obj {
	struct dbll_library_obj *next;	
	struct dbll_library_obj *prev;	
	struct dbll_tar_obj *target_obj;	

	
	struct dbll_stream stream;
	struct ldr_symbol symbol;
	struct dbll_alloc allocate;
	struct dbll_init_obj init;
	void *dload_mod_obj;

	char *file_name;	
	void *fp;		
	u32 entry;		
	void *desc;	
	u32 open_ref;		
	u32 load_ref;		
	struct gh_t_hash_tab *sym_tab;	
	u32 pos;
};

struct dbll_symbol {
	struct dbll_sym_val value;
	char *name;
};

static void dof_close(struct dbll_library_obj *zl_lib);
static int dof_open(struct dbll_library_obj *zl_lib);
static s32 no_op(struct dynamic_loader_initialize *thisptr, void *bufr,
		 ldr_addr locn, struct ldr_section_info *info,
		 unsigned bytsize);

static int dbll_read_buffer(struct dynamic_loader_stream *this, void *buffer,
			    unsigned bufsize);
static int dbll_set_file_posn(struct dynamic_loader_stream *this,
			      unsigned int pos);
static struct dynload_symbol *dbll_find_symbol(struct dynamic_loader_sym *this,
					       const char *name);
static struct dynload_symbol *dbll_add_to_symbol_table(struct dynamic_loader_sym
						       *this, const char *name,
						       unsigned module_id);
static struct dynload_symbol *find_in_symbol_table(struct dynamic_loader_sym
						   *this, const char *name,
						   unsigned moduleid);
static void dbll_purge_symbol_table(struct dynamic_loader_sym *this,
				    unsigned module_id);
static void *allocate(struct dynamic_loader_sym *this, unsigned memsize);
static void deallocate(struct dynamic_loader_sym *this, void *mem_ptr);
static void dbll_err_report(struct dynamic_loader_sym *this, const char *errstr,
			    va_list args);
static int dbll_rmm_alloc(struct dynamic_loader_allocate *this,
			  struct ldr_section_info *info, unsigned align);
static void rmm_dealloc(struct dynamic_loader_allocate *this,
			struct ldr_section_info *info);

static int connect(struct dynamic_loader_initialize *this);
static int read_mem(struct dynamic_loader_initialize *this, void *buf,
		    ldr_addr addr, struct ldr_section_info *info,
		    unsigned bytes);
static int write_mem(struct dynamic_loader_initialize *this, void *buf,
		     ldr_addr addr, struct ldr_section_info *info,
		     unsigned nbytes);
static int fill_mem(struct dynamic_loader_initialize *this, ldr_addr addr,
		    struct ldr_section_info *info, unsigned bytes,
		    unsigned val);
static int execute(struct dynamic_loader_initialize *this, ldr_addr start);
static void release(struct dynamic_loader_initialize *this);

static u16 name_hash(void *key, u16 max_bucket);
static bool name_match(void *key, void *sp);
static void sym_delete(void *value);

static int redefined_symbol;
static int gbl_search = 1;

void dbll_close(struct dbll_library_obj *zl_lib)
{
	struct dbll_tar_obj *zl_target;

	zl_target = zl_lib->target_obj;
	zl_lib->open_ref--;
	if (zl_lib->open_ref == 0) {
		
		if (zl_target->head == zl_lib)
			zl_target->head = zl_lib->next;

		if (zl_lib->prev)
			(zl_lib->prev)->next = zl_lib->next;

		if (zl_lib->next)
			(zl_lib->next)->prev = zl_lib->prev;

		
		dof_close(zl_lib);
		kfree(zl_lib->file_name);

		
		if (zl_lib->sym_tab)
			gh_delete(zl_lib->sym_tab);

		
		kfree(zl_lib);
		zl_lib = NULL;
	}
}

int dbll_create(struct dbll_tar_obj **target_obj,
		       struct dbll_attrs *pattrs)
{
	struct dbll_tar_obj *pzl_target;
	int status = 0;

	
	pzl_target = kzalloc(sizeof(struct dbll_tar_obj), GFP_KERNEL);
	if (target_obj != NULL) {
		if (pzl_target == NULL) {
			*target_obj = NULL;
			status = -ENOMEM;
		} else {
			pzl_target->attrs = *pattrs;
			*target_obj = (struct dbll_tar_obj *)pzl_target;
		}
	}

	return status;
}

void dbll_delete(struct dbll_tar_obj *target)
{
	struct dbll_tar_obj *zl_target = (struct dbll_tar_obj *)target;

	kfree(zl_target);

}

void dbll_exit(void)
{
	
}

bool dbll_get_addr(struct dbll_library_obj *zl_lib, char *name,
		   struct dbll_sym_val **sym_val)
{
	struct dbll_symbol *sym;
	bool status = false;

	sym = (struct dbll_symbol *)gh_find(zl_lib->sym_tab, name);
	if (sym != NULL) {
		*sym_val = &sym->value;
		status = true;
	}

	dev_dbg(bridge, "%s: lib: %p name: %s paddr: %p, status 0x%x\n",
		__func__, zl_lib, name, sym_val, status);
	return status;
}

void dbll_get_attrs(struct dbll_tar_obj *target, struct dbll_attrs *pattrs)
{
	struct dbll_tar_obj *zl_target = (struct dbll_tar_obj *)target;

	if ((pattrs != NULL) && (zl_target != NULL))
		*pattrs = zl_target->attrs;

}

bool dbll_get_c_addr(struct dbll_library_obj *zl_lib, char *name,
		     struct dbll_sym_val **sym_val)
{
	struct dbll_symbol *sym;
	char cname[MAXEXPR + 1];
	bool status = false;

	cname[0] = '_';

	strncpy(cname + 1, name, sizeof(cname) - 2);
	cname[MAXEXPR] = '\0';	

	
	sym = (struct dbll_symbol *)gh_find(zl_lib->sym_tab, cname);

	if (sym != NULL) {
		*sym_val = &sym->value;
		status = true;
	}

	return status;
}

int dbll_get_sect(struct dbll_library_obj *lib, char *name, u32 *paddr,
			 u32 *psize)
{
	u32 byte_size;
	bool opened_doff = false;
	const struct ldr_section_info *sect = NULL;
	struct dbll_library_obj *zl_lib = (struct dbll_library_obj *)lib;
	int status = 0;

	
	if (zl_lib != NULL) {
		if (zl_lib->fp == NULL) {
			status = dof_open(zl_lib);
			if (!status)
				opened_doff = true;

		} else {
			(*(zl_lib->target_obj->attrs.fseek)) (zl_lib->fp,
							      zl_lib->pos,
							      SEEK_SET);
		}
	} else {
		status = -EFAULT;
	}
	if (!status) {
		byte_size = 1;
		if (dload_get_section_info(zl_lib->desc, name, &sect)) {
			*paddr = sect->load_addr;
			*psize = sect->size * byte_size;
			
			if (*psize % 2)
				(*psize)++;

			
			*psize = DOFF_ALIGN(*psize);
		} else {
			status = -ENXIO;
		}
	}
	if (opened_doff) {
		dof_close(zl_lib);
		opened_doff = false;
	}

	dev_dbg(bridge, "%s: lib: %p name: %s paddr: %p psize: %p, "
		"status 0x%x\n", __func__, lib, name, paddr, psize, status);

	return status;
}

bool dbll_init(void)
{
	

	return true;
}

int dbll_load(struct dbll_library_obj *lib, dbll_flags flags,
		     struct dbll_attrs *attrs, u32 *entry)
{
	struct dbll_library_obj *zl_lib = (struct dbll_library_obj *)lib;
	struct dbll_tar_obj *dbzl;
	bool got_symbols = true;
	s32 err;
	int status = 0;
	bool opened_doff = false;

	if (zl_lib->load_ref == 0 || !(flags & DBLL_DYNAMIC)) {
		dbzl = zl_lib->target_obj;
		dbzl->attrs = *attrs;
		
		if (zl_lib->sym_tab == NULL) {
			got_symbols = false;
			zl_lib->sym_tab = gh_create(MAXBUCKETS,
						    sizeof(struct dbll_symbol),
						    name_hash,
						    name_match, sym_delete);
			if (zl_lib->sym_tab == NULL)
				status = -ENOMEM;

		}
		
		zl_lib->stream.dl_stream.read_buffer = dbll_read_buffer;
		zl_lib->stream.dl_stream.set_file_posn = dbll_set_file_posn;
		zl_lib->stream.lib = zl_lib;
		
		zl_lib->symbol.dl_symbol.find_matching_symbol =
		    dbll_find_symbol;
		if (got_symbols) {
			zl_lib->symbol.dl_symbol.add_to_symbol_table =
			    find_in_symbol_table;
		} else {
			zl_lib->symbol.dl_symbol.add_to_symbol_table =
			    dbll_add_to_symbol_table;
		}
		zl_lib->symbol.dl_symbol.purge_symbol_table =
		    dbll_purge_symbol_table;
		zl_lib->symbol.dl_symbol.dload_allocate = allocate;
		zl_lib->symbol.dl_symbol.dload_deallocate = deallocate;
		zl_lib->symbol.dl_symbol.error_report = dbll_err_report;
		zl_lib->symbol.lib = zl_lib;
		
		zl_lib->allocate.dl_alloc.dload_allocate = dbll_rmm_alloc;
		zl_lib->allocate.dl_alloc.dload_deallocate = rmm_dealloc;
		zl_lib->allocate.lib = zl_lib;
		
		zl_lib->init.dl_init.connect = connect;
		zl_lib->init.dl_init.readmem = read_mem;
		zl_lib->init.dl_init.writemem = write_mem;
		zl_lib->init.dl_init.fillmem = fill_mem;
		zl_lib->init.dl_init.execute = execute;
		zl_lib->init.dl_init.release = release;
		zl_lib->init.lib = zl_lib;
		
		if (zl_lib->fp == NULL) {
			status = dof_open(zl_lib);
			if (!status)
				opened_doff = true;

		}
		if (!status) {
			zl_lib->pos = (*(zl_lib->target_obj->attrs.ftell))
			    (zl_lib->fp);
			
			(*(zl_lib->target_obj->attrs.fseek)) (zl_lib->fp,
							      (long)0,
							      SEEK_SET);
			symbols_reloaded = true;
			err = dynamic_load_module(&zl_lib->stream.dl_stream,
						  &zl_lib->symbol.dl_symbol,
						  &zl_lib->allocate.dl_alloc,
						  &zl_lib->init.dl_init,
						  DLOAD_INITBSS,
						  &zl_lib->dload_mod_obj);

			if (err != 0) {
				status = -EILSEQ;
			} else if (redefined_symbol) {
				zl_lib->load_ref++;
				dbll_unload(zl_lib, (struct dbll_attrs *)attrs);
				redefined_symbol = false;
				status = -EILSEQ;
			} else {
				*entry = zl_lib->entry;
			}
		}
	}
	if (!status)
		zl_lib->load_ref++;

	
	if (opened_doff)
		dof_close(zl_lib);

	dev_dbg(bridge, "%s: lib: %p flags: 0x%x entry: %p, status 0x%x\n",
		__func__, lib, flags, entry, status);

	return status;
}

int dbll_open(struct dbll_tar_obj *target, char *file, dbll_flags flags,
		     struct dbll_library_obj **lib_obj)
{
	struct dbll_tar_obj *zl_target = (struct dbll_tar_obj *)target;
	struct dbll_library_obj *zl_lib = NULL;
	s32 err;
	int status = 0;

	zl_lib = zl_target->head;
	while (zl_lib != NULL) {
		if (strcmp(zl_lib->file_name, file) == 0) {
			
			zl_lib->open_ref++;
			break;
		}
		zl_lib = zl_lib->next;
	}
	if (zl_lib == NULL) {
		
		zl_lib = kzalloc(sizeof(struct dbll_library_obj), GFP_KERNEL);
		if (zl_lib == NULL) {
			status = -ENOMEM;
		} else {
			zl_lib->pos = 0;
			zl_lib->open_ref++;
			zl_lib->target_obj = zl_target;
			
			zl_lib->file_name = kzalloc(strlen(file) + 1,
							GFP_KERNEL);
			if (zl_lib->file_name == NULL) {
				status = -ENOMEM;
			} else {
				strncpy(zl_lib->file_name, file,
					strlen(file) + 1);
			}
			zl_lib->sym_tab = NULL;
		}
	}
	if (status)
		goto func_cont;

	
	zl_lib->stream.dl_stream.read_buffer = dbll_read_buffer;
	zl_lib->stream.dl_stream.set_file_posn = dbll_set_file_posn;
	zl_lib->stream.lib = zl_lib;
	
	zl_lib->symbol.dl_symbol.add_to_symbol_table = dbll_add_to_symbol_table;
	zl_lib->symbol.dl_symbol.find_matching_symbol = dbll_find_symbol;
	zl_lib->symbol.dl_symbol.purge_symbol_table = dbll_purge_symbol_table;
	zl_lib->symbol.dl_symbol.dload_allocate = allocate;
	zl_lib->symbol.dl_symbol.dload_deallocate = deallocate;
	zl_lib->symbol.dl_symbol.error_report = dbll_err_report;
	zl_lib->symbol.lib = zl_lib;
	
	zl_lib->allocate.dl_alloc.dload_allocate = dbll_rmm_alloc;
	zl_lib->allocate.dl_alloc.dload_deallocate = rmm_dealloc;
	zl_lib->allocate.lib = zl_lib;
	
	zl_lib->init.dl_init.connect = connect;
	zl_lib->init.dl_init.readmem = read_mem;
	zl_lib->init.dl_init.writemem = write_mem;
	zl_lib->init.dl_init.fillmem = fill_mem;
	zl_lib->init.dl_init.execute = execute;
	zl_lib->init.dl_init.release = release;
	zl_lib->init.lib = zl_lib;
	if (!status && zl_lib->fp == NULL)
		status = dof_open(zl_lib);

	zl_lib->pos = (*(zl_lib->target_obj->attrs.ftell)) (zl_lib->fp);
	(*(zl_lib->target_obj->attrs.fseek)) (zl_lib->fp, (long)0, SEEK_SET);
	
	if (zl_lib->sym_tab != NULL || !(flags & DBLL_SYMB))
		goto func_cont;

	zl_lib->sym_tab =
	    gh_create(MAXBUCKETS, sizeof(struct dbll_symbol), name_hash,
		      name_match, sym_delete);
	if (zl_lib->sym_tab == NULL) {
		status = -ENOMEM;
	} else {
		
		zl_lib->init.dl_init.writemem = no_op;
		err = dynamic_open_module(&zl_lib->stream.dl_stream,
					  &zl_lib->symbol.dl_symbol,
					  &zl_lib->allocate.dl_alloc,
					  &zl_lib->init.dl_init, 0,
					  &zl_lib->dload_mod_obj);
		if (err != 0) {
			status = -EILSEQ;
		} else {
			
			err = dynamic_unload_module(zl_lib->dload_mod_obj,
						    &zl_lib->symbol.dl_symbol,
						    &zl_lib->allocate.dl_alloc,
						    &zl_lib->init.dl_init);
			if (err != 0)
				status = -EILSEQ;

			zl_lib->dload_mod_obj = NULL;
		}
	}
func_cont:
	if (!status) {
		if (zl_lib->open_ref == 1) {
			
			if (zl_target->head)
				(zl_target->head)->prev = zl_lib;

			zl_lib->prev = NULL;
			zl_lib->next = zl_target->head;
			zl_target->head = zl_lib;
		}
		*lib_obj = (struct dbll_library_obj *)zl_lib;
	} else {
		*lib_obj = NULL;
		if (zl_lib != NULL)
			dbll_close((struct dbll_library_obj *)zl_lib);

	}

	dev_dbg(bridge, "%s: target: %p file: %s lib_obj: %p, status 0x%x\n",
		__func__, target, file, lib_obj, status);

	return status;
}

int dbll_read_sect(struct dbll_library_obj *lib, char *name,
			  char *buf, u32 size)
{
	struct dbll_library_obj *zl_lib = (struct dbll_library_obj *)lib;
	bool opened_doff = false;
	u32 byte_size;		
	u32 ul_sect_size;	
	const struct ldr_section_info *sect = NULL;
	int status = 0;

	
	if (zl_lib != NULL) {
		if (zl_lib->fp == NULL) {
			status = dof_open(zl_lib);
			if (!status)
				opened_doff = true;

		} else {
			(*(zl_lib->target_obj->attrs.fseek)) (zl_lib->fp,
							      zl_lib->pos,
							      SEEK_SET);
		}
	} else {
		status = -EFAULT;
	}
	if (status)
		goto func_cont;

	byte_size = 1;
	if (!dload_get_section_info(zl_lib->desc, name, &sect)) {
		status = -ENXIO;
		goto func_cont;
	}
	ul_sect_size = sect->size * byte_size;
	
	if (ul_sect_size % 2)
		ul_sect_size++;

	
	ul_sect_size = DOFF_ALIGN(ul_sect_size);
	if (ul_sect_size > size) {
		status = -EPERM;
	} else {
		if (!dload_get_section(zl_lib->desc, sect, buf))
			status = -EBADF;

	}
func_cont:
	if (opened_doff) {
		dof_close(zl_lib);
		opened_doff = false;
	}

	dev_dbg(bridge, "%s: lib: %p name: %s buf: %p size: 0x%x, "
		"status 0x%x\n", __func__, lib, name, buf, size, status);
	return status;
}

void dbll_unload(struct dbll_library_obj *lib, struct dbll_attrs *attrs)
{
	struct dbll_library_obj *zl_lib = (struct dbll_library_obj *)lib;
	s32 err = 0;

	dev_dbg(bridge, "%s: lib: %p\n", __func__, lib);
	zl_lib->load_ref--;
	
	if (zl_lib->load_ref != 0)
		return;

	zl_lib->target_obj->attrs = *attrs;
	if (zl_lib->dload_mod_obj) {
		err = dynamic_unload_module(zl_lib->dload_mod_obj,
					    &zl_lib->symbol.dl_symbol,
					    &zl_lib->allocate.dl_alloc,
					    &zl_lib->init.dl_init);
		if (err != 0)
			dev_dbg(bridge, "%s: failed: 0x%x\n", __func__, err);
	}
	
	if (zl_lib->sym_tab != NULL) {
		gh_delete(zl_lib->sym_tab);
		zl_lib->sym_tab = NULL;
	}
	dof_close(zl_lib);
}

static void dof_close(struct dbll_library_obj *zl_lib)
{
	if (zl_lib->desc) {
		dload_module_close(zl_lib->desc);
		zl_lib->desc = NULL;
	}
	
	if (zl_lib->fp) {
		(zl_lib->target_obj->attrs.fclose) (zl_lib->fp);
		zl_lib->fp = NULL;
	}
}

static int dof_open(struct dbll_library_obj *zl_lib)
{
	void *open = *(zl_lib->target_obj->attrs.fopen);
	int status = 0;

	
	zl_lib->fp =
	    (void *)((dbll_f_open_fxn) (open)) (zl_lib->file_name, "rb");

	
	if (zl_lib->fp && zl_lib->desc == NULL) {
		(*(zl_lib->target_obj->attrs.fseek)) (zl_lib->fp, (long)0,
						      SEEK_SET);
		zl_lib->desc =
		    dload_module_open(&zl_lib->stream.dl_stream,
				      &zl_lib->symbol.dl_symbol);
		if (zl_lib->desc == NULL) {
			(zl_lib->target_obj->attrs.fclose) (zl_lib->fp);
			zl_lib->fp = NULL;
			status = -EBADF;
		}
	} else {
		status = -EBADF;
	}

	return status;
}

static u16 name_hash(void *key, u16 max_bucket)
{
	u16 ret;
	u16 hash;
	char *name = (char *)key;

	hash = 0;

	while (*name) {
		hash <<= 1;
		hash ^= *name++;
	}

	ret = hash % max_bucket;

	return ret;
}

static bool name_match(void *key, void *sp)
{
	if ((key != NULL) && (sp != NULL)) {
		if (strcmp((char *)key, ((struct dbll_symbol *)sp)->name) ==
		    0)
			return true;
	}
	return false;
}

static int no_op(struct dynamic_loader_initialize *thisptr, void *bufr,
		 ldr_addr locn, struct ldr_section_info *info, unsigned bytsize)
{
	return 1;
}

static void sym_delete(void *value)
{
	struct dbll_symbol *sp = (struct dbll_symbol *)value;

	kfree(sp->name);
}


static int dbll_read_buffer(struct dynamic_loader_stream *this, void *buffer,
			    unsigned bufsize)
{
	struct dbll_stream *pstream = (struct dbll_stream *)this;
	struct dbll_library_obj *lib;
	int bytes_read = 0;

	lib = pstream->lib;
	if (lib != NULL) {
		bytes_read =
		    (*(lib->target_obj->attrs.fread)) (buffer, 1, bufsize,
						       lib->fp);
	}
	return bytes_read;
}

static int dbll_set_file_posn(struct dynamic_loader_stream *this,
			      unsigned int pos)
{
	struct dbll_stream *pstream = (struct dbll_stream *)this;
	struct dbll_library_obj *lib;
	int status = 0;		

	lib = pstream->lib;
	if (lib != NULL) {
		status = (*(lib->target_obj->attrs.fseek)) (lib->fp, (long)pos,
							    SEEK_SET);
	}

	return status;
}


static struct dynload_symbol *dbll_find_symbol(struct dynamic_loader_sym *this,
					       const char *name)
{
	struct dynload_symbol *ret_sym;
	struct ldr_symbol *ldr_sym = (struct ldr_symbol *)this;
	struct dbll_library_obj *lib;
	struct dbll_sym_val *dbll_sym = NULL;
	bool status = false;	

	lib = ldr_sym->lib;
	if (lib != NULL) {
		if (lib->target_obj->attrs.sym_lookup) {
			status = (*(lib->target_obj->attrs.sym_lookup))
			    (lib->target_obj->attrs.sym_handle,
			     lib->target_obj->attrs.sym_arg,
			     lib->target_obj->attrs.rmm_handle, name,
			     &dbll_sym);
		} else {
			
			status = dbll_get_addr((struct dbll_library_obj *)lib,
					       (char *)name, &dbll_sym);
			if (!status) {
				status =
				    dbll_get_c_addr((struct dbll_library_obj *)
						    lib, (char *)name,
						    &dbll_sym);
			}
		}
	}

	if (!status && gbl_search)
		dev_dbg(bridge, "%s: Symbol not found: %s\n", __func__, name);

	ret_sym = (struct dynload_symbol *)dbll_sym;
	return ret_sym;
}

static struct dynload_symbol *find_in_symbol_table(struct dynamic_loader_sym
						   *this, const char *name,
						   unsigned moduleid)
{
	struct dynload_symbol *ret_sym;
	struct ldr_symbol *ldr_sym = (struct ldr_symbol *)this;
	struct dbll_library_obj *lib;
	struct dbll_symbol *sym;

	lib = ldr_sym->lib;
	sym = (struct dbll_symbol *)gh_find(lib->sym_tab, (char *)name);

	ret_sym = (struct dynload_symbol *)&sym->value;
	return ret_sym;
}

static struct dynload_symbol *dbll_add_to_symbol_table(struct dynamic_loader_sym
						       *this, const char *name,
						       unsigned module_id)
{
	struct dbll_symbol *sym_ptr = NULL;
	struct dbll_symbol symbol;
	struct dynload_symbol *dbll_sym = NULL;
	struct ldr_symbol *ldr_sym = (struct ldr_symbol *)this;
	struct dbll_library_obj *lib;
	struct dynload_symbol *ret;

	lib = ldr_sym->lib;

	
	if (!(lib->target_obj->attrs.base_image)) {
		gbl_search = false;
		dbll_sym = dbll_find_symbol(this, name);
		gbl_search = true;
		if (dbll_sym) {
			redefined_symbol = true;
			dev_dbg(bridge, "%s already defined in symbol table\n",
				name);
			return NULL;
		}
	}
	
	symbol.name = kzalloc(strlen((char *const)name) + 1, GFP_KERNEL);
	if (symbol.name == NULL)
		return NULL;

	if (symbol.name != NULL) {
		
		strncpy(symbol.name, (char *const)name,
			strlen((char *const)name) + 1);

		
		sym_ptr =
		    (struct dbll_symbol *)gh_insert(lib->sym_tab, (void *)name,
						    (void *)&symbol);
		if (sym_ptr == NULL)
			kfree(symbol.name);

	}
	if (sym_ptr != NULL)
		ret = (struct dynload_symbol *)&sym_ptr->value;
	else
		ret = NULL;

	return ret;
}

static void dbll_purge_symbol_table(struct dynamic_loader_sym *this,
				    unsigned module_id)
{
	struct ldr_symbol *ldr_sym = (struct ldr_symbol *)this;
	struct dbll_library_obj *lib;

	lib = ldr_sym->lib;
	
}

static void *allocate(struct dynamic_loader_sym *this, unsigned memsize)
{
	struct ldr_symbol *ldr_sym = (struct ldr_symbol *)this;
	struct dbll_library_obj *lib;
	void *buf;

	lib = ldr_sym->lib;

	buf = kzalloc(memsize, GFP_KERNEL);

	return buf;
}

static void deallocate(struct dynamic_loader_sym *this, void *mem_ptr)
{
	struct ldr_symbol *ldr_sym = (struct ldr_symbol *)this;
	struct dbll_library_obj *lib;

	lib = ldr_sym->lib;

	kfree(mem_ptr);
}

static void dbll_err_report(struct dynamic_loader_sym *this, const char *errstr,
			    va_list args)
{
	struct ldr_symbol *ldr_sym = (struct ldr_symbol *)this;
	struct dbll_library_obj *lib;
	char temp_buf[MAXEXPR];

	lib = ldr_sym->lib;
	vsnprintf((char *)temp_buf, MAXEXPR, (char *)errstr, args);
	dev_dbg(bridge, "%s\n", temp_buf);
}


static int dbll_rmm_alloc(struct dynamic_loader_allocate *this,
			  struct ldr_section_info *info, unsigned align)
{
	struct dbll_alloc *dbll_alloc_obj = (struct dbll_alloc *)this;
	struct dbll_library_obj *lib;
	int status = 0;
	u32 mem_sect_type;
	struct rmm_addr rmm_addr_obj;
	s32 ret = true;
	unsigned stype = DLOAD_SECTION_TYPE(info->type);
	char *token = NULL;
	char *sz_sec_last_token = NULL;
	char *sz_last_token = NULL;
	char *sz_sect_name = NULL;
	char *psz_cur;
	s32 token_len = 0;
	s32 seg_id = -1;
	s32 req = -1;
	s32 count = 0;
	u32 alloc_size = 0;
	u32 run_addr_flag = 0;

	lib = dbll_alloc_obj->lib;

	mem_sect_type =
	    (stype == DLOAD_TEXT) ? DBLL_CODE : (stype ==
						 DLOAD_BSS) ? DBLL_BSS :
	    DBLL_DATA;

	token_len = strlen((char *)(info->name)) + 1;

	sz_sect_name = kzalloc(token_len, GFP_KERNEL);
	sz_last_token = kzalloc(token_len, GFP_KERNEL);
	sz_sec_last_token = kzalloc(token_len, GFP_KERNEL);

	if (sz_sect_name == NULL || sz_sec_last_token == NULL ||
	    sz_last_token == NULL) {
		status = -ENOMEM;
		goto func_cont;
	}
	strncpy(sz_sect_name, (char *)(info->name), token_len);
	psz_cur = sz_sect_name;
	while ((token = strsep(&psz_cur, ":")) && *token != '\0') {
		strncpy(sz_sec_last_token, sz_last_token,
			strlen(sz_last_token) + 1);
		strncpy(sz_last_token, token, strlen(token) + 1);
		token = strsep(&psz_cur, ":");
		count++;	
	}
	if (count >= 3)
		strict_strtol(sz_last_token, 10, (long *)&req);

	if ((req == 0) || (req == 1)) {
		if (strcmp(sz_sec_last_token, "DYN_DARAM") == 0) {
			seg_id = 0;
		} else {
			if (strcmp(sz_sec_last_token, "DYN_SARAM") == 0) {
				seg_id = 1;
			} else {
				if (strcmp(sz_sec_last_token,
					   "DYN_EXTERNAL") == 0)
					seg_id = 2;
			}
		}
	}
func_cont:
	kfree(sz_sect_name);
	sz_sect_name = NULL;
	kfree(sz_last_token);
	sz_last_token = NULL;
	kfree(sz_sec_last_token);
	sz_sec_last_token = NULL;

	if (mem_sect_type == DBLL_CODE)
		alloc_size = info->size + GEM_L1P_PREFETCH_SIZE;
	else
		alloc_size = info->size;

	if (info->load_addr != info->run_addr)
		run_addr_flag = 1;
	if (lib != NULL) {
		status =
		    (lib->target_obj->attrs.alloc) (lib->target_obj->attrs.
						    rmm_handle, mem_sect_type,
						    alloc_size, align,
						    (u32 *) &rmm_addr_obj,
						    seg_id, req, false);
	}
	if (status) {
		ret = false;
	} else {
		
		info->load_addr = rmm_addr_obj.addr * DSPWORDSIZE;
		if (!run_addr_flag)
			info->run_addr = info->load_addr;
		info->context = (u32) rmm_addr_obj.segid;
		dev_dbg(bridge, "%s: %s base = 0x%x len = 0x%x, "
			"info->run_addr 0x%x, info->load_addr 0x%x\n",
			__func__, info->name, info->load_addr / DSPWORDSIZE,
			info->size / DSPWORDSIZE, info->run_addr,
			info->load_addr);
	}
	return ret;
}

static void rmm_dealloc(struct dynamic_loader_allocate *this,
			struct ldr_section_info *info)
{
	struct dbll_alloc *dbll_alloc_obj = (struct dbll_alloc *)this;
	struct dbll_library_obj *lib;
	u32 segid;
	int status = 0;
	unsigned stype = DLOAD_SECTION_TYPE(info->type);
	u32 mem_sect_type;
	u32 free_size = 0;

	mem_sect_type =
	    (stype == DLOAD_TEXT) ? DBLL_CODE : (stype ==
						 DLOAD_BSS) ? DBLL_BSS :
	    DBLL_DATA;
	lib = dbll_alloc_obj->lib;
	
	segid = (u32) info->context;
	if (mem_sect_type == DBLL_CODE)
		free_size = info->size + GEM_L1P_PREFETCH_SIZE;
	else
		free_size = info->size;
	if (lib != NULL) {
		status =
		    (lib->target_obj->attrs.free) (lib->target_obj->attrs.
						   sym_handle, segid,
						   info->load_addr /
						   DSPWORDSIZE, free_size,
						   false);
	}
}

static int connect(struct dynamic_loader_initialize *this)
{
	return true;
}

static int read_mem(struct dynamic_loader_initialize *this, void *buf,
		    ldr_addr addr, struct ldr_section_info *info,
		    unsigned nbytes)
{
	struct dbll_init_obj *init_obj = (struct dbll_init_obj *)this;
	struct dbll_library_obj *lib;
	int bytes_read = 0;

	lib = init_obj->lib;
	
	return bytes_read;
}

static int write_mem(struct dynamic_loader_initialize *this, void *buf,
		     ldr_addr addr, struct ldr_section_info *info,
		     unsigned bytes)
{
	struct dbll_init_obj *init_obj = (struct dbll_init_obj *)this;
	struct dbll_library_obj *lib;
	struct dbll_tar_obj *target_obj;
	struct dbll_sect_info sect_info;
	u32 mem_sect_type;
	bool ret = true;

	lib = init_obj->lib;
	if (!lib)
		return false;

	target_obj = lib->target_obj;

	mem_sect_type =
	    (DLOAD_SECTION_TYPE(info->type) ==
	     DLOAD_TEXT) ? DBLL_CODE : DBLL_DATA;
	if (target_obj && target_obj->attrs.write) {
		ret =
		    (*target_obj->attrs.write) (target_obj->attrs.input_params,
						addr, buf, bytes,
						mem_sect_type);

		if (target_obj->attrs.log_write) {
			sect_info.name = info->name;
			sect_info.sect_run_addr = info->run_addr;
			sect_info.sect_load_addr = info->load_addr;
			sect_info.size = info->size;
			sect_info.type = mem_sect_type;
			/* Pass the information about what we've written to
			 * another module */
			(*target_obj->attrs.log_write) (target_obj->attrs.
							log_write_handle,
							&sect_info, addr,
							bytes);
		}
	}
	return ret;
}

static int fill_mem(struct dynamic_loader_initialize *this, ldr_addr addr,
		    struct ldr_section_info *info, unsigned bytes, unsigned val)
{
	bool ret = true;
	char *pbuf;
	struct dbll_library_obj *lib;
	struct dbll_init_obj *init_obj = (struct dbll_init_obj *)this;

	lib = init_obj->lib;
	pbuf = NULL;
	if ((lib->target_obj->attrs.write) != (dbll_write_fxn) no_op)
		write_mem(this, &pbuf, addr, info, 0);
	if (pbuf)
		memset(pbuf, val, bytes);

	return ret;
}

static int execute(struct dynamic_loader_initialize *this, ldr_addr start)
{
	struct dbll_init_obj *init_obj = (struct dbll_init_obj *)this;
	struct dbll_library_obj *lib;
	bool ret = true;

	lib = init_obj->lib;
	
	if (lib != NULL)
		lib->entry = (u32) start;

	return ret;
}

static void release(struct dynamic_loader_initialize *this)
{
}

#ifdef CONFIG_TIDSPBRIDGE_BACKTRACE
struct find_symbol_context {
	
	u32 address;
	u32 offset_range;
	
	u32 cur_best_offset;
	
	u32 sym_addr;
	char name[120];
};

void find_symbol_callback(void *elem, void *user_data)
{
	struct dbll_symbol *symbol = elem;
	struct find_symbol_context *context = user_data;
	u32 symbol_addr = symbol->value.value;
	u32 offset = context->address - symbol_addr;

	if (context->address >= symbol_addr && symbol_addr < (u32)-1 &&
		offset < context->cur_best_offset) {
		context->cur_best_offset = offset;
		context->sym_addr = symbol_addr;
		strncpy(context->name, symbol->name, sizeof(context->name));
	}

	return;
}

bool dbll_find_dsp_symbol(struct dbll_library_obj *zl_lib, u32 address,
				u32 offset_range, u32 *sym_addr_output,
				char *name_output)
{
	bool status = false;
	struct find_symbol_context context;

	context.address = address;
	context.offset_range = offset_range;
	context.cur_best_offset = offset_range;
	context.sym_addr = 0;
	context.name[0] = '\0';

	gh_iterate(zl_lib->sym_tab, find_symbol_callback, &context);

	if (context.name[0]) {
		status = true;
		strcpy(name_output, context.name);
		*sym_addr_output = context.sym_addr;
	}

	return status;
}
#endif
