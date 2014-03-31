
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
ACPI_MODULE_NAME("evgpeinit")
#if (!ACPI_REDUCED_HARDWARE)	

acpi_status acpi_ev_gpe_initialize(void)
{
	u32 register_count0 = 0;
	u32 register_count1 = 0;
	u32 gpe_number_max = 0;
	acpi_status status;

	ACPI_FUNCTION_TRACE(ev_gpe_initialize);

	status = acpi_ut_acquire_mutex(ACPI_MTX_NAMESPACE);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}


	if (acpi_gbl_FADT.gpe0_block_length &&
	    acpi_gbl_FADT.xgpe0_block.address) {

		

		register_count0 = (u16)(acpi_gbl_FADT.gpe0_block_length / 2);

		gpe_number_max =
		    (register_count0 * ACPI_GPE_REGISTER_WIDTH) - 1;

		

		status = acpi_ev_create_gpe_block(acpi_gbl_fadt_gpe_device,
						  &acpi_gbl_FADT.xgpe0_block,
						  register_count0, 0,
						  acpi_gbl_FADT.sci_interrupt,
						  &acpi_gbl_gpe_fadt_blocks[0]);

		if (ACPI_FAILURE(status)) {
			ACPI_EXCEPTION((AE_INFO, status,
					"Could not create GPE Block 0"));
		}
	}

	if (acpi_gbl_FADT.gpe1_block_length &&
	    acpi_gbl_FADT.xgpe1_block.address) {

		

		register_count1 = (u16)(acpi_gbl_FADT.gpe1_block_length / 2);

		

		if ((register_count0) &&
		    (gpe_number_max >= acpi_gbl_FADT.gpe1_base)) {
			ACPI_ERROR((AE_INFO,
				    "GPE0 block (GPE 0 to %u) overlaps the GPE1 block "
				    "(GPE %u to %u) - Ignoring GPE1",
				    gpe_number_max, acpi_gbl_FADT.gpe1_base,
				    acpi_gbl_FADT.gpe1_base +
				    ((register_count1 *
				      ACPI_GPE_REGISTER_WIDTH) - 1)));

			

			register_count1 = 0;
		} else {
			

			status =
			    acpi_ev_create_gpe_block(acpi_gbl_fadt_gpe_device,
						     &acpi_gbl_FADT.xgpe1_block,
						     register_count1,
						     acpi_gbl_FADT.gpe1_base,
						     acpi_gbl_FADT.
						     sci_interrupt,
						     &acpi_gbl_gpe_fadt_blocks
						     [1]);

			if (ACPI_FAILURE(status)) {
				ACPI_EXCEPTION((AE_INFO, status,
						"Could not create GPE Block 1"));
			}

			gpe_number_max = acpi_gbl_FADT.gpe1_base +
			    ((register_count1 * ACPI_GPE_REGISTER_WIDTH) - 1);
		}
	}

	

	if ((register_count0 + register_count1) == 0) {

		

		ACPI_DEBUG_PRINT((ACPI_DB_INIT,
				  "There are no GPE blocks defined in the FADT\n"));
		status = AE_OK;
		goto cleanup;
	}

	

	if (gpe_number_max > ACPI_GPE_MAX) {
		ACPI_ERROR((AE_INFO,
			    "Maximum GPE number from FADT is too large: 0x%X",
			    gpe_number_max));
		status = AE_BAD_VALUE;
		goto cleanup;
	}

      cleanup:
	(void)acpi_ut_release_mutex(ACPI_MTX_NAMESPACE);
	return_ACPI_STATUS(AE_OK);
}


