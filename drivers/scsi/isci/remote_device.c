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
#include <scsi/sas.h>
#include <linux/bitops.h>
#include "isci.h"
#include "port.h"
#include "remote_device.h"
#include "request.h"
#include "remote_node_context.h"
#include "scu_event_codes.h"
#include "task.h"

#undef C
#define C(a) (#a)
const char *dev_state_name(enum sci_remote_device_states state)
{
	static const char * const strings[] = REMOTE_DEV_STATES;

	return strings[state];
}
#undef C

static void isci_remote_device_not_ready(struct isci_host *ihost,
				  struct isci_remote_device *idev, u32 reason)
{
	struct isci_request *ireq;

	dev_dbg(&ihost->pdev->dev,
		"%s: isci_device = %p\n", __func__, idev);

	switch (reason) {
	case SCIC_REMOTE_DEVICE_NOT_READY_STOP_REQUESTED:
		set_bit(IDEV_GONE, &idev->flags);
		break;
	case SCIC_REMOTE_DEVICE_NOT_READY_SATA_SDB_ERROR_FIS_RECEIVED:
		set_bit(IDEV_IO_NCQERROR, &idev->flags);

		
		list_for_each_entry(ireq, &idev->reqs_in_process, dev_node) {

			dev_dbg(&ihost->pdev->dev,
				"%s: isci_device = %p request = %p\n",
				__func__, idev, ireq);

			sci_controller_terminate_request(ihost,
							  idev,
							  ireq);
		}
		
	default:
		clear_bit(IDEV_IO_READY, &idev->flags);
		break;
	}
}

static void isci_remote_device_ready(struct isci_host *ihost, struct isci_remote_device *idev)
{
	dev_dbg(&ihost->pdev->dev,
		"%s: idev = %p\n", __func__, idev);

	clear_bit(IDEV_IO_NCQERROR, &idev->flags);
	set_bit(IDEV_IO_READY, &idev->flags);
	if (test_and_clear_bit(IDEV_START_PENDING, &idev->flags))
		wake_up(&ihost->eventq);
}

static void rnc_destruct_done(void *_dev)
{
	struct isci_remote_device *idev = _dev;

	BUG_ON(idev->started_request_count != 0);
	sci_change_state(&idev->sm, SCI_DEV_STOPPED);
}

static enum sci_status sci_remote_device_terminate_requests(struct isci_remote_device *idev)
{
	struct isci_host *ihost = idev->owning_port->owning_controller;
	enum sci_status status  = SCI_SUCCESS;
	u32 i;

	for (i = 0; i < SCI_MAX_IO_REQUESTS; i++) {
		struct isci_request *ireq = ihost->reqs[i];
		enum sci_status s;

		if (!test_bit(IREQ_ACTIVE, &ireq->flags) ||
		    ireq->target_device != idev)
			continue;

		s = sci_controller_terminate_request(ihost, idev, ireq);
		if (s != SCI_SUCCESS)
			status = s;
	}

	return status;
}

enum sci_status sci_remote_device_stop(struct isci_remote_device *idev,
					u32 timeout)
{
	struct sci_base_state_machine *sm = &idev->sm;
	enum sci_remote_device_states state = sm->current_state_id;

	switch (state) {
	case SCI_DEV_INITIAL:
	case SCI_DEV_FAILED:
	case SCI_DEV_FINAL:
	default:
		dev_warn(scirdev_to_dev(idev), "%s: in wrong state: %s\n",
			 __func__, dev_state_name(state));
		return SCI_FAILURE_INVALID_STATE;
	case SCI_DEV_STOPPED:
		return SCI_SUCCESS;
	case SCI_DEV_STARTING:
		
		BUG_ON(idev->started_request_count != 0);
		sci_remote_node_context_destruct(&idev->rnc,
						      rnc_destruct_done, idev);
		sci_change_state(sm, SCI_DEV_STOPPING);
		return SCI_SUCCESS;
	case SCI_DEV_READY:
	case SCI_STP_DEV_IDLE:
	case SCI_STP_DEV_CMD:
	case SCI_STP_DEV_NCQ:
	case SCI_STP_DEV_NCQ_ERROR:
	case SCI_STP_DEV_AWAIT_RESET:
	case SCI_SMP_DEV_IDLE:
	case SCI_SMP_DEV_CMD:
		sci_change_state(sm, SCI_DEV_STOPPING);
		if (idev->started_request_count == 0) {
			sci_remote_node_context_destruct(&idev->rnc,
							      rnc_destruct_done, idev);
			return SCI_SUCCESS;
		} else
			return sci_remote_device_terminate_requests(idev);
		break;
	case SCI_DEV_STOPPING:
		return sci_remote_device_terminate_requests(idev);
	case SCI_DEV_RESETTING:
		sci_change_state(sm, SCI_DEV_STOPPING);
		return SCI_SUCCESS;
	}
}

enum sci_status sci_remote_device_reset(struct isci_remote_device *idev)
{
	struct sci_base_state_machine *sm = &idev->sm;
	enum sci_remote_device_states state = sm->current_state_id;

