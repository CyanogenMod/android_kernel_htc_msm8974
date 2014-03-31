
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
#include "acdispat.h"
#include "amlcode.h"

#define _COMPONENT          ACPI_PARSER
ACPI_MODULE_NAME("psloop")

static u32 acpi_gbl_depth = 0;


static acpi_status acpi_ps_get_aml_opcode(struct acpi_walk_state *walk_state);

static acpi_status
acpi_ps_build_named_op(struct acpi_walk_state *walk_state,
		       u8 * aml_op_start,
		       union acpi_parse_object *unnamed_op,
		       union acpi_parse_object **op);

static acpi_status
acpi_ps_create_op(struct acpi_walk_state *walk_state,
		  u8 * aml_op_start, union acpi_parse_object **new_op);

static acpi_status
acpi_ps_get_arguments(struct acpi_walk_state *walk_state,
		      u8 * aml_op_start, union acpi_parse_object *op);

static acpi_status
acpi_ps_complete_op(struct acpi_walk_state *walk_state,
		    union acpi_parse_object **op, acpi_status status);

static acpi_status
acpi_ps_complete_final_op(struct acpi_walk_state *walk_state,
			  union acpi_parse_object *op, acpi_status status);

static void
acpi_ps_link_module_code(union acpi_parse_object *parent_op,
			 u8 *aml_start, u32 aml_length, acpi_owner_id owner_id);


static acpi_status acpi_ps_get_aml_opcode(struct acpi_walk_state *walk_state)
{

	ACPI_FUNCTION_TRACE_PTR(ps_get_aml_opcode, walk_state);

	walk_state->aml_offset =
	    (u32) ACPI_PTR_DIFF(walk_state->parser_state.aml,
				walk_state->parser_state.aml_start);
	walk_state->opcode = acpi_ps_peek_opcode(&(walk_state->parser_state));

	walk_state->op_info = acpi_ps_get_opcode_info(walk_state->opcode);

	switch (walk_state->op_info->class) {
	case AML_CLASS_ASCII:
	case AML_CLASS_PREFIX:
		walk_state->opcode = AML_INT_NAMEPATH_OP;
		walk_state->arg_types = ARGP_NAMESTRING;
		break;

	case AML_CLASS_UNKNOWN:

		

		ACPI_ERROR((AE_INFO,
			    "Found unknown opcode 0x%X at AML address %p offset 0x%X, ignoring",
			    walk_state->opcode, walk_state->parser_state.aml,
			    walk_state->aml_offset));

		ACPI_DUMP_BUFFER(walk_state->parser_state.aml, 128);

		

		walk_state->parser_state.aml++;
		return_ACPI_STATUS(AE_CTRL_PARSE_CONTINUE);

	default:

		

		walk_state->parser_state.aml +=
		    acpi_ps_get_opcode_size(walk_state->opcode);
		walk_state->arg_types = walk_state->op_info->parse_args;
		break;
	}

	return_ACPI_STATUS(AE_OK);
}


static acpi_status
acpi_ps_build_named_op(struct acpi_walk_state *walk_state,
		       u8 * aml_op_start,
		       union acpi_parse_object *unnamed_op,
		       union acpi_parse_object **op)
{
	acpi_status status = AE_OK;
	union acpi_parse_object *arg = NULL;

	ACPI_FUNCTION_TRACE_PTR(ps_build_named_op, walk_state);

	unnamed_op->common.value.arg = NULL;
	unnamed_op->common.arg_list_length = 0;
	unnamed_op->common.aml_opcode = walk_state->opcode;

	while (GET_CURRENT_ARG_TYPE(walk_state->arg_types) &&
	       (GET_CURRENT_ARG_TYPE(walk_state->arg_types) != ARGP_NAME)) {
		status =
		    acpi_ps_get_next_arg(walk_state,
					 &(walk_state->parser_state),
					 GET_CURRENT_ARG_TYPE(walk_state->
							      arg_types), &arg);
		if (ACPI_FAILURE(status)) {
			return_ACPI_STATUS(status);
		}

		acpi_ps_append_arg(unnamed_op, arg);
		INCREMENT_ARG_LIST(walk_state->arg_types);
	}

	if (!GET_CURRENT_ARG_TYPE(walk_state->arg_types)) {
		return_ACPI_STATUS(AE_AML_NO_OPERAND);
	}

	

	INCREMENT_ARG_LIST(walk_state->arg_types);

	walk_state->op = NULL;

