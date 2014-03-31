
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
ACPI_MODULE_NAME("evgpeblk")
#if (!ACPI_REDUCED_HARDWARE)	
static acpi_status
acpi_ev_install_gpe_block(struct acpi_gpe_block_info *gpe_block,
			  u32 interrupt_number);

static acpi_status
acpi_ev_create_gpe_info_blocks(struct acpi_gpe_block_info *gpe_block);


static acpi_status
acpi_ev_install_gpe_block(struct acpi_gpe_block_info *gpe_block,
			  u32 interrupt_number)
{
	struct acpi_gpe_block_info *next_gpe_block;
	struct acpi_gpe_xrupt_info *gpe_xrupt_block;
	acpi_status status;
	acpi_cpu_flags flags;

	ACPI_FUNCTION_TRACE(ev_install_gpe_block);

	status = acpi_ut_acquire_mutex(ACPI_MTX_EVENTS);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	gpe_xrupt_block = acpi_ev_get_gpe_xrupt_block(interrupt_number);
	if (!gpe_xrupt_block) {
		status = AE_NO_MEMORY;
		goto unlock_and_exit;
	}

	

	flags = acpi_os_acquire_lock(acpi_gbl_gpe_lock);
	if (gpe_xrupt_block->gpe_block_list_head) {
		next_gpe_block = gpe_xrupt_block->gpe_block_list_head;
		while (next_gpe_block->next) {
			next_gpe_block = next_gpe_block->next;
		}

		next_gpe_block->next = gpe_block;
		gpe_block->previous = next_gpe_block;
	} else {
		gpe_xrupt_block->gpe_block_list_head = gpe_block;
	}

	gpe_block->xrupt_block = gpe_xrupt_block;
	acpi_os_release_lock(acpi_gbl_gpe_lock, flags);

      unlock_and_exit:
	status = acpi_ut_release_mutex(ACPI_MTX_EVENTS);
	return_ACPI_STATUS(status);
}


acpi_status acpi_ev_delete_gpe_block(struct acpi_gpe_block_info *gpe_block)
{
	acpi_status status;
	acpi_cpu_flags flags;

	ACPI_FUNCTION_TRACE(ev_install_gpe_block);

	status = acpi_ut_acquire_mutex(ACPI_MTX_EVENTS);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	

	status =
	    acpi_hw_disable_gpe_block(gpe_block->xrupt_block, gpe_block, NULL);

	if (!gpe_block->previous && !gpe_block->next) {

		

		status = acpi_ev_delete_gpe_xrupt(gpe_block->xrupt_block);
		if (ACPI_FAILURE(status)) {
			goto unlock_and_exit;
		}
	} else {
		

		flags = acpi_os_acquire_lock(acpi_gbl_gpe_lock);
		if (gpe_block->previous) {
			gpe_block->previous->next = gpe_block->next;
		} else {
			gpe_block->xrupt_block->gpe_block_list_head =
			    gpe_block->next;
		}

		if (gpe_block->next) {
			gpe_block->next->previous = gpe_block->previous;
		}
		acpi_os_release_lock(acpi_gbl_gpe_lock, flags);
	}

	acpi_current_gpe_count -= gpe_block->gpe_count;

	

	ACPI_FREE(gpe_block->register_info);
	ACPI_FREE(gpe_block->event_info);
	ACPI_FREE(gpe_block);

      unlock_and_exit:
	status = acpi_ut_release_mutex(ACPI_MTX_EVENTS);
	return_ACPI_STATUS(status);
}


static acpi_status
acpi_ev_create_gpe_info_blocks(struct acpi_gpe_block_info *gpe_block)
{
	struct acpi_gpe_register_info *gpe_register_info = NULL;
	struct acpi_gpe_event_info *gpe_event_info = NULL;
	struct acpi_gpe_event_info *this_event;
	struct acpi_gpe_register_info *this_register;
	u32 i;
	u32 j;
	acpi_status status;

	ACPI_FUNCTION_TRACE(ev_create_gpe_info_blocks);

	

	gpe_register_info = ACPI_ALLOCATE_ZEROED((acpi_size) gpe_block->
						 register_count *
						 sizeof(struct
							acpi_gpe_register_info));
	if (!gpe_register_info) {
		ACPI_ERROR((AE_INFO,
			    "Could not allocate the GpeRegisterInfo table"));
		return_ACPI_STATUS(AE_NO_MEMORY);
	}

	gpe_event_info = ACPI_ALLOCATE_ZEROED((acpi_size) gpe_block->gpe_count *
					      sizeof(struct
						     acpi_gpe_event_info));
	if (!gpe_event_info) {
		ACPI_ERROR((AE_INFO,
			    "Could not allocate the GpeEventInfo table"));
		status = AE_NO_MEMORY;
		goto error_exit;
	}

	

	gpe_block->register_info = gpe_register_info;
	gpe_block->event_info = gpe_event_info;

	this_register = gpe_register_info;
	this_event = gpe_event_info;

	for (i = 0; i < gpe_block->register_count; i++) {

		

		this_register->base_gpe_number =
		    (u8) (gpe_block->block_base_number +
			  (i * ACPI_GPE_REGISTER_WIDTH));

		this_register->status_address.address =
		    gpe_block->block_address.address + i;

		this_register->enable_address.address =
		    gpe_block->block_address.address + i +
		    gpe_block->register_count;

		this_register->status_address.space_id =
		    gpe_block->block_address.space_id;
		this_register->enable_address.space_id =
		    gpe_block->block_address.space_id;
		this_register->status_address.bit_width =
		    ACPI_GPE_REGISTER_WIDTH;
		this_register->enable_address.bit_width =
		    ACPI_GPE_REGISTER_WIDTH;
		this_register->status_address.bit_offset = 0;
		this_register->enable_address.bit_offset = 0;

		

		for (j = 0; j < ACPI_GPE_REGISTER_WIDTH; j++) {
			this_event->gpe_number =
			    (u8) (this_register->base_gpe_number + j);
			this_event->register_info = this_register;
			this_event++;
		}

		

		status = acpi_hw_write(0x00, &this_register->enable_address);
		if (ACPI_FAILURE(status)) {
			goto error_exit;
		}

		

		status = acpi_hw_write(0xFF, &this_register->status_address);
		if (ACPI_FAILURE(status)) {
			goto error_exit;
		}

		this_register++;
	}

	return_ACPI_STATUS(AE_OK);

      error_exit:
	if (gpe_register_info) {
		ACPI_FREE(gpe_register_info);
	}
	if (gpe_event_info) {
		ACPI_FREE(gpe_event_info);
	}

	return_ACPI_STATUS(status);
}


