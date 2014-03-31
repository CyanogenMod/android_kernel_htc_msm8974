/*
 * chnl.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * DSP API channel interface: multiplexes data streams through the single
 * physical link managed by a Bridge driver.
 *
 * See DSP API chnl.h for more details.
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

#ifndef CHNL_
#define CHNL_

#include <dspbridge/chnlpriv.h>

extern int chnl_create(struct chnl_mgr **channel_mgr,
			      struct dev_object *hdev_obj,
			      const struct chnl_mgrattrs *mgr_attrts);

extern int chnl_destroy(struct chnl_mgr *hchnl_mgr);

#endif 
