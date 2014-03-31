
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

#define _COMPONENT          ACPI_UTILITIES
ACPI_MODULE_NAME("utobject")

static acpi_status
acpi_ut_get_simple_object_size(union acpi_operand_object *obj,
			       acpi_size * obj_length);

static acpi_status
acpi_ut_get_package_object_size(union acpi_operand_object *obj,
				acpi_size * obj_length);

static acpi_status
acpi_ut_get_element_length(u8 object_type,
			   union acpi_operand_object *source_object,
			   union acpi_generic_state *state, void *context);


union acpi_operand_object *acpi_ut_create_internal_object_dbg(const char
							      *module_name,
							      u32 line_number,
							      u32 component_id,
							      acpi_object_type
							      type)
{
	union acpi_operand_object *object;
	union acpi_operand_object *second_object;

	ACPI_FUNCTION_TRACE_STR(ut_create_internal_object_dbg,
				acpi_ut_get_type_name(type));

	

	object =
	    acpi_ut_allocate_object_desc_dbg(module_name, line_number,
					     component_id);
	if (!object) {
		return_PTR(NULL);
	}

	switch (type) {
	case ACPI_TYPE_REGION:
	case ACPI_TYPE_BUFFER_FIELD:
	case ACPI_TYPE_LOCAL_BANK_FIELD:

		

		second_object = acpi_ut_allocate_object_desc_dbg(module_name,
								 line_number,
								 component_id);
		if (!second_object) {
			acpi_ut_delete_object_desc(object);
			return_PTR(NULL);
		}

		second_object->common.type = ACPI_TYPE_LOCAL_EXTRA;
		second_object->common.reference_count = 1;

		

		object->common.next_object = second_object;
		break;

	default:
		
		break;
	}

	

	object->common.type = (u8) type;

	

	object->common.reference_count = 1;

	

	return_PTR(object);
}


union acpi_operand_object *acpi_ut_create_package_object(u32 count)
{
	union acpi_operand_object *package_desc;
	union acpi_operand_object **package_elements;

	ACPI_FUNCTION_TRACE_U32(ut_create_package_object, count);

	

	package_desc = acpi_ut_create_internal_object(ACPI_TYPE_PACKAGE);
	if (!package_desc) {
		return_PTR(NULL);
	}

	package_elements = ACPI_ALLOCATE_ZEROED(((acpi_size) count +
						 1) * sizeof(void *));
	if (!package_elements) {
		acpi_ut_remove_reference(package_desc);
		return_PTR(NULL);
	}

	package_desc->package.count = count;
	package_desc->package.elements = package_elements;
	return_PTR(package_desc);
}


union acpi_operand_object *acpi_ut_create_integer_object(u64 initial_value)
{
	union acpi_operand_object *integer_desc;

	ACPI_FUNCTION_TRACE(ut_create_integer_object);

	

	integer_desc = acpi_ut_create_internal_object(ACPI_TYPE_INTEGER);
	if (!integer_desc) {
		return_PTR(NULL);
	}

	integer_desc->integer.value = initial_value;
	return_PTR(integer_desc);
}


union acpi_operand_object *acpi_ut_create_buffer_object(acpi_size buffer_size)
{
	union acpi_operand_object *buffer_desc;
	u8 *buffer = NULL;

	ACPI_FUNCTION_TRACE_U32(ut_create_buffer_object, buffer_size);

	

	buffer_desc = acpi_ut_create_internal_object(ACPI_TYPE_BUFFER);
	if (!buffer_desc) {
		return_PTR(NULL);
	}

	

	if (buffer_size > 0) {

		

		buffer = ACPI_ALLOCATE_ZEROED(buffer_size);
		if (!buffer) {
			ACPI_ERROR((AE_INFO, "Could not allocate size %u",
				    (u32) buffer_size));
			acpi_ut_remove_reference(buffer_desc);
			return_PTR(NULL);
		}
	}

	

