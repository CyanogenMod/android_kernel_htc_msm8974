
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
ACPI_MODULE_NAME("utcopy")

static acpi_status
acpi_ut_copy_isimple_to_esimple(union acpi_operand_object *internal_object,
				union acpi_object *external_object,
				u8 * data_space, acpi_size * buffer_space_used);

static acpi_status
acpi_ut_copy_ielement_to_ielement(u8 object_type,
				  union acpi_operand_object *source_object,
				  union acpi_generic_state *state,
				  void *context);

static acpi_status
acpi_ut_copy_ipackage_to_epackage(union acpi_operand_object *internal_object,
				  u8 * buffer, acpi_size * space_used);

static acpi_status
acpi_ut_copy_esimple_to_isimple(union acpi_object *user_obj,
				union acpi_operand_object **return_obj);

static acpi_status
acpi_ut_copy_epackage_to_ipackage(union acpi_object *external_object,
				  union acpi_operand_object **internal_object);

static acpi_status
acpi_ut_copy_simple_object(union acpi_operand_object *source_desc,
			   union acpi_operand_object *dest_desc);

static acpi_status
acpi_ut_copy_ielement_to_eelement(u8 object_type,
				  union acpi_operand_object *source_object,
				  union acpi_generic_state *state,
				  void *context);

static acpi_status
acpi_ut_copy_ipackage_to_ipackage(union acpi_operand_object *source_obj,
				  union acpi_operand_object *dest_obj,
				  struct acpi_walk_state *walk_state);


static acpi_status
acpi_ut_copy_isimple_to_esimple(union acpi_operand_object *internal_object,
				union acpi_object *external_object,
				u8 * data_space, acpi_size * buffer_space_used)
{
	acpi_status status = AE_OK;

	ACPI_FUNCTION_TRACE(ut_copy_isimple_to_esimple);

	*buffer_space_used = 0;

	if (!internal_object) {
		return_ACPI_STATUS(AE_OK);
	}

	

	ACPI_MEMSET(external_object, 0, sizeof(union acpi_object));

	external_object->type = internal_object->common.type;

	

	switch (internal_object->common.type) {
	case ACPI_TYPE_STRING:

		external_object->string.pointer = (char *)data_space;
		external_object->string.length = internal_object->string.length;
		*buffer_space_used = ACPI_ROUND_UP_TO_NATIVE_WORD((acpi_size)
								  internal_object->
								  string.
								  length + 1);

		ACPI_MEMCPY((void *)data_space,
			    (void *)internal_object->string.pointer,
			    (acpi_size) internal_object->string.length + 1);
		break;

	case ACPI_TYPE_BUFFER:

		external_object->buffer.pointer = data_space;
		external_object->buffer.length = internal_object->buffer.length;
		*buffer_space_used =
		    ACPI_ROUND_UP_TO_NATIVE_WORD(internal_object->string.
						 length);

		ACPI_MEMCPY((void *)data_space,
			    (void *)internal_object->buffer.pointer,
			    internal_object->buffer.length);
		break;

	case ACPI_TYPE_INTEGER:

		external_object->integer.value = internal_object->integer.value;
		break;

	case ACPI_TYPE_LOCAL_REFERENCE:

		

		switch (internal_object->reference.class) {
		case ACPI_REFCLASS_NAME:

			external_object->reference.handle =
			    internal_object->reference.node;
			external_object->reference.actual_type =
			    acpi_ns_get_type(internal_object->reference.node);
			break;

		default:

			

			return_ACPI_STATUS(AE_TYPE);
		}
		break;

	case ACPI_TYPE_PROCESSOR:

		external_object->processor.proc_id =
		    internal_object->processor.proc_id;
		external_object->processor.pblk_address =
		    internal_object->processor.address;
		external_object->processor.pblk_length =
		    internal_object->processor.length;
		break;

	case ACPI_TYPE_POWER:

		external_object->power_resource.system_level =
		    internal_object->power_resource.system_level;

		external_object->power_resource.resource_order =
		    internal_object->power_resource.resource_order;
		break;

	default:
		ACPI_ERROR((AE_INFO,
			    "Unsupported object type, cannot convert to external object: %s",
			    acpi_ut_get_type_name(internal_object->common.
						  type)));

		return_ACPI_STATUS(AE_SUPPORT);
	}

	return_ACPI_STATUS(status);
}


