
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
#include "acinterp.h"
#include "amlcode.h"
#include "acnamesp.h"

#define _COMPONENT          ACPI_EXECUTER
ACPI_MODULE_NAME("excreate")
#ifndef ACPI_NO_METHOD_EXECUTION
acpi_status acpi_ex_create_alias(struct acpi_walk_state *walk_state)
{
	struct acpi_namespace_node *target_node;
	struct acpi_namespace_node *alias_node;
	acpi_status status = AE_OK;

	ACPI_FUNCTION_TRACE(ex_create_alias);

	

	alias_node = (struct acpi_namespace_node *)walk_state->operands[0];
	target_node = (struct acpi_namespace_node *)walk_state->operands[1];

	if ((target_node->type == ACPI_TYPE_LOCAL_ALIAS) ||
	    (target_node->type == ACPI_TYPE_LOCAL_METHOD_ALIAS)) {
		target_node =
		    ACPI_CAST_PTR(struct acpi_namespace_node,
				  target_node->object);
	}

	switch (target_node->type) {

		

	case ACPI_TYPE_INTEGER:
	case ACPI_TYPE_STRING:
	case ACPI_TYPE_BUFFER:
	case ACPI_TYPE_PACKAGE:
	case ACPI_TYPE_BUFFER_FIELD:

	case ACPI_TYPE_DEVICE:
	case ACPI_TYPE_POWER:
	case ACPI_TYPE_PROCESSOR:
	case ACPI_TYPE_THERMAL:
	case ACPI_TYPE_LOCAL_SCOPE:

		alias_node->type = ACPI_TYPE_LOCAL_ALIAS;
		alias_node->object =
		    ACPI_CAST_PTR(union acpi_operand_object, target_node);
		break;

	case ACPI_TYPE_METHOD:

		alias_node->type = ACPI_TYPE_LOCAL_METHOD_ALIAS;
		alias_node->object =
		    ACPI_CAST_PTR(union acpi_operand_object, target_node);
		break;

	default:

		

		status = acpi_ns_attach_object(alias_node,
					       acpi_ns_get_attached_object
					       (target_node),
					       target_node->type);
		break;
	}

	

	return_ACPI_STATUS(status);
}


acpi_status acpi_ex_create_event(struct acpi_walk_state *walk_state)
{
	acpi_status status;
	union acpi_operand_object *obj_desc;

	ACPI_FUNCTION_TRACE(ex_create_event);

	obj_desc = acpi_ut_create_internal_object(ACPI_TYPE_EVENT);
	if (!obj_desc) {
		status = AE_NO_MEMORY;
		goto cleanup;
	}

	status = acpi_os_create_semaphore(ACPI_NO_UNIT_LIMIT, 0,
					  &obj_desc->event.os_semaphore);
	if (ACPI_FAILURE(status)) {
		goto cleanup;
	}

	

	status =
	    acpi_ns_attach_object((struct acpi_namespace_node *)walk_state->
				  operands[0], obj_desc, ACPI_TYPE_EVENT);

      cleanup:
	acpi_ut_remove_reference(obj_desc);
	return_ACPI_STATUS(status);
}


acpi_status acpi_ex_create_mutex(struct acpi_walk_state *walk_state)
{
	acpi_status status = AE_OK;
	union acpi_operand_object *obj_desc;

	ACPI_FUNCTION_TRACE_PTR(ex_create_mutex, ACPI_WALK_OPERANDS);

	

	obj_desc = acpi_ut_create_internal_object(ACPI_TYPE_MUTEX);
	if (!obj_desc) {
		status = AE_NO_MEMORY;
		goto cleanup;
	}

	

	status = acpi_os_create_mutex(&obj_desc->mutex.os_mutex);
	if (ACPI_FAILURE(status)) {
		goto cleanup;
	}

	

	obj_desc->mutex.sync_level =
	    (u8) walk_state->operands[1]->integer.value;
	obj_desc->mutex.node =
	    (struct acpi_namespace_node *)walk_state->operands[0];

	status =
	    acpi_ns_attach_object(obj_desc->mutex.node, obj_desc,
				  ACPI_TYPE_MUTEX);

      cleanup:
	acpi_ut_remove_reference(obj_desc);
	return_ACPI_STATUS(status);
}


