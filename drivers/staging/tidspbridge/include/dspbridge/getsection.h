/*
 * getsection.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * This file provides an API add-on to the dynamic loader that allows the user
 * to query section information and extract section data from dynamic load
 * modules.
 *
 * Notes:
 *   Functions in this API assume that the supplied dynamic_loader_stream
 *   object supports the set_file_posn method.
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

#ifndef _GETSECTION_H_
#define _GETSECTION_H_

#include "dynamic_loader.h"

extern void *dload_module_open(struct dynamic_loader_stream
					   *module, struct dynamic_loader_sym
					   *syms);

extern int dload_get_section_info(void *minfo,
				  const char *section_name,
				  const struct ldr_section_info
				  **const section_info);

extern int dload_get_section(void *minfo,
			     const struct ldr_section_info *section_info,
			     void *section_data);

extern void dload_module_close(void *minfo);

#endif 