	switch (state) {
	case SCI_DEV_INITIAL:
	case SCI_DEV_STOPPED:
	case SCI_DEV_STARTING:
	case SCI_SMP_DEV_IDLE:
	case SCI_SMP_DEV_CMD:
	case SCI_DEV_STOPPING:
	case SCI_DEV_FAILED:
	case SCI_DEV_RESETTING:
	case SCI_DEV_FINAL:
	default:
		dev_warn(scirdev_to_dev(idev), "%s: in wrong state: %s\n",
			 __func__, dev_state_name(state));
		return SCI_FAILURE_INVALID_STATE;
	case SCI_DEV_READY:
	case SCI_STP_DEV_IDLE:
	case SCI_STP_DEV_CMD:
	case SCI_STP_DEV_NCQ:
	case SCI_STP_DEV_NCQ_ERROR:
	case SCI_STP_DEV_AWAIT_RESET:
		sci_change_state(sm, SCI_DEV_RESETTING);
		return SCI_SUCCESS;
	}
}

enum sci_status sci_remote_device_reset_complete(struct isci_remote_device *idev)
{
	struct sci_base_state_machine *sm = &idev->sm;
	enum sci_remote_device_states state = sm->current_state_id;

	if (state != SCI_DEV_RESETTING) {
		dev_warn(scirdev_to_dev(idev), "%s: in wrong state: %s\n",
			 __func__, dev_state_name(state));
		return SCI_FAILURE_INVALID_STATE;
	}

	sci_change_state(sm, SCI_DEV_READY);
	return SCI_SUCCESS;
}

enum sci_status sci_remote_device_suspend(struct isci_remote_device *idev,
					       u32 suspend_type)
{
	struct sci_base_state_machine *sm = &idev->sm;
	enum sci_remote_device_states state = sm->current_state_id;

	if (state != SCI_STP_DEV_CMD) {
		dev_warn(scirdev_to_dev(idev), "%s: in wrong state: %s\n",
			 __func__, dev_state_name(state));
		return SCI_FAILURE_INVALID_STATE;
	}

	return sci_remote_node_context_suspend(&idev->rnc,
						    suspend_type, NULL, NULL);
}

enum sci_status sci_remote_device_frame_handler(struct isci_remote_device *idev,
						     u32 frame_index)
{
	struct sci_base_state_machine *sm = &idev->sm;
	enum sci_remote_device_states state = sm->current_state_id;
	struct isci_host *ihost = idev->owning_port->owning_controller;
	enum sci_status status;

	switch (state) {
	case SCI_DEV_INITIAL:
	case SCI_DEV_STOPPED:
	case SCI_DEV_STARTING:
	case SCI_STP_DEV_IDLE:
	case SCI_SMP_DEV_IDLE:
	case SCI_DEV_FINAL:
	default:
		dev_warn(scirdev_to_dev(idev), "%s: in wrong state: %s\n",
			 __func__, dev_state_name(state));
		
		sci_controller_release_frame(ihost, frame_index);
		return SCI_FAILURE_INVALID_STATE;
	case SCI_DEV_READY:
	case SCI_STP_DEV_NCQ_ERROR:
	case SCI_STP_DEV_AWAIT_RESET:
	case SCI_DEV_STOPPING:
	case SCI_DEV_FAILED:
	case SCI_DEV_RESETTING: {
		struct isci_request *ireq;
		struct ssp_frame_hdr hdr;
		void *frame_header;
		ssize_t word_cnt;

		status = sci_unsolicited_frame_control_get_header(&ihost->uf_control,
								       frame_index,
								       &frame_header);
		if (status != SCI_SUCCESS)
			return status;

		word_cnt = sizeof(hdr) / sizeof(u32);
		sci_swab32_cpy(&hdr, frame_header, word_cnt);

		ireq = sci_request_by_tag(ihost, be16_to_cpu(hdr.tag));
		if (ireq && ireq->target_device == idev) {
			
			status = sci_io_request_frame_handler(ireq, frame_index);
		} else {
			sci_controller_release_frame(ihost, frame_index);
		}
		break;
	}
	case SCI_STP_DEV_NCQ: {
		struct dev_to_host_fis *hdr;

		status = sci_unsolicited_frame_control_get_header(&ihost->uf_control,
								       frame_index,
								       (void **)&hdr);
		if (status != SCI_SUCCESS)
			return status;

		if (hdr->fis_type == FIS_SETDEVBITS &&
		    (hdr->status & ATA_ERR)) {
			idev->not_ready_reason = SCIC_REMOTE_DEVICE_NOT_READY_SATA_SDB_ERROR_FIS_RECEIVED;

			
			sci_change_state(sm, SCI_STP_DEV_NCQ_ERROR);
		} else if (hdr->fis_type == FIS_REGD2H &&
			   (hdr->status & ATA_ERR)) {
			idev->not_ready_reason = SCIC_REMOTE_DEVICE_NOT_READY_SATA_SDB_ERROR_FIS_RECEIVED;
			sci_change_state(&idev->sm, SCI_STP_DEV_NCQ_ERROR);
		} else
			status = SCI_FAILURE;

		sci_controller_release_frame(ihost, frame_index);
		break;
	}
	case SCI_STP_DEV_CMD:
	case SCI_SMP_DEV_CMD:
		status = sci_io_request_frame_handler(idev->working_request, frame_index);
		break;
	}

	return status;
}

