
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
#include "acnamesp.h"
#include "acinterp.h"
#include "acpredef.h"

#define _COMPONENT          ACPI_NAMESPACE
ACPI_MODULE_NAME("nsrepair")

static acpi_status
acpi_ns_convert_to_integer(union acpi_operand_object *original_object,
			   union acpi_operand_object **return_object);

static acpi_status
acpi_ns_convert_to_string(union acpi_operand_object *original_object,
			  union acpi_operand_object **return_object);

static acpi_status
acpi_ns_convert_to_buffer(union acpi_operand_object *original_object,
			  union acpi_operand_object **return_object);


acpi_status
acpi_ns_repair_object(struct acpi_predefined_data *data,
		      u32 expected_btypes,
		      u32 package_index,
		      union acpi_operand_object **return_object_ptr)
{
	union acpi_operand_object *return_object = *return_object_ptr;
	union acpi_operand_object *new_object;
	acpi_status status;

	ACPI_FUNCTION_NAME(ns_repair_object);

	if (expected_btypes & ACPI_RTYPE_INTEGER) {
		status = acpi_ns_convert_to_integer(return_object, &new_object);
		if (ACPI_SUCCESS(status)) {
			goto object_repaired;
		}
	}
	if (expected_btypes & ACPI_RTYPE_STRING) {
		status = acpi_ns_convert_to_string(return_object, &new_object);
		if (ACPI_SUCCESS(status)) {
			goto object_repaired;
		}
	}
	if (expected_btypes & ACPI_RTYPE_BUFFER) {
		status = acpi_ns_convert_to_buffer(return_object, &new_object);
		if (ACPI_SUCCESS(status)) {
			goto object_repaired;
		}
	}
	if (expected_btypes & ACPI_RTYPE_PACKAGE) {
		status =
		    acpi_ns_wrap_with_package(data, return_object, &new_object);
		if (ACPI_SUCCESS(status)) {
			*return_object_ptr = new_object;	
			data->flags |= ACPI_OBJECT_REPAIRED;
			return (AE_OK);
		}
	}

	

	return (AE_AML_OPERAND_TYPE);

      object_repaired:

	

	if (package_index != ACPI_NOT_PACKAGE_ELEMENT) {
		if (!(data->flags & ACPI_OBJECT_WRAPPED)) {
			new_object->common.reference_count =
			    return_object->common.reference_count;

			if (return_object->common.reference_count > 1) {
				return_object->common.reference_count--;
			}
		}

		ACPI_DEBUG_PRINT((ACPI_DB_REPAIR,
				  "%s: Converted %s to expected %s at Package index %u\n",
				  data->pathname,
				  acpi_ut_get_object_type_name(return_object),
				  acpi_ut_get_object_type_name(new_object),
				  package_index));
	} else {
		ACPI_DEBUG_PRINT((ACPI_DB_REPAIR,
				  "%s: Converted %s to expected %s\n",
				  data->pathname,
				  acpi_ut_get_object_type_name(return_object),
				  acpi_ut_get_object_type_name(new_object)));
	}

	

	acpi_ut_remove_reference(return_object);
	*return_object_ptr = new_object;
	data->flags |= ACPI_OBJECT_REPAIRED;
	return (AE_OK);
}


static acpi_status
acpi_ns_convert_to_integer(union acpi_operand_object *original_object,
			   union acpi_operand_object **return_object)
{
	union acpi_operand_object *new_object;
	acpi_status status;
	u64 value = 0;
	u32 i;

	switch (original_object->common.type) {
	case ACPI_TYPE_STRING:

		

		status = acpi_ut_strtoul64(original_object->string.pointer,
					   ACPI_ANY_BASE, &value);
		if (ACPI_FAILURE(status)) {
			return (status);
		}
		break;

	case ACPI_TYPE_BUFFER:

		

		if (original_object->buffer.length > 8) {
			return (AE_AML_OPERAND_TYPE);
		}

		

		for (i = 0; i < original_object->buffer.length; i++) {
			value |=
			    ((u64) original_object->buffer.
			     pointer[i] << (i * 8));
		}
		break;

	default:
		return (AE_AML_OPERAND_TYPE);
	}

	new_object = acpi_ut_create_integer_object(value);
	if (!new_object) {
		return (AE_NO_MEMORY);
	}

	*return_object = new_object;
	return (AE_OK);
}


static acpi_status
acpi_ns_convert_to_string(union acpi_operand_object *original_object,
			  union acpi_operand_object **return_object)
{
	union acpi_operand_object *new_object;
	acpi_size length;
	acpi_status status;

	switch (original_object->common.type) {
	case ACPI_TYPE_INTEGER:
		if (original_object->integer.value == 0) {

			

			new_object = acpi_ut_create_string_object(0);
			if (!new_object) {
				return (AE_NO_MEMORY);
			}
		} else {
			status =
			    acpi_ex_convert_to_string(original_object,
						      &new_object,
						      ACPI_IMPLICIT_CONVERT_HEX);
			if (ACPI_FAILURE(status)) {
				return (status);
			}
		}
		break;

	case ACPI_TYPE_BUFFER:
		length = 0;
		while ((length < original_object->buffer.length) &&
		       (original_object->buffer.pointer[length])) {
			length++;
		}

		

		new_object = acpi_ut_create_string_object(length);
		if (!new_object) {
			return (AE_NO_MEMORY);
		}

		ACPI_MEMCPY(new_object->string.pointer,
			    original_object->buffer.pointer, length);
		break;

	default:
		return (AE_AML_OPERAND_TYPE);
	}

	*return_object = new_object;
	return (AE_OK);
}


