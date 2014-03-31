/*
 *	Adaptec AAC series RAID controller driver
 *	(c) Copyright 2001 Red Hat Inc.
 *
 * based on the old aacraid driver that is..
 * Adaptec aacraid device driver for Linux.
 *
 * Copyright (c) 2000-2010 Adaptec, Inc.
 *               2010 PMC-Sierra, Inc. (aacraid@pmc-sierra.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Module Name:
 *  rkt.c
 *
 * Abstract: Hardware miniport for Drawbridge specific hardware functions.
 *
 */

#include <linux/blkdev.h>

#include <scsi/scsi_host.h>

#include "aacraid.h"

#define AAC_NUM_IO_FIB_RKT      (246 - AAC_NUM_MGT_FIB)


static int aac_rkt_select_comm(struct aac_dev *dev, int comm)
{
	int retval;
	retval = aac_rx_select_comm(dev, comm);
	if (comm == AAC_COMM_MESSAGE) {
		if (dev->scsi_host_ptr->can_queue > AAC_NUM_IO_FIB_RKT) {
			dev->init->MaxIoCommands =
				cpu_to_le32(AAC_NUM_IO_FIB_RKT + AAC_NUM_MGT_FIB);
			dev->scsi_host_ptr->can_queue = AAC_NUM_IO_FIB_RKT;
		}
	}
	return retval;
}

static int aac_rkt_ioremap(struct aac_dev * dev, u32 size)
{
	if (!size) {
		iounmap(dev->regs.rkt);
		return 0;
	}
	dev->base = dev->regs.rkt = ioremap(dev->scsi_host_ptr->base, size);
	if (dev->base == NULL)
		return -1;
	dev->IndexRegs = &dev->regs.rkt->IndexRegs;
	return 0;
}


int aac_rkt_init(struct aac_dev *dev)
{
	dev->a_ops.adapter_ioremap = aac_rkt_ioremap;
	dev->a_ops.adapter_comm = aac_rkt_select_comm;

	return _aac_rx_init(dev);
}