	status = walk_state->descending_callback(walk_state, op);
	if (ACPI_FAILURE(status)) {
		ACPI_EXCEPTION((AE_INFO, status, "During name lookup/catalog"));
		return_ACPI_STATUS(status);
	}

	if (!*op) {
		return_ACPI_STATUS(AE_CTRL_PARSE_CONTINUE);
	}

	status = acpi_ps_next_parse_state(walk_state, *op, status);
	if (ACPI_FAILURE(status)) {
		if (status == AE_CTRL_PENDING) {
			return_ACPI_STATUS(AE_CTRL_PARSE_PENDING);
		}
		return_ACPI_STATUS(status);
	}

	acpi_ps_append_arg(*op, unnamed_op->common.value.arg);
	acpi_gbl_depth++;

	if ((*op)->common.aml_opcode == AML_REGION_OP ||
	    (*op)->common.aml_opcode == AML_DATA_REGION_OP) {
		(*op)->named.data = aml_op_start;
		(*op)->named.length = 0;
	}

	return_ACPI_STATUS(AE_OK);
}


static acpi_status
acpi_ps_create_op(struct acpi_walk_state *walk_state,
		  u8 * aml_op_start, union acpi_parse_object **new_op)
{
	acpi_status status = AE_OK;
	union acpi_parse_object *op;
	union acpi_parse_object *named_op = NULL;
	union acpi_parse_object *parent_scope;
	u8 argument_count;
	const struct acpi_opcode_info *op_info;

	ACPI_FUNCTION_TRACE_PTR(ps_create_op, walk_state);

	status = acpi_ps_get_aml_opcode(walk_state);
	if (status == AE_CTRL_PARSE_CONTINUE) {
		return_ACPI_STATUS(AE_CTRL_PARSE_CONTINUE);
	}

	

	walk_state->op_info = acpi_ps_get_opcode_info(walk_state->opcode);
	op = acpi_ps_alloc_op(walk_state->opcode);
	if (!op) {
		return_ACPI_STATUS(AE_NO_MEMORY);
	}

	if (walk_state->op_info->flags & AML_NAMED) {
		status =
		    acpi_ps_build_named_op(walk_state, aml_op_start, op,
					   &named_op);
		acpi_ps_free_op(op);
		if (ACPI_FAILURE(status)) {
			return_ACPI_STATUS(status);
		}

		*new_op = named_op;
		return_ACPI_STATUS(AE_OK);
	}

	

	if (walk_state->op_info->flags & AML_CREATE) {
		op->named.data = aml_op_start;
		op->named.length = 0;
	}

	if (walk_state->opcode == AML_BANK_FIELD_OP) {
		op->named.data = aml_op_start;
		op->named.length = 0;
	}

	parent_scope = acpi_ps_get_parent_scope(&(walk_state->parser_state));
	acpi_ps_append_arg(parent_scope, op);

	if (parent_scope) {
		op_info =
		    acpi_ps_get_opcode_info(parent_scope->common.aml_opcode);
		if (op_info->flags & AML_HAS_TARGET) {
			argument_count =
			    acpi_ps_get_argument_count(op_info->type);
			if (parent_scope->common.arg_list_length >
			    argument_count) {
				op->common.flags |= ACPI_PARSEOP_TARGET;
			}
		} else if (parent_scope->common.aml_opcode == AML_INCREMENT_OP) {
			op->common.flags |= ACPI_PARSEOP_TARGET;
		}
	}

	if (walk_state->descending_callback != NULL) {
		walk_state->op = *new_op = op;

		status = walk_state->descending_callback(walk_state, &op);
		status = acpi_ps_next_parse_state(walk_state, op, status);
		if (status == AE_CTRL_PENDING) {
			status = AE_CTRL_PARSE_PENDING;
		}
	}

	return_ACPI_STATUS(status);
}


