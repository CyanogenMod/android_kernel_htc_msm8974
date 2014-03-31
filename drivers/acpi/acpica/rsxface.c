
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

#include <linux/export.h>
#include <acpi/acpi.h>
#include "accommon.h"
#include "acresrc.h"
#include "acnamesp.h"

#define _COMPONENT          ACPI_RESOURCES
ACPI_MODULE_NAME("rsxface")

#define ACPI_COPY_FIELD(out, in, field)  ((out)->field = (in)->field)
#define ACPI_COPY_ADDRESS(out, in)                      \
	ACPI_COPY_FIELD(out, in, resource_type);             \
	ACPI_COPY_FIELD(out, in, producer_consumer);         \
	ACPI_COPY_FIELD(out, in, decode);                    \
	ACPI_COPY_FIELD(out, in, min_address_fixed);         \
	ACPI_COPY_FIELD(out, in, max_address_fixed);         \
	ACPI_COPY_FIELD(out, in, info);                      \
	ACPI_COPY_FIELD(out, in, granularity);               \
	ACPI_COPY_FIELD(out, in, minimum);                   \
	ACPI_COPY_FIELD(out, in, maximum);                   \
	ACPI_COPY_FIELD(out, in, translation_offset);        \
	ACPI_COPY_FIELD(out, in, address_length);            \
	ACPI_COPY_FIELD(out, in, resource_source);
static acpi_status
acpi_rs_match_vendor_resource(struct acpi_resource *resource, void *context);

static acpi_status
acpi_rs_validate_parameters(acpi_handle device_handle,
			    struct acpi_buffer *buffer,
			    struct acpi_namespace_node **return_node);


static acpi_status
acpi_rs_validate_parameters(acpi_handle device_handle,
			    struct acpi_buffer *buffer,
			    struct acpi_namespace_node **return_node)
{
	acpi_status status;
	struct acpi_namespace_node *node;

	ACPI_FUNCTION_TRACE(rs_validate_parameters);

	if (!device_handle) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	node = acpi_ns_validate_handle(device_handle);
	if (!node) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	if (node->type != ACPI_TYPE_DEVICE) {
		return_ACPI_STATUS(AE_TYPE);
	}

	status = acpi_ut_validate_buffer(buffer);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	*return_node = node;
	return_ACPI_STATUS(AE_OK);
}


acpi_status
acpi_get_irq_routing_table(acpi_handle device_handle,
			   struct acpi_buffer *ret_buffer)
{
	acpi_status status;
	struct acpi_namespace_node *node;

	ACPI_FUNCTION_TRACE(acpi_get_irq_routing_table);

	

	status = acpi_rs_validate_parameters(device_handle, ret_buffer, &node);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	status = acpi_rs_get_prt_method_data(node, ret_buffer);
	return_ACPI_STATUS(status);
}

ACPI_EXPORT_SYMBOL(acpi_get_irq_routing_table)

acpi_status
acpi_get_current_resources(acpi_handle device_handle,
			   struct acpi_buffer *ret_buffer)
{
	acpi_status status;
	struct acpi_namespace_node *node;

	ACPI_FUNCTION_TRACE(acpi_get_current_resources);

	

	status = acpi_rs_validate_parameters(device_handle, ret_buffer, &node);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	status = acpi_rs_get_crs_method_data(node, ret_buffer);
	return_ACPI_STATUS(status);
}

ACPI_EXPORT_SYMBOL(acpi_get_current_resources)
#ifdef ACPI_FUTURE_USAGE
acpi_status
acpi_get_possible_resources(acpi_handle device_handle,
			    struct acpi_buffer *ret_buffer)
{
	acpi_status status;
	struct acpi_namespace_node *node;

	ACPI_FUNCTION_TRACE(acpi_get_possible_resources);

	

	status = acpi_rs_validate_parameters(device_handle, ret_buffer, &node);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	status = acpi_rs_get_prs_method_data(node, ret_buffer);
	return_ACPI_STATUS(status);
}

ACPI_EXPORT_SYMBOL(acpi_get_possible_resources)
#endif				
acpi_status
acpi_set_current_resources(acpi_handle device_handle,
			   struct acpi_buffer *in_buffer)
{
	acpi_status status;
	struct acpi_namespace_node *node;

	ACPI_FUNCTION_TRACE(acpi_set_current_resources);

	

	if ((!in_buffer) || (!in_buffer->pointer) || (!in_buffer->length)) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	

	status = acpi_rs_validate_parameters(device_handle, in_buffer, &node);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	status = acpi_rs_set_srs_method_data(node, in_buffer);
	return_ACPI_STATUS(status);
}

ACPI_EXPORT_SYMBOL(acpi_set_current_resources)

acpi_status
acpi_get_event_resources(acpi_handle device_handle,
			 struct acpi_buffer *ret_buffer)
{
	acpi_status status;
	struct acpi_namespace_node *node;

	ACPI_FUNCTION_TRACE(acpi_get_event_resources);

	

	status = acpi_rs_validate_parameters(device_handle, ret_buffer, &node);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	status = acpi_rs_get_aei_method_data(node, ret_buffer);
	return_ACPI_STATUS(status);
}

