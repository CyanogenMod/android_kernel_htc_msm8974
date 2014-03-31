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

#ifndef __ISCI_H__
#define __ISCI_H__

#include <linux/interrupt.h>
#include <linux/types.h>

#define DRV_NAME "isci"
#define SCI_PCI_BAR_COUNT 2
#define SCI_NUM_MSI_X_INT 2
#define SCI_SMU_BAR       0
#define SCI_SMU_BAR_SIZE  (16*1024)
#define SCI_SCU_BAR       1
#define SCI_SCU_BAR_SIZE  (4*1024*1024)
#define SCI_IO_SPACE_BAR0 2
#define SCI_IO_SPACE_BAR1 3
#define ISCI_CAN_QUEUE_VAL 250 
#define SCIC_CONTROLLER_STOP_TIMEOUT 5000

#define SCI_CONTROLLER_INVALID_IO_TAG 0xFFFF

#define SCI_MAX_PHYS  (4UL)
#define SCI_MAX_PORTS SCI_MAX_PHYS
#define SCI_MAX_SMP_PHYS  (384) 
#define SCI_MAX_REMOTE_DEVICES (256UL)
#define SCI_MAX_IO_REQUESTS (256UL)
#define SCI_MAX_SEQ (16)
#define SCI_MAX_MSIX_MESSAGES  (2)
#define SCI_MAX_SCATTER_GATHER_ELEMENTS 130 
#define SCI_MAX_CONTROLLERS 2
#define SCI_MAX_DOMAINS  SCI_MAX_PORTS

#define SCU_MAX_CRITICAL_NOTIFICATIONS    (384)
#define SCU_MAX_EVENTS_SHIFT		  (7)
#define SCU_MAX_EVENTS                    (1 << SCU_MAX_EVENTS_SHIFT)
#define SCU_MAX_UNSOLICITED_FRAMES        (128)
#define SCU_MAX_COMPLETION_QUEUE_SCRATCH  (128)
#define SCU_MAX_COMPLETION_QUEUE_ENTRIES  (SCU_MAX_CRITICAL_NOTIFICATIONS \
					   + SCU_MAX_EVENTS \
					   + SCU_MAX_UNSOLICITED_FRAMES	\
					   + SCI_MAX_IO_REQUESTS \
					   + SCU_MAX_COMPLETION_QUEUE_SCRATCH)
#define SCU_MAX_COMPLETION_QUEUE_SHIFT	  (ilog2(SCU_MAX_COMPLETION_QUEUE_ENTRIES))

#define SCU_ABSOLUTE_MAX_UNSOLICITED_FRAMES (4096)
#define SCU_UNSOLICITED_FRAME_BUFFER_SIZE   (1024U)
#define SCU_INVALID_FRAME_INDEX             (0xFFFF)

#define SCU_IO_REQUEST_MAX_SGE_SIZE         (0x00FFFFFF)
#define SCU_IO_REQUEST_MAX_TRANSFER_LENGTH  (0x00FFFFFF)

static inline void check_sizes(void)
{
	BUILD_BUG_ON_NOT_POWER_OF_2(SCU_MAX_EVENTS);
	BUILD_BUG_ON(SCU_MAX_UNSOLICITED_FRAMES <= 8);
	BUILD_BUG_ON_NOT_POWER_OF_2(SCU_MAX_UNSOLICITED_FRAMES);
	BUILD_BUG_ON_NOT_POWER_OF_2(SCU_MAX_COMPLETION_QUEUE_ENTRIES);
	BUILD_BUG_ON(SCU_MAX_UNSOLICITED_FRAMES > SCU_ABSOLUTE_MAX_UNSOLICITED_FRAMES);
	BUILD_BUG_ON_NOT_POWER_OF_2(SCI_MAX_IO_REQUESTS);
	BUILD_BUG_ON_NOT_POWER_OF_2(SCI_MAX_SEQ);
}

enum sci_status {
	SCI_SUCCESS = 0,

	SCI_SUCCESS_IO_COMPLETE_BEFORE_START,

	SCI_SUCCESS_IO_DONE_EARLY,

	SCI_WARNING_ALREADY_IN_STATE,

	SCI_WARNING_TIMER_CONFLICT,

	SCI_WARNING_SEQUENCE_INCOMPLETE,

	SCI_FAILURE,

	SCI_FATAL_ERROR,

	SCI_FAILURE_INVALID_STATE,