acpi_status
acpi_ex_create_region(u8 * aml_start,
		      u32 aml_length,
		      u8 space_id, struct acpi_walk_state *walk_state)
{
	acpi_status status;
	union acpi_operand_object *obj_desc;
	struct acpi_namespace_node *node;
	union acpi_operand_object *region_obj2;

	ACPI_FUNCTION_TRACE(ex_create_region);

	

	node = walk_state->op->common.node;

	if (acpi_ns_get_attached_object(node)) {
		return_ACPI_STATUS(AE_OK);
	}

	if (!acpi_is_valid_space_id(space_id)) {
		ACPI_ERROR((AE_INFO,
			    "Invalid/unknown Address Space ID: 0x%2.2X",
			    space_id));
	}

	ACPI_DEBUG_PRINT((ACPI_DB_LOAD, "Region Type - %s (0x%X)\n",
			  acpi_ut_get_region_name(space_id), space_id));

	

	obj_desc = acpi_ut_create_internal_object(ACPI_TYPE_REGION);
	if (!obj_desc) {
		status = AE_NO_MEMORY;
		goto cleanup;
	}

	region_obj2 = obj_desc->common.next_object;
	region_obj2->extra.aml_start = aml_start;
	region_obj2->extra.aml_length = aml_length;
	if (walk_state->scope_info) {
		region_obj2->extra.scope_node =
		    walk_state->scope_info->scope.node;
	} else {
		region_obj2->extra.scope_node = node;
	}

	

	obj_desc->region.space_id = space_id;
	obj_desc->region.address = 0;
	obj_desc->region.length = 0;
	obj_desc->region.node = node;

	

	status = acpi_ns_attach_object(node, obj_desc, ACPI_TYPE_REGION);

      cleanup:

	

	acpi_ut_remove_reference(obj_desc);
	return_ACPI_STATUS(status);
}


acpi_status acpi_ex_create_processor(struct acpi_walk_state *walk_state)
{
	union acpi_operand_object **operand = &walk_state->operands[0];
	union acpi_operand_object *obj_desc;
	acpi_status status;

	ACPI_FUNCTION_TRACE_PTR(ex_create_processor, walk_state);

	

	obj_desc = acpi_ut_create_internal_object(ACPI_TYPE_PROCESSOR);
	if (!obj_desc) {
		return_ACPI_STATUS(AE_NO_MEMORY);
	}

	

	obj_desc->processor.proc_id = (u8) operand[1]->integer.value;
	obj_desc->processor.length = (u8) operand[3]->integer.value;
	obj_desc->processor.address =
	    (acpi_io_address) operand[2]->integer.value;

	

	status = acpi_ns_attach_object((struct acpi_namespace_node *)operand[0],
				       obj_desc, ACPI_TYPE_PROCESSOR);

	

	acpi_ut_remove_reference(obj_desc);
	return_ACPI_STATUS(status);
}


acpi_status acpi_ex_create_power_resource(struct acpi_walk_state *walk_state)
{
	union acpi_operand_object **operand = &walk_state->operands[0];
	acpi_status status;
	union acpi_operand_object *obj_desc;

	ACPI_FUNCTION_TRACE_PTR(ex_create_power_resource, walk_state);

	

	obj_desc = acpi_ut_create_internal_object(ACPI_TYPE_POWER);
	if (!obj_desc) {
		return_ACPI_STATUS(AE_NO_MEMORY);
	}

	

	obj_desc->power_resource.system_level = (u8) operand[1]->integer.value;
	obj_desc->power_resource.resource_order =
	    (u16) operand[2]->integer.value;

	

	status = acpi_ns_attach_object((struct acpi_namespace_node *)operand[0],
				       obj_desc, ACPI_TYPE_POWER);

	

	acpi_ut_remove_reference(obj_desc);
	return_ACPI_STATUS(status);
}
#endif


acpi_status
acpi_ex_create_method(u8 * aml_start,
		      u32 aml_length, struct acpi_walk_state *walk_state)
{
	union acpi_operand_object **operand = &walk_state->operands[0];
	union acpi_operand_object *obj_desc;
	acpi_status status;
	u8 method_flags;

	ACPI_FUNCTION_TRACE_PTR(ex_create_method, walk_state);

	

	obj_desc = acpi_ut_create_internal_object(ACPI_TYPE_METHOD);
	if (!obj_desc) {
		status = AE_NO_MEMORY;
		goto exit;
	}

	

	obj_desc->method.aml_start = aml_start;
	obj_desc->method.aml_length = aml_length;

	method_flags = (u8) operand[1]->integer.value;

	obj_desc->method.param_count =
	    (u8) (method_flags & AML_METHOD_ARG_COUNT);

	if (method_flags & AML_METHOD_SERIALIZED) {
		obj_desc->method.info_flags = ACPI_METHOD_SERIALIZED;

		obj_desc->method.sync_level = (u8)
		    ((method_flags & AML_METHOD_SYNC_LEVEL) >> 4);
	}

	

	status = acpi_ns_attach_object((struct acpi_namespace_node *)operand[0],
				       obj_desc, ACPI_TYPE_METHOD);

	

	acpi_ut_remove_reference(obj_desc);

      exit:
	

	acpi_ut_remove_reference(operand[1]);
	return_ACPI_STATUS(status);
}
