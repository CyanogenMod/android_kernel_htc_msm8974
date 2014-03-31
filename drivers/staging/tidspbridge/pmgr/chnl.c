/*
 * chnl.c
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * DSP API channel interface: multiplexes data streams through the single
 * physical link managed by a Bridge Bridge driver.
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

#include <dspbridge/sync.h>

#include <dspbridge/proc.h>
#include <dspbridge/dev.h>

#include <dspbridge/chnlpriv.h>
#include <chnlobj.h>

#include <dspbridge/chnl.h>

int chnl_create(struct chnl_mgr **channel_mgr,
		       struct dev_object *hdev_obj,
		       const struct chnl_mgrattrs *mgr_attrts)
{
	int status;
	struct chnl_mgr *hchnl_mgr;
	struct chnl_mgr_ *chnl_mgr_obj = NULL;

	*channel_mgr = NULL;

	
	if ((0 < mgr_attrts->max_channels) &&
	    (mgr_attrts->max_channels <= CHNL_MAXCHANNELS))
		status = 0;
	else if (mgr_attrts->max_channels == 0)
		status = -EINVAL;
	else
		status = -ECHRNG;

	if (mgr_attrts->word_size == 0)
		status = -EINVAL;

	if (!status) {
		status = dev_get_chnl_mgr(hdev_obj, &hchnl_mgr);
		if (!status && hchnl_mgr != NULL)
			status = -EEXIST;

	}

	if (!status) {
		struct bridge_drv_interface *intf_fxns;
		dev_get_intf_fxns(hdev_obj, &intf_fxns);
		
		status = (*intf_fxns->chnl_create) (&hchnl_mgr, hdev_obj,
							mgr_attrts);
		if (!status) {
			chnl_mgr_obj = (struct chnl_mgr_ *)hchnl_mgr;
			chnl_mgr_obj->intf_fxns = intf_fxns;
			
			*channel_mgr = hchnl_mgr;
		}
	}

	return status;
}

int chnl_destroy(struct chnl_mgr *hchnl_mgr)
{
	struct chnl_mgr_ *chnl_mgr_obj = (struct chnl_mgr_ *)hchnl_mgr;
	struct bridge_drv_interface *intf_fxns;
	int status;

	if (chnl_mgr_obj) {
		intf_fxns = chnl_mgr_obj->intf_fxns;
		
		status = (*intf_fxns->chnl_destroy) (hchnl_mgr);
	} else {
		status = -EFAULT;
	}

	return status;
}
