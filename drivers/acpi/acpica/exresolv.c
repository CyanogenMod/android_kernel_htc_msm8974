

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

#define _COMPONENT          ACPI_EXECUTER
ACPI_MODULE_NAME("exresolv")

static acpi_status
acpi_ex_resolve_object_to_value(union acpi_operand_object **stack_ptr,
				struct acpi_walk_state *walk_state);


acpi_status
acpi_ex_resolve_to_value(union acpi_operand_object **stack_ptr,
			 struct acpi_walk_state *walk_state)
{
	acpi_status status;

	ACPI_FUNCTION_TRACE_PTR(ex_resolve_to_value, stack_ptr);

	if (!stack_ptr || !*stack_ptr) {
		ACPI_ERROR((AE_INFO, "Internal - null pointer"));
		return_ACPI_STATUS(AE_AML_NO_OPERAND);
	}

	if (ACPI_GET_DESCRIPTOR_TYPE(*stack_ptr) == ACPI_DESC_TYPE_OPERAND) {
		status = acpi_ex_resolve_object_to_value(stack_ptr, walk_state);
		if (ACPI_FAILURE(status)) {
			return_ACPI_STATUS(status);
		}

		if (!*stack_ptr) {
			ACPI_ERROR((AE_INFO, "Internal - null pointer"));
			return_ACPI_STATUS(AE_AML_NO_OPERAND);
		}
	}

	if (ACPI_GET_DESCRIPTOR_TYPE(*stack_ptr) == ACPI_DESC_TYPE_NAMED) {
		status =
		    acpi_ex_resolve_node_to_value(ACPI_CAST_INDIRECT_PTR
						  (struct acpi_namespace_node,
						   stack_ptr), walk_state);
		if (ACPI_FAILURE(status)) {
			return_ACPI_STATUS(status);
		}
	}

	ACPI_DEBUG_PRINT((ACPI_DB_EXEC, "Resolved object %p\n", *stack_ptr));
	return_ACPI_STATUS(AE_OK);
}


static acpi_status
acpi_ex_resolve_object_to_value(union acpi_operand_object **stack_ptr,
				struct acpi_walk_state *walk_state)
{
	acpi_status status = AE_OK;
	union acpi_operand_object *stack_desc;
	union acpi_operand_object *obj_desc = NULL;
	u8 ref_type;

	ACPI_FUNCTION_TRACE(ex_resolve_object_to_value);

	stack_desc = *stack_ptr;

	

	switch (stack_desc->common.type) {
	case ACPI_TYPE_LOCAL_REFERENCE:

		ref_type = stack_desc->reference.class;

		switch (ref_type) {
		case ACPI_REFCLASS_LOCAL:
		case ACPI_REFCLASS_ARG:

			status = acpi_ds_method_data_get_value(ref_type,
							       stack_desc->
							       reference.value,
							       walk_state,
							       &obj_desc);
			if (ACPI_FAILURE(status)) {
				return_ACPI_STATUS(status);
			}

			ACPI_DEBUG_PRINT((ACPI_DB_EXEC,
					  "[Arg/Local %X] ValueObj is %p\n",
					  stack_desc->reference.value,
					  obj_desc));

			acpi_ut_remove_reference(stack_desc);
			*stack_ptr = obj_desc;
			break;

		case ACPI_REFCLASS_INDEX:

			switch (stack_desc->reference.target_type) {
			case ACPI_TYPE_BUFFER_FIELD:

				
				break;

			case ACPI_TYPE_PACKAGE:

				

				if ((walk_state->opcode ==
				     AML_INT_METHODCALL_OP)
				    || (walk_state->opcode == AML_COPY_OP)) {
					break;
				}

				

				obj_desc = *stack_desc->reference.where;
				if (obj_desc) {
					acpi_ut_remove_reference(stack_desc);
					acpi_ut_add_reference(obj_desc);
					*stack_ptr = obj_desc;
				} else {
					ACPI_ERROR((AE_INFO,
						    "Attempt to dereference an Index to NULL package element Idx=%p",
						    stack_desc));
					status = AE_AML_UNINITIALIZED_ELEMENT;
				}
				break;

			default:

				

				ACPI_ERROR((AE_INFO,
					    "Unknown TargetType 0x%X in Index/Reference object %p",
					    stack_desc->reference.target_type,
					    stack_desc));
				status = AE_AML_INTERNAL;
				break;
			}
			break;

		case ACPI_REFCLASS_REFOF:
		case ACPI_REFCLASS_DEBUG:
		case ACPI_REFCLASS_TABLE:

			

			break;

		case ACPI_REFCLASS_NAME:	

			

			if ((stack_desc->reference.node->type ==
			     ACPI_TYPE_DEVICE)
			    || (stack_desc->reference.node->type ==
				ACPI_TYPE_THERMAL)) {

				

				*stack_ptr = (void *)stack_desc->reference.node;
			} else {
				

				*stack_ptr =
				    (stack_desc->reference.node)->object;
				acpi_ut_add_reference(*stack_ptr);
			}

			acpi_ut_remove_reference(stack_desc);
			break;

		default:

			ACPI_ERROR((AE_INFO,
				    "Unknown Reference type 0x%X in %p",
				    ref_type, stack_desc));
			status = AE_AML_INTERNAL;
			break;
		}
		break;

	case ACPI_TYPE_BUFFER:

		status = acpi_ds_get_buffer_arguments(stack_desc);
		break;

	case ACPI_TYPE_PACKAGE:

		status = acpi_ds_get_package_arguments(stack_desc);
		break;

	case ACPI_TYPE_BUFFER_FIELD:
	case ACPI_TYPE_LOCAL_REGION_FIELD:
	case ACPI_TYPE_LOCAL_BANK_FIELD:
	case ACPI_TYPE_LOCAL_INDEX_FIELD:

		ACPI_DEBUG_PRINT((ACPI_DB_EXEC,
				  "FieldRead SourceDesc=%p Type=%X\n",
				  stack_desc, stack_desc->common.type));

		status =
		    acpi_ex_read_data_from_field(walk_state, stack_desc,
						 &obj_desc);

		

		acpi_ut_remove_reference(*stack_ptr);
		*stack_ptr = (void *)obj_desc;
		break;

	default:
		break;
	}

