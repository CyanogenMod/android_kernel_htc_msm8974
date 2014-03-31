
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
#include "amlcode.h"
#include "actables.h"

#define _COMPONENT          ACPI_NAMESPACE
ACPI_MODULE_NAME("nsutils")

static u8 acpi_ns_valid_path_separator(char sep);

#ifdef ACPI_OBSOLETE_FUNCTIONS
acpi_name acpi_ns_find_parent_name(struct acpi_namespace_node *node_to_search);
#endif


void
acpi_ns_print_node_pathname(struct acpi_namespace_node *node,
			    const char *message)
{
	struct acpi_buffer buffer;
	acpi_status status;

	if (!node) {
		acpi_os_printf("[NULL NAME]");
		return;
	}

	

	buffer.length = ACPI_ALLOCATE_LOCAL_BUFFER;

	status = acpi_ns_handle_to_pathname(node, &buffer);
	if (ACPI_SUCCESS(status)) {
		if (message) {
			acpi_os_printf("%s ", message);
		}

		acpi_os_printf("[%s] (Node %p)", (char *)buffer.pointer, node);
		ACPI_FREE(buffer.pointer);
	}
}


u8 acpi_ns_valid_root_prefix(char prefix)
{

	return ((u8) (prefix == '\\'));
}


static u8 acpi_ns_valid_path_separator(char sep)
{

	return ((u8) (sep == '.'));
}


acpi_object_type acpi_ns_get_type(struct acpi_namespace_node * node)
{
	ACPI_FUNCTION_TRACE(ns_get_type);

	if (!node) {
		ACPI_WARNING((AE_INFO, "Null Node parameter"));
		return_UINT32(ACPI_TYPE_ANY);
	}

	return_UINT32((acpi_object_type) node->type);
}


u32 acpi_ns_local(acpi_object_type type)
{
	ACPI_FUNCTION_TRACE(ns_local);

	if (!acpi_ut_valid_object_type(type)) {

		

		ACPI_WARNING((AE_INFO, "Invalid Object Type 0x%X", type));
		return_UINT32(ACPI_NS_NORMAL);
	}

	return_UINT32((u32) acpi_gbl_ns_properties[type] & ACPI_NS_LOCAL);
}


void acpi_ns_get_internal_name_length(struct acpi_namestring_info *info)
{
	const char *next_external_char;
	u32 i;

	ACPI_FUNCTION_ENTRY();

	next_external_char = info->external_name;
	info->num_carats = 0;
	info->num_segments = 0;
	info->fully_qualified = FALSE;

	if (acpi_ns_valid_root_prefix(*next_external_char)) {
		info->fully_qualified = TRUE;
		next_external_char++;

		

		while (acpi_ns_valid_root_prefix(*next_external_char)) {
			next_external_char++;
		}
	} else {
		

		while (*next_external_char == '^') {
			info->num_carats++;
			next_external_char++;
		}
	}

	if (*next_external_char) {
		info->num_segments = 1;
		for (i = 0; next_external_char[i]; i++) {
			if (acpi_ns_valid_path_separator(next_external_char[i])) {
				info->num_segments++;
			}
		}
	}

	info->length = (ACPI_NAME_SIZE * info->num_segments) +
	    4 + info->num_carats;

	info->next_external_char = next_external_char;
}


