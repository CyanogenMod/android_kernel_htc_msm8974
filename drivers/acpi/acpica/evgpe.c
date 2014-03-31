
/*
 * Copyright (C) 2000 - 2012, Intel Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */

#include <acpi/acpi.h>
#include "accommon.h"
#include "acevents.h"
#include "acnamesp.h"

#define _COMPONENT          ACPI_EVENTS
ACPI_MODULE_NAME("evgpe")
#if (!ACPI_REDUCED_HARDWARE)	
static void ACPI_SYSTEM_XFACE acpi_ev_asynch_execute_gpe_method(void *context);

static void ACPI_SYSTEM_XFACE acpi_ev_asynch_enable_gpe(void *context);


acpi_status
acpi_ev_update_gpe_enable_mask(struct acpi_gpe_event_info *gpe_event_info)
{
	struct acpi_gpe_register_info *gpe_register_info;
	u32 register_bit;

	ACPI_FUNCTION_TRACE(ev_update_gpe_enable_mask);

	gpe_register_info = gpe_event_info->register_info;
	if (!gpe_register_info) {
		return_ACPI_STATUS(AE_NOT_EXIST);
	}

	register_bit = acpi_hw_get_gpe_register_bit(gpe_event_info,
						gpe_register_info);

	

	ACPI_CLEAR_BIT(gpe_register_info->enable_for_run, register_bit);

	

	if (gpe_event_info->runtime_count) {
		ACPI_SET_BIT(gpe_register_info->enable_for_run, (u8)register_bit);
	}

	return_ACPI_STATUS(AE_OK);
}

acpi_status
acpi_ev_enable_gpe(struct acpi_gpe_event_info *gpe_event_info)
{
	acpi_status status;

	ACPI_FUNCTION_TRACE(ev_enable_gpe);

	if ((gpe_event_info->flags & ACPI_GPE_DISPATCH_MASK) ==
	    ACPI_GPE_DISPATCH_NONE) {
		return_ACPI_STATUS(AE_NO_HANDLER);
	}

	
	status = acpi_hw_clear_gpe(gpe_event_info);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	
	status = acpi_hw_low_set_gpe(gpe_event_info, ACPI_GPE_ENABLE);

	return_ACPI_STATUS(status);
}



acpi_status acpi_ev_add_gpe_reference(struct acpi_gpe_event_info *gpe_event_info)
{
	acpi_status status = AE_OK;

	ACPI_FUNCTION_TRACE(ev_add_gpe_reference);

	if (gpe_event_info->runtime_count == ACPI_UINT8_MAX) {
		return_ACPI_STATUS(AE_LIMIT);
	}

	gpe_event_info->runtime_count++;
	if (gpe_event_info->runtime_count == 1) {

		

		status = acpi_ev_update_gpe_enable_mask(gpe_event_info);
		if (ACPI_SUCCESS(status)) {
			status = acpi_ev_enable_gpe(gpe_event_info);
		}

		if (ACPI_FAILURE(status)) {
			gpe_event_info->runtime_count--;
		}
	}

	return_ACPI_STATUS(status);
}


acpi_status acpi_ev_remove_gpe_reference(struct acpi_gpe_event_info *gpe_event_info)
{
	acpi_status status = AE_OK;

	ACPI_FUNCTION_TRACE(ev_remove_gpe_reference);

	if (!gpe_event_info->runtime_count) {
		return_ACPI_STATUS(AE_LIMIT);
	}

	gpe_event_info->runtime_count--;
	if (!gpe_event_info->runtime_count) {

		

		status = acpi_ev_update_gpe_enable_mask(gpe_event_info);
		if (ACPI_SUCCESS(status)) {
			status = acpi_hw_low_set_gpe(gpe_event_info,
						     ACPI_GPE_DISABLE);
		}

		if (ACPI_FAILURE(status)) {
			gpe_event_info->runtime_count++;
		}
	}

	return_ACPI_STATUS(status);
}


struct acpi_gpe_event_info *acpi_ev_low_get_gpe_info(u32 gpe_number,
						     struct acpi_gpe_block_info
						     *gpe_block)
{
	u32 gpe_index;

	if (!gpe_block || (gpe_number < gpe_block->block_base_number)) {
		return (NULL);
	}

	gpe_index = gpe_number - gpe_block->block_base_number;
	if (gpe_index >= gpe_block->gpe_count) {
		return (NULL);
	}

	return (&gpe_block->event_info[gpe_index]);
}



