
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
#include "amlcode.h"
#include "acdispat.h"
#include "acinterp.h"
#include "acnamesp.h"
#include "acparser.h"

#define _COMPONENT          ACPI_DISPATCHER
ACPI_MODULE_NAME("dsfield")

static acpi_status
acpi_ds_get_field_names(struct acpi_create_field_info *info,
			struct acpi_walk_state *walk_state,
			union acpi_parse_object *arg);


acpi_status
acpi_ds_create_buffer_field(union acpi_parse_object *op,
			    struct acpi_walk_state *walk_state)
{
	union acpi_parse_object *arg;
	struct acpi_namespace_node *node;
	acpi_status status;
	union acpi_operand_object *obj_desc;
	union acpi_operand_object *second_desc = NULL;
	u32 flags;

	ACPI_FUNCTION_TRACE(ds_create_buffer_field);

	if (op->common.aml_opcode == AML_CREATE_FIELD_OP) {

		

		arg = acpi_ps_get_arg(op, 3);
	} else {
		

		arg = acpi_ps_get_arg(op, 2);
	}

	if (!arg) {
		return_ACPI_STATUS(AE_AML_NO_OPERAND);
	}

	if (walk_state->deferred_node) {
		node = walk_state->deferred_node;
		status = AE_OK;
	} else {
		

		if (!(walk_state->parse_flags & ACPI_PARSE_EXECUTE)) {
			return_ACPI_STATUS(AE_AML_INTERNAL);
		}

		

		flags = ACPI_NS_NO_UPSEARCH | ACPI_NS_DONT_OPEN_SCOPE |
		    ACPI_NS_ERROR_IF_FOUND;

		if (walk_state->method_node &&
		    !(walk_state->parse_flags & ACPI_PARSE_MODULE_LEVEL)) {
			flags |= ACPI_NS_TEMPORARY;
		}

		

		status =
		    acpi_ns_lookup(walk_state->scope_info,
				   arg->common.value.string, ACPI_TYPE_ANY,
				   ACPI_IMODE_LOAD_PASS1, flags, walk_state,
				   &node);
		if (ACPI_FAILURE(status)) {
			ACPI_ERROR_NAMESPACE(arg->common.value.string, status);
			return_ACPI_STATUS(status);
		}
	}

	op->common.node = node;

	obj_desc = acpi_ns_get_attached_object(node);
	if (obj_desc) {
		return_ACPI_STATUS(AE_OK);
	}


	

	obj_desc = acpi_ut_create_internal_object(ACPI_TYPE_BUFFER_FIELD);
	if (!obj_desc) {
		status = AE_NO_MEMORY;
		goto cleanup;
	}

	second_desc = obj_desc->common.next_object;
	second_desc->extra.aml_start = op->named.data;
	second_desc->extra.aml_length = op->named.length;
	obj_desc->buffer_field.node = node;

	

	status = acpi_ns_attach_object(node, obj_desc, ACPI_TYPE_BUFFER_FIELD);
	if (ACPI_FAILURE(status)) {
		goto cleanup;
	}

      cleanup:

	

	acpi_ut_remove_reference(obj_desc);
	return_ACPI_STATUS(status);
}


static acpi_status
acpi_ds_get_field_names(struct acpi_create_field_info *info,
			struct acpi_walk_state *walk_state,
			union acpi_parse_object *arg)
{
	acpi_status status;
	u64 position;
	union acpi_parse_object *child;

	ACPI_FUNCTION_TRACE_PTR(ds_get_field_names, info);

	

	info->field_bit_position = 0;

	