	buffer_desc->buffer.flags |= AOPOBJ_DATA_VALID;
	buffer_desc->buffer.pointer = buffer;
	buffer_desc->buffer.length = (u32) buffer_size;

	

	return_PTR(buffer_desc);
}


union acpi_operand_object *acpi_ut_create_string_object(acpi_size string_size)
{
	union acpi_operand_object *string_desc;
	char *string;

	ACPI_FUNCTION_TRACE_U32(ut_create_string_object, string_size);

	

	string_desc = acpi_ut_create_internal_object(ACPI_TYPE_STRING);
	if (!string_desc) {
		return_PTR(NULL);
	}

	string = ACPI_ALLOCATE_ZEROED(string_size + 1);
	if (!string) {
		ACPI_ERROR((AE_INFO, "Could not allocate size %u",
			    (u32) string_size));
		acpi_ut_remove_reference(string_desc);
		return_PTR(NULL);
	}

	

	string_desc->string.pointer = string;
	string_desc->string.length = (u32) string_size;

	

	return_PTR(string_desc);
}


u8 acpi_ut_valid_internal_object(void *object)
{

	ACPI_FUNCTION_NAME(ut_valid_internal_object);

	

	if (!object) {
		ACPI_DEBUG_PRINT((ACPI_DB_EXEC, "**** Null Object Ptr\n"));
		return (FALSE);
	}

	

	switch (ACPI_GET_DESCRIPTOR_TYPE(object)) {
	case ACPI_DESC_TYPE_OPERAND:

		

		return (TRUE);

	default:
		ACPI_DEBUG_PRINT((ACPI_DB_EXEC,
				  "%p is not not an ACPI operand obj [%s]\n",
				  object, acpi_ut_get_descriptor_name(object)));
		break;
	}

	return (FALSE);
}


void *acpi_ut_allocate_object_desc_dbg(const char *module_name,
				       u32 line_number, u32 component_id)
{
	union acpi_operand_object *object;

	ACPI_FUNCTION_TRACE(ut_allocate_object_desc_dbg);

	object = acpi_os_acquire_object(acpi_gbl_operand_cache);
	if (!object) {
		ACPI_ERROR((module_name, line_number,
			    "Could not allocate an object descriptor"));

		return_PTR(NULL);
	}

	

	memset(object, 0, sizeof(union acpi_operand_object));
	ACPI_SET_DESCRIPTOR_TYPE(object, ACPI_DESC_TYPE_OPERAND);

	ACPI_DEBUG_PRINT((ACPI_DB_ALLOCATIONS, "%p Size %X\n",
			  object, (u32) sizeof(union acpi_operand_object)));

	return_PTR(object);
}


void acpi_ut_delete_object_desc(union acpi_operand_object *object)
{
	ACPI_FUNCTION_TRACE_PTR(ut_delete_object_desc, object);

	

	if (ACPI_GET_DESCRIPTOR_TYPE(object) != ACPI_DESC_TYPE_OPERAND) {
		ACPI_ERROR((AE_INFO,
			    "%p is not an ACPI Operand object [%s]", object,
			    acpi_ut_get_descriptor_name(object)));
		return_VOID;
	}

	(void)acpi_os_release_object(acpi_gbl_operand_cache, object);
	return_VOID;
}