	SCI_FAILURE_INSUFFICIENT_RESOURCES,

	SCI_FAILURE_CONTROLLER_NOT_FOUND,

	SCI_FAILURE_UNSUPPORTED_CONTROLLER_TYPE,

	SCI_FAILURE_UNSUPPORTED_INIT_DATA_VERSION,

	SCI_FAILURE_UNSUPPORTED_PORT_CONFIGURATION,

	SCI_FAILURE_UNSUPPORTED_PROTOCOL,

	SCI_FAILURE_UNSUPPORTED_INFORMATION_TYPE,

	SCI_FAILURE_DEVICE_EXISTS,

	SCI_FAILURE_ADDING_PHY_UNSUPPORTED,

	SCI_FAILURE_UNSUPPORTED_INFORMATION_FIELD,

	SCI_FAILURE_UNSUPPORTED_TIME_LIMIT,

	SCI_FAILURE_INVALID_PHY,

	SCI_FAILURE_INVALID_PORT,

	SCI_FAILURE_RESET_PORT_PARTIAL_SUCCESS,

	SCI_FAILURE_RESET_PORT_FAILURE,

	SCI_FAILURE_INVALID_REMOTE_DEVICE,

	SCI_FAILURE_REMOTE_DEVICE_RESET_REQUIRED,

	SCI_FAILURE_INVALID_IO_TAG,

	SCI_FAILURE_IO_RESPONSE_VALID,

	SCI_FAILURE_CONTROLLER_SPECIFIC_IO_ERR,

	SCI_FAILURE_IO_TERMINATED,

	SCI_FAILURE_IO_REQUIRES_SCSI_ABORT,

	SCI_FAILURE_DEVICE_NOT_FOUND,

	SCI_FAILURE_INVALID_ASSOCIATION,

	SCI_FAILURE_TIMEOUT,

	SCI_FAILURE_INVALID_PARAMETER_VALUE,

	SCI_FAILURE_UNSUPPORTED_MESSAGE_COUNT,

	SCI_FAILURE_NO_NCQ_TAG_AVAILABLE,

	SCI_FAILURE_PROTOCOL_VIOLATION,

	SCI_FAILURE_RETRY_REQUIRED,

	SCI_FAILURE_RETRY_LIMIT_REACHED,

	SCI_FAILURE_RESET_DEVICE_PARTIAL_SUCCESS,

	SCI_FAILURE_ILLEGAL_ROUTING_ATTRIBUTE_CONFIGURATION,

	SCI_FAILURE_EXCEED_MAX_ROUTE_INDEX,

	SCI_FAILURE_UNSUPPORTED_PCI_DEVICE_ID

};

enum sci_io_status {
	SCI_IO_SUCCESS                         = SCI_SUCCESS,
	SCI_IO_FAILURE                         = SCI_FAILURE,
	SCI_IO_SUCCESS_COMPLETE_BEFORE_START   = SCI_SUCCESS_IO_COMPLETE_BEFORE_START,
	SCI_IO_SUCCESS_IO_DONE_EARLY           = SCI_SUCCESS_IO_DONE_EARLY,
	SCI_IO_FAILURE_INVALID_STATE           = SCI_FAILURE_INVALID_STATE,
	SCI_IO_FAILURE_INSUFFICIENT_RESOURCES  = SCI_FAILURE_INSUFFICIENT_RESOURCES,
	SCI_IO_FAILURE_UNSUPPORTED_PROTOCOL    = SCI_FAILURE_UNSUPPORTED_PROTOCOL,
	SCI_IO_FAILURE_RESPONSE_VALID          = SCI_FAILURE_IO_RESPONSE_VALID,
	SCI_IO_FAILURE_CONTROLLER_SPECIFIC_ERR = SCI_FAILURE_CONTROLLER_SPECIFIC_IO_ERR,
	SCI_IO_FAILURE_TERMINATED              = SCI_FAILURE_IO_TERMINATED,
	SCI_IO_FAILURE_REQUIRES_SCSI_ABORT     = SCI_FAILURE_IO_REQUIRES_SCSI_ABORT,
	SCI_IO_FAILURE_INVALID_PARAMETER_VALUE = SCI_FAILURE_INVALID_PARAMETER_VALUE,
	SCI_IO_FAILURE_NO_NCQ_TAG_AVAILABLE    = SCI_FAILURE_NO_NCQ_TAG_AVAILABLE,
	SCI_IO_FAILURE_PROTOCOL_VIOLATION      = SCI_FAILURE_PROTOCOL_VIOLATION,

