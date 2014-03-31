
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
#include "acinterp.h"
#include "acevents.h"
#include "amlcode.h"

#define _COMPONENT          ACPI_EXECUTER
ACPI_MODULE_NAME("exoparg2")

acpi_status acpi_ex_opcode_2A_0T_0R(struct acpi_walk_state *walk_state)
{
	union acpi_operand_object **operand = &walk_state->operands[0];
	struct acpi_namespace_node *node;
	u32 value;
	acpi_status status = AE_OK;

	ACPI_FUNCTION_TRACE_STR(ex_opcode_2A_0T_0R,
				acpi_ps_get_opcode_name(walk_state->opcode));

	

	switch (walk_state->opcode) {
	case AML_NOTIFY_OP:	

		

		node = (struct acpi_namespace_node *)operand[0];

		

		value = (u32) operand[1]->integer.value;

		

		if (!acpi_ev_is_notify_object(node)) {
			ACPI_ERROR((AE_INFO,
				    "Unexpected notify object type [%s]",
				    acpi_ut_get_type_name(node->type)));

			status = AE_AML_OPERAND_TYPE;
			break;
		}

		status = acpi_ev_queue_notify_request(node, value);
		break;

	default:

		ACPI_ERROR((AE_INFO, "Unknown AML opcode 0x%X",
			    walk_state->opcode));
		status = AE_AML_BAD_OPCODE;
	}

	return_ACPI_STATUS(status);
}


acpi_status acpi_ex_opcode_2A_2T_1R(struct acpi_walk_state *walk_state)
{
	union acpi_operand_object **operand = &walk_state->operands[0];
	union acpi_operand_object *return_desc1 = NULL;
	union acpi_operand_object *return_desc2 = NULL;
	acpi_status status;

	ACPI_FUNCTION_TRACE_STR(ex_opcode_2A_2T_1R,
				acpi_ps_get_opcode_name(walk_state->opcode));

	

	switch (walk_state->opcode) {
	case AML_DIVIDE_OP:

		

		return_desc1 =
		    acpi_ut_create_internal_object(ACPI_TYPE_INTEGER);
		if (!return_desc1) {
			status = AE_NO_MEMORY;
			goto cleanup;
		}

		return_desc2 =
		    acpi_ut_create_internal_object(ACPI_TYPE_INTEGER);
		if (!return_desc2) {
			status = AE_NO_MEMORY;
			goto cleanup;
		}

		

		status = acpi_ut_divide(operand[0]->integer.value,
					operand[1]->integer.value,
					&return_desc1->integer.value,
					&return_desc2->integer.value);
		if (ACPI_FAILURE(status)) {
			goto cleanup;
		}
		break;

	default:

		ACPI_ERROR((AE_INFO, "Unknown AML opcode 0x%X",
			    walk_state->opcode));
		status = AE_AML_BAD_OPCODE;
		goto cleanup;
	}

	

	status = acpi_ex_store(return_desc2, operand[2], walk_state);
	if (ACPI_FAILURE(status)) {
		goto cleanup;
	}

	status = acpi_ex_store(return_desc1, operand[3], walk_state);
	if (ACPI_FAILURE(status)) {
		goto cleanup;
	}

      cleanup:
	acpi_ut_remove_reference(return_desc2);

	if (ACPI_FAILURE(status)) {

		

		acpi_ut_remove_reference(return_desc1);
	}

	

	else {
		walk_state->result_obj = return_desc1;
	}

	return_ACPI_STATUS(status);
}