static bool is_remote_device_ready(struct isci_remote_device *idev)
{

	struct sci_base_state_machine *sm = &idev->sm;
	enum sci_remote_device_states state = sm->current_state_id;

	switch (state) {
	case SCI_DEV_READY:
	case SCI_STP_DEV_IDLE:
	case SCI_STP_DEV_CMD:
	case SCI_STP_DEV_NCQ:
	case SCI_STP_DEV_NCQ_ERROR:
	case SCI_STP_DEV_AWAIT_RESET:
	case SCI_SMP_DEV_IDLE:
	case SCI_SMP_DEV_CMD:
		return true;
	default:
		return false;
	}
}

static void atapi_remote_device_resume_done(void *_dev)
{
	struct isci_remote_device *idev = _dev;
	struct isci_request *ireq = idev->working_request;

	sci_change_state(&ireq->sm, SCI_REQ_COMPLETED);
}

enum sci_status sci_remote_device_event_handler(struct isci_remote_device *idev,
						     u32 event_code)
{
	struct sci_base_state_machine *sm = &idev->sm;
	enum sci_remote_device_states state = sm->current_state_id;
	enum sci_status status;

	switch (scu_get_event_type(event_code)) {
	case SCU_EVENT_TYPE_RNC_OPS_MISC:
	case SCU_EVENT_TYPE_RNC_SUSPEND_TX:
	case SCU_EVENT_TYPE_RNC_SUSPEND_TX_RX:
		status = sci_remote_node_context_event_handler(&idev->rnc, event_code);
		break;
	case SCU_EVENT_TYPE_PTX_SCHEDULE_EVENT:
		if (scu_get_event_code(event_code) == SCU_EVENT_IT_NEXUS_TIMEOUT) {
			status = SCI_SUCCESS;

			
			sci_remote_node_context_suspend(&idev->rnc,
							      SCI_SOFTWARE_SUSPENSION,
							      NULL, NULL);

			dev_dbg(scirdev_to_dev(idev),
				"%s: device: %p event code: %x: %s\n",
				__func__, idev, event_code,
				is_remote_device_ready(idev)
				? "I_T_Nexus_Timeout event"
				: "I_T_Nexus_Timeout event in wrong state");

			break;
		}
	
	default:
		dev_dbg(scirdev_to_dev(idev),
			"%s: device: %p event code: %x: %s\n",
			__func__, idev, event_code,
			is_remote_device_ready(idev)
			? "unexpected event"
			: "unexpected event in wrong state");
		status = SCI_FAILURE_INVALID_STATE;
		break;
	}

	if (status != SCI_SUCCESS)
		return status;

	if (state == SCI_STP_DEV_ATAPI_ERROR) {
		
		if (scu_get_event_type(event_code) == SCU_EVENT_TYPE_RNC_SUSPEND_TX ||
		    scu_get_event_type(event_code) == SCU_EVENT_TYPE_RNC_SUSPEND_TX_RX) {
			return sci_remote_node_context_resume(&idev->rnc,
							      atapi_remote_device_resume_done,
							      idev);
		}
	}

	if (state == SCI_STP_DEV_IDLE) {

		if (scu_get_event_type(event_code) == SCU_EVENT_TYPE_RNC_SUSPEND_TX ||
		    scu_get_event_type(event_code) == SCU_EVENT_TYPE_RNC_SUSPEND_TX_RX)
			status = sci_remote_node_context_resume(&idev->rnc, NULL, NULL);
	}

	return status;
}

static void sci_remote_device_start_request(struct isci_remote_device *idev,
						 struct isci_request *ireq,
						 enum sci_status status)
{
	struct isci_port *iport = idev->owning_port;

	
	if (status != SCI_SUCCESS)
		sci_port_complete_io(iport, idev, ireq);
	else {
		kref_get(&idev->kref);
		idev->started_request_count++;
	}
}

enum sci_status sci_remote_device_start_io(struct isci_host *ihost,
						struct isci_remote_device *idev,
						struct isci_request *ireq)
{
	struct sci_base_state_machine *sm = &idev->sm;
	enum sci_remote_device_states state = sm->current_state_id;
	struct isci_port *iport = idev->owning_port;
	enum sci_status status;

