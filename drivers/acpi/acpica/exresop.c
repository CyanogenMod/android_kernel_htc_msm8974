

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
#include "acparser.h"
#include "acinterp.h"
#include "acnamesp.h"

#define _COMPONENT          ACPI_EXECUTER
ACPI_MODULE_NAME("exresop")

static acpi_status
acpi_ex_check_object_type(acpi_object_type type_needed,
			  acpi_object_type this_type, void *object);


static acpi_status
acpi_ex_check_object_type(acpi_object_type type_needed,
			  acpi_object_type this_type, void *object)
{
	ACPI_FUNCTION_ENTRY();

	if (type_needed == ACPI_TYPE_ANY) {

		

		return (AE_OK);
	}

	if (type_needed == ACPI_TYPE_LOCAL_REFERENCE) {
		if ((this_type == ACPI_TYPE_INTEGER) &&
		    (((union acpi_operand_object *)object)->common.
		     flags & AOPOBJ_AML_CONSTANT)) {
			return (AE_OK);
		}
	}

	if (type_needed != this_type) {
		ACPI_ERROR((AE_INFO,
			    "Needed type [%s], found [%s] %p",
			    acpi_ut_get_type_name(type_needed),
			    acpi_ut_get_type_name(this_type), object));

		return (AE_AML_OPERAND_TYPE);
	}

	return (AE_OK);
}


acpi_status
acpi_ex_resolve_operands(u16 opcode,
			 union acpi_operand_object ** stack_ptr,
			 struct acpi_walk_state * walk_state)
{
	union acpi_operand_object *obj_desc;
	acpi_status status = AE_OK;
	u8 object_type;
	u32 arg_types;
	const struct acpi_opcode_info *op_info;
	u32 this_arg_type;
	acpi_object_type type_needed;
	u16 target_op = 0;

	ACPI_FUNCTION_TRACE_U32(ex_resolve_operands, opcode);

	op_info = acpi_ps_get_opcode_info(opcode);
	if (op_info->class == AML_CLASS_UNKNOWN) {
		return_ACPI_STATUS(AE_AML_BAD_OPCODE);
	}

	arg_types = op_info->runtime_args;
	if (arg_types == ARGI_INVALID_OPCODE) {
		ACPI_ERROR((AE_INFO, "Unknown AML opcode 0x%X", opcode));

		return_ACPI_STATUS(AE_AML_INTERNAL);
	}

	ACPI_DEBUG_PRINT((ACPI_DB_EXEC,
			  "Opcode %X [%s] RequiredOperandTypes=%8.8X\n",
			  opcode, op_info->name, arg_types));