static acpi_status
acpi_ut_copy_ielement_to_eelement(u8 object_type,
				  union acpi_operand_object *source_object,
				  union acpi_generic_state *state,
				  void *context)
{
	acpi_status status = AE_OK;
	struct acpi_pkg_info *info = (struct acpi_pkg_info *)context;
	acpi_size object_space;
	u32 this_index;
	union acpi_object *target_object;

	ACPI_FUNCTION_ENTRY();

	this_index = state->pkg.index;
	target_object = (union acpi_object *)
	    &((union acpi_object *)(state->pkg.dest_object))->package.
	    elements[this_index];

	switch (object_type) {
	case ACPI_COPY_TYPE_SIMPLE:

		status = acpi_ut_copy_isimple_to_esimple(source_object,
							 target_object,
							 info->free_space,
							 &object_space);
		if (ACPI_FAILURE(status)) {
			return (status);
		}
		break;

	case ACPI_COPY_TYPE_PACKAGE:

		target_object->type = ACPI_TYPE_PACKAGE;
		target_object->package.count = source_object->package.count;
		target_object->package.elements =
		    ACPI_CAST_PTR(union acpi_object, info->free_space);

		state->pkg.this_target_obj = target_object;

		object_space = ACPI_ROUND_UP_TO_NATIVE_WORD((acpi_size)
							    target_object->
							    package.count *
							    sizeof(union
								   acpi_object));
		break;

	default:
		return (AE_BAD_PARAMETER);
	}

	info->free_space += object_space;
	info->length += object_space;
	return (status);
}


static acpi_status
acpi_ut_copy_ipackage_to_epackage(union acpi_operand_object *internal_object,
				  u8 * buffer, acpi_size * space_used)
{
	union acpi_object *external_object;
	acpi_status status;
	struct acpi_pkg_info info;

	ACPI_FUNCTION_TRACE(ut_copy_ipackage_to_epackage);

	external_object = ACPI_CAST_PTR(union acpi_object, buffer);

	info.length = ACPI_ROUND_UP_TO_NATIVE_WORD(sizeof(union acpi_object));
	info.free_space =
	    buffer + ACPI_ROUND_UP_TO_NATIVE_WORD(sizeof(union acpi_object));
	info.object_space = 0;
	info.num_packages = 1;

	external_object->type = internal_object->common.type;
	external_object->package.count = internal_object->package.count;
	external_object->package.elements = ACPI_CAST_PTR(union acpi_object,
							  info.free_space);

	info.length += (acpi_size) external_object->package.count *
	    ACPI_ROUND_UP_TO_NATIVE_WORD(sizeof(union acpi_object));
	info.free_space += external_object->package.count *
	    ACPI_ROUND_UP_TO_NATIVE_WORD(sizeof(union acpi_object));

	status = acpi_ut_walk_package_tree(internal_object, external_object,
					   acpi_ut_copy_ielement_to_eelement,
					   &info);

	*space_used = info.length;
	return_ACPI_STATUS(status);
}


acpi_status
acpi_ut_copy_iobject_to_eobject(union acpi_operand_object *internal_object,
				struct acpi_buffer *ret_buffer)
{
	acpi_status status;

	ACPI_FUNCTION_TRACE(ut_copy_iobject_to_eobject);

