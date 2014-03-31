/*
 * Linux 2.6.32 and later Kernel module for VMware MVP Guest Communications
 *
 * Copyright (C) 2010-2013 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; see the file COPYING.  If not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#line 5

#ifndef _MKSCK_H
#define _MKSCK_H



#define INCLUDE_ALLOW_MVPD
#define INCLUDE_ALLOW_VMX
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_MONITOR
#define INCLUDE_ALLOW_HOSTUSER
#define INCLUDE_ALLOW_GUESTUSER
#define INCLUDE_ALLOW_GPL
#include "include_check.h"

#include "vmid.h"

#define MKSCK_XFER_MAX        1024

#define MKSCK_ADDR_UNDEF      ((uint32)0xffffffff)

#define MKSCK_PORT_UNDEF            ((uint16)0xffff)
#define MKSCK_PORT_MASTER           (MKSCK_PORT_UNDEF-1)
#define MKSCK_PORT_HOST_FB          (MKSCK_PORT_UNDEF-2)
#define MKSCK_PORT_BALLOON          (MKSCK_PORT_UNDEF-3)
#define MKSCK_PORT_HOST_HID         (MKSCK_PORT_UNDEF-4)
#define MKSCK_PORT_CHECKPOINT       (MKSCK_PORT_UNDEF-5)
#define MKSCK_PORT_COMM_EV          (MKSCK_PORT_UNDEF-6)
#define MKSCK_PORT_HIGH             (MKSCK_PORT_UNDEF-7)

#define MKSCK_VMID_UNDEF      VMID_UNDEF
#define MKSCK_VMID_HIGH       (MKSCK_VMID_UNDEF-1)

#define MKSCK_DETACH          3

typedef uint16 Mksck_Port;
typedef VmId   Mksck_VmId;

typedef struct {
	uint32 mpn:20;   
	uint32 order:12; 
} Mksck_PageDesc;

#define MKSCK_DESC_TYPE(type, pages) \
	struct { \
		type umsg; \
		Mksck_PageDesc page[pages]; \
	}

typedef union {
	uint32 addr;		 
	struct {		 
		Mksck_Port port; 
		Mksck_VmId vmId; 
	};
} Mksck_Address;

static inline uint32
Mksck_AddrInit(Mksck_VmId vmId,
	       Mksck_Port port)
{
	Mksck_Address aa;

	aa.vmId = vmId;
	aa.port = port;
	return aa.addr;
}
#endif