struct acpi_gpe_event_info *acpi_ev_get_gpe_event_info(acpi_handle gpe_device,
						       u32 gpe_number)
{
	union acpi_operand_object *obj_desc;
	struct acpi_gpe_event_info *gpe_info;
	u32 i;

	ACPI_FUNCTION_ENTRY();

	

	if (!gpe_device) {

		

		for (i = 0; i < ACPI_MAX_GPE_BLOCKS; i++) {
			gpe_info = acpi_ev_low_get_gpe_info(gpe_number,
							    acpi_gbl_gpe_fadt_blocks
							    [i]);
			if (gpe_info) {
				return (gpe_info);
			}
		}

		

		return (NULL);
	}

	

	obj_desc = acpi_ns_get_attached_object((struct acpi_namespace_node *)
					       gpe_device);
	if (!obj_desc || !obj_desc->device.gpe_block) {
		return (NULL);
	}

	return (acpi_ev_low_get_gpe_info
		(gpe_number, obj_desc->device.gpe_block));
}


u32 acpi_ev_gpe_detect(struct acpi_gpe_xrupt_info * gpe_xrupt_list)
{
	acpi_status status;
	struct acpi_gpe_block_info *gpe_block;
	struct acpi_gpe_register_info *gpe_register_info;
	u32 int_status = ACPI_INTERRUPT_NOT_HANDLED;
	u8 enabled_status_byte;
	u32 status_reg;
	u32 enable_reg;
	acpi_cpu_flags flags;
	u32 i;
	u32 j;

	ACPI_FUNCTION_NAME(ev_gpe_detect);

	

	if (!gpe_xrupt_list) {
		return (int_status);
	}

	flags = acpi_os_acquire_lock(acpi_gbl_gpe_lock);

	

	gpe_block = gpe_xrupt_list->gpe_block_list_head;
	while (gpe_block) {
		for (i = 0; i < gpe_block->register_count; i++) {

			

			gpe_register_info = &gpe_block->register_info[i];

			if (!(gpe_register_info->enable_for_run |
			      gpe_register_info->enable_for_wake)) {
				continue;
			}

			

			status =
			    acpi_hw_read(&status_reg,
					 &gpe_register_info->status_address);
			if (ACPI_FAILURE(status)) {
				goto unlock_and_exit;
			}

			

			status =
			    acpi_hw_read(&enable_reg,
					 &gpe_register_info->enable_address);
			if (ACPI_FAILURE(status)) {
				goto unlock_and_exit;
			}

			ACPI_DEBUG_PRINT((ACPI_DB_INTERRUPTS,
					  "Read GPE Register at GPE%02X: Status=%02X, Enable=%02X\n",
					  gpe_register_info->base_gpe_number,
					  status_reg, enable_reg));

			

			enabled_status_byte = (u8) (status_reg & enable_reg);
			if (!enabled_status_byte) {

				

				continue;
			}

			

			for (j = 0; j < ACPI_GPE_REGISTER_WIDTH; j++) {

				

				if (enabled_status_byte & (1 << j)) {
					int_status |=
					    acpi_ev_gpe_dispatch(gpe_block->
								 node,
								 &gpe_block->
						event_info[((acpi_size) i * ACPI_GPE_REGISTER_WIDTH) + j], j + gpe_register_info->base_gpe_number);
				}
			}
		}

		gpe_block = gpe_block->next;
	}

      unlock_and_exit:

	acpi_os_release_lock(acpi_gbl_gpe_lock, flags);
	return (int_status);
}


static void ACPI_SYSTEM_XFACE acpi_ev_asynch_execute_gpe_method(void *context)
{
	struct acpi_gpe_event_info *gpe_event_info = context;
	acpi_status status;
	struct acpi_gpe_event_info *local_gpe_event_info;
	struct acpi_evaluate_info *info;
	struct acpi_gpe_notify_object *notify_object;

	ACPI_FUNCTION_TRACE(ev_asynch_execute_gpe_method);

	

	local_gpe_event_info =
	    ACPI_ALLOCATE_ZEROED(sizeof(struct acpi_gpe_event_info));
	if (!local_gpe_event_info) {
		ACPI_EXCEPTION((AE_INFO, AE_NO_MEMORY, "while handling a GPE"));
		return_VOID;
	}

	status = acpi_ut_acquire_mutex(ACPI_MTX_EVENTS);
	if (ACPI_FAILURE(status)) {
		ACPI_FREE(local_gpe_event_info);
		return_VOID;
	}

	

	if (!acpi_ev_valid_gpe_event(gpe_event_info)) {
		status = acpi_ut_release_mutex(ACPI_MTX_EVENTS);
		ACPI_FREE(local_gpe_event_info);
		return_VOID;
	}

	ACPI_MEMCPY(local_gpe_event_info, gpe_event_info,
		    sizeof(struct acpi_gpe_event_info));

	status = acpi_ut_release_mutex(ACPI_MTX_EVENTS);
	if (ACPI_FAILURE(status)) {
		return_VOID;
	}

	

	switch (local_gpe_event_info->flags & ACPI_GPE_DISPATCH_MASK) {
	case ACPI_GPE_DISPATCH_NOTIFY:

		status = acpi_ev_queue_notify_request(
				local_gpe_event_info->dispatch.device.node,
				ACPI_NOTIFY_DEVICE_WAKE);

		notify_object = local_gpe_event_info->dispatch.device.next;
		while (ACPI_SUCCESS(status) && notify_object) {
			status = acpi_ev_queue_notify_request(
					notify_object->node,
					ACPI_NOTIFY_DEVICE_WAKE);
			notify_object = notify_object->next;
		}

		break;

	case ACPI_GPE_DISPATCH_METHOD:

		

		info = ACPI_ALLOCATE_ZEROED(sizeof(struct acpi_evaluate_info));
		if (!info) {
			status = AE_NO_MEMORY;
		} else {
			info->prefix_node =
			    local_gpe_event_info->dispatch.method_node;
			info->flags = ACPI_IGNORE_RETURN_VALUE;

			status = acpi_ns_evaluate(info);
			ACPI_FREE(info);
		}

		if (ACPI_FAILURE(status)) {
			ACPI_EXCEPTION((AE_INFO, status,
					"while evaluating GPE method [%4.4s]",
					acpi_ut_get_node_name
					(local_gpe_event_info->dispatch.
					 method_node)));
		}

		break;

	default:
		return_VOID;    
	}

	

	status = acpi_os_execute(OSL_NOTIFY_HANDLER,
				 acpi_ev_asynch_enable_gpe,
				 local_gpe_event_info);
	if (ACPI_FAILURE(status)) {
		ACPI_FREE(local_gpe_event_info);
	}
	return_VOID;
}