static acpi_status
acpi_ps_get_arguments(struct acpi_walk_state *walk_state,
		      u8 * aml_op_start, union acpi_parse_object *op)
{
	acpi_status status = AE_OK;
	union acpi_parse_object *arg = NULL;
	const struct acpi_opcode_info *op_info;

	ACPI_FUNCTION_TRACE_PTR(ps_get_arguments, walk_state);

	switch (op->common.aml_opcode) {
	case AML_BYTE_OP:	
	case AML_WORD_OP:	
	case AML_DWORD_OP:	
	case AML_QWORD_OP:	
	case AML_STRING_OP:	

		

		acpi_ps_get_next_simple_arg(&(walk_state->parser_state),
					    GET_CURRENT_ARG_TYPE(walk_state->
								 arg_types),
					    op);
		break;

	case AML_INT_NAMEPATH_OP:	

		status =
		    acpi_ps_get_next_namepath(walk_state,
					      &(walk_state->parser_state), op,
					      1);
		if (ACPI_FAILURE(status)) {
			return_ACPI_STATUS(status);
		}

		walk_state->arg_types = 0;
		break;

	default:
		while (GET_CURRENT_ARG_TYPE(walk_state->arg_types)
		       && !walk_state->arg_count) {
			walk_state->aml_offset =
			    (u32) ACPI_PTR_DIFF(walk_state->parser_state.aml,
						walk_state->parser_state.
						aml_start);

			status =
			    acpi_ps_get_next_arg(walk_state,
						 &(walk_state->parser_state),
						 GET_CURRENT_ARG_TYPE
						 (walk_state->arg_types), &arg);
			if (ACPI_FAILURE(status)) {
				return_ACPI_STATUS(status);
			}

			if (arg) {
				arg->common.aml_offset = walk_state->aml_offset;
				acpi_ps_append_arg(op, arg);
			}

			INCREMENT_ARG_LIST(walk_state->arg_types);
		}

		if ((walk_state->pass_number <= ACPI_IMODE_LOAD_PASS2) &&
		    ((walk_state->parse_flags & ACPI_PARSE_DISASSEMBLE) == 0)) {
			switch (op->common.aml_opcode) {
			case AML_IF_OP:
			case AML_ELSE_OP:
			case AML_WHILE_OP:

				if (walk_state->pass_number ==
				    ACPI_IMODE_LOAD_PASS1) {
					acpi_ps_link_module_code(op->common.
								 parent,
								 aml_op_start,
								 (u32)
								 (walk_state->
								 parser_state.
								 pkg_end -
								 aml_op_start),
								 walk_state->
								 owner_id);
				}

				ACPI_DEBUG_PRINT((ACPI_DB_PARSE,
						  "Pass1: Skipping an If/Else/While body\n"));

				

				walk_state->parser_state.aml =
				    walk_state->parser_state.pkg_end;
				walk_state->arg_count = 0;
				break;

			default:
				if ((walk_state->pass_number ==
				     ACPI_IMODE_LOAD_PASS1)
				    && (op->common.parent->common.aml_opcode ==
					AML_SCOPE_OP)) {
					op_info =
					    acpi_ps_get_opcode_info(op->common.
								    aml_opcode);
					if ((op_info->class ==
					     AML_CLASS_EXECUTE) && (!arg)) {
						ACPI_WARNING((AE_INFO,
							      "Detected an unsupported executable opcode "
							      "at module-level: [0x%.4X] at table offset 0x%.4X",
							      op->common.aml_opcode,
							      (u32)((aml_op_start - walk_state->parser_state.aml_start)
								+ sizeof(struct acpi_table_header))));
					}
				}
				break;
			}
		}

		

		switch (op->common.aml_opcode) {
		case AML_METHOD_OP:
			op->named.data = walk_state->parser_state.aml;
			op->named.length = (u32)
			    (walk_state->parser_state.pkg_end -
			     walk_state->parser_state.aml);

			

			walk_state->parser_state.aml =
			    walk_state->parser_state.pkg_end;
			walk_state->arg_count = 0;
			break;

		case AML_BUFFER_OP:
		case AML_PACKAGE_OP:
		case AML_VAR_PACKAGE_OP:

			if ((op->common.parent) &&
			    (op->common.parent->common.aml_opcode ==
			     AML_NAME_OP)
			    && (walk_state->pass_number <=
				ACPI_IMODE_LOAD_PASS2)) {
				op->named.data = aml_op_start;
				op->named.length = (u32)
				    (walk_state->parser_state.pkg_end -
				     aml_op_start);

				

				walk_state->parser_state.aml =
				    walk_state->parser_state.pkg_end;
				walk_state->arg_count = 0;
			}
			break;

		case AML_WHILE_OP:

			if (walk_state->control_state) {
				walk_state->control_state->control.package_end =
				    walk_state->parser_state.pkg_end;
			}
			break;

		default:

			
			break;
		}

		break;
	}

	return_ACPI_STATUS(AE_OK);
}


