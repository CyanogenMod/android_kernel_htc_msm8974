
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
#include "acnamesp.h"
#include "acinterp.h"

#define _COMPONENT          ACPI_NAMESPACE
ACPI_MODULE_NAME("nsxfeval")

static void acpi_ns_resolve_references(struct acpi_evaluate_info *info);


acpi_status
acpi_evaluate_object_typed(acpi_handle handle,
			   acpi_string pathname,
			   struct acpi_object_list *external_params,
			   struct acpi_buffer *return_buffer,
			   acpi_object_type return_type)
{
	acpi_status status;
	u8 must_free = FALSE;

	ACPI_FUNCTION_TRACE(acpi_evaluate_object_typed);

	

	if (!return_buffer) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	if (return_buffer->length == ACPI_ALLOCATE_BUFFER) {
		must_free = TRUE;
	}

	

	status =
	    acpi_evaluate_object(handle, pathname, external_params,
				 return_buffer);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	

	if (return_type == ACPI_TYPE_ANY) {
		return_ACPI_STATUS(AE_OK);
	}

	if (return_buffer->length == 0) {

		

		ACPI_ERROR((AE_INFO, "No return value"));
		return_ACPI_STATUS(AE_NULL_OBJECT);
	}

	

	if (((union acpi_object *)return_buffer->pointer)->type == return_type) {
		return_ACPI_STATUS(AE_OK);
	}

	

	ACPI_ERROR((AE_INFO,
		    "Incorrect return type [%s] requested [%s]",
		    acpi_ut_get_type_name(((union acpi_object *)return_buffer->
					   pointer)->type),
		    acpi_ut_get_type_name(return_type)));

	if (must_free) {

		

		ACPI_FREE(return_buffer->pointer);
		return_buffer->pointer = NULL;
	}

	return_buffer->length = 0;
	return_ACPI_STATUS(AE_TYPE);
}

ACPI_EXPORT_SYMBOL(acpi_evaluate_object_typed)

acpi_status
acpi_evaluate_object(acpi_handle handle,
		     acpi_string pathname,
		     struct acpi_object_list *external_params,
		     struct acpi_buffer *return_buffer)
{
	acpi_status status;
	struct acpi_evaluate_info *info;
	acpi_size buffer_space_needed;
	u32 i;

	ACPI_FUNCTION_TRACE(acpi_evaluate_object);

	

	info = ACPI_ALLOCATE_ZEROED(sizeof(struct acpi_evaluate_info));
	if (!info) {
		return_ACPI_STATUS(AE_NO_MEMORY);
	}

	info->pathname = pathname;

	

	info->prefix_node = acpi_ns_validate_handle(handle);
	if (!info->prefix_node) {
		status = AE_BAD_PARAMETER;
		goto cleanup;
	}

	if (external_params && external_params->count) {
		info->parameters = ACPI_ALLOCATE_ZEROED(((acpi_size)
							 external_params->
							 count +
							 1) * sizeof(void *));
		if (!info->parameters) {
			status = AE_NO_MEMORY;
			goto cleanup;
		}

		

		for (i = 0; i < external_params->count; i++) {
			status =
			    acpi_ut_copy_eobject_to_iobject(&external_params->
							    pointer[i],
							    &info->
							    parameters[i]);
			if (ACPI_FAILURE(status)) {
				goto cleanup;
			}
		}
		info->parameters[external_params->count] = NULL;
	}

	if ((pathname) && (acpi_ns_valid_root_prefix(pathname[0]))) {

		

		info->prefix_node = NULL;
		status = acpi_ns_evaluate(info);
	} else if (!handle) {
		if (!pathname) {
			ACPI_DEBUG_PRINT((ACPI_DB_INFO,
					  "Both Handle and Pathname are NULL"));
		} else {
			ACPI_DEBUG_PRINT((ACPI_DB_INFO,
					  "Null Handle with relative pathname [%s]",
					  pathname));
		}

		status = AE_BAD_PARAMETER;
	} else {
		

		status = acpi_ns_evaluate(info);
	}

