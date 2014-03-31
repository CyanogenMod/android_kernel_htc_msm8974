
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
#include "acparser.h"
#include "amlcode.h"
#include "acdispat.h"
#include "acinterp.h"
#include "acnamesp.h"
#include "acevents.h"
#include "actables.h"

#define _COMPONENT          ACPI_DISPATCHER
ACPI_MODULE_NAME("dsopcode")

static acpi_status
acpi_ds_init_buffer_field(u16 aml_opcode,
			  union acpi_operand_object *obj_desc,
			  union acpi_operand_object *buffer_desc,
			  union acpi_operand_object *offset_desc,
			  union acpi_operand_object *length_desc,
			  union acpi_operand_object *result_desc);


acpi_status acpi_ds_initialize_region(acpi_handle obj_handle)
{
	union acpi_operand_object *obj_desc;
	acpi_status status;

	obj_desc = acpi_ns_get_attached_object(obj_handle);

	

	status = acpi_ev_initialize_region(obj_desc, FALSE);
	return (status);
}


static acpi_status
acpi_ds_init_buffer_field(u16 aml_opcode,
			  union acpi_operand_object *obj_desc,
			  union acpi_operand_object *buffer_desc,
			  union acpi_operand_object *offset_desc,
			  union acpi_operand_object *length_desc,
			  union acpi_operand_object *result_desc)
{
	u32 offset;
	u32 bit_offset;
	u32 bit_count;
	u8 field_flags;
	acpi_status status;

	ACPI_FUNCTION_TRACE_PTR(ds_init_buffer_field, obj_desc);

	

	if (buffer_desc->common.type != ACPI_TYPE_BUFFER) {
		ACPI_ERROR((AE_INFO,
			    "Target of Create Field is not a Buffer object - %s",
			    acpi_ut_get_object_type_name(buffer_desc)));

		status = AE_AML_OPERAND_TYPE;
		goto cleanup;
	}

	if (ACPI_GET_DESCRIPTOR_TYPE(result_desc) != ACPI_DESC_TYPE_NAMED) {
		ACPI_ERROR((AE_INFO,
			    "(%s) destination not a NS Node [%s]",
			    acpi_ps_get_opcode_name(aml_opcode),
			    acpi_ut_get_descriptor_name(result_desc)));

		status = AE_AML_OPERAND_TYPE;
		goto cleanup;
	}

	offset = (u32) offset_desc->integer.value;

	switch (aml_opcode) {
	case AML_CREATE_FIELD_OP:

		

		field_flags = AML_FIELD_ACCESS_BYTE;
		bit_offset = offset;
		bit_count = (u32) length_desc->integer.value;

		

		if (bit_count == 0) {
			ACPI_ERROR((AE_INFO,
				    "Attempt to CreateField of length zero"));
			status = AE_AML_OPERAND_VALUE;
			goto cleanup;
		}
		break;

	case AML_CREATE_BIT_FIELD_OP:

		

		bit_offset = offset;
		bit_count = 1;
		field_flags = AML_FIELD_ACCESS_BYTE;
		break;

	case AML_CREATE_BYTE_FIELD_OP:

		

		bit_offset = 8 * offset;
		bit_count = 8;
		field_flags = AML_FIELD_ACCESS_BYTE;
		break;

	case AML_CREATE_WORD_FIELD_OP:

		

		bit_offset = 8 * offset;
		bit_count = 16;
		field_flags = AML_FIELD_ACCESS_WORD;
		break;

	case AML_CREATE_DWORD_FIELD_OP:

		

		bit_offset = 8 * offset;
		bit_count = 32;
		field_flags = AML_FIELD_ACCESS_DWORD;
		break;

	case AML_CREATE_QWORD_FIELD_OP:

		

		bit_offset = 8 * offset;
		bit_count = 64;
		field_flags = AML_FIELD_ACCESS_QWORD;
		break;

	default:

		ACPI_ERROR((AE_INFO,
			    "Unknown field creation opcode 0x%02X",
			    aml_opcode));
		status = AE_AML_BAD_OPCODE;
		goto cleanup;
	}

	

	if ((bit_offset + bit_count) > (8 * (u32) buffer_desc->buffer.length)) {
		ACPI_ERROR((AE_INFO,
			    "Field [%4.4s] at %u exceeds Buffer [%4.4s] size %u (bits)",
			    acpi_ut_get_node_name(result_desc),
			    bit_offset + bit_count,
			    acpi_ut_get_node_name(buffer_desc->buffer.node),
			    8 * (u32) buffer_desc->buffer.length));
		status = AE_AML_BUFFER_LIMIT;
		goto cleanup;
	}

	status = acpi_ex_prep_common_field_object(obj_desc, field_flags, 0,
						  bit_offset, bit_count);
	if (ACPI_FAILURE(status)) {
		goto cleanup;
	}

