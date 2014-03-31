/*
 * io.c
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * IO manager interface: Manages IO between CHNL and msg_ctrl.
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

#include <dspbridge/dev.h>

#include <ioobj.h>
#include <dspbridge/io.h>

int io_create(struct io_mgr **io_man, struct dev_object *hdev_obj,
		     const struct io_attrs *mgr_attrts)
{
	struct bridge_drv_interface *intf_fxns;
	struct io_mgr *hio_mgr = NULL;
	struct io_mgr_ *pio_mgr = NULL;
	int status = 0;

	*io_man = NULL;

	
	if ((mgr_attrts->shm_base != 0) && (mgr_attrts->sm_length == 0))
		status = -EINVAL;

	if (mgr_attrts->word_size == 0)
		status = -EINVAL;

	if (!status) {
		dev_get_intf_fxns(hdev_obj, &intf_fxns);

		
		status = (*intf_fxns->io_create) (&hio_mgr, hdev_obj,
						      mgr_attrts);

		if (!status) {
			pio_mgr = (struct io_mgr_ *)hio_mgr;
			pio_mgr->intf_fxns = intf_fxns;
			pio_mgr->dev_obj = hdev_obj;

			
			*io_man = hio_mgr;
		}
	}

	return status;
}

int io_destroy(struct io_mgr *hio_mgr)
{
	struct bridge_drv_interface *intf_fxns;
	struct io_mgr_ *pio_mgr = (struct io_mgr_ *)hio_mgr;
	int status;

	intf_fxns = pio_mgr->intf_fxns;

	
	status = (*intf_fxns->io_destroy) (hio_mgr);

	return status;
}