static acpi_status
acpi_ut_get_simple_object_size(union acpi_operand_object *internal_object,
			       acpi_size * obj_length)
{
	acpi_size length;
	acpi_size size;
	acpi_status status = AE_OK;

	ACPI_FUNCTION_TRACE_PTR(ut_get_simple_object_size, internal_object);

	if (!internal_object) {
		*obj_length = sizeof(union acpi_object);
		return_ACPI_STATUS(AE_OK);
	}

	

	length = sizeof(union acpi_object);

	if (ACPI_GET_DESCRIPTOR_TYPE(internal_object) == ACPI_DESC_TYPE_NAMED) {

		

		*obj_length = ACPI_ROUND_UP_TO_NATIVE_WORD(length);
		return_ACPI_STATUS(status);
	}

	switch (internal_object->common.type) {
	case ACPI_TYPE_STRING:

		length += (acpi_size) internal_object->string.length + 1;
		break;

	case ACPI_TYPE_BUFFER:

		length += (acpi_size) internal_object->buffer.length;
		break;

	case ACPI_TYPE_INTEGER:
	case ACPI_TYPE_PROCESSOR:
	case ACPI_TYPE_POWER:

		

		break;

	case ACPI_TYPE_LOCAL_REFERENCE:

		switch (internal_object->reference.class) {
		case ACPI_REFCLASS_NAME:

			size =
			    acpi_ns_get_pathname_length(internal_object->
							reference.node);
			if (!size) {
				return_ACPI_STATUS(AE_BAD_PARAMETER);
			}

			length += ACPI_ROUND_UP_TO_NATIVE_WORD(size);
			break;

		default:

			ACPI_ERROR((AE_INFO,
				    "Cannot convert to external object - "
				    "unsupported Reference Class [%s] 0x%X in object %p",
				    acpi_ut_get_reference_name(internal_object),
				    internal_object->reference.class,
				    internal_object));
			status = AE_TYPE;
			break;
		}
		break;

	default:

		ACPI_ERROR((AE_INFO, "Cannot convert to external object - "
			    "unsupported type [%s] 0x%X in object %p",
			    acpi_ut_get_object_type_name(internal_object),
			    internal_object->common.type, internal_object));
		status = AE_TYPE;
		break;
	}

	*obj_length = ACPI_ROUND_UP_TO_NATIVE_WORD(length);
	return_ACPI_STATUS(status);
}


static acpi_status
acpi_ut_get_element_length(u8 object_type,
			   union acpi_operand_object *source_object,
			   union acpi_generic_state *state, void *context)
{
	acpi_status status = AE_OK;
	struct acpi_pkg_info *info = (struct acpi_pkg_info *)context;
	acpi_size object_space;

	switch (object_type) {
	case ACPI_COPY_TYPE_SIMPLE:

		status =
		    acpi_ut_get_simple_object_size(source_object,
						   &object_space);
		if (ACPI_FAILURE(status)) {
			return (status);
		}

		info->length += object_space;
		break;

	case ACPI_COPY_TYPE_PACKAGE:

		

		info->num_packages++;
		state->pkg.this_target_obj = NULL;
		break;

	default:

		

		return (AE_BAD_PARAMETER);
	}

	return (status);
}


static acpi_status
acpi_ut_get_package_object_size(union acpi_operand_object *internal_object,
				acpi_size * obj_length)
{
	acpi_status status;
	struct acpi_pkg_info info;

	ACPI_FUNCTION_TRACE_PTR(ut_get_package_object_size, internal_object);

	info.length = 0;
	info.object_space = 0;
	info.num_packages = 1;

	status = acpi_ut_walk_package_tree(internal_object, NULL,
					   acpi_ut_get_element_length, &info);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	info.length += ACPI_ROUND_UP_TO_NATIVE_WORD(sizeof(union acpi_object)) *
	    (acpi_size) info.num_packages;

	

	*obj_length = info.length;
	return_ACPI_STATUS(status);
}


acpi_status
acpi_ut_get_object_size(union acpi_operand_object *internal_object,
			acpi_size * obj_length)
{
	acpi_status status;

	ACPI_FUNCTION_ENTRY();

	if ((ACPI_GET_DESCRIPTOR_TYPE(internal_object) ==
	     ACPI_DESC_TYPE_OPERAND)
	    && (internal_object->common.type == ACPI_TYPE_PACKAGE)) {
		status =
		    acpi_ut_get_package_object_size(internal_object,
						    obj_length);
	} else {
		status =
		    acpi_ut_get_simple_object_size(internal_object, obj_length);
	}

	return (status);
}
