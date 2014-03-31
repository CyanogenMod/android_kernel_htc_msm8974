/*
 * dynamic_loader.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
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

#ifndef _DYNAMIC_LOADER_H_
#define _DYNAMIC_LOADER_H_
#include <linux/kernel.h>
#include <linux/types.h>

	
struct dynamic_loader_stream;

	
struct dynamic_loader_sym;

	
struct dynamic_loader_allocate;

	
struct dynamic_loader_initialize;

#define DLOAD_INITBSS 0x1	

extern int dynamic_load_module(
				      
				      struct dynamic_loader_stream *module,
				      
				      struct dynamic_loader_sym *syms,
				      
				      struct dynamic_loader_allocate *alloc,
				      
				      struct dynamic_loader_initialize *init,
				      unsigned options,	
				      
				      void **mhandle);

extern int dynamic_open_module(
				      
				      struct dynamic_loader_stream *module,
				      
				      struct dynamic_loader_sym *syms,
				      
				      struct dynamic_loader_allocate *alloc,
				      
				      struct dynamic_loader_initialize *init,
				      unsigned options,	
				      
				      void **mhandle);

extern int dynamic_unload_module(void *mhandle,	
				 struct dynamic_loader_sym *syms,
				 
				 struct dynamic_loader_allocate *alloc,
				 
				 struct dynamic_loader_initialize *init);

struct dynamic_loader_stream {
	int (*read_buffer) (struct dynamic_loader_stream *thisptr,
			    void *buffer, unsigned bufsiz);

	int (*set_file_posn) (struct dynamic_loader_stream *thisptr,
			      
			      unsigned int posn);

};


typedef u32 ldr_addr;

struct dynload_symbol {
	ldr_addr value;
};

struct dynamic_loader_sym {
	struct dynload_symbol *(*find_matching_symbol)
	 (struct dynamic_loader_sym *thisptr, const char *name);

	struct dynload_symbol *(*add_to_symbol_table)
	 (struct dynamic_loader_sym *
	  thisptr, const char *nname, unsigned moduleid);

	void (*purge_symbol_table) (struct dynamic_loader_sym *thisptr,
				    unsigned moduleid);

	void *(*dload_allocate) (struct dynamic_loader_sym *thisptr,
				 unsigned memsiz);

	void (*dload_deallocate) (struct dynamic_loader_sym *thisptr,
				  void *memptr);

	void (*error_report) (struct dynamic_loader_sym *thisptr,
			      const char *errstr, va_list args);

};				


struct ldr_section_info {
	
	const char *name;
	ldr_addr run_addr;	
	ldr_addr load_addr;	
	ldr_addr size;		
#ifndef _BIG_ENDIAN
	u16 page;		
	u16 type;		
#else
	u16 type;		
	u16 page;		
#endif
	u32 context;
};

#define DLOAD_SECTION_TYPE(typeinfo) (typeinfo & 0xF)

#define DLOAD_TEXT 0
#define DLOAD_DATA 1
#define DLOAD_BSS 2
	
#define DLOAD_CINIT 3

struct dynamic_loader_allocate {

	int (*dload_allocate) (struct dynamic_loader_allocate *thisptr,
			       struct ldr_section_info *info, unsigned align);

	void (*dload_deallocate) (struct dynamic_loader_allocate *thisptr,
				  struct ldr_section_info *info);

};				


struct dynamic_loader_initialize {
	int (*connect) (struct dynamic_loader_initialize *thisptr);

	int (*readmem) (struct dynamic_loader_initialize *thisptr,
			void *bufr,
			ldr_addr locn,
			struct ldr_section_info *info, unsigned bytsiz);

    /*************************************************************************
    * Function writemem
    *
    * Parameters:
    *   bufr        Pointer to a word-aligned buffer of data
    *   locn        Target address of first data element to be written
    *   info        Section info for the section in which the address resides
    *   bytsiz      Size of the data to be written in sizeof() units
    *
    * Effect:
    *   Writes the specified buffer to the target.  Returns TRUE for success,
    * FALSE for failure.
    ************************************************************************ */
	int (*writemem) (struct dynamic_loader_initialize *thisptr,
			 void *bufr,
			 ldr_addr locn,
			 struct ldr_section_info *info, unsigned bytsiz);

    /*************************************************************************
    * Function fillmem
    *
    * Parameters:
    *   locn        Target address of first data element to be written
    *   info        Section info for the section in which the address resides
    *   bytsiz      Size of the data to be written in sizeof() units
    *   val         Value to be written in each byte
    * Effect:
    *   Fills the specified area of target memory.  Returns TRUE for success,
    * FALSE for failure.
    ************************************************************************ */
	int (*fillmem) (struct dynamic_loader_initialize *thisptr,
			ldr_addr locn, struct ldr_section_info *info,
			unsigned bytsiz, unsigned val);

	int (*execute) (struct dynamic_loader_initialize *thisptr,
			ldr_addr start);

	void (*release) (struct dynamic_loader_initialize *thisptr);

};				

#endif 