acpi_status
acpi_ev_create_gpe_block(struct acpi_namespace_node *gpe_device,
			 struct acpi_generic_address *gpe_block_address,
			 u32 register_count,
			 u8 gpe_block_base_number,
			 u32 interrupt_number,
			 struct acpi_gpe_block_info **return_gpe_block)
{
	acpi_status status;
	struct acpi_gpe_block_info *gpe_block;
	struct acpi_gpe_walk_info walk_info;

	ACPI_FUNCTION_TRACE(ev_create_gpe_block);

	if (!register_count) {
		return_ACPI_STATUS(AE_OK);
	}

	

	gpe_block = ACPI_ALLOCATE_ZEROED(sizeof(struct acpi_gpe_block_info));
	if (!gpe_block) {
		return_ACPI_STATUS(AE_NO_MEMORY);
	}

	

	gpe_block->node = gpe_device;
	gpe_block->gpe_count = (u16)(register_count * ACPI_GPE_REGISTER_WIDTH);
	gpe_block->initialized = FALSE;
	gpe_block->register_count = register_count;
	gpe_block->block_base_number = gpe_block_base_number;

	ACPI_MEMCPY(&gpe_block->block_address, gpe_block_address,
		    sizeof(struct acpi_generic_address));

	status = acpi_ev_create_gpe_info_blocks(gpe_block);
	if (ACPI_FAILURE(status)) {
		ACPI_FREE(gpe_block);
		return_ACPI_STATUS(status);
	}

	

	status = acpi_ev_install_gpe_block(gpe_block, interrupt_number);
	if (ACPI_FAILURE(status)) {
		ACPI_FREE(gpe_block);
		return_ACPI_STATUS(status);
	}

	acpi_gbl_all_gpes_initialized = FALSE;

	

	walk_info.gpe_block = gpe_block;
	walk_info.gpe_device = gpe_device;
	walk_info.execute_by_owner_id = FALSE;

	status = acpi_ns_walk_namespace(ACPI_TYPE_METHOD, gpe_device,
					ACPI_UINT32_MAX, ACPI_NS_WALK_NO_UNLOCK,
					acpi_ev_match_gpe_method, NULL,
					&walk_info, NULL);

	

	if (return_gpe_block) {
		(*return_gpe_block) = gpe_block;
	}

	ACPI_DEBUG_PRINT((ACPI_DB_INIT,
			  "GPE %02X to %02X [%4.4s] %u regs on int 0x%X\n",
			  (u32) gpe_block->block_base_number,
			  (u32) (gpe_block->block_base_number +
				(gpe_block->gpe_count - 1)),
			  gpe_device->name.ascii, gpe_block->register_count,
			  interrupt_number));

	

	acpi_current_gpe_count += gpe_block->gpe_count;
	return_ACPI_STATUS(AE_OK);
}


acpi_status
acpi_ev_initialize_gpe_block(struct acpi_gpe_xrupt_info *gpe_xrupt_info,
			     struct acpi_gpe_block_info *gpe_block,
			     void *ignored)
{
	acpi_status status;
	struct acpi_gpe_event_info *gpe_event_info;
	u32 gpe_enabled_count;
	u32 gpe_index;
	u32 i;
	u32 j;

	ACPI_FUNCTION_TRACE(ev_initialize_gpe_block);

	if (!gpe_block || gpe_block->initialized) {
		return_ACPI_STATUS(AE_OK);
	}

	gpe_enabled_count = 0;

	for (i = 0; i < gpe_block->register_count; i++) {
		for (j = 0; j < ACPI_GPE_REGISTER_WIDTH; j++) {

			

			gpe_index = (i * ACPI_GPE_REGISTER_WIDTH) + j;
			gpe_event_info = &gpe_block->event_info[gpe_index];

			if (((gpe_event_info->flags & ACPI_GPE_DISPATCH_MASK) ==
			     ACPI_GPE_DISPATCH_NONE)
			    || ((gpe_event_info->flags & ACPI_GPE_DISPATCH_MASK)
				== ACPI_GPE_DISPATCH_HANDLER)
			    || (gpe_event_info->flags & ACPI_GPE_CAN_WAKE)) {
				continue;
			}

			status = acpi_ev_add_gpe_reference(gpe_event_info);
			if (ACPI_FAILURE(status)) {
				ACPI_EXCEPTION((AE_INFO, status,
					"Could not enable GPE 0x%02X",
					gpe_index + gpe_block->block_base_number));
				continue;
			}

			gpe_enabled_count++;
		}
	}

	if (gpe_enabled_count) {
		ACPI_DEBUG_PRINT((ACPI_DB_INIT,
				  "Enabled %u GPEs in this block\n",
				  gpe_enabled_count));
	}

	gpe_block->initialized = TRUE;

	return_ACPI_STATUS(AE_OK);
}

#endif				