	while (arg) {
		switch (arg->common.aml_opcode) {
		case AML_INT_RESERVEDFIELD_OP:

			position = (u64) info->field_bit_position
			    + (u64) arg->common.value.size;

			if (position > ACPI_UINT32_MAX) {
				ACPI_ERROR((AE_INFO,
					    "Bit offset within field too large (> 0xFFFFFFFF)"));
				return_ACPI_STATUS(AE_SUPPORT);
			}

			info->field_bit_position = (u32) position;
			break;

		case AML_INT_ACCESSFIELD_OP:
		case AML_INT_EXTACCESSFIELD_OP:

			

			info->field_flags = (u8)
			    ((info->
			      field_flags & ~(AML_FIELD_ACCESS_TYPE_MASK)) |
			     ((u8)((u32)(arg->common.value.integer & 0x07))));

			

			info->attribute =
			    (u8)((arg->common.value.integer >> 8) & 0xFF);

			

			info->access_length =
			    (u8)((arg->common.value.integer >> 16) & 0xFF);
			break;

		case AML_INT_CONNECTION_OP:
			info->resource_buffer = NULL;
			info->connection_node = NULL;

			child = arg->common.value.arg;
			if (child->common.aml_opcode == AML_INT_BYTELIST_OP) {
				info->resource_buffer = child->named.data;
				info->resource_length =
				    (u16)child->named.value.integer;
			} else {
				

				status = acpi_ns_lookup(walk_state->scope_info,
							child->common.value.
							name, ACPI_TYPE_ANY,
							ACPI_IMODE_EXECUTE,
							ACPI_NS_DONT_OPEN_SCOPE,
							walk_state,
							&info->connection_node);
				if (ACPI_FAILURE(status)) {
					ACPI_ERROR_NAMESPACE(child->common.
							     value.name,
							     status);
					return_ACPI_STATUS(status);
				}
			}
			break;

		case AML_INT_NAMEDFIELD_OP:

			

			status = acpi_ns_lookup(walk_state->scope_info,
						(char *)&arg->named.name,
						info->field_type,
						ACPI_IMODE_EXECUTE,
						ACPI_NS_DONT_OPEN_SCOPE,
						walk_state, &info->field_node);
			if (ACPI_FAILURE(status)) {
				ACPI_ERROR_NAMESPACE((char *)&arg->named.name,
						     status);
				return_ACPI_STATUS(status);
			} else {
				arg->common.node = info->field_node;
				info->field_bit_length = arg->common.value.size;

				if (!acpi_ns_get_attached_object
				    (info->field_node)) {
					status = acpi_ex_prep_field_value(info);
					if (ACPI_FAILURE(status)) {
						return_ACPI_STATUS(status);
					}
				}
			}

			

			position = (u64) info->field_bit_position
			    + (u64) arg->common.value.size;

			if (position > ACPI_UINT32_MAX) {
				ACPI_ERROR((AE_INFO,
					    "Field [%4.4s] bit offset too large (> 0xFFFFFFFF)",
					    ACPI_CAST_PTR(char,
							  &info->field_node->
							  name)));
				return_ACPI_STATUS(AE_SUPPORT);
			}

			info->field_bit_position += info->field_bit_length;
			break;

		default:

			ACPI_ERROR((AE_INFO,
				    "Invalid opcode in field list: 0x%X",
				    arg->common.aml_opcode));
			return_ACPI_STATUS(AE_AML_BAD_OPCODE);
		}

		arg = arg->common.next;
	}

	return_ACPI_STATUS(AE_OK);
}


acpi_status
acpi_ds_create_field(union acpi_parse_object *op,
		     struct acpi_namespace_node *region_node,
		     struct acpi_walk_state *walk_state)
{
	acpi_status status;
	union acpi_parse_object *arg;
	struct acpi_create_field_info info;

	ACPI_FUNCTION_TRACE_PTR(ds_create_field, op);

	

	arg = op->common.value.arg;
	if (!region_node) {
		status =
		    acpi_ns_lookup(walk_state->scope_info,
				   arg->common.value.name, ACPI_TYPE_REGION,
				   ACPI_IMODE_EXECUTE, ACPI_NS_SEARCH_PARENT,
				   walk_state, &region_node);
		if (ACPI_FAILURE(status)) {
			ACPI_ERROR_NAMESPACE(arg->common.value.name, status);
			return_ACPI_STATUS(status);
		}
	}

	ACPI_MEMSET(&info, 0, sizeof(struct acpi_create_field_info));

	

	arg = arg->common.next;
	info.field_flags = (u8) arg->common.value.integer;
	info.attribute = 0;

	

	info.field_type = ACPI_TYPE_LOCAL_REGION_FIELD;
	info.region_node = region_node;

	status = acpi_ds_get_field_names(&info, walk_state, arg->common.next);
	return_ACPI_STATUS(status);
}


acpi_status
acpi_ds_init_field_objects(union acpi_parse_object *op,
			   struct acpi_walk_state *walk_state)
{
	acpi_status status;
	union acpi_parse_object *arg = NULL;
	struct acpi_namespace_node *node;
	u8 type = 0;
	u32 flags;

	ACPI_FUNCTION_TRACE_PTR(ds_init_field_objects, op);

	

	if (!(walk_state->parse_flags & ACPI_PARSE_EXECUTE)) {
		if (walk_state->parse_flags & ACPI_PARSE_DEFERRED_OP) {

			

			return_ACPI_STATUS(AE_OK);
		}

		return_ACPI_STATUS(AE_AML_INTERNAL);
	}