	return_ACPI_STATUS(status);
}


acpi_status
acpi_ex_resolve_multiple(struct acpi_walk_state *walk_state,
			 union acpi_operand_object *operand,
			 acpi_object_type * return_type,
			 union acpi_operand_object **return_desc)
{
	union acpi_operand_object *obj_desc = (void *)operand;
	struct acpi_namespace_node *node;
	acpi_object_type type;
	acpi_status status;

	ACPI_FUNCTION_TRACE(acpi_ex_resolve_multiple);

	

	switch (ACPI_GET_DESCRIPTOR_TYPE(obj_desc)) {
	case ACPI_DESC_TYPE_OPERAND:
		type = obj_desc->common.type;
		break;

	case ACPI_DESC_TYPE_NAMED:
		type = ((struct acpi_namespace_node *)obj_desc)->type;
		obj_desc =
		    acpi_ns_get_attached_object((struct acpi_namespace_node *)
						obj_desc);

		

		if (type == ACPI_TYPE_LOCAL_ALIAS) {
			type = ((struct acpi_namespace_node *)obj_desc)->type;
			obj_desc =
			    acpi_ns_get_attached_object((struct
							 acpi_namespace_node *)
							obj_desc);
		}
		break;

	default:
		return_ACPI_STATUS(AE_AML_OPERAND_TYPE);
	}

	

	if (type != ACPI_TYPE_LOCAL_REFERENCE) {
		goto exit;
	}

	while (obj_desc->common.type == ACPI_TYPE_LOCAL_REFERENCE) {
		switch (obj_desc->reference.class) {
		case ACPI_REFCLASS_REFOF:
		case ACPI_REFCLASS_NAME:

			

			if (obj_desc->reference.class == ACPI_REFCLASS_REFOF) {
				node = obj_desc->reference.object;
			} else {	

				node = obj_desc->reference.node;
			}

			

			if (ACPI_GET_DESCRIPTOR_TYPE(node) !=
			    ACPI_DESC_TYPE_NAMED) {
				ACPI_ERROR((AE_INFO,
					    "Not a namespace node %p [%s]",
					    node,
					    acpi_ut_get_descriptor_name(node)));
				return_ACPI_STATUS(AE_AML_INTERNAL);
			}

			

			obj_desc = acpi_ns_get_attached_object(node);
			if (!obj_desc) {

				

				type = acpi_ns_get_type(node);
				goto exit;
			}

			

			if (obj_desc == operand) {
				return_ACPI_STATUS(AE_AML_CIRCULAR_REFERENCE);
			}
			break;

		case ACPI_REFCLASS_INDEX:

			

			type = obj_desc->reference.target_type;
			if (type != ACPI_TYPE_PACKAGE) {
				goto exit;
			}

			obj_desc = *(obj_desc->reference.where);
			if (!obj_desc) {

				

				type = 0;	
				goto exit;
			}
			break;

		case ACPI_REFCLASS_TABLE:

			type = ACPI_TYPE_DDB_HANDLE;
			goto exit;

		case ACPI_REFCLASS_LOCAL:
		case ACPI_REFCLASS_ARG:

			if (return_desc) {
				status =
				    acpi_ds_method_data_get_value(obj_desc->
								  reference.
								  class,
								  obj_desc->
								  reference.
								  value,
								  walk_state,
								  &obj_desc);
				if (ACPI_FAILURE(status)) {
					return_ACPI_STATUS(status);
				}
				acpi_ut_remove_reference(obj_desc);
			} else {
				status =
				    acpi_ds_method_data_get_node(obj_desc->
								 reference.
								 class,
								 obj_desc->
								 reference.
								 value,
								 walk_state,
								 &node);
				if (ACPI_FAILURE(status)) {
					return_ACPI_STATUS(status);
				}

				obj_desc = acpi_ns_get_attached_object(node);
				if (!obj_desc) {
					type = ACPI_TYPE_ANY;
					goto exit;
				}
			}
			break;

		case ACPI_REFCLASS_DEBUG:

			

			type = ACPI_TYPE_DEBUG_OBJECT;
			goto exit;

		default:

			ACPI_ERROR((AE_INFO,
				    "Unknown Reference Class 0x%2.2X",
				    obj_desc->reference.class));
			return_ACPI_STATUS(AE_AML_INTERNAL);
		}
	}

	type = obj_desc->common.type;

      exit:
	

	switch (type) {
	case ACPI_TYPE_LOCAL_REGION_FIELD:
	case ACPI_TYPE_LOCAL_BANK_FIELD:
	case ACPI_TYPE_LOCAL_INDEX_FIELD:

		type = ACPI_TYPE_FIELD_UNIT;
		break;

	case ACPI_TYPE_LOCAL_SCOPE:

		

		type = ACPI_TYPE_ANY;
		break;

	default:
		
		break;
	}

	*return_type = type;
	if (return_desc) {
		*return_desc = obj_desc;
	}
	return_ACPI_STATUS(AE_OK);
}