	switch (state) {
	case SCI_DEV_INITIAL:
	case SCI_DEV_STOPPED:
	case SCI_DEV_STARTING:
	case SCI_STP_DEV_NCQ_ERROR:
	case SCI_DEV_STOPPING:
	case SCI_DEV_FAILED:
	case SCI_DEV_RESETTING:
	case SCI_DEV_FINAL:
	default:
		dev_warn(scirdev_to_dev(idev), "%s: in wrong state: %s\n",
			 __func__, dev_state_name(state));
		return SCI_FAILURE_INVALID_STATE;
	case SCI_DEV_READY:
		status = sci_port_start_io(iport, idev, ireq);
		if (status != SCI_SUCCESS)
			return status;

		status = sci_remote_node_context_start_io(&idev->rnc, ireq);
		if (status != SCI_SUCCESS)
			break;

		status = sci_request_start(ireq);
		break;
	case SCI_STP_DEV_IDLE: {
		enum sci_remote_device_states new_state;
		struct sas_task *task = isci_request_access_task(ireq);

		status = sci_port_start_io(iport, idev, ireq);
		if (status != SCI_SUCCESS)
			return status;

		status = sci_remote_node_context_start_io(&idev->rnc, ireq);
		if (status != SCI_SUCCESS)
			break;

		status = sci_request_start(ireq);
		if (status != SCI_SUCCESS)
			break;

		if (task->ata_task.use_ncq)
			new_state = SCI_STP_DEV_NCQ;
		else {
			idev->working_request = ireq;
			new_state = SCI_STP_DEV_CMD;
		}
		sci_change_state(sm, new_state);
		break;
	}
	case SCI_STP_DEV_NCQ: {
		struct sas_task *task = isci_request_access_task(ireq);

		if (task->ata_task.use_ncq) {
			status = sci_port_start_io(iport, idev, ireq);
			if (status != SCI_SUCCESS)
				return status;

			status = sci_remote_node_context_start_io(&idev->rnc, ireq);
			if (status != SCI_SUCCESS)
				break;

			status = sci_request_start(ireq);
		} else
			return SCI_FAILURE_INVALID_STATE;
		break;
	}
	case SCI_STP_DEV_AWAIT_RESET:
		return SCI_FAILURE_REMOTE_DEVICE_RESET_REQUIRED;
	case SCI_SMP_DEV_IDLE:
		status = sci_port_start_io(iport, idev, ireq);
		if (status != SCI_SUCCESS)
			return status;

		status = sci_remote_node_context_start_io(&idev->rnc, ireq);
		if (status != SCI_SUCCESS)
			break;

		status = sci_request_start(ireq);
		if (status != SCI_SUCCESS)
			break;

		idev->working_request = ireq;
		sci_change_state(&idev->sm, SCI_SMP_DEV_CMD);
		break;
	case SCI_STP_DEV_CMD:
	case SCI_SMP_DEV_CMD:
		return SCI_FAILURE_INVALID_STATE;
	}

	sci_remote_device_start_request(idev, ireq, status);
	return status;
}

static enum sci_status common_complete_io(struct isci_port *iport,
					  struct isci_remote_device *idev,
					  struct isci_request *ireq)
{
	enum sci_status status;

	status = sci_request_complete(ireq);
	if (status != SCI_SUCCESS)
		return status;

	status = sci_port_complete_io(iport, idev, ireq);
	if (status != SCI_SUCCESS)
		return status;

	sci_remote_device_decrement_request_count(idev);
	return status;
}

enum sci_status sci_remote_device_complete_io(struct isci_host *ihost,
						   struct isci_remote_device *idev,
						   struct isci_request *ireq)
{
	struct sci_base_state_machine *sm = &idev->sm;
	enum sci_remote_device_states state = sm->current_state_id;
	struct isci_port *iport = idev->owning_port;
	enum sci_status status;

	switch (state) {
	case SCI_DEV_INITIAL:
	case SCI_DEV_STOPPED:
	case SCI_DEV_STARTING:
	case SCI_STP_DEV_IDLE:
	case SCI_SMP_DEV_IDLE:
	case SCI_DEV_FAILED:
	case SCI_DEV_FINAL:
	default:
		dev_warn(scirdev_to_dev(idev), "%s: in wrong state: %s\n",
			 __func__, dev_state_name(state));
		return SCI_FAILURE_INVALID_STATE;
	case SCI_DEV_READY:
	case SCI_STP_DEV_AWAIT_RESET:
	case SCI_DEV_RESETTING:
		status = common_complete_io(iport, idev, ireq);
		break;
	case SCI_STP_DEV_CMD:
	case SCI_STP_DEV_NCQ:
	case SCI_STP_DEV_NCQ_ERROR:
	case SCI_STP_DEV_ATAPI_ERROR:
		status = common_complete_io(iport, idev, ireq);
		if (status != SCI_SUCCESS)
			break;

		if (ireq->sci_status == SCI_FAILURE_REMOTE_DEVICE_RESET_REQUIRED) {
			sci_change_state(sm, SCI_STP_DEV_AWAIT_RESET);
		} else if (idev->started_request_count == 0)
			sci_change_state(sm, SCI_STP_DEV_IDLE);
		break;
	case SCI_SMP_DEV_CMD:
		status = common_complete_io(iport, idev, ireq);
		if (status != SCI_SUCCESS)
			break;
		sci_change_state(sm, SCI_SMP_DEV_IDLE);
		break;
	case SCI_DEV_STOPPING:
		status = common_complete_io(iport, idev, ireq);
		if (status != SCI_SUCCESS)
			break;

		if (idev->started_request_count == 0)
			sci_remote_node_context_destruct(&idev->rnc,
							 rnc_destruct_done,
							 idev);
		break;
	}

	if (status != SCI_SUCCESS)
		dev_err(scirdev_to_dev(idev),
			"%s: Port:0x%p Device:0x%p Request:0x%p Status:0x%x "
			"could not complete\n", __func__, iport,
			idev, ireq, status);
	else
		isci_put_device(idev);

	return status;
}

static void sci_remote_device_continue_request(void *dev)
{
	struct isci_remote_device *idev = dev;

	
	if (idev->working_request)
		sci_controller_continue_io(idev->working_request);
}