	if (internal_object->common.type == ACPI_TYPE_PACKAGE) {
		status = acpi_ut_copy_ipackage_to_epackage(internal_object,
							   ret_buffer->pointer,
							   &ret_buffer->length);
	} else {
		status = acpi_ut_copy_isimple_to_esimple(internal_object,
							 ACPI_CAST_PTR(union
								       acpi_object,
								       ret_buffer->
								       pointer),
							 ACPI_ADD_PTR(u8,
								      ret_buffer->
								      pointer,
								      ACPI_ROUND_UP_TO_NATIVE_WORD
								      (sizeof
								       (union
									acpi_object))),
							 &ret_buffer->length);
		ret_buffer->length += sizeof(union acpi_object);
	}

	return_ACPI_STATUS(status);
}


static acpi_status
acpi_ut_copy_esimple_to_isimple(union acpi_object *external_object,
				union acpi_operand_object **ret_internal_object)
{
	union acpi_operand_object *internal_object;

	ACPI_FUNCTION_TRACE(ut_copy_esimple_to_isimple);

	switch (external_object->type) {
	case ACPI_TYPE_STRING:
	case ACPI_TYPE_BUFFER:
	case ACPI_TYPE_INTEGER:
	case ACPI_TYPE_LOCAL_REFERENCE:

		internal_object = acpi_ut_create_internal_object((u8)
								 external_object->
								 type);
		if (!internal_object) {
			return_ACPI_STATUS(AE_NO_MEMORY);
		}
		break;

	case ACPI_TYPE_ANY:	

		*ret_internal_object = NULL;
		return_ACPI_STATUS(AE_OK);

	default:
		

		ACPI_ERROR((AE_INFO,
			    "Unsupported object type, cannot convert to internal object: %s",
			    acpi_ut_get_type_name(external_object->type)));

		return_ACPI_STATUS(AE_SUPPORT);
	}

	

	switch (external_object->type) {
	case ACPI_TYPE_STRING:

		internal_object->string.pointer =
		    ACPI_ALLOCATE_ZEROED((acpi_size)
					 external_object->string.length + 1);

		if (!internal_object->string.pointer) {
			goto error_exit;
		}

		ACPI_MEMCPY(internal_object->string.pointer,
			    external_object->string.pointer,
			    external_object->string.length);

		internal_object->string.length = external_object->string.length;
		break;

	case ACPI_TYPE_BUFFER:

		internal_object->buffer.pointer =
		    ACPI_ALLOCATE_ZEROED(external_object->buffer.length);
		if (!internal_object->buffer.pointer) {
			goto error_exit;
		}

		ACPI_MEMCPY(internal_object->buffer.pointer,
			    external_object->buffer.pointer,
			    external_object->buffer.length);

		internal_object->buffer.length = external_object->buffer.length;

		

		internal_object->buffer.flags |= AOPOBJ_DATA_VALID;
		break;

	case ACPI_TYPE_INTEGER:

		internal_object->integer.value = external_object->integer.value;
		break;

	case ACPI_TYPE_LOCAL_REFERENCE:

		

		internal_object->reference.class = ACPI_REFCLASS_NAME;
		internal_object->reference.node =
		    external_object->reference.handle;
		break;

	default:
		
		break;
	}

	*ret_internal_object = internal_object;
	return_ACPI_STATUS(AE_OK);

      error_exit:
	acpi_ut_remove_reference(internal_object);
	return_ACPI_STATUS(AE_NO_MEMORY);
}


static acpi_status
acpi_ut_copy_epackage_to_ipackage(union acpi_object *external_object,
				  union acpi_operand_object **internal_object)
{
	acpi_status status = AE_OK;
	union acpi_operand_object *package_object;
	union acpi_operand_object **package_elements;
	u32 i;

	ACPI_FUNCTION_TRACE(ut_copy_epackage_to_ipackage);

	

	package_object =
	    acpi_ut_create_package_object(external_object->package.count);
	if (!package_object) {
		return_ACPI_STATUS(AE_NO_MEMORY);
	}

	package_elements = package_object->package.elements;