static acpi_status
acpi_ns_convert_to_buffer(union acpi_operand_object *original_object,
			  union acpi_operand_object **return_object)
{
	union acpi_operand_object *new_object;
	acpi_status status;
	union acpi_operand_object **elements;
	u32 *dword_buffer;
	u32 count;
	u32 i;

	switch (original_object->common.type) {
	case ACPI_TYPE_INTEGER:
		status =
		    acpi_ex_convert_to_buffer(original_object, &new_object);
		if (ACPI_FAILURE(status)) {
			return (status);
		}
		break;

	case ACPI_TYPE_STRING:

		

		new_object =
		    acpi_ut_create_buffer_object(original_object->string.
						 length);
		if (!new_object) {
			return (AE_NO_MEMORY);
		}

		ACPI_MEMCPY(new_object->buffer.pointer,
			    original_object->string.pointer,
			    original_object->string.length);
		break;

	case ACPI_TYPE_PACKAGE:

		

		elements = original_object->package.elements;
		count = original_object->package.count;

		for (i = 0; i < count; i++) {
			if ((!*elements) ||
			    ((*elements)->common.type != ACPI_TYPE_INTEGER)) {
				return (AE_AML_OPERAND_TYPE);
			}
			elements++;
		}

		

		new_object = acpi_ut_create_buffer_object(ACPI_MUL_4(count));
		if (!new_object) {
			return (AE_NO_MEMORY);
		}

		

		elements = original_object->package.elements;
		dword_buffer = ACPI_CAST_PTR(u32, new_object->buffer.pointer);

		for (i = 0; i < count; i++) {
			*dword_buffer = (u32) (*elements)->integer.value;
			dword_buffer++;
			elements++;
		}
		break;

	default:
		return (AE_AML_OPERAND_TYPE);
	}

	*return_object = new_object;
	return (AE_OK);
}


acpi_status
acpi_ns_repair_null_element(struct acpi_predefined_data *data,
			    u32 expected_btypes,
			    u32 package_index,
			    union acpi_operand_object **return_object_ptr)
{
	union acpi_operand_object *return_object = *return_object_ptr;
	union acpi_operand_object *new_object;

	ACPI_FUNCTION_NAME(ns_repair_null_element);

	

	if (return_object) {
		return (AE_OK);
	}

	if (expected_btypes & ACPI_RTYPE_INTEGER) {

		

		new_object = acpi_ut_create_integer_object((u64)0);
	} else if (expected_btypes & ACPI_RTYPE_STRING) {

		

		new_object = acpi_ut_create_string_object(0);
	} else if (expected_btypes & ACPI_RTYPE_BUFFER) {

		

		new_object = acpi_ut_create_buffer_object(0);
	} else {
		

		return (AE_AML_OPERAND_TYPE);
	}

	if (!new_object) {
		return (AE_NO_MEMORY);
	}

	

	new_object->common.reference_count =
	    data->parent_package->common.reference_count;

	ACPI_DEBUG_PRINT((ACPI_DB_REPAIR,
			  "%s: Converted NULL package element to expected %s at index %u\n",
			  data->pathname,
			  acpi_ut_get_object_type_name(new_object),
			  package_index));

	*return_object_ptr = new_object;
	data->flags |= ACPI_OBJECT_REPAIRED;
	return (AE_OK);
}


void
acpi_ns_remove_null_elements(struct acpi_predefined_data *data,
			     u8 package_type,
			     union acpi_operand_object *obj_desc)
{
	union acpi_operand_object **source;
	union acpi_operand_object **dest;
	u32 count;
	u32 new_count;
	u32 i;

	ACPI_FUNCTION_NAME(ns_remove_null_elements);

	switch (package_type) {
	case ACPI_PTYPE1_VAR:
	case ACPI_PTYPE2:
	case ACPI_PTYPE2_COUNT:
	case ACPI_PTYPE2_PKG_COUNT:
	case ACPI_PTYPE2_FIXED:
	case ACPI_PTYPE2_MIN:
	case ACPI_PTYPE2_REV_FIXED:
	case ACPI_PTYPE2_FIX_VAR:
		break;

	default:
	case ACPI_PTYPE1_FIXED:
	case ACPI_PTYPE1_OPTION:
		return;
	}

	count = obj_desc->package.count;
	new_count = count;

	source = obj_desc->package.elements;
	dest = source;

	

	for (i = 0; i < count; i++) {
		if (!*source) {
			new_count--;
		} else {
			*dest = *source;
			dest++;
		}
		source++;
	}

	

	if (new_count < count) {
		ACPI_DEBUG_PRINT((ACPI_DB_REPAIR,
				  "%s: Found and removed %u NULL elements\n",
				  data->pathname, (count - new_count)));

		

		*dest = NULL;
		obj_desc->package.count = new_count;
	}
}


acpi_status
acpi_ns_wrap_with_package(struct acpi_predefined_data *data,
			  union acpi_operand_object *original_object,
			  union acpi_operand_object **obj_desc_ptr)
{
	union acpi_operand_object *pkg_obj_desc;

	ACPI_FUNCTION_NAME(ns_wrap_with_package);

	pkg_obj_desc = acpi_ut_create_package_object(1);
	if (!pkg_obj_desc) {
		return (AE_NO_MEMORY);
	}

	pkg_obj_desc->package.elements[0] = original_object;

	ACPI_DEBUG_PRINT((ACPI_DB_REPAIR,
			  "%s: Wrapped %s with expected Package object\n",
			  data->pathname,
			  acpi_ut_get_object_type_name(original_object)));

	

	*obj_desc_ptr = pkg_obj_desc;
	data->flags |= ACPI_OBJECT_REPAIRED | ACPI_OBJECT_WRAPPED;
	return (AE_OK);
}