enum sci_status sci_remote_device_start_task(struct isci_host *ihost,
						  struct isci_remote_device *idev,
						  struct isci_request *ireq)
{
	struct sci_base_state_machine *sm = &idev->sm;
	enum sci_remote_device_states state = sm->current_state_id;
	struct isci_port *iport = idev->owning_port;
	enum sci_status status;

	switch (state) {
	case SCI_DEV_INITIAL:
	case SCI_DEV_STOPPED:
	case SCI_DEV_STARTING:
	case SCI_SMP_DEV_IDLE:
	case SCI_SMP_DEV_CMD:
	case SCI_DEV_STOPPING:
	case SCI_DEV_FAILED:
	case SCI_DEV_RESETTING:
	case SCI_DEV_FINAL:
	default:
		dev_warn(scirdev_to_dev(idev), "%s: in wrong state: %s\n",
			 __func__, dev_state_name(state));
		return SCI_FAILURE_INVALID_STATE;
	case SCI_STP_DEV_IDLE:
	case SCI_STP_DEV_CMD:
	case SCI_STP_DEV_NCQ:
	case SCI_STP_DEV_NCQ_ERROR:
	case SCI_STP_DEV_AWAIT_RESET:
		status = sci_port_start_io(iport, idev, ireq);
		if (status != SCI_SUCCESS)
			return status;

		status = sci_remote_node_context_start_task(&idev->rnc, ireq);
		if (status != SCI_SUCCESS)
			goto out;

		status = sci_request_start(ireq);
		if (status != SCI_SUCCESS)
			goto out;

		idev->working_request = ireq;
		sci_change_state(sm, SCI_STP_DEV_CMD);

		sci_remote_node_context_suspend(&idev->rnc,
				SCI_SOFTWARE_SUSPENSION, NULL, NULL);
		sci_remote_node_context_resume(&idev->rnc,
				sci_remote_device_continue_request,
						    idev);

	out:
		sci_remote_device_start_request(idev, ireq, status);
		return SCI_FAILURE_RESET_DEVICE_PARTIAL_SUCCESS;
	case SCI_DEV_READY:
		status = sci_port_start_io(iport, idev, ireq);
		if (status != SCI_SUCCESS)
			return status;

		status = sci_remote_node_context_start_task(&idev->rnc, ireq);
		if (status != SCI_SUCCESS)
			break;

		status = sci_request_start(ireq);
		break;
	}
	sci_remote_device_start_request(idev, ireq, status);

	return status;
}

void sci_remote_device_post_request(struct isci_remote_device *idev, u32 request)
{
	struct isci_port *iport = idev->owning_port;
	u32 context;

	context = request |
		  (ISCI_PEG << SCU_CONTEXT_COMMAND_PROTOCOL_ENGINE_GROUP_SHIFT) |
		  (iport->physical_port_index << SCU_CONTEXT_COMMAND_LOGICAL_PORT_SHIFT) |
		  idev->rnc.remote_node_index;

	sci_controller_post_request(iport->owning_controller, context);
}

static void remote_device_resume_done(void *_dev)
{
	struct isci_remote_device *idev = _dev;

	if (is_remote_device_ready(idev))
		return;

	
	sci_change_state(&idev->sm, SCI_DEV_READY);
}

static void sci_stp_remote_device_ready_idle_substate_resume_complete_handler(void *_dev)
{
	struct isci_remote_device *idev = _dev;
	struct isci_host *ihost = idev->owning_port->owning_controller;

	if (idev->sm.previous_state_id != SCI_STP_DEV_NCQ)
		isci_remote_device_ready(ihost, idev);
}

static void sci_remote_device_initial_state_enter(struct sci_base_state_machine *sm)
{
	struct isci_remote_device *idev = container_of(sm, typeof(*idev), sm);

	
	sci_change_state(&idev->sm, SCI_DEV_STOPPED);
}

static enum sci_status sci_remote_device_destruct(struct isci_remote_device *idev)
{
	struct sci_base_state_machine *sm = &idev->sm;
	enum sci_remote_device_states state = sm->current_state_id;
	struct isci_host *ihost;

	if (state != SCI_DEV_STOPPED) {
		dev_warn(scirdev_to_dev(idev), "%s: in wrong state: %s\n",
			 __func__, dev_state_name(state));
		return SCI_FAILURE_INVALID_STATE;
	}

	ihost = idev->owning_port->owning_controller;
	sci_controller_free_remote_node_context(ihost, idev,
						     idev->rnc.remote_node_index);
	idev->rnc.remote_node_index = SCIC_SDS_REMOTE_NODE_CONTEXT_INVALID_INDEX;
	sci_change_state(sm, SCI_DEV_FINAL);

	return SCI_SUCCESS;
}

static void isci_remote_device_deconstruct(struct isci_host *ihost, struct isci_remote_device *idev)
{
	dev_dbg(&ihost->pdev->dev,
		"%s: isci_device = %p\n", __func__, idev);

	BUG_ON(!list_empty(&idev->reqs_in_process));

	sci_remote_device_destruct(idev);
	list_del_init(&idev->node);
	isci_put_device(idev);
}

static void sci_remote_device_stopped_state_enter(struct sci_base_state_machine *sm)
{
	struct isci_remote_device *idev = container_of(sm, typeof(*idev), sm);
	struct isci_host *ihost = idev->owning_port->owning_controller;
	u32 prev_state;

	prev_state = idev->sm.previous_state_id;
	if (prev_state == SCI_DEV_STOPPING)
		isci_remote_device_deconstruct(ihost, idev);

	sci_controller_remote_device_stopped(ihost, idev);
}