acpi_status acpi_ns_build_internal_name(struct acpi_namestring_info *info)
{
	u32 num_segments = info->num_segments;
	char *internal_name = info->internal_name;
	const char *external_name = info->next_external_char;
	char *result = NULL;
	u32 i;

	ACPI_FUNCTION_TRACE(ns_build_internal_name);

	

	if (info->fully_qualified) {
		internal_name[0] = '\\';

		if (num_segments <= 1) {
			result = &internal_name[1];
		} else if (num_segments == 2) {
			internal_name[1] = AML_DUAL_NAME_PREFIX;
			result = &internal_name[2];
		} else {
			internal_name[1] = AML_MULTI_NAME_PREFIX_OP;
			internal_name[2] = (char)num_segments;
			result = &internal_name[3];
		}
	} else {
		i = 0;
		if (info->num_carats) {
			for (i = 0; i < info->num_carats; i++) {
				internal_name[i] = '^';
			}
		}

		if (num_segments <= 1) {
			result = &internal_name[i];
		} else if (num_segments == 2) {
			internal_name[i] = AML_DUAL_NAME_PREFIX;
			result = &internal_name[(acpi_size) i + 1];
		} else {
			internal_name[i] = AML_MULTI_NAME_PREFIX_OP;
			internal_name[(acpi_size) i + 1] = (char)num_segments;
			result = &internal_name[(acpi_size) i + 2];
		}
	}

	

	for (; num_segments; num_segments--) {
		for (i = 0; i < ACPI_NAME_SIZE; i++) {
			if (acpi_ns_valid_path_separator(*external_name) ||
			    (*external_name == 0)) {

				

				result[i] = '_';
			} else {
				

				result[i] =
				    (char)ACPI_TOUPPER((int)*external_name);
				external_name++;
			}
		}

		

		if (!acpi_ns_valid_path_separator(*external_name) &&
		    (*external_name != 0)) {
			return_ACPI_STATUS(AE_BAD_PATHNAME);
		}

		

		external_name++;
		result += ACPI_NAME_SIZE;
	}

	

	*result = 0;

	if (info->fully_qualified) {
		ACPI_DEBUG_PRINT((ACPI_DB_EXEC,
				  "Returning [%p] (abs) \"\\%s\"\n",
				  internal_name, internal_name));
	} else {
		ACPI_DEBUG_PRINT((ACPI_DB_EXEC, "Returning [%p] (rel) \"%s\"\n",
				  internal_name, internal_name));
	}

	return_ACPI_STATUS(AE_OK);
}


acpi_status
acpi_ns_internalize_name(const char *external_name, char **converted_name)
{
	char *internal_name;
	struct acpi_namestring_info info;
	acpi_status status;

	ACPI_FUNCTION_TRACE(ns_internalize_name);

	if ((!external_name) || (*external_name == 0) || (!converted_name)) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	

	info.external_name = external_name;
	acpi_ns_get_internal_name_length(&info);

	

	internal_name = ACPI_ALLOCATE_ZEROED(info.length);
	if (!internal_name) {
		return_ACPI_STATUS(AE_NO_MEMORY);
	}

	

	info.internal_name = internal_name;
	status = acpi_ns_build_internal_name(&info);
	if (ACPI_FAILURE(status)) {
		ACPI_FREE(internal_name);
		return_ACPI_STATUS(status);
	}

	*converted_name = internal_name;
	return_ACPI_STATUS(AE_OK);
}


acpi_status
acpi_ns_externalize_name(u32 internal_name_length,
			 const char *internal_name,
			 u32 * converted_name_length, char **converted_name)
{
	u32 names_index = 0;
	u32 num_segments = 0;
	u32 required_length;
	u32 prefix_length = 0;
	u32 i = 0;
	u32 j = 0;

	ACPI_FUNCTION_TRACE(ns_externalize_name);

	if (!internal_name_length || !internal_name || !converted_name) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	

	switch (internal_name[0]) {
	case '\\':
		prefix_length = 1;
		break;

	case '^':
		for (i = 0; i < internal_name_length; i++) {
			if (internal_name[i] == '^') {
				prefix_length = i + 1;
			} else {
				break;
			}
		}

		if (i == internal_name_length) {
			prefix_length = i;
		}

		break;

	default:
		break;
	}

	if (prefix_length < internal_name_length) {
		switch (internal_name[prefix_length]) {
		case AML_MULTI_NAME_PREFIX_OP:

			

			names_index = prefix_length + 2;
			num_segments = (u8)
			    internal_name[(acpi_size) prefix_length + 1];
			break;

		case AML_DUAL_NAME_PREFIX:

			

			names_index = prefix_length + 1;
			num_segments = 2;
			break;

		case 0:

			

			names_index = 0;
			num_segments = 0;
			break;

		default:

			

			names_index = prefix_length;
			num_segments = 1;
			break;
		}
	}

	required_length = prefix_length + (4 * num_segments) +
	    ((num_segments > 0) ? (num_segments - 1) : 0) + 1;

	if (required_length > internal_name_length) {
		ACPI_ERROR((AE_INFO, "Invalid internal name"));
		return_ACPI_STATUS(AE_BAD_PATHNAME);
	}

	

	*converted_name = ACPI_ALLOCATE_ZEROED(required_length);
	if (!(*converted_name)) {
		return_ACPI_STATUS(AE_NO_MEMORY);
	}

	j = 0;

	for (i = 0; i < prefix_length; i++) {
		(*converted_name)[j++] = internal_name[i];
	}

	if (num_segments > 0) {
		for (i = 0; i < num_segments; i++) {
			if (i > 0) {
				(*converted_name)[j++] = '.';
			}

			(*converted_name)[j++] = internal_name[names_index++];
			(*converted_name)[j++] = internal_name[names_index++];
			(*converted_name)[j++] = internal_name[names_index++];
			(*converted_name)[j++] = internal_name[names_index++];
		}
	}

	if (converted_name_length) {
		*converted_name_length = (u32) required_length;
	}

	return_ACPI_STATUS(AE_OK);
}


