
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

#define _COMPONENT          ACPI_DISPATCHER
ACPI_MODULE_NAME("dswload2")

acpi_status
acpi_ds_load2_begin_op(struct acpi_walk_state *walk_state,
		       union acpi_parse_object **out_op)
{
	union acpi_parse_object *op;
	struct acpi_namespace_node *node;
	acpi_status status;
	acpi_object_type object_type;
	char *buffer_ptr;
	u32 flags;

	ACPI_FUNCTION_TRACE(ds_load2_begin_op);

	op = walk_state->op;
	ACPI_DEBUG_PRINT((ACPI_DB_DISPATCH, "Op=%p State=%p\n", op,
			  walk_state));

	if (op) {
		if ((walk_state->control_state) &&
		    (walk_state->control_state->common.state ==
		     ACPI_CONTROL_CONDITIONAL_EXECUTING)) {

			

			status = acpi_ds_exec_begin_op(walk_state, out_op);
			return_ACPI_STATUS(status);
		}

		

		if ((!(walk_state->op_info->flags & AML_NSOPCODE) &&
		     (walk_state->opcode != AML_INT_NAMEPATH_OP)) ||
		    (!(walk_state->op_info->flags & AML_NAMED))) {
			return_ACPI_STATUS(AE_OK);
		}

		

		if (walk_state->opcode == AML_INT_NAMEPATH_OP) {

			

			buffer_ptr = op->common.value.string;
			if (!buffer_ptr) {

				

				return_ACPI_STATUS(AE_OK);
			}
		} else {
			

			buffer_ptr = ACPI_CAST_PTR(char, &op->named.name);
		}
	} else {
		

		buffer_ptr =
		    acpi_ps_get_next_namestring(&walk_state->parser_state);
	}

	

	object_type = walk_state->op_info->object_type;

	ACPI_DEBUG_PRINT((ACPI_DB_DISPATCH,
			  "State=%p Op=%p Type=%X\n", walk_state, op,
			  object_type));

	switch (walk_state->opcode) {
	case AML_FIELD_OP:
	case AML_BANK_FIELD_OP:
	case AML_INDEX_FIELD_OP:

		node = NULL;
		status = AE_OK;
		break;

	case AML_INT_NAMEPATH_OP:
		status =
		    acpi_ns_lookup(walk_state->scope_info, buffer_ptr,
				   object_type, ACPI_IMODE_EXECUTE,
				   ACPI_NS_SEARCH_PARENT, walk_state, &(node));
		break;

	case AML_SCOPE_OP:

		

		if (op && (op->named.node == acpi_gbl_root_node)) {
			node = op->named.node;

			status =
			    acpi_ds_scope_stack_push(node, object_type,
						     walk_state);
			if (ACPI_FAILURE(status)) {
				return_ACPI_STATUS(status);
			}
		} else {
			status =
			    acpi_ns_lookup(walk_state->scope_info, buffer_ptr,
					   object_type, ACPI_IMODE_EXECUTE,
					   ACPI_NS_SEARCH_PARENT, walk_state,
					   &(node));
			if (ACPI_FAILURE(status)) {
#ifdef ACPI_ASL_COMPILER
				if (status == AE_NOT_FOUND) {
					status = AE_OK;
				} else {
					ACPI_ERROR_NAMESPACE(buffer_ptr,
							     status);
				}
#else
				ACPI_ERROR_NAMESPACE(buffer_ptr, status);
#endif
				return_ACPI_STATUS(status);
			}
		}

		switch (node->type) {
		case ACPI_TYPE_ANY:
		case ACPI_TYPE_LOCAL_SCOPE:	
		case ACPI_TYPE_DEVICE:
		case ACPI_TYPE_POWER:
		case ACPI_TYPE_PROCESSOR:
		case ACPI_TYPE_THERMAL:

			
			break;

		case ACPI_TYPE_INTEGER:
		case ACPI_TYPE_STRING:
		case ACPI_TYPE_BUFFER:

			ACPI_WARNING((AE_INFO,
				      "Type override - [%4.4s] had invalid type (%s) "
				      "for Scope operator, changed to type ANY\n",
				      acpi_ut_get_node_name(node),
				      acpi_ut_get_type_name(node->type)));

			node->type = ACPI_TYPE_ANY;
			walk_state->scope_info->common.value = ACPI_TYPE_ANY;
			break;

		default:

			

			ACPI_ERROR((AE_INFO,
				    "Invalid type (%s) for target of "
				    "Scope operator [%4.4s] (Cannot override)",
				    acpi_ut_get_type_name(node->type),
				    acpi_ut_get_node_name(node)));

			return (AE_AML_OPERAND_TYPE);
		}
		break;

	default:

		

		if (op && op->common.node) {

			

			node = op->common.node;

			if (acpi_ns_opens_scope(object_type)) {
				status =
				    acpi_ds_scope_stack_push(node, object_type,
							     walk_state);
				if (ACPI_FAILURE(status)) {
					return_ACPI_STATUS(status);
				}
			}

			return_ACPI_STATUS(AE_OK);
		}

		if (walk_state->deferred_node) {

			

			node = walk_state->deferred_node;
			status = AE_OK;
			break;
		}

		flags = ACPI_NS_NO_UPSEARCH;
		if (walk_state->pass_number == ACPI_IMODE_EXECUTE) {

			

			flags |= ACPI_NS_ERROR_IF_FOUND;

			if (!
			    (walk_state->
			     parse_flags & ACPI_PARSE_MODULE_LEVEL)) {
				flags |= ACPI_NS_TEMPORARY;
			}
		}

		

		status =
		    acpi_ns_lookup(walk_state->scope_info, buffer_ptr,
				   object_type, ACPI_IMODE_LOAD_PASS2, flags,
				   walk_state, &node);

		if (ACPI_SUCCESS(status) && (flags & ACPI_NS_TEMPORARY)) {
			ACPI_DEBUG_PRINT((ACPI_DB_DISPATCH,
					  "***New Node [%4.4s] %p is temporary\n",
					  acpi_ut_get_node_name(node), node));
		}
		break;
	}