ACPI_EXPORT_SYMBOL(acpi_get_event_resources)

acpi_status
acpi_resource_to_address64(struct acpi_resource *resource,
			   struct acpi_resource_address64 *out)
{
	struct acpi_resource_address16 *address16;
	struct acpi_resource_address32 *address32;

	if (!resource || !out) {
		return (AE_BAD_PARAMETER);
	}

	

	switch (resource->type) {
	case ACPI_RESOURCE_TYPE_ADDRESS16:

		address16 =
		    ACPI_CAST_PTR(struct acpi_resource_address16,
				  &resource->data);
		ACPI_COPY_ADDRESS(out, address16);
		break;

	case ACPI_RESOURCE_TYPE_ADDRESS32:

		address32 =
		    ACPI_CAST_PTR(struct acpi_resource_address32,
				  &resource->data);
		ACPI_COPY_ADDRESS(out, address32);
		break;

	case ACPI_RESOURCE_TYPE_ADDRESS64:

		

		ACPI_MEMCPY(out, &resource->data,
			    sizeof(struct acpi_resource_address64));
		break;

	default:
		return (AE_BAD_PARAMETER);
	}

	return (AE_OK);
}

ACPI_EXPORT_SYMBOL(acpi_resource_to_address64)

acpi_status
acpi_get_vendor_resource(acpi_handle device_handle,
			 char *name,
			 struct acpi_vendor_uuid * uuid,
			 struct acpi_buffer * ret_buffer)
{
	struct acpi_vendor_walk_info info;
	acpi_status status;

	

	if (!uuid || !ret_buffer) {
		return (AE_BAD_PARAMETER);
	}

	info.uuid = uuid;
	info.buffer = ret_buffer;
	info.status = AE_NOT_EXIST;

	

	status =
	    acpi_walk_resources(device_handle, name,
				acpi_rs_match_vendor_resource, &info);
	if (ACPI_FAILURE(status)) {
		return (status);
	}

	return (info.status);
}

ACPI_EXPORT_SYMBOL(acpi_get_vendor_resource)

static acpi_status
acpi_rs_match_vendor_resource(struct acpi_resource *resource, void *context)
{
	struct acpi_vendor_walk_info *info = context;
	struct acpi_resource_vendor_typed *vendor;
	struct acpi_buffer *buffer;
	acpi_status status;

	

	if (resource->type != ACPI_RESOURCE_TYPE_VENDOR) {
		return (AE_OK);
	}

	vendor = &resource->data.vendor_typed;

	if ((vendor->byte_length < (ACPI_UUID_LENGTH + 1)) ||
	    (vendor->uuid_subtype != info->uuid->subtype) ||
	    (ACPI_MEMCMP(vendor->uuid, info->uuid->data, ACPI_UUID_LENGTH))) {
		return (AE_OK);
	}

	

	buffer = info->buffer;
	status = acpi_ut_initialize_buffer(buffer, resource->length);
	if (ACPI_FAILURE(status)) {
		return (status);
	}

	

	ACPI_MEMCPY(buffer->pointer, resource, resource->length);
	buffer->length = resource->length;

	

	info->status = AE_OK;
	return (AE_CTRL_TERMINATE);
}

acpi_status
acpi_walk_resources(acpi_handle device_handle,
		    char *name,
		    acpi_walk_resource_callback user_function, void *context)
{
	acpi_status status;
	struct acpi_buffer buffer;
	struct acpi_resource *resource;
	struct acpi_resource *resource_end;

	ACPI_FUNCTION_TRACE(acpi_walk_resources);

	

	if (!device_handle || !user_function || !name ||
	    (!ACPI_COMPARE_NAME(name, METHOD_NAME__CRS) &&
	     !ACPI_COMPARE_NAME(name, METHOD_NAME__PRS) &&
	     !ACPI_COMPARE_NAME(name, METHOD_NAME__AEI))) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	

	buffer.length = ACPI_ALLOCATE_LOCAL_BUFFER;
	status = acpi_rs_get_method_data(device_handle, name, &buffer);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	

	resource = ACPI_CAST_PTR(struct acpi_resource, buffer.pointer);
	resource_end =
	    ACPI_ADD_PTR(struct acpi_resource, buffer.pointer, buffer.length);

	

	while (resource < resource_end) {

		

		if (resource->type > ACPI_RESOURCE_TYPE_MAX) {
			status = AE_AML_INVALID_RESOURCE_TYPE;
			break;
		}

		

		status = user_function(resource, context);
		if (ACPI_FAILURE(status)) {
			if (status == AE_CTRL_TERMINATE) {

				

				status = AE_OK;
			}
			break;
		}

		

		if (resource->type == ACPI_RESOURCE_TYPE_END_TAG) {
			break;
		}

		

		resource =
		    ACPI_ADD_PTR(struct acpi_resource, resource,
				 resource->length);
	}

	ACPI_FREE(buffer.pointer);
	return_ACPI_STATUS(status);
}

ACPI_EXPORT_SYMBOL(acpi_walk_resources)
