
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
#include "acparser.h"
#include "amlcode.h"

#define _COMPONENT          ACPI_NAMESPACE
ACPI_MODULE_NAME("nsxfname")

static char *acpi_ns_copy_device_id(struct acpica_device_id *dest,
				    struct acpica_device_id *source,
				    char *string_area);


acpi_status
acpi_get_handle(acpi_handle parent,
		acpi_string pathname, acpi_handle * ret_handle)
{
	acpi_status status;
	struct acpi_namespace_node *node = NULL;
	struct acpi_namespace_node *prefix_node = NULL;

	ACPI_FUNCTION_ENTRY();

	

	if (!ret_handle || !pathname) {
		return (AE_BAD_PARAMETER);
	}

	

	if (parent) {
		prefix_node = acpi_ns_validate_handle(parent);
		if (!prefix_node) {
			return (AE_BAD_PARAMETER);
		}
	}

	if (acpi_ns_valid_root_prefix(pathname[0])) {

		

		

		if (!ACPI_STRCMP(pathname, ACPI_NS_ROOT_PATH)) {
			*ret_handle =
			    ACPI_CAST_PTR(acpi_handle, acpi_gbl_root_node);
			return (AE_OK);
		}
	} else if (!prefix_node) {

		

		return (AE_BAD_PARAMETER);
	}

	

	status =
	    acpi_ns_get_node(prefix_node, pathname, ACPI_NS_NO_UPSEARCH, &node);
	if (ACPI_SUCCESS(status)) {
		*ret_handle = ACPI_CAST_PTR(acpi_handle, node);
	}

	return (status);
}

ACPI_EXPORT_SYMBOL(acpi_get_handle)

acpi_status
acpi_get_name(acpi_handle handle, u32 name_type, struct acpi_buffer * buffer)
{
	acpi_status status;
	struct acpi_namespace_node *node;

	

	if (name_type > ACPI_NAME_TYPE_MAX) {
		return (AE_BAD_PARAMETER);
	}

	status = acpi_ut_validate_buffer(buffer);
	if (ACPI_FAILURE(status)) {
		return (status);
	}

	if (name_type == ACPI_FULL_PATHNAME) {

		

		status = acpi_ns_handle_to_pathname(handle, buffer);
		return (status);
	}

	status = acpi_ut_acquire_mutex(ACPI_MTX_NAMESPACE);
	if (ACPI_FAILURE(status)) {
		return (status);
	}

	node = acpi_ns_validate_handle(handle);
	if (!node) {
		status = AE_BAD_PARAMETER;
		goto unlock_and_exit;
	}

	

	status = acpi_ut_initialize_buffer(buffer, ACPI_PATH_SEGMENT_LENGTH);
	if (ACPI_FAILURE(status)) {
		goto unlock_and_exit;
	}

	

	ACPI_STRNCPY(buffer->pointer, acpi_ut_get_node_name(node),
		     ACPI_NAME_SIZE);
	((char *)buffer->pointer)[ACPI_NAME_SIZE] = 0;
	status = AE_OK;

      unlock_and_exit:

	(void)acpi_ut_release_mutex(ACPI_MTX_NAMESPACE);
	return (status);
}

ACPI_EXPORT_SYMBOL(acpi_get_name)

static char *acpi_ns_copy_device_id(struct acpica_device_id *dest,
				    struct acpica_device_id *source,
				    char *string_area)
{
	

	dest->string = string_area;
	dest->length = source->length;

	

	ACPI_MEMCPY(string_area, source->string, source->length);
	return (string_area + source->length);
}


acpi_status
acpi_get_object_info(acpi_handle handle,
		     struct acpi_device_info **return_buffer)
{
	struct acpi_namespace_node *node;
	struct acpi_device_info *info;
	struct acpica_device_id_list *cid_list = NULL;
	struct acpica_device_id *hid = NULL;
	struct acpica_device_id *uid = NULL;
	char *next_id_string;
	acpi_object_type type;
	acpi_name name;
	u8 param_count = 0;
	u8 valid = 0;
	u32 info_size;
	u32 i;
	acpi_status status;

	

	if (!handle || !return_buffer) {
		return (AE_BAD_PARAMETER);
	}