	for (i = 0; i < external_object->package.count; i++) {
		status =
		    acpi_ut_copy_eobject_to_iobject(&external_object->package.
						    elements[i],
						    &package_elements[i]);
		if (ACPI_FAILURE(status)) {

			

			package_object->package.count = i;
			package_elements[i] = NULL;
			acpi_ut_remove_reference(package_object);
			return_ACPI_STATUS(status);
		}
	}

	

	package_object->package.flags |= AOPOBJ_DATA_VALID;

	*internal_object = package_object;
	return_ACPI_STATUS(status);
}


acpi_status
acpi_ut_copy_eobject_to_iobject(union acpi_object *external_object,
				union acpi_operand_object **internal_object)
{
	acpi_status status;

	ACPI_FUNCTION_TRACE(ut_copy_eobject_to_iobject);

	if (external_object->type == ACPI_TYPE_PACKAGE) {
		status =
		    acpi_ut_copy_epackage_to_ipackage(external_object,
						      internal_object);
	} else {
		status =
		    acpi_ut_copy_esimple_to_isimple(external_object,
						    internal_object);
	}

	return_ACPI_STATUS(status);
}


static acpi_status
acpi_ut_copy_simple_object(union acpi_operand_object *source_desc,
			   union acpi_operand_object *dest_desc)
{
	u16 reference_count;
	union acpi_operand_object *next_object;
	acpi_status status;
	acpi_size copy_size;

	

	reference_count = dest_desc->common.reference_count;
	next_object = dest_desc->common.next_object;

	copy_size = sizeof(union acpi_operand_object);
	if (ACPI_GET_DESCRIPTOR_TYPE(source_desc) == ACPI_DESC_TYPE_NAMED) {
		copy_size = sizeof(struct acpi_namespace_node);
	}

	ACPI_MEMCPY(ACPI_CAST_PTR(char, dest_desc),
		    ACPI_CAST_PTR(char, source_desc), copy_size);

	

	dest_desc->common.reference_count = reference_count;
	dest_desc->common.next_object = next_object;

	

	dest_desc->common.flags &= ~AOPOBJ_STATIC_POINTER;

	

	switch (dest_desc->common.type) {
	case ACPI_TYPE_BUFFER:
		if ((source_desc->buffer.pointer) &&
		    (source_desc->buffer.length)) {
			dest_desc->buffer.pointer =
			    ACPI_ALLOCATE(source_desc->buffer.length);
			if (!dest_desc->buffer.pointer) {
				return (AE_NO_MEMORY);
			}

			

			ACPI_MEMCPY(dest_desc->buffer.pointer,
				    source_desc->buffer.pointer,
				    source_desc->buffer.length);
		}
		break;

	case ACPI_TYPE_STRING:
		if (source_desc->string.pointer) {
			dest_desc->string.pointer =
			    ACPI_ALLOCATE((acpi_size) source_desc->string.
					  length + 1);
			if (!dest_desc->string.pointer) {
				return (AE_NO_MEMORY);
			}

			

			ACPI_MEMCPY(dest_desc->string.pointer,
				    source_desc->string.pointer,
				    (acpi_size) source_desc->string.length + 1);
		}
		break;

	case ACPI_TYPE_LOCAL_REFERENCE:
		if (source_desc->reference.class == ACPI_REFCLASS_TABLE) {
			break;
		}

		acpi_ut_add_reference(source_desc->reference.object);
		break;

	case ACPI_TYPE_REGION:
		if (dest_desc->region.handler) {
			acpi_ut_add_reference(dest_desc->region.handler);
		}
		break;

	case ACPI_TYPE_MUTEX:

		status = acpi_os_create_mutex(&dest_desc->mutex.os_mutex);
		if (ACPI_FAILURE(status)) {
			return status;
		}
		break;

	case ACPI_TYPE_EVENT:

		status = acpi_os_create_semaphore(ACPI_NO_UNIT_LIMIT, 0,
						  &dest_desc->event.
						  os_semaphore);
		if (ACPI_FAILURE(status)) {
			return status;
		}
		break;

	default:
		
		break;
	}

