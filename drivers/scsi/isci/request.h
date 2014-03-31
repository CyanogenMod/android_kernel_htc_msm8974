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

#ifndef _ISCI_REQUEST_H_
#define _ISCI_REQUEST_H_

#include "isci.h"
#include "host.h"
#include "scu_task_context.h"

enum isci_request_status {
	unallocated = 0x00,
	allocated   = 0x01,
	started     = 0x02,
	completed   = 0x03,
	aborting    = 0x04,
	aborted     = 0x05,
	terminating = 0x06,
	dead        = 0x07
};

enum sci_request_protocol {
	SCIC_NO_PROTOCOL,
	SCIC_SMP_PROTOCOL,
	SCIC_SSP_PROTOCOL,
	SCIC_STP_PROTOCOL
}; ;

struct isci_stp_request {
	u32 pio_len;
	u8 status;

	struct isci_stp_pio_sgl {
		int index;
		u8 set;
		u32 offset;
	} sgl;
};

struct isci_request {
	enum isci_request_status status;
	#define IREQ_COMPLETE_IN_TARGET 0
	#define IREQ_TERMINATED 1
	#define IREQ_TMF 2
	#define IREQ_ACTIVE 3
	unsigned long flags;
	
	union ttype_ptr_union {
		struct sas_task *io_task_ptr;   
		struct isci_tmf *tmf_task_ptr;  
	} ttype_ptr;
	struct isci_host *isci_host;
	
	struct list_head completed_node;
	
	struct list_head dev_node;
	spinlock_t state_lock;
	dma_addr_t request_daddr;
	dma_addr_t zero_scatter_daddr;
	unsigned int num_sg_entries;
	struct completion *io_request_completion;
	struct sci_base_state_machine sm;
	struct isci_host *owning_controller;
	struct isci_remote_device *target_device;
	u16 io_tag;
	enum sci_request_protocol protocol;
	u32 scu_status; 
	u32 sci_status; 
	u32 post_context;
	struct scu_task_context *tc;
	
	#define SCU_SGL_SIZE ((SCI_MAX_SCATTER_GATHER_ELEMENTS + 1) / 2)
	struct scu_sgl_element_pair sg_table[SCU_SGL_SIZE] __attribute__ ((aligned(32)));
	u32 saved_rx_frame_index;

	union {
		struct {
			union {
				struct ssp_cmd_iu cmd;
				struct ssp_task_iu tmf;
			};
			union {
				struct ssp_response_iu rsp;
				u8 rsp_buf[SSP_RESP_IU_MAX_SIZE];
			};
		} ssp;
		struct {
			struct isci_stp_request req;
			struct host_to_dev_fis cmd;
			struct dev_to_host_fis rsp;
		} stp;
	};
};

static inline struct isci_request *to_ireq(struct isci_stp_request *stp_req)
{
	struct isci_request *ireq;

	ireq = container_of(stp_req, typeof(*ireq), stp.req);
	return ireq;
}

#define REQUEST_STATES {\
	C(REQ_INIT),\
	C(REQ_CONSTRUCTED),\
	C(REQ_STARTED),\
	C(REQ_STP_UDMA_WAIT_TC_COMP),\
	C(REQ_STP_UDMA_WAIT_D2H),\
	C(REQ_STP_NON_DATA_WAIT_H2D),\
	C(REQ_STP_NON_DATA_WAIT_D2H),\
	C(REQ_STP_PIO_WAIT_H2D),\
	C(REQ_STP_PIO_WAIT_FRAME),\
	C(REQ_STP_PIO_DATA_IN),\
	C(REQ_STP_PIO_DATA_OUT),\
	C(REQ_ATAPI_WAIT_H2D),\
	C(REQ_ATAPI_WAIT_PIO_SETUP),\
	C(REQ_ATAPI_WAIT_D2H),\
	C(REQ_ATAPI_WAIT_TC_COMP),\
	C(REQ_TASK_WAIT_TC_COMP),\
	C(REQ_TASK_WAIT_TC_RESP),\
	C(REQ_SMP_WAIT_RESP),\
	C(REQ_SMP_WAIT_TC_COMP),\
	C(REQ_COMPLETED),\
	C(REQ_ABORTING),\
	C(REQ_FINAL),\
	}
