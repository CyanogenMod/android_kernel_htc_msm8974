
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
#include "acdebug.h"

#define _COMPONENT          ACPI_DISPATCHER
ACPI_MODULE_NAME("dswexec")

static ACPI_EXECUTE_OP acpi_gbl_op_type_dispatch[] = {
	acpi_ex_opcode_0A_0T_1R,
	acpi_ex_opcode_1A_0T_0R,
	acpi_ex_opcode_1A_0T_1R,
	acpi_ex_opcode_1A_1T_0R,
	acpi_ex_opcode_1A_1T_1R,
	acpi_ex_opcode_2A_0T_0R,
	acpi_ex_opcode_2A_0T_1R,
	acpi_ex_opcode_2A_1T_1R,
	acpi_ex_opcode_2A_2T_1R,
	acpi_ex_opcode_3A_0T_0R,
	acpi_ex_opcode_3A_1T_1R,
	acpi_ex_opcode_6A_0T_1R
};


acpi_status
acpi_ds_get_predicate_value(struct acpi_walk_state *walk_state,
			    union acpi_operand_object *result_obj)
{
	acpi_status status = AE_OK;
	union acpi_operand_object *obj_desc;
	union acpi_operand_object *local_obj_desc = NULL;

	ACPI_FUNCTION_TRACE_PTR(ds_get_predicate_value, walk_state);

	walk_state->control_state->common.state = 0;

	if (result_obj) {
		status = acpi_ds_result_pop(&obj_desc, walk_state);
		if (ACPI_FAILURE(status)) {
			ACPI_EXCEPTION((AE_INFO, status,
					"Could not get result from predicate evaluation"));

			return_ACPI_STATUS(status);
		}
	} else {
		status = acpi_ds_create_operand(walk_state, walk_state->op, 0);
		if (ACPI_FAILURE(status)) {
			return_ACPI_STATUS(status);
		}

		status =
		    acpi_ex_resolve_to_value(&walk_state->operands[0],
					     walk_state);
		if (ACPI_FAILURE(status)) {
			return_ACPI_STATUS(status);
		}

		obj_desc = walk_state->operands[0];
	}

	if (!obj_desc) {
		ACPI_ERROR((AE_INFO,
			    "No predicate ObjDesc=%p State=%p",
			    obj_desc, walk_state));

		return_ACPI_STATUS(AE_AML_NO_OPERAND);
	}

	status = acpi_ex_convert_to_integer(obj_desc, &local_obj_desc, 16);
	if (ACPI_FAILURE(status)) {
		goto cleanup;
	}

	if (local_obj_desc->common.type != ACPI_TYPE_INTEGER) {
		ACPI_ERROR((AE_INFO,
			    "Bad predicate (not an integer) ObjDesc=%p State=%p Type=0x%X",
			    obj_desc, walk_state, obj_desc->common.type));

		status = AE_AML_OPERAND_TYPE;
		goto cleanup;
	}

	

	acpi_ex_truncate_for32bit_table(local_obj_desc);

	if (local_obj_desc->integer.value) {
		walk_state->control_state->common.value = TRUE;
	} else {
		walk_state->control_state->common.value = FALSE;
		status = AE_CTRL_FALSE;
	}

	

	(void)acpi_ds_do_implicit_return(local_obj_desc, walk_state, TRUE);

      cleanup:

	ACPI_DEBUG_PRINT((ACPI_DB_EXEC, "Completed a predicate eval=%X Op=%p\n",
			  walk_state->control_state->common.value,
			  walk_state->op));

	

	ACPI_DEBUGGER_EXEC(acpi_db_display_result_object
			   (local_obj_desc, walk_state));

	if (local_obj_desc != obj_desc) {
		acpi_ut_remove_reference(local_obj_desc);
	}
	acpi_ut_remove_reference(obj_desc);

	walk_state->control_state->common.state = ACPI_CONTROL_NORMAL;
	return_ACPI_STATUS(status);
}


acpi_status
acpi_ds_exec_begin_op(struct acpi_walk_state *walk_state,
		      union acpi_parse_object **out_op)
{
	union acpi_parse_object *op;
	acpi_status status = AE_OK;
	u32 opcode_class;

