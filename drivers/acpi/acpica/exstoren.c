

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

#define _COMPONENT          ACPI_EXECUTER
ACPI_MODULE_NAME("exstoren")

acpi_status
acpi_ex_resolve_object(union acpi_operand_object **source_desc_ptr,
		       acpi_object_type target_type,
		       struct acpi_walk_state *walk_state)
{
	union acpi_operand_object *source_desc = *source_desc_ptr;
	acpi_status status = AE_OK;

	ACPI_FUNCTION_TRACE(ex_resolve_object);

	

	switch (target_type) {
	case ACPI_TYPE_BUFFER_FIELD:
	case ACPI_TYPE_LOCAL_REGION_FIELD:
	case ACPI_TYPE_LOCAL_BANK_FIELD:
	case ACPI_TYPE_LOCAL_INDEX_FIELD:

	case ACPI_TYPE_INTEGER:
	case ACPI_TYPE_STRING:
	case ACPI_TYPE_BUFFER:

		if (source_desc->common.type == ACPI_TYPE_LOCAL_REFERENCE) {

			

			status =
			    acpi_ex_resolve_to_value(source_desc_ptr,
						     walk_state);
			if (ACPI_FAILURE(status)) {
				break;
			}
		}

		

		if (walk_state->opcode == AML_COPY_OP) {
			break;
		}

		

		if ((source_desc->common.type != ACPI_TYPE_INTEGER) &&
		    (source_desc->common.type != ACPI_TYPE_BUFFER) &&
		    (source_desc->common.type != ACPI_TYPE_STRING) &&
		    !((source_desc->common.type == ACPI_TYPE_LOCAL_REFERENCE) &&
		      (source_desc->reference.class == ACPI_REFCLASS_TABLE))) {

			

			ACPI_ERROR((AE_INFO,
				    "Cannot assign type %s to %s (must be type Int/Str/Buf)",
				    acpi_ut_get_object_type_name(source_desc),
				    acpi_ut_get_type_name(target_type)));
			status = AE_AML_OPERAND_TYPE;
		}
		break;

	case ACPI_TYPE_LOCAL_ALIAS:
	case ACPI_TYPE_LOCAL_METHOD_ALIAS:

		ACPI_ERROR((AE_INFO, "Store into an unresolved Alias object"));
		status = AE_AML_INTERNAL;
		break;

	case ACPI_TYPE_PACKAGE:
	default:

		break;
	}

	return_ACPI_STATUS(status);
}


acpi_status
acpi_ex_store_object_to_object(union acpi_operand_object *source_desc,
			       union acpi_operand_object *dest_desc,
			       union acpi_operand_object **new_desc,
			       struct acpi_walk_state *walk_state)
{
	union acpi_operand_object *actual_src_desc;
	acpi_status status = AE_OK;

	ACPI_FUNCTION_TRACE_PTR(ex_store_object_to_object, source_desc);

	actual_src_desc = source_desc;
	if (!dest_desc) {
		status =
		    acpi_ut_copy_iobject_to_iobject(actual_src_desc, new_desc,
						    walk_state);
		return_ACPI_STATUS(status);
	}

	if (source_desc->common.type != dest_desc->common.type) {
		status = acpi_ex_convert_to_target_type(dest_desc->common.type,
							source_desc,
							&actual_src_desc,
							walk_state);
		if (ACPI_FAILURE(status)) {
			return_ACPI_STATUS(status);
		}

		if (source_desc == actual_src_desc) {
			*new_desc = source_desc;
			return_ACPI_STATUS(AE_OK);
		}
	}

	switch (dest_desc->common.type) {
	case ACPI_TYPE_INTEGER:

		dest_desc->integer.value = actual_src_desc->integer.value;

		

		acpi_ex_truncate_for32bit_table(dest_desc);
		break;

	case ACPI_TYPE_STRING:

		status =
		    acpi_ex_store_string_to_string(actual_src_desc, dest_desc);
		break;

	case ACPI_TYPE_BUFFER:

		status =
		    acpi_ex_store_buffer_to_buffer(actual_src_desc, dest_desc);
		break;

	case ACPI_TYPE_PACKAGE:

		status =
		    acpi_ut_copy_iobject_to_iobject(actual_src_desc, &dest_desc,
						    walk_state);
		break;

	default:
		ACPI_WARNING((AE_INFO, "Store into type %s not implemented",
			      acpi_ut_get_object_type_name(dest_desc)));

		status = AE_NOT_IMPLEMENTED;
		break;
	}

	if (actual_src_desc != source_desc) {

		

		acpi_ut_remove_reference(actual_src_desc);
	}

	*new_desc = dest_desc;
	return_ACPI_STATUS(status);
}