static void sci_remote_device_starting_state_enter(struct sci_base_state_machine *sm)
{
	struct isci_remote_device *idev = container_of(sm, typeof(*idev), sm);
	struct isci_host *ihost = idev->owning_port->owning_controller;

	isci_remote_device_not_ready(ihost, idev,
				     SCIC_REMOTE_DEVICE_NOT_READY_START_REQUESTED);
}

static void sci_remote_device_ready_state_enter(struct sci_base_state_machine *sm)
{
	struct isci_remote_device *idev = container_of(sm, typeof(*idev), sm);
	struct isci_host *ihost = idev->owning_port->owning_controller;
	struct domain_device *dev = idev->domain_dev;

	if (dev->dev_type == SATA_DEV || (dev->tproto & SAS_PROTOCOL_SATA)) {
		sci_change_state(&idev->sm, SCI_STP_DEV_IDLE);
	} else if (dev_is_expander(dev)) {
		sci_change_state(&idev->sm, SCI_SMP_DEV_IDLE);
	} else
		isci_remote_device_ready(ihost, idev);
}

static void sci_remote_device_ready_state_exit(struct sci_base_state_machine *sm)
{
	struct isci_remote_device *idev = container_of(sm, typeof(*idev), sm);
	struct domain_device *dev = idev->domain_dev;

	if (dev->dev_type == SAS_END_DEV) {
		struct isci_host *ihost = idev->owning_port->owning_controller;

		isci_remote_device_not_ready(ihost, idev,
					     SCIC_REMOTE_DEVICE_NOT_READY_STOP_REQUESTED);
	}
}

static void sci_remote_device_resetting_state_enter(struct sci_base_state_machine *sm)
{
	struct isci_remote_device *idev = container_of(sm, typeof(*idev), sm);

	sci_remote_node_context_suspend(
		&idev->rnc, SCI_SOFTWARE_SUSPENSION, NULL, NULL);
}

static void sci_remote_device_resetting_state_exit(struct sci_base_state_machine *sm)
{
	struct isci_remote_device *idev = container_of(sm, typeof(*idev), sm);

	sci_remote_node_context_resume(&idev->rnc, NULL, NULL);
}

static void sci_stp_remote_device_ready_idle_substate_enter(struct sci_base_state_machine *sm)
{
	struct isci_remote_device *idev = container_of(sm, typeof(*idev), sm);

	idev->working_request = NULL;
	if (sci_remote_node_context_is_ready(&idev->rnc)) {
		sci_stp_remote_device_ready_idle_substate_resume_complete_handler(idev);
	} else {
		sci_remote_node_context_resume(&idev->rnc,
			sci_stp_remote_device_ready_idle_substate_resume_complete_handler,
			idev);
	}
}

static void sci_stp_remote_device_ready_cmd_substate_enter(struct sci_base_state_machine *sm)
{
	struct isci_remote_device *idev = container_of(sm, typeof(*idev), sm);
	struct isci_host *ihost = idev->owning_port->owning_controller;

	BUG_ON(idev->working_request == NULL);

	isci_remote_device_not_ready(ihost, idev,
				     SCIC_REMOTE_DEVICE_NOT_READY_SATA_REQUEST_STARTED);
}

static void sci_stp_remote_device_ready_ncq_error_substate_enter(struct sci_base_state_machine *sm)
{
	struct isci_remote_device *idev = container_of(sm, typeof(*idev), sm);
	struct isci_host *ihost = idev->owning_port->owning_controller;

	if (idev->not_ready_reason == SCIC_REMOTE_DEVICE_NOT_READY_SATA_SDB_ERROR_FIS_RECEIVED)
		isci_remote_device_not_ready(ihost, idev,
					     idev->not_ready_reason);
}

static void sci_smp_remote_device_ready_idle_substate_enter(struct sci_base_state_machine *sm)
{
	struct isci_remote_device *idev = container_of(sm, typeof(*idev), sm);
	struct isci_host *ihost = idev->owning_port->owning_controller;

	isci_remote_device_ready(ihost, idev);
}

static void sci_smp_remote_device_ready_cmd_substate_enter(struct sci_base_state_machine *sm)
{
	struct isci_remote_device *idev = container_of(sm, typeof(*idev), sm);
	struct isci_host *ihost = idev->owning_port->owning_controller;

	BUG_ON(idev->working_request == NULL);

	isci_remote_device_not_ready(ihost, idev,
				     SCIC_REMOTE_DEVICE_NOT_READY_SMP_REQUEST_STARTED);
}

static void sci_smp_remote_device_ready_cmd_substate_exit(struct sci_base_state_machine *sm)
{
	struct isci_remote_device *idev = container_of(sm, typeof(*idev), sm);

	idev->working_request = NULL;
}