	while (GET_CURRENT_ARG_TYPE(arg_types)) {
		if (!stack_ptr || !*stack_ptr) {
			ACPI_ERROR((AE_INFO, "Null stack entry at %p",
				    stack_ptr));

			return_ACPI_STATUS(AE_AML_INTERNAL);
		}

		

		obj_desc = *stack_ptr;

		

		switch (ACPI_GET_DESCRIPTOR_TYPE(obj_desc)) {
		case ACPI_DESC_TYPE_NAMED:

			

			object_type =
			    ((struct acpi_namespace_node *)obj_desc)->type;

			if (object_type == ACPI_TYPE_LOCAL_ALIAS) {
				obj_desc =
				    acpi_ns_get_attached_object((struct
								 acpi_namespace_node
								 *)obj_desc);
				*stack_ptr = obj_desc;
				object_type =
				    ((struct acpi_namespace_node *)obj_desc)->
				    type;
			}
			break;

		case ACPI_DESC_TYPE_OPERAND:

			

			object_type = obj_desc->common.type;

			

			if (!acpi_ut_valid_object_type(object_type)) {
				ACPI_ERROR((AE_INFO,
					    "Bad operand object type [0x%X]",
					    object_type));

				return_ACPI_STATUS(AE_AML_OPERAND_TYPE);
			}

			if (object_type == (u8) ACPI_TYPE_LOCAL_REFERENCE) {

				

				switch (obj_desc->reference.class) {
				case ACPI_REFCLASS_DEBUG:

					target_op = AML_DEBUG_OP;

					

				case ACPI_REFCLASS_ARG:
				case ACPI_REFCLASS_LOCAL:
				case ACPI_REFCLASS_INDEX:
				case ACPI_REFCLASS_REFOF:
				case ACPI_REFCLASS_TABLE:	
				case ACPI_REFCLASS_NAME:	

					ACPI_DEBUG_PRINT((ACPI_DB_EXEC,
							  "Operand is a Reference, Class [%s] %2.2X\n",
							  acpi_ut_get_reference_name
							  (obj_desc),
							  obj_desc->reference.
							  class));
					break;

				default:

					ACPI_ERROR((AE_INFO,
						    "Unknown Reference Class 0x%2.2X in %p",
						    obj_desc->reference.class,
						    obj_desc));

					return_ACPI_STATUS(AE_AML_OPERAND_TYPE);
				}
			}
			break;

		default:

			

			ACPI_ERROR((AE_INFO, "Invalid descriptor %p [%s]",
				    obj_desc,
				    acpi_ut_get_descriptor_name(obj_desc)));

			return_ACPI_STATUS(AE_AML_OPERAND_TYPE);
		}

		

		this_arg_type = GET_CURRENT_ARG_TYPE(arg_types);
		INCREMENT_ARG_LIST(arg_types);

		switch (this_arg_type) {
		case ARGI_REF_OR_STRING:	

			if ((ACPI_GET_DESCRIPTOR_TYPE(obj_desc) ==
			     ACPI_DESC_TYPE_OPERAND)
			    && (obj_desc->common.type == ACPI_TYPE_STRING)) {
				goto next_operand;
			}

			

		case ARGI_REFERENCE:	
		case ARGI_INTEGER_REF:
		case ARGI_OBJECT_REF:
		case ARGI_DEVICE_REF:
		case ARGI_TARGETREF:	
		case ARGI_FIXED_TARGET:	
		case ARGI_SIMPLE_TARGET:	

			if (ACPI_GET_DESCRIPTOR_TYPE(obj_desc) ==
			    ACPI_DESC_TYPE_NAMED) {
				goto next_operand;
			}

			status =
			    acpi_ex_check_object_type(ACPI_TYPE_LOCAL_REFERENCE,
						      object_type, obj_desc);
			if (ACPI_FAILURE(status)) {
				return_ACPI_STATUS(status);
			}
			goto next_operand;

		case ARGI_DATAREFOBJ:	

			if ((opcode == AML_STORE_OP) &&
			    ((*stack_ptr)->common.type ==
			     ACPI_TYPE_LOCAL_REFERENCE)
			    && ((*stack_ptr)->reference.class == ACPI_REFCLASS_INDEX)) {
				goto next_operand;
			}
			break;

		default:
			
			break;
		}

		status = acpi_ex_resolve_to_value(stack_ptr, walk_state);
		if (ACPI_FAILURE(status)) {
			return_ACPI_STATUS(status);
		}

		

		obj_desc = *stack_ptr;

		switch (this_arg_type) {
		case ARGI_MUTEX:

			

			type_needed = ACPI_TYPE_MUTEX;
			break;

		case ARGI_EVENT:

			

			type_needed = ACPI_TYPE_EVENT;
			break;

		case ARGI_PACKAGE:	

			

			type_needed = ACPI_TYPE_PACKAGE;
			break;

		case ARGI_ANYTYPE:

			

			type_needed = ACPI_TYPE_ANY;
			break;

		case ARGI_DDBHANDLE:

			

			type_needed = ACPI_TYPE_LOCAL_REFERENCE;
			break;

		case ARGI_INTEGER:

			status =
			    acpi_ex_convert_to_integer(obj_desc, stack_ptr, 16);
			if (ACPI_FAILURE(status)) {
				if (status == AE_TYPE) {
					ACPI_ERROR((AE_INFO,
						    "Needed [Integer/String/Buffer], found [%s] %p",
						    acpi_ut_get_object_type_name
						    (obj_desc), obj_desc));

					return_ACPI_STATUS(AE_AML_OPERAND_TYPE);
				}

				return_ACPI_STATUS(status);
			}

			if (obj_desc != *stack_ptr) {
				acpi_ut_remove_reference(obj_desc);
			}
			goto next_operand;

		case ARGI_BUFFER:

			status = acpi_ex_convert_to_buffer(obj_desc, stack_ptr);
			if (ACPI_FAILURE(status)) {
				if (status == AE_TYPE) {
					ACPI_ERROR((AE_INFO,
						    "Needed [Integer/String/Buffer], found [%s] %p",
						    acpi_ut_get_object_type_name
						    (obj_desc), obj_desc));

					return_ACPI_STATUS(AE_AML_OPERAND_TYPE);
				}

				return_ACPI_STATUS(status);
			}

			if (obj_desc != *stack_ptr) {
				acpi_ut_remove_reference(obj_desc);
			}
			goto next_operand;

		case ARGI_STRING:

			status = acpi_ex_convert_to_string(obj_desc, stack_ptr,
							   ACPI_IMPLICIT_CONVERT_HEX);
			if (ACPI_FAILURE(status)) {
				if (status == AE_TYPE) {
					ACPI_ERROR((AE_INFO,
						    "Needed [Integer/String/Buffer], found [%s] %p",
						    acpi_ut_get_object_type_name
						    (obj_desc), obj_desc));

					return_ACPI_STATUS(AE_AML_OPERAND_TYPE);
				}

				return_ACPI_STATUS(status);
			}

			if (obj_desc != *stack_ptr) {
				acpi_ut_remove_reference(obj_desc);
			}
			goto next_operand;

		case ARGI_COMPUTEDATA:

			

			switch (obj_desc->common.type) {
			case ACPI_TYPE_INTEGER:
			case ACPI_TYPE_STRING:
			case ACPI_TYPE_BUFFER:

				
				break;

			default:
				ACPI_ERROR((AE_INFO,
					    "Needed [Integer/String/Buffer], found [%s] %p",
					    acpi_ut_get_object_type_name
					    (obj_desc), obj_desc));

				return_ACPI_STATUS(AE_AML_OPERAND_TYPE);
			}
			goto next_operand;

		case ARGI_BUFFER_OR_STRING:

			

			switch (obj_desc->common.type) {
			case ACPI_TYPE_STRING:
			case ACPI_TYPE_BUFFER:

				
				break;

			case ACPI_TYPE_INTEGER:

				

				status =
				    acpi_ex_convert_to_buffer(obj_desc,
							      stack_ptr);
				if (ACPI_FAILURE(status)) {
					return_ACPI_STATUS(status);
				}

				if (obj_desc != *stack_ptr) {
					acpi_ut_remove_reference(obj_desc);
				}
				break;

			default:
				ACPI_ERROR((AE_INFO,
					    "Needed [Integer/String/Buffer], found [%s] %p",
					    acpi_ut_get_object_type_name
					    (obj_desc), obj_desc));

				return_ACPI_STATUS(AE_AML_OPERAND_TYPE);
			}
			goto next_operand;

		case ARGI_DATAOBJECT:
			switch (obj_desc->common.type) {
			case ACPI_TYPE_PACKAGE:
			case ACPI_TYPE_STRING:
			case ACPI_TYPE_BUFFER:
			case ACPI_TYPE_LOCAL_REFERENCE:

				
				break;

			default:
				ACPI_ERROR((AE_INFO,
					    "Needed [Buffer/String/Package/Reference], found [%s] %p",
					    acpi_ut_get_object_type_name
					    (obj_desc), obj_desc));

				return_ACPI_STATUS(AE_AML_OPERAND_TYPE);
			}
			goto next_operand;

		case ARGI_COMPLEXOBJ:

			

			switch (obj_desc->common.type) {
			case ACPI_TYPE_PACKAGE:
			case ACPI_TYPE_STRING:
			case ACPI_TYPE_BUFFER:

				
				break;

			default:
				ACPI_ERROR((AE_INFO,
					    "Needed [Buffer/String/Package], found [%s] %p",
					    acpi_ut_get_object_type_name
					    (obj_desc), obj_desc));

				return_ACPI_STATUS(AE_AML_OPERAND_TYPE);
			}
			goto next_operand;

		case ARGI_REGION_OR_BUFFER:	

			

			switch (obj_desc->common.type) {
			case ACPI_TYPE_BUFFER:
			case ACPI_TYPE_REGION:

				
				break;

			default:
				ACPI_ERROR((AE_INFO,
					    "Needed [Region/Buffer], found [%s] %p",
					    acpi_ut_get_object_type_name
					    (obj_desc), obj_desc));

				return_ACPI_STATUS(AE_AML_OPERAND_TYPE);
			}
			goto next_operand;

		case ARGI_DATAREFOBJ:

			

			switch (obj_desc->common.type) {
			case ACPI_TYPE_INTEGER:
			case ACPI_TYPE_PACKAGE:
			case ACPI_TYPE_STRING:
			case ACPI_TYPE_BUFFER:
			case ACPI_TYPE_BUFFER_FIELD:
			case ACPI_TYPE_LOCAL_REFERENCE:
			case ACPI_TYPE_LOCAL_REGION_FIELD:
			case ACPI_TYPE_LOCAL_BANK_FIELD:
			case ACPI_TYPE_LOCAL_INDEX_FIELD:
			case ACPI_TYPE_DDB_HANDLE:

				
				break;

			default:

				if (acpi_gbl_enable_interpreter_slack) {
					break;
				}

				if (target_op == AML_DEBUG_OP) {

					

					break;
				}

				ACPI_ERROR((AE_INFO,
					    "Needed Integer/Buffer/String/Package/Ref/Ddb], found [%s] %p",
					    acpi_ut_get_object_type_name
					    (obj_desc), obj_desc));

				return_ACPI_STATUS(AE_AML_OPERAND_TYPE);
			}
			goto next_operand;

		default:

			

			ACPI_ERROR((AE_INFO,
				    "Internal - Unknown ARGI (required operand) type 0x%X",
				    this_arg_type));

			return_ACPI_STATUS(AE_BAD_PARAMETER);
		}

		status = acpi_ex_check_object_type(type_needed,
						   (*stack_ptr)->common.type,
						   *stack_ptr);
		if (ACPI_FAILURE(status)) {
			return_ACPI_STATUS(status);
		}

	      next_operand:
		if (GET_CURRENT_ARG_TYPE(arg_types)) {
			stack_ptr--;
		}
	}

	ACPI_DUMP_OPERANDS(walk_state->operands,
			   acpi_ps_get_opcode_name(opcode),
			   walk_state->num_operands);

	return_ACPI_STATUS(status);
}