	obj_desc->buffer_field.buffer_obj = buffer_desc;

	

	buffer_desc->common.reference_count = (u16)
	    (buffer_desc->common.reference_count +
	     obj_desc->common.reference_count);

      cleanup:

	

	acpi_ut_remove_reference(offset_desc);
	acpi_ut_remove_reference(buffer_desc);

	if (aml_opcode == AML_CREATE_FIELD_OP) {
		acpi_ut_remove_reference(length_desc);
	}

	

	if (ACPI_FAILURE(status)) {
		acpi_ut_remove_reference(result_desc);	
	} else {
		

		obj_desc->buffer_field.flags |= AOPOBJ_DATA_VALID;
	}

	return_ACPI_STATUS(status);
}


acpi_status
acpi_ds_eval_buffer_field_operands(struct acpi_walk_state *walk_state,
				   union acpi_parse_object *op)
{
	acpi_status status;
	union acpi_operand_object *obj_desc;
	struct acpi_namespace_node *node;
	union acpi_parse_object *next_op;

	ACPI_FUNCTION_TRACE_PTR(ds_eval_buffer_field_operands, op);

	node = op->common.node;

	

	next_op = op->common.value.arg;

	

	status = acpi_ds_create_operands(walk_state, next_op);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	obj_desc = acpi_ns_get_attached_object(node);
	if (!obj_desc) {
		return_ACPI_STATUS(AE_NOT_EXIST);
	}

	

	status = acpi_ex_resolve_operands(op->common.aml_opcode,
					  ACPI_WALK_OPERANDS, walk_state);
	if (ACPI_FAILURE(status)) {
		ACPI_ERROR((AE_INFO, "(%s) bad operand(s), status 0x%X",
			    acpi_ps_get_opcode_name(op->common.aml_opcode),
			    status));

		return_ACPI_STATUS(status);
	}

	

	if (op->common.aml_opcode == AML_CREATE_FIELD_OP) {

		

		status =
		    acpi_ds_init_buffer_field(op->common.aml_opcode, obj_desc,
					      walk_state->operands[0],
					      walk_state->operands[1],
					      walk_state->operands[2],
					      walk_state->operands[3]);
	} else {
		

		status =
		    acpi_ds_init_buffer_field(op->common.aml_opcode, obj_desc,
					      walk_state->operands[0],
					      walk_state->operands[1], NULL,
					      walk_state->operands[2]);
	}

	return_ACPI_STATUS(status);
}


acpi_status
acpi_ds_eval_region_operands(struct acpi_walk_state *walk_state,
			     union acpi_parse_object *op)
{
	acpi_status status;
	union acpi_operand_object *obj_desc;
	union acpi_operand_object *operand_desc;
	struct acpi_namespace_node *node;
	union acpi_parse_object *next_op;

	ACPI_FUNCTION_TRACE_PTR(ds_eval_region_operands, op);

	node = op->common.node;

	

	next_op = op->common.value.arg;

	

	next_op = next_op->common.next;

	

	status = acpi_ds_create_operands(walk_state, next_op);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	

	status = acpi_ex_resolve_operands(op->common.aml_opcode,
					  ACPI_WALK_OPERANDS, walk_state);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	obj_desc = acpi_ns_get_attached_object(node);
	if (!obj_desc) {
		return_ACPI_STATUS(AE_NOT_EXIST);
	}

	operand_desc = walk_state->operands[walk_state->num_operands - 1];

	obj_desc->region.length = (u32) operand_desc->integer.value;
	acpi_ut_remove_reference(operand_desc);

	operand_desc = walk_state->operands[walk_state->num_operands - 2];

	obj_desc->region.address = (acpi_physical_address)
	    operand_desc->integer.value;
	acpi_ut_remove_reference(operand_desc);

	ACPI_DEBUG_PRINT((ACPI_DB_EXEC, "RgnObj %p Addr %8.8X%8.8X Len %X\n",
			  obj_desc,
			  ACPI_FORMAT_NATIVE_UINT(obj_desc->region.address),
			  obj_desc->region.length));

	

	obj_desc->region.flags |= AOPOBJ_DATA_VALID;

	return_ACPI_STATUS(status);
}


acpi_status
acpi_ds_eval_table_region_operands(struct acpi_walk_state *walk_state,
				   union acpi_parse_object *op)
{
	acpi_status status;
	union acpi_operand_object *obj_desc;
	union acpi_operand_object **operand;
	struct acpi_namespace_node *node;
	union acpi_parse_object *next_op;
	u32 table_index;
	struct acpi_table_header *table;

	ACPI_FUNCTION_TRACE_PTR(ds_eval_table_region_operands, op);

	node = op->common.node;

	

	next_op = op->common.value.arg;

