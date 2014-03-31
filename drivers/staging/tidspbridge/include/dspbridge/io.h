/*
 * io.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * The io module manages IO between CHNL and msg_ctrl.
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

#ifndef IO_
#define IO_

#include <dspbridge/cfgdefs.h>
#include <dspbridge/devdefs.h>

struct io_mgr;

struct io_attrs {
	u8 birq;		
	bool irq_shared;	
	u32 word_size;		
	u32 shm_base;		
	u32 sm_length;		
};


extern int io_create(struct io_mgr **io_man,
			    struct dev_object *hdev_obj,
			    const struct io_attrs *mgr_attrts);

extern int io_destroy(struct io_mgr *hio_mgr);

#endif 
