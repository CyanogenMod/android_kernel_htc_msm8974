/*
 * Freescale hypervisor ioctl and kernel interface
 *
 * Copyright (C) 2008-2011 Freescale Semiconductor, Inc.
 * Author: Timur Tabi <timur@freescale.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Freescale Semiconductor nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * This software is provided by Freescale Semiconductor "as is" and any
 * express or implied warranties, including, but not limited to, the implied
 * warranties of merchantability and fitness for a particular purpose are
 * disclaimed. In no event shall Freescale Semiconductor be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential damages
 * (including, but not limited to, procurement of substitute goods or services;
 * loss of use, data, or profits; or business interruption) however caused and
 * on any theory of liability, whether in contract, strict liability, or tort
 * (including negligence or otherwise) arising in any way out of the use of this
 * software, even if advised of the possibility of such damage.
 *
 * This file is used by the Freescale hypervisor management driver.  It can
 * also be included by applications that need to communicate with the driver
 * via the ioctl interface.
 */

#ifndef FSL_HYPERVISOR_H
#define FSL_HYPERVISOR_H

#include <linux/types.h>

struct fsl_hv_ioctl_restart {
	__u32 ret;
	__u32 partition;
};

struct fsl_hv_ioctl_status {
	__u32 ret;
	__u32 partition;
	__u32 status;
};

struct fsl_hv_ioctl_start {
	__u32 ret;
	__u32 partition;
	__u32 entry_point;
	__u32 load;
};

struct fsl_hv_ioctl_stop {
	__u32 ret;
	__u32 partition;
};

struct fsl_hv_ioctl_memcpy {
	__u32 ret;
	__u32 source;
	__u32 target;
	__u32 reserved;	
	__u64 local_vaddr;
	__u64 remote_paddr;
	__u64 count;
};

struct fsl_hv_ioctl_doorbell {
	__u32 ret;
	__u32 doorbell;
};

struct fsl_hv_ioctl_prop {
	__u32 ret;
	__u32 handle;
	__u64 path;
	__u64 propname;
	__u64 propval;
	__u32 proplen;
	__u32 reserved;	
};

#define FSL_HV_IOCTL_TYPE	0xAF

#define FSL_HV_IOCTL_PARTITION_RESTART \
	_IOWR(FSL_HV_IOCTL_TYPE, 1, struct fsl_hv_ioctl_restart)

#define FSL_HV_IOCTL_PARTITION_GET_STATUS \
	_IOWR(FSL_HV_IOCTL_TYPE, 2, struct fsl_hv_ioctl_status)

#define FSL_HV_IOCTL_PARTITION_START \
	_IOWR(FSL_HV_IOCTL_TYPE, 3, struct fsl_hv_ioctl_start)

#define FSL_HV_IOCTL_PARTITION_STOP \
	_IOWR(FSL_HV_IOCTL_TYPE, 4, struct fsl_hv_ioctl_stop)

#define FSL_HV_IOCTL_MEMCPY \
	_IOWR(FSL_HV_IOCTL_TYPE, 5, struct fsl_hv_ioctl_memcpy)

#define FSL_HV_IOCTL_DOORBELL \
	_IOWR(FSL_HV_IOCTL_TYPE, 6, struct fsl_hv_ioctl_doorbell)

#define FSL_HV_IOCTL_GETPROP \
	_IOWR(FSL_HV_IOCTL_TYPE, 7, struct fsl_hv_ioctl_prop)

#define FSL_HV_IOCTL_SETPROP \
	_IOWR(FSL_HV_IOCTL_TYPE, 8, struct fsl_hv_ioctl_prop)

#ifdef __KERNEL__

int fsl_hv_failover_register(struct notifier_block *nb);

int fsl_hv_failover_unregister(struct notifier_block *nb);

#endif

#endif