	if (return_buffer) {
		if (!info->return_object) {
			return_buffer->length = 0;
		} else {
			if (ACPI_GET_DESCRIPTOR_TYPE(info->return_object) ==
			    ACPI_DESC_TYPE_NAMED) {
				status = AE_TYPE;
				info->return_object = NULL;	
				return_buffer->length = 0;
			}

			if (ACPI_SUCCESS(status)) {

				

				acpi_ns_resolve_references(info);

				

				status =
				    acpi_ut_get_object_size(info->return_object,
							    &buffer_space_needed);
				if (ACPI_SUCCESS(status)) {

					

					status =
					    acpi_ut_initialize_buffer
					    (return_buffer,
					     buffer_space_needed);
					if (ACPI_FAILURE(status)) {
						ACPI_DEBUG_PRINT((ACPI_DB_INFO,
								  "Needed buffer size %X, %s\n",
								  (u32)
								  buffer_space_needed,
								  acpi_format_exception
								  (status)));
					} else {
						

						status =
						    acpi_ut_copy_iobject_to_eobject
						    (info->return_object,
						     return_buffer);
					}
				}
			}
		}
	}

	if (info->return_object) {
		acpi_ex_enter_interpreter();

		

		acpi_ut_remove_reference(info->return_object);
		acpi_ex_exit_interpreter();
	}

      cleanup:

	

	if (info->parameters) {

		

		acpi_ut_delete_internal_object_list(info->parameters);
	}

	ACPI_FREE(info);
	return_ACPI_STATUS(status);
}

ACPI_EXPORT_SYMBOL(acpi_evaluate_object)

static void acpi_ns_resolve_references(struct acpi_evaluate_info *info)
{
	union acpi_operand_object *obj_desc = NULL;
	struct acpi_namespace_node *node;

	

	if ((info->return_object)->common.type != ACPI_TYPE_LOCAL_REFERENCE) {
		return;
	}

	switch (info->return_object->reference.class) {
	case ACPI_REFCLASS_INDEX:

		obj_desc = *(info->return_object->reference.where);
		break;

	case ACPI_REFCLASS_REFOF:

		node = info->return_object->reference.object;
		if (node) {
			obj_desc = node->object;
		}
		break;

	default:
		return;
	}

	

	if (obj_desc) {
		acpi_ut_add_reference(obj_desc);
		acpi_ut_remove_reference(info->return_object);
		info->return_object = obj_desc;
	}

	return;
}


acpi_status
acpi_walk_namespace(acpi_object_type type,
		    acpi_handle start_object,
		    u32 max_depth,
		    acpi_walk_callback pre_order_visit,
		    acpi_walk_callback post_order_visit,
		    void *context, void **return_value)
{
	acpi_status status;

	ACPI_FUNCTION_TRACE(acpi_walk_namespace);

	

	if ((type > ACPI_TYPE_LOCAL_MAX) ||
	    (!max_depth) || (!pre_order_visit && !post_order_visit)) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	status = acpi_ut_acquire_read_lock(&acpi_gbl_namespace_rw_lock);
	if (ACPI_FAILURE(status)) {
		return status;
	}

	status = acpi_ut_acquire_mutex(ACPI_MTX_NAMESPACE);
	if (ACPI_FAILURE(status)) {
		goto unlock_and_exit;
	}

	status = acpi_ns_walk_namespace(type, start_object, max_depth,
					ACPI_NS_WALK_UNLOCK, pre_order_visit,
					post_order_visit, context,
					return_value);

	(void)acpi_ut_release_mutex(ACPI_MTX_NAMESPACE);

      unlock_and_exit:
	(void)acpi_ut_release_read_lock(&acpi_gbl_namespace_rw_lock);
	return_ACPI_STATUS(status);
}

ACPI_EXPORT_SYMBOL(acpi_walk_namespace)

static acpi_status
acpi_ns_get_device_callback(acpi_handle obj_handle,
			    u32 nesting_level,
			    void *context, void **return_value)
{
	struct acpi_get_devices_info *info = context;
	acpi_status status;
	struct acpi_namespace_node *node;
	u32 flags;
	struct acpica_device_id *hid;
	struct acpica_device_id_list *cid;
	u32 i;
	u8 found;
	int no_match;

	status = acpi_ut_acquire_mutex(ACPI_MTX_NAMESPACE);
	if (ACPI_FAILURE(status)) {
		return (status);
	}

	node = acpi_ns_validate_handle(obj_handle);
	status = acpi_ut_release_mutex(ACPI_MTX_NAMESPACE);
	if (ACPI_FAILURE(status)) {
		return (status);
	}

	if (!node) {
		return (AE_BAD_PARAMETER);
	}

	if (info->hid != NULL) {
		status = acpi_ut_execute_HID(node, &hid);
		if (status == AE_NOT_FOUND) {
			return (AE_OK);
		} else if (ACPI_FAILURE(status)) {
			return (AE_CTRL_DEPTH);
		}

		no_match = ACPI_STRCMP(hid->string, info->hid);
		ACPI_FREE(hid);

		if (no_match) {
			status = acpi_ut_execute_CID(node, &cid);
			if (status == AE_NOT_FOUND) {
				return (AE_OK);
			} else if (ACPI_FAILURE(status)) {
				return (AE_CTRL_DEPTH);
			}

			

			found = 0;
			for (i = 0; i < cid->count; i++) {
				if (ACPI_STRCMP(cid->ids[i].string, info->hid)
				    == 0) {
					found = 1;
					break;
				}
			}
			ACPI_FREE(cid);
			if (!found)
				return (AE_OK);
		}
	}

	

	status = acpi_ut_execute_STA(node, &flags);
	if (ACPI_FAILURE(status)) {
		return (AE_CTRL_DEPTH);
	}

	if (!(flags & ACPI_STA_DEVICE_PRESENT) &&
	    !(flags & ACPI_STA_DEVICE_FUNCTIONING)) {
		return (AE_CTRL_DEPTH);
	}

	

	status = info->user_function(obj_handle, nesting_level, info->context,
				     return_value);
	return (status);
}