void acpi_ev_update_gpes(acpi_owner_id table_owner_id)
{
	struct acpi_gpe_xrupt_info *gpe_xrupt_info;
	struct acpi_gpe_block_info *gpe_block;
	struct acpi_gpe_walk_info walk_info;
	acpi_status status = AE_OK;

	status = acpi_ut_acquire_mutex(ACPI_MTX_EVENTS);
	if (ACPI_FAILURE(status)) {
		return;
	}

	walk_info.count = 0;
	walk_info.owner_id = table_owner_id;
	walk_info.execute_by_owner_id = TRUE;

	

	gpe_xrupt_info = acpi_gbl_gpe_xrupt_list_head;
	while (gpe_xrupt_info) {

		

		gpe_block = gpe_xrupt_info->gpe_block_list_head;
		while (gpe_block) {
			walk_info.gpe_block = gpe_block;
			walk_info.gpe_device = gpe_block->node;

			status = acpi_ns_walk_namespace(ACPI_TYPE_METHOD,
							walk_info.gpe_device,
							ACPI_UINT32_MAX,
							ACPI_NS_WALK_NO_UNLOCK,
							acpi_ev_match_gpe_method,
							NULL, &walk_info, NULL);
			if (ACPI_FAILURE(status)) {
				ACPI_EXCEPTION((AE_INFO, status,
						"While decoding _Lxx/_Exx methods"));
			}

			gpe_block = gpe_block->next;
		}

		gpe_xrupt_info = gpe_xrupt_info->next;
	}

	if (walk_info.count) {
		ACPI_INFO((AE_INFO, "Enabled %u new GPEs", walk_info.count));
	}

	(void)acpi_ut_release_mutex(ACPI_MTX_EVENTS);
	return;
}


acpi_status
acpi_ev_match_gpe_method(acpi_handle obj_handle,
			 u32 level, void *context, void **return_value)
{
	struct acpi_namespace_node *method_node =
	    ACPI_CAST_PTR(struct acpi_namespace_node, obj_handle);
	struct acpi_gpe_walk_info *walk_info =
	    ACPI_CAST_PTR(struct acpi_gpe_walk_info, context);
	struct acpi_gpe_event_info *gpe_event_info;
	u32 gpe_number;
	char name[ACPI_NAME_SIZE + 1];
	u8 type;

	ACPI_FUNCTION_TRACE(ev_match_gpe_method);

	

	if ((walk_info->execute_by_owner_id) &&
	    (method_node->owner_id != walk_info->owner_id)) {
		return_ACPI_STATUS(AE_OK);
	}

	ACPI_MOVE_32_TO_32(name, &method_node->name.integer);
	name[ACPI_NAME_SIZE] = 0;

	

	if (name[0] != '_') {
		return_ACPI_STATUS(AE_OK);	
	}

	switch (name[1]) {
	case 'L':
		type = ACPI_GPE_LEVEL_TRIGGERED;
		break;

	case 'E':
		type = ACPI_GPE_EDGE_TRIGGERED;
		break;

	default:
		

		ACPI_DEBUG_PRINT((ACPI_DB_LOAD,
				  "Ignoring unknown GPE method type: %s "
				  "(name not of form _Lxx or _Exx)", name));
		return_ACPI_STATUS(AE_OK);
	}

	

	gpe_number = ACPI_STRTOUL(&name[2], NULL, 16);
	if (gpe_number == ACPI_UINT32_MAX) {

		

		ACPI_DEBUG_PRINT((ACPI_DB_LOAD,
				  "Could not extract GPE number from name: %s "
				  "(name is not of form _Lxx or _Exx)", name));
		return_ACPI_STATUS(AE_OK);
	}

	

	gpe_event_info =
	    acpi_ev_low_get_gpe_info(gpe_number, walk_info->gpe_block);
	if (!gpe_event_info) {
		return_ACPI_STATUS(AE_OK);
	}

	if ((gpe_event_info->flags & ACPI_GPE_DISPATCH_MASK) ==
	    ACPI_GPE_DISPATCH_HANDLER) {

		

		return_ACPI_STATUS(AE_OK);
	}

	if ((gpe_event_info->flags & ACPI_GPE_DISPATCH_MASK) ==
	    ACPI_GPE_DISPATCH_METHOD) {
		if (type != (gpe_event_info->flags & ACPI_GPE_XRUPT_TYPE_MASK)) {
			ACPI_ERROR((AE_INFO,
				    "For GPE 0x%.2X, found both _L%2.2X and _E%2.2X methods",
				    gpe_number, gpe_number, gpe_number));
		}
		return_ACPI_STATUS(AE_OK);
	}

	
	(void)acpi_hw_low_set_gpe(gpe_event_info, ACPI_GPE_DISABLE);

	gpe_event_info->flags &= ~(ACPI_GPE_DISPATCH_MASK);
	gpe_event_info->flags |= (u8)(type | ACPI_GPE_DISPATCH_METHOD);
	gpe_event_info->dispatch.method_node = method_node;

	ACPI_DEBUG_PRINT((ACPI_DB_LOAD,
			  "Registered GPE method %s as GPE number 0x%.2X\n",
			  name, gpe_number));
	return_ACPI_STATUS(AE_OK);
}

#endif				