	ACPI_FUNCTION_TRACE_PTR(ds_exec_begin_op, walk_state);

	op = walk_state->op;
	if (!op) {
		status = acpi_ds_load2_begin_op(walk_state, out_op);
		if (ACPI_FAILURE(status)) {
			goto error_exit;
		}

		op = *out_op;
		walk_state->op = op;
		walk_state->opcode = op->common.aml_opcode;
		walk_state->op_info =
		    acpi_ps_get_opcode_info(op->common.aml_opcode);

		if (acpi_ns_opens_scope(walk_state->op_info->object_type)) {
			ACPI_DEBUG_PRINT((ACPI_DB_DISPATCH,
					  "(%s) Popping scope for Op %p\n",
					  acpi_ut_get_type_name(walk_state->
								op_info->
								object_type),
					  op));

			status = acpi_ds_scope_stack_pop(walk_state);
			if (ACPI_FAILURE(status)) {
				goto error_exit;
			}
		}
	}

	if (op == walk_state->origin) {
		if (out_op) {
			*out_op = op;
		}

		return_ACPI_STATUS(AE_OK);
	}

	if ((walk_state->control_state) &&
	    (walk_state->control_state->common.state ==
	     ACPI_CONTROL_CONDITIONAL_EXECUTING)) {
		ACPI_DEBUG_PRINT((ACPI_DB_EXEC,
				  "Exec predicate Op=%p State=%p\n", op,
				  walk_state));

		walk_state->control_state->common.state =
		    ACPI_CONTROL_PREDICATE_EXECUTING;

		

		walk_state->control_state->control.predicate_op = op;
	}

	opcode_class = walk_state->op_info->class;

	

	if (op->common.aml_opcode == AML_INT_NAMEPATH_OP) {
		opcode_class = AML_CLASS_NAMED_OBJECT;
	}

	switch (opcode_class) {
	case AML_CLASS_CONTROL:

		status = acpi_ds_exec_begin_control_op(walk_state, op);
		break;

	case AML_CLASS_NAMED_OBJECT:

		if (walk_state->walk_type & ACPI_WALK_METHOD) {
			if (op->common.aml_opcode != AML_SCOPE_OP) {
				status =
				    acpi_ds_load2_begin_op(walk_state, NULL);
			} else {
				status =
				    acpi_ds_scope_stack_push(op->named.node,
							     op->named.node->
							     type, walk_state);
				if (ACPI_FAILURE(status)) {
					return_ACPI_STATUS(status);
				}
			}
		}
		break;

	case AML_CLASS_EXECUTE:
	case AML_CLASS_CREATE:

		break;

	default:
		break;
	}

	

	return_ACPI_STATUS(status);

      error_exit:
	status = acpi_ds_method_error(status, walk_state);
	return_ACPI_STATUS(status);
}