	status = acpi_ds_create_operands(walk_state, next_op);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	status = acpi_ex_resolve_operands(op->common.aml_opcode,
					  ACPI_WALK_OPERANDS, walk_state);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	operand = &walk_state->operands[0];

	

	status = acpi_tb_find_table(operand[0]->string.pointer,
				    operand[1]->string.pointer,
				    operand[2]->string.pointer, &table_index);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	acpi_ut_remove_reference(operand[0]);
	acpi_ut_remove_reference(operand[1]);
	acpi_ut_remove_reference(operand[2]);

	status = acpi_get_table_by_index(table_index, &table);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	obj_desc = acpi_ns_get_attached_object(node);
	if (!obj_desc) {
		return_ACPI_STATUS(AE_NOT_EXIST);
	}

	obj_desc->region.address =
	    (acpi_physical_address) ACPI_TO_INTEGER(table);
	obj_desc->region.length = table->length;

	ACPI_DEBUG_PRINT((ACPI_DB_EXEC, "RgnObj %p Addr %8.8X%8.8X Len %X\n",
			  obj_desc,
			  ACPI_FORMAT_NATIVE_UINT(obj_desc->region.address),
			  obj_desc->region.length));

	

	obj_desc->region.flags |= AOPOBJ_DATA_VALID;

	return_ACPI_STATUS(status);
}


acpi_status
acpi_ds_eval_data_object_operands(struct acpi_walk_state *walk_state,
				  union acpi_parse_object *op,
				  union acpi_operand_object *obj_desc)
{
	acpi_status status;
	union acpi_operand_object *arg_desc;
	u32 length;

	ACPI_FUNCTION_TRACE(ds_eval_data_object_operands);

	

	walk_state->operand_index = walk_state->num_operands;

	status = acpi_ds_create_operand(walk_state, op->common.value.arg, 1);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	status = acpi_ex_resolve_operands(walk_state->opcode,
					  &(walk_state->
					    operands[walk_state->num_operands -
						     1]), walk_state);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	

	arg_desc = walk_state->operands[walk_state->num_operands - 1];
	length = (u32) arg_desc->integer.value;

	

	status = acpi_ds_obj_stack_pop(1, walk_state);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	acpi_ut_remove_reference(arg_desc);

	switch (op->common.aml_opcode) {
	case AML_BUFFER_OP:

		status =
		    acpi_ds_build_internal_buffer_obj(walk_state, op, length,
						      &obj_desc);
		break;

	case AML_PACKAGE_OP:
	case AML_VAR_PACKAGE_OP:

		status =
		    acpi_ds_build_internal_package_obj(walk_state, op, length,
						       &obj_desc);
		break;

	default:
		return_ACPI_STATUS(AE_AML_BAD_OPCODE);
	}

	if (ACPI_SUCCESS(status)) {
		if ((!op->common.parent) ||
		    ((op->common.parent->common.aml_opcode != AML_PACKAGE_OP) &&
		     (op->common.parent->common.aml_opcode !=
		      AML_VAR_PACKAGE_OP)
		     && (op->common.parent->common.aml_opcode != AML_NAME_OP))) {
			walk_state->result_obj = obj_desc;
		}
	}

	return_ACPI_STATUS(status);
}


acpi_status
acpi_ds_eval_bank_field_operands(struct acpi_walk_state *walk_state,
				 union acpi_parse_object *op)
{
	acpi_status status;
	union acpi_operand_object *obj_desc;
	union acpi_operand_object *operand_desc;
	struct acpi_namespace_node *node;
	union acpi_parse_object *next_op;
	union acpi_parse_object *arg;

	ACPI_FUNCTION_TRACE_PTR(ds_eval_bank_field_operands, op);


	

	next_op = op->common.value.arg;

	

	next_op = next_op->common.next;

	

	next_op = next_op->common.next;

	walk_state->operand_index = 0;

	status = acpi_ds_create_operand(walk_state, next_op, 0);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	status = acpi_ex_resolve_to_value(&walk_state->operands[0], walk_state);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	ACPI_DUMP_OPERANDS(ACPI_WALK_OPERANDS,
			   acpi_ps_get_opcode_name(op->common.aml_opcode), 1);
	operand_desc = walk_state->operands[0];

	

	arg = acpi_ps_get_arg(op, 4);
	while (arg) {

		

		if (arg->common.aml_opcode == AML_INT_NAMEDFIELD_OP) {
			node = arg->common.node;

			obj_desc = acpi_ns_get_attached_object(node);
			if (!obj_desc) {
				return_ACPI_STATUS(AE_NOT_EXIST);
			}

			obj_desc->bank_field.value =
			    (u32) operand_desc->integer.value;
		}

		

		arg = arg->common.next;
	}

	acpi_ut_remove_reference(operand_desc);
	return_ACPI_STATUS(status);
}