struct acpi_namespace_node *acpi_ns_validate_handle(acpi_handle handle)
{

	ACPI_FUNCTION_ENTRY();

	

	if ((!handle) || (handle == ACPI_ROOT_OBJECT)) {
		return (acpi_gbl_root_node);
	}

	

	if (ACPI_GET_DESCRIPTOR_TYPE(handle) != ACPI_DESC_TYPE_NAMED) {
		return (NULL);
	}

	return (ACPI_CAST_PTR(struct acpi_namespace_node, handle));
}


void acpi_ns_terminate(void)
{
	union acpi_operand_object *obj_desc;

	ACPI_FUNCTION_TRACE(ns_terminate);

	acpi_ns_delete_namespace_subtree(acpi_gbl_root_node);

	

	obj_desc = acpi_ns_get_attached_object(acpi_gbl_root_node);
	if (obj_desc) {
		acpi_ns_detach_object(acpi_gbl_root_node);
	}

	ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Namespace freed\n"));
	return_VOID;
}


u32 acpi_ns_opens_scope(acpi_object_type type)
{
	ACPI_FUNCTION_TRACE_STR(ns_opens_scope, acpi_ut_get_type_name(type));

	if (!acpi_ut_valid_object_type(type)) {

		

		ACPI_WARNING((AE_INFO, "Invalid Object Type 0x%X", type));
		return_UINT32(ACPI_NS_NORMAL);
	}

	return_UINT32(((u32) acpi_gbl_ns_properties[type]) & ACPI_NS_NEWSCOPE);
}


acpi_status
acpi_ns_get_node(struct acpi_namespace_node *prefix_node,
		 const char *pathname,
		 u32 flags, struct acpi_namespace_node **return_node)
{
	union acpi_generic_state scope_info;
	acpi_status status;
	char *internal_path;

	ACPI_FUNCTION_TRACE_PTR(ns_get_node, ACPI_CAST_PTR(char, pathname));

	if (!pathname) {
		*return_node = prefix_node;
		if (!prefix_node) {
			*return_node = acpi_gbl_root_node;
		}
		return_ACPI_STATUS(AE_OK);
	}

	

	status = acpi_ns_internalize_name(pathname, &internal_path);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	

	status = acpi_ut_acquire_mutex(ACPI_MTX_NAMESPACE);
	if (ACPI_FAILURE(status)) {
		goto cleanup;
	}

	

	scope_info.scope.node = prefix_node;

	

	status = acpi_ns_lookup(&scope_info, internal_path, ACPI_TYPE_ANY,
				ACPI_IMODE_EXECUTE,
				(flags | ACPI_NS_DONT_OPEN_SCOPE), NULL,
				return_node);
	if (ACPI_FAILURE(status)) {
		ACPI_DEBUG_PRINT((ACPI_DB_EXEC, "%s, %s\n",
				  pathname, acpi_format_exception(status)));
	}

	(void)acpi_ut_release_mutex(ACPI_MTX_NAMESPACE);

      cleanup:
	ACPI_FREE(internal_path);
	return_ACPI_STATUS(status);
}
