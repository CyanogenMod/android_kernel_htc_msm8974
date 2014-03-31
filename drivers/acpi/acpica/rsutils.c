
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
#include "acresrc.h"

#define _COMPONENT          ACPI_RESOURCES
ACPI_MODULE_NAME("rsutils")

u8 acpi_rs_decode_bitmask(u16 mask, u8 * list)
{
	u8 i;
	u8 bit_count;

	ACPI_FUNCTION_ENTRY();

	

	for (i = 0, bit_count = 0; mask; i++) {
		if (mask & 0x0001) {
			list[bit_count] = i;
			bit_count++;
		}

		mask >>= 1;
	}

	return (bit_count);
}


u16 acpi_rs_encode_bitmask(u8 * list, u8 count)
{
	u32 i;
	u16 mask;

	ACPI_FUNCTION_ENTRY();

	

	for (i = 0, mask = 0; i < count; i++) {
		mask |= (0x1 << list[i]);
	}

	return mask;
}


void
acpi_rs_move_data(void *destination, void *source, u16 item_count, u8 move_type)
{
	u32 i;

	ACPI_FUNCTION_ENTRY();

	

	for (i = 0; i < item_count; i++) {
		switch (move_type) {
		case ACPI_RSC_MOVE8:
		case ACPI_RSC_MOVE_GPIO_RES:
		case ACPI_RSC_MOVE_SERIAL_VEN:
		case ACPI_RSC_MOVE_SERIAL_RES:
			ACPI_MEMCPY(destination, source, item_count);
			return;

		case ACPI_RSC_MOVE16:
		case ACPI_RSC_MOVE_GPIO_PIN:
			ACPI_MOVE_16_TO_16(&ACPI_CAST_PTR(u16, destination)[i],
					   &ACPI_CAST_PTR(u16, source)[i]);
			break;

		case ACPI_RSC_MOVE32:
			ACPI_MOVE_32_TO_32(&ACPI_CAST_PTR(u32, destination)[i],
					   &ACPI_CAST_PTR(u32, source)[i]);
			break;

		case ACPI_RSC_MOVE64:
			ACPI_MOVE_64_TO_64(&ACPI_CAST_PTR(u64, destination)[i],
					   &ACPI_CAST_PTR(u64, source)[i]);
			break;

		default:
			return;
		}
	}
}


void
acpi_rs_set_resource_length(acpi_rsdesc_size total_length,
			    union aml_resource *aml)
{
	acpi_rs_length resource_length;

	ACPI_FUNCTION_ENTRY();

	

	resource_length = (acpi_rs_length)
	    (total_length - acpi_ut_get_resource_header_length(aml));

	

	if (aml->small_header.descriptor_type & ACPI_RESOURCE_NAME_LARGE) {

		

		ACPI_MOVE_16_TO_16(&aml->large_header.resource_length,
				   &resource_length);
	} else {
		

		aml->small_header.descriptor_type = (u8)

		    
		    ((aml->small_header.
		      descriptor_type & ~ACPI_RESOURCE_NAME_SMALL_LENGTH_MASK)

		     | resource_length);
	}
}


void
acpi_rs_set_resource_header(u8 descriptor_type,
			    acpi_rsdesc_size total_length,
			    union aml_resource *aml)
{
	ACPI_FUNCTION_ENTRY();

	

	aml->small_header.descriptor_type = descriptor_type;

	

	acpi_rs_set_resource_length(total_length, aml);
}


static u16 acpi_rs_strcpy(char *destination, char *source)
{
	u16 i;

	ACPI_FUNCTION_ENTRY();

	for (i = 0; source[i]; i++) {
		destination[i] = source[i];
	}

	destination[i] = 0;

	

	return ((u16) (i + 1));
}


acpi_rs_length
acpi_rs_get_resource_source(acpi_rs_length resource_length,
			    acpi_rs_length minimum_length,
			    struct acpi_resource_source * resource_source,
			    union aml_resource * aml, char *string_ptr)
{
	acpi_rsdesc_size total_length;
	u8 *aml_resource_source;

	ACPI_FUNCTION_ENTRY();

	total_length =
	    resource_length + sizeof(struct aml_resource_large_header);
	aml_resource_source = ACPI_ADD_PTR(u8, aml, minimum_length);

	if (total_length > (acpi_rsdesc_size) (minimum_length + 1)) {

		

		resource_source->index = aml_resource_source[0];

		resource_source->string_ptr = string_ptr;
		if (!string_ptr) {
			resource_source->string_ptr =
			    ACPI_ADD_PTR(char, resource_source,
					 sizeof(struct acpi_resource_source));
		}

		total_length = (u32)
		ACPI_STRLEN(ACPI_CAST_PTR(char, &aml_resource_source[1])) + 1;
		total_length = (u32) ACPI_ROUND_UP_TO_NATIVE_WORD(total_length);

		ACPI_MEMSET(resource_source->string_ptr, 0, total_length);

		

		resource_source->string_length =
		    acpi_rs_strcpy(resource_source->string_ptr,
				   ACPI_CAST_PTR(char,
						 &aml_resource_source[1]));

		return ((acpi_rs_length) total_length);
	}

	

	resource_source->index = 0;
	resource_source->string_length = 0;
	resource_source->string_ptr = NULL;
	return (0);
}


acpi_rsdesc_size
acpi_rs_set_resource_source(union aml_resource * aml,
			    acpi_rs_length minimum_length,
			    struct acpi_resource_source * resource_source)
{
	u8 *aml_resource_source;
	acpi_rsdesc_size descriptor_length;

