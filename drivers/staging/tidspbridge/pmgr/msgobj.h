/*
 * msgobj.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Structure subcomponents of channel class library msg_ctrl objects which
 * are exposed to DSP API from Bridge driver.
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

#ifndef MSGOBJ_
#define MSGOBJ_

#include <dspbridge/dspdefs.h>

#include <dspbridge/msgdefs.h>

struct msg_mgr_ {
	

	
	struct bridge_drv_interface *intf_fxns;
};

#endif 