	return (AE_OK);
}


static acpi_status
acpi_ut_copy_ielement_to_ielement(u8 object_type,
				  union acpi_operand_object *source_object,
				  union acpi_generic_state *state,
				  void *context)
{
	acpi_status status = AE_OK;
	u32 this_index;
	union acpi_operand_object **this_target_ptr;
	union acpi_operand_object *target_object;

	ACPI_FUNCTION_ENTRY();

	this_index = state->pkg.index;
	this_target_ptr = (union acpi_operand_object **)
	    &state->pkg.dest_object->package.elements[this_index];

	switch (object_type) {
	case ACPI_COPY_TYPE_SIMPLE:

		

		if (source_object) {
			target_object =
			    acpi_ut_create_internal_object(source_object->
							   common.type);
			if (!target_object) {
				return (AE_NO_MEMORY);
			}

			status =
			    acpi_ut_copy_simple_object(source_object,
						       target_object);
			if (ACPI_FAILURE(status)) {
				goto error_exit;
			}

			*this_target_ptr = target_object;
		} else {
			

			*this_target_ptr = NULL;
		}
		break;

	case ACPI_COPY_TYPE_PACKAGE:

		target_object =
		    acpi_ut_create_package_object(source_object->package.count);
		if (!target_object) {
			return (AE_NO_MEMORY);
		}

		target_object->common.flags = source_object->common.flags;

		

		state->pkg.this_target_obj = target_object;

		

		*this_target_ptr = target_object;
		break;

	default:
		return (AE_BAD_PARAMETER);
	}

	return (status);

      error_exit:
	acpi_ut_remove_reference(target_object);
	return (status);
}


static acpi_status
acpi_ut_copy_ipackage_to_ipackage(union acpi_operand_object *source_obj,
				  union acpi_operand_object *dest_obj,
				  struct acpi_walk_state *walk_state)
{
	acpi_status status = AE_OK;

	ACPI_FUNCTION_TRACE(ut_copy_ipackage_to_ipackage);

	dest_obj->common.type = source_obj->common.type;
	dest_obj->common.flags = source_obj->common.flags;
	dest_obj->package.count = source_obj->package.count;

	dest_obj->package.elements = ACPI_ALLOCATE_ZEROED(((acpi_size)
							   source_obj->package.
							   count +
							   1) * sizeof(void *));
	if (!dest_obj->package.elements) {
		ACPI_ERROR((AE_INFO, "Package allocation failure"));
		return_ACPI_STATUS(AE_NO_MEMORY);
	}

	status = acpi_ut_walk_package_tree(source_obj, dest_obj,
					   acpi_ut_copy_ielement_to_ielement,
					   walk_state);
	if (ACPI_FAILURE(status)) {

		

		acpi_ut_remove_reference(dest_obj);
	}

	return_ACPI_STATUS(status);
}


acpi_status
acpi_ut_copy_iobject_to_iobject(union acpi_operand_object *source_desc,
				union acpi_operand_object **dest_desc,
				struct acpi_walk_state *walk_state)
{
	acpi_status status = AE_OK;

	ACPI_FUNCTION_TRACE(ut_copy_iobject_to_iobject);

	

	*dest_desc = acpi_ut_create_internal_object(source_desc->common.type);
	if (!*dest_desc) {
		return_ACPI_STATUS(AE_NO_MEMORY);
	}

	

	if (source_desc->common.type == ACPI_TYPE_PACKAGE) {
		status =
		    acpi_ut_copy_ipackage_to_ipackage(source_desc, *dest_desc,
						      walk_state);
	} else {
		status = acpi_ut_copy_simple_object(source_desc, *dest_desc);
	}

	return_ACPI_STATUS(status);
}