	switch (walk_state->opcode) {
	case AML_FIELD_OP:
		arg = acpi_ps_get_arg(op, 2);
		type = ACPI_TYPE_LOCAL_REGION_FIELD;
		break;

	case AML_BANK_FIELD_OP:
		arg = acpi_ps_get_arg(op, 4);
		type = ACPI_TYPE_LOCAL_BANK_FIELD;
		break;

	case AML_INDEX_FIELD_OP:
		arg = acpi_ps_get_arg(op, 3);
		type = ACPI_TYPE_LOCAL_INDEX_FIELD;
		break;

	default:
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	

	flags = ACPI_NS_NO_UPSEARCH | ACPI_NS_DONT_OPEN_SCOPE |
	    ACPI_NS_ERROR_IF_FOUND;

	if (walk_state->method_node &&
	    !(walk_state->parse_flags & ACPI_PARSE_MODULE_LEVEL)) {
		flags |= ACPI_NS_TEMPORARY;
	}

	while (arg) {
		if (arg->common.aml_opcode == AML_INT_NAMEDFIELD_OP) {
			status = acpi_ns_lookup(walk_state->scope_info,
						(char *)&arg->named.name, type,
						ACPI_IMODE_LOAD_PASS1, flags,
						walk_state, &node);
			if (ACPI_FAILURE(status)) {
				ACPI_ERROR_NAMESPACE((char *)&arg->named.name,
						     status);
				if (status != AE_ALREADY_EXISTS) {
					return_ACPI_STATUS(status);
				}

				

				status = AE_OK;
			}

			arg->common.node = node;
		}

		

		arg = arg->common.next;
	}

	return_ACPI_STATUS(AE_OK);
}


acpi_status
acpi_ds_create_bank_field(union acpi_parse_object *op,
			  struct acpi_namespace_node *region_node,
			  struct acpi_walk_state *walk_state)
{
	acpi_status status;
	union acpi_parse_object *arg;
	struct acpi_create_field_info info;

	ACPI_FUNCTION_TRACE_PTR(ds_create_bank_field, op);

	

	arg = op->common.value.arg;
	if (!region_node) {
		status =
		    acpi_ns_lookup(walk_state->scope_info,
				   arg->common.value.name, ACPI_TYPE_REGION,
				   ACPI_IMODE_EXECUTE, ACPI_NS_SEARCH_PARENT,
				   walk_state, &region_node);
		if (ACPI_FAILURE(status)) {
			ACPI_ERROR_NAMESPACE(arg->common.value.name, status);
			return_ACPI_STATUS(status);
		}
	}

	

	arg = arg->common.next;
	status =
	    acpi_ns_lookup(walk_state->scope_info, arg->common.value.string,
			   ACPI_TYPE_ANY, ACPI_IMODE_EXECUTE,
			   ACPI_NS_SEARCH_PARENT, walk_state,
			   &info.register_node);
	if (ACPI_FAILURE(status)) {
		ACPI_ERROR_NAMESPACE(arg->common.value.string, status);
		return_ACPI_STATUS(status);
	}

	arg = arg->common.next;

	

	arg = arg->common.next;
	info.field_flags = (u8) arg->common.value.integer;

	

	info.field_type = ACPI_TYPE_LOCAL_BANK_FIELD;
	info.region_node = region_node;

	info.data_register_node = (struct acpi_namespace_node *)op;

	status = acpi_ds_get_field_names(&info, walk_state, arg->common.next);
	return_ACPI_STATUS(status);
}


acpi_status
acpi_ds_create_index_field(union acpi_parse_object *op,
			   struct acpi_namespace_node *region_node,
			   struct acpi_walk_state *walk_state)
{
	acpi_status status;
	union acpi_parse_object *arg;
	struct acpi_create_field_info info;

	ACPI_FUNCTION_TRACE_PTR(ds_create_index_field, op);

	

	arg = op->common.value.arg;
	status =
	    acpi_ns_lookup(walk_state->scope_info, arg->common.value.string,
			   ACPI_TYPE_ANY, ACPI_IMODE_EXECUTE,
			   ACPI_NS_SEARCH_PARENT, walk_state,
			   &info.register_node);
	if (ACPI_FAILURE(status)) {
		ACPI_ERROR_NAMESPACE(arg->common.value.string, status);
		return_ACPI_STATUS(status);
	}

	

	arg = arg->common.next;
	status =
	    acpi_ns_lookup(walk_state->scope_info, arg->common.value.string,
			   ACPI_TYPE_ANY, ACPI_IMODE_EXECUTE,
			   ACPI_NS_SEARCH_PARENT, walk_state,
			   &info.data_register_node);
	if (ACPI_FAILURE(status)) {
		ACPI_ERROR_NAMESPACE(arg->common.value.string, status);
		return_ACPI_STATUS(status);
	}

	

	arg = arg->common.next;
	info.field_flags = (u8) arg->common.value.integer;

	

	info.field_type = ACPI_TYPE_LOCAL_INDEX_FIELD;
	info.region_node = region_node;

	status = acpi_ds_get_field_names(&info, walk_state, arg->common.next);
	return_ACPI_STATUS(status);
}