	if (ACPI_FAILURE(status)) {
		ACPI_ERROR_NAMESPACE(buffer_ptr, status);
		return_ACPI_STATUS(status);
	}

	if (!op) {

		

		op = acpi_ps_alloc_op(walk_state->opcode);
		if (!op) {
			return_ACPI_STATUS(AE_NO_MEMORY);
		}

		

		if (node) {
			op->named.name = node->name.integer;
		}
		*out_op = op;
	}

	op->common.node = node;
	return_ACPI_STATUS(status);
}


acpi_status acpi_ds_load2_end_op(struct acpi_walk_state *walk_state)
{
	union acpi_parse_object *op;
	acpi_status status = AE_OK;
	acpi_object_type object_type;
	struct acpi_namespace_node *node;
	union acpi_parse_object *arg;
	struct acpi_namespace_node *new_node;
#ifndef ACPI_NO_METHOD_EXECUTION
	u32 i;
	u8 region_space;
#endif

	ACPI_FUNCTION_TRACE(ds_load2_end_op);

	op = walk_state->op;
	ACPI_DEBUG_PRINT((ACPI_DB_DISPATCH, "Opcode [%s] Op %p State %p\n",
			  walk_state->op_info->name, op, walk_state));

	

	if (!(walk_state->op_info->flags & AML_NSOBJECT)) {
		return_ACPI_STATUS(AE_OK);
	}

	if (op->common.aml_opcode == AML_SCOPE_OP) {
		ACPI_DEBUG_PRINT((ACPI_DB_DISPATCH,
				  "Ending scope Op=%p State=%p\n", op,
				  walk_state));
	}

	object_type = walk_state->op_info->object_type;

	node = op->common.node;

	walk_state->operands[0] = (void *)node;
	walk_state->num_operands = 1;

	

	if (acpi_ns_opens_scope(object_type) &&
	    (op->common.aml_opcode != AML_INT_METHODCALL_OP)) {
		ACPI_DEBUG_PRINT((ACPI_DB_DISPATCH,
				  "(%s) Popping scope for Op %p\n",
				  acpi_ut_get_type_name(object_type), op));

		status = acpi_ds_scope_stack_pop(walk_state);
		if (ACPI_FAILURE(status)) {
			goto cleanup;
		}
	}


	ACPI_DEBUG_PRINT((ACPI_DB_DISPATCH,
			  "Create-Load [%s] State=%p Op=%p NamedObj=%p\n",
			  acpi_ps_get_opcode_name(op->common.aml_opcode),
			  walk_state, op, node));

	

	arg = op->common.value.arg;

	switch (walk_state->op_info->type) {
#ifndef ACPI_NO_METHOD_EXECUTION

	case AML_TYPE_CREATE_FIELD:
		status = acpi_ds_create_buffer_field(op, walk_state);
		break;

	case AML_TYPE_NAMED_FIELD:
		if (walk_state->method_node) {
			status = acpi_ds_init_field_objects(op, walk_state);
		}

		switch (op->common.aml_opcode) {
		case AML_INDEX_FIELD_OP:

			status =
			    acpi_ds_create_index_field(op,
						       (acpi_handle) arg->
						       common.node, walk_state);
			break;

		case AML_BANK_FIELD_OP:

			status =
			    acpi_ds_create_bank_field(op, arg->common.node,
						      walk_state);
			break;

		case AML_FIELD_OP:

			status =
			    acpi_ds_create_field(op, arg->common.node,
						 walk_state);
			break;

		default:
			
			break;
		}
		break;

	case AML_TYPE_NAMED_SIMPLE:

		status = acpi_ds_create_operands(walk_state, arg);
		if (ACPI_FAILURE(status)) {
			goto cleanup;
		}

		switch (op->common.aml_opcode) {
		case AML_PROCESSOR_OP:

			status = acpi_ex_create_processor(walk_state);
			break;

		case AML_POWER_RES_OP:

			status = acpi_ex_create_power_resource(walk_state);
			break;

		case AML_MUTEX_OP:

			status = acpi_ex_create_mutex(walk_state);
			break;

		case AML_EVENT_OP:

			status = acpi_ex_create_event(walk_state);
			break;

		case AML_ALIAS_OP:

			status = acpi_ex_create_alias(walk_state);
			break;

		default:
			

			status = AE_OK;
			goto cleanup;
		}

		

		for (i = 1; i < walk_state->num_operands; i++) {
			acpi_ut_remove_reference(walk_state->operands[i]);
			walk_state->operands[i] = NULL;
		}

		break;
#endif				

	case AML_TYPE_NAMED_COMPLEX:

		switch (op->common.aml_opcode) {
#ifndef ACPI_NO_METHOD_EXECUTION
		case AML_REGION_OP:
		case AML_DATA_REGION_OP:

			if (op->common.aml_opcode == AML_REGION_OP) {
				region_space = (acpi_adr_space_type)
				    ((op->common.value.arg)->common.value.
				     integer);
			} else {
				region_space = ACPI_ADR_SPACE_DATA_TABLE;
			}

			if (walk_state->method_node) {
				status =
				    acpi_ex_create_region(op->named.data,
							  op->named.length,
							  region_space,
							  walk_state);
				if (ACPI_FAILURE(status)) {
					return (status);
				}

				acpi_ex_exit_interpreter();
			}

			status =
			    acpi_ev_initialize_region
			    (acpi_ns_get_attached_object(node), FALSE);
			if (walk_state->method_node) {
				acpi_ex_enter_interpreter();
			}

			if (ACPI_FAILURE(status)) {
				if (AE_NOT_EXIST == status) {
					status = AE_OK;
				}
			}
			break;

		case AML_NAME_OP:

			status = acpi_ds_create_node(walk_state, node, op);
			break;

		case AML_METHOD_OP:
			ACPI_DEBUG_PRINT((ACPI_DB_DISPATCH,
					  "LOADING-Method: State=%p Op=%p NamedObj=%p\n",
					  walk_state, op, op->named.node));

			if (!acpi_ns_get_attached_object(op->named.node)) {
				walk_state->operands[0] =
				    ACPI_CAST_PTR(void, op->named.node);
				walk_state->num_operands = 1;

				status =
				    acpi_ds_create_operands(walk_state,
							    op->common.value.
							    arg);
				if (ACPI_SUCCESS(status)) {
					status =
					    acpi_ex_create_method(op->named.
								  data,
								  op->named.
								  length,
								  walk_state);
				}
				walk_state->operands[0] = NULL;
				walk_state->num_operands = 0;

				if (ACPI_FAILURE(status)) {
					return_ACPI_STATUS(status);
				}
			}
			break;

#endif				

		default:
			
			break;
		}
		break;

	case AML_CLASS_INTERNAL:

		
		break;

	case AML_CLASS_METHOD_CALL:

		ACPI_DEBUG_PRINT((ACPI_DB_DISPATCH,
				  "RESOLVING-MethodCall: State=%p Op=%p NamedObj=%p\n",
				  walk_state, op, node));

		status =
		    acpi_ns_lookup(walk_state->scope_info,
				   arg->common.value.string, ACPI_TYPE_ANY,
				   ACPI_IMODE_LOAD_PASS2,
				   ACPI_NS_SEARCH_PARENT |
				   ACPI_NS_DONT_OPEN_SCOPE, walk_state,
				   &(new_node));
		if (ACPI_SUCCESS(status)) {
			if (new_node->type != ACPI_TYPE_METHOD) {
				status = AE_AML_OPERAND_TYPE;
			}

			op->common.node = new_node;
		} else {
			ACPI_ERROR_NAMESPACE(arg->common.value.string, status);
		}
		break;

	default:
		break;
	}

      cleanup:

	

	walk_state->operands[0] = NULL;
	walk_state->num_operands = 0;
	return_ACPI_STATUS(status);
}