static const struct sci_base_state sci_remote_device_state_table[] = {
	[SCI_DEV_INITIAL] = {
		.enter_state = sci_remote_device_initial_state_enter,
	},
	[SCI_DEV_STOPPED] = {
		.enter_state = sci_remote_device_stopped_state_enter,
	},
	[SCI_DEV_STARTING] = {
		.enter_state = sci_remote_device_starting_state_enter,
	},
	[SCI_DEV_READY] = {
		.enter_state = sci_remote_device_ready_state_enter,
		.exit_state  = sci_remote_device_ready_state_exit
	},
	[SCI_STP_DEV_IDLE] = {
		.enter_state = sci_stp_remote_device_ready_idle_substate_enter,
	},
	[SCI_STP_DEV_CMD] = {
		.enter_state = sci_stp_remote_device_ready_cmd_substate_enter,
	},
	[SCI_STP_DEV_NCQ] = { },
	[SCI_STP_DEV_NCQ_ERROR] = {
		.enter_state = sci_stp_remote_device_ready_ncq_error_substate_enter,
	},
	[SCI_STP_DEV_ATAPI_ERROR] = { },
	[SCI_STP_DEV_AWAIT_RESET] = { },
	[SCI_SMP_DEV_IDLE] = {
		.enter_state = sci_smp_remote_device_ready_idle_substate_enter,
	},
	[SCI_SMP_DEV_CMD] = {
		.enter_state = sci_smp_remote_device_ready_cmd_substate_enter,
		.exit_state  = sci_smp_remote_device_ready_cmd_substate_exit,
	},
	[SCI_DEV_STOPPING] = { },
	[SCI_DEV_FAILED] = { },
	[SCI_DEV_RESETTING] = {
		.enter_state = sci_remote_device_resetting_state_enter,
		.exit_state  = sci_remote_device_resetting_state_exit
	},
	[SCI_DEV_FINAL] = { },
};

static void sci_remote_device_construct(struct isci_port *iport,
				  struct isci_remote_device *idev)
{
	idev->owning_port = iport;
	idev->started_request_count = 0;

	sci_init_sm(&idev->sm, sci_remote_device_state_table, SCI_DEV_INITIAL);

	sci_remote_node_context_construct(&idev->rnc,
					       SCIC_SDS_REMOTE_NODE_CONTEXT_INVALID_INDEX);
}

static enum sci_status sci_remote_device_da_construct(struct isci_port *iport,
						       struct isci_remote_device *idev)
{
	enum sci_status status;
	struct sci_port_properties properties;
	struct domain_device *dev = idev->domain_dev;

	sci_remote_device_construct(iport, idev);

	idev->is_direct_attached = true;

	sci_port_get_properties(iport, &properties);
	
	idev->device_port_width = hweight32(properties.phy_mask);

	status = sci_controller_allocate_remote_node_context(iport->owning_controller,
								  idev,
								  &idev->rnc.remote_node_index);

	if (status != SCI_SUCCESS)
		return status;

	if (dev->dev_type == SAS_END_DEV || dev->dev_type == SATA_DEV ||
	    (dev->tproto & SAS_PROTOCOL_STP) || dev_is_expander(dev))
		;
	else
		return SCI_FAILURE_UNSUPPORTED_PROTOCOL;

	idev->connection_rate = sci_port_get_max_allowed_speed(iport);

	return SCI_SUCCESS;
}

static enum sci_status sci_remote_device_ea_construct(struct isci_port *iport,
						       struct isci_remote_device *idev)
{
	struct domain_device *dev = idev->domain_dev;
	enum sci_status status;

	sci_remote_device_construct(iport, idev);

	status = sci_controller_allocate_remote_node_context(iport->owning_controller,
								  idev,
								  &idev->rnc.remote_node_index);
	if (status != SCI_SUCCESS)
		return status;

	if (dev->dev_type == SAS_END_DEV || dev->dev_type == SATA_DEV ||
	    (dev->tproto & SAS_PROTOCOL_STP) || dev_is_expander(dev))
		;
	else
		return SCI_FAILURE_UNSUPPORTED_PROTOCOL;

	idev->connection_rate = min_t(u16, sci_port_get_max_allowed_speed(iport),
					 dev->linkrate);

	
	idev->device_port_width = 1;

	return SCI_SUCCESS;
}

static enum sci_status sci_remote_device_start(struct isci_remote_device *idev,
						u32 timeout)
{
	struct sci_base_state_machine *sm = &idev->sm;
	enum sci_remote_device_states state = sm->current_state_id;
	enum sci_status status;

	if (state != SCI_DEV_STOPPED) {
		dev_warn(scirdev_to_dev(idev), "%s: in wrong state: %s\n",
			 __func__, dev_state_name(state));
		return SCI_FAILURE_INVALID_STATE;
	}

	status = sci_remote_node_context_resume(&idev->rnc,
						     remote_device_resume_done,
						     idev);
	if (status != SCI_SUCCESS)
		return status;

	sci_change_state(sm, SCI_DEV_STARTING);

	return SCI_SUCCESS;
}

static enum sci_status isci_remote_device_construct(struct isci_port *iport,
						    struct isci_remote_device *idev)
{
	struct isci_host *ihost = iport->isci_host;
	struct domain_device *dev = idev->domain_dev;
	enum sci_status status;

	if (dev->parent && dev_is_expander(dev->parent))
		status = sci_remote_device_ea_construct(iport, idev);
	else
		status = sci_remote_device_da_construct(iport, idev);