	status = acpi_ut_acquire_mutex(ACPI_MTX_NAMESPACE);
	if (ACPI_FAILURE(status)) {
		goto cleanup;
	}

	node = acpi_ns_validate_handle(handle);
	if (!node) {
		(void)acpi_ut_release_mutex(ACPI_MTX_NAMESPACE);
		return (AE_BAD_PARAMETER);
	}

	

	info_size = sizeof(struct acpi_device_info);
	type = node->type;
	name = node->name.integer;

	if (node->type == ACPI_TYPE_METHOD) {
		param_count = node->object->method.param_count;
	}

	status = acpi_ut_release_mutex(ACPI_MTX_NAMESPACE);
	if (ACPI_FAILURE(status)) {
		return (status);
	}

	if ((type == ACPI_TYPE_DEVICE) || (type == ACPI_TYPE_PROCESSOR)) {

		

		status = acpi_ut_execute_HID(node, &hid);
		if (ACPI_SUCCESS(status)) {
			info_size += hid->length;
			valid |= ACPI_VALID_HID;
		}

		

		status = acpi_ut_execute_UID(node, &uid);
		if (ACPI_SUCCESS(status)) {
			info_size += uid->length;
			valid |= ACPI_VALID_UID;
		}

		

		status = acpi_ut_execute_CID(node, &cid_list);
		if (ACPI_SUCCESS(status)) {

			

			info_size +=
			    (cid_list->list_size -
			     sizeof(struct acpica_device_id_list));
			valid |= ACPI_VALID_CID;
		}
	}

	info = ACPI_ALLOCATE_ZEROED(info_size);
	if (!info) {
		status = AE_NO_MEMORY;
		goto cleanup;
	}

	

	if ((type == ACPI_TYPE_DEVICE) || (type == ACPI_TYPE_PROCESSOR)) {

		

		status = acpi_ut_execute_STA(node, &info->current_status);
		if (ACPI_SUCCESS(status)) {
			valid |= ACPI_VALID_STA;
		}

		

		status = acpi_ut_evaluate_numeric_object(METHOD_NAME__ADR, node,
							 &info->address);
		if (ACPI_SUCCESS(status)) {
			valid |= ACPI_VALID_ADR;
		}

		

		status = acpi_ut_execute_power_methods(node,
						       acpi_gbl_lowest_dstate_names,
						       ACPI_NUM_sx_w_METHODS,
						       info->lowest_dstates);
		if (ACPI_SUCCESS(status)) {
			valid |= ACPI_VALID_SXWS;
		}

		

		status = acpi_ut_execute_power_methods(node,
						       acpi_gbl_highest_dstate_names,
						       ACPI_NUM_sx_d_METHODS,
						       info->highest_dstates);
		if (ACPI_SUCCESS(status)) {
			valid |= ACPI_VALID_SXDS;
		}
	}

	next_id_string = ACPI_CAST_PTR(char, info->compatible_id_list.ids);
	if (cid_list) {

		

		next_id_string +=
		    ((acpi_size) cid_list->count *
		     sizeof(struct acpica_device_id));
	}

	if (hid) {
		next_id_string = acpi_ns_copy_device_id(&info->hardware_id,
							hid, next_id_string);

		if (acpi_ut_is_pci_root_bridge(hid->string)) {
			info->flags |= ACPI_PCI_ROOT_BRIDGE;
		}
	}

	if (uid) {
		next_id_string = acpi_ns_copy_device_id(&info->unique_id,
							uid, next_id_string);
	}

	if (cid_list) {
		info->compatible_id_list.count = cid_list->count;
		info->compatible_id_list.list_size = cid_list->list_size;

		

		for (i = 0; i < cid_list->count; i++) {
			next_id_string =
			    acpi_ns_copy_device_id(&info->compatible_id_list.
						   ids[i], &cid_list->ids[i],
						   next_id_string);

			if (acpi_ut_is_pci_root_bridge(cid_list->ids[i].string)) {
				info->flags |= ACPI_PCI_ROOT_BRIDGE;
			}
		}
	}

	

	info->info_size = info_size;
	info->type = type;
	info->name = name;
	info->param_count = param_count;
	info->valid = valid;

	*return_buffer = info;
	status = AE_OK;

