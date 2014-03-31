
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

#include <linux/export.h>
#include <acpi/acpi.h>
#include "accommon.h"
#include "acnamesp.h"
#include "acevents.h"

#define _COMPONENT          ACPI_EVENTS
ACPI_MODULE_NAME("evxfregn")

acpi_status
acpi_install_address_space_handler(acpi_handle device,
				   acpi_adr_space_type space_id,
				   acpi_adr_space_handler handler,
				   acpi_adr_space_setup setup, void *context)
{
	struct acpi_namespace_node *node;
	acpi_status status;

	ACPI_FUNCTION_TRACE(acpi_install_address_space_handler);

	

	if (!device) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	status = acpi_ut_acquire_mutex(ACPI_MTX_NAMESPACE);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	

	node = acpi_ns_validate_handle(device);
	if (!node) {
		status = AE_BAD_PARAMETER;
		goto unlock_and_exit;
	}

	

	status =
	    acpi_ev_install_space_handler(node, space_id, handler, setup,
					  context);
	if (ACPI_FAILURE(status)) {
		goto unlock_and_exit;
	}

	switch (space_id) {
	case ACPI_ADR_SPACE_SYSTEM_MEMORY:
	case ACPI_ADR_SPACE_SYSTEM_IO:
	case ACPI_ADR_SPACE_PCI_CONFIG:
	case ACPI_ADR_SPACE_DATA_TABLE:

		if (!acpi_gbl_reg_methods_executed) {

			
			goto unlock_and_exit;
		}
		break;

	default:
		break;
	}

	

	status = acpi_ev_execute_reg_methods(node, space_id);

      unlock_and_exit:
	(void)acpi_ut_release_mutex(ACPI_MTX_NAMESPACE);
	return_ACPI_STATUS(status);
}

ACPI_EXPORT_SYMBOL(acpi_install_address_space_handler)

acpi_status
acpi_remove_address_space_handler(acpi_handle device,
				  acpi_adr_space_type space_id,
				  acpi_adr_space_handler handler)
{
	union acpi_operand_object *obj_desc;
	union acpi_operand_object *handler_obj;
	union acpi_operand_object *region_obj;
	union acpi_operand_object **last_obj_ptr;
	struct acpi_namespace_node *node;
	acpi_status status;

	ACPI_FUNCTION_TRACE(acpi_remove_address_space_handler);

	

	if (!device) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	status = acpi_ut_acquire_mutex(ACPI_MTX_NAMESPACE);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	

	node = acpi_ns_validate_handle(device);
	if (!node ||
	    ((node->type != ACPI_TYPE_DEVICE) &&
	     (node->type != ACPI_TYPE_PROCESSOR) &&
	     (node->type != ACPI_TYPE_THERMAL) &&
	     (node != acpi_gbl_root_node))) {
		status = AE_BAD_PARAMETER;
		goto unlock_and_exit;
	}

	

	obj_desc = acpi_ns_get_attached_object(node);
	if (!obj_desc) {
		status = AE_NOT_EXIST;
		goto unlock_and_exit;
	}

	

	handler_obj = obj_desc->device.handler;
	last_obj_ptr = &obj_desc->device.handler;
	while (handler_obj) {

		

		if (handler_obj->address_space.space_id == space_id) {

			

			if (handler_obj->address_space.handler != handler) {
				status = AE_BAD_PARAMETER;
				goto unlock_and_exit;
			}

			

			ACPI_DEBUG_PRINT((ACPI_DB_OPREGION,
					  "Removing address handler %p(%p) for region %s "
					  "on Device %p(%p)\n",
					  handler_obj, handler,
					  acpi_ut_get_region_name(space_id),
					  node, obj_desc));

			region_obj = handler_obj->address_space.region_list;

			

			while (region_obj) {
				acpi_ev_detach_region(region_obj, TRUE);

				region_obj =
				    handler_obj->address_space.region_list;

			}

			

			*last_obj_ptr = handler_obj->address_space.next;

			

			acpi_ut_remove_reference(handler_obj);
			goto unlock_and_exit;
		}

		

		last_obj_ptr = &handler_obj->address_space.next;
		handler_obj = handler_obj->address_space.next;
	}

	

	ACPI_DEBUG_PRINT((ACPI_DB_OPREGION,
			  "Unable to remove address handler %p for %s(%X), DevNode %p, obj %p\n",
			  handler, acpi_ut_get_region_name(space_id), space_id,
			  node, obj_desc));

	status = AE_NOT_EXIST;

      unlock_and_exit:
	(void)acpi_ut_release_mutex(ACPI_MTX_NAMESPACE);
	return_ACPI_STATUS(status);
}

ACPI_EXPORT_SYMBOL(acpi_remove_address_space_handler)