	if (status != SCI_SUCCESS) {
		dev_dbg(&ihost->pdev->dev, "%s: construct failed: %d\n",
			__func__, status);

		return status;
	}

	
	status = sci_remote_device_start(idev, ISCI_REMOTE_DEVICE_START_TIMEOUT);

	if (status != SCI_SUCCESS)
		dev_warn(&ihost->pdev->dev, "remote device start failed: %d\n",
			 status);

	return status;
}

void isci_remote_device_nuke_requests(struct isci_host *ihost, struct isci_remote_device *idev)
{
	DECLARE_COMPLETION_ONSTACK(aborted_task_completion);

	dev_dbg(&ihost->pdev->dev,
		"%s: idev = %p\n", __func__, idev);

	
	isci_terminate_pending_requests(ihost, idev);

	dev_dbg(&ihost->pdev->dev,
		"%s: idev = %p, done\n", __func__, idev);
}

static struct isci_remote_device *
isci_remote_device_alloc(struct isci_host *ihost, struct isci_port *iport)
{
	struct isci_remote_device *idev;
	int i;

	for (i = 0; i < SCI_MAX_REMOTE_DEVICES; i++) {
		idev = &ihost->devices[i];
		if (!test_and_set_bit(IDEV_ALLOCATED, &idev->flags))
			break;
	}

	if (i >= SCI_MAX_REMOTE_DEVICES) {
		dev_warn(&ihost->pdev->dev, "%s: failed\n", __func__);
		return NULL;
	}

	if (WARN_ONCE(!list_empty(&idev->reqs_in_process), "found requests in process\n"))
		return NULL;

	if (WARN_ONCE(!list_empty(&idev->node), "found non-idle remote device\n"))
		return NULL;

	return idev;
}

void isci_remote_device_release(struct kref *kref)
{
	struct isci_remote_device *idev = container_of(kref, typeof(*idev), kref);
	struct isci_host *ihost = idev->isci_port->isci_host;

	idev->domain_dev = NULL;
	idev->isci_port = NULL;
	clear_bit(IDEV_START_PENDING, &idev->flags);
	clear_bit(IDEV_STOP_PENDING, &idev->flags);
	clear_bit(IDEV_IO_READY, &idev->flags);
	clear_bit(IDEV_GONE, &idev->flags);
	smp_mb__before_clear_bit();
	clear_bit(IDEV_ALLOCATED, &idev->flags);
	wake_up(&ihost->eventq);
}

enum sci_status isci_remote_device_stop(struct isci_host *ihost, struct isci_remote_device *idev)
{
	enum sci_status status;
	unsigned long flags;

	dev_dbg(&ihost->pdev->dev,
		"%s: isci_device = %p\n", __func__, idev);

	spin_lock_irqsave(&ihost->scic_lock, flags);
	idev->domain_dev->lldd_dev = NULL; 
	set_bit(IDEV_GONE, &idev->flags);
	spin_unlock_irqrestore(&ihost->scic_lock, flags);

	
	isci_remote_device_nuke_requests(ihost, idev);

	set_bit(IDEV_STOP_PENDING, &idev->flags);

	spin_lock_irqsave(&ihost->scic_lock, flags);
	status = sci_remote_device_stop(idev, 50);
	spin_unlock_irqrestore(&ihost->scic_lock, flags);

	
	if (WARN_ONCE(status != SCI_SUCCESS, "failed to stop device\n"))
		;
	else
		wait_for_device_stop(ihost, idev);

	return status;
}

void isci_remote_device_gone(struct domain_device *dev)
{
	struct isci_host *ihost = dev_to_ihost(dev);
	struct isci_remote_device *idev = dev->lldd_dev;

	dev_dbg(&ihost->pdev->dev,
		"%s: domain_device = %p, isci_device = %p, isci_port = %p\n",
		__func__, dev, idev, idev->isci_port);

	isci_remote_device_stop(ihost, idev);
}


int isci_remote_device_found(struct domain_device *dev)
{
	struct isci_host *isci_host = dev_to_ihost(dev);
	struct isci_port *isci_port = dev->port->lldd_port;
	struct isci_remote_device *isci_device;
	enum sci_status status;

	dev_dbg(&isci_host->pdev->dev,
		"%s: domain_device = %p\n", __func__, dev);

	if (!isci_port)
		return -ENODEV;

	isci_device = isci_remote_device_alloc(isci_host, isci_port);
	if (!isci_device)
		return -ENODEV;

	kref_init(&isci_device->kref);
	INIT_LIST_HEAD(&isci_device->node);

	spin_lock_irq(&isci_host->scic_lock);
	isci_device->domain_dev = dev;
	isci_device->isci_port = isci_port;
	list_add_tail(&isci_device->node, &isci_port->remote_dev_list);

	set_bit(IDEV_START_PENDING, &isci_device->flags);
	status = isci_remote_device_construct(isci_port, isci_device);

	dev_dbg(&isci_host->pdev->dev,
		"%s: isci_device = %p\n",
		__func__, isci_device);

	if (status == SCI_SUCCESS) {
		
		dev->lldd_dev = isci_device;
	} else
		isci_put_device(isci_device);
	spin_unlock_irq(&isci_host->scic_lock);

	
	wait_for_device_start(isci_host, isci_device);

	return status == SCI_SUCCESS ? 0 : -ENODEV;
}
