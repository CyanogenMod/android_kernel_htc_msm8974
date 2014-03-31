/*
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2008 - 2011 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.GPL.
 *
 * BSD LICENSE
 *
 * Copyright(c) 2008 - 2011 Intel Corporation. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _ISCI_REMOTE_DEVICE_H_
#define _ISCI_REMOTE_DEVICE_H_
#include <scsi/libsas.h>
#include <linux/kref.h>
#include "scu_remote_node_context.h"
#include "remote_node_context.h"
#include "port.h"

enum sci_remote_device_not_ready_reason_code {
	SCIC_REMOTE_DEVICE_NOT_READY_START_REQUESTED,
	SCIC_REMOTE_DEVICE_NOT_READY_STOP_REQUESTED,
	SCIC_REMOTE_DEVICE_NOT_READY_SATA_REQUEST_STARTED,
	SCIC_REMOTE_DEVICE_NOT_READY_SATA_SDB_ERROR_FIS_RECEIVED,
	SCIC_REMOTE_DEVICE_NOT_READY_SMP_REQUEST_STARTED,
	SCIC_REMOTE_DEVICE_NOT_READY_REASON_CODE_MAX
};

struct isci_remote_device {
	#define IDEV_START_PENDING 0
	#define IDEV_STOP_PENDING 1
	#define IDEV_ALLOCATED 2
	#define IDEV_GONE 3
	#define IDEV_IO_READY 4
	#define IDEV_IO_NCQERROR 5
	unsigned long flags;
	struct kref kref;
	struct isci_port *isci_port;
	struct domain_device *domain_dev;
	struct list_head node;
	struct list_head reqs_in_process;
	struct sci_base_state_machine sm;
	u32 device_port_width;
	enum sas_linkrate connection_rate;
	bool is_direct_attached;
	struct isci_port *owning_port;
	struct sci_remote_node_context rnc;
	
	u32 started_request_count;
	struct isci_request *working_request;
	u32 not_ready_reason;
};

#define ISCI_REMOTE_DEVICE_START_TIMEOUT 5000

static inline struct isci_remote_device *isci_lookup_device(struct domain_device *dev)
{
	struct isci_remote_device *idev = dev->lldd_dev;

	if (idev && !test_bit(IDEV_GONE, &idev->flags)) {
		kref_get(&idev->kref);
		return idev;
	}

	return NULL;
}

void isci_remote_device_release(struct kref *kref);
static inline void isci_put_device(struct isci_remote_device *idev)
{
	if (idev)
		kref_put(&idev->kref, isci_remote_device_release);
}

enum sci_status isci_remote_device_stop(struct isci_host *ihost,
					struct isci_remote_device *idev);
void isci_remote_device_nuke_requests(struct isci_host *ihost,
				      struct isci_remote_device *idev);
void isci_remote_device_gone(struct domain_device *domain_dev);
int isci_remote_device_found(struct domain_device *domain_dev);

enum sci_status sci_remote_device_stop(
	struct isci_remote_device *idev,
	u32 timeout);

enum sci_status sci_remote_device_reset(
	struct isci_remote_device *idev);

enum sci_status sci_remote_device_reset_complete(
	struct isci_remote_device *idev);

#define REMOTE_DEV_STATES {\
	C(DEV_INITIAL),\
	C(DEV_STOPPED),\
	C(DEV_STARTING),\
	C(DEV_READY),\
	C(STP_DEV_IDLE),\
	C(STP_DEV_CMD),\
	C(STP_DEV_NCQ),\
	C(STP_DEV_NCQ_ERROR),\
	C(STP_DEV_ATAPI_ERROR),\
	C(STP_DEV_AWAIT_RESET),\
	C(SMP_DEV_IDLE),\
	C(SMP_DEV_CMD),\
	C(DEV_STOPPING),\
	C(DEV_FAILED),\
	C(DEV_RESETTING),\
	C(DEV_FINAL),\
	}
#undef C
#define C(a) SCI_##a
enum sci_remote_device_states REMOTE_DEV_STATES;
#undef C
const char *dev_state_name(enum sci_remote_device_states state);

static inline struct isci_remote_device *rnc_to_dev(struct sci_remote_node_context *rnc)
{
	struct isci_remote_device *idev;

	idev = container_of(rnc, typeof(*idev), rnc);

	return idev;
}

static inline bool dev_is_expander(struct domain_device *dev)
{
	return dev->dev_type == EDGE_DEV || dev->dev_type == FANOUT_DEV;
}

static inline void sci_remote_device_decrement_request_count(struct isci_remote_device *idev)
{
	if (WARN_ONCE(idev->started_request_count == 0,
		      "%s: tried to decrement started_request_count past 0!?",
			__func__))
		;
	else
		idev->started_request_count--;
}

enum sci_status sci_remote_device_frame_handler(
	struct isci_remote_device *idev,
	u32 frame_index);

enum sci_status sci_remote_device_event_handler(
	struct isci_remote_device *idev,
	u32 event_code);

enum sci_status sci_remote_device_start_io(
	struct isci_host *ihost,
	struct isci_remote_device *idev,
	struct isci_request *ireq);

enum sci_status sci_remote_device_start_task(
	struct isci_host *ihost,
	struct isci_remote_device *idev,
	struct isci_request *ireq);

enum sci_status sci_remote_device_complete_io(
	struct isci_host *ihost,
	struct isci_remote_device *idev,
	struct isci_request *ireq);

enum sci_status sci_remote_device_suspend(
	struct isci_remote_device *idev,
	u32 suspend_type);

void sci_remote_device_post_request(
	struct isci_remote_device *idev,
	u32 request);

#endif 