static void
acpi_ps_link_module_code(union acpi_parse_object *parent_op,
			 u8 *aml_start, u32 aml_length, acpi_owner_id owner_id)
{
	union acpi_operand_object *prev;
	union acpi_operand_object *next;
	union acpi_operand_object *method_obj;
	struct acpi_namespace_node *parent_node;

	

	prev = next = acpi_gbl_module_code_list;
	while (next) {
		prev = next;
		next = next->method.mutex;
	}

	if (!prev ||
	    ((prev->method.aml_start + prev->method.aml_length) != aml_start)) {

		

		method_obj = acpi_ut_create_internal_object(ACPI_TYPE_METHOD);
		if (!method_obj) {
			return;
		}

		if (parent_op->common.node) {
			parent_node = parent_op->common.node;
		} else {
			parent_node = acpi_gbl_root_node;
		}

		method_obj->method.aml_start = aml_start;
		method_obj->method.aml_length = aml_length;
		method_obj->method.owner_id = owner_id;
		method_obj->method.info_flags |= ACPI_METHOD_MODULE_LEVEL;

		method_obj->method.next_object =
		    ACPI_CAST_PTR(union acpi_operand_object, parent_node);

		if (!prev) {
			acpi_gbl_module_code_list = method_obj;
		} else {
			prev->method.mutex = method_obj;
		}
	} else {
		prev->method.aml_length += aml_length;
	}
}


static acpi_status
acpi_ps_complete_op(struct acpi_walk_state *walk_state,
		    union acpi_parse_object **op, acpi_status status)
{
	acpi_status status2;

	ACPI_FUNCTION_TRACE_PTR(ps_complete_op, walk_state);

	walk_state->parser_state.scope->parse_scope.arg_count--;

	

	status2 = acpi_ps_complete_this_op(walk_state, *op);
	if (ACPI_FAILURE(status2)) {
		return_ACPI_STATUS(status2);
	}

	*op = NULL;

	switch (status) {
	case AE_OK:
		break;

	case AE_CTRL_TRANSFER:

		

		walk_state->prev_op = NULL;
		walk_state->prev_arg_types = walk_state->arg_types;
		return_ACPI_STATUS(status);

	case AE_CTRL_END:

		acpi_ps_pop_scope(&(walk_state->parser_state), op,
				  &walk_state->arg_types,
				  &walk_state->arg_count);

		if (*op) {
			walk_state->op = *op;
			walk_state->op_info =
			    acpi_ps_get_opcode_info((*op)->common.aml_opcode);
			walk_state->opcode = (*op)->common.aml_opcode;

			status = walk_state->ascending_callback(walk_state);
			status =
			    acpi_ps_next_parse_state(walk_state, *op, status);

			status2 = acpi_ps_complete_this_op(walk_state, *op);
			if (ACPI_FAILURE(status2)) {
				return_ACPI_STATUS(status2);
			}
		}

		status = AE_OK;
		break;

	case AE_CTRL_BREAK:
	case AE_CTRL_CONTINUE:

		

		while (!(*op) || ((*op)->common.aml_opcode != AML_WHILE_OP)) {
			acpi_ps_pop_scope(&(walk_state->parser_state), op,
					  &walk_state->arg_types,
					  &walk_state->arg_count);
		}

		

		walk_state->op = *op;
		walk_state->op_info =
		    acpi_ps_get_opcode_info((*op)->common.aml_opcode);
		walk_state->opcode = (*op)->common.aml_opcode;

		status = walk_state->ascending_callback(walk_state);
		status = acpi_ps_next_parse_state(walk_state, *op, status);

		status2 = acpi_ps_complete_this_op(walk_state, *op);
		if (ACPI_FAILURE(status2)) {
			return_ACPI_STATUS(status2);
		}

		status = AE_OK;
		break;

	case AE_CTRL_TERMINATE:

		
		do {
			if (*op) {
				status2 =
				    acpi_ps_complete_this_op(walk_state, *op);
				if (ACPI_FAILURE(status2)) {
					return_ACPI_STATUS(status2);
				}

				acpi_ut_delete_generic_state
				    (acpi_ut_pop_generic_state
				     (&walk_state->control_state));
			}

			acpi_ps_pop_scope(&(walk_state->parser_state), op,
					  &walk_state->arg_types,
					  &walk_state->arg_count);

		} while (*op);

		return_ACPI_STATUS(AE_OK);

	default:		

		do {
			if (*op) {
				status2 =
				    acpi_ps_complete_this_op(walk_state, *op);
				if (ACPI_FAILURE(status2)) {
					return_ACPI_STATUS(status2);
				}
			}

			acpi_ps_pop_scope(&(walk_state->parser_state), op,
					  &walk_state->arg_types,
					  &walk_state->arg_count);

		} while (*op);

#if 0
		if (*op == NULL) {
			acpi_ps_pop_scope(parser_state, op,
					  &walk_state->arg_types,
					  &walk_state->arg_count);
		}
#endif
		walk_state->prev_op = NULL;
		walk_state->prev_arg_types = walk_state->arg_types;
		return_ACPI_STATUS(status);
	}

	