acpi_status acpi_ex_opcode_2A_1T_1R(struct acpi_walk_state *walk_state)
{
	union acpi_operand_object **operand = &walk_state->operands[0];
	union acpi_operand_object *return_desc = NULL;
	u64 index;
	acpi_status status = AE_OK;
	acpi_size length;

	ACPI_FUNCTION_TRACE_STR(ex_opcode_2A_1T_1R,
				acpi_ps_get_opcode_name(walk_state->opcode));

	

	if (walk_state->op_info->flags & AML_MATH) {

		

		return_desc = acpi_ut_create_internal_object(ACPI_TYPE_INTEGER);
		if (!return_desc) {
			status = AE_NO_MEMORY;
			goto cleanup;
		}

		return_desc->integer.value =
		    acpi_ex_do_math_op(walk_state->opcode,
				       operand[0]->integer.value,
				       operand[1]->integer.value);
		goto store_result_to_target;
	}

	switch (walk_state->opcode) {
	case AML_MOD_OP:	

		return_desc = acpi_ut_create_internal_object(ACPI_TYPE_INTEGER);
		if (!return_desc) {
			status = AE_NO_MEMORY;
			goto cleanup;
		}

		

		status = acpi_ut_divide(operand[0]->integer.value,
					operand[1]->integer.value,
					NULL, &return_desc->integer.value);
		break;

	case AML_CONCAT_OP:	

		status = acpi_ex_do_concatenate(operand[0], operand[1],
						&return_desc, walk_state);
		break;

	case AML_TO_STRING_OP:	


		length = 0;
		while ((length < operand[0]->buffer.length) &&
		       (length < operand[1]->integer.value) &&
		       (operand[0]->buffer.pointer[length])) {
			length++;
		}

		

		return_desc = acpi_ut_create_string_object(length);
		if (!return_desc) {
			status = AE_NO_MEMORY;
			goto cleanup;
		}

		ACPI_MEMCPY(return_desc->string.pointer,
			    operand[0]->buffer.pointer, length);
		break;

	case AML_CONCAT_RES_OP:

		

		status = acpi_ex_concat_template(operand[0], operand[1],
						 &return_desc, walk_state);
		break;

	case AML_INDEX_OP:	

		

		return_desc =
		    acpi_ut_create_internal_object(ACPI_TYPE_LOCAL_REFERENCE);
		if (!return_desc) {
			status = AE_NO_MEMORY;
			goto cleanup;
		}

		

		index = operand[1]->integer.value;
		return_desc->reference.value = (u32) index;
		return_desc->reference.class = ACPI_REFCLASS_INDEX;

		switch ((operand[0])->common.type) {
		case ACPI_TYPE_STRING:

			if (index >= operand[0]->string.length) {
				status = AE_AML_STRING_LIMIT;
			}

			return_desc->reference.target_type =
			    ACPI_TYPE_BUFFER_FIELD;
			break;

		case ACPI_TYPE_BUFFER:

			if (index >= operand[0]->buffer.length) {
				status = AE_AML_BUFFER_LIMIT;
			}

			return_desc->reference.target_type =
			    ACPI_TYPE_BUFFER_FIELD;
			break;

		case ACPI_TYPE_PACKAGE:

			if (index >= operand[0]->package.count) {
				status = AE_AML_PACKAGE_LIMIT;
			}

			return_desc->reference.target_type = ACPI_TYPE_PACKAGE;
			return_desc->reference.where =
			    &operand[0]->package.elements[index];
			break;

		default:

			status = AE_AML_INTERNAL;
			goto cleanup;
		}

		

		if (ACPI_FAILURE(status)) {
			ACPI_EXCEPTION((AE_INFO, status,
					"Index (0x%8.8X%8.8X) is beyond end of object",
					ACPI_FORMAT_UINT64(index)));
			goto cleanup;
		}

		return_desc->reference.object = operand[0];
		acpi_ut_add_reference(operand[0]);

		

		status = acpi_ex_store(return_desc, operand[2], walk_state);

		

		walk_state->result_obj = return_desc;
		goto cleanup;

	default:

		ACPI_ERROR((AE_INFO, "Unknown AML opcode 0x%X",
			    walk_state->opcode));
		status = AE_AML_BAD_OPCODE;
		break;
	}

      store_result_to_target:

	if (ACPI_SUCCESS(status)) {
		status = acpi_ex_store(return_desc, operand[2], walk_state);
		if (ACPI_FAILURE(status)) {
			goto cleanup;
		}

		if (!walk_state->result_obj) {
			walk_state->result_obj = return_desc;
		}
	}

      cleanup:

	

	if (ACPI_FAILURE(status)) {
		acpi_ut_remove_reference(return_desc);
		walk_state->result_obj = NULL;
	}

	return_ACPI_STATUS(status);
}


acpi_status acpi_ex_opcode_2A_0T_1R(struct acpi_walk_state *walk_state)
{
	union acpi_operand_object **operand = &walk_state->operands[0];
	union acpi_operand_object *return_desc = NULL;
	acpi_status status = AE_OK;
	u8 logical_result = FALSE;

	ACPI_FUNCTION_TRACE_STR(ex_opcode_2A_0T_1R,
				acpi_ps_get_opcode_name(walk_state->opcode));

	

	return_desc = acpi_ut_create_internal_object(ACPI_TYPE_INTEGER);
	if (!return_desc) {
		status = AE_NO_MEMORY;
		goto cleanup;
	}

	

	if (walk_state->op_info->flags & AML_LOGICAL_NUMERIC) {

		

		status = acpi_ex_do_logical_numeric_op(walk_state->opcode,
						       operand[0]->integer.
						       value,
						       operand[1]->integer.
						       value, &logical_result);
		goto store_logical_result;
	} else if (walk_state->op_info->flags & AML_LOGICAL) {

		

		status = acpi_ex_do_logical_op(walk_state->opcode, operand[0],
					       operand[1], &logical_result);
		goto store_logical_result;
	}

	switch (walk_state->opcode) {
	case AML_ACQUIRE_OP:	

		status =
		    acpi_ex_acquire_mutex(operand[1], operand[0], walk_state);
		if (status == AE_TIME) {
			logical_result = TRUE;	
			status = AE_OK;
		}
		break;

	case AML_WAIT_OP:	

		status = acpi_ex_system_wait_event(operand[1], operand[0]);
		if (status == AE_TIME) {
			logical_result = TRUE;	
			status = AE_OK;
		}
		break;

	default:

		ACPI_ERROR((AE_INFO, "Unknown AML opcode 0x%X",
			    walk_state->opcode));
		status = AE_AML_BAD_OPCODE;
		goto cleanup;
	}

      store_logical_result:
	if (logical_result) {
		return_desc->integer.value = ACPI_UINT64_MAX;
	}

      cleanup:

	

	if (ACPI_FAILURE(status)) {
		acpi_ut_remove_reference(return_desc);
	}

	

	else {
		walk_state->result_obj = return_desc;
	}

	return_ACPI_STATUS(status);
}