acpi_status acpi_ds_exec_end_op(struct acpi_walk_state *walk_state)
{
	union acpi_parse_object *op;
	acpi_status status = AE_OK;
	u32 op_type;
	u32 op_class;
	union acpi_parse_object *next_op;
	union acpi_parse_object *first_arg;

	ACPI_FUNCTION_TRACE_PTR(ds_exec_end_op, walk_state);

	op = walk_state->op;
	op_type = walk_state->op_info->type;
	op_class = walk_state->op_info->class;

	if (op_class == AML_CLASS_UNKNOWN) {
		ACPI_ERROR((AE_INFO, "Unknown opcode 0x%X",
			    op->common.aml_opcode));
		return_ACPI_STATUS(AE_NOT_IMPLEMENTED);
	}

	first_arg = op->common.value.arg;

	

	walk_state->num_operands = 0;
	walk_state->operand_index = 0;
	walk_state->return_desc = NULL;
	walk_state->result_obj = NULL;

	

	ACPI_DEBUGGER_EXEC(status =
			   acpi_db_single_step(walk_state, op, op_class));
	ACPI_DEBUGGER_EXEC(if (ACPI_FAILURE(status)) {
			   return_ACPI_STATUS(status);}
	) ;

	

	switch (op_class) {
	case AML_CLASS_ARGUMENT:	

		if (walk_state->opcode == AML_INT_NAMEPATH_OP) {
			status = acpi_ds_evaluate_name_path(walk_state);
			if (ACPI_FAILURE(status)) {
				goto cleanup;
			}
		}
		break;

	case AML_CLASS_EXECUTE:	

		

		status = acpi_ds_create_operands(walk_state, first_arg);
		if (ACPI_FAILURE(status)) {
			goto cleanup;
		}

		if (!(walk_state->op_info->flags & AML_NO_OPERAND_RESOLVE)) {

			

			status = acpi_ex_resolve_operands(walk_state->opcode,
							  &(walk_state->
							    operands
							    [walk_state->
							     num_operands - 1]),
							  walk_state);
		}

		if (ACPI_SUCCESS(status)) {
			status =
			    acpi_gbl_op_type_dispatch[op_type] (walk_state);
		} else {
			if ((status == AE_AML_UNINITIALIZED_LOCAL) &&
			    (walk_state->opcode == AML_STORE_OP) &&
			    (walk_state->operands[0]->common.type ==
			     ACPI_TYPE_LOCAL_REFERENCE)
			    && (walk_state->operands[1]->common.type ==
				ACPI_TYPE_LOCAL_REFERENCE)
			    && (walk_state->operands[0]->reference.class ==
				walk_state->operands[1]->reference.class)
			    && (walk_state->operands[0]->reference.value ==
				walk_state->operands[1]->reference.value)) {
				status = AE_OK;
			} else {
				ACPI_EXCEPTION((AE_INFO, status,
						"While resolving operands for [%s]",
						acpi_ps_get_opcode_name
						(walk_state->opcode)));
			}
		}

		

		acpi_ds_clear_operands(walk_state);

		if (ACPI_SUCCESS(status) && walk_state->result_obj) {
			status =
			    acpi_ds_result_push(walk_state->result_obj,
						walk_state);
		}
		break;

	default:

		switch (op_type) {
		case AML_TYPE_CONTROL:	

			

			status = acpi_ds_exec_end_control_op(walk_state, op);

			break;

		case AML_TYPE_METHOD_CALL:

			if ((op->asl.parent) &&
			    ((op->asl.parent->asl.aml_opcode == AML_PACKAGE_OP)
			     || (op->asl.parent->asl.aml_opcode ==
				 AML_VAR_PACKAGE_OP))) {
				ACPI_DEBUG_PRINT((ACPI_DB_DISPATCH,
						  "Method Reference in a Package, Op=%p\n",
						  op));

				op->common.node =
				    (struct acpi_namespace_node *)op->asl.value.
				    arg->asl.node;
				acpi_ut_add_reference(op->asl.value.arg->asl.
						      node->object);
				return_ACPI_STATUS(AE_OK);
			}

			ACPI_DEBUG_PRINT((ACPI_DB_DISPATCH,
					  "Method invocation, Op=%p\n", op));

			

			next_op = first_arg;

			

			next_op = next_op->common.next;

			status = acpi_ds_create_operands(walk_state, next_op);
			if (ACPI_FAILURE(status)) {
				break;
			}

			status = acpi_ds_resolve_operands(walk_state);
			if (ACPI_FAILURE(status)) {

				

				acpi_ds_clear_operands(walk_state);
				break;
			}

			status = AE_CTRL_TRANSFER;

			return_ACPI_STATUS(status);

		case AML_TYPE_CREATE_FIELD:

			ACPI_DEBUG_PRINT((ACPI_DB_EXEC,
					  "Executing CreateField Buffer/Index Op=%p\n",
					  op));

			status = acpi_ds_load2_end_op(walk_state);
			if (ACPI_FAILURE(status)) {
				break;
			}

			status =
			    acpi_ds_eval_buffer_field_operands(walk_state, op);
			break;

		case AML_TYPE_CREATE_OBJECT:

			ACPI_DEBUG_PRINT((ACPI_DB_EXEC,
					  "Executing CreateObject (Buffer/Package) Op=%p\n",
					  op));

			switch (op->common.parent->common.aml_opcode) {
			case AML_NAME_OP:

				walk_state->operands[0] =
				    (void *)op->common.parent->common.node;
				walk_state->num_operands = 1;

				status = acpi_ds_create_node(walk_state,
							     op->common.parent->
							     common.node,
							     op->common.parent);
				if (ACPI_FAILURE(status)) {
					break;
				}

				
				

			case AML_INT_EVAL_SUBTREE_OP:

				status =
				    acpi_ds_eval_data_object_operands
				    (walk_state, op,
				     acpi_ns_get_attached_object(op->common.
								 parent->common.
								 node));
				break;

			default:

				status =
				    acpi_ds_eval_data_object_operands
				    (walk_state, op, NULL);
				break;
			}

			if (walk_state->result_obj) {
				status =
				    acpi_ds_result_push(walk_state->result_obj,
							walk_state);
			}
			break;

		case AML_TYPE_NAMED_FIELD:
		case AML_TYPE_NAMED_COMPLEX:
		case AML_TYPE_NAMED_SIMPLE:
		case AML_TYPE_NAMED_NO_OBJ:

			status = acpi_ds_load2_end_op(walk_state);
			if (ACPI_FAILURE(status)) {
				break;
			}

			if (op->common.aml_opcode == AML_REGION_OP) {
				ACPI_DEBUG_PRINT((ACPI_DB_EXEC,
						  "Executing OpRegion Address/Length Op=%p\n",
						  op));

				status =
				    acpi_ds_eval_region_operands(walk_state,
								 op);
				if (ACPI_FAILURE(status)) {
					break;
				}
			} else if (op->common.aml_opcode == AML_DATA_REGION_OP) {
				ACPI_DEBUG_PRINT((ACPI_DB_EXEC,
						  "Executing DataTableRegion Strings Op=%p\n",
						  op));

				status =
				    acpi_ds_eval_table_region_operands
				    (walk_state, op);
				if (ACPI_FAILURE(status)) {
					break;
				}
			} else if (op->common.aml_opcode == AML_BANK_FIELD_OP) {
				ACPI_DEBUG_PRINT((ACPI_DB_EXEC,
						  "Executing BankField Op=%p\n",
						  op));

				status =
				    acpi_ds_eval_bank_field_operands(walk_state,
								     op);
				if (ACPI_FAILURE(status)) {
					break;
				}
			}
			break;

		case AML_TYPE_UNDEFINED:

			ACPI_ERROR((AE_INFO,
				    "Undefined opcode type Op=%p", op));
			return_ACPI_STATUS(AE_NOT_IMPLEMENTED);

		case AML_TYPE_BOGUS:

			ACPI_DEBUG_PRINT((ACPI_DB_DISPATCH,
					  "Internal opcode=%X type Op=%p\n",
					  walk_state->opcode, op));
			break;

		default:

			ACPI_ERROR((AE_INFO,
				    "Unimplemented opcode, class=0x%X type=0x%X Opcode=-0x%X Op=%p",
				    op_class, op_type, op->common.aml_opcode,
				    op));

			status = AE_NOT_IMPLEMENTED;
			break;
		}
	}

	acpi_ex_truncate_for32bit_table(walk_state->result_obj);

	if ((ACPI_SUCCESS(status)) &&
	    (walk_state->control_state) &&
	    (walk_state->control_state->common.state ==
	     ACPI_CONTROL_PREDICATE_EXECUTING) &&
	    (walk_state->control_state->control.predicate_op == op)) {
		status =
		    acpi_ds_get_predicate_value(walk_state,
						walk_state->result_obj);
		walk_state->result_obj = NULL;
	}

      cleanup:

	if (walk_state->result_obj) {

		

		ACPI_DEBUGGER_EXEC(acpi_db_display_result_object
				   (walk_state->result_obj, walk_state));

		acpi_ds_delete_result_if_not_used(op, walk_state->result_obj,
						  walk_state);
	}
#ifdef _UNDER_DEVELOPMENT

	if (walk_state->parser_state.aml == walk_state->parser_state.aml_end) {
		acpi_db_method_end(walk_state);
	}
#endif

	

	if (ACPI_FAILURE(status)) {
		status = acpi_ds_method_error(status, walk_state);
	}

	

	walk_state->num_operands = 0;
	return_ACPI_STATUS(status);
}