      cleanup:
	if (hid) {
		ACPI_FREE(hid);
	}
	if (uid) {
		ACPI_FREE(uid);
	}
	if (cid_list) {
		ACPI_FREE(cid_list);
	}
	return (status);
}

ACPI_EXPORT_SYMBOL(acpi_get_object_info)

/******************************************************************************
 *
 * FUNCTION:    acpi_install_method
 *
 * PARAMETERS:  Buffer         - An ACPI table containing one control method
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Install a control method into the namespace. If the method
 *              name already exists in the namespace, it is overwritten. The
 *              input buffer must contain a valid DSDT or SSDT containing a
 *              single control method.
 *
 ******************************************************************************/
acpi_status acpi_install_method(u8 *buffer)
{
	struct acpi_table_header *table =
	    ACPI_CAST_PTR(struct acpi_table_header, buffer);
	u8 *aml_buffer;
	u8 *aml_start;
	char *path;
	struct acpi_namespace_node *node;
	union acpi_operand_object *method_obj;
	struct acpi_parse_state parser_state;
	u32 aml_length;
	u16 opcode;
	u8 method_flags;
	acpi_status status;

	

	if (!buffer) {
		return AE_BAD_PARAMETER;
	}

	

	if (!ACPI_COMPARE_NAME(table->signature, ACPI_SIG_DSDT) &&
	    !ACPI_COMPARE_NAME(table->signature, ACPI_SIG_SSDT)) {
		return AE_BAD_HEADER;
	}

	

	parser_state.aml = buffer + sizeof(struct acpi_table_header);
	opcode = acpi_ps_peek_opcode(&parser_state);
	if (opcode != AML_METHOD_OP) {
		return AE_BAD_PARAMETER;
	}

	

	parser_state.aml += acpi_ps_get_opcode_size(opcode);
	parser_state.pkg_end = acpi_ps_get_next_package_end(&parser_state);
	path = acpi_ps_get_next_namestring(&parser_state);
	method_flags = *parser_state.aml++;
	aml_start = parser_state.aml;
	aml_length = ACPI_PTR_DIFF(parser_state.pkg_end, aml_start);

	aml_buffer = ACPI_ALLOCATE(aml_length);
	if (!aml_buffer) {
		return AE_NO_MEMORY;
	}

	method_obj = acpi_ut_create_internal_object(ACPI_TYPE_METHOD);
	if (!method_obj) {
		ACPI_FREE(aml_buffer);
		return AE_NO_MEMORY;
	}

	

	status = acpi_ut_acquire_mutex(ACPI_MTX_NAMESPACE);
	if (ACPI_FAILURE(status)) {
		goto error_exit;
	}

	

	status =
	    acpi_ns_lookup(NULL, path, ACPI_TYPE_METHOD, ACPI_IMODE_LOAD_PASS1,
			   ACPI_NS_DONT_OPEN_SCOPE | ACPI_NS_ERROR_IF_FOUND,
			   NULL, &node);

	(void)acpi_ut_release_mutex(ACPI_MTX_NAMESPACE);

	if (ACPI_FAILURE(status)) {	
		if (status != AE_ALREADY_EXISTS) {
			goto error_exit;
		}

		

		if (node->type != ACPI_TYPE_METHOD) {
			status = AE_TYPE;
			goto error_exit;
		}
	}

	

	ACPI_MEMCPY(aml_buffer, aml_start, aml_length);

	

	method_obj->method.aml_start = aml_buffer;
	method_obj->method.aml_length = aml_length;

	method_obj->method.param_count = (u8)
	    (method_flags & AML_METHOD_ARG_COUNT);

	if (method_flags & AML_METHOD_SERIALIZED) {
		method_obj->method.info_flags = ACPI_METHOD_SERIALIZED;

		method_obj->method.sync_level = (u8)
		    ((method_flags & AML_METHOD_SYNC_LEVEL) >> 4);
	}

	status = acpi_ns_attach_object(node, method_obj, ACPI_TYPE_METHOD);

	node->flags |= ANOBJ_ALLOCATED_BUFFER;

	

	acpi_ut_remove_reference(method_obj);
	return status;

error_exit:

	ACPI_FREE(aml_buffer);
	ACPI_FREE(method_obj);
	return status;
}
ACPI_EXPORT_SYMBOL(acpi_install_method)