	if (acpi_ps_has_completed_scope(&(walk_state->parser_state))) {
		acpi_ps_pop_scope(&(walk_state->parser_state), op,
				  &walk_state->arg_types,
				  &walk_state->arg_count);
		ACPI_DEBUG_PRINT((ACPI_DB_PARSE, "Popped scope, Op=%p\n", *op));
	} else {
		*op = NULL;
	}

	ACPI_PREEMPTION_POINT();

	return_ACPI_STATUS(AE_OK);
}


static acpi_status
acpi_ps_complete_final_op(struct acpi_walk_state *walk_state,
			  union acpi_parse_object *op, acpi_status status)
{
	acpi_status status2;

	ACPI_FUNCTION_TRACE_PTR(ps_complete_final_op, walk_state);

	ACPI_DEBUG_PRINT((ACPI_DB_PARSE, "AML package complete at Op %p\n",
			  op));
	do {
		if (op) {
			if (walk_state->ascending_callback != NULL) {
				walk_state->op = op;
				walk_state->op_info =
				    acpi_ps_get_opcode_info(op->common.
							    aml_opcode);
				walk_state->opcode = op->common.aml_opcode;

				status =
				    walk_state->ascending_callback(walk_state);
				status =
				    acpi_ps_next_parse_state(walk_state, op,
							     status);
				if (status == AE_CTRL_PENDING) {
					status =
					    acpi_ps_complete_op(walk_state, &op,
								AE_OK);
					if (ACPI_FAILURE(status)) {
						return_ACPI_STATUS(status);
					}
				}

				if (status == AE_CTRL_TERMINATE) {
					status = AE_OK;

					
					do {
						if (op) {
							status2 =
							    acpi_ps_complete_this_op
							    (walk_state, op);
							if (ACPI_FAILURE
							    (status2)) {
								return_ACPI_STATUS
								    (status2);
							}
						}

						acpi_ps_pop_scope(&
								  (walk_state->
								   parser_state),
								  &op,
								  &walk_state->
								  arg_types,
								  &walk_state->
								  arg_count);

					} while (op);

					return_ACPI_STATUS(status);
				}

				else if (ACPI_FAILURE(status)) {

					

					(void)
					    acpi_ps_complete_this_op(walk_state,
								     op);
					return_ACPI_STATUS(status);
				}
			}

			status2 = acpi_ps_complete_this_op(walk_state, op);
			if (ACPI_FAILURE(status2)) {
				return_ACPI_STATUS(status2);
			}
		}

		acpi_ps_pop_scope(&(walk_state->parser_state), &op,
				  &walk_state->arg_types,
				  &walk_state->arg_count);

	} while (op);

	return_ACPI_STATUS(status);
}