#undef C
#define C(a) SCI_##a
enum sci_base_request_states REQUEST_STATES;
#undef C
const char *req_state_name(enum sci_base_request_states state);

enum sci_status sci_request_start(struct isci_request *ireq);
enum sci_status sci_io_request_terminate(struct isci_request *ireq);
enum sci_status
sci_io_request_event_handler(struct isci_request *ireq,
				  u32 event_code);
enum sci_status
sci_io_request_frame_handler(struct isci_request *ireq,
				  u32 frame_index);
enum sci_status
sci_task_request_terminate(struct isci_request *ireq);
extern enum sci_status
sci_request_complete(struct isci_request *ireq);
extern enum sci_status
sci_io_request_tc_completion(struct isci_request *ireq, u32 code);

static inline dma_addr_t
sci_io_request_get_dma_addr(struct isci_request *ireq, void *virt_addr)
{

	char *requested_addr = (char *)virt_addr;
	char *base_addr = (char *)ireq;

	BUG_ON(requested_addr < base_addr);
	BUG_ON((requested_addr - base_addr) >= sizeof(*ireq));

	return ireq->request_daddr + (requested_addr - base_addr);
}

static inline enum isci_request_status
isci_request_change_state(struct isci_request *isci_request,
			  enum isci_request_status status)
{
	enum isci_request_status old_state;
	unsigned long flags;

	dev_dbg(&isci_request->isci_host->pdev->dev,
		"%s: isci_request = %p, state = 0x%x\n",
		__func__,
		isci_request,
		status);

	BUG_ON(isci_request == NULL);

	spin_lock_irqsave(&isci_request->state_lock, flags);
	old_state = isci_request->status;
	isci_request->status = status;
	spin_unlock_irqrestore(&isci_request->state_lock, flags);

	return old_state;
}

static inline enum isci_request_status
isci_request_change_started_to_newstate(struct isci_request *isci_request,
					struct completion *completion_ptr,
					enum isci_request_status newstate)
{
	enum isci_request_status old_state;
	unsigned long flags;

	spin_lock_irqsave(&isci_request->state_lock, flags);

	old_state = isci_request->status;

	if (old_state == started || old_state == aborting) {
		BUG_ON(isci_request->io_request_completion != NULL);

		isci_request->io_request_completion = completion_ptr;
		isci_request->status = newstate;
	}

	spin_unlock_irqrestore(&isci_request->state_lock, flags);

	dev_dbg(&isci_request->isci_host->pdev->dev,
		"%s: isci_request = %p, old_state = 0x%x\n",
		__func__,
		isci_request,
		old_state);

	return old_state;
}

static inline enum isci_request_status
isci_request_change_started_to_aborted(struct isci_request *isci_request,
				       struct completion *completion_ptr)
{
	return isci_request_change_started_to_newstate(isci_request,
						       completion_ptr,
						       aborted);
}

#define isci_request_access_task(req) ((req)->ttype_ptr.io_task_ptr)

#define isci_request_access_tmf(req) ((req)->ttype_ptr.tmf_task_ptr)

struct isci_request *isci_tmf_request_from_tag(struct isci_host *ihost,
					       struct isci_tmf *isci_tmf,
					       u16 tag);
int isci_request_execute(struct isci_host *ihost, struct isci_remote_device *idev,
			 struct sas_task *task, u16 tag);
void isci_terminate_pending_requests(struct isci_host *ihost,
				     struct isci_remote_device *idev);
enum sci_status
sci_task_request_construct(struct isci_host *ihost,
			    struct isci_remote_device *idev,
			    u16 io_tag,
			    struct isci_request *ireq);
enum sci_status sci_task_request_construct_ssp(struct isci_request *ireq);
void sci_smp_request_copy_response(struct isci_request *ireq);

static inline int isci_task_is_ncq_recovery(struct sas_task *task)
{
	return (sas_protocol_ata(task->task_proto) &&
		task->ata_task.fis.command == ATA_CMD_READ_LOG_EXT &&
		task->ata_task.fis.lbal == ATA_LOG_SATA_NCQ);

}

#endif 