acpi_status
acpi_get_devices(const char *HID,
		 acpi_walk_callback user_function,
		 void *context, void **return_value)
{
	acpi_status status;
	struct acpi_get_devices_info info;

	ACPI_FUNCTION_TRACE(acpi_get_devices);

	

	if (!user_function) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	info.hid = HID;
	info.context = context;
	info.user_function = user_function;

	status = acpi_ut_acquire_mutex(ACPI_MTX_NAMESPACE);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	status = acpi_ns_walk_namespace(ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT,
					ACPI_UINT32_MAX, ACPI_NS_WALK_UNLOCK,
					acpi_ns_get_device_callback, NULL,
					&info, return_value);

	(void)acpi_ut_release_mutex(ACPI_MTX_NAMESPACE);
	return_ACPI_STATUS(status);
}

ACPI_EXPORT_SYMBOL(acpi_get_devices)

acpi_status
acpi_attach_data(acpi_handle obj_handle,
		 acpi_object_handler handler, void *data)
{
	struct acpi_namespace_node *node;
	acpi_status status;

	

	if (!obj_handle || !handler || !data) {
		return (AE_BAD_PARAMETER);
	}

	status = acpi_ut_acquire_mutex(ACPI_MTX_NAMESPACE);
	if (ACPI_FAILURE(status)) {
		return (status);
	}

	

	node = acpi_ns_validate_handle(obj_handle);
	if (!node) {
		status = AE_BAD_PARAMETER;
		goto unlock_and_exit;
	}

	status = acpi_ns_attach_data(node, handler, data);

      unlock_and_exit:
	(void)acpi_ut_release_mutex(ACPI_MTX_NAMESPACE);
	return (status);
}

ACPI_EXPORT_SYMBOL(acpi_attach_data)

acpi_status
acpi_detach_data(acpi_handle obj_handle, acpi_object_handler handler)
{
	struct acpi_namespace_node *node;
	acpi_status status;

	

	if (!obj_handle || !handler) {
		return (AE_BAD_PARAMETER);
	}

	status = acpi_ut_acquire_mutex(ACPI_MTX_NAMESPACE);
	if (ACPI_FAILURE(status)) {
		return (status);
	}

	

	node = acpi_ns_validate_handle(obj_handle);
	if (!node) {
		status = AE_BAD_PARAMETER;
		goto unlock_and_exit;
	}

	status = acpi_ns_detach_data(node, handler);

      unlock_and_exit:
	(void)acpi_ut_release_mutex(ACPI_MTX_NAMESPACE);
	return (status);
}

ACPI_EXPORT_SYMBOL(acpi_detach_data)

acpi_status
acpi_get_data(acpi_handle obj_handle, acpi_object_handler handler, void **data)
{
	struct acpi_namespace_node *node;
	acpi_status status;

	

	if (!obj_handle || !handler || !data) {
		return (AE_BAD_PARAMETER);
	}

	status = acpi_ut_acquire_mutex(ACPI_MTX_NAMESPACE);
	if (ACPI_FAILURE(status)) {
		return (status);
	}

	

	node = acpi_ns_validate_handle(obj_handle);
	if (!node) {
		status = AE_BAD_PARAMETER;
		goto unlock_and_exit;
	}

	status = acpi_ns_get_attached_data(node, handler, data);

      unlock_and_exit:
	(void)acpi_ut_release_mutex(ACPI_MTX_NAMESPACE);
	return (status);
}

ACPI_EXPORT_SYMBOL(acpi_get_data)