static void ACPI_SYSTEM_XFACE acpi_ev_asynch_enable_gpe(void *context)
{
	struct acpi_gpe_event_info *gpe_event_info = context;

	(void)acpi_ev_finish_gpe(gpe_event_info);

	ACPI_FREE(gpe_event_info);
	return;
}



acpi_status acpi_ev_finish_gpe(struct acpi_gpe_event_info *gpe_event_info)
{
	acpi_status status;

	if ((gpe_event_info->flags & ACPI_GPE_XRUPT_TYPE_MASK) ==
	    ACPI_GPE_LEVEL_TRIGGERED) {
		status = acpi_hw_clear_gpe(gpe_event_info);
		if (ACPI_FAILURE(status)) {
			return (status);
		}
	}

	(void)acpi_hw_low_set_gpe(gpe_event_info, ACPI_GPE_CONDITIONAL_ENABLE);
	return (AE_OK);
}



u32
acpi_ev_gpe_dispatch(struct acpi_namespace_node *gpe_device,
		    struct acpi_gpe_event_info *gpe_event_info, u32 gpe_number)
{
	acpi_status status;
	u32 return_value;

	ACPI_FUNCTION_TRACE(ev_gpe_dispatch);

	

	acpi_gpe_count++;
	if (acpi_gbl_global_event_handler) {
		acpi_gbl_global_event_handler(ACPI_EVENT_TYPE_GPE, gpe_device,
					      gpe_number,
					      acpi_gbl_global_event_handler_context);
	}

	if ((gpe_event_info->flags & ACPI_GPE_XRUPT_TYPE_MASK) ==
	    ACPI_GPE_EDGE_TRIGGERED) {
		status = acpi_hw_clear_gpe(gpe_event_info);
		if (ACPI_FAILURE(status)) {
			ACPI_EXCEPTION((AE_INFO, status,
					"Unable to clear GPE%02X", gpe_number));
			return_UINT32(ACPI_INTERRUPT_NOT_HANDLED);
		}
	}

	status = acpi_hw_low_set_gpe(gpe_event_info, ACPI_GPE_DISABLE);
	if (ACPI_FAILURE(status)) {
		ACPI_EXCEPTION((AE_INFO, status,
				"Unable to disable GPE%02X", gpe_number));
		return_UINT32(ACPI_INTERRUPT_NOT_HANDLED);
	}

	switch (gpe_event_info->flags & ACPI_GPE_DISPATCH_MASK) {
	case ACPI_GPE_DISPATCH_HANDLER:

		

		return_value =
		    gpe_event_info->dispatch.handler->address(gpe_device,
							      gpe_number,
							      gpe_event_info->
							      dispatch.handler->
							      context);

		

		if (return_value & ACPI_REENABLE_GPE) {
			(void)acpi_ev_finish_gpe(gpe_event_info);
		}
		break;

	case ACPI_GPE_DISPATCH_METHOD:
	case ACPI_GPE_DISPATCH_NOTIFY:

		status = acpi_os_execute(OSL_GPE_HANDLER,
					 acpi_ev_asynch_execute_gpe_method,
					 gpe_event_info);
		if (ACPI_FAILURE(status)) {
			ACPI_EXCEPTION((AE_INFO, status,
					"Unable to queue handler for GPE%2X - event disabled",
					gpe_number));
		}
		break;

	default:

		ACPI_ERROR((AE_INFO,
			    "No handler or method for GPE%02X, disabling event",
			    gpe_number));

		break;
	}

	return_UINT32(ACPI_INTERRUPT_HANDLED);
}

#endif				