	ACPI_FUNCTION_ENTRY();

	descriptor_length = minimum_length;

	

	if (resource_source->string_length) {

		

		aml_resource_source = ACPI_ADD_PTR(u8, aml, minimum_length);

		

		aml_resource_source[0] = (u8) resource_source->index;

		

		ACPI_STRCPY(ACPI_CAST_PTR(char, &aml_resource_source[1]),
			    resource_source->string_ptr);

		descriptor_length +=
		    ((acpi_rsdesc_size) resource_source->string_length + 1);
	}

	

	return (descriptor_length);
}


acpi_status
acpi_rs_get_prt_method_data(struct acpi_namespace_node * node,
			    struct acpi_buffer * ret_buffer)
{
	union acpi_operand_object *obj_desc;
	acpi_status status;

	ACPI_FUNCTION_TRACE(rs_get_prt_method_data);

	

	

	status = acpi_ut_evaluate_object(node, METHOD_NAME__PRT,
					 ACPI_BTYPE_PACKAGE, &obj_desc);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	status = acpi_rs_create_pci_routing_table(obj_desc, ret_buffer);

	

	acpi_ut_remove_reference(obj_desc);
	return_ACPI_STATUS(status);
}


acpi_status
acpi_rs_get_crs_method_data(struct acpi_namespace_node *node,
			    struct acpi_buffer *ret_buffer)
{
	union acpi_operand_object *obj_desc;
	acpi_status status;

	ACPI_FUNCTION_TRACE(rs_get_crs_method_data);

	

	

	status = acpi_ut_evaluate_object(node, METHOD_NAME__CRS,
					 ACPI_BTYPE_BUFFER, &obj_desc);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	status = acpi_rs_create_resource_list(obj_desc, ret_buffer);

	

	acpi_ut_remove_reference(obj_desc);
	return_ACPI_STATUS(status);
}


#ifdef ACPI_FUTURE_USAGE
acpi_status
acpi_rs_get_prs_method_data(struct acpi_namespace_node *node,
			    struct acpi_buffer *ret_buffer)
{
	union acpi_operand_object *obj_desc;
	acpi_status status;

	ACPI_FUNCTION_TRACE(rs_get_prs_method_data);

	

	

	status = acpi_ut_evaluate_object(node, METHOD_NAME__PRS,
					 ACPI_BTYPE_BUFFER, &obj_desc);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	status = acpi_rs_create_resource_list(obj_desc, ret_buffer);

	

	acpi_ut_remove_reference(obj_desc);
	return_ACPI_STATUS(status);
}
#endif				


acpi_status
acpi_rs_get_aei_method_data(struct acpi_namespace_node *node,
			    struct acpi_buffer *ret_buffer)
{
	union acpi_operand_object *obj_desc;
	acpi_status status;

	ACPI_FUNCTION_TRACE(rs_get_aei_method_data);

	

	

	status = acpi_ut_evaluate_object(node, METHOD_NAME__AEI,
					 ACPI_BTYPE_BUFFER, &obj_desc);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	status = acpi_rs_create_resource_list(obj_desc, ret_buffer);

	

	acpi_ut_remove_reference(obj_desc);
	return_ACPI_STATUS(status);
}


acpi_status
acpi_rs_get_method_data(acpi_handle handle,
			char *path, struct acpi_buffer *ret_buffer)
{
	union acpi_operand_object *obj_desc;
	acpi_status status;

	ACPI_FUNCTION_TRACE(rs_get_method_data);

	

	

	status =
	    acpi_ut_evaluate_object(handle, path, ACPI_BTYPE_BUFFER, &obj_desc);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	status = acpi_rs_create_resource_list(obj_desc, ret_buffer);

	

	acpi_ut_remove_reference(obj_desc);
	return_ACPI_STATUS(status);
}


acpi_status
acpi_rs_set_srs_method_data(struct acpi_namespace_node *node,
			    struct acpi_buffer *in_buffer)
{
	struct acpi_evaluate_info *info;
	union acpi_operand_object *args[2];
	acpi_status status;
	struct acpi_buffer buffer;

	ACPI_FUNCTION_TRACE(rs_set_srs_method_data);

	

	info = ACPI_ALLOCATE_ZEROED(sizeof(struct acpi_evaluate_info));
	if (!info) {
		return_ACPI_STATUS(AE_NO_MEMORY);
	}

	info->prefix_node = node;
	info->pathname = METHOD_NAME__SRS;
	info->parameters = args;
	info->flags = ACPI_IGNORE_RETURN_VALUE;

	buffer.length = ACPI_ALLOCATE_LOCAL_BUFFER;
	status = acpi_rs_create_aml_resources(in_buffer->pointer, &buffer);
	if (ACPI_FAILURE(status)) {
		goto cleanup;
	}

	

	args[0] = acpi_ut_create_internal_object(ACPI_TYPE_BUFFER);
	if (!args[0]) {
		ACPI_FREE(buffer.pointer);
		status = AE_NO_MEMORY;
		goto cleanup;
	}

	args[0]->buffer.length = (u32) buffer.length;
	args[0]->buffer.pointer = buffer.pointer;
	args[0]->common.flags = AOPOBJ_DATA_VALID;
	args[1] = NULL;

	

	status = acpi_ns_evaluate(info);

	

	acpi_ut_remove_reference(args[0]);

      cleanup:
	ACPI_FREE(info);
	return_ACPI_STATUS(status);
}