acpi_status acpi_ps_parse_loop(struct acpi_walk_state *walk_state)
{
	acpi_status status = AE_OK;
	union acpi_parse_object *op = NULL;	
	struct acpi_parse_state *parser_state;
	u8 *aml_op_start = NULL;

	ACPI_FUNCTION_TRACE_PTR(ps_parse_loop, walk_state);

	if (walk_state->descending_callback == NULL) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	parser_state = &walk_state->parser_state;
	walk_state->arg_types = 0;

#if (!defined (ACPI_NO_METHOD_EXECUTION) && !defined (ACPI_CONSTANT_EVAL_ONLY))

	if (walk_state->walk_type & ACPI_WALK_METHOD_RESTART) {

		

		if (acpi_ps_has_completed_scope(parser_state)) {
			if ((parser_state->scope->parse_scope.op) &&
			    ((parser_state->scope->parse_scope.op->common.
			      aml_opcode == AML_IF_OP)
			     || (parser_state->scope->parse_scope.op->common.
				 aml_opcode == AML_WHILE_OP))
			    && (walk_state->control_state)
			    && (walk_state->control_state->common.state ==
				ACPI_CONTROL_PREDICATE_EXECUTING)) {
				walk_state->op = NULL;
				status =
				    acpi_ds_get_predicate_value(walk_state,
								ACPI_TO_POINTER
								(TRUE));
				if (ACPI_FAILURE(status)
				    && ((status & AE_CODE_MASK) !=
					AE_CODE_CONTROL)) {
					if (status == AE_AML_NO_RETURN_VALUE) {
						ACPI_EXCEPTION((AE_INFO, status,
								"Invoked method did not return a value"));
					}

					ACPI_EXCEPTION((AE_INFO, status,
							"GetPredicate Failed"));
					return_ACPI_STATUS(status);
				}

				status =
				    acpi_ps_next_parse_state(walk_state, op,
							     status);
			}

			acpi_ps_pop_scope(parser_state, &op,
					  &walk_state->arg_types,
					  &walk_state->arg_count);
			ACPI_DEBUG_PRINT((ACPI_DB_PARSE,
					  "Popped scope, Op=%p\n", op));
		} else if (walk_state->prev_op) {

			

			op = walk_state->prev_op;
			walk_state->arg_types = walk_state->prev_arg_types;
		}
	}
#endif

	

	while ((parser_state->aml < parser_state->aml_end) || (op)) {
		aml_op_start = parser_state->aml;
		if (!op) {
			status =
			    acpi_ps_create_op(walk_state, aml_op_start, &op);
			if (ACPI_FAILURE(status)) {
				if (status == AE_CTRL_PARSE_CONTINUE) {
					continue;
				}

				if (status == AE_CTRL_PARSE_PENDING) {
					status = AE_OK;
				}

				status =
				    acpi_ps_complete_op(walk_state, &op,
							status);
				if (ACPI_FAILURE(status)) {
					return_ACPI_STATUS(status);
				}

				continue;
			}

			op->common.aml_offset = walk_state->aml_offset;

			if (walk_state->op_info) {
				ACPI_DEBUG_PRINT((ACPI_DB_PARSE,
						  "Opcode %4.4X [%s] Op %p Aml %p AmlOffset %5.5X\n",
						  (u32) op->common.aml_opcode,
						  walk_state->op_info->name, op,
						  parser_state->aml,
						  op->common.aml_offset));
			}
		}

		walk_state->arg_count = 0;

		

		if (walk_state->arg_types) {

			

			status =
			    acpi_ps_get_arguments(walk_state, aml_op_start, op);
			if (ACPI_FAILURE(status)) {
				status =
				    acpi_ps_complete_op(walk_state, &op,
							status);
				if (ACPI_FAILURE(status)) {
					return_ACPI_STATUS(status);
				}

				continue;
			}
		}

		

		if (walk_state->arg_count) {
			status = acpi_ps_push_scope(parser_state, op,
						    walk_state->arg_types,
						    walk_state->arg_count);
			if (ACPI_FAILURE(status)) {
				status =
				    acpi_ps_complete_op(walk_state, &op,
							status);
				if (ACPI_FAILURE(status)) {
					return_ACPI_STATUS(status);
				}

				continue;
			}

			op = NULL;
			continue;
		}

		walk_state->op_info =
		    acpi_ps_get_opcode_info(op->common.aml_opcode);
		if (walk_state->op_info->flags & AML_NAMED) {
			if (acpi_gbl_depth) {
				acpi_gbl_depth--;
			}

			if (op->common.aml_opcode == AML_REGION_OP ||
			    op->common.aml_opcode == AML_DATA_REGION_OP) {
				op->named.length =
				    (u32) (parser_state->aml - op->named.data);
			}
		}

		if (walk_state->op_info->flags & AML_CREATE) {
			op->named.length =
			    (u32) (parser_state->aml - op->named.data);
		}

		if (op->common.aml_opcode == AML_BANK_FIELD_OP) {
			op->named.length =
			    (u32) (parser_state->aml - op->named.data);
		}

		

		if (walk_state->ascending_callback != NULL) {
			walk_state->op = op;
			walk_state->opcode = op->common.aml_opcode;

			status = walk_state->ascending_callback(walk_state);
			status =
			    acpi_ps_next_parse_state(walk_state, op, status);
			if (status == AE_CTRL_PENDING) {
				status = AE_OK;
			}
		}

		status = acpi_ps_complete_op(walk_state, &op, status);
		if (ACPI_FAILURE(status)) {
			return_ACPI_STATUS(status);
		}

	}			

	status = acpi_ps_complete_final_op(walk_state, op, status);
	return_ACPI_STATUS(status);
}
