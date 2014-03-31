/*
 * msg.c
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * DSP/BIOS Bridge msg_ctrl Module.
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

#include <dspbridge/dspdefs.h>

#include <dspbridge/dev.h>

#include <msgobj.h>
#include <dspbridge/msg.h>

int msg_create(struct msg_mgr **msg_man,
		      struct dev_object *hdev_obj, msg_onexit msg_callback)
{
	struct bridge_drv_interface *intf_fxns;
	struct msg_mgr_ *msg_mgr_obj;
	struct msg_mgr *hmsg_mgr;
	int status = 0;

	*msg_man = NULL;

	dev_get_intf_fxns(hdev_obj, &intf_fxns);

	
	status =
	    (*intf_fxns->msg_create) (&hmsg_mgr, hdev_obj, msg_callback);

	if (!status) {
		msg_mgr_obj = (struct msg_mgr_ *)hmsg_mgr;
		msg_mgr_obj->intf_fxns = intf_fxns;

		
		*msg_man = hmsg_mgr;
	} else {
		status = -EPERM;
	}
	return status;
}

void msg_delete(struct msg_mgr *hmsg_mgr)
{
	struct msg_mgr_ *msg_mgr_obj = (struct msg_mgr_ *)hmsg_mgr;
	struct bridge_drv_interface *intf_fxns;

	if (msg_mgr_obj) {
		intf_fxns = msg_mgr_obj->intf_fxns;

		
		(*intf_fxns->msg_delete) (hmsg_mgr);
	} else {
		dev_dbg(bridge, "%s: Error hmsg_mgr handle: %p\n",
			__func__, hmsg_mgr);
	}
}