	SCI_IO_FAILURE_REMOTE_DEVICE_RESET_REQUIRED = SCI_FAILURE_REMOTE_DEVICE_RESET_REQUIRED,

	SCI_IO_FAILURE_RETRY_REQUIRED      = SCI_FAILURE_RETRY_REQUIRED,
	SCI_IO_FAILURE_RETRY_LIMIT_REACHED = SCI_FAILURE_RETRY_LIMIT_REACHED,
	SCI_IO_FAILURE_INVALID_REMOTE_DEVICE = SCI_FAILURE_INVALID_REMOTE_DEVICE
};

enum sci_task_status {
	SCI_TASK_SUCCESS                         = SCI_SUCCESS,
	SCI_TASK_FAILURE                         = SCI_FAILURE,
	SCI_TASK_FAILURE_INVALID_STATE           = SCI_FAILURE_INVALID_STATE,
	SCI_TASK_FAILURE_INSUFFICIENT_RESOURCES  = SCI_FAILURE_INSUFFICIENT_RESOURCES,
	SCI_TASK_FAILURE_UNSUPPORTED_PROTOCOL    = SCI_FAILURE_UNSUPPORTED_PROTOCOL,
	SCI_TASK_FAILURE_INVALID_TAG             = SCI_FAILURE_INVALID_IO_TAG,
	SCI_TASK_FAILURE_RESPONSE_VALID          = SCI_FAILURE_IO_RESPONSE_VALID,
	SCI_TASK_FAILURE_CONTROLLER_SPECIFIC_ERR = SCI_FAILURE_CONTROLLER_SPECIFIC_IO_ERR,
	SCI_TASK_FAILURE_TERMINATED              = SCI_FAILURE_IO_TERMINATED,
	SCI_TASK_FAILURE_INVALID_PARAMETER_VALUE = SCI_FAILURE_INVALID_PARAMETER_VALUE,

	SCI_TASK_FAILURE_REMOTE_DEVICE_RESET_REQUIRED = SCI_FAILURE_REMOTE_DEVICE_RESET_REQUIRED,
	SCI_TASK_FAILURE_RESET_DEVICE_PARTIAL_SUCCESS = SCI_FAILURE_RESET_DEVICE_PARTIAL_SUCCESS

};

static inline void sci_swab32_cpy(void *_dest, void *_src, ssize_t word_cnt)
{
	u32 *dest = _dest, *src = _src;

	while (--word_cnt >= 0)
		dest[word_cnt] = swab32(src[word_cnt]);
}

extern unsigned char no_outbound_task_to;
extern u16 ssp_max_occ_to;
extern u16 stp_max_occ_to;
extern u16 ssp_inactive_to;
extern u16 stp_inactive_to;
extern unsigned char phy_gen;
extern unsigned char max_concurr_spinup;
extern uint cable_selection_override;

irqreturn_t isci_msix_isr(int vec, void *data);
irqreturn_t isci_intx_isr(int vec, void *data);
irqreturn_t isci_error_isr(int vec, void *data);

struct sci_timer {
	struct timer_list	timer;
	bool			cancel;
};

static inline
void sci_init_timer(struct sci_timer *tmr, void (*fn)(unsigned long))
{
	tmr->timer.function = fn;
	tmr->timer.data = (unsigned long) tmr;
	tmr->cancel = 0;
	init_timer(&tmr->timer);
}

static inline void sci_mod_timer(struct sci_timer *tmr, unsigned long msec)
{
	tmr->cancel = 0;
	mod_timer(&tmr->timer, jiffies + msecs_to_jiffies(msec));
}

static inline void sci_del_timer(struct sci_timer *tmr)
{
	tmr->cancel = 1;
	del_timer(&tmr->timer);
}

struct sci_base_state_machine {
	const struct sci_base_state *state_table;
	u32 initial_state_id;
	u32 current_state_id;
	u32 previous_state_id;
};

typedef void (*sci_state_transition_t)(struct sci_base_state_machine *sm);

struct sci_base_state {
	sci_state_transition_t enter_state;	
	sci_state_transition_t exit_state;	
};

extern void sci_init_sm(struct sci_base_state_machine *sm,
			const struct sci_base_state *state_table,
			u32 initial_state);
extern void sci_change_state(struct sci_base_state_machine *sm, u32 next_state);
#endif  
